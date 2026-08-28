// ============================================================================
//  clinic_ops.cpp — cashier, shifts, calendar, HTML .bak backup (v1.82.0)
// ============================================================================
#include "clinic_ops.h"
#include "web_thread_pool.h"
#include <commdlg.h>
#include <process.h>
#include <mutex>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <map>

// ---------------------------------------------------------------------------
//  small local helpers
// ---------------------------------------------------------------------------
static std::string opsW2u8(const std::wstring& w){
    if(w.empty()) return "";
    int n=WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),NULL,0,NULL,NULL);
    std::string s(n,0);
    WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),&s[0],n,NULL,NULL);
    return s;
}
static std::wstring opsU82w(const std::string& s){
    if(s.empty()) return L"";
    int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),NULL,0);
    std::wstring w(n,0);
    MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),&w[0],n);
    return w;
}
static std::string opsJstr(const std::wstring& w){
    std::string u=opsW2u8(w); std::string o="\"";
    for(unsigned char c: u){
        switch(c){
            case '"': o+="\\\""; break;
            case '\\':o+="\\\\"; break;
            case '\n':o+="\\n"; break;
            case '\r':o+="\\r"; break;
            case '\t':o+="\\t"; break;
            default:
                if(c<0x20){ char b[8]; sprintf(b,"\\u%04x",c); o+=b; }
                else o+=(char)c;
        }
    }
    o+="\""; return o;
}
static std::string opsJnum(long long v){ char b[32]; sprintf(b,"%lld",v); return b; }

static std::vector<std::wstring> opsSplit(const std::wstring& s, wchar_t d){
    std::vector<std::wstring> out; std::wstring cur;
    for(wchar_t c:s){ if(c==d){ out.push_back(cur); cur.clear(); } else cur+=c; }
    out.push_back(cur); return out;
}
static std::wstring opsEscPipe(const std::wstring& s){
    std::wstring o=s;
    for(auto& c:o){ if(c==L'|') c=L'¦'; if(c==L'\n'||c==L'\r') c=L' '; }
    return o;
}
static long long opsEpochMin(){
    SYSTEMTIME st=iranNow();
    FILETIME ft; SystemTimeToFileTime(&st,&ft);
    ULARGE_INTEGER u; u.LowPart=ft.dwLowDateTime; u.HighPart=ft.dwHighDateTime;
    return (long long)(u.QuadPart/600000000ULL);
}
static std::wstring opsTicketsPath(){ return dataDir()+L"\\cashier_tickets.dat"; }
static std::wstring opsShiftsPath(){ return dataDir()+L"\\shifts.dat"; }

static std::wstring opsNormDigits(const std::wstring& in){
    std::wstring o; o.reserve(in.size());
    for(wchar_t c:in){
        if(c>=0x06F0 && c<=0x06F9) o+=(wchar_t)(L'0'+(c-0x06F0));      // ۰-۹
        else if(c>=0x0660 && c<=0x0669) o+=(wchar_t)(L'0'+(c-0x0660)); // ٠-٩
        else o+=c;
    }
    return o;
}
static bool opsNextLine(const std::wstring& all, size_t& pos, std::wstring& line){
    if(pos>=all.size()) return false;
    size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
    line=all.substr(pos,e-pos); pos=e+1;
    while(!line.empty()&&(line.back()==L'\r'||line.back()==L' ')) line.pop_back();
    return true;
}

static std::mutex g_opsMx;
static void shiftAddIncome(long long amount);

bool Ops_IsReception(const Section& s){
    if(s.kind==L"reception") return true;
    if(s.kind.find(L"\u067e\u0630\u06cc\u0631\u0634")!=std::wstring::npos) return true;
    return s.name_fa.find(L"\u067e\u0630\u06cc\u0631\u0634")!=std::wstring::npos;
}

static const Section* opsFind(const std::vector<Section>& all, int id){
    if(id<=0) return nullptr;
    for(const auto& s:all) if(s.id==id) return &s;
    return nullptr;
}
static int opsTopId(const Section& s){ return s.parent_id>0 ? s.parent_id : s.id; }

CashScope Cash_ResolveScope(){
    CashScope sc;
    sc.canView = userHasPerm(g_session.user, L"cashier_view");

    PersonDef pd;
    bool havePerson = personByUsername(g_session.user.username, pd);
    std::vector<Section> all; Sections_All(all);

    if(havePerson){
        int deptId=_wtoi(pd.deptId.c_str());
        int subId=_wtoi(pd.subId.c_str());
        const Section* dept=opsFind(all, deptId);
        const Section* sub=opsFind(all, subId);
        sc.homeSectionId=deptId;
        sc.homeSubId=subId;
        if(dept) sc.homeSectionName=dept->name_fa;
        if(sub)  sc.homeSubName=sub->name_fa;

        bool deptIsTopRec = dept && dept->parent_id==0 && Ops_IsReception(*dept);
        bool subIsRec = sub && Ops_IsReception(*sub);
        bool subParentIsRec=false;
        if(sub && sub->parent_id>0){
            const Section* p=opsFind(all, sub->parent_id);
            if(p && p->parent_id==0 && Ops_IsReception(*p)) subParentIsRec=true;
        }
        if(deptIsTopRec || (subIsRec && subParentIsRec) ||
           (sub && sub->parent_id==0 && Ops_IsReception(*sub))){
            sc.supervisor=true;
        } else if(subIsRec && sub->parent_id>0){
            sc.homeSectionId=sub->parent_id;
            sc.homeSubId=sub->id;
            sc.homeSubName=sub->name_fa;
            const Section* p=opsFind(all, sub->parent_id);
            if(p) sc.homeSectionName=p->name_fa;
        } else if(dept){
            int top=opsTopId(*dept);
            sc.homeSectionId=top;
            const Section* t=opsFind(all, top);
            if(t) sc.homeSectionName=t->name_fa;
            if(dept->parent_id>0){
                sc.homeSubId=dept->id;
                sc.homeSubName=dept->name_fa;
            }
        }
    }

    if(sc.homeSectionId<=0){
        std::wstring dept=g_session.user.dept;
        if(!dept.empty()){
            for(const auto& s:all){
                if(!s.is_active) continue;
                if(s.name_fa==dept || s.code==dept){
                    int top=opsTopId(s);
                    sc.homeSectionId=top;
                    const Section* ts=opsFind(all, top);
                    if(ts) sc.homeSectionName=ts->name_fa;
                    if(s.parent_id>0){ sc.homeSubId=s.id; sc.homeSubName=s.name_fa; }
                    break;
                }
            }
        }
    }

    if(g_session.user.role>=1) sc.supervisor=true;
    return sc;
}

// ---------------------------------------------------------------------------
//  tickets
// ---------------------------------------------------------------------------
static std::vector<CashTicket> cashLoad(){
    std::vector<CashTicket> out;
    std::wstring all=readFileUtf8(opsTicketsPath());
    size_t pos=0; std::wstring line;
    while(opsNextLine(all,pos,line)){
        if(line.empty()) continue;
        auto f=opsSplit(line,L'|');
        if(f.size()<17) continue;
        CashTicket t;
        t.id=f[0]; t.barcode=f[1]; t.nid=f[2]; t.first=f[3]; t.last=f[4];
        t.doctor=f[5];
        t.sectionId=_wtoi(f[6].c_str()); t.sectionName=f[7];
        t.subId=_wtoi(f[8].c_str()); t.subName=f[9];
        t.payable=_wtoi64(f[10].c_str()); t.paid=_wtoi64(f[11].c_str());
        t.paidAt=f[12]; t.user=f[13]; t.jdate=f[14]; t.time=f[15];
        t.epochMin=_wtoi64(f[16].c_str());
        if(f.size()>=18) t.servicesJson=f[17];
        if(f.size()>=19) t.paidUser=f[18];
        if(f.size()>=20) t.mobile=f[19];
        if(f.size()>=21) t.fileNo=f[20];
        if(f.size()>=22) t.archiveNo=f[21];
        if(f.size()>=23) t.insBase=f[22];
        if(f.size()>=24) t.insSupp=f[23];
        if(f.size()>=25) t.receiptNo=f[24];
        if(f.size()>=26) t.apptDate=f[25];
        if(f.size()>=27) t.turn=f[26];
        if(f.size()>=28) t.shift=f[27];
        if(f.size()>=29) t.status=f[28];
        if(f.size()>=30) t.cancelReason=f[29];
        if(f.size()>=31) t.cancelUser=f[30];
        if(f.size()>=32) t.cancelAt=f[31];
        if(f.size()>=33) t.payMethod=f[32];
        if(f.size()>=34) t.cashAmt=_wtoi64(f[33].c_str());
        if(f.size()>=35) t.posAmt=_wtoi64(f[34].c_str());
        if(f.size()>=36) t.discountAmt=_wtoi64(f[35].c_str());
        if(f.size()>=37) t.hasPos=_wtoi(f[36].c_str());
        if(t.status.empty()) t.status = t.paid>0 ? L"paid" : L"unpaid";
        out.push_back(t);
    }
    return out;
}
static bool cashSave(const std::vector<CashTicket>& rows){
    std::wstring all;
    for(const auto& t:rows){
        wchar_t sid[16],suid[16],pay[32],paid[32],ep[32];
        swprintf(sid,16,L"%d",t.sectionId);
        swprintf(suid,16,L"%d",t.subId);
        swprintf(pay,32,L"%lld",t.payable);
        swprintf(paid,32,L"%lld",t.paid);
        swprintf(ep,32,L"%lld",t.epochMin);
        all += opsEscPipe(t.id)+L"|"+opsEscPipe(t.barcode)+L"|"+opsEscPipe(t.nid)+L"|"+
               opsEscPipe(t.first)+L"|"+opsEscPipe(t.last)+L"|"+opsEscPipe(t.doctor)+L"|"+
               sid+L"|"+opsEscPipe(t.sectionName)+L"|"+suid+L"|"+opsEscPipe(t.subName)+L"|"+
               pay+L"|"+paid+L"|"+opsEscPipe(t.paidAt)+L"|"+opsEscPipe(t.user)+L"|"+
               opsEscPipe(t.jdate)+L"|"+opsEscPipe(t.time)+L"|"+ep+L"|"+
               opsEscPipe(t.servicesJson)+L"|"+opsEscPipe(t.paidUser)+L"|"+
               opsEscPipe(t.mobile)+L"|"+opsEscPipe(t.fileNo)+L"|"+opsEscPipe(t.archiveNo)+L"|"+
               opsEscPipe(t.insBase)+L"|"+opsEscPipe(t.insSupp)+L"|"+opsEscPipe(t.receiptNo)+L"|"+
               opsEscPipe(t.apptDate)+L"|"+opsEscPipe(t.turn)+L"|"+opsEscPipe(t.shift)+L"|"+
               opsEscPipe(t.status)+L"|"+opsEscPipe(t.cancelReason)+L"|"+
               opsEscPipe(t.cancelUser)+L"|"+opsEscPipe(t.cancelAt)+L"|"+
               opsEscPipe(t.payMethod)+L"|"+std::to_wstring(t.cashAmt)+L"|"+
               std::to_wstring(t.posAmt)+L"|"+std::to_wstring(t.discountAmt)+L"|"+
               std::to_wstring(t.hasPos)+L"\r\n";
    }
    return writeFileUtf8(opsTicketsPath(), all, false);
}

