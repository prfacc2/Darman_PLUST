// ============================================================================
//  web_crm_api.inc — the C++ side of the CRM management IPC bridge (v1.70.0).
//  Included into web_crm.cpp (after the UTF/JSON helpers). Every crm.* verb the
//  HTML management UI can send is handled here against the REAL data layer
//  (sections, services, doctors, patients, users/employees, messages, settings
//  + backup). Returns a JSON string. The UI never fabricates identities or
//  prices — every save round-trips through the authoritative C++ stores, so the
//  on-disk file formats (sections.dat, services.dat, doctors.dat, patients.dat,
//  users.dat, …) are preserved exactly.
// ============================================================================
#include "sections.h"

// ----------------------------------------------------------------------------
//  JSON serializers for each management entity (mirror the admission style).
// ----------------------------------------------------------------------------
static std::string crmSectionJson(const Section& s){
    std::string o="{";
    o+="\"id\":"+jnum(s.id)+",";
    o+="\"code\":"+jstr(s.code)+",";
    o+="\"name\":"+jstr(s.name_fa)+",";
    o+="\"kind\":"+jstr(s.kind)+",";
    o+="\"kindLabel\":"+jstr(Sections_KindLabel(s.kind))+",";
    o+="\"parentId\":"+jnum(s.parent_id)+",";
    o+="\"active\":";
    o+=(s.is_active?"true":"false");
    o+=",\"createdAt\":"+jstr(s.created_at)+",";
    o+="\"updatedAt\":"+jstr(s.updated_at)+"";
    o+="}";
    return o;
}

static std::string crmServiceJson(const ServiceDef& s){
    std::string o="{";
    o+="\"code\":"+jstr(s.code)+",";
    o+="\"name\":"+jstr(s.name)+",";
    o+="\"category\":"+jstr(s.category)+",";
    o+="\"dept\":"+jstr(s.dept)+",";
    o+="\"price\":"+jnum(s.price)+",";
    o+="\"insType\":"+jstr(s.insType)+",";
    o+="\"desc\":"+jstr(s.desc)+",";
    o+="\"status\":"+jnum(s.status)+",";
    o+="\"created\":"+jstr(s.created)+",";
    o+="\"modified\":"+jstr(s.modified)+",";
    // v1.74 professional tariffs
    o+="\"insName\":"+jstr(s.insName)+",";
    o+="\"multiplier\":"+jstr(s.multiplier)+",";
    o+="\"priceFree\":"+jnum(s.priceFree)+",";
    o+="\"priceFreeNew\":"+jnum(s.priceFreeNew)+",";
    o+="\"priceGov\":"+jnum(s.priceGov)+",";
    o+="\"priceGovNew\":"+jnum(s.priceGovNew)+",";
    o+="\"priceIns\":"+jnum(s.priceIns)+",";
    o+="\"priceInsNew\":"+jnum(s.priceInsNew)+"";
    o+="}";
    return o;
}

static std::string crmDoctorJson(const DoctorDef& d, int idx){
    std::string o="{";
    o+="\"idx\":"+jnum(idx)+",";
    o+="\"name\":"+jstr(d.name)+",";
    o+="\"specialty\":"+jstr(d.specialty)+",";
    o+="\"deptId\":"+jstr(d.deptId)+",";
    o+="\"docType\":"+jnum(d.docType)+",";
    o+="\"docCode\":"+jstr(d.docCode)+",";
    o+="\"active\":"; o+=(d.active?"true":"false"); o+=",";
    // v1.78.0: «تعریف به عنوان انجام دهنده» flag for the admission performer list
    o+="\"isPerformer\":"; o+=(d.isPerformer?"true":"false"); o+=",";
    o+="\"medicalId\":"+jstr(d.medicalId)+",";
    o+="\"namePrefix\":"+jstr(d.namePrefix)+",";
    o+="\"firstName\":"+jstr(d.firstName)+",";
    o+="\"lastName\":"+jstr(d.lastName)+",";
    o+="\"nationalId\":"+jstr(d.nationalId)+",";
    o+="\"mobile\":"+jstr(d.mobile)+",";
    o+="\"email\":"+jstr(d.email)+",";
    o+="\"address\":"+jstr(d.address)+",";
    o+="\"franchise\":"+jstr(d.franchise)+",";
    o+="\"printOnReceipt\":"; o+=(d.printOnReceipt?"true":"false"); o+=",";
    o+="\"insSpecialty\":"+jstr(d.insSpecialty)+",";
    o+="\"degree\":"+jstr(d.degree)+",";
    o+="\"contractType\":"+jnum(d.contractType)+",";
    o+="\"emergencyContract\":"+jstr(d.emergencyContract)+",";
    o+="\"accounting\":"+jstr(d.accounting)+",";
    o+="\"services\":[";
    for(size_t i=0;i<d.services.size();++i){ if(i)o+=","; o+=jstr(d.services[i]); }
    o+="]";
    o+="}";
    return o;
}

static std::string crmPatientJson(const PatientRow& p){
    std::string o="{";
    o+="\"nid\":"+jstr(p.nid)+",";
    o+="\"first\":"+jstr(p.first)+",";
    o+="\"last\":"+jstr(p.last)+",";
    o+="\"father\":"+jstr(p.father)+",";
    o+="\"gender\":"+jstr(p.gender)+",";
    o+="\"birth\":"+jstr(p.birth)+",";
    o+="\"mobile\":"+jstr(p.mobile)+",";
    o+="\"phone\":"+jstr(p.landline)+",";
    o+="\"addr\":"+jstr(p.address)+",";
    o+="\"suppIdx\":"+jnum(p.suppIdx)+",";
    o+="\"insurances\":[";
    for(size_t i=0;i<p.insurances.size();++i){ if(i)o+=","; o+=jnum(p.insurances[i]); }
    o+="]";
    o+="}";
    return o;
}

static std::string crmUserJson(const User& u){
    std::string o="{";
    o+="\"id\":"+jnum(u.id)+",";
    o+="\"username\":"+jstr(u.username)+",";
    o+="\"fullname\":"+jstr(u.fullname)+",";
    o+="\"dept\":"+jstr(u.dept)+",";
    o+="\"role\":"+jnum(u.role)+",";
    // v1.79.0: permission keys (empty = full access) + linked personnel info
    o+="\"perms\":"+jstr(u.perms)+",";
    PersonDef _pu;
    if(personByUsername(u.username,_pu)){
        o+="\"personCode\":"+jstr(_pu.code)+",";
        o+="\"position\":"+jstr(_pu.position.empty()?personRoleLabel(_pu):_pu.position)+",";
        o+="\"hasPhoto\":"; o+=(_pu.photo.empty()?"false":"true"); o+=",";
    }
    o+="\"online\":"; o+=(isUserOnline(u.username)?"true":"false"); o+="";
    o+="}";
    return o;
}

// ---- v1.79.0: personnel registry JSON -------------------------------------
//  v1.80.0: deptId/subId reference the CLINICAL sections tree (sections.dat);
//  the caller hands the tree in once per verb call (no per-row disk reads).
static std::string crmPersonJson(const PersonDef& p, const std::vector<Section>& ss){
    std::string o="{";
    o+="\"code\":"+jstr(p.code)+",";
    o+="\"firstName\":"+jstr(p.firstName)+",";
    o+="\"lastName\":"+jstr(p.lastName)+",";
    o+="\"fatherName\":"+jstr(p.fatherName)+",";
    o+="\"nationalId\":"+jstr(p.nationalId)+",";
    o+="\"birthDate\":"+jstr(p.birthDate)+",";
    o+="\"address\":"+jstr(p.address)+",";
    o+="\"mobile\":"+jstr(p.mobile)+",";
    o+="\"phone\":"+jstr(p.phone)+",";
    o+="\"email\":"+jstr(p.email)+",";
    o+="\"education\":"+jstr(p.education)+",";
    o+="\"field\":"+jstr(p.field)+",";
    o+="\"degree\":"+jstr(p.degree)+",";
    o+="\"roleKind\":"+jnum(p.roleKind)+",";
    o+="\"roleCustom\":"+jstr(p.roleCustom)+",";
    o+="\"roleLabel\":"+jstr(personRoleLabel(p))+",";
    o+="\"position\":"+jstr(p.position)+",";
    o+="\"deptId\":"+jstr(p.deptId)+",";
    // section/subsection NAMES for display (the clinical tree is id-keyed)
    {
        std::wstring sn, subn;
        int wantD=_wtoi(p.deptId.c_str()), wantS=_wtoi(p.subId.c_str());
        for(auto& s2:ss){
            if(s2.id==wantD) sn=s2.name_fa;
            if(s2.id==wantS) subn=s2.name_fa;
        }
        o+="\"deptName\":"+jstr(sn)+",";
        o+="\"subName\":"+jstr(subn)+",";
    }
    o+="\"subId\":"+jstr(p.subId)+",";
    o+="\"suspended\":"; o+=(p.deptId.empty()?"true":"false"); o+=",";
    o+="\"username\":"+jstr(p.username)+",";
    o+="\"hasPhoto\":"; o+=(p.photo.empty()?"false":"true"); o+=",";
    o+="\"created\":"+jstr(p.created);
    o+="}";
    return o;
}

