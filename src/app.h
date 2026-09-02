// ============================================================================
//  DarmanPlus  (درمان پلاس)
//  Clinic Reception & Management System
//  Core shared header
//  Target: Windows 7/8/8.1/10/11+  (single x86 exe, runs on x86 & x64)
// ============================================================================
#pragma once
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601   // Windows 7 baseline
#define WINVER       0x0601
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <winspool.h>
#include <math.h>
#include <string>
#include <vector>

// ---------------------------------------------------------------- version --
#define APP_VERSION_W   L"2.08.1"

// ----------------------------------------------------------- logging policy -
//  RELEASE 1.2.0 (Section A): all general user-behavior logging is gated behind
//  AZ_DEBUG_LOGS, which is OFF (0) in release. The only log channels that remain
//  active are the dedicated Backup Log (backup_log.h) and the crash dump.
#ifndef AZ_DEBUG_LOGS
#define AZ_DEBUG_LOGS 0
#endif
#define APP_NAME_W      L"\u062f\u0631\u0645\u0627\u0646 \u067e\u0644\u0627\u0633"   // درمان پلاس
#define APP_CLASS_W     L"DarmanPlusFrame"

// ----------------------------------------------------------------- globals -
extern HINSTANCE g_hInst;
extern HWND      g_hFrame;          // fullscreen main frame
extern double    g_scale;           // responsive UI scale
extern bool      g_lowSpec;         // speed-handler: weak hardware mode
extern bool      g_dark;            // dark theme active
enum ThemeMode { TM_LIGHT=0, TM_DARK=1, TM_NEON=2 };
extern ThemeMode g_themeMode;       // v1.93: three themes — light / dark / neon

// fonts (Vazirmatn, embedded)
extern HFONT g_fUI, g_fUIB, g_fSmall, g_fTitle, g_fBig, g_fHuge, g_fMono;
// v1.27.0 UI redesign: dedicated field-label font (13px medium — readable, not
// tiny) and a stronger section-title font (16 bold). Used by the reception
// admission form so labels never blend into the background.
extern HFONT g_fLabel, g_fSection;
// v1.31.0 RESPONSIVE-LABEL FIX: on tight screens the reception layout shrinks
// its row heights / label bands by a single fit-factor. The label/section fonts
// used to stay FIXED (S(13)/S(16)) while the band around them shrank — so labels
// were clipped or covered by the controls sitting below them. These helpers
// give the painter a font whose pixel height tracks the same fit-factor, so a
// label always fits inside its (shrunken) band and is never covered.
//   fitFont(px, weight, f)  → cached CreateFontW(-S(px)*f) keyed by (px,weight,f).
HFONT fitFont(int px, int weight, double f);
// §G (1.11.0): a TRUE fixed-pitch font for section / personnel CODES so digits
// align in a clean column. Falls back Consolas → Courier New at build time.
extern HFONT g_fCode;

// scale helper
inline int S(int v){ return (int)(v * g_scale + 0.5); }

// Blend color `a` toward `b` by `pct` percent (0 = pure a, 100 = pure b).
// Used for subtle, theme-aware UI tints (e.g. a soft focus ring).
inline COLORREF blendColor(COLORREF a, COLORREF b, int pct){
    if(pct<0) pct=0;
    if(pct>100) pct=100;
    int ra=GetRValue(a), ga=GetGValue(a), ba=GetBValue(a);
    int rb=GetRValue(b), gb=GetGValue(b), bb=GetBValue(b);
    int r=ra+(rb-ra)*pct/100;
    int g=ga+(gb-ga)*pct/100;
    int bl=ba+(bb-ba)*pct/100;
    return RGB(r,g,bl);
}

// ------------------------------------------------------------------ theme --
struct Theme {
    COLORREF bg;          // window background
    COLORREF bg2;         // secondary page tint (subtle gradient bottom)
    COLORREF surface;     // card background
    COLORREF surface2;    // secondary surface (bars)
    COLORREF surfaceTop;  // card gradient top (lighter)
    COLORREF border;      // borders / separators
    COLORREF text;        // primary text
    COLORREF textDim;     // secondary text
    COLORREF labelInk;    // v1.27.0: form field labels — readable (#374151 light)
    COLORREF sectionInk;  // v1.27.0: section titles — strong (#1F2937 light)
    COLORREF accent;      // primary accent
    COLORREF accent2;     // accent gradient end (for buttons/header)
    COLORREF accentHover;
    COLORREF accentText;  // text on accent
    COLORREF danger;
    COLORREF dangerHover;
    COLORREF success;
    COLORREF warn;        // amber for highlights / chips
    COLORREF inputBg;
    COLORREF inputText;
    COLORREF hover;       // ghost-button hover
    COLORREF headerTop;   // top header bar gradient top
    COLORREF headerBot;   // top header bar gradient bottom
};
extern Theme   g_theme;
extern HBRUSH  g_brBg, g_brSurface, g_brSurface2, g_brInput;
void applyTheme(bool dark);             // rebuild colors + brushes
void applyThemeMode(ThemeMode mode);    // v1.93: three-mode theme (light/dark/neon)
void broadcastThemeChange();            // invalidate everything
#define WM_APP_THEME (WM_APP+11)        // sent to every window on theme switch
// ---------------------------------------------------------- 1.4.0 messages --
//  Broadcast / internal messages introduced by release 1.4.0.
#define WM_APP_THEME_CHANGED (WM_APP+12) // a settings panel changed the theme
#define WM_APP_LAYOUT_REDO   (WM_APP+13) // AzLayoutGuard requests a relayout
#define WM_APP_DESIGN_PUSHED (WM_APP+14) // a print design was pushed to sections
// v1.40.0 (multi-page shell): the embedded-web worker pool marshals a callable
// back onto the UI thread by PostMessage(g_hFrame, WM_APP_UI_TASK, 0, task).
// The frame proc runs it and frees it. See src/web_thread_pool.{h,cpp} +
// RunOnUiThread(). WM_APP+15 keeps clear of the 1.4.0 message block above.
#define WM_APP_UI_TASK       (WM_APP+15) // run a marshalled callable on the UI thread

// ------------------------------------------------------------- flat button -
enum IconId {
    ICO_NONE=0, ICO_X, ICO_CALC, ICO_PRINT, ICO_UPDATE, ICO_MOON, ICO_SUN,
    ICO_USER, ICO_SHIELD, ICO_PLUS, ICO_LOGOUT, ICO_DETACH, ICO_CROSS_MED,
    ICO_CHECK, ICO_TRASH, ICO_SAVE, ICO_BACK,
    ICO_ID, ICO_PHONE, ICO_CAL, ICO_PIN, ICO_RECEIPT, ICO_CLOCK, ICO_REFRESH,
    ICO_GEAR, ICO_BELL, ICO_TAB, ICO_CHEVRON,
    // v1.11.0 §F: bookmark glyph for the Saved-Messages feature.
    ICO_SAVED_MSG,
    // v1.11.0 §A: people / contact / palette glyphs for messenger-style rows.
    ICO_PALETTE, ICO_INFO, ICO_PEOPLE,
    // v1.19.0: wallet glyph for the «مبلغ نهایی» (final-amount) summary card.
    ICO_WALLET,
    // v1.77.0: پاکت نامه — closed envelope / letter glyph for the کارتابل
    // (management inbox) tab, replacing the old bell/message look.
    ICO_LETTER,
    // v1.87.0: person-plus glyph for the «پذیرش بیمار» header action so it is
    // visually distinct from «تب جدید» (which keeps the plain plus/tab glyph).
    ICO_USER_ADD,
    // v1.89.0: house glyph for the permanent «داشبورد» reception tab.
    ICO_HOME
};
// §F: spec name alias — the work order references this symbol explicitly.
#define IC_SAVED_MSG ICO_SAVED_MSG
enum BtnStyle { BS_GHOST=0, BS_PRIMARY=1, BS_DANGER=2, BS_OUTLINE=3, BS_CARD=4,
                //  v1.8.0: a distinct, non-red "attention" style (violet/teal)
                //  used for the management change-request categories so they
                //  stand out clearly WITHOUT using the danger red.
                BS_INFO=5 };
//  v1.8.0: a distinct accent for the "attention / requests" controls (NOT red).
extern COLORREF g_infoAccent, g_infoAccent2;
void  registerFlatButton();
HWND  createFlatButton(HWND parent, int id, const wchar_t* text,
                       int icon, int style,
                       int x,int y,int w,int h, const wchar_t* sub=NULL);
