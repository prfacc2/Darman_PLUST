// ============================================================================
//  setup_splash.cpp — first-run / prerequisite preparation splash.
//
//  WHY: the hybrid Reception/Appointment surface is rendered by the *system*
//  MSHTML (Trident) WebBrowser OLE control. On a fresh client machine three
//  things must be true for it to render the modern HTML/CSS/JS correctly AND
//  for the synchronous C++↔JS bridge to work:
//    1. The EXE must be registered under FEATURE_BROWSER_EMULATION (value
//       11001 = IE11 standards), otherwise Trident defaults to IE7 *quirks*
//       mode and flexbox/grid/JSON/querySelector silently fail — which looks
//       exactly like "the UI didn't change / the bridge is broken".
//    2. The Vazirmatn font should be installed per-user so Persian text renders
//       with the intended metrics.
//    3. MSHTML itself must be resolvable (it is on every Windows 7→11, but we
//       probe so we can fall back deterministically).
//  We also make sure the data\ and logs\ folders exist so saveReception() and
//  the error log can actually be written (a missing/locked folder is the #1
//  real cause of "خطا در ثبت").
//
//  This module shows a small, centred, branded progress window while it does
//  the above, the first time it runs (and again after a version bump, so a new
//  build re-applies the emulation key). On subsequent runs it returns instantly
//  with no window. Pure Win32/GDI — no extra DLLs, fits the single-EXE rule.
// ============================================================================
#include "app.h"   // brings in <windows.h>, <string>, and the project decls
#include "ui_kit.h"
#include <objbase.h>
#include <gdiplus.h>

// from handlers.cpp
extern void installVazirFont();

// ----------------------------------------------------------------------------
//  Small state shared between the worker thread and the painting window.
// ----------------------------------------------------------------------------
struct SetupState {
    volatile LONG pct   = 0;        // 0..100
    wchar_t       step[160] = L"در حال آماده‌سازی…";
    volatile LONG done  = 0;        // worker finished
    bool          webOk = false;    // MSHTML probe result (legacy, unused)
    bool          firstRun = false; // first launch for THIS build → do heavy work
    bool          lowSpec  = false; // detected weak hardware
};
static SetupState* g_ss = nullptr;

static const wchar_t* SS_CLASS = L"DarmanPlusSetupSplash";

// ----------------------------------------------------------------------------
//  Helpers
// ----------------------------------------------------------------------------
static void ssSet(SetupState* s, int pct, const wchar_t* step){
    InterlockedExchange(&s->pct, pct);
    if(step) lstrcpynW(s->step, step, 160);
}

// §1.18.1: whether THIS build has *ever* completed first-run preparation. The
// heavy/one-time work (font install, marking the build prepared) is gated on
// this so we don't reinstall the font on every launch. The lightweight
// per-system configuration (folders, control classes, GDI+ probe, capability
// detection) runs on EVERY launch inside ssWorker regardless — see RunSetupSplash.
static bool ssFirstRunForBuild(){
    std::wstring done = getSetting(L"setup_done_version", L"");
    if(done != std::wstring(APP_VERSION_W)) return true;
    // v1.89.0: if the exe was MOVED/copied to a new location, re-run the full
    // preparation (font install + per-system configuration) before the app
    // window shows — the user asked for prepare-first behaviour after a
    // relocation, and this also covers a stale/broken per-machine setup.
    wchar_t exe[MAX_PATH]={0};
    if(GetModuleFileNameW(NULL,exe,MAX_PATH)){
        std::wstring stamped = getSetting(L"setup_exe_path", L"");
        if(stamped.empty() || stamped != std::wstring(exe)) return true;
    }
    return false;
}

// §1.18.1: detect weak hardware (≤2GB RAM or ≤2 logical CPUs) so the app can
// trim animations on weak systems. Cheap, no polling.
static bool ssDetectLowSpec(){
    MEMORYSTATUSEX ms; ms.dwLength=sizeof(ms);
    unsigned long long ramMB = 0;
    if(GlobalMemoryStatusEx(&ms)) ramMB = (unsigned long long)(ms.ullTotalPhys/(1024*1024));
    SYSTEM_INFO si; GetSystemInfo(&si);
    DWORD cores = si.dwNumberOfProcessors;
    return (ramMB>0 && ramMB<=2200) || (cores>0 && cores<=2);
}

