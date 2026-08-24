// ============================================================================
//  theme.cpp — light/dark themes, owner-drawn flat buttons, vector icons
// ============================================================================
#include "app.h"
#include <uxtheme.h>

Theme   g_theme;
bool    g_dark = false;
HBRUSH  g_brBg=0, g_brSurface=0, g_brSurface2=0, g_brInput=0;
//  v1.8.0: distinct non-red "attention" accent (violet) for change-requests.
COLORREF g_infoAccent  = RGB(124, 92, 230);
COLORREF g_infoAccent2 = RGB(98, 70, 210);

void applyTheme(bool dark){
    g_dark = dark;
    if(dark){
        // ---- True-black dark palette. The page background is (near) pure black
        //      as requested so card edges & button corners no longer glow white;
        //      surfaces step up in lightness only slightly so the UI has depth
        //      without bright halos. Labels/text are deliberately bright. ----
        g_theme.bg          = RGB(0, 0, 0);       // pure black page
        g_theme.bg2         = RGB(8, 10, 14);
        g_theme.surface     = RGB(20, 24, 30);    // card
        g_theme.surfaceTop  = RGB(28, 33, 41);    // card gradient top
        g_theme.surface2    = RGB(12, 14, 18);    // bars
        g_theme.border      = RGB(54, 62, 76);    // clearly visible separators
        g_theme.text        = RGB(238, 243, 249);
        g_theme.textDim     = RGB(170, 182, 198); // brighter dim text (legible)
        g_theme.labelInk    = RGB(205, 214, 226); // dark theme: light-gray labels
        g_theme.sectionInk  = RGB(238, 243, 249); // dark theme: bright titles
        g_theme.accent      = RGB(56, 170, 255);  // bright sky-blue
        g_theme.accent2     = RGB(30, 120, 220);  // gradient end
        g_theme.accentHover = RGB(96, 190, 255);
        g_theme.accentText  = RGB(255, 255, 255);
        g_theme.danger      = RGB(240, 100, 100);
        g_theme.dangerHover = RGB(250, 128, 128);
        g_theme.success     = RGB(74, 210, 148);
        g_theme.warn        = RGB(245, 184, 84);
        g_theme.inputBg     = RGB(26, 31, 39);    // distinctly lighter than card
        g_theme.inputText   = RGB(240, 245, 250);
        g_theme.hover       = RGB(34, 40, 50);
        g_theme.headerTop   = RGB(14, 17, 22);
        g_theme.headerBot   = RGB(6, 8, 11);
        g_infoAccent  = RGB(150, 120, 245);   // bright violet (stands out, not red)
        g_infoAccent2 = RGB(120, 92, 225);
    } else {
        // ---- Premium light palette (v1.10.0 §C): a genuinely LAYERED light
        //      theme, not a washed-out white sheet. Five clearly distinct
        //      elevation tones — page → wells/bars → cards → card-top highlight
        //      → header — give real depth. Borders are crisp, text is high
        //      contrast (near-WCAG-AAA on white), the accent is a richer, more
        //      saturated indigo→sky, and hover/focus/active/disabled states are
        //      visually separated so every control reads clearly.
        //
        //  Elevation ladder (lightness):
        //      bg2  214 ── page bottom (deepest)
        //      bg   222 ── page
        //      surface2 234 ── wells / bars / list backgrounds
        //      border 196 ── crisp hairline between layers
        //      surface 255 ── cards (clean white, pops off the tinted page)
        //      surfaceTop 250 ── soft top-light on cards
        // ---- v1.19.0: premium "DarmanPlus 2026" light palette — matches the
        //  reference reception design exactly.
        //  bg #F5F8FD · surface #FFFFFF · surface2 #EAF4FF · border #DCE6F2
        //  text #233042 · muted #6B7A90 · accent #1976F3 · hover #2D8CFF
        //  success #16C47F · warning #F4B740 · danger #E24C4B.
        // v1.86.0: premium light "CLINIC GLASS" — NOT a flat white sheet.
        // Page is a soft blue-grey clay (#E6EEF8 → #DDE8F5) so white glass cards
        // extrude with depth. Header is PURE WHITE frosted glass (#FFFFFF → #EEF3FB)
        // — never dark. Borders are crisp clay edges (#C8D8EB). Accent is refined
        // indigo #4B63E6 that matches the embedded admission page.
        // The goal: modern, layered, readable, NEVER white-on-white washed out.
        g_theme.bg          = RGB(0xE6, 0xEE, 0xF8); // #E6EEF8 soft clay page top
        g_theme.bg2         = RGB(0xD9, 0xE3, 0xF2); // #D9E3F2 page gradient bottom
        g_theme.surface     = RGB(0xFF, 0xFF, 0xFF); // #FFFFFF cards / sheets
        g_theme.surfaceTop  = RGB(0xFE, 0xFE, 0xFF); // #FEFEFF card top-light
        g_theme.surface2    = RGB(0xEE, 0xF3, 0xFB); // #EEF3FB wells / bars / action-bar
        g_theme.border      = RGB(0xC8, 0xD8, 0xEB); // #C8D8EB soft clay edge, visible
        g_theme.text        = RGB(0x18, 0x26, 0x3D); // #18263D primary ink
        g_theme.textDim     = RGB(0x61, 0x6E, 0x83); // #616E83 muted
        g_theme.labelInk    = RGB(0x32, 0x3E, 0x52); // readable labels
        g_theme.sectionInk  = RGB(0x16, 0x22, 0x35); // strong titles
        g_theme.accent      = RGB(0x4B, 0x63, 0xE6); // #4B63E6 refined indigo
        g_theme.accent2     = RGB(0x6B, 0x83, 0xFF); // #6B83FF gradient end
        g_theme.accentHover = RGB(0x63, 0x7A, 0xF5); // hover
        g_theme.accentText  = RGB(0xFF, 0xFF, 0xFF);
        g_theme.danger      = RGB(0xE0, 0x53, 0x52); // #E05352
        g_theme.dangerHover = RGB(0xEC, 0x6C, 0x6B);
        g_theme.success     = RGB(0x15, 0xB5, 0x81); // #15B581
        g_theme.warn        = RGB(0xF0, 0xB4, 0x3C); // #F0B43C
        g_theme.inputBg     = RGB(0xF1, 0xF5, 0xFB); // #F1F5FB tinted well
        g_theme.inputText   = RGB(0x18, 0x26, 0x3D);
        g_theme.hover       = RGB(0xE7, 0xED, 0xFA); // soft accent wash
        // v1.86: HEADER MUST BE LIGHT — pure white → #ECF2FA frosted band
        g_theme.headerTop   = RGB(0xFF, 0xFF, 0xFF); // #FFFFFF
        g_theme.headerBot   = RGB(0xEC, 0xF2, 0xFA); // #ECF2FA
        g_infoAccent  = RGB(0x7C, 0x56, 0xE4);    // violet (distinct, non-red)
        g_infoAccent2 = RGB(0x5E, 0x42, 0xD0);
        // v1.77: the calm/warm eye-comfort palettes were RETIRED — the app now
        // ships exactly two themes, light (روشن) and dark (مشکی). The former
        // theme.palette setting is no longer consulted here, so any stale calm/
        // warm value silently falls back to the standard medical-white light
        // theme (and applyThemeByName() rewrites it to "blue" on the next pick).
    }
    if(g_brBg)       DeleteObject(g_brBg);
    if(g_brSurface)  DeleteObject(g_brSurface);
    if(g_brSurface2) DeleteObject(g_brSurface2);
    if(g_brInput)    DeleteObject(g_brInput);
    g_brBg       = CreateSolidBrush(g_theme.bg);
    g_brSurface  = CreateSolidBrush(g_theme.surface);
    g_brSurface2 = CreateSolidBrush(g_theme.surface2);
    g_brInput    = CreateSolidBrush(g_theme.inputBg);
    setSetting(L"theme", dark ? L"dark" : L"light");
}
static BOOL CALLBACK invProc(HWND h, LPARAM){
    SendMessageW(h, WM_APP_THEME, 0, 0);     // let controls re-color themselves
    InvalidateRect(h, NULL, TRUE);
    EnumChildWindows(h, invProc, 0);
    return TRUE;
}
static BOOL CALLBACK topProc(HWND h, LPARAM){
    // also refresh OUR other top-level windows (calculator, detached tabs)
    DWORD pid=0; GetWindowThreadProcessId(h,&pid);
    if(pid==GetCurrentProcessId()){
        SendMessageW(h, WM_APP_THEME, 0, 0);
        InvalidateRect(h,NULL,TRUE);
        EnumChildWindows(h, invProc, 0);
    }
    return TRUE;
}
void broadcastThemeChange(){
    // v1.63.0: the welcome artwork composite is cached per (size, theme, scrim);
    // a theme flip changes the scrim, so drop the cache BEFORE anyone repaints
    // or the first frame after the switch would blit the previous theme's wash.
    gpFreeBackgroundCache();
    EnumWindows(topProc, 0);
}

// =================================================================== draw ==
void fillRoundRect(HDC dc, RECT rc, int rad, COLORREF fill, COLORREF border){
    // CLR_INVALID means outline-only. Treating it as a COLORREF produces white
    // (0x00FFFFFF), which used to overwrite button bodies and dark medallions.
    HBRUSH br = (fill==CLR_INVALID) ? (HBRUSH)GetStockObject(NULL_BRUSH)
                                    : CreateSolidBrush(fill);
    HPEN   pn = (border==CLR_INVALID) ? (HPEN)GetStockObject(NULL_PEN)
                                      : CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(dc, br), op = SelectObject(dc, pn);
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, rad, rad);
    SelectObject(dc, ob); SelectObject(dc, op);
    if(fill!=CLR_INVALID) DeleteObject(br);
    if(border!=CLR_INVALID) DeleteObject(pn);
}

