// ============================================================================
//  web_admission_api.inc — the C++ side of the admission IPC bridge.
//  Included into web_admission.cpp. Every verb the JS UI can send is handled
//  here against the REAL data layer (patients, services, insurances, doctors,
//  reception persistence + the Management print design). Returns a JSON string.
//  This is the single source of truth — the UI never fabricates prices,
//  identities or bill amounts.
// ============================================================================
#include "service_identity.h"
#include "service_canonicalization.h"
#include <algorithm>          // v1.93: std::reverse for portal message ordering

// ---- serialize a PatientRow to JSON (v1.40.0 struct-based, authoritative) ---
//  The struct overload is the single source of truth for the wire format; the
//  legacy positional overload below delegates to it so the two never diverge.
static std::string patientJson(const PatientRow& p){
    std::string o="{";
    o+="\"nid\":"    +jstr(p.nid)    +",";
    o+="\"first\":"  +jstr(p.first)  +",";
    o+="\"last\":"   +jstr(p.last)   +",";
    o+="\"father\":" +jstr(p.father) +",";
    o+="\"gender\":" +jstr(p.gender) +",";
    o+="\"birth\":"  +jstr(p.birth)  +",";
    o+="\"mobile\":" +jstr(p.mobile) +",";
    o+="\"phone\":"  +jstr(p.landline)+",";   // v1.40.0 — تلفن ثابت
    o+="\"addr\":"   +jstr(p.address) +",";   // v1.40.0 — آدرس
    o+="\"suppIdx\":"+jnum(p.suppIdx) +",";   // v1.40.0 — SUPP_INSURANCES[] index
    o+="\"insurances\":[";
    for(size_t i=0;i<p.insurances.size();++i){ if(i)o+=","; o+=jnum(p.insurances[i]); }
    o+="]}";
    return o;
}
// ---- legacy positional overload — delegates to the struct one --------------
static std::string patientJson(const std::wstring& nid, const std::wstring& first,
        const std::wstring& last, const std::wstring& father, const std::wstring& gender,
        const std::wstring& birth, const std::wstring& mobile, const std::vector<int>& ins,
        const std::wstring& landline=L"", const std::wstring& address=L"", int suppIdx=-1){
    PatientRow p;
    p.nid=nid; p.first=first; p.last=last; p.father=father; p.gender=gender;
    p.birth=birth; p.mobile=mobile; p.landline=landline; p.address=address;
    p.insurances=ins; p.suppIdx=suppIdx;
    return patientJson(p);
}

static std::string serviceJson(const ServiceDef& s){
    std::string o="{";
    o+="\"code\":"+jstr(s.code)+",";
    o+="\"name\":"+jstr(s.name)+",";
    o+="\"category\":"+jstr(s.category)+",";
    o+="\"dept\":"+jstr(s.dept)+",";
    o+="\"desc\":"+jstr(s.desc)+",";
    o+="\"insType\":"+jstr(s.insType)+",";
    o+="\"insName\":"+jstr(s.insName)+",";
    o+="\"multiplier\":"+jstr(s.multiplier)+",";
    o+="\"price\":"+jnum(s.price)+",";
    // v1.75.0: expose the tariff prices so the JS billing can pick the right one
    o+="\"priceFree\":"+jnum(s.priceFree)+",";
    o+="\"priceGov\":"+jnum(s.priceGov)+",";
    o+="\"priceIns\":"+jnum(s.priceIns)+",";
    o+="\"status\":"+jnum(s.status);
    o+="}";
    return o;
}

// ---- insurance lists (base + supplementary) --------------------------------
// v1.74: merged with the user-managed «تعریف بیمه» registry. The canonical
// hardcoded table is overridden per-index by any user definition (name + سهم
// سازمان) and extended with user-added rows, so the admission dropdown shows
// exactly the insurances defined in the CRM. Positions 0..N-1 stay stable so
// existing patient records keep their meaning; user-added rows append after.
static std::string insurancesJson(){
    auto defs=loadInsDefs();
    int maxIdx=N_INSURANCES-1;
    for(const auto& d:defs) if(d.idx>maxIdx) maxIdx=d.idx;
    std::string o="[";
    bool first=true;
    for(int i=0;i<=maxIdx;i++){
        const InsDef* def=nullptr;
        for(const auto& d:defs) if(d.idx==i){ def=&d; break; }
        std::wstring name; int pct;
        if(def){
            if(!def->groupName.empty())      name=def->groupName;
            else if(!def->insCode.empty())    name=def->insCode;
            else if(i<N_INSURANCES)           name=INSURANCES[i].name;
        } else if(i<N_INSURANCES){ name=INSURANCES[i].name; }
        else continue;
        if(name.empty()) continue;
        pct=Ins_Percent(i);
        if(!first) o+=","; first=false;
        o+="{\"name\":"+jstr(name)+",\"pct\":"+jnum(pct)+"}";
    }
    o+="]"; return o;
}
static std::string suppInsurancesJson(){
    auto defs=loadSuppDefs();
    int maxIdx=N_SUPP-1;
    for(const auto& d:defs) if(d.idx>maxIdx) maxIdx=d.idx;
    std::string o="[";
    bool first=true;
    for(int i=0;i<=maxIdx;i++){
        const SuppDef* def=nullptr;
        for(const auto& d:defs) if(d.idx==i){ def=&d; break; }
        std::wstring name; int pct;
        if(def && !def->name.empty()) name=def->name;
        else if(i<N_SUPP) name=SUPP_INSURANCES[i].name;
        else continue;
        if(name.empty()) continue;
        pct=Supp_Percent(i);
        if(!first) o+=","; first=false;
        o+="{\"name\":"+jstr(name)+",\"pct\":"+jnum(pct)+"}";
    }
    o+="]"; return o;
}

// case-insensitive contains for wide strings (Persian is unaffected)
static bool wcontains(const std::wstring& hay, const std::wstring& needle){
    if(needle.empty()) return true;
    auto lower=[](std::wstring s){ for(auto&c:s) c=(wchar_t)towlower(c); return s; };
    return lower(hay).find(lower(needle))!=std::wstring::npos;
}

// ---- current Jalali date + time strings (for the header clock + records) ---
static std::wstring adTodayJalali(){
    SYSTEMTIME st=iranNow();
    int jy,jm,jd; gregToJalali(st.wYear,st.wMonth,st.wDay,jy,jm,jd);
    wchar_t b[32]; swprintf(b,32,L"%04d/%02d/%02d",jy,jm,jd);
    return b;
}
static std::wstring adNowTime(){
    SYSTEMTIME st=iranNow();
    wchar_t b[16]; swprintf(b,16,L"%02d:%02d:%02d",st.wHour,st.wMinute,st.wSecond);
    return b;
}
static std::string adDoctorStatsJson(const std::wstring& doctor){
    int total=countTodayDoctorReceptions(doctor,false);
    int paid=countTodayDoctorReceptions(doctor,true);
    int waiting=total-paid; if(waiting<0) waiting=0;
    // v1.69.0: P=total admissions in 24h, S=cashiered (paid), plus turn stats:
    // seen=doctor has processed (paid), waiting=still in queue for this doctor.
    return "{\"P\":"+jnum(total)+",\"S\":"+jnum(paid)+
           ",\"seen\":"+jnum(paid)+",\"waiting\":"+jnum(waiting)+"}";
}
static std::wstring adBlockRemaining(const BlacklistEntry& b){
    if(b.expiresEpochMin==0) return L"مسدودی دائم";
    long long mins=b.expiresEpochMin-Blacklist_NowEpochMinutes();
    if(mins<1) mins=1;
    if(mins<60) return std::to_wstring(mins)+L" دقیقه باقی‌مانده";
    if(mins<1440) return std::to_wstring((mins+59)/60)+L" ساعت باقی‌مانده";
    return std::to_wstring((mins+1439)/1440)+L" روز باقی‌مانده";
}
static std::string adBlockJson(const BlacklistEntry& b){
    return "{\"nid\":"+jstr(b.nid)+",\"reason\":"+jstr(b.reason)+
           ",\"remaining\":"+jstr(adBlockRemaining(b))+"}";
}
// v1.60.0 — the catalogue lookup now also RETURNS the matched ServiceDef so
// callers can enrich every printed service line with the real نام/شرح/دسته
// defined in Management → خدمات. This is the hard link the user kept asking
// for: the reception service row is bound to the catalogue entry BY CODE, and
// whatever the HTML form sent (or failed to send) is replaced with the
// authoritative catalogue text before the record reaches the print engine.
// v1.61.0 — THE services-not-printing ROOT CAUSE FIX.
// The previous implementation returned FALSE for (a) a zero-priced catalogue
// entry and (b) any row it could not match, and both call sites did
// `if(!adCatalogPrice(...)) continue;`. Consequence: the service the operator
// picked in پذیرش was SILENTLY DROPPED from ReceptionRecord::services and could
// therefore never appear in a printed receipt — the single biggest bug reported.
//
// New contract: a row is accepted whenever it carries ANY identity at all
// (code, name, or a free rate). Price 0 is perfectly legal (رایگان / تحت پوشش
// کامل بیمه). Only a completely empty row is rejected.
static bool adCatalogPrice(const std::vector<ServiceDef>& catalog,
                           const std::string& obj,long long& price,
                           const ServiceDef** defOut=nullptr,
                           bool hasIns=false){
    bool freeRate=jsonGetBool(obj,"freeRate",false);
    double n=0;
    std::string codeU; jsonGetString(obj,"code",codeU);
    std::string nameU; jsonGetString(obj,"name",nameU);
    std::wstring want     = serviceIdentityKey(u82w(codeU));
    std::wstring wantName = serviceIdentityKey(u82w(nameU));

    auto pickTariff=[&](const ServiceDef& s)->long long{
        long long a=0;
        if(hasIns){
            if(s.priceIns>0) a=s.priceIns;
            else if(s.priceInsNew>0) a=s.priceInsNew;
            else if(s.priceGov>0) a=s.priceGov;
            else if(s.priceGovNew>0) a=s.priceGovNew;
            else if(s.priceFree>0) a=s.priceFree;
            else if(s.priceFreeNew>0) a=s.priceFreeNew;
            else a=s.price;
        } else {
            if(s.priceFree>0) a=s.priceFree;
            else if(s.priceFreeNew>0) a=s.priceFreeNew;
            else if(s.priceGov>0) a=s.priceGov;
            else if(s.priceGovNew>0) a=s.priceGovNew;
            else if(s.priceIns>0) a=s.priceIns;
            else if(s.priceInsNew>0) a=s.priceInsNew;
            else a=s.price;
        }
        if(a<0) a=0;
        if(!s.multiplier.empty()){
            std::wstring m=s.multiplier;
            for(auto& c:m){
                if(c==L'٫'||c==L'،') c=L'.';
                if(c>=0x06F0&&c<=0x06F9) c=(wchar_t)(L'0'+(c-0x06F0));
                if(c>=0x0660&&c<=0x0669) c=(wchar_t)(L'0'+(c-0x0660));
            }
            double mul=_wtof(m.c_str());
            if(mul>0.0001 && mul!=1.0) a=(long long)llround((double)a * mul);
        }
        return a;
    };
    if(freeRate){
        // Manual («آزاد») rate row: trust the typed amount, but still bind the
        // row to its catalogue entry when we can so نام/شرح/دسته stay real.
        price = (jsonGetNumber(obj,"freePrice",n) && n>0) ? (long long)llround(n) : 0;
        if(defOut && !want.empty()){
            for(const auto& s:catalog)
                if(s.status!=0 && serviceIdentityKey(s.code)==want){ *defOut=&s; break; }
        }
        return !(want.empty() && wantName.empty() && price<=0);
    }

    // exact code match first — price 0 is accepted (رایگان است، حذف نمی‌شود)
    // v1.75.0: select the tariff price based on the patient's insurance type.
    //   - no insurance / آزاد → priceFree (or s.price as legacy fallback)
    //   - government insurance → priceGov
    //   - base insurance → priceIns
    //   This ensures the price defined in the CRM services page actually flows
    //   to the admission billing calculation.
    if(!want.empty()){
        for(const auto& s:catalog){
            if(s.status!=0 && serviceIdentityKey(s.code)==want){
                price = pickTariff(s);
                if(defOut) *defOut=&s;
                return true;
            }
        }
    }
    // Name is only a fallback when the submitted row has no code. A non-empty
    // unmatched code must not silently bind to a different same-named service,
    // because that would change both identity and authoritative catalogue price.
    if(want.empty()&&!wantName.empty()){
        for(const auto& s:catalog){
            if(s.status!=0 && serviceIdentityKey(s.name)==wantName){
                price = pickTariff(s);
                if(defOut) *defOut=&s;
                return true;
            }
        }
    }
    // Not in the catalogue at all (deleted/deactivated service, or a row typed
    // by hand). Keep its identity so it still prints, but never trust a normal-
    // rate amount supplied by the browser: only an explicitly enabled manual/
    // free rate may carry a submitted unit price.
    if(!want.empty() || !wantName.empty()){
        price=0;
        return true;
    }
    return false;   // truly empty row
}