// Configure the system MSHTML fallback for modern standards. WebView2 remains
// the preferred engine when available, but Windows 7 and clean/offline systems
// must work without downloading any runtime. Per-user registry writes need no
// elevation and are safe/idempotent on every launch.
static bool ssConfigureBrowserFeatures(){
    wchar_t exePath[MAX_PATH]={0};
    if(!GetModuleFileNameW(NULL,exePath,MAX_PATH)) return false;
    const wchar_t* exeName=exePath;
    for(const wchar_t* p=exePath; *p; ++p) if(*p==L'\\'||*p==L'/') exeName=p+1;
    if(!*exeName) return false;

    struct Feature { const wchar_t* name; DWORD value; } features[]={
        {L"FEATURE_BROWSER_EMULATION",11001}, // IE11 Edge/standards mode
        {L"FEATURE_GPU_RENDERING",1},
        {L"FEATURE_DISABLE_NAVIGATION_SOUNDS",1}
    };
    bool ok=true;
    for(const auto& f:features){
        std::wstring path=L"Software\\Microsoft\\Internet Explorer\\Main\\FeatureControl\\";
        path+=f.name;
        HKEY key=NULL; DWORD disposition=0;
        LONG rc=RegCreateKeyExW(HKEY_CURRENT_USER,path.c_str(),0,NULL,0,
                                KEY_SET_VALUE,NULL,&key,&disposition);
        (void)disposition;
        if(rc==ERROR_SUCCESS){
            rc=RegSetValueExW(key,exeName,0,REG_DWORD,
                              reinterpret_cast<const BYTE*>(&f.value),sizeof(f.value));
            RegCloseKey(key);
        }
        if(rc!=ERROR_SUCCESS) ok=false;
    }
    return ok;
}

// v1.69.0: the preparation work is now done SILENTLY and INSTANTLY — no splash
// window, no progress bar, no artificial sleeps. The user explicitly requested
// removing the loader animation ("انیمیشن loader رو حذف کن، برای وارد شدن
// نیازی به انیمیشن نیست"). All operations below are near-instant (<50ms total).
static DWORD WINAPI ssWorker(LPVOID p){
    SetupState* s = (SetupState*)p;

    ssSet(s, 6,  L"بررسی پوشه‌های برنامه…");
    dataDir();   // auto-creates <exe>\data (or override)
    logsDir();   // auto-creates <exe>\logs

    ssSet(s, 24, L"شناسایی سخت‌افزار سیستم…");
    {
        MEMORYSTATUSEX ms; ms.dwLength=sizeof(ms);
        unsigned long long ramMB=0;
        if(GlobalMemoryStatusEx(&ms)) ramMB=(unsigned long long)(ms.ullTotalPhys/(1024*1024));
        SYSTEM_INFO si; GetSystemInfo(&si);
        setSetting(L"sys_low_spec", s->lowSpec?L"1":L"0");
        (void)ramMB; (void)si;
    }

    ssSet(s, 46, s->firstRun ? L"نصب فونت فارسی (Vazirmatn)…"
                             : L"بارگذاری فونت فارسی…");
    installVazirFont();

    ssSet(s, 66, L"آماده‌سازی موتور گرافیکی (GDI+)…");
    {
        ULONG_PTR tok=0; Gdiplus::GdiplusStartupInput in;
        Gdiplus::Status gs=Gdiplus::GdiplusStartup(&tok,&in,NULL);
        if(gs==Gdiplus::Ok && tok){ Gdiplus::GdiplusShutdown(tok); }
        else logLine(L"setup: GDI+ probe failed (will fall back to plain GDI)");
    }

    ssSet(s, 82, L"ثبت اجزای رابط و موتور نمایش…");
    uikit::Az_RegisterControls();
    s->webOk = ssConfigureBrowserFeatures();
    setSetting(L"sys_mshtml_configured",s->webOk?L"1":L"0");

    ssSet(s, 94, L"تکمیل پیکربندی برای این سیستم…");
    if(s->firstRun){
        setSetting(L"setup_done_version", APP_VERSION_W);
        wchar_t exe[MAX_PATH]={0};
        if(GetModuleFileNameW(NULL,exe,MAX_PATH))
            setSetting(L"setup_exe_path", exe);   // v1.89.0 relocation stamp
    }

    ssSet(s, 100, L"آماده است");
    Sleep(s->firstRun ? 300 : 0);   // brief hold on first run only
    InterlockedExchange(&s->done, 1);
    return 0;
}

