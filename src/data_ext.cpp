// ============================================================================
//  data_ext.cpp  (v1.7.0)
//  Extended data layer for the upgraded reception and management screens:
//    • Iranian Civil-Registry (ثبت احوال) + insurance lookup with NO fabrication.
//      We validate the 10-digit national-code checksum, then resolve the
//      identity ONLY from a trusted source: (1) a configured online registry
//      web-service, or (2) the local store of patients this clinic already
//      verified by hand. If neither verifies the code we return "not found" and
//      the UI shows an unverified state + manual entry — we never invent a name,
//      birth date, address or insurance.
//    • Doctors + their services (file-backed, seeded with realistic specialties)
//    • Profile-change request workflow (reception → management approval)
//    • Cartable v2 actions: pin / seen-one / delete-one (each reported back)
//
//  Everything is file-backed under data\ so the program stays a single static
//  EXE and is ready to migrate to a DB/REST backend without touching the UI.
// ============================================================================
#include "app.h"
#include <stdio.h>
#include <algorithm>
#include <wininet.h>

// ---- shared little helpers (kept local to avoid clashing with employees.cpp)
static std::vector<std::wstring> dx_split(const std::wstring& s, wchar_t sep){
    std::vector<std::wstring> out; size_t pos=0;
    while(true){ size_t e=s.find(sep,pos);
        if(e==std::wstring::npos){ out.push_back(s.substr(pos)); break; }
        out.push_back(s.substr(pos,e-pos)); pos=e+1; }
    return out;
}
static std::wstring dx_esc(const std::wstring& s){
    std::wstring o=s; for(auto&c:o) if(c==L'|'||c==L'\n'||c==L'\r') c=L' '; return o;
}

// ============================================================================
//  NATIONAL REGISTRY (ثبت احوال) — checksum validation + trusted lookup only
//  (no fabrication; see lookupCitizen() below)
// ============================================================================
bool validNationalId(const std::wstring& id){
    if(id.size()!=10) return false;
    for(wchar_t c:id) if(c<L'0'||c>L'9') return false;
    bool allSame=true; for(wchar_t c:id) if(c!=id[0]){ allSame=false; break; }
    if(allSame) return false;
    int sum=0; for(int i=0;i<9;i++) sum+=(id[i]-L'0')*(10-i);
    int rem=sum%11, chk=id[9]-L'0';
    return (rem<2)? (chk==rem) : (chk==11-rem);
}

// ----------------------------------------------------------------------------
//  LOCAL PATIENT STORE  (data\patients.dat) — REAL identities the clinic has
//  already verified / entered by hand. This is NOT fabricated data: it only
//  ever contains records an operator confirmed and saved. Recalling the same
//  national code returns exactly what was stored before.
//    nid|first|last|father|gender|birth|mobile|insCsv
// ----------------------------------------------------------------------------
static std::wstring patientsPath(){ return dataDir()+L"\\patients.dat"; }

static bool parseInsCsv(const std::wstring& csv, std::vector<int>& out){
    out.clear();
    for(auto& s : dx_split(csv,L',')){
        std::wstring t=trim(s);
        if(t.empty()) continue;
        int v=_wtoi(t.c_str());
        if(v>=0 && v<N_INSURANCES) out.push_back(v);
    }
    return !out.empty();
}
static std::wstring insToCsv(const std::vector<int>& v){
    std::wstring s;
    for(size_t i=0;i<v.size();i++){ if(i) s+=L","; wchar_t b[8]; swprintf(b,8,L"%d",v[i]); s+=b; }
    return s;
}

//  v1.40.0: format one store row using the NEW 11-column schema. Empty trailing
//  columns are still written (so the field count is stable) — the reader below
//  tolerates any legacy count (7/8/9/10/11) so nothing is ever lost.
static std::wstring patientRowLine(const std::wstring& nationalId,
        const std::wstring& firstName, const std::wstring& lastName,
        const std::wstring& fatherName, const std::wstring& gender,
        const std::wstring& birthDate, const std::wstring& mobile,
        const std::wstring& landline, const std::wstring& address,
        const std::vector<int>& insurances, int suppIdx){
    wchar_t sb[16]; swprintf(sb,16,L"%d",suppIdx);
    return dx_esc(nationalId)+L"|"+dx_esc(firstName)+L"|"+dx_esc(lastName)
        + L"|"+dx_esc(fatherName)+L"|"+dx_esc(gender)+L"|"+dx_esc(birthDate)
        + L"|"+dx_esc(mobile)+L"|"+insToCsv(insurances)
        + L"|"+dx_esc(landline)+L"|"+dx_esc(address)+L"|"+sb;
}

