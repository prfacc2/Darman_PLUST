// ============================================================================
//  print_designer.cpp — implementation of the vector print designer
//  Part 1: data model, JSON, store and built-in templates.
//  Part 2 (UI: section picker + editor + restore) follows.
// ============================================================================
#include "app.h"
#include "print_designer.h"
#include "web_designer.h"      // §1.19.0: HTML/CSS/JS designer host
#include "sections.h"
#include "ui_kit.h"
#include "undo.h"
#include <objbase.h>
#include <olectl.h>
#include <gdiplus.h>
#include <algorithm>
#include <sstream>
#include <map>
#include <csetjmp>

// ============================================================================
//  defaults
// ============================================================================
PrintItem::PrintItem()
    : id(0), type(PIT_LABEL), x(10), y(10), w(40), h(8), rot(0),
      locked(false), is_frame(false), z(0),
      fontName(L"Vazirmatn"), fontPt(10), bold(false), italic(false),
      align(0), dir(0), valign(0), lineSpacing(1.0),
      textColor(0x000000), fillColor(0xFFFFFF), fillTransparent(true),
      borderColor(0x000000), borderWidth(0), corner(0), padding(1),
      opacity(1.0), visibility(0),
      objectFit(0),
      // v1.55.0: 0 == automatic row geometry (backward compatible: an old
      // design that never stored rowH/headerH keeps its previous rendering).
      rowH(0), headerH(0) {}

PrintDesign::PrintDesign()
    : id(0), kind(L"user"), paper(L"A5"), paperW(148), paperH(210),
      orientation(0),
      // v2.01 (Part E1): 0/0 = authored size unknown — reflowDesign() adopts
      // the current paper size on first use (no scaling) so legacy and
      // freshly-built template layouts are never rescaled spuriously.
      baseW(0), baseH(0) {}

// ============================================================================
//  v2.01 (Part E1) — responsive reflow. Proportionally re-scales all item
//  positions and sizes when the paper size changes: x/w by newW/oldW, y/h by
//  newH/oldH, then clamps each item inside the printable area. baseW/baseH
//  track the paper size the current layout was authored for and follow the
//  paper after every reflow.
// ============================================================================
void reflowDesign(PrintDesign& d){
    if(d.paperW<=0 || d.paperH<=0) return;
    if(d.baseW<=0 || d.baseH<=0){           // legacy/unknown base: adopt, no scale
        d.baseW=d.paperW; d.baseH=d.paperH; return;
    }
    if(d.baseW==d.paperW && d.baseH==d.paperH) return;   // no size change
    double sx=d.paperW/d.baseW, sy=d.paperH/d.baseH;
    for(auto& it : d.items){
        it.x*=sx; it.w*=sx;
        it.y*=sy; it.h*=sy;
        if(it.w<1) it.w=1;
        if(it.h<1) it.h=1;
        // clamp within the printable area (keep the whole item on the page)
        if(it.w>d.paperW) it.w=d.paperW;
        if(it.h>d.paperH) it.h=d.paperH;
        if(it.x<0) it.x=0;
        if(it.y<0) it.y=0;
        if(it.x+it.w>d.paperW) it.x=d.paperW-it.w;
        if(it.y+it.h>d.paperH) it.y=d.paperH-it.h;
    }
    d.baseW=d.paperW; d.baseH=d.paperH;
}

// ============================================================================
//  paper presets (portrait mm)
// ============================================================================
bool Paper_Dims(const std::wstring& name, double& wmm, double& hmm){
    struct P { const wchar_t* n; double w,h; };
    static const P ps[] = {
        { L"A3", 297, 420 }, { L"A4", 210, 297 }, { L"A5", 148, 210 },
        { L"A6", 105, 148 }, { L"A7", 74, 105 },
        { L"B4", 250, 353 }, { L"B5", 176, 250 }, { L"B6", 125, 176 },
        { L"Letter", 215.9, 279.4 }, { L"Legal", 215.9, 355.6 },
        { L"HalfLetter", 139.7, 215.9 },
        // thermal rolls (width × default cut length) shared with JS designer.
        { L"R80", 80, 200 }, { L"R80L", 80, 297 }, { L"R72", 72, 200 },
        { L"R58", 58, 200 }, { L"R58L", 58, 297 }, { L"R44", 44, 150 },
        // small / laser label sizes shared with the JS designer.
        { L"L90", 90, 130 }, { L"L100", 100, 150 },
        { L"L102", 102, 152 }, { L"L130", 130, 180 },
    };
    for(const auto& p : ps) if(name==p.n){ wmm=p.w; hmm=p.h; return true; }
    return false;   // custom
}

