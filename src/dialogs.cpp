// ============================================================================
//  dialogs.cpp — Win11-style centered login card + shift selection dialog
//  v1.0.1: dialogs are now OWNED POPUP windows (not children) so modal
//  EnableWindow() on the frame no longer disables the dialog itself.
//  No WS_EX_LAYOUTRTL anywhere (avoids GDI mirroring bugs) — RTL is manual.
//  v1.1.0 CRITICAL FIX: dialogs no longer call PostQuitMessage(0) in
//  WM_DESTROY. The old code queued a WM_QUIT that runModal never consumed
//  (its IsWindow() check exits the loop first), so the stray WM_QUIT leaked
//  into the MAIN message loop and silently terminated the whole app right
//  after every login / cancel — the "instant crash" bug.
// ============================================================================
#include "app.h"
#include <stdio.h>

// =============================================================== LOGIN =====
#define LGN_CLASS L"AzLogin"
#define ID_LG_USER  201
#define ID_LG_PASS  202
#define ID_LG_OK    203
#define ID_LG_CANCEL 204

struct LoginData {
    int role; User* out; bool* ok;
    HWND eUser, ePass, bOk, bCancel;
    std::wstring errMsg;
    int shake;
};

static const wchar_t* roleTitle(int r){
    switch(r){
        // v1.79.0: the reception entrance is now «حساب پرسنل» — one shared
        // staff login (doctor / nurse / receptionist / intern), with per-account
        // access ticks deciding what they can do afterwards.
        case 0: return L"ورود به حساب پرسنل";
        case 1: return L"ورود به پنل مدیریت";
        default:return L"پنل مخفی ادمین";
    }
}

// v1.86 PREMIUM CLAY LOGIN — card geometry with breathing room, taller to fit well depth
static void loginCard(HWND h, RECT& card){
    RECT rc; GetClientRect(h,&rc);
    int cw=S(440), chh=S(490);
    int cx=(rc.right-cw)/2, cy=(rc.bottom-chh)/2;
    card.left=cx; card.top=cy; card.right=cx+cw; card.bottom=cy+chh;
}
static void loginLayout(HWND h, LoginData* d){
    RECT c; loginCard(h,c);
    int cw=c.right-c.left;
    int ew=cw-S(96), ex=c.left+S(48);
    MoveWindow(d->eUser, ex, c.top+S(170), ew, S(34), TRUE);
    MoveWindow(d->ePass, ex, c.top+S(258), ew, S(34), TRUE);
    MoveWindow(d->bOk,    c.left+S(40), c.top+S(356), cw-S(80), S(50), TRUE);
    MoveWindow(d->bCancel,c.left+S(40), c.top+S(414), cw-S(80), S(42), TRUE);
}

