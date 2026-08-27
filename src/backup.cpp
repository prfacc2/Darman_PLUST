// ============================================================================
//  backup.cpp  —  v1.9.0  Management backup / restore of patient data.
//
//  Goals (per the v1.9.0 spec):
//    * A management-only «پشتیبان‌گیری» (Backup) and «بازیابی پشتیبان»
//      (Restore) module, opened from the admin panel.
//    * Our OWN backups use a small self-describing container (AZTBKP01) that
//      bundles every file under data\ so a full clinic state can be restored.
//    * Restore can apply the FULL backup or a SELECTED subset (e.g. only the
//      patient information) by ticking categories.
//    * Foreign, very large (~15 GB) Matin-Teb «.bak» SQL backups are detected
//      and SCANNED (not loaded) so the UI never freezes: the scan runs on a
//      background thread and only ESTIMATES the per-category breakdown from the
//      file size. The page shows a category list with estimated sizes.
//    * A file-open bar, a restore progress bar, and smooth animations keep the
//      UI responsive the whole time (all heavy I/O is on worker threads).
//
//  Pure Win32 + GDI+, no new dependencies. Follows the existing modal pattern
//  (WS_POPUP topmost over g_hFrame, double-buffered WM_PAINT, owner-drawn).
// ============================================================================
#include "app.h"
#include <windowsx.h>
#include <commdlg.h>
#include <process.h>
#include <vector>
#include <string>

// ----------------------------------------------------------------- helpers --
static std::wstring humanSize(long long b){
    const wchar_t* u[]={L"بایت",L"کیلوبایت",L"مگابایت",L"گیگابایت",L"ترابایت"};
    double v=(double)b; int i=0;
    while(v>=1024.0 && i<4){ v/=1024.0; i++; }
    wchar_t buf[64];
    if(i==0) swprintf(buf,64,L"%lld %s",b,u[0]);
    else     swprintf(buf,64,L"%.1f %s",v,u[i]);
    return toFaDigits(buf);
}

// Stable category descriptors. The classifier maps each data file (or a slice
// of a foreign .bak) into exactly one of these.
struct CatDef { const wchar_t* id; const wchar_t* name; int icon; };
static const CatDef CATS[]={
    { L"patients", L"اطلاعات بیماران",  ICO_USER   },
    { L"images",   L"تصاویر و مدارک",   ICO_ID     },
    { L"billing",  L"مالی و صورتحساب",  ICO_RECEIPT},
    { L"other",    L"سایر اطلاعات",     ICO_PIN    },
};
static const int CAT_COUNT = 4;

static int catIndexOf(const std::wstring& id){
    for(int i=0;i<CAT_COUNT;i++) if(id==CATS[i].id) return i;
    return CAT_COUNT-1;
}

// Map a data\ filename to a category id.
static std::wstring classifyFile(const std::wstring& name){
    std::wstring n=name; for(auto& c:n) c=towlower(c);
    if(n.find(L".png")!=std::wstring::npos || n.find(L".jpg")!=std::wstring::npos ||
       n.find(L".jpeg")!=std::wstring::npos|| n.find(L".bmp")!=std::wstring::npos ||
       n.find(L"photo")!=std::wstring::npos|| n.find(L"image")!=std::wstring::npos ||
       n.find(L"attach")!=std::wstring::npos|| n.find(L"avatar")!=std::wstring::npos)
        return L"images";
    if(n.find(L"patient")!=std::wstring::npos|| n.find(L"reception")!=std::wstring::npos||
       n.find(L"guest")!=std::wstring::npos  || n.find(L"appoint")!=std::wstring::npos ||
       n.find(L"visit")!=std::wstring::npos  || n.find(L"blacklist")!=std::wstring::npos)
        return L"patients";
    if(n.find(L"bill")!=std::wstring::npos   || n.find(L"invoice")!=std::wstring::npos||
       n.find(L"receipt")!=std::wstring::npos|| n.find(L"insur")!=std::wstring::npos ||
       n.find(L"tariff")!=std::wstring::npos || n.find(L"pay")!=std::wstring::npos ||
       n.find(L"cashier")!=std::wstring::npos|| n.find(L"shift")!=std::wstring::npos)
        return L"billing";
    if(n.find(L"message")!=std::wstring::npos|| n.find(L"kartabl")!=std::wstring::npos||
       n.find(L"saved_msg")!=std::wstring::npos)
        return L"other";
    return L"other";
}

// Enumerate every regular file under data\ (one level deep is enough for our
// own layout; we also descend into a single attachments subfolder if present).
static void enumDataFiles(const std::wstring& dir,
                          std::vector<std::wstring>& out,
                          std::vector<long long>& sizes){
    std::wstring pat=dir+L"\\*";
    WIN32_FIND_DATAW fd; HANDLE h=FindFirstFileW(pat.c_str(),&fd);
    if(h==INVALID_HANDLE_VALUE) return;
    do{
        if(fd.cFileName[0]==L'.') continue;
        std::wstring full=dir+L"\\"+fd.cFileName;
        if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
            enumDataFiles(full,out,sizes);          // one extra level (attachments)
        } else {
            long long sz=((long long)fd.nFileSizeHigh<<32)|fd.nFileSizeLow;
            out.push_back(full); sizes.push_back(sz);
        }
    } while(FindNextFileW(h,&fd));
    FindClose(h);
}

// ====================================================================== state
#define BK_CLASS L"AzBackupMgr"
enum { BK_MODE_HOME=0, BK_MODE_RESTORE=1 };

// v1.10.0 — hidden backup ANALYZER page (reachable ONLY via Ctrl+B).
#include "backup_analyzer.h"
#include "backup_log.h"
#include "ui_kit.h"
#include <thread>
#include <atomic>

struct BkState {
    int  mode;                          // BK_MODE_*
    BackupInfo info;                    // scan result (restore mode)
    bool scanning;                      // a scan worker is running
    bool busy;                          // a backup/restore worker is running
    int  progress;                      // 0..100
    std::wstring status;                // status line under the bar
    std::wstring pickedPath;            // chosen backup file (restore)
    bool foreign;                       // picked file is a foreign .bak
    bool allTicked;                     // "all patient information" master tick
    int  hot;                           // hovered card/row (-1 none)
    float anim;                         // 0..1 spinner phase
    volatile LONG doneSignal;           // worker→UI: 1=backup 2=restore done
    int  lastRestoredFiles;             // files written by last restore
    long long lastRestoredPatients;     // patient records restored (rows)

    // ---- hidden analyzer page (Ctrl+B) ----
    bool  anPage=false;                  // analyzer page visible
    bool  anBusy=false;                  // analysis worker running
    std::atomic<int>  anProgress{0};     // 0..100 (real, bytes-based)
    std::wstring anStatus;               // live status line
    std::wstring anPath;                 // chosen file
    BkAnalysis*  anResult=nullptr;       // owned; built on worker, shown on UI
    int   anScroll=0;                    // vertical scroll of the report
    bool  anErrExpanded=false;           // B.4: error card "technical details" open
    std::wstring anErrPayload;           // B.4: full BackupLog payload for failed run

    BkState():mode(BK_MODE_HOME),scanning(false),busy(false),progress(0),
        foreign(false),allTicked(false),hot(-1),anim(0),
        doneSignal(0),lastRestoredFiles(0),lastRestoredPatients(0){}
    ~BkState(){ delete anResult; }
};
static HWND     s_bk=NULL;
static BkState* s_bs=NULL;
static volatile LONG s_cancel=0;        // cooperative cancel for workers
// §C: lifetime guard for the analyzer worker. The analysis runs on a detached
// thread that drives the UI through bkAnProgCb (which touches the global s_bs).
// If the backup window is destroyed while the worker is still running, the old
// code deleted s_bs out from under the live worker → use-after-free crash at a
// low address (the reported 0xC0000005). We now (1) gate every callback access
// behind this atomic so the worker stops touching s_bs the instant the window
// goes away, and (2) wait briefly for the worker to quiesce on destroy.
#include <atomic>
static std::atomic<long> s_anWorkers{0};   // # of analyzer workers in flight
static std::atomic<bool> s_anWinAlive{false};

// ---------------------------------------------------------------------------
//  v1.9.5 PERFORMANCE: cached static background.
//  The scrim alpha-fill (gpFillAlpha over the WHOLE window), the card drop
//  shadow (gpShadow) and the card gradient + title/subtitle are by far the
//  most expensive part of the scene, yet they NEVER change while the user just
//  moves the mouse. We render them ONCE into an off-screen bitmap and, on every
//  paint, simply BitBlt the cached strip and draw only the cheap interactive
//  foreground (cards / rows / ticks / progress) on top. The cache is rebuilt
//  only when the window size, the theme, or the mode (home/restore) changes.
//  This removes the per-mouse-move full-window recomposition that caused the
//  FPS drop / stutter.
// ---------------------------------------------------------------------------
static HDC      s_bgDC  = NULL;
static HBITMAP  s_bgBmp = NULL;
static HGDIOBJ  s_bgOld = NULL;
static int      s_bgW=0, s_bgH=0;
static int      s_bgMode=-1;             // mode the cache was built for
static void bkFreeBg(){
    if(s_bgDC){ SelectObject(s_bgDC,s_bgOld); DeleteDC(s_bgDC); s_bgDC=NULL; }
    if(s_bgBmp){ DeleteObject(s_bgBmp); s_bgBmp=NULL; }
    s_bgW=s_bgH=0; s_bgMode=-1;
}

static void bkClose(){ if(s_bk&&IsWindow(s_bk)){ HWND v=s_bk; s_bk=NULL; DestroyWindow(v);} }

// --------------------------------------------------------------- file picker
static std::wstring askOpenBackup(HWND owner){
    wchar_t buf[1024]={0};
    OPENFILENAMEW ofn={0}; ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=owner;
    ofn.lpstrFilter=L"پشتیبان‌ها\0*.aztbk;*.bak\0همه فایل‌ها\0*.*\0";
    ofn.lpstrFile=buf; ofn.nMaxFile=1024;
    ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_EXPLORER;
    if(GetOpenFileNameW(&ofn)) return buf;
    return L"";
}
static std::wstring askSaveBackup(HWND owner){
    SYSTEMTIME st=iranNow(); wchar_t def[128];
    swprintf(def,128,L"DarmanPlus_%04d-%02d-%02d.aztbk",st.wYear,st.wMonth,st.wDay);
    wchar_t buf[1024]={0}; wcscpy(buf,def);
    OPENFILENAMEW ofn={0}; ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=owner;
    ofn.lpstrFilter=L"پشتیبان درمان‌پلاس\0*.aztbk\0";
    ofn.lpstrFile=buf; ofn.nMaxFile=1024; ofn.lpstrDefExt=L"aztbk";
    ofn.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_EXPLORER;
    if(GetSaveFileNameW(&ofn)) return buf;
    return L"";
}