void  setFlatButtonIcon(HWND btn, int icon);   // in-place icon swap (v1.1.0)
//  v2.01 (Part E2): in-place STYLE swap (BS_PRIMARY ⇄ BS_OUTLINE, …) for tool
//  toggles — repaints the button without recreating it.
void  setFlatButtonStyle(HWND btn, int style);
//  v1.4.0: tell a flat button what colour sits BEHIND its rounded corners so
//  the antialiased corners blend into the real surface (fixes the "white
//  corners in dark mode" bug on header/bar buttons). Pass CLR_INVALID to let
//  the button ask its parent (default behaviour).
void  setFlatButtonBg(HWND btn, COLORREF bg);
//  v1.78.0: per-button brand accent for BS_CARD (0 = theme accent)
void  setFlatButtonAccent(HWND btn, COLORREF accent);
//  v1.77: blend a header button's rounded corners with the header GRADIENT
//  (its vertical midpoint) instead of the flat headerTop colour — fixes the
//  "white square behind the header gear/calculator icons" on the light theme.
void  setFlatButtonHeaderMid(HWND btn);
//  v1.4.1: give a flat button a real raster icon (RCDATA id; 0 = vector icon).
void  setFlatButtonImage(HWND btn, int resId);
void  drawIcon(HDC dc, int icon, RECT rc, COLORREF col, int thick);
void  fillRoundRect(HDC dc, RECT rc, int rad, COLORREF fill, COLORREF border);
//  v1.6.0: a fully theme-aware owner-draw combobox (fixes dark-mode dropdown
//  white-on-white) — create with createThemedCombo, forward WM_DRAWITEM to
//  drawThemedComboItem in the parent.
HWND  createThemedCombo(HWND parent, int id);
bool  drawThemedComboItem(LPDRAWITEMSTRUCT dis);
//  Theme a report ListView and its native header. Safe for admin/management
//  tables; call again after applyTheme() to refresh all colors in place.
void  applyThemedListView(HWND list);
//  Handle NM_CUSTOMDRAW for a ListView header themed by applyThemedListView().
bool  drawThemedListViewHeader(LPNMCUSTOMDRAW cd, LRESULT* result);

// ---------------------------------------------------------------- GDI+ -----
//  v1.3.0: a thin GDI+ helper layer gives us the "richer colours, lighting,
//  smooth layers" the UI redesign needs — anti-aliased rounded cards, vertical
//  gradients, soft drop-shadows, translucent overlays and a real background
//  image — all without leaving the single static EXE (gdiplus ships with
//  every Windows since XP).  All helpers degrade gracefully if GDI+ is absent.
void gdipStartup();
void gdipShutdown();
//  Anti-aliased filled rounded rectangle (optionally with a 1px border).
void gpRoundRect(HDC dc, RECT rc, int rad, COLORREF fill, COLORREF border, int alpha=255);
//  Vertical 2-stop gradient rounded rectangle.
void gpGradRoundRect(HDC dc, RECT rc, int rad, COLORREF top, COLORREF bottom, COLORREF border);
//  v1.8.0: rounded-rect variants that FIRST paint the 4 corner triangles (the
//  area outside the rounded path but inside the bounding rect) with `bg` so the
//  corners always match the surrounding theme background — no dark/black/wrong
//  colour artefacts on rounded controls, cards, wells, lists or combos.
void gpRoundRectBg(HDC dc, RECT rc, int rad, COLORREF fill, COLORREF border, COLORREF bg, int alpha=255);
void gpGradRoundRectBg(HDC dc, RECT rc, int rad, COLORREF top, COLORREF bottom, COLORREF border, COLORREF bg);
//  v1.19.0: HORIZONTAL 2-stop gradient rounded rect (left→right), corners first
//  filled with `bg`. Used for the «مبلغ نهایی» card (sky-blue → royal-blue).
void gpGradRoundRectBgH(HDC dc, RECT rc, int rad, COLORREF left, COLORREF right, COLORREF border, COLORREF bg);
//  v1.87.0: three-stop horizontal "gradialism" ribbon (a → b → c) — the
//  signature sweep used by the header, the welcome hero and the login card.
void gpGradRibbon3(HDC dc, RECT rc, int rad, COLORREF a, COLORREF b, COLORREF c);
//  Paint only the 4 rounded-corner gaps of `rc` (radius `rad`) with `bg`. Use
//  this to "patch" the corners behind any rounded region whose interior is
//  already drawn (e.g. owner-drawn lists / combos / regions).
void gpFillCorners(HDC dc, RECT rc, int rad, COLORREF bg);
//  Soft drop shadow behind a rounded rect (blurred, layered look).
void gpShadow(HDC dc, RECT rc, int rad, int spread, int alpha);
//  v1.63.0: same shadow, tinted with `tint` shaded to ~32% so a coloured
//  control casts coloured light (used by the modern solid button styles).
void gpShadowColor(HDC dc, RECT rc, int rad, int spread, int alpha, COLORREF tint);
//  Solid translucent rounded fill (for glass overlays / dim layers).
void gpFillAlpha(HDC dc, RECT rc, int rad, COLORREF fill, int alpha);
//  Draw the embedded background image (id 103 light / 104 dark) cover-fitted
//  into rc, then a translucent scrim of `scrim` colour at `scrimA` alpha so
//  foreground text stays perfectly legible.  Returns false if no image.
bool gpDrawBackground(HDC dc, RECT rc, bool dark, COLORREF scrim, int scrimA);
//  v1.63.0: gpDrawBackground caches the scaled artwork + scrim composite keyed
//  by (size, theme, scrim). Call this to drop the cache when the theme changes
//  so the next paint rebuilds it (a stale cache would show the old scrim).
void gpFreeBackgroundCache();
//  v1.4.1: draw a real (raster) RCDATA PNG icon, aspect-fit & centred in rc and
//  recoloured to `tint`. Used for the print-action buttons. Returns false if
//  GDI+ / resource unavailable so callers fall back to the vector drawIcon().
bool gpDrawTintedImageRes(HDC dc, int resId, RECT rc, COLORREF tint);
//  v1.64.0 (درمان پلاس): draw an embedded RCDATA PNG (the brand logo) cover-
//  fitted into a CIRCLE that fits `rc`. Returns false if GDI+/resource is off so
//  callers can fall back to a vector glyph.
bool gpDrawImageResCircle(HDC dc, int resId, RECT rc);
bool gpDrawImageFileCircle(HDC dc, const std::wstring& path, RECT rc);
// v1.20.0: aspect-fit an image (file path OR data:base64 URI) into a rect.
bool gpDrawImageRectAny(HDC dc, const std::wstring& src, RECT rc);
// v1.23.0: render an image with explicit object-fit + padding so the print
// engine and the designer preview produce IDENTICAL output. The image is hard-
// clipped to (rc minus padding) and can NEVER overflow.
//   fit: 0=contain (aspect-fit, no crop) 1=cover (fill+crop) 2=fill (stretch)
//   padPx: inner padding in device pixels applied on every side
bool gpDrawImageRectFit(HDC dc, const std::wstring& src, RECT rc, int fit, int padPx);
//  RCDATA ids of the print-action raster icons (see app.rc):
#define IMG_IC_PRINTER 201
#define IMG_IC_RECEIPT 202
#define IMG_IC_SHIELD  203
#define IMG_IC_LAST    204
//  v1.8.0: header settings (gear) + calculator raster icons.
#define IMG_IC_SETTINGS 205
#define IMG_IC_CALC     206
//  v1.64.0 (درمان پلاس): the circular brand logo (RCDATA 207).
#define IMG_LOGO        207
//  Crisp anti-aliased line / circle helpers used by the new header clock etc.
void gpLine(HDC dc, int x1,int y1,int x2,int y2, COLORREF col, float w, int alpha=255);

// ------------------------------------------------------------------- time --
SYSTEMTIME   iranNow();                                  // UTC+3:30 (fixed; no DST since 2022)
void         gregToJalali(int gy,int gm,int gd,int&jy,int&jm,int&jd);
std::wstring jalaliDateStr(const SYSTEMTIME& st);        // e.g. سه‌شنبه ۲۰ خرداد ۱۴۰۵
std::wstring jalaliDateShort(const SYSTEMTIME& st);      // 1405/03/20
std::wstring iranTimeStr(const SYSTEMTIME& st, bool seconds);
//  v1.4.0: single canonical Persian Jalali formatter. Returns the date for a
//  UTC time_t as «۱۴۰۵/۰۴/۰۲» using Persian-Indic digits with RTL-safe marks.
//  Every date label in the app must route through this helper. Pass 0 for "now".
std::wstring FormatJalaliPersian(time_t utc);
//  Jalali Y/M/D string for *today* in Tehran, as "YYYY/MM/DD" (ASCII digits) —
//  used as the per-day key for appointment counters.
std::wstring JalaliTodayKey();
std::wstring toFaDigits(const std::wstring& s);
int          iranMinutesOfDay();
int          detectShift();                              // 0=صبح 1=عصر 2=شب
std::wstring shiftName(int s);
// v2.01 (Part B): validation utilities
bool validateNationalId(const std::wstring& nid);
bool validateMobile(const std::wstring& mob);
bool validateEmail(const std::wstring& email);