//  Look the national code up in the local patient store. Returns CS_LOCAL on
//  hit; CS_NONE otherwise. Never invents anything.
//  v1.40.0: also recalls landline «تلفن ثابت», address «آدرس» and the
//  supplementary-insurance index, tolerating 7/8/9/10/11-column legacy lines.
static bool lookupLocalPatient(const std::wstring& nationalId, CitizenInfo& c){
    std::wstring all=readFileUtf8(patientsPath());
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=trim(all.substr(pos,e-pos)); pos=e+1;
        if(line.empty()) continue;
        auto f=dx_split(line,L'|');
        if(f.size()<7) continue;
        if(trim(f[0])!=nationalId) continue;
        c.firstName=f[1]; c.lastName=f[2]; c.fatherName=f[3];
        c.gender=f[4]; c.birthDate=f[5]; c.mobile=f[6];
        if(f.size()>=8) parseInsCsv(f[7],c.insurances);
        if(f.size()>=9)  c.landline=f[8];             // v1.40.0
        if(f.size()>=10) c.address=f[9];              // v1.40.0
        if(f.size()>=11){ std::wstring si=trim(f[10]); c.suppIdx = si.empty()? -1 : _wtoi(si.c_str()); }
        c.found=true; c.source=CS_LOCAL;
        return true;
    }
    return false;
}

void rememberPatient(const std::wstring& nationalId,
        const std::wstring& firstName, const std::wstring& lastName,
        const std::wstring& fatherName, const std::wstring& gender,
        const std::wstring& birthDate, const std::wstring& mobile,
        const std::wstring& landline, const std::wstring& address,
        const std::vector<int>& insurances, int suppIdx){
    if(nationalId.empty()) return;
    if(trim(firstName).empty() && trim(lastName).empty()) return;  // nothing real
    // load all, MIGRATE every kept legacy line to the NEW schema on the fly,
    // replace the existing record for this nid, otherwise append.
    std::wstring all=readFileUtf8(patientsPath());
    std::vector<std::wstring> kept;
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=trim(all.substr(pos,e-pos)); pos=e+1;
        if(line.empty()) continue;
        auto f=dx_split(line,L'|');
        if(f.size()>=1 && trim(f[0])==nationalId) continue;   // drop old copy of THIS nid
        if(f.size()<7){ kept.push_back(line); continue; }     // too short to migrate — keep verbatim
        // rewrite the kept row in the NEW 11-column schema (migrate-on-read/write)
        std::vector<int> ins; if(f.size()>=8) parseInsCsv(f[7],ins);
        std::wstring land = (f.size()>=9)? f[8] : L"";
        std::wstring addr = (f.size()>=10)? f[9] : L"";
        int si = -1; if(f.size()>=11){ std::wstring s=trim(f[10]); if(!s.empty()) si=_wtoi(s.c_str()); }
        kept.push_back(patientRowLine(trim(f[0]),f[1],f[2],f[3],f[4],f[5],f[6],land,addr,ins,si));
    }
    kept.push_back(patientRowLine(nationalId,firstName,lastName,fatherName,gender,
        birthDate,mobile,landline,address,insurances,suppIdx));
    std::wstring out; for(auto& l:kept) out+=l+L"\r\n";
    writeFileUtf8(patientsPath(),out,false);
}

//  v1.10.0: enumerate every locally-stored patient. Newest record is written
//  last by rememberPatient(), so we reverse to present newest first.
//  v1.40.0: parse the new landline/address/suppIdx columns, tolerating any
//  legacy field count (7..11) so old stores load without dropping any row.
std::vector<PatientRow> loadAllPatients(){
    std::vector<PatientRow> out;
    std::wstring all=readFileUtf8(patientsPath());
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=trim(all.substr(pos,e-pos)); pos=e+1;
        if(line.empty()) continue;
        auto f=dx_split(line,L'|');
        if(f.size()<7) continue;
        PatientRow r;
        r.nid=trim(f[0]); r.first=f[1]; r.last=f[2]; r.father=f[3];
        r.gender=f[4]; r.birth=f[5]; r.mobile=f[6];
        if(f.size()>=8) parseInsCsv(f[7],r.insurances);
        if(f.size()>=9)  r.landline=f[8];             // v1.40.0
        if(f.size()>=10) r.address=f[9];              // v1.40.0
        if(f.size()>=11){ std::wstring si=trim(f[10]); r.suppIdx = si.empty()? -1 : _wtoi(si.c_str()); }
        out.push_back(std::move(r));
    }
    std::reverse(out.begin(),out.end());   // newest first
    return out;
}

//  v1.10.0: remove one patient record by national code.
bool deletePatient(const std::wstring& nationalId){
    if(nationalId.empty()) return false;
    std::wstring all=readFileUtf8(patientsPath());
    std::vector<std::wstring> kept; bool removed=false;
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=trim(all.substr(pos,e-pos)); pos=e+1;
        if(line.empty()) continue;
        auto f=dx_split(line,L'|');
        if(f.size()>=1 && trim(f[0])==nationalId){ removed=true; continue; }
        kept.push_back(line);
    }
    if(removed){
        std::wstring out; for(auto& l:kept) out+=l+L"\r\n";
        writeFileUtf8(patientsPath(),out,false);
    }
    return removed;
}