// ============================================================================
//  minimal JSON serializer / parser (self-contained, designer-only)
// ============================================================================
static std::string js_esc(const std::wstring& s){
    // UTF-8 + JSON escaping
    std::string out;
    int len = WideCharToMultiByte(CP_UTF8,0,s.c_str(),(int)s.size(),NULL,0,NULL,NULL);
    std::string u8(len,0);
    if(len) WideCharToMultiByte(CP_UTF8,0,s.c_str(),(int)s.size(),&u8[0],len,NULL,NULL);
    for(char c : u8){
        switch(c){
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}
static std::wstring js_utf8_to_w(const std::string& u8){
    int len = MultiByteToWideChar(CP_UTF8,0,u8.c_str(),(int)u8.size(),NULL,0);
    std::wstring w(len,0);
    if(len) MultiByteToWideChar(CP_UTF8,0,u8.c_str(),(int)u8.size(),&w[0],len);
    return w;
}
static std::string num(double v){
    std::ostringstream o; o<<v; return o.str();
}
static std::string inum(long long v){ std::ostringstream o; o<<v; return o.str(); }

std::string Design_ToJson(const PrintDesign& d){
    std::ostringstream o;
    o << "{\"magic\":\"AZTEMPLATE/1\",";
    o << "\"name\":\"" << js_esc(d.name) << "\",";
    o << "\"kind\":\"" << js_esc(d.kind) << "\",";
    o << "\"paper\":\"" << js_esc(d.paper) << "\",";
    o << "\"paperW\":" << num(d.paperW) << ",";
    o << "\"paperH\":" << num(d.paperH) << ",";
    // v2.01 (Part E1): authored-for paper size (reflow base). Older parsers
    // skip unknown keys; older files simply lack them (base stays 0 = adopt).
    if(d.baseW>0 && d.baseH>0){
        o << "\"baseW\":" << num(d.baseW) << ",";
        o << "\"baseH\":" << num(d.baseH) << ",";
    }
    o << "\"orientation\":" << inum(d.orientation) << ",";
    o << "\"items\":[";
    for(size_t i=0;i<d.items.size();++i){
        const PrintItem& it=d.items[i];
        if(i) o << ",";
        o << "{";
        o << "\"id\":" << inum(it.id) << ",";
        o << "\"type\":" << inum(it.type) << ",";
        o << "\"x\":" << num(it.x) << ",\"y\":" << num(it.y) << ",";
        o << "\"w\":" << num(it.w) << ",\"h\":" << num(it.h) << ",";
        o << "\"rot\":" << num(it.rot) << ",";
        o << "\"locked\":" << (it.locked?1:0) << ",";
        o << "\"frame\":" << (it.is_frame?1:0) << ",";
        o << "\"z\":" << inum(it.z) << ",";
        o << "\"text\":\"" << js_esc(it.text) << "\",";
        o << "\"field\":\"" << js_esc(it.field) << "\",";
        o << "\"prefix\":\"" << js_esc(it.prefix) << "\",";
        o << "\"suffix\":\"" << js_esc(it.suffix) << "\",";
        o << "\"fmt\":\"" << js_esc(it.fmt) << "\",";
        o << "\"font\":\"" << js_esc(it.fontName) << "\",";
        o << "\"pt\":" << num(it.fontPt) << ",";
        o << "\"bold\":" << (it.bold?1:0) << ",";
        o << "\"italic\":" << (it.italic?1:0) << ",";
        o << "\"align\":" << inum(it.align) << ",";
        o << "\"dir\":" << inum(it.dir) << ",";
        o << "\"valign\":" << inum(it.valign) << ",";
        o << "\"fit\":" << inum(it.objectFit) << ",";
        o << "\"ls\":" << num(it.lineSpacing) << ",";
        o << "\"tc\":" << inum(it.textColor) << ",";
        o << "\"fc\":" << inum(it.fillColor) << ",";
        o << "\"ft\":" << (it.fillTransparent?1:0) << ",";
        o << "\"bc\":" << inum(it.borderColor) << ",";
        o << "\"bw\":" << num(it.borderWidth) << ",";
        o << "\"corner\":" << num(it.corner) << ",";
        o << "\"pad\":" << num(it.padding) << ",";
        o << "\"op\":" << num(it.opacity) << ",";
        o << "\"vis\":" << inum(it.visibility) << ",";
        o << "\"img\":\"" << js_esc(it.imgPath) << "\"";
        // v1.55.0: only emit the row-geometry keys when they were actually set.
        if(it.rowH    > 0) o << ",\"rh\":" << num(it.rowH);
        if(it.headerH > 0) o << ",\"hh\":" << num(it.headerH);
        o << "}";
    }
    o << "]}";
    return o.str();
}

// --- tiny tolerant JSON reader (objects/arrays/strings/numbers/bools) -------
namespace {
struct JParser {
    const std::string& s; size_t p; bool ok;
    JParser(const std::string& str):s(str),p(0),ok(true){}
    void ws(){ while(p<s.size() && (s[p]==' '||s[p]=='\t'||s[p]=='\n'||s[p]=='\r')) ++p; }
    bool match(char c){ ws(); if(p<s.size()&&s[p]==c){++p;return true;} return false; }
    std::string str(){
        ws(); std::string out;
        if(p>=s.size()||s[p]!='"'){ ok=false; return out; }
        ++p;
        while(p<s.size() && s[p]!='"'){
            char c=s[p++];
            if(c=='\\' && p<s.size()){
                char e=s[p++];
                switch(e){ case 'n':out+='\n';break; case 'r':out+='\r';break;
                    case 't':out+='\t';break; case '"':out+='"';break;
                    case '\\':out+='\\';break; case '/':out+='/';break;
                    default: out+=e; }
            } else out+=c;
        }
        if(p<s.size()&&s[p]=='"') ++p; else ok=false;
        return out;
    }
    double dbl(){
        ws(); size_t st=p;
        while(p<s.size() && (isdigit((unsigned char)s[p])||s[p]=='-'||s[p]=='+'||s[p]=='.'||s[p]=='e'||s[p]=='E')) ++p;
        if(p==st){ ok=false; return 0; }
        return atof(s.substr(st,p-st).c_str());
    }
    // skip a whole value (for unknown keys)
    void skipValue(){
        ws(); if(p>=s.size()) return;
        char c=s[p];
        if(c=='"'){ str(); }
        else if(c=='{'){ skipObj(); }
        else if(c=='['){ skipArr(); }
        else { while(p<s.size() && s[p]!=','&&s[p]!='}'&&s[p]!=']') ++p; }
    }
    void skipObj(){ match('{'); while(true){ ws(); if(match('}'))break; str(); match(':'); skipValue(); if(!match(',')){ match('}'); break; } if(!ok)break; } }
    void skipArr(){ match('['); while(true){ ws(); if(match(']'))break; skipValue(); if(!match(',')){ match(']'); break; } if(!ok)break; } }
};
} // namespace

bool Design_FromJson(const std::string& json, PrintDesign& out, std::wstring& err){
    err.clear();
    if(json.find("AZTEMPLATE/1")==std::string::npos){
        err=L"\u0633\u0631\u0622\u0645\u062f \u0641\u0627\u06cc\u0644 \u0646\u0627\u0645\u0639\u062a\u0628\u0631 (AZTEMPLATE/1 \u06cc\u0627\u0641\u062a \u0646\u0634\u062f)";
        return false;
    }
    JParser jp(json);
    out = PrintDesign();
    if(!jp.match('{')){ err=L"JSON \u0646\u0627\u0645\u0639\u062a\u0628\u0631"; return false; }
    while(true){
        jp.ws(); if(jp.match('}')) break;
        std::string key=jp.str(); jp.match(':');
        if(key=="name") out.name=js_utf8_to_w(jp.str());
        else if(key=="kind") out.kind=js_utf8_to_w(jp.str());
        else if(key=="paper") out.paper=js_utf8_to_w(jp.str());
        else if(key=="paperW") out.paperW=jp.dbl();
        else if(key=="paperH") out.paperH=jp.dbl();
        else if(key=="baseW") out.baseW=jp.dbl();   // v2.01 (Part E1)
        else if(key=="baseH") out.baseH=jp.dbl();   // v2.01 (Part E1)
        else if(key=="orientation") out.orientation=(int)jp.dbl();
        else if(key=="items"){
            jp.match('[');
            while(true){
                jp.ws(); if(jp.match(']')) break;
                PrintItem it;
                if(!jp.match('{')){ err=L"items \u0646\u0627\u0645\u0639\u062a\u0628\u0631"; return false; }
                while(true){
                    jp.ws(); if(jp.match('}')) break;
                    std::string k=jp.str(); jp.match(':');
                    if(k=="id") it.id=(int)jp.dbl();
                    else if(k=="type") it.type=(int)jp.dbl();
                    else if(k=="x") it.x=jp.dbl();
                    else if(k=="y") it.y=jp.dbl();
                    else if(k=="w") it.w=jp.dbl();
                    else if(k=="h") it.h=jp.dbl();
                    else if(k=="rot") it.rot=jp.dbl();
                    else if(k=="locked") it.locked=jp.dbl()!=0;
                    else if(k=="frame") it.is_frame=jp.dbl()!=0;
                    else if(k=="z") it.z=(int)jp.dbl();
                    else if(k=="text") it.text=js_utf8_to_w(jp.str());
                    else if(k=="field") it.field=js_utf8_to_w(jp.str());
                    else if(k=="prefix") it.prefix=js_utf8_to_w(jp.str());
                    else if(k=="suffix") it.suffix=js_utf8_to_w(jp.str());
                    else if(k=="fmt") it.fmt=js_utf8_to_w(jp.str());
                    else if(k=="font") it.fontName=js_utf8_to_w(jp.str());
                    else if(k=="pt") it.fontPt=jp.dbl();
                    else if(k=="bold") it.bold=jp.dbl()!=0;
                    else if(k=="italic") it.italic=jp.dbl()!=0;
                    else if(k=="align") it.align=(int)jp.dbl();
                    else if(k=="dir") it.dir=(int)jp.dbl();
                    else if(k=="valign") it.valign=(int)jp.dbl();
                    else if(k=="fit") it.objectFit=(int)jp.dbl();
                    else if(k=="ls") it.lineSpacing=jp.dbl();
                    else if(k=="tc") it.textColor=(unsigned)jp.dbl();
                    else if(k=="fc") it.fillColor=(unsigned)jp.dbl();
                    else if(k=="ft") it.fillTransparent=jp.dbl()!=0;
                    else if(k=="bc") it.borderColor=(unsigned)jp.dbl();
                    else if(k=="bw") it.borderWidth=jp.dbl();
                    else if(k=="corner") it.corner=jp.dbl();
                    else if(k=="pad") it.padding=jp.dbl();
                    else if(k=="op") it.opacity=jp.dbl();
                    else if(k=="vis") it.visibility=(int)jp.dbl();
                    else if(k=="img") it.imgPath=js_utf8_to_w(jp.str());
                    else if(k=="sv" || k=="sp") jp.skipValue(); // ignored legacy counter data
                    // v1.55.0 row geometry (absent in older files ⇒ stays 0 = auto)
                    else if(k=="rh") it.rowH=jp.dbl();
                    else if(k=="hh") it.headerH=jp.dbl();
                    else jp.skipValue();
                    if(!jp.match(',')){ jp.match('}'); break; }
                    if(!jp.ok) break;
                }
                // §1.53.0 (Bug E): guarantee a shaping-capable Persian font on the
                // native load path too (mirrors webJsonToDesign) so no item ever
                // draws with an empty/undefined face name.
                if(it.fontName.empty()) it.fontName=L"Vazirmatn";
                if(it.fontPt<=0) it.fontPt=10;
                // Type id 10 is permanently reserved. Silently discard it while
                // importing old .aztpl/design files so retired UI elements cannot
                // reappear through persisted user data.
                if(it.type!=10) out.items.push_back(it);
                if(!jp.match(',')){ jp.match(']'); break; }
                if(!jp.ok) break;
            }
        } else {
            jp.skipValue();
        }
        if(!jp.match(',')){ jp.match('}'); break; }
        if(!jp.ok) break;
    }
    if(!jp.ok){ err=L"\u062e\u0637\u0627 \u062f\u0631 \u062a\u062c\u0632\u06cc\u0647 JSON"; return false; }
    // resolve paper dims if a preset
    double pw,ph; if(Paper_Dims(out.paper,pw,ph)){ out.paperW=pw; out.paperH=ph; }
    // v2.01 (Part E1): a file without a stored reflow base was authored for
    // the paper it was saved on — adopt it so the first paper switch scales
    // from the correct reference instead of guessing.
    if(out.baseW<=0 || out.baseH<=0){ out.baseW=out.paperW; out.baseH=out.paperH; }
    return true;
}

// ============================================================================
//  design store — file-backed (data\designs\*.json + index)
// ============================================================================
static std::wstring designsDir(){
    std::wstring d = dataDir()+L"\\designs";
    CreateDirectoryW(d.c_str(), NULL);
    return d;
}
static std::wstring designPath(int id){
    wchar_t b[32]; swprintf(b,32,L"\\%05d.json",id);
    return designsDir()+b;
}
static std::wstring sectionDesignPath(){ return dataDir()+L"\\section_designs.dat"; }

static int nextDesignId(){
    int maxId=0;
    WIN32_FIND_DATAW fd; std::wstring pat=designsDir()+L"\\*.json";
    HANDLE h=FindFirstFileW(pat.c_str(),&fd);
    if(h!=INVALID_HANDLE_VALUE){
        do { int id=_wtoi(fd.cFileName); if(id>maxId) maxId=id; }
        while(FindNextFileW(h,&fd));
        FindClose(h);
    }
    return maxId+1;
}

static bool writeRawUtf8(const std::wstring& path, const std::string& bytes){
    HANDLE h=CreateFileW(path.c_str(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE) return false;
    DWORD wr=0; WriteFile(h,bytes.data(),(DWORD)bytes.size(),&wr,NULL);
    CloseHandle(h); return wr==bytes.size();
}
static std::string readRawUtf8(const std::wstring& path){
    HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,
                         OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE) return "";
    DWORD sz=GetFileSize(h,NULL); std::string out;
    if(sz!=INVALID_FILE_SIZE && sz>0){ out.resize(sz); DWORD rd=0;
        ReadFile(h,&out[0],sz,&rd,NULL); out.resize(rd); }
    CloseHandle(h); return out;
}

int Designs_Insert(const PrintDesign& d){
    PrintDesign x=d; x.id=nextDesignId();
    writeRawUtf8(designPath(x.id), Design_ToJson(x));
    return x.id;
}
bool Designs_Update(const PrintDesign& d){
    if(d.id<=0) return false;
    return writeRawUtf8(designPath(d.id), Design_ToJson(d));
}
bool Designs_Delete(int id){ return DeleteFileW(designPath(id).c_str())!=0; }
bool Designs_Get(int id, PrintDesign& out){
    std::string j=readRawUtf8(designPath(id));
    if(j.empty()) return false;
    std::wstring err; if(!Design_FromJson(j,out,err)) return false;
    out.id=id; return true;
}
int Designs_All(std::vector<PrintDesign>& out){
    out.clear();
    WIN32_FIND_DATAW fd; std::wstring pat=designsDir()+L"\\*.json";
    HANDLE h=FindFirstFileW(pat.c_str(),&fd);
    if(h!=INVALID_HANDLE_VALUE){
        do {
            int id=_wtoi(fd.cFileName); if(id<=0) continue;
            PrintDesign d; if(Designs_Get(id,d)) out.push_back(d);
        } while(FindNextFileW(h,&fd));
        FindClose(h);
    }
    std::sort(out.begin(),out.end(),[](const PrintDesign&a,const PrintDesign&b){return a.id<b.id;});
    return (int)out.size();
}
int Designs_Builtins(std::vector<PrintDesign>& out){
    std::vector<PrintDesign> all; Designs_All(all); out.clear();
    for(auto& d:all) if(d.kind==L"builtin") out.push_back(d);
    return (int)out.size();
}
int Designs_User(std::vector<PrintDesign>& out){
    std::vector<PrintDesign> all; Designs_All(all); out.clear();
    for(auto& d:all) if(d.kind!=L"builtin") out.push_back(d);
    return (int)out.size();
}

// ----------------------------------------------------- section <-> design ----
static std::map<int,int> loadSectionDesigns(){
    std::map<int,int> m;
    std::wstring all=readFileUtf8(sectionDesignPath());
    size_t pos=0;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos)e=all.size();
        std::wstring line=all.substr(pos,e-pos); pos=e+1;
        while(!line.empty()&&(line.back()==L'\r'||line.back()==L'\n'))line.pop_back();
        if(line.empty())continue;
        size_t bar=line.find(L'|'); if(bar==std::wstring::npos)continue;
        int sid=_wtoi(line.substr(0,bar).c_str());
        int did=_wtoi(line.substr(bar+1).c_str());
        m[sid]=did;
    }
    return m;
}
static void saveSectionDesigns(const std::map<int,int>& m){
    std::wstring out;
    for(auto& kv:m){ wchar_t b[48]; swprintf(b,48,L"%d|%d\r\n",kv.first,kv.second); out+=b; }
    writeFileUtf8(sectionDesignPath(),out,false);
}
bool SectionDesign_Set(int sectionId,int designId){
    auto m=loadSectionDesigns(); m[sectionId]=designId; saveSectionDesigns(m); return true;
}
int SectionDesign_Get(int sectionId){
    auto m=loadSectionDesigns(); auto it=m.find(sectionId);
    return it!=m.end()?it->second:0;
}
bool SectionDesign_Resolve(int sectionId, PrintDesign& out){
    int did=SectionDesign_Get(sectionId);
    if(did>0 && Designs_Get(did,out)) return true;
    // v1.22.0: no binding yet → pick a builtin that MATCHES the section type so
    // each section shows a different, appropriate default (was: always b[0],
    // which made every section preview look identical). We classify by the
    // section's Persian name and choose a group keyword the builtin name fits.
    std::vector<PrintDesign> b; Designs_Builtins(b);
    if(b.empty()) return false;
    std::wstring nm;
    { std::vector<Section> all; Sections_All(all);
      for(const auto& s:all) if(s.id==sectionId){ nm=s.name_fa; break; } }
    auto has=[&](const wchar_t* kw){ return nm.find(kw)!=std::wstring::npos; };
    // keyword to look for inside builtin design names, by section family
    const wchar_t* want=L"";
    if(has(L"نوبت"))                         want=L"نوبت";
    else if(has(L"آزمایش")||has(L"لاب"))     want=L"آزمایش";
    else if(has(L"رادیو")||has(L"تصویر")||has(L"سونو")) want=L"رادیولوژی";
    else if(has(L"دندان"))                   want=L"دندانپزشک";
    else if(has(L"تزریق"))                   want=L"سرپایی";
    else if(has(L"دارو"))                    want=L"شرح خدمت";
    else if(has(L"بیمه"))                    want=L"بیمه";
    else if(has(L"صورتحساب")||has(L"قبض")||has(L"مبلغ")) want=L"صورتحساب";
    if(*want){
        for(const auto& d:b) if(d.name.find(want)!=std::wstring::npos){ out=d; return true; }
    }
    // deterministic spread: different sections fall on different builtins even
    // when no keyword matches, so previews are not all identical.
    out=b[(size_t)((sectionId-1+ (int)b.size())% (int)b.size())];
    if(sectionId<=0) out=b[0];
    return true;
}