// ------------------------------------------------------------------ utils --
std::wstring exeDir();
std::wstring dataDir();      // <exe>\data   (auto-created)
std::wstring logsDir();      // <exe>\logs   (auto-created)
void         writeSchemaVersion();   // §I: stamp data\.schema_version (informational only)
void         logLine(const std::wstring& s);
// v1.82.0: real failures only (not gated by AZ_DEBUG_LOGS). Writes logs\errors.log.
void         logError(const std::wstring& s);
// v1.92.0: dedicated HTML/CSS/JS error log — only front-end bugs/crashes/load
// failures, never normal activity. Writes logs\html errors\errors.log.
void         logHtmlError(const std::wstring& msg);
// §J: record a flow breadcrumb (last 32 are dumped into the crash report).
void         Breadcrumb(const wchar_t* what);
std::wstring formatMoney(long long v);                   // 1,234,567
long long    parseMoney(const std::wstring& s);
std::wstring trim(const std::wstring& s);
bool         writeFileUtf8(const std::wstring& path, const std::wstring& text, bool append);
std::wstring readFileUtf8(const std::wstring& path);

// --------------------------------------------------------------- settings --
std::wstring getSetting(const std::wstring& key, const std::wstring& def);
void         setSetting(const std::wstring& key, const std::wstring& val);

// --------------------------------------------------------------- handlers --
void installCrashHandler();   // crash-handler: dump + log + friendly message
void detectSpec();            // speed-handler: sets g_lowSpec
void installVazirFont();      // embed-load + install on user system if missing

// ------------------------------------------------------------- setup splash --
// First-run / every-run prerequisite preparation with a visible progress bar.
// Verifies + installs the things the hybrid (MSHTML) reception surface needs to
// work on *this* client machine: data/log folders, the Vazirmatn font, the
// FEATURE_BROWSER_EMULATION registry key (so Trident runs in IE11 standards
// mode instead of IE7-quirks), and a live MSHTML availability probe. Returns
// true when the environment is ready for the hybrid UI; false means the native
// fallback form will be used. Shows the bar only when there is real work to do
// (first run, or after a version bump), otherwise returns instantly.
bool RunSetupSplash(HINSTANCE hInst);

// ------------------------------------------------------------------ users --
// Role constants (kept as plain ints for ABI stability with stored data).
#define ROLE_RECEPTION 0
#define ROLE_ADMIN     1
struct User {
    std::wstring username, fullname, dept, hash;
    int role;                 // 0 = پذیرش/پرسنل, 1 = مدیریت
    int id;                   // stable per-user id (index into users store)
    // v1.79.0: comma-separated permission keys (column 6 of users.dat).
    // EMPTY = full access (back-compat: accounts created before v1.79 keep
    // everything). Known keys: "admission" (پذیرش بیمار),
    // "worklist" (کارتابل), "cashier_view" (دیدن صندوق),
    // "cashier_edit" (تغییر در صندوق), "settings" (تنظیمات).
    // Legacy "cashier" still means both view and edit.
    std::wstring perms;
    // v2.01 (Part F2): assigned work shift. -1 = not assigned (falls back to
    // detectShift() at login). 0=صبح, 1=عصر, 2=شب, or a custom shift index.
    int shift;
    // §H forward-compat: any extra pipe-delimited columns written by a FUTURE
    // version (fields 8,…) are captured verbatim here and written back
    // unchanged, so an older build never silently drops newer data.
    std::wstring extra;
    User():role(0),id(0),shift(-1){}
};
// v1.79.0: does this account hold the given permission key? Empty perms = yes
// (legacy full access). Management accounts (role>=1) always pass.
bool userHasPerm(const User& u, const wchar_t* key);
std::vector<User> loadUsers();
bool addUser(const User& u, std::wstring& err);
bool removeUser(const std::wstring& username);
bool setUserFullName(const std::wstring& username, const std::wstring& fullname); // §5
// v1.70.0: edit an existing user (fullname/dept/role) and optionally reset its
// password (when newPassword is non-empty). Used by the HTML CRM employees page.
bool updateUserAccount(const std::wstring& username, const std::wstring& fullname,
                       const std::wstring& dept, int role,
                       const std::wstring& newPassword, std::wstring& err);
// v1.79.0: update ONLY the permission keys of an account (CRM accounts page)
bool setUserPerms(const std::wstring& username, const std::wstring& perms,
                  std::wstring& err);
bool verifyLogin(const std::wstring& u, const std::wstring& p,
                 int wantRole, User& out, std::wstring& err);
std::wstring hashPassword(const std::wstring& p);

// --------------------------------------------------------- work shifts (F2) --
// v2.01 (Part F2): managed shift definitions. The three built-in shifts
// (0=صبح, 1=عصر, 2=شب) are always present; custom shifts take id ≥ 3.
// startMin/endMin are minutes-from-midnight; endMin < startMin means the
// shift crosses midnight (e.g. شب 22:30→06:00). Stored in data\shiftdefs.dat.
struct ShiftDef {
    int          id;         // 0,1,2 built-in; 3+ custom
    std::wstring name;       // display name (صبح / عصر / شب / custom)
    int          startMin;   // start time, minutes from midnight
    int          endMin;     // end time (may be < startMin if crosses midnight)
    ShiftDef():id(-1),startMin(0),endMin(0){}
};
std::vector<ShiftDef> loadShiftDefs();
bool saveShiftDefs(const std::vector<ShiftDef>& defs);
int  addShiftDef(const std::wstring& name, int startMin, int endMin);
bool deleteShiftDef(int id);
bool setUserShift(const std::wstring& username, int shift, std::wstring& err);
std::wstring shiftDisplayName(int idx);
void shiftDefHours(int idx, int& startMin, int& endMin);
int  shiftIdByStoredName(const std::wstring& storedName);

// -------------------------------------------------------------- insurance --
struct InsuranceDef { const wchar_t* name; int pct; };
extern const InsuranceDef INSURANCES[];     extern const int N_INSURANCES;
extern const InsuranceDef SUPP_INSURANCES[];extern const int N_SUPP;
// v1.55.0 — coverage percentage for a base / supplementary insurance INDEX.
// Returns -1 when the index is out of range (⇒ the {ins_percent} /
// {supp_percent} print tokens resolve to an empty string, never a fake 0٪).
int Ins_Percent(int idx);
int Supp_Percent(int idx);