// ----------------------------------------------------------------------------
//  v1.12.0 (§11-13): PATIENT IMPORT PIPELINE — dedup-aware bulk import into the
//  same data\patients.dat store the reception auto-fill (lookupLocalPatient)
//  reads. The national code is the clinical primary key; an incoming row whose
//  code is already on file UPDATES it (newer wins), so re-importing a refreshed
//  export never produces duplicates. Invalid/empty codes and nameless rows are
//  skipped and counted so the operator gets an honest reconciliation summary.
// ----------------------------------------------------------------------------
ImportResult importPatients(const std::vector<ImportPatientRow>& rows){
    ImportResult res; res.total=(int)rows.size();
    // Load the current store ONCE into an index so a large import is O(n) over
    // the file rather than re-reading patients.dat per row.
    std::wstring all=readFileUtf8(patientsPath());
    std::vector<std::wstring> nids;          // existing national codes (order)
    std::vector<std::wstring> lines;         // existing serialized rows
    {
        size_t pos=0;
        while(pos<all.size()){
            size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
            std::wstring line=trim(all.substr(pos,e-pos)); pos=e+1;
            if(line.empty()) continue;
            auto f=dx_split(line,L'|');
            if(f.empty()) continue;
            nids.push_back(trim(f[0]));
            lines.push_back(line);
        }
    }
    auto findIdx=[&](const std::wstring& nid)->int{
        for(size_t i=0;i<nids.size();i++) if(nids[i]==nid) return (int)i;
        return -1;
    };
    for(const auto& r : rows){
        std::wstring nid=trim(r.nid);
        if(!validNationalId(nid)){ res.skippedInvalid++; continue; }
        if(trim(r.first).empty() && trim(r.last).empty()){ res.skippedEmpty++; continue; }
        std::wstring row = dx_esc(nid)+L"|"+dx_esc(r.first)+L"|"+dx_esc(r.last)
            + L"|"+dx_esc(r.father)+L"|"+dx_esc(r.gender)+L"|"+dx_esc(r.birth)
            + L"|"+dx_esc(r.mobile)+L"|"+insToCsv(r.insurances);
        int idx=findIdx(nid);
        if(idx>=0){ lines[idx]=row; res.updated++; }      // dedup: refresh in place
        else { nids.push_back(nid); lines.push_back(row); res.inserted++; }
    }
    // persist once
    std::wstring out; for(auto& l:lines) out+=l+L"\r\n";
    writeFileUtf8(patientsPath(),out,false);
    res.ok=true;
    return res;
}

