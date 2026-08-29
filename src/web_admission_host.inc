// ============================================================================
//  web_admission_host.inc — embeds the WebView2 control inside the reception
//  tab and wires the two-way IPC. Included into web_admission.cpp.
//
//  Design:
//   * WebView2Loader.dll (or the Evergreen runtime export) is loaded lazily.
//   * CreateCoreWebView2EnvironmentWithOptions -> environment; then
//     CreateCoreWebView2Controller(parent) -> controller -> ICoreWebView2.
//   * Both async steps are driven by lightweight IUnknown callback objects; we
//     pump messages until the controller is ready (bounded by a timeout) so the
//     public API stays synchronous and simple for the caller.
//   * JS messages (chrome.webview.postMessage) arrive on WebMessageReceived; we
//     parse {verb,id,payload}, call admissionApi, and post {id,result} back.
//   * C++ -> JS pushes go through PostWebMessageAsJson.
// ============================================================================
namespace {

// per-view WebView2 objects, keyed by the host child HWND.
struct AdView {
    HWND host=nullptr;                 // child window that owns the webview
    ICoreWebView2Controller* ctrl=nullptr;
    ICoreWebView2* web=nullptr;
    EventRegistrationToken msgTok{0};
    EventRegistrationToken accelTok{0};
    EventRegistrationToken navTok{0};
    volatile LONG bridgeReady=0;
};
static std::map<HWND,AdView*> g_adViews;
static std::mutex g_adViewsMx;

static AdView* adViewFor(HWND h){
    std::lock_guard<std::mutex> lk(g_adViewsMx);
    auto it=g_adViews.find(h); return it==g_adViews.end()?nullptr:it->second;
}

// v2.02: keep the Chromium composition locked to the host HWND. DefWindowProc
// never called put_Bounds, so a later MoveWindow left پذیرش inset in the C++
// container. Also pin ZoomFactor at 1 — CSS zoom (per tab) is the user control.
static LRESULT CALLBACK wv2HostProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_SIZE:{
        AdView* v=adViewFor(h);
        if(v && v->ctrl){
            RECT rc; GetClientRect(h,&rc);
            v->ctrl->put_Bounds(rc);
            v->ctrl->put_ZoomFactor(1.0);
        }
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    }
    return DefWindowProcW(h,m,w,l);
}

// ---- WebView2Loader.dll dynamic binding ------------------------------------
static PFN_CreateWebView2Env         pCreateEnv=nullptr;
static PFN_GetAvailableBrowserVersion pGetVer=nullptr;
static bool adLoaderTried=false;

// Extract the embedded WebView2Loader.dll (RCDATA 700) to a per-user writable
// folder and return its full path. The EXE folder is often read-only (Program
// Files), so we use %LOCALAPPDATA%\DarmanPlus\runtime. Idempotent: a file that
// already matches the embedded size is reused (no per-launch rewrite churn).
static std::wstring adExtractLoaderPath(){
    wchar_t base[MAX_PATH]={0};
    if(FAILED(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA,NULL,0,base))) return L"";
    std::wstring dir=base; dir+=L"\\DarmanPlus\\runtime";
    CreateDirectoryW(dir.c_str(),NULL);   // ok if it already exists
    std::wstring path=dir+L"\\WebView2Loader.dll";

    // Load the embedded blob so we can (re)write it when missing or truncated.
    HRSRC hr=FindResourceW(g_hInst,MAKEINTRESOURCEW(700),RT_RCDATA);
    if(!hr) return L"";
    DWORD need=SizeofResource(g_hInst,hr);
    HGLOBAL hg=LoadResource(g_hInst,hr); if(!hg) return L"";
    void* dat=LockResource(hg); if(!dat||!need) return L"";

    bool write=true;
    HANDLE ex=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,
                          OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(ex!=INVALID_HANDLE_VALUE){
        DWORD sz=GetFileSize(ex,NULL);
        CloseHandle(ex);
        if(sz==need) write=false;     // unchanged build → reuse the cached copy
    }
    if(write){
        HANDLE hf=CreateFileW(path.c_str(),GENERIC_WRITE,0,NULL,
                              CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
        if(hf!=INVALID_HANDLE_VALUE){
            DWORD wr=0; WriteFile(hf,dat,need,&wr,NULL); CloseHandle(hf);
            if(wr!=need) return L"";  // partial write → refuse to load it
        } else return L"";
    }
    return path;
}

