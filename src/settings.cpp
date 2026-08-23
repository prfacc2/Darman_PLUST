// ============================================================================
//  settings.cpp — CENTERED modal settings panel (v1.4.0)
//
//  Redesign brief:
//   • The settings panel now opens in the CENTER of the screen (like the
//     "messenger" settings dialogs), not slid-over from the edge.
//   • ESC closes it. Clicking the dim scrim closes it.
//   • The rows shown depend on WHO is signed in:
//       - NOT logged in (guest)      → only «بررسی به‌روزرسانی» + «تغییر تم».
//                                       Profile shows «کاربر مهمان», no avatar
//                                       editing.
//       - پذیرش (reception)           → تغییر تم، بررسی آپدیت، تنظیمات اعلان،
//                                       تنظیمات چاپگر, و profile (name+photo)
//                                       that must be APPROVED by management.
//       - مدیریت / admin              → same + printer-design access toggle.
//   • Theme switch & check-for-update were removed from the header and live
//     here now.
//
//  Pure owner-drawn (GDI+) so it inherits the theme automatically.
// ============================================================================
#include "app.h"
#include "print_designer.h"   // §A: PrintDesigner_Open — so «دیزاین چاپ» opens
                              // the print DESIGNER, not the printer settings.
#include <stdio.h>

#define SET_CLASS  L"AzSettings"
#define IDC_SRV     900     // server-url edit box

// ----- the option rows we may draw (id drives the click action) ------------
enum {
    ROW_THEME = 1,
    ROW_UPDATE,
    ROW_NOTIFY,       // notification settings (reception+)
    ROW_PRINTER,      // printer settings (reception+)
    ROW_PRINTDESIGN,  // print designer (reception+/manager)
    ROW_PROFILE,      // edit profile name+photo (reception+, needs approval)
    ROW_DENSITY,
    ROW_AUTOPRINT,
    ROW_SAVEDMSGS,    // v1.8.0: enable «پیام‌های ذخیره‌شده» (off by default)
    ROW_SERVER,
    ROW_ABOUT,
    ROW_LOGOUT,
    ROW_ZOOMRESET     // v1.84: «ریست کردن بزرگنمایی»
};

struct RowDef { int id; const wchar_t* label; int icon; const wchar_t* hint; bool toggle; bool disabled; };

struct SetState {
    HWND owner;
    HWND eServer;
    int  hot;
    bool dark, compact, autoPrint, notify, savedMsgs;
    std::vector<RowDef> rows;       // built per-role on open
    int  role;                      // -1 guest, 0 reception, 1 manage, 2 admin
};
static HWND s_set = NULL;
static SetState* s_st = NULL;
// v1.7.0 perf: cache of the EXPENSIVE static layers (scrim + GDI+ shadow +
// alpha blur + card gradient + cover + avatar + identity). Rebuilt only when
// the window is sized or the theme flips — NOT on every mouse move. Hover
// repaints just blit this cache for the dirty strip, then redraw the rows.
static HDC     s_bgDC  = NULL;
static HBITMAP s_bgBmp = NULL;
static HGDIOBJ s_bgOld = NULL;
static int     s_bgW=0, s_bgH=0;
static void freeBgCache(){
    if(s_bgDC){ SelectObject(s_bgDC,s_bgOld); DeleteDC(s_bgDC); s_bgDC=NULL; }
    if(s_bgBmp){ DeleteObject(s_bgBmp); s_bgBmp=NULL; }
    s_bgW=s_bgH=0;
}

// ---- geometry: a centered card -------------------------------------------
static int cardW(){ return S(460); }
static int headerH(){ return S(212); }   // cover + avatar + name + role (own lines) — §5 tuned
static int rowH(){ return S(58); }
static int cardH(){
    int n = s_st ? (int)s_st->rows.size() : 5;
    return headerH() + S(14) + n*rowH() + S(20);
}
static RECT cardRect(HWND h){
    RECT rc; GetClientRect(h,&rc);
    int w=cardW(), hh=cardH();
    if(hh > rc.bottom-S(40)) hh = rc.bottom-S(40);
    RECT c={ (rc.right-w)/2, (rc.bottom-hh)/2, (rc.right+w)/2, (rc.bottom+hh)/2 };
    return c;
}
static RECT rowRect(const RECT& card, int index){
    int x0=card.left+S(20), x1=card.right-S(20);
    int y =card.top+headerH()+S(8)+index*rowH();
    RECT r={x0,y,x1,y+rowH()-S(8)};
    return r;
}
static int hitRow(HWND h, POINT pt){
    RECT card=cardRect(h);
    if(!PtInRect(&card,pt)) return -1;       // outside card → scrim (close)
    if(!s_st) return 0;
    for(size_t i=0;i<s_st->rows.size();i++){
        RECT r=rowRect(card,(int)i);
        if(PtInRect(&r,pt)) return s_st->rows[i].id;
    }
    return 0;
}
// v1.7.0 perf: return the on-screen rectangle for a hot-id (a row, the close
// button, or nothing). Used so WM_MOUSEMOVE invalidates ONLY the small region
// whose hover state changed — never the whole full-screen scrim/shadow/blur
// (that full repaint caused the stuttering on the settings window).
// §5: the close (×) button lives at the TOP-RIGHT corner of the card. A single
// helper defines its rect so paint, hover and hit-testing can never drift apart.
static RECT closeBtnRect(const RECT& card){
    RECT cb={card.right-S(40),card.top+S(14),card.right-S(14),card.top+S(40)};
    return cb;
}

