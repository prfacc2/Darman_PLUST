// ============================================================================
//  persons.cpp — v1.79.0 personnel registry («تعریف پرسنل»)
//  ----------------------------------------------------------------------------
//  A PERSON exists independently of any login account (the account is attached
//  later in «تعریف حساب کاربری»). One flat file: data\persons.dat — key=value
//  blocks separated by a blank line (same convention as doctors.dat, so older
//  tooling can read it and §H forward-compat keeps unknown keys on save).
//
//  کد پرسنلی: PREFIX_NNNN where PREFIX is the first two letters of the owning
//  department's name transliterated to Latin (Persian letters included):
//      آزمایشگاه → AZ_0001     تزریقات → TZ_0001     پذیرش → PZ_0001
//  Persons with no department (در حالت تعلیق) get the neutral PER_ prefix.
//  Manual codes are honoured as long as they are unique.
// ============================================================================
#include "app.h"
#include "sections.h"
#include "clinic_ops.h"
#include <stdio.h>
#include <algorithm>

static std::wstring personsPath(){ return dataDir()+L"\\persons.dat"; }

// ---- Persian letter → Latin (single-char) transliteration ------------------
//  Digraphs collapse to their first character so prefixes stay 2 chars long.
static wchar_t faToLatin(wchar_t c){
    switch(c){
        case L'ا': case L'آ': case L'أ': case L'إ': case L'ع': return L'A';
        case L'ب': return L'B';
        case L'پ': return L'P';
        case L'ت': case L'ط': case L'ظ': return L'T';
        case L'ث': case L'س': case L'ص': case L'ش': return L'S';
        case L'ج': return L'J';
        case L'چ': return L'C';
        case L'ح': case L'ه': return L'H';
        case L'خ': return L'K';
        case L'د': return L'D';
        case L'ذ': case L'ز': case L'ژ': case L'ض': return L'Z';
        case L'ر': return L'R';
        case L'غ': case L'گ': return L'G';
        case L'ف': return L'F';
        case L'ق': return L'Q';
        case L'ک': return L'K';
        case L'ل': return L'L';
        case L'م': return L'M';
        case L'ن': return L'N';
        case L'و': return L'V';
        case L'ی': case L'ي': return L'Y';
        default: break;
    }
    if((c>=L'a'&&c<=L'z')) return (wchar_t)(c-L'a'+L'A');
    if((c>=L'A'&&c<=L'Z')) return c;
    return 0;
}

//  Two-letter Latin prefix from a department name (Persian or Latin).
//  «آزمایشگاه»→AZ, «تزریقات»→TZ, «پذیرش»→PZ, «Lab»→LA. Fallback: DEP.
std::wstring deptCodePrefix(const std::wstring& deptName){
    std::wstring p;
    for(wchar_t c:deptName){
        wchar_t l=faToLatin(c);
        if(!l) continue;
        p+=l;
        if(p.size()>=2) break;
    }
    if(p.empty()) p=L"DEP";
    return p;
}

//  v1.79.0: persons.dat is tiny, but list-building code paths (crmUserJson per
//  row, the header identity, …) would otherwise re-read it many times per
//  render. Cache by file write-time: reload only when the file changed.
static std::vector<PersonDef> s_personsCache;
static FILETIME s_personsStamp={0,0};
static bool     s_personsCacheValid=false;
static void invalidatePersonsCache(){ s_personsCacheValid=false; }

