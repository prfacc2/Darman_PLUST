// ============================================================================
//  util.cpp — time (Iran/Jalali), settings, files, money helpers, logging
// ============================================================================
#include "app.h"
#include <shlwapi.h>
#include <stdio.h>

// ----------------------------------------------------------------- exe dir -
std::wstring exeDir(){
    wchar_t buf[MAX_PATH]; GetModuleFileNameW(NULL, buf, MAX_PATH);
    wchar_t* p = wcsrchr(buf, L'\\'); if(p) *p = 0;
    return buf;
}
static std::wstring ensureDir(const std::wstring& d){
    CreateDirectoryW(d.c_str(), NULL); return d;
}
// data root may be redirected to a SHARED NETWORK FOLDER so that all reception
// terminals + the management station read/write the SAME files (designs,
// messages, settings-change requests) → live sync across the network.
//   • create  <exe>\dataroot.ini  containing a single line with a UNC/drive path
//     e.g.  \\SERVER\DarmanPlus\data    or   Z:\DarmanPlus\data
//   • if absent (or unreachable) we fall back to the local  <exe>\data  folder.
static std::wstring readDataRootOverride(){
    std::wstring cfg = exeDir()+L"\\dataroot.ini";
    HANDLE h=CreateFileW(cfg.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,
        OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE) return L"";
    char buf[1024]={0}; DWORD rd=0;
    ReadFile(h,buf,sizeof(buf)-1,&rd,NULL); CloseHandle(h);
    if(rd==0) return L"";
    // strip a UTF-8 BOM if present
    int off=0; if(rd>=3 && (unsigned char)buf[0]==0xEF) off=3;
    int wn=MultiByteToWideChar(CP_UTF8,0,buf+off,(int)rd-off,NULL,0);
    std::wstring s(wn,0);
    MultiByteToWideChar(CP_UTF8,0,buf+off,(int)rd-off,&s[0],wn);
    // first non-empty trimmed line
    std::wstring line; for(wchar_t c:s){ if(c==L'\r'||c==L'\n'){ if(!line.empty())break; } else line+=c; }
    while(!line.empty() && (line.front()==L' '||line.front()==L'\t')) line.erase(line.begin());
    while(!line.empty() && (line.back()==L' '||line.back()==L'\t')) line.pop_back();
    return line;
}
std::wstring dataDir(){
    static std::wstring cached;
    static bool resolved=false;
    if(!resolved){
        resolved=true;
        std::wstring ov=readDataRootOverride();
        if(!ov.empty()){
            // try to create / reach it; if it works, use it
            CreateDirectoryW(ov.c_str(),NULL);
            DWORD a=GetFileAttributesW(ov.c_str());
            if(a!=INVALID_FILE_ATTRIBUTES && (a&FILE_ATTRIBUTE_DIRECTORY))
                cached=ov;
        }
        if(cached.empty()) cached=ensureDir(exeDir()+L"\\data");
    }
    return cached;
}
std::wstring logsDir(){ return ensureDir(exeDir()+L"\\logs"); }

// §I (1.11.0): stamp data\.schema_version with the current app version. This is
// STRICTLY INFORMATIONAL — it is written once at startup and read by NOTHING in
// the load path, so it can never gate, migrate, or rewrite any data file. It
// exists only so a human (or a backup tool) can see which build last touched the
// data folder. The previous value (if any) is preserved as a second line for an
// audit trail, and any extra lines already present are kept verbatim (§H
// forward-compat: never discard what we did not write).
void writeSchemaVersion(){
    std::wstring path = dataDir()+L"\\.schema_version";
    std::wstring prev = readFileUtf8(path);
    // first existing line (the version last stamped), if any
    std::wstring prevFirst;
    { size_t e=prev.find(L'\n'); prevFirst = (e==std::wstring::npos)?prev:prev.substr(0,e);
      // strip trailing CR
      while(!prevFirst.empty() && (prevFirst.back()==L'\r'||prevFirst.back()==L'\n'))
          prevFirst.pop_back(); }
    std::wstring cur = APP_VERSION_W;
    if(prevFirst==cur) return;                 // already stamped — leave untouched
    std::wstring out = cur + L"\r\n";
    if(!prevFirst.empty()) out += L"# previously: " + prevFirst + L"\r\n";
    writeFileUtf8(path,out,false);
}