//  Simple vector icons drawn with pens — crisp at any DPI, zero assets.
void drawIcon(HDC dc, int icon, RECT rc, COLORREF col, int thick){
    int cx=(rc.left+rc.right)/2, cy=(rc.top+rc.bottom)/2;
    int r = ((rc.right-rc.left) < (rc.bottom-rc.top) ? (rc.right-rc.left)
                                                     : (rc.bottom-rc.top)) / 2;
    // v1.9.0: a GEOMETRIC pen with ROUND end-caps + ROUND joins renders every
    // line-art glyph with smooth, professional terminations (no hard "childish"
    // corners). Falls back to a plain cosmetic pen if creation fails.
    LOGBRUSH lb={ BS_SOLID, col, 0 };
    HPEN pen = ExtCreatePen(PS_GEOMETRIC|PS_SOLID|PS_ENDCAP_ROUND|PS_JOIN_ROUND,
                            thick<1?1:thick, &lb, 0, NULL);
    if(!pen) pen = CreatePen(PS_SOLID, thick, col);
    HGDIOBJ op = SelectObject(dc, pen);
    HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    int oldBk = SetBkMode(dc, TRANSPARENT);
    switch(icon){
    case ICO_X: {
        int d=(r*60)/100;
        MoveToEx(dc,cx-d,cy-d,0); LineTo(dc,cx+d+1,cy+d+1);
        MoveToEx(dc,cx+d,cy-d,0); LineTo(dc,cx-d-1,cy+d+1);
        break; }
    case ICO_CALC: {
        int w=(r*70)/100, h=(r*88)/100;
        Rectangle(dc,cx-w,cy-h,cx+w,cy+h);
        MoveToEx(dc,cx-w,cy-h/3,0); LineTo(dc,cx+w,cy-h/3);
        int s=(w*45)/100;
        MoveToEx(dc,cx-s,cy+h/4,0); LineTo(dc,cx-s,cy+h/4);
        SetPixel(dc,cx-s,cy+h/4,col); SetPixel(dc,cx,cy+h/4,col); SetPixel(dc,cx+s,cy+h/4,col);
        SetPixel(dc,cx-s,cy+h/2+2,col); SetPixel(dc,cx,cy+h/2+2,col); SetPixel(dc,cx+s,cy+h/2+2,col);
        Ellipse(dc,cx-s-1,cy+h/4-1,cx-s+2,cy+h/4+2); Ellipse(dc,cx-1,cy+h/4-1,cx+2,cy+h/4+2);
        Ellipse(dc,cx+s-1,cy+h/4-1,cx+s+2,cy+h/4+2);
        break; }
    case ICO_PRINT: {
        int w=(r*75)/100;
        Rectangle(dc,cx-w,cy-r/4,cx+w,cy+r/2);                 // body
        Rectangle(dc,cx-w/2,cy-r+2,cx+w/2,cy-r/4);             // paper top
        Rectangle(dc,cx-w/2,cy+r/6,cx+w/2,cy+r-1);             // paper out
        break; }
    case ICO_UPDATE: {
        Arc(dc,cx-r+2,cy-r+2,cx+r-2,cy+r-2, cx+r,cy-r, cx-r,cy+r);
        POINT a={cx+(r*55)/100, cy-(r*78)/100};
        MoveToEx(dc,a.x-r/3,a.y,0); LineTo(dc,a.x+1,a.y+1);
        MoveToEx(dc,a.x,a.y-r/3,0); LineTo(dc,a.x+1,a.y+1);
        break; }
    case ICO_MOON: {
        Arc(dc,cx-r+2,cy-r+2,cx+r-2,cy+r-2, cx,cy-r, cx,cy+r);
        Arc(dc,cx-r/2,cy-r+2,cx+r,cy+r-2, cx,cy+r, cx,cy-r);
        break; }
    case ICO_SUN: {
        Ellipse(dc,cx-r/2,cy-r/2,cx+r/2,cy+r/2);
        for(int i=0;i<8;i++){
            double a=i*3.14159/4;
            int x1=cx+(int)((r*65/100)*cos(a)+0.5), y1=cy+(int)((r*65/100)*sin(a)+0.5);
            int x2=cx+(int)((r*95/100)*cos(a)+0.5), y2=cy+(int)((r*95/100)*sin(a)+0.5);
            MoveToEx(dc,x1,y1,0); LineTo(dc,x2,y2);
        }
        break; }
    case ICO_USER: {
        Ellipse(dc,cx-r/2,cy-r+1,cx+r/2,cy);                   // head
        Arc(dc,cx-r+1,cy+1,cx+r-1,cy+2*r, cx+r,cy+r, cx-r,cy+r); // shoulders
        break; }
    case ICO_SHIELD: {
        POINT p[6]={{cx,cy-r+1},{cx+r-2,cy-r/2},{cx+r-2,cy+r/4},
                    {cx,cy+r-1},{cx-r+2,cy+r/4},{cx-r+2,cy-r/2}};
        Polygon(dc,p,6);
        MoveToEx(dc,cx-r/3,cy,0); LineTo(dc,cx-r/8,cy+r/4); LineTo(dc,cx+r/3,cy-r/4);
        break; }
    case ICO_PLUS: {
        int d=(r*65)/100;
        MoveToEx(dc,cx-d,cy,0); LineTo(dc,cx+d+1,cy);
        MoveToEx(dc,cx,cy-d,0); LineTo(dc,cx,cy+d+1);
        break; }
    case ICO_LOGOUT: {
        Rectangle(dc,cx-r+2,cy-r+3,cx+r/4,cy+r-2);
        MoveToEx(dc,cx-r/4,cy,0); LineTo(dc,cx+r-1,cy);
        MoveToEx(dc,cx+r/2,cy-r/3,0); LineTo(dc,cx+r-1,cy);
        LineTo(dc,cx+r/2,cy+r/3);
        break; }
    case ICO_DETACH: {
        Rectangle(dc,cx-r+2,cy-r/4,cx+r/4,cy+r-2);
        MoveToEx(dc,cx-r/4,cy-r+2,0); LineTo(dc,cx+r-2,cy-r+2);
        LineTo(dc,cx+r-2,cy+r/4);
        break; }
    case ICO_CROSS_MED: {
        int a=(r*35)/100, b=(r*85)/100;
        POINT p[12]={{cx-a,cy-b},{cx+a,cy-b},{cx+a,cy-a},{cx+b,cy-a},
                     {cx+b,cy+a},{cx+a,cy+a},{cx+a,cy+b},{cx-a,cy+b},
                     {cx-a,cy+a},{cx-b,cy+a},{cx-b,cy-a},{cx-a,cy-a}};
        Polygon(dc,p,12);
        break; }
    case ICO_CHECK: {
        MoveToEx(dc,cx-r+3,cy,0); LineTo(dc,cx-r/4,cy+r/2); LineTo(dc,cx+r-2,cy-r/2);
        break; }
    case ICO_TRASH: {
        Rectangle(dc,cx-r/2,cy-r/3,cx+r/2,cy+r-2);
        MoveToEx(dc,cx-r+3,cy-r/3,0); LineTo(dc,cx+r-3,cy-r/3);
        MoveToEx(dc,cx-r/4,cy-r/3,0); LineTo(dc,cx-r/4,cy-(r*60)/100);
        LineTo(dc,cx+r/4,cy-(r*60)/100); LineTo(dc,cx+r/4,cy-r/3);
        break; }
    case ICO_SAVE: {
        Rectangle(dc,cx-r+2,cy-r+2,cx+r-2,cy+r-2);
        Rectangle(dc,cx-r/2,cy-r+2,cx+r/2,cy-r/4);
        Rectangle(dc,cx-r/2,cy+r/6,cx+r/2,cy+r-2);
        break; }
    case ICO_BACK: {
        MoveToEx(dc,cx-r+3,cy,0); LineTo(dc,cx+r-2,cy);
        MoveToEx(dc,cx-r/4,cy-r/2,0); LineTo(dc,cx-r+3,cy); LineTo(dc,cx-r/4,cy+r/2);
        break; }
    case ICO_ID: {   // ID card
        Rectangle(dc,cx-r+1,cy-r/2,cx+r-1,cy+r/2);
        Ellipse(dc,cx-r/2,cy-r/4,cx-r/8,cy+r/8);             // photo head
        MoveToEx(dc,cx,cy-r/5,0);  LineTo(dc,cx+r-r/3,cy-r/5);
        MoveToEx(dc,cx,cy+r/8,0);  LineTo(dc,cx+r-r/3,cy+r/8);
        break; }
    case ICO_PHONE: {  // phone handset
        int a=(r*70)/100;
        MoveToEx(dc,cx-a,cy-a,0);
        LineTo(dc,cx-a/3,cy-a/3); LineTo(dc,cx,cy);
        LineTo(dc,cx+a/3,cy+a/3); LineTo(dc,cx+a,cy+a);
        MoveToEx(dc,cx-a,cy-a,0); LineTo(dc,cx-a/2,cy-a-2);
        MoveToEx(dc,cx+a,cy+a,0); LineTo(dc,cx+a+2,cy+a/2);
        break; }
    case ICO_CAL: {   // calendar
        Rectangle(dc,cx-r+1,cy-r+3,cx+r-1,cy+r-1);
        MoveToEx(dc,cx-r+1,cy-r/3,0); LineTo(dc,cx+r-1,cy-r/3);
        MoveToEx(dc,cx-r/2,cy-r+3,0); LineTo(dc,cx-r/2,cy-r-1);
        MoveToEx(dc,cx+r/2,cy-r+3,0); LineTo(dc,cx+r/2,cy-r-1);
        break; }
    case ICO_PIN: {   // location pin
        Ellipse(dc,cx-r/2,cy-r+1,cx+r/2,cy);
        MoveToEx(dc,cx-r/2,cy-r/3,0); LineTo(dc,cx,cy+r-1); LineTo(dc,cx+r/2,cy-r/3);
        SetPixel(dc,cx,cy-r/2,col);
        break; }
    case ICO_RECEIPT: {  // receipt / invoice
        int w=(r*65)/100;
        MoveToEx(dc,cx-w,cy-r+2,0); LineTo(dc,cx+w,cy-r+2);
        LineTo(dc,cx+w,cy+r-2); LineTo(dc,cx+w-w/2,cy+r-r/3);
        LineTo(dc,cx,cy+r-2); LineTo(dc,cx-w+w/2,cy+r-r/3);
        LineTo(dc,cx-w,cy+r-2); LineTo(dc,cx-w,cy-r+2);
        MoveToEx(dc,cx-w/2,cy-r/3,0); LineTo(dc,cx+w/2,cy-r/3);
        MoveToEx(dc,cx-w/2,cy+r/8,0); LineTo(dc,cx+w/2,cy+r/8);
        break; }
    case ICO_CLOCK: {
        Ellipse(dc,cx-r+1,cy-r+1,cx+r-1,cy+r-1);
        MoveToEx(dc,cx,cy,0); LineTo(dc,cx,cy-r/2);
        MoveToEx(dc,cx,cy,0); LineTo(dc,cx+r/3,cy);
        break; }
    case ICO_REFRESH: {
        Arc(dc,cx-r+2,cy-r+2,cx+r-2,cy+r-2, cx-r,cy, cx,cy-r);
        Arc(dc,cx-r+2,cy-r+2,cx+r-2,cy+r-2, cx+r,cy, cx,cy+r);
        POINT a={cx, cy-r+1};
        MoveToEx(dc,a.x-r/3,a.y,0); LineTo(dc,a.x,a.y); LineTo(dc,a.x,a.y+r/3);
        break; }
    case ICO_GEAR: {
        // A clear, recognisable cog drawn as an OUTLINE (no hole-punch needed):
        //   • a toothed outer ring (single closed polygon with 8 teeth)
        //   • a small centre circle hole.
        // Drawn with the current pen so it inherits icon colour & thickness and
        // blends onto any background.
        const int N=8;
        double rOut=r*0.96, rIn=r*0.66;
        POINT poly[N*4]; int n=0;
        double tw=0.22;   // tooth angular half width (fraction of the gap)
        double half=(3.14159265/N);
        for(int i=0;i<N;i++){
            double a=i*2*3.14159265/N;
            double aA=a-half*(1.0-tw), aB=a-half*tw;
            double aC=a+half*tw,       aD=a+half*(1.0-tw);
            poly[n].x=cx+(int)(rIn *cos(aA)+0.5); poly[n].y=cy+(int)(rIn *sin(aA)+0.5); n++;
            poly[n].x=cx+(int)(rOut*cos(aB)+0.5); poly[n].y=cy+(int)(rOut*sin(aB)+0.5); n++;
            poly[n].x=cx+(int)(rOut*cos(aC)+0.5); poly[n].y=cy+(int)(rOut*sin(aC)+0.5); n++;
            poly[n].x=cx+(int)(rIn *cos(aD)+0.5); poly[n].y=cy+(int)(rIn *sin(aD)+0.5); n++;
        }
        Polygon(dc,poly,n);                       // toothed ring (outline)
        int rh=(r*34)/100;
        Ellipse(dc,cx-rh,cy-rh,cx+rh,cy+rh);      // centre hole
        break; }
    case ICO_BELL: {
        Arc(dc,cx-r+2,cy-r+2,cx+r-2,cy+r,  cx-r+2,cy+r/3, cx+r-2,cy+r/3);
        MoveToEx(dc,cx-r+2,cy+r/3,0); LineTo(dc,cx-r+2,cy-r/3);
        MoveToEx(dc,cx+r-2,cy+r/3,0); LineTo(dc,cx+r-2,cy-r/3);
        Arc(dc,cx-r+2,cy-r,cx+r-2,cy+r/2, cx+r-2,cy-r/3, cx-r+2,cy-r/3);
        MoveToEx(dc,cx-r/2,cy+r/3,0); LineTo(dc,cx+r/2,cy+r/3);
        MoveToEx(dc,cx-r/5,cy+r/2,0); LineTo(dc,cx+r/5,cy+r/2);
        break; }
    case ICO_TAB: {
        MoveToEx(dc,cx-r+2,cy+r-2,0);
        LineTo(dc,cx-r+2,cy-r/3); LineTo(dc,cx-r/4,cy-r/3);
        LineTo(dc,cx,cy-r+2); LineTo(dc,cx+r/4,cy-r/3);
        LineTo(dc,cx+r-2,cy-r/3); LineTo(dc,cx+r-2,cy+r-2);
        break; }
    case ICO_CHEVRON: {
        MoveToEx(dc,cx-r/2,cy-r/2,0); LineTo(dc,cx+r/3,cy); LineTo(dc,cx-r/2,cy+r/2);
        break; }
    case ICO_SAVED_MSG: {   // §F: bookmark / ribbon glyph
        int w=(r*60)/100, top=cy-r+2, bot=cy+r-2;
        MoveToEx(dc,cx-w,top,0);
        LineTo(dc,cx+w,top); LineTo(dc,cx+w,bot);
        LineTo(dc,cx,bot-r/3);                 // notch up
        LineTo(dc,cx-w,bot); LineTo(dc,cx-w,top);
        break; }
    case ICO_PALETTE: {     // §A: theme / palette (a swatch ring + dot)
        Ellipse(dc,cx-r+1,cy-r+1,cx+r-1,cy+r-1);
        int d=(r*22)/100;
        Ellipse(dc,cx-r/2-d,cy-r/3-d,cx-r/2+d,cy-r/3+d);
        Ellipse(dc,cx+r/3-d,cy-r/2-d,cx+r/3+d,cy-r/2+d);
        Ellipse(dc,cx+r/2-d,cy+r/4-d,cx+r/2+d,cy+r/4+d);
        break; }
    case ICO_INFO: {        // §A: about / info (circle + i)
        Ellipse(dc,cx-r+1,cy-r+1,cx+r-1,cy+r-1);
        SetPixel(dc,cx,cy-r/2,col);
        MoveToEx(dc,cx,cy-r/5,0); LineTo(dc,cx,cy+r/2);
        break; }
    case ICO_PEOPLE: {      // §A/§G: two-person group glyph
        int rr=(r*30)/100;
        Ellipse(dc,cx-r/2-rr,cy-r/3-rr,cx-r/2+rr,cy-r/3+rr);   // head 1
        Arc(dc,cx-r,cy,cx,cy+r, cx,cy, cx-r,cy);               // body 1
        Ellipse(dc,cx+r/3-rr,cy-r/4-rr,cx+r/3+rr,cy-r/4+rr);   // head 2
        Arc(dc,cx,cy+r/8,cx+r,cy+r, cx+r,cy+r/8, cx,cy+r/8);   // body 2
        break; }
    case ICO_WALLET: {      // v1.19.0: wallet / billfold glyph (rounded body +
        // a flap with a button stud — reads clearly at 18-28px sizes).
        int w=(r*86)/100, hh=(r*62)/100;
        // rounded wallet body
        RoundRect(dc,cx-w,cy-hh,cx+w,cy+hh, r/2, r/2);
        // flap on the right edge (RTL-neutral: a small pocket on one side)
        int fx=cx+w-(r*52)/100;
        MoveToEx(dc,fx,cy-hh,0); LineTo(dc,fx,cy+hh);
        // button stud on the flap
        int sr=(r*16)/100; if(sr<1) sr=1;
        Ellipse(dc,fx+(r*22)/100-sr,cy-sr,fx+(r*22)/100+sr,cy+sr);
        break; }
    case ICO_LETTER: {     // v1.77.0: پاکت نامه — closed envelope / letter
        int w=(r*88)/100, h=(r*62)/100;
        Rectangle(dc,cx-w,cy-h,cx+w,cy+h);        // envelope body
        MoveToEx(dc,cx-w,cy-h,0);                  // flap: top-left → centre → top-right
        LineTo(dc,cx,cy+h/3);
        LineTo(dc,cx+w,cy-h);
        break; }
    }
    SetBkMode(dc, oldBk);
    SelectObject(dc, op); SelectObject(dc, ob);
    DeleteObject(pen);
}

