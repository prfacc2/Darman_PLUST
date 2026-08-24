// ============================================================================
//  gdiplus.cpp — v1.3.0 GDI+ rendering layer
//
//  Why GDI+?  The UI redesign asked for "richer colours, soft lighting, open
//  layers" and a real background image. Plain GDI RoundRect/FillRect can't do
//  anti-aliasing, gradients, alpha-blended shadows or JPEG decoding. GDI+ is
//  part of every Windows since XP, links statically-friendly (-lgdiplus) and
//  keeps us at a SINGLE exe with no external runtime.
//
//  Everything here is defensive: if GDI+ fails to start (extremely old / weird
//  systems) the helpers fall back to the existing plain-GDI fillRoundRect so
//  the program still runs.  The background image is embedded as RCDATA
//  (id 103 = light, id 104 = dark) and decoded once, cached as an HBITMAP.
// ============================================================================
#include "app.h"
#include <objbase.h>
#include <gdiplus.h>

using namespace Gdiplus;

static ULONG_PTR s_gdipToken = 0;
static bool      s_gdipOK    = false;

// GDI+ keeps resource streams for lazy image decoding. Cached images therefore
// must keep their IStream alive until the image itself is destroyed.
static void freeCachedResourceImages();

void gdipStartup(){
    GdiplusStartupInput in;
    if(GdiplusStartup(&s_gdipToken, &in, NULL) == Ok)
        s_gdipOK = true;
}
void gdipShutdown(){
    if(s_gdipOK){
        // Images must be deleted before GdiplusShutdown. Their retained streams
        // own the copied HGLOBAL resource bytes and are released immediately
        // afterwards, so shutdown is leak-free even in debug/smoke early exits.
        gpFreeBackgroundCache();
        freeCachedResourceImages();
        GdiplusShutdown(s_gdipToken);
        s_gdipOK=false;
    }
}

static inline Color C(COLORREF c, int a=255){
    return Color((BYTE)a, GetRValue(c), GetGValue(c), GetBValue(c));
}

//  Build a rounded-rect GraphicsPath (radius clamped to half the shorter side).
static void roundPath(GraphicsPath& p, const Rect& r, int rad){
    int w=r.Width, h=r.Height;
    int d = rad*2;
    if(d > w) d = w;
    if(d > h) d = h;
    if(d < 2){ p.AddRectangle(r); return; }
    p.AddArc(r.X,           r.Y,           d, d, 180, 90);
    p.AddArc(r.X+w-d,       r.Y,           d, d, 270, 90);
    p.AddArc(r.X+w-d,       r.Y+h-d,       d, d,   0, 90);
    p.AddArc(r.X,           r.Y+h-d,       d, d,  90, 90);
    p.CloseFigure();
}

void gpRoundRect(HDC dc, RECT rc, int rad, COLORREF fill, COLORREF border, int alpha){
    if(!s_gdipOK){ fillRoundRect(dc,rc,rad*2,fill,border); return; }
    Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
    Rect r(rc.left, rc.top, rc.right-rc.left-1, rc.bottom-rc.top-1);
    GraphicsPath p; roundPath(p,r,rad);
    if(fill!=CLR_INVALID){ SolidBrush br(C(fill,alpha)); g.FillPath(&br,&p); }
    if(border!=CLR_INVALID){ Pen pn(C(border,alpha),1.0f); g.DrawPath(&pn,&p); }
}

void gpGradRoundRect(HDC dc, RECT rc, int rad, COLORREF top, COLORREF bottom, COLORREF border){
    if(!s_gdipOK){ fillRoundRect(dc,rc,rad*2,top,border); return; }
    Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
    Rect r(rc.left, rc.top, rc.right-rc.left-1, rc.bottom-rc.top-1);
    if(r.Width<=0||r.Height<=0) return;
    GraphicsPath p; roundPath(p,r,rad);
    LinearGradientBrush br(Rect(r.X,r.Y,r.Width,r.Height+1),
        C(top), C(bottom), LinearGradientModeVertical);
    g.FillPath(&br,&p);
    if(border!=CLR_INVALID){ Pen pn(C(border),1.0f); g.DrawPath(&pn,&p); }
}

