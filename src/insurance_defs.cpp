// ============================================================================
//  insurance_defs.cpp — user-managed insurance DEFINITION registry (v1.74).
//  Backing store for the «تعریف بیمه» page in the CRM panel: the full contract /
//  tariff / franchise / colour metadata for base (InsDef) and supplementary
//  (SuppDef) insurances. One pipe-delimited line per record in
//  data\insdefs.dat / data\suppdefs.dat (UTF-8). `idx` is the stable key — for
//  the predefined Iranian insurances it matches the position in INSURANCES[] /
//  SUPP_INSURANCES[] so existing patient records keep their meaning; user-added
//  rows take the next free idx. Ins_Percent/Supp_Percent honour a definition's
//  orgShare when present (else fall back to the hardcoded table).
// ============================================================================
#include "app.h"
#include <vector>
#include <string>
#include <algorithm>

// ---- local pipe escaping helpers (mirror services.cpp) --------------------
static std::wstring idefEsc(const std::wstring& s){
    std::wstring o=s; for(auto&c:o) if(c==L'|'||c==L'\n'||c==L'\r') c=L' '; return o;
}
static std::vector<std::wstring> idefSplit(const std::wstring& line){
    std::vector<std::wstring> f; std::wstring cur;
    for(wchar_t c:line){ if(c==L'|'){ f.push_back(cur); cur.clear(); } else cur+=c; }
    f.push_back(cur); return f;
}
static std::wstring insDefsPath(){ return dataDir()+L"\\insdefs.dat"; }
static std::wstring suppDefsPath(){ return dataDir()+L"\\suppdefs.dat"; }

static long long i64(const std::wstring& s){ return _wtoi64(s.c_str()); }
static int        i32(const std::wstring& s){ return _wtoi(s.c_str()); }

// =============================================================== base (InsDef)
std::vector<InsDef> loadInsDefs(){
    std::vector<InsDef> out;
    std::wstring all=readFileUtf8(insDefsPath());
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=trim(all.substr(pos,e-pos)); pos=e+1;
        if(line.empty()) continue;
        auto f=idefSplit(line);
        if(f.size()<4) continue;            // idx|sectionCode|insCode|orgShare minimum
        InsDef d;
        d.idx         = i32(f[0]);
        d.sectionCode = f.size()>1  ? f[1]  : L"";
        d.insCode     = f.size()>2  ? f[2]  : L"";
        d.orgShare    = f.size()>3  ? i32(f[3]) : -1;
        d.groupName   = f.size()>4  ? f[4]  : L"";
        d.flipName    = f.size()>5  ? f[5]  : L"";
        d.contractCode= f.size()>6  ? f[6]  : L"";
        d.insType     = f.size()>7  ? f[7]  : L"";
        d.tech        = f.size()>8  ? i64(f[8])  : 0;
        d.prof        = f.size()>9  ? i64(f[9])  : 0;
        d.cons        = f.size()>10 ? i64(f[10]) : 0;
        d.active      = f.size()>11 ? i32(f[11]) : 1;
        d.created     = f.size()>12 ? f[12] : L"";
        d.modified    = f.size()>13 ? f[13] : L"";
        out.push_back(d);
    }
    return out;
}

static void saveInsDefs(const std::vector<InsDef>& v){
    std::wstring out;
    for(const auto& d:v){
        wchar_t ib[16];  swprintf(ib,16,L"%d",d.idx);
        wchar_t ob[16];  swprintf(ob,16,L"%d",d.orgShare);
        wchar_t tb[32];  swprintf(tb,32,L"%lld",d.tech);
        wchar_t pb[32];  swprintf(pb,32,L"%lld",d.prof);
        wchar_t cb[32];  swprintf(cb,32,L"%lld",d.cons);
        wchar_t ab[8];   swprintf(ab,8,L"%d",d.active);
        out += std::wstring(ib)+L"|"+idefEsc(d.sectionCode)+L"|"+idefEsc(d.insCode)+L"|"+std::wstring(ob)+L"|"+
               idefEsc(d.groupName)+L"|"+idefEsc(d.flipName)+L"|"+idefEsc(d.contractCode)+L"|"+
               idefEsc(d.insType)+L"|"+std::wstring(tb)+L"|"+std::wstring(pb)+L"|"+std::wstring(cb)+L"|"+std::wstring(ab)+L"|"+
               idefEsc(d.created)+L"|"+idefEsc(d.modified)+L"\r\n";
    }
    writeFileUtf8(insDefsPath(),out,false);
}