static std::wstring cashBuildServicesJson(const ReceptionRecord& r){
    std::wstring o=L"[";
    for(size_t i=0;i<r.services.size();++i){
        const auto& s=r.services[i];
        if(i) o+=L",";
        wchar_t pr[32],qty[16],pat[32];
        swprintf(pr,32,L"%lld",s.price);
        swprintf(qty,16,L"%d",s.qty>0?s.qty:1);
        swprintf(pat,32,L"%lld",s.patShare);
        std::wstring nm=opsEscPipe(s.name);
        for(auto& c:nm) if(c==L'"') c=L'\'';
        o += L"{\"code\":\""+opsEscPipe(s.code)+L"\",\"name\":\""+nm+
             L"\",\"qty\":"+qty+L",\"price\":"+pr+L",\"patShare\":"+pat+L"}";
    }
    o+=L"]";
    return o;
}

static void cashFillHome(CashTicket& t){
    CashScope sc=Cash_ResolveScope();
    t.sectionId=sc.homeSectionId;
    t.sectionName=sc.homeSectionName;
    t.subId=sc.homeSubId;
    t.subName=sc.homeSubName;
}

static bool opsNeedCashEdit(std::wstring& err){
    if(userHasPerm(g_session.user, L"cashier_edit")) return true;
    err=L"دسترسی تغییر صندوق ندارید.";
    return false;
}
static void cashStampNow(CashTicket& t, bool paidNow){
    SYSTEMTIME st=iranNow();
    t.user=g_session.user.username;
    t.jdate=jalaliDateShort(st);
    t.time=iranTimeStr(st,false);
    t.epochMin=opsEpochMin();
    if(paidNow){
        t.paidAt=t.jdate+L" "+t.time;
        t.paidUser=g_session.user.username;
    }
}

static std::wstring cashNewId(){
    SYSTEMTIME st=iranNow();
    static volatile LONG seq=0;
    LONG n=InterlockedIncrement(&seq);
    wchar_t buf[48];
    swprintf(buf,48,L"T%04d%02d%02d%02d%02d%02d%04d",
             st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond,(int)(n%10000));
    return buf;
}

bool Cash_CreateFromReception(const ReceptionRecord& r, std::wstring& err){
    CashTicket unused;
    return Cash_CreateFromReception(r, err, unused);
}
bool Cash_CreateFromReception(const ReceptionRecord& r, std::wstring& err, CashTicket& created){
    CashTicket t;
    t.id=cashNewId();
    t.nid=r.nationalId; t.first=r.firstName; t.last=r.lastName;
    if(!r.receiptBarcode.empty()) t.barcode=r.receiptBarcode;
    else {
        long long seed = r.receiptNo>0 ? r.receiptNo
                         : (r.queueNo>0 ? (long long)r.queueNo : 0);
        if(seed<=0){
            unsigned h=2166136261u;
            for(wchar_t c: t.id) h = (h^ (unsigned)c)*16777619u;
            seed = 100000LL + (long long)(h % 899999LL);
        }
        long long v = 100000000000LL + ((seed * 2654435761LL) % 899999999999LL);
        if(v<0) v=-v;
        wchar_t bb[24]; swprintf(bb,24,L"%012lld",v);
        t.barcode=bb;
    }
    t.doctor=r.treatingDoctor;
    cashFillHome(t);
    // v1.97: POS on the subsection first, then the section. No parent walk.
    if(t.subId>0 && Sections_HasPos(t.subId)) t.hasPos=1;
    else if(t.sectionId>0 && Sections_HasPos(t.sectionId)) t.hasPos=1;
    else t.hasPos=0;
    t.payable=r.finalTotal;
    if(t.payable<0) t.payable=0;
    t.servicesJson=cashBuildServicesJson(r);
    t.mobile=r.mobile;
    t.fileNo=r.nationalId;
    if(!r.receiptCode.empty()) t.archiveNo=r.receiptCode;
    else t.archiveNo=r.receiptBarcode;
    t.insBase=r.insurance;
    t.insSupp=r.suppInsurance;
    if(r.receiptNo>0){
        wchar_t rn[24]; swprintf(rn,24,L"%lld",r.receiptNo);
        t.receiptNo=rn;
    } else if(r.queueNo>0){
        wchar_t tb[16]; swprintf(tb,16,L"%d",r.queueNo);
        t.receiptNo=tb;
    }
    if(r.queueNo>0){
        wchar_t tb[16]; swprintf(tb,16,L"%d",r.queueNo);
        t.turn=tb;
    }
    t.shift=r.shift;
    if(t.hasPos){
        t.paid=t.payable;
        t.posAmt=t.payable;
        t.payMethod=L"pos";
        t.status=L"paid";
        cashStampNow(t, true);
    } else {
        t.paid=0;
        t.status=L"unpaid";
        cashStampNow(t, false);
    }
    t.apptDate=r.apptDate.empty()?t.jdate:r.apptDate;
    if(!r.apptTime.empty()) t.time=r.apptTime;
    std::lock_guard<std::mutex> lk(g_opsMx);
    auto rows=cashLoad();
    rows.push_back(t);
    if(!cashSave(rows)){ err=L"نوشتن بلیت صندوق ناموفق بود."; return false; }
    if(t.hasPos && t.paid>0) shiftAddIncome(t.paid);
    created=t;
    return true;
}

// ---------------------------------------------------------------------------
//  shifts
// ---------------------------------------------------------------------------
static std::vector<CashShift> shiftLoad(){
    std::vector<CashShift> out;
    std::wstring all=readFileUtf8(opsShiftsPath());
    size_t pos=0; std::wstring line;
    while(opsNextLine(all,pos,line)){
        if(line.empty()) continue;
        auto f=opsSplit(line,L'|');
        if(f.size()<15) continue;
        CashShift s;
        s.id=f[0]; s.username=f[1]; s.fullname=f[2];
        s.sectionId=_wtoi(f[3].c_str()); s.sectionName=f[4];
        s.subId=_wtoi(f[5].c_str()); s.subName=f[6];
        s.startJalali=f[7]; s.startTime=f[8]; s.endJalali=f[9]; s.endTime=f[10];
        s.income=_wtoi64(f[11].c_str()); s.status=f[12];
        s.startEpoch=_wtoi64(f[13].c_str()); s.endEpoch=_wtoi64(f[14].c_str());
        if(f.size()>=16) s.jdate=f[15];
        if(f.size()>=17) s.carryIncome=_wtoi64(f[16].c_str());
        out.push_back(s);
    }
    return out;
}
static bool shiftSave(const std::vector<CashShift>& rows){
    std::wstring all;
    for(const auto& s:rows){
        wchar_t sid[16],suid[16],inc[32],se[32],ee[32],ci[32];
        swprintf(sid,16,L"%d",s.sectionId);
        swprintf(suid,16,L"%d",s.subId);
        swprintf(inc,32,L"%lld",s.income);
        swprintf(se,32,L"%lld",s.startEpoch);
        swprintf(ee,32,L"%lld",s.endEpoch);
        swprintf(ci,32,L"%lld",s.carryIncome);
        all += opsEscPipe(s.id)+L"|"+opsEscPipe(s.username)+L"|"+opsEscPipe(s.fullname)+L"|"+
               sid+L"|"+opsEscPipe(s.sectionName)+L"|"+suid+L"|"+opsEscPipe(s.subName)+L"|"+
               opsEscPipe(s.startJalali)+L"|"+opsEscPipe(s.startTime)+L"|"+
               opsEscPipe(s.endJalali)+L"|"+opsEscPipe(s.endTime)+L"|"+
               inc+L"|"+opsEscPipe(s.status)+L"|"+se+L"|"+ee+L"|"+
               opsEscPipe(s.jdate)+L"|"+ci+L"\r\n";
    }
    return writeFileUtf8(opsShiftsPath(), all, false);
}
static CashShift* shiftFindOpen(std::vector<CashShift>& rows, const std::wstring& user){
    for(auto& s:rows) if(s.username==user && s.status==L"open") return &s;
    return nullptr;
}
// Most recent shift for a user (by start epoch), regardless of status. Used to
// carry income forward when a new shift opens on the same calendar day.
static CashShift* shiftFindRecent(std::vector<CashShift>& rows, const std::wstring& user){
    CashShift* best=nullptr;
    for(auto& s:rows){
        if(s.username!=user) continue;
        if(!best || s.startEpoch>best->startEpoch) best=&s;
    }
    return best;
}

