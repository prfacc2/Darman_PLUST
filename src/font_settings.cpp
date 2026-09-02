// ============================================================================
//  font_settings.cpp — v2.08 «تنظیمات فونت و بزرگنمایی» (Font & Zoom settings)
//
//  A self-contained owner-drawn modal dialog (window class AzFontZoom) opened
//  from the C++ settings header. Mirrors the AzPrinterLink dialog pattern:
//  - Searchable list of installed system fonts (EnumFontFamiliesExW)
//  - Page zoom stepper (50–200 %, 0 = auto-fit)
//  - Font size stepper (8–32 px)
//  - Font weight toggle (نازک / کلفت → normal / bold)
//  - Live preview drawn with CreateFontW using the chosen family/size/weight
//  - Save persists four keys to settings.ini (round-trip safe) and pushes the
//    new values live to every admission WebView tab via reception.settings.
//
//  No Qt/MFC — pure Win32 + GDI/GDI+ using the shared helpers in theme.cpp /
//  gdiplus.cpp. Trident-safe (this dialog is native C++; the font change
//  reaches the embedded HTML admission page as inline styles).
// ============================================================================
#include "app.h"

#include <algorithm>

extern HWND g_hFrame;
extern double g_scale;
extern HFONT g_fUI, g_fUIB, g_fSmall, g_fTitle;

// forward declarations from the admission bridge
void WebAdmission_PushEvent(const char* eventName, const std::string& jsonData);

// ---------------------------------------------------------------------------
//  system font enumeration (EnumFontFamiliesExW → sorted unique family list)
// ---------------------------------------------------------------------------
static int CALLBACK fzEnumProc(const LOGFONTW* lf, const TEXTMETRICW*,
                               DWORD, LPARAM lp){
    auto* vec = reinterpret_cast<std::vector<std::wstring>*>(lp);
    std::wstring name(lf->lfFaceName);
    // skip vertical (rotated) fonts and the empty placeholder
    if(name.empty() || name[0]==L'@') return 1;
    // dedupe
    for(const auto& n : *vec) if(n==name) return 1;
    vec->push_back(name);
    return 1;
}

std::vector<std::wstring> EnumSystemFonts(){
    std::vector<std::wstring> vec;
    HDC dc = GetDC(NULL);
    if(dc){
        LOGFONTW lf={0};
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfFaceName[0] = 0;
        EnumFontFamiliesExW(dc, &lf, fzEnumProc, (LPARAM)&vec, 0);
        ReleaseDC(NULL, dc);
    }
    // ensure Vazirmatn is present even if not enumerated (it is embedded)
    bool hasVazir=false;
    for(const auto& n:vec) if(n==L"Vazirmatn"||n==L"Vazir"){ hasVazir=true; break; }
    if(!hasVazir) vec.insert(vec.begin(), L"Vazirmatn");
    std::sort(vec.begin(), vec.end(), [](const std::wstring& a, const std::wstring& b){
        // Vazirmatn first (the default), then alphabetical
        if(a==L"Vazirmatn") return true;
        if(b==L"Vazirmatn") return false;
        return _wcsicmp(a.c_str(), b.c_str()) < 0;
    });
    return vec;
}

// ---------------------------------------------------------------------------
//  string helpers (same pattern as clinic_ops.cpp opsW2u8 / opsJstr)
// ---------------------------------------------------------------------------
static std::string fzW2U8(const std::wstring& ws){
    if(ws.empty()) return "";
    int n=WideCharToMultiByte(CP_UTF8,0,ws.c_str(),(int)ws.size(),NULL,0,NULL,NULL);
    std::string s; if(n>0){ s.resize(n); WideCharToMultiByte(CP_UTF8,0,ws.c_str(),(int)ws.size(),&s[0],n,NULL,NULL); }
    return s;
}
static std::string fzJstr(const std::wstring& s){
    std::string o="\"", u=fzW2U8(s);
    for(char c : u){
        if(c=='"'||c=='\\') o+='\\';
        if((unsigned char)c<0x20){
            switch(c){
            case '\n': o+="\\n"; break;
            case '\r': o+="\\r"; break;
            case '\t': o+="\\t"; break;
            default: { char buf[8]; snprintf(buf,8,"\\u%04x",(unsigned char)c); o+=buf; break; }
            }
        } else o+=c;
    }
    o+='"'; return o;
}
static std::wstring fzNormalize(const std::wstring& s){
    // simple case-insensitive substring helper for the font filter
    std::wstring lo=s;
    for(auto& c:lo) c=(wchar_t)towlower(c);
    return lo;
}