// ============================================================ flat button ==
// v1.18.3 THEME-TOGGLE BUG FIX: previously BtnData::bg cached an ABSOLUTE
// COLORREF (e.g. the value of g_theme.surface at button-creation time). When
// the theme toggled (dark→light), applyTheme() changed g_theme.surface but the
// button still held the OLD raw colour — so the rounded-corner background stayed
// the previous theme's colour (the "black behind buttons" bug). The fix: store a
// SEMANTIC TOKEN identifying WHICH theme colour the button sits on, and resolve
// it to the LIVE g_theme value at paint time, so a theme switch is always
// reflected immediately with no per-button refresh bookkeeping.
enum BtnBgToken {
    BBG_PARENT = 0,   // ask parent (CLR_INVALID behaviour)
    BBG_BG,           // g_theme.bg
    BBG_BG2,          // g_theme.bg2
    BBG_SURFACE,      // g_theme.surface
    BBG_SURFACE2,     // g_theme.surface2
    BBG_HEADERTOP,    // g_theme.headerTop
    BBG_HEADERMID,    // midpoint of the header gradient (headerTop→headerBot)
    BBG_EXPLICIT      // a literal colour stored in `bg` (theme-independent)
};
struct BtnData {
    std::wstring text, sub;
    int icon, style;
    bool hover, down;
    COLORREF bg;     // literal colour (only used when bgToken==BBG_EXPLICIT)
    int      bgToken;// which live theme colour to paint behind the corners
    int imgIcon;     // RCDATA id of a raster icon (0 = use vector `icon`)
    // v1.78.0: optional per-button accent (0 = theme accent). Lets twin cards
    // on the welcome screen carry distinct brand hues (reception blue /
    // management violet) instead of one shared accent.
    COLORREF accentOv;
};
// resolve the live background colour for a button from its semantic token.
static COLORREF btnBgColor(const BtnData* d){
    if(!d) return CLR_INVALID;
    switch(d->bgToken){
        case BBG_BG:        return g_theme.bg;
        case BBG_BG2:       return g_theme.bg2;
        case BBG_SURFACE:   return g_theme.surface;
        case BBG_SURFACE2:  return g_theme.surface2;
        case BBG_HEADERTOP: return g_theme.headerTop;
        case BBG_HEADERMID: return blendColor(g_theme.headerTop, g_theme.headerBot, 50);
        case BBG_EXPLICIT:  return d->bg;
        case BBG_PARENT: default: return CLR_INVALID;
    }
}
// v1.78.0: a button's brand accent — the per-button override (BS_CARD brand
// hues), else the live theme accent.
static COLORREF btnAccent(const BtnData* d){
    return (d && d->accentOv) ? d->accentOv : g_theme.accent;
}
static LRESULT CALLBACK btnProc(HWND h, UINT m, WPARAM w, LPARAM l){
    BtnData* d = (BtnData*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch(m){
    case WM_NCCREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)l;
        d = new BtnData();
        d->icon  = LOWORD((UINT_PTR)cs->lpCreateParams);
        d->style = HIWORD((UINT_PTR)cs->lpCreateParams);
        d->hover = d->down = false;
        d->bg    = CLR_INVALID;
        d->bgToken = BBG_PARENT;
        d->imgIcon = 0;
        d->accentOv = 0;
        if(cs->lpszName) d->text = cs->lpszName;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
        return TRUE; }
    case WM_NCDESTROY: delete d; break;
    case WM_SIZE: {
        // v1.78.0: GHOST bar buttons (header gear / calculator / exit) get a
        // real ROUNDED WINDOW REGION. Until now their corners were faked by
        // filling the square client rect with a static "header midpoint"
        // colour — but the header is a vertical gradient, so a sharp-cornered
        // patch stayed visible behind the rounded button. Clipping the window
        // to the button's own silhouette makes the corners truly transparent:
        // the parent's live gradient shows through at every Y, on every theme.
        if(d && d->style==BS_GHOST){
            int bw=LOWORD(l), bh2=HIWORD(l);
            if(bw>0 && bh2>0){
                int rad=bh2/3;
                if(rad>S(14)) rad=S(14);
                if(rad<S(6))  rad=S(6);
                HRGN rgn=CreateRoundRectRgn(0,0,bw+1,bh2+1,rad*2+1,rad*2+1);
                SetWindowRgn(h,rgn,TRUE);   // the system owns the region now
            }
        }
        break; }
    case WM_SETTEXT:
        if(d){ d->text = (const wchar_t*)l; InvalidateRect(h,NULL,TRUE); }
        return TRUE;
    case WM_MOUSEMOVE:
        if(d && !d->hover){
            d->hover = true; InvalidateRect(h,NULL,TRUE);
            TRACKMOUSEEVENT t={sizeof(t),TME_LEAVE,h,0}; TrackMouseEvent(&t);
        }
        break;
    case WM_MOUSELEAVE:
        if(d){ d->hover=false; d->down=false; InvalidateRect(h,NULL,TRUE); }
        break;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case WM_ENABLE:
        InvalidateRect(h,NULL,TRUE);   // §C: repaint focus ring / disabled state
        break;
    case WM_LBUTTONDOWN:
        if(d){ d->down=true; InvalidateRect(h,NULL,TRUE); SetCapture(h); }
        break;
    case WM_LBUTTONUP: {
        if(d && d->down){
            d->down=false; InvalidateRect(h,NULL,TRUE); ReleaseCapture();
            RECT rc; GetClientRect(h,&rc);
            POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
            if(PtInRect(&rc,pt)){
                // v1.1.0: POST (not send) the click. Some handlers destroy
                // this very button (close-tab, switch-screen, theme-toggle);
                // with SendMessage we'd return into freed window state.
                PostMessageW(GetParent(h), WM_COMMAND,
                    MAKEWPARAM(GetDlgCtrlID(h), BN_CLICKED), (LPARAM)h);
                return 0;
            }
        }
        break; }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0 = BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        if(rc.right<=0 || rc.bottom<=0){ EndPaint(h,&ps); return 0; }
        // Double-buffer when GDI can allocate the backing objects. On old or
        // resource-constrained systems allocation may fail; painting directly
        // to the window DC still produces a complete, opaque, legible button.
        HDC mem = CreateCompatibleDC(dc0);
        HBITMAP bmp = mem ? CreateCompatibleBitmap(dc0, rc.right, rc.bottom) : NULL;
        HGDIOBJ obm = NULL;
        HDC dc = dc0;
        bool buffered = false;
        if(mem && bmp){
            obm = SelectObject(mem, bmp);
            if(obm && obm!=HGDI_ERROR){ dc=mem; buffered=true; }
        }
        if(!buffered){
            if(bmp) DeleteObject(bmp);
            if(mem) DeleteDC(mem);
            bmp=NULL; mem=NULL; obm=NULL;
        }

        // Background behind the rounded corners. v1.4.0: when the host told us
        // the exact colour it sits on (header gradient, surface2 bar, card,…)
        // we paint THAT solid colour so the antialiased corners blend perfectly
        // — this is the definitive fix for the "white corners in dark mode" bug.
        COLORREF liveBg = btnBgColor(d);   // resolves the LIVE theme colour
        if(d && liveBg!=CLR_INVALID){
            HBRUSH eb=CreateSolidBrush(liveBg);
            FillRect(dc,&rc,eb); DeleteObject(eb);
        } else {
            HWND par = GetParent(h);
            HBRUSH pb = (HBRUSH)SendMessageW(par, WM_CTLCOLORSTATIC,(WPARAM)dc,(LPARAM)h);
            if(!pb) pb = g_brBg;
            FillRect(dc,&rc,pb);
        }

        COLORREF fill, txt, bord = CLR_INVALID;
        int st = d ? d->style : BS_GHOST;
        bool enabled = IsWindowEnabled(h)!=FALSE;
        bool hv = enabled && d && d->hover, dn = enabled && d && d->down;
        bool focused = enabled && (GetFocus()==h);   // §C: explicit focus ring
        // v1.64.0: relative-luminance contrast guard. If a style's text colour
        // is too close to its fill colour (the white-on-white regression), the
        // ink is forced to the opposite extreme so every label stays legible.
        auto lum = [](COLORREF c){ int r=GetRValue(c),g=GetGValue(c),b=GetBValue(c);
            return (2126*r+7152*g+722*b)/10000; };
        auto readable = [&](COLORREF bg, COLORREF ink){
            int lb=lum(bg), li=lum(ink);
            int diff = lb>li ? lb-li : li-lb;
            if(diff >= 90) return ink;                 // already legible
            return lb>140 ? RGB(0x1F,0x29,0x37) : RGB(255,255,255);
        };
        switch(st){
        case BS_PRIMARY:
            fill = dn ? g_theme.accent : hv ? g_theme.accentHover : g_theme.accent;
            txt  = readable(fill, g_theme.accentText); break;
        case BS_DANGER:
            fill = dn||hv ? g_theme.dangerHover : g_theme.danger;
            txt  = readable(fill, RGB(255,255,255)); break;
        case BS_INFO:
            fill = dn||hv ? g_infoAccent2 : g_infoAccent;
            txt  = readable(fill, RGB(255,255,255)); break;
        case BS_OUTLINE:
            fill = hv ? g_theme.hover : g_theme.surface;
            txt  = readable(fill, g_theme.text); bord = g_theme.border; break;
        case BS_CARD: {
            // v1.78.0: per-button accent override (0 → theme accent).
            // v1.79.0: resting brand edge a touch stronger (26→34 blend) so the
            // card reads as *branded* even before hover.
            COLORREF cardAcc = btnAccent(d);
            fill = hv ? blendColor(g_theme.hover, cardAcc, 10) : g_theme.surface;
            txt  = readable(fill, g_theme.text);
            bord = hv ? cardAcc : blendColor(g_theme.border, cardAcc, 34); break; }
        default: // ghost
            // v1.79.0: header tool buttons rest as frosted translucent tiles
            // (soft white plate + whisper hairline painted below) instead of
            // blending invisibly into the band — they read as deliberate
            // controls now, not smudges. Hover keeps the accent wash.
            fill = hv ? g_theme.hover
                      : blendColor(g_theme.surface, RGB(255,255,255), g_dark?0:62);
            txt  = readable(fill, hv ? g_theme.text : g_theme.textDim);
            if(d && d->icon==ICO_X && hv){ fill=g_theme.danger; txt=RGB(255,255,255); }
            break;
        }
        RECT rr = rc;
        if(dn){ rr.top+=1; rr.bottom+=1; }
        // ------------------------------------------------------------------
        //  v1.63.0 BUTTON REDESIGN (solid + quiet styles).
        //  1.62.0 modernised only BS_CARD; every other style was still the
        //  1.3.0 look: a flat two-stop gradient, no elevation, no sheen, a
        //  hard 10 px radius at every size and no visible pressed state.
        //  The refresh below gives the whole set one coherent language:
        //    * radius scales with the control height (pill-ish on short bars,
        //      soft-rounded on tall ones) instead of a fixed 10 px,
        //    * solid styles (PRIMARY/DANGER/INFO) get a tinted drop shadow
        //      that GROWS on hover and COLLAPSES on press, so the button
        //      physically lifts and depresses,
        //    * a 45 %-height top sheen + a darker bottom rim reads as a real
        //      moulded surface rather than a printed rectangle,
        //    * quiet styles (OUTLINE/GHOST) get a hairline that warms toward
        //      the accent on hover plus a faint inner wash, so they respond
        //      without shouting.
        // ------------------------------------------------------------------
        // v1.86 Claymorphism: chunky, rounded, extruded with soft shadows
        int hgt = rr.bottom-rr.top; if(hgt<1) hgt=1;
        int rad = S(st==BS_CARD?30:12);
        if(st!=BS_CARD){
            rad = hgt/3;
            if(rad > S(16)) rad = S(16);
            if(rad < S(7))  rad = S(7);
        }
        // v1.86 Claymorphic solid button: concave illusion via gradient
        // (darker top → lighter bottom for pressed-in feel at rest, lighter top
        // for pop-out; sheen strip + inner white highlight + bottom rim)
        auto solidBody = [&](COLORREF top, COLORREF bot, COLORREF glow){
            if(!dn){
                gpShadowColor(dc, rr, rad, hv?S(14):S(9), hv?120:72, glow);
            }
            // True concave: darker saturated top, lighter luminous bottom
            COLORREF cTop, cBot;
            if(dn){
                cTop = bot; cBot = top;
            } else {
                cTop = blendColor(top, RGB(0,0,0), 10);
                cBot = blendColor(bot, RGB(255,255,255), 14);
            }
            fillRoundRect(dc, rr, rad*2, cBot, CLR_INVALID);
            gpGradRoundRect(dc, rr, rad, cTop, cBot, CLR_INVALID);
            // top sheen 45% white wash
            RECT sh = rr; sh.bottom = rr.top + (hgt*44)/100;
            if(sh.bottom > sh.top+1)
                gpFillAlpha(dc, sh, rad, RGB(255,255,255), dn?10:28);
            // inner white rim for clay highlight
            RECT inner = rr; InflateRect(&inner, -S(1), -S(1));
            if(inner.right>inner.left && inner.bottom>inner.top)
                gpRoundRect(dc, inner, rad>S(1)?rad-S(1):rad, CLR_INVALID,
                            blendColor(RGB(255,255,255), cBot, 22), 130);
            // bottom rim
            gpRoundRect(dc, rr, rad, CLR_INVALID,
                blendColor(bot, RGB(0,0,0), dn?24:14));
        };
        // v1.3.0: anti-aliased GDI+ fills with a soft gradient on solid styles
        if(st==BS_PRIMARY){
            COLORREF a = dn ? g_theme.accent2 : (hv?g_theme.accentHover:g_theme.accent);
            COLORREF b = dn ? g_theme.accent  : (hv?g_theme.accent:g_theme.accent2);
            solidBody(a, b, g_theme.accent);
        } else if(st==BS_DANGER){
            COLORREF a = dn ? g_theme.danger : (hv?g_theme.dangerHover:g_theme.danger);
            COLORREF b = dn ? g_theme.dangerHover
                            : blendColor(g_theme.danger, RGB(0,0,0), 18);
            solidBody(a, b, g_theme.danger);
        } else if(st==BS_INFO){
            COLORREF a = dn ? g_infoAccent2 : (hv?g_infoAccent2:g_infoAccent);
            COLORREF b = dn ? g_infoAccent
                            : blendColor(g_infoAccent2, RGB(0,0,0), 14);
            solidBody(a, b, g_infoAccent);
        } else if(st==BS_OUTLINE){
            // quiet, bordered. hover warms the hairline toward the accent and
            // lays down a faint accent wash; press flattens both.
            COLORREF bd = hv ? blendColor(g_theme.border, g_theme.accent, dn?70:48)
                             : g_theme.border;
            COLORREF f  = dn ? g_theme.hover
                             : (hv ? blendColor(g_theme.surface, g_theme.accent, 8)
                                   : g_theme.surface);
            if(hv && !dn) gpShadow(dc, rr, rad, S(5), 40);
            // v1.66.0: unconditional GDI base before GDI+ decoration
            fillRoundRect(dc, rr, rad*2, f, CLR_INVALID);
            gpGradRoundRect(dc, rr, rad,
                blendColor(g_theme.surfaceTop, f, 45), f, bd);
        } else if(st==BS_CARD){
            // v1.78.0 CARD refresh — per-card brand accent (accentOv), tinted
            // elevation and a livelier hover: the body warms toward the accent,
            // the border saturates, and the icon badge flips to a solid brand
            // medallion with a white glyph (see the content block below).
            COLORREF cardAcc  = btnAccent(d);
            // elevation — tinted with the card's own hue so it reads as
            // coloured light, not grey dirt.
            //  v1.85: deeper, softer resting lift for a claymorphic extrusion.
            gpShadowColor(dc, rr, rad, hv?S(18):S(12), hv?110:68, cardAcc);
            // v1.66.0: unconditional GDI base before GDI+ decoration
            fillRoundRect(dc, rr, rad*2,
                hv?blendColor(g_theme.hover,cardAcc,10):g_theme.surface, CLR_INVALID);
            gpGradRoundRect(dc, rr, rad,
                hv?blendColor(g_theme.surfaceTop,cardAcc,10):g_theme.surfaceTop,
                hv?blendColor(g_theme.hover,cardAcc,14):g_theme.surface, bord);
            // v1.77: soft top sheen — a white wash over the upper ~40% so the
            // card has a subtle highlight (lit edge on the dark theme, gentle
            // sheen on the light theme) instead of a flat sheet.
            {
                RECT csh = rr; csh.bottom = rr.top + (hgt*40)/100;
                if(csh.bottom > csh.top+1)
                    gpFillAlpha(dc, csh, rad, RGB(255,255,255), hv?24:18);
            }
            // Inner Highlight (white outline with alpha 140) for Claymorphism
            {
                RECT inner = rr; InflateRect(&inner, -S(2), -S(2));
                gpRoundRect(dc, inner, rad>S(2)?rad-S(2):rad, CLR_INVALID, RGB(255,255,255), 140);
            }
            if(hv){
                // accent glow ring just outside the border (drawn under text)
                RECT halo=rr; InflateRect(&halo,S(1),S(1));
                gpRoundRect(dc, halo, rad+S(1), CLR_INVALID, cardAcc);
            }
        } else {
            // BS_GHOST — the borderless bar/toolbar button. At rest it is a
            // bare tinted plate; hover raises a soft tinted pill with a barely
            // visible hairline so it reads as a target without adding chrome;
            // press sinks it to the flat hover colour with no highlight.
            bool danger = (d && d->icon==ICO_X && hv);
            COLORREF acc = danger ? g_theme.danger : g_theme.accent;
            if(hv && !dn){
                // v1.78.0: the soft tinted shadow was dropped — the ghost button
                // now has a real rounded window region (see WM_SIZE), so any
                // shadow ring would be hard-clipped at the silhouette. A clean
                // top-lit pill + accent hairline reads better and never shows
                // a clipped halo.
                fillRoundRect(dc, rr, rad*2, fill, CLR_INVALID);
                gpGradRoundRect(dc, rr, rad,
                    blendColor(fill, RGB(255,255,255), danger?0:16), fill,
                    blendColor(fill, acc, danger?0:34));
            } else if(dn){
                // v1.66.0: unconditional GDI base before GDI+ decoration
                fillRoundRect(dc, rr, rad*2, fill, CLR_INVALID);
                gpRoundRect(dc, rr, rad, fill, blendColor(fill, acc, 22));
            } else {
                // v1.66.0: unconditional GDI base before GDI+ decoration
                fillRoundRect(dc, rr, rad*2, fill, CLR_INVALID);
                gpRoundRect(dc, rr, rad, fill, bord);
            }
        }
        // §C: explicit focus ring — a crisp accent hairline inset 2px so keyboard
        //     focus is always visible without shifting layout.
        if(focused){
            RECT fr=rr; InflateRect(&fr,-S(2),-S(2));
            gpRoundRect(dc, fr, rad>S(3)?rad-S(2):rad, CLR_INVALID, g_theme.accent);
        }

        SetBkMode(dc, TRANSPARENT);
        // §C: disabled controls use a body-relative ink. A fixed dim token can
        // disappear on solid success/accent overrides in old themes.
        if(!enabled) txt = readable(fill, blendColor(txt, fill, 45));
        SetTextColor(dc, txt);
        if(st==BS_CARD){
            // v1.62.0 CARD REDESIGN. The old card pinned its badge/title/sub to
            // absolute offsets (16 / 78 / 112) so any card that was not exactly
            // 170 px tall printed its caption in the wrong place — and on the
            // welcome screen the caption sometimes fell outside the card. The
            // content block is now CENTRED in the available height and every
            // metric derives from the card's own size, so it looks correct at
            // any dimension and any DPI.
            int cw = rc.right, chh = rc.bottom;
            int bd = S(58);                       // badge diameter
            if(bd > chh/3) bd = chh/3;
            if(bd < S(30)) bd = S(30);
            int tH = S(30), sH = S(22);
            bool hasSub = (d && !d->sub.empty());
            int gap1 = S(14), gap2 = S(2);
            int blockH = bd + gap1 + tH + (hasSub? gap2+sH : 0);
            int by = rr.top + (chh - blockH)/2;
            if(by < rr.top + S(10)) by = rr.top + S(10);

            if(d && d->icon){
                COLORREF cardAcc  = btnAccent(d);
                COLORREF cardAccH = d->accentOv ? blendColor(d->accentOv, RGB(255,255,255), 24)
                                                : g_theme.accentHover;
                RECT br={cw/2-bd/2, by, cw/2+bd/2, by+bd};
                // the glyph box is identical on both branches — only its ink
                // (white on the hover medallion / brand accent at rest) differs.
                int isz=(bd*52)/100;
                RECT ir={cw/2-isz/2, br.top+(bd-isz)/2,
                         cw/2+isz/2, br.top+(bd-isz)/2+isz};
                if(hv){
                    // hovering flips the badge to a SOLID brand medallion with a
                    // white glyph + a soft halo — the card reads fully alive.
                    RECT halo=br; InflateRect(&halo,S(5),S(5));
                    gpFillAlpha(dc, halo, (bd+S(10))/2,
                        blendColor(cardAcc, g_theme.surface, 40), 70);
                    fillRoundRect(dc, br, bd, cardAcc, CLR_INVALID);
                    gpGradRoundRect(dc, br, bd/2, cardAccH, cardAcc, CLR_INVALID);
                    drawIcon(dc, d->icon, ir, RGB(255,255,255), S(2)+1);
                } else {
                    // v1.79.0: a soft resting glow behind the badge (was 34 →
                    // now 50 alpha) — the entry cards feel lit even at rest.
                    COLORREF badgeBg = blendColor(g_theme.surface, cardAcc, 24);
                    gpShadowColor(dc, br, bd/2, S(5), 50, cardAcc);
                    fillRoundRect(dc, br, bd, badgeBg, CLR_INVALID);
                    gpGradRoundRect(dc, br, bd/2,
                        blendColor(g_theme.surfaceTop, badgeBg, 55), badgeBg,
                        blendColor(g_theme.border,cardAcc,38));
                    drawIcon(dc, d->icon, ir, cardAcc, S(2)+1);
                }
            }
            int ty = by + bd + gap1;
            SelectObject(dc, g_fTitle);
            SetTextColor(dc, hv?btnAccent(d):txt);
            RECT tr = {S(8), ty, cw-S(8), ty+tH};
            DrawTextW(dc, d?d->text.c_str():L"", -1, &tr,
                DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            if(hasSub){
                SelectObject(dc, g_fSmall);
                SetTextColor(dc, g_theme.textDim);
                RECT sr = {S(10), ty+tH+gap2, cw-S(10), ty+tH+gap2+sH};
                DrawTextW(dc, d->sub.c_str(), -1, &sr,
                    DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            }
            // a subtle "enter" chevron at the bottom edge, revealed on hover
            if(hv){
                int chv=S(11), cyv=rr.bottom-S(16);
                RECT cr={cw/2-chv/2, cyv-chv/2, cw/2+chv/2, cyv+chv/2};
                drawIcon(dc, ICO_CHEVRON, cr, btnAccent(d), S(2));
            }
        } else {
            bool hasText = d && !d->text.empty();
            bool hasImg  = d && d->imgIcon;
            int isz = S(17);
            int iszImg = S(22);   // raster icons read better a touch larger
            if(hasImg && hasText){
                // raster icon flush-right, text to its left (RTL feel)
                RECT ir={rc.right-S(12)-iszImg, rc.bottom/2-iszImg/2,
                         rc.right-S(12), rc.bottom/2+iszImg/2};
                if(!gpDrawTintedImageRes(dc, d->imgIcon, ir, txt))
                    drawIcon(dc, d->icon, ir, txt, S(2));
                SelectObject(dc, g_fUI);
                RECT tr={S(10),0,rc.right-S(18)-iszImg,rc.bottom};
                DrawTextW(dc, d->text.c_str(), -1, &tr,
                    DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            } else if(hasImg){
                RECT ir={rc.right/2-iszImg/2, rc.bottom/2-iszImg/2,
                         rc.right/2+iszImg/2, rc.bottom/2+iszImg/2};
                if(!gpDrawTintedImageRes(dc, d->imgIcon, ir, txt))
                    drawIcon(dc, d->icon, ir, txt, S(2));
            } else if(d && d->icon && hasText){
                // icon right, text left of it (RTL feel)
                RECT ir={rc.right-S(12)-isz, rc.bottom/2-isz/2,
                         rc.right-S(12), rc.bottom/2+isz/2};
                drawIcon(dc, d->icon, ir, txt, S(2));
                SelectObject(dc, g_fUI);
                RECT tr={S(10),0,rc.right-S(16)-isz,rc.bottom};
                DrawTextW(dc, d->text.c_str(), -1, &tr,
                    DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            } else if(d && d->icon){
                RECT ir={rc.right/2-isz/2, rc.bottom/2-isz/2,
                         rc.right/2+isz/2, rc.bottom/2+isz/2};
                drawIcon(dc, d->icon, ir, txt, S(2));
            } else if(hasText){
                SelectObject(dc, g_fUI);
                DrawTextW(dc, d->text.c_str(), -1, &rc,
                    DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            }
        }
        // Keep disabled buttons opaque and legible. An alpha veil applied after
        // text could turn both body and ink white on failing/older GDI+ paths.
        if(!enabled){
            RECT fr=rr; InflateRect(&fr,-S(1),-S(1));
            gpRoundRect(dc,fr,rad>S(2)?rad-S(1):rad,CLR_INVALID,
                        blendColor(fill,g_theme.border,55));
        }
        if(buffered){
            BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
            SelectObject(mem,obm); DeleteObject(bmp); DeleteDC(mem);
        }
        EndPaint(h,&ps);
        return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}
void registerFlatButton(){
    WNDCLASSW wc={0};
    wc.lpfnWndProc = btnProc;
    wc.hInstance   = g_hInst;
    wc.hCursor     = LoadCursor(NULL, IDC_HAND);
    wc.lpszClassName = L"AzFlatBtn";
    RegisterClassW(&wc);
}
HWND createFlatButton(HWND parent,int id,const wchar_t* text,int icon,int style,
                      int x,int y,int w,int h,const wchar_t* sub){
    HWND b = CreateWindowExW(0, L"AzFlatBtn", text, WS_CHILD|WS_VISIBLE,
        x,y,w,h, parent,(HMENU)(UINT_PTR)id, g_hInst,
        (LPVOID)(UINT_PTR)MAKELONG(icon,style));
    if(sub && b){
        BtnData* d=(BtnData*)GetWindowLongPtrW(b,GWLP_USERDATA);
        if(d) d->sub = sub;
    }
    return b;
}
//  v1.1.0: change a flat button's icon in place (e.g. moon↔sun on theme
//  toggle) instead of destroying & recreating it mid-click.
void setFlatButtonIcon(HWND btn, int icon){
    if(!btn || !IsWindow(btn)) return;
    BtnData* d=(BtnData*)GetWindowLongPtrW(btn,GWLP_USERDATA);
    if(d){ d->icon = icon; InvalidateRect(btn,NULL,TRUE); }
}
void setFlatButtonBg(HWND btn, COLORREF bg){
    if(!btn || !IsWindow(btn)) return;
    BtnData* d=(BtnData*)GetWindowLongPtrW(btn,GWLP_USERDATA);
    if(!d) return;
    // v1.18.3: map the caller's colour to a SEMANTIC theme token where it
    // matches a live g_theme colour, so a later theme toggle re-resolves it
    // automatically (fixes the "black behind buttons after dark→light" bug).
    // Any colour that is not a recognised theme slot is kept as an explicit
    // literal (theme-independent, e.g. a one-off brand colour).
    if(bg==CLR_INVALID)              d->bgToken = BBG_PARENT;
    else if(bg==g_theme.bg)          d->bgToken = BBG_BG;
    else if(bg==g_theme.bg2)         d->bgToken = BBG_BG2;
    else if(bg==g_theme.surface)     d->bgToken = BBG_SURFACE;
    else if(bg==g_theme.surface2)    d->bgToken = BBG_SURFACE2;
    else if(bg==g_theme.headerTop)   d->bgToken = BBG_HEADERTOP;
    else { d->bgToken = BBG_EXPLICIT; d->bg = bg; }
    InvalidateRect(btn,NULL,TRUE);
}
// v1.77: pin a button to the MIDPOINT of the header gradient so its rounded
// corners blend with the gradient at the button's vertical centre — the fix for
// the "white square behind the header gear/calculator icons" on the light theme
// (the corners were painted headerTop=white while the header there is a
// headerTop→headerBot blend). Resolved live from g_theme so a theme switch
// updates it automatically, with no per-button refresh bookkeeping.
void setFlatButtonHeaderMid(HWND btn){
    if(!btn || !IsWindow(btn)) return;
    BtnData* d=(BtnData*)GetWindowLongPtrW(btn,GWLP_USERDATA);
    if(!d) return;
    d->bgToken = BBG_HEADERMID;
    InvalidateRect(btn,NULL,TRUE);
}
//  v1.78.0: give a BS_CARD button its own brand accent (border, badge, halo,
//  hover title). Pass 0 to fall back to the theme accent. Used by the welcome
//  screen's twin entry cards (reception blue / management violet).
void setFlatButtonAccent(HWND btn, COLORREF accent){
    if(!btn || !IsWindow(btn)) return;
    BtnData* d=(BtnData*)GetWindowLongPtrW(btn,GWLP_USERDATA);
    if(d){ d->accentOv = accent; InvalidateRect(btn,NULL,TRUE); }
}
//  v1.4.1: give a flat button a real raster icon (RCDATA id). Pass 0 to clear
//  and fall back to the vector icon.
void setFlatButtonImage(HWND btn, int resId){
    if(!btn || !IsWindow(btn)) return;
    BtnData* d=(BtnData*)GetWindowLongPtrW(btn,GWLP_USERDATA);
    if(d){ d->imgIcon = resId; InvalidateRect(btn,NULL,TRUE); }
}

// ============================================================================
//  Themed owner-draw combobox (v1.6.0)
//  CBS_DROPDOWNLIST combos painted their dropdown LIST with the system colours
//  (white bg / black text) which is unreadable in dark mode. Creating the combo
//  with CBS_OWNERDRAWFIXED|CBS_HASSTRINGS and forwarding WM_DRAWITEM here paints
//  every row with the theme palette and RTL-aligns Persian text.
// ============================================================================
// v1.8.0: combo boxes must have NO visible system edge. Subclass each combo
// and paint its closed face after the default pass; this also hides Wine's
// fixed white dropdown-button rectangle while preserving normal combo behavior.
static const wchar_t* const THEMED_COMBO_OLDPROC_PROP=L"AzThemedComboOldProc";
static bool comboHasPersian(const wchar_t* s);
static LRESULT CALLBACK themedComboProc(HWND h, UINT m, WPARAM w, LPARAM l){
    WNDPROC old=(WNDPROC)GetPropW(h,THEMED_COMBO_OLDPROC_PROP);
    if(!old) return DefWindowProcW(h,m,w,l);
    if(m==WM_NCDESTROY){
        RemovePropW(h,THEMED_COMBO_OLDPROC_PROP);
        return CallWindowProcW(old,h,m,w,l);
    }
    if(m==WM_APP_THEME){
        InvalidateRect(h,NULL,TRUE);
        return 0;
    }
    LRESULT r=CallWindowProcW(old,h,m,w,l);
    if(m==WM_PAINT || m==WM_NCPAINT){
        HDC dc=GetWindowDC(h);
        if(dc){
            RECT wr; GetWindowRect(h,&wr);
            RECT rc={0,0,wr.right-wr.left,wr.bottom-wr.top};
            HBRUSH bg=CreateSolidBrush(g_theme.inputBg);
            FillRect(dc,&rc,bg);
            DeleteObject(bg);

            wchar_t text[256]={0};
            int sel=(int)SendMessageW(h,CB_GETCURSEL,0,0);
            if(sel>=0) SendMessageW(h,CB_GETLBTEXT,sel,(LPARAM)text);
            SetBkMode(dc,TRANSPARENT);
            SetTextColor(dc,g_theme.inputText);
            HGDIOBJ oldFont=SelectObject(dc,g_fUI);
            RECT tr=rc; tr.left+=S(28); tr.right-=S(8);
            UINT flags=DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX;
            if(comboHasPersian(text)) flags|=DT_RIGHT|DT_RTLREADING;
            else flags|=DT_LEFT;
            DrawTextW(dc,text,-1,&tr,flags);
            SelectObject(dc,oldFont);

            int cx=rc.left+S(14), cy=(rc.top+rc.bottom)/2;
            POINT tri[3]={{cx-S(4),cy-S(2)},{cx+S(4),cy-S(2)},{cx,cy+S(3)}};
            HBRUSH ab=CreateSolidBrush(g_theme.textDim);
            HPEN ap=CreatePen(PS_SOLID,1,g_theme.textDim);
            HGDIOBJ ob=SelectObject(dc,ab), op=SelectObject(dc,ap);
            Polygon(dc,tri,3);
            SelectObject(dc,ob); SelectObject(dc,op);
            DeleteObject(ab); DeleteObject(ap);

            HBRUSH border=CreateSolidBrush(g_theme.border);
            FrameRect(dc,&rc,border);
            DeleteObject(border);
            ReleaseDC(h,dc);
        }
    }
    return r;
}
HWND createThemedCombo(HWND parent, int id){
    HWND c = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|
        CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS,
        0,0,10,10, parent,(HMENU)(UINT_PTR)id, g_hInst,0);
    SendMessageW(c,WM_SETFONT,(WPARAM)g_fUI,TRUE);
    WNDPROC old=(WNDPROC)SetWindowLongPtrW(c,GWLP_WNDPROC,(LONG_PTR)themedComboProc);
    SetPropW(c,THEMED_COMBO_OLDPROC_PROP,(HANDLE)old);
    return c;
}
static bool comboHasPersian(const wchar_t* s){
    for(const wchar_t* p=s; *p; ++p){
        wchar_t c=*p;
        if(c>=0x06F0&&c<=0x06F9) continue;
        if(c>=0x0660&&c<=0x0669) continue;
        if((c>=0x0600&&c<=0x06FF)||(c>=0xFB50&&c<=0xFDFF)||(c>=0xFE70&&c<=0xFEFF))
            return true;
    }
    return false;
}
//  Call from the parent's WM_DRAWITEM. Returns true if it handled a combo item.
bool drawThemedComboItem(LPDRAWITEMSTRUCT dis){
    if(!dis) return false;
    if(dis->CtlType!=ODT_COMBOBOX) return false;
    HDC dc=dis->hDC;
    RECT rc=dis->rcItem;
    bool selected = (dis->itemState & ODS_SELECTED)!=0;
    COLORREF bg = selected ? g_theme.accent : g_theme.inputBg;
    COLORREF fg = selected ? g_theme.accentText : g_theme.inputText;
    HBRUSH br=CreateSolidBrush(bg); FillRect(dc,&rc,br); DeleteObject(br);
    // itemID == -1 is the COLLAPSED SELECTION FIELD (the always-visible part of
    // the combo). We must paint its text too, otherwise the chosen value would
    // be invisible. Pull it from the current selection.
    wchar_t buf[256]={0};
    int item=(int)dis->itemID;
    if(item<0) item=(int)SendMessageW(dis->hwndItem,CB_GETCURSEL,0,0);
    if(item>=0){
        SendMessageW(dis->hwndItem,CB_GETLBTEXT,item,(LPARAM)buf);
        SetBkMode(dc,TRANSPARENT);
        SetTextColor(dc,fg);
        HGDIOBJ of=SelectObject(dc,g_fUI);
        // leave room on the LEFT for the dropdown arrow when this is the
        // collapsed field (RTL: arrow sits on the left, text on the right).
        RECT tr=rc; tr.right-=6; tr.left+=((int)dis->itemID<0?S(22):6);
        UINT flags=DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX;
        if(comboHasPersian(buf)) flags|=DT_RIGHT|DT_RTLREADING; else flags|=DT_LEFT;
        DrawTextW(dc,buf,-1,&tr,flags);
        SelectObject(dc,of);
    }
    // v1.7.0: draw a flat, theme-coloured dropdown arrow on the collapsed
    // field so dark mode no longer shows the system's thick white arrow box.
    if((int)dis->itemID<0){
        int cx=rc.left+S(12), cy=(rc.top+rc.bottom)/2;
        POINT tri[3]={ {cx-S(5),cy-S(2)}, {cx+S(5),cy-S(2)}, {cx,cy+S(4)} };
        HBRUSH ab=CreateSolidBrush(g_theme.textDim);
        HPEN   ap=CreatePen(PS_SOLID,1,g_theme.textDim);
        HGDIOBJ ob=SelectObject(dc,ab), op=SelectObject(dc,ap);
        Polygon(dc,tri,3);
        SelectObject(dc,ob); SelectObject(dc,op);
        DeleteObject(ab); DeleteObject(ap);
    }
    if((dis->itemState & ODS_FOCUS) && (int)dis->itemID>=0) DrawFocusRect(dc,&rc);
    return true;
}

// ============================================================================
//  Themed report ListView + header
//  The body colors cover both populated rows and the empty client area. Native
//  header visual styles are disabled only for headers opted in here, allowing
//  the parent to provide deterministic dark/light custom drawing on Win7/Wine.
// ============================================================================
static const wchar_t* const THEMED_LIST_HEADER_PROP=L"AzThemedListHeader";
static const wchar_t* const THEMED_HEADER_OLDPROC_PROP=L"AzThemedHeaderOldProc";
static const wchar_t* const THEMED_LIST_OLDPROC_PROP=L"AzThemedListOldProc";
static const wchar_t* const THEMED_LIST_OVERLAY_PROP=L"AzThemedHeaderOverlay";
static const wchar_t* const THEMED_HEADER_OVERLAY_CLASS=L"AzThemedHeaderOverlay";

static void paintThemedListHeader(HWND header, HDC dc){
    RECT client; GetClientRect(header,&client);
    HBRUSH bg=CreateSolidBrush(g_theme.surface2);
    FillRect(dc,&client,bg);
    DeleteObject(bg);

    int count=Header_GetItemCount(header);
    for(int i=0;i<count;i++){
        RECT rc={0};
        if(!Header_GetItemRect(header,i,&rc)) continue;
        wchar_t text[256]={0};
        HDITEMW item={0};
        item.mask=HDI_TEXT|HDI_FORMAT;
        item.pszText=text;
        item.cchTextMax=(int)(sizeof(text)/sizeof(text[0]));
        Header_GetItem(header,i,&item);

        HPEN pen=CreatePen(PS_SOLID,1,g_theme.border);
        HGDIOBJ oldPen=SelectObject(dc,pen);
        MoveToEx(dc,rc.left,rc.bottom-1,NULL); LineTo(dc,rc.right,rc.bottom-1);
        MoveToEx(dc,rc.right-1,rc.top,NULL); LineTo(dc,rc.right-1,rc.bottom);
        SelectObject(dc,oldPen);
        DeleteObject(pen);

        RECT tr=rc; tr.left+=S(8); tr.right-=S(8);
        UINT flags=DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_END_ELLIPSIS;
        if((item.fmt&HDF_RTLREADING) || comboHasPersian(text))
            flags|=DT_RIGHT|DT_RTLREADING;
        else if((item.fmt&HDF_JUSTIFYMASK)==HDF_CENTER)
            flags|=DT_CENTER;
        else if((item.fmt&HDF_JUSTIFYMASK)==HDF_RIGHT)
            flags|=DT_RIGHT;
        else
            flags|=DT_LEFT;
        SetBkMode(dc,TRANSPARENT);
        SetTextColor(dc,g_theme.text);
        HGDIOBJ oldFont=SelectObject(dc,g_fUIB ? g_fUIB : g_fUI);
        DrawTextW(dc,text,-1,&tr,flags);
        SelectObject(dc,oldFont);
    }
}

static LRESULT CALLBACK themedListHeaderProc(HWND h, UINT m, WPARAM w, LPARAM l){
    WNDPROC old=(WNDPROC)GetPropW(h,THEMED_HEADER_OLDPROC_PROP);
    if(!old) return DefWindowProcW(h,m,w,l);
    if(m==WM_NCDESTROY){
        RemovePropW(h,THEMED_LIST_HEADER_PROP);
        RemovePropW(h,THEMED_HEADER_OLDPROC_PROP);
        return CallWindowProcW(old,h,m,w,l);
    }
    if(m==WM_ERASEBKGND) return 1;
    if(m==WM_APP_THEME){ InvalidateRect(h,NULL,TRUE); return 0; }
    if(m==WM_PAINT){
        PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps);
        paintThemedListHeader(h,dc);
        EndPaint(h,&ps);
        return 0;
    }
    if(m==WM_PRINTCLIENT){ paintThemedListHeader(h,(HDC)w); return 0; }
    return CallWindowProcW(old,h,m,w,l);
}

// Wine can repaint a native header with system colors after NM_CUSTOMDRAW and
// even after a header subclass handles WM_PAINT. A transparent hit-test overlay
// keeps the native header fully interactive underneath while making the final
// pixels deterministic. It is a child of the ListView and follows its header.
static LRESULT CALLBACK themedHeaderOverlayProc(HWND h, UINT m, WPARAM w, LPARAM l){
    HWND header=(HWND)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_NCHITTEST: return HTTRANSPARENT;
    case WM_ERASEBKGND: return 1;
    case WM_APP_THEME: InvalidateRect(h,NULL,TRUE); return 0;
    case WM_PAINT:{
        PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps);
        if(header && IsWindow(header)) paintThemedListHeader(header,dc);
        EndPaint(h,&ps);
        return 0; }
    case WM_PRINTCLIENT:
        if(header && IsWindow(header)) paintThemedListHeader(header,(HDC)w);
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

static void updateThemedHeaderOverlay(HWND list){
    if(!list || !IsWindow(list)) return;
    HWND header=ListView_GetHeader(list);
    HWND overlay=(HWND)GetPropW(list,THEMED_LIST_OVERLAY_PROP);
    HWND host=GetParent(list);
    if(!host || !header || !IsWindow(header) || !overlay || !IsWindow(overlay)) return;
    if(!IsWindowVisible(list)){
        ShowWindow(overlay,SW_HIDE);
        return;
    }
    RECT hr; GetWindowRect(header,&hr);
    POINT tl={hr.left,hr.top}, br={hr.right,hr.bottom};
    ScreenToClient(host,&tl); ScreenToClient(host,&br);
    SetWindowPos(overlay,HWND_TOP,tl.x,tl.y,br.x-tl.x,br.y-tl.y,
                 SWP_NOACTIVATE|SWP_SHOWWINDOW);
    InvalidateRect(overlay,NULL,TRUE);
}

static bool drawThemedHeaderOwnerItem(HWND list, LPDRAWITEMSTRUCT dis){
    if(!dis || dis->CtlType!=ODT_HEADER) return false;
    HWND header=ListView_GetHeader(list);
    if(!header || !IsWindow(header) || dis->hwndItem!=header) return false;

    RECT rc=dis->rcItem;
    HBRUSH bg=CreateSolidBrush(g_theme.surface2);
    FillRect(dis->hDC,&rc,bg);
    DeleteObject(bg);

    HPEN pen=CreatePen(PS_SOLID,1,g_theme.border);
    HGDIOBJ oldPen=SelectObject(dis->hDC,pen);
    MoveToEx(dis->hDC,rc.left,rc.bottom-1,NULL);
    LineTo(dis->hDC,rc.right,rc.bottom-1);
    MoveToEx(dis->hDC,rc.right-1,rc.top,NULL);
    LineTo(dis->hDC,rc.right-1,rc.bottom);
    SelectObject(dis->hDC,oldPen);
    DeleteObject(pen);

    wchar_t text[256]={0};
    HDITEMW item={0};
    item.mask=HDI_TEXT|HDI_FORMAT;
    item.pszText=text;
    item.cchTextMax=(int)(sizeof(text)/sizeof(text[0]));
    Header_GetItem(header,(int)dis->itemID,&item);

    RECT tr=rc; tr.left+=S(8); tr.right-=S(8);
    UINT flags=DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_END_ELLIPSIS;
    if((item.fmt&HDF_RTLREADING) || comboHasPersian(text))
        flags|=DT_RIGHT|DT_RTLREADING;
    else if((item.fmt&HDF_JUSTIFYMASK)==HDF_CENTER)
        flags|=DT_CENTER;
    else if((item.fmt&HDF_JUSTIFYMASK)==HDF_RIGHT)
        flags|=DT_RIGHT;
    else
        flags|=DT_LEFT;
    SetBkMode(dis->hDC,TRANSPARENT);
    SetTextColor(dis->hDC,g_theme.text);
    HGDIOBJ oldFont=SelectObject(dis->hDC,g_fUIB ? g_fUIB : g_fUI);
    DrawTextW(dis->hDC,text,-1,&tr,flags);
    SelectObject(dis->hDC,oldFont);
    return true;
}

static LRESULT CALLBACK themedListViewProc(HWND h, UINT m, WPARAM w, LPARAM l){
    WNDPROC old=(WNDPROC)GetPropW(h,THEMED_LIST_OLDPROC_PROP);
    if(!old) return DefWindowProcW(h,m,w,l);
    if(m==WM_DRAWITEM && drawThemedHeaderOwnerItem(h,(LPDRAWITEMSTRUCT)l))
        return TRUE;
    if(m==WM_NCDESTROY){
        HWND overlay=(HWND)GetPropW(h,THEMED_LIST_OVERLAY_PROP);
        if(overlay && IsWindow(overlay)) DestroyWindow(overlay);
        RemovePropW(h,THEMED_LIST_OVERLAY_PROP);
        RemovePropW(h,THEMED_LIST_OLDPROC_PROP);
        return CallWindowProcW(old,h,m,w,l);
    }
    LRESULT r=CallWindowProcW(old,h,m,w,l);
    if(m==WM_SIZE || m==WM_WINDOWPOSCHANGED || m==WM_SHOWWINDOW)
        updateThemedHeaderOverlay(h);
    else if(m==WM_APP_THEME){
        HWND overlay=(HWND)GetPropW(h,THEMED_LIST_OVERLAY_PROP);
        if(overlay && IsWindow(overlay)) InvalidateRect(overlay,NULL,TRUE);
    }
    return r;
}

static void ensureThemedHeaderOverlay(HWND list, HWND header){
    static bool registered=false;
    if(!registered){
        WNDCLASSW wc={0};
        wc.lpfnWndProc=themedHeaderOverlayProc;
        wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.lpszClassName=THEMED_HEADER_OVERLAY_CLASS;
        RegisterClassW(&wc);
        registered=true;
    }
    if(!GetPropW(list,THEMED_LIST_OLDPROC_PROP)){
        WNDPROC old=(WNDPROC)SetWindowLongPtrW(list,GWLP_WNDPROC,
                                              (LONG_PTR)themedListViewProc);
        SetPropW(list,THEMED_LIST_OLDPROC_PROP,(HANDLE)old);
    }
    HWND overlay=(HWND)GetPropW(list,THEMED_LIST_OVERLAY_PROP);
    if(!overlay || !IsWindow(overlay)){
        overlay=CreateWindowExW(WS_EX_NOACTIVATE,
            THEMED_HEADER_OVERLAY_CLASS,L"",WS_CHILD|WS_VISIBLE,
            0,0,1,1,GetParent(list),NULL,g_hInst,NULL);
        SetWindowLongPtrW(overlay,GWLP_USERDATA,(LONG_PTR)header);
        SetPropW(list,THEMED_LIST_OVERLAY_PROP,(HANDLE)overlay);
    }
    updateThemedHeaderOverlay(list);
}

void applyThemedListView(HWND list){
    if(!list || !IsWindow(list)) return;
    ListView_SetBkColor(list,g_theme.surface);
    ListView_SetTextBkColor(list,g_theme.surface);
    ListView_SetTextColor(list,g_theme.text);

    HWND header=ListView_GetHeader(list);
    if(header && IsWindow(header)){
        SetPropW(header,THEMED_LIST_HEADER_PROP,(HANDLE)1);
        SetWindowTheme(header,L"",L"");
        // Wine can ignore NM_CUSTOMDRAW for an empty report header. Opt every
        // column into the native owner-draw contract; WM_DRAWITEM is delivered
        // to the ListView and handled by themedListViewProc without changing
        // sorting, sizing, ordering, or hit testing.
        int count=Header_GetItemCount(header);
        for(int i=0;i<count;i++){
            HDITEMW item={0}; item.mask=HDI_FORMAT;
            if(Header_GetItem(header,i,&item) && !(item.fmt&HDF_OWNERDRAW)){
                item.fmt|=HDF_OWNERDRAW;
                Header_SetItem(header,i,&item);
            }
        }
        if(!GetPropW(header,THEMED_HEADER_OLDPROC_PROP)){
            WNDPROC old=(WNDPROC)SetWindowLongPtrW(header,GWLP_WNDPROC,
                                                  (LONG_PTR)themedListHeaderProc);
            SetPropW(header,THEMED_HEADER_OLDPROC_PROP,(HANDLE)old);
        }
        ensureThemedHeaderOverlay(list,header);
        InvalidateRect(header,NULL,TRUE);
    }
    InvalidateRect(list,NULL,TRUE);
}

bool drawThemedListViewHeader(LPNMCUSTOMDRAW cd, LRESULT* result){
    if(!cd || !result || cd->hdr.code!=NM_CUSTOMDRAW ||
       !cd->hdr.hwndFrom || !GetPropW(cd->hdr.hwndFrom,THEMED_LIST_HEADER_PROP))
        return false;

    if(cd->dwDrawStage==CDDS_PREPAINT){
        *result=CDRF_NOTIFYITEMDRAW;
        return true;
    }
    if(cd->dwDrawStage!=CDDS_ITEMPREPAINT) return false;

    HDC dc=cd->hdc;
    RECT rc=cd->rc;
    HBRUSH bg=CreateSolidBrush(g_theme.surface2);
    FillRect(dc,&rc,bg);
    DeleteObject(bg);

    HPEN pen=CreatePen(PS_SOLID,1,g_theme.border);
    HGDIOBJ oldPen=SelectObject(dc,pen);
    MoveToEx(dc,rc.left,rc.bottom-1,NULL); LineTo(dc,rc.right,rc.bottom-1);
    MoveToEx(dc,rc.right-1,rc.top,NULL); LineTo(dc,rc.right-1,rc.bottom);
    SelectObject(dc,oldPen);
    DeleteObject(pen);

    wchar_t text[256]={0};
    HDITEMW item={0};
    item.mask=HDI_TEXT|HDI_FORMAT;
    item.pszText=text;
    item.cchTextMax=(int)(sizeof(text)/sizeof(text[0]));
    SendMessageW(cd->hdr.hwndFrom,HDM_GETITEMW,(WPARAM)cd->dwItemSpec,(LPARAM)&item);

    RECT tr=rc;
    tr.left+=S(8); tr.right-=S(8);
    UINT flags=DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_END_ELLIPSIS;
    if((item.fmt&HDF_RTLREADING) || comboHasPersian(text))
        flags|=DT_RIGHT|DT_RTLREADING;
    else if((item.fmt&HDF_JUSTIFYMASK)==HDF_CENTER)
        flags|=DT_CENTER;
    else if((item.fmt&HDF_JUSTIFYMASK)==HDF_RIGHT)
        flags|=DT_RIGHT;
    else
        flags|=DT_LEFT;
    SetBkMode(dc,TRANSPARENT);
    SetTextColor(dc,g_theme.text);
    HGDIOBJ oldFont=SelectObject(dc,g_fUIB ? g_fUIB : g_fUI);
    DrawTextW(dc,text,-1,&tr,flags);
    SelectObject(dc,oldFont);

    *result=CDRF_SKIPDEFAULT;
    return true;
}