//  Detect the most likely field delimiter on a sample line.
static wchar_t dx_detectDelim(const std::wstring& line){
    int pipe=0,comma=0,semi=0,tab=0;
    for(wchar_t ch:line){
        if(ch==L'|')pipe++; else if(ch==L',')comma++;
        else if(ch==L';')semi++; else if(ch==L'\t')tab++;
    }
    if(pipe>=comma && pipe>=semi && pipe>=tab && pipe>0) return L'|';
    if(tab>=comma && tab>=semi && tab>0) return L'\t';
    if(semi>=comma && semi>0) return L';';
    if(comma>0) return L',';
    return L'|';
}
//  Map a header cell (English or Persian) to a canonical column index, or -1.
//   0=nid 1=first 2=last 3=father 4=gender 5=birth 6=mobile 7=insurance
static int dx_headerCol(const std::wstring& raw){
    std::wstring h=trim(raw);
    // lower-case the ASCII part for English matching
    std::wstring l=h; for(auto& c:l) if(c>=L'A'&&c<=L'Z') c=(wchar_t)(c-L'A'+L'a');
    auto has=[&](const wchar_t* s){ return l.find(s)!=std::wstring::npos; };
    auto fa =[&](const wchar_t* s){ return h.find(s)!=std::wstring::npos; };
    if(has(L"national")||has(L"nid")||has(L"melli")||has(L"codemeli")||fa(L"کد ملی")||fa(L"کدملی")||fa(L"ملی")) return 0;
    if(has(L"father")||fa(L"پدر")) return 3;   // check father BEFORE first/last (both contain "نام")
    // last/family name (check before generic "first/name" so خانوادگی wins)
    if(has(L"last")||has(L"family")||has(L"surname")||fa(L"خانوادگی")) return 2;
    if(has(L"first")||has(L"fname")||(has(L"name")&&!has(L"last"))||fa(L"نام")) return 1;
    if(has(L"gender")||has(L"sex")||fa(L"جنس")) return 4;
    if(has(L"birth")||has(L"dob")||has(L"tavalod")||fa(L"تولد")||fa(L"تاریخ تولد")) return 5;
    if(has(L"mobile")||has(L"phone")||has(L"cell")||fa(L"موبایل")||fa(L"همراه")||fa(L"تلفن")) return 6;
    if(has(L"insur")||has(L"bime")||fa(L"بیمه")) return 7;
    return -1;
}
//  Normalise a free-text gender value to the store's canonical مرد/زن.
static std::wstring dx_normGender(const std::wstring& g){
    std::wstring t=trim(g);
    if(t.empty()) return t;
    std::wstring l=t; for(auto&c:l) if(c>=L'A'&&c<=L'Z') c=(wchar_t)(c-L'A'+L'a');
    if(t.find(L"زن")!=std::wstring::npos||l==L"f"||l.find(L"female")!=std::wstring::npos||l==L"0") return L"زن";
    if(t.find(L"مرد")!=std::wstring::npos||l==L"m"||l.find(L"male")!=std::wstring::npos||l==L"1") return L"مرد";
    return t;   // leave unknown values untouched (no fabrication)
}
std::vector<ImportPatientRow> parsePatientImportFile(const std::wstring& path,
                                                     std::wstring& parseError){
    std::vector<ImportPatientRow> out;
    parseError.clear();
    std::wstring all=readFileUtf8(path);   // handles UTF-8/UTF-16 BOM
    if(trim(all).empty()){ parseError=L"فایل ورودی خالی یا غیرقابل‌خواندن است."; return out; }
    // split into non-empty trimmed lines
    std::vector<std::wstring> rawLines;
    { size_t pos=0;
      while(pos<all.size()){
          size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
          std::wstring line=all.substr(pos,e-pos); pos=e+1;
          // strip trailing \r
          while(!line.empty() && (line.back()==L'\r')) line.pop_back();
          if(!trim(line).empty()) rawLines.push_back(line);
      } }
    if(rawLines.empty()){ parseError=L"هیچ ردیف داده‌ای یافت نشد."; return out; }
    wchar_t delim=dx_detectDelim(rawLines[0]);
    // header detection: if the first line has no digits in a national-id-shaped
    // cell, treat it as a header and build a column map; else assume positional.
    int colMap[8]; for(int i=0;i<8;i++) colMap[i]= (i<8?i:-1);   // default positional
    bool positional=true;
    {
        auto f0=dx_split(rawLines[0],delim);
        int mapped=0, mapTmp[16]; for(int i=0;i<16;i++) mapTmp[i]=-1;
        for(size_t i=0;i<f0.size() && i<16;i++){
            int c=dx_headerCol(f0[i]);
            if(c>=0){ mapTmp[i]=c; mapped++; }
        }
        // require at least the national-id column to trust a header row
        bool hasNid=false; for(size_t i=0;i<f0.size()&&i<16;i++) if(mapTmp[i]==0) hasNid=true;
        if(mapped>=2 && hasNid){
            positional=false;
            // invert: colMap[canonical]=sourceIndex
            for(int c=0;c<8;c++) colMap[c]=-1;
            for(size_t i=0;i<f0.size()&&i<16;i++) if(mapTmp[i]>=0) colMap[mapTmp[i]]=(int)i;
        }
    }
    size_t startRow = positional?0:1;
    for(size_t li=startRow; li<rawLines.size(); li++){
        auto f=dx_split(rawLines[li],delim);
        auto get=[&](int canonical)->std::wstring{
            int src = positional?canonical:colMap[canonical];
            if(src<0 || src>=(int)f.size()) return std::wstring();
            return trim(f[src]);
        };
        ImportPatientRow r;
        r.nid    = get(0);
        r.first  = get(1);
        r.last   = get(2);
        r.father = get(3);
        r.gender = dx_normGender(get(4));
        r.birth  = get(5);
        r.mobile = get(6);
        parseInsCsv(get(7),r.insurances);
        // skip an obvious header that slipped through positional mode
        if(positional && li==0 && !validNationalId(r.nid) && r.nid.find_first_of(L"0123456789۰۱۲۳۴۵۶۷۸۹")==std::wstring::npos)
            continue;
        out.push_back(std::move(r));
    }
    if(out.empty()) parseError=L"هیچ ردیف قابل‌واردکردنی استخراج نشد.";
    return out;
}
ImportResult importPatientsFromFile(const std::wstring& path){
    ImportResult res;
    std::wstring err;
    auto rows=parsePatientImportFile(path,err);
    if(!err.empty() && rows.empty()){ res.error=err; res.ok=false; return res; }
    res=importPatients(rows);
    if(!err.empty() && res.error.empty()) res.error=err;
    return res;
}