void gpFillAlpha(HDC dc, RECT rc, int rad, COLORREF fill, int alpha){
    if(alpha<=0) return;
    if(alpha>=255){ gpRoundRect(dc,rc,rad,fill,CLR_INVALID,255); return; }
    // GDI+ can fail to create translucent brushes on older/low-resource hosts.
    // Alpha overlays are decoration, so leave the already-painted opaque host
    // untouched rather than falling back to an incorrect fully opaque colour.
    if(!s_gdipOK) return;
    Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
    Rect r(rc.left,rc.top,rc.right-rc.left-1,rc.bottom-rc.top-1);
    if(r.Width<=0 || r.Height<=0) return;
    GraphicsPath p; roundPath(p,r,rad);
    SolidBrush br(C(fill,alpha));
    if(br.GetLastStatus()==Ok) g.FillPath(&br,&p);
}

// ---------------------------------------------------------------------------
//  v1.8.0  rounded-corner background fix
//  gpRoundRect / gpGradRoundRect only fill the rounded path, so the 4 corner
//  triangles (inside the bounding rect but outside the path) keep whatever was
//  in the DC before — that produced the "wrong colour / black corner" artefact
//  on rounded controls when the surrounding pixels were not pre-painted with
//  the theme background.  The *Bg variants below paint those corner gaps with
//  `bg` FIRST, so corners always blend into the theme surface.
// ---------------------------------------------------------------------------

void gpGradRibbon3(HDC dc, RECT rc, int rad, COLORREF a, COLORREF b, COLORREF c){
    int mid=(rc.left+rc.right)/2;
    RECT rL={rc.left,rc.top,mid,rc.bottom};
    RECT rR={mid,rc.top,rc.right,rc.bottom};
    gpGradRoundRectBgH(dc,rL,rad,a,b,CLR_INVALID,CLR_INVALID);
    gpGradRoundRectBgH(dc,rR,rad,b,c,CLR_INVALID,CLR_INVALID);
}

//  Paint just the 4 corner gaps of a rounded rect with `bg`.
void gpFillCorners(HDC dc, RECT rc, int rad, COLORREF bg){
    int w = rc.right-rc.left, h = rc.bottom-rc.top;
    if(w<=0||h<=0) return;
    int d = rad*2;
    if(d>w) d=w;
    if(d>h) d=h;
    if(d<2) return;                 // square — nothing to patch
    int r = d/2;
    if(!s_gdipOK){
        // Plain-GDI fallback: subtract a round-rect region from the full rect
        // and flood the remainder (the corners) with bg.
        HRGN full  = CreateRectRgn(rc.left, rc.top, rc.right, rc.bottom);
        HRGN round = CreateRoundRectRgn(rc.left, rc.top, rc.right+1, rc.bottom+1, d, d);
        CombineRgn(full, full, round, RGN_DIFF);
        HBRUSH br = CreateSolidBrush(bg);
        FillRgn(dc, full, br);
        DeleteObject(br); DeleteObject(full); DeleteObject(round);
        return;
    }
    Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
    // Build a region = boundingRect - roundedPath, fill with bg.
    Rect b(rc.left, rc.top, w-1, h-1);
    GraphicsPath rp; roundPath(rp, b, rad);
    Region reg(b);                  // whole bounding box
    reg.Exclude(&rp);               // remove the rounded interior → corners only
    SolidBrush br(C(bg));
    g.FillRegion(&br, &reg);
    (void)r;
}

