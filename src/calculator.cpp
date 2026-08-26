// ============================================================================
//  calculator.cpp — custom always-on-top calculator
//  keyboard + numpad + mouse, fast double math, special UI
// ============================================================================
#include "app.h"
#include <math.h>
#include <stdio.h>

#define CALC_CLASS L"AzCalc"

struct CalcState {
    std::wstring display;     // current entry
    std::wstring history;     // expression line
    double acc;               // accumulator
    wchar_t pendOp;           // 0 + - * /
    bool fresh;               // next digit starts new entry
    bool err;
    HWND btns[24];
    int  hot;                 // hovered button index (-1)
    int  press;               // pressed index
    int  flash;               // v1.93: keyboard press flash index (-1 = none)
};

// layout: 6 rows x 4 cols
static const wchar_t* KEYS[24] = {
    L"C",  L"\u232B", L"%",  L"\u00F7",
    L"7",  L"8",  L"9",  L"\u00D7",
    L"4",  L"5",  L"6",  L"\u2212",
    L"1",  L"2",  L"3",  L"+",
    L"\u00B1", L"0",  L".",  L"=",
    L"\u221A", L"x\u00B2", L"1/x", L"M"   // extra row at top? keep 5 rows -> use 20
};
// We'll actually use 5 rows of 4 = 20 keys: indices 0..19

// v1.93: map a typed character to the KEYS[] button index it corresponds to,
// so a keyboard press can flash the matching on-screen button. Returns -1 when
// the character is not a calculator key. (VK-only keys like Return/Backspace/
// Delete are flashed directly from WM_KEYDOWN — they never arrive here.)
static int keyToIndex(wchar_t c){
    switch(c){
        case L'0': return 17;
        case L'1': return 12;
        case L'2': return 13;
        case L'3': return 14;
        case L'4': return 8;
        case L'5': return 9;
        case L'6': return 10;
        case L'7': return 4;
        case L'8': return 5;
        case L'9': return 6;
        case L'+': return 15;
        case L'-': return 11;
        case L'*': return 7;
        case L'/': return 3;
        case L'=': return 19;
        case L'.': return 18;
        case L'%': return 2;
        default:   return -1;
    }
}

// v1.93: briefly highlight the on-screen button that matches a keyboard press.
// SetTimer with id 99 (re)arms a 120ms timer; WM_TIMER clears the flash.
static void calcFlash(HWND h, CalcState* s, int idx){
    if(!s || idx<0) return;
    s->flash = idx;
    InvalidateRect(h, NULL, FALSE);
    SetTimer(h, 99, 120, NULL);
}

static double toNum(const std::wstring& s){
    if(s.empty()) return 0;
    return _wtof(s.c_str());
}
static std::wstring numStr(double v){
    if(v != v) return L"خطا";
    wchar_t buf[64];
    if(fabs(v) < 1e15 && v == floor(v) && fabs(v) >= 1.0 - 1e-9)
        swprintf(buf,64,L"%.0f",v);
    else if(v == 0) wcscpy(buf, L"0");
    else {
        swprintf(buf,64,L"%.10G",v);
    }
    return buf;
}
static std::wstring groupNum(const std::wstring& s){
    // thousands grouping for integer part
    size_t dot = s.find(L'.');
    std::wstring ip = (dot==std::wstring::npos)?s:s.substr(0,dot);
    std::wstring fp = (dot==std::wstring::npos)?L"":s.substr(dot);
    bool neg = !ip.empty() && ip[0]==L'-';
    if(neg) ip = ip.substr(1);
    if(ip.size()>3 && ip.find_first_not_of(L"0123456789")==std::wstring::npos){
        std::wstring o; int c=0;
        for(int i=(int)ip.size()-1;i>=0;i--){
            o.insert(o.begin(), ip[i]);
            if(++c%3==0 && i>0) o.insert(o.begin(), L',');
        }
        ip = o;
    }
    return (neg?L"-":L"")+ip+fp;
}