// ----------------------------------------------------------------------------
//  ONLINE REGISTRY WEB-SERVICE (optional). Configure `registry_url` in
//  data\settings.ini, e.g.  https://host/api/citizen?nid={NID}
//  The endpoint must return a small UTF-8 key=value body. Recognised keys:
//    found=1|0  first=  last=  father=  gender=مرد|زن  birth=YYYY/MM/DD
//    mobile=  insurances=1,2  (INSURANCES[] indices)
//  If no URL is configured, or the request fails / times out, we DO NOT guess —
//  the caller falls back to manual entry and the UI shows the failure.
// ----------------------------------------------------------------------------
static std::string regHttpGet(const std::wstring& url, bool* okOut){
    *okOut=false; std::string body;
    HINTERNET net=InternetOpenW(L"DarmanPlus/1.7", INTERNET_OPEN_TYPE_PRECONFIG,NULL,NULL,0);
    if(!net) return body;
    // short timeouts so a missing/slow server never freezes the UI
    DWORD to=4000;
    InternetSetOptionW(net,INTERNET_OPTION_CONNECT_TIMEOUT,&to,sizeof(to));
    InternetSetOptionW(net,INTERNET_OPTION_RECEIVE_TIMEOUT,&to,sizeof(to));
    InternetSetOptionW(net,INTERNET_OPTION_SEND_TIMEOUT,&to,sizeof(to));
    HINTERNET f=InternetOpenUrlW(net,url.c_str(),NULL,0,
        INTERNET_FLAG_RELOAD|INTERNET_FLAG_NO_CACHE_WRITE,0);
    if(f){
        char buf[2048]; DWORD rd;
        while(InternetReadFile(f,buf,sizeof(buf),&rd)&&rd) body.append(buf,rd);
        InternetCloseHandle(f); *okOut=true;
    }
    InternetCloseHandle(net);
    return body;
}
static std::wstring regKV(const std::wstring& body, const std::wstring& key){
    // find "key=" at a line start, return trimmed value to end of line
    std::wstring k=key+L"=";
    size_t pos=0;
    while(pos<body.size()){
        size_t e=body.find(L'\n',pos); if(e==std::wstring::npos) e=body.size();
        std::wstring line=trim(body.substr(pos,e-pos)); pos=e+1;
        if(line.size()>=k.size() && line.compare(0,k.size(),k)==0)
            return trim(line.substr(k.size()));
    }
    return L"";
}
static bool lookupRegistry(const std::wstring& nationalId, CitizenInfo& c){
    std::wstring tmpl=getSetting(L"registry_url",L"");
    if(trim(tmpl).empty()) return false;     // no online source configured
    c.lookupTried=true;
    // substitute {NID}
    std::wstring url=tmpl; size_t p=url.find(L"{NID}");
    if(p!=std::wstring::npos) url.replace(p,5,nationalId);
    else { url += (url.find(L'?')==std::wstring::npos?L"?nid=":L"&nid=")+nationalId; }
    bool ok=false; std::string raw=regHttpGet(url,&ok);
    if(!ok || raw.empty()){ c.lookupFailed=true; return false; }
    int n=MultiByteToWideChar(CP_UTF8,0,raw.c_str(),(int)raw.size(),NULL,0);
    std::wstring body(n,0);
    MultiByteToWideChar(CP_UTF8,0,raw.c_str(),(int)raw.size(),&body[0],n);
    if(regKV(body,L"found")==L"0"){ return false; }  // server says not found (not a failure)
    std::wstring first=regKV(body,L"first"), last=regKV(body,L"last");
    if(first.empty() && last.empty()){ c.lookupFailed=true; return false; }
    c.firstName=first; c.lastName=last;
    c.fatherName=regKV(body,L"father");
    c.gender=regKV(body,L"gender");
    c.birthDate=regKV(body,L"birth");
    c.mobile=regKV(body,L"mobile");
    parseInsCsv(regKV(body,L"insurances"),c.insurances);
    c.found=true; c.source=CS_REGISTRY;
    return true;
}

//  No fabrication. Order of trust:
//   1) configured online registry web-service (if any),
//   2) the local store of patients this clinic already verified,
//   3) nothing — caller enables manual entry and shows the unverified state.
CitizenInfo lookupCitizen(const std::wstring& nationalId){
    CitizenInfo c;
    c.idValid = validNationalId(nationalId);
    if(!c.idValid) return c;                 // invalid code → not found, no guess
    if(lookupRegistry(nationalId,c)) return c;
    if(lookupLocalPatient(nationalId,c)) return c;
    return c;                                // c.found stays false
}