void gpRoundRectBg(HDC dc, RECT rc, int rad, COLORREF fill, COLORREF border, COLORREF bg, int alpha){
    if(bg!=CLR_INVALID) gpFillCorners(dc, rc, rad, bg);
    gpRoundRect(dc, rc, rad, fill, border, alpha);
}

void gpGradRoundRectBg(HDC dc, RECT rc, int rad, COLORREF top, COLORREF bottom, COLORREF border, COLORREF bg){
    if(bg!=CLR_INVALID) gpFillCorners(dc, rc, rad, bg);
    gpGradRoundRect(dc, rc, rad, top, bottom, border);
}

//  v1.19.0: horizontal (left→right) gradient rounded rect. Mirrors
//  gpGradRoundRect but uses LinearGradientModeHorizontal.
void gpGradRoundRectBgH(HDC dc, RECT rc, int rad, COLORREF left, COLORREF right, COLORREF border, COLORREF bg){
    if(bg!=CLR_INVALID) gpFillCorners(dc, rc, rad, bg);
    if(!s_gdipOK){ fillRoundRect(dc,rc,rad*2,left,border); return; }
    Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
    Rect r(rc.left, rc.top, rc.right-rc.left-1, rc.bottom-rc.top-1);
    if(r.Width<=0||r.Height<=0) return;
    GraphicsPath p; roundPath(p,r,rad);
    LinearGradientBrush br(Rect(r.X,r.Y,r.Width+1,r.Height),
        C(left), C(right), LinearGradientModeHorizontal);
    g.FillPath(&br,&p);
    if(border!=CLR_INVALID){ Pen pn(C(border),1.0f); g.DrawPath(&pn,&p); }
}

//  Soft drop shadow: draw several expanding translucent rounded rects so the
//  edge fades out — a cheap, dependency-free blur that gives cards real depth.
//  v1.63.0: tinted variant. A coloured button casts coloured light, so its
//  elevation shadow is drawn in a very dark mix of its own hue rather than
//  neutral black — that is what makes the modern solid buttons read as lit
//  objects instead of stickers. gpShadow() is now this function with black.
void gpShadowColor(HDC dc, RECT rc, int rad, int spread, int alpha, COLORREF tint){
    if(!s_gdipOK) return;
    // pull the tint down hard: a shadow is the colour's shade, not the colour.
    BYTE tr=(BYTE)((GetRValue(tint)*32)/100);
    BYTE tg=(BYTE)((GetGValue(tint)*32)/100);
    BYTE tb=(BYTE)((GetBValue(tint)*32)/100);
    Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
    int layers = spread; if(layers<1) layers=1; if(layers>24) layers=24;
    int step = (layers + 11) / 12; if(step<1) step=1;
    for(int i=layers;i>=1;i-=step){
        int a = (alpha * (layers-i+1) * step) / (layers*layers);
        if(a<1) a=1;
        if(a>255) a=255;
        Rect r(rc.left-i, rc.top-i+2, (rc.right-rc.left)+2*i-1,
               (rc.bottom-rc.top)+2*i-1);
        GraphicsPath p; roundPath(p,r,rad+i);
        SolidBrush br(Color((BYTE)a,tr,tg,tb));
        g.FillPath(&br,&p);
    }
}

void gpShadow(HDC dc, RECT rc, int rad, int spread, int alpha){
    if(!s_gdipOK) return;
    Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
    int layers = spread; if(layers<1) layers=1; if(layers>24) layers=24;
    // v1.63.0 PERF: a wide shadow (spread 20-30, as used by the welcome hero
    // panel and the settings card) previously issued ONE antialiased FillPath
    // per pixel of spread — 24 full-panel path fills on every repaint, which is
    // where most of the paint budget went. The ring count is now capped at 12 by
    // walking the spread in steps; each drawn ring carries the alpha of the
    // steps it replaces, so the falloff — and therefore the look — is preserved
    // while the number of GDI+ path fills is halved or better.
    int step = (layers + 11) / 12; if(step<1) step=1;
    for(int i=layers;i>=1;i-=step){
        int a = (alpha * (layers-i+1) * step) / (layers*layers);
        if(a<1) a=1;
        if(a>255) a=255;
        Rect r(rc.left-i, rc.top-i+2, (rc.right-rc.left)+2*i-1,
               (rc.bottom-rc.top)+2*i-1);
        GraphicsPath p; roundPath(p,r,rad+i);
        SolidBrush br(Color((BYTE)a,0,0,0));
        g.FillPath(&br,&p);
    }
}