static bool hotRectFor(HWND h, int id, RECT& out){
    if(id==0 || id==-1) return false;
    RECT card=cardRect(h);
    if(id==-2){ // close (×) button top-right (§5)
        RECT cb=closeBtnRect(card);
        out = {cb.left-S(4),cb.top-S(4),cb.right+S(4),cb.bottom+S(4)};
        return true;
    }
    if(!s_st) return false;
    for(size_t i=0;i<s_st->rows.size();i++){
        if(s_st->rows[i].id==id){
            RECT r=rowRect(card,(int)i);
            // pad a couple px so the rounded border/hover ring is fully covered
            out = {r.left-S(2),r.top-S(2),r.right+S(2),r.bottom+S(2)};
            return true;
        }
    }
    return false;
}

// position the server edit box (only shown for admin/manage) ----------------
static int serverRowIndex(){
    if(!s_st) return -1;
    for(size_t i=0;i<s_st->rows.size();i++)
        if(s_st->rows[i].id==ROW_SERVER) return (int)i;
    return -1;
}
static void layoutServerEdit(HWND h){
    if(!s_st || !s_st->eServer) return;
    int idx=serverRowIndex();
    if(idx<0){ ShowWindow(s_st->eServer,SW_HIDE); return; }
    RECT card=cardRect(h);
    RECT r=rowRect(card,idx);
    // v1.9.0: sit the edit box on its own line BELOW the label (which is drawn
    // at the top of the row) so the two never overlap. Leave room for the row
    // icon at the right edge.
    MoveWindow(s_st->eServer, r.left+S(14), r.top+S(28),
               (r.right-r.left)-S(28), S(22), TRUE);
    ShowWindow(s_st->eServer, SW_SHOW);
}

// ---------------------------------------------------------------- actions --
// ---------------------------------------------------------------------------
//  v1.9.0 — settings CHANGE-REQUEST workflow.
//  For reception / staff (role < 1) a settings or printer-type change must NOT
//  be applied immediately. Instead:
//    1) a confirm dialog «آیا از ذخیرهٔ این تنظیمات اطمینان دارید؟»
//    2) on confirm, the change is QUEUED for management (pushSetReqEx) — never
//       applied locally — and a notice «این تنظیمات برای مدیریت ارسال شد. پس از
//       تأیید اعمال خواهد شد.» is shown.
//  Management (role >= 1) keeps applying changes instantly.
//  Returns TRUE when the caller should apply the change directly (manager), or
//  FALSE when the change was routed to management for approval.
static bool settingsRequestGate(HWND h, const std::wstring& title,
                                const std::wstring& detail,
                                const std::wstring& payload,
                                const std::wstring& preview){
    if(!s_st) return true;
    if(s_st->role>=1) return true;   // managers apply directly
    if(MessageBoxW(h,L"آیا از ذخیرهٔ این تنظیمات اطمینان دارید؟",
        L"تأیید ذخیره", MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2)!=IDYES)
        return false;                // user cancelled — do nothing
    pushSetReqEx(g_session.user.fullname.empty()?g_session.user.username
                                                :g_session.user.fullname,
                 systemSourceName(), title, detail, payload, preview);
    MessageBoxW(h,L"این تنظیمات برای مدیریت ارسال شد. پس از تأیید اعمال خواهد شد.",
        L"ارسال برای تأیید", MB_OK|MB_ICONINFORMATION);
    return false;
}