// ============================================================================
//  DOCTORS
// ============================================================================
static std::wstring docsPath(){ return dataDir()+L"\\doctors.dat"; }
//  v1.69.0: expanded doctor record. New format is multi-line key=value per
//  doctor (like EmpProfile), separated by a blank line. Old 3-field pipe
//  format (name|specialty|svc1;svc2) is still parsed for backward compat.
static void seedDoctors(){
    struct D{ const wchar_t* n; const wchar_t* sp; const wchar_t* sv; };
    static const D defs[] = {
        { L"دکتر علی رضایی",      L"رادیولوژی",   L"رادیوگرافی ساده;سونوگرافی;سی‌تی‌اسکن" },
        { L"دکتر مریم حسینی",     L"داخلی",       L"ویزیت داخلی;نوار قلب;مشاوره" },
        { L"دکتر رضا محمدی",      L"دندانپزشکی",  L"معاینه;جرم‌گیری;ترمیم;عصب‌کشی" },
        { L"دکتر زهرا کریمی",     L"زنان و زایمان",L"ویزیت;سونوگرافی;پاپ‌اسمیر" },
        { L"دکتر حسین اکبری",     L"اطفال",        L"ویزیت کودک;واکسیناسیون;مشاوره تغذیه" },
        { L"دکتر سارا موسوی",     L"پوست و مو",    L"ویزیت پوست;لیزر;مزوتراپی" },
        { L"دکتر مهدی صادقی",     L"ارتوپدی",      L"ویزیت;گچ‌گیری;تزریق مفصل" },
        { L"دکتر لیلا نوری",      L"چشم‌پزشکی",    L"معاینه بینایی;اپتومتری;لیزیک" },
    };
    std::wstring out;
    for(auto&d:defs){
        out += L"name="+std::wstring(d.n)+L"\r\n";
        out += L"specialty="+std::wstring(d.sp)+L"\r\n";
        out += L"services="+std::wstring(d.sv)+L"\r\n";
        out += L"active=1\r\n";
        out += L"printOnReceipt=1\r\n";
        out += L"\r\n";
    }
    writeFileUtf8(docsPath(),out,false);
}
static DoctorDef parseOldDoctor(const std::wstring& line){
    DoctorDef d;
    auto f=dx_split(line,L'|');
    if(f.size()>=1) d.name=f[0];
    if(f.size()>=2) d.specialty=f[1];
    if(f.size()>=3) for(auto&s:dx_split(f[2],L';')) if(!trim(s).empty()) d.services.push_back(trim(s));
    d.active=true; d.printOnReceipt=true;
    return d;
}
static DoctorDef parseKvDoctor(const std::wstring& block){
    DoctorDef d; d.active=true; d.printOnReceipt=true;
    std::wstring name,sp,sv;
    size_t pos=0;
    while(pos<block.size()){
        size_t e=block.find(L'\n',pos); if(e==std::wstring::npos) e=block.size();
        std::wstring line=trim(block.substr(pos,e-pos)); pos=e+1;
        if(line.empty()) continue;
        size_t eq=line.find(L'=');
        if(eq==std::wstring::npos) continue;
        std::wstring k=trim(line.substr(0,eq));
        std::wstring v=trim(line.substr(eq+1));
        if(k==L"name")          name=v;
        else if(k==L"specialty") sp=v;
        else if(k==L"services")  sv=v;
        else if(k==L"deptId")    d.deptId=v;
        else if(k==L"docType")   d.docType=_wtoi(v.c_str());
        else if(k==L"docCode")   d.docCode=v;
        else if(k==L"active")    d.active=(v!=L"0");
        else if(k==L"medicalId") d.medicalId=v;
        else if(k==L"namePrefix")d.namePrefix=v;
        else if(k==L"firstName") d.firstName=v;
        else if(k==L"lastName")  d.lastName=v;
        else if(k==L"nationalId")d.nationalId=v;
        else if(k==L"mobile")    d.mobile=v;
        else if(k==L"email")     d.email=v;
        else if(k==L"address")   d.address=v;
        else if(k==L"franchise") d.franchise=v;
        else if(k==L"printOnReceipt") d.printOnReceipt=(v!=L"0");
        else if(k==L"insSpecialty") d.insSpecialty=v;
        else if(k==L"degree")    d.degree=v;
        else if(k==L"contractType") d.contractType=_wtoi(v.c_str());
        else if(k==L"emergencyContract") d.emergencyContract=v;
        else if(k==L"accounting") d.accounting=v;
        // v1.78.0: missing key (old files) keeps the ctor default → performer.
        else if(k==L"isPerformer") d.isPerformer=(v!=L"0");
        // v2.01 (Part B): complementary checkboxes.
        else if(k==L"showOnSiteList") d.showOnSiteList=(v!=L"0");
        else if(k==L"onlineAppointment") d.onlineAppointment=(v!=L"0");
        else if(k==L"isAnesthesiologist") d.isAnesthesiologist=(v!=L"0");
    }
    d.name=name; d.specialty=sp;
    if(!sv.empty()) for(auto&s:dx_split(sv,L';')) if(!trim(s).empty()) d.services.push_back(trim(s));
    if(d.name.empty() && (!d.firstName.empty()||!d.lastName.empty()))
        d.name=(d.namePrefix.empty()?L"":d.namePrefix+L" ")+d.firstName+L" "+d.lastName;
    return d;
}
std::vector<DoctorDef> loadDoctors(){
    std::wstring all=readFileUtf8(docsPath());
    if(trim(all).empty()){ seedDoctors(); all=readFileUtf8(docsPath()); }
    std::vector<DoctorDef> out;
    bool isNewFormat = all.find(L"name=")!=std::wstring::npos || all.find(L"specialty=")!=std::wstring::npos;
    if(isNewFormat){
        size_t pos=0;
        while(pos<all.size()){
            size_t next=all.find(L"\r\n\r\n",pos);
            if(next==std::wstring::npos) next=all.find(L"\n\n",pos);
            if(next==std::wstring::npos) next=all.size();
            std::wstring block=all.substr(pos,next-pos);
            pos=next+2; if(pos<all.size() && (all[pos]==L'\n'||all[pos]==L'\r')) pos++;
            if(trim(block).empty()) continue;
            DoctorDef d=parseKvDoctor(block);
            if(!d.name.empty()) out.push_back(d);
        }
    } else {
        size_t pos=0;
        while(pos<all.size()){
            size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
            std::wstring line=trim(all.substr(pos,e-pos)); pos=e+1;
            if(line.empty()) continue;
            if(line.find(L'|')!=std::wstring::npos) out.push_back(parseOldDoctor(line));
        }
    }
    return out;
}
bool saveDoctors(const std::vector<DoctorDef>& doctors){
    std::wstring out;
    for(const auto& d:doctors){
        out += L"name="+d.name+L"\r\n";
        out += L"specialty="+d.specialty+L"\r\n";
        std::wstring sv;
        for(size_t i=0;i<d.services.size();++i){ if(i) sv+=L";"; sv+=d.services[i]; }
        out += L"services="+sv+L"\r\n";
        out += L"deptId="+d.deptId+L"\r\n";
        out += L"docType="+std::to_wstring(d.docType)+L"\r\n";
        out += L"docCode="+d.docCode+L"\r\n";
        out += L"active="+std::wstring(d.active?L"1":L"0")+L"\r\n";
        out += L"medicalId="+d.medicalId+L"\r\n";
        out += L"namePrefix="+d.namePrefix+L"\r\n";
        out += L"firstName="+d.firstName+L"\r\n";
        out += L"lastName="+d.lastName+L"\r\n";
        out += L"nationalId="+d.nationalId+L"\r\n";
        out += L"mobile="+d.mobile+L"\r\n";
        out += L"email="+d.email+L"\r\n";
        out += L"address="+d.address+L"\r\n";
        out += L"franchise="+d.franchise+L"\r\n";
        out += L"printOnReceipt="+std::wstring(d.printOnReceipt?L"1":L"0")+L"\r\n";
        out += L"insSpecialty="+d.insSpecialty+L"\r\n";
        out += L"degree="+d.degree+L"\r\n";
        out += L"contractType="+std::to_wstring(d.contractType)+L"\r\n";
        out += L"emergencyContract="+d.emergencyContract+L"\r\n";
        out += L"accounting="+d.accounting+L"\r\n";
        out += L"isPerformer="+std::wstring(d.isPerformer?L"1":L"0")+L"\r\n";
        out += L"showOnSiteList="+std::wstring(d.showOnSiteList?L"1":L"0")+L"\r\n";
        out += L"onlineAppointment="+std::wstring(d.onlineAppointment?L"1":L"0")+L"\r\n";
        out += L"isAnesthesiologist="+std::wstring(d.isAnesthesiologist?L"1":L"0")+L"\r\n";
        out += L"\r\n";
    }
    return writeFileUtf8(docsPath(),out,false);
}
std::vector<DoctorDef> todaysDoctors(){
    // deterministic subset "on shift today" based on the Jalali day number so
    // the «پزشکان امروز» button shows a stable, realistic roster each day.
    auto all=loadDoctors();
    SYSTEMTIME st=iranNow();
    int jy,jm,jd; gregToJalali(st.wYear,st.wMonth,st.wDay,jy,jm,jd);
    std::vector<DoctorDef> out;
    for(size_t i=0;i<all.size();i++)
        if(((int)i + jd) % 2 == 0) out.push_back(all[i]);
    if(out.empty() && !all.empty()) out.push_back(all[0]);
    return out;
}