// ---------------------------------------------------------------------------
//  dialog state & geometry
// ---------------------------------------------------------------------------
#define FZ_CLASS L"AzFontZoom"

enum {
    FZB_CLOSE = 600, FZB_SAVE, FZB_CANCEL,
    FZB_ZOOM_DEC, FZB_ZOOM_INC, FZB_ZOOM_AUTO,
    FZB_SIZE_DEC, FZB_SIZE_INC,
    FZB_WEIGHT_TOGGLE,
    FZB_ITEM_BASE = 700
};

struct FontZoomState {
    HWND owner;
    HWND eSearch;          // real EDIT child for the font search
    int  hot;              // hovered list row (-1 none)
    std::vector<std::wstring> allFonts;
    std::vector<int>          filtered;   // indices into allFonts
    std::wstring searchText;
    std::wstring selFont;  // current font selection (L"" = Vazirmatn default)
    int  zoom;             // 0 = auto-fit, else 50..200
    int  fontSize;         // 8..32 px
    bool bold;             // weight toggle
    int  scroll;           // list scroll offset
    HFONT previewFont;     // created from current selection for live preview
    FontZoomState():owner(NULL),eSearch(NULL),hot(-1),zoom(0),fontSize(14),
                    bold(false),scroll(0),previewFont(NULL){}
    ~FontZoomState(){ if(previewFont) DeleteObject(previewFont); }
};
static HWND s_fz=NULL;
static FontZoomState* s_fzs=NULL;
#define IDC_FZ_SEARCH 902

static int fzCardW(){ return S(560); }
static int fzCardH(){ return S(620); }
static RECT fzCard(HWND h){
    RECT rc; GetClientRect(h,&rc);
    int w=fzCardW(), hh=fzCardH();
    if(hh > rc.bottom-S(40)) hh = rc.bottom-S(40);
    RECT c={(rc.right-w)/2,(rc.bottom-hh)/2,(rc.right+w)/2,(rc.bottom+hh)/2};
    return c;
}
static int fzListRows(){ return 6; }
static int fzRowH(){ return S(34); }

static RECT fzSearchEditRect(const RECT& c){
    int x=c.left+S(20), y=c.top+S(58), w=(c.right-c.left)-S(40), h=S(36);
    RECT r={x,y,x+w,y+h}; return r;
}
static RECT fzListRect(const RECT& c){
    int x=c.left+S(20), y=c.top+S(104), w=(c.right-c.left)-S(40);
    int rows=fzListRows();
    int h=fzRowH()*rows;
    RECT r={x,y,x+w,y+h}; return r;
}
static RECT fzControlsRect(const RECT& c){
    int x=c.left+S(20), y=c.top+S(104)+fzRowH()*fzListRows()+S(14), w=(c.right-c.left)-S(40);
    RECT r={x,y,x+w,y+S(170)}; return r;
}
static RECT fzPreviewRect(const RECT& c){
    RECT ctrl=fzControlsRect(c);
    int x=ctrl.left, y=ctrl.bottom+S(12), w=ctrl.right-ctrl.left, h=S(70);
    RECT r={x,y,x+w,y+h}; return r;
}
static RECT fzBtnRowRect(const RECT& c){
    RECT pv=fzPreviewRect(c);
    int x=c.left+S(20), y=pv.bottom+S(12), w=(c.right-c.left)-S(40);
    RECT r={x,y,x+w,y+S(40)}; return r;
}
static RECT fzCloseRect(const RECT& c){
    RECT r={c.right-S(42),c.top+S(14),c.right-S(14),c.top+S(42)}; return r;
}

// ---------------------------------------------------------------------------
//  font filter
// ---------------------------------------------------------------------------
static void fzApplyFilter(){
    if(!s_fzs) return;
    wchar_t buf[256]={0};
    GetWindowTextW(s_fzs->eSearch, buf, 255);
    s_fzs->searchText = trim(buf);
    s_fzs->filtered.clear();
    std::wstring needle = fzNormalize(s_fzs->searchText);
    for(int i=0;i<(int)s_fzs->allFonts.size();++i){
        if(needle.empty()){
            s_fzs->filtered.push_back(i);
        } else {
            std::wstring hay = fzNormalize(s_fzs->allFonts[i]);
            if(hay.find(needle)!=std::wstring::npos)
                s_fzs->filtered.push_back(i);
        }
    }
    s_fzs->scroll=0;
    InvalidateRect(s_fz,NULL,FALSE);
}