static void adLoadLoader(){
    if(adLoaderTried) return; adLoaderTried=true;
    HMODULE m=LoadLibraryW(L"WebView2Loader.dll");
    if(!m){
        // try the fixed-runtime folder next to the exe
        wchar_t path[MAX_PATH]; GetModuleFileNameW(NULL,path,MAX_PATH);
        std::wstring p=path; size_t s=p.find_last_of(L"\\/"); if(s!=std::wstring::npos) p=p.substr(0,s+1);
        p+=L"WebView2Loader.dll";
        m=LoadLibraryW(p.c_str());
    }
    if(!m){
        // v1.68.0: last resort — the redistributable loader extracted from the
        // embedded RCDATA blob to a per-user folder. This lights up the modern
        // Chromium engine on machines that have the Evergreen runtime but no
        // loose loader DLL beside the EXE (the common case for a single-file
        // build). A failure here just leaves pCreateEnv NULL → MSHTML fallback.
        std::wstring extracted=adExtractLoaderPath();
        if(!extracted.empty()) m=LoadLibraryW(extracted.c_str());
    }
    if(m){
        pCreateEnv=reinterpret_cast<PFN_CreateWebView2Env>(
            reinterpret_cast<void*>(GetProcAddress(m,"CreateCoreWebView2EnvironmentWithOptions")));
        pGetVer=reinterpret_cast<PFN_GetAvailableBrowserVersion>(
            reinterpret_cast<void*>(GetProcAddress(m,"GetAvailableCoreWebView2BrowserVersionString")));
    }
}

// ============================================================================
//  COM callback objects (single-use, self-deleting on final Release).
// ============================================================================

// WebMessageReceived handler → route to admissionApi and reply.
struct MsgHandler : public ICoreWebView2WebMessageReceivedEventHandler {
    LONG rc=1; HWND host;
    MsgHandler(HWND h):host(h){}
    virtual ~MsgHandler(){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if(!ppv) return E_POINTER;
        if(IsEqualIID(riid,IID_IUnknown)||
           IsEqualIID(riid,IID_ICoreWebView2WebMessageReceivedEventHandler_)){
            *ppv=this; InterlockedIncrement(&rc); return S_OK; }
        *ppv=nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&rc); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r=InterlockedDecrement(&rc); if(!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender,
            ICoreWebView2WebMessageReceivedEventArgs* args) override {
        if(!args) return S_OK;
        LPWSTR js=nullptr;
        if(FAILED(args->get_WebMessageAsJson(&js))||!js) return S_OK;
        std::string body=w2u8(js); CoTaskMemFree(js);
        // body = {"verb":"...","id":"...","page":"...","payload":{...}}
        std::string verb,id,page;
        jsonGetString(body,"verb",verb);
        jsonGetString(body,"id",id);
        jsonGetString(body,"page",page);
        if(verb=="init"){
            AdView* v=adViewFor(host);
            if(v) InterlockedExchange(&v->bridgeReady,1);
        }
        std::string payload; if(!jsonGetRaw(body,"payload",payload)) payload="{}";
        std::string result=AdmissionDispatchVerb(verb,payload,page);
        // reply: {"id":"<id>","result":<result>}
        std::string reply="{\"id\":"+jstr(u82w(id))+",\"result\":"+result+"}";
        if(sender) sender->PostWebMessageAsJson(u82w(reply).c_str());
        return S_OK;
    }
};

// AcceleratorKeyPressed handler → let the WebView handle nav/edit keys itself
// (Tab / Enter / Ctrl+A / arrows / Home / End / Esc) so the page keydown
// listeners fire. Everything else keeps WebView2's default behaviour.
struct AccelHandler : public ICoreWebView2AcceleratorKeyPressedEventHandler {
    LONG rc=1;
    AccelHandler(){}
    virtual ~AccelHandler(){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if(!ppv) return E_POINTER;
        if(IsEqualIID(riid,IID_IUnknown)||
           IsEqualIID(riid,IID_ICoreWebView2AcceleratorKeyPressedEventHandler_)){
            *ppv=this; InterlockedIncrement(&rc); return S_OK; }
        *ppv=nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&rc); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r=InterlockedDecrement(&rc); if(!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2Controller*,
            ICoreWebView2AcceleratorKeyPressedEventArgs* args) override {
        if(!args) return S_OK;
        int kind=0; UINT vk=0;
        args->get_KeyEventKind(&kind);
        args->get_VirtualKey(&vk);
        bool isDown = (kind==COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN ||
                       kind==COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN);
        if(isDown){
            bool ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0;
            /* v2.06: Ctrl+Tab is the app-level tab-switch hotkey — when Ctrl
               is held the webview must NOT take the Tab (the page's JS would
               treat it as a plain field-advance). Mark it handled here and
               forward to the main frame so the global router cycles the tabs. */
            if(vk==VK_TAB && ctrl){
                args->put_Handled(TRUE);
                if(g_hFrame) PostMessageW(g_hFrame, WM_APP+42, 0, 0);
                return S_OK;
            }
            if(vk==VK_TAB || vk==VK_RETURN || vk==VK_LEFT || vk==VK_RIGHT ||
               vk==VK_UP  || vk==VK_DOWN   || vk==VK_HOME || vk==VK_END  ||
               vk==VK_ESCAPE || (ctrl && vk=='A')){
                args->put_Handled(FALSE);   // WebView processes it → JS keydown fires
            }
        }
        return S_OK;
    }
};