// ============================================================================
//  PROFILE-CHANGE REQUESTS  (reception → management)
// ============================================================================
static std::wstring profReqPath(){ return dataDir()+L"\\profreq.dat"; }
//  user|oldName|newName|oldPhoto|newPhoto|time|status|reason
std::vector<ProfReq> loadProfReqs(){
    std::vector<ProfReq> out;
    std::wstring all=readFileUtf8(profReqPath());
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=trim(all.substr(pos,e-pos)); pos=e+1;
        if(line.empty()) continue;
        auto f=dx_split(line,L'|');
        if(f.size()<7) continue;
        ProfReq r; r.user=f[0]; r.oldName=f[1]; r.newName=f[2];
        r.oldPhoto=f[3]; r.newPhoto=f[4]; r.time=f[5]; r.status=_wtoi(f[6].c_str());
        if(f.size()>=8) r.reason=f[7];
        out.push_back(r);
    }
    std::reverse(out.begin(),out.end());     // newest first
    return out;
}
static void saveAllProfReqs_newestFirst(const std::vector<ProfReq>& v){
    // stored oldest-first on disk
    std::wstring out;
    for(int i=(int)v.size()-1;i>=0;i--){
        const ProfReq& r=v[i];
        wchar_t sb[8]; swprintf(sb,8,L"%d",r.status);
        out += dx_esc(r.user)+L"|"+dx_esc(r.oldName)+L"|"+dx_esc(r.newName)+L"|"
            + dx_esc(r.oldPhoto)+L"|"+dx_esc(r.newPhoto)+L"|"+dx_esc(r.time)+L"|"
            + std::wstring(sb)+L"|"+dx_esc(r.reason)+L"\r\n";
    }
    writeFileUtf8(profReqPath(),out,false);
}
void pushProfReq(const ProfReq& r){
    SYSTEMTIME st=iranNow();
    wchar_t tb[32]; swprintf(tb,32,L"%s %02d:%02d",jalaliDateShort(st).c_str(),st.wHour,st.wMinute);
    ProfReq x=r; x.time=tb; x.status=0;
    std::wstring row=dx_esc(x.user)+L"|"+dx_esc(x.oldName)+L"|"+dx_esc(x.newName)+L"|"
        + dx_esc(x.oldPhoto)+L"|"+dx_esc(x.newPhoto)+L"|"+dx_esc(x.time)+L"|0|"+L"\r\n";
    writeFileUtf8(profReqPath(),row,true);
    logLine(L"profile change request from "+x.user);
}
void setProfReqStatus(int indexNewestFirst, int status, const std::wstring& reason){
    auto v=loadProfReqs();   // newest first
    if(indexNewestFirst<0||indexNewestFirst>=(int)v.size()) return;
    ProfReq& r=v[indexNewestFirst];
    r.status=status; r.reason=reason;
    if(status==1){
        // apply: persist the new display name as the user's fullname override
        if(!r.newName.empty()) setSetting(L"name_override_"+r.user, r.newName);
        if(!r.newPhoto.empty()) setSetting(L"photo_"+r.user, r.newPhoto);
        pushMessageT(L"مدیریت", r.user,
            L"درخواست تغییر پروفایل شما تأیید شد.", KMSG_NORMAL);
    } else if(status==2){
        std::wstring t=L"درخواست تغییر پروفایل شما رد شد.";
        if(!reason.empty()) t+=L" دلیل: "+reason;
        pushMessageT(L"مدیریت", r.user, t, KMSG_CRITICAL);
    }
    saveAllProfReqs_newestFirst(v);
}
int unseenProfReqCount(){
    int n=0; for(auto&r:loadProfReqs()) if(r.status==0) n++; return n;
}