std::vector<PersonDef> loadPersons(){
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if(GetFileAttributesExW(personsPath().c_str(),GetFileExInfoStandard,&fa)){
        if(s_personsCacheValid &&
           fa.ftLastWriteTime.dwHighDateTime==s_personsStamp.dwHighDateTime &&
           fa.ftLastWriteTime.dwLowDateTime ==s_personsStamp.dwLowDateTime)
            return s_personsCache;
        s_personsStamp=fa.ftLastWriteTime;
    } else {
        s_personsStamp.dwHighDateTime=s_personsStamp.dwLowDateTime=0;
        if(s_personsCacheValid) return s_personsCache;   // deleted → keep cache
    }
    std::vector<PersonDef> out;
    std::wstring all=readFileUtf8(personsPath());
    PersonDef cur; bool inBlock=false;
    size_t pos=0;
    auto flush=[&](){ if(inBlock && !trim(cur.code).empty()) out.push_back(cur);
                      cur=PersonDef(); inBlock=false; };
    while(pos<=all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=trim(all.substr(pos,e-pos)); pos=e+1;
        if(line.empty()){ flush(); continue; }
        size_t eq=line.find(L'=');
        if(eq==std::wstring::npos){ cur.extraKv+=line+L"\r\n"; continue; }
        std::wstring k=trim(line.substr(0,eq)), v=trim(line.substr(eq+1));
        inBlock=true;
        if(k==L"code")           cur.code=v;
        else if(k==L"firstName") cur.firstName=v;
        else if(k==L"lastName")  cur.lastName=v;
        else if(k==L"fatherName")cur.fatherName=v;
        else if(k==L"nationalId")cur.nationalId=v;
        else if(k==L"birthDate") cur.birthDate=v;
        else if(k==L"address")   cur.address=v;
        else if(k==L"mobile")    cur.mobile=v;
        else if(k==L"phone")     cur.phone=v;
        else if(k==L"email")     cur.email=v;
        else if(k==L"education") cur.education=v;
        else if(k==L"field")     cur.field=v;
        else if(k==L"degree")    cur.degree=v;
        else if(k==L"roleKind")  cur.roleKind=_wtoi(v.c_str());
        else if(k==L"roleCustom")cur.roleCustom=v;
        else if(k==L"position")  cur.position=v;
        else if(k==L"deptId")    cur.deptId=v;
        else if(k==L"subId")     cur.subId=v;
        else if(k==L"photo")     cur.photo=v;
        else if(k==L"username")  cur.username=v;
        else if(k==L"created")   cur.created=v;
        else cur.extraKv+=line+L"\r\n";   // §H forward-compat
        if(pos>=all.size()) break;
    }
    flush();
    s_personsCache=out; s_personsCacheValid=true;
    return out;
}

static void savePersons(const std::vector<PersonDef>& v){
    std::wstring out;
    auto kv=[&](const wchar_t* k, const std::wstring& v){ out+=k; out+=L"="; out+=v; out+=L"\r\n"; };
    for(auto& p:v){
        kv(L"code",p.code);           kv(L"firstName",p.firstName);
        kv(L"lastName",p.lastName);   kv(L"fatherName",p.fatherName);
        kv(L"nationalId",p.nationalId); kv(L"birthDate",p.birthDate);
        kv(L"address",p.address);     kv(L"mobile",p.mobile);
        kv(L"phone",p.phone);         kv(L"email",p.email);
        kv(L"education",p.education); kv(L"field",p.field);
        kv(L"degree",p.degree);
        wchar_t rk[8]; swprintf(rk,8,L"%d",p.roleKind);
        kv(L"roleKind",rk);           kv(L"roleCustom",p.roleCustom);
        kv(L"position",p.position);   kv(L"deptId",p.deptId);
        kv(L"subId",p.subId);
        kv(L"photo",p.photo);         kv(L"username",p.username);
        kv(L"created",p.created);
        out+=p.extraKv;                 // §H: keep unknown keys verbatim
        out+=L"\r\n";
    }
    writeFileUtf8(personsPath(),out,false);
    invalidatePersonsCache();
}

bool personByCode(const std::wstring& code, PersonDef& out){
    auto v=loadPersons();
    for(auto& p:v) if(p.code==code){ out=p; return true; }
    return false;
}
bool personByUsername(const std::wstring& username, PersonDef& out){
    if(username.empty()) return false;
    auto v=loadPersons();
    for(auto& p:v) if(p.username==username){ out=p; return true; }
    return false;
}