bool Shift_Start(std::wstring& err){
    if(!opsNeedCashEdit(err)) return false;
    std::lock_guard<std::mutex> lk(g_opsMx);
    auto rows=shiftLoad();
    if(shiftFindOpen(rows, g_session.user.username)){
        err=L"شیفت باز از قبل وجود دارد."; return false;
    }
    CashScope sc=Cash_ResolveScope();
    CashShift s;
    s.id=L"S"+cashNewId();
    s.username=g_session.user.username;
    s.fullname=g_session.user.fullname.empty()?g_session.user.username:g_session.user.fullname;
    s.sectionId=sc.homeSectionId; s.sectionName=sc.homeSectionName;
    s.subId=sc.homeSubId; s.subName=sc.homeSubName;
    SYSTEMTIME st=iranNow();
    std::wstring today=jalaliDateShort(st);
    // Income accumulates within the same calendar day: if the user already had
    // a shift today, carry its income forward instead of starting at 0. Income
    // only resets when the previous shift was on a different (Jalali) day.
    CashShift* prev=shiftFindRecent(rows, g_session.user.username);
    long long carry=0;
    if(prev){
        std::wstring prevDay = prev->jdate.empty()?prev->startJalali:prev->jdate;
        if(prevDay==today) carry=prev->income;
    }
    s.startJalali=today;
    s.startTime=iranTimeStr(st,true);
    s.status=L"open";
    s.startEpoch=opsEpochMin();
    s.jdate=today;
    s.income=carry;
    s.carryIncome=carry;
    rows.push_back(s);
    if(!shiftSave(rows)){ err=L"ذخیره شیفت ناموفق بود."; return false; }
    return true;
}
bool Shift_End(std::wstring& err){
    if(!opsNeedCashEdit(err)) return false;
    std::lock_guard<std::mutex> lk(g_opsMx);
    auto rows=shiftLoad();
    CashShift* cur=shiftFindOpen(rows, g_session.user.username);
    if(!cur){ err=L"شیفت بازی برای بستن نیست."; return false; }
    SYSTEMTIME st=iranNow();
    cur->endJalali=jalaliDateShort(st);
    cur->endTime=iranTimeStr(st,true);
    cur->endEpoch=opsEpochMin();
    // Income is intentionally preserved here (NOT reset) so a new shift opened
    // later the same day can carry it forward. Reset only happens across days,
    // decided in Shift_Start. Backfill the accumulation day for legacy rows.
    if(cur->jdate.empty()) cur->jdate=cur->startJalali;
    cur->status=L"closed";
    if(!shiftSave(rows)){ err=L"ذخیره پایان شیفت ناموفق بود."; return false; }
    return true;
}
static std::string shiftJson(const CashShift& s, bool open){
    std::string o="{";
    o+="\"open\":"; o+=(open?"true":"false"); o+=",";
    o+="\"id\":"+opsJstr(s.id)+",";
    o+="\"username\":"+opsJstr(s.username)+",";
    o+="\"fullname\":"+opsJstr(s.fullname)+",";
    o+="\"sectionId\":"+opsJnum(s.sectionId)+",";
    o+="\"sectionName\":"+opsJstr(s.sectionName)+",";
    o+="\"subId\":"+opsJnum(s.subId)+",";
    o+="\"subName\":"+opsJstr(s.subName)+",";
    o+="\"startJalali\":"+opsJstr(s.startJalali)+",";
    o+="\"startTime\":"+opsJstr(s.startTime)+",";
    o+="\"endJalali\":"+opsJstr(s.endJalali)+",";
    o+="\"endTime\":"+opsJstr(s.endTime)+",";
    o+="\"income\":"+opsJnum(s.income)+",";
    o+="\"jdate\":"+opsJstr(s.jdate)+",";
    o+="\"carryIncome\":"+opsJnum(s.carryIncome)+",";
    o+="\"status\":"+opsJstr(s.status);
    o+="}";
    return o;
}
bool Shift_IsOpen(){
    std::lock_guard<std::mutex> lk(g_opsMx);
    auto rows=shiftLoad();
    return shiftFindOpen(rows, g_session.user.username)!=nullptr;
}
std::string Shift_StatusJson(){
    std::lock_guard<std::mutex> lk(g_opsMx);
    auto rows=shiftLoad();
    CashShift* cur=shiftFindOpen(rows, g_session.user.username);
    if(!cur) return "{\"ok\":true,\"open\":false,\"income\":0}";
    return std::string("{\"ok\":true,\"open\":true,")+ "\"shift\":"+shiftJson(*cur,true)+
           ",\"income\":"+opsJnum(cur->income)+"}";
}

// Caller MUST already hold g_opsMx (non-recursive mutex).
static void shiftAddIncome(long long amount){
    if(amount<=0) return;
    auto srows=shiftLoad();
    CashShift* cur=shiftFindOpen(srows, g_session.user.username);
    if(!cur) return;
    cur->income += amount;
    shiftSave(srows);
}

static CashTicket* cashFind(std::vector<CashTicket>& rows, const std::wstring& id){
    for(auto& r:rows) if(r.id==id) return &r;
    return nullptr;
}

static long long cashRemain(const CashTicket& t){
    long long r=t.payable - t.paid - t.discountAmt;
    return r>0?r:0;
}
static std::wstring opsLowerAscii(const std::wstring& s){
    std::wstring o=s;
    for(auto& c:o) if(c>=L'A'&&c<=L'Z') c=(wchar_t)(c-L'A'+L'a');
    return o;
}
bool Cash_PayEx(const std::wstring& id, const std::wstring& method,
                long long amount, long long discount, std::wstring& err){
    if(!opsNeedCashEdit(err)) return false;
    std::wstring m=opsLowerAscii(method);
    if(m.empty()) m=L"cash";
    if(m!=L"cash" && m!=L"pos" && m!=L"free" && m!=L"discount" && m!=L"test"){
        err=L"روش پرداخت نامعتبر است."; return false;
    }
    bool asTest = (m==L"test");
    if(m==L"test") m=L"cash";
    std::lock_guard<std::mutex> lk(g_opsMx);
    auto rows=cashLoad();
    CashTicket* t=cashFind(rows,id);
    if(!t){ err=L"بلیت پیدا نشد."; return false; }
    if(t->status==L"cancelled"){ err=L"این بلیت لغو شده است."; return false; }
    if(discount<0) discount=0;
    if(amount<0) amount=0;
    long long remain=cashRemain(*t);
    if(remain<=0){ err=L"این بلیت قبلاً صندوق شده است."; return false; }
    if(discount>0){
        t->discountAmt += discount;
        if(t->discountAmt > t->payable) t->discountAmt=t->payable;
        remain=cashRemain(*t);
    }
    long long took=0;
    if(m==L"free"){
        t->discountAmt = t->payable - t->paid;
        if(t->discountAmt<0) t->discountAmt=0;
        t->payMethod=L"free";
        remain=0;
    } else if(m==L"discount"){
        if(discount<=0){ err=L"مبلغ تخفیف نامعتبر است."; return false; }
        t->payMethod=L"discount";
        remain=cashRemain(*t);
    } else if(remain>0){
        long long take = amount>0 ? amount : remain;
        if(take>remain) take=remain;
        if(asTest){ t->cashAmt += take; t->payMethod=L"test"; }
        else if(m==L"pos"){ t->posAmt += take; t->payMethod=L"pos"; }
        else { t->cashAmt += take; t->payMethod=L"cash"; }
        t->paid += take;
        took=take;
        remain=cashRemain(*t);
    } else if(discount>0){
        t->payMethod=L"discount";
    }
    if(remain<=0){
        t->status=L"paid";
        SYSTEMTIME st=iranNow();
        t->paidAt = jalaliDateShort(st)+L" "+iranTimeStr(st,false);
        t->paidUser = g_session.user.username;
    } else if(t->status.empty() || t->status==L"unpaid"){
        t->status=L"unpaid";
    }
    if(!cashSave(rows)){ err=L"ذخیره پرداخت ناموفق بود."; return false; }
    if(took>0) shiftAddIncome(took);
    return true;
}
bool Cash_Pay(const std::wstring& id, std::wstring& err){
    return Cash_PayEx(id, L"cash", 0, 0, err);
}

