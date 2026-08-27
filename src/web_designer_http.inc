// ============================================================================
//  web_designer_http.inc — loopback (127.0.0.1) HTTP host (Winsock).
//  Included into web_designer.cpp. Single accept thread, blocking I/O, tiny
//  request parser. Only serves the embedded designer + /api bridge. Loopback
//  only, so there is no network exposure.
// ============================================================================
namespace {

// shared state guarded by a CS — the section being edited + the design that the
// page last saved (so the host window's "apply" path can read it).
static CRITICAL_SECTION g_wdCs;
static bool   g_wdCsInit=false;
static int    g_wdPort=0;
static SOCKET g_wdListen=INVALID_SOCKET;
static HANDLE g_wdThread=NULL;
static int    g_wdSectionId=0;
static volatile LONG g_wdExitFlag=0;     // page asked to close
static PrintDesign g_wdLastSaved;        // last design the page persisted
static bool   g_wdHaveSaved=false;

static void wdLock(){ if(g_wdCsInit) EnterCriticalSection(&g_wdCs); }
static void wdUnlock(){ if(g_wdCsInit) LeaveCriticalSection(&g_wdCs); }

// map RCDATA id + content-type for a request path
static bool assetForPath(const std::string& path, int& id, const char*& ctype){
    std::string p=path;
    size_t q=p.find('?'); if(q!=std::string::npos) p=p.substr(0,q);
    if(p=="/"||p=="/index.html"){ id=300; ctype="text/html; charset=utf-8"; return true; }
    if(p=="/designer.css"){ id=301; ctype="text/css; charset=utf-8"; return true; }
    if(p=="/bootstrap.min.css"){ id=302; ctype="text/css; charset=utf-8"; return true; }
    if(p=="/fields.js"){ id=303; ctype="application/javascript; charset=utf-8"; return true; }
    if(p=="/templates.js"){ id=304; ctype="application/javascript; charset=utf-8"; return true; }
    if(p=="/designer.js"){ id=305; ctype="application/javascript; charset=utf-8"; return true; }
    return false;
}

static void sendAll(SOCKET s, const char* data, int len){
    int off=0; while(off<len){ int n=send(s,data+off,len-off,0); if(n<=0)break; off+=n; }
}
static void httpReply(SOCKET s, int code, const char* ctype, const std::string& body){
    const char* st=(code==200)?"OK":(code==404?"Not Found":(code==400?"Bad Request":"Error"));
    char hdr[512];
    int n=snprintf(hdr,sizeof(hdr),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n"
        "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
        code,st,ctype,(int)body.size());
    sendAll(s,hdr,n); if(!body.empty()) sendAll(s,body.data(),(int)body.size());
}

// ---- /api bridge -----------------------------------------------------------
//  Returns JSON. `verb` is the path after /api/. `bodyJson` is the POST body.
static std::string apiHandle(const std::string& verb, const std::string& bodyJson){
    if(verb=="init"){
        int sid; wdLock(); sid=g_wdSectionId; wdUnlock();
        PrintDesign d; bool ok=false;
        if(sid>0) ok=SectionDesign_Resolve(sid,d);
        if(!ok) ok=MachineDesign_Resolve(d);
        if(!ok){ std::vector<PrintDesign> b; Designs_Builtins(b); if(!b.empty()){ d=b[0]; ok=true; } }
        std::wstring secName;
        if(sid>0){ std::vector<Section> all; Sections_All(all);
            for(const auto& s:all) if(s.id==sid){ secName=s.name_fa; break; } }
        std::string o="{\"design\":"+(ok?designToWebJson(d):std::string("null"));
        o+=",\"sectionName\":"+jstr(secName)+"}";
        return o;
    }
    if(verb=="templates"){
        std::vector<PrintDesign> b; Designs_Builtins(b);
        std::string o="{\"templates\":[";
        for(size_t i=0;i<b.size();++i){ if(i)o+=","; o+=designToWebJson(b[i]); }
        o+="]}"; return o;
    }
    if(verb=="mydesigns"){
        // v1.22.0: "طراحی‌های من" — user-saved designs from the DB.
        std::vector<PrintDesign> u; Designs_User(u);
        std::string o="{\"designs\":[";
        for(size_t i=0;i<u.size();++i){ if(i)o+=","; o+=designToWebJson(u[i]); }
        o+="]}"; return o;
    }
    if(verb=="delete"){
        double idd=0; if(!jsonGetNumber(bodyJson,"id",idd) || idd<=0)
            return "{\"ok\":false,\"err\":\"no id\"}";
        int delId=(int)idd;
        std::vector<PrintDesign> b; Designs_Builtins(b);
        if(!b.empty() && b[0].id==delId)
            return "{\"ok\":false,\"err\":\"طرح پیش‌فرض قابل حذف نیست.\"}";
        bool ok=Designs_Delete(delId);
        return ok?"{\"ok\":true}":"{\"ok\":false,\"err\":\"delete failed\"}";
    }
    if(verb=="save"){
        std::string dj; if(!jsonGetObject(bodyJson,"design",dj))
            return "{\"ok\":false,\"err\":\"no design\"}";
        PrintDesign d; if(!webJsonToDesign(dj,d))
            return "{\"ok\":false,\"err\":\"bad json\"}";
        int sid; wdLock(); sid=g_wdSectionId; wdUnlock();
        // persist: insert if new/builtin, else update
        if(d.kind==L"builtin") d.kind=L"user";
        int id;
        if(d.id<=0){ id=Designs_Insert(d); d.id=id; }
        else { Designs_Update(d); id=d.id; }
        MachineDesign_Set(id);
        if(sid>0) SectionDesign_Set(sid,id);
        wdLock(); g_wdLastSaved=d; g_wdHaveSaved=true; wdUnlock();
        // notify the app so reception picks up the new design immediately
        if(g_hFrame) PostMessageW(g_hFrame,WM_APP_DESIGN_PUSHED,(WPARAM)id,0);
        char b[64]; snprintf(b,64,"{\"ok\":true,\"id\":%d}",id);
        return b;
    }
    if(verb=="ready"||verb=="exit"){
        if(verb=="exit") InterlockedExchange(&g_wdExitFlag,1);
        return "{\"ok\":true}";
    }
    // download / upload are handled by the host window (native file dialogs);
    // the page calls them but we just acknowledge here — the host polls flags.
    if(verb=="download"){
        std::string dj; if(jsonGetObject(bodyJson,"design",dj)){
            PrintDesign d; if(webJsonToDesign(dj,d)){ wdLock(); g_wdLastSaved=d; g_wdHaveSaved=true; wdUnlock(); }
        }
        return "{\"ok\":true}";
    }
    return "{\"ok\":false,\"err\":\"unknown verb\"}";
}

static DWORD WINAPI httpThread(LPVOID){
    for(;;){
        sockaddr_in ca; int cl=sizeof(ca);
        SOCKET c=accept(g_wdListen,(sockaddr*)&ca,&cl);
        if(c==INVALID_SOCKET){ if(InterlockedCompareExchange(&g_wdExitFlag,0,0)) break; Sleep(5); continue; }
        // read request (headers + optional body up to Content-Length)
        std::string req; char buf[2048]; int got;
        // read until we have headers
        size_t hdrEnd=std::string::npos;
        while((got=recv(c,buf,sizeof(buf),0))>0){
            req.append(buf,got);
            hdrEnd=req.find("\r\n\r\n");
            if(hdrEnd!=std::string::npos) break;
            if(req.size()>65536) break;
        }
        if(hdrEnd==std::string::npos){ closesocket(c); continue; }
        // parse method + path
        std::string method, path;
        { size_t sp1=req.find(' '); size_t sp2=(sp1==std::string::npos)?std::string::npos:req.find(' ',sp1+1);
          if(sp1!=std::string::npos&&sp2!=std::string::npos){ method=req.substr(0,sp1); path=req.substr(sp1+1,sp2-sp1-1); } }
        // content-length
        size_t clen=0;
        { size_t cp=req.find("Content-Length:"); if(cp==std::string::npos) cp=req.find("content-length:");
          if(cp!=std::string::npos){ clen=(size_t)atoi(req.c_str()+cp+15); } }
        std::string body=req.substr(hdrEnd+4);
        while(body.size()<clen && (got=recv(c,buf,sizeof(buf),0))>0) body.append(buf,got);
        if(body.size()>clen) body=body.substr(0,clen);

        if(path.compare(0,5,"/api/")==0){
            std::string verb=path.substr(5);
            size_t q=verb.find('?'); if(q!=std::string::npos) verb=verb.substr(0,q);
            std::string out=apiHandle(verb,body);
            httpReply(c,200,"application/json; charset=utf-8",out);
        } else {
            int id; const char* ctype;
            if(assetForPath(path,id,ctype)){
                std::string data;
                if(loadRes(id,data)) httpReply(c,200,ctype,data);
                else httpReply(c,404,"text/plain","not found");
            } else httpReply(c,404,"text/plain","not found");
        }
        closesocket(c);
        if(InterlockedCompareExchange(&g_wdExitFlag,0,0)) break;
    }
    return 0;
}

} // namespace