void gpLine(HDC dc, int x1,int y1,int x2,int y2, COLORREF col, float w, int alpha){
    if(!s_gdipOK){
        HPEN pn=CreatePen(PS_SOLID,(int)(w+0.5f),col);
        HGDIOBJ op=SelectObject(dc,pn);
        MoveToEx(dc,x1,y1,0); LineTo(dc,x2,y2);
        SelectObject(dc,op); DeleteObject(pn); return;
    }
    Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pn(C(col,alpha), w);
    g.DrawLine(&pn,(REAL)x1,(REAL)y1,(REAL)x2,(REAL)y2);
}

// ----------------------------------------------------- background image -----
//  Decode an embedded JPEG/PNG into an Image while retaining the source stream.
//  Image::FromStream may decode lazily; releasing `st` while the Image is cached
//  is explicitly unsupported and caused empty logo circles on some GDI+ builds.
static Image* loadResourceImage(int resId, IStream** retainedStream){
    if(retainedStream) *retainedStream=NULL;
    HRSRC hr = FindResourceW(g_hInst, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if(!hr) return NULL;
    HGLOBAL hg = LoadResource(g_hInst, hr);
    DWORD   sz = SizeofResource(g_hInst, hr);
    void*  dat = LockResource(hg);
    if(!dat || !sz) return NULL;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, sz);
    if(!mem) return NULL;
    void* p = GlobalLock(mem);
    if(!p){ GlobalFree(mem); return NULL; }
    memcpy(p, dat, sz); GlobalUnlock(mem);
    IStream* st = NULL;
    if(CreateStreamOnHGlobal(mem, TRUE, &st) != S_OK){ GlobalFree(mem); return NULL; }
    Image* img = Image::FromStream(st);
    if(!img || img->GetLastStatus()!=Ok){
        delete img;
        st->Release();
        return NULL;
    }
    if(retainedStream) *retainedStream=st;
    else st->Release();
    return img;
}

// Decode the embedded JPEG (RCDATA 103/104) once and cache it together with its
// owning stream. The stream owns the copied HGLOBAL (fDeleteOnRelease=TRUE).
static Image*   s_bgLight       = NULL;
static Image*   s_bgDark        = NULL;
static IStream* s_bgLightStream = NULL;
static IStream* s_bgDarkStream  = NULL;

// ---------------------------------------------------------------------------
//  v1.63.0 PERFORMANCE FIX — cached background composite.
//
//  The old gpDrawBackground re-scaled the FULL 1920x1080 embedded JPEG with
//  InterpolationModeHighQualityBicubic and re-filled the scrim on EVERY single
//  WM_PAINT. A bicubic resample of a 2-megapixel image costs tens of
//  milliseconds; the welcome screen repaints on every theme flip, on every
//  settings-panel close (InvalidateRect(frame,NULL,TRUE) invalidates children
//  too) and on every frame resize. That per-paint resample was the dominant
//  cost behind the "opening the settings panel drops FPS" report.
//
//  The artwork + scrim are now composited ONCE into a DIB cache keyed by
//  (width, height, theme, scrim colour, scrim alpha). Subsequent paints are a
//  single BitBlt, i.e. O(pixels) memory copy with zero resampling. The cache is
//  invalidated automatically whenever any key changes, so correctness is
//  unchanged — only the cost is.
// ---------------------------------------------------------------------------
static HDC      s_bgcDC   = NULL;
static HBITMAP  s_bgcBmp  = NULL;
static HGDIOBJ  s_bgcOld  = NULL;
static int      s_bgcW    = 0, s_bgcH = 0;
static bool     s_bgcDark = false;
static COLORREF s_bgcScrim= 0;
static int      s_bgcA    = -1;

