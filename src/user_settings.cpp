// ============================================================================
//  user_settings.cpp — messenger-style settings window (release 1.11.0 §A).
//
//  The settings window is modelled on a modern messenger settings page:
//    • a top-center circular avatar (user photo, or a tinted vector initial,
//      or a generic silhouette for a guest),
//    • an identity block (full name + role — department),
//    • a vertical stack of full-width TAPPABLE CARD ROWS (icon · title ·
//      subtitle · right chevron) with hover/pressed states.
//
//  Navigation is push/pop WITHIN THE SAME WINDOW (no new top-level windows):
//  clicking a row pushes a dedicated sub-page; a back button on the top-left of
//  every sub-page pops it. The stack is `SettingsWin::pageStack`.
//
//  Audiences (SettingsMode) and the row matrix are governed by a SINGLE SOURCE
//  OF TRUTH: canAccess(row, session). Every row-click handler consults it; a
//  row the session cannot access is neither shown NOR actionable. A debug-build
//  startup self-check asserts the full row×role matrix matches the spec.
//
//  Guest contract (SM_GUEST): EXACTLY two rows — «تغییر پوسته» + «ارتباط با ما».
//  Nothing else is shown and no other page is reachable; if the stack ever
//  lands on a forbidden page while role==guest, we immediately pop back.
//
//  Forward-compat (§H): we only ever READ the settings keys we render and write
//  them through setSetting (which preserves unknown keys/comments). No data file
//  is rewritten as a side-effect of opening settings.
// ============================================================================
#include "app.h"
#include "ui_kit.h"
#include "sections.h"
#include "print_designer.h"
#include "net_sync.h"
#include "backup_log.h"
#include "web_admission.h"
#include <objbase.h>
#include <vector>
#include <string>
#include <cassert>

void OpenProfileRequestsInbox(HWND owner);   // §5 (profile_requests.cpp)
void OpenBackupLogViewer(HWND owner);        // §7.2 (backup_log_viewer.cpp)
void SavedMessages_Show(HWND owner);         // §F (saved_messages.cpp)
void openBackupManager(HWND owner);          // backup.cpp
void checkRemoteUpdate(HWND owner);          // update.cpp

