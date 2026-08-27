// ============================================================================
//  services.cpp — clinic Service Management data layer (v1.28.0)
//  Backing store for the «مدیریت خدمات» page in the management panel and the
//  service picker inside admission. One pipe-delimited line per service in
//  data/services.dat (UTF-8). The admission operator NEVER types a price — the
//  price always comes from this file (single source of truth).
//
//  Line format (columns):
//    code | name | category | dept | price | insType | desc | status |
//    created | modified | [future extra columns…]
// ============================================================================
#include "app.h"
#include <vector>
#include <string>

// ---- local pipe escaping helpers (mirror employees.cpp) --------------------
static std::wstring svcEsc(const std::wstring& s){
    std::wstring o=s; for(auto&c:o) if(c==L'|'||c==L'\n'||c==L'\r') c=L' '; return o;
}
static std::vector<std::wstring> svcSplit(const std::wstring& line){
    std::vector<std::wstring> f; std::wstring cur;
    for(wchar_t c:line){ if(c==L'|'){ f.push_back(cur); cur.clear(); } else cur+=c; }
    f.push_back(cur); return f;
}
static std::wstring servicesPath(){ return dataDir()+L"\\services.dat"; }

std::vector<ServiceDef> loadServices(){
    std::vector<ServiceDef> out;
    std::wstring all=readFileUtf8(servicesPath());
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=trim(all.substr(pos,e-pos)); pos=e+1;
        if(line.empty()) continue;
        auto f=svcSplit(line);
        if(f.size()<8) continue;
        ServiceDef s;
        s.code     = trim(f[0]);
        s.name     = f[1];
        s.category = f[2];
        s.dept     = f[3];
        s.price    = _wtoi64(f[4].c_str());
        s.insType  = f[5];
        s.desc     = f[6];
        s.status   = _wtoi(f[7].c_str());
        if(f.size()>8)  s.created  = f[8];
        if(f.size()>9)  s.modified = f[9];
        // v1.74 professional tariffs (columns 10..17). Older files have only 10
        // columns and load with every tariff at 0 / empty — unchanged behaviour.
        if(f.size()>10) s.insName     = f[10];
        if(f.size()>11) s.multiplier  = f[11];
        if(f.size()>12) s.priceFree   = _wtoi64(f[12].c_str());
        if(f.size()>13) s.priceFreeNew= _wtoi64(f[13].c_str());
        if(f.size()>14) s.priceGov    = _wtoi64(f[14].c_str());
        if(f.size()>15) s.priceGovNew = _wtoi64(f[15].c_str());
        if(f.size()>16) s.priceIns    = _wtoi64(f[16].c_str());
        if(f.size()>17) s.priceInsNew = _wtoi64(f[17].c_str());
        // v2.01 (Part C) — new service fields (columns 18..24). Older files
        // load with these empty/zero — unchanged behaviour.
        if(f.size()>18) s.shortName        = f[18];
        if(f.size()>19) s.lovingCode       = f[19];
        if(f.size()>20) s.equivCode        = f[20];
        if(f.size()>21) s.serviceId        = f[21];
        if(f.size()>22) s.revenueGroup     = f[22];
        if(f.size()>23) s.healthNationalId = f[23];
        if(f.size()>24) s.sectionId        = _wtoi(f[24].c_str());
        // v2.01 (Part C3) — historical rates (columns 25..26).
        if(f.size()>25) s.priceFreeOld     = _wtoi64(f[25].c_str());
        if(f.size()>26) s.priceGovOld      = _wtoi64(f[26].c_str());
        // §H: keep any future extra columns verbatim. v2.01 fields occupy
        // 18..26, so extras start at 27.
        for(size_t i=27;i<f.size();i++){ s.extra+=L"|"; s.extra+=svcEsc(f[i]); }
        out.push_back(s);
    }
    return out;
}