void gpFreeBackgroundCache(){
    if(s_bgcDC){ SelectObject(s_bgcDC, s_bgcOld); DeleteDC(s_bgcDC); s_bgcDC=NULL; }
    if(s_bgcBmp){ DeleteObject(s_bgcBmp); s_bgcBmp=NULL; }
    s_bgcW=s_bgcH=0; s_bgcA=-1;
}

//  Render the artwork + scrim into the cache DC at the given size. Returns
//  false when the artwork is unavailable (caller then paints a gradient).
static bool buildBgComposite(HDC ref, int W, int H, bool dark,
                             COLORREF scrim, int scrimA){
    Image*& slot = dark ? s_bgDark : s_bgLight;
    IStream*& stream = dark ? s_bgDarkStream : s_bgLightStream;
    if(!slot) slot = loadResourceImage(dark ? 104 : 103, &stream);
    if(!slot) return false;
    REAL iw = (REAL)slot->GetWidth(), ih = (REAL)slot->GetHeight();
    if(iw<=0||ih<=0) return false;

    gpFreeBackgroundCache();
    s_bgcDC  = CreateCompatibleDC(ref);
    if(!s_bgcDC) return false;
    s_bgcBmp = CreateCompatibleBitmap(ref, W, H);
    if(!s_bgcBmp){ DeleteDC(s_bgcDC); s_bgcDC=NULL; return false; }
    s_bgcOld = SelectObject(s_bgcDC, s_bgcBmp);

    {
        Graphics g(s_bgcDC);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(PixelOffsetModeHalf);
        // cover-fit (fill the whole area, crop overflow, keep aspect)
        REAL scale = (W/iw > H/ih) ? W/iw : H/ih;
        REAL dw = iw*scale, dh = ih*scale;
        REAL dx = (W-dw)/2, dy = (H-dh)/2;
        g.DrawImage(slot, RectF(dx,dy,dw,dh), 0,0,iw,ih, UnitPixel);
        if(scrimA>0){
            SolidBrush br(C(scrim, scrimA));
            g.FillRectangle(&br, 0, 0, W, H);
        }
    }
    s_bgcW=W; s_bgcH=H; s_bgcDark=dark; s_bgcScrim=scrim; s_bgcA=scrimA;
    return true;
}

bool gpDrawBackground(HDC dc, RECT rc, bool dark, COLORREF scrim, int scrimA){
    if(!s_gdipOK) return false;
    int W = rc.right-rc.left, H = rc.bottom-rc.top;
    if(W<=0||H<=0) return false;

    bool fresh = s_bgcDC && s_bgcW==W && s_bgcH==H && s_bgcDark==dark &&
                 s_bgcScrim==scrim && s_bgcA==scrimA;
    if(!fresh && !buildBgComposite(dc, W, H, dark, scrim, scrimA)) return false;
    if(!s_bgcDC) return false;
    BitBlt(dc, rc.left, rc.top, W, H, s_bgcDC, 0, 0, SRCCOPY);
    return true;
}

// ------------------------------------------------- tinted raster icons ------
//  Real (raster) icons embedded as RCDATA PNGs are drawn white-on-alpha and
//  recoloured to any theme colour at draw time via a GDI+ colour matrix. This
//  gives the print buttons proper image icons that still adapt to the theme.
static const int RES_IMAGE_CACHE_SIZE = 16;
static Image*   s_iconCache[RES_IMAGE_CACHE_SIZE] = {0};
static IStream* s_iconStream[RES_IMAGE_CACHE_SIZE] = {0};
static int      s_iconResId[RES_IMAGE_CACHE_SIZE] = {0};

