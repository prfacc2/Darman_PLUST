// ============================================================================
//  sections.cpp — implementation of the Sections registry (release 1.4.0, §2).
//  Storage: data\sections.dat, one record per line, '|' separated:
//      id|code|name_fa|kind|is_active|created_at|updated_at
//  Lines are escaped so the separator never appears inside a field.
// ============================================================================
#include "app.h"
#include "sections.h"
#include "ui_kit.h"
#include "clinic_ops.h"   // v2.07: Ops_IsReception for Sections_AccountRoleLabel
#include <algorithm>

static std::wstring sec_path(){ return dataDir()+L"\\sections.dat"; }

static std::wstring sec_esc(const std::wstring& s){
    std::wstring o=s; for(auto&c:o) if(c==L'|'||c==L'\n'||c==L'\r') c=L' '; return o;
}
static std::vector<std::wstring> sec_split(const std::wstring& s, wchar_t sep){
    std::vector<std::wstring> out; size_t pos=0;
    while(true){ size_t e=s.find(sep,pos);
        if(e==std::wstring::npos){ out.push_back(s.substr(pos)); break; }
        out.push_back(s.substr(pos,e-pos)); pos=e+1; }
    return out;
}
static std::wstring sec_now(){
    SYSTEMTIME st=iranNow(); wchar_t b[40];
    swprintf(b,40,L"%04d-%02d-%02dT%02d:%02d:%02d",
             st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
    return b;
}

static bool sec_writeAll(const std::vector<Section>& v);

static std::vector<Section> sec_readAll(){
    std::vector<Section> v;
    std::wstring all = readFileUtf8(sec_path());
    size_t pos=0;
    while(pos < all.size()){
        size_t e = all.find(L'\n', pos);
        if(e==std::wstring::npos) e=all.size();
        std::wstring line = all.substr(pos, e-pos);
        pos = e+1;
        while(!line.empty() && (line.back()==L'\r'||line.back()==L'\n')) line.pop_back();
        if(line.empty()) continue;
        auto f = sec_split(line, L'|');
        if(f.size() < 3) continue;
        Section s;
        s.id        = f.size()>0 ? _wtoi(f[0].c_str()) : 0;
        s.code      = f.size()>1 ? f[1] : L"";
        s.name_fa   = f.size()>2 ? f[2] : L"";
        s.kind      = f.size()>3 ? f[3] : L"";
        s.is_active = f.size()>4 ? _wtoi(f[4].c_str()) : 1;
        s.created_at= f.size()>5 ? f[5] : L"";
        s.updated_at= f.size()>6 ? f[6] : L"";
        // §7 (1.14.0): optional 8th field = net_meta. Older files have only 7
        // fields and load unchanged (net_meta stays empty).
        if(f.size() >= 8) s.net_meta = f[7];
        // v1.74: optional 9th field = parent_id (subsection). Older files have
        // only 7-8 fields and load unchanged (parent_id stays 0 = top-level).
        if(f.size() >= 9) s.parent_id = _wtoi(f[8].c_str());
        // v1.93: optional 10th field = cashier_tab (ثبت پذیرش در صندوق). Older
        // files have only 9 fields and load unchanged (cashier_tab stays 0/OFF).
        if(f.size() >= 10) s.cashier_tab = _wtoi(f[9].c_str());
        // v1.97: optional 11th field = has_pos (دستگاه پوز). Older files load
        // unchanged (has_pos stays 0/OFF).
        if(f.size() >= 11) s.has_pos = _wtoi(f[10].c_str());
        // v2.07 §7.1: optional 12th field = زیربخش پذیرش. Older files load
        // unchanged (recept_sub stays 0/OFF).
        if(f.size() >= 12) s.recept_sub = _wtoi(f[11].c_str());
        v.push_back(s);
    }
    int maxId=0;
    for(const auto& x:v) if(x.id>maxId) maxId=x.id;
    bool dirty=false;
    for(auto& s:v){
        if(s.id<=0){ s.id=++maxId; dirty=true; }
        if(s.code.empty()){
            const wchar_t* pre=Sections_CategoryCode(s.kind);
            int mx=0; std::wstring p=pre;
            for(const auto& y:v){
                if(y.code.compare(0,p.size(),p)==0){
                    int n=_wtoi(y.code.c_str()+p.size());
                    if(n>mx) mx=n;
                }
            }
            wchar_t b[24]; swprintf(b,24,L"%s%02d",pre,mx+1);
            s.code=b; dirty=true;
        }
    }
    if(dirty) sec_writeAll(v);
    return v;
}

static bool sec_writeAll(const std::vector<Section>& v){
    std::wstring out;
    for(const auto& s : v){
        wchar_t idb[16]; swprintf(idb,16,L"%d",s.id);
        wchar_t act[8];  swprintf(act,8,L"%d",s.is_active);
        wchar_t pid[16]; swprintf(pid,16,L"%d",s.parent_id);   // v1.74 subsection
        out += idb; out += L'|';
        out += sec_esc(s.code);    out += L'|';
        out += sec_esc(s.name_fa); out += L'|';
        out += sec_esc(s.kind);    out += L'|';
        out += act;                out += L'|';
        out += sec_esc(s.created_at);out += L'|';
        out += sec_esc(s.updated_at);out += L'|';
        out += sec_esc(s.net_meta); out += L'|';
        out += pid; out += L'|';                          // v1.74 subsection
        out += std::to_wstring(s.cashier_tab); out += L'|';       // v1.93 cashier tab
        out += std::to_wstring(s.has_pos); out += L'|';            // v1.97 POS flag
        out += std::to_wstring(s.recept_sub); out += L"\r\n";      // v2.07 زیربخش پذیرش

    }
    return writeFileUtf8(sec_path(), out, false);
}

// v1.17.0: one-time migration. Earlier builds (≤1.16.1) seeded NINE demo
// departments (REC01/REC02/APR01/INJ01/LAB01/PHR01/BIL01/RAD01/PHY01) that were
// never actually defined by the clinic. If the stored file is EXACTLY that
// untouched demo set, collapse it to the single real «پذیرش» section so the
// print-designer picker stops listing phantom departments. If the admin has
// touched the set in any way (added/removed/renamed), we leave it untouched.
static bool sec_isLegacyDemoSet(const std::vector<Section>& v){
    static const wchar_t* demo[] = {
        L"REC01",L"REC02",L"APR01",L"INJ01",L"LAB01",
        L"PHR01",L"BIL01",L"RAD01",L"PHY01" };
    const int N = (int)(sizeof(demo)/sizeof(demo[0]));
    if((int)v.size() != N) return false;
    // codes must match the demo set exactly (order-independent)
    for(int i=0;i<N;i++){
        bool found=false;
        for(const auto& s : v) if(s.code==demo[i]){ found=true; break; }
        if(!found) return false;
    }
    return true;
}

void Sections_Init(){
    std::vector<Section> v = sec_readAll();

    // Migrate an untouched legacy demo set down to the single real section.
    if(!v.empty() && sec_isLegacyDemoSet(v) &&
       getSetting(L"sections_demo_migrated", L"") != L"1"){
        std::wstring now = sec_now();
        std::vector<Section> only;
        Section x; x.id=1; x.code=L"REC01";
        x.name_fa=L"\u067e\u0630\u06cc\u0631\u0634"; x.kind=L"reception";
        x.is_active=1; x.created_at=now; x.updated_at=now;
        only.push_back(x);
        sec_writeAll(only);
        setSetting(L"sections_demo_migrated", L"1");
        return;
    }

    if(!v.empty()) return;     // already seeded
    struct Seed { const wchar_t* code; const wchar_t* name; const wchar_t* kind; };
    // §7 (1.14.0): seeds use the stable category-code scheme. Codes are durable
    // routing/sync keys; the Persian names are display-only.
    //
    // v1.17.0 — ONLY the single REAL, DEFINED section is seeded: «پذیرش». The
    // clinic has exactly one reception section, so the print-designer section
    // picker (and every other consumer) must surface exactly one row, never a
    // list of departments that were never created. Additional sections are
    // added by the admin from the management panel and persist in sections.dat
    // — they are NOT fabricated here.
    static const Seed seeds[] = {
        { L"REC01", L"\u067e\u0630\u06cc\u0631\u0634", L"reception" },
    };
    std::wstring now = sec_now();
    int id=1;
    for(const auto& s : seeds){
        Section x; x.id=id++; x.code=s.code; x.name_fa=s.name; x.kind=s.kind;
        x.is_active=1; x.created_at=now; x.updated_at=now;
        v.push_back(x);
    }
    sec_writeAll(v);
}

int Sections_All(std::vector<Section>& out){
    out = sec_readAll();
    std::sort(out.begin(), out.end(), [](const Section&a,const Section&b){return a.id<b.id;});
    return (int)out.size();
}

int Sections_Find(const std::wstring& query, std::vector<Section>& out){
    out.clear();
    std::vector<Section> all = sec_readAll();
    std::sort(all.begin(), all.end(), [](const Section&a,const Section&b){return a.id<b.id;});
    std::wstring q = uikit::NormalizeFa(query);
    if(q.empty()){ out = all; return (int)out.size(); }
    for(const auto& s : all){
        std::wstring code = uikit::NormalizeFa(s.code);
        std::wstring name = uikit::NormalizeFa(s.name_fa);
        if(code.find(q)!=std::wstring::npos || name.find(q)!=std::wstring::npos)
            out.push_back(s);
    }
    return (int)out.size();
}

static std::wstring sec_nextCode(const std::vector<Section>& v, const std::wstring& kind){
    const wchar_t* pre=Sections_CategoryCode(kind);
    int mx=0; std::wstring p=pre;
    for(const auto& y:v){
        if(y.code.compare(0,p.size(),p)==0){
            int n=_wtoi(y.code.c_str()+p.size());
            if(n>mx) mx=n;
        }
    }
    wchar_t b[24]; swprintf(b,24,L"%s%02d",pre,mx+1);
    return b;
}

int Sections_Upsert(const Section& s){
    std::vector<Section> v = sec_readAll();
    std::wstring now = sec_now();
    if(s.id > 0){
        for(auto& x : v){
            if(x.id==s.id){
                std::wstring created = x.created_at;
                std::wstring keepCode = x.code;
                x = s; x.created_at = created; x.updated_at = now;
                if(x.code.empty()) x.code = keepCode;
                if(x.code.empty()) x.code = sec_nextCode(v, x.kind);
                sec_writeAll(v); return x.id;
            }
        }
    }
    // insert
    int maxId=0; for(const auto& x : v) maxId = (x.id>maxId)?x.id:maxId;
    Section x = s; if(x.id<=0) x.id = maxId+1;
    if(x.code.empty()) x.code = sec_nextCode(v, x.kind);
    if(x.created_at.empty()) x.created_at = now;
    x.updated_at = now;
    v.push_back(x);
    sec_writeAll(v);
    return x.id;
}

int Sections_Delete(int id){
    std::vector<Section> v = sec_readAll();
    size_t before = v.size();
    v.erase(std::remove_if(v.begin(), v.end(),
            [id](const Section&x){return x.id==id;}), v.end());
    if(v.size()==before) return 0;
    sec_writeAll(v);
    return 1;
}

const wchar_t* Sections_KindLabel(const std::wstring& kind){
    if(kind==L"reception")   return L"\u067e\u0630\u06cc\u0631\u0634";
    if(kind==L"appointment") return L"\u0646\u0648\u0628\u062a\u200c\u062f\u0647\u06cc";
    if(kind==L"injection")   return L"\u062a\u0632\u0631\u06cc\u0642\u0627\u062a";
    if(kind==L"lab")         return L"\u0622\u0632\u0645\u0627\u06cc\u0634\u06af\u0627\u0647";
    if(kind==L"pharmacy")    return L"\u062f\u0627\u0631\u0648\u062e\u0627\u0646\u0647";
    if(kind==L"billing")     return L"\u0635\u0646\u062f\u0648\u0642";
    if(kind==L"radiology")   return L"\u0631\u0627\u062f\u06cc\u0648\u0644\u0648\u0698\u06cc";
    if(kind==L"physio")      return L"\u0641\u06cc\u0632\u06cc\u0648\u062a\u0631\u0627\u067e\u06cc";
    if(kind.empty() || kind==L"other") return L"\u0633\u0627\u06cc\u0631";
    static thread_local std::wstring buf;
    buf=kind;
    return buf.c_str();
}

// §7 (1.14.0): stable, durable category code for a section `kind`. These short
// prefixes are the canonical routing/sync keys and never change with display
// names. Unknown kinds collapse to a stable "GEN" (general) so a code is always
// produced.
const wchar_t* Sections_CategoryCode(const std::wstring& kind){
    if(kind==L"reception" || kind.find(L"\u067e\u0630\u06cc\u0631\u0634")!=std::wstring::npos) return L"REC";
    if(kind==L"appointment" || kind.find(L"\u0646\u0648\u0628\u062a")!=std::wstring::npos) return L"APR";
    if(kind==L"lab" || kind.find(L"\u0622\u0632\u0645\u0627\u06cc\u0634")!=std::wstring::npos) return L"LAB";
    if(kind==L"injection" || kind.find(L"\u062a\u0632\u0631\u06cc\u0642")!=std::wstring::npos) return L"INJ";
    if(kind==L"pharmacy" || kind.find(L"\u062f\u0627\u0631\u0648\u062e\u0627\u0646\u0647")!=std::wstring::npos) return L"PHR";
    if(kind==L"billing" || kind.find(L"\u0635\u0646\u062f\u0648\u0642")!=std::wstring::npos) return L"BIL";
    if(kind==L"radiology" || kind.find(L"\u0631\u0627\u062f\u06cc\u0648")!=std::wstring::npos) return L"RAD";
    if(kind==L"physio" || kind.find(L"\u0641\u06cc\u0632\u06cc\u0648")!=std::wstring::npos) return L"PHY";
    return L"GEN";
}

bool Sections_HasPos(int id){
    if(id<=0) return false;
    std::vector<Section> all = sec_readAll();
    for(const auto& s : all) if(s.id==id) return s.has_pos!=0;
    return false;
}

// v2.07 §7.1 — «زیربخش پذیرش» on THIS id only; NO parent inheritance
// (mirrors Sections_HasPos exactly). Missing/unknown → false.
bool Sections_IsReceptionSub(int id){
    if(id<=0) return false;
    std::vector<Section> all = sec_readAll();
    for(const auto& s : all) if(s.id==id) return s.recept_sub!=0;
    return false;
}

// v2.07 §7.2 — deterministic account-role label for the header's second line.
//   زیربخش پذیرش  → «پذیرش » + the PARENT section's name_fa (e.g. «پذیرش آزمایشگاه»)
//   top-level reception section → «پذیرش»
//   anything else → empty (caller falls through to the next precedence step).
// The parent_id is resolved at call time; never derived from `code` and never
// from a display name that could be renamed without going through here.
std::wstring Sections_AccountRoleLabel(int sectionId){
    if(sectionId<=0) return L"";
    std::vector<Section> all = sec_readAll();
    const Section* me=nullptr;
    for(const auto& s : all) if(s.id==sectionId){ me=&s; break; }
    if(!me) return L"";
    if(me->recept_sub!=0 && me->parent_id>0){
        for(const auto& s : all)
            if(s.id==me->parent_id) return L"پذیرش "+s.name_fa;
        return L"پذیرش";
    }
    if(Ops_IsReception(*me)) return L"پذیرش";
    return L"";
}

std::wstring Sections_CodePrefix(const Section& s){
    // Leading alpha run of the stored code, e.g. "REC01" -> "REC".
    std::wstring p;
    for(wchar_t c : s.code){
        if((c>=L'A'&&c<=L'Z') || (c>=L'a'&&c<=L'z')) p += (wchar_t)towupper(c);
        else break;
    }
    if(!p.empty()) return p;
    // No alpha prefix in the code — derive a durable one from the kind.
    return Sections_CategoryCode(s.kind);
}
