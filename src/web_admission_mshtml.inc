// ============================================================================
//  web_admission_mshtml.inc — UNIVERSAL fallback renderer for the admission
//  page: an embedded OLE WebBrowser control (Trident/MSHTML) hosted inside a
//  child window of the reception tab. Included into web_admission.cpp.
//
//  Why: the modern WebView2 (Chromium) surface is preferred, but it needs the
//  Evergreen runtime installed. On machines without it we STILL render the
//  HTML/CSS/JS admission page *inside the app* (never an external browser) via
//  the WebBrowser ActiveX control, which ships with every Windows and is
//  available under Wine. The page is served by the same loopback HTTP host and
//  talks to C++ through the same /api bridge (fetch), so it stays fully synced.
//
//  We force FEATURE_BROWSER_EMULATION to the highest available document mode so
//  the control uses the most modern Trident engine (not IE7 quirks) — the page
//  is written in tolerant ES5-compatible JS, so it runs correctly here too.
// ============================================================================
#include <exdisp.h>
#include <exdispid.h>
#include <mshtml.h>
#include <mshtmhst.h>   // IDocHostUIHandler — needed so Tab/Enter/Ctrl+A reach the page
#include <docobj.h>
#include <ocidl.h>

namespace {

// ICustomDoc's IID/interface is not always exported by MinGW's import libs, so
// declare both here (MinGW-friendly). ICustomDoc::SetUIHandler lets us register
// our IDocHostUIHandler on the document so MSHTML consults it for accelerators.
static const IID IID_ICustomDoc_local =
  {0x3050f3f0,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
struct ICustomDocLocal : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE SetUIHandler(IDocHostUIHandler* pUIHandler) = 0;
};

// ---- put the process into the highest browser-emulation mode (per-user) ----
static void mshtmlSetBrowserEmulation(){
    wchar_t exe[MAX_PATH]={0}; GetModuleFileNameW(NULL,exe,MAX_PATH);
    const wchar_t* base=exe; for(const wchar_t* p=exe;*p;++p) if(*p==L'\\'||*p==L'/') base=p+1;
    HKEY k;
    if(RegCreateKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Internet Explorer\\Main\\FeatureControl\\FEATURE_BROWSER_EMULATION",
        0,NULL,0,KEY_SET_VALUE,NULL,&k,NULL)==ERROR_SUCCESS){
        DWORD v=11001; // edge/latest available document mode
        RegSetValueExW(k,base,0,REG_DWORD,(const BYTE*)&v,sizeof(v));
        RegCloseKey(k);
    }
}

// ============================================================================
//  window.external IDispatch — the SERVERLESS JS -> C++ bridge (v1.66.0).
//  The page calls  window.external.azCall(verb, payloadJson, page)  and gets
//  the JSON reply back synchronously as a BSTR. Invoke arrives on the UI
//  thread (Trident is single-threaded apartment), where the api layer's
//  RunOnUiThreadSync already inlines safely.
// ============================================================================
struct MshtmlExternal : public IDispatch {
    LONG rc=1;
    volatile LONG initHits=0;
    static const DISPID DISPID_AZCALL=1;
    virtual ~MshtmlExternal(){}