// Heap-owned state shared by both WebView2 completion callbacks. The caller
// releases its reference after the bounded wait; callbacks keep their own
// references, so a late completion can never write to a dead stack frame.
struct CreateState {
    LONG rc=1;
    volatile LONG done=0;
    volatile LONG cancelled=0;
    volatile LONG navReady=0;
    HRESULT hr=E_PENDING;
    HWND host;
    explicit CreateState(HWND h):host(h){}
    void AddRef(){ InterlockedIncrement(&rc); }
    void Release(){ if(!InterlockedDecrement(&rc)) delete this; }
    bool IsCancelled(){ return InterlockedCompareExchange(&cancelled,0,0)!=0; }
    void Cancel(){ InterlockedExchange(&cancelled,1); }
    void Complete(HRESULT h){ hr=h; InterlockedExchange(&done,1); }
};

// Navigation completion is the readiness boundary for an embedded view: a
// controller alone can exist while NavigateToString failed, yielding a blank
// host. Only a successful navigation publishes completion to the caller.
struct NavHandler : public ICoreWebView2NavigationCompletedEventHandler {
    LONG rc=1; CreateState* state;
    explicit NavHandler(CreateState* s):state(s){ state->AddRef(); }
    virtual ~NavHandler(){ state->Release(); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if(!ppv) return E_POINTER;
        if(IsEqualIID(riid,IID_IUnknown)||
           IsEqualIID(riid,IID_ICoreWebView2NavigationCompletedEventHandler_)){
            *ppv=this; InterlockedIncrement(&rc); return S_OK; }
        *ppv=nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&rc); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r=InterlockedDecrement(&rc); if(!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2*,
            ICoreWebView2NavigationCompletedEventArgs* args) override {
        if(state->IsCancelled()) return S_OK;
        BOOL ok=FALSE;
        HRESULT hr=args?args->get_IsSuccess(&ok):E_POINTER;
        if(SUCCEEDED(hr) && ok){
            InterlockedExchange(&state->navReady,1);
            AdView* v=adViewFor(state->host);
            if(v && v->ctrl) v->ctrl->put_IsVisible(TRUE);
        }
        else state->Complete(FAILED(hr)?hr:E_FAIL);
        return S_OK;
    }
};