static Image* cachedResImage(int resId){
    for(int i=0;i<RES_IMAGE_CACHE_SIZE;i++)
        if(s_iconResId[i]==resId) return s_iconCache[i];
    for(int i=0;i<RES_IMAGE_CACHE_SIZE;i++) if(s_iconResId[i]==0){
        // Cache failures too; otherwise a missing/corrupt resource is decoded
        // again on every paint and can become a persistent FPS drain.
        s_iconResId[i]=resId;
        s_iconCache[i]=loadResourceImage(resId,&s_iconStream[i]);
        return s_iconCache[i];
    }
    // The app currently embeds fewer image resources than the fixed cache can
    // hold. Refuse an uncached lazy image rather than returning one whose stream
    // cannot be owned and whose lifetime the caller cannot express.
    return NULL;
}

static void freeCachedResourceImages(){
    delete s_bgLight; s_bgLight=NULL;
    delete s_bgDark;  s_bgDark=NULL;
    if(s_bgLightStream){ s_bgLightStream->Release(); s_bgLightStream=NULL; }
    if(s_bgDarkStream){ s_bgDarkStream->Release(); s_bgDarkStream=NULL; }
    for(int i=0;i<RES_IMAGE_CACHE_SIZE;i++){
        delete s_iconCache[i]; s_iconCache[i]=NULL;
        if(s_iconStream[i]){ s_iconStream[i]->Release(); s_iconStream[i]=NULL; }
        s_iconResId[i]=0;
    }
}

//  Draw RCDATA PNG `resId` centred & aspect-fit inside `rc`, recoloured to
//  `tint`.  Returns false if GDI+/resource unavailable so callers can fall back
//  to the vector drawIcon().
bool gpDrawTintedImageRes(HDC dc, int resId, RECT rc, COLORREF tint){
    if(!s_gdipOK) return false;
    Image* img = cachedResImage(resId);
    if(!img) return false;
    Graphics g(dc);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    int W = rc.right-rc.left, H = rc.bottom-rc.top;
    if(W<=0||H<=0) return false;
    REAL iw=(REAL)img->GetWidth(), ih=(REAL)img->GetHeight();
    if(iw<=0||ih<=0) return false;
    REAL scale = (W/iw < H/ih) ? W/iw : H/ih;   // contain-fit
    REAL dw=iw*scale, dh=ih*scale;
    REAL dx=rc.left+(W-dw)/2, dy=rc.top+(H-dh)/2;

    // colour matrix: replace RGB with tint, keep source alpha
    REAL r=GetRValue(tint)/255.0f, gg=GetGValue(tint)/255.0f, b=GetBValue(tint)/255.0f;
    ColorMatrix cm = {
        0,0,0,0,0,
        0,0,0,0,0,
        0,0,0,0,0,
        0,0,0,1,0,
        r,gg,b,0,1 };
    ImageAttributes ia; ia.SetColorMatrix(&cm);
    // v1.66.0: GDI+ decodes lazily at DRAW time — a corrupt/undecodable image
    // returns non-Ok HERE even though loading "succeeded". Report failure so
    // the caller draws the vector-glyph fallback instead of leaving the button
    // face empty (white-on-white icons seen on some machines).
    Status st = g.DrawImage(img, RectF(dx,dy,dw,dh), 0,0,iw,ih, UnitPixel, &ia);
    return st==Ok;
}