// ---- resolve the operator's print-design SECTION (mirrors reception.cpp) ----
static int adResolveSectionId(){
    std::wstring dept=g_session.user.dept;
    std::vector<Section> all; Sections_All(all);
    if(all.empty()) return 0;
    if(!dept.empty()){
        for(const auto& s:all) if(s.is_active && s.name_fa==dept) return s.id;
        for(const auto& s:all) if(s.is_active && s.code==dept) return s.id;
        for(const auto& s:all)
            if(s.is_active && (s.name_fa.find(dept)!=std::wstring::npos ||
                               dept.find(s.name_fa)!=std::wstring::npos)) return s.id;
    }
    for(const auto& s:all) if(s.is_active && s.kind==L"reception") return s.id;
    for(const auto& s:all) if(s.is_active) return s.id;
    return 0;
}

// ============================================================================
//  «صندوق نرفته‌ها» — unpaid / not-yet-cashiered queue.
//  File-backed (data\unpaid_queue.dat) so it survives restarts and stays a
//  single static EXE. One pipe-delimited line per entry:
//    id|nid|first|last|mobile|barcode|jdate|time|epochMin|amount
// ============================================================================
static std::wstring adQueuePath(bool admissionQueue){
    return dataDir()+(admissionQueue?L"\\recept_queue_web.dat":L"\\unpaid_queue.dat");
}

static long long adEpochMinutes(){
    // minutes since a fixed epoch, from Iran-local wall clock (stable ordering)
    SYSTEMTIME st=iranNow();
    FILETIME ft; SystemTimeToFileTime(&st,&ft);
    ULARGE_INTEGER u; u.LowPart=ft.dwLowDateTime; u.HighPart=ft.dwHighDateTime;
    return (long long)(u.QuadPart/600000000ULL); // 100ns→minutes
}

struct AdQueueRow {
    std::wstring id,nid,first,last,mobile,barcode,jdate,time;
    long long epochMin, amount;
    int turn;
    AdQueueRow():epochMin(0),amount(0),turn(0){}
};

static std::vector<std::wstring> adSplit(const std::wstring& s, wchar_t d){
    std::vector<std::wstring> out; std::wstring cur;
    for(wchar_t c:s){ if(c==d){ out.push_back(cur); cur.clear(); } else cur+=c; }
    out.push_back(cur); return out;
}
static std::wstring adEscPipe(const std::wstring& s){
    std::wstring o=s; for(auto&c:o){ if(c==L'|') c=L'¦'; if(c==L'\n'||c==L'\r') c=L' '; } return o;
}

static std::vector<AdQueueRow> adQueueLoad(bool admissionQueue){
    std::vector<AdQueueRow> out;
    std::wstring all=readFileUtf8(adQueuePath(admissionQueue));
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=all.substr(pos,e-pos); pos=e+1;
        while(!line.empty()&&(line.back()==L'\r'||line.back()==L' ')) line.pop_back();
        if(line.empty()) continue;
        auto f=adSplit(line,L'|');
        if(f.size()<10) continue;
        AdQueueRow r;
        r.id=f[0]; r.nid=f[1]; r.first=f[2]; r.last=f[3]; r.mobile=f[4];
        r.barcode=f[5]; r.jdate=f[6]; r.time=f[7];
        r.epochMin=_wtoi64(f[8].c_str()); r.amount=_wtoi64(f[9].c_str());
        if(f.size()>=11) r.turn=_wtoi(f[10].c_str());
        out.push_back(r);
    }
    return out;
}
static void adQueueSave(const std::vector<AdQueueRow>& rows,bool admissionQueue){
    std::wstring all;
    for(const auto& r:rows){
        wchar_t em[32],am[32],tn[16];
        swprintf(em,32,L"%lld",r.epochMin); swprintf(am,32,L"%lld",r.amount);
        swprintf(tn,16,L"%d",r.turn);
        all += adEscPipe(r.id)+L"|"+adEscPipe(r.nid)+L"|"+adEscPipe(r.first)+L"|"+
               adEscPipe(r.last)+L"|"+adEscPipe(r.mobile)+L"|"+adEscPipe(r.barcode)+L"|"+
               adEscPipe(r.jdate)+L"|"+adEscPipe(r.time)+L"|"+em+L"|"+am+L"|"+tn+L"\r\n";
    }
    writeFileUtf8(adQueuePath(admissionQueue),all,false);
}
static std::string adQueueJson(bool admissionQueue, int hours){
    auto rows=adQueueLoad(admissionQueue);
    long long now=adEpochMinutes();
    long long win = hours>0 ? now-(long long)hours*60 : 0;
    // newest first
    std::string o="{\"rows\":[";
    bool first=true;
    for(size_t i=0;i<rows.size();++i){
        const AdQueueRow& r=rows[rows.size()-1-i];
        if(hours>0 && r.epochMin<win) continue;
        if(!first) o+=",";
        first=false;
        long long mins=now-r.epochMin; if(mins<0) mins=0;
        std::wstring full=r.first+L" "+r.last;
        o+="{";
        o+="\"id\":"+jstr(r.id)+",";
        o+="\"nid\":"+jstr(r.nid)+",";
        o+="\"first\":"+jstr(r.first)+",";
        o+="\"last\":"+jstr(r.last)+",";
        o+="\"name\":"+jstr(full)+",";
        o+="\"mobile\":"+jstr(r.mobile)+",";
        o+="\"barcode\":"+jstr(r.barcode)+",";
        o+="\"date\":"+jstr(r.jdate)+",";
        o+="\"time\":"+jstr(r.time)+",";
        o+="\"minsAgo\":"+jnum(mins)+",";
        o+="\"amount\":"+jnum(r.amount)+",";
        o+="\"turn\":"+jnum(r.turn);
        o+="}";
    }
    o+="]}"; return o;
}

static bool adQueueTake(std::vector<AdQueueRow>& rows, const std::wstring& id, AdQueueRow& found){
    std::vector<AdQueueRow> keep;
    bool have=false;
    for(const auto& r:rows){
        if(r.id==id){ found=r; have=true; }
        else keep.push_back(r);
    }
    if(!have) return false;
    rows.swap(keep);
    return true;
}
static int adQueueNextTurn(const std::vector<AdQueueRow>& rows){
    int mx=0;
    for(const auto& r:rows) if(r.turn>mx) mx=r.turn;
    return mx+1;
}

// ---- parse the JS "patient" object out of a record body -------------------
struct AdPatient { std::wstring nid,first,last,father,gender,birth,mobile,phone,addr; };
static AdPatient adReadPatient(const std::string& body){
    AdPatient p;
    std::string patRaw; if(!jsonGetRaw(body,"patient",patRaw)) patRaw=body;
    std::string s;
    if(jsonGetString(patRaw,"nid",s))    p.nid=normDigits(u82w(s));
    if(jsonGetString(patRaw,"first",s))  p.first=u82w(s);
    if(jsonGetString(patRaw,"last",s))   p.last=u82w(s);
    if(jsonGetString(patRaw,"father",s)) p.father=u82w(s);
    if(jsonGetString(patRaw,"gender",s)) p.gender=u82w(s);
    if(jsonGetString(patRaw,"birth",s))  p.birth=normDigits(u82w(s));
    if(jsonGetString(patRaw,"mobile",s)) p.mobile=normDigits(u82w(s));
    if(jsonGetString(patRaw,"phone",s))  p.phone=normDigits(u82w(s));
    if(jsonGetString(patRaw,"addr",s))   p.addr=u82w(s);
    return p;
}