// Controller-created handler.
struct CtrlHandler : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
    LONG rc=1; CreateState* state;
    explicit CtrlHandler(CreateState* s):state(s){ state->AddRef(); }
    virtual ~CtrlHandler(){ state->Release(); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if(!ppv) return E_POINTER;
        if(IsEqualIID(riid,IID_IUnknown)||
           IsEqualIID(riid,IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler_)){
            *ppv=this; InterlockedIncrement(&rc); return S_OK; }
        *ppv=nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&rc); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r=InterlockedDecrement(&rc); if(!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Controller* ctrl) override {
        if(state->IsCancelled()) return S_OK;
        if(FAILED(result) || !ctrl){ state->Complete(FAILED(result)?result:E_FAIL); return S_OK; }

        AdView* v=adViewFor(state->host);
        if(!v){ state->Complete(E_ABORT); return S_OK; }
        ctrl->AddRef(); v->ctrl=ctrl;
        HRESULT hr=ctrl->get_CoreWebView2(&v->web);
        if(FAILED(hr) || !v->web){ state->Complete(FAILED(hr)?hr:E_FAIL); return S_OK; }

        // Subscribe before navigation so completion cannot be missed.
        NavHandler* nh=new NavHandler(state);
        hr=v->web->add_NavigationCompleted(nh,&v->navTok);
        nh->Release();
        if(FAILED(hr)){ state->Complete(hr); return S_OK; }

        AccelHandler* ah=new AccelHandler();
        hr=ctrl->add_AcceleratorKeyPressed(ah,&v->accelTok);
        ah->Release();
        if(FAILED(hr)){ state->Complete(hr); return S_OK; }

        MsgHandler* mh=new MsgHandler(state->host);
        hr=v->web->add_WebMessageReceived(mh,&v->msgTok);
        mh->Release();
        if(FAILED(hr)){ state->Complete(hr); return S_OK; }

        RECT bounds; GetClientRect(state->host,&bounds);
        hr=ctrl->put_Bounds(bounds);
        if(SUCCEEDED(hr)) ctrl->put_ZoomFactor(1.0);
        // v2.00: keep the controller hidden until NavigateToString finishes so
        // the user never sees the WebView2 default black flash. The host HWND
        // is already painted with g_brBg.
        if(SUCCEEDED(hr)) hr=ctrl->put_IsVisible(FALSE);
        if(FAILED(hr)){ state->Complete(hr); return S_OK; }

        std::string page=AdmissionPageForHost(true,state->host);
        if(page.empty()){ state->Complete(E_FAIL); return S_OK; }
        hr=v->web->NavigateToString(u82w(page).c_str());
        if(FAILED(hr)) state->Complete(hr);
        return S_OK;
    }
};

// ---- shared WebView2 environment (created ONCE, reused for every controller) -
//  v1.95 PERF: the old path called CreateCoreWebView2EnvironmentWithOptions on
//  EVERY tab open. That launches/connects the WebView2 browser process and can
//  take 2-4s on first use (non-trivial afterwards too) — and it leaked the env
//  every time. The environment is designed to be SHARED: we create ONE (pre-
//  warmed at startup via WV2_PreWarm) and hand it to every controller. Per-tab
//  cost then drops to just CreateCoreWebView2Controller + NavigateToString, so
//  the synchronous WM_CREATE block (the black-screen window) shrinks drastically.
static ICoreWebView2Environment* g_wv2Env=nullptr;   // strong ref, app lifetime
static volatile LONG g_wv2EnvReq=0;                  // 0=idle 1=in-flight 2=settled
static HRESULT g_wv2EnvHr=E_PENDING;
static std::mutex g_wv2EnvMx;

// Environment-created handler: just cache the environment. No controller is
// created here — each view creates its own controller from the cached env in
// WV2_CreateView. Self-deleting on final Release (WebView2 drops its callback).
struct EnvReadyHandler : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
    LONG rc=1;
    EnvReadyHandler(){}
    virtual ~EnvReadyHandler(){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if(!ppv) return E_POINTER;
        if(IsEqualIID(riid,IID_IUnknown)||
           IsEqualIID(riid,IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler_)){
            *ppv=this; InterlockedIncrement(&rc); return S_OK; }
        *ppv=nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&rc); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r=InterlockedDecrement(&rc); if(!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Environment* env) override {
        if(SUCCEEDED(result) && env){
            env->AddRef();   // cache holds a strong reference for the app lifetime
            std::lock_guard<std::mutex> lk(g_wv2EnvMx);
            g_wv2Env=env;
            g_wv2EnvHr=S_OK;
        } else {
            std::lock_guard<std::mutex> lk(g_wv2EnvMx);
            g_wv2EnvHr=FAILED(result)?result:E_FAIL;
        }
        InterlockedExchange(&g_wv2EnvReq,2);   // settled (success or failure)
        return S_OK;
    }
};

// user-data folder for the runtime (per-user, offline-safe)
static std::wstring adUserDataFolder(){
    wchar_t buf[MAX_PATH]={0};
    if(SUCCEEDED(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA,NULL,0,buf))){
        std::wstring p=buf; p+=L"\\DarmanPlus\\WebView2";
        return p;
    }
    return L"";
}

} // namespace

// ============================================================================
//  Public API
// ============================================================================
static bool WV2_DestroyView(HWND view);

static bool WV2_Available(){
    adLoadLoader();
    if(!pCreateEnv) return false;
    if(pGetVer){
        LPWSTR ver=nullptr;
        HRESULT hr=pGetVer(nullptr,&ver);
        bool ok = SUCCEEDED(hr) && ver && ver[0];
        if(ver) CoTaskMemFree(ver);
        return ok;
    }
    return true; // loader present but no version query — assume usable
}