    // ---- IUnknown ----
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if(!ppv) return E_POINTER;
        if(IsEqualIID(riid,IID_IUnknown)||IsEqualIID(riid,IID_IDispatch)){
            *ppv=static_cast<IDispatch*>(this); AddRef(); return S_OK;
        }
        *ppv=nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&rc); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r=InterlockedDecrement(&rc); if(!r) delete this; return r; }

    // ---- IDispatch ----
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* n) override { if(n)*n=0; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT,LCID,ITypeInfo** t) override { if(t)*t=nullptr; return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPOLESTR* names,UINT cNames,
            LCID,DISPID* ids) override {
        if(!names||!ids) return E_POINTER;
        HRESULT hr=S_OK;
        for(UINT i=0;i<cNames;++i){
            if(names[i] && _wcsicmp(names[i],L"azCall")==0) ids[i]=DISPID_AZCALL;
            else { ids[i]=DISPID_UNKNOWN; hr=DISP_E_UNKNOWNNAME; }
        }
        return hr;
    }
    HRESULT STDMETHODCALLTYPE Invoke(DISPID id,REFIID,LCID,WORD flags,
            DISPPARAMS* dp,VARIANT* result,EXCEPINFO*,UINT*) override {
        if(id!=DISPID_AZCALL || !(flags&DISPATCH_METHOD)) return DISP_E_MEMBERNOTFOUND;
        // JS argument order azCall(verb, payload, page) arrives REVERSED in
        // rgvarg: [argc-1]=verb … [0]=last. Coerce each to BSTR defensively.
        auto argStr=[&](int jsIdx)->std::string{
            if(!dp) return "";
            int argc=(int)dp->cArgs;
            int vbIdx=argc-1-jsIdx;               // JS index -> rgvarg index
            if(vbIdx<0||vbIdx>=argc) return "";
            VARIANT v; VariantInit(&v);
            std::string out;
            if(SUCCEEDED(VariantChangeType(&v,&dp->rgvarg[vbIdx],0,VT_BSTR)) && v.bstrVal)
                out=w2u8(v.bstrVal);
            VariantClear(&v);
            return out;
        };
        std::string verb=argStr(0);
        if(verb=="init") InterlockedIncrement(&initHits);
        std::string payload=argStr(1); if(payload.empty()) payload="{}";
        std::string page=argStr(2);
        std::string reply=AdmissionDispatchVerb(verb,payload,page);
        if(result){
            VariantInit(result);
            result->vt=VT_BSTR;
            result->bstrVal=SysAllocString(u82w(reply).c_str());
        }
        return S_OK;
    }
};