bool Cash_Manual(const std::wstring& nid, const std::wstring& first,
                 const std::wstring& last, const std::wstring& doctor,
                 long long amount, std::wstring& err){
    if(!opsNeedCashEdit(err)) return false;
    if(amount<=0){ err=L"مبلغ نامعتبر است."; return false; }
    CashTicket t;
    t.id=cashNewId();
    t.barcode=nid;
    t.nid=nid; t.first=first; t.last=last; t.doctor=doctor;
    cashFillHome(t);
    t.payable=amount; t.paid=amount;
    t.cashAmt=amount; t.payMethod=L"cash"; t.status=L"paid"; t.hasPos=0;
    cashStampNow(t, true);
    t.servicesJson=L"[]";
    std::lock_guard<std::mutex> lk(g_opsMx);
    auto rows=cashLoad();
    rows.push_back(t);
    if(!cashSave(rows)){ err=L"ذخیره سند دستی ناموفق بود."; return false; }
    shiftAddIncome(amount);
    return true;
}

static bool cashHit(const CashTicket& t, const std::wstring& q){
    if(q.empty()) return true;
    std::wstring hay=opsNormDigits(t.barcode+L" "+t.nid+L" "+t.first+L" "+t.last+L" "+
                     t.doctor+L" "+t.sectionName+L" "+t.subName);
    return hay.find(q)!=std::wstring::npos;
}

static int cashQueueCount(){
    std::wstring all=readFileUtf8(dataDir()+L"\\recept_queue_web.dat");
    int n=0; size_t pos=0; std::wstring line;
    while(opsNextLine(all,pos,line)) if(!line.empty()) n++;
    return n;
}

static std::string ticketRowJson(const CashTicket& t, const std::string& extra=""){
    std::wstring full=t.first;
    if(!t.last.empty()){ if(!full.empty()) full+=L" "; full+=t.last; }
    std::string o="{";
    o+="\"id\":"+opsJstr(t.id)+",";
    o+="\"barcode\":"+opsJstr(t.barcode)+",";
    o+="\"nid\":"+opsJstr(t.nid)+",";
    o+="\"first\":"+opsJstr(t.first)+",";
    o+="\"last\":"+opsJstr(t.last)+",";
    o+="\"name\":"+opsJstr(full)+",";
    o+="\"doctor\":"+opsJstr(t.doctor)+",";
    o+="\"sectionId\":"+opsJnum(t.sectionId)+",";
    o+="\"section\":"+opsJstr(t.sectionName)+",";
    o+="\"subId\":"+opsJnum(t.subId)+",";
    o+="\"sub\":"+opsJstr(t.subName)+",";
    o+="\"payable\":"+opsJnum(t.payable)+",";
    o+="\"paid\":"+opsJnum(t.paid)+",";
    o+="\"time\":"+opsJstr(t.time)+",";
    o+="\"date\":"+opsJstr(t.jdate)+",";
    o+="\"mobile\":"+opsJstr(t.mobile)+",";
    o+="\"fileNo\":"+opsJstr(t.fileNo)+",";
    o+="\"archiveNo\":"+opsJstr(t.archiveNo)+",";
    o+="\"insBase\":"+opsJstr(t.insBase)+",";
    o+="\"insSupp\":"+opsJstr(t.insSupp)+",";
    o+="\"receiptNo\":"+opsJstr(t.receiptNo)+",";
    o+="\"apptDate\":"+opsJstr(t.apptDate)+",";
    o+="\"turn\":"+opsJstr(t.turn)+",";
    o+="\"shift\":"+opsJstr(t.shift)+",";
    o+="\"status\":"+opsJstr(t.status)+",";
    o+="\"cancelReason\":"+opsJstr(t.cancelReason)+",";
    o+="\"cancelUser\":"+opsJstr(t.cancelUser)+",";
    o+="\"cancelAt\":"+opsJstr(t.cancelAt)+",";
    o+="\"payMethod\":"+opsJstr(t.payMethod)+",";
    o+="\"cashAmt\":"+opsJnum(t.cashAmt)+",";
    o+="\"posAmt\":"+opsJnum(t.posAmt)+",";
    o+="\"discountAmt\":"+opsJnum(t.discountAmt)+",";
    o+="\"remain\":"+opsJnum(cashRemain(t))+",";
    o+="\"hasPos\":"; o+=(t.hasPos?"true":"false"); o+=",";
    o+="\"user\":"+opsJstr(t.user);
    if(!extra.empty()){ o+=","; o+=extra; }
    o+="}";
    return o;
}

static bool cashStatusEq(const CashTicket& t, const std::wstring& want){
    if(want.empty()) return true;
    std::wstring st=opsLowerAscii(t.status);
    if(st==want) return true;
    if(want==L"refund"   && (st==L"cancelled" || t.status.find(L"\u0627\u0633\u062a\u0631\u062f\u0627\u062f")!=std::wstring::npos)) return true;
    if(want==L"waiting"){
        if(st==L"waiting" || t.status.find(L"\u0627\u0646\u062a\u0638\u0627\u0631")!=std::wstring::npos) return true;
        return cashRemain(t)>0 && st!=L"cancelled" && st!=L"paid" && st!=L"refund";
    }
    if(want==L"debtor"){
        if(t.status.find(L"\u0628\u062f\u0647\u06a9\u0627\u0631")!=std::wstring::npos) return true;
        return t.paid>0 && cashRemain(t)>0;
    }
    if(want==L"creditor"){
        if(t.status.find(L"\u0628\u0633\u062a\u0627\u0646\u06a9\u0627\u0631")!=std::wstring::npos) return true;
        return t.paid > t.payable && t.payable>=0;
    }
    return false;
}

std::string Cash_PageJson(const std::wstring& q, int tabSectionId,
                          const std::wstring& statusFilter){
    CashScope sc=Cash_ResolveScope();
    if(!sc.canView)
        return "{\"ok\":false,\"err\":\"دسترسی مشاهده صندوق ندارید.\"}";
    std::wstring qn=opsNormDigits(q);
    std::wstring sf=opsLowerAscii(statusFilter);
    bool statusTab = (sf==L"refund"||sf==L"waiting"||sf==L"debtor"||sf==L"creditor");
    std::lock_guard<std::mutex> lk(g_opsMx);
    auto rows=cashLoad();
    long long now=opsEpochMin();
    long long win=now-24*60;
    std::vector<Section> secs; Sections_All(secs);

    std::string tabs="[{\"id\":0,\"name\":\"\u0635\u0646\u062f\u0648\u0642 \u0646\u0631\u0641\u062a\u0647\u200c\u0647\u0627\",\"kind\":\"unpaid\"}";
    if(sc.supervisor){
        for(const auto& s:secs){
            if(!s.is_active || s.has_pos) continue;
            if(s.parent_id==0 && !s.cashier_tab) continue;
            if(s.parent_id!=0 && !s.cashier_tab) continue;
            if(s.parent_id!=0){
                bool parentCash=false;
                for(const auto& p:secs) if(p.id==s.parent_id && p.cashier_tab) parentCash=true;
                if(parentCash) continue;
            }
            tabs+=",{\"id\":"+opsJnum(s.id)+",\"name\":"+opsJstr(s.name_fa)+",\"kind\":\"section\"}";
        }
    } else if(sc.homeSectionId>0 && !Sections_HasPos(sc.homeSectionId)){
        tabs+=",{\"id\":"+opsJnum(sc.homeSectionId)+",\"name\":"+opsJstr(sc.homeSectionName)+",\"kind\":\"section\"}";
    }
    tabs+="]";

    int patients=0, paidN=0, unpaidN=0;
    int cRefund=0, cWait=0, cDebt=0, cCred=0;
    std::string list="[";
    bool first=true;
    for(int i=(int)rows.size()-1;i>=0;--i){
        const CashTicket& t=rows[i];
        if(t.epochMin<win) continue;
        if(!sc.supervisor && sc.homeSectionId>0 && t.sectionId!=sc.homeSectionId)
            continue;
        if(t.hasPos) continue;                 // POS-origin never in cashier
        if(t.status==L"cancelled" && !statusTab) continue;
        if(cashStatusEq(t,L"refund"))   cRefund++;
        else if(cashStatusEq(t,L"waiting"))  cWait++;
        else if(cashStatusEq(t,L"debtor"))   cDebt++;
        else if(cashStatusEq(t,L"creditor")) cCred++;
        bool inTab=false;
        if(statusTab) inTab=cashStatusEq(t,sf);
        else {
            // صندوق = فقط پرداخت‌نشده‌ها (حتی در تب بخش)
            if(cashRemain(t)<=0) continue;
            if(tabSectionId<=0) inTab=true;
            else inTab=(t.sectionId==tabSectionId || t.subId==tabSectionId);
        }
        if(!inTab) continue;
        patients++;
        if(cashRemain(t)<=0) paidN++; else unpaidN++;
        if(!cashHit(t,qn)) continue;
        if(!first) list+=",";
        first=false;
        list+=ticketRowJson(t);
    }
    list+="]";

    auto srows=shiftLoad();
    CashShift* cur=shiftFindOpen(srows, g_session.user.username);
    std::string shiftPart = cur ? shiftJson(*cur,true) : "{\"open\":false,\"income\":0}";
    long long income = cur ? cur->income : 0;

    std::string o="{\"ok\":true,";
    o+="\"tabs\":"+tabs+",";
    o+="\"shift\":"+shiftPart+",";
    o+="\"income\":"+opsJnum(income)+",";
    o+="\"stats\":{";
    o+="\"patients\":"+opsJnum(patients)+",";
    o+="\"paid\":"+opsJnum(paidN)+",";
    o+="\"unpaid\":"+opsJnum(unpaidN)+",";
    o+="\"queue\":"+opsJnum(cashQueueCount());
    o+="},";
    o+="\"statusCounts\":{";
    o+="\"refund\":"+opsJnum(cRefund)+",";
    o+="\"waiting\":"+opsJnum(cWait)+",";
    o+="\"debtor\":"+opsJnum(cDebt)+",";
    o+="\"creditor\":"+opsJnum(cCred);
    o+="},";
    o+="\"rows\":"+list;
    o+="}";
    return o;
}