// v1.74 — user-managed insurance DEFINITIONS (base + supplementary). A registry
// of the full contract / tariff / franchise / colour metadata the «تعریف بیمه»
// page edits. Stored in data\insdefs.dat / data\suppdefs.dat (pipe-delimited,
// one record per line, UTF-8). `idx` is the stable key: for the predefined
// Iranian insurances it matches the position in INSURANCES[] / SUPP_INSURANCES[]
// so existing patient records keep their meaning; user-added rows take the next
// free idx. Ins_Percent/Supp_Percent honour a definition's orgShare when present
// (else fall back to the hardcoded table), so an edited سهم سازمان flows to the
// printed percent + billing spots that use those lookups — no random/test data.
struct InsDef {
    int          idx;            // stable key (aligns with INSURANCES[] for predefined)
    std::wstring sectionCode;    // بخش
    std::wstring insCode;        // کد بیمه
    int          orgShare;       // سهم سازمان (٪) — -1 = inherit hardcoded pct
    std::wstring groupName;      // نام گروه
    std::wstring flipName;       // نام وارونه
    std::wstring contractCode;   // کد قرارداد
    std::wstring insType;        // دولتی / خصوصی
    long long    tech;           // مبلغ فنی
    long long    prof;           // مبلغ حرفه‌ای
    long long    cons;           // مبلغ مصرفی
    int          active;
    std::wstring created, modified;
    InsDef():idx(-1),orgShare(-1),tech(0),prof(0),cons(0),active(1){}
};
struct SuppDef {
    int          idx;            // stable key (aligns with SUPP_INSURANCES[])
    std::wstring sectionCode;    // بخش
    std::wstring insSpec;        // مشخصات بیمه
    std::wstring name;           // نام بیمه تکمیلی
    std::wstring tariffType;     // نوع تعرفه
    std::wstring franchise;      // فرانشیز (دستی/پیش‌فرض)
    int          franchiseDefault; // پیش‌فرض دستی در پذیرش (1/0)
    int          byLaw;          // محاسبه بر اساس قانون بیمه (1/0)
    long long    ceiling;        // مبلغ سقف
    std::wstring insTypeCode;    // کد نوع بیمه
    std::wstring contractCode;   // کد قرارداد
    long long    tech, prof, cons;       // فنی / حرفه‌ای / مصرفی
    int          franchiseOrgPct;        // درصد سهم سازمان در فرانشیز
    int          defaultOff;     // بدون فرانشیز اگر بیمه پایه پذیرفته شد (1/0)
    std::wstring priceCalcType;  // دولتی / خصوصی (پیش‌فرض خصوصی)
    int          difference;     // اختلاف (پیش‌فرض خاموش)
    std::wstring color;          // رنگ (#rrggbb)
    // table columns
    std::wstring validityDate;   // تاریخ اعتبار
    std::wstring username;       // نام کاربری
    std::wstring nationalId;     // کد ملی (الزامی)
    int          booklet;        // دفترچه (1/0)
    std::wstring fileName;       // نام فایل
    int          active;
    std::wstring created, modified;
    SuppDef():idx(-1),franchiseDefault(1),byLaw(0),ceiling(0),tech(0),prof(0),cons(0),
              franchiseOrgPct(0),defaultOff(0),difference(0),booklet(0),active(1){}
};
std::vector<InsDef>  loadInsDefs();
void  InsDefs_SeedDefaults();
bool  upsertInsDef(const InsDef& d);
bool  deleteInsDef(int idx);
const InsDef*  insDefByIndex(int idx);
std::vector<SuppDef> loadSuppDefs();
bool  upsertSuppDef(const SuppDef& d);
bool  deleteSuppDef(int idx);
const SuppDef* suppDefByIndex(int idx);

// -------------------------------------------------------------- tariffs ----
//  Default service tariffs (Rial) so the program computes the bill itself.
//  Indexed by patient-visit type: 0=عادی 1=سرپایی 2=بستری.
extern const long long VISIT_TARIFF[3];
//  Surcharge multipliers for appointment type: 0=عادی 1=اورژانس 2=پرسنلی
long long applyApptTariff(long long base, int apptType);
//  Returns the default service price for a given patient + appointment type.
long long defaultServicePrice(int patientType, int apptType);

// -------------------------------------------------------------- reception --
// §1.51.0: a single billed service carried alongside the reception record so
// the print pipeline (PIT_SERVICES) can render a dynamic services list.
struct ServiceLine {
    std::wstring code;     // service code
    std::wstring name;     // service name (as shown to the user)
    std::wstring category; // optional category / dept label
    // v1.55.0: «شرح خدمت» — the descriptive text of the service, resolved from
    // the Service Management catalogue (ServiceDef::desc, falling back to
    // ServiceDef::category). Printed in the middle column of the receipt's
    // services table (نام خدمت | شرح خدمت | تعداد). Never randomised — when the
    // catalogue has no description this stays EMPTY and prints as "—".
    std::wstring desc;
    long long    price;    // unit price (Toman)
    int          qty;      // quantity (>=1)
    long long    discount; // per-line discount (Toman)
    long long    insShare; // insurance share for this line (Toman)
    long long    patShare; // patient share for this line (Toman)
    // v2.08: کد ملی سلامت (national health code) from the service catalog,
    // carried on the line so the receipt detail table can show it without a
    // separate catalog lookup. Empty when the catalog has no national code.
    std::wstring natCode;
    ServiceLine():price(0),qty(1),discount(0),insShare(0),patShare(0){}
};
struct ReceptionRecord {
    std::wstring firstName, lastName, nationalId, fatherName, birthDate,
                 gender, mobile, landline, address, patientType,
                 insurance, insuranceType, suppInsurance, apptDate, apptTime,
                 shift, dept, userName;
    // §1.53.0 (Bug D/F): a dedicated treating-doctor name captured from the
    // reception form. {doctor} resolves to this in preference to `dept`, and
    // falls back to `dept` when empty (backward-compatible with §1.52.0).
    std::wstring treatingDoctor;
    // §1.53.0 (Bug D): optional clinical / vitals fields, filled from the
    // reception "علائم حیاتی / اطلاعات تکمیلی" inputs when present. Empty by
    // default so templates that reference them print cleanly (hidden when
    // visibility==1). insNo/insExp hold the insurance booklet no & expiry.
    std::wstring weight, height, bp, temp, pulse, allergy, diagnosis,
                 refDoctor, nextVisit, insNo, insExp;
    // v1.55.0 — REAL-RECEIPT fields captured at admission so the print designer
    // can bind them. Every one of these is filled from live form/session data;
    // nothing here is ever generated randomly. Empty means "not entered", and a
    // bound field simply prints blank (or is hidden when visibility==1).
    std::wstring doctorCode;      // کد پزشک معالج (as typed in the doctor-code box)
    std::wstring performerCode;   // کد انجام‌دهنده
    std::wstring performer;       // نام انجام‌دهنده
    std::wstring specialty;       // شرح تخصص (from the doctors store)
    std::wstring specialtyCode;   // کد تخصص (short group letter/code)
    std::wstring receptionist;    // نام پذیرش‌کننده (full name of logged-in user)
    std::wstring cashierName;     // نام صندوق‌دار
    std::wstring scNum;           // ش.ص — cashier sheet/shift document number
    std::wstring receiptBarcode;  // بارکد اختصاصی رسید (deterministic, from receiptNo)
    std::wstring receiptCode;      // کد کوتاه رسید (deterministic alnum tag)
    std::wstring eprescription;   // کد رهگیری نسخهٔ الکترونیک (as entered)
    std::wstring referralNo;      // شماره معرفی‌نامه (as entered)
    std::wstring regStamp;        // تاریخ و ساعت ثبت پذیرش (Jalali date + hh:mm:ss)
    std::wstring apptSec;         // ساعت نوبت با ثانیه (hh:mm:ss)
    int  insPercent;              // درصد بیمهٔ پایه (-1 = unknown/none)
    int  suppPercent;             // درصد بیمهٔ مکمل (-1 = unknown/none)
    long long cash;               // مبلغ نقدی
    long long pos;                // مبلغ کارتخوان (POS)
    long long receiptNo;          // شمارهٔ قبض/رسید (0 = not assigned yet)
    long long total, mainShare, patientShare, baseDiff, orgShare,
              finalTotal, discount, paid;
    int queueNo, insIdx, suppIdx;
    std::vector<ServiceLine> services;   // §1.51.0: billed services list
    // v2.07.0 — additional receipt fields. All additive, appended at the END
    // of the member list (existing serialization order is untouched).
    std::wstring certNo;        // شماره شناسنامه — as entered; empty when not captured
    std::wstring receiptTitle;  // لیبل معرف رسید (e.g. «رسید بیمه تکمیلی»)
    std::wstring clinicName, clinicAddr, clinicPhone; // resolved from settings at print time
    ReceptionRecord():insPercent(-1),suppPercent(-1),cash(0),pos(0),receiptNo(0),
        total(0),mainShare(0),patientShare(0),baseDiff(0),
        orgShare(0),finalTotal(0),discount(0),paid(0),queueNo(0),
        insIdx(0),suppIdx(0){}
};
int  saveReception(ReceptionRecord& r);          // assigns queue no, persists CSV
int  countTodayReceptions();
// Count today's receptions for one treating doctor. `paidOnly` restricts the
// result to records with a positive paid amount. Exact doctor-name matching is
// used so similarly named physicians never share counters.
int  countTodayDoctorReceptions(const std::wstring& doctor, bool paidOnly);
void saveLastReceipt(const ReceptionRecord& r);
bool loadLastReceipt(ReceptionRecord& r);

// --------------------------------------------------------------- printing --
// kind: 0 = رسید بیمه  1 = نسخه  2 = قبض
bool printReceipt(const ReceptionRecord& r, int kind, HWND owner);
bool printLastReceipt(HWND owner);