//  v1.79.0: header identity helper — the مقام/سمت line shown next to the user
//  name. Resolved ONCE at login (main.cpp caches it in g_session.title); never
//  call this from a paint/timer path.
std::wstring resolveSessionTitle(const User& u){
    std::wstring pos;
    PersonDef p;
    bool haveP=personByUsername(u.username,p);
    // v2.07 §6.1 — deterministic precedence for the SECOND identity line:
    //   1) the مقام/سمت on the personnel record, if set
    //   2) the employee-profile role, if set
    //   3) the section-derived label (Sections_AccountRoleLabel — e.g. «پذیرش آزمایشگاه»)
    //   4) the access-level fallback (پذیرش / مدیریت / کارآموز)
    if(haveP && !p.position.empty())
        pos = p.position;
    if(pos.empty()){
        EmpProfile e=loadEmpProfile(u.username);
        pos=e.position;
    }
    if(pos.empty() && haveP){
        // §6.1(3): section-derived label — a زیربخش پذیرش operator shows
        // «پذیرش <parent section>», a top-level reception operator «پذیرش».
        std::vector<Section> all; Sections_All(all);
        int did=_wtoi(p.deptId.c_str());
        int sid=_wtoi(p.subId.c_str());
        for(const auto& s:all){
            if(s.id!=did && s.id!=sid) continue;
            std::wstring lbl=Sections_AccountRoleLabel(s.id);
            if(!lbl.empty()){ pos=lbl; break; }
        }
    }
    if(pos.empty()){
        if(u.role==2) pos=L"مدیر سامانه";
        else if(u.role==1) pos=L"مدیریت";
        else {
            // §6.1(4): access-level fallback. کارآموز when the personnel
            // record says trainee; پذیرش for reception-scope users; else پرسنل.
            bool rec=false;
            if(haveP){
                if(p.roleKind==3){ pos=L"کارآموز"; return pos; }
                std::vector<Section> all; Sections_All(all);
                int did=_wtoi(p.deptId.c_str());
                int sid=_wtoi(p.subId.c_str());
                for(const auto& s:all){
                    if(s.id!=did && s.id!=sid) continue;
                    if(Ops_IsReception(s)){ rec=true; break; }
                }
            }
            if(!rec && u.dept.find(L"پذیرش")!=std::wstring::npos) rec=true;
            pos = rec ? L"پذیرش" : L"پرسنل";
        }
    }
    // §6.4: the header shows the FULL NAME — never the login username. When we
    // only have a username to show, log the data gap so it is visible.
    if(u.fullname.empty() && !u.username.empty())
        logError(L"resolveSessionTitle: user '"+u.username+L"' has no fullname on record");
    return pos;
}

//  next free numeric suffix for a prefix (scans existing codes)
static int nextPersonSeq(const std::wstring& prefix){
    int mx=0;
    auto v=loadPersons();
    std::wstring head=prefix+L"_";
    for(auto& p:v){
        if(p.code.compare(0,head.size(),head)==0){
            int n=_wtoi(p.code.c_str()+head.size());
            if(n>mx) mx=n;
        }
    }
    return mx+1;
}

//  resolve the display name of a section/subsection id from the clinical tree
static std::wstring sectionNameById(const std::wstring& id){
    if(id.empty()) return L"";
    int want=_wtoi(id.c_str());
    if(want<=0) return L"";
    std::vector<Section> all; Sections_All(all);
    for(auto& s2:all) if(s2.id==want) return s2.name_fa;
    return L"";
}
//  v1.80.0: prefix comes from the زیربخش name when set, else the بخش name.
std::wstring nextPersonCode(const std::wstring& deptId, const std::wstring& subId){
    std::wstring prefix=L"PER";
    std::wstring nm = sectionNameById(!subId.empty()?subId:deptId);
    if(!nm.empty()) prefix=deptCodePrefix(nm);
    wchar_t b[40]; swprintf(b,40,L"%s_%04d",prefix.c_str(),nextPersonSeq(prefix));
    return b;
}

std::wstring personRoleLabel(const PersonDef& p){
    switch(p.roleKind){
        case 1: return L"پزشک";
        case 2: return L"پرستار";
        case 3: return L"کارآموز";
        case 4: return p.roleCustom.empty()?L"سایر":p.roleCustom;
        default: return L"پرسنل";
    }
}