// v1.95 PERF: kick off the shared environment creation as early as possible
// (called from WebAdmission_Prepare at launch, only when WV2 is available).
// Non-blocking — the env-ready callback fires later on the UI thread once
// messages are pumped, so by the time the user opens the first tab the
// environment is usually already warm. Idempotent: a second call is a no-op
// once creation is in flight or settled.
static void WV2_PreWarm(){
    if(InterlockedCompareExchange(&g_wv2EnvReq,1,0)!=0) return;  // already started
    adLoadLoader();
    if(!pCreateEnv){
        std::lock_guard<std::mutex> lk(g_wv2EnvMx);
        g_wv2EnvHr=E_FAIL; InterlockedExchange(&g_wv2EnvReq,2);
        return;
    }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);   // idempotent on UI thread
    std::wstring udf=adUserDataFolder();
    EnvReadyHandler* eh=new EnvReadyHandler();
    HRESULT hr=pCreateEnv(nullptr, udf.empty()?nullptr:udf.c_str(), nullptr, eh);
    eh->Release();
    if(FAILED(hr)){
        std::lock_guard<std::mutex> lk(g_wv2EnvMx);
        g_wv2EnvHr=hr; InterlockedExchange(&g_wv2EnvReq,2);
    }
}

// Return the shared environment, pumping the UI message queue until the
// (possibly pre-warmed) creation settles. Returns null (→ MSHTML fallback) when
// WebView2 is unavailable or environment creation failed/timed out.
static ICoreWebView2Environment* WV2_EnsureEnvironment(){
    if(InterlockedCompareExchange(&g_wv2EnvReq,0,0)==0) WV2_PreWarm();  // lazy start
    DWORD t0=GetTickCount(); MSG pm;
    while(InterlockedCompareExchange(&g_wv2EnvReq,0,0)!=2){
        if(GetTickCount()-t0>8000) break;   // env launch can take a few seconds
        if(PeekMessageW(&pm,NULL,0,0,PM_REMOVE)){
            TranslateMessage(&pm); DispatchMessageW(&pm);
        } else Sleep(5);
    }
    std::lock_guard<std::mutex> lk(g_wv2EnvMx);
    if(SUCCEEDED(g_wv2EnvHr) && g_wv2Env){ g_wv2Env->AddRef(); return g_wv2Env; }
    return nullptr;
}

