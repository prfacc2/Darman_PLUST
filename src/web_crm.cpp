// ============================================================================
//  web_crm.cpp — embedded CRM Management surface (v1.70.0 SERVERLESS).
//
//  * The management page (HTML/CSS/JS from RCDATA assets/crm) is fully INLINED
//    into one string at startup and handed directly to the engine — WebView2 via
//    NavigateToString, MSHTML via about:blank + IHTMLDocument2::write. There is
//    NO local server, NO loopback socket and NO port: the page is attached to
//    the program, so it can never show "Can't reach this page".
//  * C++ <-> JS is bridged BOTH ways, transport-native per engine:
//      WebView2 : chrome.webview.postMessage  /  PostWebMessageAsJson
//      MSHTML   : window.external.azCall(...)  /  execScript push (azShellReceive)
//  * Graceful fallback: if WebView2 is unavailable, the universal MSHTML
//    WebBrowser control (every Windows + Wine) renders the same page inside the
//    app. The legacy native GDI manage panel (src/manage.inc) is DISABLED, not
//    deleted — its createManageScreen() body is preserved under #if 0.
//  * Offline-first: assets are embedded, the API reads/writes the local stores
//    (sections.dat, services.dat, doctors.dat, patients.dat, users.dat, …) so
//    the on-disk file formats are preserved exactly.
// ============================================================================
#include "app.h"
#include "web_crm.h"
#include "web_thread_pool.h"    // RunOnUiThreadSync (UI-thread marshalling)
#include "print_designer.h"     // PrintCfg_Open (print-settings modal)
#include "clinic_ops.h"
#include <shlobj.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <algorithm>

// WebView2 COM interface declarations (shared with web_admission.cpp — the
// symbols are internal-linkage statics, so the two TUs never collide).
#include "web_admission_webview2.inc"

extern HINSTANCE g_hInst;

// ----------------------------------------------------------------------------
//  UTF helpers + tiny JSON helpers (self-contained, mirror web_admission.cpp)
// ----------------------------------------------------------------------------
static std::string w2u8(const std::wstring& w){
    if(w.empty()) return "";
    int n=WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),NULL,0,NULL,NULL);
    std::string s(n,0); WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),&s[0],n,NULL,NULL);
    return s;
}
static std::wstring u82w(const std::string& s){
    if(s.empty()) return L"";
    int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),NULL,0);
    std::wstring w(n,0); MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),&w[0],n);
    return w;
}
// JSON string-escape a wide string -> "..."
static std::string jstr(const std::wstring& w){
    std::string u=w2u8(w); std::string o="\"";
    for(unsigned char c: u){
        switch(c){
            case '"': o+="\\\""; break;
            case '\\':o+="\\\\"; break;
            case '\n':o+="\\n"; break;
            case '\r':o+="\\r"; break;
            case '\t':o+="\\t"; break;
            default:
                if(c<0x20){ char b[8]; sprintf(b,"\\u%04x",c); o+=b; }
                else o+=(char)c;
        }
    }
    o+="\""; return o;
}
static std::string jnum(long long v){ char b[32]; sprintf(b,"%lld",v); return b; }

// find a top-level string value for key (very small, tolerant parser)
static bool jsonGetString(const std::string& j, const std::string& key, std::string& out){
    std::string pat="\""+key+"\"";
    size_t p=j.find(pat); if(p==std::string::npos) return false;
    p=j.find(':',p+pat.size()); if(p==std::string::npos) return false;
    p++; while(p<j.size()&&(j[p]==' '||j[p]=='\t')) p++;
    if(p>=j.size()||j[p]!='"') return false;
    p++; std::string s;
    while(p<j.size()){
        char c=j[p++];
        if(c=='\\'&&p<j.size()){ char e=j[p++];
            switch(e){ case 'n':s+='\n';break; case 'r':s+='\r';break; case 't':s+='\t';break;
                case '"':s+='"';break; case '\\':s+='\\';break; case '/':s+='/';break;
                case 'u':{ if(p+4<=j.size()){ int cp=(int)strtol(j.substr(p,4).c_str(),NULL,16); p+=4;
                    if(cp<0x80) s+=(char)cp;
                    else if(cp<0x800){ s+=(char)(0xC0|(cp>>6)); s+=(char)(0x80|(cp&0x3F)); }
                    else { s+=(char)(0xE0|(cp>>12)); s+=(char)(0x80|((cp>>6)&0x3F)); s+=(char)(0x80|(cp&0x3F)); } } break; }
                default: s+=e; }
        } else if(c=='"') break;
        else s+=c;
    }
    out=s; return true;
}
static bool jsonGetNumber(const std::string& j, const std::string& key, double& out){
    std::string pat="\""+key+"\"";
    size_t p=j.find(pat); if(p==std::string::npos) return false;
    p=j.find(':',p+pat.size()); if(p==std::string::npos) return false;
    p++; while(p<j.size()&&(j[p]==' '||j[p]=='\t')) p++;
    char* end=NULL; out=strtod(j.c_str()+p,&end);
    return end!=j.c_str()+p;
}
static bool jsonGetBool(const std::string& j, const std::string& key, bool def){
    std::string pat="\""+key+"\"";
    size_t p=j.find(pat); if(p==std::string::npos) return def;
    p=j.find(':',p+pat.size()); if(p==std::string::npos) return def;
    p++; while(p<j.size()&&(j[p]==' '||j[p]=='\t')) p++;
    return j.compare(p,4,"true")==0;
}
// extract the raw substring of a nested object/array value for `key`
static bool jsonGetRaw(const std::string& j, const std::string& key, std::string& out){
    std::string pat="\""+key+"\"";
    size_t p=j.find(pat); if(p==std::string::npos) return false;
    p=j.find(':',p+pat.size()); if(p==std::string::npos) return false;
    p++; while(p<j.size()&&(j[p]==' '||j[p]=='\t')) p++;
    if(p>=j.size()) return false;
    char open=j[p], close=0;
    if(open=='{') close='}'; else if(open=='[') close=']'; else return false;
    int depth=0; bool inStr=false; size_t start=p;
    for(;p<j.size();++p){
        char c=j[p];
        if(inStr){ if(c=='\\'){p++;continue;} if(c=='"') inStr=false; continue; }
        if(c=='"'){ inStr=true; continue; }
        if(c==open) depth++;
        else if(c==close){ depth--; if(depth==0){ out=j.substr(start,p-start+1); return true; } }
    }
    return false;
}

static std::wstring normDigits(const std::wstring& in){
    std::wstring o; for(wchar_t c:in){ if(c>=L'۰'&&c<=L'۹') o+=(wchar_t)(L'0'+(c-L'۰')); else o+=c; }
    return o;
}

// case-insensitive contains for wide strings (Persian is unaffected)
static bool wcontains(const std::wstring& hay, const std::wstring& needle){
    if(needle.empty()) return true;
    auto lower=[](std::wstring s){ for(auto&c:s) c=(wchar_t)towlower(c); return s; };
    return lower(hay).find(lower(needle))!=std::wstring::npos;
}

#include "web_crm_api.inc"
#include "web_crm_embed.inc"
#include "web_crm_host.inc"
#include "web_crm_mshtml.inc"
#include "web_crm_dispatch.inc"