// §1.12.0 (§7): single-source-of-truth reconciliation. Detach (archive) any
// section→design binding whose section no longer exists in the live Sections
// registry, and drop any binding that points at a design file that is gone.
// This keeps the print-designer section picker — which reads Sections_Find —
// from ever surfacing stale/orphaned mappings, and prevents the editor from
// resolving a dangling design id. Returns the number of bindings removed.
int SectionDesign_Cleanup(){
    // live section ids
    std::vector<Section> secs; Sections_All(secs);
    std::map<int,bool> liveSection;
    for(const auto& s : secs) liveSection[s.id]=true;
    // existing design ids
    std::vector<PrintDesign> designs; Designs_All(designs);
    std::map<int,bool> liveDesign;
    for(const auto& d : designs) liveDesign[d.id]=true;

    auto m=loadSectionDesigns();
    int removed=0;
    for(auto it=m.begin(); it!=m.end(); ){
        bool secOk    = liveSection.count(it->first)>0;
        bool designOk = (it->second>0) && liveDesign.count(it->second)>0;
        if(!secOk || !designOk){ it=m.erase(it); ++removed; }
        else ++it;
    }
    if(removed>0) saveSectionDesigns(m);

    // also archive orphaned per-department .az_design files (move *.az_design
    // whose code has no matching live section into an `_archived` subfolder, so
    // we never delete user work, just detach it).
    std::wstring dir = dataDir()+L"\\print_designs";
    WIN32_FIND_DATAW fd; std::wstring pat=dir+L"\\*.az_design";
    HANDLE h=FindFirstFileW(pat.c_str(),&fd);
    if(h!=INVALID_HANDLE_VALUE){
        std::map<std::wstring,bool> liveCode;
        for(const auto& s : secs) liveCode[s.code]=true;
        std::wstring arch=dir+L"\\_archived";
        do {
            std::wstring fn=fd.cFileName;
            size_t dot=fn.rfind(L'.');
            std::wstring code=(dot==std::wstring::npos)?fn:fn.substr(0,dot);
            if(liveCode.count(code)==0){
                CreateDirectoryW(arch.c_str(),NULL);
                std::wstring src=dir+L"\\"+fn;
                std::wstring dst=arch+L"\\"+fn;
                DeleteFileW(dst.c_str());
                if(!MoveFileW(src.c_str(),dst.c_str())) DeleteFileW(src.c_str());
                ++removed;
            }
        } while(FindNextFileW(h,&fd));
        FindClose(h);
    }
    return removed;
}

int MachineDesign_Get(){
    std::wstring s=getSetting(L"machine_design_id", L"0");
    return _wtoi(s.c_str());
}
bool MachineDesign_Set(int designId){
    wchar_t b[24]; swprintf(b,24,L"%d", designId);
    setSetting(L"machine_design_id", b);
    return true;
}
bool MachineDesign_Resolve(PrintDesign& out){
    Designs_Init();
    int did=MachineDesign_Get();
    if(did>0 && Designs_Get(did,out) && !out.items.empty()) return true;
    std::vector<PrintDesign> b; Designs_Builtins(b);
    if(b.empty()){ out=Design_BuiltinTemplate(0); return !out.items.empty(); }
    out=b[0];
    return !out.items.empty();
}

#include "print_designer_templates.inc"
#include "print_designer_ui.inc"