// ----------------------------------------------------------------------------
//  Window proc — paints a centred branded card with a determinate bar.
// ----------------------------------------------------------------------------
static LRESULT CALLBACK ssProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_ERASEBKGND: return 1; // we paint everything in WM_PAINT
    case WM_TIMER:
        InvalidateRect(h, NULL, FALSE);
        if(g_ss && InterlockedCompareExchange(&g_ss->done,0,0)){
            // hold one extra tick at 100% so it doesn't blink away
            KillTimer(h,1);
            PostMessageW(h, WM_CLOSE, 0, 0);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        int W=rc.right, H=rc.bottom;

        // double-buffer
        HDC mem=CreateCompatibleDC(dc);
        HBITMAP bm=CreateCompatibleBitmap(dc,W,H);
        HBITMAP ob=(HBITMAP)SelectObject(mem,bm);

        // card background (white) with a soft top accent band
        HBRUSH bg=CreateSolidBrush(RGB(255,255,255));
        FillRect(mem,&rc,bg); DeleteObject(bg);
        // accent header band
        RECT band={0,0,W,86};
        HBRUSH hb=CreateSolidBrush(RGB(43,109,244));
        FillRect(mem,&band,hb); DeleteObject(hb);

        SetBkMode(mem,TRANSPARENT);

        // brand glyph circle — v1.64.0 (درمان پلاس): draw the real circular
        // logo on a white disc; fall back to the «آ» glyph if unavailable.
        int cx=W/2, cyTop=43;
        HBRUSH wb=CreateSolidBrush(RGB(255,255,255));
        HPEN   np=(HPEN)GetStockObject(NULL_PEN);
        HGDIOBJ opn=SelectObject(mem,np); HGDIOBJ obr=SelectObject(mem,wb);
        Ellipse(mem, cx-26, cyTop-26, cx+26, cyTop+26);
        SelectObject(mem,opn); SelectObject(mem,obr); DeleteObject(wb);
        RECT logoRc={cx-24, cyTop-24, cx+24, cyTop+24};
        HFONT fBrandGlyph=NULL;
        if(!gpDrawImageResCircle(mem, IMG_LOGO, logoRc)){
            fBrandGlyph=CreateFontW(34,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
                DEFAULT_PITCH,L"Vazirmatn");
            // brand glyph «آ» (blue on white circle)
            SelectObject(mem,fBrandGlyph); SetTextColor(mem,RGB(43,109,244));
            RECT rg={cx-26,cyTop-22,cx+26,cyTop+26};
            DrawTextW(mem,L"\u0622",-1,&rg,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        }

        // fonts
        HFONT fTitle=CreateFontW(22,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
            DEFAULT_PITCH,L"Vazirmatn");
        HFONT fStep=CreateFontW(15,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
            DEFAULT_PITCH,L"Vazirmatn");
        HFONT fPct=CreateFontW(13,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
            DEFAULT_PITCH,L"Vazirmatn");

        // title
        SelectObject(mem,fTitle); SetTextColor(mem,RGB(31,42,68));
        RECT rt={0,108,W,140};
        DrawTextW(mem, APP_NAME_W, -1, &rt, DT_CENTER|DT_SINGLELINE);

        // step text
        SelectObject(mem,fStep); SetTextColor(mem,RGB(110,122,140));
        RECT rs={20,150,W-20,176};
        DrawTextW(mem, g_ss?g_ss->step:L"", -1, &rs,
                  DT_CENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_RTLREADING);

        // progress track + fill
        int pct = g_ss? (int)InterlockedCompareExchange(&g_ss->pct,0,0):0;
        if(pct<3) pct=3; if(pct>100) pct=100;
        int trackL=34, trackR=W-34, trackY=196, trackH=12;
        RECT trk={trackL,trackY,trackR,trackY+trackH};
        HBRUSH tb=CreateSolidBrush(RGB(231,237,245));
        // rounded track
        HGDIOBJ obr2=SelectObject(mem,tb);
        HGDIOBJ opn2=SelectObject(mem,(HPEN)GetStockObject(NULL_PEN));
        RoundRect(mem,trk.left,trk.top,trk.right,trk.bottom,trackH,trackH);
        // fill
        int fillW=(int)((trackR-trackL)*(pct/100.0));
        if(fillW<trackH) fillW=trackH;
        HBRUSH fb=CreateSolidBrush(RGB(43,109,244));
        SelectObject(mem,fb);
        RoundRect(mem,trk.left,trk.top,trk.left+fillW,trk.bottom,trackH,trackH);
        SelectObject(mem,obr2); SelectObject(mem,opn2);
        DeleteObject(tb); DeleteObject(fb);

        // percent label
        SelectObject(mem,fPct); SetTextColor(mem,RGB(43,109,244));
        wchar_t pb[16]; wsprintfW(pb,L"%d%%",pct);
        RECT rp={trackL,trackY+18,trackR,trackY+40};
        DrawTextW(mem,pb,-1,&rp,DT_CENTER|DT_SINGLELINE);

        // blit
        BitBlt(dc,0,0,W,H,mem,0,0,SRCCOPY);

        if(fBrandGlyph) DeleteObject(fBrandGlyph); DeleteObject(fTitle);
        DeleteObject(fStep); DeleteObject(fPct);
        SelectObject(mem,ob); DeleteObject(bm); DeleteDC(mem);
        EndPaint(h,&ps);
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

// ----------------------------------------------------------------------------
//  Public entry.
// ----------------------------------------------------------------------------
// v1.70.0: first-run shows a brief "optimization" window that installs all
// prerequisites (fonts, folders, GDI+, browser config, control registration)
// before the app opens. Repeat launches are SILENT and instant (<50ms).
// The user requested: "یک صفحه ای باز میشه برای اولین بار... هرچیزی که برنامه
// احتیاج داره رو نصب کن... تمام پیش نیاز ها... و بعد که کامل شد برنامه رو باز کن".
bool RunSetupSplash(HINSTANCE hInst){
    SetupState st; g_ss=&st;
    st.firstRun = ssFirstRunForBuild();
    st.lowSpec  = ssDetectLowSpec();

    if(st.firstRun){
        // Show a visible optimization window on first run only.
        WNDCLASSW wc={0};
        wc.lpfnWndProc   = ssProc;
        wc.hInstance     = hInst;
        wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
        wc.lpszClassName = SS_CLASS;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
        RegisterClassW(&wc);

        int sw=GetSystemMetrics(SM_CXSCREEN), sh=GetSystemMetrics(SM_CYSCREEN);
        int W=420, H=260;
        int x=(sw-W)/2, y=(sh-H)/2;
        HWND h=CreateWindowExW(WS_EX_TOPMOST|WS_EX_DLGMODALFRAME,
            SS_CLASS, APP_NAME_W, WS_POPUP|WS_BORDER,
            x,y,W,H, NULL,NULL,hInst,NULL);
        if(h){
            ssSet(&st, 0, L"در حال بهینه‌سازی و نصب پیش‌نیازها…");
            ShowWindow(h, SW_SHOW);
            UpdateWindow(h);
            SetTimer(h, 1, 33, NULL);
            // Run the work on a thread so the window can paint.
            HANDLE th=CreateThread(NULL,0,ssWorker,&st,0,NULL);
            MSG msg;
            while(GetMessageW(&msg,NULL,0,0)){
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            if(th){ WaitForSingleObject(th, 5000); CloseHandle(th); }
            UnregisterClassW(SS_CLASS, hInst);
        } else {
            ssWorker(&st);
        }
    } else {
        // Repeat launch: silent and instant.
        ssWorker(&st);
    }

    g_ss=nullptr;
    (void)hInst;
    return st.webOk;
}