// ============================================================================
//  Minimal OLE client site so the WebBrowser control can be hosted in-place.
// ============================================================================
struct MshtmlHost : public IOleClientSite, public IOleInPlaceSite,
                    public IOleInPlaceFrame, public IStorage,
                    public IDocHostUIHandler {
    LONG rc=1;
    HWND hwnd=nullptr;             // container child window
    IOleObject* oleObj=nullptr;
    IWebBrowser2* wb=nullptr;
    MshtmlExternal* external=nullptr;   // window.external bridge (owned)

    MshtmlHost(HWND h):hwnd(h){ external=new MshtmlExternal(); }
    virtual ~MshtmlHost(){ if(external){ external->Release(); external=nullptr; } }

    // ---- IUnknown ----
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if(!ppv) return E_POINTER;
        if(IsEqualIID(riid,IID_IUnknown)||IsEqualIID(riid,IID_IOleClientSite)) *ppv=static_cast<IOleClientSite*>(this);
        else if(IsEqualIID(riid,IID_IOleInPlaceSite)) *ppv=static_cast<IOleInPlaceSite*>(this);
        else if(IsEqualIID(riid,IID_IOleInPlaceFrame)) *ppv=static_cast<IOleInPlaceFrame*>(this);
        else if(IsEqualIID(riid,IID_IStorage)) *ppv=static_cast<IStorage*>(this);
        else if(IsEqualIID(riid,IID_IDocHostUIHandler)) *ppv=static_cast<IDocHostUIHandler*>(this);
        else { *ppv=nullptr; return E_NOINTERFACE; }
        AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&rc); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r=InterlockedDecrement(&rc); if(!r) delete this; return r; }

    // ---- IOleClientSite ----
    HRESULT STDMETHODCALLTYPE SaveObject() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetMoniker(DWORD,DWORD,IMoniker** m) override { if(m)*m=nullptr; return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetContainer(IOleContainer** c) override { if(c)*c=nullptr; return E_NOINTERFACE; }
    HRESULT STDMETHODCALLTYPE ShowObject() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnShowWindow(BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE RequestNewObjectLayout() override { return E_NOTIMPL; }

    // ---- IOleWindow (shared by InPlaceSite/Frame) ----
    HRESULT STDMETHODCALLTYPE GetWindow(HWND* p) override { if(p)*p=hwnd; return S_OK; }
    HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL) override { return S_OK; }

    // ---- IOleInPlaceSite ----
    HRESULT STDMETHODCALLTYPE CanInPlaceActivate() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnInPlaceActivate() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnUIActivate() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetWindowContext(IOleInPlaceFrame** frame, IOleInPlaceUIWindow** doc,
            LPRECT pos, LPRECT clip, LPOLEINPLACEFRAMEINFO info) override {
        if(frame){ *frame=static_cast<IOleInPlaceFrame*>(this); AddRef(); }
        if(doc) *doc=nullptr;
        RECT rc; ::GetClientRect(hwnd,&rc);
        if(pos) *pos=rc;
        if(clip) *clip=rc;
        if(info){ info->cb=sizeof(OLEINPLACEFRAMEINFO); info->fMDIApp=FALSE; info->hwndFrame=hwnd;
                  info->haccel=NULL; info->cAccelEntries=0; }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Scroll(SIZE) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnUIDeactivate(BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnInPlaceDeactivate() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE DiscardUndoState() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE DeactivateAndUndo() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPosRectChange(LPCRECT) override { return S_OK; }

    // ---- IOleInPlaceFrame ----
    HRESULT STDMETHODCALLTYPE GetBorder(LPRECT) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE RequestBorderSpace(LPCBORDERWIDTHS) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetBorderSpace(LPCBORDERWIDTHS) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetActiveObject(IOleInPlaceActiveObject*, LPCOLESTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE InsertMenus(HMENU, LPOLEMENUGROUPWIDTHS) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetMenu(HMENU, HOLEMENU, HWND) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE RemoveMenus(HMENU) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetStatusText(LPCOLESTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE EnableModeless(BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE TranslateAccelerator(LPMSG, WORD) override { return S_FALSE; }

    // ---- IStorage (stubbed — the control tolerates a minimal impl) ----
    HRESULT STDMETHODCALLTYPE CreateStream(const OLECHAR*,DWORD,DWORD,DWORD,IStream**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE OpenStream(const OLECHAR*,void*,DWORD,DWORD,IStream**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateStorage(const OLECHAR*,DWORD,DWORD,DWORD,IStorage**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE OpenStorage(const OLECHAR*,IStorage*,DWORD,SNB,DWORD,IStorage**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CopyTo(DWORD,const IID*,SNB,IStorage*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE MoveElementTo(const OLECHAR*,IStorage*,const OLECHAR*,DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Commit(DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Revert() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE EnumElements(DWORD,void*,DWORD,IEnumSTATSTG**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE DestroyElement(const OLECHAR*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE RenameElement(const OLECHAR*,const OLECHAR*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetElementTimes(const OLECHAR*,const FILETIME*,const FILETIME*,const FILETIME*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetClass(REFCLSID) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetStateBits(DWORD,DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Stat(STATSTG*,DWORD) override { return E_NOTIMPL; }

    // ---- IDocHostUIHandler --------------------------------------------------
    //  The whole reason this interface exists here: MSHTML asks the host site
    //  whether it wants to consume an accelerator BEFORE routing the key into
    //  the document. Returning S_FALSE from TranslateAccelerator tells MSHTML
    //  "the container did NOT handle it — please dispatch it inside the page",
    //  which is exactly what makes Tab / Enter / Ctrl+A / arrows reach the JS
    //  keydown listeners on each admission field. Without this, the embedded
    //  WebBrowser control never fires keydown at all.
    HRESULT STDMETHODCALLTYPE ShowContextMenu(DWORD,POINT*,IUnknown*,IDispatch*) override {
        return S_OK;   // suppress default IE menu; the page ships contextmenu.js
    }
    HRESULT STDMETHODCALLTYPE GetHostInfo(DOCHOSTUIINFO* pInfo) override {
        if(pInfo){
            pInfo->cbSize=sizeof(DOCHOSTUIINFO);
            pInfo->dwFlags=DOCHOSTUIFLAG_NO3DBORDER|DOCHOSTUIFLAG_THEME;
            pInfo->dwDoubleClick=DOCHOSTUIDBLCLK_DEFAULT;
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ShowUI(DWORD,IOleInPlaceActiveObject*,IOleCommandTarget*,
            IOleInPlaceFrame*,IOleInPlaceUIWindow*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE HideUI() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE UpdateUI() override { return S_OK; }
    // NOTE: EnableModeless(BOOL) is shared with IOleInPlaceFrame — the single
    // implementation above (in the IOleInPlaceFrame section) satisfies both, so
    // it must NOT be re-declared here.
    HRESULT STDMETHODCALLTYPE OnDocWindowActivate(BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnFrameWindowActivate(BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE ResizeBorder(LPCRECT,IOleInPlaceUIWindow*,BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE TranslateAccelerator(LPMSG,const GUID*,DWORD) override {
        return S_FALSE;   // <-- key line: let the document handle the keystroke
    }
    HRESULT STDMETHODCALLTYPE GetOptionKeyPath(LPOLESTR* pchKey,DWORD) override {
        if(pchKey) *pchKey=NULL; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDropTarget(IDropTarget*,IDropTarget** ppDropTarget) override {
        if(ppDropTarget) *ppDropTarget=NULL; return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE GetExternal(IDispatch** ppDispatch) override {
        // v1.66.0: hand the page its serverless JS -> C++ bridge. MSHTML asks
        // once per document; window.external.azCall(verb,payload,page) then
        // routes synchronously into AdmissionDispatchVerb on the UI thread.
        if(!ppDispatch) return E_POINTER;
        if(external){ external->AddRef(); *ppDispatch=external; return S_OK; }
        *ppDispatch=NULL; return S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE TranslateUrl(DWORD,LPWSTR,LPWSTR* ppchURLOut) override {
        if(ppchURLOut) *ppchURLOut=NULL; return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE FilterDataObject(IDataObject*,IDataObject** ppDORet) override {
        if(ppDORet) *ppDORet=NULL; return E_NOTIMPL;
    }
};

// per-mshtml-view record
struct MshtmlView {
    HWND host=nullptr;
    MshtmlHost* site=nullptr;
    IWebBrowser2* wb=nullptr;
    IOleObject* oleObj=nullptr;
    IOleInPlaceActiveObject* ipao=nullptr;   // for TranslateAccelerator routing
};
static std::map<HWND,MshtmlView*> g_mshtmlViews;
static std::mutex g_mshtmlMx;

static MshtmlView* mshtmlViewFor(HWND h){
    std::lock_guard<std::mutex> lk(g_mshtmlMx);
    auto it=g_mshtmlViews.find(h); return it==g_mshtmlViews.end()?nullptr:it->second;
}

static void mshtmlResize(HWND host){
    MshtmlView* v=mshtmlViewFor(host); if(!v||!v->oleObj) return;
    RECT rc; GetClientRect(host,&rc);
    IOleInPlaceObject* ipo=nullptr;
    if(SUCCEEDED(v->oleObj->QueryInterface(IID_IOleInPlaceObject,(void**)&ipo)) && ipo){
        ipo->SetObjectRects(&rc,&rc); ipo->Release();
    }
    if(v->wb){ v->wb->put_Width(rc.right); v->wb->put_Height(rc.bottom); }
}

// window proc for the mshtml host child window
static LRESULT CALLBACK mshtmlHostProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_SIZE: mshtmlResize(h); return 0;
    case WM_ERASEBKGND: return 1;
    }
    return DefWindowProcW(h,m,w,l);
}

} // namespace

// ---------------------------------------------------------------------------
//  Create an embedded MSHTML WebBrowser view in `parent`, navigate to the
//  loopback admission page. Returns the host HWND or NULL.
// ---------------------------------------------------------------------------
static HWND MshtmlAdmission_CreateView(HWND parent){
    if(!parent) return nullptr;
    if(AdmissionInlinePage(false).empty()) return nullptr;
    mshtmlSetBrowserEmulation();
    OleInitialize(NULL); // S_FALSE is normal when this UI thread is initialized.

    static bool cls=false; static const wchar_t* CLS=L"AzMshtmlAdmHost";
    if(!cls){
        WNDCLASSW wc={}; wc.lpfnWndProc=mshtmlHostProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
        wc.lpszClassName=CLS;
        if(!RegisterClassW(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)
            return nullptr;
        cls=true;
    }
    RECT rc; GetClientRect(parent,&rc);
    // Keep the host hidden until activation, navigation, document.write and
    // the native bridge have all succeeded; callers otherwise see a blank box.
    HWND host=CreateWindowExW(0,CLS,L"",WS_CHILD|WS_CLIPCHILDREN,
        0,0,rc.right,rc.bottom,parent,NULL,g_hInst,NULL);
    if(!host) return nullptr;
    adBindPendingView(host);

    MshtmlView* v=new MshtmlView(); v->host=host;
    MshtmlHost* site=new MshtmlHost(host); v->site=site;
    bool registered=false;
    auto fail=[&]()->HWND{
        adForgetHost(host);
        if(registered){
            std::lock_guard<std::mutex> lk(g_mshtmlMx);
            g_mshtmlViews.erase(host);
            registered=false;
        }
        // v1.68.0 CRASH FIX — correct OLE teardown order. Close() must run
        // BEFORE any interface ref is dropped or the host window is destroyed:
        // it synchronously deactivates in-place and stops the control, calling
        // back into the (still-alive) site. Releasing ipao/wb first, or tearing
        // down the window while Trident still holds queued callbacks, lets a
        // late callback dispatch into a half-destroyed MshtmlHost vtable → a
        // call through a wild/freed pointer (the reported 0xC0000005 with
        // EIP=0xFFxxxxxx, "Fault: <unknown>"). Order: Stop → Close → disconnect
        // site → release interfaces → release object → destroy window →
        // release site → delete view.
        if(v->wb) v->wb->Stop();
        if(v->oleObj){
            v->oleObj->Close(OLECLOSE_NOSAVE);
            v->oleObj->SetClientSite(NULL);
        }
        if(v->ipao){ v->ipao->Release(); v->ipao=nullptr; }
        if(v->wb){ v->wb->Release(); v->wb=nullptr; }
        if(v->oleObj){ v->oleObj->Release(); v->oleObj=nullptr; }
        if(IsWindow(host)) DestroyWindow(host);
        site->Release(); delete v;
        Breadcrumb(L"admission mshtml: create failed → native fallback");
        return nullptr;
    };

    IOleObject* oleObj=nullptr;
    HRESULT hr=CoCreateInstance(CLSID_WebBrowser,NULL,CLSCTX_INPROC_SERVER,
        IID_IOleObject,(void**)&oleObj);
    if(FAILED(hr)||!oleObj) return fail();
    v->oleObj=oleObj; site->oleObj=oleObj;
    hr=oleObj->SetClientSite(site);
    if(FAILED(hr)) return fail();

    RECT r2; GetClientRect(host,&r2);
    hr=oleObj->DoVerb(OLEIVERB_INPLACEACTIVATE,NULL,site,0,host,&r2);
    if(FAILED(hr)) return fail();

    hr=oleObj->QueryInterface(IID_IOleInPlaceActiveObject,(void**)&v->ipao);
    if(FAILED(hr) || !v->ipao) return fail();

    IWebBrowser2* wb=nullptr;
    hr=oleObj->QueryInterface(IID_IWebBrowser2,(void**)&wb);
    if(FAILED(hr)||!wb) return fail();
    v->wb=wb; site->wb=wb;
    if(FAILED(wb->put_Silent(VARIANT_TRUE)) ||
       FAILED(wb->put_Width(r2.right)) || FAILED(wb->put_Height(r2.bottom)))
        return fail();

    // Navigate to an in-process blank document and require actual readiness.
    VARIANT ve; VariantInit(&ve);
    BSTR burl=SysAllocString(L"about:blank");
    if(!burl) return fail();
    hr=wb->Navigate(burl,&ve,&ve,&ve,&ve);
    SysFreeString(burl);
    if(FAILED(hr)) return fail();

    // v1.69.0: show the host immediately so the user sees the page loading
    // instead of a black box during navigation. Reduced the about:blank wait
    // from 6s to 2s (it completes almost instantly) to cut the black-screen
    // delay the user reported.
    ShowWindow(host,SW_SHOW);

    bool docReady=false;
    DWORD t0=GetTickCount(); MSG pm;
    while(GetTickCount()-t0<=2000){   // v1.69.0: was 6000
        READYSTATE rs=READYSTATE_UNINITIALIZED;
        if(SUCCEEDED(wb->get_ReadyState(&rs)) && rs==READYSTATE_COMPLETE){
            docReady=true; break;
        }
        if(PeekMessageW(&pm,NULL,0,0,PM_REMOVE)){
            TranslateMessage(&pm); DispatchMessageW(&pm);
        } else Sleep(5);
    }
    if(!docReady) return fail();

    IDispatch* dd=nullptr;
    hr=wb->get_Document(&dd);
    if(FAILED(hr)||!dd) return fail();
    // ICustomDoc belongs to the loaded HTML document, not the outer OLE object.
    // Installing it here makes window.external and accelerator routing reliable.
    ICustomDocLocal* cd=nullptr;
    hr=dd->QueryInterface(IID_ICustomDoc_local,(void**)&cd);
    if(FAILED(hr)||!cd){ dd->Release(); return fail(); }
    hr=cd->SetUIHandler(static_cast<IDocHostUIHandler*>(site));
    cd->Release();
    if(FAILED(hr)){ dd->Release(); return fail(); }
    IHTMLDocument2* doc=nullptr;
    hr=dd->QueryInterface(IID_IHTMLDocument2,(void**)&doc);
    dd->Release();
    if(FAILED(hr)||!doc) return fail();

    std::string page=AdmissionPageForHost(false,host);
    if(page.empty()){ doc->Release(); return fail(); }
    std::wstring wpage=u82w(page);
    BSTR bpage=SysAllocStringLen(wpage.c_str(),(UINT)wpage.size());
    SAFEARRAY* sa=SafeArrayCreateVector(VT_VARIANT,0,1);
    if(!sa || !bpage){
        if(sa) SafeArrayDestroy(sa);
        if(bpage) SysFreeString(bpage);
        doc->Release(); return fail();
    }
    VARIANT* pv=nullptr;
    hr=SafeArrayAccessData(sa,(void**)&pv);
    if(FAILED(hr)||!pv){
        SafeArrayDestroy(sa); SysFreeString(bpage);
        doc->Release(); return fail();
    }
    VariantInit(&pv[0]);
    pv[0].vt=VT_BSTR; pv[0].bstrVal=bpage; // SAFEARRAY owns the BSTR now.
    hr=SafeArrayUnaccessData(sa);
    if(FAILED(hr)){
        SafeArrayDestroy(sa);
        doc->Release(); return fail();
    }
    hr=doc->write(sa);
    if(SUCCEEDED(hr)) hr=doc->close();
    SafeArrayDestroy(sa);
    doc->Release();
    if(FAILED(hr)) return fail();

    // Require the bundled JS to call init through window.external before
    // exposing the child. A parser/bridge failure therefore falls back to the
    // native form instead of leaving a blank or permanently loading host.
    bool bridgeReady=false;
    t0=GetTickCount();
    while(GetTickCount()-t0<=3000){   // v1.69.0: was 6000
        if(InterlockedCompareExchange(&site->external->initHits,0,0)>0){
            bridgeReady=true; break;
        }
        if(PeekMessageW(&pm,NULL,0,0,PM_REMOVE)){
            TranslateMessage(&pm); DispatchMessageW(&pm);
        } else Sleep(5);
    }
    if(!bridgeReady) return fail();

    {
        std::lock_guard<std::mutex> lk(g_mshtmlMx);
        g_mshtmlViews[host]=v; registered=true;
    }
    SetFocus(host);
    return host;
}

static void MshtmlAdmission_Resize(HWND host, int, int){ mshtmlResize(host); }

// Deliver a native event directly into one Trident document. This avoids the
// shared /api/poll queue, which cannot distinguish multiple admission tabs.
static bool MshtmlAdmission_PushEvent(HWND host, const char* eventName,
                                      const std::string& jsonData){
    MshtmlView* v=mshtmlViewFor(host);
    if(!v || !v->wb) return false;
    IDispatch* disp=nullptr;
    if(FAILED(v->wb->get_Document(&disp)) || !disp) return false;
    IHTMLDocument2* doc=nullptr;
    HRESULT hr=disp->QueryInterface(IID_IHTMLDocument2,(void**)&doc);
    disp->Release();
    if(FAILED(hr) || !doc) return false;
    IHTMLWindow2* win=nullptr;
    hr=doc->get_parentWindow(&win); doc->Release();
    if(FAILED(hr) || !win) return false;
    std::string msg="{\"event\":"+jstr(u82w(eventName))+",\"data\":"+
        (jsonData.empty()?std::string("{}"):jsonData)+"}";
    std::wstring wmsg=u82w(msg);
    // deliver to BOTH bridges: the admission page's bridge.js entry point and
    // the shared shell runtime (AzBridge.on listeners) from common.js.
    std::wstring script=L"if(window.azAdmissionReceive){window.azAdmissionReceive("+
        wmsg+L");}if(window.azShellReceive){window.azShellReceive("+wmsg+L");}";
    BSTR code=SysAllocString(script.c_str());
    BSTR lang=SysAllocString(L"JavaScript");
    VARIANT ret; VariantInit(&ret);
    hr=win->execScript(code,lang,&ret);
    VariantClear(&ret); SysFreeString(code); SysFreeString(lang); win->Release();
    return SUCCEEDED(hr);
}

static void MshtmlAdmission_Destroy(HWND host){
    adForgetHost(host);
    MshtmlView* v=nullptr;
    { std::lock_guard<std::mutex> lk(g_mshtmlMx);
      auto it=g_mshtmlViews.find(host); if(it!=g_mshtmlViews.end()){ v=it->second; g_mshtmlViews.erase(it); } }
    if(!v) return;
    // v1.68.0: same correct teardown order as the create-time fail() path —
    // Close+disconnect first (site alive for deactivation callbacks), then
    // release interfaces/object, then destroy the window, then release site.
    if(v->wb) v->wb->Stop();
    if(v->oleObj){
        v->oleObj->Close(OLECLOSE_NOSAVE);
        v->oleObj->SetClientSite(NULL);
    }
    if(v->ipao){ v->ipao->Release(); v->ipao=nullptr; }
    if(v->wb){ v->wb->Release(); v->wb=nullptr; }
    if(v->oleObj){ v->oleObj->Release(); v->oleObj=nullptr; }
    if(v->site) v->site->Release();
    if(IsWindow(host)) DestroyWindow(host);
    delete v;
}

// True — the MSHTML WebBrowser control is present on every Windows (and Wine).
static bool MshtmlAdmission_Available(){ return true; }

// ---------------------------------------------------------------------------
//  Route a WM_KEYDOWN (or any) message through the embedded WebBrowser control
//  BEFORE the main pump calls TranslateMessage/DispatchMessage. We only forward
//  the message to the view whose host HWND is an ancestor of pMsg->hwnd, so we
//  never blindly eat keystrokes destined for other windows. Returns true iff a
//  view consumed the message (TranslateAccelerator returned S_OK).
// ---------------------------------------------------------------------------
static bool MshtmlAdmission_TranslateAccel(MSG* pMsg){
    if(!pMsg) return false;
    IOleInPlaceActiveObject* ipao=nullptr;
    {
        std::lock_guard<std::mutex> lk(g_mshtmlMx);
        for(std::map<HWND,MshtmlView*>::iterator it=g_mshtmlViews.begin();
            it!=g_mshtmlViews.end(); ++it){
            MshtmlView* v=it->second;
            if(!v || !v->ipao || !v->host) continue;
            // is the target window inside this view's host subtree?
            HWND w=pMsg->hwnd;
            bool inside=false;
            while(w){
                if(w==v->host){ inside=true; break; }
                w=GetParent(w);
            }
            if(inside){ ipao=v->ipao; ipao->AddRef(); break; }
        }
    }
    if(!ipao) return false;
    HRESULT hr=ipao->TranslateAccelerator(pMsg);
    ipao->Release();
    return hr==S_OK;
}