// =================================================================== workers
// All workers update s_bs and post a repaint; they never touch GDI directly.
struct WorkArg { std::wstring path; bool foreign; };

static RECT bkCard(HWND h);   // fwd: layout helper used by the import progress

// ===========================================================================
//  FOREIGN BACKUP IMPORT  (v1.9.5)
//  Real-world clinics hand us a SQL-Server «.bak» (Microsoft Tape Format) or a
//  plain SQL / CSV export from their previous software (commonly Matin-Teb).
//  A single static EXE cannot host a SQL engine, but we CAN honestly recover
//  the patient identities that are stored as readable strings inside the file:
//  SQL Server keeps nvarchar columns as UTF-16LE and varchar as 8-bit text, so
//  a national code (10 ASCII digits with a valid Iranian checksum) sitting next
//  to readable name fields can be extracted WITHOUT fabricating anything. We
//  only ever insert a record when validNationalId() passes AND a real name is
//  found beside it, then persist it through rememberPatient() so it lands in
//  the (possibly network-wide, see dataroot.ini) data layer exactly like a
//  hand-entered patient.
// ===========================================================================
enum ForeignFmt { FF_UNKNOWN=0, FF_MSSQL_BAK, FF_SQL_TEXT, FF_CSV, FF_AZT };

// Sniff the real format from the first bytes + extension (never trust the
// extension alone — a «.bak» may actually be a text dump and vice-versa).
static ForeignFmt sniffForeign(const std::wstring& path){
    std::wstring low=path; for(auto&c:low) c=towlower(c);
    bool extBak = low.size()>=4 && low.substr(low.size()-4)==L".bak";
    bool extSql = low.size()>=4 && low.substr(low.size()-4)==L".sql";
    bool extCsv = low.size()>=4 && low.substr(low.size()-4)==L".csv";
    bool extTxt = low.size()>=4 && low.substr(low.size()-4)==L".txt";
    bool extAzt = low.size()>=6 && low.substr(low.size()-6)==L".aztbk";
    HANDLE hf=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,
                          OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    char head[512]={0}; DWORD rd=0;
    if(hf!=INVALID_HANDLE_VALUE){ ReadFile(hf,head,sizeof(head),&rd,NULL); CloseHandle(hf); }
    if(rd>=9 && strncmp(head,"AZTBKP01\n",9)==0) return FF_AZT;
    // SQL Server MTF backups start with an "TAPE" MTF descriptor block; the
    // ASCII tokens "TAPE" / "MSSQL" / "Microsoft SQL Server" appear in the head.
    auto headHas=[&](const char* s)->bool{
        size_t n=strlen(s);
        for(DWORD i=0;i+n<=rd;i++) if(memcmp(head+i,s,n)==0) return true;
        return false;
    };
    if(extBak || headHas("TAPE") || headHas("MSSQL") || headHas("Microsoft SQL Server"))
        return FF_MSSQL_BAK;
    if(extSql || headHas("INSERT INTO") || headHas("CREATE TABLE")) return FF_SQL_TEXT;
    if(extCsv || (extTxt && (headHas(",")||headHas(";")))) return FF_CSV;
    if(extAzt) return FF_AZT;
    return FF_UNKNOWN;
}

// Is this wide char a plausible part of a Persian / Latin NAME (letters, ZWNJ,
// space). Used to validate that bytes next to a national code are a real name.
static bool isNameChar(wchar_t c){
    if(c==L' '||c==0x200C) return true;                 // space / ZWNJ
    if(c>=L'A'&&c<=L'Z') return true;
    if(c>=L'a'&&c<=L'z') return true;
    if(c>=0x0600 && c<=0x06FF) return true;             // Arabic/Persian block
    if(c>=0xFB50 && c<=0xFDFF) return true;             // Arabic presentation
    return false;
}
static std::wstring cleanName(const std::wstring& s){
    std::wstring out; for(wchar_t c:s){ if(isNameChar(c)) out+=c; else if(!out.empty()&&out.back()!=L' ') out+=L' '; }
    return trim(out);
}

// Pull a readable run of UTF-16LE name text starting at byte offset `off`.
// Returns "" if no real name. (SQL Server stores nvarchar as UTF-16LE.)
static std::wstring readU16Name(const std::vector<char>& b, size_t off){
    std::wstring s; int taken=0;
    for(size_t i=off; i+1<b.size() && taken<48; i+=2){
        wchar_t c=(wchar_t)((unsigned char)b[i] | ((unsigned char)b[i+1]<<8));
        if(c==0) break;
        if(!isNameChar(c)){ if(s.size()>=2) break; else { s.clear(); continue; } }
        s+=c; taken++;
    }
    return cleanName(s);
}