// ---- compute the bill for a record body (server-authoritative) -------------
// Services are first canonicalized by catalogue code/name. Repeated selections
// merge into one line by adding quantity and discount; only then are every
// amount and insurance share recomputed from authoritative unit prices.
struct AdBill {
    long long gross=0, disc=0, org=0, supp=0, pat=0, paid=0;
    int insIdx=0, suppIdx=0, basePct=0, suppPct=0;
};
static std::vector<ServiceLine> adCanonicalServices(const std::string& body,
        const std::vector<ServiceDef>& catalog,int basePct,int suppPct){
    std::vector<ServiceLine> lines;
    // Identity codes preserve what the browser actually submitted. Catalogue
    // enrichment may fill an empty ServiceLine.code later, but duplicate-name
    // fallback must still know that this particular selection arrived uncoded.
    std::vector<std::wstring> identityCodes;
    std::vector<bool> identityFreeRates;
    std::string arr; jsonGetRaw(body,"services",arr);
    size_t pos=0;
    while(pos<arr.size()){
        size_t o=arr.find('{',pos); if(o==std::string::npos) break;
        int depth=0; bool inStr=false; size_t q=o;
        for(;q<arr.size();++q){
            char c=arr[q];
            if(inStr){ if(c=='\\'){q++;continue;} if(c=='"') inStr=false; continue; }
            if(c=='"'){ inStr=true; continue; }
            if(c=='{') depth++;
            else if(c=='}'){ depth--; if(depth==0){ q++; break; } }
        }
        std::string obj=arr.substr(o,q-o); pos=q;
        long long unitPrice=0;
        bool freeRate=jsonGetBool(obj,"freeRate",false);
        const ServiceDef* def=nullptr;
        bool hasIns=jsonGetBool(body,"hasIns",false);
        if(!adCatalogPrice(catalog,obj,unitPrice,&def,hasIns)) continue;

        ServiceLine ln;
        std::string cs,ns,cats,desc;
        if(jsonGetString(obj,"code",cs))       ln.code=normDigits(trim(u82w(cs)));
        if(jsonGetString(obj,"name",ns))       ln.name=trim(u82w(ns));
        std::wstring identityCode=serviceIdentityKey(ln.code);
        if(jsonGetString(obj,"category",cats)) ln.category=trim(u82w(cats));
        if(jsonGetString(obj,"desc",desc))     ln.desc=trim(u82w(desc));
        if(def){
            ln.code=normDigits(trim(def->code));
            ln.name=trim(def->name);
            ln.category=trim(def->category);
            ln.desc=trim(def->desc);
            if(ln.desc.empty()) ln.desc=ln.category;
        } else if(!freeRate) {
            // Unknown/deactivated normal-rate rows may remain visible, but their
            // amount is never accepted from the browser. A manual/free row keeps
            // its explicitly submitted freePrice as its effective unit rate.
            unitPrice=0;
        }
        if(ln.name.empty()){
            if(!ln.desc.empty()) ln.name=ln.desc;
            else if(!ln.category.empty()) ln.name=ln.category;
            else ln.name=ln.code;
        }

        double qty=1,discount=0;
        jsonGetNumber(obj,"qty",qty); jsonGetNumber(obj,"discount",discount);
        if(qty<1) qty=1; if(qty>999) qty=999;
        ln.qty=(int)qty; ln.price=unitPrice<0?0:unitPrice;
        ln.discount=(long long)llround(discount); if(ln.discount<0) ln.discount=0;
        long long gross=ln.price*(long long)ln.qty;
        if(ln.discount>gross) ln.discount=gross;

        // Codes are the primary identity. A shared display name is only a
        // fallback when at least one row genuinely has no code; normal/free or
        // effective-price variants remain distinct.
        serviceCanonicalAdd(lines,identityCodes,identityFreeRates,ln,
                            identityCode,freeRate);
    }

    for(auto& ln:lines){
        int qty=ln.qty>0?ln.qty:1;
        long long gross=ln.price*(long long)qty;
        if(ln.discount<0) ln.discount=0; if(ln.discount>gross) ln.discount=gross;
        long long net=gross-ln.discount;
        long long org=(long long)llround((double)net*basePct/100.0);
        if(org<0) org=0; if(org>net) org=net;
        long long afterBase=net-org;
        long long sup=(long long)llround((double)afterBase*suppPct/100.0);
        if(sup<0) sup=0; if(sup>afterBase) sup=afterBase;
        ln.insShare=org+sup;
        ln.patShare=afterBase-sup;
    }
    // Stable variant ordering makes canonical rows, printed order and totals
    // identical for normal→free and free→normal submissions.
    serviceCanonicalSort(lines,identityCodes,identityFreeRates);
    return lines;
}
static AdBill adComputeBill(const std::string& body,
                            const std::vector<ServiceDef>& catalog,
                            std::vector<ServiceLine>* canonicalOut=nullptr){
    AdBill b;
    bool hasIns=jsonGetBool(body,"hasIns",false);
    double d;
    if(jsonGetNumber(body,"insMain",d)) b.insIdx=(int)d;
    if(jsonGetNumber(body,"insSupp",d)) b.suppIdx=(int)d;
    if(jsonGetNumber(body,"insSuppPct",d)) b.suppPct=(int)llround(d);
    if(b.insIdx<0 || b.insIdx>=N_INSURANCES) b.insIdx=0;
    if(b.suppIdx<0 || b.suppIdx>=N_SUPP) b.suppIdx=0;
    if(hasIns) b.basePct=INSURANCES[b.insIdx].pct;
    if(b.suppPct<=0) b.suppPct=SUPP_INSURANCES[b.suppIdx].pct;
    if(b.basePct<0) b.basePct=0; if(b.basePct>100) b.basePct=100;
    if(b.suppPct<0) b.suppPct=0; if(b.suppPct>100) b.suppPct=100;
    if(!hasIns){ b.basePct=0; b.insIdx=0; }

    std::vector<ServiceLine> lines=adCanonicalServices(body,catalog,b.basePct,b.suppPct);
    for(const auto& ln:lines){
        long long gross=ln.price*(long long)(ln.qty>0?ln.qty:1);
        b.gross+=gross; b.disc+=ln.discount; b.pat+=ln.patShare;
    }
    // ServiceLine stores combined base+supp insurance. Split it into invoice
    // buckets with the same authoritative formula used for each canonical row.
    for(const auto& ln:lines){
        long long net=ln.price*(long long)(ln.qty>0?ln.qty:1)-ln.discount;
        long long org=(long long)llround((double)net*b.basePct/100.0);
        if(org<0) org=0; if(org>net) org=net;
        long long afterBase=net-org;
        long long sup=(long long)llround((double)afterBase*b.suppPct/100.0);
        if(sup<0) sup=0; if(sup>afterBase) sup=afterBase;
        b.org+=org; b.supp+=sup;
    }
    b.paid=jsonGetBool(body,"noPay",false)?0:b.pat;
    if(canonicalOut) *canonicalOut=lines;
    return b;
}
static AdBill adComputeBill(const std::string& body){
    return adComputeBill(body,loadServices(),nullptr);
}

// ---- build a ReceptionRecord from a record body ---------------------------
static ReceptionRecord adBuildRecord(const std::string& body){
    AdPatient p=adReadPatient(body);
    const std::vector<ServiceDef> catalog=loadServices();
    std::vector<ServiceLine> canonicalServices;
    AdBill b=adComputeBill(body,catalog,&canonicalServices);
    ReceptionRecord r;
    r.firstName=p.first; r.lastName=p.last; r.nationalId=p.nid;
    r.fatherName=p.father; r.birthDate=p.birth; r.gender=p.gender;
    r.mobile=p.mobile; r.landline=p.phone; r.address=p.addr;
    std::string s;
    if(jsonGetString(body,"ptype",s)) r.patientType=u82w(s);
    if(jsonGetString(body,"insType",s)) r.insuranceType=u82w(s);
    // Date and shift are assigned from the trusted native session, not HTML.
    r.apptDate=adTodayJalali();
    r.shift=shiftName(g_session.shift);
    r.dept=g_session.user.dept;
    if(jsonGetString(body,"doc2name",s)) r.treatingDoctor=u82w(s);
    if(jsonGetString(body,"doc2code",s)) r.doctorCode=normDigits(u82w(s));
    if(jsonGetString(body,"perfcode",s)) r.performerCode=normDigits(u82w(s));
    if(jsonGetString(body,"perfname",s)) r.performer=u82w(s);
    if(jsonGetString(body,"insBooklet",s)) r.insNo=normDigits(u82w(s));
    if(jsonGetString(body,"insValid",s)) r.insExp=normDigits(u82w(s));
    r.apptTime=adNowTime();
    r.userName=g_session.user.username;
    r.receptionist=g_session.user.fullname.empty()?g_session.user.username:g_session.user.fullname;
    r.insIdx=b.insIdx; r.suppIdx=b.suppIdx;
    if(r.insIdx>=0 && r.insIdx<N_INSURANCES) r.insurance=INSURANCES[r.insIdx].name;
    if(r.suppIdx>=0 && r.suppIdx<N_SUPP) r.suppInsurance=SUPP_INSURANCES[r.suppIdx].name;
    r.total=b.gross; r.discount=b.disc; r.orgShare=b.supp;
    r.mainShare=b.org; r.baseDiff=b.gross-b.disc-b.org;
    if(r.baseDiff<0) r.baseDiff=0;
    r.patientShare=b.pat; r.finalTotal=b.paid;
    r.paid=b.paid;
    r.insPercent=b.basePct; r.suppPercent=b.suppPct;

    // Print exactly the canonical rows used for the authoritative bill.
    r.services.swap(canonicalServices);

    return r;
}

