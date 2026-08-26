// ============================================================================
//  web_crm_dispatch.inc — the PUBLIC WebCrm_* API. Picks the best available
//  embedded engine at runtime, routes resize/destroy/push to whichever one
//  actually created the view, and owns the management SCREEN host window that
//  SC_MANAGE switches to. Included last into web_crm.cpp.
//
//  Engine preference:
//    1) Microsoft WebView2 (Chromium) — modern, best. Needs the runtime.
//    2) OLE WebBrowser (Trident/MSHTML) — UNIVERSAL fallback (every Windows).
// ============================================================================

// True whenever ANY embedded engine can host the page (always true in practice).
bool WebCrm_Available(){
    return true;
}

// SERVERLESS BOOTSTRAP — pre-build both inlined page variants so the first
// «مدیریت درمانگاه» open is instant. No sockets, no ports, no loopback host.
void WebCrm_Prepare(){
    CrmInlinePage(false);   // MSHTML variant (no @font-face)
    CrmInlinePage(true);    // WebView2 variant (data: URI font)
}

// Create the embedded CRM view as a child of `parent`, sized to fill it, and
// load the management page. Returns the view HWND on success, or NULL.
static HWND WebCrm_CreateView(HWND parent){
    if(!parent) return nullptr;
    WebCrm_Prepare();

    // 1) modern Chromium engine when its runtime is present
    if(WV2Crm_Available()){
        HWND wv=WV2Crm_CreateView(parent);
        if(wv) return wv;
    }
    // 2) universal Trident/MSHTML fallback (still inside the app)
    if(CrmMshtml_Available()){
        HWND mv=CrmMshtml_CreateView(parent);
        if(mv) return mv;
    }
    return nullptr;
}

static void WebCrm_Resize(HWND view, int w, int h){
    if(!view) return;
    if(WV2Crm_Owns(view)){ WV2Crm_Resize(view,w,h); return; }
    CrmMshtml_Resize(view,w,h);
}

void WebCrm_PushEvent(const char* eventName, const std::string& jsonData){
    WV2Crm_PushEvent(eventName,jsonData);
    std::vector<HWND> hosts;
    {
        std::lock_guard<std::mutex> lk(g_crmMshtmlMx);
        for(auto& kv:g_crmMshtmlViews) hosts.push_back(kv.first);
    }
    for(HWND h:hosts) CrmMshtml_PushEvent(h,eventName,jsonData);
}

static void WebCrm_DestroyView(HWND view){
    if(!view) return;
    if(WV2Crm_DestroyView(view)) return;   // was a WebView2 view
    CrmMshtml_Destroy(view);               // else an MSHTML view
}

bool WebCrm_TranslateAccel(MSG* msg){
    if(!msg) return false;
    if(WV2Crm_TranslateAccel(msg)) return false;          // (always false today)
    if(CrmMshtml_TranslateAccel(msg)) return true;        // MSHTML consumed it
    return false;
}

// ============================================================================
//  Management SCREEN host window (the SC_MANAGE surface).
//  A plain child of the frame that owns the embedded CRM web-view lifecycle:
//  the view is created on WM_CREATE, resized to fill on WM_SIZE, and destroyed
//  on WM_DESTROY (so switching away from SC_MANAGE tears the browser down).
// ============================================================================
#define CRM_SCREEN_CLASS L"AzCrmScreen"

static LRESULT CALLBACK crmScreenProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE: {
        // Host the embedded management page. Pumping happens inside (bounded),
        // exactly like the reception tab hosting the admission view.
        HWND view=WebCrm_CreateView(h);
        SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)view);
        if(view) ShowWindow(view,SW_SHOW);
        return 0;
    }
    case WM_SIZE: {
        HWND view=(HWND)GetWindowLongPtrW(h,GWLP_USERDATA);
        if(view) WebCrm_Resize(view,LOWORD(l),HIWORD(l));
        return 0;
    }
    case WM_APP_THEME: {
        std::wstring tn=getSetting(L"theme",L"light");
        std::string ts=std::string("{\"theme\":\"")+
            (tn==L"dark"?"dark":tn==L"neon"?"neon":"light")+"\"}";
        WebCrm_PushEvent("theme.changed", ts);
        InvalidateRect(h,NULL,TRUE);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        HWND view=(HWND)GetWindowLongPtrW(h,GWLP_USERDATA);
        if(view) { DefWindowProcW(h,m,w,l); return 0; }   // view paints itself
        // No view (engine unavailable): paint a friendly centered notice.
        PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        FillRect(dc,&rc,g_brBg);
        SetBkMode(dc,TRANSPARENT);
        SetTextColor(dc,g_theme.textDim);
        HFONT f=CreateFontW(S(20),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
                            DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
                            DEFAULT_PITCH|FF_DONTCARE,L"Vazirmatn");
        HFONT of=(HFONT)SelectObject(dc,f);
        const wchar_t* msg=L"بارگذاری پنل مدیریت ناموفق بود.";
        DrawTextW(dc,msg,-1,&rc,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING);
        SelectObject(dc,of); DeleteObject(f);
        EndPaint(h,&ps);
        return 0;
    }
    case WM_DESTROY: {
        HWND view=(HWND)GetWindowLongPtrW(h,GWLP_USERDATA);
        if(view){ WebCrm_DestroyView(view); SetWindowLongPtrW(h,GWLP_USERDATA,0); }
        return 0;
    }
    }
    return DefWindowProcW(h,m,w,l);
}

HWND WebCrm_CreateScreen(HWND frame){
    if(!frame) return nullptr;
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc={0};
        wc.lpfnWndProc=crmScreenProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.hbrBackground=g_brBg;
        wc.lpszClassName=CRM_SCREEN_CLASS;
        if(!RegisterClassW(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)
            return nullptr;
        reg=true;
    }
    RECT rc=frameContentRect();
    HWND h=CreateWindowExW(0,CRM_SCREEN_CLASS,L"",
        WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
        rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,
        frame,NULL,g_hInst,NULL);
    return h;
}

#ifdef AZ_DEBUG_BUILD
// Headless diagnostics: number of times the embedded CRM page reached crm.init.
long WebCrm_DebugInitHits(){ return (long)g_crmInitHits; }
std::string WebCrm_DebugInlinePage(bool withFontFace){ return CrmInlinePage(withFontFace); }
#endif