// ============================================================================
//  CARTABLE v2 — pin / seen-one / delete-one (per-user, reported to manager)
// ============================================================================
//  Messages are stored in employees.cpp's data\messages.dat with the format:
//      from|to|time|seen|type|text
//  We add an OPTIONAL 7th field "pin" (0/1). Older 6-field rows read pin=0.
//  Acting on "the i-th message newest-first for this user" means we must map
//  back to the matching disk row. We re-load, filter by recipient, and toggle.
static std::wstring msgPath2(){ return dataDir()+L"\\messages.dat"; }
struct RawMsg { std::vector<std::wstring> f; };
static std::vector<RawMsg> loadRaw(){
    std::vector<RawMsg> out;
    std::wstring all=readFileUtf8(msgPath2());
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=all.substr(pos,e-pos); pos=e+1;
        if(trim(line).empty()) continue;
        RawMsg r; r.f=dx_split(line,L'|');
        if(r.f.size()<5) continue;
        while(r.f.size()<7) r.f.push_back(r.f.size()==4?L"":L"0"); // pad type/text/pin
        out.push_back(r);
    }
    return out;
}
static void saveRaw(const std::vector<RawMsg>& v){
    std::wstring out;
    for(auto&r:v){
        std::wstring line;
        for(size_t i=0;i<r.f.size();i++){ if(i) line+=L"|"; line+=r.f[i]; }
        out+=line+L"\r\n";
    }
    writeFileUtf8(msgPath2(),out,false);
}
//  map "i-th newest-first for forUser" → raw row index
static int rawIndexForUserMsg(const std::vector<RawMsg>& raw,
                              const std::wstring& forUser, int indexNewestFirst){
    // collect raw indices that belong to this user, in file (oldest-first) order
    std::vector<int> mine;
    for(size_t i=0;i<raw.size();i++){
        const std::wstring& to=raw[i].f[1];
        if(to==L"*" || to==forUser || forUser.empty()) mine.push_back((int)i);
    }
    // newest-first: reverse
    int n=(int)mine.size();
    int wantOldestFirst = n-1-indexNewestFirst;
    if(wantOldestFirst<0||wantOldestFirst>=n) return -1;
    return mine[wantOldestFirst];
}
void pinMessage(const std::wstring& forUser, int indexNewestFirst, bool pin){
    auto raw=loadRaw();
    int ri=rawIndexForUserMsg(raw,forUser,indexNewestFirst);
    if(ri<0) return;
    while(raw[ri].f.size()<7) raw[ri].f.push_back(L"0");
    raw[ri].f[6]= pin?L"1":L"0";
    saveRaw(raw);
}
void seenOneMessage(const std::wstring& forUser, int indexNewestFirst){
    auto raw=loadRaw();
    int ri=rawIndexForUserMsg(raw,forUser,indexNewestFirst);
    if(ri<0) return;
    if(raw[ri].f[3]!=L"1"){
        raw[ri].f[3]=L"1";
        saveRaw(raw);
        pushMessageT(forUser, L"مدیریت",
            L"کاربر «"+forUser+L"» پیام را مشاهده کرد.", KMSG_NORMAL);
    }
}
void deleteOneMessage(const std::wstring& forUser, int indexNewestFirst){
    auto raw=loadRaw();
    int ri=rawIndexForUserMsg(raw,forUser,indexNewestFirst);
    if(ri<0) return;
    std::wstring txt = raw[ri].f.size()>5?raw[ri].f[5]:L"";
    raw.erase(raw.begin()+ri);
    saveRaw(raw);
    pushMessageT(forUser, L"مدیریت",
        L"کاربر «"+forUser+L"» پیامی را حذف کرد.", KMSG_NORMAL);
}