// ---------------------------------------------------------------------------
//  v1.93: portal-message + performer helpers (folded into init / verbs).
//  reception.cpp has static equivalents (cartHtmlEsc / splitMsgTime / sevLabel)
//  but they live in a different translation unit, so the admission bridge
//  re-implements the small ones here. toFaDigits() comes from app.h.
// ---------------------------------------------------------------------------
static std::wstring portalHtmlEsc(const std::wstring& s){
    std::wstring o; o.reserve(s.size());
    for(size_t i=0;i<s.size();i++){
        wchar_t c=s[i];
        if(c==L'&') o+=L"&amp;";
        else if(c==L'<') o+=L"&lt;";
        else if(c==L'>') o+=L"&gt;";
        else if(c==L'"') o+=L"&quot;";
        else o+=c;
    }
    return o;
}
static std::wstring portalSevLabel(int type){
    return type==KMSG_CRITICAL ? L"بحرانی"
         : type==KMSG_URGENT   ? L"فوری"
         :                       L"عادی";
}
static void portalSplitTime(const std::wstring& s, std::wstring& date, std::wstring& time){
    size_t sp=s.find(L' ');
    if(sp==std::wstring::npos){ date=s; time=L""; }
    else { date=s.substr(0,sp); time=s.substr(sp+1); }
}
// v1.93: the full performer directory (performers only, no filter) — folded
// into init so the portal surface can populate its performer combo without a
// second round-trip. Mirrors the doctor.performers verb's no-filter path.
static std::string performersJson(){
    auto docs=loadDoctors();
    std::string o="{\"rows\":[";
    int n=0;
    for(size_t i=0;i<docs.size();++i){
        const auto& d=docs[i];
        if(!d.isPerformer) continue;
        wchar_t cb[16]; swprintf(cb,16,L"%d",(int)i+1);
        std::wstring dcode=cb;
        if(n++) o+=",";
        o+="{\"name\":"+jstr(d.name)+",\"specialty\":"+jstr(d.specialty)+
           ",\"code\":"+jstr(dcode)+
           ",\"medicalId\":"+jstr(d.medicalId)+
           ",\"isPerformer\":"+(d.isPerformer?std::string("true"):std::string("false"))+"}";
        if(n>=500) break;
    }
    o+="]}"; return o;
}