static void calcEval(CalcState* s){
    if(!s->pendOp){ s->acc = toNum(s->display); return; }
    double b = toNum(s->display);
    switch(s->pendOp){
        case L'+': s->acc += b; break;
        case L'-': s->acc -= b; break;
        case L'*': s->acc *= b; break;
        case L'/':
            if(b==0){ s->err=true; s->display=L"تقسیم بر صفر"; return; }
            s->acc /= b; break;
    }
    s->display = numStr(s->acc);
}

static void calcKey(HWND h, CalcState* s, const std::wstring& k){
    if(s->err && k!=L"C"){ return; }
    if(k.size()==1 && k[0]>=L'0' && k[0]<=L'9'){
        if(s->fresh){ s->display.clear(); s->fresh=false; }
        if(s->display==L"0") s->display.clear();
        if(s->display.size()<18) s->display += k;
    }
    else if(k==L"."){
        if(s->fresh){ s->display=L"0"; s->fresh=false; }
        if(s->display.find(L'.')==std::wstring::npos) s->display += L".";
    }
    else if(k==L"C"){
        s->display=L"0"; s->history.clear(); s->acc=0; s->pendOp=0;
        s->fresh=true; s->err=false;
    }
    else if(k==L"\u232B"){ // backspace
        if(!s->fresh && s->display.size()>0) s->display.erase(s->display.size()-1);
        if(s->display.empty()) { s->display=L"0"; s->fresh=true; }
    }
    else if(k==L"\u00B1"){
        if(s->display!=L"0"){
            if(s->display[0]==L'-') s->display.erase(0,1);
            else s->display.insert(s->display.begin(), L'-');
        }
    }
    else if(k==L"%"){
        double v = toNum(s->display);
        if(s->pendOp) v = s->acc * v / 100.0;
        else v = v/100.0;
        s->display = numStr(v);
    }
    else if(k==L"\u221A"){
        double v = toNum(s->display);
        if(v<0){ s->err=true; s->display=L"خطا"; }
        else s->display = numStr(sqrt(v));
        s->fresh = true;
    }
    else if(k==L"x\u00B2"){
        double v = toNum(s->display);
        s->display = numStr(v*v); s->fresh=true;
    }
    else if(k==L"1/x"){
        double v = toNum(s->display);
        if(v==0){ s->err=true; s->display=L"تقسیم بر صفر"; }
        else s->display = numStr(1.0/v);
        s->fresh=true;
    }
    else if(k==L"+"||k==L"\u2212"||k==L"-"||k==L"\u00D7"||k==L"*"||k==L"\u00F7"||k==L"/"){
        wchar_t op = k==L"+"?L'+' : (k==L"\u2212"||k==L"-")?L'-'
                   : (k==L"\u00D7"||k==L"*")?L'*' : L'/';
        if(s->pendOp && !s->fresh) calcEval(s);
        else if(!s->pendOp) s->acc = toNum(s->display);
        if(s->err){ InvalidateRect(h,NULL,TRUE); return; }
        s->pendOp = op;
        s->history = numStr(s->acc) + L" " + std::wstring(1,
            op==L'*'?L'\u00D7':op==L'/'?L'\u00F7':op==L'-'?L'\u2212':op);
        s->fresh = true;
    }
    else if(k==L"="){
        if(s->pendOp){
            std::wstring b = s->display;
            s->history += L" " + b + L" =";
            calcEval(s);
            s->pendOp = 0;
            s->fresh = true;
        }
    }
    InvalidateRect(h, NULL, FALSE);
}

// Visual key categories drive the per-button colour scheme (not the math —
// calcKey/calcEval stay unchanged). Classified from KEYS so the paint layer
// never duplicates the operator/magic-char logic.
enum CalcKeyType { CK_DIGIT, CK_OP, CK_FUNC, CK_EQ };
static CalcKeyType calcKeyType(int i){
    const wchar_t* k = KEYS[i];
    if(wcscmp(k,L"=")==0) return CK_EQ;
    if(wcscmp(k,L"+")==0 || wcscmp(k,L"\u2212")==0 ||
       wcscmp(k,L"\u00D7")==0 || wcscmp(k,L"\u00F7")==0) return CK_OP;
    if(wcscmp(k,L"C")==0 || wcscmp(k,L"\u232B")==0 ||
       wcscmp(k,L"%")==0 || wcscmp(k,L"\u00B1")==0 ||
       wcscmp(k,L".")==0) return CK_FUNC;
    return CK_DIGIT;
}