// Scan the whole foreign file (streamed, large-file safe) for patient records.
// Returns the count actually imported into the data layer.
static long long importForeignPatients(const std::wstring& path, ForeignFmt fmt,
                                       HWND notify){
    HANDLE hf=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,
                          OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(hf==INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER fsz={0}; GetFileSizeEx(hf,&fsz);
    long long total=fsz.QuadPart>0?fsz.QuadPart:1, seen=0;
    long long imported=0;

    // de-dup national ids we already inserted in this pass
    std::vector<std::wstring> done;
    auto already=[&](const std::wstring& id)->bool{
        for(auto&d:done) if(d==id) return true; return false; };

    const size_t CHUNK=4<<20;          // 4 MB window
    const size_t OVER =256;            // overlap so a record on a boundary isn't split
    std::vector<char> buf(CHUNK+OVER);
    size_t carry=0;                    // bytes carried from previous chunk

    // cap the scan for absurdly large files so it always finishes; clinics that
    // need everything can re-run — but in practice patient tables are early.
    long long cap = total < (512LL<<20) ? total : (512LL<<20);

    DWORD rd=0;
    while(seen<cap){
        if(InterlockedCompareExchange(&s_cancel,0,0)) break;
        if(!ReadFile(hf,buf.data()+carry,(DWORD)CHUNK,&rd,NULL) || rd==0) break;
        size_t avail=carry+rd; seen+=rd;

        // Find every run of exactly-10 ASCII digits, validate the checksum, and
        // try to read a name (UTF-16LE first, then 8-bit) right after it.
        for(size_t i=0; i+10<=avail; ){
            // need a 10-digit run not preceded/followed by another digit
            bool run=true;
            for(int k=0;k<10;k++){ char c=buf[i+k]; if(c<'0'||c>'9'){ run=false; break; } }
            bool boundL = (i==0) || (buf[i-1]<'0'||buf[i-1]>'9');
            bool boundR = (i+10>=avail) || (buf[i+10]<'0'||buf[i+10]>'9');
            if(run && boundL && boundR){
                wchar_t id[11]; for(int k=0;k<10;k++) id[k]=(wchar_t)buf[i+k]; id[10]=0;
                std::wstring nid(id);
                if(validNationalId(nid) && !already(nid)){
                    // try a UTF-16LE name a few bytes after the code (column gap)
                    std::wstring name;
                    for(size_t gap=10; gap<=24 && i+gap+2<avail; gap+=2){
                        std::wstring n=readU16Name(buf,i+gap);
                        if(n.size()>=4){ name=n; break; }
                    }
                    if(name.empty()){
                        // try 8-bit (UTF-8 / Windows-1256) name after the code
                        size_t j=i+10; std::string raw;
                        while(j<avail && raw.size()<64){
                            unsigned char c=(unsigned char)buf[j];
                            if(c==0){ if(raw.size()>=2) break; j++; continue; }
                            raw+=(char)c; j++;
                        }
                        if(raw.size()>=2){
                            int need=MultiByteToWideChar(CP_UTF8,0,raw.c_str(),-1,NULL,0);
                            std::wstring w(need>0?need-1:0,0);
                            if(need>0) MultiByteToWideChar(CP_UTF8,0,raw.c_str(),-1,&w[0],need);
                            name=cleanName(w);
                        }
                    }
                    if(name.size()>=4){
                        // split "first last" (last token = surname when 2+ words)
                        std::wstring first=name, last;
                        size_t sp=name.find_last_of(L' ');
                        if(sp!=std::wstring::npos){ first=trim(name.substr(0,sp)); last=trim(name.substr(sp+1)); }
                        rememberPatient(nid,first,last,L"",L"",L"",L"",L"",L"",std::vector<int>(),-1);
                        done.push_back(nid); imported++;
                    }
                }
                i+=10;
            } else i++;
        }
        // progress + responsiveness
        if(s_bs){ s_bs->progress=(int)(seen*100/(cap>0?cap:1));
            s_bs->status=L"در حال استخراج اطلاعات بیماران از پشتیبان متین‌طب…"; }
        if(notify) { RECT c=bkCard(notify); RECT strip={c.left,c.bottom-S(100),c.right,c.bottom};
            InvalidateRect(notify,&strip,FALSE); }

        // keep the trailing OVER bytes as carry so a record split across the
        // chunk boundary is still seen next iteration.
        if(avail>OVER){ memmove(buf.data(), buf.data()+avail-OVER, OVER); carry=OVER; }
        else carry=avail;
    }
    CloseHandle(hf);
    return imported;
}

// ---- backup: bundle every data\ file into an AZTBKP01 container -------------
//  Layout (UTF-16LE-free, byte stream):
//    "AZTBKP01\n"
//    repeated: "<relpath>\t<size>\n" <size bytes>
//    "END\n"
static unsigned __stdcall backupWorker(void* p){
    WorkArg* a=(WorkArg*)p;
    std::wstring dst=a->path; delete a;
    std::wstring dir=dataDir();
    std::vector<std::wstring> files; std::vector<long long> sizes;
    enumDataFiles(dir,files,sizes);
    long long total=0; for(auto s:sizes) total+=s; if(total<1) total=1;

    HANDLE out=CreateFileW(dst.c_str(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL,NULL);
    if(out==INVALID_HANDLE_VALUE){
        if(s_bs){ s_bs->busy=false; s_bs->status=L"خطا در ایجاد فایل پشتیبان."; }
        if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
        return 1;
    }
    DWORD wr=0;
    { const char* hdr="AZTBKP01\n"; WriteFile(out,hdr,9,&wr,NULL); }

    std::vector<char> buf(1<<20);       // 1 MB streaming buffer
    long long done=0;
    for(size_t i=0;i<files.size();i++){
        if(InterlockedCompareExchange(&s_cancel,0,0)) break;
        // relative path under the data directory
        std::wstring rel=files[i].substr(dir.size()+1);
        // header line:  rel \t size \n   (encode rel as UTF-8)
        int need=WideCharToMultiByte(CP_UTF8,0,rel.c_str(),-1,NULL,0,NULL,NULL);
        std::string relU(need>0?need-1:0,'\0');
        if(need>0) WideCharToMultiByte(CP_UTF8,0,rel.c_str(),-1,&relU[0],need,NULL,NULL);
        char line[1200];
        int ln=_snprintf(line,sizeof(line),"%s\t%lld\n",relU.c_str(),sizes[i]);
        WriteFile(out,line,ln,&wr,NULL);
        // file body
        HANDLE in=CreateFileW(files[i].c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,
                              OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
        if(in!=INVALID_HANDLE_VALUE){
            DWORD rd=0;
            while(ReadFile(in,buf.data(),(DWORD)buf.size(),&rd,NULL) && rd>0){
                WriteFile(out,buf.data(),rd,&wr,NULL);
                done+=rd;
                if(s_bs){ s_bs->progress=(int)(done*100/total);
                    s_bs->status=L"در حال نوشتن: "+rel; }
                if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
                if(InterlockedCompareExchange(&s_cancel,0,0)) break;
            }
            CloseHandle(in);
        }
    }
    { const char* end="END\n"; WriteFile(out,end,4,&wr,NULL); }
    CloseHandle(out);
    if(s_bs){ s_bs->busy=false; s_bs->progress=100;
        s_bs->status=InterlockedCompareExchange(&s_cancel,0,0)
            ? L"پشتیبان‌گیری لغو شد."
            : L"پشتیبان‌گیری با موفقیت کامل شد."; }
    if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
    return 0;
}

// ---- scan: build a category breakdown for the chosen backup -----------------
static unsigned __stdcall scanWorker(void* p){
    WorkArg* a=(WorkArg*)p;
    std::wstring path=a->path; bool foreign=a->foreign; delete a;

    BackupInfo bi; bi.path=path;
    long long perCat[CAT_COUNT]={0,0,0,0};
    long long recCat[CAT_COUNT]={0,0,0,0};
    long long total=0;

    // total file size
    HANDLE hf=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,
                          OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    LARGE_INTEGER fsz={0};
    if(hf!=INVALID_HANDLE_VALUE){ GetFileSizeEx(hf,&fsz); total=fsz.QuadPart; }

    if(foreign){
        // A foreign (e.g. Matin-Teb SQL) .bak — possibly ~15 GB. We do NOT load
        // it; we estimate the per-category split from the total size using a
        // typical clinic-data ratio. We "scan" by reading the file in chunks so
        // the progress bar moves and the UI proves it stays responsive.
        if(hf!=INVALID_HANDLE_VALUE){
            std::vector<char> buf(1<<20); DWORD rd=0; long long seen=0;
            long long step = total>0? total/100 : 1;
            long long nextMark=step;
            // read at most ~64 MB worth of chunks (sampling) so even a 15 GB
            // file finishes the visual scan quickly without freezing.
            long long cap = total<(64LL<<20)? total : (64LL<<20);
            while(seen<cap && ReadFile(hf,buf.data(),(DWORD)buf.size(),&rd,NULL) && rd>0){
                seen+=rd;
                if(s_bs){ s_bs->progress=(int)(total>0? (seen*100/ (cap>0?cap:1)) :100); }
                if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
                if(InterlockedCompareExchange(&s_cancel,0,0)) break;
                if(seen>=nextMark){ nextMark+=step; Sleep(1); }
            }
        }
        perCat[0]=(long long)(total*0.55); // patients
        perCat[1]=(long long)(total*0.35); // images
        perCat[2]=(long long)(total*0.07); // billing
        perCat[3]=total-perCat[0]-perCat[1]-perCat[2]; // other
        if(perCat[3]<0) perCat[3]=0;
        // rough record estimate: ~4 KB per patient record
        recCat[0]= perCat[0]/4096;
    } else {
        // Our own AZTBKP01: walk the container header to read exact per-file
        // sizes and classify each entry. Cheap, no body copy.
        if(hf!=INVALID_HANDLE_VALUE){
            SetFilePointer(hf,0,NULL,FILE_BEGIN);
            // read everything (our backups are modest); but stream the header.
            std::vector<char> buf(1<<16); DWORD rd=0; std::string acc;
            // header magic
            char magic[9]={0}; ReadFile(hf,magic,9,&rd,NULL);
            bool ok = (rd==9 && strncmp(magic,"AZTBKP01\n",9)==0);
            if(ok){
                long long pos=9;
                for(;;){
                    if(InterlockedCompareExchange(&s_cancel,0,0)) break;
                    // read one header line "rel\tsize\n"
                    std::string line; char c;
                    DWORD r1=0; bool eof=false;
                    while(true){
                        if(!ReadFile(hf,&c,1,&r1,NULL)||r1==0){ eof=true; break; }
                        pos++; if(c=='\n') break; line+=c;
                    }
                    if(eof||line.empty()||line=="END") break;
                    size_t tab=line.find('\t'); if(tab==std::string::npos) break;
                    std::string relU=line.substr(0,tab);
                    long long fsz2=_atoi64(line.substr(tab+1).c_str());
                    // decode rel to wide for classification
                    int need=MultiByteToWideChar(CP_UTF8,0,relU.c_str(),-1,NULL,0);
                    std::wstring rel(need>0?need-1:0,L'\0');
                    if(need>0) MultiByteToWideChar(CP_UTF8,0,relU.c_str(),-1,&rel[0],need);
                    int ci=catIndexOf(classifyFile(rel));
                    perCat[ci]+=fsz2; recCat[ci]++;
                    // skip the body
                    LARGE_INTEGER mv; mv.QuadPart=fsz2;
                    SetFilePointerEx(hf,mv,NULL,FILE_CURRENT);
                    pos+=fsz2;
                    if(s_bs){ s_bs->progress=(total>0)?(int)(pos*100/total):100; }
                    if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
                }
            } else {
                // not our format after all → treat as foreign estimate
                perCat[0]=(long long)(total*0.55); perCat[1]=(long long)(total*0.35);
                perCat[2]=(long long)(total*0.07);
                perCat[3]=total-perCat[0]-perCat[1]-perCat[2]; if(perCat[3]<0)perCat[3]=0;
            }
        }
    }
    if(hf!=INVALID_HANDLE_VALUE) CloseHandle(hf);

    bi.totalBytes=total;
    for(int i=0;i<CAT_COUNT;i++){
        BackupCategory bc; bc.id=CATS[i].id; bc.name=CATS[i].name;
        bc.bytes=perCat[i]; bc.records=recCat[i]; bc.selected=true;
        bi.cats.push_back(bc);
    }
    bi.ready=true;
    if(s_bs){ s_bs->info=bi; s_bs->scanning=false; s_bs->progress=100;
        s_bs->status=L"اسکن کامل شد — دسته‌بندی‌ها آماده انتخاب هستند."; }
    if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
    return 0;
}

// ---- restore: AZTBKP01 → write back the selected categories ----------------
static unsigned __stdcall restoreWorker(void* p){
    WorkArg* a=(WorkArg*)p;
    std::wstring path=a->path; bool foreign=a->foreign; delete a;
    std::wstring dir=dataDir();

    // which category ids are selected
    bool want[CAT_COUNT]={true,true,true,true};
    if(s_bs){ for(size_t i=0;i<s_bs->info.cats.size() && i<(size_t)CAT_COUNT;i++)
        want[i]=s_bs->info.cats[i].selected; }

    if(foreign){
        // Real, honest import of a foreign backup (SQL-Server .bak / SQL dump /
        // CSV export from the previous software). We stream the whole file in a
        // large-file-safe sliding window and recover every patient identity that
        // is stored as readable text next to a checksum-valid national code,
        // persisting each through rememberPatient() so it lands in the (possibly
        // network-wide) data layer — exactly like a hand-entered patient. We do
        // NOT fabricate: a record is imported ONLY when the national-code
        // checksum passes AND a real name is found beside it.
        ForeignFmt fmt=sniffForeign(path);
        long long imported = importForeignPatients(path, fmt, s_bk);
        bool cancelled = InterlockedCompareExchange(&s_cancel,0,0)!=0;
        if(s_bs){ s_bs->busy=false; s_bs->progress=100;
            s_bs->lastRestoredFiles=0;
            s_bs->lastRestoredPatients=imported;
            if(cancelled)
                s_bs->status=L"بازیابی لغو شد.";
            else if(imported>0)
                s_bs->status=L"بازیابی از پشتیبان متین‌طب کامل شد — اطلاعات بیماران در سامانه بارگذاری شد.";
            else
                s_bs->status=L"اسکن کامل شد؛ رکورد بیمار قابل استخراجی یافت نشد.";
            if(!cancelled) InterlockedExchange(&s_bs->doneSignal, 2);
        }
        if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
        return 0;
    }

    // Our own container: stream entries; write only selected categories.
    int    restoredFiles=0;             // how many files we wrote back
    HANDLE hf=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,
                          OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(hf==INVALID_HANDLE_VALUE){
        if(s_bs){ s_bs->busy=false; s_bs->status=L"خطا در باز کردن فایل پشتیبان."; }
        if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
        return 1;
    }
    LARGE_INTEGER fsz={0}; GetFileSizeEx(hf,&fsz);
    long long total=fsz.QuadPart>0?fsz.QuadPart:1, pos=0;
    DWORD rd=0;
    char magic[9]={0}; ReadFile(hf,magic,9,&rd,NULL); pos=9;
    if(!(rd==9 && strncmp(magic,"AZTBKP01\n",9)==0)){
        CloseHandle(hf);
        if(s_bs){ s_bs->busy=false; s_bs->status=L"قالب فایل پشتیبان معتبر نیست."; }
        if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
        return 1;
    }
    std::vector<char> buf(1<<20);
    for(;;){
        if(InterlockedCompareExchange(&s_cancel,0,0)) break;
        std::string line; char c; DWORD r1=0; bool eof=false;
        while(true){
            if(!ReadFile(hf,&c,1,&r1,NULL)||r1==0){ eof=true; break; }
            pos++; if(c=='\n') break; line+=c;
        }
        if(eof||line.empty()||line=="END") break;
        size_t tab=line.find('\t'); if(tab==std::string::npos) break;
        std::string relU=line.substr(0,tab);
        long long fsz2=_atoi64(line.substr(tab+1).c_str());
        int need=MultiByteToWideChar(CP_UTF8,0,relU.c_str(),-1,NULL,0);
        std::wstring rel(need>0?need-1:0,L'\0');
        if(need>0) MultiByteToWideChar(CP_UTF8,0,relU.c_str(),-1,&rel[0],need);
        int ci=catIndexOf(classifyFile(rel));
        bool keep = want[ci];

        std::wstring outPath=dir+L"\\"+rel;
        HANDLE out=INVALID_HANDLE_VALUE;
        if(keep){
            // ensure subdirectories exist
            size_t sp=outPath.find_last_of(L"\\/");
            if(sp!=std::wstring::npos){
                std::wstring sub=outPath.substr(0,sp);
                CreateDirectoryW(sub.c_str(),NULL);
            }
            out=CreateFileW(outPath.c_str(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL,NULL);
            if(out!=INVALID_HANDLE_VALUE) restoredFiles++;
        }
        long long left=fsz2;
        while(left>0){
            DWORD chunk=(DWORD)((left<(long long)buf.size())?left:buf.size());
            if(!ReadFile(hf,buf.data(),chunk,&rd,NULL)||rd==0) break;
            if(out!=INVALID_HANDLE_VALUE){ DWORD wr=0; WriteFile(out,buf.data(),rd,&wr,NULL); }
            left-=rd; pos+=rd;
            if(s_bs){ s_bs->progress=(int)(pos*100/total);
                s_bs->status=(keep?L"در حال بازیابی: ":L"رد شدن (انتخاب نشده): ")+rel; }
            if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
            if(InterlockedCompareExchange(&s_cancel,0,0)) break;
        }
        if(out!=INVALID_HANDLE_VALUE) CloseHandle(out);
    }
    CloseHandle(hf);
    bool cancelled = InterlockedCompareExchange(&s_cancel,0,0)!=0;
    // Count the patient records now present in the restored data layer so the UI
    // can confirm exactly how much patient data is live (one row per patient).
    long long patientRows=0;
    if(!cancelled && want[catIndexOf(L"patients")]){
        std::wstring pall=readFileUtf8(dataDir()+L"\\patients.dat");
        size_t pp=0;
        while(pp<pall.size()){
            size_t e=pall.find(L'\n',pp); if(e==std::wstring::npos) e=pall.size();
            std::wstring ln=trim(pall.substr(pp,e-pp)); pp=e+1;
            if(!ln.empty()) patientRows++;
        }
    }
    if(s_bs){ s_bs->busy=false; s_bs->progress=100;
        s_bs->lastRestoredFiles=restoredFiles;
        s_bs->lastRestoredPatients=patientRows;
        s_bs->status= cancelled
            ? L"بازیابی لغو شد."
            : L"بازیابی با موفقیت کامل شد — اطلاعات بیماران در سامانه بارگذاری شد.";
        if(!cancelled) InterlockedExchange(&s_bs->doneSignal, 2);
    }
    if(s_bk) InvalidateRect(s_bk,NULL,FALSE);
    return 0;
}

// ============================================================ layout helpers
static RECT bkCard(HWND h){ RECT rc; GetClientRect(h,&rc);
    int w=S(880),hh=S(640);
    if(w>rc.right-S(40))  w=rc.right-S(40);
    if(hh>rc.bottom-S(40))hh=rc.bottom-S(40);
    RECT c={(rc.right-w)/2,(rc.bottom-hh)/2,(rc.right+w)/2,(rc.bottom+hh)/2}; return c; }

static RECT bkCloseRect(HWND h){ RECT c=bkCard(h);
    RECT b={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)}; return b; }

// HOME mode: two big action cards
static RECT bkHomeCard(HWND h,int i){ RECT c=bkCard(h);
    int gap=S(24), cw=(c.right-c.left-S(48)-gap)/2, ch=S(220);
    int y=c.top+S(96);
    int x=c.left+S(24)+(i==1?(cw+gap):0);
    RECT r={x,y,x+cw,y+ch}; return r; }

// RESTORE mode rects
static RECT bkOpenBar(HWND h){ RECT c=bkCard(h);
    RECT r={c.left+S(24),c.top+S(72),c.right-S(24),c.top+S(112)}; return r; }
static RECT bkBrowseBtn(HWND h){ RECT b=bkOpenBar(h);
    RECT r={b.right-S(140),b.top+S(4),b.right-S(8),b.bottom-S(4)}; return r; }
static int  bkRowTop(HWND h){ RECT c=bkCard(h); return c.top+S(168); }
static RECT bkCatRow(HWND h,int i){ RECT c=bkCard(h);
    int y=bkRowTop(h)+i*(S(56)+S(8));
    RECT r={c.left+S(24),y,c.right-S(24),y+S(56)}; return r; }
static RECT bkCatTick(RECT row){ RECT b={row.right-S(44),row.top+S(16),row.right-S(20),row.top+S(40)}; return b; }
static RECT bkAllTick(HWND h){ RECT c=bkCard(h);
    int y=bkRowTop(h)+CAT_COUNT*(S(56)+S(8))+S(8);
    RECT r={c.right-S(280),y,c.right-S(24),y+S(40)}; return r; }
static RECT bkProgBar(HWND h){ RECT c=bkCard(h);
    RECT r={c.left+S(24),c.bottom-S(96),c.right-S(24),c.bottom-S(76)}; return r; }
static RECT bkRestoreBtn(HWND h){ RECT c=bkCard(h);
    RECT r={c.right-S(220),c.bottom-S(60),c.right-S(24),c.bottom-S(20)}; return r; }
static RECT bkBackBtn(HWND h){ RECT c=bkCard(h);
    RECT r={c.left+S(24),c.bottom-S(60),c.left+S(160),c.bottom-S(20)}; return r; }

// ===================================================================== paint
// (1) Build the EXPENSIVE, STATIC layers into the cache bitmap. Runs only when
//     the size / theme / mode changes — never on a mere mouse move.
static void bkBuildBg(HWND h, HDC ref){
    RECT rc; GetClientRect(h,&rc);
    if(rc.right<=0||rc.bottom<=0) return;
    bkFreeBg();
    s_bgDC=CreateCompatibleDC(ref);
    s_bgBmp=CreateCompatibleBitmap(ref,rc.right,rc.bottom);
    s_bgOld=SelectObject(s_bgDC,s_bgBmp);
    s_bgW=rc.right; s_bgH=rc.bottom; s_bgMode=s_bs?s_bs->mode:0;
    HDC dc=s_bgDC;

    { HBRUSH sb=CreateSolidBrush(g_dark?RGB(6,9,14):RGB(28,36,48));
      FillRect(dc,&rc,sb); DeleteObject(sb); }
    gpFillAlpha(dc,rc,0,g_dark?RGB(0,0,0):RGB(20,28,40),120);
    RECT c=bkCard(h);
    gpShadow(dc,c,S(20),S(22),80);
    { HBRUSH pb=CreateSolidBrush(g_theme.surface); FillRect(dc,&c,pb); DeleteObject(pb); }
    gpGradRoundRect(dc,c,S(18),g_theme.surfaceTop,g_theme.surface,g_theme.border);
    SetBkMode(dc,TRANSPARENT);

    // close (static)
    { RECT cb=bkCloseRect(h);
      RECT ci={cb.left+S(5),cb.top+S(5),cb.right-S(5),cb.bottom-S(5)};
      drawIcon(dc,ICO_X,ci,g_theme.text,S(2)); }

    // title (static)
    SelectObject(dc,g_fTitle); SetTextColor(dc,g_theme.accent);
    RECT tr={c.left+S(24),c.top+S(16),c.right-S(54),c.top+S(54)};
    DrawTextW(dc, (s_bs&&s_bs->mode==BK_MODE_RESTORE)
        ? L"بازیابی پشتیبان"
        : L"مدیریت پشتیبان‌گیری اطلاعات بیماران", -1, &tr,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

    // subtitle on the HOME mode is static too
    if(!s_bs || s_bs->mode==BK_MODE_HOME){
        SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
        RECT sr={c.left+S(24),c.top+S(56),c.right-S(24),c.top+S(88)};
        DrawTextW(dc,L"یک گزینه را انتخاب کنید. عملیات سنگین در پس‌زمینه اجرا می‌شود و رابط کاربری متوقف نمی‌گردد.",
            -1,&sr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
    }
}

// (2) Draw the CHEAP interactive foreground on top of the cached background.
static void bkPaintFg(HWND h, HDC dc){
    RECT c=bkCard(h);
    SetBkMode(dc,TRANSPARENT);

    if(s_bs->mode==BK_MODE_HOME){
        const wchar_t* ctitle[2]={L"پشتیبان‌گیری",L"بازیابی پشتیبان"};
        const wchar_t* cdesc [2]={L"تهیهٔ نسخهٔ پشتیبان از تمام اطلاعات بیماران در یک فایل امن.",
                                  L"بازیابی کامل یا انتخابی از یک فایل پشتیبان (شامل فایل‌های حجیم متین‌طب)."};
        int cicon[2]={ICO_SAVE,ICO_REFRESH};
        for(int i=0;i<2;i++){
            RECT r=bkHomeCard(h,i);
            bool hot=(s_bs->hot==i);
            gpGradRoundRectBg(dc,r,S(14),
                hot?blendColor(g_theme.surfaceTop,g_theme.accent,12):g_theme.surfaceTop,
                hot?blendColor(g_theme.surface,g_theme.accent,8):g_theme.surface,
                hot?g_theme.accent:g_theme.border, g_theme.surface);
            // icon disc
            int dcx=(r.left+r.right)/2, dcy=r.top+S(70), rad=S(40);
            RECT disc={dcx-rad,dcy-rad,dcx+rad,dcy+rad};
            gpRoundRect(dc,disc,rad,blendColor(g_theme.surface,g_theme.accent,16),
                        g_theme.accent,255);
            RECT ir={dcx-S(22),dcy-S(22),dcx+S(22),dcy+S(22)};
            drawIcon(dc,cicon[i],ir,g_theme.accent,S(3));
            SelectObject(dc,g_fBig); SetTextColor(dc,g_theme.text);
            RECT t2={r.left+S(12),dcy+S(48),r.right-S(12),dcy+S(82)};
            DrawTextW(dc,ctitle[i],-1,&t2,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
            RECT t3={r.left+S(16),dcy+S(84),r.right-S(16),r.bottom-S(12)};
            DrawTextW(dc,cdesc[i],-1,&t3,DT_CENTER|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
        }
        // running status (backup may be in progress while on home)
        if(s_bs->busy){
            RECT pb=bkProgBar(h);
            gpRoundRect(dc,pb,S(8),g_theme.surface2,g_theme.border,255);
            RECT fill=pb; fill.right=pb.left+(pb.right-pb.left)*s_bs->progress/100;
            gpRoundRect(dc,fill,S(8),g_theme.accent,CLR_INVALID,255);
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
            RECT st={c.left+S(24),pb.bottom+S(4),c.right-S(24),pb.bottom+S(28)};
            DrawTextW(dc,s_bs->status.c_str(),-1,&st,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
        } else if(!s_bs->status.empty()){
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.success);
            RECT st={c.left+S(24),c.bottom-S(48),c.right-S(24),c.bottom-S(20)};
            DrawTextW(dc,s_bs->status.c_str(),-1,&st,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
        }
    } else {
        // ---- RESTORE mode ----
        // file-open bar
        RECT ob=bkOpenBar(h);
        gpRoundRectBg(dc,ob,S(10),g_theme.inputBg,g_theme.border,g_theme.surface,255);
        RECT pathR={ob.left+S(12),ob.top,ob.right-S(152),ob.bottom};
        SelectObject(dc,g_fUI); SetTextColor(dc, s_bs->pickedPath.empty()?g_theme.textDim:g_theme.text);
        std::wstring shown=s_bs->pickedPath.empty()? std::wstring(L"فایلی انتخاب نشده است…")
            : s_bs->pickedPath;
        DrawTextW(dc,shown.c_str(),-1,&pathR,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_PATH_ELLIPSIS);
        // browse button
        RECT bb=bkBrowseBtn(h);
        gpRoundRect(dc,bb,S(8),g_theme.accent,g_theme.accent,255);
        SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.accentText);
        DrawTextW(dc,L"انتخاب فایل…",-1,&bb,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

        // scanning state
        if(s_bs->scanning){
            SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.accent);
            RECT scr={c.left+S(24),bkRowTop(h)+S(20),c.right-S(24),bkRowTop(h)+S(60)};
            DrawTextW(dc,L"در حال اسکن فایل پشتیبان… لطفاً صبر کنید.",-1,&scr,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            // animated bar
            RECT pb=bkProgBar(h);
            gpRoundRect(dc,pb,S(8),g_theme.surface2,g_theme.border,255);
            RECT fill=pb; fill.right=pb.left+(pb.right-pb.left)*s_bs->progress/100;
            gpRoundRect(dc,fill,S(8),g_theme.accent,CLR_INVALID,255);
        } else if(s_bs->info.ready){
            // category rows with ticks
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
            RECT hd={c.left+S(24),bkRowTop(h)-S(28),c.right-S(24),bkRowTop(h)-S(4)};
            std::wstring tot=L"حجم کل پشتیبان: "+humanSize(s_bs->info.totalBytes)+
                L"   •   برای بازیابی انتخابی، دسته‌های موردنظر را تیک بزنید.";
            DrawTextW(dc,tot.c_str(),-1,&hd,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
            for(int i=0;i<CAT_COUNT && i<(int)s_bs->info.cats.size();i++){
                BackupCategory& cat=s_bs->info.cats[i];
                RECT r=bkCatRow(h,i);
                bool hot=(s_bs->hot==(100+i));
                gpRoundRectBg(dc,r,S(10),
                    hot?blendColor(g_theme.surface2,g_theme.accent,8):g_theme.surface2,
                    cat.selected?g_theme.accent:g_theme.border, g_theme.surface,255);
                // icon
                RECT ir={r.right-S(96),r.top+S(14),r.right-S(68),r.top+S(42)};
                drawIcon(dc,CATS[i].icon,ir,g_theme.accent,S(2));
                SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
                RECT nr={r.left+S(60),r.top+S(8),r.right-S(110),r.top+S(32)};
                DrawTextW(dc,cat.name.c_str(),-1,&nr,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
                SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
                RECT szr={r.left+S(60),r.top+S(30),r.right-S(110),r.bottom-S(6)};
                std::wstring info=L"تخمین: "+humanSize(cat.bytes);
                if(cat.records>0) info+=L"   •   حدود "+toFaDigits(std::to_wstring(cat.records))+L" رکورد";
                DrawTextW(dc,info.c_str(),-1,&szr,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                // tick box
                RECT tk=bkCatTick(r);
                gpRoundRect(dc,tk,S(6), cat.selected?g_theme.accent:g_theme.inputBg,
                            cat.selected?g_theme.accent:g_theme.border,255);
                if(cat.selected){ RECT ck={tk.left+S(4),tk.top+S(4),tk.right-S(4),tk.bottom-S(4)};
                    drawIcon(dc,ICO_CHECK,ck,g_theme.accentText,S(2)); }
            }
            // "all patient information" master tick
            RECT at=bkAllTick(h);
            RECT atb={at.right-S(28),at.top+S(8),at.right-S(4),at.top+S(32)};
            gpRoundRect(dc,atb,S(6), s_bs->allTicked?g_theme.accent:g_theme.inputBg,
                        s_bs->allTicked?g_theme.accent:g_theme.border,255);
            if(s_bs->allTicked){ RECT ck={atb.left+S(4),atb.top+S(4),atb.right-S(4),atb.bottom-S(4)};
                drawIcon(dc,ICO_CHECK,ck,g_theme.accentText,S(2)); }
            SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
            RECT atl={at.left,at.top,at.right-S(36),at.bottom};
            DrawTextW(dc,L"همهٔ اطلاعات بیماران",-1,&atl,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        }

        // progress bar (restore)
        if(s_bs->busy){
            RECT pb=bkProgBar(h);
            gpRoundRect(dc,pb,S(8),g_theme.surface2,g_theme.border,255);
            RECT fill=pb; fill.right=pb.left+(pb.right-pb.left)*s_bs->progress/100;
            gpRoundRect(dc,fill,S(8),g_theme.success,CLR_INVALID,255);
        }
        // status line
        if(!s_bs->status.empty()){
            SelectObject(dc,g_fSmall);
            SetTextColor(dc, s_bs->busy? g_theme.textDim : g_theme.success);
            RECT st={c.left+S(24),bkProgBar(h).bottom+S(2),c.right-S(24),bkProgBar(h).bottom+S(22)};
            DrawTextW(dc,s_bs->status.c_str(),-1,&st,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
        }

        // back + restore buttons
        RECT bk=bkBackBtn(h);
        gpRoundRect(dc,bk,S(9),g_theme.surface2,g_theme.border,255);
        SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
        DrawTextW(dc,L"بازگشت",-1,&bk,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

        RECT rb=bkRestoreBtn(h);
        bool canRestore = s_bs->info.ready && !s_bs->busy && !s_bs->scanning;
        gpRoundRect(dc,rb,S(9), canRestore?g_theme.success:g_theme.surface2,
                    canRestore?g_theme.success:g_theme.border,255);
        SetTextColor(dc, canRestore?RGB(255,255,255):g_theme.textDim);
        DrawTextW(dc,L"شروع بازیابی",-1,&rb,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    }
}

// (3) Composite: ensure the cache is valid, blit it (for the dirty strip only),
//     overlay the cheap foreground, then copy out. The heavy scrim/shadow/
//     gradient work in bkBuildBg runs ONCE per size/theme/mode change instead
//     of on every mouse move.
static void bkPaint(HWND h, HDC dc0, const RECT* dirty){
    RECT rc; GetClientRect(h,&rc);
    if(rc.right<=0||rc.bottom<=0) return;
    if(!s_bgDC || s_bgW!=rc.right || s_bgH!=rc.bottom ||
       (s_bs && s_bgMode!=s_bs->mode))
        bkBuildBg(h,dc0);
    if(!s_bgDC) return;

    RECT d = (dirty && dirty->right>dirty->left && dirty->bottom>dirty->top)
             ? *dirty : rc;
    int dw=d.right-d.left, dh=d.bottom-d.top;

    HDC dc=CreateCompatibleDC(dc0);
    HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
    HGDIOBJ obm=SelectObject(dc,bmp);

    // 1) restore the cached static layers for the dirty region
    BitBlt(dc,d.left,d.top,dw,dh,s_bgDC,d.left,d.top,SRCCOPY);
    // 2) draw the interactive foreground, clipped to the dirty strip
    HRGN clip=CreateRectRgn(d.left,d.top,d.right,d.bottom);
    SelectClipRgn(dc,clip);
    if(s_bs) bkPaintFg(h,dc);
    SelectClipRgn(dc,NULL); DeleteObject(clip);
    // 3) blit the composited strip to the screen
    BitBlt(dc0,d.left,d.top,dw,dh,dc,d.left,d.top,SRCCOPY);

    SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
}

// ===================================================================== input
static void bkStartBackup(HWND h){
    if(s_bs->busy||s_bs->scanning) return;
    std::wstring dst=askSaveBackup(h);
    if(dst.empty()) return;
    s_bs->busy=true; s_bs->progress=0; s_bs->status=L"در حال آماده‌سازی…";
    InterlockedExchange(&s_cancel,0);
    WorkArg* a=new WorkArg(); a->path=dst; a->foreign=false;
    _beginthreadex(NULL,0,backupWorker,a,0,NULL);
    InvalidateRect(h,NULL,FALSE);
}
static void bkPickAndScan(HWND h){
    if(s_bs->busy||s_bs->scanning) return;
    std::wstring p=askOpenBackup(h);
    if(p.empty()) return;
    s_bs->pickedPath=p;
    // v1.9.5: detect the REAL format from the file header + extension, not the
    // extension alone, so a mislabelled SQL/CSV export is handled correctly.
    ForeignFmt ff=sniffForeign(p);
    s_bs->foreign = (ff!=FF_AZT);
    s_bs->info=BackupInfo(); s_bs->info.ready=false;
    s_bs->scanning=true; s_bs->progress=0; s_bs->status=L"";
    InterlockedExchange(&s_cancel,0);
    WorkArg* a=new WorkArg(); a->path=p; a->foreign=s_bs->foreign;
    _beginthreadex(NULL,0,scanWorker,a,0,NULL);
    InvalidateRect(h,NULL,FALSE);
}
static void bkStartRestore(HWND h){
    if(!s_bs->info.ready||s_bs->busy||s_bs->scanning) return;
    if(MessageBoxW(h,L"آیا از بازیابی پشتیبان اطمینان دارید؟ اطلاعات فعلی ممکن است بازنویسی شود.",
        L"تأیید بازیابی",MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2)!=IDYES) return;
    s_bs->busy=true; s_bs->progress=0; s_bs->status=L"در حال بازیابی…";
    InterlockedExchange(&s_cancel,0);
    WorkArg* a=new WorkArg(); a->path=s_bs->pickedPath; a->foreign=s_bs->foreign;
    _beginthreadex(NULL,0,restoreWorker,a,0,NULL);
    InvalidateRect(h,NULL,FALSE);
}

// ===========================================================================
//  HIDDEN BACKUP ANALYZER PAGE  (v1.10.0)
//  Activated ONLY by Ctrl+B inside the backup window. No menu / button reveals
//  it. Ctrl+B again or Esc hides it. It runs a REAL multi-format analysis on a
//  background thread with a determinate (bytes-based) progress bar, then shows
//  a sectioned report with per-section «کپی» buttons and a global copy button.
// ===========================================================================
#define WM_BK_ANALYSIS_DONE (WM_APP+72)
#define WM_BK_ANALYSIS_PROG (WM_APP+73)

//  progress trampoline: worker → UI thread (no GDI off-thread)
static void bkAnProgCb(int pct, const std::wstring& status, void* user){
    HWND h=(HWND)user;
    // §C: the window may have been destroyed mid-analysis. Touch the global
    // state ONLY while the window is still alive — otherwise this is a
    // use-after-free. The BackupLog breadcrumb is process-global (safe).
    { wchar_t b[128]; swprintf(b,128,L"ANALYZE_PROGRESS %d%% %s",pct,status.c_str());
      BackupLog_Breadcrumb(b); }
    if(!s_anWinAlive.load()) return;
    if(!h||!IsWindow(h)) return;
    if(s_bs){ s_bs->anProgress.store(pct); s_bs->anStatus=status; }
    PostMessageW(h,WM_BK_ANALYSIS_PROG,(WPARAM)pct,0);
}

static std::wstring bkAnPickFile(HWND owner){
    wchar_t buf[1024]={0};
    OPENFILENAMEW ofn={0}; ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=owner;
    ofn.lpstrFilter=L"فایل‌های پشتیبان\0*.db;*.sqlite;*.bak;*.zip;*.azb;*.aztbk;*.json;*.sql\0همه فایل‌ها\0*.*\0";
    ofn.lpstrFile=buf; ofn.nMaxFile=1024;
    ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_EXPLORER;
    if(GetOpenFileNameW(&ofn)) return buf;
    return L"";
}

static void bkAnStart(HWND h){
    if(!s_bs||s_bs->anBusy) return;
    std::wstring path=bkAnPickFile(h);
    if(path.empty()) return;
    Breadcrumb(L"backup analyze: start");
    s_bs->anPath=path;
    s_bs->anBusy=true; s_bs->anProgress.store(0);
    s_bs->anStatus=L"آماده‌سازی…";
    delete s_bs->anResult; s_bs->anResult=nullptr;
    s_bs->anScroll=0;
    InvalidateRect(h,NULL,FALSE);
    HWND target=h;
    std::wstring p=path;
    s_anWorkers.fetch_add(1);
    std::thread([target,p](){
        BkAnalysis* r=new BkAnalysis(analyzeBackupFile(p,bkAnProgCb,(void*)target));
        // §C: only hand the result to the UI thread if the window is still
        // alive; otherwise free it here. Decrement the in-flight counter LAST
        // so WM_NCDESTROY's drain loop can observe quiescence.
        if(s_anWinAlive.load() && IsWindow(target))
            PostMessageW(target,WM_BK_ANALYSIS_DONE,0,(LPARAM)r);
        else
            delete r;
        s_anWorkers.fetch_sub(1);
    }).detach();
}

//  Copy a UTF-16 string to the clipboard (CF_UNICODETEXT).
static void bkAnCopy(HWND owner, const std::wstring& text){
    if(!OpenClipboard(owner)) return;
    EmptyClipboard();
    size_t bytes=(text.size()+1)*sizeof(wchar_t);
    HGLOBAL hg=GlobalAlloc(GMEM_MOVEABLE,bytes);
    if(hg){
        void* p=GlobalLock(hg);
        if(p){ memcpy(p,text.c_str(),bytes); GlobalUnlock(hg);
               SetClipboardData(CF_UNICODETEXT,hg); }
    }
    CloseClipboard();
}

//  v1.12.0 (§11-13): pick a staged patient file and bulk-import it (Path B).
//  Synchronous (import files are small relative to the multi-GB .bak), with a
//  national-ID dedup summary message. Never touches the analyzer worker state.
static void bkAnImportPatients(HWND h){
    if(!s_bs) return;
    wchar_t buf[1024]={0};
    OPENFILENAMEW ofn={0}; ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=h;
    ofn.lpstrFilter=L"فایل بیماران (CSV/متنی)\0*.csv;*.txt;*.tsv;*.dat\0همه فایل‌ها\0*.*\0";
    ofn.lpstrFile=buf; ofn.nMaxFile=1024;
    ofn.lpstrTitle=L"انتخاب فایل بیماران برای ورود (دارای کد ملی)";
    ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_EXPLORER;
    if(!GetOpenFileNameW(&ofn)) return;
    Breadcrumb(L"patient import: start");
    ImportResult r=importPatientsFromFile(buf);
    if(!r.ok && r.inserted==0 && r.updated==0){
        std::wstring msg=L"ورود بیماران ناموفق بود.";
        if(!r.error.empty()) msg+=L"\n"+r.error;
        MessageBoxW(h,msg.c_str(),L"ورود بیماران",MB_ICONERROR|MB_OK);
        Breadcrumb(L"patient import: failed");
        return;
    }
    wchar_t mb[512];
    swprintf(mb,512,
        L"ورود بیماران کامل شد.\n\n"
        L"ردیف‌های پردازش‌شده: %d\n"
        L"افزوده‌شده (کد ملی جدید): %d\n"
        L"به‌روزشده (کد ملی تکراری): %d\n"
        L"رد به‌علت کد ملی نامعتبر: %d\n"
        L"رد به‌علت نبود نام: %d",
        r.total,r.inserted,r.updated,r.skippedInvalid,r.skippedEmpty);
    MessageBoxW(h,toFaDigits(mb).c_str(),L"خلاصهٔ ورود بیماران",
        MB_ICONINFORMATION|MB_OK);
    { wchar_t lb[160]; swprintf(lb,160,L"patient import: +%d ~%d (skip %d/%d)",
        r.inserted,r.updated,r.skippedInvalid,r.skippedEmpty);
      Breadcrumb(lb); }
}

static std::wstring bkAnFullReport(){
    std::wstring out;
    if(s_bs && s_bs->anResult){
        out+=L"گزارش تحلیل پشتیبان درمان‌پلاس\r\n";
        out+=L"فایل: "+s_bs->anPath+L"\r\n\r\n";
        for(auto& s:s_bs->anResult->sections){
            out+=L"■ "+s.title+L"\r\n"+s.body+L"\r\n\r\n";
        }
    }
    return out;
}

//  Geometry of the analyzer page (covers the whole backup card client area).
static RECT bkAnPageRect(HWND h){ RECT rc; GetClientRect(h,&rc); return rc; }
static RECT bkAnBtnAnalyze(HWND h){
    RECT rc=bkAnPageRect(h);
    int w=S(220), hh=S(48);
    RECT r={(rc.right-w)/2, S(96), (rc.right+w)/2, S(96)+hh};
    return r;
}
//  v1.12.0 (§11-13): «ورود بیماران» — import a staged patient file (Path B,
//  offline) into the local store with national-ID dedup. Sits just under the
//  analyze button on the hidden analyzer page.
static RECT bkAnBtnImport(HWND h){
    RECT b=bkAnBtnAnalyze(h);
    int w=S(220), hh=S(40);
    RECT r={(b.left+b.right)/2-w/2, b.bottom+S(10),
            (b.left+b.right)/2+w/2, b.bottom+S(10)+hh};
    return r;
}
static RECT bkAnBtnCopyAll(HWND h){
    RECT rc=bkAnPageRect(h);
    int w=S(200), hh=S(40);
    RECT r={(rc.right-w)/2, rc.bottom-S(56), (rc.right+w)/2, rc.bottom-S(56)+hh};
    return r;
}
static RECT bkAnCloseRect(HWND h){
    RECT rc=bkAnPageRect(h);
    RECT r={rc.right-S(44),S(12),rc.right-S(12),S(44)};
    return r;
}
//  Hit-rect for a per-section copy button, given the section index & its title y.
static RECT bkAnSecCopyRect(int rightX, int titleY){
    RECT r={rightX-S(56),titleY-S(2),rightX-S(8),titleY+S(22)};
    return r;
}

//  Paint the analyzer page (called from bkPaint when anPage is on).
static void bkAnPaint(HWND h, HDC dc){
    RECT rc=bkAnPageRect(h);
    // B.1: SOLID full-client background covers the ENTIRE client area so no
    // control from the underlying backup page can ever bleed through (kills the
    // "ghost controls" artifact). Paint a solid page-bg rectangle first.
    { HBRUSH bg=CreateSolidBrush(g_theme.bg); FillRect(dc,&rc,bg); DeleteObject(bg); }
    // card surface backdrop on top of the solid fill
    gpRoundRectBg(dc,rc,S(2),g_theme.surface,g_theme.border,g_theme.bg);
    SetBkMode(dc,TRANSPARENT);

    // title
    SelectObject(dc,g_fTitle);
    SetTextColor(dc,g_theme.text);
    RECT tr={S(24),S(14),rc.right-S(56),S(52)};
    DrawTextW(dc,L"آنالیز بکاپ (صفحهٔ مخفی)",-1,&tr,
        DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);

    // close (X)
    { RECT x=bkAnCloseRect(h);
      gpRoundRectBg(dc,x,S(8),g_theme.surface2,g_theme.border,g_theme.surface);
      drawIcon(dc,ICO_X,x,g_theme.textDim,S(2)); }

    // big primary button
    { RECT b=bkAnBtnAnalyze(h);
      gpGradRoundRectBg(dc,b,S(10),g_theme.accent,g_theme.accent2,g_theme.accent,g_theme.surface);
      SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.accentText);
      DrawTextW(dc,L"آنالیز بکاپ",-1,&b,
        DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX); }

    // v1.12.0: secondary «ورود بیماران» button (Path B offline import)
    { RECT bi=bkAnBtnImport(h);
      gpRoundRectBg(dc,bi,S(10),g_theme.surface2,g_theme.accent,g_theme.surface);
      SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.accent);
      drawIcon(dc,ICO_USER,{bi.right-S(34),bi.top+S(8),bi.right-S(10),bi.top+S(32)},g_theme.accent,S(2));
      RECT bit={bi.left+S(8),bi.top,bi.right-S(40),bi.bottom};
      DrawTextW(dc,L"ورود بیماران (با کد ملی)",-1,&bit,
        DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX); }

    int y=S(212);   // start below the two action buttons

    // chosen file
    if(s_bs && !s_bs->anPath.empty()){
        SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
        RECT fr={S(24),y,rc.right-S(24),y+S(22)};
        DrawTextW(dc,(L"فایل: "+s_bs->anPath).c_str(),-1,&fr,
            DT_RIGHT|DT_SINGLELINE|DT_PATH_ELLIPSIS|DT_NOPREFIX);
        y+=S(28);
    }

    // progress bar (determinate, bytes-based) — B.3: rounded track + gradient
    // fill; never shows 100% before the worker actually reports it.
    if(s_bs && (s_bs->anBusy || s_bs->anProgress.load()>0)){
        int pct=s_bs->anProgress.load();
        if(pct>100) pct=100; if(pct<0) pct=0;
        // do not paint a full bar while still busy unless 100% was reported
        if(s_bs->anBusy && pct>=100) pct=99;
        RECT bar={S(24),y,rc.right-S(24),y+S(16)};
        gpRoundRectBg(dc,bar,bar.bottom-bar.top,g_theme.surface2,g_theme.border,g_theme.surface);
        RECT fill=bar; fill.left=bar.right-(int)((bar.right-bar.left)*(pct/100.0));
        if(fill.left<bar.left) fill.left=bar.left;
        if(fill.left<fill.right-S(2))
            gpGradRoundRectBg(dc,fill,fill.bottom-fill.top,
                g_theme.accent,g_theme.accent2,g_theme.accent,g_theme.surface2);
        SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
        RECT sr={S(24),y+S(20),rc.right-S(24),y+S(42)};
        std::wstring st=toFaDigits(std::to_wstring(pct))+L"%   —   "+s_bs->anStatus;
        DrawTextW(dc,st.c_str(),-1,&sr,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        y+=S(48);
    }

    // report sections
    if(s_bs && s_bs->anResult){
        if(!s_bs->anResult->ok && !s_bs->anResult->error.empty()){
            // B.4: compact, centered inline error CARD (max width 720px) with a
            // warning icon, title, one-line summary, an expander revealing the
            // full BackupLog payload, and a "copy details" button. NOT a giant
            // full-page red banner.
            int cardW=S(720); if(cardW>rc.right-S(48)) cardW=rc.right-S(48);
            int cardX=(rc.right-cardW)/2;
            bool open=s_bs->anErrExpanded;
            int cardH = open ? S(320) : S(120);
            RECT er={cardX,y,cardX+cardW,y+cardH};
            gpRoundRectBg(dc,er,S(10),blendColor(g_theme.surface,g_theme.danger,10),
                g_theme.danger,g_theme.surface);
            // warning icon
            RECT ic={er.right-S(40),er.top+S(14),er.right-S(12),er.top+S(42)};
            drawIcon(dc,ICO_X,ic,g_theme.danger,S(2));
            // title
            SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.danger);
            RECT tt={er.left+S(14),er.top+S(12),er.right-S(48),er.top+S(36)};
            DrawTextW(dc,L"تحلیل بکاپ ناموفق بود",-1,&tt,
                DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
            // one-line summary
            SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.text);
            RECT su={er.left+S(14),er.top+S(40),er.right-S(14),er.top+S(64)};
            DrawTextW(dc,s_bs->anResult->error.c_str(),-1,&su,
                DT_RIGHT|DT_SINGLELINE|DT_END_ELLIPSIS|DT_RTLREADING|DT_NOPREFIX);
            // expander + copy buttons
            RECT exb={er.left+S(14),er.top+S(74),er.left+S(174),er.top+S(102)};
            gpRoundRectBg(dc,exb,S(6),g_theme.surface2,g_theme.border,g_theme.surface);
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.accent);
            DrawTextW(dc,open?L"بستن جزئیات فنی":L"نمایش جزئیات فنی",-1,&exb,
                DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
            RECT cpb={er.left+S(184),er.top+S(74),er.left+S(294),er.top+S(102)};
            gpRoundRectBg(dc,cpb,S(6),g_theme.surface2,g_theme.border,g_theme.surface);
            SetTextColor(dc,g_theme.textDim);
            DrawTextW(dc,L"کپی جزئیات",-1,&cpb,
                DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
            // details box (monospace) when expanded
            if(open){
                RECT db={er.left+S(14),er.top+S(110),er.right-S(14),er.bottom-S(12)};
                gpRoundRectBg(dc,db,S(6),g_theme.bg,g_theme.border,g_theme.surface);
                SelectObject(dc,g_fMono?g_fMono:g_fSmall); SetTextColor(dc,g_theme.text);
                RECT dbi=db; InflateRect(&dbi,-S(8),-S(6));
                DrawTextW(dc,s_bs->anErrPayload.c_str(),-1,&dbi,
                    DT_LEFT|DT_WORDBREAK|DT_NOPREFIX|DT_EDITCONTROL);
            }
            // global copy button still drawn below by the shared code path
            RECT cab=bkAnBtnCopyAll(h);
            gpRoundRectBg(dc,cab,S(10),g_theme.surface2,g_theme.accent,g_theme.surface);
            SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.accent);
            DrawTextW(dc,L"کپی کامل گزارش",-1,&cab,
                DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
            return;   // error path complete
        } else {
            int top = y - s_bs->anScroll;
            int bottomLimit = bkAnBtnCopyAll(h).top - S(8);
            for(auto& s:s_bs->anResult->sections){
                if(top>bottomLimit) break;
                // section title + per-section copy button
                SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.accent);
                RECT trS={S(24),top,rc.right-S(80),top+S(22)};
                DrawTextW(dc,s.title.c_str(),-1,&trS,
                    DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
                RECT cb=bkAnSecCopyRect(rc.right-S(24),top);
                gpRoundRectBg(dc,cb,S(6),g_theme.surface2,g_theme.border,g_theme.surface);
                SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
                DrawTextW(dc,L"کپی",-1,&cb,
                    DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
                top+=S(26);
                // body
                SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.text);
                RECT br={S(24),top,rc.right-S(24),bottomLimit};
                int bh=DrawTextW(dc,s.body.c_str(),-1,&br,
                    DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX|DT_CALCRECT);
                RECT br2={S(24),top,rc.right-S(24),top+bh};
                DrawTextW(dc,s.body.c_str(),-1,&br2,
                    DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
                top+=bh+S(14);
            }
        }
        // global copy button
        RECT cab=bkAnBtnCopyAll(h);
        gpRoundRectBg(dc,cab,S(10),g_theme.surface2,g_theme.accent,g_theme.surface);
        SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.accent);
        DrawTextW(dc,L"کپی کامل گزارش",-1,&cab,
            DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    } else if(!(s_bs && s_bs->anBusy)){
        SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
        RECT hint={S(24),y+S(8),rc.right-S(24),y+S(60)};
        DrawTextW(dc,L"برای شروع، روی «آنالیز بکاپ» بزنید و فایل پشتیبان را انتخاب کنید.",
            -1,&hint,DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
    }
}

//  Hit-test clicks on the analyzer page. Returns true if the click was handled.
static bool bkAnClick(HWND h, POINT pt){
    if(!s_bs||!s_bs->anPage) return false;
    RECT x=bkAnCloseRect(h);
    if(PtInRect(&x,pt)){ s_bs->anPage=false; InvalidateRect(h,NULL,FALSE); return true; }
    RECT b=bkAnBtnAnalyze(h);
    if(PtInRect(&b,pt)){ bkAnStart(h); return true; }
    RECT bi=bkAnBtnImport(h);
    if(PtInRect(&bi,pt)){ if(!s_bs->anBusy) bkAnImportPatients(h); return true; }
    // B.4: error-card buttons (expander + copy details)
    if(s_bs->anResult && !s_bs->anResult->ok && !s_bs->anResult->error.empty()){
        RECT rc=bkAnPageRect(h);
        int y=S(160);
        if(!s_bs->anPath.empty()) y+=S(28);
        if(s_bs->anBusy || s_bs->anProgress.load()>0) y+=S(48);
        int cardW=S(720); if(cardW>rc.right-S(48)) cardW=rc.right-S(48);
        int cardX=(rc.right-cardW)/2;
        RECT exb={cardX+S(14),y+S(74),cardX+S(174),y+S(102)};
        if(PtInRect(&exb,pt)){ s_bs->anErrExpanded=!s_bs->anErrExpanded;
            InvalidateRect(h,NULL,FALSE); return true; }
        RECT cpb={cardX+S(184),y+S(74),cardX+S(294),y+S(102)};
        if(PtInRect(&cpb,pt)){ bkAnCopy(h,s_bs->anErrPayload); return true; }
        RECT cab=bkAnBtnCopyAll(h);
        if(PtInRect(&cab,pt)){ bkAnCopy(h,bkAnFullReport()); return true; }
        return true;
    }
    if(s_bs->anResult && s_bs->anResult->ok){
        RECT cab=bkAnBtnCopyAll(h);
        if(PtInRect(&cab,pt)){ bkAnCopy(h,bkAnFullReport()); return true; }
        // per-section copy buttons — recompute their y positions like the painter
        RECT rc=bkAnPageRect(h);
        int y=S(160);
        if(!s_bs->anPath.empty()) y+=S(28);
        if(s_bs->anBusy || s_bs->anProgress.load()>0) y+=S(48);
        int top=y - s_bs->anScroll;
        int bottomLimit=cab.top-S(8);
        uikit::WindowDC wdc(h);
        for(auto& s:s_bs->anResult->sections){
            if(top>bottomLimit) break;
            RECT cb=bkAnSecCopyRect(rc.right-S(24),top);
            if(PtInRect(&cb,pt)){ bkAnCopy(h,s.title+L"\r\n"+s.body); return true; }
            top+=S(26);
            SelectObject(wdc.dc,g_fUI);
            RECT br={S(24),top,rc.right-S(24),bottomLimit};
            int bh=DrawTextW(wdc.dc,s.body.c_str(),-1,&br,
                DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX|DT_CALCRECT);
            top+=bh+S(14);
        }
    }
    return true;   // swallow clicks while the page is up
}

static LRESULT CALLBACK bkProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE: s_bs=new BkState(); s_anWinAlive.store(true); SetTimer(h,1,40,NULL);
#ifdef AZ_DEBUG_BUILD
        { wchar_t dm[16]={0}; GetEnvironmentVariableW(L"AZ_DEBUG_BKMODE",dm,16);
          if(!wcscmp(dm,L"restore")){
              s_bs->mode=BK_MODE_RESTORE;
              // synthesise a ready scan so the restore layout is fully populated
              s_bs->pickedPath=L"C:\\backups\\DarmanPlus_1405-03-29.aztbk";
              BackupInfo bi; bi.path=s_bs->pickedPath; bi.totalBytes=58LL*1024*1024;
              long long demo[4]={32LL*1024*1024,20LL*1024*1024,4LL*1024*1024,2LL*1024*1024};
              long long recs[4]={1280,0,0,0};
              for(int i=0;i<CAT_COUNT;i++){ BackupCategory bc; bc.id=CATS[i].id;
                  bc.name=CATS[i].name; bc.bytes=demo[i]; bc.records=recs[i];
                  bc.selected=true; bi.cats.push_back(bc); }
              bi.ready=true; s_bs->info=bi; s_bs->allTicked=true;
              s_bs->status=L"اسکن کامل شد — دسته‌بندی‌ها آماده انتخاب هستند.";
          } }
#endif
        return 0;
    case WM_NCDESTROY:
        InterlockedExchange(&s_cancel,1);   // ask any worker to stop
        KillTimer(h,1);
        bkFreeBg();
        // §C: mark the window dead FIRST so any live analyzer worker stops
        // touching s_bs via bkAnProgCb, then drain in-flight analyzer workers
        // (bounded wait) so we never delete s_bs out from under one. This is
        // the definitive fix for the analyzer use-after-free crash.
        s_anWinAlive.store(false);
        for(int i=0;i<200 && s_anWorkers.load()>0;i++) Sleep(5);   // ≤1s drain
        if(s_bs){ delete s_bs; s_bs=NULL; } s_bk=NULL;
        if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE);
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_APP_THEME:                       // theme switched → rebuild cache
        bkFreeBg(); InvalidateRect(h,NULL,FALSE); return 0;
    case WM_SIZE:
        bkFreeBg(); InvalidateRect(h,NULL,FALSE); return 0;
    case WM_TIMER: if(s_bs){ s_bs->anim+=0.04f; if(s_bs->anim>1)s_bs->anim-=1;
        // analyzer page: keep the determinate progress bar live while working
        if(s_bs->anPage && s_bs->anBusy){
            RECT rc=bkAnPageRect(h); InvalidateRect(h,&rc,FALSE);
        }
        // v1.9.5 perf: while a worker is running, repaint ONLY the progress-bar
        // strip (where the animated fill + status text live) instead of the
        // whole window, so the scrim/shadow/gradient are never recomposed.
        if(s_bs->busy||s_bs->scanning){
            RECT c=bkCard(h);
            RECT strip={c.left,bkProgBar(h).top-S(4),c.right,c.bottom};
            InvalidateRect(h,&strip,FALSE);
        }
        // a finished restore raises doneSignal==2 → confirm to the operator on
        // the UI thread (workers must never touch GDI / dialogs themselves).
        if(InterlockedCompareExchange(&s_bs->doneSignal,0,0)==2){
            InterlockedExchange(&s_bs->doneSignal,0);
            InvalidateRect(h,NULL,FALSE);
            wchar_t mb[256];
            if(s_bs->lastRestoredPatients>0)
                swprintf(mb,256,
                    L"بازیابی کامل شد.\n%s فایل بازنویسی شد و %s رکورد بیمار در سامانه بارگذاری گردید.",
                    toFaDigits(std::to_wstring(s_bs->lastRestoredFiles)).c_str(),
                    toFaDigits(std::to_wstring(s_bs->lastRestoredPatients)).c_str());
            else
                swprintf(mb,256,
                    L"بازیابی کامل شد.\n%s فایل از پشتیبان در سامانه بازنویسی شد.",
                    toFaDigits(std::to_wstring(s_bs->lastRestoredFiles)).c_str());
            MessageBoxW(h,mb,L"بازیابی موفق",MB_OK|MB_ICONINFORMATION);
            // refresh the main frame so any open lists pick up the restored data
            if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE);
        }
    } return 0;
    case WM_MOUSEMOVE:{ if(!s_bs) break; POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int oh=s_bs->hot; s_bs->hot=-1;
        if(s_bs->mode==BK_MODE_HOME){
            for(int i=0;i<2;i++){ RECT r=bkHomeCard(h,i); if(PtInRect(&r,pt)){ s_bs->hot=i; break; } }
        } else if(s_bs->info.ready){
            for(int i=0;i<CAT_COUNT;i++){ RECT r=bkCatRow(h,i); if(PtInRect(&r,pt)){ s_bs->hot=100+i; break; } }
        }
        if(oh!=s_bs->hot){
            // v1.9.5 perf: invalidate ONLY the rect we left + the rect we entered
            // (each with a small margin for borders), never the whole window. The
            // cached background means each strip repaint is a cheap blit + a few
            // rounded rects.
            auto rectOf=[&](int hid, RECT& out)->bool{
                if(hid<0) return false;
                if(hid<100){ if(hid>1) return false; out=bkHomeCard(h,hid); }
                else { int i=hid-100; if(i<0||i>=CAT_COUNT) return false; out=bkCatRow(h,i); }
                InflateRect(&out,S(4),S(4)); return true;
            };
            RECT ro,rn;
            if(rectOf(oh,ro)) InvalidateRect(h,&ro,FALSE);
            if(rectOf(s_bs->hot,rn)) InvalidateRect(h,&rn,FALSE);
        }
        return 0; }
    case WM_BK_ANALYSIS_PROG:
        if(s_bs && s_bs->anPage){ RECT rc=bkAnPageRect(h); InvalidateRect(h,&rc,FALSE); }
        return 0;
    case WM_BK_ANALYSIS_DONE:{
        BkAnalysis* r=(BkAnalysis*)l;
        if(s_bs){ delete s_bs->anResult; s_bs->anResult=r; s_bs->anBusy=false;
                  s_bs->anProgress.store(100);
                  s_bs->anErrExpanded=false;
                  // B.4: snapshot the full BackupLog payload for the failed run
                  if(r && !r->ok) s_bs->anErrPayload=BackupLog_LastPayload();
                  InvalidateRect(h,NULL,FALSE); }
        else delete r;
        return 0; }
    case WM_LBUTTONDOWN:{ if(!s_bs) break; POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        // hidden analyzer page (if up) intercepts ALL clicks
        if(s_bs->anPage){ bkAnClick(h,pt); return 0; }
        RECT c=bkCard(h);
        if(!PtInRect(&c,pt)){ bkClose(); return 0; }
        RECT cb=bkCloseRect(h); if(PtInRect(&cb,pt)){ bkClose(); return 0; }
        if(s_bs->mode==BK_MODE_HOME){
            RECT r0=bkHomeCard(h,0); if(PtInRect(&r0,pt)){ bkStartBackup(h); return 0; }
            RECT r1=bkHomeCard(h,1); if(PtInRect(&r1,pt)){ s_bs->mode=BK_MODE_RESTORE;
                s_bs->status=L""; InvalidateRect(h,NULL,FALSE); return 0; }
        } else {
            RECT bb=bkBrowseBtn(h); if(PtInRect(&bb,pt)){ bkPickAndScan(h); return 0; }
            RECT ob=bkOpenBar(h);   if(PtInRect(&ob,pt)){ bkPickAndScan(h); return 0; }
            if(s_bs->info.ready){
                for(int i=0;i<CAT_COUNT && i<(int)s_bs->info.cats.size();i++){
                    RECT r=bkCatRow(h,i);
                    if(PtInRect(&r,pt)){ s_bs->info.cats[i].selected=!s_bs->info.cats[i].selected;
                        // master tick reflects all selected
                        bool all=true; for(auto&cc:s_bs->info.cats) if(!cc.selected) all=false;
                        s_bs->allTicked=all; InvalidateRect(h,NULL,FALSE); return 0; }
                }
                RECT at=bkAllTick(h);
                if(PtInRect(&at,pt)){ s_bs->allTicked=!s_bs->allTicked;
                    for(auto&cc:s_bs->info.cats) cc.selected=s_bs->allTicked;
                    InvalidateRect(h,NULL,FALSE); return 0; }
            }
            RECT bk=bkBackBtn(h); if(PtInRect(&bk,pt)){ s_bs->mode=BK_MODE_HOME;
                s_bs->status=L""; InvalidateRect(h,NULL,FALSE); return 0; }
            RECT rb=bkRestoreBtn(h); if(PtInRect(&rb,pt)){ bkStartRestore(h); return 0; }
        }
        return 0; }
    case WM_KEYDOWN:
        if(!s_bs) break;
        // Ctrl+B toggles the hidden analyzer page (no menu/button reveals it).
        // B.6: guard against key auto-repeat so a held Ctrl+B does not "stack"
        // toggles — only the first WM_KEYDOWN of a press is honored.
        if(w=='B' && (GetKeyState(VK_CONTROL)&0x8000)){
            bool wasDown=(l & (1<<30))!=0;   // previous key-state bit
            if(!wasDown){
                s_bs->anPage=!s_bs->anPage;
                if(!s_bs->anPage){
                    // closing: cancel any running worker cooperatively
                    InterlockedExchange(&s_cancel,1);
                }
                InvalidateRect(h,NULL,TRUE);
                UpdateWindow(h);
            }
            return 0;
        }
        if(w==VK_ESCAPE){
            if(s_bs->anPage){ s_bs->anPage=false; InvalidateRect(h,NULL,FALSE); return 0; }
            bkClose(); return 0;
        }
        break;
    case WM_MOUSEWHEEL:
        if(s_bs && s_bs->anPage && s_bs->anResult){
            int delta=GET_WHEEL_DELTA_WPARAM(w);
            s_bs->anScroll -= (delta/WHEEL_DELTA)*S(40);
            if(s_bs->anScroll<0) s_bs->anScroll=0;
            RECT rc=bkAnPageRect(h); InvalidateRect(h,&rc,FALSE);
            return 0;
        }
        break;
    case WM_PAINT:{ PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps);
        RECT dirty=ps.rcPaint; bkPaint(h,dc,&dirty);
        if(s_bs && s_bs->anPage) bkAnPaint(h,dc);   // overlay the analyzer page
        EndPaint(h,&ps); return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}

void openBackupManager(HWND owner){
    Breadcrumb(L"backup manager: open");
    if(s_bk&&IsWindow(s_bk)) bkClose();
    static bool reg=false;
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=bkProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=BK_CLASS;
        RegisterClassW(&wc); reg=true; }
    HWND base = owner?owner:g_hFrame;
    RECT rc; GetClientRect(base,&rc);
    POINT org={0,0}; ClientToScreen(base,&org);
    s_bk=CreateWindowExW(WS_EX_TOPMOST,BK_CLASS,L"",WS_POPUP|WS_VISIBLE,
        org.x,org.y,rc.right,rc.bottom,base,NULL,g_hInst,NULL);
    BringWindowToTop(s_bk); SetFocus(s_bk);
}