static LRESULT CALLBACK loginProc(HWND h, UINT m, WPARAM w, LPARAM l){
    LoginData* d=(LoginData*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_CREATE: {
        CREATESTRUCTW* cs=(CREATESTRUCTW*)l;
        d=(LoginData*)cs->lpCreateParams;
        SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)d);
        DWORD es = WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL;
        d->eUser = CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_LG_USER,g_hInst,0);
        d->ePass = CreateWindowExW(0,L"EDIT",L"",es|ES_PASSWORD,0,0,10,10,h,(HMENU)ID_LG_PASS,g_hInst,0);
        SendMessageW(d->eUser,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        SendMessageW(d->ePass,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        d->bOk     = createFlatButton(h,ID_LG_OK,L"ورود",ICO_CHECK,BS_PRIMARY,0,0,10,10);
        d->bCancel = createFlatButton(h,ID_LG_CANCEL,L"انصراف",0,BS_OUTLINE,0,0,10,10);
        setFlatButtonBg(d->bOk,g_theme.surface);
        setFlatButtonBg(d->bCancel,g_theme.surface);
        loginLayout(h,d);
        return 0; }
    case WM_SIZE: if(d) loginLayout(h,d); return 0;
    case WM_CTLCOLOREDIT: {
        HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText);
        SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_CTLCOLORSTATIC: {
        HDC dc=(HDC)w;
        SetBkColor(dc,g_theme.surface);
        return (LRESULT)g_brSurface; }
    case WM_COMMAND: {
        if(!d) return 0;
        int id=LOWORD(w);
        if(id==ID_LG_OK){
            wchar_t ub[128], pb[128];
            GetWindowTextW(d->eUser,ub,128);
            GetWindowTextW(d->ePass,pb,128);
            std::wstring err;
            if(verifyLogin(trim(ub),pb,d->role,*d->out,err)){
                *d->ok = true; DestroyWindow(h);
            } else {
                d->errMsg = err; d->shake = 8;
                SetTimer(h, 7, 30, NULL);
                InvalidateRect(h,NULL,TRUE);
            }
        } else if(id==ID_LG_CANCEL){
            *d->ok=false; DestroyWindow(h);
        }
        return 0; }
    case WM_TIMER:
        if(w==7 && d){
            d->shake--;
            if(d->shake<=0){ d->shake=0; KillTimer(h,7); }
            InvalidateRect(h,NULL,TRUE);
        }
        return 0;
    case WM_CLOSE:
        if(d){ *d->ok=false; } DestroyWindow(h); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        // dim layer
        // v1.63.0 SCRIM: a flat mid-grey wash made the modal look like a
        // screenshot pasted over the app. It is now a soft vertical gradient
        // (darker at the edges, lighter behind the card) which reads as depth
        // and keeps the dialog visually attached to the window beneath it.
        gpGradRoundRect(dc,rc,0,
            g_dark?RGB(6,8,12):RGB(126,138,158),
            g_dark?RGB(14,18,26):RGB(168,178,194),CLR_INVALID);

        RECT c; loginCard(h,c);
        int sh = (d && d->shake>0) ? ((d->shake%2)? S(6):-S(6)) : 0;
        OffsetRect(&c, sh, 0);
        int cx=c.left, cy=c.top, cw=c.right-c.left;
        // ------------------------------------------------------------------
        //  v1.63.0 LOGIN CARD REDESIGN. The card used a hard offset rectangle
        //  as a "shadow" (a visible grey duplicate behind it) and a flat fill.
        //  It is now a genuinely elevated panel: a real layered blur shadow, a
        //  soft surface gradient, and a hairline accent ring; the role badge is
        //  a glowing gradient disc with a coloured halo instead of a flat blob.
        // ------------------------------------------------------------------
        // v1.86 PREMIUM CLAY LOGIN — deeper double-shadow, white glass panel, accent ring
        COLORREF roleCol = (d&&d->role==2)?g_theme.danger:g_theme.accent;
        // double shadow for clay extrusion
        gpShadowColor(dc,c,S(26),S(30),60,roleCol);
        gpShadow(dc,c,S(26),S(30),110);
        // glass white panel with subtle gradient and inset highlight
        gpGradRoundRectBg(dc,c,S(28),
            blendColor(g_theme.surfaceTop, RGB(255,255,255), 40),
            g_theme.surface,
            blendColor(g_theme.border,roleCol,28),
            g_dark?RGB(6,8,12):RGB(160,172,190));
        // inset top highlight line for clay
        {
            RECT innerTop = c; innerTop.bottom = c.top + S(50); innerTop.left+=S(2); innerTop.right-=S(2); innerTop.top+=S(2);
            if(innerTop.bottom>innerTop.top+1)
                gpFillAlpha(dc, innerTop, S(26), RGB(255,255,255), g_dark?22:38);
        }

        SetBkMode(dc,TRANSPARENT);
        // role badge — circular brand logo halo+ring, concave
        int ir=S(32);
        RECT ic={cx+cw/2-ir,cy+S(30),cx+cw/2+ir,cy+S(30)+2*ir};
        { RECT halo=ic; InflateRect(&halo,S(8),S(8));
          gpFillAlpha(dc,halo,(halo.bottom-halo.top)/2,roleCol,g_dark?68:56); }
        gpShadowColor(dc,ic,ir,S(12),120,roleCol);
        gpShadow(dc,ic,ir,S(8),80);
        if(!gpDrawImageResCircle(dc, IMG_LOGO, ic)){
            gpGradRoundRect(dc,ic,ir,
                blendColor(roleCol,RGB(255,255,255),18),roleCol,CLR_INVALID);
            RECT ii={ic.left+S(14),ic.top+S(14),ic.right-S(14),ic.bottom-S(14)};
            drawIcon(dc, (d&&d->role==2)?ICO_SHIELD:ICO_USER, ii, RGB(255,255,255), S(2)+1);
        }
        gpRoundRect(dc, ic, ir, CLR_INVALID, blendColor(roleCol, g_theme.border, 36));
        // inner highlight
        {
            RECT innerI = ic; InflateRect(&innerI, -S(2), -S(2));
            if(innerI.right>innerI.left) gpRoundRect(dc, innerI, S(30), CLR_INVALID, RGB(255,255,255), 140);
        }

        SetTextColor(dc,g_theme.sectionInk);
        SelectObject(dc,g_fTitle);
        RECT tr={cx,cy+S(106),cx+cw,cy+S(140)};
        DrawTextW(dc,roleTitle(d?d->role:0),-1,&tr, DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        // small underline accent bar under title
        {
            int uw=S(56), ux=cx+cw/2-uw/2, uy=tr.bottom-S(2);
            RECT ur={ux,uy,ux+uw,uy+S(3)};
            gpGradRoundRectBgH(dc, ur, S(2), g_theme.accent, g_theme.accent2, CLR_INVALID, CLR_INVALID);
        }

        // labels — stronger readable ink
        SelectObject(dc,g_fLabel);
        SetTextColor(dc,g_theme.labelInk);
        RECT lu={cx+S(48),cy+S(148),cx+cw-S(48),cy+S(170)};
        DrawTextW(dc,L"نام کاربری",-1,&lu,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        RECT lp={cx+S(48),cy+S(230),cx+cw-S(48),cy+S(252)};
        DrawTextW(dc,L"رمز عبور",-1,&lp,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);

        // input wells — clay concave inset + focus halo
        { HWND foc=GetFocus();
          RECT bu={cx+S(40),cy+S(174),cx+cw-S(40),cy+S(214)};
          RECT bp={cx+S(40),cy+S(256),cx+cw-S(40),cy+S(296)};
          bool fu=(d && d->eUser && foc==d->eUser);
          bool fp=(d && d->ePass && foc==d->ePass);
          for(int k=0;k<2;k++){
              RECT wr2 = k? bp : bu;
              bool fo  = k? fp : fu;
              if(fo) gpShadowColor(dc,wr2,S(11),S(7),90,g_theme.accent);
              gpGradRoundRectBg(dc,wr2,S(11),
                  blendColor(g_theme.inputBg, g_theme.border, 20),
                  g_theme.inputBg,
                  fo?g_theme.accent:blendColor(g_theme.border,g_theme.accent,18),
                  g_theme.bg);
              // inset highlight top edge
              if(!fo) {
                  RECT hi={wr2.left+S(4), wr2.top+S(2), wr2.right-S(4), wr2.top+S(8)};
                  if(hi.bottom>hi.top) gpFillAlpha(dc, hi, S(8), RGB(255,255,255), 120);
              }
          } }

        if(d && !d->errMsg.empty()){
            SetTextColor(dc,g_theme.danger);
            SelectObject(dc,g_fUIB);
            RECT er={cx+S(40),cy+S(308),cx+cw-S(40),cy+S(344)};
            DrawTextW(dc,d->errMsg.c_str(),-1,&er, DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        }
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    // NOTE: no PostQuitMessage here — runModal exits via IsWindow() check.
    }
    return DefWindowProcW(h,m,w,l);
}

static void regClass(const wchar_t* name, WNDPROC proc){
    WNDCLASSW wc={0};
    wc.lpfnWndProc=proc; wc.hInstance=g_hInst;
    wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.lpszClassName=name;
    RegisterClassW(&wc);
}

//  Nested message loop until the popup closes. The popup is an OWNED
//  top-level window, so disabling the frame doesn't disable it.
static void runModal(HWND overlay, HWND parent, HWND firstFocus,
                     int idUserEdit, int idOkBtn){
    if(!overlay || !IsWindow(overlay)) return;   // creation failed — never
                                                 // leave the frame disabled
    EnableWindow(parent, FALSE);
    SetForegroundWindow(overlay);
    if(firstFocus) SetFocus(firstFocus);
    MSG msg;
    while(IsWindow(overlay) && GetMessageW(&msg,NULL,0,0)){
        if(msg.message==WM_QUIT){               // never ours — re-post and bail
            PostQuitMessage((int)msg.wParam);
            break;
        }
        if(msg.message==WM_KEYDOWN && msg.wParam==VK_TAB &&
           GetAncestor(msg.hwnd,GA_ROOT)==overlay){
            HWND f=GetFocus();
            HWND nxt=GetNextDlgTabItem(overlay,f,
                (GetKeyState(VK_SHIFT)&0x8000)?TRUE:FALSE);
            if(nxt && nxt!=f){ SetFocus(nxt); SendMessageW(nxt,EM_SETSEL,0,-1); }
            continue;
        }
        if(msg.message==WM_KEYDOWN && msg.wParam==VK_RETURN &&
           GetAncestor(msg.hwnd,GA_ROOT)==overlay){
            HWND f=GetFocus();
            wchar_t cls[32]={0}; if(f) GetClassNameW(f,cls,32);
            if(!wcscmp(cls,L"EDIT")){
                if(idUserEdit && GetDlgCtrlID(f)==idUserEdit){
                    HWND nxt=GetNextDlgTabItem(overlay,f,FALSE);
                    if(nxt){ SetFocus(nxt); SendMessageW(nxt,EM_SETSEL,0,-1); }
                } else if(idOkBtn){
                    SendMessageW(overlay,WM_COMMAND,MAKEWPARAM(idOkBtn,0),0);
                }
                continue;
            }
            if(idOkBtn){ SendMessageW(overlay,WM_COMMAND,MAKEWPARAM(idOkBtn,0),0); continue; }
        }
        if(msg.message==WM_KEYDOWN && msg.wParam==VK_ESCAPE &&
           GetAncestor(msg.hwnd,GA_ROOT)==overlay){
            SendMessageW(overlay,WM_CLOSE,0,0);
            continue;
        }
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    SetFocus(parent);
}

bool showLoginDialog(HWND parent, int role, User& out){
    static bool reg=false;
    if(!reg){ regClass(LGN_CLASS, loginProc); reg=true; }
    bool ok=false;
    LoginData d; d.role=role; d.out=&out; d.ok=&ok; d.shake=0;
    d.eUser=d.ePass=d.bOk=d.bCancel=NULL;
    RECT fr; GetWindowRect(parent,&fr);
    HWND ov = CreateWindowExW(0,LGN_CLASS,L"",
        WS_POPUP|WS_VISIBLE,
        fr.left,fr.top,fr.right-fr.left,fr.bottom-fr.top,
        parent,NULL,g_hInst,&d);
    if(!ov) return false;
    runModal(ov, parent, d.eUser, ID_LG_USER, ID_LG_OK);
    return ok;
}

// =============================================================== SHIFT =====
#define SH_CLASS L"AzShift"
#define ID_SH_AUTO   301
#define ID_SH_S0     302
#define ID_SH_S1     303
#define ID_SH_S2     304
#define ID_SH_OK     305
#define ID_SH_CANCEL 306

struct ShiftData {
    int* shift; bool* ok;
    bool autoMode;
    int sel;
    HWND bAuto, b0, b1, b2, bOk, bCancel;
};
static void shiftCard(HWND h, RECT& card){
    RECT rc; GetClientRect(h,&rc);
    int cw=S(470), chh=S(430);
    card.left=(rc.right-cw)/2; card.top=(rc.bottom-chh)/2;
    card.right=card.left+cw; card.bottom=card.top+chh;
}
static void shiftRefresh(ShiftData* d){
    int as = detectShift();
    if(d->autoMode) d->sel = as;
    const wchar_t* names[3]={L"شیفت صبح  (۶:۰۰ تا ۱۴:۳۰)",
                             L"شیفت بعد از ظهر  (۱۴:۳۰ تا ۲۲:۳۰)",
                             L"شیفت شب  (۲۲:۳۰ تا ۶:۰۰)"};
    HWND bs[3]={d->b0,d->b1,d->b2};
    for(int i=0;i<3;i++){
        std::wstring t = names[i];
        if(i==as)            t = L"\u25CF  " + t + L"  (شیفت جاری)";
        else if(i==d->sel)   t = L"\u25CF  " + t;
        SetWindowTextW(bs[i], t.c_str());
        // v1.4.0: in manual mode only the CURRENT (detected) shift is valid —
        // the two non-matching shifts are disabled so the user can't pick an
        // out-of-hours shift by mistake. In auto mode all are disabled (locked).
        bool enabled = d->autoMode ? false : (i==as);
        EnableWindow(bs[i], enabled);
        // These controls all sit on the card. Selection is already conveyed by
        // the bullet/text and enabled state; keep the rounded corner host tied
        // to the live card surface instead of storing a body colour as a host.
        setFlatButtonBg(bs[i],g_theme.surface);
    }
    // keep the manual selection aligned with the only valid shift
    if(!d->autoMode) d->sel = as;
    SetWindowTextW(d->bAuto, d->autoMode
        ? L"\u2611  حالت خودکار فعال است — تشخیص بر اساس ساعت ایران"
        : L"\u2610  حالت دستی — تنها شیفت جاری قابل انتخاب است");
}
static void shiftLayout(HWND h, ShiftData* d){
    RECT c; shiftCard(h,c);
    int cw=c.right-c.left;
    int ew=cw-S(60), ex=c.left+S(30);
    MoveWindow(d->bAuto, ex, c.top+S(100), ew, S(44), TRUE);
    MoveWindow(d->b0,    ex, c.top+S(160), ew, S(48), TRUE);
    MoveWindow(d->b1,    ex, c.top+S(216), ew, S(48), TRUE);
    MoveWindow(d->b2,    ex, c.top+S(272), ew, S(48), TRUE);
    MoveWindow(d->bOk,   ex, c.top+S(344), ew/2-S(6), S(46), TRUE);
    MoveWindow(d->bCancel, ex+ew/2+S(6), c.top+S(344), ew/2-S(6), S(46), TRUE);
}
static LRESULT CALLBACK shiftProc(HWND h, UINT m, WPARAM w, LPARAM l){
    ShiftData* d=(ShiftData*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_CREATE: {
        CREATESTRUCTW* cs=(CREATESTRUCTW*)l;
        d=(ShiftData*)cs->lpCreateParams;
        SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)d);
        d->bAuto  = createFlatButton(h,ID_SH_AUTO,L"",0,BS_OUTLINE,0,0,10,10);
        d->b0     = createFlatButton(h,ID_SH_S0,L"",0,BS_OUTLINE,0,0,10,10);
        d->b1     = createFlatButton(h,ID_SH_S1,L"",0,BS_OUTLINE,0,0,10,10);
        d->b2     = createFlatButton(h,ID_SH_S2,L"",0,BS_OUTLINE,0,0,10,10);
        d->bOk    = createFlatButton(h,ID_SH_OK,L"تأیید و ورود",ICO_CHECK,BS_PRIMARY,0,0,10,10);
        d->bCancel= createFlatButton(h,ID_SH_CANCEL,L"انصراف",0,BS_OUTLINE,0,0,10,10);
        HWND buttons[]={d->bAuto,d->b0,d->b1,d->b2,d->bOk,d->bCancel};
        for(HWND b:buttons) setFlatButtonBg(b,g_theme.surface);
        shiftRefresh(d);
        shiftLayout(h,d);
        SetTimer(h, 9, 30000, NULL);
        return 0; }
    case WM_TIMER:
        if(w==9 && d && d->autoMode){ shiftRefresh(d); InvalidateRect(h,NULL,TRUE);}
        return 0;
    case WM_SIZE: if(d) shiftLayout(h,d); return 0;
    case WM_COMMAND: {
        if(!d) return 0;
        int id=LOWORD(w);
        switch(id){
        case ID_SH_AUTO:
            d->autoMode = !d->autoMode;
            setSetting(L"shift_auto", d->autoMode?L"1":L"0");
            shiftRefresh(d); InvalidateRect(h,NULL,TRUE);
            break;
        case ID_SH_S0: case ID_SH_S1: case ID_SH_S2:
            if(!d->autoMode){ d->sel = id-ID_SH_S0; shiftRefresh(d); }
            break;
        case ID_SH_OK:
            *d->shift = d->autoMode ? detectShift() : d->sel;
            *d->ok = true; KillTimer(h,9); DestroyWindow(h);
            break;
        case ID_SH_CANCEL:
            *d->ok=false; KillTimer(h,9); DestroyWindow(h);
            break;
        }
        return 0; }
    case WM_CTLCOLORSTATIC: {
        HDC dc=(HDC)w; SetBkColor(dc,g_theme.surface);
        return (LRESULT)g_brSurface; }
    case WM_CLOSE:
        if(d){ *d->ok=false; } KillTimer(h,9); DestroyWindow(h); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        // v1.63.0 SCRIM: a flat mid-grey wash made the modal look like a
        // screenshot pasted over the app. It is now a soft vertical gradient
        // (darker at the edges, lighter behind the card) which reads as depth
        // and keeps the dialog visually attached to the window beneath it.
        gpGradRoundRect(dc,rc,0,
            g_dark?RGB(6,8,12):RGB(126,138,158),
            g_dark?RGB(14,18,26):RGB(168,178,194),CLR_INVALID);
        RECT c; shiftCard(h,c);
        // v1.86 PREMIUM CLAY SHIFT — double shadow + white glass gradient
        gpShadowColor(dc,c,S(22),S(32),52,g_theme.accent);
        gpShadow(dc,c,S(22),S(32),120);
        gpGradRoundRectBg(dc,c,S(22),
            blendColor(g_theme.surfaceTop, RGB(255,255,255), 40),
            g_theme.surface,
            blendColor(g_theme.border, g_theme.accent, 24),
            g_dark?RGB(6,8,12):RGB(160,172,190));
        // top highlight
        {
            RECT ht = c; ht.bottom = c.top + S(60); ht.left+=S(2); ht.right-=S(2); ht.top+=S(2);
            if(ht.bottom>ht.top+1) gpFillAlpha(dc, ht, S(22), RGB(255,255,255), g_dark?18:34);
        }
        SetBkMode(dc,TRANSPARENT);
        SetTextColor(dc,g_theme.sectionInk);
        SelectObject(dc,g_fTitle);
        RECT tr={c.left,c.top+S(24),c.right,c.top+S(58)};
        DrawTextW(dc,L"انتخاب شیفت کاری",-1,&tr,
            DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        SelectObject(dc,g_fSmall);
        SetTextColor(dc,g_theme.textDim);
        SYSTEMTIME st=iranNow();
        std::wstring sub = L"ساعت ایران: " + toFaDigits(iranTimeStr(st,false))
            + L"  —  شیفت تشخیص داده‌شده: " + shiftName(detectShift());
        RECT sr={c.left,c.top+S(62),c.right,c.top+S(90)};
        DrawTextW(dc,sub.c_str(),-1,&sr,
            DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    // NOTE: no PostQuitMessage here — runModal exits via IsWindow() check.
    }
    return DefWindowProcW(h,m,w,l);
}
bool showShiftDialog(HWND parent, int& shift){
    static bool reg=false;
    if(!reg){ regClass(SH_CLASS, shiftProc); reg=true; }
    bool ok=false;
    ShiftData d; d.shift=&shift; d.ok=&ok;
    d.autoMode = getSetting(L"shift_auto", L"1") == L"1";
    d.sel = detectShift();
    d.bAuto=d.b0=d.b1=d.b2=d.bOk=d.bCancel=NULL;
    RECT fr; GetWindowRect(parent,&fr);
    HWND ov=CreateWindowExW(0,SH_CLASS,L"",
        WS_POPUP|WS_VISIBLE,
        fr.left,fr.top,fr.right-fr.left,fr.bottom-fr.top,
        parent,NULL,g_hInst,&d);
    if(!ov) return false;
    runModal(ov,parent,NULL,0,ID_SH_OK);
    return ok;
}

// ============================================================ PROFILE ======
//  Edit display-name + photo. The change is NOT applied immediately; it is
//  queued as a ProfReq that management must approve (see manage.inc). The
//  current name is shown read-only, the user types a new name and may pick a
//  photo, then «تأیید» queues the request.
#define PF_CLASS     L"AzProfile"
#define ID_PF_OLD     701
#define ID_PF_NEW     702
#define ID_PF_PICK    703
#define ID_PF_OK      704
#define ID_PF_CANCEL  705

struct ProfData {
    bool* ok;
    std::wstring user, oldName, oldPhoto, newPhoto;
    HWND eOld, eNew, bPick, bOk, bCancel;
};
static void profCard(HWND h, RECT& card){
    RECT rc; GetClientRect(h,&rc);
    int cw=S(460), chh=S(420);
    card.left=(rc.right-cw)/2; card.top=(rc.bottom-chh)/2;
    card.right=card.left+cw; card.bottom=card.top+chh;
}
static void profLayout(HWND h, ProfData* d){
    RECT c; profCard(h,c);
    int cw=c.right-c.left;
    int ew=cw-S(60), ex=c.left+S(30);
    MoveWindow(d->eOld,  ex, c.top+S(150), ew, S(32), TRUE);
    MoveWindow(d->eNew,  ex, c.top+S(216), ew, S(32), TRUE);
    MoveWindow(d->bPick, ex, c.top+S(272), ew, S(40), TRUE);
    MoveWindow(d->bOk,   ex, c.top+S(338), ew/2-S(6), S(46), TRUE);
    MoveWindow(d->bCancel, ex+ew/2+S(6), c.top+S(338), ew/2-S(6), S(46), TRUE);
}
static LRESULT CALLBACK profProc(HWND h, UINT m, WPARAM w, LPARAM l){
    ProfData* d=(ProfData*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_CREATE: {
        CREATESTRUCTW* cs=(CREATESTRUCTW*)l;
        d=(ProfData*)cs->lpCreateParams;
        SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)d);
        DWORD es = WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL;
        d->eOld = CreateWindowExW(0,L"EDIT",d->oldName.c_str(),
            es|ES_READONLY,0,0,10,10,h,(HMENU)ID_PF_OLD,g_hInst,0);
        d->eNew = CreateWindowExW(0,L"EDIT",d->oldName.c_str(),
            es,0,0,10,10,h,(HMENU)ID_PF_NEW,g_hInst,0);
        SendMessageW(d->eOld,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        SendMessageW(d->eNew,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        SendMessageW(d->eNew,EM_SETSEL,0,-1);
        d->bPick   = createFlatButton(h,ID_PF_PICK,L"انتخاب عکس پروفایل…",ICO_USER,BS_OUTLINE,0,0,10,10);
        d->bOk     = createFlatButton(h,ID_PF_OK,L"تأیید",ICO_CHECK,BS_PRIMARY,0,0,10,10);
        d->bCancel = createFlatButton(h,ID_PF_CANCEL,L"انصراف",0,BS_OUTLINE,0,0,10,10);
        setFlatButtonBg(d->bPick,g_theme.surface);
        setFlatButtonBg(d->bOk,g_theme.surface);
        setFlatButtonBg(d->bCancel,g_theme.surface);
        profLayout(h,d);
        return 0; }
    case WM_SIZE: if(d) profLayout(h,d); return 0;
    case WM_CTLCOLOREDIT: {
        HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText);
        SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_CTLCOLORSTATIC: {
        HDC dc=(HDC)w; SetBkColor(dc,g_theme.surface);
        return (LRESULT)g_brSurface; }
    case WM_COMMAND: {
        if(!d) return 0;
        int id=LOWORD(w);
        if(id==ID_PF_PICK){
            wchar_t file[MAX_PATH]={0};
            OPENFILENAMEW ofn={0}; ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=h;
            ofn.lpstrFilter=L"تصاویر\0*.png;*.jpg;*.jpeg;*.bmp\0همه فایل‌ها\0*.*\0";
            ofn.lpstrFile=file; ofn.nMaxFile=MAX_PATH;
            ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
            if(GetOpenFileNameW(&ofn)){
                d->newPhoto=file;
                std::wstring lbl=L"عکس انتخاب شد: ";
                std::wstring p=d->newPhoto;
                size_t sl=p.find_last_of(L"\\/");
                lbl += (sl==std::wstring::npos)? p : p.substr(sl+1);
                SetWindowTextW(d->bPick, lbl.c_str());
            }
            InvalidateRect(h,NULL,TRUE);
        } else if(id==ID_PF_OK){
            wchar_t nb[256]={0}; GetWindowTextW(d->eNew,nb,256);
            std::wstring nn=trim(nb);
            if(nn.empty() && d->newPhoto.empty()){
                MessageBoxW(h,L"نام جدید را وارد کنید یا عکسی انتخاب نمایید.",
                    L"پروفایل کاربر", MB_OK|MB_ICONWARNING);
                return 0;
            }
            int r=MessageBoxW(h,
                L"این تغییر بلافاصله اعمال نمی‌شود.\n"
                L"درخواست شما برای تأیید به مدیریت ارسال خواهد شد.\n\nادامه می‌دهید؟",
                L"تأیید درخواست", MB_YESNO|MB_ICONQUESTION);
            if(r!=IDYES) return 0;
            ProfReq req;
            req.user    = d->user;
            req.oldName = d->oldName;
            req.newName = nn.empty()? d->oldName : nn;
            req.oldPhoto= d->oldPhoto;
            req.newPhoto= d->newPhoto;
            pushProfReq(req);
            *d->ok=true;
            MessageBoxW(h,
                L"درخواست تغییر پروفایل ثبت شد.\n"
                L"پس از تأیید مدیریت اعمال خواهد شد و در کارتابل به شما اطلاع داده می‌شود.",
                L"پروفایل کاربر", MB_OK|MB_ICONINFORMATION);
            DestroyWindow(h);
        } else if(id==ID_PF_CANCEL){
            *d->ok=false; DestroyWindow(h);
        }
        return 0; }
    case WM_CLOSE: if(d){ *d->ok=false; } DestroyWindow(h); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        // v1.63.0 SCRIM: a flat mid-grey wash made the modal look like a
        // screenshot pasted over the app. It is now a soft vertical gradient
        // (darker at the edges, lighter behind the card) which reads as depth
        // and keeps the dialog visually attached to the window beneath it.
        gpGradRoundRect(dc,rc,0,
            g_dark?RGB(6,8,12):RGB(126,138,158),
            g_dark?RGB(14,18,26):RGB(168,178,194),CLR_INVALID);
        RECT c; profCard(h,c);
        // v1.86 PREMIUM CLAY PROFILE — double shadow + white glass
        gpShadowColor(dc,c,S(22),S(32),52,g_theme.accent);
        gpShadow(dc,c,S(22),S(32),120);
        gpGradRoundRectBg(dc,c,S(22),
            blendColor(g_theme.surfaceTop, RGB(255,255,255), 40),
            g_theme.surface,
            blendColor(g_theme.border, g_theme.accent, 24),
            g_dark?RGB(6,8,12):RGB(160,172,190));
        {
            RECT ht = c; ht.bottom = c.top + S(60); ht.left+=S(2); ht.right-=S(2); ht.top+=S(2);
            if(ht.bottom>ht.top+1) gpFillAlpha(dc, ht, S(22), RGB(255,255,255), g_dark?18:34);
        }
        SetBkMode(dc,TRANSPARENT);
        SetTextColor(dc,g_theme.sectionInk);
        SelectObject(dc,g_fTitle);
        RECT tr={c.left,c.top+S(22),c.right,c.top+S(56)};
        DrawTextW(dc,L"ویرایش پروفایل کاربر",-1,&tr,
            DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        SelectObject(dc,g_fSmall);
        SetTextColor(dc,g_theme.textDim);
        RECT sr={c.left,c.top+S(60),c.right,c.top+S(88)};
        DrawTextW(dc,L"تغییر پس از تأیید مدیریت اعمال می‌شود",-1,&sr,
            DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        // labels above the edit boxes
        SelectObject(dc,g_fUI);
        SetTextColor(dc,g_theme.textDim);
        RECT l1={c.left+S(30),c.top+S(126),c.right-S(30),c.top+S(148)};
        DrawTextW(dc,L"نام فعلی",-1,&l1,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        RECT l2={c.left+S(30),c.top+S(192),c.right-S(30),c.top+S(214)};
        DrawTextW(dc,L"نام جدید",-1,&l2,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}
bool showProfileDialog(HWND parent){
    static bool reg=false;
    if(!reg){ regClass(PF_CLASS, profProc); reg=true; }
    bool ok=false;
    ProfData d; d.ok=&ok;
    d.user    = g_session.user.username;
    d.oldName = g_session.user.fullname;
    d.oldPhoto= getSetting(L"photo_"+d.user, L"");
    d.newPhoto= L"";
    d.eOld=d.eNew=d.bPick=d.bOk=d.bCancel=NULL;
    RECT fr; GetWindowRect(parent,&fr);
    HWND ov=CreateWindowExW(0,PF_CLASS,L"",
        WS_POPUP|WS_VISIBLE,
        fr.left,fr.top,fr.right-fr.left,fr.bottom-fr.top,
        parent,NULL,g_hInst,&d);
    if(!ov) return false;
    runModal(ov,parent,d.eNew,ID_PF_NEW,ID_PF_OK);
    return ok;
}