// --------------------------------------------------------------- blacklist --
// File-backed patient blocking. National ID is the only matching key.
struct BlacklistEntry {
    std::wstring nid, first, last, father, mobile, reason, durationLabel;
    std::wstring createdText, createdBy;
    long long createdEpochMin, expiresEpochMin; // expires==0 means permanent
    BlacklistEntry():createdEpochMin(0),expiresEpochMin(0){}
};
std::vector<BlacklistEntry> Blacklist_Load();
bool Blacklist_Add(const BlacklistEntry& entry, std::wstring& err);
bool Blacklist_FindActive(const std::wstring& nationalId, BlacklistEntry& out);
std::vector<BlacklistEntry> Blacklist_Search(const std::wstring& query);
//  v1.64.0 (درمان پلاس): permanently remove every entry for a national id.
int  Blacklist_Remove(const std::wstring& nationalId);
void Blacklist_AuditOverride(const BlacklistEntry& entry,
                             const std::wstring& operatorName);
long long Blacklist_NowEpochMinutes();

// ---------------------------------------------------------------- session --
struct Session {
    User user;
    int  shift;          // chosen at login, never auto-revoked
    SYSTEMTIME loginAt;
    // v1.79.0: the header's second identity line (مقام/سمت) resolved ONCE at
    // login — WM_PAINT must never hit persons.dat / emp_*.dat on every clock
    // tick (the timer repaints the header band up to twice a second).
    std::wstring title;
};
//  resolve the logged-in person's مقام/سمت once (personnel record → employee
//  profile → access-level fallback); called at login, cached in session.title
std::wstring resolveSessionTitle(const User& u);
extern Session g_session;

// ---------------------------------------------------------------- screens --
enum ScreenId { SC_HOME=0, SC_RECEPTION=1, SC_ADMIN=2, SC_MANAGE=3, SC_ACCOUNTING=4 };
void switchScreen(ScreenId id);
RECT frameContentRect();                 // area between top & bottom bars

HWND createHomeScreen(HWND frame);       // main.cpp
HWND createReceptionScreen(HWND frame);  // reception.cpp
HWND createAdminScreen(HWND frame);      // admin.cpp
HWND createManageScreen(HWND frame);     // admin.cpp
HWND createAccountingScreen(HWND frame); // accounting.cpp

// v1.7.0: header→reception action routing. The frame header (main.cpp) owns
// the «پذیرش بیمار» / «تب جدید» buttons and routes them to the
// active reception screen via these helpers. RA_* names the requested action.
//  v1.60.0: «نوبت‌دهی» (RA_APPOINTMENT) removed — the feature no longer exists.
enum RecAction { RA_NEWPAT=0, RA_NEWTAB=2 };
enum RecPrintAction { RPA_INSURANCE=0, RPA_RX=1, RPA_LAST=2 };
HWND receptionWindow();                  // the live reception HWND (or NULL)
void receptionAction(RecAction a);       // route a header action to reception
void receptionPrintAction(RecPrintAction a); // native bottom-bar print action
void Reception_OpenTools();
void Reception_OpenCashier();
void Reception_OpenQueue();
void Reception_OpenReceipts();
void Reception_OpenPortal();
void Reception_OpenBlacklist();
void Reception_OpenSvReport();   // v2.02: «تفکیک خدمات» report
void Reception_OpenLabAnswer();  // v2.08: «جوابدهی تک بیمار» lab answer
void Reception_CloseLabAnswer();
void Reception_OpenLabAnswerTicket(const std::string& ticketJson);
void Reception_CloseSvReport();
void Reception_OpenNewTab();
void Reception_OpenAdmissionNew();
void Reception_OpenAdmissionTicket(const std::string& ticketJson);
void Reception_CloseTools();
void Reception_CloseCashier();
void Reception_CloseQueue();
void Reception_CloseReceipts();
void Reception_ClosePortal();
void Reception_ResetZoom();
void Reception_CycleTab();   // v2.05: Ctrl+Tab — next C++ tab (RTL wrap)

// ---------------------------------------------------------------- dialogs --
// role: 0 پذیرش / 1 مدیریت / 2 admin (hidden, prf)
bool showLoginDialog(HWND parent, int role, User& out);
// v2.01: showShiftDialog removed (Part F1) — shift comes from the user account.
// profile edit (name + photo) — submits a ProfReq for management approval.
// returns true if the user pressed «تأیید» and a request was queued.
bool showProfileDialog(HWND parent);

// ------------------------------------------------------------- calculator --
void openCalculator(HWND owner);

// ------------------------------------------------------------- settings ----
//  v1.3.0: a slide-over "settings" panel styled like a social-network profile
//  page (avatar, identity card, then grouped option rows). Opened from the
//  gear button in the top header. Hosts: theme switch, check-for-update,
//  density, auto-print toggle, server URL, about.
void openSettingsPanel(HWND frameOwner);
bool settingsPanelVisible();
void closeSettingsPanel();

// ------------------------------------------------------ v1.4.0 settings tiers
//  Role dispatcher: reception users get OpenReceptionSettings, admins get
//  OpenManagementSettings (§1.1). The gear icon routes here.
void OpenSettings(HWND hMain, const User& u);
void OpenReceptionSettings(HWND hMain, const User& u);
void OpenManagementSettings(HWND hMain, const User& u);

// ------------------------------------------------- v1.4.0 header collapse (§6)
//  A small state machine animates the reception header's action bar between
//  fully-expanded (factor 1.0) and collapsed (factor 0.0). The frame queries
//  HeaderCollapse_Factor() while laying out the header; HeaderCollapse_Set()
//  starts an animation toward the target; HeaderCollapse_Tick() advances it and
//  returns true while still animating (so the frame keeps the timer alive).
#define HEADER_COLLAPSE_TIMER 0xC0A1
void  HeaderCollapse_Set(HWND frame, bool collapsed);  // begin animating
bool  HeaderCollapse_Tick(HWND frame);                 // advance; true if more
float HeaderCollapse_Factor();                         // 0.0..1.0 (1 = expanded)
bool  HeaderCollapse_Collapsed();                      // current target state

// ----------------------------------------------------------------- update --
void checkRemoteUpdate(HWND owner);      // remote-update over HTTP(S)

// ------------------------------------------------------------- print system --
//  v1.4.0: printer configuration + a full print-layout DESIGNER.
//  Sections (each prints differently): پذیرش / تزریقات / آزمایشگاه …
extern const wchar_t* PRINT_SECTIONS[];  extern const int N_PRINT_SECTIONS;
//  Open the printer-settings dialog (default printer, test, paper size,
//  fit/fill, advanced) — persisted in settings.
void openPrinterSettings(HWND owner);
//  v2.07.0: «ارتباط با چاپگر» — dedicated printer picker (searchable list,
//  Windows-default pre-selection, «پیرو پیش‌فرض ویندوز» switch, test page,
//  reload) opened from the C++ settings header.
void PrinterLink_Open(HWND owner);
//  v2.08.0: «تنظیمات فونت و بزرگنمایی» — font family / size / weight / zoom
//  picker opened from the C++ settings header. Persists to settings.ini and
//  live-applies to the admission WebView.
void FontZoom_Open(HWND owner);
std::vector<std::wstring> EnumSystemFonts();
//  Open the visual print designer for a given section index.
void openPrintDesigner(HWND owner, int sectionIdx);
//  Render the saved design for a section onto a printer DC for a real receipt.
//  Returns false if no design exists (caller falls back to the classic layout).
bool printDesignedReceipt(const ReceptionRecord& r, int sectionIdx, HWND owner);
//  §1.19.0 — Render the new-model (print_designer.h PrintDesign) design bound
//  to a *section id* onto the connected printer. The HTML/CSS/JS designer and
//  the native designer both persist a PrintDesign per section. Returns false if
//  no such design exists so the caller can fall back to printDesignedReceipt /
//  printReceipt. First call (per session, no saved printer) shows the standard
//  print dialog so the operator picks printer + paper (A4/A5).
bool printPrintDesign(const ReceptionRecord& r, int sectionId, HWND owner);
// v2.07 §7.5: print a SPECIFIC design (زیربخش پذیرش barcode-only TB1 route)
// regardless of the machine/section binding.
bool printPrintDesignWith(const ReceptionRecord& r, const struct PrintDesign& forced, HWND owner);
//  Pulse the cash drawer connected to the configured printer (ESC/POS kick),
//  but only when the «باز کردن کشوی پول» option is enabled in printer settings.
void kickCashDrawer();

// --------------------------------------------------------------- employees --
//  Department categories + employee directory (management panel).
struct DeptCat { std::wstring id, name, manager, icon;
    // §H forward-compat: extra pipe columns from a newer version, kept verbatim
    // (already pipe-prefixed + escaped) so a save round-trip never drops them.
    std::wstring extra; };