static void doThemeToggle(HWND h){
    // v1.9.2: the theme is a LOCAL, preference-only setting. It is never routed
    // to management for approval and never travels to the server — every
    // workstation keeps its own appearance. Applied instantly.
    bool wantDark=!g_dark;
    applyTheme(wantDark);
    setSetting(L"theme", wantDark?L"dark":L"light");
    if(s_st) s_st->dark=g_dark;
    broadcastThemeChange();
    freeBgCache();                 // theme colours changed → rebuild cached layers
    InvalidateRect(h,NULL,FALSE);
}
static void doDensityToggle(HWND h){
    if(!s_st) return;
    s_st->compact=!s_st->compact;
    setSetting(L"density", s_st->compact?L"compact":L"normal");
    MessageBoxW(h,L"تغییر چگالی رابط در اجرای بعدی برنامه اعمال می‌شود.",
        L"چگالی رابط", MB_OK|MB_ICONINFORMATION);
    InvalidateRect(h,NULL,FALSE);
}
static void doAutoPrintToggle(HWND h){
    if(!s_st) return;
    bool want=!s_st->autoPrint;
    if(!settingsRequestGate(h,L"چاپ خودکار قبض",
            std::wstring(L"چاپ خودکار قبض: ")+(want?L"روشن":L"خاموش"),
            std::wstring(L"auto_print=")+(want?L"1":L"0"),L""))
        return;
    s_st->autoPrint=want;
    setSetting(L"auto_print", s_st->autoPrint?L"1":L"0");
    InvalidateRect(h,NULL,FALSE);
}
static void doNotifyToggle(HWND h){
    if(!s_st) return;
    bool want=!s_st->notify;
    if(!settingsRequestGate(h,L"تنظیمات اعلان",
            std::wstring(L"اعلان پیام جدید: ")+(want?L"روشن":L"خاموش"),
            std::wstring(L"notify=")+(want?L"1":L"0"),L""))
        return;
    s_st->notify=want;
    setSetting(L"notify", s_st->notify?L"1":L"0");
    InvalidateRect(h,NULL,FALSE);
}
// v1.8.0: enable/disable the locally-stored «پیام‌های ذخیره‌شده» archive.
static void doSavedMsgsToggle(HWND h){
    if(!s_st) return;
    bool want=!s_st->savedMsgs;
    if(!settingsRequestGate(h,L"پیام‌های ذخیره‌شده",
            std::wstring(L"بایگانی محلی پیام‌ها: ")+(want?L"فعال":L"غیرفعال"),
            std::wstring(L"saved_msgs_enabled=")+(want?L"1":L"0"),L""))
        return;
    s_st->savedMsgs=want;
    setSetting(L"saved_msgs_enabled", s_st->savedMsgs?L"1":L"0");
    InvalidateRect(h,NULL,FALSE);
    // repaint any open reception so the archive icon appears/disappears live
    if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE);
}
static void saveServerUrl(){
    if(!s_st || !s_st->eServer || serverRowIndex()<0) return;
    wchar_t buf[512]={0}; GetWindowTextW(s_st->eServer,buf,512);
    std::wstring v=trim(buf);
    if(!v.empty()) setSetting(L"server_url",v);
}
static void doAbout(HWND h){
    std::wstring msg=std::wstring(APP_NAME_W)+L"\n"
        L"سامانه پذیرش و مدیریت درمانگاه\n\n"
        L"نسخه: "+toFaDigits(APP_VERSION_W)+L"\n"
        L"اجرای تک‌فایل، سازگار با ویندوز ۷ تا ۱۱\n\n© درمان پلاس";
    MessageBoxW(h,msg.c_str(),L"درباره برنامه",MB_OK|MB_ICONINFORMATION);
}
// reception profile editing (name+photo) — pending management approval -------
static void doProfile(HWND h){
    // Open the full profile-edit modal (current name read-only, new name edit,
    // photo picker, تأیید/انصراف). It queues a ProfReq for management approval.
    HWND root=GetAncestor(h,GA_ROOT); if(!root) root=h;
    showProfileDialog(root);
}