// ---- tiny base64 DECODE (photo upload) -------------------------------------
static int b64val(char c){
    if(c>='A'&&c<='Z') return c-'A';
    if(c>='a'&&c<='z') return c-'a'+26;
    if(c>='0'&&c<='9') return c-'0'+52;
    if(c=='+') return 62; if(c=='/') return 63;
    return -1;
}
static std::string b64decode(const std::string& in){
    std::string out; unsigned int val=0; int bits=-8;
    for(char c:in){
        if(c==' '||c=='\t'||c=='\r'||c=='\n') continue;   // tolerate wrapped input
        int v=b64val(c); if(v<0) break;                    // '=' or end
        val=(val<<6)+(unsigned)v; bits+=6;
        if(bits>=0){ out+=(char)((val>>bits)&0xFFu); bits-=8; }
    }
    return out;
}
//  strip a "data:image/png;base64," prefix if present
static std::string b64payload(const std::string& dataUrl, std::wstring& ext){
    ext=L".png";
    size_t comma=dataUrl.find(',');
    if(comma==std::string::npos) return dataUrl;
    std::string head=dataUrl.substr(0,comma);
    if(head.find("jpeg")!=std::string::npos||head.find("jpg")!=std::string::npos)
        ext=L".jpg";
    return dataUrl.substr(comma+1);
}

static std::string crmDeptJson(const DeptCat& c){
    std::string o="{";
    o+="\"id\":"+jstr(c.id)+",";
    o+="\"name\":"+jstr(c.name)+",";
    o+="\"manager\":"+jstr(c.manager)+",";
    o+="\"icon\":"+jstr(c.icon)+"";
    o+="}";
    return o;
}

static std::string crmMsgJson(const KMsg& m, int idx){
    std::string o="{";
    o+="\"idx\":"+jnum(idx)+",";
    o+="\"from\":"+jstr(m.from)+",";
    o+="\"to\":"+jstr(m.to)+",";
    o+="\"text\":"+jstr(m.text)+",";
    o+="\"time\":"+jstr(m.time)+",";
    o+="\"seen\":"; o+=(m.seen?"true":"false"); o+=",";
    o+="\"type\":"+jnum(m.type)+",";
    o+="\"pinned\":"; o+=(m.pinned?"true":"false"); o+="";
    o+="}";
    return o;
}

// base + supplementary insurance lists (for the patient form selectors).
// v1.74: the canonical hardcoded lists are replaced by crmInsListJson /
// crmSuppListJson (merged with the user-managed registry) — see below.

// v1.74 — merged insurance/supplementary lists: the canonical hardcoded table,
// overridden per-index by any user definition (name + org share), and extended
// with user-added rows. Keeps idx 0..N-1 stable so existing patient records keep
// their meaning while user-defined سهم سازمان / names flow everywhere.
static std::string crmInsListJson(){
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
        o+="{\"idx\":"+jnum(i)+",\"name\":"+jstr(name)+",\"pct\":"+jnum(pct)+"}";
    }
    o+="]"; return o;
}
static std::string crmSuppListJson(){
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
        o+="{\"idx\":"+jnum(i)+",\"name\":"+jstr(name)+",\"pct\":"+jnum(pct)+"}";
    }
    o+="]"; return o;
}
static std::string crmInsDefJson(const InsDef& d){
    std::string o="{";
    o+="\"idx\":"+jnum(d.idx)+",";
    o+="\"sectionCode\":"+jstr(d.sectionCode)+",";
    o+="\"insCode\":"+jstr(d.insCode)+",";
    o+="\"orgShare\":"+jnum(d.orgShare)+",";
    o+="\"groupName\":"+jstr(d.groupName)+",";
    o+="\"flipName\":"+jstr(d.flipName)+",";
    o+="\"contractCode\":"+jstr(d.contractCode)+",";
    o+="\"insType\":"+jstr(d.insType)+",";
    o+="\"tech\":"+jnum(d.tech)+",";
    o+="\"prof\":"+jnum(d.prof)+",";
    o+="\"cons\":"+jnum(d.cons)+",";
    o+="\"total\":"+jnum(d.tech+d.prof+d.cons)+",";
    o+="\"active\":"; o+=(d.active?"true":"false"); o+=",";
    o+="\"created\":"+jstr(d.created)+",";
    o+="\"modified\":"+jstr(d.modified)+"";
    o+="}";
    return o;
}
static std::string crmSuppDefJson(const SuppDef& d){
    std::string o="{";
    o+="\"idx\":"+jnum(d.idx)+",";
    o+="\"sectionCode\":"+jstr(d.sectionCode)+",";
    o+="\"insSpec\":"+jstr(d.insSpec)+",";
    o+="\"name\":"+jstr(d.name)+",";
    o+="\"tariffType\":"+jstr(d.tariffType)+",";
    o+="\"franchise\":"+jstr(d.franchise)+",";
    o+="\"franchiseDefault\":"+jnum(d.franchiseDefault)+",";
    o+="\"byLaw\":"+jnum(d.byLaw)+",";
    o+="\"ceiling\":"+jnum(d.ceiling)+",";
    o+="\"insTypeCode\":"+jstr(d.insTypeCode)+",";
    o+="\"contractCode\":"+jstr(d.contractCode)+",";
    o+="\"tech\":"+jnum(d.tech)+",";
    o+="\"prof\":"+jnum(d.prof)+",";
    o+="\"cons\":"+jnum(d.cons)+",";
    o+="\"total\":"+jnum(d.tech+d.prof+d.cons)+",";
    o+="\"franchiseOrgPct\":"+jnum(d.franchiseOrgPct)+",";
    o+="\"defaultOff\":"+jnum(d.defaultOff)+",";
    o+="\"priceCalcType\":"+jstr(d.priceCalcType)+",";
    o+="\"difference\":"+jnum(d.difference)+",";
    o+="\"color\":"+jstr(d.color)+",";
    o+="\"validityDate\":"+jstr(d.validityDate)+",";
    o+="\"username\":"+jstr(d.username)+",";
    o+="\"nationalId\":"+jstr(d.nationalId)+",";
    o+="\"booklet\":"+jnum(d.booklet)+",";
    o+="\"fileName\":"+jstr(d.fileName)+",";
    o+="\"active\":"; o+=(d.active?"true":"false"); o+=",";
    o+="\"created\":"+jstr(d.created)+",";
    o+="\"modified\":"+jstr(d.modified)+"";
    o+="}";
    return o;
}

// current Jalali date + time strings (header clock + record stamps)
static std::wstring crmTodayJalali(){
    SYSTEMTIME st=iranNow();
    int jy,jm,jd; gregToJalali(st.wYear,st.wMonth,st.wDay,jy,jm,jd);
    wchar_t b[32]; swprintf(b,32,L"%04d/%02d/%02d",jy,jm,jd);
    return b;
}
static std::wstring crmNowTime(){
    SYSTEMTIME st=iranNow();
    wchar_t b[16]; swprintf(b,16,L"%02d:%02d:%02d",st.wHour,st.wMinute,st.wSecond);
    return b;
}

// parse a JSON array of strings (["a","b"]) into a vector<wstring>. Tolerant.
static std::vector<std::wstring> crmParseStrArray(const std::string& raw){
    std::vector<std::wstring> out;
    if(raw.empty()) return out;
    std::string s=raw;
    size_t p=s.find('['); if(p==std::string::npos) return out;
    size_t e=s.find(']',p); if(e==std::string::npos) e=s.size();
    for(size_t i=p+1;i<e;){
        while(i<e&&(s[i]==' '||s[i]==','||s[i]=='\t'||s[i]=='\n'||s[i]=='\r')) i++;
        if(i>=e||s[i]!='"') break;
        i++;
        std::string item;
        while(i<e){
            char c=s[i++];
            if(c=='\\'&&i<e){ char x=s[i++];
                switch(x){ case 'n':item+='\n';break; case 'r':item+='\r';break;
                    case 't':item+='\t';break; case '"':item+='"';break;
                    case '\\':item+='\\';break; case '/':item+='/';break;
                    case 'u':{ if(i+4<=e){ int cp=(int)strtol(s.substr(i,4).c_str(),NULL,16); i+=4;
                        if(cp<0x80) item+=(char)cp;
                        else if(cp<0x800){ item+=(char)(0xC0|(cp>>6)); item+=(char)(0x80|(cp&0x3F)); }
                        else { item+=(char)(0xE0|(cp>>12)); item+=(char)(0x80|((cp>>6)&0x3F)); item+=(char)(0x80|(cp&0x3F)); } } break; }
                    default: item+=x; }
            } else if(c=='"') break;
            else item+=c;
        }
        out.push_back(u82w(item));
    }
    return out;
}