static void calcLayout(HWND h, RECT& disp, RECT cells[20]){
    RECT rc; GetClientRect(h,&rc);
    int pad = S(16);
    int dispH = S(118);
    disp = { pad, pad, rc.right-pad, pad+dispH };
    int gridY = disp.bottom + S(10);
    int gridW = rc.right - 2*pad;
    int gridH = rc.bottom - gridY - pad;
    int cw = gridW/4, ch = gridH/5;
    int inset = S(5);                 // gap between adjacent keys
    for(int r2=0;r2<5;r2++) for(int c=0;c<4;c++){
        int i = r2*4+c;
        // RTL: column 0 sits at the right edge (manual — no WS_EX_LAYOUTRTL)
        int cx = rc.right - pad - (c+1)*cw;
        cells[i] = { cx+inset, gridY+r2*ch+inset,
                     cx+cw-inset, gridY+(r2+1)*ch-inset };
    }
}
static int cellHit(HWND h, POINT pt){
    RECT d, cells[20]; calcLayout(h,d,cells);
    for(int i=0;i<20;i++) if(PtInRect(&cells[i],pt)) return i;
    return -1;
}

static LRESULT CALLBACK calcProc(HWND h, UINT m, WPARAM w, LPARAM l){
    CalcState* s = (CalcState*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_CREATE: {
        s = new CalcState();
        s->display=L"0"; s->acc=0; s->pendOp=0; s->fresh=true; s->err=false;
        s->hot=-1; s->press=-1;
        s->flash=-1;
        SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)s);
        return 0; }
    case WM_DESTROY: delete s; break;
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEMOVE: {
        if(!s) break;
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int hit = cellHit(h,pt);
        if(hit != s->hot){ s->hot=hit; InvalidateRect(h,NULL,FALSE); }
        TRACKMOUSEEVENT t={sizeof(t),TME_LEAVE,h,0}; TrackMouseEvent(&t);
        break; }
    case WM_MOUSELEAVE:
        if(s && s->hot!=-1){ s->hot=-1; InvalidateRect(h,NULL,FALSE);} break;
    case WM_LBUTTONDOWN: {
        if(!s) break;
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        s->press = cellHit(h,pt);
        if(s->press>=0) InvalidateRect(h,NULL,FALSE);
        break; }
    case WM_LBUTTONUP: {
        if(!s) break;
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int hit = cellHit(h,pt);
        if(hit>=0 && hit<20 && hit==s->press) calcKey(h, s, KEYS[hit]);
        s->press=-1; InvalidateRect(h,NULL,FALSE);
        break; }
    case WM_KEYDOWN: {
        if(!s) break;
        switch(w){
        case VK_ESCAPE: DestroyWindow(h); return 0;
        case VK_RETURN:   calcFlash(h,s,19); calcKey(h,s,L"="); return 0;
        case VK_BACK:     calcFlash(h,s,1);  calcKey(h,s,L"\u232B"); return 0;
        case VK_DELETE:   calcFlash(h,s,0);  calcKey(h,s,L"C"); return 0;
        case VK_ADD:      calcFlash(h,s,15); calcKey(h,s,L"+"); return 0;
        case VK_SUBTRACT: calcFlash(h,s,11); calcKey(h,s,L"-"); return 0;
        case VK_MULTIPLY: calcFlash(h,s,7);  calcKey(h,s,L"*"); return 0;
        case VK_DIVIDE:   calcFlash(h,s,3);  calcKey(h,s,L"/"); return 0;
        case VK_DECIMAL:  calcFlash(h,s,18); calcKey(h,s,L"."); return 0;
        }
        if(w>=L'0'&&w<=L'9' && !(GetKeyState(VK_SHIFT)&0x8000)){
            calcFlash(h,s,keyToIndex((wchar_t)w));
            calcKey(h,s,std::wstring(1,(wchar_t)w)); return 0;
        }
        if(w>=VK_NUMPAD0 && w<=VK_NUMPAD9){
            wchar_t d=(wchar_t)(L'0'+w-VK_NUMPAD0);
            calcFlash(h,s,keyToIndex(d));
            calcKey(h,s,std::wstring(1,d)); return 0;
        }
        break; }
    case WM_CHAR: {
        if(!s) break;
        wchar_t c=(wchar_t)w;
        if(c==L'+'||c==L'-'||c==L'*'||c==L'/'){
            calcFlash(h,s,keyToIndex(c));
            calcKey(h,s,std::wstring(1,c));
        }
        else if(c==L'.'||c==L','){
            calcFlash(h,s,18);
            calcKey(h,s,L".");
        }
        else if(c==L'%'){
            calcFlash(h,s,2);
            calcKey(h,s,L"%");
        }
        else if(c==L'='){
            calcFlash(h,s,19);
            calcKey(h,s,L"=");
        }
        return 0; }
    case WM_TIMER:
        // v1.93: end the keyboard-press flash after its 120ms window.
        if(w==99 && s){ KillTimer(h,99); s->flash=-1; InvalidateRect(h,NULL,FALSE); }
        return 0;
    case WM_PAINT: {
        if(!s){ PAINTSTRUCT ps; BeginPaint(h,&ps); EndPaint(h,&ps); return 0; }
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        // Window background: the deeper bg2 tone so the bright display/key
        // surfaces pop off the page in the light theme (on the plain bg they
        // previously blended in). Depth comes from the panel + key shadows.
        { HBRUSH bb=CreateSolidBrush(g_theme.bg2); FillRect(dc,&rc,bb); DeleteObject(bb); }

        RECT disp, cells[20]; calcLayout(h,disp,cells);
        int dispRad = S(16);
        int keyRad  = S(12);

        // ---- display panel: soft drop shadow + gradient card ----
        gpShadow(dc, disp, dispRad, S(10), 70);
        gpGradRoundRectBg(dc, disp, dispRad, g_theme.surfaceTop, g_theme.surface,
                          g_theme.border, g_theme.bg2);

        SetBkMode(dc, TRANSPARENT);
        // history line — muted, right-aligned. v1.94: keep ASCII digits (no
        // toFaDigits) — Persian Extended digits reverse under DT_RIGHT.
        SetTextColor(dc, g_theme.textDim);
        SelectObject(dc, g_fSmall);
        RECT hr={ disp.left+S(16), disp.top+S(12),
                  disp.right-S(16), disp.top+S(36) };
        DrawTextW(dc, s->history.c_str(), -1, &hr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_END_ELLIPSIS);
        // main number — right-aligned (calculator style). v1.94: keep ASCII
        // digits for the calculator display — Persian digits (U+06F0-U+06F9)
        // are classified as AN in the Unicode Bidi Algorithm and get reversed
        // by DrawTextW under DT_RIGHT. ASCII digits are always LTR-safe.
        SetTextColor(dc, s->err ? g_theme.danger : g_theme.text);
        SelectObject(dc, g_fHuge);
        RECT mr={ disp.left+S(16), disp.top+S(36),
                  disp.right-S(16), disp.bottom-S(12) };
        std::wstring shown = s->err ? toFaDigits(s->display) : groupNum(s->display);
        DWORD nfmt = DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_END_ELLIPSIS;
        if(s->err) nfmt |= DT_RTLREADING;
        DrawTextW(dc, shown.c_str(), -1, &mr, nfmt);

        // ---- keys: modern elevated rounded buttons ----
        for(int i=0;i<20;i++){
            CalcKeyType kt = calcKeyType(i);
            const wchar_t* k = KEYS[i];
            bool hot = (i==s->hot);
            bool pressed = (i==s->press);
            bool flashed = (i==s->flash);   // v1.93: keyboard-press flash

            // base fill (top/bottom gradient stops) + ink + border per category
            COLORREF fillTop, fillBot, txt, border;
            if(kt==CK_EQ){                       // hero action — solid accent
                fillTop=g_theme.accentHover; fillBot=g_theme.accent;
                txt=g_theme.accentText;      border=CLR_INVALID;
            } else if(kt==CK_OP){                // operators — soft accent wash
                fillTop=blendColor(g_theme.surface, g_theme.accent, 22);
                fillBot=blendColor(g_theme.surface, g_theme.accent, 12);
                txt=g_theme.accent;
                border=blendColor(g_theme.border, g_theme.accent, 30);
            } else if(kt==CK_FUNC){              // utilities — muted, recede
                fillTop=fillBot=blendColor(g_theme.surface2, g_theme.border, 15);
                txt=g_theme.textDim; border=g_theme.border;
            } else {                             // digits — clean card
                fillTop=blendColor(g_theme.surface, g_theme.surfaceTop, 50);
                fillBot=g_theme.surface;
                txt=g_theme.text;      border=g_theme.border;
            }

            // hover: lift + brighten
            if(hot){
                if(kt==CK_EQ){
                    fillBot=blendColor(g_theme.accent, g_theme.accentHover, 55);
                } else if(kt==CK_OP){
                    fillTop=blendColor(g_theme.surface, g_theme.accent, 32);
                    fillBot=blendColor(g_theme.surface, g_theme.accent, 20);
                } else if(kt==CK_FUNC){
                    fillTop=fillBot=g_theme.hover;
                } else {
                    fillTop=g_theme.surfaceTop;
                    fillBot=blendColor(g_theme.surface, g_theme.surfaceTop, 60);
                }
            }

            // v1.93: keyboard-press flash — a distinct accent tint so the operator
            // can see which on-screen key their keystroke landed on. This is
            // separate from a mouse press (which sinks + darkens the button); the
            // flash keeps the button in place and just washes it toward the accent.
            if(flashed && !pressed){
                fillTop = blendColor(fillBot, g_theme.accent, 40);
                fillBot = blendColor(fillBot, g_theme.accent, 20);
            }

            RECT cell = cells[i];

            // elevation: equals always casts an accent glow; others lift on hover.
            // A flashed key also drops its shadow so the accent wash reads as
            // "active" rather than floating.
            if(!pressed && !flashed){
                if(kt==CK_EQ)
                    gpShadowColor(dc, cell, keyRad, S(hot?8:6), hot?115:85, g_theme.accent);
                else if(hot)
                    gpShadow(dc, cell, keyRad, S(4), 50);
            }
            if(pressed){
                // sink down + lose the shadow; darken toward the page bg (darkens
                // in both themes). Equals keeps its gradient, just shifts.
                cell.top += S(2); cell.bottom += S(2);
                if(kt==CK_EQ){ fillTop=g_theme.accentHover; fillBot=g_theme.accent; }
                else fillTop=fillBot=blendColor(fillBot, g_theme.bg2, 34);
            }

            // corners filled with the window bg so the AA rounded edges blend
            if(kt==CK_FUNC)
                gpRoundRectBg(dc, cell, keyRad, fillBot, border, g_theme.bg2);
            else
                gpGradRoundRectBg(dc, cell, keyRad, fillTop, fillBot, border, g_theme.bg2);

            SetTextColor(dc, txt);
            SelectObject(dc, g_fTitle);
            DrawTextW(dc, k, -1, &cell,
                DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        }

        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    case WM_CLOSE: DestroyWindow(h); return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

void openCalculator(HWND owner){
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc={0};
        wc.lpfnWndProc=calcProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.lpszClassName=CALC_CLASS;
        RegisterClassW(&wc);
        reg=true;
    }
    // if already open — bring to front
    HWND ex = FindWindowW(CALC_CLASS, NULL);
    if(ex){ SetForegroundWindow(ex); return; }

    int w=S(340), hgt=S(520);
    RECT scr; SystemParametersInfoW(SPI_GETWORKAREA,0,&scr,0);
    int x = scr.right - w - S(60), y = (scr.bottom-hgt)/2;
    HWND c = CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW, CALC_CLASS,
        L"ماشین حساب — درمان پلاس",
        WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_VISIBLE,
        x,y,w,hgt, owner, NULL, g_hInst, NULL);
    SetForegroundWindow(c);
    SetFocus(c);
}