//  v1.64.0 (درمان پلاس): draw an embedded RCDATA PNG cover-fitted into a circle.
//  Used for the brand logo on the welcome screen, the login card and the header.
bool gpDrawImageResCircle(HDC dc, int resId, RECT rc){
    if(!s_gdipOK) return false;
    Image* img = cachedResImage(resId);
    if(!img) return false;
    REAL iw=(REAL)img->GetWidth(), ih=(REAL)img->GetHeight();
    if(iw<=0||ih<=0) return false;
    int W=rc.right-rc.left, H=rc.bottom-rc.top;
    if(W<=0||H<=0) return false;

    Graphics g(dc);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    // circular clip
    GraphicsPath clip;
    clip.AddEllipse(rc.left, rc.top, W-1, H-1);
    g.SetClip(&clip);

    // cover-fit (fill the circle, crop overflow)
    REAL scale = (W/iw > H/ih) ? W/iw : H/ih;
    REAL dw=iw*scale, dh=ih*scale;
    REAL dx=rc.left+(W-dw)/2, dy=rc.top+(H-dh)/2;
    // v1.66.0: check the DRAW status — GDI+ decodes lazily, so a PNG that
    // "loaded" fine can still fail here (seen as EMPTY logo circles on some
    // machines). Returning false lets callers paint their disc+glyph fallback.
    Status st = g.DrawImage(img, RectF(dx,dy,dw,dh), 0,0,iw,ih, UnitPixel);
    g.ResetClip();
    return st==Ok;
}

//  v1.6.0: draw a profile photo from a file, cropped/scaled into a CIRCLE that
//  fits the given rect. Used for user avatars (settings panel / reception info
//  panel). Returns false if GDI+ is off or the file can't be loaded, so callers
//  fall back to the initials/guest icon.
bool gpDrawImageFileCircle(HDC dc, const std::wstring& path, RECT rc){
    if(!s_gdipOK || path.empty()) return false;
    Image img(path.c_str());
    if(img.GetLastStatus()!=Ok) return false;
    REAL iw=(REAL)img.GetWidth(), ih=(REAL)img.GetHeight();
    if(iw<=0||ih<=0) return false;
    int W=rc.right-rc.left, H=rc.bottom-rc.top;
    if(W<=0||H<=0) return false;

    Graphics g(dc);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    // circular clip
    GraphicsPath clip;
    clip.AddEllipse(rc.left, rc.top, W-1, H-1);
    g.SetClip(&clip);

    // cover-fit (fill the circle, crop overflow)
    REAL scale = (W/iw > H/ih) ? W/iw : H/ih;
    REAL dw=iw*scale, dh=ih*scale;
    REAL dx=rc.left+(W-dw)/2, dy=rc.top+(H-dh)/2;
    g.DrawImage(&img, RectF(dx,dy,dw,dh), 0,0,iw,ih, UnitPixel);
    g.ResetClip();
    return true;
}

// ---------------------------------------------------------------------------
//  v1.20.0: draw an image into a rect (aspect-fit, no crop). Accepts either a
//  file path OR a "data:image/...;base64,..." URI (the print designer stores
//  uploaded logos / patient photos as base64 data URIs). Used by the print
//  renderer so PIT_LOGO / PIT_PHOTO / PIT_IMAGE actually print the picture.
//  Returns false if GDI+ is off or the source can't be decoded (caller then
//  falls back to drawing a labelled placeholder box).
// ---------------------------------------------------------------------------
static int b64val(int c){
    if(c>='A'&&c<='Z') return c-'A';
    if(c>='a'&&c<='z') return c-'a'+26;
    if(c>='0'&&c<='9') return c-'0'+52;
    if(c=='+') return 62; if(c=='/') return 63; return -1;
}
static std::string b64decode(const std::string& in){
    std::string out; int val=0,bits=-8;
    for(unsigned char c:in){ if(c=='='){break;} int d=b64val(c); if(d<0)continue;
        val=(val<<6)|d; bits+=6; if(bits>=0){ out.push_back((char)((val>>bits)&0xFF)); bits-=8; } }
    return out;
}