// ---------------------------------------------------------------- model ------
namespace {

// §A: the three audiences. Kept as an explicit mode so future tiers never
// silently regress the guest contract (guest = theme + contact only).
enum SettingsMode { SM_GUEST=0, SM_RECEPTION=1, SM_ADMIN=2 };

// §A.3: every selectable settings destination. canAccess() is keyed on these.
enum SettingsRow {
    ROW_PROFILE = 0,   // edit profile sub-page
    ROW_THEME,         // theme picker (light/dark)
    ROW_RECEPTION,     // per-user reception interface settings
    ROW_BLACKLIST,     // v1.58: patient blacklist — now its OWN page (moved
                       //         out of the reception/appearance settings)
    ROW_DESIGNER,      // print designer hub
    ROW_BACKUP,        // backup & restore
    ROW_EMP_SECT,      // employees / sections (admin)
    ROW_SAVED_MSG,     // saved messages
    ROW_UPDATE,        // update
    ROW_CONTACT,       // contact us
    ROW_ABOUT,         // about
    ROW_LOGOUT,        // logout
    ROW__COUNT
};

// §A.3 SINGLE SOURCE OF TRUTH. Returns whether `row` is reachable for `mode`.
// EVERY row-click handler MUST consult this; a false row is neither rendered
// nor actionable. Guest sees ONLY theme + contact.
static bool canAccess(int row, int mode){
    if(mode==SM_GUEST){
        return row==ROW_THEME || row==ROW_CONTACT;
    }
    // Reception + Admin share the option set; admin-only rows are gated.
    // v1.93: «تنظیمات پذیرش» (ROW_RECEPTION) and «پیام‌های ذخیره» (ROW_SAVED_MSG)
    // are no longer surfaced here — reception settings move to the HTML portal
    // and saved messages open from the portal's hamburger menu. The enum values
    // are retained (see SettingsRow) so existing indices never shift; the rows
    // are simply unreachable.
    switch(row){
    case ROW_PROFILE:   return true;
    case ROW_THEME:     return true;
    case ROW_RECEPTION: return false;
    case ROW_BLACKLIST: return false;
    case ROW_DESIGNER:  return true;
    case ROW_BACKUP:    return mode==SM_ADMIN;   // admin-only — removed from «حساب پرسنل»
    case ROW_EMP_SECT:  return mode==SM_ADMIN;   // admin-only
    case ROW_SAVED_MSG: return false;
    case ROW_UPDATE:    return true;
    case ROW_CONTACT:   return true;
    case ROW_ABOUT:     return true;
    case ROW_LOGOUT:    return true;
    default:            return false;
    }
}

// §A.3 debug self-check: assert the row×role matrix matches the documented
// spec. Runs once at startup in debug builds only (no cost in release).
static void selfCheckMatrix(){
#ifdef AZ_DEBUG_BUILD
    // Guest: EXACTLY theme + contact.
    for(int r=0;r<ROW__COUNT;r++){
        bool g = canAccess(r,SM_GUEST);
        bool want = (r==ROW_THEME || r==ROW_CONTACT);
        assert(g==want && "guest row matrix mismatch");
    }
    // Reception: everything except employees/sections and backup. v1.93: also skip
    // ROW_RECEPTION and ROW_SAVED_MSG, which are now intentionally unreachable
    // (moved to the HTML portal) so the matrix self-check stays accurate.
    // v1.94: ROW_BLACKLIST joins the same skip set — the patient blacklist
    // moves to the HTML dashboard and is unreachable from here.
    // v2.01: ROW_BACKUP is admin-only — «پشتیبان‌گیری و بازیابی» removed from
    // «حساب پرسنل» as it is an administrative function a «پذیرشگر» must not see.
    assert(!canAccess(ROW_EMP_SECT,SM_RECEPTION));
    assert( canAccess(ROW_EMP_SECT,SM_ADMIN));
    assert(!canAccess(ROW_BACKUP,SM_RECEPTION));
    assert( canAccess(ROW_BACKUP,SM_ADMIN));
    for(int r=0;r<ROW__COUNT;r++){
        if(r==ROW_EMP_SECT || r==ROW_RECEPTION || r==ROW_SAVED_MSG
           || r==ROW_BLACKLIST || r==ROW_BACKUP) continue;
        assert(canAccess(r,SM_RECEPTION) && "reception should access this row");
        assert(canAccess(r,SM_ADMIN)     && "admin should access this row");
    }
#endif
}

// Page ids for the push/pop stack. PAGE_HOME is the card-row list; the rest are
// dedicated sub-pages. (Some map 1:1 onto a SettingsRow; others are leaf pages.)
enum PageId {
    PAGE_HOME = 0,
    PAGE_PROFILE, PAGE_THEME, PAGE_RECEPTION, PAGE_BLACKLIST,
    PAGE_DESIGNER, PAGE_BACKUP, PAGE_EMP_SECT,
    PAGE_SAVED_MSG, PAGE_UPDATE, PAGE_CONTACT, PAGE_ABOUT
};

struct RowDef {
    int          row;        // SettingsRow
    int          page;       // PageId to push (0 = handled inline)
    int          icon;       // IconId
    const wchar_t* title;
    const wchar_t* sub;      // optional subtitle (may be NULL)
};

struct SettingsWin {
    User              user;
    int               mode;          // SettingsMode
    std::vector<int>  pageStack;     // navigation stack (top = current page)
    std::vector<HWND> ctrls;         // controls of the current page
    HWND              hwnd  = NULL;
    HWND              hMain = NULL;
    int               hotRow = -1;   // hovered card-row index (home page)
    int               downRow = -1;  // pressed card-row index
    bool              backHot = false, backDown = false;
    // §2: top-right close button hover/pressed state (present on every page).
    bool              closeHot = false, closeDown = false;
    // §1.12.0: full-page settings now scroll. `scrollY` is the current vertical
    // offset (px, >=0); `contentH` is the laid-out content height of the page.
    int               scrollY = 0;
    int               contentH = 0;
    // v1.68.0 PERF: cache of the static frame (outer rails + sheet shadow +
    // sheet fill + side edges). These never change between paints unless the
    // window resizes or the theme switches, yet gpShadow(spread 10) was issuing
    // ~10 full-panel GDI+ path fills on EVERY WM_PAINT — including the frequent
    // hover-driven repaints. Building them once into a backing DC and blitting
    // per-paint removes that cost from the hot path.
    HDC               bgDC = NULL;
    HBITMAP           bgBmp = NULL;
    int               bgW = 0, bgH = 0;
};

#define IDC_BACK_BTN   6900
#define IDC_PANEL_BASE 7100

static void destroyCtrls(SettingsWin* sw){
    for(HWND c : sw->ctrls) if(c && IsWindow(c)) DestroyWindow(c);
    sw->ctrls.clear();
}
static int curPage(SettingsWin* sw){
    return sw->pageStack.empty()? PAGE_HOME : sw->pageStack.back();
}

// ---------------------------------------------------------- row table --------
// The home page row list for the current mode (filtered by canAccess).
static std::vector<RowDef> homeRows(int mode){
    static const RowDef ALL[] = {
        { ROW_PROFILE,   PAGE_PROFILE,   ICO_USER,
          L"\u0648\u06cc\u0631\u0627\u06cc\u0634 \u067e\u0631\u0648\u0641\u0627\u06cc\u0644",          // ویرایش پروفایل
          L"\u0646\u0627\u0645\u060c \u062a\u0644\u0641\u0646\u060c \u0639\u06a9\u0633\u060c \u0631\u0645\u0632" },   // نام، تلفن، عکس، رمز
        { ROW_THEME,     PAGE_THEME,     ICO_PALETTE,
          L"\u062a\u063a\u06cc\u06cc\u0631 \u067e\u0648\u0633\u062a\u0647",                            // تغییر پوسته
          L"\u0631\u0648\u0634\u0646 \u0648 \u0645\u0634\u06a9\u06cc \u0648 \u0646\u0626\u0648\u0646\u06cc" },                    // روشن و مشکی و نئونی
        { ROW_DESIGNER,  PAGE_DESIGNER,  ICO_PRINT,
          L"\u0637\u0631\u0627\u062d\u06cc \u0686\u0627\u067e",                                        // طراحی چاپ
          L"\u0637\u0631\u0627\u062d\u06cc \u0642\u0627\u0644\u0628 \u0686\u0627\u067e \u0628\u0631\u0627\u06cc \u0647\u0631 \u0628\u062e\u0634" },
        { ROW_BACKUP,    PAGE_BACKUP,    ICO_SHIELD,
          L"\u067e\u0634\u062a\u06cc\u0628\u0627\u0646\u200c\u06af\u06cc\u0631\u06cc \u0648 \u0628\u0627\u0632\u06cc\u0627\u0628\u06cc",   // پشتیبان‌گیری و بازیابی
          L"\u062a\u062d\u0644\u06cc\u0644\u060c \u0648\u0631\u0648\u062f\u060c \u062a\u0627\u0631\u06cc\u062e\u0686\u0647" },
        { ROW_EMP_SECT,  PAGE_EMP_SECT,  ICO_PEOPLE,
          L"\u06a9\u0627\u0631\u0645\u0646\u062f\u0627\u0646 \u0648 \u0628\u062e\u0634\u200c\u0647\u0627",  // کارمندان و بخش‌ها
          L"\u0645\u062f\u06cc\u0631\u06cc\u062a \u06a9\u0627\u0631\u0645\u0646\u062f\u0627\u0646 \u0648 \u0628\u062e\u0634\u200c\u0647\u0627" },
        { ROW_UPDATE,    PAGE_UPDATE,    ICO_UPDATE,
          L"\u0628\u0647\u200c\u0631\u0648\u0632\u0631\u0633\u0627\u0646\u06cc",                         // به‌روزرسانی
          NULL },
        { ROW_CONTACT,   PAGE_CONTACT,   ICO_PHONE,
          L"\u0627\u0631\u062a\u0628\u0627\u0637 \u0628\u0627 \u0645\u0627",                            // ارتباط با ما
          NULL },
        { ROW_ABOUT,     PAGE_ABOUT,     ICO_INFO,
          L"\u062f\u0631\u0628\u0627\u0631\u0647 \u0628\u0631\u0646\u0627\u0645\u0647",                  // درباره برنامه
          NULL },
        { ROW_LOGOUT,    0,              ICO_LOGOUT,
          L"\u062e\u0631\u0648\u062c \u0627\u0632 \u062d\u0633\u0627\u0628",                            // خروج از حساب
          NULL },
    };
    std::vector<RowDef> out;
    for(const auto& r : ALL) if(canAccess(r.row, mode)) out.push_back(r);
    return out;
}

// ---------------------------------------------------------- geometry ---------
// §2: profile circle is slightly SMALLER than before (was S(96)) and sits a
// touch LOWER (see paintAvatar) so it reads as a deliberate, balanced header
// rather than hugging the top edge.
static int g_scaleAvatar(){ return S(84); }
// §2: vertical drop applied to the profile circle so it sits slightly lower in
// the header. The identity block (name/role) is anchored to the circle's bottom
// so it stays visually connected and moves down together.
static int avatarTopDrop(){ return S(22); }
// §1.12.0: the settings window is now a FULL-PAGE surface. To keep a clean,
// modern look on wide monitors the actual content lives in a CENTERED COLUMN of
// at most COLW px; the rest of the page is the flat #bg surface (no background
// bleed from the screen behind — this is a top-level opaque window). The column
// also carries the vertical scroll offset (sw->scrollY) so both the owner-drawn
// rows AND the HWND child controls move together when scrolling.
static int colW(){ return S(720); }
static RECT contentRect(SettingsWin* sw){
    RECT rc; GetClientRect(sw->hwnd,&rc);
    int w = colW(); if(w > rc.right - S(24)) w = rc.right - S(24);
    int cx = rc.right/2;
    RECT c;
    c.left   = cx - w/2;
    c.right  = cx + w/2;
    c.top    = S(8) - (sw ? sw->scrollY : 0);   // scroll offset applied here
    c.bottom = rc.bottom - S(8);
    return c;
}
// content-column rect WITHOUT the scroll offset — used to compute total height
// and to clamp scrolling. (top is the unscrolled origin.)
static RECT contentRectUnscrolled(SettingsWin* sw){
    RECT c=contentRect(sw);
    if(sw) c.top += sw->scrollY;
    return c;
}
static int rowH(){ return S(70); }
static int rowGap(){ return S(10); }
// y of the first card-row on the home page (below the avatar + identity block).
// §2: recomputed for the lower + smaller circle so the option rows keep a clean,
// consistent gap below the identity block (avatar drop + diameter + name + role).
static int homeRowsTop(){ return avatarTopDrop() + S(84) + S(30) + S(54); }
static RECT homeRowRect(SettingsWin* sw, int idx){
    RECT c=contentRect(sw);
    int top = c.top + homeRowsTop() + idx*(rowH()+rowGap());
    RECT r={ c.left+S(8), top, c.right-S(8), top+rowH() };
    return r;
}
// back button (top-LEFT, since this is an RTL app the visual "back" sits left).
// The back button does NOT scroll — it stays pinned to the top of the page.
static RECT backBtnRect(SettingsWin* sw){
    RECT c=contentRectUnscrolled(sw);
    RECT r={ c.left+S(6), S(14), c.left+S(6)+S(40), S(14)+S(36) };
    return r;
}
// §2: close button — pinned to the TOP-RIGHT of the content column (the visual
// "close" corner). It never scrolls and is present on every settings page so
// the operator can always dismiss the page with one click.
static RECT closeBtnRect(SettingsWin* sw){
    RECT c=contentRectUnscrolled(sw);
    int sz=S(36);
    RECT r={ c.right-S(6)-sz, S(14), c.right-S(6), S(14)+sz };
    return r;
}

// ---------------------------------------------------------- sub-pages --------
//  Each sub-page builds its own child controls into sw->ctrls and paints its
//  title in WM_PAINT. They are intentionally compact reuses of the prior
//  panel logic so no functionality regresses.

// Sub-pages reserve a pinned header band (back button + title) of S(52) at the
// very top of the page; their scrollable content begins below it. Because
// contentRect().top already carries the scroll offset, adding the header band
// here makes child controls scroll under the pinned header.
static int subHeaderH(){ return S(52); }
static int subTop(SettingsWin* sw){ return contentRect(sw).top + subHeaderH(); }

static void buildProfilePage(SettingsWin* sw){
    RECT c=contentRect(sw);
    int x=c.left+S(20), y=subTop(sw), w=c.right-c.left-S(40);
    HWND capName=CreateWindowExW(0,L"STATIC",
        L"\u0646\u0627\u0645 \u062c\u062f\u06cc\u062f",WS_CHILD|WS_VISIBLE|SS_RIGHT|WS_CLIPSIBLINGS,
        x,y,w,S(18),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(capName,WM_SETFONT,(WPARAM)g_fSmall,TRUE); sw->ctrls.push_back(capName);
    HWND eName=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",sw->user.fullname.c_str(),
        WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|WS_CLIPSIBLINGS,x,y+S(20),w,S(30),sw->hwnd,
        (HMENU)(INT_PTR)(IDC_PANEL_BASE+1),g_hInst,NULL);
    SendMessageW(eName,WM_SETFONT,(WPARAM)g_fUI,TRUE); sw->ctrls.push_back(eName);

    HWND capPhone=CreateWindowExW(0,L"STATIC",
        L"\u062a\u0644\u0641\u0646",WS_CHILD|WS_VISIBLE|SS_RIGHT|WS_CLIPSIBLINGS,
        x,y+S(60),w,S(18),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(capPhone,WM_SETFONT,(WPARAM)g_fSmall,TRUE); sw->ctrls.push_back(capPhone);
    HWND ePhone=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",
        getSetting(L"profile.phone",L"").c_str(),
        WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|WS_CLIPSIBLINGS,x,y+S(80),w,S(30),sw->hwnd,
        (HMENU)(INT_PTR)(IDC_PANEL_BASE+5),g_hInst,NULL);
    SendMessageW(ePhone,WM_SETFONT,(WPARAM)g_fUI,TRUE); sw->ctrls.push_back(ePhone);

    HWND bPhoto=createFlatButton(sw->hwnd,IDC_PANEL_BASE+2,
        L"\u062a\u063a\u06cc\u06cc\u0631 \u0639\u06a9\u0633",ICO_USER,BS_OUTLINE,
        x,y+S(122),w/2-S(6),S(32)); sw->ctrls.push_back(bPhoto);
    HWND bPass=createFlatButton(sw->hwnd,IDC_PANEL_BASE+6,
        L"\u062a\u063a\u06cc\u06cc\u0631 \u0631\u0645\u0632",ICO_SHIELD,BS_OUTLINE,
        x+w/2+S(6),y+S(122),w/2-S(6),S(32)); sw->ctrls.push_back(bPass);

    HWND bSubmit=createFlatButton(sw->hwnd,IDC_PANEL_BASE+4,
        (sw->mode==SM_ADMIN)? L"\u0630\u062e\u06cc\u0631\u0647 \u062a\u063a\u06cc\u06cc\u0631\u0627\u062a"
                            : L"\u062b\u0628\u062a \u062f\u0631\u062e\u0648\u0627\u0633\u062a",
        ICO_CHECK,BS_PRIMARY,x,y+S(166),w,S(36)); sw->ctrls.push_back(bSubmit);
}

static void buildThemePage(SettingsWin* sw){
    RECT c=contentRect(sw);
    int x=c.left+S(20), y=subTop(sw), w=c.right-c.left-S(40), gap=S(8);
    // v1.93: three themes — light (روشن), dark (مشکی), neon (نئونی).
    int third=(w-2*gap)/3;
    HWND bLight=createFlatButton(sw->hwnd,IDC_PANEL_BASE+10,
        L"\u0631\u0648\u0634\u0646",ICO_SUN,BS_OUTLINE,x+2*(third+gap),y,third,S(52));
    HWND bDark=createFlatButton(sw->hwnd,IDC_PANEL_BASE+11,
        L"\u0645\u0634\u06a9\u06cc",ICO_MOON,BS_OUTLINE,x+third+gap,y,third,S(52));
    HWND bNeon=createFlatButton(sw->hwnd,IDC_PANEL_BASE+12,
        L"\u0646\u0626\u0648\u0646\u06cc",ICO_PALETTE,BS_OUTLINE,x,y,third,S(52));
    sw->ctrls.push_back(bLight); sw->ctrls.push_back(bDark); sw->ctrls.push_back(bNeon);
}

static HWND addSettingsLabel(SettingsWin* sw,const wchar_t* text,int x,int y,int w){
    HWND h=CreateWindowExW(0,L"STATIC",text,
        WS_CHILD|WS_VISIBLE|SS_RIGHT|WS_CLIPSIBLINGS,x,y,w,S(20),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(h,WM_SETFONT,(WPARAM)g_fSmall,TRUE);
    sw->ctrls.push_back(h); return h;
}

static std::wstring settingsText(HWND parent,int id){
    HWND h=GetDlgItem(parent,id); if(!h) return L"";
    int n=GetWindowTextLengthW(h); std::vector<wchar_t> b((size_t)n+1,0);
    GetWindowTextW(h,b.data(),n+1); return trim(b.data());
}
static std::wstring settingsDigits(const std::wstring& s){
    std::wstring out;
    for(wchar_t c:s){
        if(c>=L'0'&&c<=L'9') out+=c;
        else if(c>=L'۰'&&c<=L'۹') out+=(wchar_t)(L'0'+c-L'۰');
        else if(c>=L'٠'&&c<=L'٩') out+=(wchar_t)(L'0'+c-L'٠');
    }
    return out;
}
static void refreshBlacklistHistory(SettingsWin* sw){
    HWND list=GetDlgItem(sw->hwnd,IDC_PANEL_BASE+94); if(!list) return;
    SendMessageW(list,LB_RESETCONTENT,0,0);
    std::vector<BlacklistEntry> rows=Blacklist_Search(settingsText(sw->hwnd,IDC_PANEL_BASE+93));
    long long now=Blacklist_NowEpochMinutes();
    for(const auto& r:rows){
        bool active=r.expiresEpochMin==0||r.expiresEpochMin>now;
        std::wstring line=(active?L"فعال | ":L"منقضی | ")+toFaDigits(r.nid)+L" | "+
            r.first+L" "+r.last+L" | "+r.reason+L" | "+r.durationLabel;
        SendMessageW(list,LB_ADDSTRING,0,(LPARAM)line.c_str());
    }
    if(rows.empty()) SendMessageW(list,LB_ADDSTRING,0,(LPARAM)L"موردی یافت نشد.");
}
static void lookupBlacklistPatient(SettingsWin* sw,bool quiet){
    std::wstring nid=settingsDigits(settingsText(sw->hwnd,IDC_PANEL_BASE+84));
    if(nid.empty()) return;
    for(const auto& p:loadAllPatients()){
        if(settingsDigits(p.nid)!=nid) continue;
        SetDlgItemTextW(sw->hwnd,IDC_PANEL_BASE+85,p.first.c_str());
        SetDlgItemTextW(sw->hwnd,IDC_PANEL_BASE+86,p.last.c_str());
        SetDlgItemTextW(sw->hwnd,IDC_PANEL_BASE+87,p.father.c_str());
        SetDlgItemTextW(sw->hwnd,IDC_PANEL_BASE+88,p.mobile.c_str());
        return;
    }
    if(!quiet) MessageBoxW(sw->hwnd,
        L"بیمار در سوابق محلی پیدا نشد؛ اطلاعات را دستی وارد کنید.",
        L"جستجوی بیمار",MB_OK|MB_ICONINFORMATION);
}

// v1.93: buildReceptionPage was REMOVED — «تنظیمات پذیرش» no longer has a
// native settings page; reception display settings are now managed from the
// HTML portal. The PAGE_RECEPTION id and ROW_RECEPTION enum are retained so
// existing indices/ids do not shift; the page simply renders nothing.

// v1.58 — the patient BLACKLIST is now a STANDALONE settings page (moved out of
// the reception / appearance settings, per request). Same controls & control
// ids as before, re-based to the top of its own page so it reads as a fully
// independent feature. WM_COMMAND handlers (add/lookup/search) are unchanged.
static void buildBlacklistPage(SettingsWin* sw){
    RECT c=contentRect(sw);
    int x=c.left+S(20), y=subTop(sw), w=c.right-c.left-S(40);
    addSettingsLabel(sw,L"ثبت مسدودی جدید",x,y,w);
    addSettingsLabel(sw,L"کد ملی",x+w*2/3,y+S(28),w/3-S(6));
    addSettingsLabel(sw,L"نام",x+w/3,y+S(28),w/3-S(6));
    addSettingsLabel(sw,L"نام خانوادگی",x,y+S(28),w/3-S(6));
    HWND nid=CreateWindowExW(WS_EX_CLIENTEDGE|WS_EX_RTLREADING,L"EDIT",L"",
        WS_CHILD|WS_VISIBLE|ES_RIGHT|ES_AUTOHSCROLL,x+w*2/3,y+S(50),w/3-S(6),S(30),sw->hwnd,
        (HMENU)(INT_PTR)(IDC_PANEL_BASE+84),g_hInst,NULL);
    HWND first=CreateWindowExW(WS_EX_CLIENTEDGE|WS_EX_RTLREADING,L"EDIT",L"",
        WS_CHILD|WS_VISIBLE|ES_RIGHT|ES_AUTOHSCROLL,x+w/3,y+S(50),w/3-S(6),S(30),sw->hwnd,
        (HMENU)(INT_PTR)(IDC_PANEL_BASE+85),g_hInst,NULL);
    HWND last=CreateWindowExW(WS_EX_CLIENTEDGE|WS_EX_RTLREADING,L"EDIT",L"",
        WS_CHILD|WS_VISIBLE|ES_RIGHT|ES_AUTOHSCROLL,x,y+S(50),w/3-S(6),S(30),sw->hwnd,
        (HMENU)(INT_PTR)(IDC_PANEL_BASE+86),g_hInst,NULL);
    addSettingsLabel(sw,L"نام پدر",x+w*2/3,y+S(86),w/3-S(6));
    addSettingsLabel(sw,L"موبایل",x+w/3,y+S(86),w/3-S(6));
    addSettingsLabel(sw,L"مدت مسدودی",x,y+S(86),w/3-S(6));
    HWND father=CreateWindowExW(WS_EX_CLIENTEDGE|WS_EX_RTLREADING,L"EDIT",L"",
        WS_CHILD|WS_VISIBLE|ES_RIGHT|ES_AUTOHSCROLL,x+w*2/3,y+S(108),w/3-S(6),S(30),sw->hwnd,
        (HMENU)(INT_PTR)(IDC_PANEL_BASE+87),g_hInst,NULL);
    HWND mobile=CreateWindowExW(WS_EX_CLIENTEDGE|WS_EX_RTLREADING,L"EDIT",L"",
        WS_CHILD|WS_VISIBLE|ES_RIGHT|ES_AUTOHSCROLL,x+w/3,y+S(108),w/3-S(6),S(30),sw->hwnd,
        (HMENU)(INT_PTR)(IDC_PANEL_BASE+88),g_hInst,NULL);
    HWND duration=CreateWindowExW(0,L"COMBOBOX",L"",
        WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,x,y+S(108),w/3-S(6),S(180),sw->hwnd,
        (HMENU)(INT_PTR)(IDC_PANEL_BASE+90),g_hInst,NULL);
    const wchar_t* durations[]={L"۲۴ ساعت",L"یک هفته",L"یک ماه",L"یک سال",L"دائم"};
    for(int i=0;i<5;i++) SendMessageW(duration,CB_ADDSTRING,0,(LPARAM)durations[i]);
    SendMessageW(duration,CB_SETCURSEL,0,0);
    HWND fields[]={nid,first,last,father,mobile,duration};
    for(HWND ctl:fields){ SendMessageW(ctl,WM_SETFONT,(WPARAM)g_fUI,TRUE); sw->ctrls.push_back(ctl); }
    addSettingsLabel(sw,L"علت مسدودی (الزامی)",x,y+S(144),w);
    HWND reason=CreateWindowExW(WS_EX_CLIENTEDGE|WS_EX_RTLREADING,L"EDIT",L"",
        WS_CHILD|WS_VISIBLE|ES_RIGHT|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL,
        x,y+S(166),w,S(72),sw->hwnd,(HMENU)(INT_PTR)(IDC_PANEL_BASE+89),g_hInst,NULL);
    SendMessageW(reason,WM_SETFONT,(WPARAM)g_fUIB,TRUE); sw->ctrls.push_back(reason);
    // v1.64.0 (درمان پلاس): the «تکمیل خودکار با کد ملی» button was removed;
    // «ثبت مسدودی» is now a full-width primary action. The flat buttons are
    // told the surface they sit on so their rounded corners blend cleanly and
    // the contrast guard keeps their labels legible.
    HWND add=createFlatButton(sw->hwnd,IDC_PANEL_BASE+91,L"ثبت مسدودی",
        ICO_SHIELD,BS_PRIMARY,x,y+S(246),w,S(40));
    setFlatButtonBg(add, g_theme.bg);
    sw->ctrls.push_back(add);
    addSettingsLabel(sw,L"جستجوی لحظه‌ای تاریخچه",x,y+S(300),w);
    HWND search=CreateWindowExW(WS_EX_CLIENTEDGE|WS_EX_RTLREADING,L"EDIT",L"",
        WS_CHILD|WS_VISIBLE|ES_RIGHT|ES_AUTOHSCROLL,x,y+S(314),w,S(30),sw->hwnd,
        (HMENU)(INT_PTR)(IDC_PANEL_BASE+93),g_hInst,NULL);
    HWND list=CreateWindowExW(WS_EX_CLIENTEDGE|WS_EX_RTLREADING,L"LISTBOX",L"",
        WS_CHILD|WS_VISIBLE|WS_VSCROLL|LBS_NOINTEGRALHEIGHT|LBS_NOTIFY,
        x,y+S(352),w,S(180),sw->hwnd,(HMENU)(INT_PTR)(IDC_PANEL_BASE+94),g_hInst,NULL);
    SendMessageW(search,WM_SETFONT,(WPARAM)g_fUI,TRUE);
    SendMessageW(list,WM_SETFONT,(WPARAM)g_fSmall,TRUE);
    sw->ctrls.push_back(search); sw->ctrls.push_back(list);
    // v1.64.0 (درمان پلاس): «رفع مسدودی» — remove every blacklist entry for a
    // national id so the patient can be admitted again. A textbox + primary
    // button; the history list refreshes after a successful removal.
    addSettingsLabel(sw,L"رفع مسدودی بیمار با کد ملی",x,y+S(544),w);
    HWND unNid=CreateWindowExW(WS_EX_CLIENTEDGE|WS_EX_RTLREADING,L"EDIT",L"",
        WS_CHILD|WS_VISIBLE|ES_RIGHT|ES_AUTOHSCROLL,x,y+S(568),w-S(150)-S(8),S(32),sw->hwnd,
        (HMENU)(INT_PTR)(IDC_PANEL_BASE+95),g_hInst,NULL);
    HWND unBtn=createFlatButton(sw->hwnd,IDC_PANEL_BASE+96,L"رفع مسدودی",
        ICO_CHECK,BS_PRIMARY,x+w-S(150),y+S(568),S(150),S(32));
    setFlatButtonBg(unBtn, g_theme.bg);
    SendMessageW(unNid,WM_SETFONT,(WPARAM)g_fUI,TRUE);
    SendMessageW(unNid,EM_SETCUEBANNER,TRUE,(LPARAM)L"کد ملی بیمار مسدود…");
    sw->ctrls.push_back(unNid); sw->ctrls.push_back(unBtn);
    refreshBlacklistHistory(sw);
}

static void buildContactPage(SettingsWin* sw){
    RECT c=contentRect(sw);
    int x=c.left+S(20), y=subTop(sw), w=c.right-c.left-S(40);
    std::wstring phone = getSetting(L"contact.phone", L"\u06f0\u06f2\u06f1-\u06f1\u06f2\u06f3\u06f4\u06f5\u06f6\u06f7\u06f8");
    std::wstring mobile= getSetting(L"contact.mobile",L"\u06f0\u06f9\u06f1\u06f2-\u06f3\u06f4\u06f5\u06f6\u06f7\u06f8\u06f9");
    std::wstring email = getSetting(L"contact.email", L"support@darmanplus.ir");
    std::wstring addr  = getSetting(L"contact.address",
        L"\u062a\u0647\u0631\u0627\u0646\u060c \u062f\u0631\u0645\u0627\u0646\u06af\u0627\u0647 \u062f\u0631\u0645\u0627\u0646 \u067e\u0644\u0627\u0633");
    std::wstring hours = getSetting(L"contact.hours",
        L"\u0634\u0646\u0628\u0647 \u062a\u0627 \u067e\u0646\u062c\u200c\u0634\u0646\u0628\u0647\u060c \u06f8 \u062a\u0627 \u06f2\u06f0");
    std::wstring body;
    body += L"\u0628\u0631\u0627\u06cc \u067e\u0634\u062a\u06cc\u0628\u0627\u0646\u06cc\u060c \u0627\u0646\u062a\u0642\u0627\u062f\u0627\u062a \u0648 \u067e\u06cc\u0634\u0646\u0647\u0627\u062f\u0627\u062a:\r\n\r\n";
    body += L"\u0640 \u062a\u0644\u0641\u0646 \u062b\u0627\u0628\u062a: " + toFaDigits(phone) + L"\r\n";
    body += L"\u0640 \u062a\u0644\u0641\u0646 \u0647\u0645\u0631\u0627\u0647: " + toFaDigits(mobile) + L"\r\n";
    body += L"\u0640 \u0631\u0627\u06cc\u0627\u0646\u0627\u0645\u0647: " + email + L"\r\n";
    body += L"\u0640 \u0646\u0634\u0627\u0646\u06cc: " + addr + L"\r\n";
    body += L"\u0640 \u0633\u0627\u0639\u0627\u062a \u067e\u0627\u0633\u062e\u06af\u0648\u06cc\u06cc: " + hours;
    HWND b=CreateWindowExW(0,L"STATIC",body.c_str(),
        WS_CHILD|WS_VISIBLE|SS_RIGHT|WS_CLIPSIBLINGS,x,y,w,S(200),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(b,WM_SETFONT,(WPARAM)g_fUI,TRUE); sw->ctrls.push_back(b);
    HWND bCopy=createFlatButton(sw->hwnd,IDC_PANEL_BASE+70,
        L"\u06a9\u067e\u06cc \u0627\u0637\u0644\u0627\u0639\u0627\u062a \u062a\u0645\u0627\u0633",ICO_RECEIPT,BS_OUTLINE,
        x,y+S(212),w,S(34)); sw->ctrls.push_back(bCopy);
}

static void buildAboutPage(SettingsWin* sw){
    RECT c=contentRect(sw);
    int x=c.left+S(20), y=subTop(sw), w=c.right-c.left-S(40);
    std::wstring body=L"\u062f\u0631\u0645\u0627\u0646 \u067e\u0644\u0627\u0633 \u2014 \u0646\u0633\u062e\u0647 ";
    body+=APP_VERSION_W; body+=L"\r\n";
    body+=L"\u0633\u0627\u0645\u0627\u0646\u0647 \u067e\u0630\u06cc\u0631\u0634 \u0648 \u0645\u062f\u06cc\u0631\u06cc\u062a \u062f\u0631\u0645\u0627\u0646\u06af\u0627\u0647\r\n";
    body+=L"\u0645\u062c\u0648\u0632: \u0627\u062e\u062a\u0635\u0627\u0635\u06cc \u00a9 DarmanPlus\r\n";
    HWND b=CreateWindowExW(0,L"STATIC",body.c_str(),WS_CHILD|WS_VISIBLE|SS_RIGHT|WS_CLIPSIBLINGS,
        x,y,w,S(160),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(b,WM_SETFONT,(WPARAM)g_fUI,TRUE); sw->ctrls.push_back(b);
}

// Leaf "launcher" pages — a single primary button that opens an existing
// feature window. (Designer/Backup/Emp open their own modal child windows.)
static void buildLauncherPage(SettingsWin* sw, const wchar_t* hint,
                              const wchar_t* btnText, int icon, int cmdId){
    RECT c=contentRect(sw);
    int x=c.left+S(20), y=subTop(sw), w=c.right-c.left-S(40);
    HWND h=CreateWindowExW(0,L"STATIC",hint,WS_CHILD|WS_VISIBLE|SS_RIGHT|WS_CLIPSIBLINGS,
        x,y,w,S(60),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(h,WM_SETFONT,(WPARAM)g_fUI,TRUE); sw->ctrls.push_back(h);
    HWND b=createFlatButton(sw->hwnd,cmdId,btnText,icon,BS_PRIMARY,x,y+S(72),w,S(38));
    sw->ctrls.push_back(b);
}

// §1.19.1 — the print-design page now offers TWO actions:
//   • «باز کردن طراح چاپ»  → the visual designer (HTML page or native fallback)
//   • «تنظیمات چاپ»        → pick a section, preview/download/upload/apply a design
static void buildDesignerPage(SettingsWin* sw){
    RECT c=contentRect(sw);
    int x=c.left+S(20), y=subTop(sw), w=c.right-c.left-S(40);
    HWND h=CreateWindowExW(0,L"STATIC",
        L"\u0637\u0631\u0627\u062d\u06cc \u0642\u0627\u0644\u0628 \u0686\u0627\u067e \u0628\u0631\u0627\u06cc \u0647\u0631 \u0628\u062e\u0634\u060c "
        L"\u067e\u06cc\u0634\u200c\u0646\u0645\u0627\u06cc\u0634\u060c \u062f\u0627\u0646\u0644\u0648\u062f\u060c "
        L"\u0628\u0627\u0631\u06af\u0630\u0627\u0631\u06cc \u0648 \u0627\u0639\u0645\u0627\u0644 \u0637\u0631\u062d.",
        WS_CHILD|WS_VISIBLE|SS_RIGHT|WS_CLIPSIBLINGS,
        x,y,w,S(48),sw->hwnd,NULL,g_hInst,NULL);
    SendMessageW(h,WM_SETFONT,(WPARAM)g_fUI,TRUE); sw->ctrls.push_back(h);

    HWND b1=createFlatButton(sw->hwnd,IDC_PANEL_BASE+60,
        L"\u0628\u0627\u0632 \u06a9\u0631\u062f\u0646 \u0637\u0631\u0627\u062d \u0686\u0627\u067e",
        ICO_PRINT,BS_PRIMARY,x,y+S(58),w,S(40));
    sw->ctrls.push_back(b1);

    HWND b2=createFlatButton(sw->hwnd,IDC_PANEL_BASE+65,
        L"\u062a\u0646\u0638\u06cc\u0645\u0627\u062a \u0686\u0627\u067e",   // تنظیمات چاپ
        ICO_GEAR,BS_OUTLINE,x,y+S(58)+S(48),w,S(40));
    sw->ctrls.push_back(b2);
}

// §1.12.0: compute the unscrolled content height of the current page so we can
// clamp scrolling and decide whether a scrollbar is needed. For sub-pages we
// take the bottom-most child control; for the home page we sum the rows.
static int pageContentHeight(SettingsWin* sw){
    int top = subHeaderH();   // sub-pages reserve the pinned header band
    if(curPage(sw)==PAGE_HOME){
        std::vector<RowDef> rows=homeRows(sw->mode);
        return homeRowsTop() + (int)rows.size()*(rowH()+rowGap()) + S(24);
    }
    // sub-page: find the lowest child bottom relative to the unscrolled origin.
    RECT cu=contentRectUnscrolled(sw);
    int maxBottom = top;
    for(HWND ctl : sw->ctrls){
        if(!ctl || !IsWindow(ctl)) continue;
        RECT r; GetWindowRect(ctl,&r);
        POINT p={r.left,r.bottom}; ScreenToClient(sw->hwnd,&p);
        // convert client-y back to unscrolled-origin space
        int unscrolledBottom = p.y + sw->scrollY - cu.top;
        if(unscrolledBottom>maxBottom) maxBottom=unscrolledBottom;
    }
    return maxBottom + S(24);
}

// §1.12.0: clamp sw->scrollY into [0, maxScroll]. Returns true if it changed.
static bool clampScroll(SettingsWin* sw){
    RECT rc; GetClientRect(sw->hwnd,&rc);
    int viewH = rc.bottom - S(8);
    int maxScroll = sw->contentH - viewH;
    if(maxScroll < 0) maxScroll = 0;
    int old = sw->scrollY;
    if(sw->scrollY > maxScroll) sw->scrollY = maxScroll;
    if(sw->scrollY < 0)         sw->scrollY = 0;
    return sw->scrollY != old;
}

// Configure the standard window vertical scrollbar to match the content.
static void updateScrollbar(SettingsWin* sw){
    RECT rc; GetClientRect(sw->hwnd,&rc);
    int viewH = rc.bottom - S(8);
    SCROLLINFO si; ZeroMemory(&si,sizeof(si)); si.cbSize=sizeof(si);
    si.fMask=SIF_RANGE|SIF_PAGE|SIF_POS;
    si.nMin=0;
    si.nMax=(sw->contentH>0)?sw->contentH:0;
    si.nPage=(viewH>0)?viewH:1;
    si.nPos=sw->scrollY;
    SetScrollInfo(sw->hwnd,SB_VERT,&si,TRUE);
}

// Re-place child controls / repaint after a scroll-offset change. We rebuild
// the page layout (cheap — only a handful of controls) so every child picks up
// the new contentRect().top, then repaint.
static void buildPage(SettingsWin* sw);   // fwd
static void scrollTo(SettingsWin* sw, int newY){
    sw->scrollY = newY;
    clampScroll(sw);
    buildPage(sw);            // re-lay children at the new offset
    updateScrollbar(sw);
}

// Build the controls for the current page. PAGE_HOME has no child controls
// (rows are owner-drawn in WM_PAINT); sub-pages create their controls here.
static void buildPage(SettingsWin* sw){
    destroyCtrls(sw);
    // Guest guard: never allow a forbidden page on the stack.
    if(sw->mode==SM_GUEST){
        int p=curPage(sw);
        if(p!=PAGE_HOME && p!=PAGE_THEME && p!=PAGE_CONTACT){
            sw->pageStack.clear();   // pop back to home
        }
    }
    switch(curPage(sw)){
    case PAGE_HOME:    break;   // owner-drawn rows
    case PAGE_PROFILE: buildProfilePage(sw); break;
    case PAGE_THEME:   buildThemePage(sw);   break;
    case PAGE_RECEPTION: break;   // v1.93: removed — reception settings now HTML-side
    case PAGE_BLACKLIST: buildBlacklistPage(sw); break;
    case PAGE_CONTACT: buildContactPage(sw); break;
    case PAGE_ABOUT:   buildAboutPage(sw);   break;
    case PAGE_DESIGNER:
        buildDesignerPage(sw);
        break;
    case PAGE_BACKUP:
        buildLauncherPage(sw,
            L"\u062a\u062d\u0644\u06cc\u0644\u060c \u0648\u0631\u0648\u062f \u0648 \u062a\u0627\u0631\u06cc\u062e\u0686\u0647 \u067e\u0634\u062a\u06cc\u0628\u0627\u0646.",
            L"\u0628\u0627\u0632 \u06a9\u0631\u062f\u0646 \u067e\u0634\u062a\u06cc\u0628\u0627\u0646\u200c\u06af\u06cc\u0631\u06cc",ICO_SHIELD,IDC_PANEL_BASE+61);
        break;
    case PAGE_EMP_SECT:
        buildLauncherPage(sw,
            L"\u0645\u062f\u06cc\u0631\u06cc\u062a \u06a9\u0627\u0631\u0645\u0646\u062f\u0627\u0646 \u0648 \u0628\u062e\u0634\u200c\u0647\u0627.",
            L"\u0628\u0627\u0632 \u06a9\u0631\u062f\u0646 \u06a9\u0627\u0631\u0645\u0646\u062f\u0627\u0646",ICO_PEOPLE,IDC_PANEL_BASE+62);
        break;
    case PAGE_SAVED_MSG:
        buildLauncherPage(sw,
            L"\u067e\u06cc\u0627\u0645\u200c\u0647\u0627\u06cc \u0630\u062e\u06cc\u0631\u0647\u200c\u0634\u062f\u0647.",
            L"\u0628\u0627\u0632 \u06a9\u0631\u062f\u0646 \u067e\u06cc\u0627\u0645\u200c\u0647\u0627\u06cc \u0630\u062e\u06cc\u0631\u0647",ICO_SAVED_MSG,IDC_PANEL_BASE+63);
        break;
    case PAGE_UPDATE:
        buildLauncherPage(sw,
            L"\u0628\u0631\u0631\u0633\u06cc \u0648 \u0646\u0635\u0628 \u0622\u062e\u0631\u06cc\u0646 \u0646\u0633\u062e\u0647.",
            L"\u0628\u0631\u0631\u0633\u06cc \u0628\u0647\u200c\u0631\u0648\u0632\u0631\u0633\u0627\u0646\u06cc",ICO_UPDATE,IDC_PANEL_BASE+64);
        break;
    }
    uikit::AzLayoutGuard_Verify(sw->hwnd);
    // §1.12.0: recompute content height + sync the scrollbar after layout.
    sw->contentH = pageContentHeight(sw);
    clampScroll(sw);
    updateScrollbar(sw);
    // After the WHOLE layout pass, a single repaint (no per-control invalidate).
    RedrawWindow(sw->hwnd,NULL,NULL,
        RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN|RDW_ERASE);
}

// ---------------------------------------------------------- painting ---------
static const wchar_t* roleName(const User& u){
    if(u.role==ROLE_ADMIN) return L"\u0645\u062f\u06cc\u0631";       // مدیر
    return L"\u067e\u0630\u06cc\u0631\u0634";                         // پذیرش
}

static void paintAvatar(HDC dc, SettingsWin* sw, RECT c){
    int d=g_scaleAvatar();
    int cx=(c.left+c.right)/2;
    // §2: drop the circle slightly lower than the top edge for a balanced header.
    int avTop=c.top+avatarTopDrop();
    RECT av={cx-d/2, avTop, cx+d/2, avTop+d};
    bool guest=(sw->mode==SM_GUEST) || sw->user.username.empty();
    // try a user photo first
    bool drewPhoto=false;
    if(!guest){
        std::wstring photo=getSetting(std::wstring(L"profile.photo.")+sw->user.username,L"");
        if(!photo.empty())
            drewPhoto=gpDrawImageFileCircle(dc,photo,av);
    }
    if(!drewPhoto){
        // tinted circle (accent @ ~18% alpha) behind a vector glyph / initial
        COLORREF tint=blendColor(g_theme.surface,g_theme.accent,18);
        HBRUSH br=CreateSolidBrush(tint);
        HPEN pen=CreatePen(PS_SOLID,1,g_theme.border);
        HGDIOBJ ob=SelectObject(dc,br), op=SelectObject(dc,pen);
        Ellipse(dc,av.left,av.top,av.right,av.bottom);
        SelectObject(dc,ob); SelectObject(dc,op);
        DeleteObject(br); DeleteObject(pen);
        if(guest){
            // generic silhouette
            RECT g={av.left+d/4,av.top+d/4,av.right-d/4,av.bottom-d/4};
            drawIcon(dc,ICO_USER,g,g_theme.accent,S(2));
        } else {
            // first letter of fullname, centered
            std::wstring init = sw->user.fullname.empty()? L"?"
                              : sw->user.fullname.substr(0,1);
            SelectObject(dc,g_fHuge); SetTextColor(dc,g_theme.accent);
            SetBkMode(dc,TRANSPARENT);
            DrawTextW(dc,init.c_str(),-1,&av,
                DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        }
    }
    // identity block
    SetBkMode(dc,TRANSPARENT);
    int iy=av.bottom+S(8);
    std::wstring name = guest? L"\u00ab\u0645\u0647\u0645\u0627\u0646\u00bb"   // «مهمان»
                             : sw->user.fullname;
    SelectObject(dc,g_fTitle); SetTextColor(dc,g_theme.text);
    RECT nr={c.left,iy,c.right,iy+S(26)};
    DrawTextW(dc,name.c_str(),-1,&nr,DT_CENTER|DT_SINGLELINE|DT_NOPREFIX);
    if(!guest){
        std::wstring sub = std::wstring(roleName(sw->user)) + L" \u2014 " +
                           (sw->user.dept.empty()? L"\u2014" : sw->user.dept);
        SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
        RECT sr={c.left,iy+S(28),c.right,iy+S(48)};
        DrawTextW(dc,sub.c_str(),-1,&sr,DT_CENTER|DT_SINGLELINE|DT_NOPREFIX);
    }
}

// §2: draw the pinned top-right close button (rounded square + ✕ glyph). Used on
// every settings page so the page can always be dismissed.
static void paintCloseBtn(HDC dc, SettingsWin* sw){
    RECT b=closeBtnRect(sw);
    COLORREF fill = sw->closeDown? g_theme.surface2 : sw->closeHot? g_theme.hover : g_theme.surface;
    COLORREF brd  = sw->closeHot ? g_theme.danger : g_theme.border;
    gpRoundRectBg(dc,b,S(8),fill,brd,g_theme.surface);
    // ✕ drawn as two crossing strokes (no glyph-font dependency)
    COLORREF ix = sw->closeHot? g_theme.danger : g_theme.textDim;
    int pad=S(11);
    RECT g={ b.left+pad, b.top+pad, b.right-pad, b.bottom-pad };
    HPEN pen=CreatePen(PS_GEOMETRIC|PS_ENDCAP_ROUND,S(2),ix);
    HGDIOBJ op=SelectObject(dc,pen);
    MoveToEx(dc,g.left,g.top,0);  LineTo(dc,g.right,g.bottom);
    MoveToEx(dc,g.right,g.top,0); LineTo(dc,g.left,g.bottom);
    SelectObject(dc,op); DeleteObject(pen);
}

static void paintHome(HDC dc, SettingsWin* sw, RECT c){
    paintAvatar(dc,sw,c);
    paintCloseBtn(dc,sw);
    std::vector<RowDef> rows=homeRows(sw->mode);
    SetBkMode(dc,TRANSPARENT);
    for(size_t i=0;i<rows.size();++i){
        RECT r=homeRowRect(sw,(int)i);
        bool hot=((int)i==sw->hotRow), down=((int)i==sw->downRow);
        COLORREF fill = down? g_theme.surface2 : hot? g_theme.hover : g_theme.surface;
        gpShadow(dc,r,S(12),S(3),hot?22:12);
        gpRoundRectBg(dc,r,S(12),fill,g_theme.border,g_theme.surface);
        // icon (right side for RTL)
        bool danger=(rows[i].row==ROW_LOGOUT);
        COLORREF ic = danger? g_theme.danger : g_theme.accent;
        RECT ir={ r.right-S(48), r.top+(rowH()-S(24))/2, r.right-S(24), r.top+(rowH()-S(24))/2+S(24) };
        HPEN pen=CreatePen(PS_GEOMETRIC|PS_ENDCAP_ROUND|PS_JOIN_ROUND,S(2),ic);
        HGDIOBJ op=SelectObject(dc,pen); HGDIOBJ ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
        drawIcon(dc,rows[i].icon,ir,ic,S(2));
        SelectObject(dc,op); SelectObject(dc,ob); DeleteObject(pen);
        // chevron (left side)
        RECT cv={ r.left+S(14), r.top+(rowH()-S(16))/2, r.left+S(30), r.top+(rowH()-S(16))/2+S(16) };
        HPEN cp=CreatePen(PS_SOLID,S(2),g_theme.textDim); HGDIOBJ ocp=SelectObject(dc,cp);
        // RTL "back/more" chevron points left
        MoveToEx(dc,cv.right,cv.top,0); LineTo(dc,cv.left,(cv.top+cv.bottom)/2); LineTo(dc,cv.right,cv.bottom);
        SelectObject(dc,ocp); DeleteObject(cp);
        // title + subtitle (right-aligned text block, between chevron & icon)
        RECT tx={ r.left+S(40), r.top+S(8), r.right-S(56), r.bottom-S(8) };
        SelectObject(dc,g_fUIB); SetTextColor(dc, danger? g_theme.danger : g_theme.text);
        if(rows[i].sub){
            RECT t1=tx; t1.bottom=tx.top+S(24);
            DrawTextW(dc,rows[i].title,-1,&t1,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_END_ELLIPSIS);
            RECT t2=tx; t2.top=tx.top+S(24);
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
            DrawTextW(dc,rows[i].sub,-1,&t2,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_END_ELLIPSIS);
        } else {
            DrawTextW(dc,rows[i].title,-1,&tx,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_END_ELLIPSIS);
        }
    }
}

static const wchar_t* pageTitle(int page){
    switch(page){
    case PAGE_PROFILE:  return L"\u0648\u06cc\u0631\u0627\u06cc\u0634 \u067e\u0631\u0648\u0641\u0627\u06cc\u0644";
    case PAGE_THEME:    return L"\u062a\u063a\u06cc\u06cc\u0631 \u067e\u0648\u0633\u062a\u0647";
    case PAGE_RECEPTION:return L"\u062a\u0646\u0638\u06cc\u0645\u0627\u062a \u067e\u0630\u06cc\u0631\u0634";
    case PAGE_BLACKLIST:return L"\u0644\u06cc\u0633\u062a \u0633\u06cc\u0627\u0647 \u0628\u06cc\u0645\u0627\u0631\u0627\u0646"; // لیست سیاه بیماران
    case PAGE_DESIGNER: return L"\u0637\u0631\u0627\u062d\u06cc \u0686\u0627\u067e";
    case PAGE_BACKUP:   return L"\u067e\u0634\u062a\u06cc\u0628\u0627\u0646\u200c\u06af\u06cc\u0631\u06cc \u0648 \u0628\u0627\u0632\u06cc\u0627\u0628\u06cc";
    case PAGE_EMP_SECT: return L"\u06a9\u0627\u0631\u0645\u0646\u062f\u0627\u0646 \u0648 \u0628\u062e\u0634\u200c\u0647\u0627";
    case PAGE_SAVED_MSG:return L"\u067e\u06cc\u0627\u0645\u200c\u0647\u0627\u06cc \u0630\u062e\u06cc\u0631\u0647";
    case PAGE_UPDATE:   return L"\u0628\u0647\u200c\u0631\u0648\u0632\u0631\u0633\u0627\u0646\u06cc";
    case PAGE_CONTACT:  return L"\u0627\u0631\u062a\u0628\u0627\u0637 \u0628\u0627 \u0645\u0627";
    case PAGE_ABOUT:    return L"\u062f\u0631\u0628\u0627\u0631\u0647 \u0628\u0631\u0646\u0627\u0645\u0647";
    default:            return L"\u062a\u0646\u0638\u06cc\u0645\u0627\u062a";
    }
}

static void paintSubHeader(HDC dc, SettingsWin* sw, RECT /*c*/){
    // §1.12.0: the sub-page header (back button + title) is PINNED — it uses the
    // UNSCROLLED column rect so it never moves while the content below scrolls.
    RECT cu=contentRectUnscrolled(sw);
    // opaque header band so scrolled content never shows through behind it
    RECT band={cu.left, 0, cu.right, S(14)+S(36)+S(8)};
    HBRUSH bg=CreateSolidBrush(g_theme.surface); FillRect(dc,&band,bg); DeleteObject(bg);
    HPEN sep=CreatePen(PS_SOLID,1,g_theme.border); HGDIOBJ old=SelectObject(dc,sep);
    MoveToEx(dc,band.left,band.bottom-1,NULL); LineTo(dc,band.right,band.bottom-1);
    SelectObject(dc,old); DeleteObject(sep);
    // back button (top-left) — animation-free, just hover/pressed fill.
    RECT b=backBtnRect(sw);
    COLORREF fill = sw->backDown? g_theme.surface2 : sw->backHot? g_theme.hover : g_theme.surface;
    gpRoundRectBg(dc,b,S(8),fill,g_theme.border,g_theme.surface);
    drawIcon(dc,ICO_BACK,b,g_theme.accent,S(2));
    // §2: pinned top-right close button (same on every page).
    paintCloseBtn(dc,sw);
    // page title (centered, pinned) — keep clear of BOTH the back button (left)
    // and the close button (right) so nothing overlaps.
    SetBkMode(dc,TRANSPARENT);
    SelectObject(dc,g_fTitle); SetTextColor(dc,g_theme.text);
    RECT tr={cu.left+S(52),S(14),cu.right-S(52),S(14)+S(36)};
    DrawTextW(dc,pageTitle(curPage(sw)),-1,&tr,
        DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
}

// v1.68.0 PERF: build/free the static frame cache. The outer rails, the sheet
// drop-shadow (gpShadow spread 10 → ~10 GDI+ path fills) and the sheet fill are
// identical on every paint for a given size+theme, so they are rendered once
// into bgDC and blitted. Only the dynamic content (home rows / sub-page header)
// is redrawn on top per WM_PAINT.
static void usFreeBg(SettingsWin* sw){
    if(sw->bgBmp){ DeleteObject(sw->bgBmp); sw->bgBmp=NULL; }
    if(sw->bgDC){ DeleteDC(sw->bgDC); sw->bgDC=NULL; }
    sw->bgW=sw->bgH=0;
}
static void usBuildBg(SettingsWin* sw, HDC ref, int W, int H){
    usFreeBg(sw);
    sw->bgDC=CreateCompatibleDC(ref);
    sw->bgBmp=CreateCompatibleBitmap(ref,W,H);
    if(!sw->bgDC||!sw->bgBmp){ usFreeBg(sw); return; }
    HGDIOBJ ob=SelectObject(sw->bgDC,sw->bgBmp);
    RECT rc={0,0,W,H};
    HBRUSH outer=CreateSolidBrush(g_theme.bg2); FillRect(sw->bgDC,&rc,outer); DeleteObject(outer);
    RECT c=contentRectUnscrolled(sw); c.top=0; c.bottom=H;
    RECT shadow=c; InflateRect(&shadow,S(3),0);
    gpShadow(sw->bgDC,shadow,S(18),S(10),g_dark?55:24);
    HBRUSH sheet=CreateSolidBrush(g_theme.surface); FillRect(sw->bgDC,&c,sheet); DeleteObject(sheet);
    HPEN edge=CreatePen(PS_SOLID,1,g_theme.border); HGDIOBJ old=SelectObject(sw->bgDC,edge);
    MoveToEx(sw->bgDC,c.left,0,NULL); LineTo(sw->bgDC,c.left,H);
    MoveToEx(sw->bgDC,c.right-1,0,NULL); LineTo(sw->bgDC,c.right-1,H);
    SelectObject(sw->bgDC,old); DeleteObject(edge);
    (void)ob;
    sw->bgW=W; sw->bgH=H;
}

static void paintWin(SettingsWin* sw, HDC dc0){
    RECT rc; GetClientRect(sw->hwnd,&rc);
    int W=rc.right, H=rc.bottom;
    uikit::MemDC mem(dc0,W,H);
    // blit the cached static frame; rebuild it only on size/theme change.
    if(!sw->bgDC || sw->bgW!=W || sw->bgH!=H) usBuildBg(sw,dc0,W,H);
    if(sw->bgDC) BitBlt(mem.dc,0,0,W,H,sw->bgDC,0,0,SRCCOPY);
    else{
        // fallback if the cache could not be allocated (low-resource hosts)
        HBRUSH outer=CreateSolidBrush(g_theme.bg2); FillRect(mem.dc,&rc,outer); DeleteObject(outer);
        RECT c=contentRectUnscrolled(sw); c.top=0; c.bottom=H;
        HBRUSH sheet=CreateSolidBrush(g_theme.surface); FillRect(mem.dc,&c,sheet); DeleteObject(sheet);
    }
    RECT content=contentRect(sw);
    if(curPage(sw)==PAGE_HOME) paintHome(mem.dc,sw,content);
    else                        paintSubHeader(mem.dc,sw,content);
    mem.blitTo(dc0);
}

// ---------------------------------------------------------- navigation -------
static void pushPage(SettingsWin* sw, int page){
    sw->pageStack.push_back(page);
    sw->hotRow=sw->downRow=-1;
    sw->scrollY=0;                 // §1.12.0: each new page starts at the top
    buildPage(sw);
}
static void popPage(SettingsWin* sw){
    if(!sw->pageStack.empty()) sw->pageStack.pop_back();
    sw->hotRow=sw->downRow=-1; sw->backHot=sw->backDown=false;
    sw->scrollY=0;
    buildPage(sw);
}

static int homeRowHit(SettingsWin* sw, int mx, int my){
    std::vector<RowDef> rows=homeRows(sw->mode);
    for(size_t i=0;i<rows.size();++i){
        RECT r=homeRowRect(sw,(int)i);
        if(mx>=r.left&&mx<r.right&&my>=r.top&&my<r.bottom) return (int)i;
    }
    return -1;
}

static void applyThemeByName(SettingsWin* sw, int mode){
    // v1.93: three themes — 0=light, 1=dark, 2=neon
    ThemeMode tm = (mode==1) ? TM_DARK : (mode==2) ? TM_NEON : TM_LIGHT;
    setSetting(L"theme", (mode==1)?L"dark":(mode==2)?L"neon":L"light");
    setSetting(L"theme.palette",L"blue");
    applyThemeMode(tm);
    broadcastThemeChange();
    if(g_hFrame) PostMessageW(g_hFrame,WM_APP_THEME_CHANGED,1,0);
    RedrawWindow(sw->hwnd,NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN|RDW_ERASE);
}

// Handle a home-row click. ALWAYS consults canAccess first.
static void activateRow(SettingsWin* sw, int idx){
    std::vector<RowDef> rows=homeRows(sw->mode);
    if(idx<0 || idx>=(int)rows.size()) return;
    const RowDef& rd=rows[idx];
    if(!canAccess(rd.row, sw->mode)) return;   // §A.3: no-op if forbidden
    if(rd.row==ROW_LOGOUT){
        HWND mw=sw->hMain;
        DestroyWindow(sw->hwnd);
        if(g_hFrame){ setUserOnline(g_session.user.username,false);
            g_session=Session(); switchScreen(SC_HOME); }
        (void)mw;
        return;
    }
    if(rd.page) pushPage(sw,rd.page);
}

// ---------------------------------------------------------- window proc ------
static LRESULT CALLBACK SettingsProc(HWND h,UINT m,WPARAM w,LPARAM l){
    SettingsWin* sw=(SettingsWin*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:{ PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps); if(sw) paintWin(sw,dc);
        EndPaint(h,&ps); return 0; }
    case WM_SIZE:
        if(sw){ clampScroll(sw); buildPage(sw); usFreeBg(sw); }
        return 0;
    case WM_MOUSEWHEEL:{
        if(!sw) break;
        int delta=GET_WHEEL_DELTA_WPARAM(w);
        int step = (delta/WHEEL_DELTA) * S(48);
        scrollTo(sw, sw->scrollY - step);
        return 0; }
    case WM_VSCROLL:{
        if(!sw) break;
        SCROLLINFO si; ZeroMemory(&si,sizeof(si)); si.cbSize=sizeof(si);
        si.fMask=SIF_ALL; GetScrollInfo(h,SB_VERT,&si);
        int pos=si.nPos;
        switch(LOWORD(w)){
        case SB_LINEUP:   pos-=S(36); break;
        case SB_LINEDOWN: pos+=S(36); break;
        case SB_PAGEUP:   pos-=si.nPage; break;
        case SB_PAGEDOWN: pos+=si.nPage; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: pos=si.nTrackPos; break;
        }
        scrollTo(sw,pos);
        return 0; }
    case WM_MOUSEMOVE:{
        if(!sw) break;
        int mx=GET_X_LPARAM(l), my=GET_Y_LPARAM(l);
        // §2: close button hover (pinned, every page)
        { RECT cb=closeBtnRect(sw);
          bool nc=(mx>=cb.left&&mx<cb.right&&my>=cb.top&&my<cb.bottom);
          if(nc!=sw->closeHot){ sw->closeHot=nc; InvalidateRect(h,NULL,FALSE); } }
        if(curPage(sw)==PAGE_HOME){
            int nh=homeRowHit(sw,mx,my);
            if(nh!=sw->hotRow){ sw->hotRow=nh; InvalidateRect(h,NULL,FALSE); }
        } else {
            RECT b=backBtnRect(sw);
            bool nb=(mx>=b.left&&mx<b.right&&my>=b.top&&my<b.bottom);
            if(nb!=sw->backHot){ sw->backHot=nb; InvalidateRect(h,NULL,FALSE); }
        }
        TRACKMOUSEEVENT t={sizeof(t),TME_LEAVE,h,0}; TrackMouseEvent(&t);
        return 0; }
    case WM_MOUSELEAVE:
        if(sw){ sw->hotRow=-1; sw->backHot=false; sw->closeHot=false; InvalidateRect(h,NULL,FALSE); }
        return 0;
    case WM_LBUTTONDOWN:{
        if(!sw) break;
        int mx=GET_X_LPARAM(l), my=GET_Y_LPARAM(l);
        // §2: close button press (pinned, every page) — takes priority
        { RECT cb=closeBtnRect(sw);
          if(mx>=cb.left&&mx<cb.right&&my>=cb.top&&my<cb.bottom){
              sw->closeDown=true; InvalidateRect(h,NULL,FALSE); SetCapture(h); return 0; } }
        if(curPage(sw)==PAGE_HOME){
            sw->downRow=homeRowHit(sw,mx,my);
            if(sw->downRow>=0) InvalidateRect(h,NULL,FALSE);
        } else {
            RECT b=backBtnRect(sw);
            if(mx>=b.left&&mx<b.right&&my>=b.top&&my<b.bottom){
                sw->backDown=true; InvalidateRect(h,NULL,FALSE);
            }
        }
        SetCapture(h);
        return 0; }
    case WM_LBUTTONUP:{
        if(!sw) break;
        ReleaseCapture();
        int mx=GET_X_LPARAM(l), my=GET_Y_LPARAM(l);
        // §2: close button release — if pressed AND released over it, close page
        if(sw->closeDown){
            RECT cb=closeBtnRect(sw);
            bool onClose=(mx>=cb.left&&mx<cb.right&&my>=cb.top&&my<cb.bottom);
            sw->closeDown=false; InvalidateRect(h,NULL,FALSE);
            if(onClose){ DestroyWindow(h); return 0; }
            return 0;
        }
        if(curPage(sw)==PAGE_HOME){
            int up=homeRowHit(sw,mx,my);
            int down=sw->downRow; sw->downRow=-1;
            InvalidateRect(h,NULL,FALSE);
            if(up>=0 && up==down) activateRow(sw,up);
        } else {
            RECT b=backBtnRect(sw);
            bool onBack=(mx>=b.left&&mx<b.right&&my>=b.top&&my<b.bottom);
            bool wasDown=sw->backDown; sw->backDown=false;
            InvalidateRect(h,NULL,FALSE);
            if(onBack && wasDown) popPage(sw);
        }
        return 0; }
    case WM_CTLCOLORSTATIC:{ HDC dc=(HDC)w; SetBkColor(dc,g_theme.surface);
        SetTextColor(dc,g_theme.text); return (LRESULT)g_brSurface; }
    case WM_CTLCOLOREDIT:{ HDC dc=(HDC)w; SetBkColor(dc,g_theme.inputBg);
        SetTextColor(dc,g_theme.inputText); return (LRESULT)g_brInput; }
    case WM_COMMAND:{
        if(!sw) break;
        int id=LOWORD(w);
        if(id==IDC_PANEL_BASE+93 && HIWORD(w)==EN_CHANGE){
            refreshBlacklistHistory(sw); return 0;
        }
        if(id==IDC_PANEL_BASE+84 && HIWORD(w)==EN_CHANGE &&
           settingsDigits(settingsText(h,IDC_PANEL_BASE+84)).size()==10){
            lookupBlacklistPatient(sw,true);
        }
        switch(id){
        case IDC_PANEL_BASE+10: applyThemeByName(sw,0); return 0; // روشن (light)
        case IDC_PANEL_BASE+11: applyThemeByName(sw,1); return 0; // مشکی (dark)
        case IDC_PANEL_BASE+12: applyThemeByName(sw,2); return 0; // نئونی (neon)
        case IDC_PANEL_BASE+96: {   // v1.64.0: رفع مسدودی (unblock by national id)
            std::wstring nid=settingsDigits(settingsText(h,IDC_PANEL_BASE+95));
            if(nid.empty()){
                MessageBoxW(h,L"کد ملی را وارد کنید.",L"رفع مسدودی",
                    MB_OK|MB_ICONWARNING); return 0;
            }
            int n=Blacklist_Remove(nid);
            if(n>0){
                SetDlgItemTextW(h,IDC_PANEL_BASE+95,L"");
                refreshBlacklistHistory(sw);
                MessageBoxW(h,L"مسدودی این بیمار رفع شد.",L"رفع مسدودی",
                    MB_OK|MB_ICONINFORMATION);
            } else {
                MessageBoxW(h,L"موردی برای این کد ملی در لیست سیاه یافت نشد.",
                    L"رفع مسدودی",MB_OK|MB_ICONWARNING);
            }
            return 0; }
        case IDC_PANEL_BASE+91: {
            BlacklistEntry r;
            r.nid=settingsDigits(settingsText(h,IDC_PANEL_BASE+84));
            r.first=settingsText(h,IDC_PANEL_BASE+85);
            r.last=settingsText(h,IDC_PANEL_BASE+86);
            r.father=settingsText(h,IDC_PANEL_BASE+87);
            r.mobile=settingsDigits(settingsText(h,IDC_PANEL_BASE+88));
            r.reason=settingsText(h,IDC_PANEL_BASE+89);
            r.createdBy=sw->user.username;
            int di=(int)SendMessageW(GetDlgItem(h,IDC_PANEL_BASE+90),CB_GETCURSEL,0,0);
            const long long mins[]={1440,10080,43200,525600,0};
            const wchar_t* labels[]={L"۲۴ ساعت",L"یک هفته",L"یک ماه",L"یک سال",L"دائم"};
            if(di<0||di>4) di=0;
            r.durationLabel=labels[di]; r.createdEpochMin=Blacklist_NowEpochMinutes();
            r.expiresEpochMin=mins[di]?r.createdEpochMin+mins[di]:0;
            std::wstring err;
            if(!Blacklist_Add(r,err)){
                MessageBoxW(h,err.c_str(),L"لیست سیاه",MB_OK|MB_ICONWARNING); return 0;
            }
            const int clearIds[]={84,85,86,87,88,89};
            for(int cid:clearIds) SetDlgItemTextW(h,IDC_PANEL_BASE+cid,L"");
            refreshBlacklistHistory(sw);
            MessageBoxW(h,L"مسدودی بیمار ثبت شد.",L"لیست سیاه",MB_OK|MB_ICONINFORMATION);
            return 0; }
        case IDC_PANEL_BASE+4: {   // profile submit
            wchar_t nm[128]={0}; GetDlgItemTextW(h,IDC_PANEL_BASE+1,nm,127);
            wchar_t ph[64]={0};  GetDlgItemTextW(h,IDC_PANEL_BASE+5,ph,63);
            setSetting(L"profile.phone",ph);
            if(sw->mode==SM_ADMIN){
                if(nm[0]) setUserFullName(sw->user.username,nm);
                MessageBoxW(h,L"\u062a\u063a\u06cc\u06cc\u0631\u0627\u062a \u0630\u062e\u06cc\u0631\u0647 \u0634\u062f.",
                    L"\u067e\u0631\u0648\u0641\u0627\u06cc\u0644",MB_OK|MB_ICONINFORMATION);
            } else {
                std::string json="{\"type\":\"profile_request\",\"user\":\"";
                char ub[256]; WideCharToMultiByte(CP_UTF8,0,sw->user.username.c_str(),-1,ub,256,NULL,NULL);
                char nb[256]; WideCharToMultiByte(CP_UTF8,0,nm,-1,nb,256,NULL,NULL);
                json+=ub; json+="\",\"new_name\":\""; json+=nb; json+="\"}";
                NetSync_PostJson(L"/api/profile_requests",json);
                MessageBoxW(h,L"\u062f\u0631\u062e\u0648\u0627\u0633\u062a \u0627\u0631\u0633\u0627\u0644 \u0634\u062f.",
                    L"\u067e\u0631\u0648\u0641\u0627\u06cc\u0644",MB_OK|MB_ICONINFORMATION);
            }
            return 0; }
        case IDC_PANEL_BASE+60: { HWND mw=sw->hMain; PrintDesigner_Open(mw); return 0; }
        case IDC_PANEL_BASE+65: { HWND mw=sw->hMain; PrintCfg_Open(mw); return 0; }   // تنظیمات چاپ
        case IDC_PANEL_BASE+61: { HWND mw=sw->hMain; openBackupManager(mw); return 0; }
        case IDC_PANEL_BASE+62: { HWND mw=sw->hMain; (void)mw;
                                  // employees screen lives in the management panel
                                  if(g_hFrame) switchScreen(SC_MANAGE);
                                  DestroyWindow(h); return 0; }
        case IDC_PANEL_BASE+63: { HWND mw=sw->hMain; SavedMessages_Show(mw); return 0; }
        case IDC_PANEL_BASE+64: { checkRemoteUpdate(sw->hMain); return 0; }
        case IDC_PANEL_BASE+70: {   // copy contact details
            std::wstring c =
                getSetting(L"contact.phone", L"\u06f0\u06f2\u06f1-\u06f1\u06f2\u06f3\u06f4\u06f5\u06f6\u06f7\u06f8")+L"  |  "+
                getSetting(L"contact.email", L"support@darmanplus.ir");
            if(OpenClipboard(h)){
                EmptyClipboard();
                size_t bytes=(c.size()+1)*sizeof(wchar_t);
                HGLOBAL hg=GlobalAlloc(GMEM_MOVEABLE,bytes);
                if(hg){ void* dst=GlobalLock(hg); if(dst){ memcpy(dst,c.c_str(),bytes);
                    GlobalUnlock(hg); SetClipboardData(CF_UNICODETEXT,hg); } }
                CloseClipboard();
            }
            MessageBoxW(h,L"\u0627\u0637\u0644\u0627\u0639\u0627\u062a \u062a\u0645\u0627\u0633 \u06a9\u067e\u06cc \u0634\u062f.",
                L"\u0627\u0631\u062a\u0628\u0627\u0637 \u0628\u0627 \u0645\u0627",MB_OK|MB_ICONINFORMATION);
            return 0; }
        }
        return 0; }
    case WM_SETCURSOR:
        // §2: hand cursor over the pinned close button (and home rows / back).
        if(sw && LOWORD(l)==HTCLIENT){
            POINT pt; GetCursorPos(&pt); ScreenToClient(h,&pt);
            RECT cb=closeBtnRect(sw);
            bool hand=(pt.x>=cb.left&&pt.x<cb.right&&pt.y>=cb.top&&pt.y<cb.bottom);
            if(!hand && curPage(sw)==PAGE_HOME) hand=(homeRowHit(sw,pt.x,pt.y)>=0);
            else if(!hand){ RECT b=backBtnRect(sw);
                hand=(pt.x>=b.left&&pt.x<b.right&&pt.y>=b.top&&pt.y<b.bottom); }
            if(hand){ SetCursor(LoadCursor(NULL,IDC_HAND)); return TRUE; }
        }
        break;
    case WM_KEYDOWN:
        if(sw && (w==VK_ESCAPE || w==VK_BACK)){
            if(curPage(sw)==PAGE_HOME) DestroyWindow(h);
            else popPage(sw);
            return 0;
        }
        if(sw && (w==VK_NEXT||w==VK_PRIOR||w==VK_HOME||w==VK_END)){
            RECT rc; GetClientRect(h,&rc); int page=rc.bottom-S(8);
            if(w==VK_NEXT)  scrollTo(sw,sw->scrollY+page);
            else if(w==VK_PRIOR) scrollTo(sw,sw->scrollY-page);
            else if(w==VK_HOME)  scrollTo(sw,0);
            else if(w==VK_END)   scrollTo(sw,sw->contentH);
            return 0;
        }
        break;
    case WM_APP_THEME: if(sw) usFreeBg(sw); InvalidateRect(h,NULL,TRUE); return 0;
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY:
        if(sw){ destroyCtrls(sw); usFreeBg(sw);
            if(sw->hMain){ EnableWindow(sw->hMain,TRUE); SetForegroundWindow(sw->hMain); }
            delete sw; SetWindowLongPtrW(h,GWLP_USERDATA,0); }
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

static void openSettingsWindow(HWND hMain,const User& u,int mode){
    Breadcrumb(mode==0?L"open settings: GUEST"
             : mode==1?L"open settings: RECEPTION":L"open settings: ADMIN");
    selfCheckMatrix();
    static bool reg=false; const wchar_t* CLS=L"AzSettingsWin";
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=SettingsProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=CLS;
        wc.hbrBackground=g_brBg; wc.style=CS_HREDRAW|CS_VREDRAW;
        RegisterClassW(&wc); reg=true; }
    uikit::Az_RegisterControls();
    Sections_Init(); Designs_Init();

    SettingsWin* sw=new SettingsWin();
    sw->user=u; sw->mode=mode; sw->hMain=hMain;

    const wchar_t* titleTxt =
        mode==SM_ADMIN?  L"\u062a\u0646\u0638\u06cc\u0645\u0627\u062a \u0645\u062f\u06cc\u0631\u06cc\u062a" :
        mode==SM_GUEST?  L"\u062a\u0646\u0638\u06cc\u0645\u0627\u062a" :
                         L"\u062a\u0646\u0638\u06cc\u0645\u0627\u062a \u067e\u0630\u06cc\u0631\u0634";
    // §1.12.0 (§1): settings now opens as a SEPARATE FULL-PAGE view. v1.93: it
    // covers the ENTIRE screen — including the taskbar — as an opaque WS_POPUP
    // top-level window, so the screen behind it (reception/management) is NOT
    // visible and nothing bleeds through. A standard vertical scrollbar
    // (WS_VSCROLL) + WS_CLIPCHILDREN gives smooth, smear-free scrolling. The
    // content is rendered in a centered column so it still reads as a clean,
    // modern settings page on wide monitors.
    int W = GetSystemMetrics(SM_CXSCREEN);
    int H = GetSystemMetrics(SM_CYSCREEN);
    HWND h=CreateWindowExW(0,CLS,titleTxt,
        WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN|WS_VSCROLL,0,0,W,H,hMain,NULL,g_hInst,NULL);
    sw->hwnd=h;
    SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)sw);
    buildPage(sw);
    ShowWindow(h,SW_SHOW);
    EnableWindow(hMain,FALSE);
    SetForegroundWindow(h);
}

} // anonymous namespace

// ---------------------------------------------------------------- public -----
void OpenReceptionSettings(HWND hMain, const User& u){ openSettingsWindow(hMain,u,SM_RECEPTION); }
void OpenManagementSettings(HWND hMain, const User& u){ openSettingsWindow(hMain,u,SM_ADMIN); }
void OpenSettings(HWND hMain, const User& u){
    // §A: a GUEST (no login → empty username) gets the minimal settings window
    // (ONLY «تغییر پوسته» + «ارتباط با ما»). A logged-in reception user gets the
    // full reception rows; an admin gets the management rows.
    if(u.username.empty())          openSettingsWindow(hMain,u,SM_GUEST);
    else if(u.role==ROLE_ADMIN)     OpenManagementSettings(hMain,u);
    else                            OpenReceptionSettings(hMain,u);
}
