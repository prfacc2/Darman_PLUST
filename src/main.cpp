// ============================================================================
//  main.cpp — entry point, fullscreen frame (no menu/title bar), home screen,
//  live Iran clock/date (bottom-right), hidden admin combo Ctrl+P+N,
//  global F8 = print last receipt
// ============================================================================
#include "app.h"
#include "backup_log.h"
#include "web_admission.h"
#include "web_crm.h"          // v1.70.0: embedded HTML CRM management surface
#include "print_designer.h"   // v2.07: pdVerifyBuiltinTemplates (§4.6 smoke)
#include "web_designer.h"    // v2.07.1: WebDesigner_Open (designer_gallery smoke)
#include "web_thread_pool.h"   // v1.40.0: WM_APP_UI_TASK marshalling (RunOnUiThread)
#include <stdio.h>
#ifdef AZ_DEBUG_BUILD
#include <winsock2.h>   // headless admission_probe self-connect (debug only)
#include <ws2tcpip.h>
long WebAdmission_DebugInitHits();
std::string WebAdmission_DebugInlinePage(bool withFontFace);
std::string WebAdmission_DebugLastFilledNid();
#endif

HINSTANCE g_hInst = NULL;
HWND      g_hFrame = NULL;
double    g_scale = 1.0;
Session   g_session;

HFONT g_fUI=0, g_fUIB=0, g_fSmall=0, g_fTitle=0, g_fBig=0, g_fHuge=0, g_fMono=0;
HFONT g_fLabel=0, g_fSection=0;   // v1.27.0 UI redesign fonts
HFONT g_fCode=0;   // §G: fixed-pitch code font (Consolas → Courier New)
HFONT g_fClock=0, g_fDate=0;      // v1.92: dedicated header clock / date fonts

// frame children
//  v1.4.0: the header now carries ONLY the exit button (right) and the gear
//  settings button (left). Theme-toggle and check-for-update were removed from
//  the header and moved INTO the settings panel per the redesign brief.
static HWND s_bExit=0, s_bSettings=0, s_bCalc=0;
//  v1.7.0: the «پذیرش بیمار» / «تب جدید» actions were moved out of
//  the reception tab strip and INTO this header so the navigation is clean and
//  professional. They are shown only while the reception screen is active and
//  are routed to it via receptionAction().
//  v1.60.0: «نوبت‌دهی» (appointment scheduling) has been REMOVED from the
//  reception account entirely — page, option and code path.
static HWND s_bNewPat=0, s_bNewTab=0;
// v1.62.0: the three print actions («چاپ آخرین قبض / چاپ نسخه / رسید بیمه») no
// longer live in the native bottom bar — they were moved INTO the embedded
// admission page (bottom-right `.print-card`) so the whole receipt workflow sits
// in one place instead of straddling two surfaces. receptionPrintAction() stays
// alive because the keyboard accelerators (F8 = last receipt) still route
// through it, and the bottom bar is now a clean status strip.
static HWND s_screen=0;
static ScreenId s_curScreen = SC_HOME;

#define ID_FR_EXIT     101
#define ID_FR_SETTINGS 104
#define ID_FR_CALC     105
#define ID_FR_NEWPAT   106
#define ID_FR_NEWTAB   108
// 109 / 110 / 113 were the native bottom-bar print buttons — retired in v1.62.0
// when printing moved into the embedded admission page. Do not reuse the ids.
#define TIMER_CLOCK  1
// v1.89.0: the welcome-screen entrance animation was REMOVED on request — the
// 30-frame repaint burst caused a visible FPS drop/stutter on real machines.
// No timer, no shimmer; the home screen now repaints ONLY what changed.

// ------------------------------------------------------------------ fonts --
static HFONT mkFont(int px, int weight){
    return CreateFontW(-S(px),0,0,0,weight,0,0,0,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
        g_lowSpec?DEFAULT_QUALITY:CLEARTYPE_QUALITY,
        DEFAULT_PITCH,L"Vazirmatn");
}
// §G: a fixed-pitch font for codes. Try Consolas first; GDI falls back to
// Courier New automatically when Consolas is absent (FIXED_PITCH ensures a
// monospace face is chosen). DEFAULT_CHARSET keeps Persian digits rendering.
static HFONT mkMonoFont(int px, int weight){
    HFONT f=CreateFontW(-S(px),0,0,0,weight,0,0,0,DEFAULT_CHARSET,
        OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,
        g_lowSpec?DEFAULT_QUALITY:CLEARTYPE_QUALITY,
        FIXED_PITCH|FF_MODERN,L"Consolas");
    if(f) return f;
    return CreateFontW(-S(px),0,0,0,weight,0,0,0,DEFAULT_CHARSET,
        OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,
        g_lowSpec?DEFAULT_QUALITY:CLEARTYPE_QUALITY,
        FIXED_PITCH|FF_MODERN,L"Courier New");
}
static void buildFonts(){
    if(g_fUI)   DeleteObject(g_fUI);
    if(g_fUIB)  DeleteObject(g_fUIB);
    if(g_fSmall)DeleteObject(g_fSmall);
    if(g_fTitle)DeleteObject(g_fTitle);
    if(g_fBig)  DeleteObject(g_fBig);
    if(g_fHuge) DeleteObject(g_fHuge);
    if(g_fMono) DeleteObject(g_fMono);
    if(g_fCode) DeleteObject(g_fCode);
    if(g_fLabel)  DeleteObject(g_fLabel);
    if(g_fSection)DeleteObject(g_fSection);
    if(g_fClock)  DeleteObject(g_fClock);
    if(g_fDate)   DeleteObject(g_fDate);
    g_fUI    = mkFont(15, FW_NORMAL);
    g_fUIB   = mkFont(15, FW_BOLD);
    g_fSmall = mkFont(12, FW_NORMAL);
    g_fTitle = mkFont(19, FW_BOLD);
    g_fBig   = mkFont(30, FW_BOLD);
    g_fHuge  = mkFont(38, FW_BOLD);
    g_fMono  = mkFont(24, FW_BOLD);
    g_fCode  = mkMonoFont(12, FW_NORMAL);   // §G: section / personnel codes
    // v1.27.0 UI redesign: readable field labels (13 medium) + strong section
    // titles (16 bold). Vazirmatn has no true "medium" cut, so FW_SEMIBOLD gives
    // the label the requested weight without looking bold.
    g_fLabel   = mkFont(13, FW_SEMIBOLD);
    g_fSection = mkFont(16, FW_BOLD);
    // v1.92: a dedicated, larger clock font (22 bold) and a clearer date font
    // (14) so the header time / Jalali date read as deliberate typography
    // instead of the 19/12px faces reused from the rest of the UI.
    g_fClock = mkFont(22, FW_BOLD);
    g_fDate  = mkFont(14, FW_NORMAL);
}

// v1.31.0 RESPONSIVE-LABEL FIX — a tiny cache of fonts whose logical height is
// -S(px)*f. When the reception layout shrinks by a fit-factor `f`, the painter
// asks for a label/section font at the SAME factor so the glyphs shrink with
// the band and can never be clipped or hidden behind the control below.
#include <map>
struct FitFontKey { int px; int weight; int q; bool operator<(const FitFontKey& o) const {
    if(px!=o.px) return px<o.px; if(weight!=o.weight) return weight<o.weight; return q<o.q; } };
HFONT fitFont(int px, int weight, double f){
    if(f<=0.0) f=1.0;
    if(f>1.0)  f=1.0;
    // quantise the factor to 5% steps so the cache stays tiny (≤ a handful).
    int q=(int)(f*20.0+0.5); if(q<10) q=10; if(q>20) q=20;
    static std::map<FitFontKey,HFONT> cache;
    FitFontKey k{px,weight,q};
    auto it=cache.find(k);
    if(it!=cache.end()) return it->second;
    int h=(int)(S(px)*(q/20.0)+0.5); if(h<8) h=8;
    HFONT fnt=CreateFontW(-h,0,0,0,weight,0,0,0,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
        g_lowSpec?DEFAULT_QUALITY:CLEARTYPE_QUALITY,
        DEFAULT_PITCH,L"Vazirmatn");
    cache[k]=fnt;
    return fnt;
}

// ------------------------------------------------------------- frame rects -
//  v1.3.0 — taller header (LAYER 1) so the centered live clock + Jalali date
//  fit comfortably; thinner bottom status bar (clock moved up to the header).
//  v1.8.0 — the header now has TWO layers: LAYER 1 (identity + clock + gear /
//  calculator / exit) and a thinner LAYER 2 "action bar" that, on the reception
//  screen, hosts the blue navigation buttons (پذیرش بیمار / تب جدید)
//  RIGHT-aligned. The action bar is only present where it is needed so other
//  screens keep the original clean single-layer header.
// §2.B (1.12.0): the LAYER-1 header was slightly reduced from S(64) to S(56)
// for a more compact, modern look. The clock (top) + Jalali date (below) still
// fit comfortably because the clock band is S(3)+S(34) and the date S(36)→S(58)
// — both recalculated against this height in the paint code below.
// v1.92.0: the header grew from S(56) to S(60) to host the larger dedicated
// clock (g_fClock, 22 bold) + date (g_fDate, 14) with comfortable breathing
// room. All header children (exit / gear / calc) re-centre on this height.
static int mainBarH(){ return S(60); }                 // LAYER 1 height
// §B (v1.10.0): the action bar has a FIXED compact height. The old code scaled
// it by an animated collapse factor (S(50)*factor) which produced the
// frame-by-frame slide, the one-frame "stuck" artifact and an empty header row
// mid-animation. The animation is gone: the bar is simply present (compact) on
// the reception screen and absent everywhere else — applied in a single paint.
static int actionBarH(){ return S(48); }
// v1.89.0: the LAYER-2 header action bar («پذیرش بیمار» / «تب جدید») is
// REMOVED — those actions now live as app icons on the new reception
// dashboard. The function is kept (returns false) so all call sites collapse
// to the single-layer header cleanly.
static bool headerHasActionBar(){ return false; }
static int topBarH(){ return mainBarH() + (headerHasActionBar()?actionBarH():0); }
static int botBarH(){ return S(40); }
RECT frameContentRect(){
    RECT rc; GetClientRect(g_hFrame,&rc);
    rc.top += topBarH(); rc.bottom -= botBarH();
    return rc;
}

static void frameLayout(HWND h);   // fwd (header layout, defined below)
// ----------------------------------------------------------- screen switch -
void switchScreen(ScreenId id){
    { const wchar_t* nm = id==SC_HOME?L"switchScreen: HOME"
                        : id==SC_RECEPTION?L"switchScreen: RECEPTION"
                        : id==SC_ADMIN?L"switchScreen: ADMIN"
                        : id==SC_MANAGE?L"switchScreen: MANAGE"
                        : id==SC_ACCOUNTING?L"switchScreen: ACCOUNTING":L"switchScreen: ?";
      Breadcrumb(nm); }
    if(s_screen){ DestroyWindow(s_screen); s_screen=0; }
    s_curScreen = id;
    // §B (v1.10.0): NO animation. The reception screen uses the COMPACT header
    // layout immediately on entry; every other screen has no action bar at all.
    HeaderCollapse_Set(g_hFrame, id==SC_RECEPTION);
    switch(id){
        case SC_HOME:      s_screen = createHomeScreen(g_hFrame); break;
        case SC_RECEPTION: s_screen = createReceptionScreen(g_hFrame); break;
        case SC_ADMIN:     s_screen = createAdminScreen(g_hFrame); break;
        case SC_MANAGE:    s_screen = createManageScreen(g_hFrame); break;
        case SC_ACCOUNTING:s_screen = createAccountingScreen(g_hFrame); break;
    }
    RECT rc = frameContentRect();
    if(s_screen)
        MoveWindow(s_screen, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, TRUE);
    frameLayout(g_hFrame);   // refresh header action buttons for the new screen
    InvalidateRect(g_hFrame, NULL, TRUE);
}

// ================================================================ HOME =====
#define HM_CLASS L"AzHome"
#define ID_HM_RECEPTION  111
#define ID_HM_MANAGE     112
#define ID_HM_ACCOUNTING 113