std::string Cash_GetJson(const std::wstring& id){
    std::lock_guard<std::mutex> lk(g_opsMx);
    auto rows=cashLoad();
    CashTicket* t=cashFind(rows,id);
    if(!t) return "{\"ok\":false,\"err\":\"بلیت پیدا نشد.\"}";
    std::string svc=opsW2u8(t->servicesJson);
    if(svc.empty()) svc="[]";
    std::string extra="\"services\":"+svc+",\"paidAt\":"+opsJstr(t->paidAt)+
        ",\"paidUser\":"+opsJstr(t->paidUser);
    return std::string("{\"ok\":true,\"ticket\":")+ticketRowJson(*t, extra)+"}";
}

std::wstring Cash_LookupId(const std::wstring& nid, const std::wstring& barcode, bool unpaidOnly){
    if(nid.empty() && barcode.empty()) return L"";
    std::lock_guard<std::mutex> lk(g_opsMx);
    auto rows=cashLoad();
    const CashTicket* unpaid=nullptr;
    const CashTicket* any=nullptr;
    const bool nidOnly=barcode.empty() || (!nid.empty() && barcode==nid);
    for(const auto& r:rows){
        if(!nid.empty() && r.nid!=nid) continue;
        if(!nidOnly && r.barcode!=barcode && r.receiptNo!=barcode && r.fileNo!=barcode)
            continue;
        if(!any || r.epochMin>=any->epochMin) any=&r;
        if(r.status!=L"cancelled" && cashRemain(r)>0){
            if(!unpaid || r.epochMin>=unpaid->epochMin) unpaid=&r;
        }
    }
    if(unpaid) return unpaid->id;
    if(unpaidOnly) return L"";
    return any ? any->id : L"";
}

std::string Calendar_ListJson(const std::wstring& fromJalali,
                              const std::wstring& toJalali,
                              const std::wstring& username){
    std::lock_guard<std::mutex> lk(g_opsMx);
    auto rows=shiftLoad();
    std::wstring fromN=opsNormDigits(fromJalali);
    std::wstring toN=opsNormDigits(toJalali);
    std::string o="{\"ok\":true,\"rows\":[";
    bool first=true;
    for(int i=(int)rows.size()-1;i>=0;--i){
        const CashShift& s=rows[i];
        if(!username.empty() && s.username!=username) continue;
        std::wstring day=opsNormDigits(s.startJalali);
        if(!fromN.empty() && day<fromN) continue;
        if(!toN.empty() && day>toN) continue;
        if(!first) o+=",";
        first=false;
        o+=shiftJson(s, s.status==L"open");
    }
    o+="]}";
    return o;
}

static bool recHas(const std::wstring& hay, const std::wstring& needle){
    if(needle.empty()) return true;
    return opsNormDigits(hay).find(opsNormDigits(needle))!=std::wstring::npos;
}

std::string Receipt_SearchJson(const ReceiptQuery& q){
    std::lock_guard<std::mutex> lk(g_opsMx);
    auto rows=cashLoad();
    std::wstring fromN=opsNormDigits(q.from);
    std::wstring toN=opsNormDigits(q.to);
    std::wstring user=g_session.user.username;
    std::string list="[";
    bool first=true;
    int nOut=0;
    for(int i=(int)rows.size()-1;i>=0;--i){
        const CashTicket& t=rows[i];
        std::wstring day=opsNormDigits(t.jdate);
        if(!fromN.empty() && day<fromN) continue;
        if(!toN.empty() && day>toN) continue;
        if(q.sectionId>0 && t.sectionId!=q.sectionId) continue;
        if(q.onlyUser && t.user!=user) continue;
        if(q.byAppt && t.apptDate.empty()) continue;
        if(!recHas(t.first,q.first)) continue;
        if(!recHas(t.last,q.last)) continue;
        if(!recHas(t.nid,q.nid)) continue;
        if(!recHas(t.mobile,q.mobile)) continue;
        if(!recHas(t.fileNo,q.fileNo)) continue;
        if(!recHas(t.archiveNo,q.archive)) continue;
        if(!recHas(t.barcode,q.barcode) && !recHas(t.receiptNo,q.barcode)) continue;
        if(!recHas(t.doctor,q.doctor)) continue;
        if(!q.q.empty()){
            std::wstring hay=t.first+L" "+t.last+L" "+t.nid+L" "+t.barcode+L" "+
                             t.doctor+L" "+t.mobile+L" "+t.fileNo+L" "+t.receiptNo;
            if(!recHas(hay,q.q)) continue;
        }
        if(!first) list+=",";
        first=false;
        list+=ticketRowJson(t);
        nOut++;
        bool noDates = fromN.empty() && toN.empty();
        if(noDates && nOut>=30) break;
    }
    list+="]";
    return std::string("{\"ok\":true,\"rows\":")+list+"}";
}

bool Receipt_DeleteMany(const std::vector<std::wstring>& ids, std::wstring& err){
    if(g_session.user.role<1){ err=L"فقط مدیر می‌تواند قبض را حذف کند."; return false; }
    if(ids.empty()){ err=L"موردی انتخاب نشده."; return false; }
    std::lock_guard<std::mutex> lk(g_opsMx);
    auto rows=cashLoad();
    std::vector<CashTicket> keep;
    for(const auto& t:rows){
        bool drop=false;
        for(const auto& id:ids) if(t.id==id){ drop=true; break; }
        if(!drop) keep.push_back(t);
    }
    if(keep.size()==rows.size()){ err=L"قبض پیدا نشد."; return false; }
    if(!cashSave(keep)){ err=L"حذف گروهی ناموفق بود."; return false; }
    return true;
}

bool Receipt_Cancel(const std::wstring& id, const std::wstring& reason, std::wstring& err){
    std::lock_guard<std::mutex> lk(g_opsMx);
    auto rows=cashLoad();
    CashTicket* t=cashFind(rows,id);
    if(!t){ err=L"قبض پیدا نشد."; return false; }
    SYSTEMTIME st=iranNow();
    t->status=L"cancelled";
    t->cancelReason=reason;
    t->cancelUser=g_session.user.username;
    t->cancelAt=jalaliDateShort(st)+L" "+iranTimeStr(st,false);
    if(!cashSave(rows)){ err=L"لغو قبض ناموفق بود."; return false; }
    return true;
}

bool Receipt_BuildRecord(const std::wstring& id, ReceptionRecord& out){
    std::lock_guard<std::mutex> lk(g_opsMx);
    auto rows=cashLoad();
    CashTicket* t=cashFind(rows,id);
    if(!t) return false;
    out=ReceptionRecord();
    out.firstName=t->first; out.lastName=t->last; out.nationalId=t->nid;
    out.mobile=t->mobile; out.treatingDoctor=t->doctor;
    out.insurance=t->insBase; out.suppInsurance=t->insSupp;
    out.apptDate=t->apptDate.empty()?t->jdate:t->apptDate;
    out.apptTime=t->time;
    out.queueNo=_wtoi(t->turn.c_str());
    if(out.queueNo<=0 && !t->receiptNo.empty()) out.queueNo=_wtoi(t->receiptNo.c_str());
    out.receiptNo=_wtoi64(t->receiptNo.c_str());
    if(out.receiptNo<=0) out.receiptNo=out.queueNo;
    out.shift=t->shift; out.insNo=t->barcode;
    out.receiptBarcode=t->barcode;
    out.receiptCode=t->archiveNo;
    out.finalTotal=t->payable; out.paid=t->paid; out.patientShare=t->payable;
    out.userName=t->user;
    out.regStamp=t->jdate+L" "+t->time;
    std::wstring js=t->servicesJson;
    // parse a tiny [{"code":"..","name":"..","qty":1,"price":0,"patShare":0},...]
    size_t p=0;
    while(true){
        size_t c=js.find(L"\"code\":\"",p); if(c==std::wstring::npos) break;
        c+=8; size_t ce=js.find(L'"',c); if(ce==std::wstring::npos) break;
        ServiceLine sl; sl.code=js.substr(c,ce-c);
        size_t n=js.find(L"\"name\":\"",ce); if(n==std::wstring::npos) break;
        n+=8; size_t ne=js.find(L'"',n); if(ne==std::wstring::npos) break;
        sl.name=js.substr(n,ne-n);
        size_t qpos=js.find(L"\"qty\":",ne);
        if(qpos!=std::wstring::npos) sl.qty=_wtoi(js.c_str()+qpos+6);
        size_t pr=js.find(L"\"price\":",ne);
        if(pr!=std::wstring::npos) sl.price=_wtoi64(js.c_str()+pr+8);
        size_t ps=js.find(L"\"patShare\":",ne);
        if(ps!=std::wstring::npos) sl.patShare=_wtoi64(js.c_str()+ps+11);
        if(sl.qty<1) sl.qty=1;
        out.services.push_back(sl);
        p=ne+1;
    }
    return true;
}