bool upsertInsDef(const InsDef& in){
    auto v=loadInsDefs();
    std::wstring today=JalaliTodayKey();
    for(auto& d:v) if(d.idx==in.idx){
        std::wstring created=d.created;
        d=in; d.created=created; d.modified=today;
        saveInsDefs(v); return true;
    }
    InsDef d=in;
    if(d.idx<0){                           // assign the next free stable idx
        int maxIdx=-1; for(const auto& x:v) if(x.idx>maxIdx) maxIdx=x.idx;
        d.idx=maxIdx+1;
    }
    if(d.created.empty()) d.created=today;
    d.modified=today;
    v.push_back(d); saveInsDefs(v);
    logLine(L"insdef upsert: idx "+std::to_wstring(d.idx));
    return true;
}

bool deleteInsDef(int idx){
    auto v=loadInsDefs();
    size_t before=v.size();
    v.erase(std::remove_if(v.begin(),v.end(),
            [idx](const InsDef&d){return d.idx==idx;}), v.end());
    if(v.size()==before) return false;
    saveInsDefs(v);
    logLine(L"insdef deleted: idx "+std::to_wstring(idx));
    return true;
}

const InsDef* insDefByIndex(int idx){
    static std::vector<InsDef> cache;
    cache=loadInsDefs();
    for(const auto& d:cache) if(d.idx==idx) return &d;
    return nullptr;
}

// ===================================================== supplementary (SuppDef)
std::vector<SuppDef> loadSuppDefs(){
    std::vector<SuppDef> out;
    std::wstring all=readFileUtf8(suppDefsPath());
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=trim(all.substr(pos,e-pos)); pos=e+1;
        if(line.empty()) continue;
        auto f=idefSplit(line);
        if(f.size()<4) continue;
        SuppDef d;
        d.idx             = i32(f[0]);
        d.sectionCode     = f.size()>1  ? f[1]  : L"";
        d.insSpec         = f.size()>2  ? f[2]  : L"";
        d.name            = f.size()>3  ? f[3]  : L"";
        d.tariffType      = f.size()>4  ? f[4]  : L"";
        d.franchise       = f.size()>5  ? f[5]  : L"";
        d.franchiseDefault= f.size()>6  ? i32(f[6])  : 1;
        d.byLaw           = f.size()>7  ? i32(f[7])  : 0;
        d.ceiling         = f.size()>8  ? i64(f[8])  : 0;
        d.insTypeCode     = f.size()>9  ? f[9]  : L"";
        d.contractCode    = f.size()>10 ? f[10] : L"";
        d.tech            = f.size()>11 ? i64(f[11]) : 0;
        d.prof            = f.size()>12 ? i64(f[12]) : 0;
        d.cons            = f.size()>13 ? i64(f[13]) : 0;
        d.franchiseOrgPct = f.size()>14 ? i32(f[14]) : 0;
        d.defaultOff      = f.size()>15 ? i32(f[15]) : 0;
        d.priceCalcType   = f.size()>16 ? f[16] : L"";
        d.difference      = f.size()>17 ? i32(f[17]) : 0;
        d.color           = f.size()>18 ? f[18] : L"";
        d.validityDate    = f.size()>19 ? f[19] : L"";
        d.username        = f.size()>20 ? f[20] : L"";
        d.nationalId      = f.size()>21 ? f[21] : L"";
        d.booklet         = f.size()>22 ? i32(f[22]) : 0;
        d.fileName        = f.size()>23 ? f[23] : L"";
        d.active          = f.size()>24 ? i32(f[24]) : 1;
        d.created         = f.size()>25 ? f[25] : L"";
        d.modified        = f.size()>26 ? f[26] : L"";
        out.push_back(d);
    }
    return out;
}