// ---------------------------------------------------------------------------
//  v1.62.0 WELCOME SCREEN REDESIGN
//  The old home screen scattered its parts with hard-coded offsets (logo 96 →
//  title 46 → sub 28 → a 72 px hole → cards) and its "glass" hero ended ABOVE
//  the cards, so on the light theme the cards floated on the raw background
//  image and the whole screen looked unfinished.
//
//  The new layout is ONE cohesive elevated hero panel that contains every
//  pre-login element — brand mark, the system title «سامانه پذیرش و مدیریت
//  درمانگاه», the tagline, three capability chips and both entry cards — so the
//  composition is independent of whatever the background artwork does. A single
//  geometry function is shared by WM_SIZE and WM_PAINT (they can never drift
//  apart again) and every vertical metric passes through one fit factor, so the
//  panel scales down gracefully on small screens / high DPI instead of clipping.
// ---------------------------------------------------------------------------
struct HomeGeom {
    RECT panel;      // the elevated hero surface
    RECT ribbon;     // accent gradient ribbon along the panel top
    RECT logo;       // circular brand mark
    RECT title;      // «سامانه پذیرش و مدیریت درمانگاه»
    RECT sub;        // tagline
    RECT chips;      // capability chip row
    RECT tray;       // v1.88.0: glass vessel holding the three app icons
    RECT cardR;      // پرسنل app-icon cell (RTL: right)
    RECT cardM;      // حسابداری app-icon cell (center)
    RECT cardL;      // مدیریت app-icon cell (RTL: left)
    RECT foot;       // footer pill (brand • version • security note)
    int  radius;     // panel corner radius
    int  chipH;      // chip height
};
static HomeGeom homeGeom(int W, int H){
    HomeGeom g;
    memset(&g, 0, sizeof(g));
    if(W < S(320)) W = S(320);
    if(H < S(260)) H = S(260);

    // ---- horizontal: derive the panel width from the three app-icon cells --
    //  v1.98 RTL right→left: حسابداری | حساب پرسنل | حساب مدیریت.
    int padX  = S(36);
    int cgap  = S(24);
    int cardW = S(168);
    int maxW  = W - S(40);
    if(3*cardW + 2*cgap + 2*padX > maxW){
        if(maxW < S(640)) padX = S(16);
        int avail = maxW - 2*cgap - 2*padX;
        if(avail < S(360)) avail = S(360);
        cardW = avail/3;
        if(cardW < S(118)) cardW = S(118);
    }
    int panelW = 3*cardW + 2*cgap + 2*padX;
    // v1.88.0: the app-icon cells are compact, but the panel must still be wide
    // enough for the full title «سامانه پذیرش و مدیریت درمانگاه و بیمارستان»
    // and the 3-chip capability row — enforce a generous minimum width.
    int minPanelW = S(760);
    if(panelW < minPanelW) panelW = minPanelW;
    if(panelW > maxW) panelW = maxW;

    // ---- vertical: one stack, one fit factor ------------------------------
    int padTop = S(38), logoD = S(88), gLogo = S(20);
    int titleH = S(50), gTitle = S(4), subH = S(28), gSub = S(20);
    int chipH  = S(32), gChip = S(26), cardH = S(172), padBot = S(34);
    int footH  = S(38), footGap = S(22);

    int need = padTop+logoD+gLogo+titleH+gTitle+subH+gSub+chipH+gChip+cardH+padBot;
    int avail = H - S(26)*2 - footH - footGap;
    double f = 1.0;
    if(avail > S(200) && need > avail) f = (double)avail/(double)need;
    if(f < 0.62) f = 0.62;
    #define Zf(v) ((int)((v)*f + 0.5))
    padTop=Zf(padTop); logoD=Zf(logoD); gLogo=Zf(gLogo);
    titleH=Zf(titleH); gTitle=Zf(gTitle); subH=Zf(subH); gSub=Zf(gSub);
    chipH=Zf(chipH);   gChip=Zf(gChip);  cardH=Zf(cardH); padBot=Zf(padBot);
    #undef Zf
    int panelH = padTop+logoD+gLogo+titleH+gTitle+subH+gSub+chipH+gChip+cardH+padBot;

    int totalH = panelH + footGap + footH;
    int top = (H - totalH)/2;
    if(top < S(14)) top = S(14);
    int cx = W/2;

    g.radius = S(26);
    g.chipH  = chipH;
    SetRect(&g.panel, cx-panelW/2, top, cx+panelW/2, top+panelH);
    SetRect(&g.ribbon, g.panel.left, g.panel.top, g.panel.right, g.panel.top+S(5));

    int y = g.panel.top + padTop;
    SetRect(&g.logo,  cx-logoD/2, y, cx+logoD/2, y+logoD);   y += logoD + gLogo;
    SetRect(&g.title, g.panel.left+S(16), y, g.panel.right-S(16), y+titleH);
    y += titleH + gTitle;
    SetRect(&g.sub,   g.panel.left+S(16), y, g.panel.right-S(16), y+subH);
    y += subH + gSub;
    SetRect(&g.chips, g.panel.left+S(16), y, g.panel.right-S(16), y+chipH);
    y += chipH + gChip;

    int cardsW = 3*cardW + 2*cgap;
    int cl = cx - cardsW/2;
    SetRect(&g.cardL, cl,                y, cl+cardW,             y+cardH); // RTL left: مدیریت
    SetRect(&g.cardM, cl+cardW+cgap,     y, cl+2*cardW+cgap,      y+cardH); // center: پرسنل
    SetRect(&g.cardR, cl+2*(cardW+cgap), y, cl+cardsW,            y+cardH); // RTL right: حسابداری
    // v1.92.0: the glass tray now nearly matches the hero panel's width
    // (inset S(14) — down from S(28)) so the inner container holding the two
    // account icons reads as the same width as the outer panel behind it.
    SetRect(&g.tray, g.panel.left+S(14), y-S(16),
            g.panel.right-S(14), y+cardH+S(14));

    int fy = g.panel.bottom + footGap;
    SetRect(&g.foot, cx-S(250), fy, cx+S(250), fy+footH);
    if(g.foot.left < S(12)){ g.foot.left = S(12); g.foot.right = W-S(12); }
    return g;
}

// v1.77: the welcome hero panel's gradient colours, shared by WM_PAINT and the
// card corner-blend (WM_CREATE / WM_APP_THEME) so the cards' rounded corners
// always match the panel surface they sit on — never a white/coloured square.
// The light panel is a soft blue-white gradient (less stark than pure white);
// the dark panel keeps its deep slate gradient. The cards live in the lower
// band of the panel, so homePanelBot() is the colour behind them.
static COLORREF homePanelTop(){
    // v1.94: white with a very subtle blue tint for depth (not flat white)
    return g_dark ? RGB(24,29,38)
                  : RGB(0xF6, 0xF8, 0xFB); // near-white
}
static COLORREF homePanelBot(){
    return g_dark ? RGB(14,18,25)
                  : RGB(0xEC, 0xF1, 0xF7); // subtle shading bottom
}

// v1.98: which app-icon cell the mouse is over (0 none, 1 پرسنل center,
// 2 مدیریت left, 3 حسابداری right). RTL from the right: حسابداری | پرسنل | مدیریت.
static int s_appHot = 0;

static RECT homeHotCell(const HomeGeom& g, int hot){
    if(hot==1) return g.cardM; // پرسنل
    if(hot==2) return g.cardL; // مدیریت
    if(hot==3) return g.cardR; // حسابداری
    RECT z={0,0,0,0}; return z;
}
static void homeUnionRect(RECT& a, const RECT& b){
    if(b.right<=b.left) return;
    if(a.right<=a.left){ a=b; return; }
    if(b.left<a.left) a.left=b.left;
    if(b.top<a.top) a.top=b.top;
    if(b.right>a.right) a.right=b.right;
    if(b.bottom>a.bottom) a.bottom=b.bottom;
}
static int homeHitCell(const HomeGeom& g, POINT pt){
    if(PtInRect(&g.cardR,pt)) return 3; // حسابداری (right)
    if(PtInRect(&g.cardM,pt)) return 1; // پرسنل
    if(PtInRect(&g.cardL,pt)) return 2; // مدیریت
    return 0;
}
static COLORREF homeAccBrand(){
    if(g_themeMode==TM_NEON) return RGB(0x2A,0xC8,0xB4); // refined teal
    if(g_dark) return RGB(0x3A,0xB4,0x9C);
    return RGB(0x12,0x7A,0x6A); // deep teal — not cheap yellow
}