bool addPerson(PersonDef& p, std::wstring& err){
    p.firstName=trim(p.firstName); p.lastName=trim(p.lastName);
    if(p.firstName.empty() && p.lastName.empty()){
        err=L"نام و نام خانوادگی الزامی است."; return false;
    }
    p.code=trim(p.code);
    // v1.80.0: empty code → auto-generate from the زیربخش/بخش (matches the
    // form's live preview — the same two ids drive both).
    if(p.code.empty()) p.code=nextPersonCode(p.deptId,p.subId);
    auto v=loadPersons();
    for(auto& e:v)
        if(e.code==p.code){ err=L"این کد پرسنلی قبلاً ثبت شده است."; return false; }
    if(!p.nationalId.empty())
        for(auto& e:v)
            if(!e.nationalId.empty() && e.nationalId==p.nationalId){
                err=L"این کد ملی قبلاً برای پرسنل دیگری ثبت شده است."; return false;
            }
    if(p.created.empty()) p.created=jalaliDateStr(iranNow());
    v.push_back(p);
    savePersons(v);
    logLine(L"person added: "+p.code+L" "+p.firstName+L" "+p.lastName);
    return true;
}
bool updatePerson(const PersonDef& p, std::wstring& err){
    if(trim(p.code).empty()){ err=L"کد پرسنلی خالی است."; return false; }
    auto v=loadPersons();
    for(auto& e:v){
        if(e.code==p.code){
            if(!p.nationalId.empty())
                for(auto& o:v)
                    if(o.code!=p.code && !o.nationalId.empty() && o.nationalId==p.nationalId){
                        err=L"این کد ملی قبلاً برای پرسنل دیگری ثبت شده است."; return false;
                    }
            PersonDef np=p; np.extraKv=e.extraKv;   // keep §H unknown keys
            e=np;
            savePersons(v);
            logLine(L"person updated: "+p.code);
            return true;
        }
    }
    err=L"پرسنل پیدا نشد."; return false;
}
bool removePerson(const std::wstring& code){
    auto v=loadPersons();
    for(size_t i=0;i<v.size();i++)
        if(v[i].code==code){
            v.erase(v.begin()+i); savePersons(v);
            logLine(L"person removed: "+code);
            return true;
        }
    return false;
}

// ============================================================ photos ========
//  Personnel photos live under data\persons\photos\<code>.<ext>. The CRM page
//  sends them base64 over the bridge; we decode + store the raw bytes, and the
//  relative path goes into the person's `photo` key. Reading back re-encodes
//  for a data: URL (the page never touches the filesystem itself).
std::wstring personPhotoDir(){
    std::wstring d=dataDir()+L"\\persons";
    CreateDirectoryW(d.c_str(),NULL);
    d+=L"\\photos";
    CreateDirectoryW(d.c_str(),NULL);
    return d;
}
bool savePersonPhoto(const std::wstring& code, const std::string& bytes,
                     const std::wstring& ext, std::wstring& relOut){
    if(code.empty()||bytes.empty()) return false;
    std::wstring e=ext.empty()?L".png":ext;
    std::wstring fn=code+e;
    for(auto& c:fn) if(c==L'\\'||c==L'/'||c==L':'||c==L' ') c=L'_';
    std::wstring full=personPhotoDir()+L"\\"+fn;
    HANDLE hf=CreateFileW(full.c_str(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL,NULL);
    if(hf==INVALID_HANDLE_VALUE) return false;
    DWORD wr=0;
    WriteFile(hf,bytes.data(),(DWORD)bytes.size(),&wr,NULL);
    CloseHandle(hf);
    relOut=L"persons\\photos\\"+fn;
    return wr==bytes.size();
}
bool loadPersonPhoto(const std::wstring& relPath, std::string& bytesOut,
                     std::wstring& mimeOut){
    if(relPath.empty()) return false;
    // never let a hand-edited persons.dat escape the data dir
    if(relPath.find(L"..")!=std::wstring::npos) return false;
    std::wstring full=dataDir()+L"\\"+relPath;
    HANDLE hf=CreateFileW(full.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,
                          OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(hf==INVALID_HANDLE_VALUE) return false;
    DWORD sz=GetFileSize(hf,NULL);
    if(sz==0||sz>8*1024*1024){ CloseHandle(hf); return false; }
    bytesOut.resize(sz); DWORD rd=0;
    ReadFile(hf,&bytesOut[0],sz,&rd,NULL);
    CloseHandle(hf);
    if(rd!=sz) return false;
    std::wstring low=relPath;
    for(auto& c:low) c=towlower(c);
    mimeOut = (low.find(L".jpg")!=std::wstring::npos||low.find(L".jpeg")!=std::wstring::npos)
              ? L"image/jpeg" : L"image/png";
    return true;
}