// --------------------------------------------------------------- painting --
//  v1.63.0: a modern switch — the ON track is a gradient that GLOWS in the
//  accent colour, the OFF track is a recessed grey well, and the knob carries
//  its own small shadow so it reads as a physical thumb sitting in the track.
static void drawToggle(HDC dc, int cx, int cy, bool on){
    int w=S(46), hh=S(24);
    RECT tr={cx-w/2,cy-hh/2,cx+w/2,cy+hh/2};
    if(on){
        gpShadowColor(dc,tr,hh/2,S(5),90,g_theme.accent);
        gpGradRoundRect(dc,tr,hh/2,g_theme.accentHover,g_theme.accent,CLR_INVALID);
    } else {
        gpGradRoundRect(dc,tr,hh/2,
            blendColor(g_theme.border,g_theme.bg,30),
            blendColor(g_theme.border,g_theme.surface,15),
            blendColor(g_theme.border,g_theme.textDim,20));
    }
    int kn=hh-S(6);
    int kx=on?(tr.right-S(3)-kn):(tr.left+S(3));
    RECT kr={kx,tr.top+S(3),kx+kn,tr.bottom-S(3)};
    gpShadow(dc,kr,kn/2,S(4),90);
    gpGradRoundRect(dc,kr,kn/2,RGB(255,255,255),RGB(238,241,246),CLR_INVALID);
}
//  v1.63.0: the value chip is a soft gradient pill with a slightly stronger
//  hairline, so a row's current value reads as a badge rather than flat text.
static void drawValueChip(HDC dc, RECT row, const wchar_t* val){
    SIZE sz; HGDIOBJ of=SelectObject(dc,g_fSmall);
    GetTextExtentPoint32W(dc,val,(int)wcslen(val),&sz);
    int pad=S(12);
    RECT chip={ row.left+S(8),(row.top+row.bottom)/2-S(13),
                row.left+S(8)+sz.cx+pad*2,(row.top+row.bottom)/2+S(13) };
    gpGradRoundRect(dc,chip,S(13),
        blendColor(g_theme.surfaceTop,g_theme.surface2,40),
        g_theme.surface2, blendColor(g_theme.border,g_theme.accent,18));
    SetTextColor(dc,g_theme.textDim);
    DrawTextW(dc,val,-1,&chip,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
    SelectObject(dc,of);
}

// Build the static (expensive) background into the cache DC. Called only on
// open / size / theme change — never on hover.
static void buildBgCache(HWND h, HDC ref){
    RECT rc; GetClientRect(h,&rc);
    if(rc.right<=0||rc.bottom<=0) return;
    freeBgCache();
    s_bgDC=CreateCompatibleDC(ref);
    s_bgBmp=CreateCompatibleBitmap(ref,rc.right,rc.bottom);
    s_bgOld=SelectObject(s_bgDC,s_bgBmp);
    s_bgW=rc.right; s_bgH=rc.bottom;
    HDC dc=s_bgDC;

    // dim scrim
    { HBRUSH sb=CreateSolidBrush(g_dark?RGB(6,9,14):RGB(28,36,48));
      FillRect(dc,&rc,sb); DeleteObject(sb); }
    // a soft translucent dim using GDI+ for a glassy look
    gpFillAlpha(dc,rc,0,g_dark?RGB(0,0,0):RGB(20,28,40),120);

    RECT card=cardRect(h);
    // shadow + opaque card
    gpShadow(dc,card,S(20),S(22),80);
    gpGradRoundRectBg(dc,card,S(20),g_theme.surfaceTop,g_theme.surface,
                      g_theme.border,g_dark?RGB(6,9,14):RGB(28,36,48));

    SetBkMode(dc,TRANSPARENT);

    // cover gradient — v1.9.2: clip ALL header drawing to the card's rounded
    // path so the blue band can never bleed a square past the rounded top
    // corners. The clip region is the rounded card itself; everything painted
    // while it is active is guaranteed to stay inside the radius.
    {
        int rad=S(20);
        HRGN cardRgn=CreateRoundRectRgn(card.left,card.top,
                                        card.right+1,card.bottom+1,rad*2,rad*2);
        SelectClipRgn(dc,cardRgn);
        // top blue band: square is fine now because the clip rounds it for us
        RECT cover={card.left,card.top,card.right,card.top+S(96)};
        gpGradRoundRect(dc,cover,0,g_theme.accent2,g_theme.accent,CLR_INVALID);
        SelectClipRgn(dc,NULL);
        DeleteObject(cardRgn);
    }

    // close (×) icon top-RIGHT (§5) — the hover highlight is drawn live on top
    { RECT cb=closeBtnRect(card);
      RECT ci={cb.left+S(5),cb.top+S(5),cb.right-S(5),cb.bottom-S(5)};
      drawIcon(dc,ICO_X,ci,RGB(255,255,255),S(2)); }

    // avatar (§5: slightly smaller and slightly lower so it sits clear of the
    // cover band's top edge and is visually balanced under the close button)
    int avR=S(37), avCx=(card.left+card.right)/2, avCy=card.top+S(104);
    RECT avo={avCx-avR-S(4),avCy-avR-S(4),avCx+avR+S(4),avCy+avR+S(4)};
    gpRoundRect(dc,avo,avR+S(4),g_theme.surfaceTop,CLR_INVALID,255);
    RECT av={avCx-avR,avCy-avR,avCx+avR,avCy+avR};
    bool guest = (s_st && s_st->role<0);
    gpGradRoundRect(dc,av,avR,
        guest?g_theme.textDim:g_theme.accent2,
        guest?g_theme.border:g_theme.accent,CLR_INVALID);
    std::wstring fn = guest ? L"" : g_session.user.fullname;
    std::wstring photo = guest ? L"" : getSetting(L"photo_"+g_session.user.username,L"");
    if(!photo.empty() && gpDrawImageFileCircle(dc,photo,av)){
        // photo drawn into the circle — nothing else to do
    } else if(!fn.empty()){
        std::wstring ini=fn.substr(0,1);
        SelectObject(dc,g_fHuge); SetTextColor(dc,RGB(255,255,255));
        DrawTextW(dc,ini.c_str(),-1,&av,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
    } else {
        RECT ui={av.left+S(20),av.top+S(20),av.right-S(20),av.bottom-S(20)};
        drawIcon(dc,ICO_USER,ui,RGB(255,255,255),S(3));
    }

    // identity — name and role each get their OWN vertical line, both fully
    // inside the (now taller) header so neither is clipped by the first row.
    SelectObject(dc,g_fTitle); SetTextColor(dc,g_theme.text);
    int nameTop = avCy+avR+S(8);
    RECT nr={card.left+S(16),nameTop,card.right-S(16),nameTop+S(30)};
    DrawTextW(dc, guest?L"کاربر مهمان":(fn.empty()?L"کاربر":fn.c_str()),-1,&nr,
        DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
    const wchar_t* role = guest ? L"وارد نشده" :
        s_st->role==2 ? L"مدیر سامانه" :
        s_st->role==1 ? L"مدیریت درمانگاه" : L"پذیرش درمانگاه";
    std::wstring sub=std::wstring(role)+
        (guest||g_session.user.dept.empty()?L"":(L"  •  "+g_session.user.dept));
    RECT srr={card.left+S(16),nr.bottom+S(2),card.right-S(16),nr.bottom+S(22)};
    DrawTextW(dc,sub.c_str(),-1,&srr,
        DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
}

// Draw the interactive rows (and live close-button hover) onto dc, clipped by
// the caller. Cheap enough to run on every hover change.
static void paintRows(HWND h, HDC dc){
    RECT card=cardRect(h);
    SetBkMode(dc,TRANSPARENT);
    // live close (×) hover highlight (icon itself lives in the cached bg)
    if(s_st && s_st->hot==-2){
        RECT cb=closeBtnRect(card);
        // v1.63.0: the close target now lights up as a soft white pill with a
        // faint ring, so it reads as a real button on the coloured cover band.
        gpFillAlpha(dc,cb,S(9),RGB(255,255,255),86);
        gpRoundRect(dc,cb,S(9),CLR_INVALID,RGB(255,255,255),150);
        RECT ci={cb.left+S(5),cb.top+S(5),cb.right-S(5),cb.bottom-S(5)};
        drawIcon(dc,ICO_X,ci,RGB(255,255,255),S(2));
    }
    if(s_st){
        for(size_t i=0;i<s_st->rows.size();i++){
            RowDef& rd=s_st->rows[i];
            RECT r=rowRect(card,(int)i);
            bool dis=rd.disabled;
            bool hov=(!dis && s_st->hot==rd.id);
            bool danger=(rd.id==ROW_LOGOUT);
            // --------------------------------------------------------------
            //  v1.63.0 SETTINGS ROW REDESIGN (the "settings buttons" fix).
            //  A row was a flat rect whose only hover feedback was swapping two
            //  colours — it neither looked nor felt like a button. Now each row
            //  is a real pressable tile:
            //    * at rest  — soft surface gradient + hairline border,
            //    * on hover — elevation shadow tinted with the row's OWN accent
            //                 (danger red for خروج), a warmed border, and a
            //                 bright 3 px leading indicator on the RTL edge,
            //    * disabled — a flat, de-saturated plate with no indicator, so
            //                 it visibly cannot be pressed.
            //  The icon always sits in a tinted medallion, which is what makes
            //  the list scan as a menu instead of a wall of text.
            // --------------------------------------------------------------
            COLORREF rowAcc = danger ? g_theme.danger : g_theme.accent;
            if(dis){
                gpRoundRect(dc,r,S(12),
                    blendColor(g_theme.surface,g_theme.bg,35),
                    blendColor(g_theme.border,g_theme.bg,40),255);
            } else if(hov){
                gpShadowColor(dc,r,S(12),S(7),86,rowAcc);
                gpGradRoundRectBg(dc,r,S(12),
                    blendColor(g_theme.surfaceTop,g_theme.hover,45),
                    g_theme.hover, blendColor(g_theme.border,rowAcc,60),g_theme.surface);
                RECT li={r.right-S(5),r.top+S(8),r.right-S(2),r.bottom-S(8)};
                if(li.bottom>li.top) gpRoundRect(dc,li,S(2),rowAcc,CLR_INVALID);
            } else {
                gpGradRoundRectBg(dc,r,S(12),
                    blendColor(g_theme.surfaceTop,g_theme.surface,55),
                    g_theme.surface, g_theme.border,g_theme.surface);
            }
            COLORREF ic= dis ? g_theme.textDim : (danger?g_theme.danger:g_theme.accent);
            RECT ir={r.right-S(40),r.top+S(10),r.right-S(16),r.top+S(34)};
            { RECT md=ir; InflateRect(&md,S(6),S(6));
              int mr2=(md.bottom-md.top)/2;
              // Opaque pre-blended fill: alpha medallions could composite as
              // white on older GDI+, especially over dark cached surfaces.
              int tint=dis?12:(hov?28:20);
              COLORREF medFill=blendColor(g_theme.surface,ic,tint);
              gpRoundRectBg(dc,md,mr2,medFill,
                  blendColor(g_theme.border,ic,dis?18:(hov?58:38)),g_theme.surface); }
            drawIcon(dc,rd.icon,ir,ic,S(2));
            SelectObject(dc,g_fUIB);
            SetTextColor(dc, dis ? g_theme.textDim : (danger?g_theme.danger:g_theme.text));
            // v1.9.0: the server-address row carries an inline edit box, so its
            // label sits at the TOP of the row (never vertically centred under
            // the box) to avoid the old overlap where the box covered the label.
            bool hasHint=rd.hint!=NULL;
            bool serverRow=(rd.id==ROW_SERVER);
            RECT lr={r.left+S(14),r.top+((hasHint||serverRow)?S(6):S(0)),r.right-S(46),
                     r.top+((hasHint||serverRow)?S(26):rowH()-S(8))};
            DrawTextW(dc,rd.label,-1,&lr,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            if(hasHint){
                SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
                RECT hr={r.left+S(14),r.top+S(30),r.right-S(46),r.bottom-S(4)};
                DrawTextW(dc,rd.hint,-1,&hr,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            }
            int lcx=r.left+S(34), lcy=(r.top+r.bottom)/2;
            if(dis){
                // disabled control: show a small «غیرفعال» chip instead of a
                // chevron so it never looks clickable.
                drawValueChip(dc,r, L"غیرفعال");
            }
            else if(rd.id==ROW_THEME)        drawValueChip(dc,r, s_st->dark?L"تیره":L"روشن");
            else if(rd.id==ROW_DENSITY) drawValueChip(dc,r, s_st->compact?L"فشرده":L"متعارف");
            else if(rd.id==ROW_AUTOPRINT) drawToggle(dc,lcx,lcy,s_st->autoPrint);
            else if(rd.id==ROW_NOTIFY)  drawToggle(dc,lcx,lcy,s_st->notify);
            else if(rd.id==ROW_SAVEDMSGS) drawToggle(dc,lcx,lcy,s_st->savedMsgs);
            else if(rd.id==ROW_SERVER){ /* edit box overlays */ }
            else {
                RECT cv={r.left+S(14),lcy-S(8),r.left+S(28),lcy+S(8)};
                drawIcon(dc,ICO_CHEVRON,cv,g_theme.textDim,S(2));
            }
        }
    }
}

// Composite: blit the cached static background for the dirty strip, draw the
// (cheap) rows on top, then copy out. On open / size / theme change the cache
// is (re)built first; on hover it is reused, so the heavy GDI+ scrim/shadow/
// gradient work runs ONCE, not on every mouse move.
static void paintPanel(HWND h, HDC dc0, const RECT* dirty){
    RECT rc; GetClientRect(h,&rc);
    if(rc.right<=0||rc.bottom<=0) return;
    if(!s_bgDC || s_bgW!=rc.right || s_bgH!=rc.bottom)
        buildBgCache(h,dc0);
    if(!s_bgDC) return;

    RECT d = (dirty && dirty->right>dirty->left && dirty->bottom>dirty->top)
             ? *dirty : rc;
    // clamp to the client area (invalidation rects can overhang by 1px)
    if(d.left<0) d.left=0; if(d.top<0) d.top=0;
    if(d.right>rc.right) d.right=rc.right;
    if(d.bottom>rc.bottom) d.bottom=rc.bottom;
    int dw=d.right-d.left, dh=d.bottom-d.top;
    if(dw<=0||dh<=0) return;

    // v1.60.0 FPS FIX: the double buffer is now sized to the DIRTY STRIP only
    // (one row ≈ 460×54px) instead of the whole 1920×1080 screen. The old code
    // allocated + destroyed a full-screen 8MB bitmap on EVERY mouse move — that
    // allocation churn was the cause of the visible FPS drop while the panel
    // was open. The strip DC uses SetViewportOrgEx so paintRows can keep its
    // absolute coordinates unchanged.
    HDC dc=CreateCompatibleDC(dc0);
    HBITMAP bmp=CreateCompatibleBitmap(dc0,dw,dh);
    HGDIOBJ obm=SelectObject(dc,bmp);
    SetViewportOrgEx(dc,-d.left,-d.top,NULL);

    // 1) bring back the cached static layers for the dirty region only
    BitBlt(dc,d.left,d.top,dw,dh,s_bgDC,d.left,d.top,SRCCOPY);
    // 2) draw the interactive rows on top (GDI clip keeps this to the strip)
    HRGN clip=CreateRectRgn(d.left,d.top,d.right,d.bottom);
    SelectClipRgn(dc,clip);
    paintRows(h,dc);
    SelectClipRgn(dc,NULL); DeleteObject(clip);
    SetViewportOrgEx(dc,0,0,NULL);
    // 3) copy the composited strip to the screen
    BitBlt(dc0,d.left,d.top,dw,dh,dc,0,0,SRCCOPY);

    SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
}

// ----------------------------------------------------------------- wndproc -
static LRESULT CALLBACK setProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps);
        // pass the invalidated rect so hover repaints copy only that strip
        RECT dirty=ps.rcPaint;
        paintPanel(h,dc, &dirty); EndPaint(h,&ps); return 0; }
    case WM_APP_THEME: freeBgCache(); InvalidateRect(h,NULL,FALSE); return 0;
    case WM_MOUSEMOVE: {
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        RECT card=cardRect(h);
        int hr;
        RECT cb=closeBtnRect(card);
        if(PtInRect(&cb,pt)) hr=-2; else hr=hitRow(h,pt);
        if(s_st && hr!=s_st->hot){
            // v1.7.0 perf: repaint ONLY the two affected rectangles (the row we
            // left + the row we entered) instead of the whole scrim. This stops
            // the heavy full-screen redraw (shadow+gradient+alpha) on every
            // mouse move that made the panel stutter.
            RECT oldR, newR;
            bool haveOld=hotRectFor(h,s_st->hot,oldR);
            int old=s_st->hot; s_st->hot=hr;
            bool haveNew=hotRectFor(h,hr,newR);
            (void)old;
            if(haveOld) InvalidateRect(h,&oldR,FALSE);
            if(haveNew) InvalidateRect(h,&newR,FALSE);
        }
        TRACKMOUSEEVENT te={sizeof(te),TME_LEAVE,h,0}; TrackMouseEvent(&te);
        return 0; }
    case WM_MOUSELEAVE:
        if(s_st && s_st->hot!=0){
            RECT oldR; bool haveOld=hotRectFor(h,s_st->hot,oldR);
            s_st->hot=0;
            if(haveOld) InvalidateRect(h,&oldR,FALSE);
        }
        return 0;
    case WM_LBUTTONDOWN: {
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        RECT card=cardRect(h);
        RECT cb=closeBtnRect(card);
        if(PtInRect(&cb,pt)){ closeSettingsPanel(); return 0; }
        int id=hitRow(h,pt);
        if(id==-1){ closeSettingsPanel(); return 0; }   // scrim
        // ignore clicks on any disabled row (e.g. the update-check, off until a
        // server exists) so the dialog never fakes an action.
        if(s_st){
            for(auto& rd:s_st->rows)
                if(rd.id==id && rd.disabled) return 0;
        }
        switch(id){
            case ROW_THEME:       doThemeToggle(h); break;
            case ROW_UPDATE:      /* disabled — no server yet */ break;
            case ROW_NOTIFY:      doNotifyToggle(h); break;
            case ROW_SAVEDMSGS:   doSavedMsgsToggle(h); break;
            case ROW_PRINTER:     closeSettingsPanel(); openPrinterSettings(g_hFrame); break;
            // §A CRITICAL: «دیزاین چاپ» must open the print DESIGNER (was wrongly
            // routed to the printer-settings dialog).
            case ROW_PRINTDESIGN: closeSettingsPanel(); PrintDesigner_Open(g_hFrame); break;
            case ROW_PROFILE:     doProfile(h); break;
            case ROW_ZOOMRESET:   Reception_ResetZoom(); closeSettingsPanel(); break;
            case ROW_DENSITY:     doDensityToggle(h); break;
            case ROW_AUTOPRINT:   doAutoPrintToggle(h); break;
            case ROW_SERVER:      if(s_st&&s_st->eServer) SetFocus(s_st->eServer); break;
            case ROW_ABOUT:       doAbout(h); break;
            case ROW_LOGOUT:      saveServerUrl(); closeSettingsPanel();
                                  PostMessageW(g_hFrame,WM_COMMAND,101,0); break;
        }
        return 0; }
    case WM_KEYDOWN:
        if(w==VK_ESCAPE){ closeSettingsPanel(); return 0; }
        break;
    case WM_COMMAND:
        if(LOWORD(w)==IDC_SRV && HIWORD(w)==EN_KILLFOCUS) saveServerUrl();
        return 0;
    case WM_CTLCOLOREDIT: { HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_SIZE: freeBgCache(); layoutServerEdit(h); InvalidateRect(h,NULL,FALSE); return 0;
    case WM_DESTROY:
        freeBgCache();
        if(s_st){ delete s_st; s_st=NULL; }
        s_set=NULL; return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

// ----- build the row list for the current role -----------------------------
//  v1.9.2 — Honest, minimal settings:
//    • The ONLY user-changeable preference is the LOCAL theme (light/dark).
//      It is never sent to the server / management.
//    • «بررسی به‌روزرسانی» is shown but DISABLED (greyed out) because there is
//      no update server yet — it must not look clickable or fake success.
//    • «درباره برنامه» (read-only info) and «خروج از حساب» (the only way to
//      sign out) remain for every signed-in role.
//  Notification / printer / profile / saved-msgs / density / auto-print /
//  server-address rows were removed from this dialog so nothing here pretends
//  to be a synced/management-side control.
static void buildRows(SetState* st){
    st->rows.clear();
    bool guest = st->role<0;
    // theme — the single live, local preference
    st->rows.push_back({ROW_THEME,  L"تغییر پوستهٔ نمایش",  ICO_MOON,
                        L"ظاهر این رایانه — محلی و بدون نیاز به تأیید",false,false});
    // update — visible but disabled (no server yet)
    st->rows.push_back({ROW_UPDATE, L"بررسی به‌روزرسانی", ICO_UPDATE,
                        L"در حال حاضر غیرفعال (سرور به‌روزرسانی موجود نیست)",false,true});
    if(guest) return;
    // v1.3.0: surface the (already-implemented) profile change request flow —
    // edit display name + avatar, queued for management approval. This is the
    // reception-user "settings/profile" entry point requested for this release.
    st->rows.push_back({ROW_PROFILE, L"پروفایل من",        ICO_USER,
                        L"تغییر نام و عکس — ارسال درخواست برای تأیید مدیریت",false,false});
    st->rows.push_back({ROW_ZOOMRESET, L"ریست کردن بزرگنمایی", ICO_REFRESH,
                        L"بازگشت بزرگنمایی پذیرش به اندازهٔ پیش‌فرض",false,false});
    // v1.60.0: professional icon — the about row used ICO_BELL (notification
    // bell) which made no semantic sense; ICO_INFO (circle + i) is correct.
    st->rows.push_back({ROW_ABOUT,  L"درباره برنامه",     ICO_INFO,
                        L"نسخه و اطلاعات",false,false});
    st->rows.push_back({ROW_LOGOUT, L"خروج از حساب",      ICO_LOGOUT,NULL,false,false});
}

// ------------------------------------------------------------------ public -
void openSettingsPanel(HWND frameOwner){
    if(s_set && IsWindow(s_set)){ closeSettingsPanel(); return; }
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc={0};
        wc.lpfnWndProc=setProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.lpszClassName=SET_CLASS;
        RegisterClassW(&wc); reg=true;
    }
    RECT rc; GetClientRect(frameOwner,&rc);
    POINT org={0,0}; ClientToScreen(frameOwner,&org);
    s_st=new SetState();
    s_st->owner=frameOwner; s_st->hot=0;
    s_st->dark=g_dark;
    s_st->compact=(getSetting(L"density",L"normal")==L"compact");
    s_st->autoPrint=(getSetting(L"auto_print",L"0")==L"1");
    s_st->notify=(getSetting(L"notify",L"1")==L"1");
    s_st->savedMsgs=(getSetting(L"saved_msgs_enabled",L"0")==L"1");
    s_st->role = g_session.user.username.empty() ? -1 : g_session.user.role;
    buildRows(s_st);

    s_set=CreateWindowExW(WS_EX_TOPMOST,SET_CLASS,L"",
        WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN,
        org.x,org.y,rc.right,rc.bottom,frameOwner,NULL,g_hInst,NULL);

    s_st->eServer=CreateWindowExW(0,L"EDIT",
        getSetting(L"server_url",L"").c_str(),
        WS_CHILD|ES_AUTOHSCROLL,0,0,10,10,s_set,(HMENU)IDC_SRV,g_hInst,NULL);
    SendMessageW(s_st->eServer,WM_SETFONT,(WPARAM)g_fSmall,TRUE);
    layoutServerEdit(s_set);

    BringWindowToTop(s_set);
    SetFocus(s_set);
    InvalidateRect(s_set,NULL,FALSE);
}
bool settingsPanelVisible(){ return s_set && IsWindow(s_set); }
void closeSettingsPanel(){
    if(s_set && IsWindow(s_set)){
        HWND v=s_set; s_set=NULL;
        DestroyWindow(v);
        if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE);
    }
}