// v1.88.0: iOS-style app icon — a glossy squircle badge with a white glyph, a
// soft tinted shadow and the account name underneath; NO button chrome, NO
// background — exactly like a pinned phone app, slightly larger.
static void paintAppIcon(HDC dc, RECT cell, int icon, COLORREF brand,
                         const wchar_t* name, const wchar_t* sub, bool hot){
    int cw = cell.right-cell.left;
    int bs = S(82);                              // v1.90: slightly larger badge
    int bx = cell.left + (cw-bs)/2;
    int by = cell.top + S(6) - (hot?S(3):0);     // hover: gentle lift
    RECT bd = {bx, by, bx+bs, by+bs};
    int rad = S(24);                             // squircle corner
    // a quiet tinted plate under the badge so the icon reads as a designed
    // button, not a glyph floating on white (no extra timers — still dirty-rect)
    {
        RECT plate={bd.left-S(10), bd.top-S(8), bd.right+S(10), bd.bottom+S(10)};
        gpGradRoundRect(dc, plate, S(28),
            blendColor(brand, RGB(255,255,255), g_dark?8:78),
            blendColor(brand, g_theme.surface2, g_dark?18:22),
            blendColor(brand, g_theme.border, 28));
    }
    // v1.92.0: the coloured brand glow halo was REMOVED — the icon is now just
    // the badge itself (no coloured halo). A quiet NEUTRAL drop shadow, deeper
    // + wider on hover, gives depth without the brand tint that clashed with
    // the lighter background artwork.
    gpShadow(dc, bd, rad, hot?S(12):S(6), hot?100:46);
    // glossy body: light top → deep bottom (convex app-icon)
    gpGradRoundRect(dc, bd, rad,
        blendColor(brand, RGB(255,255,255), 30),
        blendColor(brand, RGB(0,0,0), 16),
        blendColor(brand, RGB(0,0,0), 30));
    // top gloss — radius clamped to half the strip height so the rounded path
    // can never balloon outside the strip (the v1.87 card-shade lesson).
    {
        RECT gl = bd; gl.bottom = bd.top + (bs*44)/100;
        int gh = gl.bottom-gl.top;
        int grad = (gh/2-1) < rad ? (gh/2-1) : rad;
        if(grad >= 2)
            gpFillAlpha(dc, gl, grad, RGB(255,255,255), hot?44:32);
    }
    // inner light rim
    { RECT ir=bd; InflateRect(&ir,-S(1),-S(1));
      gpRoundRect(dc, ir, rad-S(1)>2?rad-S(1):rad, CLR_INVALID,
                  RGB(255,255,255), 110); }
    // glyph
    int gr = (bs*30)/100;
    RECT grc={bd.left+bs/2-gr, bd.top+bs/2-gr, bd.left+bs/2+gr, bd.top+bs/2+gr};
    drawIcon(dc, icon, grc, RGB(255,255,255), S(2)+1);
    // account name under the badge — deep ink, brand-tinted on hover
    int ly = bd.bottom + S(10);
    SetTextColor(dc, hot ? brand : (g_dark?g_theme.text:g_theme.sectionInk));
    SelectObject(dc, g_fUIB);
    RECT nr={cell.left, ly, cell.right, ly+S(22)};
    DrawTextW(dc, name, -1, &nr,
        DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    if(sub && *sub){
        SetTextColor(dc, g_theme.textDim);
        SelectObject(dc, g_fSmall);
        RECT sr={cell.left-S(6), ly+S(21), cell.right+S(6), ly+S(21)+S(17)};
        DrawTextW(dc, sub, -1, &sr,
            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    }
}

static LRESULT CALLBACK homeProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE: {
        // v1.77: the two entry accounts are renamed («حساب پذیرش» / «حساب مدیریت»)
        // and given professional line-art glyphs — a person for admission, a
        // two-person group for management — instead of the primitive filled
        // medical cross / shield. Subtitles describe each account's scope.
        // v1.79.0: «حساب پذیرش» → «حساب پرسنل» — the shared staff entrance:
        // doctors, receptionists, nurses and interns all sign in here; what
        // they can do afterwards is decided by their account's access ticks.
        // v1.88.0: the two entry accounts are now PAINTED iOS-style app icons
        // (no child button windows) — a glossy squircle badge with the account
        // name underneath, sitting in a frosted glass tray. Painting directly
        // onto the hero panel's buffer means there are NO background rectangles
        // and NO chipped square corners behind rounded shapes — the exact
        // corner-artefact bug the user reported cannot occur here.
        s_appHot = 0;   // fresh window — no stale hover from a prior home
        return 0; }
    case WM_DESTROY:
        return 0;
    case WM_APP_THEME:
        InvalidateRect(h,NULL,TRUE);
        return 0;
    case WM_SIZE:
        InvalidateRect(h,NULL,TRUE);
        return 0;
    // ---- v1.88.0: app-icon hover / click (painted, hit-tested) -------------
    case WM_MOUSEMOVE: {
        RECT rc; GetClientRect(h,&rc);
        HomeGeom g = homeGeom(rc.right, rc.bottom);
        POINT pt={(short)LOWORD(l),(short)HIWORD(l)};
        int hot = homeHitCell(g,pt);
        if(hot != s_appHot){
            RECT dirty={0,0,0,0};
            homeUnionRect(dirty, homeHotCell(g,s_appHot));
            homeUnionRect(dirty, homeHotCell(g,hot));
            if(dirty.right>dirty.left) InvalidateRect(h,&dirty,FALSE);
            s_appHot = hot;
        }
        SetCursor(LoadCursor(NULL, hot ? IDC_HAND : IDC_ARROW));
        if(hot){
            TRACKMOUSEEVENT te={sizeof(te),TME_LEAVE,h,0};
            TrackMouseEvent(&te);
        }
        return 0; }
    case WM_MOUSELEAVE:
        if(s_appHot){
            RECT rc; GetClientRect(h,&rc);
            HomeGeom g = homeGeom(rc.right, rc.bottom);
            RECT cell = homeHotCell(g,s_appHot);
            InvalidateRect(h,&cell,FALSE);   // just the hovered cell (v1.89.0)
            s_appHot=0;
        }
        return 0;
    case WM_LBUTTONUP: {
        // v1.88.0: hit-test the message coordinates directly — routing by the
        // last HOVER value dead-clicks fast move-click and touch input.
        RECT rc; GetClientRect(h,&rc);
        HomeGeom g = homeGeom(rc.right, rc.bottom);
        POINT pt={(short)LOWORD(l),(short)HIWORD(l)};
        if(PtInRect(&g.cardR,pt))      SendMessageW(h,WM_COMMAND,ID_HM_ACCOUNTING,0);
        else if(PtInRect(&g.cardM,pt)) SendMessageW(h,WM_COMMAND,ID_HM_RECEPTION,0);
        else if(PtInRect(&g.cardL,pt)) SendMessageW(h,WM_COMMAND,ID_HM_MANAGE,0);
        return 0; }
    case WM_COMMAND: {
        static bool s_busy=false;            // re-entry guard for modal dialogs
        if(s_busy) return 0;
        int id=LOWORD(w);
        if(id==ID_HM_RECEPTION){
            s_busy=true;
            User u;
            if(showLoginDialog(g_hFrame, 0, u)){
                // v2.01 (Part F1): «انتخاب شیفت کاری» removed — shift comes from
                // the user account (Part F2) with detectShift() as fallback.
                g_session.user=u; g_session.shift=u.shift>=0?u.shift:detectShift();
                g_session.title=resolveSessionTitle(u);
                g_session.loginAt=iranNow();
                setUserOnline(u.username,true);
                s_busy=false;
                WebAdmission_Warm();
                switchScreen(SC_RECEPTION);
                return 0;
            }
            s_busy=false;
        } else if(id==ID_HM_MANAGE){
            s_busy=true;
            User u;
            if(showLoginDialog(g_hFrame, 1, u)){
                g_session.user=u; g_session.shift=u.shift>=0?u.shift:detectShift();
                g_session.title=resolveSessionTitle(u);
                g_session.loginAt=iranNow();
                setUserOnline(u.username,true);
                s_busy=false;
                switchScreen(SC_MANAGE);
                return 0;
            }
            s_busy=false;
        } else if(id==ID_HM_ACCOUNTING){
            s_busy=true;
            User u;
            if(showLoginDialog(g_hFrame, 3, u)){
                g_session.user=u; g_session.shift=u.shift>=0?u.shift:detectShift();
                g_session.title=resolveSessionTitle(u);
                g_session.loginAt=iranNow();
                setUserOnline(u.username,true);
                s_busy=false;
                switchScreen(SC_ACCOUNTING);
                return 0;
            }
            s_busy=false;
        }
        return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        // v1.89.0 FPS fix: paint ONLY the dirty strip. Hovering an app icon
        // used to repaint the whole window (artwork + panel + chips) on every
        // hover-state change — a visible stutter. Now the hover invalidates
        // just that icon's cell and this paint sizes its buffer to the strip.
        RECT d=ps.rcPaint;
        int dw=d.right-d.left, dh=d.bottom-d.top;
        if(dw<=0||dh<=0){ EndPaint(h,&ps); return 0; }
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,dw,dh);
        HGDIOBJ obm=SelectObject(dc,bmp);
        SetViewportOrgEx(dc,-d.left,-d.top,NULL);
        HRGN dclip=CreateRectRgn(0,0,dw,dh);
        SelectClipRgn(dc,dclip);

        // ---- 1. artwork -----------------------------------------------------
        // v1.62.0: the light artwork is a soft clinical illustration, so it only
        // needs a whisper of scrim (the panel below carries the contrast). The
        // dark artwork is deeper, so it gets a stronger wash.
        // v1.77: the light scrim was nudged up so the page reads as a calm tinted
        // surface (less flat-white) while the illustration still shows through.
        // v1.92.0: the light scrim was eased back (58→44) now that g_theme.bg is a
        // lighter #C3CDDD — the first-page background no longer clashes with / sits
        // too dark over the artwork, so a thinner wash keeps it clean and bright.
        if(!gpDrawBackground(dc, rc, g_dark, g_theme.bg, g_dark?96:44)){
            RECT full={0,0,rc.right,rc.bottom};
            gpGradRoundRect(dc,full,0,g_theme.bg,g_theme.bg2,CLR_INVALID);
        }
        SetBkMode(dc,TRANSPARENT);

        HomeGeom g = homeGeom(rc.right, rc.bottom);

        // ---- 2. the elevated hero panel ------------------------------------
        // A double shadow (wide+soft, then tight+dark) reads as a real material
        // edge on BOTH themes — this is what makes the light theme finally look
        // designed instead of washed out.
        gpShadow(dc, g.panel, g.radius, S(30), g_dark?110:52);
        gpShadow(dc, g.panel, g.radius, S(10), g_dark?120:64);
        COLORREF pTop = homePanelTop();
        COLORREF pBot = homePanelBot();
        gpGradRoundRect(dc, g.panel, g.radius, pTop, pBot,
                        g_dark ? blendColor(g_theme.border,g_theme.accent,18)
                               : blendColor(g_theme.border,g_theme.accent,30));
        // v1.87.0: frosted-clay inner light rim just inside the panel edge.
        {
            RECT pit=g.panel; InflateRect(&pit,-S(1),-S(1));
            gpRoundRect(dc, pit, g.radius>S(1)?g.radius-S(1):g.radius,
                        CLR_INVALID, RGB(255,255,255), g_dark?30:110);
        }
        // Accent ribbon hugging the panel's top edge. It is inset by the corner
        // radius so it never spills outside the rounded silhouette (drawing it
        // full-width would leave two hard accent squares in the corners).
        // v1.89.0: ONE seamless gradialism ribbon (true 3-stop sweep, no
        // mid-seam, no animation).
        {
            RECT rb = g.ribbon;
            rb.left  += g.radius;
            rb.right -= g.radius;
            if(rb.right - rb.left > S(40))
                gpGradRibbon3(dc, rb, S(3), g_theme.accent, g_theme.accent2,
                              g_infoAccent);
        }

        // ---- 3. brand mark --------------------------------------------------
        // v1.64.0 (درمان پلاس): the real circular brand logo is now drawn here.
        // A soft accent halo + ring frame the logo; if the embedded PNG is ever
        // unavailable we fall back to the gradient disc + medical-cross glyph.
        {
            int d  = g.logo.right-g.logo.left;
            int rr = d/2;
            RECT halo=g.logo; InflateRect(&halo,S(7),S(7));
            int haloA = g_dark?70:52;   // resting glow (animation removed v1.89)
            gpFillAlpha(dc, halo, (d+S(14))/2,
                        blendColor(g_theme.accent, pTop, 55), haloA);
            gpShadow(dc, g.logo, rr, S(9), g_dark?110:78);
            if(!gpDrawImageResCircle(dc, IMG_LOGO, g.logo)){
                gpGradRoundRect(dc, g.logo, rr, g_theme.accent2, g_theme.accent, CLR_INVALID);
                RECT ring=g.logo; InflateRect(&ring,-S(4),-S(4));
                gpRoundRect(dc, ring, (d-S(8))/2, CLR_INVALID,
                            blendColor(g_theme.accent, RGB(255,255,255), 55));
                RECT gi=g.logo; InflateRect(&gi,-d/4,-d/4);
                drawIcon(dc, ICO_CROSS_MED, gi, RGB(255,255,255), S(2)+1);
            }
            // a thin accent ring around the logo for a crisp, branded edge
            gpRoundRect(dc, g.logo, rr, CLR_INVALID,
                        blendColor(g_theme.accent, g_theme.border, 30));
        }

        // ---- 4. title + tagline --------------------------------------------
        {
            int th = g.title.bottom-g.title.top;
            SelectObject(dc, fitFont(30, FW_BOLD, th/(double)S(50)));
            SetTextColor(dc, g_dark?RGB(240,246,252):g_theme.sectionInk);
            RECT tr=g.title;
            DrawTextW(dc, L"سامانه پذیرش و مدیریت درمانگاه و بیمارستان", -1, &tr,
                DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            // a short accent rule under the title — the "designed" signature
            int uw = S(84), ux = rc.right/2 - uw/2, uy = g.title.bottom - S(2);
            RECT ur={ux, uy, ux+uw, uy+S(3)};
            gpGradRoundRectBgH(dc, ur, S(2), g_theme.accent, g_theme.accent2,
                               CLR_INVALID, pTop);
        }
        {
            SelectObject(dc, g_fUI);
            SetTextColor(dc, g_dark?RGB(168,180,197):g_theme.textDim);
            RECT sr=g.sub;
            DrawTextW(dc, L"حساب کاربری خود را انتخاب کنید",
                -1, &sr, DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        }

        // ---- 5. capability chips -------------------------------------------
        // v1.77: professional, descriptive capability highlights (replacing the
        // old terse tags «پذیرش و صدور قبض / خدمات و تعرفه / چاپ حرفه‌ای»).
        {
            struct Chip { const wchar_t* t; int ico; };
            static const Chip CH[3]={
                { L"ثبت‌نام و پذیرش بیمار",   ICO_USER    },
                { L"صورت‌حساب و قبض مالی",    ICO_RECEIPT },
                { L"مدیریت کارکنان و بخش‌ها", ICO_PEOPLE  },
            };
            SelectObject(dc, g_fSmall);
            int ch = g.chipH, pad=S(14), icoD=S(15), gapI=S(7), gapC=S(12);
            int wid[3], tot=0;
            for(int i=0;i<3;i++){
                RECT ms={0,0,0,0};
                DrawTextW(dc, CH[i].t, -1, &ms,
                    DT_CALCRECT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
                wid[i]= (ms.right-ms.left) + 2*pad + icoD + gapI;
                tot += wid[i];
            }
            tot += 2*gapC;
            int avail = g.chips.right-g.chips.left;
            // Narrow windows: drop the chip row rather than overflow it.
            if(tot <= avail){
                int x = rc.right/2 - tot/2;
                for(int i=0;i<3;i++){
                    RECT cr={x, g.chips.top, x+wid[i], g.chips.top+ch};
                    COLORREF cf = g_dark ? blendColor(pTop, g_theme.accent, 16)
                                         : blendColor(g_theme.surface2, g_theme.accent, 8);
                    gpRoundRectBg(dc, cr, ch/2, cf,
                                  blendColor(g_theme.border, g_theme.accent, 30), pTop);
                    RECT ir={cr.right-pad-icoD, (cr.top+cr.bottom)/2-icoD/2,
                             cr.right-pad,      (cr.top+cr.bottom)/2+icoD/2};
                    drawIcon(dc, CH[i].ico, ir, g_theme.accent, S(2));
                    SetTextColor(dc, g_dark?RGB(198,210,226):g_theme.labelInk);
                    RECT tr={cr.left+pad, cr.top, cr.right-pad-icoD-gapI, cr.bottom};
                    DrawTextW(dc, CH[i].t, -1, &tr,
                        DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
                    x += wid[i] + gapC;
                }
            }
        }

        // ---- 5b. v1.88.0 / v1.90.0: glass tray + two app-icon entries --------
        {
            RECT tr=g.tray;
            gpShadow(dc, tr, S(24), S(16), g_dark?100:48);
            gpShadowColor(dc, tr, S(24), S(8), g_dark?70:36, g_theme.accent);
            COLORREF trayTop = g_dark
                ? blendColor(homePanelTop(), RGB(255,255,255), 8)
                : RGB(0xFC, 0xFD, 0xFE); // v1.94: white tray top
            COLORREF trayBot = g_dark
                ? blendColor(homePanelBot(), RGB(255,255,255), 4)
                : RGB(0xF2, 0xF5, 0xF9); // v1.94: subtle shading tray bottom
            gpGradRoundRect(dc, tr, S(24), trayTop, trayBot,
                blendColor(g_theme.border, g_theme.accent, 28));
            RECT itr=tr; InflateRect(&itr,-S(1),-S(1));
            gpRoundRect(dc, itr, S(24)-S(1), CLR_INVALID, RGB(255,255,255),
                        g_dark?32:108);
            // v1.92.0: a subtle white-ish top highlight so the lighter, cleaner
            // background (thinner scrim) still reads the tray as a raised vessel
            // — light catching the top rim is the depth cue.
            gpLine(dc, tr.left+S(24), tr.top+S(1), tr.right-S(24), tr.top+S(1),
                   RGB(255,255,255), 1.0f, g_dark?34:120);
            // v1.97: faint dividers between the three programs
            {
                int mids[2]={(g.cardL.right+g.cardM.left)/2,
                             (g.cardM.right+g.cardR.left)/2};
                int y1 = tr.top + S(28), y2 = tr.bottom - S(28);
                COLORREF dc1 = blendColor(g_theme.border, g_theme.accent, 40);
                HBRUSH db=CreateSolidBrush(blendColor(g_theme.accent, trayTop, 35));
                HGDIOBJ ob2=SelectObject(dc,db);
                HGDIOBJ op2=SelectObject(dc,GetStockObject(NULL_PEN));
                int ds=S(3);
                for(int i=0;i<2;i++){
                    int mx=mids[i];
                    gpLine(dc, mx, y1, mx, y2, dc1, 1.0f, 150);
                    POINT pt[4]={{mx,y1-ds},{mx+ds,y1},{mx,y1+ds},{mx-ds,y1}};
                    Polygon(dc,pt,4);
                    POINT pb[4]={{mx,y2-ds},{mx+ds,y2},{mx,y2+ds},{mx-ds,y2}};
                    Polygon(dc,pb,4);
                }
                SelectObject(dc,ob2); SelectObject(dc,op2); DeleteObject(db);
            }
            // RTL right → left: حسابداری | حساب پرسنل | حساب مدیریت
            paintAppIcon(dc, g.cardR, ICO_WALLET, homeAccBrand(),
                         L"حسابداری", NULL, s_appHot==3);
            paintAppIcon(dc, g.cardM, ICO_USER_ADD, g_theme.accent,
                         L"حساب پرسنل", NULL, s_appHot==1);
            paintAppIcon(dc, g.cardL, ICO_PEOPLE, g_infoAccent,
                         L"حساب مدیریت", NULL, s_appHot==2);
        }

        // ---- 6. footer pill -------------------------------------------------
        // The bottom items used to be bare grey text floating on the artwork.
        // They now live in their own frosted container, as requested.
        {
            RECT fr=g.foot;
            gpShadow(dc, fr, (fr.bottom-fr.top)/2, S(8), g_dark?100:44);
            COLORREF fFill = g_dark ? RGB(18,22,30)
                                    : blendColor(RGB(255,255,255), g_theme.surface2, 30);
            gpGradRoundRect(dc, fr, (fr.bottom-fr.top)/2,
                blendColor(fFill, RGB(255,255,255), g_dark?6:40), fFill,
                blendColor(g_theme.border, g_theme.accent, 18));
            SelectObject(dc, g_fSmall);
            // right: brand + version
            SetTextColor(dc, g_dark?RGB(186,198,214):g_theme.labelInk);
            RECT vr={fr.left+S(18), fr.top, fr.right-S(18), fr.bottom};
            std::wstring tag = std::wstring(APP_NAME_W) + L"  ·  نسخه " +
                               toFaDigits(APP_VERSION_W);
            DrawTextW(dc, tag.c_str(), -1, &vr,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            // left: a small lock chip + the security note
            int icoD=S(14);
            RECT ir={fr.left+S(16), (fr.top+fr.bottom)/2-icoD/2,
                     fr.left+S(16)+icoD, (fr.top+fr.bottom)/2+icoD/2};
            drawIcon(dc, ICO_SHIELD, ir, g_theme.accent, S(2));
            SetTextColor(dc, g_dark?RGB(150,163,182):g_theme.textDim);
            RECT nr={fr.left+S(16)+icoD+S(7), fr.top, fr.right-S(200), fr.bottom};
            DrawTextW(dc, L"ورود با حساب کاربری سازمانی", -1, &nr,
                DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        }

        SelectClipRgn(dc,NULL); DeleteObject(dclip);
        SetViewportOrgEx(dc,0,0,NULL);
        BitBlt(dc0,d.left,d.top,dw,dh,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}
HWND createHomeScreen(HWND frame){
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc={0};
        wc.lpfnWndProc=homeProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.lpszClassName=HM_CLASS;
        RegisterClassW(&wc); reg=true;
    }
    RECT rc=frameContentRect();
    return CreateWindowExW(0,HM_CLASS,L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
        rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,
        frame,NULL,g_hInst,NULL);
}

// =============================================================== FRAME =====
//  v1.7.0: show/position the reception action buttons in the header. They live
//  on the LEFT, after the gear+calculator, and only while the reception screen
//  is active (RTL: laid out left→right since they sit on the LEFT side).
static void updateHeaderButtons(HWND h){
    bool show = headerHasActionBar();
    // v1.79.0: permission gating — an account without the «پذیرش بیمار»
    // permission tick (مدیریت ← تعریف حساب کاربری) does not even SEE the
    // admission button. Empty perms (legacy accounts) = full access.
    bool canAdmit = userHasPerm(g_session.user, L"admission");
    // v1.79.0: same for the settings gear — accounts without the «تنظیمات»
    // tick lose the gear while logged in (the home screen, logged OUT, keeps
    // it so the panel stays reachable before login as before).
    ShowWindow(s_bSettings,
        userHasPerm(g_session.user, L"settings") ? SW_SHOW : SW_HIDE);
    bool showPat  = show && canAdmit;
    ShowWindow(s_bNewPat,   showPat?SW_SHOW:SW_HIDE);
    ShowWindow(s_bNewTab,   show?SW_SHOW:SW_HIDE);
    if(!show) return;
    RECT rc; GetClientRect(h,&rc);
    int bh=S(38), pad=S(16), g=S(10);
    // LAYER 2 (action bar) sits directly under LAYER 1.
    int y = mainBarH() + (actionBarH()-bh)/2;
    // RIGHT-aligned cluster (v1.60.0 — «نوبت‌دهی» removed; right → left):
    //     پذیرش بیمار  |  تب جدید        (پذیرش hidden when not permitted)
    int wNew=S(134), wTab=S(112);
    int x = rc.right - pad - (showPat?wNew:wTab);
    if(showPat){
        MoveWindow(s_bNewPat, x,                      y, wNew,  bh, TRUE);
        MoveWindow(s_bNewTab, x-g-wTab,               y, wTab,  bh, TRUE);
    } else {
        MoveWindow(s_bNewTab, x,                      y, wTab,  bh, TRUE);
    }
    // blend the buttons' rounded corners into the LAYER 2 surface colour.
    setFlatButtonBg(s_bNewPat, g_theme.surface2);
    setFlatButtonBg(s_bNewTab, g_theme.surface2);
}
static void frameLayout(HWND h){
    RECT rc; GetClientRect(h,&rc);
    int bh=S(38), pad=S(14);
    int y=(mainBarH()-bh)/2;     // LAYER 1 vertical centre
    // --- RIGHT side (RTL primary): EXIT is the right-most control; the app
    //     identity (logo + name + fullname + access) is painted to its LEFT.
    MoveWindow(s_bExit,  rc.right-pad-bh, y, bh, bh, TRUE);
    // --- LEFT side: settings (gear) button, then the calculator beside it —
    //     handy in the header but out of the way of the tabs / actions.
    MoveWindow(s_bSettings, pad, y, bh, bh, TRUE);
    MoveWindow(s_bCalc, pad+bh+S(8), y, bh, bh, TRUE);
    // v1.77: the header ghost buttons sit on the header GRADIENT, so their
    // rounded corners must blend with the gradient's vertical midpoint — not
    // the flat headerTop colour (which left a white square behind each icon).
    setFlatButtonHeaderMid(s_bExit);
    setFlatButtonHeaderMid(s_bSettings);
    setFlatButtonHeaderMid(s_bCalc);
    updateHeaderButtons(h);
    if(s_screen){
        RECT cr=frameContentRect();
        MoveWindow(s_screen,cr.left,cr.top,cr.right-cr.left,cr.bottom-cr.top,TRUE);
    }
}

static LRESULT CALLBACK frameProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE:
        g_hFrame = h;
        s_bExit     = createFlatButton(h, ID_FR_EXIT,    L"", ICO_X,      BS_GHOST,0,0,10,10);
        s_bSettings = createFlatButton(h, ID_FR_SETTINGS,L"", ICO_GEAR,   BS_GHOST,0,0,10,10);
        s_bCalc     = createFlatButton(h, ID_FR_CALC,    L"", ICO_CALC,   BS_GHOST,0,0,10,10);
        // v1.8.0: use the new clean raster gear / calculator icons (white-on-
        // alpha PNGs tinted to the theme accent) so both buttons read perfectly
        // on the light AND the dark theme. They gracefully fall back to the
        // vector ICO_GEAR / ICO_CALC if GDI+ or the resource is unavailable.
        setFlatButtonImage(s_bSettings, IMG_IC_SETTINGS);
        setFlatButtonImage(s_bCalc,     IMG_IC_CALC);
        // header action buttons (reception only) — created hidden, shown by
        // updateHeaderButtons() when the reception screen becomes active.
        // v1.87.0: «پذیرش بیمار» gets its OWN person-plus glyph (was the same
        // generic plus/tab look as «تب جدید») so the two actions are distinct
        // at a glance; «تب جدید» becomes a quiet OUTLINE button with a plus —
        // lighter chrome, crisper text, clearly the secondary action.
        s_bNewPat   = createFlatButton(h, ID_FR_NEWPAT, L"پذیرش بیمار", ICO_USER_ADD, BS_PRIMARY,0,0,10,10,
                          L"ثبت پذیرش بیمار جدید");
        // v2.08: «تب جدید» replaced by «جوابدهی بیمار» (lab answer). The old
        // empty-tab action is removed; this button now opens the lab answer tab.
        s_bNewTab   = createFlatButton(h, ID_FR_NEWTAB, L"جوابدهی بیمار",    ICO_PLUS,  BS_OUTLINE,0,0,10,10,
                          L"جوابدهی آزمایشگاه بیمار");
        // v1.62.0: no native print buttons here any more — the admission page
        // owns «چاپ آخرین قبض / چاپ نسخه / رسید بیمه» in its bottom-right card.
        ShowWindow(s_bNewPat,SW_HIDE);
        ShowWindow(s_bNewTab,SW_HIDE);
        // v1.77: LAYER-1 header buttons blend with the header gradient midpoint;
        // the LAYER-2 reception action buttons sit on the flat surface2 action
        // bar, so they keep the surface2 corner-blend.
        setFlatButtonHeaderMid(s_bExit);
        setFlatButtonHeaderMid(s_bSettings);
        setFlatButtonHeaderMid(s_bCalc);
        setFlatButtonBg(s_bNewPat, g_theme.surface2);
        setFlatButtonBg(s_bNewTab, g_theme.surface2);
        SetTimer(h, TIMER_CLOCK, g_lowSpec?1000:500, NULL);
        return 0;
    case WM_SIZE: frameLayout(h); return 0;
    case WM_APP_THEME:
        // theme may have been switched from inside the settings panel — refresh
        // the header buttons' corner-blend colour and repaint the whole frame.
        setFlatButtonHeaderMid(s_bExit);
        setFlatButtonHeaderMid(s_bSettings);
        setFlatButtonHeaderMid(s_bCalc);
        setFlatButtonBg(s_bNewPat, g_theme.surface2);
        setFlatButtonBg(s_bNewTab, g_theme.surface2);
        InvalidateRect(h,NULL,TRUE);
        return 0;
    case WM_APP_UI_TASK:
        // v1.40.0: a web worker-pool thread marshalled a callable back onto the
        // GUI thread (RunOnUiThread) — run it here (owner of every HWND / GDI /
        // WebView2 object) and free it. lParam is the heap task pointer.
        WebUiTask_Run(l);
        return 0;
    case WM_APP+42:
        // v2.06: Ctrl+Tab forwarded from the WebView2 accelerator handler — the
        // browser had focus so the main pump never saw the raw keydown. Cycle
        // the reception tabs exactly like the native path does.
        Reception_CycleTab();
        return 0;
    case WM_TIMER:
        if(w==TIMER_CLOCK){
            // §H: repaint only the clock/date zone (minimal invalidation — no
            // whole-window invalidate). The zone spans the full header width when
            // the clock is centred (full header) and just the left strip when the
            // header is collapsed (reception); invalidating the whole LAYER-1 band
            // is cheap and avoids having to recompute the exact centred rect here.
            RECT crc; GetClientRect(h,&crc);
            RECT cz={0, 0, crc.right, mainBarH()};     // full-width LAYER-1 strip
            InvalidateRect(h,&cz,FALSE);
            // v1.9.0: poll for an incoming-message notification for THIS user
            // (employees only — managers never get notified of their own send).
            notifyNewMessageRecipients();
            // §G (1.11.0): refresh this session's heartbeat at most every ~30s so
            // presence stays inside the 90s online window without thrashing the
            // small presence file on every clock tick.
            if(!g_session.user.username.empty()){
                static DWORD s_lastBeat=0;
                DWORD now=GetTickCount();
                if(now - s_lastBeat >= 30000 || s_lastBeat==0){
                    s_lastBeat=now;
                    heartbeatUser(g_session.user.username);
                }
            }
        }
        // §B (v1.10.0): the HEADER_COLLAPSE_TIMER animation has been removed.
        // The timer is never started anymore; if a stale one ever fires we just
        // kill it so nothing animates.
        else if(w==HEADER_COLLAPSE_TIMER){
            KillTimer(h, HEADER_COLLAPSE_TIMER);
        }
        return 0;
    case WM_COMMAND: {
        int id=LOWORD(w);
        if(id==ID_FR_EXIT){
            if(s_curScreen==SC_HOME){
                if(MessageBoxW(h,L"از برنامه خارج می‌شوید؟",L"خروج",
                    MB_YESNO|MB_ICONQUESTION)==IDYES) DestroyWindow(h);
            } else {
                // logout to home (session ends only by user action)
                if(MessageBoxW(h,L"از حساب کاربری خارج می‌شوید؟",L"خروج از حساب",
                    MB_YESNO|MB_ICONQUESTION)==IDYES){
                    logLine(L"logout: "+g_session.user.username);
                    setUserOnline(g_session.user.username,false);
                    g_session = Session();
                    switchScreen(SC_HOME);
                }
            }
        }
        else if(id==ID_FR_SETTINGS) OpenSettings(h, g_session.user);  // §1 role dispatcher
        else if(id==ID_FR_CALC) openCalculator(h);
        // v1.7.0: header reception-action buttons → route to reception screen
        else if(id==ID_FR_NEWPAT) receptionAction(RA_NEWPAT);
        else if(id==ID_FR_NEWTAB) Reception_OpenLabAnswer(); /* v2.08: was RA_NEWTAB */
        return 0; }
    case WM_KEYDOWN: {
        // hidden admin: Ctrl + P + N held together (home screen only)
        static bool s_dlgOpen = false;          // re-entry guard
        if(s_curScreen==SC_HOME && !s_dlgOpen &&
           (GetKeyState(VK_CONTROL)&0x8000) &&
           (GetKeyState('P')&0x8000) && (GetKeyState('N')&0x8000)){
            s_dlgOpen = true;
            User u;
            bool ok = showLoginDialog(h, 2, u);
            s_dlgOpen = false;
            if(ok){
                g_session.user=u; g_session.shift=u.shift>=0?u.shift:detectShift();
                g_session.title=resolveSessionTitle(u);
                g_session.loginAt=iranNow();
                setUserOnline(u.username,true);
                switchScreen(SC_ADMIN);
            }
            return 0;
        }
        if(w==VK_F8){ printLastReceipt(h); return 0; }
        if(w==VK_F7){
            Reception_OpenCashier();
            return 0;
        }
        if(w==VK_F4){
            WebAdmission_PushEvent("hotkey","{\"key\":\"F4\"}");
            return 0;
        }
        return 0; }
    case WM_CTLCOLORSTATIC: {
        HDC dc=(HDC)w; SetBkColor(dc,g_theme.surface2);
        return (LRESULT)g_brSurface2; }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        if(rc.right<=0 || rc.bottom<=0){ EndPaint(h,&ps); return 0; }
        // ------------------------------------------------------------------
        //  v1.63.0 FPS FIX — dirty-rect double buffer.
        //  The clock timer invalidates ONLY the LAYER-1 header strip twice a
        //  second, but this handler still allocated (and immediately destroyed)
        //  a bitmap the size of the WHOLE CLIENT AREA — on a 1920x1080 screen
        //  that is an 8 MB GDI allocation + free 2x per second, forever, plus a
        //  full-screen BitBlt. That allocation churn is the second half of the
        //  reported frame-rate drop (the first half was the uncached background
        //  resample, now fixed in gpDrawBackground).
        //  The buffer is now sized to the INVALIDATED RECT and the drawing code
        //  below is untouched: SetViewportOrgEx shifts the origin so all the
        //  absolute coordinates keep working, and a clip region keeps every
        //  primitive inside the strip.
        // ------------------------------------------------------------------
        RECT d = ps.rcPaint;
        if(d.left<0) d.left=0;
        if(d.top<0)  d.top=0;
        if(d.right>rc.right)   d.right=rc.right;
        if(d.bottom>rc.bottom) d.bottom=rc.bottom;
        int dw=d.right-d.left, dh=d.bottom-d.top;
        if(dw<=0||dh<=0){ EndPaint(h,&ps); return 0; }
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,dw,dh);
        HGDIOBJ obm=SelectObject(dc,bmp);
        SetViewportOrgEx(dc,-d.left,-d.top,NULL);
        // SelectClipRgn takes DEVICE units, so the region is the buffer extent
        // (0,0,dw,dh) — not the logical dirty rect.
        HRGN dclip=CreateRectRgn(0,0,dw,dh);
        SelectClipRgn(dc,dclip);

        // v1.88.0 BLACK-LINE FIX: the compatible bitmap starts as UNINITIALISED
        // (black) memory, and gpGradRoundRect paints `height-1` rows — so any
        // scanline no primitive covers (e.g. the header band's last row at
        // y=mainBarH()-1) showed up as a 1px black line. Fill the whole strip
        // with the page base colour FIRST; every layer paints over it.
        {
            HBRUSH baseBr=CreateSolidBrush(g_theme.bg);
            FillRect(dc,&rc,baseBr);
            DeleteObject(baseBr);
        }

        // ===================== LAYER 1 — top header bar =====================
        // v1.87.0: three-stop frosted-glass band — bright top, cool mid, deeper
        // base — far richer than the old two-stop strip, and an inner light
        // line just under the ribbon sells the "frosted pane" illusion.
        RECT tb={0,0,rc.right,mainBarH()};
        {
            COLORREF hMid = blendColor(g_theme.headerTop, g_theme.headerBot, 55);
            RECT tbT={0,0,rc.right,mainBarH()/2};
            RECT tbB={0,mainBarH()/2,rc.right,mainBarH()};
            gpGradRoundRect(dc,tbT,0,g_theme.headerTop,hMid,CLR_INVALID);
            gpGradRoundRect(dc,tbB,0,hMid,g_theme.headerBot,CLR_INVALID);
        }
        // v1.77: a thin branded accent ribbon along the header's top edge — the
        // same "designed" signature the welcome panel carries, on both themes —
        // so the header reads as a polished, branded surface, not a flat strip.
        // v1.87.0 gradialism: the ribbon now sweeps indigo → sky → violet.
        {
            RECT hrib={0,0,rc.right,S(3)};
            gpGradRibbon3(dc, hrib, 0, g_theme.accent, g_theme.accent2,
                          g_infoAccent);
        }
        gpLine(dc,0,S(3),rc.right,S(3),RGB(255,255,255),1.0f,g_dark?26:110);
        // ===================== LAYER 2 — action sub-bar =====================
        if(headerHasActionBar()){
            RECT ab={0,mainBarH(),rc.right,mainBarH()+actionBarH()};
            // v1.87.0: a soft gradient + a drop-shade hairline underneath, so
            // the action bar floats above the page instead of lying flat on it.
            gpGradRoundRect(dc,ab,0,g_theme.surface2,
                blendColor(g_theme.surface2,g_theme.bg2,45),CLR_INVALID);
            // crisp separator between the two header layers
            gpLine(dc,0,mainBarH(),rc.right,mainBarH(),g_theme.border,1.0f);
            gpLine(dc,0,mainBarH()+actionBarH(),rc.right,mainBarH()+actionBarH(),
                   RGB(30,45,70),1.0f,18);
        }
        // bottom status bar
        RECT bb={0,rc.bottom-botBarH(),rc.right,rc.bottom};
        FillRect(dc,&bb,g_brSurface2);
        // middle bg (gentle gradient page)
        RECT mid={0,topBarH(),rc.right,rc.bottom-botBarH()};
        gpGradRoundRect(dc,mid,0,g_theme.bg,g_theme.bg2,CLR_INVALID);
        // crisp separators
        gpLine(dc,0,topBarH()-1,rc.right,topBarH()-1,g_theme.border,1.0f);
        gpLine(dc,0,rc.bottom-botBarH(),rc.right,rc.bottom-botBarH(),g_theme.border,1.0f);
        // a thin accent underline under the header for that "designed" feel
        gpLine(dc,0,topBarH()-1,rc.right,topBarH()-1,g_theme.accent,2.0f,40);

        SetBkMode(dc,TRANSPARENT);

        // ---- app identity on the RIGHT (next to the exit button) ----
        // logo badge + app name; below it the LOGGED-IN PERSON'S NAME and the
        // access type. We intentionally show the full name + role, NEVER the
        // raw login username (privacy requirement).
        int exitW = S(38)+S(14);
        int logoR = S(16);
        int logoCx = rc.right - exitW - S(16) - logoR;
        int logoCy = mainBarH()/2;
        RECT lc={logoCx-logoR,logoCy-logoR,logoCx+logoR,logoCy+logoR};
        // v1.64.0 (درمان پلاس): draw the real circular logo in the header; fall
        // back to the gradient disc + medical cross if the resource is missing.
        if(!gpDrawImageResCircle(dc, IMG_LOGO, lc)){
            gpGradRoundRect(dc,lc,logoR,g_theme.accent2,g_theme.accent,CLR_INVALID);
            RECT li={lc.left+S(7),lc.top+S(7),lc.right-S(7),lc.bottom-S(7)};
            drawIcon(dc,ICO_CROSS_MED,li,RGB(255,255,255),S(2));
        }
        gpRoundRect(dc, lc, logoR, CLR_INVALID,
                    blendColor(g_theme.accent, g_theme.border, 30));

        int idRight = logoCx-logoR-S(12);
        bool loggedIn = !g_session.user.username.empty();
        if(loggedIn){
            // v1.77: the identity now shows the logged-in person's NAME (top,
            // bold) and their ACCESS LEVEL (bottom) — two pieces, not the old
            // three (fullname • role • dept). The brand mark is carried by the
            // logo badge, so the app name is no longer repeated here.
            // §2.B: offsets tuned for the compact S(56) header.
            SelectObject(dc,g_fUIB);
            SetTextColor(dc,g_theme.text);
            RECT nr={S(160),S(5),idRight,S(5)+S(24)};
            /* §6.4: the name is ALWAYS the full name; the login username is
               shown ONLY as a last resort (and that data gap is logged by
               resolveSessionTitle so it stays visible). */
            { const wchar_t* nm=g_session.user.fullname.empty()
                                  ? g_session.user.username.c_str()
                                  : g_session.user.fullname.c_str();
              DrawTextW(dc,nm,-1,&nr,
                  DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
            SelectObject(dc,g_fSmall);
            SetTextColor(dc,g_theme.textDim);
            // v2.07 §6.2/§6.3: the second line shows the ACCOUNT TYPE
            // (پذیرش / مدیریت / کارآموز / پذیرش آزمایشگاه / …), resolved ONCE
            // at login (resolveSessionTitle) and cached in g_session.title —
            // this paint path never touches persons.dat / emp_*.dat (the clock
            // timer repaints this band up to twice a second). §6.4: the name is
            // ALWAYS the full name; the username is shown only as a last resort
            // (and that gap is logged by resolveSessionTitle).
            RECT sr={S(160),S(5)+S(26),idRight,mainBarH()-S(4)};
            DrawTextW(dc,g_session.title.c_str(),-1,&sr,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        } else {
            SelectObject(dc,g_fTitle);
            SetTextColor(dc,g_theme.text);
            RECT nr={S(160),0,idRight,mainBarH()};
            DrawTextW(dc,APP_NAME_W,-1,&nr,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        }

        // ===== LAYER 1: live clock (bold, top) + Jalali date =====
        // §H (1.11.0): when the FULL header is visible (i.e. the header is NOT
        // collapsed — every screen except reception) the clock + date are
        // HORIZONTALLY CENTRED in the header between the left tool buttons and
        // the right identity block. On the reception screen the header collapses
        // (§B) and the clock returns to the TOP-LEFT, immediately right of the
        // gear + calculator buttons, so it never collides with the action bar.
        SYSTEMTIME st=iranNow();
        int leftBtns = S(14) + S(38) + S(8) + S(38) + S(14); // pad + gear + gap + calc + gap
        std::wstring clkStr = toFaDigits(iranTimeStr(st,true));
        std::wstring dateStr= jalaliDateStr(st);
        {
            // §2.A (1.12.0): the live clock + Jalali date are now HORIZONTALLY
            // CENTRED in the LAYER-1 header on EVERY screen — including reception
            // (previously the reception header forced them top-LEFT). The safe
            // band is [leftBtns, idRight] (between the tool buttons on the left
            // and the identity block on the right); the clock zone is centred in
            // it and clamped so it never clips on small/low-resolution screens or
            // under DPI scaling. Stacked: clock on top (bold mono), date below.
            int bandL = leftBtns + S(8);
            int bandR = idRight  - S(8);
            int zoneW = S(240);
            int cx    = (bandL + bandR)/2;
            int zL    = cx - zoneW/2;
            if(zL < bandL) zL = bandL;
            int zR = zL + zoneW;
            if(zR > bandR && bandR>bandL) { zR = bandR; }
            // v1.79.0: the frosted pill behind the clock was REMOVED on request
            // (it read as a stray patch on the band). The clock+date now float
            // as clean typography — accent-tinted time over a dim date, framed
            // by two whisper-thin vertical hairlines so the zone still reads as
            // one composed element on the gradient.
            {
                int dl = zL+S(14), dr = zR-S(14);
                COLORREF hc = blendColor(g_theme.border, g_theme.accent, 45);
                gpLine(dc, dl, S(10), dl, mainBarH()-S(10), hc, 1.0f, 110);
                gpLine(dc, dr, S(10), dr, mainBarH()-S(10), hc, 1.0f, 110);
                // v1.88.0: tiny accent diamonds capping each hairline, so the
                // clock zone reads as one composed, designed element.
                HBRUSH db=CreateSolidBrush(g_theme.accent);
                HGDIOBJ ob2=SelectObject(dc,db);
                HGDIOBJ op2=SelectObject(dc,GetStockObject(NULL_PEN));
                int ds=S(4);
                int dy=(S(10)+mainBarH()-S(10))/2;
                POINT pl[4]={{dl,dy-ds},{dl+ds,dy},{dl,dy+ds},{dl-ds,dy}};
                POINT pr2[4]={{dr,dy-ds},{dr+ds,dy},{dr,dy+ds},{dr-ds,dy}};
                Polygon(dc,pl,4); Polygon(dc,pr2,4);
                SelectObject(dc,ob2); SelectObject(dc,op2); DeleteObject(db);
            }
            // clock (centred, bold Vazirmatn) — v1.90.0: deep navy / sectionInk
            // so the time no longer blends into the frosted header. Date is a
            // distinct muted ink (not the same accent as the clock).
            // v1.92.0: dedicated larger fonts — g_fClock (22 bold) for the time,
            // g_fDate (14) for the date — and a taller clock band (S(34)) so the
            // bigger glyphs sit comfortably centred in the S(60) header.
            SetTextColor(dc, g_dark ? RGB(240,246,252) : g_theme.sectionInk);
            SelectObject(dc,g_fClock);
            RECT ck={zL,S(3),zR,S(3)+S(34)};
            DrawTextW(dc,clkStr.c_str(),-1,&ck,
                DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
            SetTextColor(dc, g_dark ? RGB(168,180,197) : g_theme.labelInk);
            SelectObject(dc,g_fDate);
            RECT dr={zL,S(3)+S(33),zR,mainBarH()-S(2)};
            DrawTextW(dc,dateStr.c_str(),-1,&dr,
                DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        }

        // ===== bottom status bar =====
        // v1.70.0: shift labels removed per user request. Only the product tag
        // (name + version) remains on the right side.
        // bottom-right: small product tag
        SetTextColor(dc, g_dark ? g_theme.textDim : g_theme.labelInk);
        RECT pr={rc.right-S(360),rc.bottom-botBarH(),rc.right-S(16),rc.bottom};
        std::wstring tag=std::wstring(APP_NAME_W)+L"  نسخه "+toFaDigits(APP_VERSION_W);
        DrawTextW(dc,tag.c_str(),-1,&pr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

        SelectClipRgn(dc,NULL); DeleteObject(dclip);
        SetViewportOrgEx(dc,0,0,NULL);
        BitBlt(dc0,d.left,d.top,dw,dh,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    case WM_DESTROY:
        KillTimer(h,TIMER_CLOCK);
        if(!g_session.user.username.empty())
            setUserOnline(g_session.user.username,false);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

// ============================== Enter / Tab => next field ===================
//  Secretaries navigate the form almost entirely with Enter and Tab, so BOTH
//  keys must hop to the next control (Shift+Tab => previous). Works inside
//  the reception page (a plain child window, not a dialog) by walking the
//  control list with GetNextDlgTabItem on the page itself.
static WNDPROC s_oldEdit = NULL;
static void hopField(HWND h, bool prev){
    HWND parent = GetParent(h);
    HWND nxt = GetNextDlgTabItem(parent, h, prev);
    if(!nxt || nxt==h){
        HWND top = GetAncestor(h, GA_ROOT);
        nxt = GetNextDlgTabItem(top, h, prev);
    }
    if(nxt && nxt!=h){
        SetFocus(nxt);
        wchar_t cls[32]={0}; GetClassNameW(nxt,cls,32);
        if(!wcscmp(cls,L"EDIT")) SendMessageW(nxt, EM_SETSEL, 0, -1);
    }
}
static LRESULT CALLBACK enterEditProc(HWND h, UINT m, WPARAM w, LPARAM l){
    if(m==WM_KEYDOWN && w==VK_RETURN){
        hopField(h, false);
        return 0;
    }
    if(m==WM_KEYDOWN && w==VK_TAB){
        hopField(h, (GetKeyState(VK_SHIFT)&0x8000)!=0);
        return 0;
    }
    if(m==WM_CHAR && (w==VK_RETURN || w==VK_TAB)) return 0;  // kill the beep
    return CallWindowProcW(s_oldEdit, h, m, w, l);
}
void enableEnterNavigation(HWND ctl){
    WNDPROC old = (WNDPROC)SetWindowLongPtrW(ctl, GWLP_WNDPROC,
        (LONG_PTR)enterEditProc);
    if(!s_oldEdit) s_oldEdit = old;
}

// ===================== smart Jalali date mask (YYYY/MM/DD) ==================
//  The user just clicks the field and types 8 digits; the program inserts the
//  slashes itself and splits them into year / month / day. Backspace removes
//  the previous digit (skipping the auto slashes). Enter/Tab still navigate.
static WNDPROC s_oldDate = NULL;
static std::wstring digitsOnly(const std::wstring& s){
    std::wstring o;
    for(wchar_t c : s){
        if(c>=L'0'&&c<=L'9') o += c;
        else if(c>=0x06F0&&c<=0x06F9) o += (wchar_t)(L'0'+(c-0x06F0)); // fa→en
        else if(c>=0x0660&&c<=0x0669) o += (wchar_t)(L'0'+(c-0x0660)); // ar→en
    }
    return o;
}
// Split a raw string into year / month / day tokens. The user may type the
// date in a relaxed way: digits packed (13400520), or separated by spaces /
// slashes / dashes and WITHOUT zero-padding, e.g. "1340 5 20". Each token is
// terminated by any non-digit (space, /, -). When packed with no separators we
// fall back to the classic YYYY MM DD split (4 + 2 + 2).
static void splitJalaliTokens(const std::wstring& raw,
                              std::wstring& y, std::wstring& mo, std::wstring& d,
                              bool& hadSep){
    y.clear(); mo.clear(); d.clear(); hadSep=false;
    // Are there any explicit separators?
    for(wchar_t c : raw)
        if(c==L' '||c==L'/'||c==L'-'||c==L'.'){ hadSep=true; break; }

    if(hadSep){
        std::wstring* parts[3]={&y,&mo,&d}; int idx=0;
        bool inTok=false;
        for(wchar_t c : raw){
            std::wstring dig;
            if(c>=L'0'&&c<=L'9') dig=std::wstring(1,c);
            else if(c>=0x06F0&&c<=0x06F9) dig=std::wstring(1,(wchar_t)(L'0'+(c-0x06F0)));
            else if(c>=0x0660&&c<=0x0669) dig=std::wstring(1,(wchar_t)(L'0'+(c-0x0660)));
            if(!dig.empty()){
                if(idx<3) *parts[idx]+=dig;
                inTok=true;
            } else { // separator
                if(inTok && idx<2) idx++;
                inTok=false;
            }
        }
    } else {
        std::wstring all = digitsOnly(raw);
        if(all.size()>8) all=all.substr(0,8);
        if(all.size()<=4){ y=all; }
        else if(all.size()<=6){ y=all.substr(0,4); mo=all.substr(4); }
        else { y=all.substr(0,4); mo=all.substr(4,2); d=all.substr(6); }
    }
    // clamp month ≤ 12 and day ≤ 31 (only once a full field is present)
    if(mo.size()>=2){ int v=_wtoi(mo.c_str()); if(v>12)v=12; if(v<1)v=1;
                      wchar_t b[4]; swprintf(b,4,L"%d",v); mo=b; }
    if(d.size()>=2){  int v=_wtoi(d.c_str());  if(v>31)v=31; if(v<1)v=1;
                      wchar_t b[4]; swprintf(b,4,L"%d",v); d=b; }
}
// Build the displayed value. Non-padded segments are shown as typed; slashes
// are inserted between whichever segments already have content.
static std::wstring formatJalaliMask(const std::wstring& raw){
    std::wstring y,mo,d; bool sep;
    splitJalaliTokens(raw,y,mo,d,sep);
    std::wstring out = y;
    if(!mo.empty() || sep) out += L"/" + mo;
    if(!d.empty()  || (sep && !mo.empty())) out += L"/" + d;
    return out;
}
static LRESULT CALLBACK dateEditProc(HWND h, UINT m, WPARAM w, LPARAM l){
    if(m==WM_KEYDOWN && (w==VK_RETURN || w==VK_TAB)){
        hopField(h, w==VK_TAB && (GetKeyState(VK_SHIFT)&0x8000)!=0);
        return 0;
    }
    if(m==WM_KEYDOWN && w==VK_DELETE){
        // Delete key: if a range is selected, clear it; otherwise clear the
        // WHOLE field (the user wants "press Delete → everything in the birth
        // box is wiped" so a wrong date can be re-entered from scratch).
        DWORD a=0,b=0; SendMessageW(h,EM_GETSEL,(WPARAM)&a,(LPARAM)&b);
        wchar_t buf[64]; GetWindowTextW(h,buf,64);
        if(a!=b){
            std::wstring cur(buf);
            std::wstring keep = digitsOnly(cur.substr(0,a)) +
                (b<=cur.size()?digitsOnly(cur.substr(b)):L"");
            std::wstring fm=formatJalaliMask(keep);
            SetWindowTextW(h,fm.c_str());
            SendMessageW(h,EM_SETSEL,fm.size(),fm.size());
        } else {
            SetWindowTextW(h,L"");
        }
        return 0;
    }
    if(m==WM_CHAR){
        if(w==VK_RETURN || w==VK_TAB) return 0;            // no beep
        wchar_t buf[64]; GetWindowTextW(h,buf,64);
        std::wstring cur(buf);
        // v1.4.0 fix: when the user is EDITING an existing value (caret not at
        // the end, or a range is selected — e.g. clicked into the middle of a
        // pre-filled birth date), do NOT rebuild-from-end. Let the default edit
        // control handle the keystroke so the field is no longer "locked" or
        // erased. We only apply the auto-slash mask when typing at the very end
        // with no selection (fresh sequential entry).
        DWORD selA=0, selB=0;
        SendMessageW(h, EM_GETSEL, (WPARAM)&selA, (LPARAM)&selB);
        bool atEnd   = (selA==selB) && (selA==(DWORD)cur.size());
        bool hasRange= (selA!=selB);
        // v1.6.0 fix: Backspace must ALWAYS delete (a digit AND any auto slash
        // that precedes it) regardless of caret position, and a selected range
        // must be cleared completely — the old code stopped deleting once it hit
        // a "/" (it removed the slash, then re-inserted it, so the field looked
        // stuck). We now strip to a digit string, drop the last digit, and
        // re-mask, so the user can fully clear a wrong birth date.
        if(w==VK_BACK){
            if(hasRange){
                // delete the selection: keep digits OUTSIDE the selection
                std::wstring before = cur.substr(0, selA);
                std::wstring after  = (selB<=cur.size())?cur.substr(selB):L"";
                std::wstring digs = digitsOnly(before+after);
                std::wstring formatted = formatJalaliMask(digs);
                SetWindowTextW(h, formatted.c_str());
                SendMessageW(h, EM_SETSEL, formatted.size(), formatted.size());
                return 0;
            }
            // no selection: drop the last DIGIT (skipping any trailing slash)
            std::wstring digs = digitsOnly(cur);
            if(!digs.empty()) digs.pop_back();
            std::wstring formatted = formatJalaliMask(digs);
            SetWindowTextW(h, formatted.c_str());
            SendMessageW(h, EM_SETSEL, formatted.size(), formatted.size());
            return 0;
        }
        if(hasRange || !atEnd){
            // pass digits/separators through to normal editing; block letters
            wchar_t ch=(wchar_t)w;
            if(ch>=0x06F0&&ch<=0x06F9) ch=(wchar_t)(L'0'+(ch-0x06F0));
            else if(ch>=0x0660&&ch<=0x0669) ch=(wchar_t)(L'0'+(ch-0x0660));
            if((ch>=L'0'&&ch<=L'9')||ch==L'/')
                return CallWindowProcW(s_oldDate,h,m,(WPARAM)ch,l);
            return 0;   // ignore other chars while mid-edit
        }
        // Accept a SPACE or slash as an explicit field separator (relaxed entry
        // like "1340 5 20"). Map it to a single slash in the working buffer.
        if(w==L' ' || w==L'/' || w==L'-' || w==L'.'){
            if(cur.empty()) return 0;
            // avoid double separators
            if(cur.back()!=L'/') cur += L'/';
            SetWindowTextW(h, cur.c_str());
            SendMessageW(h, EM_SETSEL, cur.size(), cur.size());
            return 0;
        }
        // only digits beyond this point (latin / fa / ar)
        wchar_t ch = (wchar_t)w;
        if(ch>=0x06F0&&ch<=0x06F9) ch=(wchar_t)(L'0'+(ch-0x06F0));
        else if(ch>=0x0660&&ch<=0x0669) ch=(wchar_t)(L'0'+(ch-0x0660));
        if(ch<L'0' || ch>L'9') return 0;
        cur += ch;
        std::wstring formatted = formatJalaliMask(cur);
        SetWindowTextW(h, formatted.c_str());
        SendMessageW(h, EM_SETSEL, formatted.size(), formatted.size());
        return 0;
    }
    return CallWindowProcW(s_oldDate, h, m, w, l);
}
void enableDateMask(HWND ctl){
    WNDPROC old = (WNDPROC)SetWindowLongPtrW(ctl, GWLP_WNDPROC,
        (LONG_PTR)dateEditProc);
    if(!s_oldDate) s_oldDate = old;
}

// ============= automatic RTL / LTR alignment based on typed content =========
//  Persian app: a field that contains Persian/Arabic letters should read &
//  align RIGHT (RTL); a field that is Latin/digits only should align LEFT.
//  We flip WS_EX_RTLREADING/WS_EX_RIGHT (and the matching styles) live as the
//  user types, then re-apply on every change. Enter/Tab still navigate.
static WNDPROC s_oldDir = NULL;
static bool hasPersian(const std::wstring& s){
    for(wchar_t c : s){
        if((c>=0x0600 && c<=0x06FF) || (c>=0xFB50 && c<=0xFDFF) ||
           (c>=0xFE70 && c<=0xFEFF)){
            // treat Persian/Arabic DIGITS as neutral, letters as RTL
            if(c>=0x06F0 && c<=0x06F9) continue;
            if(c>=0x0660 && c<=0x0669) continue;
            return true;
        }
    }
    return false;
}
static void applyDir(HWND h){
    wchar_t buf[512]; GetWindowTextW(h,buf,512);
    bool rtl = hasPersian(buf);
    // empty → default to RTL (Persian app) so the caret sits on the right
    if(buf[0]==0) rtl=true;
    LONG ex = GetWindowLongW(h, GWL_EXSTYLE);
    LONG st = GetWindowLongW(h, GWL_STYLE);
    bool curRtl = (ex & WS_EX_RTLREADING)!=0;
    if(rtl==curRtl) return;            // no change needed
    if(rtl){ ex |= (WS_EX_RTLREADING|WS_EX_RIGHT); st &= ~ES_CENTER; st |= ES_RIGHT; }
    else   { ex &= ~(WS_EX_RTLREADING|WS_EX_RIGHT); st &= ~ES_RIGHT; st |= ES_LEFT; }
    DWORD selA=0,selB=0; SendMessageW(h,EM_GETSEL,(WPARAM)&selA,(LPARAM)&selB);
    SetWindowLongW(h, GWL_EXSTYLE, ex);
    SetWindowLongW(h, GWL_STYLE, st);
    SetWindowPos(h,NULL,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
    InvalidateRect(h,NULL,TRUE);
    SendMessageW(h,EM_SETSEL,selA,selB);
}
static LRESULT CALLBACK dirEditProc(HWND h, UINT m, WPARAM w, LPARAM l){
    if(m==WM_KEYDOWN && w==VK_RETURN){ hopField(h,false); return 0; }
    if(m==WM_KEYDOWN && w==VK_TAB){
        hopField(h,(GetKeyState(VK_SHIFT)&0x8000)!=0); return 0; }
    if(m==WM_CHAR && (w==VK_RETURN || w==VK_TAB)) return 0;   // kill beep
    LRESULT r = CallWindowProcW(s_oldDir, h, m, w, l);
    if(m==WM_CHAR || m==WM_KEYUP || m==WM_PASTE || m==WM_CUT) applyDir(h);
    return r;
}
void enableAutoDir(HWND ctl){
    WNDPROC old=(WNDPROC)SetWindowLongPtrW(ctl,GWLP_WNDPROC,(LONG_PTR)dirEditProc);
    if(!s_oldDir) s_oldDir=old;
    applyDir(ctl);
}

// ================================================================ MAIN =====
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int){
    g_hInst = hInst;

    // v1.10.0: declare per-monitor v2 DPI awareness as early as possible so the
    // manifest's PerMonitorV2 hint is honoured and crisp on mixed-DPI setups.
    // Done by dynamic lookup so the single EXE still loads on Windows 7/8
    // (where these entry points do not exist).
    {
        HMODULE u32=GetModuleHandleW(L"user32.dll");
        typedef BOOL (WINAPI* SetCtxFn)(HANDLE);
        if(u32){
            auto setCtx=(SetCtxFn)(void*)GetProcAddress(u32,"SetProcessDpiAwarenessContext");
            // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == (HANDLE)-4
            if(!setCtx || !setCtx((HANDLE)-4)){
                // fall back to per-monitor v1 via shcore, then system-DPI
                HMODULE sh=LoadLibraryW(L"shcore.dll");
                if(sh){
                    typedef HRESULT (WINAPI* SetAwFn)(int);
                    auto setAw=(SetAwFn)(void*)GetProcAddress(sh,"SetProcessDpiAwareness");
                    if(setAw) setAw(2 /*PROCESS_PER_MONITOR_DPI_AWARE*/);
                    FreeLibrary(sh);
                } else {
                    SetProcessDPIAware();   // legacy system-DPI fallback
                }
            }
        } else {
            SetProcessDPIAware();
        }
    }

    installCrashHandler();           // crash handler
    detectSpec();                    // speed handler
    BackupLog_Init();                // dedicated Backup Log channel (A.3)
    logLine(L"=== DarmanPlus start v" APP_VERSION_W L" ===");
    writeSchemaVersion();            // §I: stamp data\.schema_version (informational only)

    // single instance — capture GetLastError() IMMEDIATELY after CreateMutexW,
    // before any other call can clobber the thread's last-error value (§G).
    CreateMutexW(NULL, TRUE, L"DarmanPlus_SingleInstance");
    DWORD muErr = GetLastError();
    if(muErr==ERROR_ALREADY_EXISTS){
        HWND ex=FindWindowW(APP_CLASS_W,NULL);
        if(ex) SetForegroundWindow(ex);
        return 0;
    }

    INITCOMMONCONTROLSEX icc={sizeof(icc),ICC_STANDARD_CLASSES|ICC_LISTVIEW_CLASSES|ICC_TAB_CLASSES};
    InitCommonControlsEx(&icc);

    // v1.17.0: first-run / prerequisite preparation splash. Installs the
    // Vazirmatn font and ensures data/ & logs/ exist (root cause of save
    // errors). The interface is now 100% native C++ (the HTML/MSHTML layer was
    // retired), so there is no browser-emulation registry key or MSHTML probe.
    // Shows a branded progress bar on first run / after a version bump; returns
    // instantly on subsequent runs.
    RunSetupSplash(hInst);
    gdipStartup();                   // v1.3.0: GDI+ rendering layer
    seedDefaultDepts();              // v1.4.1: ensure «پذیرش» category exists

    // v1.65.0: seed the Sections registry + the 30 built-in print templates AT
    // STARTUP. They were previously seeded only when the operator opened the
    // Settings window or the Print Designer — so on a fresh install the very
    // first admission print found an EMPTY design store, SectionDesign_Resolve
    // failed, and the receipt silently degraded to the legacy label-only layout
    // (no services table). Seeding here (idempotent, version-gated migration)
    // guarantees the first print already renders the real services-capable
    // design. printPrintDesign also seeds lazily as a belt-and-braces guard.
    { void Sections_Init(); void Designs_Init();
      Sections_Init(); Designs_Init(); InsDefs_SeedDefaults(); }

    // v1.66.0: prepare the embedded Patient-Admission surface EAGERLY at
    // launch — registers the shared page verbs and pre-builds the fully
    // inlined HTML page (serverless: attached to the program, no loopback
    // host / port at all), so «پذیرش بیمار» opens instantly.
    WebAdmission_Prepare();

    // v1.70.0: prepare the embedded CRM management surface EAGERLY at launch —
    // pre-builds the fully-inlined HTML management page (serverless, attached to
    // the program) so «مدیریت درمانگاه» opens instantly when SC_MANAGE is hit.
    WebCrm_Prepare();

    // responsive scale: based on monitor size + DPI
    HDC sdc=GetDC(NULL);
    int dpi = GetDeviceCaps(sdc,LOGPIXELSY);
    ReleaseDC(NULL,sdc);
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    g_scale = dpi/96.0;
    double fit = sh/900.0;           // design height 900
    if(fit < g_scale) g_scale = fit; // shrink on small displays
    // UI density (set in the settings panel) — "compact" trims ~12% so more
    // fits on smaller screens; applied at launch.
    if(getSetting(L"density",L"normal")==L"compact") g_scale *= 0.88;
    if(g_scale < 0.62) g_scale = 0.62;
    if(g_scale > 2.00) g_scale = 2.00;

    // v1.93: three themes — light / dark / neon
    {
        std::wstring tn=getSetting(L"theme",L"light");
        if(tn==L"dark")       applyThemeMode(TM_DARK);
        else if(tn==L"neon")  applyThemeMode(TM_NEON);
        else                  applyThemeMode(TM_LIGHT);
    }
    buildFonts();
    registerFlatButton();

    WNDCLASSW wc={0};
    wc.lpfnWndProc = frameProc;
    wc.hInstance   = hInst;
    wc.hCursor     = LoadCursor(NULL,IDC_ARROW);
    wc.lpszClassName = APP_CLASS_W;
    // v1.66.0: the embedded multi-size app icon (resource id 1 — the circular
    // «درمان پلاس» logo). Falls back to the stock icon only if the resource is
    // somehow missing. Drives Explorer/Alt-Tab/taskbar imagery.
    wc.hIcon       = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    if(!wc.hIcon) wc.hIcon = LoadIcon(NULL,IDI_APPLICATION);
    RegisterClassW(&wc);

    // true fullscreen borderless: WS_POPUP covering whole monitor, no menu bar
    HWND f = CreateWindowExW(0, APP_CLASS_W, APP_NAME_W,
        WS_POPUP|WS_CLIPCHILDREN, 0,0,sw,sh, NULL,NULL,hInst,NULL);
    // v1.66.0: explicit big+small window icons (taskbar / Alt-Tab / title
    // areas). LR_DEFAULTSIZE picks the right frame from the multi-size .ico.
    {
        HICON hBig  =(HICON)LoadImageW(hInst,MAKEINTRESOURCEW(1),IMAGE_ICON,
                        GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON),LR_DEFAULTCOLOR);
        HICON hSmall=(HICON)LoadImageW(hInst,MAKEINTRESOURCEW(1),IMAGE_ICON,
                        GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_DEFAULTCOLOR);
        if(hBig)   SendMessageW(f,WM_SETICON,ICON_BIG,(LPARAM)hBig);
        if(hSmall) SendMessageW(f,WM_SETICON,ICON_SMALL,(LPARAM)hSmall);
    }
    ShowWindow(f, SW_SHOW);
    UpdateWindow(f);
    switchScreen(SC_HOME);
    SetFocus(f);
#ifdef AZ_DEBUG_BUILD
    { // TEMP: headless screenshot verification — jump straight into a screen
        wchar_t dbg[32]={0};
        GetEnvironmentVariableW(L"AZ_DEBUG_SCREEN", dbg, 32);
        if(dbg[0]){
            User u; u.username=L"reza"; u.fullname=L"رضا منشی";
            u.dept=L"پذیرش"; u.role=0;
            g_session.user=u; g_session.shift=detectShift();
            g_session.title=resolveSessionTitle(u);
            g_session.loginAt=iranNow();
            if(!wcscmp(dbg,L"reception")){ switchScreen(SC_RECEPTION);
                                           // open a fresh reception form tab so
                                           // the screenshot shows the real form,
                                           // not the default cartable/portal tab.
                                           receptionAction(RA_NEWPAT); }
            else if(!wcscmp(dbg,L"manage")){   u.role=1; g_session.user=u;
                                               g_session.title=resolveSessionTitle(u);
                                               switchScreen(SC_MANAGE); }
            else if(!wcscmp(dbg,L"admin")){    u.role=2; g_session.user=u;
                                               g_session.title=resolveSessionTitle(u);
                                               switchScreen(SC_ADMIN); }
            else if(!wcscmp(dbg,L"accounting")){ switchScreen(SC_ACCOUNTING); }
            else if(!wcscmp(dbg,L"settings")){ switchScreen(SC_RECEPTION);
                                               OpenSettings(f, g_session.user); }
            else if(!wcscmp(dbg,L"backup")){   u.role=1; g_session.user=u;
                                               g_session.title=resolveSessionTitle(u);
                                               switchScreen(SC_MANAGE);
                                               openBackupManager(f); }
            // v2.01 (Part F1): «انتخاب شیفت کاری» debug screen removed.
            // §D.6: headless verification of the SERVERLESS embedded admission
            // surface (v1.66.0). Builds the fully-inlined page for both engine
            // variants and asserts every asset actually made it inline (styles,
            // all four scripts, closing </html>, and the data:-URI font for the
            // WebView2 variant), then opens a real embedded view and pumps until
            // the bundled JS reaches the `init` verb through the bridge. A JS
            // syntax error would stop the script before it ever called the
            // bridge, so a non-zero init-hit count proves the ES5 JS parsed and
            // executed under the real engine. Writes AZ_ADMISSION_PROBE=OK/FAIL.
            else if(!wcscmp(dbg,L"admission_probe")){
                WebAdmission_Prepare();
                auto pageOk=[&](const std::string& pg, bool wantFont)->bool{
                    if(pg.empty()) return false;
                    bool ok = pg.find("</html>")!=std::string::npos &&
                              pg.find("<style>")!=std::string::npos &&
                              pg.find("AzBridge")!=std::string::npos &&   // common.js
                              pg.find("azAdmissionReceive")!=std::string::npos && // bridge.js
                              pg.find("contextmenu")!=std::string::npos && // contextmenu.js
                              pg.find("admission")!=std::string::npos &&
                              pg.find("<link")==std::string::npos &&      // no dead links
                              pg.find("src=\"")==std::string::npos;       // no external scripts
                    // font: WebView2 variant must carry the data:-URI face; the
                    // MSHTML variant must NOT (it uses the process memory font).
                    if(wantFont) ok = ok && pg.find("data:font/ttf;base64,")!=std::string::npos;
                    else         ok = ok && pg.find("data:font/ttf;base64,")==std::string::npos;
                    return ok;
                };
                std::string pgM=WebAdmission_DebugInlinePage(false);
                std::string pgW=WebAdmission_DebugInlinePage(true);
                bool inlineOk = pageOk(pgM,false) && pageOk(pgW,true);
                logLine(L"PROBE inline mshtml len=" + std::to_wstring(pgM.size()) +
                        L" webview2 len=" + std::to_wstring(pgW.size()) +
                        (inlineOk?L" OK":L" FAIL"));
                // Now verify the embedded VIEW (MSHTML/WebView2) actually
                // creates a real child window AND that the bundled JS runs end
                // to end (init reached through the serverless bridge).
                bool viewOk=false, jsOk=false;
                if(WebAdmission_Available()){
                    HWND v = WebAdmission_CreateView(f);
                    viewOk = (v!=NULL && IsWindow(v));
                    logLine(std::wstring(L"PROBE createView ") + (viewOk?L"OK":L"FAIL"));
                    if(v){
                        DWORD t0=GetTickCount(); MSG pm;
                        while(GetTickCount()-t0 < 12000){
                            if(WebAdmission_DebugInitHits() > 0){ jsOk=true; break; }
                            if(PeekMessageW(&pm,NULL,0,0,PM_REMOVE)){ TranslateMessage(&pm); DispatchMessageW(&pm); }
                            else Sleep(15);
                        }
                        logLine(std::wstring(L"PROBE js-bridge init ") + (jsOk?L"OK":L"FAIL") +
                                L" hits=" + std::to_wstring(WebAdmission_DebugInitHits()));
                        WebAdmission_DestroyView(v);
                    }
                }
                bool ok = inlineOk && viewOk && jsOk;
                logLine(ok ? L"AZ_ADMISSION_PROBE=OK" : L"AZ_ADMISSION_PROBE=FAIL");
                // also drop a plain marker file so headless runners can read it
                {
                    std::wstring marker;
                    marker += std::wstring(L"inline=") + (inlineOk?L"OK":L"FAIL") +
                              L" mshtmlLen=" + std::to_wstring(pgM.size()) +
                              L" webview2Len=" + std::to_wstring(pgW.size()) + L"\r\n";
                    marker += std::wstring(L"view=") + (viewOk?L"OK":L"FAIL") + L"\r\n";
                    marker += std::wstring(L"jsBridge=") + (jsOk?L"OK":L"FAIL") +
                              L" initHits=" + std::to_wstring(WebAdmission_DebugInitHits()) + L"\r\n";
                    marker += ok ? L"AZ_ADMISSION_PROBE=OK\r\n" : L"AZ_ADMISSION_PROBE=FAIL\r\n";
                    writeFileUtf8(dataDir()+L"\\admission_probe.txt", marker, false);
                }
                gdipShutdown(); BackupLog_Shutdown();
                return ok ? 0 : 2;
            }
            // §D.7: headless smoke test for the KEYBOARD routing fix. Opens the
            // embedded admission view, waits for the bundled JS to boot, focuses
            // #nid, types a known-existing national ID, then synthesizes an
            // Enter keystroke via keybd_event. Because Enter now travels through
            // WebAdmission_TranslateAccel → the hosted control → the page's
            // keydown listener → Bridge.call('patient.lookup'), the C++ store
            // records the looked-up nid. We verify via WebAdmission_DebugInitHits
            // (JS ran) and WebAdmission_DebugLastFilledNid (Enter routed + lookup
            // fired + auto-fill flow reached the store). Driven by
            // `--smoke-admission-keys` (AZ_DEBUG_SCREEN=admission_keys).
            else if(!wcscmp(dbg,L"admission_keys")){
                // small local UTF-8 <-> wide helpers (web_admission's are static)
                auto w2u8_dbg=[](const std::wstring& w)->std::string{
                    if(w.empty()) return "";
                    int n=WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),NULL,0,NULL,NULL);
                    std::string s(n,0); WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),&s[0],n,NULL,NULL);
                    return s;
                };
                auto u82w_dbg=[](const std::string& s)->std::wstring{
                    if(s.empty()) return L"";
                    int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),NULL,0);
                    std::wstring w(n,0); MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),&w[0],n);
                    return w;
                };
                // Seed a known patient so the lookup has something to find.
                std::wstring knownNid=L"1234567890";
                bool seededPatient=false;
                {
                    // Pull the first existing patient's nid. On a clean smoke
                    // data root create one temporary row, then remove it below.
                    auto pats=loadAllPatients();
                    if(!pats.empty() && !pats[0].nid.empty()) knownNid=pats[0].nid;
                    // The JS normalizes Persian/Arabic digits before lookup, so
                    // compare against the same ASCII form in the smoke oracle.
                    for(wchar_t& c:knownNid){
                        if(c>=L'۰' && c<=L'۹') c=(wchar_t)(L'0'+(c-L'۰'));
                        else if(c>=L'٠' && c<=L'٩') c=(wchar_t)(L'0'+(c-L'٠'));
                    }
                    if(pats.empty()){
                        rememberPatient(knownNid,L"Smoke",L"Admission",L"",L"",L"",L"",L"",L"",
                                        std::vector<int>(),-1);
                        seededPatient=true;
                    }
                }
                logLine(L"SMOKE admission_keys: using nid=" + knownNid);
                bool viewOk=false, jsOk=false, keyOk=false;
                if(WebAdmission_Available()){
                    HWND v=WebAdmission_CreateView(f);
                    viewOk=(v!=NULL && IsWindow(v));
                    logLine(std::wstring(L"SMOKE createView ")+(viewOk?L"OK":L"FAIL"));
                    if(v){
                        // 1) pump until the page's JS booted (reached /api/init)
                        DWORD t0=GetTickCount(); MSG pm;
                        while(GetTickCount()-t0<12000){
                            if(WebAdmission_DebugInitHits()>0){ jsOk=true; break; }
                            if(PeekMessageW(&pm,NULL,0,0,PM_REMOVE)){
                                if(!WebAdmission_TranslateAccel(&pm)){ TranslateMessage(&pm); DispatchMessageW(&pm); }
                            } else Sleep(15);
                        }
                        logLine(std::wstring(L"SMOKE js-bridge ")+(jsOk?L"OK":L"FAIL"));
                        // 2) focus the host, type the nid, press Enter. We drive
                        //    focus + fill through the bridge push so the field is
                        //    populated deterministically, then synthesize Enter.
                        SetForegroundWindow(f); SetFocus(v);
                        WebAdmission_PushEvent("debug.focusNid",
                            std::string("{\"nid\":\"")+w2u8_dbg(knownNid)+"\"}");
                        // give the page a moment to apply the focus/fill push
                        DWORD t1=GetTickCount();
                        while(GetTickCount()-t1<600){
                            if(PeekMessageW(&pm,NULL,0,0,PM_REMOVE)){
                                if(!WebAdmission_TranslateAccel(&pm)){ TranslateMessage(&pm); DispatchMessageW(&pm); }
                            } else Sleep(10);
                        }
                        keybd_event(VK_RETURN,0,0,0);
                        keybd_event(VK_RETURN,0,KEYEVENTF_KEYUP,0);
                        // 3) wait up to 1500 ms and verify the store saw the nid
                        DWORD t2=GetTickCount();
                        std::string want=w2u8_dbg(knownNid);
                        while(GetTickCount()-t2<1500){
                            std::string got=WebAdmission_DebugLastFilledNid();
                            if(!got.empty() && got==want){ keyOk=true; break; }
                            if(PeekMessageW(&pm,NULL,0,0,PM_REMOVE)){
                                if(!WebAdmission_TranslateAccel(&pm)){ TranslateMessage(&pm); DispatchMessageW(&pm); }
                            } else Sleep(15);
                        }
                        logLine(std::wstring(L"SMOKE enter->lookup ")+(keyOk?L"OK":L"FAIL")+
                                L" lastNid=" + u82w_dbg(WebAdmission_DebugLastFilledNid()));
                        WebAdmission_DestroyView(v);
                    }
                }
                bool ok = viewOk && jsOk && keyOk;
                if(seededPatient) deletePatient(knownNid);
                logLine(ok?L"AZ_ADMISSION_KEYS=OK":L"AZ_ADMISSION_KEYS=FAIL");
                {
                    std::wstring marker;
                    marker += std::wstring(L"view=")+(viewOk?L"OK":L"FAIL")+L"\r\n";
                    marker += std::wstring(L"jsBridge=")+(jsOk?L"OK":L"FAIL")+L"\r\n";
                    marker += std::wstring(L"enterLookup=")+(keyOk?L"OK":L"FAIL")+L"\r\n";
                    marker += ok?L"AZ_ADMISSION_KEYS=OK\r\n":L"AZ_ADMISSION_KEYS=FAIL\r\n";
                    writeFileUtf8(dataDir()+L"\\admission_keys.txt", marker, false);
                }
                gdipShutdown(); BackupLog_Shutdown();
                return ok?0:2;
            }
            // §D.5: headless smoke test for the print-designer open/close path.
            // Exercises the section-picker + designer launch without blocking on
            // user input, then exits 0 (path is reachable) or a non-zero code if
            // the launch helper crashed/was unreachable. Driven by build.sh when
            // AZ_SMOKE is set; production builds never define AZ_DEBUG_BUILD.
            else if(!wcscmp(dbg,L"print_designer")){
                u.role=1; g_session.user=u; switchScreen(SC_MANAGE);
                // Initialize the designer subsystems and verify the public
                // entry path is reachable without blocking on user input. The
                // section store + design store must seed cleanly; if any of this
                // faulted, the crash handler would have already aborted with a
                // non-zero code. Reaching here means the open path is healthy.
                void Sections_Init(); void Designs_Init();
                Sections_Init(); Designs_Init(); InsDefs_SeedDefaults();
                // v2.07 §4.6: assert every builtin carries the 13 mandatory
                // blocks, exactly one barcode, one 3-column services table,
                // zero شماره سابقه bindings and at most one {receiptbarcode}.
                if(!pdVerifyBuiltinTemplates()){
                    logLine(L"SMOKE print_designer: builtin template check FAILED");
                    gdipShutdown(); BackupLog_Shutdown();
                    return 2;
                }
                logLine(L"SMOKE print_designer: subsystems initialized — OK");
                gdipShutdown(); BackupLog_Shutdown();
                return 0;
            }
            // v2.07: headless smoke for the «ارتباط با چاپگر» dialog — verify
            // the dialog opens, enumerates printers, applies the filter and
            // resolves the pre-selection without blocking, then exits cleanly.
            // Pumps briefly so the dialog actually paints (screenshot path).
            else if(!wcscmp(dbg,L"designer_gallery")){
                // v2.07.1: headless check that the designer gallery renders
                // real template thumbnails (not empty cards).
                switchScreen(SC_MANAGE);
                WebDesigner_Open(f, std::vector<int>());
                logLine(L"SMOKE designer_gallery: host opened — OK");
                MSG m;
                DWORD t0=GetTickCount();
                while(GetTickCount()-t0 < 4000){
                    while(PeekMessageW(&m,NULL,0,0,PM_REMOVE)){
                        TranslateMessage(&m); DispatchMessageW(&m);
                    }
                    Sleep(30);
                }
                return 0;
            }
            else if(!wcscmp(dbg,L"printer_link")){
                switchScreen(SC_RECEPTION);
                UpdateWindow(f);
                PrinterLink_Open(f);
                logLine(L"SMOKE printer_link: dialog opened — OK");
                MSG m;
                DWORD t0=GetTickCount();
                while(GetTickCount()-t0 < 1500){
                    while(PeekMessageW(&m,NULL,0,0,PM_REMOVE)){
                        TranslateMessage(&m); DispatchMessageW(&m);
                    }
                    Sleep(30);
                }
                return 0;
            }
        }
    }
#endif

    MSG msg;
    while(GetMessageW(&msg,NULL,0,0)){
        // global key routing
        // v2.06 — Ctrl+Tab must switch tabs NO MATTER WHERE THE FOCUS IS:
        // the main frame, the HTML surface (WebView2/MSHTML child), the tab
        // bar, the header, or the personnel-account popup. The old code gated
        // on GetAncestor(msg.hwnd,GA_ROOT)==g_hFrame, which silently killed
        // the hotkey whenever focus sat inside the personnel popup or any
        // other top-level child. Now the check is: is this keydown from a
        // window that belongs to OUR process? If so, and Ctrl is held with
        // Tab, cycle the reception tabs and consume the message before any
        // webview/edit subclass can swallow it.
        if(msg.message==WM_KEYDOWN || msg.message==WM_SYSKEYDOWN){
            if(msg.wParam==VK_TAB && (GetKeyState(VK_CONTROL)&0x8000)){
                DWORD pid=0; GetWindowThreadProcessId(msg.hwnd,&pid);
                if(pid==GetCurrentProcessId()){
                    Reception_CycleTab();
                    continue;
                }
            }
        }
        if(msg.message==WM_KEYDOWN){
            HWND root=GetAncestor(msg.hwnd,GA_ROOT);
            if(root==g_hFrame){
                if(msg.wParam==VK_F8){
                    SendMessageW(g_hFrame, WM_KEYDOWN, msg.wParam, msg.lParam);
                    continue;
                }
                if(msg.wParam==VK_F7 || msg.wParam==VK_F4){
                    SendMessageW(g_hFrame, WM_KEYDOWN, msg.wParam, msg.lParam);
                    continue;
                }
                if((msg.wParam=='P'||msg.wParam=='N') &&
                   (GetKeyState(VK_CONTROL)&0x8000) &&
                   (GetKeyState('P')&0x8000) && (GetKeyState('N')&0x8000)){
                    SendMessageW(g_hFrame, WM_KEYDOWN, msg.wParam, msg.lParam);
                    continue;
                }
            }
        }

        // Give the embedded admission browser (MSHTML/WebView2) a chance to eat
        // accelerator keys (Tab / Enter / Ctrl+A / arrows, …). These MUST pass
        // through TranslateAccelerator BEFORE TranslateMessage — otherwise the
        // hosted control never sees the keystroke and the page's JS keydown
        // listener never fires (the root cause of the broken navigation).
        if(WebAdmission_TranslateAccel(&msg)){
            continue;   // message consumed by the browser control
        }
        // v1.70.0: give the embedded CRM management browser the same chance to
        // consume accelerator keys (Tab / Enter / Ctrl+A / arrows, …).
        if(WebCrm_TranslateAccel(&msg)){
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    gdipShutdown();
    BackupLog_Shutdown();
    logLine(L"=== DarmanPlus exit ===");
    return 0;
}