std::vector<DeptCat> loadDepts();
bool addDept(const DeptCat& c, std::wstring& err);
bool removeDept(const std::wstring& id);
void seedDefaultDepts();   // ensure the «پذیرش» default category exists
//  Extended employee profile (beyond the login User record).
//  v1.8.0: added empId (auto/manual personnel code), uniqueId (system-unique
//  identifier, auto/manual), position/title, mobile, email, hireDate and
//  workHours (weekly hours) so the new-employee form is complete.
struct EmpProfile {
    std::wstring username, nationalId, fatherName, address, landline,
                 shiftFrom, shiftTo, photoPath, idCardPath, deptId;
    std::wstring empId, uniqueId, position, mobile, email, hireDate, workHours;
    // §H forward-compat: unknown key=value lines written by a FUTURE version are
    // captured here (already including their trailing CRLF) and re-emitted on
    // save so older builds never silently drop newer profile fields.
    std::wstring extraKv;
};
EmpProfile loadEmpProfile(const std::wstring& username);
void       saveEmpProfile(const EmpProfile& p);
bool       isUserOnline(const std::wstring& username);   // session presence (heartbeat <90s)
void       setUserOnline(const std::wstring& username, bool on);
void       heartbeatUser(const std::wstring& username);  // §G: refresh presence on a timer

// --------------------------------------------------------------- persons ----
//  v1.79.0: personnel registry («تعریف پرسنل»). A PERSON exists independently
//  of any login account — the account is attached later in «تعریف حساب
//  کاربری». Keyed by کد پرسنلی (personnel code), auto-generated from the
//  owning department's name (first two letters, Persian→Latin) + a running
//  number (آزمایشگاه → AZ_0001, تزریقات → TZ_…, پذیرش → PZ_…) or entered
//  manually. A person with no department is «در حالت تعلیق» (suspended).
struct PersonDef {
    std::wstring code;        // کد پرسنلی (unique) e.g. AZ_0001 / PER_0001
    std::wstring firstName, lastName, fatherName, nationalId;
    std::wstring birthDate;   // تاریخ تولد (Jalali text, free-form)
    std::wstring address, mobile, phone, email;
    std::wstring education;   // مدرک تحصیلی
    std::wstring field;       // شاخه/رشته تحصیلی
    std::wstring degree;      // عنوان دقیق مدرک
    int         roleKind;     // 0=پرسنل 1=پزشک 2=پرستار 3=کارآموز 4=سایر
    std::wstring roleCustom;  // نقش آزاد وقتی roleKind==4
    std::wstring position;    // مقام/سمت — توی هدر برنامه بعد از ورود نمایش داده می‌شود
    //  v1.80.0: the person's بخش/زیربخش reference the CLINICAL sections tree
    //  (sections.dat — the same «بخش‌ها و زیربخش‌ها» the manager defines in the
    //  CRM). deptId = section id (as digits), subId = زیربخش id (empty = the
    //  person works directly under the section). BOTH empty = در حالت تعلیق.
    //  (v1.79.0 stored a DeptCat id here; that registry was brand-new so the
    //  switch is safe — unresolvable legacy ids simply display raw.)
    std::wstring deptId;      // Section id — empty = در حالت تعلیق
    std::wstring subId;       // زیربخش (Section with parent_id=deptId) — optional
    std::wstring photo;       // مسیر نسبی عکس (data\persons\photos\<code>.<ext>)
    std::wstring username;    // حساب کاربری مرتبط (empty = هنوز حساب ندارد)
    std::wstring created;     // تاریخ ایجاد (Jalali)
    std::wstring extraKv;     // §H forward-compat: unknown key=value lines
    PersonDef():roleKind(0){}
};
std::vector<PersonDef> loadPersons();
bool addPerson(PersonDef& p, std::wstring& err);      // auto-fills p.code when empty
bool updatePerson(const PersonDef& p, std::wstring& err);
bool removePerson(const std::wstring& code);
bool personByCode(const std::wstring& code, PersonDef& out);
bool personByUsername(const std::wstring& username, PersonDef& out);
//  پیشنهاد کد بعدی برای یک بخش/زیربخش (PREFIX_####) یا برای پرسنل بدون بخش
//  (PER_####) — پیشوند از نام زیربخش اگر هست، وگرنه نام بخش (v1.80.0)
std::wstring nextPersonCode(const std::wstring& deptId, const std::wstring& subId=L"");
//  پیشوند لاتین دوحرفی از روی نام بخش (فارسی هم پوشش داده می‌شود)
std::wstring deptCodePrefix(const std::wstring& deptName);
//  نقش متنی از روی roleKind/roleCustom
std::wstring personRoleLabel(const PersonDef& p);
//  عکس پرسنلی: ذخیره باینری زیر data\persons\photos\ و خواندن آن (برای نمایش
//  در صفحه CRM به‌صورت data URL از طریق پل)
std::wstring personPhotoDir();
bool savePersonPhoto(const std::wstring& code, const std::string& bytes,
                     const std::wstring& ext, std::wstring& relOut);
bool loadPersonPhoto(const std::wstring& relPath, std::string& bytesOut,
                     std::wstring& mimeOut);

// ------------------------------------------------------------- services --
//  Clinic services managed from the «مدیریت خدمات» (Service Management) page.
//  Saved to data/services.dat (one pipe-delimited line per service, UTF-8).
//  Admission picks services from this list; the price ALWAYS comes from here —
//  the admission operator never types a price.
struct ServiceDef {
    std::wstring code;      // کد خدمت (unique)
    std::wstring name;      // نام خدمت
    std::wstring category;  // دسته/گروه (kept on disk for back-compat; UI retired v1.74)
    std::wstring dept;      // بخش (department id or name)
    long long    price;     // مبلغ پایه (ریال) — kept as the legacy base price
    std::wstring insType;   // نوع بیمه (legacy free text; UI retired v1.74)
    std::wstring desc;      // توضیحات
    int          status;    // 1=فعال 0=غیرفعال
    std::wstring created;   // تاریخ ایجاد (Jalali)
    std::wstring modified;  // تاریخ ویرایش (Jalali)
    // v1.74 — professional service definition. insName is the base-insurance
    // name (matches an entry the insurance page manages); multiplier is a raw
    // decimal string ("1.5") so the UI/billing parse it identically. The six
    // tariff columns hold the current + «new» Rial prices for آزاد / دولتی /
    // بیمه tariffs. They are appended AFTER `modified` and BEFORE the §H
    // forward-compat `extra` catch-all, so older 10-column files load unchanged.
    std::wstring insName;   // نام بیمه (base insurance name)
    std::wstring multiplier;// ضریب (decimal string, e.g. "1.5")
    long long    priceFree;     // نرخ آزاد (current)
    long long    priceFreeNew;  // نرخ آزاد جدید (next/current alternative — billing fallback)
    long long    priceGov;      // نرخ دولتی (current)
    long long    priceGovNew;   // نرخ دولتی جدید (next/current alternative — billing fallback)
    long long    priceIns;      // قیمت بیمه
    long long    priceInsNew;   // قیمت بیمه جدید
    // v2.01 (Part C) — new service definition fields:
    std::wstring shortName;     // نام اختصار (required)
    std::wstring lovingCode;    // کد لوینگ (free-form, optional)
    std::wstring equivCode;     // کد معادل
    std::wstring serviceId;     // شناسه خدمت
    std::wstring revenueGroup;  // گروه درآمد
    std::wstring healthNationalId; // کد ملی سلامت
    int          sectionId;     // بخش/زیربخش id (links to sections.dat tree)
    // v2.01 (Part C3) — historical rates. When the user changes «نرخ آزاد» or
    // «نرخ دولتی», the previous value is moved here automatically. These are
    // informational only — billing ALWAYS uses the current rates above.
    long long    priceFreeOld;  // نرخ آزاد قدیم (historical, not used for billing)
    long long    priceGovOld;   // نرخ دولتی قدیم (historical, not used for billing)
    std::wstring extra;     // §H forward-compat: unknown trailing columns
    ServiceDef():price(0),status(1),priceFree(0),priceFreeNew(0),
                 priceGov(0),priceGovNew(0),priceIns(0),priceInsNew(0),
                 sectionId(0),priceFreeOld(0),priceGovOld(0){}
};
std::vector<ServiceDef> loadServices();
bool  addService(const ServiceDef& s, std::wstring& err);   // insert (code must be unique)
bool  updateService(const ServiceDef& s, std::wstring& err);// edit by code
bool  removeService(const std::wstring& code);
bool  saveAllServices(const std::vector<ServiceDef>& v);    // v1.74: bulk persist (round/adjust)
const ServiceDef* findService(const std::wstring& code);    // NULL when not found