int WebDesigner_EnsureHost(){
    if(g_wdPort) return g_wdPort;
    if(!g_wdCsInit){ InitializeCriticalSection(&g_wdCs); g_wdCsInit=true; }
    WSADATA wsa; static bool wsaUp=false;
    if(!wsaUp){ if(WSAStartup(MAKEWORD(2,2),&wsa)!=0) return 0; wsaUp=true; }
    SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(s==INVALID_SOCKET) return 0;
    sockaddr_in a={}; a.sin_family=AF_INET;
    a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);   // 127.0.0.1 only
    a.sin_port=0;                               // ask OS for a free port
    if(bind(s,(sockaddr*)&a,sizeof(a))!=0){ closesocket(s); return 0; }
    int alen=sizeof(a);
    if(getsockname(s,(sockaddr*)&a,&alen)!=0){ closesocket(s); return 0; }
    if(listen(s,8)!=0){ closesocket(s); return 0; }
    g_wdListen=s; g_wdPort=ntohs(a.sin_port);
    g_wdThread=CreateThread(NULL,0,httpThread,NULL,0,NULL);
    return g_wdPort;
}

std::wstring WebDesigner_Url(int sectionId){
    int port=WebDesigner_EnsureHost(); if(!port) return L"";
    wdLock(); g_wdSectionId=sectionId; g_wdHaveSaved=false; InterlockedExchange(&g_wdExitFlag,0); wdUnlock();
    wchar_t b[96]; swprintf(b,96,L"http://127.0.0.1:%d/index.html?section=%d",port,sectionId);
    return b;
}