// --------------------------------------------------------------- utf8 file -
bool writeFileUtf8(const std::wstring& path, const std::wstring& text, bool append){
    HANDLE h = CreateFileW(path.c_str(), append?FILE_APPEND_DATA:GENERIC_WRITE,
        FILE_SHARE_READ, NULL, append?OPEN_ALWAYS:CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if(h==INVALID_HANDLE_VALUE) return false;
    int n = WideCharToMultiByte(CP_UTF8,0,text.c_str(),(int)text.size(),NULL,0,NULL,NULL);
    std::vector<char> buf(n);
    WideCharToMultiByte(CP_UTF8,0,text.c_str(),(int)text.size(),buf.data(),n,NULL,NULL);
    DWORD wr; WriteFile(h, buf.data(), n, &wr, NULL);
    CloseHandle(h); return true;
}
std::wstring readFileUtf8(const std::wstring& path){
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(h==INVALID_HANDLE_VALUE) return L"";
    DWORD sz = GetFileSize(h, NULL);
    std::vector<char> buf(sz+1, 0);
    DWORD rd; ReadFile(h, buf.data(), sz, &rd, NULL); CloseHandle(h);
    const char* s = buf.data(); int len=(int)rd;
    if(len>=3 && (unsigned char)s[0]==0xEF){ s+=3; len-=3; }   // BOM
    int n = MultiByteToWideChar(CP_UTF8,0,s,len,NULL,0);
    std::wstring out(n, 0);
    MultiByteToWideChar(CP_UTF8,0,s,len,&out[0],n);
    return out;
}

// ---------------------------------------------------------------- logging --
//  RELEASE 1.2.0 — user-behavior logging policy overhaul (Section A.2).
//  The general-purpose app.log channel is DISABLED in release builds. The only
//  remaining log channels are the dedicated Backup Log (src/backup_log.cpp) and
//  the crash handler dump. logLine() is now a no-op unless AZ_DEBUG_LOGS is
//  explicitly defined at compile time (it is OFF in release). Call sites are
//  left intact so debug builds can still trace, but nothing is written to disk
//  during normal operation.
void logLine(const std::wstring& s){
#if AZ_DEBUG_LOGS
    SYSTEMTIME st = iranNow();
    wchar_t pre[64];
    swprintf(pre,64,L"[%04d-%02d-%02d %02d:%02d:%02d] ",
        st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
    writeFileUtf8(logsDir()+L"\\logs\\app.log", std::wstring(pre)+s+L"\r\n", true);
#else
    (void)s;   // release: do not write any user-behavior telemetry
#endif
}
void logError(const std::wstring& s){
    SYSTEMTIME st = iranNow();
    wchar_t pre[64];
    swprintf(pre,64,L"[%04d-%02d-%02d %02d:%02d:%02d] ",
        st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
    writeFileUtf8(logsDir()+L"\\errors.log", std::wstring(pre)+s+L"\r\n", true);
}

// --------------------------------------------------------------- settings --
//  simple key=value store: data\settings.ini
static std::wstring settingsPath(){ return dataDir()+L"\\settings.ini"; }
std::wstring getSetting(const std::wstring& key, const std::wstring& def){
    std::wstring all = readFileUtf8(settingsPath());
    size_t pos = 0;
    while(pos < all.size()){
        size_t e = all.find(L'\n', pos);
        if(e==std::wstring::npos) e = all.size();
        std::wstring line = trim(all.substr(pos, e-pos));
        pos = e+1;
        // §H: skip comment lines so a future "# key=note" can never be mistaken
        //     for a real value.
        if(line.empty() || line[0]==L'#' || line[0]==L';') continue;
        size_t eq = line.find(L'=');
        if(eq!=std::wstring::npos && trim(line.substr(0,eq))==key)
            return trim(line.substr(eq+1));
    }
    return def;
}
void setSetting(const std::wstring& key, const std::wstring& val){
    //  §H future-update safety: this rewriter PRESERVES every line it does not
    //  own — comments (#, ;), blank lines, section markers ([..]) and any
    //  unknown key=value pairs written by a newer version are all carried over
    //  verbatim. Only the single matching key is replaced (or appended).
    std::wstring all = readFileUtf8(settingsPath());
    std::wstring out; bool done=false; size_t pos=0;
    while(pos < all.size()){
        size_t e = all.find(L'\n', pos);
        if(e==std::wstring::npos) e = all.size();
        std::wstring raw  = all.substr(pos, e-pos);
        // strip a trailing CR so we re-emit with a single canonical CRLF
        if(!raw.empty() && raw.back()==L'\r') raw.pop_back();
        std::wstring line = trim(raw);
        pos = e+1;
        bool isComment = (!line.empty() && (line[0]==L'#' || line[0]==L';'));
        size_t eq = line.find(L'=');
        if(!isComment && !line.empty() && eq!=std::wstring::npos &&
           trim(line.substr(0,eq))==key){
            out += key+L"="+val+L"\r\n"; done=true;     // replace our key
        } else {
            // preserve EVERYTHING else exactly — comments, blanks, other keys.
            out += raw+L"\r\n";
        }
    }
    if(!done) out += key+L"="+val+L"\r\n";
    writeFileUtf8(settingsPath(), out, false);
}

// ------------------------------------------------------------------- trim --
std::wstring trim(const std::wstring& s){
    size_t a = s.find_first_not_of(L" \t\r\n");
    if(a==std::wstring::npos) return L"";
    size_t b = s.find_last_not_of(L" \t\r\n");
    return s.substr(a, b-a+1);
}

// ------------------------------------------------------------------ money --
std::wstring formatMoney(long long v){
    bool neg = v<0; if(neg) v=-v;
    wchar_t raw[32]; swprintf(raw,32,L"%lld",v);
    std::wstring s(raw), out;
    int c=0;
    for(int i=(int)s.size()-1;i>=0;i--){
        out.insert(out.begin(), s[i]);
        if(++c%3==0 && i>0) out.insert(out.begin(), L',');
    }
    if(neg) out.insert(out.begin(), L'-');
    return out;
}
long long parseMoney(const std::wstring& s){
    long long v=0; bool neg=false;
    for(wchar_t c : s){
        if(c==L'-') neg=true;
        else if(c>=L'0'&&c<=L'9') v = v*10 + (c-L'0');
        else if(c>=0x06F0&&c<=0x06F9) v = v*10 + (c-0x06F0);   // ۰..۹
        else if(c>=0x0660&&c<=0x0669) v = v*10 + (c-0x0660);   // ٠..٩
    }
    return neg?-v:v;
}
std::wstring toFaDigits(const std::wstring& s){
    std::wstring out = s;
    for(auto& c : out) if(c>=L'0'&&c<=L'9') c = (wchar_t)(0x06F0 + (c-L'0'));
    return out;
}

// -------------------------------------------------------------- Iran time --
//  Iran abolished DST in 2022 → fixed UTC+3:30 all year.
SYSTEMTIME iranNow(){
    FILETIME ft; GetSystemTimeAsFileTime(&ft);          // UTC
    ULARGE_INTEGER u; u.LowPart=ft.dwLowDateTime; u.HighPart=ft.dwHighDateTime;
    u.QuadPart += 12600LL * 10000000LL;                  // +3h30m
    ft.dwLowDateTime=u.LowPart; ft.dwHighDateTime=u.HighPart;
    SYSTEMTIME st; FileTimeToSystemTime(&ft,&st);
    return st;
}
int iranMinutesOfDay(){
    SYSTEMTIME st = iranNow();
    return st.wHour*60 + st.wMinute;
}

// ----------------------------------------------------- Gregorian → Jalali --
void gregToJalali(int gy,int gm,int gd,int&jy,int&jm,int&jd){
    static const int gdm[] = {0,31,59,90,120,151,181,212,243,273,304,334};
    int gy2 = (gm>2) ? gy+1 : gy;
    long days = 355666L + 365L*gy + (gy2+3)/4 - (gy2+99)/100
              + (gy2+399)/400 + gd + gdm[gm-1];
    jy = -1595 + 33*(int)(days/12053); days %= 12053;
    jy += 4*(int)(days/1461);          days %= 1461;
    if(days > 365){ jy += (int)((days-1)/365); days = (days-1)%365; }
    if(days < 186){ jm = 1+(int)(days/31); jd = 1+(int)(days%31); }
    else          { jm = 7+(int)((days-186)/30); jd = 1+(int)((days-186)%30); }
}
static const wchar_t* JMONTH[12] = {
    L"\u0641\u0631\u0648\u0631\u062f\u06cc\u0646", L"\u0627\u0631\u062f\u06cc\u0628\u0647\u0634\u062a",
    L"\u062e\u0631\u062f\u0627\u062f", L"\u062a\u06cc\u0631",
    L"\u0645\u0631\u062f\u0627\u062f", L"\u0634\u0647\u0631\u06cc\u0648\u0631",
    L"\u0645\u0647\u0631", L"\u0622\u0628\u0627\u0646", L"\u0622\u0630\u0631",
    L"\u062f\u06cc", L"\u0628\u0647\u0645\u0646", L"\u0627\u0633\u0641\u0646\u062f" };
static const wchar_t* WDAY[7] = {   // SYSTEMTIME wDayOfWeek: 0=Sunday
    L"\u06cc\u06a9\u0634\u0646\u0628\u0647",          // یکشنبه
    L"\u062f\u0648\u0634\u0646\u0628\u0647",          // دوشنبه
    L"\u0633\u0647\u200c\u0634\u0646\u0628\u0647",    // سه‌شنبه
    L"\u0686\u0647\u0627\u0631\u0634\u0646\u0628\u0647", // چهارشنبه
    L"\u067e\u0646\u062c\u0634\u0646\u0628\u0647",    // پنجشنبه
    L"\u062c\u0645\u0639\u0647",                      // جمعه
    L"\u0634\u0646\u0628\u0647" };                    // شنبه
std::wstring jalaliDateStr(const SYSTEMTIME& st){
    int jy,jm,jd; gregToJalali(st.wYear,st.wMonth,st.wDay,jy,jm,jd);
    // Build the date using Persian digits and wrap each numeric run with the
    // Right-to-Left Mark (U+200F). Without these marks the BiDi engine that
    // backs DT_RTLREADING reorders the two separate number groups (day + year)
    // around the Persian month/weekday words, producing garbled output. RLM
    // forces every digit run to keep its logical RTL position so the date
    // always renders as:  «weekday  day  month  year».
    const wchar_t RLM = 0x200F;
    std::wstring day  = toFaDigits(std::to_wstring(jd));
    std::wstring year = toFaDigits(std::to_wstring(jy));
    std::wstring out;
    out += WDAY[st.wDayOfWeek];  out += L' ';
    out += RLM; out += day;  out += RLM; out += L' ';
    out += JMONTH[jm-1];         out += L' ';
    out += RLM; out += year; out += RLM;
    return out;
}
std::wstring jalaliDateShort(const SYSTEMTIME& st){
    int jy,jm,jd; gregToJalali(st.wYear,st.wMonth,st.wDay,jy,jm,jd);
    wchar_t buf[32]; swprintf(buf,32,L"%04d/%02d/%02d",jy,jm,jd);
    return buf;
}
// ----------------------------------------------------------------------------
//  v1.4.0: canonical Persian Jalali date formatter.
//  Returns «۱۴۰۵/۰۴/۰۲» with Persian-Indic digits and RTL marks around each
//  numeric run so the BiDi engine keeps the order stable under DT_RTLREADING.
//  `utc==0` means "now" (Tehran). The Vazirmatn font (forced on date labels)
//  supplies the Persian-Indic glyphs.
std::wstring FormatJalaliPersian(time_t utc){
    SYSTEMTIME st;
    if(utc==0){
        st = iranNow();
    } else {
        // Convert UTC time_t -> Tehran SYSTEMTIME (fixed +3:30, no DST).
        ULONGLONG ll = (ULONGLONG)(utc) * 10000000ULL + 116444736000000000ULL;
        ll += 12600ULL * 10000000ULL;        // +3h30m
        FILETIME ft; ft.dwLowDateTime=(DWORD)ll; ft.dwHighDateTime=(DWORD)(ll>>32);
        if(!FileTimeToSystemTime(&ft,&st)) st = iranNow();
    }
    int jy,jm,jd; gregToJalali(st.wYear,st.wMonth,st.wDay,jy,jm,jd);
    const wchar_t RLM = 0x200F;
    wchar_t buf[32]; swprintf(buf,32,L"%04d/%02d/%02d",jy,jm,jd);
    std::wstring out;
    out += RLM; out += toFaDigits(buf); out += RLM;
    return out;
}
std::wstring JalaliTodayKey(){
    SYSTEMTIME st = iranNow();
    int jy,jm,jd; gregToJalali(st.wYear,st.wMonth,st.wDay,jy,jm,jd);
    wchar_t buf[16]; swprintf(buf,16,L"%04d/%02d/%02d",jy,jm,jd);
    return buf;
}
std::wstring iranTimeStr(const SYSTEMTIME& st, bool seconds){
    wchar_t buf[16];
    if(seconds) swprintf(buf,16,L"%02d:%02d:%02d",st.wHour,st.wMinute,st.wSecond);
    else        swprintf(buf,16,L"%02d:%02d",st.wHour,st.wMinute);
    return buf;
}

// ------------------------------------------------------------------ shift --
//  06:00–14:30 صبح | 14:30–22:30 عصر | 22:30–06:00 شب
int detectShift(){
    int m = iranMinutesOfDay();
    if(m >= 6*60 && m < 14*60+30)  return 0;
    if(m >= 14*60+30 && m < 22*60+30) return 1;
    return 2;
}
std::wstring shiftName(int s){
    switch(s){
        case 0: return L"\u0635\u0628\u062d";                                  // صبح
        case 1: return L"\u0628\u0639\u062f \u0627\u0632 \u0638\u0647\u0631"; // بعد از ظهر
        default:return L"\u0634\u0628";                                       // شب
    }
}