// ------------------------------------------------------------------ kartabl --
//  Cartable / inbox messages pushed from management to a user (or broadcast).
//  v1.4.1: messages carry a severity TYPE so the cartable can colour-code them:
//    0 = عادی   (green / safe)
//    1 = فوری   (yellow / warning)
//    2 = بحرانی (red / error)
enum { KMSG_NORMAL=0, KMSG_URGENT=1, KMSG_CRITICAL=2 };
struct KMsg { std::wstring from, to, text, time; bool seen; int type; bool pinned;
    KMsg():seen(false),type(0),pinned(false){} };
std::vector<KMsg> loadMessages(const std::wstring& forUser);
//  legacy 3-arg push (type defaults to عادی) and the new typed push.
void  pushMessage(const std::wstring& from, const std::wstring& to,
                  const std::wstring& text);
void  pushMessageT(const std::wstring& from, const std::wstring& to,
                   const std::wstring& text, int type);
int   unseenMessageCount(const std::wstring& forUser);
void  markMessagesSeen(const std::wstring& forUser);

// ------------------------------------------------- settings-change requests --
//  v1.4.1: when a reception workstation changes printer / design settings, a
//  change-request record is written so management sees who / which system /
//  what changed (with date+time) under a red notification badge.
//  v1.9.0: the request now carries an APPROVAL workflow. Settings are NOT
//  applied immediately — they are queued here and only applied after management
//  approves. Fields:
//    user|system|change|profile|time|seen|status|payload|title
//  status: 0=pending 1=approved 2=rejected.  payload = the pending setting(s)
//  (key=value;key=value) that are applied verbatim on approval. title = short
//  human title (e.g. «تغییر نوع چاپگر»). preview = optional preview text/path.
struct SetReq {
    std::wstring user, system, change, profile, time, payload, title, preview;
    bool seen;
    int  status;
    SetReq():seen(false),status(0){}
};
std::vector<SetReq> loadSetReqs();
//  legacy 4-arg push (kept for source compatibility — queues with no payload).
void  pushSetReq(const std::wstring& user, const std::wstring& system,
                 const std::wstring& change, const std::wstring& profile);
//  v1.9.0: full approval-aware push. Returns nothing; the change is applied
//  ONLY when management later approves it (setSetReqStatus).
void  pushSetReqEx(const std::wstring& user, const std::wstring& system,
                   const std::wstring& title, const std::wstring& change,
                   const std::wstring& payload, const std::wstring& preview);
//  Approve (1) / reject (2) the i-th request (newest-first). On approval the
//  payload key=value pairs are written to settings; on rejection an inbox
//  message «درخواست شما توسط مدیریت رد شد.» is delivered to the requester.
void  setSetReqStatus(int indexNewestFirst, int status, const std::wstring& reason);
void  markOneSetReqSeen(int indexNewestFirst);   // mark a single request read
void  deleteSetReq(int indexNewestFirst);        // remove a request entirely
int   unseenSetReqCount();
int   pendingSetReqCount();
void  markSetReqsSeen();
//  v1.9.0: the local network/system identity used to stamp requests (computer
//  name; falls back to a stored id). Shown to management as the request source.
std::wstring systemSourceName();

// ------------------------------------------------- saved (archived) messages --
//  v1.8.0: when «پیام‌های ذخیره‌شده» (Saved Messages) is enabled in settings, a
//  message can be archived to permanent local storage with its text and any
//  downloadable attachments preserved. Stored in data\saved_msgs.dat:
//      from|to|time|type|attachPath|text
struct SavedMsg { std::wstring from, to, time, attachPath, text; int type; bool seen;
    SavedMsg():type(0),seen(false){} };
std::vector<SavedMsg> loadSavedMsgs();
void  pushSavedMsg(const std::wstring& from, const std::wstring& to,
                   const std::wstring& text, int type,
                   const std::wstring& attachPath);
int   savedMsgCount();
bool  savedMsgsEnabled();                 // settings flag "saved_msgs_enabled"
//  v1.8.0: department-targeted message helper (records the dept/route in `to`).
//  Attachments (image/video/gif/png/jpg/word/…) are copied into a local
//  attachments folder and the stored path lets the recipient download them.
std::wstring copyAttachmentLocal(const std::wstring& srcPath);  // returns stored path or L""
//  v1.9.0: a SECOND, always-on saved-messages store that is strictly LOCAL to
//  this machine/user and is NEVER transmitted across the network. Both the
//  employee and the management side keep their own personal note board here:
//  the user can type text notes and attach images. Stored per-user in
//  data\local_notes_<user>.dat:  time|attachPath|text
struct LocalNote { std::wstring time, attachPath, text; };
std::vector<LocalNote> loadLocalNotes(const std::wstring& forUser);
void  pushLocalNote(const std::wstring& forUser, const std::wstring& text,
                    const std::wstring& attachPath);
void  deleteLocalNote(const std::wstring& forUser, int indexNewestFirst);
int   localNoteCount(const std::wstring& forUser);

// ------------------------------------------------------------- notifications --
//  v1.9.0: a lightweight Windows toast/balloon notification. Used so that ONLY
//  the recipients of a management message see «شما یک پیام جدید دارید.» — the
//  manager who sent it does NOT get the notification back.
void  showWindowsNotification(const std::wstring& title, const std::wstring& body);
//  Deliver a message AND fire the recipient notification on every recipient
//  workstation (but not the sender). Returns recipients count.
void  notifyNewMessageRecipients();   // checks the pending flag for THIS user

// ----------------------------------------------------------------- backup -----
//  v1.9.0: management backup / restore of patient data. Designed to read very
//  large (~15 GB) Matin-Teb (.bak) backups WITHOUT freezing the UI by scanning
//  in a background thread and streaming. A scan returns a category breakdown
//  with estimated sizes; restore can apply the full backup or a selected
//  subset (e.g. only patient information).
struct BackupCategory {
    std::wstring id;        // stable id ("patients","images",...)
    std::wstring name;      // Persian display name
    long long    bytes;     // estimated size in bytes
    long long    records;   // estimated record count (0 if N/A)
    bool         selected;  // user tick for selective restore
    BackupCategory():bytes(0),records(0),selected(false){}
};
//  Opaque scan result. Filled by the background scanner.
struct BackupInfo {
    std::wstring path;
    long long    totalBytes;
    std::vector<BackupCategory> cats;
    bool         ready;     // scan complete
    BackupInfo():totalBytes(0),ready(false){}
};
//  Open the full management backup manager page (modal over the frame).
void  openBackupManager(HWND owner);

// ----------------------------------------------------------- appointment ----
//  v1.60.0: the نوبت‌دهی (appointment) module has been REMOVED entirely from
//  the reception account — tab, page, routing and its appointment.cpp module
//  are gone. The reception date/shift fields on the admission form are kept
//  (they are part of the reception record itself).

// edit subclass: Enter / Tab => next field
void enableEnterNavigation(HWND ctl);

// edit subclass: smart Jalali date mask — user types digits only, slashes are
// inserted automatically as YYYY/MM/DD (also keeps Enter/Tab navigation).
void enableDateMask(HWND ctl);

// edit subclass: automatic RTL/LTR alignment based on the typed content —
// Persian/Arabic text aligns RIGHT (RTL), Latin/digits-only aligns LEFT.
void enableAutoDir(HWND ctl);

