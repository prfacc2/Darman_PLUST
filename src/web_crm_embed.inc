// ============================================================================
//  web_crm_embed.inc — SERVERLESS CRM page bundle (v1.70.0).
//  Included into web_crm.cpp (after web_crm_api.inc).
//
//  The management page (HTML/CSS/JS from RCDATA assets/crm) is fully INLINED
//  into one string and handed directly to the engine — WebView2 via
//  NavigateToString, MSHTML via about:blank + IHTMLDocument2::write. No sockets,
//  no ports, no loopback host: the page is attached to the program.
//
//  C++ <-> JS bridge is transport-native:
//    * WebView2 → chrome.webview.postMessage / PostWebMessageAsJson
//    * MSHTML   → window.external.azCall(verb, payloadJson, page) (synchronous,
//                 IDispatch on the UI thread) + execScript push events.
//
//  RCDATA ids (MUST match app.rc):
//    shell : 500 common.css · 501 common.js · 502 vazir.ttf
//    crm   : 800 index.html · 801 crm.css · 802 crm.js · 803 dashboard.js
//            804 sections.js · 805 patients.js · 806 doctors.js (v1.79: 808 =
//            accounts.js «تعریف حساب کاربری» — employees.js successor; 814 =
//            persons.js «تعریف پرسنل»)
//            807 services.js · 808 employees.js · 809 messages.js
//            810 settings.js · 811 insurance.js
//            812 crm_menu.js · 813 crm_dialogs.js  (v1.77 modules split from crm.js)
// ============================================================================
namespace {

// ---- RCDATA loader (shared by the inliner + any future embedded surface) ----
static bool crmLoadRes(int id, std::string& out){
    HRSRC h=FindResourceW(g_hInst,MAKEINTRESOURCEW(id),RT_RCDATA);
    if(!h) return false;
    HGLOBAL g=LoadResource(g_hInst,h); if(!g) return false;
    void* p=LockResource(g); DWORD sz=SizeofResource(g_hInst,h);
    if(!p||!sz) return false;
    out.assign((const char*)p,(size_t)sz); return true;
}

// diagnostics: how many times the loaded page actually reached the `crm.init`
// verb. Used by the host readiness loop to prove the bundled JS ran end to end
// (a broken script would never reach the bridge).
static volatile LONG g_crmInitHits=0;

// ---- tiny base64 (for the data: URI font face) ------------------------------
static std::string crmB64(const std::string& in){
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

// remove every @font-face{...} block (the url('vazir.ttf') sources are dead once
// the page no longer lives on a host; a single data: URI face is injected
// instead, with the process-wide AddFontMemResourceEx font as the in-process
// MSHTML fallback).
static bool crmStripFontFace(std::string& css){
    size_t from=0;
    for(;;){
        size_t p=css.find("@font-face",from);
        if(p==std::string::npos) return true;
        size_t ob=p+10;
        while(ob<css.size() && (css[ob]==' '||css[ob]=='\t'||css[ob]=='\r'||css[ob]=='\n')) ob++;
        if(ob>=css.size() || css[ob]!='{'){ from=p+10; continue; }
        size_t cb=css.find('}',ob); if(cb==std::string::npos) return false;
        css.erase(p,cb-p+1);
        from=p;
    }
}

// Every source marker is part of the bundle contract. Requiring exactly one
// occurrence prevents a renamed/missing asset (or an accidental duplicate) from
// producing a page that still contains dead relative URLs.
static bool crmReplaceRequired(std::string& hay, const std::string& needle,
                               const std::string& rep){
    size_t p=hay.find(needle);
    if(p==std::string::npos || hay.find(needle,p+needle.size())!=std::string::npos)
        return false;
    hay.replace(p,needle.size(),rep);
    return true;
}

// Build the single fully-inlined CRM page:
//   index.html (800) with common.css (500) + crm.css (801) inlined into
//   <style> blocks, and common.js (501) + crm.js (802) + crm_menu.js (812) +
//   crm_dialogs.js (813) + dashboard.js (803) + sections.js (804) +
//   patients.js (805) + doctors.js (806) + services.js (807) + employees.js
//   (808 = accounts.js «تعریف حساب کاربری», v1.79) + messages.js (809) +
//   settings.js (810) + insurance.js (811) + persons.js (814, «تعریف پرسنل»)
//   inlined into <script> blocks (in the same order as index.html).
//
//  Font strategy (withFontFace):
//    * WebView2 (true)  — runs OUT of process; embed vazir.ttf (502) as a
//      base64 data: URI @font-face.
//    * MSHTML (false)   — in-process Trident sees the memory font, so the
//      Trident page gets NO @font-face (a failing src would drop the family).
static std::string CrmBuildInlinePage(bool withFontFace){
    std::string html,commonCss,crmCss,commonJs,crmJs,menuJs,dlgJs,dashJs,secJs,patJs,docJs,svcJs,empJs,msgJs,setJs;
    // v1.79.0: id 808 now hosts accounts.js («تعریف حساب کاربری») — the
    // employees.js successor; the local var keeps its old name (empJs).
    if(!crmLoadRes(800,html)      || html.empty()      ||
       !crmLoadRes(500,commonCss) || commonCss.empty() ||
       !crmLoadRes(801,crmCss)    || crmCss.empty()    ||
       !crmLoadRes(501,commonJs)  || commonJs.empty()  ||
       !crmLoadRes(802,crmJs)     || crmJs.empty()     ||
       !crmLoadRes(812,menuJs)    || menuJs.empty()    ||
       !crmLoadRes(813,dlgJs)     || dlgJs.empty()     ||
       !crmLoadRes(803,dashJs)    || dashJs.empty()    ||
       !crmLoadRes(804,secJs)     || secJs.empty()     ||
       !crmLoadRes(805,patJs)     || patJs.empty()     ||
       !crmLoadRes(806,docJs)     || docJs.empty()     ||
       !crmLoadRes(807,svcJs)     || svcJs.empty()     ||
       !crmLoadRes(808,empJs)     || empJs.empty()     ||
       !crmLoadRes(809,msgJs)     || msgJs.empty()     ||
       !crmLoadRes(810,setJs)     || setJs.empty())
    {
      std::string insJs; if(!crmLoadRes(811,insJs) || insJs.empty()) return "";
      if(!crmReplaceRequired(html,"<script src=\"insurance.js\"></script>",
                          "<script>\n"+insJs+"\n</script>"))
        return "";
    }
    if(!crmStripFontFace(commonCss) || !crmStripFontFace(crmCss)) return "";

    std::string face;
    if(withFontFace){
        std::string font;
        if(!crmLoadRes(502,font) || font.empty()) return "";
        face="@font-face{font-family:'Vazirmatn';"
             "src:url(data:font/ttf;base64,"+crmB64(font)+") format('truetype');"
             "font-weight:normal;font-style:normal;}\n";
    }

    if(!crmReplaceRequired(html,"<link rel=\"stylesheet\" href=\"common.css\" />",
                          "<style>\n"+face+commonCss+"\n</style>") ||
       !crmReplaceRequired(html,"<link rel=\"stylesheet\" href=\"crm.css\" />",
                          "<style>\n"+crmCss+"\n</style>") ||
       !crmReplaceRequired(html,"<script src=\"common.js\"></script>",
                          "<script>\n"+commonJs+"\n</script>") ||
       !crmReplaceRequired(html,"<script src=\"crm.js\"></script>",
                          "<script>\n"+crmJs+"\n</script>") ||
       !crmReplaceRequired(html,"<script src=\"crm_menu.js\"></script>",
                          "<script>\n"+menuJs+"\n</script>") ||
       !crmReplaceRequired(html,"<script src=\"crm_dialogs.js\"></script>",
                          "<script>\n"+dlgJs+"\n</script>") ||
       !crmReplaceRequired(html,"<script src=\"dashboard.js\"></script>",
                          "<script>\n"+dashJs+"\n</script>") ||
       !crmReplaceRequired(html,"<script src=\"sections.js\"></script>",
                          "<script>\n"+secJs+"\n</script>") ||
       !crmReplaceRequired(html,"<script src=\"patients.js\"></script>",
                          "<script>\n"+patJs+"\n</script>") ||
       !crmReplaceRequired(html,"<script src=\"doctors.js\"></script>",
                          "<script>\n"+docJs+"\n</script>") ||
       !crmReplaceRequired(html,"<script src=\"services.js\"></script>",
                          "<script>\n"+svcJs+"\n</script>") ||
       !crmReplaceRequired(html,"<script src=\"accounts.js\"></script>",
                          "<script>\n"+empJs+"\n</script>") ||
       !crmReplaceRequired(html,"<script src=\"messages.js\"></script>",
                          "<script>\n"+msgJs+"\n</script>") ||
       !crmReplaceRequired(html,"<script src=\"settings.js\"></script>",
                          "<script>\n"+setJs+"\n</script>"))
        return "";
    // v1.75.0: insurance.js (RCDATA 811) — was missing from the bundle
    {
      std::string insJs; if(!crmLoadRes(811,insJs) || insJs.empty()) return "";
      if(!crmReplaceRequired(html,"<script src=\"insurance.js\"></script>",
                          "<script>\n"+insJs+"\n</script>"))
        return "";
    }
    // v1.79.0: persons.js (RCDATA 814) — the «تعریف پرسنل» registry page
    {
      std::string perJs; if(!crmLoadRes(814,perJs) || perJs.empty()) return "";
      if(!crmReplaceRequired(html,"<script src=\"persons.js\"></script>",
                          "<script>\n"+perJs+"\n</script>"))
        return "";
    }
    // v1.82.0: calendar.js (815) + backup.js (816)
    {
      std::string calJs; if(!crmLoadRes(815,calJs) || calJs.empty()) return "";
      if(!crmReplaceRequired(html,"<script src=\"calendar.js\"></script>",
                          "<script>\n"+calJs+"\n</script>"))
        return "";
    }
    {
      std::string bkJs; if(!crmLoadRes(816,bkJs) || bkJs.empty()) return "";
      if(!crmReplaceRequired(html,"<script src=\"backup.js\"></script>",
                          "<script>\n"+bkJs+"\n</script>"))
        return "";
    }
    return html;
}

// Cached page variants (built once — at startup via WebCrm_Prepare, or lazily on
// the first view). Immutable after the first build.
//   [0] = MSHTML variant (no @font-face)   [1] = WebView2 variant (data: URI)
static std::mutex g_crmPageMx;
static std::string g_crmPageCache[2];
static const std::string& CrmInlinePage(bool withFontFace){
    int i=withFontFace?1:0;
    std::lock_guard<std::mutex> lk(g_crmPageMx);
    if(g_crmPageCache[i].empty()) g_crmPageCache[i]=CrmBuildInlinePage(withFontFace);
    return g_crmPageCache[i];
}

// ---- serverless verb dispatch (both engines route here) ---------------------
//  Runs on the UI thread for both engines (WebMessageReceived + IDispatch::Invoke
//  are delivered there); the api layer's RunOnUiThreadSync inlines safely then.
static std::string CrmDispatchVerb(const std::string& verb,
                                   const std::string& body,
                                   const std::string& page){
    (void)page;
    if(verb=="crm.init"||verb=="init") InterlockedIncrement(&g_crmInitHits);
    if(verb=="bridge.probe") return "{\"ok\":true}";     // transport handshake
    if(verb=="poll")         return "{\"events\":[]}";   // legacy poller safety
    return crmApi(verb,body);
}

} // namespace