// ----------------------------------------------------------------------------
//  crmApi — handle every management verb. Returns a JSON response string.
// ----------------------------------------------------------------------------
static std::string crmApi(const std::string& verb, const std::string& body){
    using std::wstring;

    // ---- crm.init: everything the page needs on first render ----
    if(verb=="crm.init"||verb=="crm.ready"){
        std::string o="{\"ok\":true,";
        o+="\"date\":"+jstr(crmTodayJalali())+",";
        o+="\"time\":"+jstr(crmNowTime())+",";
        o+="\"user\":"+jstr(g_session.user.fullname.empty()?g_session.user.username:g_session.user.fullname)+",";
        o+="\"username\":"+jstr(g_session.user.username)+",";
        o+="\"role\":"+jnum(g_session.user.role)+",";
        o+="\"theme\":"+jstr(getSetting(L"theme",L"light"))+",";
        o+="\"version\":\"1.84.0\",";
        std::vector<Section> secs; Sections_All(secs);
        o+="\"sections\":"+jnum((long long)secs.size())+",";
        o+="\"patients\":"+jnum((long long)loadAllPatients().size())+",";
        o+="\"doctors\":"+jnum((long long)loadDoctors().size())+",";
        o+="\"services\":"+jnum((long long)loadServices().size())+",";
        o+="\"employees\":"+jnum((long long)loadUsers().size())+",";
        o+="\"today\":"+jnum((long long)countTodayReceptions())+",";
        o+="\"messages\":"+jnum((long long)unseenMessageCount(g_session.user.username))+",";
        o+="\"insurances\":"+crmInsListJson()+",";
        o+="\"supp\":"+crmSuppListJson()+",";
        // v1.74 — full insurance definition registries for the «تعریف بیمه» page
        {
            auto id=loadInsDefs(); auto sd=loadSuppDefs();
            std::string idb="[";
            for(size_t i=0;i<id.size();++i){ if(i)idb+=","; idb+=crmInsDefJson(id[i]); }
            idb+="]";
            std::string sdb="[";
            for(size_t i=0;i<sd.size();++i){ if(i)sdb+=","; sdb+=crmSuppDefJson(sd[i]); }
            sdb+="]";
            o+="\"insDefs\":"+idb+",";
            o+="\"suppDefs\":"+sdb+"";
        }
        o+="}";
        return o;
    }

    // ---- clock poll (keeps the Jalali date fresh) ----
    if(verb=="crm.clock"){
        return "{\"time\":"+jstr(crmNowTime())+",\"date\":"+jstr(crmTodayJalali())+"}";
    }

    // ---- dashboard KPIs ----
    if(verb=="crm.dashboard"){
        std::string o="{";
        o+="\"patients\":"+jnum((long long)loadAllPatients().size())+",";
        o+="\"doctors\":"+jnum((long long)loadDoctors().size())+",";
        o+="\"services\":"+jnum((long long)loadServices().size())+",";
        std::vector<Section> secs; Sections_All(secs);
        o+="\"sections\":"+jnum((long long)secs.size())+",";
        o+="\"employees\":"+jnum((long long)loadUsers().size())+",";
        o+="\"today\":"+jnum((long long)countTodayReceptions())+",";
        o+="\"messages\":"+jnum((long long)unseenMessageCount(g_session.user.username))+",";
        o+="\"pendingReqs\":"+jnum((long long)(pendingSetReqCount()+unseenProfReqCount()))+",";
        o+="\"date\":"+jstr(crmTodayJalali())+",";
        o+="\"time\":"+jstr(crmNowTime())+"";
        o+="}";
        return o;
    }

    // ===================================================== SECTIONS (بخش) ====
    if(verb=="crm.sections.list"){
        std::vector<Section> all; Sections_All(all);
        std::string o="{\"rows\":[";
        for(size_t i=0;i<all.size();++i){ if(i)o+=","; o+=crmSectionJson(all[i]); }
        o+="]}"; return o;
    }
    if(verb=="crm.sections.save"){
        Section s;                       // default ctor: id=0, is_active=1, parent_id=0
        double id=0; jsonGetNumber(body,"id",id); s.id=(int)id;
        double pid=0; jsonGetNumber(body,"parentId",pid); s.parent_id=(int)pid; // v1.74 زیربخش
        std::string t;
        if(jsonGetString(body,"code",t))    s.code=u82w(t);
        if(jsonGetString(body,"name",t))    s.name_fa=u82w(t);
        if(jsonGetString(body,"kind",t))    s.kind=u82w(t);
        s.is_active=jsonGetBool(body,"active",true)?1:0;
        s.updated_at=crmTodayJalali();
        if(s.id<=0) s.created_at=s.updated_at;
        int rid=Sections_Upsert(s);
        if(rid<=0) return "{\"ok\":false,\"err\":\"ذخیره بخش ناموفق بود.\"}";
        return "{\"ok\":true,\"id\":"+jnum(rid)+"}";
    }
    if(verb=="crm.sections.delete"){
        double id=0; jsonGetNumber(body,"id",id);
        if(id<=0) return "{\"ok\":false,\"err\":\"شناسه نامعتبر است.\"}";
        return Sections_Delete((int)id)>0 ? "{\"ok\":true}"
                                          : "{\"ok\":false,\"err\":\"حذف ناموفق بود.\"}";
    }

    // ===================================================== SERVICES (خدمات) ===
    if(verb=="crm.services.list"){
        std::string qU; jsonGetString(body,"q",qU);
        std::string deptU; jsonGetString(body,"dept",deptU);
        std::string insU;  jsonGetString(body,"insName",insU);
        wstring q=u82w(qU), dept=u82w(deptU), ins=u82w(insU);
        auto svcs=loadServices();
        std::string o="{\"rows\":[";
        int n=0;
        for(const auto& s:svcs){
            bool hit=q.empty()||wcontains(s.name,q)||wcontains(s.code,q)||
                    wcontains(s.category,q)||wcontains(s.insName,q);
            if(!hit) continue;
            if(!dept.empty() && s.dept!=dept) continue;          // section filter
            if(!ins.empty()  && s.insName!=ins) continue;         // insurance filter
            if(n++) o+=",";
            o+=crmServiceJson(s);
            if(n>=500) break;
        }
        o+="]}"; return o;
    }
    if(verb=="crm.services.save"){
        ServiceDef s;
        std::string t; double n=0;
        if(jsonGetString(body,"code",t))     s.code=u82w(t);
        if(jsonGetString(body,"name",t))     s.name=u82w(t);
        if(jsonGetString(body,"category",t)) s.category=u82w(t);
        if(jsonGetString(body,"dept",t))     s.dept=u82w(t);
        if(jsonGetNumber(body,"price",n))    s.price=(long long)llround(n);
        if(jsonGetString(body,"insType",t))  s.insType=u82w(t);
        if(jsonGetString(body,"desc",t))     s.desc=u82w(t);
        // v1.74 professional tariffs
        if(jsonGetString(body,"insName",t))     s.insName=u82w(t);
        if(jsonGetString(body,"multiplier",t))  s.multiplier=u82w(t);
        if(jsonGetNumber(body,"priceFree",n))    s.priceFree=(long long)llround(n);
        if(jsonGetNumber(body,"priceFreeNew",n)) s.priceFreeNew=(long long)llround(n);
        if(jsonGetNumber(body,"priceGov",n))     s.priceGov=(long long)llround(n);
        if(jsonGetNumber(body,"priceGovNew",n))  s.priceGovNew=(long long)llround(n);
        if(jsonGetNumber(body,"priceIns",n))     s.priceIns=(long long)llround(n);
        if(jsonGetNumber(body,"priceInsNew",n))  s.priceInsNew=(long long)llround(n);
        s.status=jsonGetBool(body,"active",true)?1:0;
        std::string orig; jsonGetString(body,"originalCode",orig);
        std::wstring oc=u82w(orig);
        std::wstring err;
        bool ok;
        if(oc.empty()){
            // brand-new service
            ok=addService(s,err);
        } else if(s.code==oc){
            // edit in place (code unchanged) -> updateService matches by code.
            // Preserve any §H extra columns the form does not resend.
            ok=updateService(s,err);
        } else {
            // rename: capture the original, remove the old row, add the new one.
            // On add failure, roll back the EXACT original so no data is lost.
            ServiceDef original; bool haveOrig=false;
            {
                auto all=loadServices();
                for(const auto& e:all) if(e.code==oc){ original=e; haveOrig=true; break; }
            }
            if(!haveOrig){
                err=L"خدمت با این کد یافت نشد."; ok=false;
            } else if(!removeService(oc)){
                err=L"خدمت با این کد یافت نشد."; ok=false;
            } else {
                ok=addService(s,err);
                if(!ok && haveOrig){ std::wstring e2; addService(original,e2); }
            }
        }
        if(!ok) return std::string("{\"ok\":false,\"err\":")+jstr(err)+"}";
        return "{\"ok\":true}";
    }
    if(verb=="crm.services.delete"){
        std::string cU; jsonGetString(body,"code",cU);
        wstring code=u82w(cU);
        if(code.empty()) return "{\"ok\":false,\"err\":\"کد خدمت خالی است.\"}";
        return removeService(code) ? "{\"ok\":true}"
                                   : "{\"ok\":false,\"err\":\"حذف ناموفق بود.\"}";
    }
    // v1.74 — professional price round / increase / decrease. Applies a single
    // operation across every matching service and rewrites services.dat once.
    //   mode:   percent | numeric | multiplier
    //   dir:    inc | dec
    //   amount: the percent / Rial / factor (parsed from the payload string)
    //   round:  0 (off) or one of 5,50,500,5000,50000
    //   target: which tariff pair to write — free | gov | ins  (the "new" price
    //           is computed from the matching current price)
    //   dept/insName: optional section + insurance filters
    if(verb=="crm.services.adjust"){
        std::string modeS; jsonGetString(body,"mode",modeS);
        std::string dirS;  jsonGetString(body,"dir",dirS);
        std::string amtS;  jsonGetString(body,"amount",amtS);
        std::string tgtS;  jsonGetString(body,"target",tgtS);
        std::string deptU; jsonGetString(body,"dept",deptU);
        std::string insU;  jsonGetString(body,"insName",insU);
        double roundN=0; jsonGetNumber(body,"round",roundN);
        long long round=(long long)llround(roundN);
        wstring dept=u82w(deptU), ins=u82w(insU);
        double amount=_wtof(u82w(amtS).c_str());
        auto svcs=loadServices();
        int changed=0;
        for(auto& s:svcs){
            if(!dept.empty() && s.dept!=dept) continue;
            if(!ins.empty()  && s.insName!=ins) continue;
            long long base=0, *dst=NULL;
            if(tgtS=="gov"){ base=s.priceGov;    dst=&s.priceGovNew; }
            else if(tgtS=="ins"){ base=s.priceIns;  dst=&s.priceInsNew; }
            else { base=s.priceFree; dst=&s.priceFreeNew; }
            if(base<=0) continue;                 // nothing to scale yet
            double v=(double)base;
            if(modeS=="percent"){
                v = (dirS=="dec") ? v*(100.0-amount)/100.0 : v*(100.0+amount)/100.0;
            } else if(modeS=="multiplier"){
                v = v*amount;
            } else { // numeric
                v = (dirS=="dec") ? v-amount : v+amount;
            }
            long long r=(long long)llround(v);
            if(round>0){                          // round up to the next multiple
                long long m=r%round;
                if(m!=0) r += (round-m);
            }
            if(r<0) r=0;
            *dst=r; ++changed;
        }
        saveAllServices(svcs);
        return std::string("{\"ok\":true,\"changed\":")+jnum(changed)+"}";
    }

    // ============================================ INSURANCE DEFINITIONS (بیمه) ==
    // base insurance registry (تعریف بیمه پایه)
    if(verb=="crm.insurance.list"){
        auto defs=loadInsDefs();
        std::string o="{\"rows\":[";
        for(size_t i=0;i<defs.size();++i){ if(i)o+=","; o+=crmInsDefJson(defs[i]); }
        o+="]}"; return o;
    }
    if(verb=="crm.insurance.save"){
        InsDef d;
        std::string t; double n=0;
        double idx=-1; jsonGetNumber(body,"idx",idx); d.idx=(int)idx;
        if(jsonGetString(body,"sectionCode",t)) d.sectionCode=u82w(t);
        if(jsonGetString(body,"insCode",t))     d.insCode=u82w(t);
        double os=-1; jsonGetNumber(body,"orgShare",os); d.orgShare=(int)os;
        if(jsonGetString(body,"groupName",t))   d.groupName=u82w(t);
        if(jsonGetString(body,"flipName",t))    d.flipName=u82w(t);
        if(jsonGetString(body,"contractCode",t))d.contractCode=u82w(t);
        if(jsonGetString(body,"insType",t))     d.insType=u82w(t);
        if(jsonGetNumber(body,"tech",n)) d.tech=(long long)llround(n);
        if(jsonGetNumber(body,"prof",n)) d.prof=(long long)llround(n);
        if(jsonGetNumber(body,"cons",n)) d.cons=(long long)llround(n);
        d.active=jsonGetBool(body,"active",true)?1:0;
        if(!upsertInsDef(d)) return "{\"ok\":false,\"err\":\"ذخیره بیمه ناموفق بود.\"}";
        return "{\"ok\":true,\"idx\":"+jnum(d.idx)+"}";
    }
    if(verb=="crm.insurance.delete"){
        double idx=-1; jsonGetNumber(body,"idx",idx);
        if(idx<0) return "{\"ok\":false,\"err\":\"شناسه نامعتبر است.\"}";
        return deleteInsDef((int)idx) ? "{\"ok\":true}"
                                      : "{\"ok\":false,\"err\":\"حذف ناموفق بود.\"}";
    }
    // supplementary insurance registry (تعریف بیمه تکمیلی)
    if(verb=="crm.supp.list"){
        auto defs=loadSuppDefs();
        std::string o="{\"rows\":[";
        for(size_t i=0;i<defs.size();++i){ if(i)o+=","; o+=crmSuppDefJson(defs[i]); }
        o+="]}"; return o;
    }
    if(verb=="crm.supp.save"){
        SuppDef d;
        std::string t; double n=0;
        double idx=-1; jsonGetNumber(body,"idx",idx); d.idx=(int)idx;
        if(jsonGetString(body,"sectionCode",t))  d.sectionCode=u82w(t);
        if(jsonGetString(body,"insSpec",t))      d.insSpec=u82w(t);
        if(jsonGetString(body,"name",t))         d.name=u82w(t);
        if(jsonGetString(body,"tariffType",t))   d.tariffType=u82w(t);
        if(jsonGetString(body,"franchise",t))    d.franchise=u82w(t);
        double fd=1; jsonGetNumber(body,"franchiseDefault",fd); d.franchiseDefault=(int)fd;
        double bl=0; jsonGetNumber(body,"byLaw",bl); d.byLaw=(int)bl;
        if(jsonGetNumber(body,"ceiling",n)) d.ceiling=(long long)llround(n);
        if(jsonGetString(body,"insTypeCode",t))  d.insTypeCode=u82w(t);
        if(jsonGetString(body,"contractCode",t)) d.contractCode=u82w(t);
        if(jsonGetNumber(body,"tech",n)) d.tech=(long long)llround(n);
        if(jsonGetNumber(body,"prof",n)) d.prof=(long long)llround(n);
        if(jsonGetNumber(body,"cons",n)) d.cons=(long long)llround(n);
        double fp=0; jsonGetNumber(body,"franchiseOrgPct",fp); d.franchiseOrgPct=(int)fp;
        double df=0; jsonGetNumber(body,"defaultOff",df); d.defaultOff=(int)df;
        if(jsonGetString(body,"priceCalcType",t)) d.priceCalcType=u82w(t);
        double di=0; jsonGetNumber(body,"difference",di); d.difference=(int)di;
        if(jsonGetString(body,"color",t))        d.color=u82w(t);
        if(jsonGetString(body,"validityDate",t)) d.validityDate=u82w(t);
        if(jsonGetString(body,"username",t))     d.username=u82w(t);
        if(jsonGetString(body,"nationalId",t))   d.nationalId=u82w(t);
        double bk=0; jsonGetNumber(body,"booklet",bk); d.booklet=(int)bk;
        if(jsonGetString(body,"fileName",t))     d.fileName=u82w(t);
        d.active=jsonGetBool(body,"active",true)?1:0;
        if(d.name.empty()) return "{\"ok\":false,\"err\":\"نام بیمه تکمیلی الزامی است.\"}";
        if(!upsertSuppDef(d)) return "{\"ok\":false,\"err\":\"ذخیره بیمه تکمیلی ناموفق بود.\"}";
        return "{\"ok\":true,\"idx\":"+jnum(d.idx)+"}";
    }
    if(verb=="crm.supp.delete"){
        double idx=-1; jsonGetNumber(body,"idx",idx);
        if(idx<0) return "{\"ok\":false,\"err\":\"شناسه نامعتبر است.\"}";
        return deleteSuppDef((int)idx) ? "{\"ok\":true}"
                                       : "{\"ok\":false,\"err\":\"حذف ناموفق بود.\"}";
    }

    // ===================================================== DOCTORS (پزشک/پرستار)
    if(verb=="crm.doctors.list"){
        std::string qU; jsonGetString(body,"q",qU);
        wstring q=u82w(qU);
        auto docs=loadDoctors();
        std::string o="{\"rows\":[";
        int n=0;
        for(size_t i=0;i<docs.size();++i){
            const auto& d=docs[i];
            bool hit=q.empty()||wcontains(d.name,q)||wcontains(d.specialty,q)||
                    wcontains(d.medicalId,q)||wcontains(d.docCode,q);
            if(!hit) continue;
            if(n++) o+=",";
            o+=crmDoctorJson(d,(int)i);
            if(n>=200) break;
        }
        o+="]}"; return o;
    }
    if(verb=="crm.doctors.save"){
        DoctorDef d;
        std::string t; double n=0;
        if(jsonGetString(body,"name",t))             d.name=u82w(t);
        if(jsonGetString(body,"specialty",t))        d.specialty=u82w(t);
        if(jsonGetString(body,"deptId",t))           d.deptId=u82w(t);
        if(jsonGetNumber(body,"docType",n))          d.docType=(int)n;
        if(jsonGetString(body,"docCode",t))          d.docCode=u82w(t);
        d.active=jsonGetBool(body,"active",true);
        // v1.78.0: default ON — a doctor is a performer unless explicitly unticked.
        d.isPerformer=jsonGetBool(body,"isPerformer",true);
        if(jsonGetString(body,"medicalId",t))        d.medicalId=u82w(t);
        if(jsonGetString(body,"namePrefix",t))       d.namePrefix=u82w(t);
        if(jsonGetString(body,"firstName",t))        d.firstName=u82w(t);
        if(jsonGetString(body,"lastName",t))         d.lastName=u82w(t);
        if(jsonGetString(body,"nationalId",t))       d.nationalId=u82w(t);
        if(jsonGetString(body,"mobile",t))           d.mobile=u82w(t);
        if(jsonGetString(body,"email",t))            d.email=u82w(t);
        if(jsonGetString(body,"address",t))          d.address=u82w(t);
        if(jsonGetString(body,"franchise",t))        d.franchise=u82w(t);
        d.printOnReceipt=jsonGetBool(body,"printOnReceipt",true);
        if(jsonGetString(body,"insSpecialty",t))     d.insSpecialty=u82w(t);
        if(jsonGetString(body,"degree",t))           d.degree=u82w(t);
        if(jsonGetNumber(body,"contractType",n))     d.contractType=(int)n;
        if(jsonGetString(body,"emergencyContract",t))d.emergencyContract=u82w(t);
        if(jsonGetString(body,"accounting",t))       d.accounting=u82w(t);
        std::string svcRaw; if(jsonGetRaw(body,"services",svcRaw)) d.services=crmParseStrArray(svcRaw);
        if(trim(d.name).empty()) return "{\"ok\":false,\"err\":\"نام پزشک نمی‌تواند خالی باشد.\"}";
        double idx=-1; jsonGetNumber(body,"idx",idx);
        auto docs=loadDoctors();
        if(idx>=0 && idx<(double)docs.size()){
            docs[(size_t)idx]=d;
        } else {
            // avoid exact-duplicate names on append
            for(const auto& e:docs) if(e.name==d.name)
                return "{\"ok\":false,\"err\":\"این پزشک قبلاً ثبت شده است.\"}";
            docs.push_back(d);
        }
        if(!saveDoctors(docs)) return "{\"ok\":false,\"err\":\"ذخیره پزشک ناموفق بود.\"}";
        logLine(L"crm doctor saved: "+d.name);
        return "{\"ok\":true}";
    }
    if(verb=="crm.doctors.delete"){
        double idx=-1; jsonGetNumber(body,"idx",idx);
        if(idx<0) return "{\"ok\":false,\"err\":\"پزشکی انتخاب نشده است.\"}";
        auto docs=loadDoctors();
        if(idx>=(double)docs.size()) return "{\"ok\":false,\"err\":\"پزشک یافت نشد.\"}";
        std::wstring nm=docs[(size_t)idx].name;
        docs.erase(docs.begin()+(size_t)idx);
        if(!saveDoctors(docs)) return "{\"ok\":false,\"err\":\"حذف ناموفق بود.\"}";
        logLine(L"crm doctor removed: "+nm);
        return "{\"ok\":true}";
    }

    // ===================================================== PATIENTS (بیمار) ===
    if(verb=="crm.patients.list"){
        std::string qU; jsonGetString(body,"q",qU);
        wstring q=u82w(qU); wstring qd=normDigits(q);
        auto all=loadAllPatients();
        std::string o="{\"rows\":[";
        int n=0;
        for(const auto& p:all){
            wstring full=p.first+L" "+p.last;
            bool hit=q.empty()||wcontains(full,q)||
                    normDigits(p.nid).find(qd)!=wstring::npos||
                    normDigits(p.mobile).find(qd)!=wstring::npos;
            if(!hit) continue;
            if(n++) o+=",";
            o+=crmPatientJson(p);
            if(n>=200) break;
        }
        o+="]}"; return o;
    }
    if(verb=="crm.patients.save"){
        std::string t; double si=-1;
        std::wstring nid,first,last,father,gender,birth,mobile,landline,address;
        if(jsonGetString(body,"nid",t))      nid=u82w(t);
        if(jsonGetString(body,"first",t))    first=u82w(t);
        if(jsonGetString(body,"last",t))     last=u82w(t);
        if(jsonGetString(body,"father",t))   father=u82w(t);
        if(jsonGetString(body,"gender",t))   gender=u82w(t);
        if(jsonGetString(body,"birth",t))    birth=u82w(t);
        if(jsonGetString(body,"mobile",t))   mobile=u82w(t);
        if(jsonGetString(body,"phone",t))    landline=u82w(t);
        if(jsonGetString(body,"addr",t))     address=u82w(t);
        if(jsonGetNumber(body,"suppIdx",si)) /*keep*/; else si=-1;
        nid=normDigits(trim(nid));
        if(nid.empty()) return "{\"ok\":false,\"err\":\"کد ملی خالی است.\"}";
        std::vector<int> ins;
        std::string insRaw; if(jsonGetRaw(body,"insurances",insRaw)){
            // parse [n,n,...]
            size_t p=insRaw.find('[');
            if(p!=std::string::npos){
                for(size_t i=p+1;i<insRaw.size();){
                    while(i<insRaw.size()&&(insRaw[i]==' '||insRaw[i]==',')) i++;
                    if(i>=insRaw.size()||insRaw[i]==']') break;
                    char* end=NULL; double v=strtod(insRaw.c_str()+i,&end);
                    if(end==insRaw.c_str()+i) break;
                    ins.push_back((int)v); i=(size_t)(end-insRaw.c_str());
                }
            }
        }
        rememberPatient(nid,first,last,father,gender,birth,mobile,
                        landline,address,ins,(int)si);
        logLine(L"crm patient saved: "+nid);
        return "{\"ok\":true}";
    }
    if(verb=="crm.patients.delete"){
        std::string nU; jsonGetString(body,"nid",nU);
        std::wstring nid=normDigits(trim(u82w(nU)));
        if(nid.empty()) return "{\"ok\":false,\"err\":\"کد ملی خالی است.\"}";
        return deletePatient(nid) ? "{\"ok\":true}"
                                  : "{\"ok\":false,\"err\":\"بیماری با این کد ملی یافت نشد.\"}";
    }

    // ===================================================== EMPLOYEES (کاربر) ==
    if(verb=="crm.employees.list"){
        auto us=loadUsers();
        std::string o="{\"rows\":[";
        size_t lim = us.size()>500 ? 500 : us.size();   // v1.80.0: render cap
        for(size_t i=0;i<lim;++i){ if(i)o+=","; o+=crmUserJson(us[i]); }
        o+="],\"totalUsers\":"+jnum((long long)us.size())+",\"depts\":[";
        auto ds=loadDepts();
        for(size_t i=0;i<ds.size();++i){ if(i)o+=","; o+=crmDeptJson(ds[i]); }
        o+="]}";
        // v1.80.0: the clinical sections tree too — the account pickers filter
        // by بخش/زیربخش from it (personnel live there now).
        std::vector<Section> ss; Sections_All(ss);
        o+=",\"sects\":[";
        bool fs=true;
        for(auto& s2:ss){ if(!s2.is_active) continue;
            if(!fs)o+=","; fs=false;
            o+="{\"id\":"+jstr(std::to_wstring(s2.id))+",\"code\":"+jstr(s2.code)+
               ",\"name\":"+jstr(s2.name_fa)+",\"parentId\":"+jnum(s2.parent_id)+"}"; }
        o+="]";
        return o;
    }
    if(verb=="crm.employees.save"){
        std::string t; double r=0;
        std::wstring username,fullname,dept,pass;
        if(jsonGetString(body,"username",t)) username=u82w(t);
        if(jsonGetString(body,"fullname",t)) fullname=u82w(t);
        if(jsonGetString(body,"dept",t))     dept=u82w(t);
        if(jsonGetString(body,"password",t)) pass=u82w(t);
        if(jsonGetNumber(body,"role",r))     /*keep*/; else r=0;
        username=trim(username);
        if(username.empty()) return "{\"ok\":false,\"err\":\"نام کاربری خالی است.\"}";
        // existing user -> update fields (+ optional password reset)
        auto us=loadUsers(); bool exists=false;
        for(const auto& u:us) if(u.username==username){ exists=true; break; }
        if(exists){
            std::wstring err;
            if(!updateUserAccount(username,fullname,dept,(int)r,pass,err))
                return std::string("{\"ok\":false,\"err\":")+jstr(err)+"}";
            return "{\"ok\":true}";
        }
        // new user -> add (requires a password)
        if(pass.empty()) return "{\"ok\":false,\"err\":\"رمز عبور الزامی است.\"}";
        User u; u.username=username; u.fullname=fullname; u.dept=dept;
        u.role=(int)r; u.hash=hashPassword(pass);
        std::wstring err;
        if(!addUser(u,err)) return std::string("{\"ok\":false,\"err\":")+jstr(err)+"}";
        return "{\"ok\":true}";
    }
    if(verb=="crm.employees.delete"){
        std::string uU; jsonGetString(body,"username",uU);
        std::wstring username=u82w(uU);
        if(username.empty()||username==L"prf")
            return "{\"ok\":false,\"err\":\"این کاربر قابل حذف نیست.\"}";
        // v1.79.0: unlink the personnel record too, so the person stays in the
        // registry and can receive a fresh account later (never a dangling link)
        { PersonDef p; if(personByUsername(username,p)){
            p.username.clear(); std::wstring e2; updatePerson(p,e2); } }
        return removeUser(username) ? "{\"ok\":true}"
                                    : "{\"ok\":false,\"err\":\"حذف ناموفق بود.\"}";
    }
    // ================= v1.79.0: personnel registry («تعریف پرسنل») ==========
    if(verb=="crm.persons.list"){
        std::string deptU,qU; jsonGetString(body,"deptId",deptU); jsonGetString(body,"q",qU);
        std::wstring deptId=u82w(deptU), q=u82w(qU);
        std::wstring qLower=q; for(auto& c:qLower) c=towlower(c);
        std::string qEn=w2u8(normDigits(q));
        auto ps=loadPersons();
        // v1.80.0: the clinical sections tree — loaded ONCE per call (names +
        // parent links), shared by the filter and every row's JSON.
        std::vector<Section> ss; Sections_All(ss);
        int deptNum=_wtoi(deptId.c_str());
        // v1.80.0: «__none__» is honoured SERVER-side too, so the suspended
        // view never depends on the 200-row cap (client can't see past it).
        bool wantSuspended=(deptId==L"__none__");
        std::string o="{\"rows\":[";
        int n=0, total=0;
        for(auto& p:ps){
            if(wantSuspended){ if(!p.deptId.empty()) continue; }
            else if(!deptId.empty()){
                int pd=_wtoi(p.deptId.c_str()), psu=_wtoi(p.subId.c_str());
                // a section filter also covers persons parked in its subsections
                bool inDept = (pd==deptNum) || (psu==deptNum);
                if(!inDept && psu>0){
                    for(auto& s2:ss) if(s2.id==psu && s2.parent_id==deptNum){ inDept=true; break; }
                }
                if(!inDept) continue;
            }
            if(!q.empty()){
                std::wstring hay=p.code+L" "+p.firstName+L" "+p.lastName+L" "+
                                 p.nationalId+L" "+p.position+L" "+personRoleLabel(p);
                for(auto& c:hay) c=towlower(c);
                bool hit=hay.find(qLower)!=std::wstring::npos;
                // numeric queries also match the digits form of code/nationalId
                if(!hit && !qEn.empty()){
                    std::wstring nd=normDigits(p.code)+L" "+normDigits(p.nationalId);
                    hit=nd.find(u82w(qEn))!=std::wstring::npos;
                }
                if(!hit) continue;
            }
            total++;
            // v1.80.0: hard cap — the page never renders more than 200 rows at
            // once (large clinics refine with the live search instead).
            if(n>=200) continue;
            if(n++) o+=",";
            o+=crmPersonJson(p,ss);
        }
        o+="],\"total\":"+jnum(total)+",\"capped\":"; o+=(total>200?"true":"false"); o+=",";
        o+="\"depts\":[";
        // v1.80.0: the picker dropdowns are fed from the clinical tree — top
        // sections plus their subsections (the page indents them).
        bool firstDep=true;
        for(auto& s2:ss){ if(!s2.is_active) continue;
            if(!firstDep) o+=","; firstDep=false;
            o+="{\"id\":"+jstr(std::to_wstring(s2.id))+",\"code\":"+jstr(s2.code)+
               ",\"name\":"+jstr(s2.name_fa)+",\"parentId\":"+jnum(s2.parent_id)+"}"; }
        o+="]}"; return o;
    }
    if(verb=="crm.persons.nextcode"){
        std::string dU,sU; jsonGetString(body,"deptId",dU); jsonGetString(body,"subId",sU);
        return std::string("{\"code\":")+jstr(nextPersonCode(u82w(dU),u82w(sU)))+"}";
    }
    if(verb=="crm.persons.save"){
        std::string t; double rk=0;
        PersonDef p; bool editing=false;
        // v1.79.0 review fix: «origCode» (sent by the edit form) decides the
        // update path — the free-text «code» box can never hijack another
        // person's record or duplicate a person by clearing the code.
        std::wstring origCode;
        if(jsonGetString(body,"origCode",t))  origCode=trim(u82w(t));
        if(jsonGetString(body,"code",t))        { p.code=trim(u82w(t)); }
        if(jsonGetString(body,"firstName",t))   p.firstName=u82w(t);
        if(jsonGetString(body,"lastName",t))    p.lastName=u82w(t);
        if(jsonGetString(body,"fatherName",t))  p.fatherName=u82w(t);
        if(jsonGetString(body,"nationalId",t))  p.nationalId=normDigits(u82w(t));
        if(jsonGetString(body,"birthDate",t))   p.birthDate=u82w(t);
        if(jsonGetString(body,"address",t))     p.address=u82w(t);
        if(jsonGetString(body,"mobile",t))      p.mobile=normDigits(u82w(t));
        if(jsonGetString(body,"phone",t))       p.phone=normDigits(u82w(t));
        if(jsonGetString(body,"email",t))       p.email=u82w(t);
        if(jsonGetString(body,"education",t))   p.education=u82w(t);
        if(jsonGetString(body,"field",t))       p.field=u82w(t);
        if(jsonGetString(body,"degree",t))      p.degree=u82w(t);
        if(jsonGetNumber(body,"roleKind",rk))   p.roleKind=(int)rk;
        if(jsonGetString(body,"roleCustom",t))  p.roleCustom=u82w(t);
        if(jsonGetString(body,"position",t))    p.position=u82w(t);
        if(jsonGetString(body,"deptId",t))      p.deptId=u82w(t);
        if(jsonGetString(body,"subId",t))       p.subId=u82w(t);
        if(!origCode.empty()){
            // edit path: the record is keyed by origCode; a stray/typed «code»
            // is IGNORED so it can never duplicate or hijack another person.
            PersonDef old;
            if(!personByCode(origCode,old))
                return "{\"ok\":false,\"err\":\"پرسنل مبدأ پیدا نشد.\"}";
            editing=true;
            p.code=origCode;
            p.username=old.username; p.photo=old.photo; p.created=old.created;
        } else if(!p.code.empty()){
            PersonDef old; editing=personByCode(p.code,old);
            if(editing){ p.username=old.username; p.photo=old.photo; p.created=old.created; }
        }
        std::wstring err;
        bool ok = editing ? updatePerson(p,err) : addPerson(p,err);
        if(!ok) return std::string("{\"ok\":false,\"err\":")+jstr(err)+"}";
        // optional photo upload (data URL) — AFTER a successful save so a
        // rejected form never orphans a file; the photo keeps the FINAL code
        // (addPerson may have auto-generated it).
        std::string photoU;
        if(jsonGetString(body,"photoData",photoU) && photoU.size()>32){
            std::wstring ext; std::string bytes=b64decode(b64payload(photoU,ext));
            std::wstring rel;
            if(!bytes.empty() && savePersonPhoto(p.code,bytes,ext,rel)){
                p.photo=rel; std::wstring e2; updatePerson(p,e2);
            }
        }
        return std::string("{\"ok\":true,\"code\":")+jstr(p.code)+"}";
    }
    if(verb=="crm.persons.delete"){
        std::string cU; jsonGetString(body,"code",cU);
        std::wstring code=u82w(cU);
        if(code.empty()) return "{\"ok\":false,\"err\":\"کد پرسنلی خالی است.\"}";
        // v1.79.0: deleting a person also CLOSES their login account — a
        // removed person must not be able to sign in any more. (The UI
        // confirms this explicitly before calling.)
        PersonDef p; if(personByCode(code,p) && !p.username.empty()){
            removeUser(p.username);
        }
        return removePerson(code) ? "{\"ok\":true}"
                                  : "{\"ok\":false,\"err\":\"حذف ناموفق بود.\"}";
    }
    //  v1.80.0: fetch ONE person by code — the person-sheet links use this
    //  instead of re-listing the (200-row-capped) registry.
    if(verb=="crm.persons.get"){
        std::string cU; jsonGetString(body,"code",cU);
        PersonDef p;
        if(!personByCode(u82w(cU),p)) return "{\"ok\":false}";
        std::vector<Section> ss; Sections_All(ss);
        return std::string("{\"ok\":true,\"person\":")+crmPersonJson(p,ss)+"}";
    }
    if(verb=="crm.persons.photo"){
        std::string cU; jsonGetString(body,"code",cU);
        PersonDef p;
        if(!personByCode(u82w(cU),p) || p.photo.empty()) return "{\"ok\":false}";
        std::string bytes; std::wstring mime;
        if(!loadPersonPhoto(p.photo,bytes,mime)) return "{\"ok\":false}";
        std::string b64; { static const char* T="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            for(size_t i=0;i<bytes.size();i+=3){
                unsigned v=(unsigned char)bytes[i]<<16;
                if(i+1<bytes.size()) v|=(unsigned char)bytes[i+1]<<8;
                if(i+2<bytes.size()) v|=(unsigned char)bytes[i+2];
                b64+=T[(v>>18)&63]; b64+=T[(v>>12)&63];
                b64+=(i+1<bytes.size())?T[(v>>6)&63]:'=';
                b64+=(i+2<bytes.size())?T[v&63]:'=';
            } }
        return std::string("{\"ok\":true,\"data\":\"data:")+w2u8(mime)+";base64,"+b64+"\"}";
    }
    // ================= v1.79.0: «تعریف حساب کاربری» (personnel-linked) =======
    //  create: a person + fresh username/password (+ permission ticks).
    if(verb=="crm.accounts.create"){
        std::string t; 
        std::wstring personCode,userName,pass,perms;
        if(jsonGetString(body,"personCode",t)) personCode=u82w(t);
        if(jsonGetString(body,"username",t))   userName=trim(u82w(t));
        if(jsonGetString(body,"password",t))   pass=u82w(t);
        if(jsonGetString(body,"perms",t))      perms=u82w(t);
        PersonDef p;
        if(!personByCode(personCode,p))
            return "{\"ok\":false,\"err\":\"پرسنل پیدا نشد.\"}";
        if(!p.username.empty())
            return "{\"ok\":false,\"err\":\"این پرسنل قبلاً حساب کاربری دارد.\"}";
        if(userName.empty()||pass.empty())
            return "{\"ok\":false,\"err\":\"نام کاربری و رمز عبور الزامی است.\"}";
        User u; u.username=userName;
        u.fullname=trim(p.firstName+L" "+p.lastName);
        // v1.80.0: the person's deptId is a CLINICAL section id now — resolve
        // the TOP-LEVEL section name (not the subsection) so the account's
        // dept matches the accounts-table filter and the section info sheet.
        { int pd=_wtoi(p.deptId.c_str());
          std::vector<Section> ss2; Sections_All(ss2);
          for(auto& s2:ss2) if(s2.id==pd){ u.dept=s2.name_fa; break; } }
        u.role=0; u.perms=perms; u.hash=hashPassword(pass);
        std::wstring err;
        if(!addUser(u,err)) return std::string("{\"ok\":false,\"err\":")+jstr(err)+"}";
        p.username=userName; std::wstring e2; updatePerson(p,e2);
        return "{\"ok\":true}";
    }
    //  update: permission ticks + optional password reset (username immutable)
    if(verb=="crm.accounts.update"){
        std::string t;
        std::wstring userName,pass,perms; bool hasPerms=false;
        if(jsonGetString(body,"username",t))   userName=trim(u82w(t));
        if(jsonGetString(body,"password",t))   pass=u82w(t);
        if(jsonGetString(body,"perms",t))      { perms=u82w(t); hasPerms=true; }
        if(userName.empty()) return "{\"ok\":false,\"err\":\"نام کاربری خالی است.\"}";
        if(userName==L"prf") return "{\"ok\":false,\"err\":\"این حساب قابل ویرایش نیست.\"}";
        auto us=loadUsers();
        const User* cur=nullptr;
        for(auto& u:us) if(u.username==userName){ cur=&u; break; }
        if(!cur) return "{\"ok\":false,\"err\":\"حساب پیدا نشد.\"}";
        // fullname/dept/role stay as-is — this verb only touches perms, and a
        // password when one was typed. Skipping the account write when the
        // password is empty keeps the stored role untouched (v1.79 review).
        if(!pass.empty()){
            std::wstring err;
            if(!updateUserAccount(cur->username,cur->fullname,cur->dept,cur->role,pass,err))
                return std::string("{\"ok\":false,\"err\":")+jstr(err)+"}";
        }
        if(hasPerms){
            std::wstring err;
            if(!setUserPerms(userName,perms,err))
                return std::string("{\"ok\":false,\"err\":")+jstr(err)+"}";
        }
        return "{\"ok\":true}";
    }
    //  v1.80.0: section info sheet — the section (+ its زیربخش‌ها) + personnel
    //  (in the section or any of its subsections) + linked accounts, printable.
    if(verb=="crm.sections.info"){
        std::string dU; jsonGetString(body,"id",dU);
        int id=_wtoi(u82w(dU).c_str());
        if(id<=0) return "{\"ok\":false,\"err\":\"شناسه نامعتبر است.\"}";
        std::vector<Section> ss; Sections_All(ss);
        const Section* sp=nullptr;
        for(auto& s2:ss) if(s2.id==id){ sp=&s2; break; }
        if(!sp) return "{\"ok\":false,\"err\":\"بخش پیدا نشد.\"}";
        std::string o="{\"ok\":true,\"section\":"+crmSectionJson(*sp)+",\"subs\":[";
        bool f=true; std::wstring subIds=L"|";
        for(auto& s2:ss) if(s2.parent_id==id){
            // v1.80.0: match persons parked in ANY subsection (even a
            // deactivated one) — same rule as the persons.list dept filter.
            subIds+=std::to_wstring(s2.id)+L"|";
            if(!s2.is_active) continue;      // …but only list active subs
            if(!f)o+=","; f=false;
            o+=crmSectionJson(s2);
        }
        o+="],\"persons\":[";
        int n=0;
        auto ps=loadPersons();
        for(auto& p2:ps){
            bool inS = (_wtoi(p2.deptId.c_str())==id) ||
                       (subIds.find(L"|"+p2.subId+L"|")!=std::wstring::npos);
            if(!inS) continue;
            if(n++)o+=","; o+=crmPersonJson(p2,ss);
        }
        o+="],\"users\":[";
        auto us=loadUsers(); int m=0;
        for(auto& u:us) if(u.dept==sp->name_fa){
            if(m++)o+=",";
            o+="{\"username\":"+jstr(u.username)+",\"fullname\":"+jstr(u.fullname)+
               ",\"online\":"; o+=(isUserOnline(u.username)?"true":"false"); o+="}";
        }
        o+="]";
        // v1.82.0: reception-subsection counts + colour chips (hashed by id)
        {
            static const wchar_t* PAL[]={
                L"#2563eb",L"#0f766e",L"#7c3aed",L"#be123c",
                L"#c2410c",L"#0369a1",L"#4d7c0f",L"#6d28d9"
            };
            int recN=0;
            std::string chips="[";
            bool cf=true;
            for(auto& s2:ss) if(s2.parent_id==id && s2.is_active && Ops_IsReception(s2)){
                recN++;
                unsigned h=(unsigned)s2.id*2654435761u;
                const wchar_t* col=PAL[h%8];
                if(!cf) chips+=","; cf=false;
                chips+="{\"id\":"+jnum(s2.id)+",\"name\":"+jstr(s2.name_fa)+
                       ",\"color\":"+jstr(col)+"}";
            }
            chips+="]";
            o+=",\"receptionCount\":"+jnum(recN)+",\"receptionChips\":"+chips;
        }
        o+="}";
        return o;
    }
    //  dept info sheet: the dept + its personnel + its accounts (for the view
    //  + print dialog)
    if(verb=="crm.depts.info"){
        std::string dU; jsonGetString(body,"id",dU);
        std::wstring id=u82w(dU);
        auto ds=loadDepts();
        const DeptCat* dp=nullptr;
        for(auto& d:ds) if(d.id==id){ dp=&d; break; }
        if(!dp) return "{\"ok\":false,\"err\":\"بخش پیدا نشد.\"}";
        std::string o="{\"ok\":true,\"dept\":"+crmDeptJson(*dp)+",\"persons\":[";
        int n=0;
        auto ps=loadPersons();
        std::vector<Section> ds2; Sections_All(ds2);   // v1.80.0: name resolver
        for(auto& p:ps) if(p.deptId==id){ if(n++)o+=","; o+=crmPersonJson(p,ds2); }
        o+="],\"users\":[";
        auto us=loadUsers(); int m=0;
        for(auto& u:us) if(u.dept==dp->name){
            if(m++)o+=",";
            o+="{\"username\":"+jstr(u.username)+",\"fullname\":"+jstr(u.fullname)+
               ",\"online\":"; o+=(isUserOnline(u.username)?"true":"false"); o+="}";
        }
        o+="]}";
        return o;
    }
    if(verb=="crm.depts.save"){
        DeptCat c;
        std::string t;
        if(jsonGetString(body,"id",t))      c.id=u82w(t);
        if(jsonGetString(body,"name",t))    c.name=u82w(t);
        if(jsonGetString(body,"manager",t)) c.manager=u82w(t);
        if(jsonGetString(body,"icon",t))    c.icon=u82w(t);
        std::wstring err;
        if(!addDept(c,err)) return std::string("{\"ok\":false,\"err\":")+jstr(err)+"}";
        return "{\"ok\":true}";
    }
    if(verb=="crm.depts.delete"){
        std::string idU; jsonGetString(body,"id",idU);
        std::wstring id=u82w(idU);
        return removeDept(id) ? "{\"ok\":true}"
                              : "{\"ok\":false,\"err\":\"حذف ناموفق بود.\"}";
    }

    // ===================================================== MESSAGES (کارتابل)
    if(verb=="crm.messages.list"){
        std::string forU; jsonGetString(body,"forUser",forU);
        std::wstring who=forU.empty()?g_session.user.username:u82w(forU);
        if(who.empty()) who=L"*";
        auto msgs=loadMessages(who);       // oldest-first (file order)
        // newest-first so idx == indexNewestFirst used by pin/seen/delete.
        std::reverse(msgs.begin(),msgs.end());
        std::string o="{\"rows\":[";
        for(size_t i=0;i<msgs.size();++i){ if(i)o+=","; o+=crmMsgJson(msgs[i],(int)i); }
        o+="],\"unseen\":"+jnum((long long)unseenMessageCount(who))+"}";
        return o;
    }
    if(verb=="crm.messages.send"){
        std::string fU,tU,txt; double tp=0;
        jsonGetString(body,"from",fU);
        jsonGetString(body,"to",tU);
        jsonGetString(body,"text",txt);
        jsonGetNumber(body,"type",tp);
        std::wstring from=fU.empty()?g_session.user.username:u82w(fU);
        std::wstring to=u82w(tU); if(to.empty()) to=L"*";
        std::wstring text=u82w(txt);
        if(trim(text).empty()) return "{\"ok\":false,\"err\":\"متن پیام خالی است.\"}";
        pushMessageT(from,to,text,(int)tp);
        return "{\"ok\":true}";
    }
    if(verb=="crm.messages.seen"){
        std::string forU; jsonGetString(body,"forUser",forU);
        std::wstring who=forU.empty()?g_session.user.username:u82w(forU);
        markMessagesSeen(who);
        return "{\"ok\":true}";
    }
    if(verb=="crm.messages.delete"){
        std::string forU; double idx=-1;
        jsonGetString(body,"forUser",forU);
        jsonGetNumber(body,"idx",idx);
        std::wstring who=forU.empty()?g_session.user.username:u82w(forU);
        if(idx<0) return "{\"ok\":false,\"err\":\"پیامی انتخاب نشده است.\"}";
        deleteOneMessage(who,(int)idx);
        return "{\"ok\":true}";
    }
    if(verb=="crm.messages.pin"){
        std::string forU; double idx=-1; bool pin=true;
        jsonGetString(body,"forUser",forU);
        jsonGetNumber(body,"idx",idx);
        pin=jsonGetBool(body,"pin",true);
        std::wstring who=forU.empty()?g_session.user.username:u82w(forU);
        if(idx<0) return "{\"ok\":false,\"err\":\"پیامی انتخاب نشده است.\"}";
        pinMessage(who,(int)idx,pin);
        return "{\"ok\":true}";
    }

    // ===================================================== SETTINGS / BACKUP ==
    if(verb=="crm.settings.get"){
        std::string o="{";
        o+="\"theme\":"+jstr(getSetting(L"theme",L"light"))+",";
        // v1.77: calm/warm palettes retired — only light/dark remain, so the
        // management settings no longer expose a "palette" to pick/change.
        o+="\"clinicName\":"+jstr(getSetting(L"clinic.name",L"درمان پلاس"))+",";
        o+="\"receptionMode\":"+jstr(getSetting(L"reception.mode",L"simple"))+",";
        o+="\"printCopies\":"+jstr(getSetting(L"print.copies",L"1"))+",";
        o+="\"savedMsgs\":"+jstr(getSetting(L"saved_msgs.enabled",L"0"))+"";
        o+="}";
        return o;
    }
    if(verb=="crm.settings.save"){
        std::string t;
        if(jsonGetString(body,"theme",t) && !t.empty())       setSetting(L"theme",u82w(t));
        // v1.77: "palette" is no longer accepted — calm/warm were retired and the
        // management settings can only toggle light/dark (theme).
        if(jsonGetString(body,"clinicName",t))                setSetting(L"clinic.name",u82w(t));
        if(jsonGetString(body,"receptionMode",t) && !t.empty())setSetting(L"reception.mode",u82w(t));
        if(jsonGetString(body,"printCopies",t))               setSetting(L"print.copies",u82w(t));
        if(jsonGetString(body,"savedMsgs",t))                 setSetting(L"saved_msgs.enabled",u82w(t));
        logLine(L"crm settings saved");
        return "{\"ok\":true}";
    }
    if(verb=="crm.backup"){
        // v1.82.0: the HTML backup page owns this flow. Keep the verb so an
        // old cached shell that still calls crm.backup does not crash — the
        // page itself navigates to Crm.pages.backup.
        return "{\"ok\":true,\"html\":true}";
    }
    if(verb=="crm.backup.pickSave")  return OpsBackup_PickSave();
    if(verb=="crm.backup.pickOpen")  return OpsBackup_PickOpen();
    if(verb=="crm.backup.create"){
        std::string p; jsonGetString(body,"path",p);
        return OpsBackup_Create(u82w(p));
    }
    if(verb=="crm.backup.analyze"){
        std::string p; jsonGetString(body,"path",p);
        return OpsBackup_Analyze(u82w(p));
    }
    if(verb=="crm.backup.restore"){
        std::string p; jsonGetString(body,"path",p);
        return OpsBackup_Restore(u82w(p));
    }
    if(verb=="crm.backup.progress")  return OpsBackup_ProgressJson();
    if(verb=="crm.calendar.list"){
        std::string fromU,toU,userU;
        jsonGetString(body,"from",fromU);
        jsonGetString(body,"to",toU);
        jsonGetString(body,"user",userU);
        return Calendar_ListJson(u82w(fromU),u82w(toU),u82w(userU));
    }
    if(verb=="crm.persons.assign"){
        std::string cU,dU,sU;
        jsonGetString(body,"code",cU);
        jsonGetString(body,"deptId",dU);
        jsonGetString(body,"subId",sU);
        std::wstring code=trim(u82w(cU));
        if(code.empty()) return "{\"ok\":false,\"err\":\"کد پرسنلی خالی است.\"}";
        PersonDef p;
        if(!personByCode(code,p)) return "{\"ok\":false,\"err\":\"پرسنل پیدا نشد.\"}";
        p.deptId=u82w(dU);
        p.subId=u82w(sU);
        std::wstring err;
        if(!updatePerson(p,err))
            return std::string("{\"ok\":false,\"err\":")+jstr(err)+"}";
        return "{\"ok\":true}";
    }
    if(verb=="crm.printConfig"){
        // open the print-settings modal (تنظیمات چاپ) on the UI thread.
        HWND owner=g_hFrame;
        RunOnUiThreadSync([&](){ PrintCfg_Open(owner); });
        return "{\"ok\":true}";
    }

    return "{\"ok\":false,\"err\":\"verb ناشناخته\"}";
}