static HWND WV2_CreateView(HWND parent){
    if(!parent) return nullptr;
    adLoadLoader();
    if(!pCreateEnv) return nullptr;

    // v1.66.0: serverless — the page is inlined from RCDATA, no host needed.
    if(AdmissionInlinePage(true).empty()) return nullptr;

    // COM must be initialised on this (UI) thread for WebView2.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // a plain child window to host the webview (fills the parent)
    static bool cls=false;
    static const wchar_t* CLS=L"AzWebAdmissionHost";
    if(!cls){
        WNDCLASSW wc={}; wc.lpfnWndProc=wv2HostProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
        wc.lpszClassName=CLS;
        if(!RegisterClassW(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)
            return nullptr;
        cls=true;
    }
    RECT rc; GetClientRect(parent,&rc);
    HWND host=CreateWindowExW(0,CLS,L"",WS_CHILD|WS_CLIPCHILDREN,
        0,0,rc.right,rc.bottom,parent,NULL,g_hInst,NULL);
    if(!host) return nullptr;
    adBindPendingView(host);

    AdView* v=new AdView(); v->host=host;
    { std::lock_guard<std::mutex> lk(g_adViewsMx); g_adViews[host]=v; }

    // v1.95 PERF: reuse the single shared (pre-warmed) environment instead of
    // creating a new one per tab. WV2_EnsureEnvironment pumps until the env is
    // ready (it is usually already warm by now) and returns null on failure.
    ICoreWebView2Environment* env=WV2_EnsureEnvironment();
    if(!env){
        Breadcrumb(L"admission wv2: env unavailable → mshtml fallback");
        WV2_DestroyView(host);
        return nullptr;
    }

    CreateState* state=new CreateState(host);
    // Hold one extra reference across cancellation/teardown. WebView2 owns the
    // callback refs, but Close() can synchronously release those callbacks.
    state->AddRef();
    CtrlHandler* ch=new CtrlHandler(state);
    HRESULT hr=env->CreateCoreWebView2Controller(host,ch);
    ch->Release();
    env->Release();   // drop our local ref; the cache keeps the env alive
    if(FAILED(hr)) state->Complete(hr);

    // Pump until both navigation and the JS bridge are ready (or an async
    // setup step fails), not merely until a controller exists.
    DWORD start=GetTickCount();
    MSG msg;
    while(!InterlockedCompareExchange(&state->done,0,0) &&
          !(InterlockedCompareExchange(&state->navReady,0,0) &&
            InterlockedCompareExchange(&v->bridgeReady,0,0))){
        if(GetTickCount()-start>8000) break;  // env is pre-warmed; this bounds controller+nav (was needed for 2-4s first env launch)
        if(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){
            TranslateMessage(&msg); DispatchMessageW(&msg);
        } else Sleep(5);
    }

    bool ready=InterlockedCompareExchange(&state->done,0,0)==0 &&
               InterlockedCompareExchange(&state->navReady,0,0)!=0 &&
               InterlockedCompareExchange(&v->bridgeReady,0,0)!=0 &&
               v->ctrl && v->web;
    if(!ready) state->Cancel();
    state->Release(); // caller/wait reference
    if(!ready){
        WV2_DestroyView(host);
        state->Release(); // teardown guard
        return nullptr;
    }
    state->Release(); // teardown guard
    ShowWindow(host,SW_SHOW);
    return host;
}

static void WV2_Resize(HWND view, int w, int h){
    AdView* v=adViewFor(view); if(!v||!v->ctrl) return;
    if(w<1) w=1; if(h<1) h=1;
    MoveWindow(view,0,0,w,h,TRUE);
    RECT rc={0,0,w,h};
    v->ctrl->put_Bounds(rc);
    v->ctrl->put_ZoomFactor(1.0);
}

static void WV2_PushEvent(const char* eventName, const std::string& jsonData){
    // format: {"event":"name","data":<jsonData>}
    std::string msg="{\"event\":"+jstr(u82w(eventName))+",\"data\":"+
        (jsonData.empty()?std::string("{}"):jsonData)+"}";
    // direct push to every live embedded WebView2 view (v1.66.0: the old HTTP
    // /api/poll fallback queue is gone — MSHTML views get the same event via
    // the dispatch layer's per-view execScript broadcast).
    std::lock_guard<std::mutex> lk(g_adViewsMx);
    for(auto& kv:g_adViews){
        AdView* v=kv.second;
        if(v && v->web) v->web->PostWebMessageAsJson(u82w(msg).c_str());
    }
}

// Native frame actions must target only the active admission tab. Broadcasting a
// print event would print once for every open tab, so this path deliberately does
// not enqueue into the shared HTTP fallback queue.
static bool WV2_PushEventTo(HWND view, const char* eventName, const std::string& jsonData){
    AdView* v=adViewFor(view);
    if(!v || !v->web) return false;
    std::string msg="{\"event\":"+jstr(u82w(eventName))+",\"data\":"+
        (jsonData.empty()?std::string("{}"):jsonData)+"}";
    return SUCCEEDED(v->web->PostWebMessageAsJson(u82w(msg).c_str()));
}

static bool WV2_DestroyView(HWND view){
    adForgetHost(view);
    AdView* v=nullptr;
    { std::lock_guard<std::mutex> lk(g_adViewsMx);
      auto it=g_adViews.find(view); if(it!=g_adViews.end()){ v=it->second; g_adViews.erase(it); } }
    if(!v) return false;   // not a WebView2 view
    if(v->web){
        if(v->navTok.value) v->web->remove_NavigationCompleted(v->navTok);
        if(v->msgTok.value) v->web->remove_WebMessageReceived(v->msgTok);
        v->web->Release(); v->web=nullptr;
    }
    if(v->ctrl){
        if(v->accelTok.value) v->ctrl->remove_AcceleratorKeyPressed(v->accelTok);
        v->ctrl->Close(); v->ctrl->Release(); v->ctrl=nullptr;
    }
    if(IsWindow(view)) DestroyWindow(view);
    delete v;
    return true;
}

// True if `view` is a live WebView2 host we own.
static bool WV2_Owns(HWND view){
    std::lock_guard<std::mutex> lk(g_adViewsMx);
    return g_adViews.find(view)!=g_adViews.end();
}

// WebView2 does NOT need TranslateAccelerator in the main pump — the
// add_AcceleratorKeyPressed subscription is sufficient. This exists only so the
// dispatch layer can treat both engines uniformly.
static bool WV2_TranslateAccel(MSG*){ return false; }