static void saveServices(const std::vector<ServiceDef>& v){
    std::wstring out;
    for(auto&s:v){
        wchar_t pb[32]; swprintf(pb,32,L"%lld",s.price);
        wchar_t sb[8];  swprintf(sb,8,L"%d",s.status);
        wchar_t pf[32];  swprintf(pf,32,L"%lld",s.priceFree);
        wchar_t pfn[32]; swprintf(pfn,32,L"%lld",s.priceFreeNew);
        wchar_t pg[32];  swprintf(pg,32,L"%lld",s.priceGov);
        wchar_t pgn[32]; swprintf(pgn,32,L"%lld",s.priceGovNew);
        wchar_t pi[32];  swprintf(pi,32,L"%lld",s.priceIns);
        wchar_t pin[32]; swprintf(pin,32,L"%lld",s.priceInsNew);
        wchar_t secid[16]; swprintf(secid,16,L"%d",s.sectionId);
        wchar_t pfo[32]; swprintf(pfo,32,L"%lld",s.priceFreeOld);
        wchar_t pgo[32]; swprintf(pgo,32,L"%lld",s.priceGovOld);
        out += svcEsc(s.code)+L"|"+svcEsc(s.name)+L"|"+svcEsc(s.category)+L"|"+
               svcEsc(s.dept)+L"|"+pb+L"|"+svcEsc(s.insType)+L"|"+svcEsc(s.desc)+L"|"+
               sb+L"|"+svcEsc(s.created)+L"|"+svcEsc(s.modified)+L"|"+
               svcEsc(s.insName)+L"|"+svcEsc(s.multiplier)+L"|"+
               pf+L"|"+pfn+L"|"+pg+L"|"+pgn+L"|"+pi+L"|"+pin+L"|"+
               svcEsc(s.shortName)+L"|"+svcEsc(s.lovingCode)+L"|"+
               svcEsc(s.equivCode)+L"|"+svcEsc(s.serviceId)+L"|"+
               svcEsc(s.revenueGroup)+L"|"+svcEsc(s.healthNationalId)+L"|"+
               secid+L"|"+pfo+L"|"+pgo+s.extra+L"\r\n";
    }
    writeFileUtf8(servicesPath(),out,false);
}

// v1.74: bulk persist used by the price round/adjust verb so a single operation
// rewrites services.dat once instead of N round-trips through updateService().
bool saveAllServices(const std::vector<ServiceDef>& v){ saveServices(v); return true; }

const ServiceDef* findService(const std::wstring& code){
    static std::vector<ServiceDef> cache;
    cache=loadServices();
    for(auto&s:cache) if(s.code==trim(code)) return &s;
    return nullptr;
}

bool addService(const ServiceDef& in, std::wstring& err){
    ServiceDef s=in;
    s.code=trim(s.code); s.name=trim(s.name);
    if(s.name.empty()){ err=L"نام خدمت نمی‌تواند خالی باشد."; return false; }
    auto v=loadServices();
    if(s.code.empty()){
        // auto code: SRV + (count+1)
        wchar_t b[24]; swprintf(b,24,L"SRV%04d",(int)v.size()+1); s.code=b;
        // ensure uniqueness even if a gap exists
        int n=(int)v.size()+1;
        auto exists=[&](const std::wstring& c){ for(auto&e:v) if(e.code==c) return true; return false; };
        while(exists(s.code)){ n++; swprintf(b,24,L"SRV%04d",n); s.code=b; }
    }
    for(auto&e:v) if(e.code==s.code){ err=L"این کد خدمت تکراری است."; return false; }
    std::wstring today=JalaliTodayKey();
    if(s.created.empty())  s.created=today;
    s.modified=today;
    v.push_back(s); saveServices(v);
    logLine(L"service added: "+s.code+L" "+s.name);
    return true;
}

bool updateService(const ServiceDef& in, std::wstring& err){
    ServiceDef s=in; s.code=trim(s.code); s.name=trim(s.name);
    if(s.code.empty()){ err=L"کد خدمت نامعتبر است."; return false; }
    if(s.name.empty()){ err=L"نام خدمت نمی‌تواند خالی باشد."; return false; }
    auto v=loadServices();
    for(auto&e:v) if(e.code==s.code){
        if(s.created.empty()) s.created=e.created;
        s.extra=e.extra;                 // preserve future columns
        s.modified=JalaliTodayKey();
        e=s; saveServices(v);
        logLine(L"service updated: "+s.code);
        return true;
    }
    err=L"خدمت با این کد یافت نشد."; return false;
}

bool removeService(const std::wstring& code){
    auto v=loadServices();
    for(size_t i=0;i<v.size();i++) if(v[i].code==trim(code)){
        v.erase(v.begin()+i); saveServices(v);
        logLine(L"service removed: "+code);
        return true;
    }
    return false;
}