// ---------------------------------------------------------------------------
//  admissionApi — route a verb + JSON payload to a JSON response.
// ---------------------------------------------------------------------------
static std::string admissionApi(const std::string& verb, const std::string& body){
    // ---- init: send everything the page needs to render ----
    if(verb=="init"||verb=="ready"){
        std::string o="{";
        o+="\"ok\":true,";
        o+="\"insurances\":"+insurancesJson()+",";
        o+="\"supp\":"+suppInsurancesJson()+",";
        std::wstring user=g_session.user.username;
        std::wstring suffix=user.empty()?L"":L"."+user;
        std::wstring mode=getSetting(L"reception.mode"+suffix,
                           getSetting(L"reception.mode",L"simple"));
        if(mode!=L"full") mode=L"simple";
        // v2.01 (Part H): zoom is session-scoped — always reset to the fitted
        // default (80%) on every page build. Never restore a persisted value
        // (the old per-user persistence corrupted layouts across sessions).
        int zoom=80;
        o+="\"date\":"+jstr(adTodayJalali())+",";
        o+="\"time\":"+jstr(adNowTime())+",";
        o+="\"shift\":"+jstr(shiftName(g_session.shift))+",";
        o+="\"shiftOpen\":"; o+=(Shift_IsOpen()?"true":"false"); o+=",";
        o+="\"mode\":"+jstr(mode)+",";
        o+="\"zoom\":"+jnum(zoom)+",";
        o+="\"theme\":"+jstr(getSetting(L"theme",L"light"))+",";
        // v1.77: calm/warm palettes retired — the admission surface is always the
        // standard light (blue) palette or dark, so no "palette" field is sent.
        o+="\"ps\":{\"P\":0,\"S\":0},";
        // v1.79.0: per-account access ticks (مدیریت ← تعریف حساب کاربری). The
        // page hides the elements the account has no access to (e.g. the
        // صندوق/صف launcher without «گزارش و صندوق»). Management passes all.
        o+="\"perms\":{";
        o+="\"admission\":";     o+=(userHasPerm(g_session.user,L"admission")?"true":"false"); o+=",";
        o+="\"worklist\":";      o+=(userHasPerm(g_session.user,L"worklist")?"true":"false");  o+=",";
        o+="\"cashier\":";       o+=(userHasPerm(g_session.user,L"cashier")?"true":"false");   o+=",";
        o+="\"cashier_view\":";  o+=(userHasPerm(g_session.user,L"cashier_view")?"true":"false"); o+=",";
        o+="\"cashier_edit\":";  o+=(userHasPerm(g_session.user,L"cashier_edit")?"true":"false"); o+=",";
        o+="\"settings\":";      o+=(userHasPerm(g_session.user,L"settings")?"true":"false");
        o+="},";
        o+="\"role\":"+jnum(g_session.user.role)+",";
        // v1.93: fold the performer directory + recent admission queue into
        // init so the portal surface renders without two extra round-trips
        // (doctor.performers + queue.list). Same wire format as those verbs.
        o+="\"performers\":"+performersJson()+",";
        o+="\"queue\":"+adQueueJson(true,24)+",";
        o+="\"user\":"+jstr(g_session.user.fullname.empty()?g_session.user.username:g_session.user.fullname);
        o+="}";
        return o;
    }

    // ---- clock poll (JS ticks locally; this keeps Jalali date fresh) ----
    if(verb=="clock"){
        return "{\"time\":"+jstr(adNowTime())+",\"date\":"+jstr(adTodayJalali())+"}";
    }

    // ---- patient.lookup: national-id -> full record (local store first) ----
    //  v1.40.0: build a PatientRow so landline/address/suppIdx round-trip too.
    if(verb=="patient.lookup"){
        std::string nidU; jsonGetString(body,"nid",nidU);
        std::wstring nid=normDigits(u82w(nidU));
        if(nid.empty()) return "{\"found\":false}";
        auto all=loadAllPatients();
        for(const auto& p:all){
            if(normDigits(p.nid)==nid){
                { std::lock_guard<std::mutex> lk(g_adLastNidMx); g_adLastFilledNid=w2u8(nid); }
                return "{\"found\":true,\"patient\":"+patientJson(p)+"}";
            }
        }
        CitizenInfo c=lookupCitizen(nid);
        if(c.found){
            PatientRow p;
            p.nid=nid; p.first=c.firstName; p.last=c.lastName; p.father=c.fatherName;
            p.gender=c.gender; p.birth=c.birthDate; p.mobile=c.mobile;
            p.landline=c.landline; p.address=c.address;   // v1.40.0
            p.insurances=c.insurances; p.suppIdx=c.suppIdx;
            { std::lock_guard<std::mutex> lk(g_adLastNidMx); g_adLastFilledNid=w2u8(nid); }
            return "{\"found\":true,\"patient\":"+patientJson(p)+"}";
        }
        return "{\"found\":false}";
    }

    // ---- patient.search: free-text over the local store ----
    if(verb=="patient.search"){
        std::string qU; jsonGetString(body,"q",qU);
        std::wstring q=u82w(qU);
        std::wstring qd=normDigits(q);
        auto all=loadAllPatients();
        std::string o="{\"rows\":[";
        int n=0;
        for(const auto& p:all){
            std::wstring full=p.first+L" "+p.last;
            bool hit = q.empty() || wcontains(full,q) ||
                       normDigits(p.nid).find(qd)!=std::wstring::npos ||
                       normDigits(p.mobile).find(qd)!=std::wstring::npos;
            if(!hit) continue;
            if(n++) o+=",";
            o+=patientJson(p);   // v1.40.0: struct overload → carries phone/addr/suppIdx
            if(n>=50) break;
        }
        o+="]}"; return o;
    }

    // ---- service.search: over the Management service catalog ----
    if(verb=="service.search"){
        std::string qU; jsonGetString(body,"q",qU);
        std::wstring q=u82w(qU);
        auto svcs=loadServices();
        std::string o="{\"rows\":[";
        int n=0;
        for(const auto& s:svcs){
            if(s.status==0) continue;
            bool hit = q.empty() || wcontains(s.name,q) ||
                       normDigits(s.code).find(normDigits(q))!=std::wstring::npos;
            if(!hit) continue;
            if(n++) o+=",";
            o+=serviceJson(s);
            if(n>=50) break;
        }
        o+="]}"; return o;
    }

    // ---- service.resolve: code -> {name, price} (management-driven) ----
    //  v1.50.0 FREEZE/RACE FIX: findService() keeps a function-local `static`
    //  cache with NO locking. This verb runs on loopback HTTP *worker threads*
    //  while the UI thread can call findService() concurrently (native reception
    //  path) — two threads reassigning the same static vector is a data race
    //  (corrupt heap / hang). Resolve against a LOCAL snapshot instead;
    //  loadServices() allocates its own vector so it is re-entrant.
    if(verb=="service.resolve"){
        std::string codeU; jsonGetString(body,"code",codeU);
        std::wstring want =trim(u82w(codeU));
        std::wstring wantD=normDigits(want);
        auto svcs=loadServices();
        for(const auto& s:svcs){
            if(s.status==0) continue;
            if(s.code==want || s.code==wantD || normDigits(s.code)==wantD)
                return "{\"found\":true,\"service\":"+serviceJson(s)+"}";
        }
        return "{\"found\":false}";
    }

    // ---- doctor.search (also serves doctor-by-code: index+1 acts as code) --
    //  v1.78.0: optional role filter. «پزشک معالج» (ارجاع‌دهنده) جستجو در همهٔ
    //  پزشکان است؛ «انجام دهنده» (کسی که بیمار پیش او می‌رود) فقط پزشکانی که در
    //  مدیریت تیک «تعریف به عنوان انجام دهنده» دارند (role=performer).
    if(verb=="doctor.search"||verb=="doctor.performers"){
        std::string qU; jsonGetString(body,"q",qU);
        std::string codeU; jsonGetString(body,"code",codeU);
        std::string roleU; jsonGetString(body,"role",roleU);
        bool onlyPerformers = (verb=="doctor.performers") || roleU=="performer";
        std::wstring q=u82w(qU);
        std::wstring code=normDigits(u82w(codeU));
        auto docs=loadDoctors();
        std::string o="{\"rows\":[";
        int n=0;
        for(size_t i=0;i<docs.size();++i){
            const auto& d=docs[i];
            if(onlyPerformers && !d.isPerformer) continue;
            if(!onlyPerformers && d.docType==1) continue;
            wchar_t cb[16]; swprintf(cb,16,L"%d",(int)i+1);
            std::wstring dcode=cb;
            bool hit;
            if(!code.empty())
                // v1.74.0: when code is provided (numeric input), search by medical
                // council ID and doc code — NOT just the doctor's list index.
                hit = (dcode==code)
                     || wcontains(d.medicalId,code) || wcontains(d.medicalId,normDigits(code))
                     || wcontains(d.docCode,code) || wcontains(d.docCode,normDigits(code))
                     || wcontains(d.name,code);
            else hit = q.empty() || wcontains(d.name,q) || wcontains(d.specialty,q)
                       || wcontains(d.medicalId,q) || wcontains(d.medicalId,normDigits(q))
                       || wcontains(d.docCode,q) || wcontains(d.docCode,normDigits(q));
            if(!hit) continue;
            if(n++) o+=",";
            o+="{\"name\":"+jstr(d.name)+",\"specialty\":"+jstr(d.specialty)+
               ",\"code\":"+jstr(dcode)+
               // v1.69.0: return medicalId so the JS can display it
               ",\"medicalId\":"+jstr(d.medicalId)+
               // v1.78.0: let the JS badge performers in search results
               ",\"isPerformer\":"+(d.isPerformer?std::string("true"):std::string("false"))+"}";
            // v1.78.0: a pure directory fetch (doctor.performers with no
            // q/code) feeds the FULL performer combo — cap it high (500) so
            // large clinics are not truncated; interactive searches stay at 50.
            int cap = (q.empty() && code.empty()) ? 500 : 50;
            if(n>=cap) break;
        }
        o+="]}"; return o;
    }

    // ---- doctor.stats: today's total + paid reception counts ---------------
    if(verb=="doctor.stats"){
        std::string nameU; jsonGetString(body,"name",nameU);
        return adDoctorStatsJson(u82w(nameU));
    }

    // ---- reception.zoom.save: validate only (session-scoped, NOT persisted) --
    // v2.01 (Part H): zoom is no longer persisted across sessions. This verb
    // remains for API compatibility but does not write to settings.ini.
    if(verb=="reception.zoom.save"){
        double zN=0; int z=0;
        if(jsonGetNumber(body,"zoom",zN)) z=(int)llround(zN);
        if(z<50) z=50; if(z>200) z=200;
        return "{\"ok\":true}";
    }

    // ---- bill.compute: server-authoritative recompute (optional) -----------
    if(verb=="bill.compute"){
        AdBill b=adComputeBill(body);
        std::string o="{";
        o+="\"gross\":"+jnum(b.gross)+",";
        o+="\"disc\":"+jnum(b.disc)+",";
        o+="\"org\":"+jnum(b.org)+",";
        o+="\"supp\":"+jnum(b.supp)+",";
        o+="\"pat\":"+jnum(b.pat)+",";
        o+="\"paid\":"+jnum(b.paid);
        o+="}";
        return o;
    }

    // ---- queue.list: صندوق نرفته‌ها -----------------------------------------
    if(verb=="queue.list"){
        std::string kind; jsonGetString(body,"kind",kind);
        double hours=24; jsonGetNumber(body,"hours",hours);
        return adQueueJson(kind=="admission",(int)hours);
    }

    // ---- queue.add: push current admission into صندوق نرفته‌ها --------------
    if(verb=="queue.add"){
        AdPatient p=adReadPatient(body);
        if(p.nid.empty()) return "{\"ok\":false,\"err\":\"کد ملی خالی است\"}";
        AdBill b=adComputeBill(body);
        std::string kind; jsonGetString(body,"kind",kind);
        bool admissionQueue=kind=="admission";
        auto rows=adQueueLoad(admissionQueue);
        AdQueueRow r;
        wchar_t idb[32]; swprintf(idb,32,L"%lld",adEpochMinutes());
        r.id=idb; r.nid=p.nid; r.first=p.first; r.last=p.last; r.mobile=p.mobile;
        std::string bc; jsonGetString(body,"insBooklet",bc);
        r.barcode=normDigits(u82w(bc)); if(r.barcode.empty()) r.barcode=p.nid;
        r.jdate=adTodayJalali(); r.time=adNowTime();
        r.epochMin=adEpochMinutes(); r.amount=b.pat;
        if(admissionQueue) r.turn=adQueueNextTurn(rows);
        rows.push_back(r);
        adQueueSave(rows,admissionQueue);
        logLine((admissionQueue?L"reception-queue add: ":L"unpaid-queue add: ")+p.nid);
        return "{\"ok\":true}";
    }

    // ---- queue.remove -------------------------------------------------------
    if(verb=="queue.remove"){
        std::string idU; jsonGetString(body,"id",idU);
        std::wstring id=u82w(idU);
        std::string kind; jsonGetString(body,"kind",kind);
        bool admissionQueue=kind=="admission";
        auto rows=adQueueLoad(admissionQueue);
        AdQueueRow found;
        if(!adQueueTake(rows,id,found))
            return "{\"ok\":false,\"err\":\"ردیف پیدا نشد.\"}";
        adQueueSave(rows,admissionQueue);
        return "{\"ok\":true}";
    }
    if(verb=="queue.transfer"){
        std::string idU,fromU,toU;
        jsonGetString(body,"id",idU);
        jsonGetString(body,"from",fromU);
        jsonGetString(body,"to",toU);
        std::wstring id=u82w(idU);
        bool srcAdm=fromU=="admission";
        bool dstAdm=toU=="admission";
        auto src=adQueueLoad(srcAdm);
        AdQueueRow found;
        if(!adQueueTake(src,id,found))
            return "{\"ok\":false,\"err\":\"ردیف پیدا نشد.\"}";
        adQueueSave(src,srcAdm);
        auto dst=adQueueLoad(dstAdm);
        if(dstAdm) found.turn=adQueueNextTurn(dst);
        found.epochMin=adEpochMinutes();
        dst.push_back(found);
        adQueueSave(dst,dstAdm);
        return "{\"ok\":true}";
    }
    if(verb=="queue.pay"){
        std::string idU; jsonGetString(body,"id",idU);
        std::wstring id=u82w(idU);
        auto rows=adQueueLoad(false);
        AdQueueRow found;
        if(!adQueueTake(rows,id,found))
            return "{\"ok\":false,\"err\":\"ردیف پیدا نشد.\"}";
        std::wstring tid=Cash_LookupId(found.nid,found.barcode,true);
        if(tid.empty())
            return "{\"ok\":false,\"err\":\"قبض پیدا نشد.\"}";
        std::wstring err;
        if(!Cash_Pay(tid,err))
            return std::string("{\"ok\":false,\"err\":")+jstr(err)+"}";
        adQueueSave(rows,false);
        return "{\"ok\":true}";
    }
    if(verb=="queue.cancel"){
        std::string idU,reasonU,kindU;
        jsonGetString(body,"id",idU);
        jsonGetString(body,"reason",reasonU);
        jsonGetString(body,"kind",kindU);
        std::wstring id=u82w(idU);
        bool adm=kindU=="admission";
        auto rows=adQueueLoad(adm);
        AdQueueRow found;
        if(!adQueueTake(rows,id,found))
            return "{\"ok\":false,\"err\":\"ردیف پیدا نشد.\"}";
        std::wstring tid=Cash_LookupId(found.nid,found.barcode,true);
        std::wstring err;
        if(tid.empty() || !Receipt_Cancel(tid,u82w(reasonU),err)){
            if(err.empty()) err=L"قبض پیدا نشد.";
            return std::string("{\"ok\":false,\"err\":")+jstr(err)+"}";
        }
        adQueueSave(rows,adm);
        return "{\"ok\":true}";
    }
    if(verb=="receipt.search"){
        ReceiptQuery q;
        std::string t;
        if(jsonGetString(body,"q",t)) q.q=u82w(t);
        if(jsonGetString(body,"from",t)) q.from=u82w(t);
        if(jsonGetString(body,"to",t)) q.to=u82w(t);
        if(jsonGetString(body,"first",t)) q.first=u82w(t);
        if(jsonGetString(body,"last",t)) q.last=u82w(t);
        if(jsonGetString(body,"nid",t)) q.nid=u82w(t);
        if(jsonGetString(body,"mobile",t)) q.mobile=u82w(t);
        if(jsonGetString(body,"fileNo",t)) q.fileNo=u82w(t);
        if(jsonGetString(body,"archive",t)) q.archive=u82w(t);
        if(jsonGetString(body,"barcode",t)) q.barcode=u82w(t);
        if(jsonGetString(body,"doctor",t)) q.doctor=u82w(t);
        double sid=0; jsonGetNumber(body,"sectionId",sid); q.sectionId=(int)sid;
        q.onlyUser=jsonGetBool(body,"onlyUser",false);
        q.byAppt=jsonGetBool(body,"byAppt",false);
        return Receipt_SearchJson(q);
    }
    if(verb=="receipt.sections") return Receipt_SectionsJson();
    if(verb=="receipt.get"){
        std::string idU; jsonGetString(body,"id",idU);
        return Cash_GetJson(u82w(idU));
    }
    if(verb=="receipt.delete"){
        std::string idU; jsonGetString(body,"id",idU);
        std::wstring err;
        std::vector<std::wstring> one; one.push_back(u82w(idU));
        if(!Receipt_DeleteMany(one,err))
            return std::string("{\"ok\":false,\"err\":")+jstr(err)+"}";
        return "{\"ok\":true}";
    }
    if(verb=="receipt.deleteMany"){
        std::string idsU; jsonGetString(body,"ids",idsU);
        std::vector<std::wstring> ids;
        std::wstring cur;
        std::wstring all=u82w(idsU);
        for(wchar_t c:all){
            if(c==L','||c==L'|'){ if(!cur.empty()) ids.push_back(cur); cur.clear(); }
            else cur+=c;
        }
        if(!cur.empty()) ids.push_back(cur);
        std::wstring err;
        if(!Receipt_DeleteMany(ids,err))
            return std::string("{\"ok\":false,\"err\":")+jstr(err)+"}";
        return "{\"ok\":true}";
    }
    if(verb=="receipt.print"){
        std::string idU; jsonGetString(body,"id",idU);
        ReceptionRecord rec;
        if(!Receipt_BuildRecord(u82w(idU),rec))
            return "{\"ok\":false,\"err\":\"قبض پیدا نشد.\"}";
        HWND owner=g_hFrame;
        RunOnUiThreadSync([&](){
            int sid=adResolveSectionId(); bool done=false;
            done=printPrintDesign(rec,sid,owner);
            if(!done) done=printDesignedReceipt(rec,0,owner);
            if(!done) printReceipt(rec,2,owner);
        });
        return "{\"ok\":true}";
    }

    // ---- blacklist.list (v1.94 درمان پلاس) — «لیست سیاه» همه موارد ---------
    //   Returns every blacklist entry (newest first, as Blacklist_Load returns
    //   them) with an "active" flag derived from the expiry epoch (0 == permanent).
    if(verb=="blacklist.list"){
        auto rows=Blacklist_Load();
        std::string o="{\"rows\":[";
        for(size_t i=0;i<rows.size();i++){
            if(i) o+=",";
            auto& b=rows[i];
            o+="{\"nid\":"+jstr(b.nid)+",\"first\":"+jstr(b.first)+",\"last\":"+jstr(b.last);
            o+=",\"reason\":"+jstr(b.reason)+",\"durationLabel\":"+jstr(b.durationLabel);
            o+=",\"createdText\":"+jstr(b.createdText)+",\"createdBy\":"+jstr(b.createdBy);
            long long now=Blacklist_NowEpochMinutes();
            bool active = (b.expiresEpochMin==0 || b.expiresEpochMin>now);
            o+=",\"active\":";
            o+=(active?"true":"false");
            o+="}";
        }
        o+="]}";
        return o;
    }

    // ---- blacklist.add (v1.94 درمان پلاس) — «افزودن به لیست سیاه» ------------
    //   Adds a new blacklist entry from the UI. duration is in minutes (0 ==
    //   permanent). The created timestamp is stamped here in Iran local time;
    //   Blacklist_Add keeps it (it only fills createdText when empty).
    if(verb=="blacklist.add"){
        BlacklistEntry b;
        std::string t;
        if(jsonGetString(body,"nid",t)) b.nid=u82w(t);
        if(jsonGetString(body,"first",t)) b.first=u82w(t);
        if(jsonGetString(body,"last",t)) b.last=u82w(t);
        if(jsonGetString(body,"mobile",t)) b.mobile=u82w(t);
        if(jsonGetString(body,"reason",t)) b.reason=u82w(t);
        if(jsonGetString(body,"durationLabel",t)) b.durationLabel=u82w(t);
        double dur=0; jsonGetNumber(body,"duration",dur);
        long long now=Blacklist_NowEpochMinutes();
        b.createdEpochMin=now;
        b.expiresEpochMin=(dur==0)?0:(now+(long long)dur);
        SYSTEMTIME st=iranNow();
        b.createdText=jalaliDateShort(st)+L" "+iranTimeStr(st,false);
        b.createdBy=g_session.user.username;
        std::wstring err;
        if(Blacklist_Add(b,err)) return "{\"ok\":true}";
        return "{\"ok\":false,\"err\":"+jstr(err)+"}";
    }

    // ---- blacklist.remove (v1.64.0 درمان پلاس) — «رفع مسدودی» ---------------
    //   Permanently deletes every blacklist entry for the given national id so
    //   the patient can be admitted normally from now on.
    if(verb=="blacklist.remove"){
        std::string nidU; jsonGetString(body,"nid",nidU);
        std::wstring nid=normDigits(trim(u82w(nidU)));
        if(nid.empty()) return "{\"ok\":false,\"err\":\"کد ملی خالی است\"}";
        int n=Blacklist_Remove(nid);
        logLine(L"blacklist removed nid="+nid+L" rows="+std::to_wstring(n));
        return n>0 ? "{\"ok\":true,\"removed\":"+jnum(n)+"}"
                   : "{\"ok\":false,\"err\":\"موردی برای این کد ملی یافت نشد\"}";
    }

    // ---- admission.save: persist + PRINT per the Management print design ----
    if(verb=="admission.save"){
        // v1.97: reprint / ticketId → print the existing ticket, no new save.
        {
            std::string tidU;
            if(!jsonGetString(body,"ticketId",tidU) || tidU.empty())
                jsonGetString(body,"reprintId",tidU);
            if(tidU.empty()){
                std::string rp;
                if(jsonGetString(body,"reprint",rp) && !rp.empty() &&
                   rp!="true" && rp!="false" && rp!="1" && rp!="0")
                    tidU=rp;
            }
            if(!tidU.empty()){
                ReceptionRecord rec;
                if(!Receipt_BuildRecord(u82w(tidU),rec))
                    return "{\"ok\":false,\"err\":\"قبض پیدا نشد.\"}";
                HWND owner=g_hFrame;
                int sid=adResolveSectionId();
                std::wstring printMode=L"none";
                RunOnUiThreadSync([&](){
                    bool done=false;
                    done=printPrintDesign(rec,sid,owner);
                    if(done) printMode=L"print-design section="+std::to_wstring(sid);
                    if(!done){
                        done=printDesignedReceipt(rec,0,owner);
                        if(done) printMode=L"designed-receipt idx=0";
                    }
                    if(!done){
                        printReceipt(rec,2,owner);
                        printMode=L"classic-gdi";
                    }
                });
                return std::string("{\"ok\":true,\"reprinted\":true,\"ticketId\":")+
                       jstr(u82w(tidU))+",\"printMode\":"+jstr(printMode)+"}";
            }
        }
        AdPatient p=adReadPatient(body);
        if(p.nid.empty())
            return "{\"ok\":false,\"err\":\"کد ملی خالی است\"}";
        BlacklistEntry blocked;
        bool isBlocked=Blacklist_FindActive(p.nid,blocked);
        bool overrideBlock=jsonGetBool(body,"overrideBlock",false);
        if(isBlocked && !overrideBlock)
            return "{\"ok\":false,\"blocked\":true,\"block\":"+adBlockJson(blocked)+"}";
        if(isBlocked && overrideBlock)
            Blacklist_AuditOverride(blocked,g_session.user.username);

        if(!Shift_IsOpen())
            return "{\"ok\":false,\"needShift\":true,\"err\":\"برای پذیرش بیمار ابتدا باید شروع شیفت را بزنید\"}";
        ReceptionRecord r=adBuildRecord(body);
        if(r.services.empty())
            return "{\"ok\":false,\"err\":\"حداقل یک خدمت لازم است\"}";
        if(r.total<0) r.total=0;

        int q=saveReception(r);   // assigns queue no + persists CSV
        if(r.receiptNo<=0) r.receiptNo = (q>0)? (long long)q : 1;
        if(r.receiptBarcode.empty()){
            long long seed = r.receiptNo>0 ? r.receiptNo : (long long)q;
            long long v = 100000000000LL + ((seed * 2654435761LL) % 899999999999LL);
            if(v<0) v=-v;
            wchar_t bb[24]; swprintf(bb,24,L"%012lld",v);
            r.receiptBarcode=bb;
        }
        std::wstring cashWarn;
        CashTicket cashT;
        bool cashOk=false;
        {
            std::wstring cerr;
            cashOk=Cash_CreateFromReception(r,cerr,cashT);
            if(!cashOk){
                logError(L"admission.save cashier ticket: "+cerr);
                cashWarn=cerr.empty()?L"ثبت بلیت صندوق ناموفق بود.":cerr;
            }
        }
        bool posHas = cashOk ? (cashT.hasPos!=0) : false;
        if(!cashOk){
            CashScope sc=Cash_ResolveScope();
            posHas = (sc.homeSubId>0 && Sections_HasPos(sc.homeSubId)) ||
                     Sections_HasPos(sc.homeSectionId);
        }

        // remember this operator-confirmed identity (never fabricated).
        // v1.40.0: also persist landline «تلفن ثابت», address «آدرس» and the
        // supplementary-insurance index so the same code recalls every field.
        std::vector<int> ins;
        if(r.insIdx>0 && r.insIdx<N_INSURANCES) ins.push_back(r.insIdx);
        rememberPatient(r.nationalId,r.firstName,r.lastName,r.fatherName,
            r.gender,r.birthDate,r.mobile,
            r.landline,r.address,   // v1.40.0
            ins,r.suppIdx);         // v1.40.0
        saveLastReceipt(r);

        // PRINT per the Management «دیزاین چاپ» template, same priority order
        // reception.cpp uses: 1) new print-design bound to the operator SECTION
        //                     2) legacy per-section designed receipt (idx 0)
        //                     3) classic GDI receipt
        // v1.40.0 (B5): track WHICH path actually printed and report it to JS so
        // the operator is warned when the classic fallback template was used.
        //
        // v1.49.0 FREEZE FIX: this handler runs on a loopback /api WORKER thread.
        // The print pipeline opens modal dialogs (PrintDlgW / MessageBoxW) and
        // touches UI-thread-owned HWNDs, so it MUST execute on the thread that
        // owns g_hFrame. Calling it here directly deadlocked the whole program
        // (the "everything freezes after adding a service + saving" bug). We now
        // marshal the entire print block onto the UI thread and wait for it.
        std::wstring printMode=L"none";
        // v1.97: non-POS tickets go to cashier — skip print, return needsCashier.
        if(posHas){
            HWND owner = g_hFrame;
            int  sid=adResolveSectionId();
            RunOnUiThreadSync([&](){
                bool done=false;
                // v1.65.0: ALWAYS try the services-capable print-design first —
                // even when sid==0 (SectionDesign_Resolve maps sid<=0 to the first
                // builtin, and printPrintDesign now seeds the design store lazily).
                // Previously sid==0 or an unseeded store silently degraded to the
                // legacy label-only receipt, which cannot render the services table.
                done=printPrintDesign(r,sid,owner);
                if(done) printMode=L"print-design section="+std::to_wstring(sid);
                if(!done){
                    done=printDesignedReceipt(r,0,owner);
                    if(done) printMode=L"designed-receipt idx=0";
                }
                if(!done){
                    printReceipt(r,2,owner);
                    printMode=L"classic-gdi";
                }
                kickCashDrawer();
            });
            logLine(L"admission printed via: "+printMode+L"  nid="+r.nationalId);
            if(g_hFrame) PostMessageW(g_hFrame,WM_APP_DESIGN_PUSHED,0,0);
        } else {
            logLine(L"admission saved → cashier (no POS) nid="+r.nationalId);
        }

        std::wstring fullName=r.firstName;
        if(!r.lastName.empty()){
            if(!fullName.empty()) fullName+=L" ";
            fullName+=r.lastName;
        }
        std::string svc=w2u8(cashT.servicesJson);
        if(svc.empty()) svc="[]";
        bool mainRec=false;
        {
            CashScope sc=Cash_ResolveScope();
            if(sc.homeSubId<=0){
                mainRec=true;
                std::vector<Section> all; Sections_All(all);
                for(const auto& s:all){
                    if(s.id==sc.homeSectionId){
                        mainRec = (s.parent_id==0 && Ops_IsReception(s));
                        break;
                    }
                }
            }
        }
        const char* miniKind = "none";
        if(!posHas) miniKind = mainRec ? "mainPay" : "subTicket";
        std::string o="{\"ok\":true,\"queueNo\":"+jnum(q)+
            ",\"printMode\":"+jstr(printMode)+
            ",\"needsCashier\":"; o+=(posHas?"false":"true");
        o+=",\"posHas\":"; o+=(posHas?"true":"false");
        o+=",\"miniKind\":\""; o+=miniKind; o+="\"";
        o+=",\"receiptNo\":"+jnum(r.receiptNo);
        o+=",\"nid\":"+jstr(r.nationalId);
        if(cashOk){
            o+=",\"ticketId\":"+jstr(cashT.id);
            o+=",\"barcode\":"+jstr(cashT.barcode);
            o+=",\"name\":"+jstr(fullName);
            o+=",\"payable\":"+jnum(cashT.payable);
            o+=",\"services\":"+svc;
        }
        o+=",\"ps\":"+adDoctorStatsJson(r.treatingDoctor);
        if(!cashWarn.empty()) o+=",\"cashWarn\":"+jstr(cashWarn);
        o+="}";
        WebAdmission_PushEvent("cashier.changed", Shift_StatusJson());
        WebAdmission_PushEvent("queue.update", adQueueJson(true,24));
        return o;
    }

    if(verb=="admission.clear"||verb=="admission.new"||
       verb=="ui.toggleServices"||verb=="ui.toggleQueue"||
       verb=="ui.settings"){
        return "{\"ok\":true}";
    }
    //  v1.85.0 CRASH FIX: these verbs CREATE / DESTROY WebView2 views. They are
    //  delivered synchronously inside this view's own WebMessageReceived COM
    //  callback, and WV2_CreateView pumps the thread message queue while it
    //  waits. Running that nested pump (or ctrl->Close() on the very view that
    //  is mid-callback) from inside the callback frees/uses dead objects
    //  (0xC0000005 at a page-aligned EIP). Defer every tab mutation to a fresh
    //  WM_APP_UI_TASK turn so the current callback fully unwinds first.
    if(verb=="ui.openTab"){
        std::string kind; jsonGetString(body,"kind",kind);
        if(kind=="cashier" && !userHasPerm(g_session.user, L"cashier_view"))
            return "{\"ok\":false,\"err\":\"perm\"}";
        if(kind=="tools")           RunOnUiThread([](){ Reception_OpenTools(); });
        else if(kind=="cashier")    RunOnUiThread([](){ Reception_OpenCashier(); });
        else if(kind=="queue")      RunOnUiThread([](){ Reception_OpenQueue(); });
        else if(kind=="receipts")   RunOnUiThread([](){ Reception_OpenReceipts(); });
        else if(kind=="portal")     RunOnUiThread([](){ Reception_OpenPortal(); });
        else if(kind=="blacklist")  RunOnUiThread([](){ Reception_OpenBlacklist(); });
        else if(kind=="empty")      RunOnUiThread([](){ Reception_OpenNewTab(); });
        else if(kind=="admission")  RunOnUiThread([](){ Reception_OpenAdmissionNew(); });
        else return "{\"ok\":false,\"err\":\"kind\"}";
        return "{\"ok\":true}";
    }
    if(verb=="ui.openAdmission"){
        std::string idU; jsonGetString(body,"id",idU);
        if(idU.empty()) return "{\"ok\":false}";
        std::string raw=Cash_GetJson(u82w(idU));
        std::string ticket;
        if(!jsonGetRaw(raw,"ticket",ticket) || ticket.empty()) return raw;
        RunOnUiThread([ticket](){ Reception_OpenAdmissionTicket(ticket); });
        return "{\"ok\":true}";
    }
    // ---- ui.openQueuePatient: open a queue row in a NEW پذیرش tab -----------
    //  The queue row id is synthetic (epoch minutes), not a cash-ticket id, so
    //  Cash_GetJson cannot resolve it. Build a minimal identity ticket from the
    //  queue row and open a fresh admission tab (boot clearForm()s first).
    if(verb=="ui.openQueuePatient"){
        std::string idU; jsonGetString(body,"id",idU);
        std::string kind; jsonGetString(body,"kind",kind);
        if(idU.empty()) return "{\"ok\":false}";
        std::wstring id=u82w(idU);
        bool adm = kind=="admission";
        auto rows=adQueueLoad(adm);
        AdQueueRow found; bool have=false;
        for(const auto& r:rows) if(r.id==id){ found=r; have=true; break; }
        if(!have) return "{\"ok\":false,\"err\":\"بیمار در صف پیدا نشد\"}";
        // Prefer the real UNPAID cash ticket (it carries services + amount) so
        // the reopened پذیرش keeps the bill. Queue id is synthetic, so resolve
        // by national id (barcode is often just the nid when the booklet is
        // empty). Cash_LookupId takes g_opsMx itself — call it unlocked.
        std::wstring tid=Cash_LookupId(found.nid,found.barcode,true);
        std::string ticket;
        if(!tid.empty()){
            std::string raw=Cash_GetJson(tid);
            if(jsonGetRaw(raw,"ticket",ticket) && !ticket.empty()){ /* got bill */ }
            else ticket.clear();
        }
        if(ticket.empty()){
            // Fallback: identity-only ticket (new visit — no pending bill yet).
            ticket="{";
            ticket+="\"nid\":"+jstr(found.nid)+",";
            ticket+="\"first\":"+jstr(found.first)+",";
            ticket+="\"last\":"+jstr(found.last)+",";
            ticket+="\"mobile\":"+jstr(found.mobile)+",";
            ticket+="\"barcode\":"+jstr(found.barcode);
            ticket+="}";
        }
        RunOnUiThread([ticket](){ Reception_OpenAdmissionTicket(ticket); });
        return "{\"ok\":true}";
    }
    // v1.89: the reception dashboard's envelope badge — current unseen count.
    if(verb=="portal.unread"){
        int n=unseenMessageCount(g_session.user.username);
        return "{\"ok\":true,\"n\":"+std::to_string(n)+"}";
    }

    if(verb=="ui.closeTab"){
        std::string kind; jsonGetString(body,"kind",kind);
        if(kind=="cashier")      RunOnUiThread([](){ Reception_CloseCashier(); });
        else if(kind=="tools")   RunOnUiThread([](){ Reception_CloseTools(); });
        else if(kind=="queue")   RunOnUiThread([](){ Reception_CloseQueue(); });
        else if(kind=="receipts") RunOnUiThread([](){ Reception_CloseReceipts(); });
        else if(kind=="portal")   RunOnUiThread([](){ Reception_ClosePortal(); });
        else return "{\"ok\":false,\"err\":\"kind\"}";
        return "{\"ok\":true}";
    }

    // ---- print.* : route to the real GDI print pipeline --------------------
    //  v1.49.0 FREEZE FIX: like admission.save, every print path opens a modal
    //  Windows print dialog and touches UI-thread HWNDs, so it must be marshalled
    //  onto the UI thread (running it on this worker thread froze the whole app).
    if(verb.rfind("print.",0)==0){
        HWND owner=g_hFrame;
        if(verb=="print.last"){
            RunOnUiThreadSync([&](){ printLastReceipt(owner); });
            return "{\"ok\":true}";
        }
        ReceptionRecord r;
        bool have=false;
        if(!body.empty() && body!="{}"){ r=adBuildRecord(body); have=(r.total>0)||!r.nationalId.empty(); }
        if(!have) loadLastReceipt(r);
        RunOnUiThreadSync([&](){
            if(verb=="print.insurance"){ printReceipt(r,0,owner); }
            else if(verb=="print.rx"){   printReceipt(r,1,owner); }
            else if(verb=="print.receipt"){
                // v1.65.0: same order as admission.save — services-capable
                // design first (works for sid==0 too), legacy label layout
                // only as an explicit fallback.
                int sid=adResolveSectionId(); bool done=false;
                done=printPrintDesign(r,sid,owner);
                if(!done) done=printDesignedReceipt(r,0,owner);
                if(!done) printReceipt(r,2,owner);
            }
        });
        logLine(L"admission print (web): "+u82w(verb));
        return "{\"ok\":true}";
    }

    if(verb=="rx.electronic"){
        logLine(L"electronic prescription request (web)");
        return "{\"ok\":true}";
    }

    // ---- v1.82.0 cashier page ----------------------------------------------
    if(verb=="cashier.page"){
        std::string qU, stU; jsonGetString(body,"q",qU);
        jsonGetString(body,"status",stU);
        double tab=0; jsonGetNumber(body,"tab",tab);
        return Cash_PageJson(u82w(qU),(int)tab,u82w(stU));
    }
    if(verb=="cashier.get"){
        std::string idU; jsonGetString(body,"id",idU);
        return Cash_GetJson(u82w(idU));
    }
    if(verb=="cashier.pay"){
        std::string idU, methodU; jsonGetString(body,"id",idU);
        jsonGetString(body,"method",methodU);
        double amt=0, disc=0;
        jsonGetNumber(body,"amount",amt);
        jsonGetNumber(body,"discount",disc);
        std::wstring err;
        std::wstring method=u82w(methodU);
        if(method.empty()) method=L"cash";
        if(!Cash_PayEx(u82w(idU),method,(long long)amt,(long long)disc,err))
            return std::string("{\"ok\":false,\"err\":")+jstr(err)+"}";
        WebAdmission_PushEvent("cashier.changed", Shift_StatusJson());
        // v2.00: print ONLY after a completed payment
        {
            ReceptionRecord rec;
            if(Receipt_BuildRecord(u82w(idU), rec)){
                HWND owner=g_hFrame;
                int sid=adResolveSectionId();
                RunOnUiThreadSync([&](){
                    if(!printPrintDesign(rec,sid,owner))
                        if(!printDesignedReceipt(rec,0,owner))
                            printReceipt(rec,2,owner);
                    kickCashDrawer();
                });
            }
        }
        return "{\"ok\":true,\"printed\":true}";
    }
    if(verb=="cashier.manual"){
        std::string nidU,fU,lU,dU; double amt=0;
        jsonGetString(body,"nid",nidU);
        jsonGetString(body,"first",fU);
        jsonGetString(body,"last",lU);
        jsonGetString(body,"doctor",dU);
        jsonGetNumber(body,"amount",amt);
        std::wstring err;
        if(!Cash_Manual(u82w(nidU),u82w(fU),u82w(lU),u82w(dU),(long long)amt,err))
            return std::string("{\"ok\":false,\"err\":")+jstr(err)+"}";
        return "{\"ok\":true}";
    }
    if(verb=="cashier.shift.start"){
        std::wstring err;
        if(!Shift_Start(err))
            return std::string("{\"ok\":false,\"err\":")+jstr(err)+"}";
        std::string st=Shift_StatusJson();
        WebAdmission_PushEvent("shift.changed", st);
        WebAdmission_PushEvent("cashier.changed", st);
        return st;
    }
    if(verb=="cashier.shift.end"){
        std::wstring err;
        if(!Shift_End(err))
            return std::string("{\"ok\":false,\"err\":")+jstr(err)+"}";
        std::string st=Shift_StatusJson();
        WebAdmission_PushEvent("shift.changed", st);
        WebAdmission_PushEvent("cashier.changed", st);
        return st;
    }
    if(verb=="cashier.shift.status"){
        return Shift_StatusJson();
    }
    //  v1.92.0: front-end error bridge. JS reports uncaught errors / load
    //  failures here so they land in the DEDICATED HTML error log (not the
    //  general activity log). Only real bugs/crashes/load failures — never
    //  normal events like "user logged in" or "page opened".
    if(verb=="client.error"){
        std::string msgU,srcU,stkU; double ln=0,col=0;
        jsonGetString(body,"message",msgU);
        jsonGetString(body,"source",srcU);
        jsonGetNumber(body,"line",ln);
        jsonGetNumber(body,"column",col);
        jsonGetString(body,"stack",stkU);
        wchar_t lnB[24], colB[24];
        swprintf(lnB,24,L"%d",(int)ln);
        swprintf(colB,24,L"%d",(int)col);
        std::wstring m = L"JS Error: " + u82w(msgU) + L" at " + u82w(srcU) +
                         L":" + lnB + L":" + colB + L" stack: " + u82w(stkU);
        logHtmlError(m);
        return "{\"ok\":true}";
    }

    // ====================================================================
    //  v1.93: portal (cartable / کارتابل) message verbs.
    //  loadMessages() returns oldest-first (file order); the portal works
    //  newest-first, so every verb reverses once and indexes into that order
    //  — matching seenOneMessage / deleteOneMessage / pinMessage which take
    //  an `indexNewestFirst`. Field names mirror crmMsgJson (web_crm_api.inc).
    // ====================================================================
    if(verb=="portal.messages.list"){
        auto msgs=loadMessages(g_session.user.username);
        std::reverse(msgs.begin(),msgs.end());   // newest-first
        std::string o="{\"rows\":[";
        for(size_t i=0;i<msgs.size();i++){
            const auto& m=msgs[i];
            if(i) o+=",";
            o+="{\"idx\":"+jnum((long long)i)+",";
            o+="\"from\":"+jstr(m.from)+",";
            o+="\"to\":"+jstr(m.to)+",";
            o+="\"text\":"+jstr(m.text)+",";
            o+="\"time\":"+jstr(m.time)+",";
            o+="\"seen\":"; o+=(m.seen?"true":"false"); o+=",";
            o+="\"type\":"+jnum(m.type)+",";
            o+="\"pinned\":"; o+=(m.pinned?"true":"false");
            o+="}";
        }
        o+="],\"unseen\":"+jnum((long long)unseenMessageCount(g_session.user.username))+"}";
        return o;
    }
    if(verb=="portal.messages.seenone"){
        double idx=0; jsonGetNumber(body,"idx",idx);
        seenOneMessage(g_session.user.username,(int)idx);
        // keep the dashboard envelope badge in sync after a read
        WebAdmission_PushEvent("dash.unread","{\"n\":"+std::to_string(unseenMessageCount(g_session.user.username))+"}");
        return "{\"ok\":true}";
    }
    if(verb=="portal.messages.delete"){
        double idx=0; jsonGetNumber(body,"idx",idx);
        deleteOneMessage(g_session.user.username,(int)idx);
        WebAdmission_PushEvent("dash.unread","{\"n\":"+std::to_string(unseenMessageCount(g_session.user.username))+"}");
        return "{\"ok\":true}";
    }
    if(verb=="portal.messages.pin"){
        double idx=0; jsonGetNumber(body,"idx",idx);
        bool pin=jsonGetBool(body,"pin",true);
        pinMessage(g_session.user.username,(int)idx,pin);
        return "{\"ok\":true}";
    }
    if(verb=="portal.messages.save"){
        // archive one inbox message to saved_msgs.dat (no attachment)
        double idx=0; jsonGetNumber(body,"idx",idx);
        auto msgs=loadMessages(g_session.user.username);
        std::reverse(msgs.begin(),msgs.end());
        int i=(int)idx; if(i>=0&&i<(int)msgs.size()){
            const auto& m=msgs[i];
            pushSavedMsg(m.from,g_session.user.username,m.text,m.type,L"");
        }
        return "{\"ok\":true}";
    }
    if(verb=="portal.messages.list_saved"){
        auto msgs=loadSavedMsgs();
        std::reverse(msgs.begin(),msgs.end());
        std::string o="{\"rows\":[";
        for(size_t i=0;i<msgs.size();i++){
            if(i) o+=",";
            const auto& m=msgs[i];
            o+="{\"idx\":"+jnum((long long)i)+",\"from\":"+jstr(m.from)+",\"to\":"+jstr(m.to);
            o+=",\"text\":"+jstr(m.text)+",\"time\":"+jstr(m.time)+",\"type\":"+jnum(m.type)+"}";
        }
        o+="]}";
        return o;
    }
    if(verb=="portal.message.reply"){
        // reply to the original sender (← پاسخ: prefix)
        double idx=0; jsonGetNumber(body,"idx",idx);
        std::string txt; jsonGetString(body,"text",txt);
        auto msgs=loadMessages(g_session.user.username);
        std::reverse(msgs.begin(),msgs.end());
        int i=(int)idx; if(i>=0&&i<(int)msgs.size()){
            pushMessageT(g_session.user.username,msgs[i].from,L"↩ پاسخ: "+u82w(txt),KMSG_NORMAL);
        }
        return "{\"ok\":true}";
    }
    if(verb=="portal.message.print"){
        // render one message to a temp RTL .htm and hand it to the shell
        // "print" verb (pattern copied from reception.cpp CART_BTN_PRINT).
        double idx=0; jsonGetNumber(body,"idx",idx);
        auto msgs=loadMessages(g_session.user.username);
        std::reverse(msgs.begin(),msgs.end());
        int i=(int)idx; if(i>=0&&i<(int)msgs.size()){
            const KMsg& mm=msgs[i];
            std::wstring date,time; portalSplitTime(mm.time,date,time);
            std::wstring sender=mm.from.empty()?std::wstring(L"مدیریت درمانگاه"):mm.from;
            std::wstring recip=(mm.to==L"*")?std::wstring(L"همه کاربران"):mm.to;
            std::wstring html=L"<!doctype html><html dir='rtl' lang='fa'><head>"
                L"<meta charset='utf-8'><title>پیام مدیریت</title>"
                L"<style>body{font-family:Tahoma,Arial,sans-serif;direction:rtl;"
                L"padding:34px;color:#233042;line-height:1.75;max-width:680px;margin:0 auto}"
                L"h2{color:#1f5fd6;border-bottom:2px solid #e2e8f0;padding-bottom:8px;margin-top:0}"
                L".m{margin:6px 0;font-size:15px}.k{color:#64748b;font-weight:bold}"
                L".b{margin-top:18px;padding:16px 18px;background:#f7f9fc;"
                L"border:1px solid #e2e8f0;border-radius:10px;font-size:16px;white-space:pre-wrap}</style></head><body>"
                L"<h2>پیام مدیریت درمانگاه</h2>"
                L"<div class='m'><span class='k'>فرستنده: </span>"+portalHtmlEsc(sender)+L"</div>"
                L"<div class='m'><span class='k'>گیرنده: </span>"+portalHtmlEsc(recip)+L"</div>"
                L"<div class='m'><span class='k'>اولویت: </span>"+portalSevLabel(mm.type)+L"</div>"
                L"<div class='m'><span class='k'>تاریخ: </span>"+toFaDigits(date)+
                L" &nbsp; <span class='k'>ساعت: </span>"+toFaDigits(time)+L"</div>"
                L"<div class='b'>"+portalHtmlEsc(mm.text)+L"</div>"
                L"</body></html>";
            // wstring → UTF-8 (no BOM; meta charset declares utf-8)
            std::string u8;
            int n=WideCharToMultiByte(CP_UTF8,0,html.c_str(),(int)html.size(),NULL,0,NULL,NULL);
            if(n>0){ u8.resize(n); WideCharToMultiByte(CP_UTF8,0,html.c_str(),(int)html.size(),&u8[0],n,NULL,NULL); }
            wchar_t tmpDir[MAX_PATH]; GetTempPathW(MAX_PATH,tmpDir);
            std::wstring path=std::wstring(tmpDir)+L"az_portal_msg.htm";
            // Win32 file write (keeps this TU free of a stdio dependency).
            HANDLE hf=CreateFileW(path.c_str(),GENERIC_WRITE,0,NULL,
                                  CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
            if(hf!=INVALID_HANDLE_VALUE){
                if(!u8.empty()) WriteFile(hf,u8.data(),(DWORD)u8.size(),NULL,NULL);
                CloseHandle(hf);
                if(!u8.empty())
                    ShellExecuteW(NULL,L"print",path.c_str(),NULL,NULL,SW_SHOWNORMAL);
            }
        }
        return "{\"ok\":true}";
    }

    // v1.97: lookup a cashier ticket by nid / barcode / q (or id).
    if(verb=="cashier.lookup"){
        std::string nidU, bcU, qU, idU;
        jsonGetString(body,"nid",nidU);
        jsonGetString(body,"barcode",bcU);
        jsonGetString(body,"q",qU);
        jsonGetString(body,"id",idU);
        bool unpaidOnly=jsonGetBool(body,"unpaidOnly",false);
        std::wstring tid=u82w(idU);
        if(tid.empty()){
            std::wstring nid=u82w(nidU), bc=u82w(bcU);
            if(nid.empty() && bc.empty()){
                std::wstring q=u82w(qU);
                nid=q; bc=q;
            }
            tid=Cash_LookupId(nid,bc,unpaidOnly);
        }
        if(tid.empty()) return "{\"ok\":false,\"err\":\"قبض پیدا نشد.\"}";
        std::string raw=Cash_GetJson(tid);
        if(raw.find("\"ok\":false")!=std::string::npos) return raw;
        if(!raw.empty() && raw[0]=='{')
            return std::string("{\"ok\":true,\"id\":")+jstr(tid)+","+raw.substr(1);
        return raw;
    }

    return "{\"ok\":false,\"err\":\"verb ناشناخته\"}";
}