// ---------------------------------------------------------------------------
//  live preview font
// ---------------------------------------------------------------------------
static void fzUpdatePreviewFont(){
    if(!s_fzs) return;
    if(s_fzs->previewFont) DeleteObject(s_fzs->previewFont);
    std::wstring fam = s_fzs->selFont.empty() ? L"Vazirmatn" : s_fzs->selFont;
    int weight = s_fzs->bold ? FW_BOLD : FW_NORMAL;
    int px = s_fzs->fontSize>0 ? s_fzs->fontSize : 14;
    s_fzs->previewFont = CreateFontW(-S(px), 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fam.c_str());
    InvalidateRect(s_fz,NULL,FALSE);
}

// ---------------------------------------------------------------------------
//  hit testing
// ---------------------------------------------------------------------------
static int fzHit(HWND h, POINT pt){
    if(!s_fzs) return -1;
    RECT card=fzCard(h);
    if(!PtInRect(&card,pt)) return -1;   // outside the card → scrim
    RECT closeR=fzCloseRect(card);
    if(PtInRect(&closeR,pt)) return FZB_CLOSE;
    RECT srch=fzSearchEditRect(card);
    if(PtInRect(&srch,pt)) return -2;    // search field
    // list rows
    RECT lr=fzListRect(card);
    if(PtInRect(&lr,pt)){
        int idx=(pt.y-lr.top)/fzRowH()+s_fzs->scroll;
        if(idx>=0 && idx<(int)s_fzs->filtered.size())
            return FZB_ITEM_BASE+idx;
        return -1;
    }
    return -1;
}