std::string Receipt_SectionsJson(){
    std::vector<Section> secs; Sections_All(secs);
    std::string o="{\"ok\":true,\"rows\":[";
    bool first=true;
    for(const auto& s:secs){
        if(!s.is_active || s.parent_id!=0) continue;
        if(!first) o+=",";
        first=false;
        o+="{\"id\":"+opsJnum(s.id)+",\"name\":"+opsJstr(s.name_fa)+"}";
    }
    o+="]}";
    return o;
}

AccountingStats Accounting_Stats(){
    AccountingStats st;
    SYSTEMTIME now=iranNow();
    st.date=jalaliDateShort(now);
    std::lock_guard<std::mutex> lk(g_opsMx);
    auto rows=cashLoad();
    for(const auto& t:rows){
        if(t.status==L"cancelled") continue;
        bool today = (t.jdate==st.date) ||
                     (t.paidAt.size()>=st.date.size() && t.paidAt.compare(0,st.date.size(),st.date)==0);
        bool refund = cashStatusEq(t, L"refund");
        long long remain=cashRemain(t);
        if(refund){
            if(today){ st.refund++; st.refundAmt += t.paid>0?t.paid:t.payable; }
            continue;
        }
        if(!t.hasPos && remain>0){
            st.unpaid++;
            st.unpaidAmt += remain;
        }
        if(today && t.paid>0){
            st.income += t.paid;
            if(!t.hasPos){
                st.cashed++;
                st.cashedAmt += t.paid;
            }
        }
    }
    return st;
}
std::vector<CashTicket> Accounting_Recent(int limit){
    if(limit<=0) limit=30;
    std::lock_guard<std::mutex> lk(g_opsMx);
    auto rows=cashLoad();
    std::vector<CashTicket> out;
    for(int i=(int)rows.size()-1;i>=0 && (int)out.size()<limit;--i){
        if(rows[i].status==L"cancelled") continue;
        out.push_back(rows[i]);
    }
    return out;
}

// ---------------------------------------------------------------------------
//  HTML backup — AZTBKP01, independent of the native modal workers
// ---------------------------------------------------------------------------
struct OpsBkProg {
    bool busy;
    int pct;
    std::wstring status, err;
    OpsBkProg():busy(false),pct(0){}
};
static std::mutex g_bkMx;
static OpsBkProg g_bk;