// v1.23.0 core: render an image with explicit object-fit + inner padding so the
// print engine and the designer preview are pixel-identical. The image is HARD-
// clipped to (rc minus padding); it can never stretch (unless fit==fill), never
// overflow, and never cover neighbouring text.
bool gpDrawImageRectFit(HDC dc, const std::wstring& src, RECT rc, int fit, int padPx){
    if(!s_gdipOK || src.empty()) return false;
    if(padPx<0) padPx=0;
    // apply padding (clamp so the box never inverts)
    RECT box=rc;
    box.left+=padPx; box.top+=padPx; box.right-=padPx; box.bottom-=padPx;
    if(box.right<=box.left || box.bottom<=box.top){ box=rc; }
    REAL X=(REAL)box.left, Y=(REAL)box.top;
    REAL W=(REAL)(box.right-box.left), H=(REAL)(box.bottom-box.top);
    if(W<=0||H<=0) return false;

    Image* img=NULL; IStream* st=NULL;
    if(src.compare(0,5,L"data:")==0){
        size_t comma=src.find(L','); if(comma==std::wstring::npos) return false;
        std::string b64; b64.reserve(src.size()-comma);
        for(size_t i=comma+1;i<src.size();++i){ wchar_t w=src[i]; if(w<128) b64.push_back((char)w); }
        std::string bytes=b64decode(b64); if(bytes.empty()) return false;
        HGLOBAL hg=GlobalAlloc(GMEM_MOVEABLE,bytes.size()); if(!hg) return false;
        void* p=GlobalLock(hg); if(!p){ GlobalFree(hg); return false; }
        memcpy(p,bytes.data(),bytes.size()); GlobalUnlock(hg);
        if(CreateStreamOnHGlobal(hg,TRUE,&st)!=S_OK){ GlobalFree(hg); return false; }
        img=Image::FromStream(st);
    } else {
        img=new Image(src.c_str());
    }
    if(!img || img->GetLastStatus()!=Ok){ if(img)delete img; if(st)st->Release(); return false; }
    REAL iw=(REAL)img->GetWidth(), ih=(REAL)img->GetHeight();
    if(iw<=0||ih<=0){ delete img; if(st)st->Release(); return false; }

    Graphics g(dc);
    // high-quality, high-DPI friendly resampling — no blur, no pixelation.
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetCompositingQuality(CompositingQualityHighQuality);

    // HARD-clip to the (padded) box: the image can NEVER bleed outside.
    g.SetClip(Rect((INT)X,(INT)Y,(INT)W,(INT)H));

    // image attributes: clamp the wrap mode so edge pixels don't smear when the
    // image exactly fills its box (prevents 1-px transparent borders on cover).
    ImageAttributes ia;
    ia.SetWrapMode(WrapModeTileFlipXY);

    REAL dx,dy,dw,dh;
    if(fit==2){
        // fill — stretch to the whole box (may change aspect ratio).
        dx=X; dy=Y; dw=W; dh=H;
    } else if(fit==1){
        // cover — scale to the LARGER ratio so the box is fully covered; the
        // clip above crops the overflow. Aspect ratio is preserved.
        REAL scale=(W/iw > H/ih)? W/iw : H/ih;
        dw=iw*scale; dh=ih*scale;
        dx=X+(W-dw)/2; dy=Y+(H-dh)/2;
    } else {
        // contain (default) — scale to the SMALLER ratio so the whole image
        // fits inside the box, centred, with no crop and no stretch.
        REAL scale=(W/iw < H/ih)? W/iw : H/ih;
        dw=iw*scale; dh=ih*scale;
        dx=X+(W-dw)/2; dy=Y+(H-dh)/2;
    }
    g.DrawImage(img, RectF(dx,dy,dw,dh), 0,0,iw,ih, UnitPixel, &ia);
    g.ResetClip();
    delete img; if(st)st->Release();
    return true;
}

// Backward-compatible wrapper (existing callers): contain-fit, no padding.
bool gpDrawImageRectAny(HDC dc, const std::wstring& src, RECT rc){
    return gpDrawImageRectFit(dc, src, rc, 0, 0);
}