// ----------------------------------------------------- national registry ----
//  v1.6.0: a deterministic offline simulation of the Iranian Civil Registry
//  (ثبت احوال) + insurance enquiry.  validNationalId() does the real Iranian
//  10-digit checksum; lookupCitizen() derives a stable, realistic identity
//  (name, father, gender, birth date, mobile) from the code so the whole
//  reception / appointment workflow runs end-to-end with NO external API and
//  is ready to be swapped for a real web-service call later.
//  v1.7.0: identity is NEVER fabricated. `found` means a TRUSTED source returned
//  a verified record (an online ثبت احوال web-service, or a locally-stored
//  patient previously registered by an operator). When no trusted source can
//  verify the code, `found` stays false and the UI must let the operator type
//  the identity MANUALLY — it must not invent a name, gender, birth-date or
//  insurance. `source` says where the data came from so the UI can show it.
enum CitizenSource {
    CS_NONE = 0,   // nothing known — manual entry required
    CS_LOCAL,      // recalled from a patient previously registered here
    CS_REGISTRY    // returned by a configured online registry web-service
};
struct CitizenInfo {
    bool        found;          // true ONLY when a trusted source verified it
    int         source;         // CitizenSource
    bool        idValid;        // the 10-digit checksum is valid
    bool        lookupTried;    // an online lookup was attempted
    bool        lookupFailed;   // the online lookup failed/was unavailable
    std::wstring firstName, lastName, fatherName, gender, birthDate, mobile;
    std::wstring landline;   // v1.40.0 — تلفن ثابت (persisted in the local store)
    std::wstring address;    // v1.40.0 — آدرس (persisted in the local store)
    std::vector<int> insurances;   // INSURANCES[] indices VERIFIED for this person
    int         suppIdx = -1;      // v1.40.0 — index into SUPP_INSURANCES[] (-1 = none)
    CitizenInfo():found(false),source(CS_NONE),idValid(false),
        lookupTried(false),lookupFailed(false){}
};
bool         validNationalId(const std::wstring& id);
CitizenInfo  lookupCitizen(const std::wstring& nationalId);
//  Persist a verified/manually-confirmed patient locally so the SAME national
//  code recalls the SAME real identity next time (no fabrication, no randomness).
//  v1.40.0: the signature grew two trailing text columns (landline «تلفن ثابت»
//  and address «آدرس») plus a supplementary-insurance index, so the reception
//  national-ID auto-fill can recall EVERY field the operator entered before.
//  The two new text parameters and `suppIdx` default so pre-1.40 call sites
//  keep compiling, but every in-tree caller has been updated to pass them.
void         rememberPatient(const std::wstring& nationalId,
                 const std::wstring& firstName, const std::wstring& lastName,
                 const std::wstring& fatherName, const std::wstring& gender,
                 const std::wstring& birthDate, const std::wstring& mobile,
                 const std::wstring& landline,        // v1.40.0 — تلفن ثابت
                 const std::wstring& address,         // v1.40.0 — آدرس
                 const std::vector<int>& insurances,
                 int suppIdx = -1);                   // v1.40.0 — SUPP_INSURANCES[] index

//  v1.10.0: a flat, read-only view of one row in the local patient store, used
//  by the admin «بیماران» (patients) tab. Mirrors the data\patients.dat schema.
//  v1.40.0: schema grew to 11 columns —
//      nid|first|last|father|gender|birth|mobile|insCsv|landline|address|suppIdx
struct PatientRow {
    std::wstring nid, first, last, father, gender, birth, mobile;
    std::wstring landline;   // v1.40.0 — تلفن ثابت
    std::wstring address;    // v1.40.0 — آدرس
    std::vector<int> insurances;
    int          suppIdx = -1;   // v1.40.0 — index into SUPP_INSURANCES[]
};
//  Load every patient stored locally (newest first — same order the store is
//  written: appended records last, so we reverse to show newest on top).
std::vector<PatientRow> loadAllPatients();
//  Delete a patient record by national code from the local store. Returns true
//  if a row was removed.
bool                    deletePatient(const std::wstring& nationalId);

// ----------------------------------------------- patient import pipeline ----
//  v1.12.0 (§11-13): a dedup-aware bulk import path that feeds the SAME local
//  patient store the reception national-ID auto-fill reads from. Records can
//  originate from an offline-staged CSV (Path B) extracted from a restored SQL
//  Server database, or from the in-app analyzer staging. Matching is by the
//  10-digit national code (the clinical primary key): an incoming row with a
//  code already on file UPDATES that record (newer wins) instead of creating a
//  duplicate; rows with an invalid/empty code are skipped and counted.
struct ImportPatientRow {
    std::wstring nid, first, last, father, gender, birth, mobile;
    std::vector<int> insurances;
};
struct ImportResult {
    int total=0;        // rows seen in the source
    int inserted=0;     // brand-new national codes added
    int updated=0;      // existing codes refreshed (dedup match)
    int skippedInvalid=0;// rows with an invalid/empty national code
    int skippedEmpty=0; // rows with no usable name
    std::wstring error; // non-empty on hard failure (file unreadable etc.)
    bool ok=false;
};
//  Bulk-import a vector of rows into the local store with national-ID dedup.
ImportResult importPatients(const std::vector<ImportPatientRow>& rows);
//  Parse a staged import file (UTF-8/UTF-16). Auto-detects the delimiter
//  (| , ; or TAB) and a header row. Expected columns (header names matched
//  case-insensitively, English or Persian):
//      national_id/کدملی, first/نام, last/خانوادگی, father/پدر,
//      gender/جنسیت, birth/تولد, mobile/موبایل, insurance/بیمه
//  Columns may also be positional in the canonical order above. Never throws.
std::vector<ImportPatientRow> parsePatientImportFile(const std::wstring& path,
                                                     std::wstring& parseError);
//  Convenience: parse + import a staged file in one call (Path B offline).
ImportResult importPatientsFromFile(const std::wstring& path);

// ----------------------------------------------------------- doctors --------
//  Doctors & their services for the appointment screen (file-backed, seeded
//  with realistic Iranian specialties so the workflow is usable out-of-box).
//  v1.69.0: expanded doctor record with full clinical/contract/accounting
//  fields for the new «پزشکان» management page. Backward-compatible: the old
//  3-field pipe format (name|specialty|services) is still parsed; missing
//  fields default to empty. New saves use the multi-line key=value format.
struct DoctorDef {
    // identity
    std::wstring name, specialty;
    std::vector<std::wstring> services;
    // v1.69.0 extended fields
    std::wstring deptId;        // بخش (links to a section/department)
    int  docType;               // 0=پزشک, 1=پرستار
    std::wstring docCode;       // کد پزشک
    bool active;                // فعال / غیرفعال
    // personal info
    std::wstring medicalId;     // کد نظام پزشکی
    std::wstring namePrefix;    // پیشوند نام (دکتر/پزشک/...)
    std::wstring firstName;     // نام
    std::wstring lastName;      // نام خانوادگی
    std::wstring nationalId;    // کد ملی
    std::wstring mobile;        // تلفن همراه
    std::wstring email;         // ایمیل
    std::wstring address;       // آدرس
    // contract / accounting
    std::wstring franchise;     // فرانشیز
    bool printOnReceipt;        // چاپ در قبض
    std::wstring insSpecialty;  // تخصص بیمه
    std::wstring degree;        // مدرک
    int  contractType;          // نوع قرار داد (0=... custom text below)
    std::wstring emergencyContract; // قرار پزشک اورژانس
    std::wstring accounting;    // حسابداری (free text)
    // v1.78.0: «تعریف به عنوان انجام دهنده» — وقتی فعال است (پیش‌فرض برای همه)،
    // این نیرو در فهرست «انجام دهنده» فرم پذیرش می‌آید؛ با غیرفعال‌کردن، فقط به‌عنوان
    // پزشک معالج (ارجاع‌دهنده) قابل جستجو می‌ماند. کلید isPerformer= در doctors.dat؛
    // نبودنش در فایل‌های قدیمی یعنی true (سازگاری رو‌به‌عقب).
    bool isPerformer;
    // v2.01 (Part B) — complementary options (three checkboxes):
    bool showOnSiteList;       // امکان نمایش در لیست پزشکان سایت
    bool onlineAppointment;    // امکان نوبت‌دهی اینترنتی
    bool isAnesthesiologist;   // پزشک بیهوشی می‌باشد
    DoctorDef():docType(0),active(true),printOnReceipt(true),contractType(0),
                isPerformer(true),showOnSiteList(true),onlineAppointment(false),
                isAnesthesiologist(false){}
};
std::vector<DoctorDef> loadDoctors();          // seeds defaults if empty
std::vector<DoctorDef> todaysDoctors();        // doctors on shift today
bool saveDoctors(const std::vector<DoctorDef>& doctors); // v1.69.0

// ----------------------------------------------------- profile-change reqs --
//  v1.6.0: a full profile-change request workflow (reception → management).
//  data\profreq.dat: user|oldName|newName|oldPhoto|newPhoto|time|status|reason
//   status: 0=pending 1=approved 2=rejected
struct ProfReq {
    std::wstring user, oldName, newName, oldPhoto, newPhoto, time, reason;
    int status;
    ProfReq():status(0){}
};
std::vector<ProfReq> loadProfReqs();
void pushProfReq(const ProfReq& r);
void setProfReqStatus(int indexNewestFirst, int status, const std::wstring& reason);
int  unseenProfReqCount();      // pending count (for the manager badge)

// ----------------------------------------------------------- cartable v2 ----
//  Message actions: pin / seen / delete, all reported back to the manager.
void  pinMessage(const std::wstring& forUser, int indexNewestFirst, bool pin);
void  seenOneMessage(const std::wstring& forUser, int indexNewestFirst);
void  deleteOneMessage(const std::wstring& forUser, int indexNewestFirst);