static void bkLogErr(const std::wstring& s){
    SYSTEMTIME st=iranNow();
    wchar_t pre[64];
    swprintf(pre,64,L"[%04d-%02d-%02d %02d:%02d:%02d] ",
             st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
    writeFileUtf8(logsDir()+L"\\backup_errors.log", std::wstring(pre)+s+L"\r\n", true);
    logError(L"backup: "+s);
}
static void bkSet(int pct, const std::wstring& st, const std::wstring& err=L""){
    std::lock_guard<std::mutex> lk(g_bkMx);
    g_bk.pct=pct; g_bk.status=st;
    if(!err.empty()) g_bk.err=err;
}
static void bkBusy(bool on){
    std::lock_guard<std::mutex> lk(g_bkMx);
    g_bk.busy=on;
    if(on){ g_bk.pct=0; g_bk.err.clear(); }
}

static void opsEnumFiles(const std::wstring& dir,
                         std::vector<std::wstring>& out,
                         std::vector<long long>& sizes){
    std::wstring pat=dir+L"\\*";
    WIN32_FIND_DATAW fd; HANDLE h=FindFirstFileW(pat.c_str(),&fd);
    if(h==INVALID_HANDLE_VALUE) return;
    do{
        if(fd.cFileName[0]==L'.') continue;
        std::wstring full=dir+L"\\"+fd.cFileName;
        if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            opsEnumFiles(full,out,sizes);
        else {
            long long sz=((long long)fd.nFileSizeHigh<<32)|fd.nFileSizeLow;
            out.push_back(full); sizes.push_back(sz);
        }
    } while(FindNextFileW(h,&fd));
    FindClose(h);
}

static const wchar_t* opsTableOf(const std::wstring& rel){
    std::wstring n=rel; for(auto& c:n) c=towlower(c);
    if(n.find(L"cashier")!=std::wstring::npos || n.find(L"unpaid")!=std::wstring::npos ||
       n.find(L"shift")!=std::wstring::npos || n.find(L"recept_queue")!=std::wstring::npos)
        return L"\u0635\u0646\u062f\u0648\u0642";
    if(n.find(L"section")!=std::wstring::npos)
        return L"\u0628\u062e\u0634\u200c\u0647\u0627 \u0648 \u0632\u06cc\u0631\u0628\u062e\u0634\u200c\u0647\u0627";
    if(n.find(L"person")!=std::wstring::npos || n.find(L"emp_")!=std::wstring::npos)
        return L"\u067e\u0631\u0633\u0646\u0644";
    if(n.find(L"service")!=std::wstring::npos || n.find(L"tariff")!=std::wstring::npos)
        return L"\u062e\u062f\u0645\u0627\u062a \u0648 \u062a\u0639\u0631\u0641\u0647";
    if(n.find(L"user")!=std::wstring::npos)
        return L"\u06a9\u0627\u0631\u0628\u0631\u0627\u0646 \u0648 \u062f\u0633\u062a\u0631\u0633\u06cc";
    if(n.find(L"message")!=std::wstring::npos || n.find(L"kartabl")!=std::wstring::npos)
        return L"\u06a9\u0627\u0631\u062a\u0627\u0628\u0644";
    if(n.find(L"patient")!=std::wstring::npos || n.find(L"reception")!=std::wstring::npos)
        return L"\u067e\u0631\u0648\u0646\u062f\u0647 \u0628\u06cc\u0645\u0627\u0631";
    if(n.find(L"design")!=std::wstring::npos || n.find(L"print")!=std::wstring::npos)
        return L"\u0637\u0631\u062d\u200c\u0647\u0627\u06cc \u0686\u0627\u067e";
    if(n.find(L"setting")!=std::wstring::npos || n.find(L"perm")!=std::wstring::npos)
        return L"\u062a\u0646\u0638\u06cc\u0645\u0627\u062a";
    return L"\u0633\u0627\u06cc\u0631";
}

static void opsEnsureParent(const std::wstring& path){
    size_t sp=path.find_last_of(L"\\/");
    if(sp==std::wstring::npos || sp==0) return;
    std::wstring dir=path.substr(0,sp);
    opsEnsureParent(dir);
    CreateDirectoryW(dir.c_str(), NULL);
}

static const unsigned char OPS_K[16]={
    0xA3,0x5C,0x19,0xE7,0x42,0xB8,0x6D,0x0F,0x91,0x2A,0xC4,0x77,0x3E,0xD1,0x58,0x8B
};
static void opsMix(unsigned char* p, size_t n, unsigned long long off){
    for(size_t i=0;i<n;i++){
        unsigned char k=OPS_K[(off+i)&15];
        p[i]=(unsigned char)(p[i]^k^(unsigned char)((off+i)*13u));
    }
}
static bool g_bkPacked=true;

static bool opsBkOpen(const std::wstring& path, HANDLE& hf, std::wstring& err, long long* pos){
    hf=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,
                   OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(hf==INVALID_HANDLE_VALUE){
        err=L"باز کردن فایل پشتیبان ناموفق بود.";
        return false;
    }
    DWORD rd=0; char magic[9]={0};
    ReadFile(hf,magic,9,&rd,NULL);
    if(!(rd==9 && strncmp(magic,"AZTBKP01\n",9)==0)){
        CloseHandle(hf); hf=INVALID_HANDLE_VALUE;
        err=L"قالب فایل پشتیبان معتبر نیست.";
        return false;
    }
    auto firstOk=[&](bool mix)->bool{
        SetFilePointer(hf,9,NULL,FILE_BEGIN);
        std::string line;
        char c; DWORD r1=0;
        unsigned long long off=0;
        while(true){
            if(!ReadFile(hf,&c,1,&r1,NULL)||!r1) return false;
            if(mix){
                unsigned char k=OPS_K[off&15];
                c=(char)((unsigned char)c^k^(unsigned char)(off*13u));
            }
            off++;
            if(c=='\n') break;
            if(c!='\r') line+=c;
            if(line.size()>512) return false;
        }
        if(line.empty()||line=="END") return false;
        size_t tab=line.find('\t');
        if(tab==std::string::npos||tab==0) return false;
        return _atoi64(line.c_str()+tab+1)>=0;
    };
    bool mixOk=firstOk(true);
    bool rawOk=mixOk?false:firstOk(false);
    if(!mixOk && !rawOk){
        CloseHandle(hf); hf=INVALID_HANDLE_VALUE;
        err=L"قالب فایل پشتیبان معتبر نیست.";
        return false;
    }
    g_bkPacked=mixOk;
    SetFilePointer(hf,9,NULL,FILE_BEGIN);
    if(pos) *pos=9;
    return true;
}
static bool opsBkRead(HANDLE hf, void* buf, DWORD n, DWORD* rd, long long* pos){
    if(!ReadFile(hf,buf,n,rd,NULL)||!*rd) return false;
    if(g_bkPacked){
        unsigned long long off=pos? (unsigned long long)(*pos-9) : 0;
        opsMix((unsigned char*)buf,*rd,off);
    }
    if(pos) *pos+=*rd;
    return true;
}
static bool opsBkReadLine(HANDLE hf, std::string& line, long long* pos){
    line.clear();
    char c; DWORD r1=0;
    while(true){
        if(!opsBkRead(hf,&c,1,&r1,pos)) return false;
        if(c=='\n') return true;
        line+=c;
    }
}
static bool opsBkParseEntry(const std::string& line, std::wstring& rel, long long& fsz){
    if(line.empty()||line=="END") return false;
    size_t tab=line.find('\t'); if(tab==std::string::npos) return false;
    rel=opsU82w(line.substr(0,tab));
    fsz=_atoi64(line.substr(tab+1).c_str());
    return true;
}
static void opsBkSkipBody(HANDLE hf, long long fsz, std::vector<char>& buf, long long* pos){
    long long left=fsz; DWORD rd=0;
    while(left>0){
        DWORD chunk=(DWORD)((left<(long long)buf.size())?left:buf.size());
        if(!opsBkRead(hf,buf.data(),chunk,&rd,pos)) break;
        left-=rd;
    }
}

struct OpsBkJob { std::wstring path; int kind; }; // 1=create 2=restore

static unsigned __stdcall opsBkWorker(void* p){
    OpsBkJob* job=(OpsBkJob*)p;
    std::wstring path=job->path; int kind=job->kind; delete job;
    bkBusy(true);
    if(kind==1){
        std::wstring dir=dataDir();
        std::vector<std::wstring> files; std::vector<long long> sizes;
        opsEnumFiles(dir,files,sizes);
        long long total=0; for(auto s:sizes) total+=s; if(total<1) total=1;
        HANDLE out=CreateFileW(path.c_str(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL,NULL);
        if(out==INVALID_HANDLE_VALUE){
            std::wstring e=L"ایجاد فایل پشتیبان ناموفق بود.";
            bkSet(0,e,e); bkLogErr(e+L" "+path); bkBusy(false); return 1;
        }
        DWORD wr=0;
        { const char* hdr="AZTBKP01\n"; WriteFile(out,hdr,9,&wr,NULL); }
        std::vector<char> buf(1<<20);
        long long done=0;
        unsigned long long packOff=0;
        auto packWrite=[&](const void* p, DWORD n){
            if(!n) return;
            std::vector<unsigned char> tmp((const unsigned char*)p,(const unsigned char*)p+n);
            opsMix(tmp.data(),n,packOff);
            packOff+=n;
            WriteFile(out,tmp.data(),n,&wr,NULL);
        };
        for(size_t i=0;i<files.size();i++){
            std::wstring rel=files[i].substr(dir.size()+1);
            std::string relU=opsW2u8(rel);
            char line[1600];
            int ln=_snprintf(line,sizeof(line),"%s\t%lld\n",relU.c_str(),sizes[i]);
            packWrite(line,(DWORD)ln);
            HANDLE in=CreateFileW(files[i].c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,
                                  OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
            if(in!=INVALID_HANDLE_VALUE){
                DWORD rd=0;
                while(ReadFile(in,buf.data(),(DWORD)buf.size(),&rd,NULL) && rd>0){
                    packWrite(buf.data(),rd);
                    done+=rd;
                    bkSet((int)(done*100/total), L"در حال نوشتن: "+rel);
                }
                CloseHandle(in);
            } else {
                bkLogErr(L"خواندن ناموفق: "+rel);
            }
        }
        { const char end[]="END\n"; packWrite(end,4); }
        CloseHandle(out);
        bkSet(100, L"پشتیبان‌گیری با موفقیت کامل شد.");
        bkBusy(false);
        return 0;
    }
    // restore
    std::wstring dir=dataDir();
    HANDLE hf=INVALID_HANDLE_VALUE; std::wstring openErr; long long pos=0;
    if(!opsBkOpen(path,hf,openErr,&pos)){
        bkSet(0,openErr,openErr); bkLogErr(openErr+L" "+path); bkBusy(false); return 1;
    }
    LARGE_INTEGER fsz={0}; GetFileSizeEx(hf,&fsz);
    long long total=fsz.QuadPart>0?fsz.QuadPart:1;
    DWORD rd=0;
    std::vector<char> buf(1<<20);
    int restored=0;
    for(;;){
        std::string line;
        if(!opsBkReadLine(hf,line,&pos)) break;
        std::wstring rel; long long fsz2=0;
        if(!opsBkParseEntry(line,rel,fsz2)) break;
        if(rel.find(L"..")!=std::wstring::npos){
            opsBkSkipBody(hf,fsz2,buf,&pos);
            bkLogErr(L"مسیر ناامن رد شد: "+rel);
            continue;
        }
        std::wstring outPath=dir+L"\\"+rel;
        opsEnsureParent(outPath);
        HANDLE out=CreateFileW(outPath.c_str(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL,NULL);
        if(out!=INVALID_HANDLE_VALUE) restored++;
        else bkLogErr(L"نوشتن ناموفق: "+rel);
        long long left=fsz2;
        while(left>0){
            DWORD chunk=(DWORD)((left<(long long)buf.size())?left:buf.size());
            if(!opsBkRead(hf,buf.data(),chunk,&rd,&pos)) break;
            if(out!=INVALID_HANDLE_VALUE){ DWORD wr=0; WriteFile(out,buf.data(),rd,&wr,NULL); }
            left-=rd;
            bkSet((int)(pos*100/total), L"در حال بازیابی: "+rel);
        }
        if(out!=INVALID_HANDLE_VALUE) CloseHandle(out);
    }
    CloseHandle(hf);
    wchar_t msg[80];
    swprintf(msg,80,L"بازیابی کامل شد — %d پرونده نوشته شد.", restored);
    bkSet(100, msg);
    bkBusy(false);
    return 0;
}

static bool bkStart(const std::wstring& path, int kind, std::wstring& err){
    {
        std::lock_guard<std::mutex> lk(g_bkMx);
        if(g_bk.busy){ err=L"یک عملیات پشتیبان در حال اجراست."; return false; }
        g_bk.busy=true; g_bk.pct=0; g_bk.err.clear();
        g_bk.status= kind==1 ? L"شروع پشتیبان‌گیری…" : L"شروع بازیابی…";
    }
    OpsBkJob* job=new OpsBkJob(); job->path=path; job->kind=kind;
    uintptr_t th=_beginthreadex(NULL,0,opsBkWorker,job,0,NULL);
    if(!th){
        delete job;
        bkBusy(false);
        err=L"ایجاد رشته پشتیبان ناموفق بود.";
        bkLogErr(err);
        return false;
    }
    CloseHandle((HANDLE)th);
    return true;
}

std::string OpsBackup_Create(const std::wstring& destPath){
    if(destPath.empty()) return "{\"ok\":false,\"err\":\"مسیر خالی است.\"}";
    std::wstring err;
    if(!bkStart(destPath,1,err))
        return std::string("{\"ok\":false,\"err\":")+opsJstr(err)+"}";
    return "{\"ok\":true,\"started\":true}";
}
std::string OpsBackup_Restore(const std::wstring& srcPath){
    if(srcPath.empty()) return "{\"ok\":false,\"err\":\"مسیر خالی است.\"}";
    std::wstring err;
    if(!bkStart(srcPath,2,err))
        return std::string("{\"ok\":false,\"err\":")+opsJstr(err)+"}";
    return "{\"ok\":true,\"started\":true}";
}
std::string OpsBackup_ProgressJson(){
    std::lock_guard<std::mutex> lk(g_bkMx);
    std::string o="{";
    o+="\"busy\":"; o+=(g_bk.busy?"true":"false"); o+=",";
    o+="\"pct\":"+opsJnum(g_bk.pct)+",";
    o+="\"status\":"+opsJstr(g_bk.status)+",";
    o+="\"err\":"+opsJstr(g_bk.err);
    o+="}";
    return o;
}

std::string OpsBackup_Analyze(const std::wstring& srcPath){
    if(srcPath.empty()) return "{\"ok\":false,\"err\":\"مسیر خالی است.\"}";
    HANDLE hf=INVALID_HANDLE_VALUE; std::wstring openErr;
    if(!opsBkOpen(srcPath,hf,openErr,nullptr))
        return std::string("{\"ok\":false,\"err\":")+opsJstr(openErr)+"}";
    struct Tab { std::wstring name; int files; long long bytes; };
    std::vector<Tab> tabs;
    int nfiles=0;
    std::vector<char> skip(1<<20);
    for(;;){
        std::string line;
        if(!opsBkReadLine(hf,line,nullptr)) break;
        std::wstring rel; long long fsz=0;
        if(!opsBkParseEntry(line,rel,fsz)) break;
        const wchar_t* tn=opsTableOf(rel);
        bool found=false;
        for(auto& t:tabs) if(t.name==tn){ t.files++; t.bytes+=fsz; found=true; break; }
        if(!found){ Tab t; t.name=tn; t.files=1; t.bytes=fsz; tabs.push_back(t); }
        nfiles++;
        opsBkSkipBody(hf,fsz,skip,nullptr);
    }
    CloseHandle(hf);
    std::string tables="[";
    for(size_t i=0;i<tabs.size();++i){
        if(i) tables+=",";
        tables+="{\"name\":"+opsJstr(tabs[i].name)+",\"files\":"+opsJnum(tabs[i].files)+
                ",\"bytes\":"+opsJnum(tabs[i].bytes)+"}";
    }
    tables+="]";
    return std::string("{\"ok\":true,\"magic\":\"AZTBKP01\",\"files\":")+opsJnum(nfiles)+
           ",\"tables\":"+tables+"}";
}

static std::string opsPickBak(bool save){
    std::wstring picked;
    RunOnUiThreadSync([&](){
        wchar_t file[MAX_PATH];
        if(save){
            SYSTEMTIME st=iranNow();
            swprintf(file,MAX_PATH,L"darmanplus-%04d%02d%02d.bak",st.wYear,st.wMonth,st.wDay);
        } else file[0]=0;
        OPENFILENAMEW ofn; ZeroMemory(&ofn,sizeof(ofn));
        ofn.lStructSize=sizeof(ofn);
        ofn.hwndOwner=g_hFrame;
        ofn.lpstrFilter=L"\u067e\u0634\u062a\u06cc\u0628\u0627\u0646 \u062f\u0631\u0645\u0627\u0646\u200c\u067e\u0644\u0627\u0633 (*.bak)\0*.bak\0\u0647\u0645\u0647 \u067e\u0631\u0648\u0646\u062f\u0647\u200c\u0647\u0627 (*.*)\0*.*\0";
        ofn.lpstrFile=file;
        ofn.nMaxFile=MAX_PATH;
        ofn.lpstrDefExt=L"bak";
        ofn.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
        ofn.lpstrTitle=save?L"\u0630\u062e\u06cc\u0631\u0647 \u067e\u0634\u062a\u06cc\u0628\u0627\u0646"
                           :L"\u0628\u0627\u0632 \u06a9\u0631\u062f\u0646 \u067e\u0634\u062a\u06cc\u0628\u0627\u0646";
        if(save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn)) picked=file;
    });
    if(picked.empty()) return "{\"ok\":false,\"cancelled\":true}";
    return std::string("{\"ok\":true,\"path\":")+opsJstr(picked)+"}";
}
std::string OpsBackup_PickSave(){ return opsPickBak(true); }
std::string OpsBackup_PickOpen(){ return opsPickBak(false); }

// ===========================================================================
//  v2.02 — «تفکیک خدمات» per-service breakdown report
// ===========================================================================

// Parse a ticket's servicesJson into {code, name} pairs.
struct SvcLineLite { std::wstring code, name; };
static std::vector<SvcLineLite> parseTicketServices(const std::wstring& js){
    std::vector<SvcLineLite> out;
    if(js.empty() || js==L"[]") return out;
    size_t p=0;
    while(true){
        size_t c=js.find(L"\"code\":\"",p); if(c==std::wstring::npos) break;
        c+=8; size_t ce=js.find(L'"',c); if(ce==std::wstring::npos) break;
        SvcLineLite s; s.code=js.substr(c,ce-c);
        size_t n=js.find(L"\"name\":\"",ce);
        if(n!=std::wstring::npos && n<ce+80){
            n+=8; size_t ne=js.find(L'"',n);
            if(ne!=std::wstring::npos){ s.name=js.substr(n,ne-n); p=ne+1; }
            else { s.name=s.code; p=ce+1; }
        } else {
            s.name=s.code; p=ce+1;
        }
        if(s.name.empty()) s.name=s.code;
        out.push_back(s);
    }
    return out;
}

std::string SvReport_Json(const SvReportQuery& q){
    std::lock_guard<std::mutex> lk(g_opsMx);
    auto tickets=cashLoad();

    struct ReportRow {
        int seq;
        std::wstring name, nid, insBase, insSupp, insCombo;
        bool paid;
    };
    struct ReportBlock {
        std::wstring serviceName, serviceCode;
        std::vector<ReportRow> rows;
        int paidCount=0;
    };

    std::vector<ReportBlock> blocks;
    std::map<std::wstring,int> blockIdx;

    auto inDateRange=[&](const std::wstring& d)->bool{
        if(d.empty()) return false;
        if(!q.fromJalali.empty() && d<q.fromJalali) return false;
        if(!q.toJalali.empty() && d>q.toJalali) return false;
        return true;
    };

    auto shiftMatches=[&](const std::wstring& tShift)->bool{
        if(q.shiftId<0) return true;
        int sid=shiftIdByStoredName(tShift);
        if(sid>=0) return sid==q.shiftId;
        return tShift==shiftDisplayName(q.shiftId);
    };

    int totalRecords=0;

    for(const auto& t : tickets){
        if(t.status==L"cancelled") continue;
        std::wstring d=t.jdate.empty()?t.apptDate:t.jdate;
        if(!inDateRange(d)) continue;
        if(!shiftMatches(t.shift)) continue;
        if(q.sectionId>0){
            if(t.sectionId!=q.sectionId && t.subId!=q.sectionId) continue;
        }
        if(q.subId>0 && t.subId!=q.subId) continue;
        if(!q.doctorName.empty() && t.doctor!=q.doctorName) continue;
        bool isPaid = (t.paid>0) || t.status==L"paid" || t.payMethod==L"free"
                   || t.payMethod==L"cash" || t.payMethod==L"pos"
                   || (!t.paidAt.empty() && t.paidAt!=L"0");
        if(q.payStatus==0 && isPaid) continue;
        if(q.payStatus==1 && !isPaid) continue;

        auto svcs=parseTicketServices(t.servicesJson);
        for(const auto& s : svcs){
            if(!q.serviceCode.empty() && s.code!=q.serviceCode) continue;

            auto it=blockIdx.find(s.code);
            if(it==blockIdx.end()){
                ReportBlock blk;
                blk.serviceName=s.name;
                blk.serviceCode=s.code;
                blocks.push_back(blk);
                blockIdx[s.code]=(int)blocks.size()-1;
                it=blockIdx.find(s.code);
            }
            ReportBlock& blk=blocks[it->second];
            if(!s.name.empty()) blk.serviceName=s.name;

            std::wstring insBase=t.insBase.empty()?L"آزاد":t.insBase;
            std::wstring combo;
            if(t.insSupp.empty() || t.insSupp==L"ندارد"){
                combo=insBase;
            } else {
                combo=insBase+L"/"+t.insSupp;
            }

            ReportRow row;
            row.seq=_wtoi(t.turn.c_str());
            if(row.seq<=0) row.seq=t.receiptNo.empty()?0:_wtoi(t.receiptNo.c_str());
            row.name=t.first+L" "+t.last;
            row.nid=t.nid;
            row.insBase=insBase;
            row.insSupp=t.insSupp;
            row.insCombo=combo;
            row.paid=isPaid;

            blk.rows.push_back(row);
            if(isPaid) blk.paidCount++;
            totalRecords++;
        }
    }

    std::string o="{\"ok\":true,";
    o+="\"totalRecords\":"+opsJnum(totalRecords)+",";
    o+="\"blocks\":[";
    for(size_t i=0;i<blocks.size();++i){
        const auto& blk=blocks[i];
        if(i) o+=",";
        o+="{\"serviceName\":"+opsJstr(blk.serviceName)+",";
        o+="\"serviceCode\":"+opsJstr(blk.serviceCode)+",";
        o+="\"total\":"+opsJnum((long long)blk.rows.size())+",";
        o+="\"paidCount\":"+opsJnum((long long)blk.paidCount)+",";
        o+="\"rows\":[";
        for(size_t j=0;j<blk.rows.size();++j){
            const auto& r=blk.rows[j];
            if(j) o+=",";
            o+="{\"seq\":"+opsJnum((long long)r.seq)+",";
            o+="\"name\":"+opsJstr(r.name)+",";
            o+="\"nid\":"+opsJstr(r.nid)+",";
            o+="\"insBase\":"+opsJstr(r.insBase)+",";
            o+="\"insSupp\":"+opsJstr(r.insSupp)+",";
            o+="\"insCombo\":"+opsJstr(r.insCombo)+",";
            o+="\"paid\":"+std::string(r.paid?"true":"false")+"}";
        }
        o+="]}";
    }
    o+="],";
    SYSTEMTIME st=iranNow();
    std::wstring shiftLabel=shiftDisplayName(g_session.shift);
    std::wstring timeStr=iranTimeStr(st,false);
    std::wstring dateStr=jalaliDateShort(st);
    o+="\"footer\":{\"shift\":"+opsJstr(shiftLabel)+",";
    o+="\"time\":"+opsJstr(timeStr)+",";
    o+="\"date\":"+opsJstr(dateStr)+",";
    o+="\"user\":"+opsJstr(g_session.user.fullname)+"}";
    o+="}";
    return o;
}