// ---------------------------------------------------------------------------
//  paint
// ---------------------------------------------------------------------------
static void fzPaint(HWND h, HDC dc0){
    if(!s_fzs) return;
    RECT card=fzCard(h);
    /* Paint the scrim (full window area) directly onto dc0, then paint the
       card content onto an off-screen bitmap and BitBlt it back.  The card
       bitmap must be created from dc0 (not dc) so its colour depth matches. */
    RECT full; GetClientRect(h,&full);
    HDC dc=CreateCompatibleDC(dc0);
    int w=full.right-full.left, hh=full.bottom-full.top;
    HBITMAP bmp=CreateCompatibleBitmap(dc0,w,hh);
    HBITMAP old=(HBITMAP)SelectObject(dc,bmp);

    // scrim background (dim the owner behind the dialog) — full window area
    gpFillAlpha(dc, full, 0, RGB(0,0,0), 130);

    // card shadow + gradient
    gpShadow(dc, card, S(18), S(6), 60);
    gpGradRoundRect(dc, card, S(16), g_theme.surfaceTop, g_theme.surface, g_theme.border);

    // title band
    RECT tr={card.left+S(20),card.top+S(16),card.right-S(50),card.top+S(48)};
    SetBkMode(dc,TRANSPARENT);
    SetTextColor(dc,g_theme.text);
    HFONT oldF=(HFONT)SelectObject(dc,g_fTitle);
    DrawTextW(dc,L"\u062a\u0646\u0638\u06cc\u0645\u0627\u062a \u0641\u0648\u0646\u062a \u0648 \u0628\u0632\u0631\u06af\u0646\u0645\u0627\u06cc\u06cc",-1,&tr,DT_RTLREADING|DT_SINGLELINE|DT_VCENTER);
    // section label «پذیرش بیمار»
    RECT sl={card.left+S(20),card.top+S(40),card.right-S(50),card.top+S(58)};
    SelectObject(dc,g_fSmall);
    SetTextColor(dc,g_theme.textDim);
    DrawTextW(dc,L"\u067e\u0630\u06cc\u0631\u0634 \u0628\u06cc\u0645\u0627\u0631",-1,&sl,DT_RTLREADING|DT_SINGLELINE|DT_VCENTER);

    // close ×
    RECT closeR=fzCloseRect(card);
    drawIcon(dc, ICO_X, closeR, g_theme.textDim, 2);

    // search edit frame
    RECT sr=fzSearchEditRect(card);
    gpRoundRect(dc, sr, S(8), g_theme.inputBg, g_theme.border);
    // cue text if empty
    if(GetWindowTextLengthW(s_fzs->eSearch)==0){
        RECT cue=sr; cue.right-=S(10); cue.left+=S(10);
        SetTextColor(dc,g_theme.textDim);
        SelectObject(dc,g_fSmall);
        DrawTextW(dc,L"\u062c\u0633\u062a\u062c\u0648\u06cc \u0641\u0648\u0646\u062a…",-1,&cue,DT_RTLREADING|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
    }

    // font list
    RECT lr=fzListRect(card);
    gpRoundRect(dc, lr, S(10), g_theme.inputBg, g_theme.border);
    int maxRows=fzListRows();
    int selIdx=-1;
    for(int i=0;i<(int)s_fzs->filtered.size();++i)
        if(s_fzs->allFonts[s_fzs->filtered[i]]==s_fzs->selFont){ selIdx=i; break; }
    HRGN clip=CreateRectRgn(lr.left+S(2),lr.top+S(2),lr.right-S(2),lr.bottom-S(2));
    SelectClipRgn(dc,clip);
    for(int row=0; row<maxRows; ++row){
        int idx=row+s_fzs->scroll;
        if(idx<0||idx>=(int)s_fzs->filtered.size()) break;
        int fi=s_fzs->filtered[idx];
        RECT rr={lr.left+S(4),lr.top+row*fzRowH(),lr.right-S(4),lr.top+(row+1)*fzRowH()};
        bool sel=(idx==selIdx);
        bool hot=(idx==s_fzs->hot);
        if(sel){
            gpFillAlpha(dc,rr,S(6),RGB(25,118,233),28);
            // right marker
            RECT mk={rr.right-S(3),rr.top+S(4),rr.right,rr.bottom-S(4)};
            HBRUSH b=CreateSolidBrush(g_theme.accent); FillRect(dc,&mk,b); DeleteObject(b);
        } else if(hot){
            gpFillAlpha(dc,rr,S(6),g_theme.hover,40);
        }
        SetBkMode(dc,TRANSPARENT);
        SetTextColor(dc, sel?g_theme.accentText:(hot?g_theme.text:g_theme.text));
        SelectObject(dc, sel?g_fUIB:g_fUI);
        RECT tx=rr; tx.right-=S(10); tx.left+=S(12);
        DrawTextW(dc, s_fzs->allFonts[fi].c_str(), -1, &tx,
                  DT_RTLREADING|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
    }
    SelectClipRgn(dc,NULL);
    DeleteObject(clip);

    // controls area: zoom, size, weight
    RECT cr=fzControlsRect(card);
    int y=cr.top;
    int colW=(cr.right-cr.left-S(20))/3;

    // zoom stepper
    {
        RECT lbl={cr.left,y,cr.left+colW,y+S(20)};
        SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
        DrawTextW(dc,L"\u0628\u0632\u0631\u06af\u0646\u0645\u0627\u06cc\u06cc",-1,&lbl,DT_RTLREADING|DT_SINGLELINE|DT_VCENTER);
        // value
        std::wstring z = s_fzs->zoom==0 ? L"\u062e\u0648\u062f\u06a9\u0627\u0631" : (std::to_wstring(s_fzs->zoom)+L"\u066a");
        RECT val={cr.left,y+S(22),cr.left+colW,y+S(44)};
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
        DrawTextW(dc,z.c_str(),-1,&val,DT_RTLREADING|DT_SINGLELINE|DT_VCENTER|DT_CENTER);
        // − / + buttons
        RECT dm={cr.left+S(4),y+S(24),cr.left+S(22),y+S(42)};
        RECT dp={cr.left+colW-S(22),y+S(24),cr.left+colW-S(4),y+S(42)};
        drawIcon(dc,ICO_X,dm,g_theme.textDim,2);  // reuse X as minus
        // draw plus manually
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
        DrawTextW(dc,L"+",-1,&dp,DT_SINGLELINE|DT_VCENTER|DT_CENTER);
        // auto button
        RECT ab={cr.left+S(26),y+S(24),cr.left+colW-S(26),y+S(42)};
        if(s_fzs->zoom==0){
            gpFillAlpha(dc,ab,S(6),g_theme.accent,30);
            SetTextColor(dc,g_theme.accentText);
        } else {
            SetTextColor(dc,g_theme.textDim);
        }
        SelectObject(dc,g_fSmall);
        DrawTextW(dc,L"\u062e\u0648\u062f\u06a9\u0627\u0631",-1,&ab,DT_RTLREADING|DT_SINGLELINE|DT_VCENTER|DT_CENTER);
    }

    // font size stepper
    {
        RECT lbl={cr.left+colW+S(10),y,cr.left+2*colW+S(10),y+S(20)};
        SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
        DrawTextW(dc,L"\u0633\u0627\u06cc\u0632 \u0641\u0648\u0646\u062a",-1,&lbl,DT_RTLREADING|DT_SINGLELINE|DT_VCENTER);
        std::wstring sz=std::to_wstring(s_fzs->fontSize)+L" \u067e\u06cc\u06a9\u0633\u0644";
        RECT val={cr.left+colW+S(10),y+S(22),cr.left+2*colW+S(10),y+S(44)};
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
        DrawTextW(dc,sz.c_str(),-1,&val,DT_RTLREADING|DT_SINGLELINE|DT_VCENTER|DT_CENTER);
        RECT dm={cr.left+colW+S(14),y+S(24),cr.left+colW+S(32),y+S(42)};
        RECT dp={cr.left+2*colW+S(10)-S(22),y+S(24),cr.left+2*colW+S(10)-S(4),y+S(42)};
        drawIcon(dc,ICO_X,dm,g_theme.textDim,2);
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
        DrawTextW(dc,L"+",-1,&dp,DT_SINGLELINE|DT_VCENTER|DT_CENTER);
    }

    // weight toggle
    {
        RECT lbl={cr.left+2*colW+S(20),y,cr.right,y+S(20)};
        SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
        DrawTextW(dc,L"\u0648\u0632\u0646 \u0641\u0648\u0646\u062a",-1,&lbl,DT_RTLREADING|DT_SINGLELINE|DT_VCENTER);
        std::wstring w = s_fzs->bold ? L"\u06a9\u0644\u0641\u062a" : L"\u0646\u0627\u0632\u06a9";
        RECT val={cr.left+2*colW+S(20),y+S(22),cr.right,y+S(44)};
        if(s_fzs->bold){
            gpFillAlpha(dc,val,S(8),g_theme.accent,28);
            SetTextColor(dc,g_theme.accentText);
        } else {
            gpFillAlpha(dc,val,S(8),g_theme.hover,40);
            SetTextColor(dc,g_theme.text);
        }
        SelectObject(dc,g_fUIB);
        DrawTextW(dc,w.c_str(),-1,&val,DT_RTLREADING|DT_SINGLELINE|DT_VCENTER|DT_CENTER);
    }

    // preview box
    RECT pv=fzPreviewRect(card);
    gpRoundRect(dc, pv, S(12), g_theme.inputBg, g_theme.border);
    // label
    RECT pl={pv.left+S(12),pv.top+S(8),pv.right-S(12),pv.top+S(24)};
    SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
    DrawTextW(dc,L"\u067e\u06cc\u0634\u200c\u0646\u0645\u0627\u06cc\u0634",-1,&pl,DT_RTLREADING|DT_SINGLELINE|DT_VCENTER);
    // sample text drawn with the preview font
    if(s_fzs->previewFont){
        SelectObject(dc,s_fzs->previewFont);
        SetTextColor(dc,g_theme.text);
        RECT tx={pv.left+S(14),pv.top+S(28),pv.right-S(14),pv.bottom-S(8)};
        DrawTextW(dc,L"\u067e\u0630\u06cc\u0631\u0634 \u0628\u06cc\u0645\u0627\u0631 \u2014 \u0627\u0633\u0645: \u0639\u0644\u06cc \u0631\u0636\u0627\u06cc\u06cc  \u06f9\u06f8 \u0633\u0627\u0644  \u06a9\u062f \u0645\u0644\u06cc: \u06f0\u06f0\u06f1\u06f2\u06f3\u06f4\u06f5\u06f6\u06f7\u06f8\u06f9  \u0646\u0648\u0628\u062a: \u06f0\u06f5\u06f6\u06f6\u06f3",-1,&tx,DT_RTLREADING|DT_WORDBREAK);
    }

    // buttons: save / cancel
    RECT br=fzBtnRowRect(card);
    int bw=S(120), gap=S(12);
    // save (right side in RTL)
    RECT saveR={br.right-bw,br.top,br.right,br.bottom};
    gpGradRoundRect(dc, saveR, S(10), g_theme.accent, g_theme.accent2, g_theme.accent);
    SetBkMode(dc,TRANSPARENT); SetTextColor(dc,g_theme.accentText);
    SelectObject(dc,g_fUIB);
    DrawTextW(dc,L"\u062a\u0623\u06cc\u06cc\u062f",-1,&saveR,DT_RTLREADING|DT_SINGLELINE|DT_VCENTER|DT_CENTER);
    // cancel
    RECT cancR={br.right-bw-gap-bw,br.top,br.right-bw-gap,br.bottom};
    gpRoundRect(dc, cancR, S(10), g_theme.surface2, g_theme.border);
    SetTextColor(dc,g_theme.text);
    SelectObject(dc,g_fUI);
    DrawTextW(dc,L"\u0627\u0646\u0635\u0631\u0627\u0641",-1,&cancR,DT_RTLREADING|DT_SINGLELINE|DT_VCENTER|DT_CENTER);

    SelectObject(dc,oldF);
    /* Blit the completed off-screen bitmap onto the real screen DC */
    BitBlt(dc0, 0, 0, w, hh, dc, 0, 0, SRCCOPY);
    SelectObject(dc,old);
    DeleteObject(bmp);
    DeleteDC(dc);
}

// ---------------------------------------------------------------------------
//  save
// ---------------------------------------------------------------------------
static void fzSave(){
    if(!s_fzs) return;
    std::wstring fam = s_fzs->selFont.empty() ? L"Vazirmatn" : s_fzs->selFont;
    setSetting(L"admission.font_family", fam);
    setSetting(L"admission.font_size", std::to_wstring((long long)s_fzs->fontSize));
    setSetting(L"admission.font_weight", s_fzs->bold ? L"bold" : L"600");
    setSetting(L"admission.zoom", std::to_wstring((long long)s_fzs->zoom));

    // live-apply to every admission web tab
    std::string j="{\"zoom\":"+std::to_string((long long)s_fzs->zoom);
    j+=",\"fontFamily\":"+fzJstr(fam);
    j+=",\"fontSize\":"+std::to_string((long long)s_fzs->fontSize);
    j+=",\"fontWeight\":\""+ (s_fzs->bold?std::string("bold"):std::string("600")) +"\"}";
    WebAdmission_PushEvent("reception.settings", j);

    DestroyWindow(s_fz);
}

static void fzClose(){
    if(s_fz) DestroyWindow(s_fz);
}

// ---------------------------------------------------------------------------
//  window proc
// ---------------------------------------------------------------------------
static LRESULT CALLBACK fzProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_PAINT:{
        PAINTSTRUCT ps; BeginPaint(h,&ps);
        fzPaint(h, ps.hdc);
        EndPaint(h,&ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;  // double-buffered in WM_PAINT

    case WM_MOUSEMOVE:{
        POINT pt={LOWORD(l),HIWORD(l)};
        int hit=fzHit(h,pt);
        // list hover
        int newHot=-1;
        if(hit>=FZB_ITEM_BASE){
            int idx=hit-FZB_ITEM_BASE;
            RECT lr=fzListRect(fzCard(h));
            int row=(pt.y-lr.top)/fzRowH()+s_fzs->scroll;
            if(row>=0 && row<(int)s_fzs->filtered.size()) newHot=row;
        }
        if(newHot!=s_fzs->hot){ s_fzs->hot=newHot; InvalidateRect(h,NULL,FALSE); }
        // cursor
        if(hit==FZB_CLOSE || (hit>=FZB_ZOOM_DEC) || hit>=FZB_ITEM_BASE)
            SetCursor(LoadCursor(NULL,IDC_HAND));
        else if(hit==-2) SetCursor(LoadCursor(NULL,IDC_IBEAM));
        else SetCursor(LoadCursor(NULL,IDC_ARROW));
        // track for mouse leave
        TRACKMOUSEEVENT tme={sizeof(tme),TME_LEAVE,h,0};
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
        if(s_fzs && s_fzs->hot!=-1){ s_fzs->hot=-1; InvalidateRect(h,NULL,FALSE); }
        return 0;

    case WM_LBUTTONDOWN:{
        POINT pt={LOWORD(l),HIWORD(l)};
        int hit=fzHit(h,pt);
        if(hit==-1){ /* outside card or non-interactive — ignore */ }
        else if(hit==FZB_CLOSE){ fzClose(); return 0; }
        else if(hit==-2){ SetFocus(s_fzs->eSearch); return 0; }
        else if(hit>=FZB_ITEM_BASE){
            int idx=hit-FZB_ITEM_BASE;
            if(idx>=0 && idx<(int)s_fzs->filtered.size()){
                s_fzs->selFont = s_fzs->allFonts[s_fzs->filtered[idx]];
                fzUpdatePreviewFont();
            }
            return 0;
        }
        // check control buttons by rect
        RECT card=fzCard(h);
        RECT cr=fzControlsRect(card);
        int y=cr.top, colW=(cr.right-cr.left-S(20))/3;
        // zoom −
        RECT zm={cr.left+S(4),y+S(24),cr.left+S(22),y+S(42)};
        // zoom +
        RECT zp={cr.left+colW-S(22),y+S(24),cr.left+colW-S(4),y+S(42)};
        // zoom auto
        RECT za={cr.left+S(26),y+S(24),cr.left+colW-S(26),y+S(42)};
        // size −
        RECT sm={cr.left+colW+S(14),y+S(24),cr.left+colW+S(32),y+S(42)};
        // size +
        RECT sp={cr.left+2*colW+S(10)-S(22),y+S(24),cr.left+2*colW+S(10)-S(4),y+S(42)};
        // weight
        RECT wt={cr.left+2*colW+S(20),y+S(22),cr.right,y+S(44)};
        if(PtInRect(&zm,pt)){
            if(s_fzs->zoom<=50) s_fzs->zoom=50; else if(s_fzs->zoom==0) s_fzs->zoom=80; else s_fzs->zoom-=5;
            InvalidateRect(h,NULL,FALSE); return 0;
        }
        if(PtInRect(&zp,pt)){
            if(s_fzs->zoom==0) s_fzs->zoom=100; else if(s_fzs->zoom<200) s_fzs->zoom+=5;
            InvalidateRect(h,NULL,FALSE); return 0;
        }
        if(PtInRect(&za,pt)){
            s_fzs->zoom=0; InvalidateRect(h,NULL,FALSE); return 0;
        }
        if(PtInRect(&sm,pt)){
            if(s_fzs->fontSize>8) s_fzs->fontSize--; fzUpdatePreviewFont(); return 0;
        }
        if(PtInRect(&sp,pt)){
            if(s_fzs->fontSize<32) s_fzs->fontSize++; fzUpdatePreviewFont(); return 0;
        }
        if(PtInRect(&wt,pt)){
            s_fzs->bold=!s_fzs->bold; fzUpdatePreviewFont(); return 0;
        }
        // save / cancel
        RECT br=fzBtnRowRect(card);
        int bw=S(120), gap=S(12);
        RECT saveR={br.right-bw,br.top,br.right,br.bottom};
        RECT cancR={br.right-bw-gap-bw,br.top,br.right-bw-gap,br.bottom};
        if(PtInRect(&saveR,pt)){ fzSave(); return 0; }
        if(PtInRect(&cancR,pt)){ fzClose(); return 0; }
        return 0;
    }

    case WM_MOUSEWHEEL:{
        int delta=GET_WHEEL_DELTA_WPARAM(w);
        if(delta>0 && s_fzs->scroll>0){ s_fzs->scroll--; InvalidateRect(h,NULL,FALSE); }
        else if(delta<0){
            int maxScroll=(int)s_fzs->filtered.size()-fzListRows();
            if(s_fzs->scroll<maxScroll){ s_fzs->scroll++; InvalidateRect(h,NULL,FALSE); }
        }
        return 0;
    }

    case WM_COMMAND:{
        if(LOWORD(w)==IDC_FZ_SEARCH && HIWORD(w)==EN_CHANGE){
            fzApplyFilter();
            return 0;
        }
        return 0;
    }

    case WM_CTLCOLOREDIT:{
        HDC dc=(HDC)w;
        SetBkColor(dc, g_theme.inputBg);
        SetTextColor(dc, g_theme.inputText);
        return (LRESULT)g_brInput;
    }

    case WM_KEYDOWN:{
        if(w==VK_ESCAPE){ fzClose(); return 0; }
        if(w==VK_RETURN){ fzSave(); return 0; }
        // up/down to navigate the font list
        if((w==VK_UP||w==VK_DOWN) && s_fzs){
            int selIdx=-1;
            for(int i=0;i<(int)s_fzs->filtered.size();++i)
                if(s_fzs->allFonts[s_fzs->filtered[i]]==s_fzs->selFont){ selIdx=i; break; }
            if(w==VK_UP && selIdx>0) selIdx--;
            else if(w==VK_DOWN && selIdx<(int)s_fzs->filtered.size()-1) selIdx++;
            if(selIdx>=0){
                s_fzs->selFont=s_fzs->allFonts[s_fzs->filtered[selIdx]];
                if(selIdx<s_fzs->scroll) s_fzs->scroll=selIdx;
                if(selIdx>=s_fzs->scroll+fzListRows()) s_fzs->scroll=selIdx-fzListRows()+1;
                fzUpdatePreviewFont();
            }
            return 0;
        }
        break;
    }

    case WM_DESTROY:
        if(s_fzs){ delete s_fzs; s_fzs=NULL; }
        s_fz=NULL;
        if(g_hFrame) SetFocus(g_hFrame);
        return 0;

    case WM_APP_THEME:
        InvalidateRect(h,NULL,FALSE);
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

// ---------------------------------------------------------------------------
//  entry point — FontZoom_Open(owner)
// ---------------------------------------------------------------------------
void FontZoom_Open(HWND owner){
    if(s_fz && IsWindow(s_fz)){ fzClose(); return; }
    static bool reg=false;
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=fzProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=FZ_CLASS;
        RegisterClassW(&wc); reg=true; }
    RECT rc; GetClientRect(owner,&rc);
    POINT org={0,0}; ClientToScreen(owner,&org);
    s_fzs=new FontZoomState();
    s_fzs->owner=owner;
    s_fzs->allFonts=EnumSystemFonts();

    // load current settings
    std::wstring fam=getSetting(L"admission.font_family",L"Vazirmatn");
    s_fzs->selFont = fam.empty() ? L"Vazirmatn" : fam;
    // verify the saved font is actually installed; fall back to Vazirmatn
    bool found=false;
    for(const auto& f : s_fzs->allFonts) if(f==s_fzs->selFont){ found=true; break; }
    if(!found) s_fzs->selFont=L"Vazirmatn";

    s_fzs->zoom=_wtoi(getSetting(L"admission.zoom",L"0").c_str());
    if(s_fzs->zoom<50||s_fzs->zoom>200) s_fzs->zoom=0;
    s_fzs->fontSize=_wtoi(getSetting(L"admission.font_size",L"14").c_str());
    if(s_fzs->fontSize<8||s_fzs->fontSize>32) s_fzs->fontSize=14;
    s_fzs->bold = (getSetting(L"admission.font_weight",L"600")==L"bold");

    fzApplyFilter();
    // scroll the selection into view
    for(int i=0;i<(int)s_fzs->filtered.size();++i)
        if(s_fzs->allFonts[s_fzs->filtered[i]]==s_fzs->selFont){
            if(i>=fzListRows()) s_fzs->scroll=i-fzListRows()+1;
            break;
        }

    s_fz=CreateWindowExW(WS_EX_TOPMOST,FZ_CLASS,L"",
        WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN,
        org.x,org.y,rc.right,rc.bottom,owner,NULL,g_hInst,NULL);
    if(!s_fz){ delete s_fzs; s_fzs=NULL; return; }

    // themed search EDIT child
    RECT er=fzSearchEditRect(fzCard(s_fz));
    s_fzs->eSearch=CreateWindowExW(0,L"EDIT",s_fzs->searchText.c_str(),
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,
        er.left+S(6),er.top+S(3),(er.right-er.left)-S(12),(er.bottom-er.top)-S(6),
        s_fz,(HMENU)IDC_FZ_SEARCH,g_hInst,NULL);
    SendMessageW(s_fzs->eSearch,WM_SETFONT,(WPARAM)g_fUI,TRUE);
    SendMessageW(s_fzs->eSearch,EM_SETCUEBANNER,TRUE,
        (LPARAM)L"\u062c\u0633\u062a\u062c\u0648\u06cc \u0641\u0648\u0646\u062a\u2026");

    fzUpdatePreviewFont();
    BringWindowToTop(s_fz);
    SetFocus(s_fz);
}
