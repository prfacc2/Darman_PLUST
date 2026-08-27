// ============================================================================
//  web_admission_embed.inc — SERVERLESS admission page bundle (v1.66.0).
//  Included into web_admission.cpp (after web_admission_api.inc).
//
//  v1.66.0 removes the loopback (127.0.0.1) HTTP host entirely. Machines with
//  aggressive firewalls / proxies / IE "work offline" flags showed
//  "Can't reach this page http://127.0.0.1:<port>" INSIDE the app, so the
//  admission surface now ships as ONE fully-inlined HTML string built from the
//  same RCDATA assets and handed DIRECTLY to the engine:
//    * WebView2 → ICoreWebView2::NavigateToString(page)
//    * MSHTML   → about:blank + IHTMLDocument2::write(page)
//  No sockets, no ports, no loopback — the page is attached to the program.
//
//  The C++ <-> JS bridge is transport-native as well:
//    * WebView2 → chrome.webview.postMessage / PostWebMessageAsJson (unchanged)
//    * MSHTML   → window.external.azCall(verb, payloadJson, page) (synchronous,
//                 IDispatch on the UI thread) + execScript push events.
// ============================================================================
namespace {

// ---- RCDATA loader (shared by the inliner + any future embedded surface) ----
static bool adLoadRes(int id, std::string& out){
    HRSRC h=FindResourceW(g_hInst,MAKEINTRESOURCEW(id),RT_RCDATA);
    if(!h) return false;
    HGLOBAL g=LoadResource(g_hInst,h); if(!g) return false;
    void* p=LockResource(g); DWORD sz=SizeofResource(g_hInst,h);
    if(!p||!sz) return false;
    out.assign((const char*)p,(size_t)sz); return true;
}

// diagnostics: how many times the loaded page has actually reached the `init`
// verb. Used by the headless admission probes to prove the embedded JS ran
// without a syntax error (a broken script would never reach the bridge).
static volatile LONG g_adInitHits=0;

// ---- tiny base64 (for the data: URI font face) ------------------------------
static std::string adB64(const std::string& in){
    static const char* T="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o; o.reserve(((in.size()+2)/3)*4);
    size_t i=0;
    while(i+2<in.size()){
        unsigned v=(((unsigned)(unsigned char)in[i])<<16)|
                   (((unsigned)(unsigned char)in[i+1])<<8)|
                    ((unsigned)(unsigned char)in[i+2]);
        o+=T[(v>>18)&63]; o+=T[(v>>12)&63]; o+=T[(v>>6)&63]; o+=T[v&63]; i+=3;
    }
    if(i+1==in.size()){
        unsigned v=((unsigned)(unsigned char)in[i])<<16;
        o+=T[(v>>18)&63]; o+=T[(v>>12)&63]; o+="==";
    } else if(i+2==in.size()){
        unsigned v=(((unsigned)(unsigned char)in[i])<<16)|
                   (((unsigned)(unsigned char)in[i+1])<<8);
        o+=T[(v>>18)&63]; o+=T[(v>>12)&63]; o+=T[(v>>6)&63]; o+='=';
    }
    return o;
}

// remove every @font-face{...} block (the url('vazir.ttf') sources are dead
// once the page no longer lives on the loopback host; a single data: URI face
// is injected instead, with the process-wide AddFontMemResourceEx font as the
// in-process MSHTML fallback).
static bool adStripFontFace(std::string& css){
    size_t from=0;
    for(;;){
        size_t p=css.find("@font-face",from);
        if(p==std::string::npos) return true;
        // only strip a real rule: "@font-face" + optional whitespace + "{"
        // (the string also appears inside header comments — leave those alone).
        size_t ob=p+10;
        while(ob<css.size() && (css[ob]==' '||css[ob]=='\t'||css[ob]=='\r'||css[ob]=='\n')) ob++;
        if(ob>=css.size() || css[ob]!='{'){ from=p+10; continue; }
        size_t cb=css.find('}',ob); if(cb==std::string::npos) return false;
        css.erase(p,cb-p+1);
        from=p;
    }
}

// Every source marker is part of the bundle contract. Requiring exactly one
// occurrence prevents a renamed/missing asset (or an accidental duplicate)
// from producing a page that still contains dead relative URLs.
static bool adReplaceRequired(std::string& hay, const std::string& needle,
                              const std::string& rep){
    size_t p=hay.find(needle);
    if(p==std::string::npos || hay.find(needle,p+needle.size())!=std::string::npos)
        return false;
    hay.replace(p,needle.size(),rep);
    return true;
}

// Build the single fully-inlined admission page:
//   index.html (400) with
//     common.css (500) + admission.css (401) inlined into <style> blocks
//     common.js (501) / bridge.js (402) / contextmenu.js (405) /
//     admission.js (403) inlined into <script> blocks
//
//  Font strategy (withFontFace):
//    * WebView2 (true)  — runs OUT of process, so it cannot see the
//      AddFontMemResourceEx font the app installs; embed vazir.ttf (404) as a
//      base64 data: URI @font-face (~164KB base64, well under the 2MB
//      NavigateToString limit).
//    * MSHTML (false)   — in-process Trident DOES see the memory font, and a
//      @font-face whose src fails would make IE treat the whole family as
//      unavailable (falling through to Segoe/Tahoma). So the Trident page gets
//      NO @font-face at all; plain font-family:'Vazirmatn' resolves to the
//      process-installed font.
static std::string AdmissionBuildInlinePage(bool withFontFace){
    std::string html,commonCss,admCss,commonJs,bridgeJs,ctxJs,admJs;
    if(!adLoadRes(400,html)      || html.empty()      ||
       !adLoadRes(500,commonCss) || commonCss.empty() ||
       !adLoadRes(401,admCss)    || admCss.empty()    ||
       !adLoadRes(501,commonJs)  || commonJs.empty()  ||
       !adLoadRes(402,bridgeJs)  || bridgeJs.empty()  ||
       !adLoadRes(405,ctxJs)     || ctxJs.empty()     ||
       !adLoadRes(403,admJs)     || admJs.empty())
        return "";
    if(!adStripFontFace(commonCss) || !adStripFontFace(admCss)) return "";

    std::string face;
    if(withFontFace){
        std::string font;
        if(!adLoadRes(404,font) || font.empty()) return "";
        face="@font-face{font-family:'Vazirmatn';"
             "src:url(data:font/ttf;base64,"+adB64(font)+") format('truetype');"
             "font-weight:normal;font-style:normal;}\n";
    }

    if(!adReplaceRequired(html,"<link rel=\"stylesheet\" href=\"common.css\" />",
                          "<style>\n"+face+commonCss+"\n</style>") ||
       !adReplaceRequired(html,"<link rel=\"stylesheet\" href=\"admission.css\" />",
                          "<style>\n"+admCss+"\n</style>") ||
       !adReplaceRequired(html,"<script src=\"common.js\"></script>",
                          "<script>\n"+commonJs+"\n</script>") ||
       !adReplaceRequired(html,"<script src=\"bridge.js\"></script>",
                          "<script>\n"+bridgeJs+"\n</script>") ||
       !adReplaceRequired(html,"<script src=\"contextmenu.js\"></script>",
                          "<script>\n"+ctxJs+"\n</script>") ||
       !adReplaceRequired(html,"<script src=\"admission.js\"></script>",
                          "<script>\n"+admJs+"\n</script>"))
        return "";
    return html;
}

// Cached page variants (built once — at startup via WebAdmission_Prepare, or
// lazily on the first view). Immutable after the first build.
//   [0] = MSHTML variant (no @font-face)   [1] = WebView2 variant (data: URI)
static std::mutex g_adPageMx;
static std::string g_adPageCache[2];
static const std::string& AdmissionInlinePage(bool withFontFace){
    int i=withFontFace?1:0;
    std::lock_guard<std::mutex> lk(g_adPageMx);
    if(g_adPageCache[i].empty()) g_adPageCache[i]=AdmissionBuildInlinePage(withFontFace);
    return g_adPageCache[i];
}

// v1.84.0: WebView2 NavigateToString is async — bind surface/ticket to the
// host HWND so the later navigation still sees them. A process-wide pending
// slot is only used between CreateViewEx and CreateWindow.
static std::mutex g_adPendingMx;
static std::string g_adPendingSurf="admission";
static std::string g_adPendingTicket;
static std::map<HWND,std::pair<std::string,std::string> > g_adInject;

static void adSetPendingView(const char* surface, const std::string& ticket){
    std::lock_guard<std::mutex> lk(g_adPendingMx);
    g_adPendingSurf=(surface && *surface)?surface:"admission";
    g_adPendingTicket=ticket;
}
static void adBindPendingView(HWND host){
    if(!host) return;
    std::lock_guard<std::mutex> lk(g_adPendingMx);
    g_adInject[host]=std::make_pair(g_adPendingSurf,g_adPendingTicket);
}
static void adClearPendingView(){
    std::lock_guard<std::mutex> lk(g_adPendingMx);
    g_adPendingSurf="admission";
    g_adPendingTicket.clear();
}
static void adForgetHost(HWND host){
    if(!host) return;
    std::lock_guard<std::mutex> lk(g_adPendingMx);
    g_adInject.erase(host);
}
static std::string AdmissionPageForHost(bool withFont, HWND host){
    std::string html=AdmissionInlinePage(withFont);
    if(html.empty()) return html;
    std::string surf="admission";
    std::string ticket;
    {
        std::lock_guard<std::mutex> lk(g_adPendingMx);
        auto it=g_adInject.find(host);
        if(it!=g_adInject.end()){ surf=it->second.first; ticket=it->second.second; }
    }
    if(surf!="tools" && surf!="cashier" && surf!="queue" && surf!="receipts"
       && surf!="dash" && surf!="portal" && surf!="blacklist"
       && surf!="svreport") surf="admission";  // v2.01: svreport
    if(ticket.find("</")!=std::string::npos) ticket.clear();
    std::string inj="<script>window.__azSurface=\"";
    inj+=surf;
    inj+="\";window.__azTicket=";
    inj+=ticket.empty()?"null":ticket;
    inj+=";</script>";
    size_t b=html.find("<body");
    if(b==std::string::npos) return html;
    size_t e=html.find('>',b);
    if(e==std::string::npos) return html;
    html.insert(e+1,inj);
    return html;
}

// ---- serverless verb dispatch (both engines route here) ---------------------
//  Order matches the old HTTP handler: generic multi-page registry first
//  (ping / client.log / client.metrics), then the legacy admission handler, so
//  every existing verb keeps working. Runs on the UI thread for both engines
//  (WebMessageReceived + IDispatch::Invoke are delivered there); the api layer
//  already RunOnUiThreadSync-inlines safely in that case.
static std::string AdmissionDispatchVerb(const std::string& verb,
                                         const std::string& body,
                                         const std::string& page){
    if(verb=="init") InterlockedIncrement(&g_adInitHits);
    if(verb=="bridge.probe") return "{\"ok\":true}";     // transport handshake
    if(verb=="poll")         return "{\"events\":[]}";   // legacy poller safety
    std::string out;
    if(!WebPages_DispatchVerb(verb,body,page,out)) out=admissionApi(verb,body);
    return out;
}

} // namespace
