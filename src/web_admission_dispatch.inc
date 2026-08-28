// ============================================================================
//  web_admission_dispatch.inc — the PUBLIC WebAdmission_* API. Picks the best
//  available embedded engine at runtime and routes resize/destroy to whichever
//  one actually created the view. Included last into web_admission.cpp.
//
//  Engine preference:
//    1) Microsoft WebView2 (Chromium) — modern, best. Needs the runtime.
//    2) OLE WebBrowser (Trident/MSHTML) — UNIVERSAL fallback that ships with
//       every Windows (and Wine), so the HTML admission page ALWAYS renders
//       INSIDE the app — never an external browser, never the native GDI form
//       (that stays only as an absolute last-resort safety net).
// ============================================================================

// True whenever ANY embedded engine can host the page (always true in practice,
// since the MSHTML WebBrowser control is present on every Windows).
bool WebAdmission_Available(){
    return true;
}

// v1.66.0 SERVERLESS BOOTSTRAP — replaces the old WebAdmission_EnsureHost().
// Called once at startup (and defensively before the first view): registers
// the multi-page verb handlers (ping / client.log / client.metrics), pre-builds
// both inlined page variants so the first «پذیرش بیمار» click is instant, and —
// v1.95 PERF — pre-warms the shared WebView2 environment so the browser process
// is already running by the time the user opens the first tab (that launch is
// the 2-4s black-screen delay the user reported). No sockets, no ports.
void WebAdmission_Prepare(){
    WebPages_RegisterBuiltins();
    AdmissionInlinePage(false);   // MSHTML variant (no @font-face)
    AdmissionInlinePage(true);    // WebView2 variant (data: URI font)
    if(WV2_Available()) WV2_PreWarm();   // v1.95: warm the env at launch, not at first tab
}
void WebAdmission_Warm(){
    WebAdmission_Prepare();
    if(WV2_Available()){
        ICoreWebView2Environment* env=WV2_EnsureEnvironment();
        if(env) env->Release();
    }
}

HWND WebAdmission_CreateView(HWND parent){
    if(!parent) return nullptr;
    WebAdmission_Prepare();

    // 1) modern Chromium engine when its runtime is present
    if(WV2_Available()){
        HWND wv=WV2_CreateView(parent);
        if(wv) return wv;
    }
    // 2) universal Trident/MSHTML fallback (still inside the app)
    if(MshtmlAdmission_Available()){
        HWND mv=MshtmlAdmission_CreateView(parent);
        if(mv) return mv;
    }
    return nullptr;   // caller keeps the native GDI form
}

HWND WebAdmission_CreateViewEx(HWND parent, const char* surface,
                               const std::string& ticketJson){
    adSetPendingView(surface, ticketJson);
    HWND h=WebAdmission_CreateView(parent);
    adClearPendingView();
    return h;
}

void WebAdmission_Resize(HWND view, int w, int h){
    if(!view) return;
    if(WV2_Owns(view)){ WV2_Resize(view,w,h); return; }
    MshtmlAdmission_Resize(view,w,h);
}

void WebAdmission_PushEvent(const char* eventName, const std::string& jsonData){
    // WebView2 views get a direct PostWebMessageAsJson push; MSHTML views get
    // the same event injected per-view via execScript (v1.66.0: the old HTTP
    // /api/poll queue is gone, so Trident must be pushed explicitly too).
    WV2_PushEvent(eventName,jsonData);
    std::vector<HWND> hosts;
    {
        std::lock_guard<std::mutex> lk(g_mshtmlMx);
        for(auto& kv:g_mshtmlViews) hosts.push_back(kv.first);
    }
    for(HWND h:hosts) MshtmlAdmission_PushEvent(h,eventName,jsonData);
}

void WebAdmission_PushEventTo(HWND view, const char* eventName,
                              const std::string& jsonData){
    if(!view || !eventName) return;
    if(WV2_PushEventTo(view,eventName,jsonData)) return;
    MshtmlAdmission_PushEvent(view,eventName,jsonData);
}

// ---- LIVE SERVICE / INSURANCE SYNC from Management -------------------------
//  When the Management «مدیریت خدمات» page adds / edits / deletes a service (or
//  the insurance tables change), we push the fresh catalog + insurance snapshot
//  to every open embedded Admission view. The JS side re-renders any open
//  service-suggestion list and refreshes the insurance selectors WITHOUT a
//  page reload — the admission surface reflects Management edits immediately.
void WebAdmission_NotifyCatalogChanged(){
    // Only build/push when at least one embedded view is alive (cheap guard).
    {
        bool any=false;
        { std::lock_guard<std::mutex> lk(g_adViewsMx); any=!g_adViews.empty(); }
        if(!any){ std::lock_guard<std::mutex> lk(g_mshtmlMx); any=!g_mshtmlViews.empty(); }
        if(!any) return;
    }
    // Full active service catalog (Management is the single source of truth).
    std::string cat="{\"services\":[";
    {
        auto svcs=loadServices();
        int n=0;
        for(const auto& s:svcs){
            if(s.status==0) continue;      // only sellable services
            if(n++) cat+=",";
            cat+=serviceJson(s);
        }
    }
    cat+="]}";
    WebAdmission_PushEvent("catalog.update",cat);

    // Insurance selectors (base + supplementary) may also change in Management.
    std::string ins="{\"main\":"+insurancesJson()+",\"supp\":"+suppInsurancesJson()+"}";
    WebAdmission_PushEvent("insurance.update",ins);
}

void WebAdmission_DestroyView(HWND view){
    if(!view) return;
    if(WV2_DestroyView(view)) return;   // was a WebView2 view
    MshtmlAdmission_Destroy(view);      // else an MSHTML view
}

// Give the embedded admission browser a chance to consume accelerator keys
// (Tab / Enter / Ctrl+A / arrows) before the standard TranslateMessage path.
// WebView2 uses add_AcceleratorKeyPressed (WV2_TranslateAccel is a no-op that
// returns false); MSHTML needs IOleInPlaceActiveObject::TranslateAccelerator.
bool WebAdmission_TranslateAccel(MSG* msg){
    if(!msg) return false;
    if(WV2_TranslateAccel(msg)) return true;             // (always false today)
    if(MshtmlAdmission_TranslateAccel(msg)) return true; // MSHTML consumed it
    return false;
}

#ifdef AZ_DEBUG_BUILD
// Headless diagnostics only: number of times the embedded page reached the
// `init` verb. Proves the bundled ES5 JS parsed & ran (a syntax error would
// stop the script before it ever called the bridge).
long WebAdmission_DebugInitHits(){ return (long)g_adInitHits; }

// Headless diagnostics only: the fully-inlined page for a given engine
// variant, so the probe can assert every asset actually made it inline.
std::string WebAdmission_DebugInlinePage(bool withFontFace){
    return AdmissionInlinePage(withFontFace);
}

// Headless diagnostics only: national-id of the most recent SUCCESSFUL
// patient.lookup. Empty until an Enter-on-#nid (or quick-search) actually
// reached the C++ store. Used by --smoke-admission-keys to prove the Enter key
// was routed into the page and triggered the lookup + auto-fill flow.
std::string WebAdmission_DebugLastFilledNid(){
    std::lock_guard<std::mutex> lk(g_adLastNidMx);
    return g_adLastFilledNid;
}
#endif