static void saveSuppDefs(const std::vector<SuppDef>& v){
    std::wstring out;
    for(const auto& d:v){
        wchar_t ib[16];  swprintf(ib,16,L"%d",d.idx);
        wchar_t fd[8];   swprintf(fd,8,L"%d",d.franchiseDefault);
        wchar_t bl[8];   swprintf(bl,8,L"%d",d.byLaw);
        wchar_t cl[32];  swprintf(cl,32,L"%lld",d.ceiling);
        wchar_t tb[32];  swprintf(tb,32,L"%lld",d.tech);
        wchar_t pb[32];  swprintf(pb,32,L"%lld",d.prof);
        wchar_t cb[32];  swprintf(cb,32,L"%lld",d.cons);
        wchar_t fp[8];   swprintf(fp,8,L"%d",d.franchiseOrgPct);
        wchar_t df[8];   swprintf(df,8,L"%d",d.defaultOff);
        wchar_t di[8];   swprintf(di,8,L"%d",d.difference);
        wchar_t bk[8];   swprintf(bk,8,L"%d",d.booklet);
        wchar_t ab[8];   swprintf(ab,8,L"%d",d.active);
        out += std::wstring(ib)+L"|"+idefEsc(d.sectionCode)+L"|"+idefEsc(d.insSpec)+L"|"+idefEsc(d.name)+L"|"+
               idefEsc(d.tariffType)+L"|"+idefEsc(d.franchise)+L"|"+std::wstring(fd)+L"|"+std::wstring(bl)+L"|"+std::wstring(cl)+L"|"+
               idefEsc(d.insTypeCode)+L"|"+idefEsc(d.contractCode)+L"|"+std::wstring(tb)+L"|"+std::wstring(pb)+L"|"+std::wstring(cb)+L"|"+
               std::wstring(fp)+L"|"+std::wstring(df)+L"|"+idefEsc(d.priceCalcType)+L"|"+std::wstring(di)+L"|"+idefEsc(d.color)+L"|"+
               idefEsc(d.validityDate)+L"|"+idefEsc(d.username)+L"|"+idefEsc(d.nationalId)+L"|"+
               std::wstring(bk)+L"|"+idefEsc(d.fileName)+L"|"+std::wstring(ab)+L"|"+
               idefEsc(d.created)+L"|"+idefEsc(d.modified)+L"\r\n";
    }
    writeFileUtf8(suppDefsPath(),out,false);
}

bool upsertSuppDef(const SuppDef& in){
    auto v=loadSuppDefs();
    std::wstring today=JalaliTodayKey();
    for(auto& d:v) if(d.idx==in.idx){
        std::wstring created=d.created;
        d=in; d.created=created; d.modified=today;
        saveSuppDefs(v); return true;
    }
    SuppDef d=in;
    if(d.idx<0){
        int maxIdx=-1; for(const auto& x:v) if(x.idx>maxIdx) maxIdx=x.idx;
        d.idx=maxIdx+1;
    }
    if(d.created.empty()) d.created=today;
    d.modified=today;
    v.push_back(d); saveSuppDefs(v);
    logLine(L"suppdef upsert: idx "+std::to_wstring(d.idx));
    return true;
}

bool deleteSuppDef(int idx){
    auto v=loadSuppDefs();
    size_t before=v.size();
    v.erase(std::remove_if(v.begin(),v.end(),
            [idx](const SuppDef&d){return d.idx==idx;}), v.end());
    if(v.size()==before) return false;
    saveSuppDefs(v);
    logLine(L"suppdef deleted: idx "+std::to_wstring(idx));
    return true;
}

const SuppDef* suppDefByIndex(int idx){
    static std::vector<SuppDef> cache;
    cache=loadSuppDefs();
    for(const auto& d:cache) if(d.idx==idx) return &d;
    return nullptr;
}

void InsDefs_SeedDefaults(){
    if(GetFileAttributesW(insDefsPath().c_str())==INVALID_FILE_ATTRIBUTES){
        std::vector<InsDef> v;
        std::wstring today=JalaliTodayKey();
        for(int i=0;i<N_INSURANCES;i++){
            InsDef d; d.idx=i; d.groupName=INSURANCES[i].name;
            d.flipName=INSURANCES[i].name; d.orgShare=INSURANCES[i].pct;
            d.active=1; d.insType=i==0?L"\u0622\u0632\u0627\u062f":L"\u062f\u0648\u0644\u062a\u06cc";
            d.created=today; d.modified=today;
            v.push_back(d);
        }
        saveInsDefs(v);
    }
    if(GetFileAttributesW(suppDefsPath().c_str())==INVALID_FILE_ATTRIBUTES){
        std::vector<SuppDef> v;
        std::wstring today=JalaliTodayKey();
        for(int i=0;i<N_SUPP;i++){
            SuppDef d; d.idx=i; d.name=SUPP_INSURANCES[i].name;
            d.insSpec=SUPP_INSURANCES[i].name; d.active=1;
            d.franchiseOrgPct=SUPP_INSURANCES[i].pct;
            d.created=today; d.modified=today;
            v.push_back(d);
        }
        saveSuppDefs(v);
    }
}
