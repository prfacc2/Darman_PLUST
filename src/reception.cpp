// ============================================================================
//  reception.cpp — reception workspace:
//   • info bar (current user, access type, live Iran date/time, calculator)
//   • browser-style tab strip: open/close tabs (پذیرش <بخش>)
//   • reception form (Enter = next field) + live billing (Iranian insurances)
//   • print: رسید بیمه / چاپ نسخه / چاپ آخرین قبض (F8) — real printer output
// ============================================================================
#include "app.h"
#include "sections.h"   // §1.19.0: resolve operator dept → Section id for print routing
#include "web_admission.h" // v1.33.0: embedded WebView2 «پذیرش بیمار» surface (native fallback)
//  v1.17.0: the HTML/CSS/JS (MSHTML) presentation host was retired — the
//  reception/appointment UI is now 100% native C++. `webhost.h` is no longer
//  included and the webhost_*.{cpp,inc} sources are no longer compiled. The
//  `web` HWND field on TabPage is kept (always NULL) so the existing layout
//  guards (`if(t->web) ...`) stay valid without any code churn.
#include <commctrl.h>
#include <stdio.h>
#include <algorithm>
#include <utility>
#include <ctime>

#define RC_CLASS   L"AzReception"
#define TABPG_CLASS L"AzRecTab"

// info-bar buttons
#define ID_RC_CALC    501
#define ID_RC_NEWTAB  502
#define ID_RC_NEWPAT  503   // "پذیرش بیمار" — clears the ACTIVE tab's form
//  v1.60.0: «نوبت‌دهی» (appointment scheduling, formerly ID_RC_APPT=504 /
//  TK_APPOINTMENT) has been REMOVED from the reception account completely —
//  page, option, routing and code path.
//  v1.7.0: these actions are triggered from BUTTONS IN THE FRAME
//  HEADER (main.cpp) and routed to this window via WM_COMMAND, so the tab
//  strip stays clean. The header sends ID_RC_NEWPAT / ID_RC_NEWTAB.
// per-tab form ids
#define ID_F_FIRST    601   // base for sequential edits
#define ID_F_GENDER   620
#define ID_F_PTYPE    621
#define ID_F_INS      622
#define ID_F_SUPP     623
#define ID_F_NTYPE    624
#define ID_F_PRICE    630
#define ID_F_DISCOUNT 631
#define ID_F_SUBMIT   640
#define ID_F_PRT_INS  641
#define ID_F_PRT_RX   642
#define ID_F_PRT_LAST 643
#define ID_F_CLOSE    644
#define ID_F_INQUIRY  645   // استعلام بیمه با کد ملی
#define ID_F_HASINS   646   // چک‌باکس «دارای بیمه» کنار کد ملی
// ---- right info panel ids (v1.6.0) ----
#define ID_F_ARCHIVE     650
#define ID_F_ARCHIVE_GO  651
#define ID_F_FILE        652
#define ID_F_FILE_GO     653
#define ID_F_BOOKNO      654
#define ID_F_BOOKNO_CHK  655
#define ID_F_VALID       656
#define ID_F_VALID_AUTO  657
#define ID_F_RXDATE      658
#define ID_F_RXDATE_AUTO 659
#define ID_F_SUPP_PANEL  660
#define ID_F_SUPP_PCT    661
#define ID_F_SUPP_GO     662
#define ID_F_DOCCODE     663
#define ID_F_DOCNAME     664
#define ID_F_DOCSEARCH   665
#define ID_F_RESET       666   // v1.4.0 (§6): پاک کردن فرم (Ctrl+R)
#define ID_F_NEW         667   // v1.18.0: «پذیرش بیمار» — فرم تازه (ردیف دکمه‌های پایین)
#define ID_F_SVC_ADD     668   // v1.18.0: «افزودن خدمت» به جدول خدمات
#define ID_F_SVC_SEARCH  669   // v1.18.0: جستجوی خدمت
// ---- v1.25.0: center Treating-doctor / Performer / Appointment cards ----
#define ID_F_DOC2_CODE   670   // پزشک معالج — کد نظام پزشکی (center card)
#define ID_F_DOC2_LIST   671   // پزشک معالج — لیست/نام دکتر (combo)
#define ID_F_PERF_CODE   672   // انجام دهنده — کد
#define ID_F_PERF_LIST   673   // انجام دهنده — لیست/نام (combo)
#define ID_F_APPT_DATE   674   // تاریخ نوبت
#define ID_F_APPT_SHIFT  675   // شیفت نوبت (combo)
#define ID_F_APPT_P      676   // P — پرداختی (numeric)
#define ID_F_APPT_S      677   // S — صندوق (numeric)
#define ID_F_SUPP_PCT2   678   // درصد بیمه مکمل (center doctor card)
#define ID_F_SUPP_PCTINS 679   // درصد بیمه تکمیل (center insurance card)
// ---- v1.25.0: inline «افزودن خدمت» panel ----
#define ID_F_SVC_CODE    680   // کد خدمت (small)
#define ID_F_SVC_NAME    681   // نام خدمت (wide)
#define ID_F_SVC_QTY     682   // تعداد
#define ID_F_SVC_FREECHK 683   // تیک نرخ آزاد
#define ID_F_SVC_FREEAMT 684   // مبلغ نرخ آزاد
#define ID_F_SVC_CONFIRM 685   // دکمهٔ افزودن (داخل پنل)
// ---- v1.25.0: bottom tab area (unpaid box / admission queue) ----
#define ID_F_ADD_UNPAID  690   // افزودن به صندوق نرفته‌ها
#define ID_F_ADD_QUEUE   691   // افزودن به صف پذیرش
#define ID_F_NOPAY_CHK   692   // عدم پرداخت در حال حاضر
// ---- v1.26.0: unpaid-box / queue panel toolbar (bottom-left table) ----
#define ID_F_UP_DATE     693   // «تاریخ تا» filter (Jalali date)
#define ID_F_UP_HOURS    694   // «مراجعات اخیر (ساعت)» combo
#define ID_F_UP_REFRESH  695   // refresh (reload the list from disk)
// row-operation ids (repeat/open/delete) live per-row, hit-tested in paint

// ---- v1.25.0: a single service line on the خدمات table ----
struct SvcRow {
    std::wstring code;
    std::wstring name;
    // v1.55.0 — شرح خدمت / نوع خدمت, resolved from the Service Management
    // catalogue (ServiceDef::desc / ServiceDef::category) the moment the row is
    // added. They feed the receipt's «شرح خدمت» and «نوع خدمت» columns. Never
    // fabricated — an unknown code leaves them EMPTY (printed as "—").
    std::wstring desc;
    std::wstring category;
    int          qty;
    long long    price;      // unit price (ریال) — free rate or catalogue tariff
    long long    discount;   // per-row discount
    long long    insShare;   // سهم بیمه (computed)
    long long    patShare;   // سهم بیمار (computed)
    SvcRow():qty(1),price(0),discount(0),insShare(0),patShare(0){}
};

// ============================================================== TAB PAGE ===
enum TabKind {
    TK_RECEPTION   = 0,   // the patient reception + billing form
    TK_PORTAL      = 1,   // پیام پرتابل — portal/admin message page (post-login)
    TK_EMPTY       = 2,   // a fresh blank tab (new-tab button)
    TK_TOOLS       = 3,   // v1.84: native «ابزارها» web surface
    TK_CASHIER     = 4,   // v1.84: native «صندوق» web surface
    TK_QUEUE       = 5,   // v1.85: native «صندوق نرفته‌ها و صف پذیرش» web surface
    TK_RECEIPTS    = 6,   // v1.88: native «جستجوی قبض» web surface (was in-page)
    TK_DASH        = 7,   // v1.89: native «داشبورد» — the permanent landing tab
    TK_BLACKLIST   = 8,   // v1.94: لیست سیاه بیماران — HTML surface
    TK_SVREPORT    = 9    // v2.02: «تفکیک خدمات» — per-service breakdown report
};
// v2.02: single map of tab kind → HTML surface name. Adding a new HTML tab
// means one extra case here (plus title / ui.openTab kind). Kinds that return
// nullptr (TK_EMPTY today) stay native-painted and never get a WebView.
static const char* tabKindSurface(int kind){
    switch(kind){
    case TK_RECEPTION: return "admission";
    case TK_TOOLS:     return "tools";
    case TK_CASHIER:   return "cashier";
    case TK_QUEUE:     return "queue";
    case TK_RECEIPTS:  return "receipts";
    case TK_DASH:      return "dash";
    case TK_PORTAL:    return "portal";
    case TK_BLACKLIST: return "blacklist";
    case TK_SVREPORT:  return "svreport";
    default:           return nullptr;
    }
}
struct TabPage {
    HWND page;                // container window (child of reception)
    int  kind;                // TabKind
    std::wstring title;
    std::string extraJson;    // v1.84: ticket JSON for a receipt-backed پذیرش tab
    // form controls
    HWND eFirst,eLast,eNid,eFather,eBirth,cGender,eMobile,ePhone,eAddr;
    HWND cPType,cIns,cSupp,cNType;
    HWND ePrice,eDiscount;
    HWND bSubmit,bPrtIns,bPrtRx,bPrtLast,bClose,bInquiry;
    HWND bReset;   // v1.4.0 (§6) reset/clear form
    HWND bNew;     // v1.18.0: «پذیرش بیمار» (ردیف دکمه‌های پایین فرم)
    HWND bSvcAdd;  // v1.18.0: «افزودن خدمت» بالای جدول خدمات
    HWND eSvcSearch;// v1.18.0: جستجوی خدمت
    // ---- v1.25.0: center Treating-doctor / Performer / Appointment cards ----
    HWND eDoc2Code; HWND cDoc2List;   // پزشک معالج (کد + لیست/نام)
    HWND ePerfCode; HWND cPerfList;   // انجام دهنده (کد + لیست/نام)
    HWND eApptDate; HWND cApptShift;  // نوبت: تاریخ + شیفت
    HWND eApptP;    HWND eApptS;      // P (پرداختی) + S (صندوق)
    HWND eSuppPct2;                   // درصد بیمه مکمل (center doctor card)
    HWND eSuppPctIns;                 // درصد بیمه تکمیل (center insurance card)
    // ---- v1.25.0: inline «افزودن خدمت» panel ----
    HWND eSvcCode, eSvcName, eSvcQty, chkSvcFree, eSvcFreeAmt, bSvcConfirm;
    bool svcPanelOpen;                // whether the inline add panel is shown
    //  v1.31.0: collapsible service / queue lists (script items #20-24). When
    //  collapsed the list body shows only a few rows so the whole page fits on
    //  one frame with no scrolling; expanding lets the body grow (and only THEN
    //  may the body scroll internally if it is genuinely long).
    bool svcCollapsed;                // خدمات list collapsed → show few rows
    bool upCollapsed;                 // صندوق/صف list collapsed → show few rows
    std::vector<SvcRow> services;     // the خدمات table rows
    int  svcHotRow, svcHotBtn;        // hover state for row operations
    // ---- v1.25.0: bottom tab area ----
    HWND bAddUnpaid, bAddQueue, chkNoPay;
    int  bottomTab;                   // 0 = صندوق نرفته‌ها, 1 = صف پذیرش
    //  v1.69.0: interactive row state for the redesigned cash-desk / queue panel.
    //  upHotIdx  — s_upRows index under the cursor (-1 none) for row hover.
    //  upSelRaw  — raw line of the SELECTED row (empty = none). Stored as the
    //  raw line (not an index) so the selection survives a cache reload / tab
    //  data change without ever pointing at the wrong patient.
    int  upHotIdx;
    std::wstring upSelRaw;
    // ---- v1.26.0: unpaid-box panel toolbar controls ----
    HWND eUpDate;    HWND cUpHours;   HWND bUpRefresh;
    HWND chkIns;             // «دارای بیمه» — کنار کد ملی، پیش‌فرض تیک‌خورده
    //  v1.13.0 (§3/§4): the hybrid HTML/CSS/JS presentation host. When non-NULL
    //  it is the VISIBLE interface for this reception/appointment tab (rendered
    //  via the system MSHTML control). C++ stays the source of truth — the host
    //  bridges every action to the existing repository functions. NULL means the
    //  classic native form is used (deterministic fallback if MSHTML is absent).
    HWND web;
    std::vector<int> insAllowed;   // insurances this patient carries (inquiry)
    // ---- right info panel (v1.6.0) ----
    HWND eArchive, bArchiveGo;     // ش بایگانی + استعلام
    HWND eFile,    bFileGo;        // ش پرونده + استعلام
    HWND eBookNo;  HWND chkBookNo; // ش دفترچه + enable checkbox
    HWND eValid;   HWND chkValidAuto;   // تاریخ اعتبار + اتوماتیک
    HWND eRxDate;  HWND chkRxAuto;      // تاریخ نسخه + اتوماتیک
    HWND cSuppPanel; HWND eSuppPct; HWND bSuppGo;  // بیمه مکمل + درصد + استعلام
    HWND eDocCode, eDocName, bDocSearch;           // پزشک معالج
    // computed billing
    long long total,mainShare,patientShare,baseDiff,orgShare,paid;
    bool detached;
    bool autoPrice;          // guard: ignore EN_CHANGE from our own auto-fill
    //  v1.7.0: identity-verification state. After an inquiry, if NO trusted
    //  source could verify the national code, we flag name+surname so the form
    //  shows a clear validation (danger) border instead of fabricating data.
    bool idChecked;          // an inquiry was performed
    bool idVerified;         // a trusted source verified the identity
    //  v1.7.0 cartable (کارتابل) view state — only meaningful for TK_PORTAL.
    //   cartDetail : false → tile list, true → full message details view
    //   cartSelDisp: the display index (into s_cartMsgs) being viewed
    //   cartSelNF  : the plain newest-first index (for the data layer)
    //   cartHotBtn : which detail-view button is hovered (0=none)
    bool cartDetail;
    int  cartSelDisp, cartSelNF, cartHotBtn;
    //  v1.8.0: cartShowArchive — when true the cartable shows the locally
    //  archived (saved) messages instead of the live management inbox. Only
    //  reachable when «پیام‌های ذخیره‌شده» is enabled in settings.
    bool cartShowArchive;
    //  v1.9.0: bitmask of REQUIRED fields that were empty on the last submit
    //  attempt. Each set bit maps to an index in the `inputs[]` array used by
    //  the field-well painter, and draws ONLY a very thin red hairline border
    //  (no glow / double-ring) until the user edits the field.
    int  invalidMask;
    //  v2: vertical scroll offset (device px) for the reception form when its
    //  content is taller than the visible tab-page client area. 0 = top.
    int  scrollY;
    int  contentH;           // last computed total content height
    std::wstring lastMsg; COLORREF msgCol;
    TabPage():page(0),kind(TK_RECEPTION),svcPanelOpen(false),
        svcCollapsed(true),upCollapsed(true),
        svcHotRow(-1),svcHotBtn(0),bottomTab(0),upHotIdx(-1),web(0),
        total(0),mainShare(0),patientShare(0),
        baseDiff(0),orgShare(0),paid(0),detached(false),autoPrice(false),
        idChecked(false),idVerified(false),
        cartDetail(false),cartSelDisp(-1),cartSelNF(-1),cartHotBtn(0),
        cartShowArchive(false),invalidMask(0),scrollY(0),contentH(0),msgCol(0){}
};

struct RecData {
    HWND bCalc, bNewTab, bNewPat;
    std::vector<TabPage*> tabs;
    int active;
    int hotTab, hotClose;     // hover indices
    int lastUnseen;                      // cartable poll state
    // v1.90.0: overflow scroll when too many C++ tabs to fit the strip
    int   tabOff;        // first visible docked-tab index (0 = rightmost)
    int   hotChevron;    // 0 none, 1 = prev (▶, decrease off), 2 = next (◀)
    // v1.7.0: drag-and-drop tab reordering -----------------------------------
    bool  dragArmed;     // mouse down on a tab body, may become a drag
    bool  dragging;      // a reorder drag is in progress
    int   dragIdx;       // index of the tab being dragged
    POINT dragStart;     // where the press began (to apply a small threshold)
    int   dragX;         // current mouse x (for the floating ghost)
    int   dropIdx;       // computed insertion index (drop target)
    RecData():active(-1),hotTab(-1),hotClose(-1),lastUnseen(0),
        tabOff(0),hotChevron(0),
        dragArmed(false),dragging(false),dragIdx(-1),dragX(0),dropIdx(-1){
        dragStart.x=dragStart.y=0; }
};
static RecData* s_rd = NULL;             // single reception screen at a time

// v1.25.0: per-paint hit-test map for the خدمات table row-delete buttons and
// for the P/S appointment preview squares. Rebuilt every WM_PAINT so mouse
// clicks map to the exact painted geometry.
struct SvcHit { RECT del; };
static std::vector<SvcHit> s_svcHits;    // one per visible service row (delete)
static RECT s_psPRect={0,0,0,0}, s_psSRect={0,0,0,0}; // P / S preview squares
// v1.26.0: bottom-left unpaid-box panel hit maps (tabs + per-row delete).
static RECT s_upTabR[2]={{0,0,0,0},{0,0,0,0}};        // صندوق نرفته‌ها | صف پذیرش
static RECT s_upAddRect={0,0,0,0};                    // «+ افزودن به …» blue link
// v1.31.0: collapse/expand chevron hit-rects for the two bottom lists.
static RECT s_svcToggleR={0,0,0,0};                   // خدمات list collapse toggle
static RECT s_upToggleR ={0,0,0,0};                   // صندوق/صف list collapse toggle
//  v1.32.0: draw a collapse/expand caret with GDI (a filled triangle) inside the
//  chip rect `r`. Font glyphs (▾/▸) are NOT reliably present in Vazirmatn and
//  rendered as blank tofu, so the toggle looked invisible. A polygon always
//  paints. collapsed → points DOWN-to-the-side (►), expanded → points DOWN (▼).
static void drawCollapseCaret(HDC dc, RECT r, bool collapsed, COLORREF col){
    int cx=(r.left+r.right)/2, cy=(r.top+r.bottom)/2;
    int s=(r.right-r.left)/4; if(s<3) s=3;
    HBRUSH br=CreateSolidBrush(col); HGDIOBJ ob=SelectObject(dc,br);
    HPEN   pn=CreatePen(PS_SOLID,1,col); HGDIOBJ op=SelectObject(dc,pn);
    POINT pts[3];
    if(collapsed){ // ► pointing left (RTL "closed")
        pts[0]={cx+s, cy-s}; pts[1]={cx+s, cy+s}; pts[2]={cx-s, cy};
    } else {        // ▼ pointing down ("open")
        pts[0]={cx-s, cy-s}; pts[1]={cx+s, cy-s}; pts[2]={cx, cy+s};
    }
    Polygon(dc,pts,3);
    SelectObject(dc,op); DeleteObject(pn);
    SelectObject(dc,ob); DeleteObject(br);
}
// v1.69.0: small filled triangle ▲/▼ for the queue move-up / move-down row
// actions. Font arrows are unreliable in Vazirmatn (render as blank tofu), so
// a polygon is used — same approach as drawCollapseCaret. up=true → ▲.
static void drawArrowBtn(HDC dc, RECT r, bool up, COLORREF col){
    int cx=(r.left+r.right)/2, cy=(r.top+r.bottom)/2;
    int s=(r.right-r.left)/3; if(s<3) s=3;
    HBRUSH br=CreateSolidBrush(col); HGDIOBJ ob=SelectObject(dc,br);
    HPEN   pn=CreatePen(PS_SOLID,1,col); HGDIOBJ op=SelectObject(dc,pn);
    POINT pts[3];
    if(up){   pts[0]={cx-s,cy+s}; pts[1]={cx+s,cy+s}; pts[2]={cx,cy-s}; }
    else  {   pts[0]={cx-s,cy-s}; pts[1]={cx+s,cy-s}; pts[2]={cx,cy+s}; }
    Polygon(dc,pts,3);
    SelectObject(dc,op); DeleteObject(pn);
    SelectObject(dc,ob); DeleteObject(br);
}
// v1.69.0: per-row hit map for the redesigned cash-desk / queue table. `row` is
// the whole row body (click = select). edit/up/down/del are the operation icons;
// up/down are only populated for the SELECTED row (progressive disclosure), so
// for other rows they stay zeroed and PtInRect() returns false. idx → s_upRows.
struct UpHit { RECT row, edit, up, down, del; int idx; };
static std::vector<UpHit> s_upHits;
// one parsed line of unpaid_box.dat / recept_queue.dat
struct UpRow {
    int          q;          // queue / file number
    std::wstring name;       // patient full name
    long long    paid;
    std::wstring date, time; // Jalali date + HH:MM (may be empty for old lines)
    long long    epoch;      // unix time when saved (0 = unknown)
    std::wstring raw;        // the raw line (for delete-by-line)
};
static std::vector<UpRow> s_upRows;      // rows of the ACTIVE bottom tab
static int  s_upLoadedTab = -1;          // which tab s_upRows holds (-1 = dirty)
static int  s_upCount[2]={-1,-1};        // v1.69: cached item count per tab (-1 dirty)
static void upMarkDirty(){ s_upLoadedTab = -1; s_upCount[0]=s_upCount[1]=-1; }
static std::wstring upFileFor(int tab){
    return dataDir() + (tab==0 ? L"\\unpaid_box.dat" : L"\\recept_queue.dat");
}
// tolerant line parser: q|tag|name|paid[|date|time|epoch]
static bool upParse(const std::wstring& ln, UpRow& r){
    if(ln.empty()) return false;
    std::vector<std::wstring> f; size_t p=0;
    while(p<=ln.size()){
        size_t e=ln.find(L'|',p); if(e==std::wstring::npos) e=ln.size();
        f.push_back(ln.substr(p,e-p)); p=e+1;
        if(e==ln.size()) break;
    }
    if(f.size()<4) return false;
    r.q=_wtoi(f[0].c_str());
    r.name=f[2];
    r.paid=_wtoi64(f[3].c_str());
    r.date = f.size()>4?f[4]:L"";
    r.time = f.size()>5?f[5]:L"";
    r.epoch= f.size()>6?_wtoi64(f[6].c_str()):0;
    r.raw=ln;
    return true;
}
static void upLoad(int tab){
    if(s_upLoadedTab==tab) return;
    s_upRows.clear();
    std::wstring txt=readFileUtf8(upFileFor(tab));
    size_t p=0;
    while(p<txt.size()){
        size_t e=txt.find(L'\n',p); if(e==std::wstring::npos) e=txt.size();
        std::wstring ln=txt.substr(p,e-p); p=e+1;
        while(!ln.empty() && (ln.back()==L'\r'||ln.back()==L'\n')) ln.pop_back();
        UpRow r; if(upParse(ln,r)) s_upRows.push_back(r);
    }
    s_upCount[tab]=(int)s_upRows.size();
    s_upLoadedTab=tab;
}
// v1.69.0: cached item count for EITHER bottom tab (drives the tab count badges).
// Read from disk only when the cache is dirty — never on every WM_PAINT. When the
// requested tab is already the live-loaded one, the count is free (s_upRows.size).
static int upCount(int tab){
    if(tab<0||tab>1) return 0;
    if(s_upCount[tab]>=0) return s_upCount[tab];
    if(s_upLoadedTab==tab){ s_upCount[tab]=(int)s_upRows.size(); return s_upCount[tab]; }
    std::wstring txt=readFileUtf8(upFileFor(tab));
    int n=0; size_t p=0;
    while(p<txt.size()){
        size_t e=txt.find(L'\n',p); if(e==std::wstring::npos) e=txt.size();
        std::wstring ln=txt.substr(p,e-p); p=e+1;
        while(!ln.empty() && (ln.back()==L'\r'||ln.back()==L'\n')) ln.pop_back();
        UpRow r; if(upParse(ln,r)) n++;
    }
    s_upCount[tab]=n; return n;
}
// remove one raw line from the tab's data file, then mark the cache dirty.
static void upDeleteRaw(int tab, const std::wstring& raw){
    std::wstring txt=readFileUtf8(upFileFor(tab)), out;
    bool removed=false; size_t p=0;
    while(p<txt.size()){
        size_t e=txt.find(L'\n',p); if(e==std::wstring::npos) e=txt.size();
        std::wstring ln=txt.substr(p,e-p); p=e+1;
        std::wstring cl=ln;
        while(!cl.empty() && (cl.back()==L'\r'||cl.back()==L'\n')) cl.pop_back();
        if(!removed && cl==raw){ removed=true; continue; }
        if(!cl.empty()) out += cl + L"\n";
    }
    writeFileUtf8(upFileFor(tab), out, false);
    upMarkDirty();
}
// current wall-clock "HH:MM" (Persian digits)
static std::wstring upNowHHMM(){
    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t b[8]; swprintf(b,8,L"%02d:%02d",st.wHour,st.wMinute);
    return toFaDigits(b);
}

// v1.69.0: swap two raw lines in the tab's data file — the queue "move up / move
// down" action. Each raw line is unique (it carries an epoch), so we swap the
// first occurrence of each; the file format is otherwise untouched.
static void upSwapRaw(int tab, const std::wstring& a, const std::wstring& b){
    if(a.empty()||b.empty()||a==b) return;
    std::wstring txt=readFileUtf8(upFileFor(tab)), out;
    bool didA=false, didB=false; size_t p=0;
    while(p<txt.size()){
        size_t e=txt.find(L'\n',p); if(e==std::wstring::npos) e=txt.size();
        std::wstring ln=txt.substr(p,e-p); p=e+1;
        std::wstring cl=ln;
        while(!cl.empty() && (cl.back()==L'\r'||cl.back()==L'\n')) cl.pop_back();
        if(!didA && cl==a){ out+=b+L"\n"; didA=true; continue; }
        if(!didB && cl==b){ out+=a+L"\n"; didB=true; continue; }
        if(!cl.empty()) out+=cl+L"\n";
    }
    writeFileUtf8(upFileFor(tab), out, false);
    upMarkDirty();
}
// v1.69.0: replace one raw line with a rebuilt line — used by «ویرایش» to change
// the patient name while preserving every other field (q / tag / paid / date …).
static void upReplaceLine(int tab, const std::wstring& oldRaw, const std::wstring& newRaw){
    if(oldRaw.empty()||oldRaw==newRaw) return;
    std::wstring txt=readFileUtf8(upFileFor(tab)), out;
    bool done=false; size_t p=0;
    while(p<txt.size()){
        size_t e=txt.find(L'\n',p); if(e==std::wstring::npos) e=txt.size();
        std::wstring ln=txt.substr(p,e-p); p=e+1;
        std::wstring cl=ln;
        while(!cl.empty() && (cl.back()==L'\r'||cl.back()==L'\n')) cl.pop_back();
        if(!done && cl==oldRaw){ out+=newRaw+L"\n"; done=true; continue; }
        if(!cl.empty()) out+=cl+L"\n";
    }
    writeFileUtf8(upFileFor(tab), out, false);
    upMarkDirty();
}
// rebuild a raw line with a new patient name (pipe-field index 2), keeping the
// rest of the line byte-for-byte. Falls back to the original if there are too
// few fields to rebuild safely.
static std::wstring upRebuildLine(const UpRow& r, const std::wstring& newName){
    std::vector<std::wstring> f; size_t p=0;
    while(p<=r.raw.size()){
        size_t e=r.raw.find(L'|',p); if(e==std::wstring::npos) e=r.raw.size();
        f.push_back(r.raw.substr(p,e-p)); p=e+1;
        if(e==r.raw.size()) break;
    }
    if(f.size()<4) return r.raw;
    f[2]=newName;
    std::wstring out; for(size_t i=0;i<f.size();i++){ if(i) out+=L'|'; out+=f[i]; }
    return out;
}

// v1.69.0: a tiny self-contained MODAL popup to edit one text field (the patient
// name on a cash-desk / queue row). The app uses NO Win32 dialog templates, so
// this builds a themed popup window with a real EDIT child + flat OK/Cancel
// buttons and runs a nested message loop — exactly what MessageBoxW does. The
// owner is disabled for the duration so the edit is truly modal.
struct UpEditState { std::wstring val; bool ok; bool done;
                     HWND edit, bOk, bCancel; };
static LRESULT CALLBACK upEditProc(HWND h, UINT m, WPARAM w, LPARAM l){
    UpEditState* s=(UpEditState*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_ERASEBKGND: return 1;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        SetTextColor((HDC)w,g_theme.inputText); SetBkColor((HDC)w,g_theme.inputBg);
        return (LRESULT)g_brInput;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        { HBRUSH bb=CreateSolidBrush(g_theme.bg); FillRect(dc,&rc,bb); DeleteObject(bb); }
        SetBkMode(dc,TRANSPARENT);
        SelectObject(dc,g_fLabel); SetTextColor(dc,g_theme.labelInk);
        RECT lr={S(16),S(14),rc.right-S(16),S(14)+S(22)};
        DrawTextW(dc,L"نام بیمار:",-1,&lr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        // recessed well behind the EDIT (same language as the reception form)
        if(s && s->edit){
            RECT er; GetWindowRect(s->edit,&er);
            POINT a={er.left,er.top}, b={er.right,er.bottom};
            ScreenToClient(h,&a); ScreenToClient(h,&b);
            RECT well={a.x,a.y,b.x,b.y}; InflateRect(&well,S(6),S(5));
            gpGradRoundRectBg(dc,well,S(8),
                blendColor(g_theme.inputBg,g_theme.border,g_dark?26:34),
                g_theme.inputBg,g_theme.border,g_theme.bg);
        }
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps); return 0; }
    case WM_COMMAND:
        if(s){
            if(LOWORD(w)==IDOK){            // تأیید
                int len=GetWindowTextLengthW(s->edit);
                std::wstring v;
                if(len>0){ v.resize((size_t)len+1);
                    int got=GetWindowTextW(s->edit,&v[0],len+1);
                    v.resize(got>0?(size_t)got:0); }
                s->val=v; s->ok=true; s->done=true; DestroyWindow(h);
            } else if(LOWORD(w)==IDCANCEL){  // انصراف
                s->done=true; DestroyWindow(h);
            }
        }
        return 0;
    case WM_CLOSE:
        if(s) s->done=true;
        DestroyWindow(h); return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
static bool upEditName(HWND owner, const wchar_t* title, std::wstring& val){
    static bool reg=false;
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=upEditProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=L"AzUpEdit";
        RegisterClassW(&wc); reg=true; }
    UpEditState st{}; st.val=val;
    int w=S(400), hgt=S(180);
    RECT or_; GetWindowRect(owner,&or_);
    int x=or_.left+((or_.right-or_.left)-w)/2;
    int y=or_.top+((or_.bottom-or_.top)-hgt)/2;
    HWND pw=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW, L"AzUpEdit", title,
        WS_POPUP|WS_CAPTION|WS_SYSMENU, x,y,w,hgt, owner,NULL,g_hInst,NULL);
    SetWindowLongPtrW(pw,GWLP_USERDATA,(LONG_PTR)&st);
    DWORD es=WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL|ES_RIGHT;
    st.edit=CreateWindowExW(0,L"EDIT",val.c_str(),es,
        S(16),S(44),w-S(32),S(36), pw,(HMENU)10, g_hInst,NULL);
    SendMessageW(st.edit,WM_SETFONT,(WPARAM)g_fUI,TRUE);
    enableAutoDir(st.edit);
    int bw=S(96), bh=S(38), gap=S(12);
    st.bOk    = createFlatButton(pw,IDOK,   L"تأیید",   ICO_CHECK,BS_PRIMARY,
        w-S(16)-bw,           S(100), bw, bh);
    st.bCancel= createFlatButton(pw,IDCANCEL,L"انصراف",ICO_X,     BS_OUTLINE,
        w-S(16)-2*bw-gap,     S(100), bw, bh);
    setFlatButtonBg(st.bOk,     g_theme.bg);
    setFlatButtonBg(st.bCancel, g_theme.bg);
    ShowWindow(pw,SW_SHOW); UpdateWindow(pw);
    SetFocus(st.edit); SendMessageW(st.edit,EM_SETSEL,0,-1);
    EnableWindow(owner,FALSE);
    MSG msg;
    while(!st.done && GetMessageW(&msg,NULL,0,0)>0){
        if(!IsDialogMessageW(pw,&msg)){ TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    EnableWindow(owner,TRUE);
    SetActiveWindow(owner);
    if(st.ok) val=st.val;
    return st.ok;
}

// metrics
// v1.10.0 — the old "میز پذیرش بیمار" header row was empty (the user/role/clock
// already live in the frame's Layer-1 header), so it only wasted ~54px of
// vertical space and forced the reception form to scroll. We collapse it to a
// thin spacer so the tabs + form shift up and the whole form fits at 1024×600
// without any scrollbar. (See requirement 3.1.)
static int infoBarH(){ return S(6); }
// v1.12.0 (§2.C): slightly slimmer tab strip + a touch more breathing room
// between tabs and from the strip edge, for a cleaner, more modern header.
static int tabBarH(){ return S(38); }
static int tabW()    { return S(188); }   // v1.92: slimmer ideal width for more tabs
static int tabMinW() { return S(84);  }   // v1.92: lower floor so more tabs fit before overflow
static int tabGap()  { return S(6);  }   // v1.92: tighter gap for responsiveness
// v1.75.0: each tab kind carries a distinct vector glyph so the strip reads as
// a set of labelled, iconified tabs rather than a row of plain text buttons.
static int tabIconFor(int kind){
    if(kind==TK_PORTAL)  return ICO_LETTER;  // کارتابل — پاکت نامه (management inbox)
    if(kind==TK_EMPTY)   return ICO_TAB;     // تب جدید — fresh blank tab
    if(kind==TK_TOOLS)   return ICO_GEAR;
    if(kind==TK_CASHIER) return ICO_WALLET;
    if(kind==TK_QUEUE)   return ICO_PEOPLE;  // صف پذیرش — people waiting
    if(kind==TK_RECEIPTS) return ICO_RECEIPT; // جستجوی قبض — receipt glyph
    if(kind==TK_DASH)    return ICO_HOME;     // داشبورد — home glyph
    if(kind==TK_BLACKLIST) return ICO_ID;     // لیست سیاه — national-id card glyph
    if(kind==TK_SVREPORT) return ICO_RECEIPT; // تفکیک خدمات — report glyph
    // v1.87.0: the admission tab carries the new person-plus glyph, echoing
    // the «پذیرش بیمار» header action so both read as one feature.
    return ICO_USER_ADD;                     // پذیرش بیمار — patient admission
}

// ------------------------------------------------- v1.89: message toast ----
//  Bottom-right glass toast for incoming کارتابل messages. LEFT click opens
//  the کارتابل tab (the current tab's state is untouched — tabs are
//  independent views); RIGHT click offers «محو» to dismiss. Severity colours:
//  حیاتی = red, فوری = amber, عادی = blue (never green, per request).
static const wchar_t* AZ_TOAST_CLS = L"AzMsgToast";

static LRESULT CALLBACK toastProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        // corners: fill with the page bg so the rounded silhouette blends
        HBRUSH bb=CreateSolidBrush(g_theme.bg); FillRect(dc,&rc,bb); DeleteObject(bb);
        KMsg* msg=(KMsg*)GetWindowLongPtrW(h,GWLP_USERDATA);
        int type = msg? msg->type : KMSG_NORMAL;
        COLORREF sev = type==KMSG_CRITICAL ? g_theme.danger
                     : type==KMSG_URGENT   ? RGB(0xE8,0x9C,0x1C)
                     :                       g_theme.accent;
        int rad=S(16);
        gpShadowColor(dc,rc,rad,S(12),72,sev);
        gpGradRoundRectBg(dc,rc,rad,g_theme.surfaceTop,g_theme.surface,
            blendColor(g_theme.border,sev,30),g_theme.bg);
        { RECT itr=rc; InflateRect(&itr,-S(1),-S(1));
          gpRoundRect(dc,itr,rad-S(1),CLR_INVALID,RGB(255,255,255),g_dark?30:110); }
        // severity stripe along the leading (RIGHT in RTL) edge
        RECT st={rc.right-S(7),rc.top+S(10),rc.right-S(2),rc.bottom-S(10)};
        gpRoundRect(dc,st,S(2),sev,CLR_INVALID);
        // envelope badge
        int isz=S(34);
        RECT ic={rc.right-S(16)-isz,rc.top+(rc.bottom-rc.top-isz)/2-S(6),
                 rc.right-S(16),rc.top+(rc.bottom-rc.top-isz)/2-S(6)+isz};
        gpGradRoundRect(dc,ic,S(11),blendColor(sev,RGB(255,255,255),24),
                        blendColor(sev,RGB(0,0,0),14),CLR_INVALID);
        RECT ici=ic; InflateRect(&ici,-S(8),-S(8));
        drawIcon(dc,ICO_LETTER,ici,RGB(255,255,255),S(2));
        SetBkMode(dc,TRANSPARENT);
        int txL=rc.left+S(14), txR=ic.left-S(10);
        // title row: «پیام جدید» + severity tag
        SelectObject(dc,g_fUIB);
        SetTextColor(dc,g_theme.text);
        RECT tr={txL,rc.top+S(10),txR,rc.top+S(10)+S(20)};
        std::wstring ttl = type==KMSG_CRITICAL ? L"پیام حیاتی"
                         : type==KMSG_URGENT   ? L"پیام فوری"
                         :                       L"پیام جدید";
        DrawTextW(dc,ttl.c_str(),-1,&tr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        // from + snippet
        SelectObject(dc,g_fSmall);
        SetTextColor(dc,g_theme.textDim);
        RECT fr={txL,tr.bottom+S(1),txR,tr.bottom+S(1)+S(16)};
        std::wstring who = msg? (L"از: "+msg->from) : L"";
        DrawTextW(dc,who.c_str(),-1,&fr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
        SetTextColor(dc,g_theme.labelInk);
        RECT sr={txL,fr.bottom+S(1),txR,rc.bottom-S(8)};
        std::wstring snip = msg? msg->text : L"";
        DrawTextW(dc,snip.c_str(),-1,&sr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    case WM_LBUTTONUP: {
        ShowWindow(h,SW_HIDE);
        Reception_OpenPortal();     // open/activate the کارتابل tab
        return 0; }
    case WM_RBUTTONUP: {
        HMENU mnu=CreatePopupMenu();
        AppendMenuW(mnu,MF_STRING,1,L"محو");
        POINT pt; GetCursorPos(&pt);
        // classic Win32 popup gotcha: without foreground focus the menu
        // auto-dismisses instantly on some systems.
        SetForegroundWindow(h);
        int cmd=TrackPopupMenu(mnu,TPM_RETURNCMD|TPM_NONOTIFY|TPM_RIGHTBUTTON,
                               pt.x,pt.y,0,h,NULL);
        PostMessage(h,WM_NULL,0,0);
        DestroyMenu(mnu);
        if(cmd==1) ShowWindow(h,SW_HIDE);   // محو — dismiss the toast only
        return 0; }
    case WM_TIMER:
        if(w==1){ KillTimer(h,1); ShowWindow(h,SW_HIDE); }
        return 0;
    case WM_DESTROY: {
        KMsg* msg=(KMsg*)GetWindowLongPtrW(h,GWLP_USERDATA);
        delete msg; SetWindowLongPtrW(h,GWLP_USERDATA,0);
        return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}

// Show (or update) the toast anchored to the reception screen's bottom-right.
static void MsgToast_Show(HWND recHwnd, const KMsg& msg, int unseen){
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc={0};
        wc.lpfnWndProc=toastProc; wc.hInstance=g_hInst;
        wc.lpszClassName=AZ_TOAST_CLS; wc.hbrBackground=NULL;
        wc.style=CS_HREDRAW|CS_VREDRAW;
        RegisterClassW(&wc); reg=true;
    }
    HWND t=FindWindowExW(recHwnd,NULL,AZ_TOAST_CLS,NULL);
    if(!t){
        t=CreateWindowExW(0,AZ_TOAST_CLS,L"",WS_CHILD,
                          0,0,10,10,recHwnd,NULL,g_hInst,NULL);
        if(!t){ logLine(L"toast: CreateWindowEx failed"); return; }
    }
    KMsg* old=(KMsg*)GetWindowLongPtrW(t,GWLP_USERDATA);
    delete old;
    SetWindowLongPtrW(t,GWLP_USERDATA,(LONG_PTR)new KMsg(msg));
    (void)unseen;
    RECT rc; GetClientRect(recHwnd,&rc);
    int tw=S(350), th=S(88);
    SetWindowPos(t,HWND_TOP, rc.right-tw-S(14), rc.bottom-th-S(14),
                 tw,th, SWP_NOACTIVATE|SWP_SHOWWINDOW);
    KillTimer(t,1);
    SetTimer(t,1,9000,NULL);   // auto-dismiss after 9 s
    InvalidateRect(t,NULL,TRUE);
    UpdateWindow(t);           // paint now, not at the next idle pass
}

// ---------------------------------------------------------------- billing --
//  The program computes the bill ITSELF: if the secretary leaves the service
//  price empty, a default tariff is derived from patient type + appointment
//  type (اورژانس/پرسنلی) and auto-filled into the price box.
static void recalc(TabPage* t){
    if(!t || !t->ePrice || !t->eDiscount) return;
    wchar_t buf[64];

    int pType = (int)SendMessageW(t->cPType,CB_GETCURSEL,0,0); if(pType<0)pType=0;
    int nType = (int)SendMessageW(t->cNType,CB_GETCURSEL,0,0); if(nType<0)nType=0;

    int insIdx  = (int)SendMessageW(t->cIns, CB_GETCURSEL,0,0);
    int suppIdx = (int)SendMessageW(t->cSupp,CB_GETCURSEL,0,0);
    if(insIdx<0  || insIdx>=N_INSURANCES) insIdx=0;
    if(suppIdx<0 || suppIdx>=N_SUPP)      suppIdx=0;
    // v1.74: honour a user-defined سهم سازمان from the «تعریف بیمه» registry —
    // Ins_Percent/Supp_Percent fall back to the hardcoded table when no def exists.
    int baseInsPct  = Ins_Percent(insIdx);   if(baseInsPct  < 0) baseInsPct  = INSURANCES[insIdx].pct;
    int baseSuppPct = Supp_Percent(suppIdx); if(baseSuppPct < 0) baseSuppPct = SUPP_INSURANCES[suppIdx].pct;

    long long price = 0;
    // ---- v1.25.0: when the خدمات table has rows, the bill is the SUM of the
    // service lines; each row's insurance/patient share is computed too so the
    // table footer can show per-column totals. Otherwise fall back to the single
    // auto-tariff price field (classic visit billing).
    if(!t->services.empty()){
        long long grand=0;
        for(auto& s : t->services){
            long long line = s.price * (s.qty>0?s.qty:1);
            long long ldisc = s.discount;
            long long baseAfterDisc = line - ldisc; if(baseAfterDisc<0) baseAfterDisc=0;
            s.insShare = baseAfterDisc * baseInsPct / 100;
            s.patShare = baseAfterDisc - s.insShare;
            grand += line;
        }
        price = grand;
    } else {
        GetWindowTextW(t->ePrice,buf,64);
        price = parseMoney(buf);
        // auto-fill default tariff when the field is empty / zero
        if(price <= 0){
            price = defaultServicePrice(pType, nType);
            std::wstring pf = toFaDigits(formatMoney(price));
            wchar_t cur[64]; GetWindowTextW(t->ePrice,cur,64);
            if(parseMoney(cur)!=price){
                t->autoPrice = true;
                SetWindowTextW(t->ePrice, pf.c_str());
                t->autoPrice = false;
            }
        }
    }

    GetWindowTextW(t->eDiscount,buf,64);
    long long disc = parseMoney(buf);
    // add per-row service discounts to the header discount
    for(auto& s : t->services) disc += s.discount;

    t->total      = price;
    t->mainShare  = price * baseInsPct / 100;
    t->baseDiff   = price - t->mainShare;                 // مابه‌التفاوت پایه
    t->orgShare   = t->baseDiff * baseSuppPct / 100;
    t->patientShare = t->baseDiff - t->orgShare;
    long long pay = t->patientShare - disc;
    if(pay < 0) pay = 0;
    t->paid = pay;
}

// ------------------------------------------------------------- tab layout --
//  v1.18.0 — PIXEL-MATCH REDESIGN of the reception screen to the reference
//  mock-up. Three columns (RTL):
//     ┌─ billing (LEFT) ─┐ ┌──── center form (WIDE) ────┐ ┌─ info (RIGHT) ─┐
//     │ صورتحساب          │ │  «مشخصات بیمار» card        │ │ avatar/بیمه/.. │
//     │ چک پرداختی        │ │  «بیمه و نوبت» card         │ │                │
//     │ چاپ buttons       │ │  services table             │ │                │
//     └──────────────────┘ │  4 action buttons row       │ └────────────────┘
//                          └─────────────────────────────┘
//  The center column is built from two stacked white sub-cards (3-column grid
//  for identity/contact, 4-column for appointment/insurance), a services table
//  with an empty-state, and a row of four action buttons. The exact spacing,
//  border radii, label-with-red-asterisk and light input wells reproduce the
//  reference design.
//  v1.27.0 UI CORRECTION — match the approved reference proportions:
//  outer margin 16, gap between cards 14, internal card padding 16. The right
//  sidebar is wider (~250px) and the left billing panel ~200px, so the CENTER
//  keeps ~70% of the width (reference structure).
static const int RC_OUT = 16;   // outer page padding
static const int RC_GAP = 14;   // gap between the three columns
static const int RC_IN  = 16;   // inner card padding (center cards)
static const int BILL_W = 200;  // billing card width  (LEFT)
static const int INFO_W = 250;  // right info-panel width

//  ----- horizontal metrics: the three column bounds + center grid columns.
//  cx[0..2] = LEFT edges of the 3 center columns (col 0 is the RIGHT-most in
//  RTL reading order); ccolW = width of each center column.
struct RecH {
    int billL, billR;        // LEFT billing card
    int infoL, infoR;        // RIGHT info panel
    int cardL, cardR;        // center column outer bounds
    int formL, formR;        // center inner (padded) bounds
    int fw;                  // inner form width
    int ccolW;               // one center grid column width (of 3)
    int cgap;                // gap between center grid columns
    bool stacked;            // narrow screen → drop side panels
};
static void rcH(int W, RecH& m){
    int pad=S(RC_OUT);
    m.billL = pad;
    m.billR = m.billL + S(BILL_W);
    m.infoR = W - pad;
    m.infoL = m.infoR - S(INFO_W);
    m.cardL = m.billR + S(RC_GAP);
    m.cardR = m.infoL - S(RC_GAP);
    m.stacked = (m.cardR - m.cardL) < S(420);
    if(m.stacked){
        m.billL=m.billR=0; m.infoL=m.infoR=0;
        m.cardL=pad; m.cardR=W-pad;
    }
    m.formL = m.cardL + S(RC_IN);
    m.formR = m.cardR - S(RC_IN);
    m.fw    = m.formR - m.formL;
    m.cgap  = S(14);
    m.ccolW = (m.fw - 2*m.cgap) / 3;
}
//  Back-compat shim used by a couple of remaining callers (old signature).
static void rcMetrics2(int W, int& cardL, int& cardR, int& billL, int& billR,
                       int& infoL, int& infoR, int& colW, int& xr, int& xl,
                       bool& stacked){
    RecH m; rcH(W,m);
    cardL=m.cardL; cardR=m.cardR; billL=m.billL; billR=m.billR;
    infoL=m.infoL; infoR=m.infoR; stacked=m.stacked;
    colW=m.ccolW; xr=m.formR-m.ccolW; xl=m.formL;
}
//  v1.26.0: horizontal bounds of the middle THREE cards (RTL, matches the
//  reference image): انجام دهنده (rightmost ¼) | پزشک معالج (¼) | بیمه و نوبت
//  (left, the remaining ~½ width). Shared by painter + positioner.
struct MidCards { int perfL,perfR, docL,docR, insL,insR; };
static void rcMidCards(const RecH& m, MidCards& c){
    int gap=S(RC_GAP);
    int quarter=(m.cardR-m.cardL-2*gap)/4;
    c.perfR=m.cardR;      c.perfL=c.perfR-quarter;
    c.docR =c.perfL-gap;  c.docL =c.docR-quarter;
    c.insR =c.docL-gap;   c.insL =m.cardL;
}
//  v1.26.0: the TWO side-by-side bottom panels — صندوق نرفته‌ها/صف پذیرش (LEFT)
//  and جدول خدمات (RIGHT) — reference image proportions (~52 / 48).
struct BotPanels { int upL,upR, svL,svR; };
static void rcBotPanels(const RecH& m, BotPanels& b){
    int gap=S(RC_GAP);
    int wAll=m.cardR-m.cardL-gap;
    int upW=wAll*52/100;
    b.upL=m.cardL; b.upR=m.cardL+upW;
    b.svL=b.upR+gap; b.svR=m.cardR;
}

//  ----- vertical layout of the center column (single source of truth shared
//  by the painter and the control positioner). All y are virtual (pre-scroll).
//  v1.26.0 PIXEL-MATCH: the reference image packs the whole admission page on
//  ONE screen (no scrolling needed). The vertical plan (top → bottom):
//    1) «اطلاعات بیمار»           — 3 rows × 3 columns
//    2) three side-by-side cards — انجام دهنده (right) | پزشک معالج |
//       بیمه و نوبت (left, widest: 2 rows × 4 fields)
//    3) a slim scheduling strip   — تاریخ/شیفت نوبت + P/S preview + P/S
//    4) TWO side-by-side panels   — صندوق نرفته‌ها/صف پذیرش (LEFT) |
//       خدمات table (RIGHT) — both stretch to fill the remaining height
//    5) bottom bar — پاک کردن + انصراف (left) … ثبت پذیرش (right)
struct CenterV {
    int rh;                  // input height
    int lbl;                 // label band height
    // card 1 «اطلاعات بیمار»
    int c1Top, c1Bot;
    int r1y[3];              // input top of the 3 identity rows
    // row of THREE cards (انجام دهنده | پزشک معالج | بیمه و نوبت)
    int dpTop, dpBot;
    int dpR1y, dpR2y, dpR3y; // shared row baselines inside the 3 cards
    // scheduling strip (single row)
    int apTop, apBot, apR1y;
    // TWO bottom panels (shared vertical bounds)
    int bTop, bBot;
    //   services panel internals (RIGHT half)
    int svcToolY;            // header row (خدمات title + افزودن خدمت btn)
    int svcPanelY;           // inline add-service row (0 when closed)
    int svcHeadY, svcBodyY, svcBodyBot, svcFootY;
    //   unpaid-box panel internals (LEFT half)
    int upTabY, upToolY, upHeadY, upBodyY, upBodyBot, upFootY;
    // bottom buttons row
    int btnY, btnH;
    int totalBot;            // total content bottom (== H when it fits)
    int svcRows;             // number of service rows
    bool panelOpen;
    double fitF;             // v1.31.0 fit-factor used (labels/titles scale by it)
};
static void computeCenterV(const RecH& m, CenterV& v, const TabPage* t, int H){
    (void)m;
    //  v1.27.0 UI CORRECTION — match the approved reference pixel-for-pixel:
    //    • control height 38px (textboxes == comboboxes)
    //    • label band 24px above each control = 13px medium label + 6px gap
    //    • row gap 12px, column gap 12px, internal card padding 16px
    //    • the TOP HALF (patient info + 3 cards) is given generous height so it
    //      is NOT compressed; the two bottom tables fill the remaining space.
    //  v1.29.0 UI CORRECTION — pixel-match the approved reference AND guarantee
    //  the WHOLE admission page fits inside one frame with NO page scrolling:
    //    • top «اطلاعات بیمار» card ≈ 25% (3 rows × 3 cols)
    //    • middle row of THREE cards ≈ 25%  — reference has only TWO field rows
    //      (انجام دهنده = code + name; پزشک معالج = code + name; بیمه و نوبت =
    //       row1 four combos + row2 «درصد بیمه تکمیل»). The 3rd row of the doctor
    //       card (the DUPLICATE «درصد بیمه مکمل») is REMOVED.
    //    • a very slim scheduling strip (تاریخ/شیفت نوبت)
    //    • the two bottom tables fill the remaining ≈40% and never overflow.
    //  v1.30.0 RESPONSIVE FIX — the center column now measures its own natural
    //  height (patient card + the row of three cards + the two bottom tables +
    //  action buttons) and shrinks its row heights / gaps by ONE fit-factor when
    //  that natural height would exceed the frame. This guarantees the whole
    //  admission page fits on ONE frame at every resolution — no bottom overflow,
    //  no clipped controls, no page scrolling — while giving controls generous,
    //  readable heights (40px) and clear label→control spacing whenever there is
    //  room. The two bottom tables always absorb the remaining slack so they
    //  stretch to the buttons row without pushing anything off-screen.

    //  Two bottom tables need at least this much to stay usable (header + a few
    //  rows + footer). Everything above them is the "fixed" region we may shrink.
    auto plan=[&](double f)->int{
        auto SF=[&](int px){ int r=(int)(S(px)*f+0.5); return r<1?1:r; };
        const int rh   = SF(40);   // control height (textbox == combobox)
        const int lbl  = SF(22);   // label band (13-14px label + 6-8px gap)
        const int rgap = SF(12);   // gap between grid rows
        const int hdr1 = SF(38);   // patient-card header band (title + divider)
        const int hdr3 = SF(38);   // 3-card header band
        const int gap  = SF(RC_GAP);
        const int out  = SF(RC_OUT);
        v.rh = rh; v.lbl = lbl;
        int y = out;
        // ----- card 1: اطلاعات بیمار (3 rows × 3 cols) -----
        v.c1Top = y;
        int rowsTop = y + hdr1;
        for(int i=0;i<3;i++) v.r1y[i] = rowsTop + lbl + i*(lbl+rh+rgap);
        v.c1Bot = v.r1y[2] + rh + SF(14);
        y = v.c1Bot + gap;
        // ----- row of THREE cards (TWO field rows only) -----
        v.dpTop = y;
        int dpRowsTop = y + hdr3;
        v.dpR1y = dpRowsTop + lbl;
        v.dpR2y = v.dpR1y + rh + rgap + lbl;
        v.dpR3y = v.dpR2y;               // 3rd row RETIRED (no dup percentage)
        v.dpBot = v.dpR2y + rh + SF(14);
        y = v.dpBot + gap;
        v.apTop = v.dpBot; v.apR1y = v.dpR2y; v.apBot = v.dpBot;
        // ----- bottom action buttons + the two tables region -----
        v.btnH = SF(44);
        v.bTop = y;
        v.btnY = 0;                      // filled by caller after v.bBot known
        // natural height demanded by the fixed region + a minimum table band
        int minTable = SF(180);
        return v.bTop + minTable + gap + v.btnH + out;
    };

    v.panelOpen = t ? t->svcPanelOpen : false;
    v.svcRows   = t ? (int)t->services.size() : 0;

    double usedF = 1.0;
    int natural = plan(1.0);
    if(natural > H){
        double f = (double)H / (double)natural;
        if(f<0.66) f=0.66;               // never crush below readable
        if(f>1.0)  f=1.0;
        usedF = f;
        plan(f);
    }
    v.fitF = usedF;
    // v1.31.0 GUARANTEE: the label band must always be tall enough to hold the
    // (fit-scaled) 13px label font PLUS a real 5-6px gap to the control below.
    // Even at the smallest fit-factor a label can then never be clipped or sit
    // under the textbox. If the plan's band is too small we grow it and reflow
    // every row baseline downwards by the delta (which the two bottom tables
    // absorb, so nothing overflows the frame).
    { int lblFont=(int)(S(13)*usedF+0.5); if(lblFont<9) lblFont=9;
      int minBand=lblFont+(int)(S(6)*usedF+0.5)+2;   // font + gap + 2px safety
      if(v.lbl<minBand){
          int d=minBand-v.lbl;
          v.lbl=minBand;
          // push the identity rows / three-card rows down by the extra band so
          // the extra label space is real (labels are drawn at r1y - v.lbl).
          for(int i=0;i<3;i++) v.r1y[i]+=d*(i+1);
          v.c1Bot+=d*3;
          int shift=d*3;
          v.dpTop+=shift; v.dpR1y+=shift+d; v.dpR2y+=shift+2*d; v.dpR3y=v.dpR2y;
          v.dpBot+=shift+2*d;
          v.apTop=v.dpBot; v.apR1y=v.dpR2y; v.apBot=v.dpBot;
          v.bTop+=shift+2*d;
      }
    }

    // ----- the two bottom tables absorb all remaining vertical slack -----
    const int gap = (int)(S(RC_GAP)*((double)v.rh/S(40))+0.5);
    const int out = (int)(S(RC_OUT)*((double)v.rh/S(40))+0.5);
    int wantBot = H - out - v.btnH - gap;            // panels end above buttons
    int minBot  = v.bTop + (int)(S(180)*((double)v.rh/S(40))+0.5);
    v.bBot = wantBot > minBot ? wantBot : minBot;
    //  v1.31.0 COLLAPSE: when BOTH bottom lists are collapsed the panel region is
    //  capped to a compact height (header + tabs + toolbar + ~3 rows + footer) so
    //  the whole admission page reads as one tidy frame with lots of breathing
    //  room and the action buttons pulled up right beneath it. When either list
    //  is expanded the region grows back to fill the frame (and only THEN may its
    //  body scroll internally if the list is genuinely long).
    {
        int rowH=S(32);
        bool bothCollapsed = (t? (t->svcCollapsed && t->upCollapsed) : false);
        if(bothCollapsed){
            // header band up to body start (mirror the internals computed below)
            int toBody = S(14) + S(32) + S(10) + v.rh + S(12) + S(40);
            int compact = v.bTop + toBody + 3*rowH + S(8) + S(36);
            if(compact < v.bBot) v.bBot = compact;
        }
    }
    // services panel internals
    v.svcToolY = v.bTop + S(14);
    int sy2 = v.svcToolY + v.rh + S(12);
    if(v.panelOpen){ v.svcPanelY = sy2; sy2 += v.rh + S(12); }
    else            v.svcPanelY = 0;
    v.svcHeadY = sy2;
    v.svcBodyY = v.svcHeadY + S(40);
    v.svcFootY = v.bBot - S(36);
    v.svcBodyBot = v.svcFootY - S(4);
    if(v.svcBodyBot < v.svcBodyY) v.svcBodyBot = v.svcBodyY;
    // unpaid panel internals
    v.upTabY  = v.bTop + S(14);
    v.upToolY = v.upTabY + S(32) + S(10);
    v.upHeadY = v.upToolY + v.rh + S(12);
    v.upBodyY = v.upHeadY + S(40);
    v.upFootY = v.bBot - S(34);
    v.upBodyBot = v.upFootY - S(4);
    if(v.upBodyBot < v.upBodyY) v.upBodyBot = v.upBodyY;
    // ----- bottom buttons row -----
    v.btnY = v.bBot + gap;
    v.totalBot = v.btnY + v.btnH + out;
    if(v.totalBot < H) v.totalBot = H;
}
//  Natural full height the center form needs (for the scroll decision). When
//  the client is tall enough everything fits and this returns exactly H — the
//  “no scrolling needed” requirement of the reference design.
static int rcFormContentH(int W, int H, const TabPage* t){
    RecH m; rcH(W>0?W:S(1024),m);
    CenterV v; computeCenterV(m,v,t,H>0?H:S(640));
    return v.totalBot;
}
// ----------------------------------------------------------------------------
//  Unified RIGHT INFO-PANEL layout.  A SINGLE source of truth shared by the
//  painter (paintInfoPanel) and the control positioner (tabPageLayout) so the
//  painted group titles / chips and the real edit controls can NEVER drift
//  apart and overlap (the v1.9.x right-panel overlap bug).  All y values are in
//  device pixels and already scaled.  Every row reserves its own vertical band
//  with a small label line above each control band so labels never sit under or
//  behind a field.
struct InfoLayout {
    int iL, iR, iw;                 // inner padded bounds
    int cardTop, cardBot;
    int avCx, avCy, avR;            // avatar centre + radius
    int capY;                       // «بیمار جدید» caption top (v1.31.0)
    int chipY, chipH;               // نسخه الکترونیک chip
    int boxY,  boxH;                // قبض/بارکد box
    int psY,   psH;                 // P:S counters line
    double fitF;                    // v1.31.0 fit-factor (painter scales fonts)
    // group "کلیدهای جستجو"
    int g1TitleY;
    int archiveLblY, archiveY;
    int fileLblY,    fileY;
    // group "بیمه"
    int g2TitleY;
    int bookLblY,  bookY;
    int validLblY, validY;
    int rxLblY,    rxY;
    int suppLblY,  suppY;
    // group "پزشک معالج"
    int g3TitleY;
    int docCodeLblY, docCodeY;
    int docNameLblY, docNameY;
    int rh2, gp, btnW, lblH;
};
static void computeInfoLayout(int infoL, int infoR, int H, InfoLayout& L){
    //  v1.30.0 RESPONSIVE FIX — the RIGHT info panel is the tallest column and
    //  used to overflow the frame bottom (clipping «پزشک معالج») and force the
    //  whole page to scroll. It now measures its own natural height and, when
    //  that exceeds the available panel height, shrinks its row heights and gaps
    //  by a single fit-factor so the LAST control always sits above cardBot with
    //  a safe margin. This guarantees the panel fits on ONE frame at 1366×768,
    //  1600×900 and 1920×1080 with no clipping and no page scrolling, while
    //  keeping generous, readable spacing whenever there is room.
    int ipad=S(12);
    L.iL=infoL+ipad; L.iR=infoR-ipad; L.iw=L.iR-L.iL;
    L.cardTop=S(RC_OUT); L.cardBot=H-S(RC_OUT);
    int avail = L.cardBot - L.cardTop;                 // usable panel height

    //  Pass 1: lay the panel out at the PREFERRED (generous) metrics, then
    //  measure the natural bottom. Pass 2 re-runs with a shrink factor if needed.
    auto build=[&](double f)->int{
        L.fitF=f;
        auto SF=[&](int px){ int r=(int)(S(px)*f+0.5); return r<1?1:r; };
        L.rh2 = SF(38); L.gp = SF(6); L.btnW = S(52);
        //  v1.31.0: the label band must always hold the (fit-scaled) 13px label
        //  font, so it is at least lblFont+2px. This is what keeps the right-panel
        //  captions (شماره دفترچه / تاریخ اعتبار / …) from being clipped.
        int lblFont=(int)(S(13)*f+0.5); if(lblFont<9) lblFont=9;
        L.lblH = SF(16); if(L.lblH<lblFont+2) L.lblH=lblFont+2;
        // --- header zone (matches painter) ---
        //  v1.32.0 PROFILE CARD (script items #10/#15): with the duplicate doctor
        //  group removed the right panel has room to breathe. Give the avatar a
        //  generous top inset, two clear caption lines, a full-width نسخه chip and
        //  taller P·S badges so nothing is cramped and it matches the clean
        //  reference «بیمار جدید» card.
        L.avR = SF(28); L.avCx=(L.iL+L.iR)/2; L.avCy=L.cardTop+SF(18)+L.avR;
        int y=L.avCy+L.avR+SF(12);
        L.capY=y;
        y += SF(42);                     // «بیمار جدید» + «بدون سابقه» two lines
        L.chipH=SF(28); L.chipY=y;       y+=L.chipH+SF(12);
        L.boxH =SF(44); L.boxY =y;       y+=L.boxH +SF(12); // two counter boxes
        //  P (yellow) / S (green) squares — the SINGLE location, right under the
        //  «نسخه الکترونیک» area (all duplicates elsewhere were removed).
        L.psH  =SF(48); L.psY  =y;       y+=L.psH  +SF(16);
        int lblGap=SF(6);   // label → its control (never < 4px)
        if(lblGap<4) lblGap=4;
        int rowGap=SF(12);  // control row → next control row
        int grpGap=SF(18);  // gap before next group title
        //  v1.32.0: the group-title band must clear the painted title (S(18)*f) +
        //  its divider (+2) with a real gap before the first field label, so the
        //  divider never sits on top of «شماره بایگانی / شماره دفترچه».
        int titleH=SF(30);
        auto labelled=[&](int& lblY,int& ctlY){
            lblY=y; y+=L.lblH+lblGap; ctlY=y; y+=L.rh2+rowGap;
        };
        // group 1 — جستجو و فیلتر
        L.g1TitleY=y; y+=titleH;
        labelled(L.archiveLblY,L.archiveY);
        labelled(L.fileLblY,   L.fileY);
        y+=grpGap-rowGap;
        // group 2 — بیمه
        L.g2TitleY=y; y+=titleH;
        labelled(L.bookLblY, L.bookY);
        labelled(L.validLblY,L.validY);
        labelled(L.rxLblY,   L.rxY);
        labelled(L.suppLblY, L.suppY);
        //  v1.32.0 DE-DUPLICATION (script item #9): the right panel used to carry a
        //  SECOND «جستجوی پزشک معالج» group (code + name) that duplicated the
        //  «پزشک معالج» card in the CENTER column. That duplicate is REMOVED — the
        //  doctor is selected ONCE, in the center. The old group-3 baselines are
        //  parked off-panel so any stray reference stays harmless, and the freed
        //  vertical room lets the profile header (avatar / P·S / نسخه) breathe and
        //  match the clean reference.
        L.g3TitleY=y; L.docCodeLblY=y; L.docCodeY=y; L.docNameLblY=y; L.docNameY=y;
        return y + SF(14);                // natural bottom (last control + pad)
    };
    int natural = build(1.0);
    if(natural > L.cardBot){
        // shrink uniformly so the natural bottom == cardBot (with a tiny margin)
        int headroom = avail;            // full panel height is the target
        int growable = natural - L.cardTop;
        double f = growable>0 ? (double)headroom / (double)growable : 1.0;
        if(f<0.62) f=0.62;               // never crush below readable
        if(f>1.0)  f=1.0;
        build(f);
    }
}

//  v2: the virtual page height — the taller of the visible client area and the
//  natural content height of the form + the right info-panel. When the content
//  is taller than the client area the page scrolls; the cards grow to this VH so
//  their rounded bottom edge is always below the last control.
static int recPrintGroupTop();   // fwd (defined below; shared by VH + layout)
static int recPageVH(int W, int H, const TabPage* t){
    RecH m; rcH(W,m);
    int formH = rcFormContentH(W,H,t);
    int infoH = 0;
    if(!m.stacked && m.infoR>m.infoL){
        InfoLayout L; computeInfoLayout(m.infoL,m.infoR,H,L);
        infoH = L.docNameY + L.rh2 + S(28);   // last control + bottom room
    }
    //  billing column height = print-group title + 3 buttons + bottom padding.
    int billH = recPrintGroupTop() + S(22) + 3*S(44) + S(16);
    int need = formH; if(infoH>need) need=infoH; if(billH>need) need=billH;
    return need>H ? need : H;
}
//  v1.18.3: single source of truth for the «چاپ» (print) group geometry on the
//  LEFT billing column. The reference image places the three print buttons
//  DIRECTLY BELOW the blue «مبلغ نهایی» card (not pinned to the page bottom).
//  Both the painter (WM_PAINT, draws the «چاپ» title) and the control
//  positioner (tabPageLayout, moves the 3 buttons) call this so they always
//  agree. Returns the absolute (pre-scroll) y of the «چاپ» group title; the
//  buttons start S(24) below it.
static int recPrintGroupTop(){
    //  v1.26.0 grouped «صورت حساب» summary: header (30) + three groups
    //  (بیمه اصلی / بیمه مکمل / مبلغ نهایی: title 20 + 3 rows × 20 each)
    //  + «مانده قابل پرداخت» highlight row (26) + bottom pad (8).
    int sumTop = S(RC_OUT);
    int sumBot = sumTop + S(30) + 3*(S(20)+3*S(20)) + S(26) + S(8);
    int faTop  = sumBot + S(10);                      // blue total card top
    int faBot  = faTop  + S(62);                      // ... and its bottom
    return faBot + S(12);                             // «چاپ» title just below
}
//  Clamp & return the current scroll offset for the page.
static int recClampScroll(HWND h, TabPage* t){
    RECT rc; GetClientRect(h,&rc);
    int VH=recPageVH(rc.right,rc.bottom,t);
    int maxS = VH - rc.bottom; if(maxS<0) maxS=0;
    if(t->scrollY<0) t->scrollY=0;
    if(t->scrollY>maxS) t->scrollY=maxS;
    t->contentH=VH;
    return maxS;
}
//  Sync the WS_VSCROLL thumb to the current scroll state.
//  v1.29.0: when the whole page fits (VH<=client height) the vertical scrollbar
//  is HIDDEN entirely so the admission screen reads as a single, fixed frame
//  (the reference «no page scrolling» requirement). It reappears automatically
//  only if a very small display would otherwise clip content.
static void recUpdateScrollbar(HWND h, TabPage* t){
    RECT rc; GetClientRect(h,&rc);
    int VH=recPageVH(rc.right,rc.bottom,t);
    bool needScroll = VH > rc.bottom;
    ShowScrollBar(h, SB_VERT, needScroll);
    if(!needScroll){ t->scrollY=0; return; }
    SCROLLINFO si={sizeof(si)};
    si.fMask=SIF_RANGE|SIF_PAGE|SIF_POS;
    si.nMin=0; si.nMax=VH-1; si.nPage=rc.bottom; si.nPos=t->scrollY;
    SetScrollInfo(h,SB_VERT,&si,TRUE);
}
static void tabPageLayout(HWND h, TabPage* t){
    if(!t) return;
    // §3 hybrid host: if the HTML/CSS/JS surface is up it owns the whole tab.
    // v2.02: WebAdmission_Resize also updates the engine's composition bounds
    // (WebView2 put_Bounds / MSHTML SetObjectRects). MoveWindow alone left the
    // Chromium surface at its create-time size, so پذیرش looked tiny and
    // centred inside the C++ host with gaps on the sides and bottom.
    if(t->web){
        RECT rc; GetClientRect(h,&rc);
        WebAdmission_Resize(t->web, rc.right, rc.bottom);
        return;
    }
    if(t->kind!=TK_RECEPTION) return;   // painted pages have no controls
    RECT rc; GetClientRect(h,&rc);
    int W=rc.right, H=rc.bottom;
    if(W<=0 || H<=0) return;
    RecH m; rcH(W,m);
    CenterV v; computeCenterV(m,v,t,H);
    recClampScroll(h,t);
    const int sy=t->scrollY;          // scroll offset (subtract from every y)
    recUpdateScrollbar(h,t);

    const int rh = v.rh;
    const int cgap = m.cgap, cw = m.ccolW;
    // RTL: column 0 sits at the RIGHT, column 2 at the LEFT.
    auto colX=[&](int c){ return m.formR - (c+1)*cw - c*cgap; };
    const int cx0 = colX(0), cx1 = colX(1), cx2 = colX(2);
    #define Y(yy) ((yy)-sy)

    // ===== card 1: اطلاعات بیمار (3-column grid) =====
    // row0: نام(col0) | نام خانوادگی(col1) | کد ملی(col2)
    MoveWindow(t->eFirst, cx0, Y(v.r1y[0]), cw, rh, TRUE);
    MoveWindow(t->eLast,  cx1, Y(v.r1y[0]), cw, rh, TRUE);
    MoveWindow(t->eNid,   cx2, Y(v.r1y[0]), cw, rh, TRUE);
    // row1: نام پدر(col0) | تاریخ تولد(col1) | جنسیت(col2)
    MoveWindow(t->eFather,cx0, Y(v.r1y[1]), cw, rh, TRUE);
    MoveWindow(t->eBirth, cx1, Y(v.r1y[1]), cw, rh, TRUE);
    MoveWindow(t->cGender,cx2, Y(v.r1y[1]), cw, S(200), TRUE);
    // row2: شماره موبایل(col0) | تلفن ثابت(col1) | آدرس(col2)
    MoveWindow(t->eMobile,cx0, Y(v.r1y[2]), cw, rh, TRUE);
    MoveWindow(t->ePhone, cx1, Y(v.r1y[2]), cw, rh, TRUE);
    MoveWindow(t->eAddr,  cx2, Y(v.r1y[2]), cw, rh, TRUE);

    // ===== middle row: THREE cards — انجام دهنده | پزشک معالج | بیمه و نوبت ==
    {
        MidCards mc; rcMidCards(m,mc);
        int in=S(10);
        // — انجام دهنده (rightmost ¼): code + name (2 rows)
        int pfL=mc.perfL+in, pfW=mc.perfR-in-pfL;
        MoveWindow(t->ePerfCode, pfL, Y(v.dpR1y), pfW, rh, TRUE);
        MoveWindow(t->cPerfList, pfL, Y(v.dpR2y), pfW, S(240), TRUE);
        // — پزشک معالج (¼): code + name (2 rows). The 3rd «درصد بیمه مکمل»
        //   row is REMOVED — the SINGLE complementary-percentage field lives in
        //   the «بیمه و نوبت» card (eSuppPctIns). eSuppPct2 is kept alive but
        //   hidden so any legacy read still works.
        int dcL=mc.docL+in, dcW=mc.docR-in-dcL;
        MoveWindow(t->eDoc2Code, dcL, Y(v.dpR1y), dcW, rh, TRUE);
        MoveWindow(t->cDoc2List, dcL, Y(v.dpR2y), dcW, S(240), TRUE);
        MoveWindow(t->eSuppPct2, 0, 0, 0, 0, FALSE);
        ShowWindow(t->eSuppPct2, SW_HIDE);
        // — بیمه و نوبت (left ~½): 2 rows × 4 fields (RTL inside the card)
        int inL=mc.insL+in, inR=mc.insR-in;
        int gw=(inR-inL-3*cgap)/4;
        auto gx=[&](int c){ return inR-(c+1)*gw-c*cgap; };
        // row1: نوع پذیرش | نوع نوبت | نوع بیمه (پایه) | بیمه تکمیلی
        MoveWindow(t->cPType, gx(0), Y(v.dpR1y), gw, S(200), TRUE);
        MoveWindow(t->cNType, gx(1), Y(v.dpR1y), gw, S(200), TRUE);
        MoveWindow(t->cIns,   gx(2), Y(v.dpR1y), gw, S(240), TRUE);
        MoveWindow(t->cSupp,  gx(3), Y(v.dpR1y), gw, S(240), TRUE);
        // row2 (RTL inside «بیمه و نوبت»):
        //   col3 → درصد بیمه تکمیل ٪  (the ONE complementary-percentage field)
        //   col2 → شیفت نوبت
        //   col1 → تاریخ نوبت
        //   col0 → (free)
        //  The separate scheduling strip is RETIRED (v1.29.0) so the whole page
        //  fits on one frame with no scrolling. All manual PRICE fields belong to
        //  Management and are hidden (ePrice/eDiscount kept alive for the
        //  auto-tariff billing recalc).
        MoveWindow(t->eSuppPctIns, gx(3), Y(v.dpR2y), gw, rh, TRUE);
        ShowWindow(t->eSuppPctIns, SW_SHOW);
        MoveWindow(t->cApptShift, gx(2), Y(v.dpR2y), gw, S(200), TRUE);
        MoveWindow(t->eApptDate,  gx(1), Y(v.dpR2y), gw, rh, TRUE);
        ShowWindow(t->cApptShift, SW_SHOW);
        ShowWindow(t->eApptDate,  SW_SHOW);
        MoveWindow(t->ePrice,    0, 0, 0, 0, FALSE);
        MoveWindow(t->eDiscount, 0, 0, 0, 0, FALSE);
        ShowWindow(t->ePrice,    SW_HIDE);
        ShowWindow(t->eDiscount, SW_HIDE);
        // hide the duplicate P/S numeric wells that used to sit in the strip
        MoveWindow(t->eApptS, 0,0,0,0, FALSE);
        MoveWindow(t->eApptP, 0,0,0,0, FALSE);
        ShowWindow(t->eApptS, SW_HIDE);
        ShowWindow(t->eApptP, SW_HIDE);
    }
    ShowWindow(t->chkIns, SW_HIDE);

    // ===== bottom RIGHT panel: خدمات (toolbar + optional inline add row) =====
    {
        BotPanels bp; rcBotPanels(m,bp);
        int in=S(10);
        int svL=bp.svL+in, svR=bp.svR-in, svW=svR-svL;
        //  v1.31.0: widen «افزودن خدمت» so the icon + full caption are never
        //  clipped (script item #12). The button font is one step smaller so the
        //  wider caption still reads cleanly at every scale.
        int addW=S(132);
        MoveWindow(t->bSvcAdd, svL, Y(v.svcToolY), addW, rh, TRUE);
        ShowWindow(t->bSvcAdd,SW_SHOW);
        ShowWindow(t->eSvcSearch,SW_HIDE);      // search box lives on the LEFT panel
        if(v.panelOpen){
            // inline add row inside the services panel (compact widths)
            int codeW=S(56), qtyW=S(42), freeChkW=S(64), freeAmtW=S(78), addBtnW=S(64);
            int nameW = svW-(codeW+qtyW+freeChkW+freeAmtW+addBtnW+5*S(4));
            if(nameW<S(70)) nameW=S(70);
            int g=S(4), x=svR;
            x-=codeW;          MoveWindow(t->eSvcCode,   x, Y(v.svcPanelY), codeW, rh, TRUE);
            x-=g+nameW;        MoveWindow(t->eSvcName,   x, Y(v.svcPanelY), nameW, rh, TRUE);
            x-=g+qtyW;         MoveWindow(t->eSvcQty,    x, Y(v.svcPanelY), qtyW,  rh, TRUE);
            x-=g+freeChkW;     MoveWindow(t->chkSvcFree, x, Y(v.svcPanelY), freeChkW, rh, TRUE);
            x-=g+freeAmtW;     MoveWindow(t->eSvcFreeAmt,x, Y(v.svcPanelY), freeAmtW, rh, TRUE);
            x-=g+addBtnW;      MoveWindow(t->bSvcConfirm,x, Y(v.svcPanelY), addBtnW, rh, TRUE);
            ShowWindow(t->eSvcCode,SW_SHOW); ShowWindow(t->eSvcName,SW_SHOW);
            ShowWindow(t->eSvcQty,SW_SHOW);  ShowWindow(t->chkSvcFree,SW_SHOW);
            ShowWindow(t->eSvcFreeAmt,SW_SHOW); ShowWindow(t->bSvcConfirm,SW_SHOW);
        } else {
            ShowWindow(t->eSvcCode,SW_HIDE); ShowWindow(t->eSvcName,SW_HIDE);
            ShowWindow(t->eSvcQty,SW_HIDE);  ShowWindow(t->chkSvcFree,SW_HIDE);
            ShowWindow(t->eSvcFreeAmt,SW_HIDE); ShowWindow(t->bSvcConfirm,SW_HIDE);
        }

        // ===== bottom LEFT panel: صندوق نرفته‌ها / صف پذیرش toolbar =====
        int upL=bp.upL+in, upR=bp.upR-in;
        int refW=S(30), hrsW=S(52), dtW=S(88);
        // RTL order: [search………] [تاریخ تا] [ساعت▼] [⟳]  — search fills the rest
        int upW=upR-upL;
        int fchip=(upW>S(360))?S(48):0;     // painted «فیلتر» chip (see WM_PAINT)
        int x2=upL+(fchip?fchip+S(8):0);
        MoveWindow(t->bUpRefresh, x2, Y(v.upToolY), refW, rh, TRUE); x2+=refW+S(6);
        MoveWindow(t->cUpHours,   x2, Y(v.upToolY), hrsW, S(200), TRUE); x2+=hrsW+S(6);
        MoveWindow(t->eUpDate,    x2, Y(v.upToolY), dtW, rh, TRUE); x2+=dtW+S(6);
        int seW=upR-x2; if(seW<S(80)) seW=S(80);
        MoveWindow(t->eSvcSearch, x2, Y(v.upToolY), seW, rh, TRUE);
        ShowWindow(t->eSvcSearch,SW_SHOW);
        ShowWindow(t->bUpRefresh,SW_SHOW); ShowWindow(t->cUpHours,SW_SHOW);
        ShowWindow(t->eUpDate,SW_SHOW);
        // «افزودن به صندوق نرفته‌ها / صف پذیرش» — the blue link row under the
        // table footer is painted; the real buttons are hidden (their actions
        // are wired to the painted link via hit-testing in WM_LBUTTONDOWN).
        ShowWindow(t->bAddUnpaid,SW_HIDE); ShowWindow(t->bAddQueue,SW_HIDE);
        ShowWindow(t->chkNoPay,SW_HIDE);
    }

    // ===== bottom action bar: پاک کردن + انصراف + پذیرش بیمار (LEFT) … ثبت (RIGHT) =====
    //  v1.75.0: «پذیرش بیمار» is shown in the row again as a modern iconified
    //  button (clears the ACTIVE tab's form for a new patient). It only appears
    //  when the row is wide enough to hold it without crowding the primary ثبت
    //  button; on a narrow/stacked frame it stays hidden so the row never
    //  overflows. The primary «ثبت پذیرش و صدور قبض» stays flush-right.
    {
        int gap=S(8), small=S(112);
        int wideW=S(300);
        MoveWindow(t->bSubmit, m.cardR-wideW, Y(v.btnY), wideW, v.btnH, TRUE);
        MoveWindow(t->bReset,  m.billL,             Y(v.btnY), small, v.btnH, TRUE);
        MoveWindow(t->bClose,  m.billL+small+gap,   Y(v.btnY), small, v.btnH, TRUE);
        ShowWindow(t->bSubmit,SW_SHOW); ShowWindow(t->bReset,SW_SHOW);
        ShowWindow(t->bClose,SW_SHOW);
        int newLeft = m.billL + 2*(small+gap);
        if(newLeft + small + gap < m.cardR - wideW){
            MoveWindow(t->bNew, newLeft, Y(v.btnY), small, v.btnH, TRUE);
            ShowWindow(t->bNew,SW_SHOW);
        } else {
            ShowWindow(t->bNew,SW_HIDE);
        }
    }

    // billing panel print buttons (bottom of LEFT billing card). Positioned by
    // the shared recPrintGroupTop() so the painter's «چاپ» title never drifts.
    if(!m.stacked){
        int bx = m.billL + S(10);
        int byy = recPrintGroupTop() + S(22) - sy;
        int bbw = (m.billR-m.billL) - S(20);
        MoveWindow(t->bPrtIns, bx, byy,        bbw, S(36), TRUE);
        MoveWindow(t->bPrtRx,  bx, byy+S(44),  bbw, S(36), TRUE);
        MoveWindow(t->bPrtLast,bx, byy+S(88),  bbw, S(36), TRUE);
        ShowWindow(t->bPrtIns,SW_SHOW); ShowWindow(t->bPrtRx,SW_SHOW);
        ShowWindow(t->bPrtLast,SW_SHOW);
        ShowWindow(t->bInquiry,SW_HIDE);
    } else {
        ShowWindow(t->bInquiry,SW_HIDE);
        ShowWindow(t->bPrtIns,SW_HIDE); ShowWindow(t->bPrtRx,SW_HIDE);
        ShowWindow(t->bPrtLast,SW_HIDE);
    }
    #undef Y

    // ====================== RIGHT INFO PANEL layout ======================
    HWND infoCtls[]={t->eArchive,t->bArchiveGo,t->eFile,t->bFileGo,t->eBookNo,
        t->chkBookNo,t->eValid,t->chkValidAuto,t->eRxDate,t->chkRxAuto,
        t->cSuppPanel,t->eSuppPct,t->bSuppGo,t->eDocCode,t->eDocName,t->bDocSearch};
    if(m.stacked || m.infoR<=m.infoL){
        for(HWND c: infoCtls) if(c) ShowWindow(c,SW_HIDE);
    } else {
        for(HWND c: infoCtls) if(c) ShowWindow(c,SW_SHOW);
        InfoLayout L; computeInfoLayout(m.infoL,m.infoR,H,L);
        const int iL=L.iL, iw=L.iw, rh2=L.rh2, gp=L.gp, btnW=L.btnW;
        // --- search keys group ---  (all info-panel controls also scroll)
        MoveWindow(t->bArchiveGo, iL, L.archiveY-sy, btnW, rh2, TRUE);
        MoveWindow(t->eArchive,   iL+btnW+gp, L.archiveY-sy, iw-btnW-gp, rh2, TRUE);
        MoveWindow(t->bFileGo,    iL, L.fileY-sy, btnW, rh2, TRUE);
        MoveWindow(t->eFile,      iL+btnW+gp, L.fileY-sy, iw-btnW-gp, rh2, TRUE);
        // --- insurance block group ---
        // ش دفترچه + فعال checkbox
        MoveWindow(t->chkBookNo, iL, L.bookY-sy, S(64), rh2, TRUE);
        MoveWindow(t->eBookNo,   iL+S(70), L.bookY-sy, iw-S(70), rh2, TRUE);
        // تاریخ اعتبار + اتوماتیک
        MoveWindow(t->chkValidAuto, iL, L.validY-sy, S(92), rh2, TRUE);
        MoveWindow(t->eValid,       iL+S(98), L.validY-sy, iw-S(98), rh2, TRUE);
        // تاریخ نسخه + اتوماتیک
        MoveWindow(t->chkRxAuto, iL, L.rxY-sy, S(92), rh2, TRUE);
        MoveWindow(t->eRxDate,   iL+S(98), L.rxY-sy, iw-S(98), rh2, TRUE);
        // بیمه مکمل + درصد + استعلام
        MoveWindow(t->bSuppGo,    iL, L.suppY-sy, btnW, rh2, TRUE);
        MoveWindow(t->eSuppPct,   iL+btnW+gp, L.suppY-sy, S(48), rh2, TRUE);
        MoveWindow(t->cSuppPanel, iL+btnW+gp+S(54), L.suppY-sy, iw-btnW-gp-S(54), S(200), TRUE);
        //  v1.32.0: the right-panel «جستجوی پزشک معالج» group is a DUPLICATE of the
        //  center «پزشک معالج» card — its controls are hidden (kept alive so any
        //  legacy read still works, but no longer shown or positioned on-screen).
        MoveWindow(t->eDocCode,  0,0,0,0, FALSE); ShowWindow(t->eDocCode,  SW_HIDE);
        MoveWindow(t->bDocSearch,0,0,0,0, FALSE); ShowWindow(t->bDocSearch,SW_HIDE);
        MoveWindow(t->eDocName,  0,0,0,0, FALSE); ShowWindow(t->eDocName,  SW_HIDE);
    }
}

// ----- national-id insurance inquiry ---------------------------------------
//  Validate an Iranian 10-digit national code (checksum) and look the patient
//  up against a TRUSTED source only. validNationalId() / lookupCitizen() live in
//  data_ext.cpp (shared with the appointment module). doInquiry() uses
//  lookupCitizen() to auto-fill the patient fields ONLY from a verified record
//  (online registry web-service when configured, or the local store of
//  previously-verified patients) and to detect when a patient carries 2 or 3
//  insurances — in which case it announces it and restricts the insurance combo
//  to ONLY those organisations, highlighted in a distinct colour.
//  v1.7.0: no fabrication. We only fill fields when a TRUSTED source (online
//  registry web-service or the local store of previously-verified patients)
//  returns a real record. When nothing verifies the code we DO NOT guess — the
//  name + surname wells turn into a danger/validation state and the operator
//  enters the identity by hand (which is then remembered on save).
static void rebuildInsuranceCombo(TabPage* t, const std::vector<int>& verified){
    t->insAllowed = verified;
    SendMessageW(t->cIns,CB_RESETCONTENT,0,0);
    if(verified.size()>=2){
        for(int ix : verified)
            if(ix>=0 && ix<N_INSURANCES)
                SendMessageW(t->cIns,CB_ADDSTRING,0,(LPARAM)INSURANCES[ix].name);
        SendMessageW(t->cIns,CB_SETCURSEL,0,0);
    } else {
        for(int i=0;i<N_INSURANCES;i++)
            SendMessageW(t->cIns,CB_ADDSTRING,0,(LPARAM)INSURANCES[i].name);
        int idx = verified.empty()?0:verified[0];
        if(idx<0||idx>=N_INSURANCES) idx=0;
        SendMessageW(t->cIns,CB_SETCURSEL,idx,0);
    }
}
static void doInquiry(TabPage* t, HWND h, bool quiet){
    wchar_t b[32]={0}; GetWindowTextW(t->eNid,b,32);
    std::wstring nid=trim(b);
    t->idChecked=true; t->idVerified=false;
    if(!validNationalId(nid)){
        t->insAllowed.clear();
        if(!quiet){
            t->lastMsg=L"کد ملی نامعتبر است (۱۰ رقم و رقم کنترلی صحیح). مشخصات را دستی وارد کنید.";
            t->msgCol=g_theme.danger;
        }
        InvalidateRect(h,NULL,FALSE);
        return;
    }
    CitizenInfo c = lookupCitizen(nid);
    if(!c.found){
        // No trusted source verified it → manual entry, no guessing.
        t->insAllowed.clear();
        // keep the full insurance list editable for manual selection
        rebuildInsuranceCombo(t, std::vector<int>());
        if(!quiet){
            if(c.lookupFailed)
                t->lastMsg=L"استعلام برخط ناموفق بود (سرور در دسترس نیست). مشخصات را دستی وارد و بررسی کنید.";
            else
                t->lastMsg=L"هویت تأیید نشد؛ نام و نام خانوادگی را دستی وارد کنید (قاب قرمز).";
            t->msgCol=g_theme.danger;
        }
        recalc(t);
        InvalidateRect(h,NULL,FALSE);
        SetFocus(t->eFirst);
        return;
    }
    // verified — fill only the fields the trusted source actually returned
    t->idVerified=true;
    auto setIfPresent=[&](HWND e, const std::wstring& v){
        if(!v.empty()) SetWindowTextW(e,v.c_str());
    };
    setIfPresent(t->eFirst, c.firstName);
    setIfPresent(t->eLast,  c.lastName);
    setIfPresent(t->eFather,c.fatherName);
    setIfPresent(t->eMobile,c.mobile);
    setIfPresent(t->eBirth, c.birthDate);
    if(c.gender==L"زن") SendMessageW(t->cGender,CB_SETCURSEL,1,0);
    else if(c.gender==L"مرد") SendMessageW(t->cGender,CB_SETCURSEL,0,0);

    // ---- insurance: ONLY what the trusted source verified ----
    rebuildInsuranceCombo(t, c.insurances);
    std::wstring src = c.source==CS_REGISTRY ? L"ثبت احوال/سامانه بیمه"
                     : c.source==CS_LOCAL    ? L"سوابق همین درمانگاه"
                     :                          L"منبع معتبر";
    if(c.insurances.size()>=2){
        wchar_t mb[220];
        swprintf(mb,220,L"هویت تأیید شد (%s) — این بیمار %d بیمهٔ معتبر دارد.",
            src.c_str(),(int)c.insurances.size());
        t->lastMsg=toFaDigits(mb); t->msgCol=g_theme.warn;
    } else if(c.insurances.size()==1){
        t->lastMsg=L"هویت تأیید شد ("+src+L") — بیمه: "+INSURANCES[c.insurances[0]].name;
        t->msgCol=g_theme.success;
    } else {
        t->lastMsg=L"هویت تأیید شد ("+src+L") — بیمه‌ای ثبت نشده؛ در صورت نیاز دستی انتخاب کنید.";
        t->msgCol=g_theme.success;
    }
    recalc(t);
    InvalidateRect(h,NULL,FALSE);
}

// ----- national-id field: Enter triggers a network-wide patient lookup -------
//  The operator types the 10-digit national code in t->eNid and presses Enter.
//  Regardless of the «دارای بیمه» state we run the validated, no-fabrication
//  lookupCitizen() (online registry → local verified store) and auto-fill the
//  patient identity (name / surname / father / birth date / gender / contact /
//  insurance + supplementary). Missing/partial records fill only what is known;
//  invalid/duplicate codes are handled gracefully inside doInquiry(). After the
//  lookup we hop to the first empty field so data entry stays fast.
static WNDPROC s_oldNid = NULL;
static LRESULT CALLBACK nidEditProc(HWND e, UINT m, WPARAM w, LPARAM l){
    if(m==WM_KEYDOWN && w==VK_RETURN){
        HWND page = GetParent(e);
        TabPage* t = page ? (TabPage*)GetWindowLongPtrW(page,GWLP_USERDATA) : NULL;
        if(t){
            doInquiry(t, page, false);          // always look up + auto-fill
            //  v1.25.0 admission Enter rule: when the record was auto-filled,
            //  jump to the FIRST STILL-EMPTY field in logical entry order so the
            //  operator only types what is missing (e.g. father name, birth date,
            //  gender, mobile, phone). When nothing verified, start at «نام».
            HWND nxt = NULL;
            auto isEmpty=[](HWND c){ wchar_t b[8]={0}; GetWindowTextW(c,b,2); return b[0]==0; };
            if(t->idVerified){
                // ordered scan of the identity fields (name→last→father→birth→
                // mobile→phone→address). The first empty one wins.
                HWND order[7]={t->eFirst,t->eLast,t->eFather,t->eBirth,
                               t->eMobile,t->ePhone,t->eAddr};
                for(int i=0;i<7;i++){ if(isEmpty(order[i])){ nxt=order[i]; break; } }
                if(!nxt) nxt=t->eDoc2Code;      // all filled → move to doctor code
            } else {
                nxt = t->eFirst;                // manual entry starts at name
            }
            if(nxt){ SetFocus(nxt); SendMessageW(nxt,EM_SETSEL,0,-1); }
        }
        return 0;
    }
    if(m==WM_CHAR && (w==VK_RETURN||w==VK_TAB)) return 0;   // suppress beep
    return CallWindowProcW(s_oldNid?s_oldNid:DefWindowProcW, e, m, w, l);
}
static void enableNidLookup(HWND e){
    WNDPROC old=(WNDPROC)SetWindowLongPtrW(e,GWLP_WNDPROC,(LONG_PTR)nidEditProc);
    if(!s_oldNid) s_oldNid=old;
}

// forward decls used by the service/doctor subclass procs
static bool applyDocByCode(HWND eCode, HWND cList);
// v1.78.0: performer-combo variants (filtered list, item-data backed)
static bool applyPerfByCode(HWND eCode, HWND cList);
static void mirrorPerfCodeFromList(HWND eCode, HWND cList);
static void tabPageLayout(HWND h, TabPage* t);

// ------------------------------------------------------------------
//  Built-in service catalogue (code → name/price). Defined here so the
//  service-code Enter handler can resolve «۱۱۱ → تزریقات» immediately.
// ------------------------------------------------------------------
struct SvcCat { const wchar_t* code; const wchar_t* name; long long price; };
static const SvcCat SVC_CATALOGUE[] = {
    { L"111", L"تزریقات",              120000  },
    { L"112", L"سرم‌تراپی",            250000  },
    { L"113", L"پانسمان",              180000  },
    { L"114", L"بخیه",                 350000  },
    { L"115", L"نوار قلب (ECG)",       200000  },
    { L"116", L"تست قند خون",           90000  },
    { L"117", L"فشار خون",              50000  },
    { L"121", L"ویزیت عمومی",          300000  },
    { L"122", L"ویزیت تخصصی",          450000  },
    { L"131", L"سونوگرافی",            600000  },
    { L"132", L"رادیولوژی",            400000  },
    { L"141", L"آزمایش خون (CBC)",     280000  },
    { L"151", L"فیزیوتراپی",           320000  },
};
static const int N_SVC_CAT = (int)(sizeof(SVC_CATALOGUE)/sizeof(SVC_CATALOGUE[0]));

// v1.28.0: normalise a code to Latin ASCII digits (strips Persian digits/spaces).
static std::wstring svcNormCode(const std::wstring& codeIn){
    std::wstring c; for(wchar_t ch:codeIn){
        if(ch>=L'۰'&&ch<=L'۹') c+=(wchar_t)(L'0'+(ch-L'۰'));
        else if(ch>=L'0'&&ch<=L'9') c+=ch;
    }
    return c;
}
// look up a catalogue entry by code (Persian or Latin digits). returns index or -1
static int svcCatFind(const std::wstring& codeIn){
    std::wstring c=svcNormCode(codeIn);
    if(c.empty()) return -1;
    for(int i=0;i<N_SVC_CAT;i++) if(c==SVC_CATALOGUE[i].code) return i;
    return -1;
}

// v1.28.0: resolve a service code → (name, price) preferring the Service
// Management database (data/services.dat). Falls back to the built-in
// catalogue when the DB has no matching *active* service. This makes any
// service created in the «مدیریت خدمات» page automatically usable in
// «پذیرش بیمار» without the operator ever typing a price.
static bool svcResolve(const std::wstring& codeIn, std::wstring& outName, long long& outPrice){
    std::wstring c=svcNormCode(codeIn);
    if(c.empty()) return false;
    // 1) database lookup (raw code, then Latin-normalised code)
    const ServiceDef* sd = findService(codeIn);
    if(!sd) sd = findService(c);
    if(sd && sd->status!=0){
        outName  = sd->name;
        outPrice = sd->price;
        return true;
    }
    // 2) built-in catalogue fallback
    int ci=svcCatFind(codeIn);
    if(ci>=0){
        outName  = SVC_CATALOGUE[ci].name;
        outPrice = SVC_CATALOGUE[ci].price;
        return true;
    }
    return false;
}

// v1.55.0 — resolve شرح خدمت (desc) and نوع/گروه خدمت (category) for a service
// code straight from the Service Management catalogue. Returns false (leaving
// the outputs untouched) when the code is unknown, so the printed receipt shows
// "—" for those columns instead of an invented value.
static bool svcResolveMeta(const std::wstring& codeIn,
                           std::wstring& outDesc, std::wstring& outCat){
    std::wstring c=svcNormCode(codeIn);
    if(c.empty()) return false;
    const ServiceDef* sd = findService(codeIn);
    if(!sd) sd = findService(c);
    if(sd && sd->status!=0){
        outDesc = sd->desc;
        outCat  = sd->category.empty() ? sd->dept : sd->category;
        return true;
    }
    return false;
}

// ----- service-code box: Enter looks up the catalogue and hops to name -------
//  Type «۱۱۱» + Enter → the name box is populated with «تزریقات» and focus moves
//  there (empty codes are allowed for free-text services). This never opens a
//  new window; it drives the inline panel.
static WNDPROC s_oldSvcCode=NULL, s_oldSvcName=NULL, s_oldDocCode=NULL;
static LRESULT CALLBACK svcCodeProc(HWND e, UINT m, WPARAM w, LPARAM l){
    if(m==WM_KEYDOWN && w==VK_RETURN){
        HWND page=GetParent(e);
        TabPage* t=page?(TabPage*)GetWindowLongPtrW(page,GWLP_USERDATA):NULL;
        if(t){
            wchar_t cb[64]={0}; GetWindowTextW(e,cb,64);
            std::wstring rn; long long rp=0;
            if(svcResolve(cb,rn,rp)) SetWindowTextW(t->eSvcName, rn.c_str());
            SetFocus(t->eSvcName);
            SendMessageW(t->eSvcName,EM_SETSEL,0,-1);
        }
        return 0;
    }
    if(m==WM_CHAR && w==VK_RETURN) return 0;
    return CallWindowProcW(s_oldSvcCode?s_oldSvcCode:DefWindowProcW,e,m,w,l);
}
static LRESULT CALLBACK svcNameProc(HWND e, UINT m, WPARAM w, LPARAM l){
    if(m==WM_KEYDOWN && w==VK_RETURN){
        HWND page=GetParent(e);
        TabPage* t=page?(TabPage*)GetWindowLongPtrW(page,GWLP_USERDATA):NULL;
        if(t){ SetFocus(t->eSvcQty); SendMessageW(t->eSvcQty,EM_SETSEL,0,-1); }
        return 0;
    }
    if(m==WM_CHAR && w==VK_RETURN) return 0;
    return CallWindowProcW(s_oldSvcName?s_oldSvcName:DefWindowProcW,e,m,w,l);
}
// doctor / performer code box: Enter resolves the code → selects the list item
// and hops to the next logical field.
static LRESULT CALLBACK docCodeProc(HWND e, UINT m, WPARAM w, LPARAM l){
    if(m==WM_KEYDOWN && w==VK_RETURN){
        HWND page=GetParent(e);
        TabPage* t=page?(TabPage*)GetWindowLongPtrW(page,GWLP_USERDATA):NULL;
        if(t){
            HWND list=NULL, nxt=NULL;
            if(e==t->eDoc2Code){ list=t->cDoc2List; nxt=t->ePerfCode; }
            else if(e==t->ePerfCode){
                // v1.78.0: filtered performer combo → resolve via item-data.
                if(applyPerfByCode(e,t->cPerfList)) InvalidateRect(page,NULL,FALSE);
                SetFocus(t->cIns);
                return 0;
            }
            if(list){ applyDocByCode(e,list); InvalidateRect(page,NULL,FALSE); }
            if(nxt) SetFocus(nxt);
        }
        return 0;
    }
    if(m==WM_CHAR && w==VK_RETURN) return 0;
    return CallWindowProcW(s_oldDocCode?s_oldDocCode:DefWindowProcW,e,m,w,l);
}
static void enableSvcCodeEnter(HWND e){ WNDPROC o=(WNDPROC)SetWindowLongPtrW(e,GWLP_WNDPROC,(LONG_PTR)svcCodeProc); if(!s_oldSvcCode)s_oldSvcCode=o; }
static void enableSvcNameEnter(HWND e){ WNDPROC o=(WNDPROC)SetWindowLongPtrW(e,GWLP_WNDPROC,(LONG_PTR)svcNameProc); if(!s_oldSvcName)s_oldSvcName=o; }
static void enableDocCodeEnter(HWND e){ WNDPROC o=(WNDPROC)SetWindowLongPtrW(e,GWLP_WNDPROC,(LONG_PTR)docCodeProc); if(!s_oldDocCode)s_oldDocCode=o; }

// gather form into record
static void collect(TabPage* t, ReceptionRecord& r){
    wchar_t b[512];
    GetWindowTextW(t->eFirst,b,512);  r.firstName=trim(b);
    GetWindowTextW(t->eLast,b,512);   r.lastName=trim(b);
    GetWindowTextW(t->eNid,b,512);    r.nationalId=trim(b);
    GetWindowTextW(t->eFather,b,512); r.fatherName=trim(b);
    GetWindowTextW(t->eBirth,b,512);  r.birthDate=trim(b);
    GetWindowTextW(t->eMobile,b,512); r.mobile=trim(b);
    GetWindowTextW(t->ePhone,b,512);  r.landline=trim(b);
    GetWindowTextW(t->eAddr,b,512);   r.address=trim(b);
    int gi=(int)SendMessageW(t->cGender,CB_GETCURSEL,0,0);
    r.gender = gi==1?L"زن":L"مرد";
    int pi=(int)SendMessageW(t->cPType,CB_GETCURSEL,0,0);
    r.patientType = pi==1?L"سرپایی":pi==2?L"بستری":L"عادی";
    int ni=(int)SendMessageW(t->cNType,CB_GETCURSEL,0,0);
    if(ni==1) r.patientType += L" — اورژانس";
    else if(ni==2) r.patientType += L" — پرسنلی";
    int ii=(int)SendMessageW(t->cIns,CB_GETCURSEL,0,0);  if(ii<0||ii>=N_INSURANCES)ii=0;
    int si=(int)SendMessageW(t->cSupp,CB_GETCURSEL,0,0); if(si<0||si>=N_SUPP)si=0;
    r.insIdx=ii; r.suppIdx=si;
    r.insurance=INSURANCES[ii].name; r.suppInsurance=SUPP_INSURANCES[si].name;
    recalc(t);
    r.total=t->total; r.mainShare=t->mainShare; r.patientShare=t->patientShare;
    r.baseDiff=t->baseDiff; r.orgShare=t->orgShare;
    wchar_t db[64]; GetWindowTextW(t->eDiscount,db,64);
    r.discount=parseMoney(db);
    r.paid=t->paid; r.finalTotal=t->total;
    r.shift=shiftName(g_session.shift);
    r.dept=g_session.user.dept;
    r.userName=g_session.user.username;

    // §1.53.0 (Bug D): capture the treating-doctor NAME from the center card's
    // combo (falls back to empty → {doctor} then falls back to dept). The combo
    // text is the doctor's display name mirrored from its code.
    if(t->cDoc2List){
        int di=(int)SendMessageW(t->cDoc2List,CB_GETCURSEL,0,0);
        if(di>0){   // index 0 is the «انتخاب…» placeholder
            wchar_t db2[256]={0};
            SendMessageW(t->cDoc2List,CB_GETLBTEXT,di,(LPARAM)db2);
            r.treatingDoctor=trim(db2);
        }
    }

    // §1.53.0 (Bug F ROOT CAUSE): copy every entered service row into the
    // record's services vector BEFORE printing. Previously collect() filled the
    // identity/money fields but never populated r.services, so the dynamic
    // services table on every printed receipt was empty.
    r.services.clear();
    for(size_t i=0;i<t->services.size();++i){
        const SvcRow& sr=t->services[i];
        ServiceLine sl;
        sl.code     = sr.code;
        sl.name     = sr.name;
        sl.price    = sr.price;
        sl.qty      = sr.qty>0 ? sr.qty : 1;
        sl.discount = sr.discount;
        sl.insShare = sr.insShare;
        sl.patShare = sr.patShare;
        // v1.55.0: carry شرح خدمت / نوع خدمت through to the print engine. If the
        // row was captured before this build (or its code is unknown) we make
        // one last catalogue attempt — still 100% real data, never invented.
        sl.desc     = sr.desc;
        sl.category = sr.category;
        if(sl.desc.empty() && sl.category.empty())
            svcResolveMeta(sr.code, sl.desc, sl.category);
        // v1.61.0 — never let a picked service reach the printer as a blank
        // row. A name is synthesised from whatever identity the row carries.
        if(trim(sl.name).empty()){
            const ServiceDef* sd = findService(sr.code);
            if(sd && sd->status!=0 && !sd->name.empty()) sl.name = sd->name;
            else if(!trim(sl.category).empty())          sl.name = sl.category;
            else if(!trim(sl.code).empty())              sl.name = L"خدمت "+sl.code;
            else if(!trim(sl.desc).empty())              sl.name = sl.desc;
            else                                         sl.name = L"خدمت";
        }
        if(trim(sl.desc).empty())
            sl.desc = trim(sl.category).empty()? sl.name : sl.category;
        r.services.push_back(sl);
    }

    // =====================================================================
    //  v1.55.0 — REAL-RECEIPT FIELDS (درمانگاه ثامن‌الائمه redesign)
    //  Every assignment below reads a control the operator actually filled in,
    //  the live session, or the reference tables. NOTHING is randomised: a
    //  value the operator did not enter stays EMPTY and the receipt prints a
    //  blank (or the design hides that row when visibility == 1).
    // =====================================================================

    // ---- کد پزشک معالج / کد و نام انجام‌دهنده ----------------------------
    if(t->eDoc2Code){ wchar_t cb2[64]={0}; GetWindowTextW(t->eDoc2Code,cb2,64); r.doctorCode=trim(cb2); }
    if(t->ePerfCode){ wchar_t cb2[64]={0}; GetWindowTextW(t->ePerfCode,cb2,64); r.performerCode=trim(cb2); }
    if(t->cPerfList){
        int pidx=(int)SendMessageW(t->cPerfList,CB_GETCURSEL,0,0);
        if(pidx>0){ wchar_t pb[256]={0};
            SendMessageW(t->cPerfList,CB_GETLBTEXT,pidx,(LPARAM)pb);
            r.performer=trim(pb); }
    }
    // اگر «انجام دهنده» ثبت نشده باشد، پزشک معالج انجام‌دهنده است.
    if(r.performer.empty())     r.performer     = r.treatingDoctor;
    if(r.performerCode.empty()) r.performerCode = r.doctorCode;

    // ---- شرح تخصص + کد تخصص (از مخزن پزشکان) ---------------------------
    r.specialty.clear(); r.specialtyCode.clear();
    if(!r.treatingDoctor.empty()){
        std::vector<DoctorDef> docs = loadDoctors();
        for(size_t i=0;i<docs.size();++i){
            if(docs[i].name==r.treatingDoctor){
                r.specialty = docs[i].specialty;
                // کد تخصص = همان کد ردیف پزشک (۱-مبنا و بازگشت‌پذیر)
                if(!r.doctorCode.empty()) r.specialtyCode = r.doctorCode;
                break;
            }
        }
    }

    // ---- درصد بیمهٔ پایه و مکمل ----------------------------------------
    // درصد پایه از جدول مرجع بیمه‌ها؛ درصد مکمل از جعبهٔ ورودی کاربر و اگر
    // خالی بود از جدول مرجع مکمل. مقدار نامعتبر → -1 (توکن خالی چاپ می‌شود).
    r.insPercent  = Ins_Percent(ii);
    r.suppPercent = -1;
    {
        HWND hp = t->eSuppPctIns ? t->eSuppPctIns : t->eSuppPct2;
        if(hp){
            wchar_t pb[32]={0}; GetWindowTextW(hp,pb,32);
            std::wstring pv;
            for(wchar_t ch : std::wstring(pb)){
                if(ch>=L'۰'&&ch<=L'۹')      pv+=(wchar_t)(L'0'+(ch-L'۰'));
                else if(ch>=L'0'&&ch<=L'9') pv+=ch;
            }
            if(!pv.empty()){ int pv2=_wtoi(pv.c_str()); if(pv2>0&&pv2<=100) r.suppPercent=pv2; }
        }
        if(r.suppPercent<0) r.suppPercent = Supp_Percent(si);
    }

    // ---- نقدی / کارتخوان (POS) -----------------------------------------
    // P = پرداختی (کارتخوان) و S = صندوق (نقدی) روی کارت میانی.
    r.pos = 0; r.cash = 0;
    if(t->eApptP){ wchar_t mb[64]={0}; GetWindowTextW(t->eApptP,mb,64); r.pos = parseMoney(mb); }
    if(t->eApptS){ wchar_t mb[64]={0}; GetWindowTextW(t->eApptS,mb,64); r.cash = parseMoney(mb); }
    // اگر هیچ‌کدام تکمیل نشده باشد، کل مبلغ پرداختی نقدی در نظر گرفته می‌شود.
    if(r.pos==0 && r.cash==0) r.cash = r.paid;

    // ---- تاریخ/ساعت نوبت و مهر زمانی ثبت -------------------------------
    if(t->eApptDate){
        wchar_t ab[64]={0}; GetWindowTextW(t->eApptDate,ab,64);
        std::wstring ad=trim(ab); if(!ad.empty()) r.apptDate=ad;
    }
    {
        SYSTEMTIME st; GetLocalTime(&st);
        wchar_t tb[32];
        swprintf(tb,32,L"%02d:%02d:%02d",st.wHour,st.wMinute,st.wSecond);
        r.apptSec = tb;
        if(r.apptTime.empty()){
            wchar_t t2[16]; swprintf(t2,16,L"%02d:%02d",st.wHour,st.wMinute);
            r.apptTime = t2;
        }
        if(r.apptDate.empty()) r.apptDate = jalaliDateShort(st);
        swprintf(tb,32,L"%02d:%02d",st.wHour,st.wMinute);
        r.regStamp = r.apptDate + L"  " + tb;
    }

    // ---- پذیرش‌گر / صندوق‌دار / شمارهٔ صندوق -----------------------------
    r.receptionist = g_session.user.fullname.empty()
                       ? g_session.user.username : g_session.user.fullname;
    r.cashierName  = r.receptionist;
    { wchar_t sb[16]; swprintf(sb,16,L"%d",g_session.shift+1); r.scNum=sb; }

    // ---- شمارهٔ قبض (مبنای بارکد/کد کوتاه — کاملاً قطعی، بدون تصادف) ----
    // شمارهٔ قبض همان شمارهٔ نوبت امروز است؛ موتور چاپ بارکد و کد کوتاه را از
    // همین عدد می‌سازد، بنابراین چاپ مجدد همان پذیرش همیشه یکسان است.
    if(r.receiptNo<=0 && r.queueNo>0) r.receiptNo=(long long)r.queueNo;
}

static void closeTab(TabPage* t);   // fwd

// reset all patient fields of a tab for the NEXT reception (no new tab needed)
static void resetForm(TabPage* t){
    if(!t) return;
    HWND clr[8]={t->eFirst,t->eLast,t->eNid,t->eFather,
                 t->eBirth,t->eMobile,t->ePhone,t->eAddr};
    for(int i=0;i<8;i++) SetWindowTextW(clr[i],L"");
    SendMessageW(t->cGender,CB_SETCURSEL,0,0);
    SendMessageW(t->cPType, CB_SETCURSEL,1,0);   // سرپایی default
    SendMessageW(t->cNType, CB_SETCURSEL,0,0);   // عادی
    // next patient defaults to «دارای بیمه» again
    SendMessageW(t->chkIns, BM_SETCHECK, BST_CHECKED, 0);
    EnableWindow(t->bInquiry, TRUE);
    SetWindowTextW(t->eDiscount,L"");
    SetWindowTextW(t->ePrice,L"");               // recalc auto-fills tariff
    t->idChecked=false; t->idVerified=false; t->insAllowed.clear();
    // v1.25.0: clear the center doctor/performer/appointment cards + services
    if(t->eDoc2Code) SetWindowTextW(t->eDoc2Code,L"");
    if(t->ePerfCode) SetWindowTextW(t->ePerfCode,L"");
    if(t->eSuppPct2) SetWindowTextW(t->eSuppPct2,L"");
    if(t->eSuppPctIns) SetWindowTextW(t->eSuppPctIns,L"");
    if(t->eApptP)    SetWindowTextW(t->eApptP,L"0");
    if(t->eApptS)    SetWindowTextW(t->eApptS,L"0");
    if(t->eApptDate) SetWindowTextW(t->eApptDate,FormatJalaliPersian(0).c_str());
    if(t->eSvcSearch)SetWindowTextW(t->eSvcSearch,L"");
    t->services.clear();
    t->svcPanelOpen=false;
    if(t->chkNoPay)  SendMessageW(t->chkNoPay,BM_SETCHECK,BST_UNCHECKED,0);
    t->invalidMask=0;
    recalc(t);
    t->lastMsg.clear();
    if(t->page){ tabPageLayout(t->page,t); InvalidateRect(t->page,NULL,FALSE); }
    SetFocus(t->eFirst);
}

// ==================================================================
//  v1.25.0 helpers — service catalogue commit + doctor-code lookup
//  (SvcCat / SVC_CATALOGUE / svcCatFind are defined earlier so the
//   subclass procs can use them.)
// ==================================================================

// commit whatever is in the inline add-service panel as a new bottom row.
// returns true if a row was added (i.e. there was enough to add).
static bool svcCommitPanel(TabPage* t){
    if(!t) return false;
    wchar_t cb[64]={0}, nb[256]={0}, qb[32]={0}, fb[64]={0};
    GetWindowTextW(t->eSvcCode,cb,64);
    GetWindowTextW(t->eSvcName,nb,256);
    GetWindowTextW(t->eSvcQty,qb,32);
    GetWindowTextW(t->eSvcFreeAmt,fb,64);
    std::wstring code=trim(cb), name=trim(nb);
    if(code.empty() && name.empty()) return false;   // nothing to add
    SvcRow s;
    s.code=code.empty()?L"—":toFaDigits(code);
    // v1.28.0: resolve name + price from the Service Management DB (falls back
    // to the built-in catalogue). The operator never types a service price.
    std::wstring resName; long long resPrice=0;
    bool resolved = svcResolve(code,resName,resPrice);
    if(name.empty() && resolved) name=resName;
    if(name.empty()) name=L"خدمت";
    s.name=name;
    // v1.55.0: pull شرح خدمت / نوع خدمت from the catalogue so the printed
    // services table can fill its «شرح خدمت» column with REAL data.
    svcResolveMeta(code, s.desc, s.category);
    int qv=_wtoi(qb); if(qv<=0) qv=1; s.qty=qv;
    bool freeRate = SendMessageW(t->chkSvcFree,BM_GETCHECK,0,0)==BST_CHECKED;
    if(freeRate){
        long long amt=parseMoney(fb);
        s.price = amt>0 ? amt : (resolved?resPrice:0);
    } else {
        s.price = resolved ? resPrice : 0;
    }
    t->services.push_back(s);
    // clear the panel for the next entry
    SetWindowTextW(t->eSvcCode,L"");
    SetWindowTextW(t->eSvcName,L"");
    SetWindowTextW(t->eSvcQty,L"1");
    SetWindowTextW(t->eSvcFreeAmt,L"");
    SendMessageW(t->chkSvcFree,BM_SETCHECK,BST_UNCHECKED,0);
    recalc(t);
    return true;
}

// the digits of a numeric code box, tolerating Persian digits (۰-۹ → 0-9,
// anything else dropped).
static std::wstring codeBoxDigits(HWND eCode){
    wchar_t cb[32]={0}; GetWindowTextW(eCode,cb,32);
    std::wstring c; for(wchar_t ch:std::wstring(cb)){
        if(ch>=L'۰'&&ch<=L'۹') c+=(wchar_t)(L'0'+(ch-L'۰'));
        else if(ch>=L'0'&&ch<=L'9') c+=ch;
    }
    return c;
}
// echo the 1-based code for a doctors-store index back into the box (Persian).
static void echoDocCode(HWND eCode, int srcIdx){
    wchar_t code[16]; swprintf(code,16,L"%d",srcIdx+1);
    SetWindowTextW(eCode,toFaDigits(code).c_str());
}
// doctor code → name. Codes are 1-based positions in the doctors store so the
// mapping is stable and reversible (typing «۱» selects the first doctor). This
// fills both the code textbox (canonical) and the list combo selection.
static bool applyDocByCode(HWND eCode, HWND cList){
    if(!eCode || !cList) return false;
    std::wstring c=codeBoxDigits(eCode);
    if(c.empty()) return false;
    /* v2.06: the combo now starts with a «— انتخاب —» placeholder row, so the
       doctor list index is (row - 1). No silent auto-select of the first hit
       either — the code resolves only when it names a real doctor. */
    int idx=_wtoi(c.c_str())-1;
    int n=(int)SendMessageW(cList,CB_GETCOUNT,0,0);
    if(idx<0 || idx+1>n-1) return false;
    SendMessageW(cList,CB_SETCURSEL,idx+1,0);
    echoDocCode(eCode,idx);
    return true;
}
// when the operator picks from the combo, mirror the 1-based code back.
// v2.06: row 0 is the «— انتخاب —» placeholder — selecting it clears the code.
static void mirrorDocCodeFromList(HWND eCode, HWND cList){
    if(!eCode || !cList) return;
    int sel=(int)SendMessageW(cList,CB_GETCURSEL,0,0);
    if(sel<0) return;
    if(sel==0){ SetWindowTextW(eCode,L""); return; }
    echoDocCode(eCode,sel-1);
}
// v1.78.0: the performer combo lists ONLY doctors flagged isPerformer, so the
// combo row no longer equals the doctors-list index. Each row carries the
// source index as item-data; these variants resolve the numeric «کد انجام
// دهنده» through it (the code keeps its 1-based meaning over the FULL list —
// the same convention the embedded web admission uses for its code box).
static bool applyPerfByCode(HWND eCode, HWND cList){
    if(!eCode || !cList) return false;
    std::wstring c=codeBoxDigits(eCode);
    if(c.empty()) return false;
    int want=_wtoi(c.c_str())-1;
    int n=(int)SendMessageW(cList,CB_GETCOUNT,0,0);
    for(int i=0;i<n;i++){
        if((int)SendMessageW(cList,CB_GETITEMDATA,i,0)==want){
            SendMessageW(cList,CB_SETCURSEL,i,0);
            echoDocCode(eCode,want);
            return true;
        }
    }
    // no such PERFORMER (the code is either unknown or belongs to a doctor the
    // manager unticked): clear the stale code so the saved record never pairs
    // a mistyped/invalid code with the fallback performer name.
    SetWindowTextW(eCode,L"");
    SendMessageW(cList,CB_SETCURSEL,(WPARAM)-1,0);
    return false;
}
static void mirrorPerfCodeFromList(HWND eCode, HWND cList){
    if(!eCode || !cList) return;
    int sel=(int)SendMessageW(cList,CB_GETCURSEL,0,0);
    if(sel<0) return;
    /* v2.06: row 0 is the «— انتخاب —» placeholder — selecting it clears the code. */
    if(sel==0){ SetWindowTextW(eCode,L""); return; }
    echoDocCode(eCode,(int)SendMessageW(cList,CB_GETITEMDATA,sel,0));
}

// ----- painted page for portal-message / empty tabs (no controls) ----------
//  A clean centred "glass" hero card with a vector icon, a title and a body
//  line. The portal page is the placeholder for future messages pushed by the
//  clinic management panel; the empty page invites the user to start a task.
// ----- the cartable (کارتابل): a real inbox of management messages ----------
//  v2: messages are drawn as fixed-size RECTANGLE TILES laid side-by-side and
//  wrapping into rows (not full-width lines). Pinned messages float to the
//  front; the rest are sorted by send date (newest first). Clicking a tile
//  opens a context menu (پین کردن / دیدن / پاک کردن). The background is solid
//  dark with NO gradient bleed so colours stay crisp on the dark theme.
struct CartTile { RECT r; int disp; };       // disp = index into s_cartMsgs
static std::vector<CartTile> s_cartTiles;    // hit-test map, rebuilt each paint
static std::vector<KMsg>     s_cartMsgs;      // the list shown (sorted)
static std::vector<int>      s_cartNF;        // display-pos → plain newest-first idx
// v1.7.0 detail-view buttons (rebuilt each paint of the details screen)
//  v1.77.0: پاسخ (reply) · پیوست/ارسال فایل (upload) · چاپ (print) added so the
//  cartable is a full message workspace, not just a read-only viewer.
enum { CART_BTN_NONE=0, CART_BTN_MARK=1, CART_BTN_TOGGLEREAD=2,
       CART_BTN_DELETE=3, CART_BTN_BACK=4, CART_BTN_SAVE=5,
       CART_BTN_REPLY=6, CART_BTN_UPLOAD=7, CART_BTN_PRINT=8 };
struct CartBtn { RECT r; int id; };
static std::vector<CartBtn> s_cartBtns;      // detail-view button hit map
// v1.8.0: the archive toggle hotspot in the cartable header (top-LEFT corner).
// Empty when «پیام‌های ذخیره‌شده» is disabled (it is then not drawn / not hit).
static RECT s_cartArchiveRect = {0,0,0,0};

// load + sort: pinned first, then by send time (newest first). loadMessages
// already returns oldest-first, so we reverse for newest-first then stable-
// partition the pinned ones to the front. s_cartNF remembers each tile's
// PLAIN newest-first index (what the data layer's pin/seen/delete expects).
static void cartReload(){
    auto raw=loadMessages(g_session.user.username);
    std::reverse(raw.begin(),raw.end());     // newest first (by storage order)
    // pair each message with its plain newest-first index, then stable-sort
    std::vector<std::pair<KMsg,int>> v;
    for(int i=0;i<(int)raw.size();i++) v.push_back({raw[i],i});
    std::stable_sort(v.begin(),v.end(),
        [](const std::pair<KMsg,int>&a,const std::pair<KMsg,int>&b){
            return a.first.pinned && !b.first.pinned; });
    s_cartMsgs.clear(); s_cartNF.clear();
    for(auto&p:v){ s_cartMsgs.push_back(p.first); s_cartNF.push_back(p.second); }
}
// severity colour + label helpers (priority/status of a message) -----------
static COLORREF sevColor(int type){
    return type==KMSG_CRITICAL ? g_theme.danger
         : type==KMSG_URGENT   ? g_theme.warn
         :                       g_theme.success;
}
static const wchar_t* sevLabel(int type){
    return type==KMSG_CRITICAL ? L"بحرانی"
         : type==KMSG_URGENT   ? L"فوری"
         :                       L"عادی";
}
// split a stored time string "1403/05/20 14:30" → date + time parts ----------
static void splitMsgTime(const std::wstring& s, std::wstring& date, std::wstring& time){
    size_t sp=s.find(L' ');
    if(sp==std::wstring::npos){ date=s; time=L""; }
    else { date=s.substr(0,sp); time=s.substr(sp+1); }
}
// v1.77.0: minimal HTML text escaper for the cartable print output — escapes the
// four characters that break out of text content; Persian/Arabic passes through.
static std::wstring cartHtmlEsc(const std::wstring& s){
    std::wstring o; o.reserve(s.size());
    for(size_t i=0;i<s.size();i++){
        wchar_t c=s[i];
        if(c==L'&') o+=L"&amp;";
        else if(c==L'<') o+=L"&lt;";
        else if(c==L'>') o+=L"&gt;";
        else if(c==L'"') o+=L"&quot;";
        else o+=c;
    }
    return o;
}
// v1.77.0: resolve the cartable detail selection → display index (used by the
// reply / upload / print actions). Returns -1 when the selection is stale.
static int cartSelIndex(TabPage* t){
    int sel=-1;
    for(int i=0;i<(int)s_cartNF.size();i++)
        if(s_cartNF[i]==t->cartSelNF){ sel=i; break; }
    if(sel<0 && t->cartSelDisp>=0 && t->cartSelDisp<(int)s_cartMsgs.size())
        sel=t->cartSelDisp;
    if(sel<0 || sel>=(int)s_cartMsgs.size()) return -1;
    return sel;
}

// ----- the FULL message DETAILS view (sender, priority, date, time, body) ---
static void drawCartDetail(HDC dc, const RECT& rc, TabPage* t){
    // v1.77.0: soft vertical page gradient (bg → bg2) instead of a flat fill so
    // the cartable reads as a layered, professional surface (was "too white").
    gpGradRoundRect(dc,(RECT&)rc,0,g_theme.bg,g_theme.bg2,CLR_INVALID);
    SetBkMode(dc,TRANSPARENT);
    s_cartBtns.clear();
    cartReload();
    // resolve the selected message by its STABLE newest-first index (cartSelNF)
    // so it stays correct even if the list re-sorts (e.g. after pin/seen).
    int sel=-1;
    for(int i=0;i<(int)s_cartNF.size();i++)
        if(s_cartNF[i]==t->cartSelNF){ sel=i; break; }
    if(sel<0 && t->cartSelDisp>=0 && t->cartSelDisp<(int)s_cartMsgs.size())
        sel=t->cartSelDisp;
    if(sel<0 || sel>=(int)s_cartMsgs.size()){
        t->cartDetail=false; return;     // selection no longer valid (deleted)
    }
    t->cartSelDisp=sel;
    KMsg& mm=s_cartMsgs[sel];
    COLORREF sev=sevColor(mm.type);

    int pad=S(20);
    RECT panel={rc.left+pad, rc.top+pad, rc.right-pad, rc.bottom-pad};
    // v1.8.0: draw the panel as a real rounded rect (corners patched with the
    // page background) instead of a square FillRect that left surface-coloured
    // square corners poking out beyond the rounded border.
    // v1.77.0: panel is a faint top-lit gradient (a whisper of the accent into
    // the card white) so it is no longer a stark flat-white slab.
    gpGradRoundRectBg(dc,panel,S(16),
        blendColor(g_theme.surface,g_theme.accent2,g_dark?5:9),
        g_theme.surface,g_theme.border,g_theme.bg);

    // header bar (priority-coloured so status reads at a glance)
    RECT hdr={panel.left,panel.top,panel.right,panel.top+S(52)};
    gpGradRoundRectBg(dc,hdr,S(16),sev,sev,CLR_INVALID,g_theme.surface);
    RECT hdrB={panel.left,panel.top+S(28),panel.right,panel.top+S(52)};
    gpGradRoundRect(dc,hdrB,0,sev,sev,CLR_INVALID);
    RECT bi={panel.right-S(44),panel.top+S(12),panel.right-S(18),panel.top+S(38)};
    drawIcon(dc,ICO_LETTER,bi,RGB(255,255,255),S(2));
    SelectObject(dc,g_fTitle); SetTextColor(dc,RGB(255,255,255));
    RECT tr={panel.left+S(20),panel.top+S(8),panel.right-S(52),panel.top+S(44)};
    DrawTextW(dc,L"جزئیات پیام مدیریت",-1,&tr,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    // pin icon top-right of the header when pinned
    if(mm.pinned){
        SelectObject(dc,g_fUIB); SetTextColor(dc,RGB(255,255,255));
        RECT pr={panel.left+S(16),panel.top+S(12),panel.left+S(120),panel.top+S(40)};
        DrawTextW(dc,L"\U0001F4CC سنجاق‌شده",-1,&pr,
            DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
    }

    std::wstring date,time; splitMsgTime(mm.time,date,time);
    int x0=panel.left+S(24), x1=panel.right-S(24);
    int y=panel.top+S(70);
    // metadata grid rows -----------------------------------------------------
    struct Meta{ const wchar_t* k; std::wstring v; COLORREF vc; };
    std::wstring sender=mm.from.empty()?std::wstring(L"مدیریت درمانگاه"):mm.from;
    std::wstring recip = (mm.to==L"*")?std::wstring(L"همهٔ کاربران"):mm.to;
    Meta rows[]={
        {L"فرستنده",     sender,                        g_theme.text},
        {L"گیرنده",      recip,                         g_theme.text},
        {L"اولویت / وضعیت", std::wstring(sevLabel(mm.type))+
            (mm.seen?L"  •  خوانده‌شده":L"  •  خوانده‌نشده"), sev},
        {L"تاریخ",        toFaDigits(date),             g_theme.text},
        {L"ساعت",         toFaDigits(time),             g_theme.text},
    };
    for(auto& r : rows){
        SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
        RECT kr={x1-S(150),y,x1,y+S(22)};
        DrawTextW(dc,r.k,-1,&kr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        SelectObject(dc,g_fUIB); SetTextColor(dc,r.vc);
        RECT vr={x0,y,x1-S(160),y+S(22)};
        DrawTextW(dc,r.v.c_str(),-1,&vr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        y+=S(28);
    }
    // separator
    { HPEN pn=CreatePen(PS_SOLID,1,g_theme.border); HGDIOBJ op=SelectObject(dc,pn);
      MoveToEx(dc,x0,y+S(4),0); LineTo(dc,x1,y+S(4)); SelectObject(dc,op); DeleteObject(pn); }
    y+=S(16);

    // v1.77.0: build the action-button set up front so we can measure whether it
    // fits ONE row (and reserve body height accordingly). New actions join the
    // existing بازگشت/حذف/ذخیره/خوانده:
    //   پاسخ (CART_BTN_REPLY)   — quick text reply to the sender (management)
    //   پیوست فایل (CART_BTN_UPLOAD) — pick a file and send it up to management
    //   چاپ پیام (CART_BTN_PRINT) — print / save this message to paper or file
    struct Btn{int id; const wchar_t* lbl; COLORREF fill; COLORREF txt;};
    std::vector<Btn> defs={
        {CART_BTN_BACK,       L"بازگشت",            g_theme.surface2, g_theme.text},
        {CART_BTN_REPLY,      L"پاسخ",              g_theme.accent2,  RGB(255,255,255)},
        {CART_BTN_UPLOAD,     L"پیوست فایل",        g_theme.accent,   g_theme.accentText},
        {CART_BTN_PRINT,      L"چاپ پیام",          g_theme.surface2, g_theme.text},
        {CART_BTN_TOGGLEREAD, mm.seen?L"خوانده":L"علامت خوانده‌شده",
                              g_theme.surface2, g_theme.text},
        {CART_BTN_MARK,       L"خواندن",            g_theme.surface2, g_theme.text},
        {CART_BTN_DELETE,     L"حذف",               g_theme.danger,   RGB(255,255,255)},
    };
    // v1.9.5: SAVE archives this message into the saved-messages store
    // (data\saved_msgs.dat) — only when «پیام‌های ذخیره‌شده» is enabled.
    if(savedMsgsEnabled() && !(t && t->cartShowArchive)){
        defs.push_back({CART_BTN_SAVE, L"ذخیره", g_theme.success, RGB(255,255,255)});
    }
    // measure → 1 row if it fits, else wrap to a 2nd row; reserve body height.
    int bh=S(36), padBtn=S(26), gapBtn=S(10);
    int availW=(panel.right-S(24))-(panel.left+S(24));
    int totalW=0; SelectObject(dc,g_fUIB);
    for(auto& d : defs){ SIZE sz; GetTextExtentPoint32W(dc,d.lbl,(int)wcslen(d.lbl),&sz); totalW += sz.cx+padBtn+gapBtn; }
    int btnRows = (defs.empty() || totalW-gapBtn <= availW) ? 1 : 2;
    int byTop = panel.bottom - S(54) - (btnRows==2 ? (bh+gapBtn) : 0);

    // description / body ------------------------------------------------------
    SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.accent);
    RECT dl={x0,y,x1,y+S(20)};
    DrawTextW(dc,L"متن پیام",-1,&dl,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    y+=S(24);
    SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.text);
    RECT body={x0,y,x1,byTop-S(14)};
    DrawTextW(dc,mm.text.c_str(),-1,&body,
        DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX|DT_EDITCONTROL);

    // action buttons (bottom strip) — laid out left→right, wrapping to a 2nd row
    // when the panel is narrow. v1.63.0: raised gradient pills; hover lifts them
    // with an accent glow instead of only swapping two colours.
    int bx=panel.left+S(24), by=byTop;
    for(auto& d : defs){
        SelectObject(dc,g_fUIB);
        SIZE sz; GetTextExtentPoint32W(dc,d.lbl,(int)wcslen(d.lbl),&sz);
        int bw=sz.cx+padBtn;
        if(bx+bw > panel.right-S(24)){        // wrap to next row
            bx=panel.left+S(24); by+=bh+gapBtn;
        }
        RECT r={bx,by,bx+bw,by+bh};
        bool hov=(t->cartHotBtn==d.id);
        if(hov){
            gpShadowColor(dc,r,S(10),S(6),90,g_theme.accent);
            gpGradRoundRect(dc,r,S(10),g_theme.accentHover,g_theme.accent,
                            CLR_INVALID);
        } else {
            gpShadow(dc,r,S(10),S(4),40);
            gpGradRoundRect(dc,r,S(10),
                blendColor(d.fill,RGB(255,255,255),g_dark?10:22),d.fill,
                blendColor(g_theme.border,g_theme.accent,18));
        }
        SetTextColor(dc, hov?g_theme.accentText:d.txt);
        DrawTextW(dc,d.lbl,-1,&r,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        s_cartBtns.push_back({r,d.id});
        bx+=bw+gapBtn;
    }
}

static void drawCartList(HDC dc, const RECT& rc, TabPage* t){
    // v1.77.0: soft vertical page gradient (bg → bg2) instead of a flat fill.
    gpGradRoundRect(dc,(RECT&)rc,0,g_theme.bg,g_theme.bg2,CLR_INVALID);
    SetBkMode(dc,TRANSPARENT);
    int pad=S(20);
    RECT panel={rc.left+pad, rc.top+pad, rc.right-pad, rc.bottom-pad};
    // v1.8.0: rounded panel with corners patched to the page background.
    // v1.77.0: faint top-lit gradient so the inbox panel is not stark white.
    gpGradRoundRectBg(dc,panel,S(16),
        blendColor(g_theme.surface,g_theme.accent2,g_dark?5:9),
        g_theme.surface,g_theme.border,g_theme.bg);

    bool savedOn = savedMsgsEnabled();
    bool archive = savedOn && t && t->cartShowArchive;

    // header bar
    RECT hdr={panel.left,panel.top,panel.right,panel.top+S(52)};
    gpGradRoundRectBg(dc,hdr,S(16),g_theme.accent2,g_theme.accent,CLR_INVALID,g_theme.surface);
    RECT hdrB={panel.left,panel.top+S(28),panel.right,panel.top+S(52)};
    gpGradRoundRect(dc,hdrB,0,g_theme.accent2,g_theme.accent,CLR_INVALID);
    RECT bi={panel.right-S(44),panel.top+S(12),panel.right-S(18),panel.top+S(38)};
    drawIcon(dc,archive?ICO_SAVE:ICO_LETTER,bi,RGB(255,255,255),S(2));
    SelectObject(dc,g_fTitle); SetTextColor(dc,RGB(255,255,255));
    RECT tr={panel.left+S(56),panel.top+S(8),panel.right-S(52),panel.top+S(44)};
    DrawTextW(dc, archive?L"پیام‌های ذخیره‌شده":L"کارتابل — پیام‌های مدیریت درمانگاه",-1,&tr,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

    // §D (v1.10.0): the saved-messages (archive) toggle is ALWAYS visible and
    // reachable in the TOP-LEFT corner of the cartable header — it is no longer
    // hidden behind the «پیام‌های ذخیره‌شده» setting. When the feature is off,
    // clicking it offers to enable it (handled in WM_LBUTTONUP), so the entry is
    // discoverable instead of dead.
    {
        RECT ab={panel.left+S(12),panel.top+S(12),panel.left+S(40),panel.top+S(40)};
        if(archive) gpRoundRect(dc,ab,S(8),RGB(255,255,255),CLR_INVALID,60);
        RECT ai={ab.left+S(4),ab.top+S(4),ab.right-S(4),ab.bottom-S(4)};
        // dim the icon a touch when the feature is disabled (still clickable).
        drawIcon(dc, archive?ICO_LETTER:ICO_SAVE, ai,
                 savedOn?RGB(255,255,255):RGB(210,224,242), S(2));
        s_cartArchiveRect = ab;
    }

    // v1.77.0: a crisp hairline under the header band separates it from the
    // message list (was only a colour edge) — a small professional touch.
    { HPEN pn=CreatePen(PS_SOLID,1,g_theme.border); HGDIOBJ op=SelectObject(dc,pn);
      int dy=panel.top+S(52);
      MoveToEx(dc,panel.left+S(16),dy,0); LineTo(dc,panel.right-S(16),dy);
      SelectObject(dc,op); DeleteObject(pn); }

    s_cartTiles.clear();
    int topY=panel.top+S(64);

    // ---- ARCHIVE (saved messages) branch -----------------------------------
    if(archive){
        auto saved=loadSavedMsgs();
        if(saved.empty()){
            SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
            RECT er={panel.left+S(24),topY,panel.right-S(24),topY+S(40)};
            DrawTextW(dc,L"پیام ذخیره‌شده‌ای وجود ندارد.",-1,&er,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            return;
        }
        int y=topY;
        for(int i=0;i<(int)saved.size() && y<panel.bottom-S(20); i++){
            RECT card={panel.left+S(16),y,panel.right-S(16),y+S(76)};
            gpRoundRectBg(dc,card,S(10),g_theme.surface2,g_theme.border,g_theme.surface);
            SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
            RECT hr={card.left+S(14),card.top+S(8),card.right-S(14),card.top+S(30)};
            std::wstring head=saved[i].from+L"  ←  "+saved[i].to+L"   ("+toFaDigits(saved[i].time)+L")";
            DrawTextW(dc,head.c_str(),-1,&hr,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
            RECT br={card.left+S(14),card.top+S(32),card.right-S(14),card.bottom-S(6)};
            std::wstring body=saved[i].text;
            if(!saved[i].attachPath.empty()) body=L"\U0001F4CE "+body;
            DrawTextW(dc,body.c_str(),-1,&br,
                DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS|DT_EDITCONTROL);
            y += S(76)+S(10);
        }
        return;
    }

    cartReload();
    if(s_cartMsgs.empty()){
        SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
        RECT er={panel.left+S(24),topY,panel.right-S(24),topY+S(40)};
        DrawTextW(dc,L"در حال حاضر پیامی از مدیریت دریافت نشده است.",-1,&er,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        return;
    }
    // a small caption inviting the user to click for details
    SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.accentText);
    RECT cap={panel.left+S(56),panel.top+S(34),panel.right-S(52),panel.top+S(50)};
    DrawTextW(dc,L"برای دیدن جزئیات روی پیام کلیک کنید — راست‌کلیک: سنجاق / ذخیره",-1,&cap,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

    // tile grid metrics — responsive column count
    int areaW = panel.right-panel.left-S(32);
    int tileMinW = S(300);
    int cols = areaW / (tileMinW+S(12)); if(cols<1) cols=1; if(cols>4) cols=4;
    int gap  = S(12);
    int tileW = (areaW - gap*(cols-1)) / cols;
    int tileH = S(96);
    int x0    = panel.right-S(16);            // RTL: lay out from the RIGHT edge
    int col=0, row=0;
    for(int i=0;i<(int)s_cartMsgs.size();i++){
        KMsg& mm=s_cartMsgs[i];
        int tx = x0 - (col+1)*tileW - col*gap;
        int ty = topY + row*(tileH+gap);
        if(ty>panel.bottom-tileH-S(8)) break;     // ran out of vertical room
        RECT card={tx,ty,tx+tileW,ty+tileH};
        s_cartTiles.push_back({card,(int)i});

        COLORREF sevCol = sevColor(mm.type);
        const wchar_t* sevLbl = sevLabel(mm.type);
        // v1.77.0: IMPORTANT messages (بحرانی/فوری) are distinguished from SIMPLE
        // (عادی) ones at a glance: they get a faint severity-tinted gradient card,
        // a coloured border and a WIDER gradient severity stripe. Simple messages
        // stay calm and neutral. The seen/unseen cue is preserved on both.
        bool important = (mm.type==KMSG_CRITICAL || mm.type==KMSG_URGENT);
        COLORREF cardTop = blendColor(g_theme.surface, sevCol, important?(g_dark?14:18):4);
        COLORREF cardBord = important ? (mm.seen?blendColor(sevCol,g_theme.border,58):sevCol)
                                      : (mm.seen?g_theme.border:blendColor(g_theme.border,g_theme.accent,22));
        gpGradRoundRectBg(dc,card,S(10),cardTop,g_theme.surface,cardBord,g_theme.surface);
        // severity stripe down the RIGHT (RTL leading) edge — wider + gradient for important
        int sw = important?S(7):S(4);
        RECT stripe={card.right-sw-S(2),card.top+S(4),card.right-S(2),card.bottom-S(4)};
        if(important)
            gpGradRoundRect(dc,stripe,S(2),sevCol,blendColor(sevCol,RGB(0,0,0),28),CLR_INVALID);
        else
            gpRoundRect(dc,stripe,S(2),sevCol,CLR_INVALID,255);
        // v1.7.0: PIN icon at the TOP-RIGHT corner (just left of the stripe)
        // when the message is pinned.
        if(mm.pinned){
            RECT pin={card.right-S(32),card.top+S(6),card.right-S(10),card.top+S(28)};
            gpRoundRect(dc,pin,S(9),g_theme.accent,CLR_INVALID,255);
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.accentText);
            DrawTextW(dc,L"\U0001F4CC",-1,&pin,
                DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
        }
        // unseen dot (top-LEFT)
        if(!mm.seen){
            RECT dot={card.left+S(10),card.top+S(10),card.left+S(22),card.top+S(22)};
            gpRoundRect(dc,dot,S(6),sevCol,CLR_INVALID,255);
        }
        // header line: [severity] from
        SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
        RECT fr={card.left+S(28),card.top+S(8),card.right-(mm.pinned?S(38):S(12)),card.top+S(30)};
        std::wstring head=L"["+std::wstring(sevLbl)+L"] "+
            (mm.from.empty()?std::wstring(L"مدیریت"):mm.from);
        DrawTextW(dc,head.c_str(),-1,&fr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
        // date line
        SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
        RECT dr={card.left+S(12),card.top+S(30),card.right-S(12),card.top+S(48)};
        DrawTextW(dc,(L"\U0001F4C5 "+toFaDigits(mm.time)).c_str(),-1,&dr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
        // body text (wrapped to 2 lines)
        SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.text);
        RECT br={card.left+S(12),card.top+S(50),card.right-S(12),card.bottom-S(8)};
        DrawTextW(dc,mm.text.c_str(),-1,&br,
            DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS|DT_EDITCONTROL);

        col++; if(col>=cols){ col=0; row++; }
    }
}
static void drawCartable(HDC dc, const RECT& rc, TabPage* t){
    if(t && t->cartDetail) drawCartDetail(dc,rc,t);
    else                   drawCartList(dc,rc,t);
}
// hit-test a click against the cartable tiles → display index into s_cartMsgs
static int cartHit(POINT pt){
    for(auto& t:s_cartTiles) if(PtInRect(&t.r,pt)) return t.disp;
    return -1;
}
// hit-test a click/hover against the detail-view buttons → CART_BTN_*
static int cartBtnHit(POINT pt){
    for(auto& b:s_cartBtns) if(PtInRect(&b.r,pt)) return b.id;
    return CART_BTN_NONE;
}
static void drawTabPlaceholder(HDC dc, const RECT& rc, int kind, TabPage* t){
    if(kind==TK_PORTAL){ drawCartable(dc,rc,t); return; }
    // soft page gradient
    gpGradRoundRect(dc,(RECT&)rc,0,g_theme.bg,g_theme.bg2,CLR_INVALID);

    int cw = S(560); if(cw > rc.right-S(80)) cw = rc.right-S(80);
    int chh= S(300); if(chh> rc.bottom-S(80)) chh= rc.bottom-S(80);
    int cx = rc.right/2, cy = rc.bottom/2;
    RECT card={cx-cw/2, cy-chh/2, cx+cw/2, cy+chh/2};

    gpShadow(dc,card,S(22),S(26),60);
    gpFillAlpha(dc,card,S(22),g_theme.surfaceTop,235);
    gpRoundRect(dc,card,S(22),CLR_INVALID,g_theme.border,255);

    int br=S(40), bx=cx, by=card.top+S(64);
    RECT badge={bx-br,by-br,bx+br,by+br};
    gpGradRoundRect(dc,badge,br,g_theme.accent2,g_theme.accent,CLR_INVALID);
    RECT bi={badge.left+S(18),badge.top+S(18),badge.right-S(18),badge.bottom-S(18)};
    drawIcon(dc, ICO_TAB, bi, RGB(255,255,255), S(3));

    SetBkMode(dc,TRANSPARENT);
    SelectObject(dc,g_fTitle);
    SetTextColor(dc,g_theme.text);
    RECT tr={card.left+S(20), by+br+S(14), card.right-S(20), by+br+S(14)+S(40)};
    DrawTextW(dc,L"تب جدید",-1,&tr,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

    SelectObject(dc,g_fUI);
    SetTextColor(dc,g_theme.textDim);
    RECT br1={card.left+S(28), tr.bottom+S(8), card.right-S(28), tr.bottom+S(8)+S(28)};
    DrawTextW(dc,L"برای پذیرش بیمار، روی «پذیرش بیمار» کلیک کنید.",-1,&br1,
        DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

    SelectObject(dc,g_fSmall);
    SetTextColor(dc,g_theme.textDim);
    RECT br2={card.left+S(28), br1.bottom+S(2), card.right-S(28), br1.bottom+S(2)+S(24)};
    DrawTextW(dc,L"این تب برای کارهای آینده آماده است.",-1,&br2,
        DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
}

// ------------------------------------------------- right info-panel paint --
//  Draws a round profile avatar with a GUEST icon coloured by the patient's
//  gender (no real photo), the نسخه الکترونیک / قبض-بارکد chips, the «P : S»
//  counters and the group titles for the search-keys / insurance / doctor
//  blocks. The actual edit controls are positioned by tabPageLayout.
static void drawGuestAvatar(HDC dc, int cx, int cy, int r, bool female){
    // v1.9.1: the placeholder avatar is a calm, neutral GRAY (no blue/pink) so
    // it reads as a generic "no photo" person. The silhouette is drawn SMALLER
    // than the disc and the disc is used as a CLIP region, so the head/shoulders
    // are perfectly centred and NOTHING ever spills past the rounded edge.
    (void)female;
    COLORREF ring = blendColor(g_theme.textDim, g_theme.border, 35);
    COLORREF fig  = blendColor(g_theme.textDim, g_theme.surface, 20);
    // --- disc (filled circle + ring) ---
    HBRUSH br=CreateSolidBrush(g_theme.surface2);
    HPEN pn=CreatePen(PS_SOLID,S(2),ring);
    HGDIOBJ ob=SelectObject(dc,br), op=SelectObject(dc,pn);
    Ellipse(dc,cx-r,cy-r,cx+r,cy+r);
    SelectObject(dc,op); SelectObject(dc,ob);
    // --- clip everything that follows to the inside of the disc ---
    // (slightly inset so the ring stays clean and crisp)
    int ci = r - S(2);
    HRGN clip = CreateEllipticRgn(cx-ci, cy-ci, cx+ci, cy+ci);
    int savedDC = SaveDC(dc);
    SelectClipRgn(dc, clip);
    // --- compact, centred guest silhouette (smaller than the disc) ---
    HBRUSH brh=CreateSolidBrush(fig);
    HGDIOBJ ob2=SelectObject(dc,brh);
    HGDIOBJ op2=SelectObject(dc,GetStockObject(NULL_PEN));
    int hr = r*30/100;          // head radius (smaller)
    int hy = cy - r*22/100;     // head centre y (sits in the upper third)
    Ellipse(dc,cx-hr,hy-hr,cx+hr,hy+hr);
    // shoulders: a wide, shallow rounded arc tucked under the head. Kept fully
    // inside the disc — the bottom of the arc stays above the disc edge.
    int sw = r*60/100;          // half-width of the shoulders
    int sTop = cy + r*10/100;   // top of the shoulder ellipse
    int sBot = cy + r*95/100;   // bottom — clip handles any overshoot anyway
    Ellipse(dc,cx-sw,sTop,cx+sw,sBot);
    SelectObject(dc,op2); SelectObject(dc,ob2);
    // --- restore clip + free GDI objects ---
    RestoreDC(dc, savedDC);
    SelectClipRgn(dc, NULL);
    DeleteObject(clip);
    DeleteObject(br); DeleteObject(pn); DeleteObject(brh);
}
static void paintInfoGroup(HDC dc, int iL, int iR, int y, const wchar_t* title, int icon, double f=1.0){
    int titleH=(int)(S(18)*f+0.5); if(titleH<S(13)) titleH=S(13);
    int icoW=(int)(S(16)*f+0.5); if(icoW<S(12)) icoW=S(12);
    SelectObject(dc,fitFont(15,FW_BOLD,f)); SetTextColor(dc,g_theme.accent);
    RECT si={iR-icoW,y+(titleH-icoW)/2,iR,y+(titleH-icoW)/2+icoW};
    drawIcon(dc,icon,si,g_theme.accent,S(2));
    RECT sr={iL,y,iR-icoW-S(6),y+titleH};
    DrawTextW(dc,title,-1,&sr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    // v1.63.0: the group rule now fades from the title edge (RTL: right) toward
    // the left instead of being a hard pen line across the whole panel — the
    // group heading reads as a heading, not a table row.
    { int y2=y+titleH+S(2), span=iR-iL;
      if(span>0){
        int seg=S(6); if(seg<2) seg=2;
        for(int x=iL;x<iR;x+=seg){
            int xe=x+seg; if(xe>iR) xe=iR;
            int a=200-(180*(iR-x))/span;
            if(a<8) a=8; if(a>255) a=255;
            gpLine(dc,x,y2,xe,y2,blendColor(g_theme.border,g_theme.accent,25),1.0f,a);
        }
      } }
}
//  Helper: draw a small field caption (right-aligned, dim) above a control.
static void paintInfoLabel(HDC dc, int iL, int iR, int y, const wchar_t* txt, double f=1.0){
    int lblH=(int)(S(16)*f+0.5); if(lblH<S(11)) lblH=S(11);
    SelectObject(dc,fitFont(13,FW_SEMIBOLD,f)); SetTextColor(dc,g_theme.labelInk);
    RECT tr={iL,y,iR,y+lblH};
    DrawTextW(dc,txt,-1,&tr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
}
static void paintInfoPanel(HDC dc, TabPage* t, int infoL, int infoR, int H, int sy){
    // H here is the VIRTUAL bottom (already had sy subtracted by the caller);
    // recover the real virtual height for layout, then offset everything by sy.
    int VH = H + sy;
    RECT card={infoL,S(16)-sy,infoR,VH-S(16)-sy};
    // v2: clip the rounded card so nothing bleeds outside the rounded corners,
    // and patch the corner gaps with the page background so no square shows.
    // v1.63.0: the patient panel is the tallest object on the tab, so it now
    // carries a real elevation shadow + surface gradient (matching the form
    // cards) and a soft accent wash across the profile header area so the
    // avatar block reads as a distinct "identity" zone.
    gpShadow(dc,card,S(16),S(9),g_dark?76:50);
    gpGradRoundRectBg(dc,card,S(16),g_theme.surfaceTop,g_theme.surface,
        g_theme.border,g_theme.bg);
    InfoLayout L; computeInfoLayout(infoL,infoR,VH,L);
    // shift the whole computed layout up by the scroll offset
    #define SY(v) ((v)-sy)
    const int iL=L.iL, iR=L.iR;
    //  v1.31.0: fit-scaled fonts so the whole profile card scales with the panel
    //  and never overlaps (بیمار جدید touching the border / chip on top of boxes).
    const double f=L.fitF;
    HFONT fCap = fitFont(15, FW_BOLD,     f);   // «بیمار جدید»
    HFONT fSm  = fitFont(12, FW_NORMAL,   f);   // sub-captions
    HFONT fVal = fitFont(15, FW_BOLD,     f);   // counter values / P·S digits
    HFONT fBadge=fitFont(15, FW_BOLD,     f);   // P / S glyph
    int   capLnH=(int)(S(18)*f+0.5); if(capLnH<S(13)) capLnH=S(13);
    // gender from the combo
    bool female = (SendMessageW(t->cGender,CB_GETCURSEL,0,0)==1);
    // --- avatar (neutral gray, perfectly centred) ---
    drawGuestAvatar(dc,L.avCx,SY(L.avCy),L.avR,female);
    // --- «بیمار جدید» + «بدون سابقه» caption (layout-driven, no fixed offset) ---
    { int capY=SY(L.capY);
      SelectObject(dc,fCap); SetTextColor(dc,g_theme.text);
      RECT t1={iL,capY,iR,capY+capLnH+S(2)};
      DrawTextW(dc,L"بیمار جدید",-1,&t1,DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
      SelectObject(dc,fSm); SetTextColor(dc,g_theme.textDim);
      RECT t2={iL,capY+capLnH+S(2),iR,capY+2*capLnH+S(4)};
      DrawTextW(dc,L"بدون سابقه",-1,&t2,DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX); }
    // --- نسخه الکترونیک chip (GREEN, soft fill) — near full width, centered ---
    { int pad=L.iw/8;
      RECT chip={iL+pad,SY(L.chipY),iR-pad,SY(L.chipY)+L.chipH};
      COLORREF gfill=blendColor(g_theme.success,g_theme.surface,78);
      // v1.63.0: the status chip is a gradient pill with a whisper of
      // elevation, so it sits ON the card instead of being stamped into it.
      gpShadowColor(dc,chip,L.chipH/2,S(4),46,g_theme.success);
      gpGradRoundRect(dc,chip,L.chipH/2,
          blendColor(gfill,RGB(255,255,255),g_dark?10:40),gfill,
          blendColor(g_theme.success,g_theme.surface,40));
      SelectObject(dc,fSm); SetTextColor(dc,g_theme.success);
      DrawTextW(dc,L"نسخه الکترونیک",-1,&chip,
          DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
    // --- two counter boxes: نسخه (سرپایی) | نسخه (الکترونیکی) ---
    { int half=(L.iw-S(8))/2;
      int lnH=(int)(L.boxH*0.45); if(lnH<S(13)) lnH=S(13);
      RECT bL={iL,SY(L.boxY),iL+half,SY(L.boxY)+L.boxH};
      RECT bR={iR-half,SY(L.boxY),iR,SY(L.boxY)+L.boxH};
      for(int k=0;k<2;k++){
          RECT bx = k==0?bR:bL;          // RTL: first box on the RIGHT
          // v1.63.0: an inset "well" look — a top-down gradient from a slightly
          // darker shade to the input colour reads as recessed, which is the
          // correct affordance for a read-only counter (vs. a raised button).
          gpGradRoundRect(dc,bx,S(9),
              blendColor(g_theme.inputBg,g_theme.border,g_dark?26:34),
              g_theme.inputBg,g_theme.border);
          SelectObject(dc,fSm); SetTextColor(dc,g_theme.textDim);
          RECT lr={bx.left,bx.top+S(3),bx.right,bx.top+lnH};
          DrawTextW(dc,k==0?L"نسخه (سرپایی)":L"نسخه (الکترونیکی)",-1,&lr,
              DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
          SelectObject(dc,fVal); SetTextColor(dc,g_theme.text);
          RECT vr={bx.left,bx.top+lnH,bx.right,bx.bottom-S(2)};
          DrawTextW(dc,L"۰",-1,&vr,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
      }
    }
    //  --- P (پرداختی, YELLOW) / S (صندوق, GREEN) cards — SINGLE location ---
    //  v1.32.0: each card is a bordered surface2 chip with a small colored badge
    //  square on the OUTER edge and a stacked (label / value) block filling the
    //  rest. The badge is sized well inside the card so the label + value never
    //  collide with it or the neighbour card (fixes the cramped P·S row).
    { int boxH=L.psH; int gapPS=S(8); int half=(L.iw-gapPS)/2;
      int sq=boxH-S(14); if(sq<S(18)) sq=S(18);
      int lnH=(int)(boxH*0.40); if(lnH<S(11)) lnH=S(11);
      RECT pBox={iR-half,SY(L.psY),iR,SY(L.psY)+boxH};       // P on the RIGHT
      RECT sBox={iL,SY(L.psY),iL+half,SY(L.psY)+boxH};       // S on the LEFT
      auto psCard=[&](RECT box,COLORREF badge,COLORREF glyphCol,const wchar_t* glyph,
                      const wchar_t* lbl,COLORREF txt){
          // v1.63.0: the P/S cards are raised chips with a gradient body, and
          // the badge square glows in its own colour so the two metrics are
          // instantly distinguishable at a glance.
          gpShadow(dc,box,S(9),S(5),g_dark?58:38);
          gpGradRoundRect(dc,box,S(9),
              blendColor(g_theme.surfaceTop,g_theme.surface2,40),
              g_theme.surface2,g_theme.border);
          // badge square hugging the OUTER edge, vertically centered
          int by=box.top+(boxH-sq)/2;
          RECT sqR={box.right-S(6)-sq,by,box.right-S(6),by+sq};
          gpShadowColor(dc,sqR,S(7),S(5),100,badge);
          gpGradRoundRect(dc,sqR,S(7),
              blendColor(badge,RGB(255,255,255),26),badge,CLR_INVALID);
          SelectObject(dc,fBadge); SetTextColor(dc,glyphCol);
          DrawTextW(dc,glyph,-1,&sqR,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
          // stacked label / value in the remaining LEFT space
          int txR=sqR.left-S(6), txL=box.left+S(6);
          RECT lr={txL,box.top+S(6),txR,box.top+S(6)+lnH};
          SelectObject(dc,fSm); SetTextColor(dc,g_theme.textDim);
          DrawTextW(dc,lbl,-1,&lr,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
          RECT vr={txL,box.top+S(6)+lnH,txR,box.bottom-S(4)};
          SelectObject(dc,fVal); SetTextColor(dc,txt);
          DrawTextW(dc,L"۰",-1,&vr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
      };
      psCard(pBox,g_theme.warn,   RGB(40,40,40),   L"P",L"پرداختی",g_theme.text);
      psCard(sBox,g_theme.success,RGB(255,255,255),L"S",L"صندوق", g_theme.text);
    }
    // group titles + per-row field captions — share the SAME layout as controls
    paintInfoGroup(dc,iL,iR,SY(L.g1TitleY),L"کلیدهای جستجو",ICO_ID,f);
    paintInfoLabel(dc,iL,iR,SY(L.archiveLblY),L"شماره بایگانی",f);
    paintInfoLabel(dc,iL,iR,SY(L.fileLblY),   L"شماره پرونده",f);
    paintInfoGroup(dc,iL,iR,SY(L.g2TitleY),L"بیمه",ICO_SHIELD,f);
    paintInfoLabel(dc,iL,iR,SY(L.bookLblY), L"شماره دفترچه",f);
    paintInfoLabel(dc,iL,iR,SY(L.validLblY),L"تاریخ اعتبار",f);
    paintInfoLabel(dc,iL,iR,SY(L.rxLblY),   L"تاریخ نسخه",f);
    paintInfoLabel(dc,iL,iR,SY(L.suppLblY), L"بیمه مکمل",f);
    //  v1.32.0 DE-DUPLICATION (script item #9): the right-panel «جستجوی پزشک معالج»
    //  group (code + name) duplicated the «پزشک معالج» card in the CENTER column,
    //  so it is NO LONGER painted here. The doctor is chosen once, in the center.
    //  v1.31.0: the bottom «انجام دهنده: <user>» line was likewise a DUPLICATE of
    //  the center «انجام دهنده» card and was removed (script item #27).
    #undef SY
}

// §1.19.0 — Resolve the operator's department (a display name string in
// g_session.user.dept) to a stable Section id, so the new print_designer
// design bound to that section is used. Returns 0 when no match is found
// (caller then falls back to the legacy section-index print path).
static int recResolveSectionId(){
    std::wstring dept=g_session.user.dept;
    std::vector<Section> all;
    Sections_All(all);
    if(all.empty()) return 0;
    if(!dept.empty()){
        // exact name match first
        for(const auto& s:all) if(s.is_active && s.name_fa==dept) return s.id;
        // then a code match (operator dept might already be a code)
        for(const auto& s:all) if(s.is_active && s.code==dept) return s.id;
        // then substring (tolerant of decorations)
        for(const auto& s:all)
            if(s.is_active && (s.name_fa.find(dept)!=std::wstring::npos ||
                               dept.find(s.name_fa)!=std::wstring::npos)) return s.id;
    }
    // default: first active reception section, else first active section
    for(const auto& s:all) if(s.is_active && s.kind==L"reception") return s.id;
    for(const auto& s:all) if(s.is_active) return s.id;
    return 0;
}

// ----------------------------------------------------------- tab page proc -
static LRESULT CALLBACK tabPageProc(HWND h, UINT m, WPARAM w, LPARAM l){
    TabPage* t=(TabPage*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_CREATE: {
        CREATESTRUCTW* cs=(CREATESTRUCTW*)l;
        t=(TabPage*)cs->lpCreateParams;
        SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)t);
        t->page=h;
        // §3 hybrid host: when opening the Appointment tab we first try to bring
        // up the HTML/CSS/JS surface (system MSHTML/WebBrowser OLE control) with a
        // centred loader while native state synchronises. C++ stays the host /
        // validator / lifecycle owner; JS owns only layout + interaction. If the
        // renderer is unavailable or fails, we fall back to the classic native
        // appointment page so the app NEVER loses the feature.
        // Portal-message and empty tabs are pure painted pages with no form
        // controls — they own no edit boxes, combos or buttons.
        //  v1.60.0: the appointment (نوبت‌دهی) child page branch was removed
        //  together with the whole feature.
        //  v1.33.0: PREFERRED renderer — «پذیرش بیمار» is rendered by an
        //  embedded WebView2 (Chromium) surface loaded from the in-app loopback
        //  host: the modern HTML/CSS/JS admission UI, fully two-way synced with
        //  C++ through the structured IPC bridge (see web_admission.*). When the
        //  WebView2 runtime is present we create that view and RETURN — skipping
        //  the native controls entirely. If the runtime is missing we fall
        //  through to the proven native GDI reception form below, so the app
        //  keeps working on every Windows (7→11+) offline. The page never opens
        //  in an external browser.
        //  v1.84.0: tools / cashier / a receipt-backed پذیرش reuse the same
        //  inline page with a per-host surface inject. No GDI fallback for
        //  tools or cashier.
        //  v2.02: HTML surfaces are driven by tabKindSurface() — TK_SVREPORT
        //  used to miss the old allow-list, so the tab opened as a blank
        //  painted page that looked like «تب جدید».
        t->web = NULL;
        if(const char* surf=tabKindSurface(t->kind)){
            if(WebAdmission_Available()){
                HWND wv = WebAdmission_CreateViewEx(h, surf, t->extraJson);
                if(wv){ t->web = wv; return 0; }   // embedded UI owns the whole tab
            }
            // v1.89: TK_DASH also falls back to the native admission form, so a
            // machine without WebView2 AND MSHTML still has a working پذیرش path.
            if(t->kind!=TK_RECEPTION && t->kind!=TK_DASH) return 0;
        } else {
            return 0;   // TK_EMPTY: native painted blank page
        }
        // v1.25.0: ES_RIGHT so every textbox is right-aligned (راست‌چین) by
        // default — Persian RTL data entry. Fields with enableAutoDir still flip
        // alignment live based on typed content (Latin vs Persian).
        DWORD es=WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL|ES_RIGHT;
        DWORD cbs=WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST;
        t->eFirst =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)(ID_F_FIRST+0),g_hInst,0);
        t->eLast  =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)(ID_F_FIRST+1),g_hInst,0);
        t->eNid   =CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)(ID_F_FIRST+2),g_hInst,0);
        t->eFather=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)(ID_F_FIRST+3),g_hInst,0);
        t->eBirth =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)(ID_F_FIRST+4),g_hInst,0);
        t->cGender=createThemedCombo(h,ID_F_GENDER);
        t->eMobile=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)(ID_F_FIRST+5),g_hInst,0);
        t->ePhone =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)(ID_F_FIRST+6),g_hInst,0);
        t->eAddr  =CreateWindowExW(WS_EX_RTLREADING|WS_EX_RIGHT,L"EDIT",L"",es,0,0,10,10,h,(HMENU)(ID_F_FIRST+7),g_hInst,0);
        t->cPType =createThemedCombo(h,ID_F_PTYPE);
        t->cNType =createThemedCombo(h,ID_F_NTYPE);
        t->cIns   =createThemedCombo(h,ID_F_INS);
        t->cSupp  =createThemedCombo(h,ID_F_SUPP);
        (void)cbs;
        t->ePrice =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_F_PRICE,g_hInst,0);
        t->eDiscount=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_F_DISCOUNT,g_hInst,0);
        // «دارای بیمه» checkbox — placed beside the national-id field; checked by
        // default so Enter on the id field runs the validated insurance inquiry.
        // Unchecking it switches to manual insurance selection (no auto-inquiry).
        t->chkIns =CreateWindowExW(0,L"BUTTON",L"دارای بیمه",
            WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|BS_RIGHTBUTTON|BS_RIGHT,
            0,0,10,10,h,(HMENU)ID_F_HASINS,g_hInst,0);
        SendMessageW(t->chkIns,WM_SETFONT,(WPARAM)g_fSmall,TRUE);
        SendMessageW(t->chkIns,BM_SETCHECK,BST_CHECKED,0);

        SendMessageW(t->cGender,CB_ADDSTRING,0,(LPARAM)L"مرد");
        SendMessageW(t->cGender,CB_ADDSTRING,0,(LPARAM)L"زن");
        SendMessageW(t->cGender,CB_SETCURSEL,0,0);
        SendMessageW(t->cPType,CB_ADDSTRING,0,(LPARAM)L"عادی");
        SendMessageW(t->cPType,CB_ADDSTRING,0,(LPARAM)L"سرپایی");
        SendMessageW(t->cPType,CB_ADDSTRING,0,(LPARAM)L"بستری");
        SendMessageW(t->cPType,CB_SETCURSEL,0,0);
        SendMessageW(t->cNType,CB_ADDSTRING,0,(LPARAM)L"عادی");
        SendMessageW(t->cNType,CB_ADDSTRING,0,(LPARAM)L"اورژانس");
        SendMessageW(t->cNType,CB_ADDSTRING,0,(LPARAM)L"پرسنلی");
        SendMessageW(t->cNType,CB_SETCURSEL,0,0);
        for(int i=0;i<N_INSURANCES;i++)
            SendMessageW(t->cIns,CB_ADDSTRING,0,(LPARAM)INSURANCES[i].name);
        SendMessageW(t->cIns,CB_SETCURSEL,0,0);
        for(int i=0;i<N_SUPP;i++)
            SendMessageW(t->cSupp,CB_ADDSTRING,0,(LPARAM)SUPP_INSURANCES[i].name);
        SendMessageW(t->cSupp,CB_SETCURSEL,0,0);

        // bottom action row (matches reference): ثبت (primary) | پاک کردن |
        // پذیرش بیمار | انصراف
        t->bSubmit =createFlatButton(h,ID_F_SUBMIT,L"ثبت پذیرش و صدور قبض",ICO_SAVE,BS_PRIMARY,0,0,10,10);
        // §1.18.1: «پاک کردن» is the destructive/clear action → danger (red) style,
        // matching the reference image (red trash icon + red text).
        t->bReset  =createFlatButton(h,ID_F_RESET,L"پاک کردن",ICO_TRASH,BS_DANGER,0,0,10,10);
        t->bNew    =createFlatButton(h,ID_F_NEW,L"پذیرش بیمار",ICO_PLUS,BS_OUTLINE,0,0,10,10);
        t->bClose  =createFlatButton(h,ID_F_CLOSE,L"انصراف",ICO_X,BS_OUTLINE,0,0,10,10);
        // services table toolbar
        t->bSvcAdd =createFlatButton(h,ID_F_SVC_ADD,L"افزودن خدمت",ICO_PLUS,BS_PRIMARY,0,0,10,10);
        t->eSvcSearch=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_F_SVC_SEARCH,g_hInst,0);
        SendMessageW(t->eSvcSearch,WM_SETFONT,(WPARAM)g_fUI,TRUE);

        // ---- v1.25.0: center Treating-doctor / Performer cards ----
        t->ePerfCode = CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)ID_F_PERF_CODE,g_hInst,0);
        t->cPerfList = createThemedCombo(h,ID_F_PERF_LIST);
        t->eDoc2Code = CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)ID_F_DOC2_CODE,g_hInst,0);
        t->cDoc2List = createThemedCombo(h,ID_F_DOC2_LIST);
        // supplementary percentage on the center doctor card
        t->eSuppPct2 = CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)ID_F_SUPP_PCT2,g_hInst,0);
        // complementary-insurance percentage on the center insurance card
        t->eSuppPctIns = CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)ID_F_SUPP_PCTINS,g_hInst,0);
        // appointment card
        t->eApptDate  = CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_F_APPT_DATE,g_hInst,0);
        t->cApptShift = createThemedCombo(h,ID_F_APPT_SHIFT);
        t->eApptP     = CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)ID_F_APPT_P,g_hInst,0);
        t->eApptS     = CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)ID_F_APPT_S,g_hInst,0);
        // populate the doctor / performer list combos from the doctors store
        {
            auto docs=loadDoctors();
            int perfRows=0;
            for(size_t di=0;di<docs.size();di++){
                auto& d=docs[di];
                std::wstring s=d.name+ (d.specialty.empty()?L"":(L" — "+d.specialty));
                SendMessageW(t->cDoc2List,CB_ADDSTRING,0,(LPARAM)s.c_str());
                // v1.78.0: «انجام دهنده» فقط پزشکان تیک‌دار «تعریف به عنوان
                // انجام دهنده» را نشان می‌دهد (پیش‌فرض: همه). ردیف کمبو اندیس
                // اصلی پزشک را در item-data نگه می‌دارد تا کد عددی همان معنای
                // ۱-مبنای لیست کامل را داشته باشد.
                if(d.isPerformer){
                    int row=(int)SendMessageW(t->cPerfList,CB_ADDSTRING,0,(LPARAM)s.c_str());
                    if(row>=0) SendMessageW(t->cPerfList,CB_SETITEMDATA,(WPARAM)row,(LPARAM)di);
                    perfRows++;
                }
            }
            /* v2.06: NO AUTO-SELECT (user request) — the combos open on the
               empty «— انتخاب —» row; the operator explicitly picks. */
            SendMessageW(t->cDoc2List,CB_INSERTSTRING,0,(LPARAM)L"— انتخاب —");
            SendMessageW(t->cPerfList,CB_INSERTSTRING,0,(LPARAM)L"— انتخاب —");
            SendMessageW(t->cDoc2List,CB_SETCURSEL,0,0);
            SendMessageW(t->cPerfList,CB_SETCURSEL,0,0);
        }
        SendMessageW(t->cApptShift,CB_ADDSTRING,0,(LPARAM)L"صبح");
        SendMessageW(t->cApptShift,CB_ADDSTRING,0,(LPARAM)L"عصر");
        SendMessageW(t->cApptShift,CB_ADDSTRING,0,(LPARAM)L"شب");
        SendMessageW(t->cApptShift,CB_SETCURSEL,g_session.shift>=0&&g_session.shift<3?g_session.shift:0,0);
        SetWindowTextW(t->eApptDate, FormatJalaliPersian(0).c_str());
        SetWindowTextW(t->eApptP, L"0"); SetWindowTextW(t->eApptS, L"0");

        // ---- v1.25.0: inline «افزودن خدمت» panel controls ----
        t->eSvcCode   = CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)ID_F_SVC_CODE,g_hInst,0);
        t->eSvcName   = CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_F_SVC_NAME,g_hInst,0);
        t->eSvcQty    = CreateWindowExW(0,L"EDIT",L"1",es|ES_NUMBER,0,0,10,10,h,(HMENU)ID_F_SVC_QTY,g_hInst,0);
        t->chkSvcFree = CreateWindowExW(0,L"BUTTON",L"نرخ آزاد",
            WS_CHILD|BS_AUTOCHECKBOX|BS_RIGHTBUTTON|BS_RIGHT,0,0,10,10,h,(HMENU)ID_F_SVC_FREECHK,g_hInst,0);
        t->eSvcFreeAmt= CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)ID_F_SVC_FREEAMT,g_hInst,0);
        t->bSvcConfirm= createFlatButton(h,ID_F_SVC_CONFIRM,L"افزودن",ICO_CHECK,BS_PRIMARY,0,0,10,10);

        // ---- v1.25.0: bottom tab-area buttons ----
        t->bAddUnpaid = createFlatButton(h,ID_F_ADD_UNPAID,L"افزودن به صندوق نرفته‌ها",ICO_PLUS,BS_OUTLINE,0,0,10,10);
        t->bAddQueue  = createFlatButton(h,ID_F_ADD_QUEUE,L"افزودن به صف پذیرش",ICO_PLUS,BS_OUTLINE,0,0,10,10);
        // v1.26.0: unpaid-panel toolbar (date filter + hours window + refresh)
        t->eUpDate    = CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_F_UP_DATE,g_hInst,0);
        t->cUpHours   = createThemedCombo(h,ID_F_UP_HOURS);
        t->bUpRefresh = createFlatButton(h,ID_F_UP_REFRESH,L"",ICO_REFRESH,BS_OUTLINE,0,0,10,10);
        t->chkNoPay   = CreateWindowExW(0,L"BUTTON",L"عدم پرداخت در حال حاضر",
            WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|BS_RIGHTBUTTON|BS_RIGHT,0,0,10,10,h,(HMENU)ID_F_NOPAY_CHK,g_hInst,0);
        // fonts + behaviour for the new controls
        {
            HWND newEds[]={t->ePerfCode,t->eDoc2Code,t->eSuppPct2,t->eSuppPctIns,t->eApptDate,
                t->eApptP,t->eApptS,t->eSvcCode,t->eSvcName,t->eSvcQty,t->eSvcFreeAmt};
            for(HWND e: newEds){ SendMessageW(e,WM_SETFONT,(WPARAM)g_fUI,TRUE); enableEnterNavigation(e); }
            HWND newCbs[]={t->cPerfList,t->cDoc2List,t->cApptShift};
            for(HWND c: newCbs) SendMessageW(c,WM_SETFONT,(WPARAM)g_fUI,TRUE);
            SendMessageW(t->chkSvcFree,WM_SETFONT,(WPARAM)g_fSmall,TRUE);
            SendMessageW(t->chkNoPay,WM_SETFONT,(WPARAM)g_fSmall,TRUE);
            enableAutoDir(t->eSvcName);
            SendMessageW(t->eApptDate,EM_SETLIMITTEXT,10,0); enableDateMask(t->eApptDate);
            // v1.25.0 special Enter flows: service code→name→qty, doctor codes.
            enableSvcCodeEnter(t->eSvcCode);
            enableSvcNameEnter(t->eSvcName);
            enableDocCodeEnter(t->eDoc2Code);
            enableDocCodeEnter(t->ePerfCode);
            EnableWindow(t->eSvcFreeAmt,FALSE);      // enabled only when «نرخ آزاد» ticked
            setFlatButtonBg(t->bSvcAdd,   g_theme.surface);
            setFlatButtonBg(t->bSvcConfirm,g_theme.surface);
            setFlatButtonBg(t->bAddUnpaid,g_theme.surface);
            setFlatButtonBg(t->bAddQueue, g_theme.surface);
            // v1.26.0: unpaid-panel toolbar controls
            SendMessageW(t->eUpDate,WM_SETFONT,(WPARAM)g_fUI,TRUE);
            SendMessageW(t->eUpDate,EM_SETLIMITTEXT,10,0); enableDateMask(t->eUpDate);
            SetWindowTextW(t->eUpDate, FormatJalaliPersian(0).c_str());
            SendMessageW(t->cUpHours,WM_SETFONT,(WPARAM)g_fUI,TRUE);
            SendMessageW(t->cUpHours,CB_ADDSTRING,0,(LPARAM)L"۶");
            SendMessageW(t->cUpHours,CB_ADDSTRING,0,(LPARAM)L"۱۲");
            SendMessageW(t->cUpHours,CB_ADDSTRING,0,(LPARAM)L"۲۴");
            SendMessageW(t->cUpHours,CB_SETCURSEL,1,0);
            setFlatButtonBg(t->bUpRefresh,g_theme.surface);
        }
        // print actions (LEFT billing card)
        t->bPrtIns =createFlatButton(h,ID_F_PRT_INS,L"رسید بیمه",ICO_PRINT,BS_OUTLINE,0,0,10,10);
        t->bPrtRx  =createFlatButton(h,ID_F_PRT_RX,L"چاپ نسخه",ICO_PRINT,BS_OUTLINE,0,0,10,10);
        t->bPrtLast=createFlatButton(h,ID_F_PRT_LAST,L"چاپ آخرین قبض (F8)",ICO_PRINT,BS_OUTLINE,0,0,10,10);
        t->bInquiry=createFlatButton(h,ID_F_INQUIRY,L"استعلام بیمه",ICO_SHIELD,BS_OUTLINE,0,0,10,10);
        // v1.4.1: real raster icons on the print-action buttons (left card)
        setFlatButtonImage(t->bPrtIns, IMG_IC_SHIELD);
        setFlatButtonImage(t->bPrtRx,  IMG_IC_RECEIPT);
        setFlatButtonImage(t->bPrtLast,IMG_IC_LAST);
        // blend button corners: submit sits on the form card (surface); the
        // billing/print buttons sit on the page background (bg).
        setFlatButtonBg(t->bSubmit, g_theme.surface);
        setFlatButtonBg(t->bReset,  g_theme.surface);
        setFlatButtonBg(t->bNew,    g_theme.surface);
        setFlatButtonBg(t->bClose,  g_theme.surface);
        setFlatButtonBg(t->bSvcAdd, g_theme.surface);
        setFlatButtonBg(t->bPrtIns, g_theme.surface);
        setFlatButtonBg(t->bPrtRx,  g_theme.surface);
        setFlatButtonBg(t->bPrtLast,g_theme.surface);
        setFlatButtonBg(t->bInquiry,g_theme.surface);

        // birth-date uses the smart Jalali mask (digits-only, auto slashes)
        HWND eds[11]={t->eFirst,t->eLast,t->eNid,t->eFather,
                      t->eMobile,t->ePhone,t->eAddr,t->ePrice,t->eDiscount,0,0};
        for(int i=0;eds[i];i++){
            SendMessageW(eds[i],WM_SETFONT,(WPARAM)g_fUI,TRUE);
            enableEnterNavigation(eds[i]);
        }
        // national-id field: override the plain Enter-navigation so Enter runs
        // the network-wide patient lookup + auto-fill (must be applied AFTER the
        // generic enableEnterNavigation above so it wins for this one field).
        enableNidLookup(t->eNid);
        SendMessageW(t->eNid,EM_SETLIMITTEXT,10,0);
        // auto RTL/LTR alignment on the free-text fields (names/address):
        // Persian content aligns right, Latin/digits align left.
        enableAutoDir(t->eFirst); enableAutoDir(t->eLast);
        enableAutoDir(t->eFather); enableAutoDir(t->eAddr);
        SendMessageW(t->eBirth,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        SendMessageW(t->eBirth,EM_SETLIMITTEXT,10,0);
        enableDateMask(t->eBirth);
        HWND cbsArr[5]={t->cGender,t->cPType,t->cNType,t->cIns,t->cSupp};
        for(int i=0;i<5;i++)
            SendMessageW(cbsArr[i],WM_SETFONT,(WPARAM)g_fUI,TRUE);
        // default patient type = سرپایی (most common), triggers tariff auto-fill
        SendMessageW(t->cPType,CB_SETCURSEL,1,0);

        // ====================== RIGHT INFO PANEL (v1.6.0) ====================
        // search keys
        t->eArchive  =CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)ID_F_ARCHIVE,g_hInst,0);
        t->bArchiveGo=createFlatButton(h,ID_F_ARCHIVE_GO,L"استعلام",ICO_NONE,BS_OUTLINE,0,0,10,10);
        t->eFile     =CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)ID_F_FILE,g_hInst,0);
        t->bFileGo   =createFlatButton(h,ID_F_FILE_GO,L"استعلام",ICO_NONE,BS_OUTLINE,0,0,10,10);
        // insurance block
        t->eBookNo   =CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)ID_F_BOOKNO,g_hInst,0);
        t->chkBookNo =CreateWindowExW(0,L"BUTTON",L"فعال",
            WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|BS_RIGHTBUTTON|BS_RIGHT,0,0,10,10,h,(HMENU)ID_F_BOOKNO_CHK,g_hInst,0);
        t->eValid    =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_F_VALID,g_hInst,0);
        t->chkValidAuto=CreateWindowExW(0,L"BUTTON",L"اتوماتیک",
            WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|BS_RIGHTBUTTON|BS_RIGHT,0,0,10,10,h,(HMENU)ID_F_VALID_AUTO,g_hInst,0);
        t->eRxDate   =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_F_RXDATE,g_hInst,0);
        t->chkRxAuto =CreateWindowExW(0,L"BUTTON",L"اتوماتیک",
            WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|BS_RIGHTBUTTON|BS_RIGHT,0,0,10,10,h,(HMENU)ID_F_RXDATE_AUTO,g_hInst,0);
        t->cSuppPanel=createThemedCombo(h,ID_F_SUPP_PANEL);
        t->eSuppPct  =CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)ID_F_SUPP_PCT,g_hInst,0);
        t->bSuppGo   =createFlatButton(h,ID_F_SUPP_GO,L"استعلام",ICO_NONE,BS_OUTLINE,0,0,10,10);
        // پزشک معالج
        t->eDocCode  =CreateWindowExW(0,L"EDIT",L"",es|ES_NUMBER,0,0,10,10,h,(HMENU)ID_F_DOCCODE,g_hInst,0);
        t->eDocName  =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_F_DOCNAME,g_hInst,0);
        t->bDocSearch=createFlatButton(h,ID_F_DOCSEARCH,L"جستجو",ICO_NONE,BS_OUTLINE,0,0,10,10);
        // fonts + behaviour
        HWND infoEds[]={t->eArchive,t->eFile,t->eBookNo,t->eValid,t->eRxDate,
                        t->eSuppPct,t->eDocCode,t->eDocName};
        for(HWND e: infoEds){ SendMessageW(e,WM_SETFONT,(WPARAM)g_fUI,TRUE); enableEnterNavigation(e); }
        enableAutoDir(t->eDocName);
        SendMessageW(t->eValid,EM_SETLIMITTEXT,10,0);  enableDateMask(t->eValid);
        SendMessageW(t->eRxDate,EM_SETLIMITTEXT,10,0); enableDateMask(t->eRxDate);
        SendMessageW(t->cSuppPanel,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        for(int i=0;i<N_SUPP;i++)
            SendMessageW(t->cSuppPanel,CB_ADDSTRING,0,(LPARAM)SUPP_INSURANCES[i].name);
        SendMessageW(t->cSuppPanel,CB_SETCURSEL,0,0);
        HWND infoChks[]={t->chkBookNo,t->chkValidAuto,t->chkRxAuto};
        for(HWND c: infoChks) SendMessageW(c,WM_SETFONT,(WPARAM)g_fSmall,TRUE);
        // ش دفترچه disabled until its checkbox is ticked
        EnableWindow(t->eBookNo,FALSE);
        SendMessageW(t->chkValidAuto,BM_SETCHECK,BST_CHECKED,0);
        SendMessageW(t->chkRxAuto,BM_SETCHECK,BST_CHECKED,0);
        // اتوماتیک = current Iran date/time (v1.4.0 §6: via FormatJalaliPersian)
        SetWindowTextW(t->eValid, FormatJalaliPersian(0).c_str());
        SetWindowTextW(t->eRxDate,FormatJalaliPersian(0).c_str());
        EnableWindow(t->eValid,FALSE); EnableWindow(t->eRxDate,FALSE);
        // doctor inquiry buttons sit on the surface (info panel) — blend corners
        setFlatButtonBg(t->bArchiveGo,g_theme.surface);
        setFlatButtonBg(t->bFileGo,   g_theme.surface);
        setFlatButtonBg(t->bSuppGo,   g_theme.surface);
        setFlatButtonBg(t->bDocSearch,g_theme.surface);

        recalc(t);
        return 0; }
    case WM_SIZE: if(t) tabPageLayout(h,t); return 0;
    case WM_VSCROLL: {
        // vertical scrollbar of the reception form
        if(!t || t->web || t->kind!=TK_RECEPTION) break;
        int maxS=recClampScroll(h,t);
        int old=t->scrollY;
        SCROLLINFO si={sizeof(si)}; si.fMask=SIF_TRACKPOS;
        GetScrollInfo(h,SB_VERT,&si);
        RECT rc; GetClientRect(h,&rc);
        int line=S(40), pageStep=rc.bottom-S(40); if(pageStep<line) pageStep=line;
        switch(LOWORD(w)){
            case SB_LINEUP:   t->scrollY-=line; break;
            case SB_LINEDOWN: t->scrollY+=line; break;
            case SB_PAGEUP:   t->scrollY-=pageStep; break;
            case SB_PAGEDOWN: t->scrollY+=pageStep; break;
            case SB_THUMBTRACK:
            case SB_THUMBPOSITION: t->scrollY=si.nTrackPos; break;
            case SB_TOP:      t->scrollY=0; break;
            case SB_BOTTOM:   t->scrollY=maxS; break;
        }
        recClampScroll(h,t);
        if(t->scrollY!=old){
            // v1.19.0 ultra-smooth scroll: freeze child-control repaints while we
            // batch-reposition them, then repaint the painted background AND all
            // controls together in ONE double-buffered frame. SetWindowRedraw
            // (WM_SETREDRAW) suppresses the per-control invalidations MoveWindow
            // would otherwise queue, so nothing is drawn twice and the cards,
            // field wells and edit boxes move in perfect lock-step — zero
            // flicker, zero tearing, no frame-by-frame lag even with 40+ controls.
            SendMessageW(h,WM_SETREDRAW,FALSE,0);
            tabPageLayout(h,t);
            SendMessageW(h,WM_SETREDRAW,TRUE,0);
            RedrawWindow(h,NULL,NULL,
                RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN|RDW_NOERASE);
        } else {
            recUpdateScrollbar(h,t);
        }
        return 0; }
    case WM_MOUSEWHEEL: {
        if(!t || t->web || t->kind!=TK_RECEPTION) break;
        int maxS=recClampScroll(h,t);
        if(maxS<=0) return 0;            // nothing to scroll
        int delta=GET_WHEEL_DELTA_WPARAM(w);
        int old=t->scrollY;
        t->scrollY -= (delta/WHEEL_DELTA)*S(60);
        recClampScroll(h,t);
        if(t->scrollY!=old){
            // §B (v1.10.0): no header-collapse animation on scroll. The action
            // bar stays at its fixed compact height; we just scroll the page.
            // v1.19.0: freeze child repaints, batch-reposition, then flush ONE
            // double-buffered frame so controls + painted background never
            // separate while wheel-scrolling (matches the WM_VSCROLL path).
            SendMessageW(h,WM_SETREDRAW,FALSE,0);
            tabPageLayout(h,t);
            SendMessageW(h,WM_SETREDRAW,TRUE,0);
            RedrawWindow(h,NULL,NULL,
                RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN|RDW_NOERASE);
        }
        return 0; }
    case WM_APP_THEME:
        if(t && t->kind==TK_RECEPTION){
            setFlatButtonBg(t->bSubmit, g_theme.surface);
            setFlatButtonBg(t->bPrtIns, g_theme.surface);
            setFlatButtonBg(t->bPrtRx,  g_theme.surface);
            setFlatButtonBg(t->bPrtLast,g_theme.surface);
            setFlatButtonBg(t->bClose,  g_theme.surface);
            setFlatButtonBg(t->bInquiry,g_theme.surface);
            setFlatButtonBg(t->bArchiveGo,g_theme.surface);
            setFlatButtonBg(t->bFileGo,   g_theme.surface);
            setFlatButtonBg(t->bSuppGo,   g_theme.surface);
            setFlatButtonBg(t->bDocSearch,g_theme.surface);
            setFlatButtonBg(t->bSvcAdd,   g_theme.surface);
            setFlatButtonBg(t->bSvcConfirm,g_theme.surface);
            setFlatButtonBg(t->bAddUnpaid,g_theme.surface);
            setFlatButtonBg(t->bAddQueue, g_theme.surface);
            setFlatButtonBg(t->bUpRefresh,g_theme.surface);
        }
        InvalidateRect(h,NULL,TRUE);
        return 0;
    case WM_CTLCOLOREDIT: {
        HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_CTLCOLORLISTBOX: {
        HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_CTLCOLORSTATIC: {
        HDC dc=(HDC)w;
        // Checkboxes sit on a card (surface) — paint their labels with surface
        // bg + normal text so they blend with the card (no white box).
        if(t && ((HWND)l==t->chkIns || (HWND)l==t->chkBookNo ||
                 (HWND)l==t->chkValidAuto || (HWND)l==t->chkRxAuto)){
            SetTextColor(dc,g_theme.text); SetBkColor(dc,g_theme.surface);
            return (LRESULT)g_brSurface;
        }
        // CBS_DROPDOWNLIST combos paint their closed display via STATIC color —
        // theme it so the selected text isn't white-on-white in dark mode.
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_DRAWITEM: {
        // theme-aware owner-draw combobox items (fixes dark-mode dropdown)
        if(drawThemedComboItem((LPDRAWITEMSTRUCT)l)) return TRUE;
        break; }
    case WM_COMMAND: {
        if(!t || t->web) return 0;   // hybrid host owns input; no native controls
        int id=LOWORD(w), code=HIWORD(w);
        // Repaint the field wells whenever an edit/combo gains or loses focus so
        // the focused well border updates immediately (thin red focus ring that
        // fades back to the normal hairline once the field is left/clicked away).
        if(code==EN_SETFOCUS || code==EN_KILLFOCUS ||
           code==CBN_SETFOCUS || code==CBN_KILLFOCUS){
            InvalidateRect(h,NULL,FALSE);
        }
        // v1.9.0: editing any field clears the empty-field red markers so they
        // never linger after the operator starts correcting the form.
        if((code==EN_CHANGE || code==CBN_SELCHANGE) && t->invalidMask){
            t->invalidMask=0; InvalidateRect(h,NULL,FALSE);
        }
        if((id==ID_F_INS||id==ID_F_SUPP) && code==CBN_SELCHANGE){
            recalc(t); InvalidateRect(h,NULL,FALSE);
        }
        else if((id==ID_F_PTYPE||id==ID_F_NTYPE) && code==CBN_SELCHANGE){
            // patient/appointment type changed → re-derive the default tariff
            SetWindowTextW(t->ePrice, L"");     // clear so recalc auto-fills
            recalc(t); InvalidateRect(h,NULL,FALSE);
        }
        else if(id==ID_F_PRICE && code==EN_CHANGE){
            if(!t->autoPrice){ recalc(t); InvalidateRect(h,NULL,FALSE); }
        }
        else if(id==ID_F_DISCOUNT && code==EN_CHANGE){
            recalc(t); InvalidateRect(h,NULL,FALSE);
        }
        else if(id==(ID_F_FIRST+2) && code==EN_KILLFOCUS){
            // national-id field lost focus (Tab navigates away). Auto-fill the
            // patient identity from a trusted source whenever a complete code is
            // present — quietly when not insured (so no error nags the operator),
            // verbosely when insured (so an invalid code is flagged). Enter on the
            // field itself is handled by nidEditProc which always looks up.
            wchar_t nb[16]={0}; GetWindowTextW(t->eNid,nb,16);
            std::wstring nid=trim(nb);
            bool insured = SendMessageW(t->chkIns,BM_GETCHECK,0,0)==BST_CHECKED;
            if(insured)                doInquiry(t,h,false);  // show invalid-id error
            else if(nid.size()==10)    doInquiry(t,h,true);   // quiet auto-fill
        }
        else if(id==ID_F_INQUIRY){
            // manual «استعلام بیمه» button — only meaningful when insured
            if(SendMessageW(t->chkIns,BM_GETCHECK,0,0)==BST_CHECKED)
                doInquiry(t,h,false);
            else {
                t->lastMsg=L"برای استعلام، گزینه «دارای بیمه» را فعال کنید.";
                t->msgCol=g_theme.warn; InvalidateRect(h,NULL,FALSE);
            }
        }
        else if(id==ID_F_HASINS && code==BN_CLICKED){
            // toggling insured state: when unchecked, reset to free/«آزاد» so the
            // user picks insurance manually; recalc + redraw either way.
            bool insured = SendMessageW(t->chkIns,BM_GETCHECK,0,0)==BST_CHECKED;
            EnableWindow(t->bInquiry, insured);
            if(!insured){
                SendMessageW(t->cIns,CB_SETCURSEL,0,0);   // index 0 = آزاد/بدون بیمه
                t->lastMsg=L"حالت بدون بیمه — بیمه را به‌صورت دستی انتخاب کنید.";
                t->msgCol=g_theme.textDim;
            } else {
                t->lastMsg=L"حالت دارای بیمه — کد ملی را وارد و Enter بزنید.";
                t->msgCol=g_theme.accent;
            }
            recalc(t); InvalidateRect(h,NULL,FALSE);
        }
        else if(id==ID_F_SUBMIT){
            ReceptionRecord r; collect(t,r);
            // v1.4.0 validation: REQUIRED = first/last name, national id, mobile,
            // birth date.  OPTIONAL (may be empty) = landline, address, discount.
            // v1.9.0: compute the empty-field mask so every missing REQUIRED
            // field gets a thin red hairline (indices match inputs[] order).
            // v1.25.0 admission rule: ONLY the FIRST FIVE identity fields are
            // required (cannot be empty): نام، نام‌خانوادگی، کد‌ملی، نام‌پدر،
            // تاریخ‌تولد. Mobile/landline/address are optional.
            int mask=0;
            if(r.firstName.empty())  mask|=(1<<0);
            if(r.lastName.empty())   mask|=(1<<1);
            if(r.nationalId.empty()) mask|=(1<<2);
            if(r.fatherName.empty()) mask|=(1<<3);
            if(r.birthDate.empty())  mask|=(1<<4);
            if(r.total<=0)           mask|=(1<<13);
            t->invalidMask=mask;
            if(r.firstName.empty()||r.lastName.empty()){
                t->lastMsg=L"نام و نام خانوادگی بیمار الزامی است.";
                t->msgCol=g_theme.danger;
            } else if(r.nationalId.empty()){
                t->lastMsg=L"کد ملی بیمار الزامی است.";
                t->msgCol=g_theme.danger;
            } else if(r.fatherName.empty()){
                t->lastMsg=L"نام پدر بیمار الزامی است.";
                t->msgCol=g_theme.danger;
            } else if(r.birthDate.empty()){
                t->lastMsg=L"تاریخ تولد بیمار الزامی است.";
                t->msgCol=g_theme.danger;
            } else if(r.total<=0){
                t->lastMsg=L"مبلغ ویزیت/خدمت را وارد کنید.";
                t->msgCol=g_theme.danger;
            } else {
                t->invalidMask=0;   // all required fields present
                int q=saveReception(r);
                // v1.7.0: remember this REAL (operator-confirmed) identity so the
                // same national code recalls the same patient next time — never
                // fabricated, only what was actually entered/verified here.
                {
                    std::vector<int> ins;
                    if(r.insIdx>0 && r.insIdx<N_INSURANCES) ins.push_back(r.insIdx);
                    // v1.40.0: forward the freshly entered landline/address and
                    // supplementary-insurance index so the store recalls them too.
                    rememberPatient(r.nationalId,r.firstName,r.lastName,
                        r.fatherName,r.gender,r.birthDate,r.mobile,
                        r.landline,r.address,ins,r.suppIdx);
                }
                wchar_t mb[160];
                swprintf(mb,160,L"پذیرش با شماره پذیرش %d ثبت شد — %s %s",
                    q, r.firstName.c_str(), r.lastName.c_str());
                t->lastMsg=toFaDigits(mb); t->msgCol=g_theme.success;
                // v1.4.0: prefer the user-designed layout for this section; fall
                // back to the classic GDI receipt if no design exists.
                auto doPrint=[&](const ReceptionRecord& rec){
                    // §1.19.0 print routing, in priority order:
                    //  1) new print_designer design bound to the operator's SECTION
                    //  2) legacy per-section designed receipt (section index 0)
                    //  3) classic GDI receipt
                    int sid=recResolveSectionId();
                    bool done=false;
                    // v1.65.0: try the services-capable print-design even when
                    // sid==0 (resolver maps 0 → first builtin; store now seeds
                    // lazily inside printPrintDesign).
                    done=printPrintDesign(rec,sid,h);
                    if(!done) done=printDesignedReceipt(rec,0,h);
                    if(!done) printReceipt(rec,2,h);
                    kickCashDrawer();   // pulse drawer if enabled in printer settings
                };
                if(getSetting(L"auto_print",L"0")==L"1"){
                    doPrint(r);
                } else if(MessageBoxW(h,L"پذیرش ثبت شد. قبض چاپ شود؟",
                    L"ثبت موفق",MB_YESNO|MB_ICONQUESTION)==IDYES){
                    doPrint(r);
                }
                // v1.4.0 (§6): do NOT clear the form after printing — the operator
                // often re-prints or tweaks. Fields stay populated; use the reset
                // button (or Ctrl+R) to start a fresh reception explicitly.
            }
            InvalidateRect(h,NULL,FALSE);
        }
        else if(id==ID_F_PRT_INS){
            ReceptionRecord r;
            if(loadLastReceipt(r)) printReceipt(r,0,h);
            else MessageBoxW(h,L"ابتدا یک پذیرش ثبت کنید.",L"رسید بیمه",MB_OK|MB_ICONINFORMATION);
        }
        else if(id==ID_F_PRT_RX){
            ReceptionRecord r;
            if(loadLastReceipt(r)) printReceipt(r,1,h);
            else MessageBoxW(h,L"ابتدا یک پذیرش ثبت کنید.",L"چاپ نسخه",MB_OK|MB_ICONINFORMATION);
        }
        else if(id==ID_F_PRT_LAST) printLastReceipt(h);
        else if(id==ID_F_RESET){                       // پاک کردن
            resetForm(t);
            t->lastMsg=L"فرم پاک شد."; t->msgCol=g_theme.textDim;
            InvalidateRect(h,NULL,FALSE);
        }
        else if(id==ID_F_NEW){                          // پذیرش بیمار
            resetForm(t);
            t->lastMsg=L"پذیرش بیمار — اطلاعات بیمار را وارد کنید.";
            t->msgCol=g_theme.accent;
            SetFocus(t->eFirst);
            InvalidateRect(h,NULL,FALSE);
        }
        else if(id==ID_F_SVC_ADD){
            // v1.25.0: «افزودن خدمت» toggles the INLINE add-service panel — it
            // never opens a separate window. When opening, focus the code box so
            // the operator can immediately type a service code + Enter.
            t->svcPanelOpen = !t->svcPanelOpen;
            tabPageLayout(h,t);
            if(t->svcPanelOpen){
                t->lastMsg=L"کد خدمت را وارد و Enter بزنید (مثلاً ۱۱۱ برای تزریقات).";
                t->msgCol=g_theme.accent;
                SetFocus(t->eSvcCode);
            } else {
                t->lastMsg.clear();
            }
            InvalidateRect(h,NULL,FALSE);
        }
        else if(id==ID_F_SVC_CONFIRM){
            // add the panel row to the bottom services list
            if(svcCommitPanel(t)){
                tabPageLayout(h,t);
                t->lastMsg=L"خدمت اضافه شد.";
                t->msgCol=g_theme.success;
                SetFocus(t->eSvcCode);
            } else {
                t->lastMsg=L"کد یا نام خدمت را وارد کنید.";
                t->msgCol=g_theme.danger;
            }
            InvalidateRect(h,NULL,FALSE);
        }
        else if(id==ID_F_SVC_FREECHK && code==BN_CLICKED){
            // «نرخ آزاد» reveals/enables the free-amount box
            bool on=SendMessageW(t->chkSvcFree,BM_GETCHECK,0,0)==BST_CHECKED;
            EnableWindow(t->eSvcFreeAmt,on);
            if(on) SetFocus(t->eSvcFreeAmt);
            else   SetWindowTextW(t->eSvcFreeAmt,L"");
            InvalidateRect(h,NULL,FALSE);
        }
        else if(id==ID_F_SVC_SEARCH){
            // filtering the services list — repaint (filter applied in paint)
            InvalidateRect(h,NULL,FALSE);
        }
        else if(id==ID_F_UP_REFRESH && code==BN_CLICKED){
            // v1.26.0: reload the bottom-left table from disk
            upMarkDirty();
            t->lastMsg=L"فهرست به‌روزرسانی شد."; t->msgCol=g_theme.textDim;
            InvalidateRect(h,NULL,FALSE);
        }
        else if(id==ID_F_UP_HOURS && code==CBN_SELCHANGE){
            // «مراجعات اخیر (ساعت)» window changed — repaint (filter in paint)
            InvalidateRect(h,NULL,FALSE);
        }
        else if(id==ID_F_UP_DATE && code==EN_CHANGE){
            // «تاریخ تا» filter — repaint (filter applied in paint)
            InvalidateRect(h,NULL,FALSE);
        }
        else if((id==ID_F_DOC2_CODE) && code==EN_KILLFOCUS){
            if(applyDocByCode(t->eDoc2Code,t->cDoc2List)) InvalidateRect(h,NULL,FALSE);
        }
        else if((id==ID_F_PERF_CODE) && code==EN_KILLFOCUS){
            if(applyPerfByCode(t->ePerfCode,t->cPerfList)) InvalidateRect(h,NULL,FALSE);
        }
        else if(id==ID_F_DOC2_LIST && code==CBN_SELCHANGE){
            mirrorDocCodeFromList(t->eDoc2Code,t->cDoc2List);
        }
        else if(id==ID_F_PERF_LIST && code==CBN_SELCHANGE){
            mirrorPerfCodeFromList(t->ePerfCode,t->cPerfList);
        }
        else if(id==ID_F_SUPP_PCT2 && code==EN_CHANGE){
            recalc(t); InvalidateRect(h,NULL,FALSE);
        }
        else if(id==ID_F_NOPAY_CHK && code==BN_CLICKED){
            bool on=SendMessageW(t->chkNoPay,BM_GETCHECK,0,0)==BST_CHECKED;
            t->lastMsg = on ? L"حالت «عدم پرداخت» — از «افزودن به صندوق نرفته‌ها» استفاده کنید."
                            : L"";
            t->msgCol=g_theme.textDim;
            InvalidateRect(h,NULL,FALSE);
        }
        else if(id==ID_F_ADD_UNPAID || id==ID_F_ADD_QUEUE){
            // v1.25.0: two payment-deferred paths.
            //  • «افزودن به صندوق نرفته‌ها» — patient will NOT pay now; the record
            //    is saved with paid=0 and tagged so the صندوق نرفته‌ها tab lists it.
            //  • «افزودن به صف پذیرش»       — e.g. internet outage; queued for later
            //    processing (for those who DID pay). Saved and tagged as queued.
            ReceptionRecord r; collect(t,r);
            // minimal validation: the FIRST FIVE identity fields are required.
            int mask=0;
            if(r.firstName.empty())  mask|=(1<<0);
            if(r.lastName.empty())   mask|=(1<<1);
            if(r.nationalId.empty()) mask|=(1<<2);
            if(r.fatherName.empty()) mask|=(1<<3);
            if(r.birthDate.empty())  mask|=(1<<4);
            t->invalidMask=mask;
            if(mask){
                t->lastMsg=L"نام، نام خانوادگی، کد ملی، نام پدر و تاریخ تولد الزامی است.";
                t->msgCol=g_theme.danger;
                InvalidateRect(h,NULL,FALSE);
            } else {
                bool unpaid = (id==ID_F_ADD_UNPAID);
                if(unpaid){ r.paid=0; }              // صندوق نرفته‌ها → no payment
                int q=saveReception(r);
                // persist a lightweight side-tag so the relevant tab can list it
                // v1.26.0 format: q|tag|name|paid|date|time|epoch — the extra
                // fields feed the «تاریخ / زمان / دقیقه پیش» table columns.
                {
                    std::wstring tag = unpaid ? L"unpaid" : L"queue";
                    wchar_t line[320];
                    swprintf(line,320,L"%d|%s|%s %s|%lld|%s|%s|%lld\n",
                        q, tag.c_str(), r.firstName.c_str(), r.lastName.c_str(),
                        (long long)r.paid,
                        FormatJalaliPersian(0).c_str(), upNowHHMM().c_str(),
                        (long long)time(NULL));
                    writeFileUtf8(dataDir()+ (unpaid?L"\\unpaid_box.dat":L"\\recept_queue.dat"),
                        line, true);
                }
                upMarkDirty();
                wchar_t mb[200];
                if(unpaid)
                    swprintf(mb,200,L"به «صندوق نرفته‌ها» افزوده شد — نوبت %d (بدون پرداخت).",q);
                else
                    swprintf(mb,200,L"به «صف پذیرش» افزوده شد — نوبت %d.",q);
                t->lastMsg=toFaDigits(mb); t->msgCol=g_theme.success;
                InvalidateRect(h,NULL,FALSE);
            }
        }
        else if(id==ID_F_CLOSE) closeTab(t);
        // ---- right info-panel handlers ----
        else if(id==ID_F_BOOKNO_CHK && code==BN_CLICKED){
            bool on=SendMessageW(t->chkBookNo,BM_GETCHECK,0,0)==BST_CHECKED;
            EnableWindow(t->eBookNo,on);
            if(on) SetFocus(t->eBookNo);
        }
        else if(id==ID_F_VALID_AUTO && code==BN_CLICKED){
            bool on=SendMessageW(t->chkValidAuto,BM_GETCHECK,0,0)==BST_CHECKED;
            EnableWindow(t->eValid,!on);
            if(on) SetWindowTextW(t->eValid,FormatJalaliPersian(0).c_str());
        }
        else if(id==ID_F_RXDATE_AUTO && code==BN_CLICKED){
            bool on=SendMessageW(t->chkRxAuto,BM_GETCHECK,0,0)==BST_CHECKED;
            EnableWindow(t->eRxDate,!on);
            if(on) SetWindowTextW(t->eRxDate,FormatJalaliPersian(0).c_str());
        }
        else if(id==ID_F_ARCHIVE_GO || id==ID_F_FILE_GO){
            // archive/file-number inquiry: reuse the national-id inquiry path so a
            // found record auto-fills the patient block (placeholder mapping).
            doInquiry(t,h,true);
            t->lastMsg=L"استعلام انجام شد."; t->msgCol=g_theme.accent;
            InvalidateRect(h,NULL,FALSE);
        }
        else if(id==ID_F_SUPP_GO){
            int si=(int)SendMessageW(t->cSuppPanel,CB_GETCURSEL,0,0);
            if(si>=0 && si<N_SUPP){
                SendMessageW(t->cSupp,CB_SETCURSEL,si,0);
                int sp=Supp_Percent(si); if(sp<0) sp=SUPP_INSURANCES[si].pct;
                wchar_t pb[16]; swprintf(pb,16,L"%d",sp);
                SetWindowTextW(t->eSuppPct,toFaDigits(pb).c_str());
                recalc(t); InvalidateRect(h,NULL,FALSE);
            }
        }
        else if(id==ID_F_DOCSEARCH){
            // جستجوی پزشک معالج: ابتدا با نام، سپس انتخاب از فهرست پزشکان
            wchar_t nb[128]; GetWindowTextW(t->eDocName,nb,128);
            std::wstring q=trim(nb);
            auto docs=loadDoctors();
            HMENU mnu=CreatePopupMenu();
            std::vector<int> map;
            for(int i=0;i<(int)docs.size();i++){
                if(q.empty() || docs[i].name.find(q)!=std::wstring::npos
                             || docs[i].specialty.find(q)!=std::wstring::npos){
                    std::wstring s=docs[i].name+L" — "+docs[i].specialty;
                    AppendMenuW(mnu,MF_STRING,(UINT)(map.size()+1),s.c_str());
                    map.push_back(i);
                }
            }
            if(map.empty()){
                t->lastMsg=L"پزشکی با این مشخصات یافت نشد.";
                t->msgCol=g_theme.danger; InvalidateRect(h,NULL,FALSE);
                DestroyMenu(mnu);
            } else {
                RECT br; GetWindowRect(t->bDocSearch,&br);
                int cmd=TrackPopupMenu(mnu,TPM_RETURNCMD|TPM_RIGHTBUTTON,
                    br.left,br.bottom,0,h,NULL);
                DestroyMenu(mnu);
                if(cmd>0 && cmd<=(int)map.size()){
                    const DoctorDef& d=docs[map[cmd-1]];
                    SetWindowTextW(t->eDocName,d.name.c_str());
                    // generate a stable 5-digit نظام پزشکی from the name hash
                    unsigned hsh=0; for(wchar_t c:d.name) hsh=hsh*131+(unsigned)c;
                    wchar_t code[16]; swprintf(code,16,L"%05u",10000+(hsh%89999));
                    SetWindowTextW(t->eDocCode,toFaDigits(code).c_str());
                    t->lastMsg=L"پزشک معالج انتخاب شد: "+d.name;
                    t->msgCol=g_theme.success; InvalidateRect(h,NULL,FALSE);
                }
            }
        }
        return 0; }
    case WM_KEYDOWN:
        if(w==VK_F8){ printLastReceipt(h); return 0; }
        // v1.4.0 (§6): Ctrl+R clears the reception form for a fresh patient.
        if(w=='R' && (GetKeyState(VK_CONTROL)&0x8000) &&
           t && !t->web && t->kind==TK_RECEPTION){
            resetForm(t);
            t->lastMsg=L"فرم پاک شد."; t->msgCol=g_theme.textDim;
            InvalidateRect(h,NULL,FALSE);
            return 0;
        }
        // v1.7.0: Esc returns from the message details view to the list
        if(w==VK_ESCAPE && t && t->kind==TK_PORTAL && t->cartDetail){
            t->cartDetail=false; t->cartHotBtn=0;
            InvalidateRect(h,NULL,FALSE);
            return 0;
        }
        break;
    case WM_SETCURSOR: {
        // hand cursor while hovering a clickable message tile or detail button
        if(t && t->kind==TK_PORTAL && LOWORD(l)==HTCLIENT){
            POINT pt; GetCursorPos(&pt); ScreenToClient(h,&pt);
            bool hand = t->cartDetail ? (cartBtnHit(pt)!=CART_BTN_NONE)
                                      : (cartHit(pt)>=0);
            if(hand){ SetCursor(LoadCursor(NULL,IDC_HAND)); return TRUE; }
        }
        break; }
    case WM_MOUSEMOVE: {
        // detail-view: track which action button is hovered (for highlight)
        if(t && t->kind==TK_PORTAL && t->cartDetail){
            POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
            int hb=cartBtnHit(pt);
            if(hb!=t->cartHotBtn){ t->cartHotBtn=hb; InvalidateRect(h,NULL,FALSE); }
            TRACKMOUSEEVENT te={sizeof(te),TME_LEAVE,h,0}; TrackMouseEvent(&te);
        } else if(t && t->kind==TK_RECEPTION){
            // v1.69.0: hover the cash-desk / queue rows for a highlight wash.
            POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
            int hit=-1;
            for(int i=0;i<(int)s_upHits.size();i++)
                if(PtInRect(&s_upHits[i].row,pt)){ hit=s_upHits[i].idx; break; }
            if(hit!=t->upHotIdx){ t->upHotIdx=hit; InvalidateRect(h,NULL,FALSE); }
            TRACKMOUSEEVENT te={sizeof(te),TME_LEAVE,h,0}; TrackMouseEvent(&te);
        }
        break; }
    case WM_MOUSELEAVE:
        if(t && t->kind==TK_PORTAL && t->cartHotBtn){
            t->cartHotBtn=0; InvalidateRect(h,NULL,FALSE);
        }
        if(t && t->kind==TK_RECEPTION && t->upHotIdx!=-1){
            t->upHotIdx=-1; InvalidateRect(h,NULL,FALSE);
        }
        break;
    case WM_RBUTTONDOWN: {
        // v1.7.0: RIGHT-CLICK on a tile shows ONLY the pin/unpin option.
        if(t && t->kind==TK_PORTAL && !t->cartDetail){
            POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
            int disp=cartHit(pt);
            if(disp>=0 && disp<(int)s_cartMsgs.size() && disp<(int)s_cartNF.size()){
                KMsg& mm=s_cartMsgs[disp];
                int nf=s_cartNF[disp];
                HMENU mnu=CreatePopupMenu();
                AppendMenuW(mnu,MF_STRING,1,
                    mm.pinned?L"برداشتن سنجاق":L"سنجاق کردن");
                AppendMenuW(mnu,MF_SEPARATOR,0,NULL);
                // v1.8.0: «ارسال به پیام‌های ذخیره‌شده» — disabled by default
                // (greyed out) unless the feature is enabled in settings.
                AppendMenuW(mnu,MF_STRING|(savedMsgsEnabled()?MF_ENABLED:MF_GRAYED),
                    2,L"ارسال به پیام‌های ذخیره‌شده");
                POINT sp=pt; ClientToScreen(h,&sp);
                int cmd=TrackPopupMenu(mnu,TPM_RETURNCMD|TPM_RIGHTBUTTON,
                    sp.x,sp.y,0,h,NULL);
                DestroyMenu(mnu);
                if(cmd==1){ pinMessage(g_session.user.username,nf,!mm.pinned);
                            InvalidateRect(h,NULL,FALSE); }
                else if(cmd==2 && savedMsgsEnabled()){
                    // archive a copy locally (text + any attachment preserved)
                    pushSavedMsg(mm.from.empty()?L"مدیریت":mm.from,
                                 g_session.user.fullname.empty()?g_session.user.username
                                                                :g_session.user.fullname,
                                 mm.text, mm.type, L"");
                    t->lastMsg=L"پیام به «پیام‌های ذخیره‌شده» منتقل شد.";
                    t->msgCol=g_theme.success;
                    InvalidateRect(h,NULL,FALSE);
                }
            }
            return 0;
        }
        break; }
    case WM_LBUTTONDOWN: {
        // v1.25.0: reception form — service-row delete + P/S preview squares.
        if(t && !t->web && t->kind==TK_RECEPTION){
            POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
            // 1) trash button on a service row → remove it
            for(int i=0;i<(int)s_svcHits.size();i++){
                if(PtInRect(&s_svcHits[i].del,pt)){
                    if(i<(int)t->services.size()){
                        t->services.erase(t->services.begin()+i);
                        recalc(t); tabPageLayout(h,t);
                        t->lastMsg=L"خدمت حذف شد."; t->msgCol=g_theme.textDim;
                        InvalidateRect(h,NULL,FALSE);
                    }
                    return 0;
                }
            }
            // 2) P / S preview squares → focus the matching amount field
            if(PtInRect(&s_psPRect,pt)){
                SetFocus(t->eApptP); SendMessageW(t->eApptP,EM_SETSEL,0,-1);
                return 0;
            }
            if(PtInRect(&s_psSRect,pt)){
                SetFocus(t->eApptS); SendMessageW(t->eApptS,EM_SETSEL,0,-1);
                return 0;
            }
            // 2.5) v1.31.0 — collapse/expand toggles for the two bottom lists.
            if(PtInRect(&s_svcToggleR,pt)){
                t->svcCollapsed=!t->svcCollapsed;
                tabPageLayout(h,t); InvalidateRect(h,NULL,FALSE);
                return 0;
            }
            if(PtInRect(&s_upToggleR,pt)){
                t->upCollapsed=!t->upCollapsed;
                tabPageLayout(h,t); InvalidateRect(h,NULL,FALSE);
                return 0;
            }
            // 3) v1.26.0 — bottom-left panel: flat tabs صندوق نرفته‌ها|صف پذیرش
            for(int k=0;k<2;k++){
                if(PtInRect(&s_upTabR[k],pt)){
                    if(t->bottomTab!=k){
                        t->bottomTab=k; upMarkDirty();
                        t->upSelRaw.clear(); t->upHotIdx=-1;
                        InvalidateRect(h,NULL,FALSE);
                    }
                    return 0;
                }
            }
            // 4) blue «+ افزودن به …» footer link → run the matching action
            if(PtInRect(&s_upAddRect,pt)){
                SendMessageW(h,WM_COMMAND,
                    MAKEWPARAM(t->bottomTab==0?ID_F_ADD_UNPAID:ID_F_ADD_QUEUE,
                               BN_CLICKED),0);
                return 0;
            }
            // 5) v1.69.0 — row operations on the cash-desk / queue table. The hit
            //    map carries per-row rects: up/down (selected row only), edit, del,
            //    and the row body (click = select). Each acts on the row it sits on.
            for(int i=0;i<(int)s_upHits.size();i++){
                int idx=s_upHits[i].idx;
                if(idx<0 || idx>=(int)s_upRows.size()) continue;
                const UpRow& rrow=s_upRows[idx];
                // move up / down (queue reorder) — swap raw lines; selection follows
                if(PtInRect(&s_upHits[i].up,pt) || PtInRect(&s_upHits[i].down,pt)){
                    bool mvUp=PtInRect(&s_upHits[i].up,pt);
                    int tgt=i+(mvUp?-1:1);          // s_upHits is in display order
                    if(tgt>=0 && tgt<(int)s_upHits.size()){
                        upSwapRaw(t->bottomTab,rrow.raw,
                                  s_upRows[s_upHits[tgt].idx].raw);
                        t->lastMsg= mvUp?L"نوبت به بالا منتقل شد."
                                        :L"نوبت به پایین منتقل شد.";
                        t->msgCol=g_theme.textDim;
                    }
                    InvalidateRect(h,NULL,FALSE);
                    return 0;
                }
                // edit patient name (modal popup) -> rebuild + replace the line
                if(PtInRect(&s_upHits[i].edit,pt)){
                    std::wstring nm=rrow.name;
                    if(upEditName(h,L"ویرایش نام بیمار",nm) && !trim(nm).empty()){
                        upReplaceLine(t->bottomTab,rrow.raw,upRebuildLine(rrow,trim(nm)));
                        t->upSelRaw.clear();
                        t->lastMsg=L"نام بیمار ویرایش شد."; t->msgCol=g_theme.textDim;
                        InvalidateRect(h,NULL,FALSE);
                    }
                    return 0;
                }
                // delete (confirm) -> remove the line
                if(PtInRect(&s_upHits[i].del,pt)){
                    std::wstring q2=L"«"+rrow.name+L"» از فهرست حذف شود؟";
                    if(MessageBoxW(h,q2.c_str(),
                        t->bottomTab==0?L"حذف از صندوق نرفته‌ها":L"حذف از صف پذیرش",
                        MB_YESNO|MB_ICONQUESTION)==IDYES){
                        upDeleteRaw(t->bottomTab,rrow.raw);
                        t->upSelRaw.clear();
                        t->lastMsg=L"ردیف حذف شد."; t->msgCol=g_theme.textDim;
                        InvalidateRect(h,NULL,FALSE);
                    }
                    return 0;
                }
            }
            // 6) click on a row body (not an icon) -> select that row
            for(int i=0;i<(int)s_upHits.size();i++){
                if(PtInRect(&s_upHits[i].row,pt)){
                    t->upSelRaw=s_upRows[s_upHits[i].idx].raw;
                    InvalidateRect(h,NULL,FALSE);
                    return 0;
                }
            }
        }
        if(t && t->kind==TK_PORTAL){
            POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
            std::wstring me=g_session.user.username;
            if(t->cartDetail){
                // ----- detail view: action buttons -----
                int bid=cartBtnHit(pt);
                if(bid==CART_BTN_BACK){
                    t->cartDetail=false; t->cartHotBtn=0;
                    InvalidateRect(h,NULL,FALSE);
                } else if(bid==CART_BTN_MARK || bid==CART_BTN_TOGGLEREAD){
                    // «خواندن» / «علامت خوانده‌شده» — mark this message seen
                    if(t->cartSelNF>=0) seenOneMessage(me,t->cartSelNF);
                    InvalidateRect(h,NULL,FALSE);
                } else if(bid==CART_BTN_DELETE){
                    if(MessageBoxW(h,L"این پیام حذف شود؟",L"حذف پیام",
                        MB_YESNO|MB_ICONQUESTION)==IDYES){
                        if(t->cartSelNF>=0) deleteOneMessage(me,t->cartSelNF);
                        t->cartDetail=false; t->cartHotBtn=0;
                        InvalidateRect(h,NULL,FALSE);
                    }
                } else if(bid==CART_BTN_SAVE){
                    // archive THIS message into the saved-messages store so it is
                    // kept even after it is deleted from the live inbox.
                    int sel=-1;
                    for(int i=0;i<(int)s_cartNF.size();i++)
                        if(s_cartNF[i]==t->cartSelNF){ sel=i; break; }
                    if(sel<0 && t->cartSelDisp>=0 && t->cartSelDisp<(int)s_cartMsgs.size())
                        sel=t->cartSelDisp;
                    if(savedMsgsEnabled() && sel>=0 && sel<(int)s_cartMsgs.size()){
                        KMsg& mm=s_cartMsgs[sel];
                        std::wstring from=mm.from.empty()?std::wstring(L"مدیریت درمانگاه"):mm.from;
                        pushSavedMsg(from, mm.to, mm.text, mm.type, L"");
                        MessageBoxW(h,L"پیام در «پیام‌های ذخیره‌شده» بایگانی شد.",
                            L"ذخیره شد",MB_OK|MB_ICONINFORMATION);
                    } else {
                        MessageBoxW(h,L"بایگانی پیام‌ها در تنظیمات غیرفعال است.",
                            L"غیرفعال",MB_OK|MB_ICONWARNING);
                    }
                    InvalidateRect(h,NULL,FALSE);
                } else if(bid==CART_BTN_REPLY){
                    // v1.77.0: پاسخ — quick text reply to the sender (management).
                    // Delivered as a new inbox message addressed to the original
                    // sender; a copy is also kept in «پیام‌های ذخیره‌شده».
                    int sel=cartSelIndex(t);
                    if(sel>=0){
                        KMsg& mm=s_cartMsgs[sel];
                        std::wstring to=mm.from.empty()?std::wstring(L"مدیریت درمانگاه"):mm.from;
                        std::wstring reply;
                        if(upEditName(h,L"پاسخ به مدیریت",reply)){
                            std::wstring r=trim(reply);
                            if(!r.empty()){
                                std::wstring me=g_session.user.fullname.empty()?
                                    g_session.user.username:g_session.user.fullname;
                                std::wstring body=L"\u21A9 \u067E\u0627\u0633\u062E: "+r; // ↩ پاسخ:
                                pushMessage(me,to,body);
                                if(savedMsgsEnabled())
                                    pushSavedMsg(me,to,body,mm.type,L"");
                                t->lastMsg=L"\u067E\u0627\u0633\u062E \u0634\u0645\u0627 \u0628\u0647 \u0645\u062F\u06CC\u0631\u06CC\u062A \u0627\u0631\u0633\u0627\u0644 \u0634\u062F."; // پاسخ شما به مدیریت ارسال شد.
                                t->msgCol=g_theme.success;
                            }
                        }
                    }
                    InvalidateRect(h,NULL,FALSE);
                } else if(bid==CART_BTN_UPLOAD){
                    // v1.77.0: پیوست فایل — pick a file and send it up to management
                    // as a message carrying the attachment (a local copy is kept so
                    // the stored path stays valid), mirroring the management sender.
                    int sel=cartSelIndex(t);
                    if(sel>=0){
                        wchar_t buf[MAX_PATH]; buf[0]=0;
                        OPENFILENAMEW ofn={sizeof(ofn)}; ofn.hwndOwner=h;
                        ofn.lpstrFile=buf; ofn.nMaxFile=MAX_PATH;
                        ofn.lpstrFilter=L"\u0647\u0645\u0647 \u0641\u0627\u06CC\u0644\u200C\u0647\u0627\0*.*\0"
                                       L"\u062A\u0635\u0627\u0648\u06CC\u0631\0*.png;*.jpg;*.jpeg;*.bmp;*.gif\0"
                                       L"PDF\0*.pdf\0\0";
                        ofn.nFilterIndex=1;
                        ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_EXPLORER;
                        if(GetOpenFileNameW(&ofn) && buf[0]){
                            std::wstring file=buf;
                            std::wstring stored=copyAttachmentLocal(file);
                            std::wstring base=file;
                            size_t slash=base.find_last_of(L"\\/");
                            base=(slash==std::wstring::npos)?base:base.substr(slash+1);
                            std::wstring me=g_session.user.fullname.empty()?
                                g_session.user.username:g_session.user.fullname;
                            std::wstring body=L"\U0001F4CE \u067E\u06CC\u0648\u0633\u062A: "+base+
                                L"\n["+stored+L"]"; // 📎 پیوست:
                            pushMessage(me,L"\u0645\u062F\u06CC\u0631\u06CC\u062A \u062F\u0631\u0645\u0627\u0646\u06AF\u0627\u0647",body);
                            if(savedMsgsEnabled())
                                pushSavedMsg(me,L"\u0645\u062F\u06CC\u0631\u06CC\u062A \u062F\u0631\u0645\u0627\u0646\u06AF\u0627\u0647",
                                             body,KMSG_NORMAL,stored);
                            t->lastMsg=L"\u0641\u0627\u06CC\u0644 \u0628\u0647 \u0645\u062F\u06CC\u0631\u06CC\u062A \u0627\u0631\u0633\u0627\u0644 \u0634\u062F."; // فایل به مدیریت ارسال شد.
                            t->msgCol=g_theme.success;
                        }
                    }
                    InvalidateRect(h,NULL,FALSE);
                } else if(bid==CART_BTN_PRINT){
                    // v1.77.0: چاپ پیام — render this message to a temp RTL .htm
                    // document and hand it to the shell "print" verb so the user
                    // can print it or save it to PDF (save/print in one action).
                    int sel=cartSelIndex(t);
                    if(sel>=0){
                        KMsg& mm=s_cartMsgs[sel];
                        std::wstring date,time; splitMsgTime(mm.time,date,time);
                        std::wstring sender=mm.from.empty()?std::wstring(L"\u0645\u062F\u06CC\u0631\u06CC\u062A \u062F\u0631\u0645\u0627\u0646\u06AF\u0627\u0647"):mm.from;
                        std::wstring recip=(mm.to==L"*")?std::wstring(L"\u0647\u0645\u0647 \u06A9\u0627\u0631\u0628\u0631\u0627\u0646"):mm.to;
                        std::wstring html=L"<!doctype html><html dir='rtl' lang='fa'><head>"
                            L"<meta charset='utf-8'><title>\u067E\u06CC\u0627\u0645 \u0645\u062F\u06CC\u0631\u06CC\u062A</title>"
                            L"<style>body{font-family:Tahoma,Arial,sans-serif;direction:rtl;"
                            L"padding:34px;color:#233042;line-height:1.75;max-width:680px;margin:0 auto}"
                            L"h2{color:#1f5fd6;border-bottom:2px solid #e2e8f0;padding-bottom:8px;margin-top:0}"
                            L".m{margin:6px 0;font-size:15px}.k{color:#64748b;font-weight:bold}"
                            L".b{margin-top:18px;padding:16px 18px;background:#f7f9fc;"
                            L"border:1px solid #e2e8f0;border-radius:10px;font-size:16px;white-space:pre-wrap}</style></head><body>"
                            L"<h2>\u067E\u06CC\u0627\u0645 \u0645\u062F\u06CC\u0631\u06CC\u062A \u062F\u0631\u0645\u0627\u0646\u06AF\u0627\u0647</h2>"
                            L"<div class='m'><span class='k'>\u0641\u0631\u0633\u062A\u0646\u062F\u0647: </span>"+cartHtmlEsc(sender)+L"</div>"
                            L"<div class='m'><span class='k'>\u06AF\u06CC\u0631\u0646\u062F\u0647: </span>"+cartHtmlEsc(recip)+L"</div>"
                            L"<div class='m'><span class='k'>\u0627\u0648\u0644\u0648\u06CC\u062A: </span>"+std::wstring(sevLabel(mm.type))+L"</div>"
                            L"<div class='m'><span class='k'>\u062A\u0627\u0631\u06CC\u062E: </span>"+toFaDigits(date)+
                            L" &nbsp; <span class='k'>\u0633\u0627\u0639\u062A: </span>"+toFaDigits(time)+L"</div>"
                            L"<div class='b'>"+cartHtmlEsc(mm.text)+L"</div>"
                            L"</body></html>";
                        // wstring → UTF-8 (no BOM; meta charset declares utf-8)
                        std::string u8;
                        int n=WideCharToMultiByte(CP_UTF8,0,html.c_str(),(int)html.size(),NULL,0,NULL,NULL);
                        if(n>0){ u8.resize(n); WideCharToMultiByte(CP_UTF8,0,html.c_str(),(int)html.size(),&u8[0],n,NULL,NULL); }
                        wchar_t tmpDir[MAX_PATH]; GetTempPathW(MAX_PATH,tmpDir);
                        std::wstring path=std::wstring(tmpDir)+L"az_cart_msg.htm";
                        FILE* fp=_wfopen(path.c_str(),L"wb");
                        if(fp && !u8.empty()){
                            fwrite(u8.data(),1,u8.size(),fp); fclose(fp);
                            ShellExecuteW(h,L"print",path.c_str(),NULL,NULL,SW_SHOWNORMAL);
                            t->lastMsg=L"\u067E\u06CC\u0627\u0645 \u0628\u0631\u0627\u06CC \u0686\u0627\u067E \u0627\u0631\u0633\u0627\u0644 \u0634\u062F."; // پیام برای چاپ ارسال شد.
                            t->msgCol=g_theme.accent;
                        } else {
                            if(fp) fclose(fp);
                            t->lastMsg=L"\u0686\u0627\u067E \u067E\u06CC\u0627\u0645 \u0646\u0627\u0645\u0648\u0641\u0642 \u0628\u0648\u062F."; // چاپ پیام ناموفق بود.
                            t->msgCol=g_theme.danger;
                        }
                    }
                    InvalidateRect(h,NULL,FALSE);
                }
                return 0;
            }
            // ----- §D: archive toggle icon (top-left of the header) -----
            if(PtInRect(&s_cartArchiveRect,pt)){
                if(!savedMsgsEnabled()){
                    // feature is off — offer to enable it so the entry is never
                    // a dead end (the brief: must be reachable + functional).
                    int r=MessageBoxW(h,
                        L"\u0642\u0627\u0628\u0644\u06cc\u062a \u00ab\u067e\u06cc\u0627\u0645\u200c\u0647\u0627\u06cc \u0630\u062e\u06cc\u0631\u0647\u200c\u0634\u062f\u0647\u00bb \u063a\u06cc\u0631\u0641\u0639\u0627\u0644 \u0627\u0633\u062a. \u0641\u0639\u0627\u0644 \u0634\u0648\u062f\u061f",
                        L"\u067e\u06cc\u0627\u0645\u200c\u0647\u0627\u06cc \u0630\u062e\u06cc\u0631\u0647\u200c\u0634\u062f\u0647",
                        MB_YESNO|MB_ICONQUESTION);
                    if(r==IDYES) setSetting(L"saved_msgs_enabled",L"1");
                    else { InvalidateRect(h,NULL,FALSE); return 0; }
                }
                t->cartShowArchive = !t->cartShowArchive;
                t->cartDetail=false; t->cartHotBtn=0;
                InvalidateRect(h,NULL,FALSE);
                return 0;
            }
            // archive view is read-only (no detail/tiles to click)
            if(savedMsgsEnabled() && t->cartShowArchive){ return 0; }
            // ----- list view: clicking a tile OPENS the full details -----
            int disp=cartHit(pt);
            if(disp>=0 && disp<(int)s_cartMsgs.size() && disp<(int)s_cartNF.size()){
                t->cartSelDisp=disp;
                t->cartSelNF=s_cartNF[disp];
                t->cartDetail=true; t->cartHotBtn=0;
                // opening a message marks it read (مشاهده شد)
                if(!s_cartMsgs[disp].seen) seenOneMessage(me,t->cartSelNF);
                InvalidateRect(h,NULL,FALSE);
            }
            return 0;
        }
        break; }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        if(rc.right<=0 || rc.bottom<=0){ EndPaint(h,&ps); return 0; }
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        FillRect(dc,&rc,g_brBg);
        SetBkMode(dc,TRANSPARENT);

        // -------- Hybrid HTML host: just clear the background ----------------
        // When the web surface is up it covers the whole tab, so the host only
        // needs to clear the background behind it (no native form is painted).
        if(t && t->web){
            BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
            SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
            EndPaint(h,&ps);
            return 0;
        }
        // -------- Portal-message / empty tabs: a centred glass card ----------
        if(t && t->kind!=TK_RECEPTION){
            drawTabPlaceholder(dc,rc,t->kind,t);
            BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
            SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
            EndPaint(h,&ps);
            return 0;
        }

        RecH m; rcH(rc.right,m);
        CenterV v; computeCenterV(m,v,t,rc.bottom);
        recalc(t);
        recClampScroll(h,t);
        const int sy=t->scrollY;
        int VH=recPageVH(rc.right,rc.bottom,t);
        #define Y(yy) ((yy)-sy)

        // ============ RIGHT INFO PANEL ============
        if(!m.stacked && m.infoR>m.infoL) paintInfoPanel(dc,t,m.infoL,m.infoR,VH-sy,sy);

        // ---- shared helpers for the center cards ----
        const int formL=m.formL, formR=m.formR, fw=m.fw;
        const int rh=v.rh, cgap=m.cgap, cw=m.ccolW;
        (void)fw;
        auto colX=[&](int c){ return formR - (c+1)*cw - c*cgap; };
        //  v1.27.0: draw a white sub-card with a STRONG section header (icon
        //  flush-right + 16-bold title + a thin divider under it, matching the
        //  approved reference). The title uses g_fSection / sectionInk so it
        //  clearly stands above the fields.
        //  v1.31.0: fit-scaled title/label fonts so titles & labels shrink with
        //  the band on tight screens and never clip or hide behind controls.
        HFONT fSec = fitFont(16, FW_BOLD,     v.fitF);
        HFONT fLbl = fitFont(13, FW_SEMIBOLD, v.fitF);
        int   hband= (int)(S(38)*v.fitF+0.5); if(hband<S(26)) hband=S(26);
        //  ------------------------------------------------------------------
        //  v1.63.0 CARD REDESIGN (reception form).
        //  The cards were a flat surface fill + a hard 1-px full-width pen
        //  rule, so on screen they looked like plain boxes drawn with a ruler.
        //  They now get: a real elevation shadow, a soft surface gradient, an
        //  accent-tinted rounded medallion behind the section icon, and a
        //  divider that starts strong under the title and FADES toward the
        //  other edge — the detail that separates a designed panel from a
        //  drawn rectangle. Every metric still derives from hband/v.fitF so
        //  the look survives DPI scaling and the tight-screen fit factor.
        //  ------------------------------------------------------------------
        auto cardShell=[&](RECT cr){
            gpShadow(dc,cr,S(14),S(8),g_dark?70:46);
            gpGradRoundRectBg(dc,cr,S(14),g_theme.surfaceTop,g_theme.surface,
                g_theme.border,g_theme.bg);
        };
        //  fading divider: `strongAt` is the edge the title sits on (right for
        //  RTL), so the rule is fully opaque there and dissolves to nothing.
        auto fadeRule=[&](int x0,int x1,int y){
            int span=x1-x0; if(span<=0) return;
            int seg=S(6); if(seg<2) seg=2;
            for(int x=x0;x<x1;x+=seg){
                int xe=x+seg; if(xe>x1) xe=x1;
                int a=210-(190*(x1-x))/span;   // opaque at x1 (right/RTL side)
                if(a<8) a=8; if(a>255) a=255;
                gpLine(dc,x,y,xe,y,g_theme.border,1.0f,a);
            }
        };
        //  accent medallion behind a section icon
        auto medallion=[&](RECT hi){
            RECT md=hi; InflateRect(&md,S(6),S(6));
            int mr2=(md.bottom-md.top)/2;
            gpFillAlpha(dc,md,mr2,
                blendColor(g_theme.accent,g_theme.surface,50),g_dark?80:46);
            gpRoundRect(dc,md,mr2,CLR_INVALID,
                blendColor(g_theme.border,g_theme.accent,45));
        };
        auto subcard=[&](int top,int bot,const wchar_t* title,int icon){
            RECT cr={m.cardL,Y(top),m.cardR,Y(bot)};
            cardShell(cr);
            int icoW=(int)(S(20)*v.fitF+0.5); if(icoW<S(14)) icoW=S(14);
            int icoTop=cr.top+(hband-icoW)/2;
            RECT hi={formR-icoW,icoTop,formR,icoTop+icoW};
            medallion(hi);
            drawIcon(dc,icon,hi,g_theme.accent,S(2));
            SetTextColor(dc,g_theme.sectionInk); SelectObject(dc,fSec);
            RECT ht={formL,cr.top,formR-icoW-S(14),cr.top+hband};
            DrawTextW(dc,title,-1,&ht,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            fadeRule(formL,formR,cr.top+hband);
        };
        //  v1.27.0: draw a field label (with optional red required asterisk)
        //  above (x,y). Uses the readable 13-medium label font + labelInk color
        //  so labels never blend into the card background. 5px sits between the
        //  label baseline and the control below it (the lbl band is 20px, the
        //  label text is 16px tall → ~4px breathing room).
        //  The label band is v.lbl tall; the label text is drawn at the TOP of
        //  the band and the control sits at (y). Because v.lbl is now guaranteed
        //  ≥ label-font + gap (computeCenterV), the label can never be clipped or
        //  covered by the control.
        int lblTxtH=(int)(S(16)*v.fitF+0.5); if(lblTxtH<S(11)) lblTxtH=S(11);
        auto fieldLabel=[&](int x,int y,int w,const wchar_t* txt,bool req){
            SelectObject(dc,fLbl);
            RECT lr={x,Y(y)-v.lbl,x+w,Y(y)-v.lbl+lblTxtH};
            SetTextColor(dc,g_theme.labelInk);
            DrawTextW(dc,txt,-1,&lr,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
            if(req){
                SIZE sz; GetTextExtentPoint32W(dc,txt,(int)wcslen(txt),&sz);
                int ax = x+w - sz.cx - S(8);
                SetTextColor(dc,g_theme.danger);
                RECT ar={ax-S(10),lr.top,ax,lr.bottom};
                DrawTextW(dc,L"*",-1,&ar,DT_RIGHT|DT_SINGLELINE|DT_NOPREFIX);
            }
        };

        // ============ CARD 1: اطلاعات بیمار (3-column grid) ============
        subcard(v.c1Top,v.c1Bot,L"اطلاعات بیمار",ICO_USER);
        // labels (col0=right, col2=left)
        fieldLabel(colX(0),v.r1y[0],cw,L"نام",true);
        fieldLabel(colX(1),v.r1y[0],cw,L"نام خانوادگی",true);
        fieldLabel(colX(2),v.r1y[0],cw,L"کد ملی",true);
        fieldLabel(colX(0),v.r1y[1],cw,L"نام پدر",false);
        fieldLabel(colX(1),v.r1y[1],cw,L"تاریخ تولد",false);
        fieldLabel(colX(2),v.r1y[1],cw,L"جنسیت",false);
        fieldLabel(colX(0),v.r1y[2],cw,L"شماره موبایل",true);
        fieldLabel(colX(1),v.r1y[2],cw,L"تلفن ثابت",false);
        fieldLabel(colX(2),v.r1y[2],cw,L"آدرس",false);

        // ==== MIDDLE ROW: انجام دهنده | پزشک معالج | بیمه و نوبت (3 cards) ====
        {
            MidCards mc; rcMidCards(m,mc);
            int in=S(10);
            // v1.63.0: same modern shell as subcard() (shadow + gradient +
            // icon medallion + fading rule) at the narrower sub-card scale.
            auto card3=[&](int L0,int R0,const wchar_t* title,int icon){
                RECT cr={L0,Y(v.dpTop),R0,Y(v.dpBot)};
                cardShell(cr);
                int icoW=(int)(S(18)*v.fitF+0.5); if(icoW<S(13)) icoW=S(13);
                int icoTop=cr.top+(hband-icoW)/2;
                RECT hi={R0-in-icoW,icoTop,R0-in,icoTop+icoW};
                medallion(hi);
                drawIcon(dc,icon,hi,g_theme.accent,S(2));
                SetTextColor(dc,g_theme.sectionInk); SelectObject(dc,fSec);
                RECT ht={L0+in,cr.top,R0-in-icoW-S(12),cr.top+hband};
                DrawTextW(dc,title,-1,&ht,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
                fadeRule(L0+in,R0-in,cr.top+hband);
            };
            // — انجام دهنده (rightmost)
            card3(mc.perfL,mc.perfR,L"انجام دهنده",ICO_USER);
            fieldLabel(mc.perfL+in,v.dpR1y,mc.perfR-mc.perfL-2*in,L"کد انجام دهنده",false);
            fieldLabel(mc.perfL+in,v.dpR2y,mc.perfR-mc.perfL-2*in,L"نام انجام دهنده",false);
            // — پزشک معالج (middle) — 2 rows only (no duplicate percentage)
            card3(mc.docL,mc.docR,L"پزشک معالج",ICO_CROSS_MED);
            fieldLabel(mc.docL+in,v.dpR1y,mc.docR-mc.docL-2*in,L"شماره نظام پزشکی",false);
            fieldLabel(mc.docL+in,v.dpR2y,mc.docR-mc.docL-2*in,L"نام پزشک",false);
            // — بیمه و نوبت (left, widest) — reference row layout:
            //   row1: بیمه تکمیلی | نوع بیمه پایه | نوع نوبت | نوع پذیرش  (RTL)
            //   row2: مبلغ خدمت | سهم بیمه | تخفیف | (سهم بیمه ٪ chip)   (RTL)
            card3(mc.insL,mc.insR,L"بیمه و نوبت",ICO_SHIELD);
            { int inL=mc.insL+in, inR=mc.insR-in;
              int gw=(inR-inL-3*cgap)/4;
              auto gx=[&](int c){ return inR-(c+1)*gw-c*cgap; };
              //  row1: 4 combos (no prices) — matches Management-driven design.
              fieldLabel(gx(0),v.dpR1y,gw,L"نوع پذیرش",false);
              fieldLabel(gx(1),v.dpR1y,gw,L"نوع نوبت",false);
              fieldLabel(gx(2),v.dpR1y,gw,L"نوع بیمه",false);
              fieldLabel(gx(3),v.dpR1y,gw,L"بیمه تکمیلی",false);
              //  row2 (RTL): col3 «درصد بیمه تکمیل ٪» | col2 «شیفت نوبت» |
              //  col1 «تاریخ نوبت» | col0 free. No price/discount/share fields.
              fieldLabel(gx(3),v.dpR2y,gw,L"درصد بیمه تکمیل",false);
              { RECT pu={gx(3)+gw-S(20),Y(v.dpR2y),gx(3)+gw,Y(v.dpR2y)+rh};
                SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
                DrawTextW(dc,L"٪",-1,&pu,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX); }
              fieldLabel(gx(2),v.dpR2y,gw,L"شیفت نوبت",false);
              fieldLabel(gx(1),v.dpR2y,gw,L"تاریخ نوبت",false);
            }
            // clear the stale preview hit-rects so clicks in that old area are inert
            SetRectEmpty(&s_psPRect); SetRectEmpty(&s_psSRect);
        }
        // ===== SCHEDULING STRIP RETIRED (v1.29.0) =====
        //  تاریخ/شیفت نوبت now live inside the «بیمه و نوبت» card (row 2) so the
        //  whole admission page fits on one frame without any page scrolling.

        // ==== BOTTOM RIGHT PANEL: خدمات table (matches the reference) ====
        {
            BotPanels bp; rcBotPanels(m,bp);
            int in=S(10);
            int svL=bp.svL+in, svR=bp.svR-in, svW=svR-svL;
            RECT cr={bp.svL,Y(v.bTop),bp.svR,Y(v.bBot)};
            cardShell(cr);
            // header row: «خدمات» title flush-right + (افزودن خدمت btn = control)
            SetTextColor(dc,g_theme.sectionInk); SelectObject(dc,fitFont(16,FW_BOLD,v.fitF));
            { RECT ht={svL,Y(v.svcToolY),svR,Y(v.svcToolY)+rh};
              DrawTextW(dc,L"خدمات",-1,&ht,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
            //  v1.31.0 collapse toggle: a small chevron chip just LEFT of the title.
            //  ▾ = expanded (click to collapse), ▸ = collapsed (click to expand).
            { int tsz=S(22), ty=Y(v.svcToolY)+(rh-tsz)/2;
              SIZE tsz2; SelectObject(dc,fitFont(16,FW_BOLD,v.fitF));
              GetTextExtentPoint32W(dc,L"خدمات",5,&tsz2);
              int tx=svR-tsz2.cx-S(8)-tsz;
              RECT tr={tx,ty,tx+tsz,ty+tsz};
              s_svcToggleR=tr;
              // v1.63.0: the collapse chip is a small raised control
              gpGradRoundRect(dc,tr,S(7),
                  blendColor(g_theme.surfaceTop,g_theme.surface2,45),
                  g_theme.surface2,blendColor(g_theme.border,g_theme.accent,30));
              drawCollapseCaret(dc,tr,t->svcCollapsed,g_theme.accent); }
            // table header strip — reference columns (RTL):
            // ردیف | نام خدمت | مبلغ (ریال) | تخفیف بیمه (ریال) | سهم بیمه (ریال) | عملیات
            // ------------------------------------------------------------
            //  v1.63.0 SERVICES TABLE. The header was a flat grey strip and the
            //  zebra rows were the same grey, so a filled table was hard to
            //  read: you could not tell the header from the data. Now:
            //    * the header is an accent-tinted gradient band with a bright
            //      bottom rule (a real table head),
            //    * a hairline separates the header from the first data row,
            //    * zebra rows use a much subtler wash so the DATA dominates,
            //    * every column gets a faint vertical divider, which is what
            //      makes a money table actually readable.
            // ------------------------------------------------------------
            RECT head={svL,Y(v.svcHeadY),svR,Y(v.svcHeadY)+S(28)};
            gpGradRoundRect(dc,head,S(7),
                blendColor(g_theme.accent,g_theme.surface,g_dark?76:84),
                blendColor(g_theme.accent,g_theme.surface,g_dark?84:90),
                blendColor(g_theme.border,g_theme.accent,40));
            gpLine(dc,svL+S(2),head.bottom-1,svR-S(2),head.bottom-1,
                blendColor(g_theme.accent,g_theme.surface,35),1.0f,190);
            SelectObject(dc,g_fLabel); SetTextColor(dc,g_theme.text);
            const wchar_t* cols[6]={L"ردیف",L"نام خدمت",L"مبلغ (ریال)",
                L"تخفیف بیمه (ریال)",L"سهم بیمه (ریال)",L"عملیات"};
            int weights[6]={7,24,18,20,19,12};
            int wsum=0; for(int i=0;i<6;i++) wsum+=weights[i];
            int colR[6], colW[6]; { int x=svR; for(int i=0;i<6;i++){ colW[i]=(svW*weights[i])/wsum; colR[i]=x; x-=colW[i]; } }
            for(int i=0;i<6;i++){
                RECT hr={colR[i]-colW[i]+S(2),head.top,colR[i]-S(2),head.bottom};
                DrawTextW(dc,cols[i],-1,&hr,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            }
            s_svcHits.clear();
            long long sumAmt=0,sumDisc=0,sumIns=0,sumPat=0;
            int rowH=S(32);
            int maxRows=(v.svcBodyBot-v.svcBodyY)/rowH; if(maxRows<0) maxRows=0;
            if(v.svcRows==0){
                int ecy=Y(v.svcBodyY)+(v.svcBodyBot-v.svcBodyY)/2-S(10);
                // v1.63.0: the empty table shows a small tinted glyph above the
                // hint so an un-filled bill reads as a state, not a blank void.
                { int gsz=S(26), gx=(svL+svR)/2;
                  RECT gr={gx-gsz/2,ecy-gsz-S(8),gx+gsz/2,ecy-S(8)};
                  RECT md=gr; InflateRect(&md,S(7),S(7));
                  gpFillAlpha(dc,md,(md.bottom-md.top)/2,
                      blendColor(g_theme.accent,g_theme.surface,55),g_dark?72:46);
                  drawIcon(dc,ICO_PLUS,gr,
                      blendColor(g_theme.accent,g_theme.text,10),S(2)); }
                SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
                RECT er={svL,ecy,svR,ecy+S(20)};
                DrawTextW(dc,L"هیچ خدمتی اضافه نشده است — روی «افزودن خدمت +» بزنید.",-1,&er,
                    DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
            } else {
                for(int r0=0;r0<v.svcRows;r0++){
                    const SvcRow& s=t->services[r0];
                    long long line=s.price*(s.qty>0?s.qty:1);
                    sumAmt+=line; sumDisc+=s.discount; sumIns+=s.insShare; sumPat+=s.patShare;
                    if(r0>=maxRows) continue;      // beyond the visible band
                    int ry=Y(v.svcBodyY)+r0*rowH;
                    if(r0%2==1){ RECT zr={svL,ry,svR,ry+rowH};
                        // v1.63.0: a whisper-light band (was full surface2,
                        // which fought with the header for attention)
                        gpFillAlpha(dc,zr,S(4),g_theme.surface2,g_dark?110:150); }
                    // faint row separator + column dividers
                    gpLine(dc,svL+S(2),ry+rowH,svR-S(2),ry+rowH,g_theme.border,1.0f,70);
                    for(int c=1;c<6;c++)
                        gpLine(dc,colR[c],ry+S(3),colR[c],ry+rowH-S(3),
                               g_theme.border,1.0f,55);
                    wchar_t idx[8]; swprintf(idx,8,L"%d",r0+1);
                    std::wstring cells[5]={
                        toFaDigits(idx), s.name,
                        toFaDigits(formatMoney(line)),
                        toFaDigits(formatMoney(s.discount)),
                        toFaDigits(formatMoney(s.insShare)) };
                    SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.text);
                    for(int c=0;c<5;c++){
                        RECT cellr={colR[c]-colW[c]+S(3),ry,colR[c]-S(3),ry+rowH};
                        UINT fl=(c==1)?(DT_RIGHT|DT_RTLREADING):DT_CENTER;
                        DrawTextW(dc,cells[c].c_str(),-1,&cellr,
                            fl|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_END_ELLIPSIS);
                    }
                    // operations: edit (accent pencil-ish) + red trash
                    RECT opr={colR[5]-colW[5],ry,colR[5],ry+rowH};
                    int bs=S(16), cxo=(opr.left+opr.right)/2;
                    RECT ed={cxo+S(3),ry+(rowH-bs)/2,cxo+S(3)+bs,ry+(rowH-bs)/2+bs};
                    drawIcon(dc,ICO_GEAR,ed,g_theme.accent,S(2));
                    RECT del={cxo-S(3)-bs,ry+(rowH-bs)/2,cxo-S(3),ry+(rowH-bs)/2+bs};
                    drawIcon(dc,ICO_TRASH,del,g_theme.danger,S(2));
                    SvcHit hh; hh.del=del; s_svcHits.push_back(hh);
                }
            }
            // footer summary strip — always drawn (matches the reference)
            RECT foot={svL,Y(v.svcFootY),svR,Y(v.svcFootY)+S(24)};
            // v1.63.0: the totals strip is emphasised with a slightly stronger
            // gradient + accent hairline so the money line stands out.
            gpGradRoundRect(dc,foot,S(7),
                blendColor(g_theme.surfaceTop,g_theme.surface2,35),
                g_theme.surface2, blendColor(g_theme.border,g_theme.accent,32));
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.text);
            { int qw=svW/4, x=svR;
              auto footCell=[&](const wchar_t* label,long long val){
                  RECT fr={x-qw+S(2),foot.top,x-S(2),foot.bottom};
                  std::wstring s2=std::wstring(label)+L" "+toFaDigits(formatMoney(val));
                  DrawTextW(dc,s2.c_str(),-1,&fr,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
                  x-=qw;
              };
              footCell(L"جمع خدمات:",sumAmt);
              footCell(L"جمع تخفیف:",sumDisc);
              footCell(L"جمع سهم بیمه:",sumIns);
              footCell(L"جمع سهم بیمار:",sumPat);
            }
            // inline add-service panel background strip (when open)
            if(v.panelOpen){
                RECT pr={svL,Y(v.svcPanelY)-S(3),svR,Y(v.svcPanelY)+rh+S(3)};
                // v1.63.0: the open add-service strip glows so it is obvious the
                // form is in "adding" mode.
                gpShadowColor(dc,pr,S(8),S(6),70,g_theme.accent);
                gpGradRoundRect(dc,pr,S(8),
                    blendColor(g_theme.surface2,g_theme.accent,g_dark?14:8),
                    g_theme.surface2,g_theme.accent);
            }

            // ==== BOTTOM LEFT PANEL: صندوق نرفته‌ها / صف پذیرش ====
            int upL=bp.upL+in, upR=bp.upR-in, upW=upR-upL;
            RECT ur={bp.upL,Y(v.bTop),bp.upR,Y(v.bBot)};
            cardShell(ur);
            // ---- v1.69.0 PANEL HEADER: segmented tabs (right) + RED close (left)
            //      on a faint header band. The two tabs are clearly divided pills
            //      (icon + label + count badge); the red x collapses the panel and
            //      becomes an expand chevron when collapsed. Manual RTL layout. ----
            { int bandH=S(34);
              int bandT=Y(v.upTabY)-S(2), bandB=bandT+bandH;
              RECT hbar={upL,bandT,upR,bandB};
              gpFillAlpha(dc,hbar,S(10),g_theme.surface2,g_dark?58:40);
              gpLine(dc,upL+S(6),bandB-S(1),upR-S(6),bandB-S(1),
                     g_theme.border,1.0f,120);
              // -- red close / expand toggle (far LEFT = visual end) --
              int clsS=S(30), clsY=bandT+(bandH-clsS)/2;
              RECT cls={upL+S(2),clsY,upL+S(2)+clsS,clsY+clsS};
              s_upToggleR=cls;                 // toggles upCollapsed (hit-tested)
              if(!t->upCollapsed){
                  // expanded -> RED close pill (the close button is RED)
                  gpShadowColor(dc,cls,S(8),S(5),82,g_theme.danger);
                  gpGradRoundRectBg(dc,cls,S(8),
                      blendColor(g_theme.danger,g_theme.accentText,16),
                      g_theme.danger,CLR_INVALID,g_theme.bg);
                  RECT cli=cls; InflateRect(&cli,-S(7),-S(7));
                  drawIcon(dc,ICO_X,cli,g_theme.accentText,S(2));
              } else {
                  // collapsed -> accent expand chevron
                  gpGradRoundRectBg(dc,cls,S(8),
                      blendColor(g_theme.surfaceTop,g_theme.surface2,45),
                      g_theme.surface2,blendColor(g_theme.border,g_theme.accent,30),
                      g_theme.bg);
                  drawCollapseCaret(dc,cls,true,g_theme.accent);
              }
              // -- segmented tab pills (far RIGHT = visual start); RTL: tab 0 rightmost
              int tabGap=S(6), tabH=S(30), tabW=S(140);
              int need=2*tabW+tabGap;
              int maxTabs=upW-clsS-S(10)-S(8);
              if(need>maxTabs){ tabW=(maxTabs-tabGap)/2; if(tabW<S(84)) tabW=S(84); }
              int contT=bandT+(bandH-tabH)/2, contB=contT+tabH;
              RECT t0={upR-S(4)-tabW,contT,upR-S(4),contB};                 // tab 0
              RECT t1={upR-S(4)-2*tabW-tabGap,contT,upR-S(4)-tabW-tabGap,contB}; // tab 1
              s_upTabR[0]=t0; s_upTabR[1]=t1;
              for(int k=0;k<2;k++){
                  RECT tr=k==0?t0:t1;
                  bool act=(t->bottomTab==k);
                  int icoS=S(16), cyT=(tr.top+tr.bottom)/2;
                  if(act){
                      gpShadowColor(dc,tr,S(8),S(6),88,g_theme.accent);
                      gpGradRoundRectBg(dc,tr,S(8),g_theme.accentHover,
                                        g_theme.accent,CLR_INVALID,g_theme.bg);
                  } else {
                      gpGradRoundRectBg(dc,tr,S(8),
                          blendColor(g_theme.surfaceTop,g_theme.surface2,45),
                          g_theme.surface2,
                          blendColor(g_theme.border,g_theme.accent,22),g_theme.bg);
                  }
                  // icon (right side of the segment, RTL start)
                  RECT ico={tr.right-icoS-S(8),cyT-icoS/2,tr.right-S(8),cyT+icoS/2};
                  drawIcon(dc,k==0?ICO_RECEIPT:ICO_PEOPLE,ico,
                      act?g_theme.accentText:g_theme.accent,S(2));
                  // count badge (left side of the segment)
                  int bgS=S(18);
                  RECT bdg={tr.left+S(6),cyT-bgS/2,tr.left+S(6)+bgS,cyT+bgS/2};
                  int cnt=upCount(k);
                  if(act){
                      gpFillAlpha(dc,bdg,S(9),g_theme.accentText,g_dark?215:238);
                      SetTextColor(dc,g_theme.accent);
                  } else {
                      gpFillAlpha(dc,bdg,S(9),g_theme.surface,g_dark?205:255);
                      SetTextColor(dc,g_theme.textDim);
                  }
                  SelectObject(dc,g_fSmall);
                  wchar_t cb[8]; swprintf(cb,8,L"%d",cnt);
                  DrawTextW(dc,toFaDigits(cb).c_str(),-1,&bdg,
                      DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
                  // label between badge and icon
                  SetTextColor(dc,act?g_theme.accentText:g_theme.text);
                  SelectObject(dc,act?g_fUIB:g_fUI);
                  RECT lr2={bdg.right+S(6),tr.top,ico.left-S(6),tr.bottom};
                  DrawTextW(dc,k==0?L"صندوق نرفته ها":L"صف پذیرش",-1,&lr2,
                      DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
              }
            }
            // ---- v1.69.0 FILTER BAR: a labelled chip (wide screens only) grouping
            //      the real toolbar controls (search / date / hours / refresh). The
            //      controls are positioned by tabPageLayout with the same fchip
            //      offset, so the chip and the controls never drift apart. ----
            { int fchip=(upW>S(360))?S(48):0;
              if(fchip){
                  RECT fc={upL,Y(v.upToolY)-S(2),upL+fchip,Y(v.upToolY)+v.rh+S(2)};
                  gpGradRoundRectBg(dc,fc,S(8),
                      blendColor(g_theme.surfaceTop,g_theme.surface2,40),
                      g_theme.surface2,blendColor(g_theme.border,g_theme.accent,25),
                      g_theme.bg);
                  SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.accent);
                  RECT ftr={fc.left+S(4),fc.top,fc.right-S(4),fc.bottom};
                  DrawTextW(dc,L"فیلتر",-1,&ftr,
                      DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
              }
            }
            // ---- v1.69.0 TABLE HEAD: نوبت | نام بیمار | تاریخ و ساعت | وضعیت |
            //      عملیات — accent-tinted band (matches the خدمات table). ----
            RECT uhead={upL,Y(v.upHeadY),upR,Y(v.upHeadY)+S(28)};
            gpGradRoundRect(dc,uhead,S(7),
                blendColor(g_theme.accent,g_theme.surface,g_dark?76:84),
                blendColor(g_theme.accent,g_theme.surface,g_dark?84:90),
                blendColor(g_theme.border,g_theme.accent,40));
            gpLine(dc,upL+S(2),uhead.bottom-1,upR-S(2),uhead.bottom-1,
                blendColor(g_theme.accent,g_theme.surface,35),1.0f,190);
            SelectObject(dc,g_fLabel); SetTextColor(dc,g_theme.text);
            const wchar_t* ucols[5]={L"نوبت",L"نام بیمار",L"تاریخ و ساعت",
                L"وضعیت",L"عملیات"};
            int uwt[5]={10,28,20,16,26};
            int uws=0; for(int i=0;i<5;i++) uws+=uwt[i];
            int ucR[5], ucW[5]; { int x=upR; for(int i=0;i<5;i++){ ucW[i]=(upW*uwt[i])/uws; ucR[i]=x; x-=ucW[i]; } }
            for(int i=0;i<5;i++){
                RECT hr={ucR[i]-ucW[i]+S(2),uhead.top,ucR[i]-S(2),uhead.bottom};
                DrawTextW(dc,ucols[i],-1,&hr,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            }
            // ---- rows: read from the tab's data file ----
            upLoad(t->bottomTab);
            // filter by the search box content (name / code)
            wchar_t sb[128]={0}; if(t->eSvcSearch) GetWindowTextW(t->eSvcSearch,sb,128);
            std::wstring q=trim(sb);
            // v1.26.0: «مراجعات اخیر (ساعت)» window — rows with a known epoch
            // older than the selected window are hidden (legacy rows without an
            // epoch always stay visible so nothing silently disappears).
            long long hrsWin=0;
            { int hi=(int)SendMessageW(t->cUpHours,CB_GETCURSEL,0,0);
              static const long long HW[3]={6,12,24};
              if(hi>=0 && hi<3) hrsWin=HW[hi]; }
            long long nowEp=(long long)time(NULL);
            // «تاریخ تا» filter: rows dated AFTER the chosen Jalali date are
            // hidden (string compare works — both sides share YYYY/MM/DD with
            // Persian digits). Rows without a date always stay visible.
            std::wstring upTo;
            { wchar_t db2[16]={0};
              if(t->eUpDate) GetWindowTextW(t->eUpDate,db2,16);
              std::wstring dv=trim(db2);
              if(dv.size()==10) upTo=toFaDigits(dv); }
            std::vector<int> shown;
            for(int i=(int)s_upRows.size()-1;i>=0;i--){   // newest first
                if(hrsWin>0 && s_upRows[i].epoch>0 &&
                   nowEp - s_upRows[i].epoch > hrsWin*3600) continue;
                if(!upTo.empty() && !s_upRows[i].date.empty() &&
                   toFaDigits(s_upRows[i].date) > upTo) continue;
                if(!q.empty()){
                    wchar_t qb[16]; swprintf(qb,16,L"%d",s_upRows[i].q);
                    if(s_upRows[i].name.find(q)==std::wstring::npos &&
                       std::wstring(qb).find(q)==std::wstring::npos &&
                       toFaDigits(qb).find(q)==std::wstring::npos) continue;
                }
                shown.push_back(i);
            }
            s_upHits.clear();
            int urowH=S(32);
            int umax=(v.upBodyBot-v.upBodyY)/urowH; if(umax<0) umax=0;
            if(shown.empty()){
                int ecy=Y(v.upBodyY)+(v.upBodyBot-v.upBodyY)/2-S(10);
                // v1.63.0: tinted glyph above the hint (state, not a blank void)
                { int gsz=S(26), gx=(upL+upR)/2;
                  RECT gr={gx-gsz/2,ecy-gsz-S(8),gx+gsz/2,ecy-S(8)};
                  RECT md=gr; InflateRect(&md,S(7),S(7));
                  gpFillAlpha(dc,md,(md.bottom-md.top)/2,
                      blendColor(g_theme.accent,g_theme.surface,55),g_dark?72:46);
                  drawIcon(dc,ICO_RECEIPT,gr,
                      blendColor(g_theme.accent,g_theme.text,10),S(2)); }
                SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
                RECT er={upL,ecy,upR,ecy+S(20)};
                DrawTextW(dc,t->bottomTab==0
                    ?L"موردی در صندوق نرفته ها نیست."
                    :L"موردی در صف پذیرش نیست.",-1,&er,
                    DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
            } else {
                std::wstring today=FormatJalaliPersian(0);
                for(int k=0;k<(int)shown.size() && k<umax;k++){
                    const UpRow& rrow=s_upRows[shown[k]];
                    int ry=Y(v.upBodyY)+k*urowH;
                    bool sel=!t->upSelRaw.empty() && rrow.raw==t->upSelRaw;
                    bool hot=(t->upHotIdx==shown[k]);
                    RECT rowR={upL,ry,upR,ry+urowH};
                    // zebra (skipped for highlighted rows — the wash reads cleaner)
                    if(!sel && !hot && k%2==1)
                        gpFillAlpha(dc,rowR,S(4),g_theme.surface2,g_dark?110:150);
                    // hover / selection wash
                    if(sel){
                        gpFillAlpha(dc,rowR,0,
                            blendColor(g_theme.accent,g_theme.surface,g_dark?58:70),
                            g_dark?74:52);
                        RECT bar={upR-S(3),ry+S(2),upR,ry+urowH-S(2)};
                        gpFillAlpha(dc,bar,S(2),g_theme.accent,225);
                    } else if(hot){
                        gpFillAlpha(dc,rowR,0,
                            blendColor(g_theme.accent,g_theme.surface,g_dark?80:88),
                            g_dark?40:28);
                    }
                    // row separator + column dividers
                    gpLine(dc,upL+S(2),ry+urowH,upR-S(2),ry+urowH,g_theme.border,1.0f,70);
                    for(int c=1;c<5;c++)
                        gpLine(dc,ucR[c],ry+S(3),ucR[c],ry+urowH-S(3),
                               g_theme.border,1.0f,55);
                    // ---- queue-number badge (col 0) ----
                    wchar_t qb[16]; swprintf(qb,16,L"%d",rrow.q);
                    { int bgS=S(22), cyT=ry+urowH/2;
                      RECT cell0={ucR[0]-ucW[0]+S(6),ry,ucR[0]-S(6),ry+urowH};
                      int bx=(cell0.left+cell0.right)/2-bgS/2;
                      RECT bdg={bx,cyT-bgS/2,bx+bgS,cyT+bgS/2};
                      gpFillAlpha(dc,bdg,S(7),
                          sel?g_theme.accent:blendColor(g_theme.accent,g_theme.surface,72),
                          sel?(g_dark?230:255):(g_dark?70:46));
                      SelectObject(dc,g_fUIB);
                      SetTextColor(dc,sel?g_theme.accentText:g_theme.accent);
                      DrawTextW(dc,toFaDigits(qb).c_str(),-1,&bdg,
                          DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
                    }
                    // ---- patient name (col 1) — prominent, RTL right-aligned ----
                    { RECT cellr={ucR[1]-ucW[1]+S(8),ry,ucR[1]-S(8),ry+urowH};
                      SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.text);
                      DrawTextW(dc,rrow.name.c_str(),-1,&cellr,
                          DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                    }
                    // ---- date & time (col 2) ----
                    { std::wstring dt=rrow.date.empty()?today:rrow.date;
                      if(!rrow.time.empty()) dt+=L"  "+rrow.time;
                      RECT cellr={ucR[2]-ucW[2]+S(4),ry,ucR[2]-S(4),ry+urowH};
                      SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
                      DrawTextW(dc,toFaDigits(dt).c_str(),-1,&cellr,
                          DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                    }
                    // ---- payment-status chip (col 3) ----
                    { bool paid=rrow.paid>0;
                      COLORREF sc=paid?g_theme.success:g_theme.warn;
                      int chH=S(22), cyT=ry+urowH/2;
                      RECT cellr={ucR[3]-ucW[3]+S(6),ry,ucR[3]-S(6),ry+urowH};
                      const wchar_t* sl=paid?L"پرداخت شده":L"پرداخت نشده";
                      SelectObject(dc,g_fSmall);
                      SIZE sz; GetTextExtentPoint32W(dc,sl,(int)wcslen(sl),&sz);
                      int chW=sz.cx+S(18); if(chW>cellr.right-cellr.left) chW=cellr.right-cellr.left;
                      int chx=(cellr.left+cellr.right)/2-chW/2;
                      RECT ch={chx,cyT-chH/2,chx+chW,cyT+chH/2};
                      gpFillAlpha(dc,ch,S(11),blendColor(sc,g_theme.surface,72),
                          g_dark?150:200);
                      SetTextColor(dc,sc);
                      DrawTextW(dc,sl,-1,&ch,
                          DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                    }
                    // ---- operations (col 4): edit + delete always; up/down arrows
                    //      on the selected row (progressive disclosure of reorder) ----
                    { UpHit uh{};
                      uh.row=rowR; uh.idx=shown[k];
                      RECT opr={ucR[4]-ucW[4]+S(2),ry,ucR[4]-S(2),ry+urowH};
                      int avail=opr.right-opr.left, gap2=S(3);
                      int n=sel?4:2;
                      int bs=(avail-(n-1)*gap2)/n;
                      if(bs>S(15)) bs=S(15);
                      if(bs<S(9)){ n=2; bs=(avail-gap2)/2; if(bs>S(15))bs=S(15); if(bs<S(9))bs=S(9); }
                      int totW=n*bs+(n-1)*gap2;
                      int x0=(opr.left+opr.right)/2-totW/2;
                      int iy=ry+(urowH-bs)/2, xx=x0;
                      if(sel && n==4){
                          RECT rU ={xx,iy,xx+bs,iy+bs}; xx+=bs+gap2;
                          RECT rDn={xx,iy,xx+bs,iy+bs}; xx+=bs+gap2;
                          drawArrowBtn(dc,rU,true, g_theme.accent);
                          drawArrowBtn(dc,rDn,false,g_theme.accent);
                          uh.up=rU; uh.down=rDn;
                      }
                      RECT rE={xx,iy,xx+bs,iy+bs}; xx+=bs+gap2;
                      RECT rD={xx,iy,xx+bs,iy+bs};
                      drawIcon(dc,ICO_GEAR, rE, sel?g_theme.accent:g_theme.textDim,S(2));
                      drawIcon(dc,ICO_TRASH,rD, g_theme.danger,S(2));
                      uh.edit=rE; uh.del=rD;
                      s_upHits.push_back(uh);
                    }
                }
            }
            // ---- footer: تعداد کل + selected hint + blue «+ افزودن به …» link ----
            { RECT fr={upL,Y(v.upFootY),upR,Y(v.upFootY)+S(22)};
              SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
              wchar_t cb2[64]; swprintf(cb2,64,L"تعداد کل: %d",(int)shown.size());
              DrawTextW(dc,toFaDigits(cb2).c_str(),-1,&fr,
                  DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
              // selected-patient hint in the centre (only when a row is selected)
              if(!t->upSelRaw.empty()){
                  const wchar_t* nm=L"";
                  for(int idx=0;idx<(int)s_upRows.size();idx++)
                      if(s_upRows[idx].raw==t->upSelRaw){ nm=s_upRows[idx].name.c_str(); break; }
                  if(nm[0]){
                      std::wstring hl=std::wstring(L"انتخاب: ")+nm;
                      SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.accent);
                      RECT hr={upL+S(200),fr.top,upR-S(120),fr.bottom};
                      DrawTextW(dc,hl.c_str(),-1,&hr,
                          DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                  }
              }
              // blue add-link at the LEFT of the footer (reference image)
              std::wstring lk = t->bottomTab==0
                  ? L"+ افزودن به صندوق نرفته ها" : L"+ افزودن به صف پذیرش";
              SetTextColor(dc,g_theme.accent); SelectObject(dc,g_fUIB);
              RECT lr={upL,fr.top,upL+S(190),fr.bottom};
              DrawTextW(dc,lk.c_str(),-1,&lr,
                  DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
              s_upAddRect=lr;
            }
        }

        // ============ framed input wells behind every center control ========
        {
            HWND inputs[]={t->eFirst,t->eLast,t->eNid,t->eFather,t->eBirth,
                t->cGender,t->eMobile,t->ePhone,t->eAddr,
                t->ePerfCode,t->cPerfList,t->eDoc2Code,t->cDoc2List,
                t->cPType,t->cNType,t->cIns,t->cSupp,t->eSuppPct2,
                t->ePrice,t->eDiscount,t->eApptDate,t->cApptShift,t->eApptP,t->eApptS,
                t->eSvcSearch,t->eSvcCode,t->eSvcName,t->eSvcQty,t->eSvcFreeAmt};
            const int NIN=(int)(sizeof(inputs)/sizeof(inputs[0]));
            // map well index → required-field mask bit for the FIRST 5 required
            // identity fields (نام/نام‌خانوادگی/کد‌ملی/نام‌پدر/تاریخ‌تولد) + امبلغ.
            HWND foc=GetFocus();
            for(int i=0;i<NIN;i++){
                if(!inputs[i]) continue;
                if(!IsWindowVisible(inputs[i])) continue;
                RECT wr; GetWindowRect(inputs[i],&wr);
                POINT a={wr.left,wr.top}, b={wr.right,wr.bottom};
                ScreenToClient(h,&a); ScreenToClient(h,&b);
                if(b.y<=a.y) continue;
                int wellBot = a.y + rh;          // clamp to a single row height
                RECT well={a.x-S(6),a.y-S(4),b.x+S(6),wellBot+S(4)};
                bool focused = (inputs[i]==foc);
                HWND w0=inputs[i];
                bool idErr = t->idChecked && !t->idVerified &&
                             (w0==t->eFirst || w0==t->eLast);
                bool reqErr = (t->invalidMask&(1<<0) && w0==t->eFirst)
                            ||(t->invalidMask&(1<<1) && w0==t->eLast)
                            ||(t->invalidMask&(1<<2) && w0==t->eNid)
                            ||(t->invalidMask&(1<<3) && w0==t->eFather)
                            ||(t->invalidMask&(1<<4) && w0==t->eBirth)
                            ||(t->invalidMask&(1<<13)&& w0==t->ePrice);
                bool invalid = idErr || reqErr;
                COLORREF bord = invalid ? g_theme.danger
                              : focused ? g_theme.accent : g_theme.border;
                // ------------------------------------------------------------
                //  v1.63.0 INPUT WELL. Every field was a flat rectangle whose
                //  only state cue was the border colour. A well is now visibly
                //  RECESSED (darker at the top, fading to the input colour),
                //  the FOCUSED field gets an accent glow so the caret's home is
                //  obvious, and an INVALID field gets a red glow + red rim so a
                //  failed validation cannot be missed on a dense form.
                // ------------------------------------------------------------
                if(invalid)      gpShadowColor(dc,well,S(9),S(6),86,g_theme.danger);
                else if(focused) gpShadowColor(dc,well,S(9),S(6),74,g_theme.accent);
                gpGradRoundRect(dc,well,S(9),
                    blendColor(g_theme.inputBg,g_theme.border,g_dark?26:34),
                    g_theme.inputBg,bord);
            }
        }

        // ============ status message (above the buttons) ============
        if(t && !t->lastMsg.empty()){
            SetTextColor(dc,t->msgCol); SelectObject(dc,g_fSmall);
            RECT mr2={formL,Y(v.btnY)-S(18),formR,Y(v.btnY)-S(2)};
            DrawTextW(dc,t->lastMsg.c_str(),-1,&mr2,
                DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        }

        // ============ LEFT BILLING COLUMN (صورتحساب) ============
        //  v1.19.0 premium redesign (matches the reference):
        //    1) «صورتحساب» summary card  — 8 money rows, NO inline highlight.
        //    2) «مبلغ نهایی» card        — vivid blue gradient + wallet icon +
        //                                   large white amount (replaces the old
        //                                   dark navy «چک پرداختی» strip).
        //    3) «چاپ» group + 3 print buttons (pinned to the column bottom by
        //       tabPageLayout). All three blocks scroll together with the page.
        if(!m.stacked && t){
            // ----- 1) grouped «صورت حساب» summary card -----
            //  geometry MUST mirror recPrintGroupTop():
            //    header S(30) + 3 × (group title S(20) + 3 rows × S(20))
            //    + highlight row S(26) + bottom pad S(8)
            int sumTop = S(RC_OUT);
            int sumBot = sumTop + S(30) + 3*(S(20)+3*S(20)) + S(26) + S(8);
            RECT card={m.billL,Y(sumTop),m.billR,Y(sumBot)};
            cardShell(card);
            int bl=card.left+S(10), br=card.right-S(10);
            // header «صورت حساب» (icon flush-right, RTL) — strong section title
            { int icoW=S(18);
              RECT bi={br-icoW,card.top+S(7),br,card.top+S(7)+icoW};
              medallion(bi);
              drawIcon(dc,ICO_RECEIPT,bi,g_theme.accent,S(2));
              SetTextColor(dc,g_theme.sectionInk); SelectObject(dc,g_fSection);
              RECT bt={bl,card.top+S(4),br-icoW-S(6),card.top+S(28)};
              DrawTextW(dc,L"صورت حساب",-1,&bt,
                  DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
            int ry=card.top+S(30);
            // group title (accent bold) + money-row helpers
            auto grp=[&](const wchar_t* g){
                SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.accent);
                RECT gr={bl,ry,br,ry+S(20)};
                DrawTextW(dc,g,-1,&gr,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
                ry+=S(20);
            };
            auto row=[&](const wchar_t* k,long long val,COLORREF vc){
                SelectObject(dc,g_fLabel); SetTextColor(dc,g_theme.labelInk);
                RECT kr={bl,ry,br,ry+S(20)};
                DrawTextW(dc,k,-1,&kr,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
                SetTextColor(dc,vc);
                std::wstring vs=toFaDigits(formatMoney(val));
                DrawTextW(dc,vs.c_str(),-1,&kr,
                    DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
                ry+=S(20);
            };
            grp(L"بیمه اصلی");
            row(L"جمع کل",t->total,g_theme.text);
            row(L"سهم بیمار",t->patientShare,g_theme.text);
            row(L"سهم سازمان",t->mainShare,g_theme.text);
            grp(L"بیمه مکمل");
            row(L"جمع کل",t->total,g_theme.text);
            row(L"مابه‌التفاوت پایه",t->baseDiff,g_theme.text);
            row(L"سهم سازمان",t->orgShare,g_theme.text);
            grp(L"مبلغ نهایی");
            row(L"جمع کل",t->total,g_theme.text);
            row(L"تخفیف",0,g_theme.text);
            row(L"پرداختی",t->paid,g_theme.accent);
            //  «مانده قابل پرداخت» — v1.31.0 (script item #11): NO blue chip
            //  background. It is now a normal summary row on white, separated by
            //  a subtle light-gray divider above it, with a soft light-gray fill
            //  only (readable dark text). The prominent blue is reserved for the
            //  «جمع مبلغ نهایی» total card below.
            { // v1.63.0: a fading rule (strong at the RTL edge) + a soft raised
              // plate for the balance row, instead of a hard pen line and a flat
              // grey block.
              fadeRule(bl,br,ry+S(1));
              RECT hi={bl-S(4),ry+S(3),br+S(4),ry+S(26)};
              gpGradRoundRect(dc,hi,S(7),
                  blendColor(g_theme.surfaceTop,g_theme.surface2,40),
                  g_theme.surface2,
                  blendColor(g_theme.border,g_theme.accent,26));
              SelectObject(dc,g_fLabel); SetTextColor(dc,g_theme.text);
              RECT kr={bl+S(4),ry+S(3),br-S(4),ry+S(26)};
              DrawTextW(dc,L"مانده قابل پرداخت",-1,&kr,
                  DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
              SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
              std::wstring vs=toFaDigits(formatMoney(t->paid));
              DrawTextW(dc,vs.c_str(),-1,&kr,
                  DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX); }

            // ----- 2) «جمع مبلغ نهایی» blue-gradient total card (S(62)) -----
            int faTop = sumBot + S(10);
            int faBot = faTop + S(62);
            RECT fa={m.billL,Y(faTop),m.billR,Y(faBot)};
            gpShadow(dc,fa,S(12),S(6),42);
            gpGradRoundRectBgH(dc,fa,S(12),g_theme.accent,g_theme.accent2,
                               CLR_INVALID,g_theme.bg);
            int fl=fa.left+S(12), fr=fa.right-S(12);
            int wico=S(22);
            RECT wi={fr-wico,fa.top+(fa.bottom-fa.top-wico)/2,fr,
                     fa.top+(fa.bottom-fa.top-wico)/2+wico};
            drawIcon(dc,ICO_WALLET,wi,RGB(255,255,255),S(2));
            SelectObject(dc,g_fSmall);
            SetTextColor(dc,RGB(0xDD,0xEC,0xFF));
            RECT ft={fl,fa.top+S(8),fr-wico-S(8),fa.top+S(26)};
            DrawTextW(dc,L"جمع مبلغ نهایی",-1,&ft,
                DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            SelectObject(dc,g_fUIB); SetTextColor(dc,RGB(255,255,255));
            std::wstring fv=toFaDigits(formatMoney(t->patientShare))+L" ریال";
            RECT fvr={fl,fa.top+S(28),fr-wico-S(8),fa.bottom-S(6)};
            DrawTextW(dc,fv.c_str(),-1,&fvr,
                DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

            // ----- 3) «چاپ» group title (buttons positioned by tabPageLayout) -
            int prTop = recPrintGroupTop() - sy;
            SelectObject(dc,g_fSection); SetTextColor(dc,g_theme.sectionInk);
            int icoW=S(18);
            RECT pi={br-icoW,prTop,br,prTop+S(18)};
            // v1.75.0: a compact accent medallion behind the print glyph matches
            // the other section headers (صورتحساب / بیمه / پزشک) so the «چاپ»
            // group reads as a designed section, not a bare label.
            RECT pm=pi; InflateRect(&pm,S(3),S(3));
            int pmr=(pm.bottom-pm.top)/2;
            gpFillAlpha(dc,pm,pmr,blendColor(g_theme.accent,g_theme.surface,50),g_dark?80:46);
            gpRoundRect(dc,pm,pmr,CLR_INVALID,blendColor(g_theme.border,g_theme.accent,45));
            drawIcon(dc,ICO_PRINT,pi,g_theme.accent,S(2));
            RECT pt={bl,prTop,br-icoW-S(8),prTop+S(18)};
            DrawTextW(dc,L"چاپ",-1,&pt,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        }
        #undef Y
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    case WM_DESTROY:
        // v1.33.0 deterministic teardown: release the embedded WebView2 host
        // (controller + core WebView) before the page window goes away. Null our
        // handle first so no stale reference can be reused.
        if(t && t->web){ HWND w0=t->web; t->web=0; WebAdmission_DestroyView(w0); }
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

// ============================================================== TAB STRIP ==
static HWND recWnd(){ return s_rd?FindWindowExW(g_hFrame,NULL,RC_CLASS,NULL):NULL; }
static void ensureTabVisible(HWND h, int vecIdx);   // v1.90: defined with tabRect

// v1.7.0: persist the user's preferred tab order. We store the sequence of the
// permanent tab KINDS (پذیرش / کارتابل) as a CSV in the settings
// file so a drag-reorder survives between runs. Reception (form) tabs the user
// opened ad-hoc tabs are not persisted (only the fixed kinds).
//  v1.60.0: the appointment kind (3) is gone — never persisted anymore.
static void saveTabOrder(){
    if(!s_rd) return;
    std::wstring csv;
    for(auto* t : s_rd->tabs){
        if(t->detached) continue;
        // only the singleton/permanent kinds define a stable order
        if(t->kind==TK_RECEPTION || t->kind==TK_PORTAL){
            if(!csv.empty()) csv += L",";
            wchar_t b[8]; swprintf(b,8,L"%d",t->kind); csv += b;
        }
    }
    if(!csv.empty()) setSetting(L"tab_order", csv);
}
// return the persisted kind order (e.g. {0,1}); empty if none saved.
//  v1.60.0: kind 3 (appointment) was REMOVED — silently drop it from any
//  persisted CSV written by an older build.
static std::vector<int> loadTabOrder(){
    std::vector<int> out;
    std::wstring csv=getSetting(L"tab_order",L"");
    size_t p=0;
    while(p<csv.size()){
        size_t e=csv.find(L',',p); if(e==std::wstring::npos) e=csv.size();
        std::wstring tok=csv.substr(p,e-p); p=e+1;
        if(!tok.empty()){ int k=_wtoi(tok.c_str()); if(k!=3) out.push_back(k); }
    }
    return out;
}

static void recLayoutTabs(HWND h){
    RECT rc; GetClientRect(h,&rc);
    if(!s_rd) return;
    if(s_rd->active>=0) ensureTabVisible(h, s_rd->active);
    for(size_t i=0;i<s_rd->tabs.size();i++){
        TabPage* t=s_rd->tabs[i];
        if(t->detached) continue;
        if((int)i==s_rd->active){
            MoveWindow(t->page,0,infoBarH()+tabBarH(),
                rc.right,rc.bottom-infoBarH()-tabBarH(),TRUE);
            ShowWindow(t->page,SW_SHOW);
        } else ShowWindow(t->page,SW_HIDE);
    }
    // v1.89.0: keep the message toast above the tab pages. Showing a page
    // with SW_SHOW raises it to the top of the Z order, which would sink a
    // live toast behind a full-screen page (invisible + unclickable for the
    // rest of its 9 s). Re-assert the toast on top after every relayout.
    if(HWND tw=FindWindowExW(h,NULL,AZ_TOAST_CLS,NULL))
        if(IsWindowVisible(tw)) BringWindowToTop(tw);
}
static void addTabKind(HWND h, int kind, const std::string& extra=""){
    if(!s_rd) return;
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc={0};
        wc.lpfnWndProc=tabPageProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.lpszClassName=TABPG_CLASS;
        RegisterClassW(&wc); reg=true;
    }
    TabPage* t=new TabPage();
    t->kind=kind;
    t->extraJson=extra;   // WM_CREATE reads TabPage* via lpCreateParams
    std::wstring dept=g_session.user.dept;
    if(kind==TK_PORTAL)         t->title=L"کارتابل";
    else if(kind==TK_EMPTY)     t->title=L"تب جدید";
    else if(kind==TK_TOOLS)     t->title=L"ابزارها";
    else if(kind==TK_CASHIER)   t->title=L"صندوق";
    else if(kind==TK_QUEUE)     t->title=L"صندوق نرفته‌ها و صف پذیرش";
    else if(kind==TK_RECEIPTS)  t->title=L"جستجوی قبض";
    else if(kind==TK_DASH)      t->title=L"داشبورد";
    else if(kind==TK_BLACKLIST) t->title=L"لیست سیاه بیماران";
    else if(kind==TK_SVREPORT) t->title=L"تفکیک خدمات";
    else                        t->title=L"پذیرش بیمار";
    RECT rc; GetClientRect(h,&rc);
    // Only the reception form scrolls (it has the long right-side form); the
    // painted portal / empty pages and the tools/cashier surfaces don't need
    // a native scrollbar.
    DWORD pgStyle = WS_CHILD|WS_CLIPCHILDREN;
    if(kind==TK_RECEPTION) pgStyle |= WS_VSCROLL;
    // v2.04: register the tab BEFORE CreateWindow. WV2_CreateView pumps the
    // UI thread; if tabs were still empty, recProc painted «هیچ پذیرشی باز نیست».
    s_rd->tabs.push_back(t);
    s_rd->active=(int)s_rd->tabs.size()-1;
    HWND pg=CreateWindowExW(0,TABPG_CLASS,L"",
        pgStyle,
        0,infoBarH()+tabBarH(),rc.right,rc.bottom-infoBarH()-tabBarH(),
        h,NULL,g_hInst,t);
    if(!pg){
        s_rd->tabs.pop_back();
        if(s_rd->active>=(int)s_rd->tabs.size())
            s_rd->active=(int)s_rd->tabs.size()-1;
        delete t; return;
    }
    ensureTabVisible(h, s_rd->active);
    recLayoutTabs(h);
    InvalidateRect(h,NULL,TRUE);
    if(t->kind==TK_RECEPTION && t->eFirst) SetFocus(t->eFirst);
    else SetFocus(h);
}
static void addTab(HWND h){ addTabKind(h, TK_RECEPTION); }
static void closeTab(TabPage* t){
    if(!s_rd) return;
    if(t->kind==TK_DASH) return;   // v1.89: the dashboard tab never closes
    HWND h=recWnd();
    for(size_t i=0;i<s_rd->tabs.size();i++){
        if(s_rd->tabs[i]==t){
            if(t->detached){
                HWND det=GetParent(t->page);
                DestroyWindow(t->page);
                SetWindowLongPtrW(det,GWLP_USERDATA,0);
                DestroyWindow(det);
            } else DestroyWindow(t->page);
            delete t;
            s_rd->tabs.erase(s_rd->tabs.begin()+i);
            if(s_rd->active>=(int)s_rd->tabs.size())
                s_rd->active=(int)s_rd->tabs.size()-1;
            break;
        }
    }
    if(h){ recLayoutTabs(h); InvalidateRect(h,NULL,TRUE); }
}

// tab geometry helpers — plain coords, tabs flow RIGHT → LEFT (RTL)
// v1.90.0: docked tabs shrink toward tabMinW(); if they still overflow, a
// first-visible index + left-side chevron pair scrolls the strip. Detached
// tabs never take a slot.
struct TabStripLayout {
    int  visW;      // width of each currently shown tab
    int  visN;      // how many tabs currently shown
    int  dockN;     // non-detached tab count
    int  off;       // clamped first-visible docked index
    bool overflow;
    int  chevW;     // left cluster reserved for chevrons (0 if none)
};
static int dockedCount(){
    if(!s_rd) return 0;
    int n=0;
    for(auto* t : s_rd->tabs) if(t && !t->detached) n++;
    return n;
}
static int vecToDocked(int vecIdx){
    if(!s_rd) return 0;
    int k=0;
    int n=(int)s_rd->tabs.size();
    if(vecIdx>n) vecIdx=n;
    for(int i=0;i<vecIdx;i++)
        if(s_rd->tabs[i] && !s_rd->tabs[i]->detached) k++;
    return k;
}
static TabStripLayout computeTabStrip(HWND h){
    TabStripLayout L={};
    L.visW = tabW();
    if(!s_rd) return L;
    RECT rc; GetClientRect(h,&rc);
    L.dockN = dockedCount();
    int avail = rc.right - S(10) - S(10);
    if(avail < S(80)) avail = S(80);
    int gap = tabGap();
    int n = L.dockN;
    int ideal = tabW();
    int minW = tabMinW();
    L.off = s_rd->tabOff;
    if(n<=0){ L.off=0; s_rd->tabOff=0; return L; }
    int need = n*ideal + (n>0? (n-1)*gap : 0);
    if(need <= avail){
        L.visW=ideal; L.visN=n; L.overflow=false; L.off=0;
        s_rd->tabOff=0;
        return L;
    }
    int w = n>0 ? (avail - (n-1)*gap)/n : ideal;
    if(w >= minW){
        L.visW=w; L.visN=n; L.overflow=false; L.off=0;
        s_rd->tabOff=0;
        return L;
    }
    L.overflow = true;
    L.chevW = S(32)*2 + S(6) + S(10);
    int avail2 = avail - L.chevW;
    if(avail2 < minW) avail2 = minW;
    L.visN = (avail2 + gap) / (minW + gap);
    if(L.visN < 1) L.visN = 1;
    if(L.visN > n) L.visN = n;
    L.visW = L.visN>0 ? (avail2 - (L.visN-1)*gap)/L.visN : minW;
    if(L.visW > ideal) L.visW = ideal;
    if(L.visW < minW) L.visW = minW;
    int maxOff = n - L.visN;
    if(maxOff < 0) maxOff = 0;
    if(L.off > maxOff) L.off = maxOff;
    if(L.off < 0) L.off = 0;
    s_rd->tabOff = L.off;
    return L;
}
static int visToVec(int visSlot, const TabStripLayout& L){
    if(!s_rd || visSlot<0) return -1;
    int target = L.off + visSlot;
    int k=0;
    for(size_t i=0;i<s_rd->tabs.size();i++){
        if(!s_rd->tabs[i] || s_rd->tabs[i]->detached) continue;
        if(k==target) return (int)i;
        k++;
    }
    return -1;
}
static RECT tabRectVis(HWND h, int visSlot, const TabStripLayout& L){
    RECT rc; GetClientRect(h,&rc);
    RECT r={0,0,0,0};
    r.right = rc.right - S(10) - visSlot*(L.visW + tabGap());
    r.left  = r.right - L.visW;
    r.top   = infoBarH()+S(3);
    r.bottom= infoBarH()+tabBarH()-S(2);
    return r;
}
static RECT tabRect(HWND h, int i){
    TabStripLayout L = computeTabStrip(h);
    if(!s_rd || i<0 || i>=(int)s_rd->tabs.size()) return RECT{0,0,0,0};
    if(!s_rd->tabs[i] || s_rd->tabs[i]->detached) return RECT{0,0,0,0};
    int visSlot = vecToDocked(i) - L.off;
    if(visSlot<0 || visSlot>=L.visN) return RECT{0,0,0,0};
    return tabRectVis(h, visSlot, L);
}
static RECT chevronRect(HWND h, int which){
    // which 1 = prev (right-pointing, decrease off), 2 = next (left-pointing)
    RECT rc; GetClientRect(h,&rc);
    RECT r={0,0,0,0};
    int bw=S(32), bh=tabBarH()-S(8);
    int y=infoBarH()+S(4);
    int x0=S(8);
    if(which==2){ r.left=x0; r.right=x0+bw; }
    else        { r.left=x0+bw+S(6); r.right=r.left+bw; }
    r.top=y; r.bottom=y+bh;
    return r;
}
static int hitChevron(HWND h, POINT pt){
    if(!s_rd) return 0;
    TabStripLayout L = computeTabStrip(h);
    if(!L.overflow) return 0;
    RECT n=chevronRect(h,2);
    RECT p=chevronRect(h,1);
    if(PtInRect(&n,pt)) return 2;
    if(PtInRect(&p,pt)) return 1;
    return 0;
}
static void ensureTabVisible(HWND h, int vecIdx){
    if(!s_rd) return;
    TabStripLayout L = computeTabStrip(h);
    if(!L.overflow){ s_rd->tabOff=0; return; }
    if(vecIdx<0 || vecIdx>=(int)s_rd->tabs.size()) return;
    int d = vecToDocked(vecIdx);
    if(d < L.off) s_rd->tabOff = d;
    else if(d >= L.off + L.visN) s_rd->tabOff = d - L.visN + 1;
    if(s_rd->tabOff<0) s_rd->tabOff=0;
}
static int hitTab(HWND h, POINT pt, int* part){
    // part: 0=body 1=close
    if(!s_rd) return -1;
    TabStripLayout L = computeTabStrip(h);
    for(int v=0; v<L.visN; v++){
        int i = visToVec(v, L);
        if(i<0) continue;
        RECT r=tabRectVis(h,v,L);
        if(PtInRect(&r,pt)){
            TabPage* t=s_rd->tabs[i];
            RECT cl={r.left+S(8),r.top+S(6),r.left+S(30),r.bottom-S(6)};
            if(t->kind!=TK_DASH && PtInRect(&cl,pt)) *part=1;
            else *part=0;
            return i;
        }
    }
    return -1;
}
// v1.7.0: compute the insertion slot for a drag at mouse-x. Tabs flow RTL
// (right→left): index 0 is right-most. We pick the slot whose horizontal
// centre is nearest to the cursor. Returns a value in [0, tabCount].
static int dropIndexForX(HWND h, int mouseX){
    if(!s_rd || s_rd->tabs.empty()) return 0;
    TabStripLayout L = computeTabStrip(h);
    for(int v=0; v<L.visN; v++){
        int i = visToVec(v, L);
        if(i<0) continue;
        RECT r=tabRectVis(h,v,L);
        int mid=(r.left+r.right)/2;
        // RTL: if the cursor is to the RIGHT of this tab's centre, it belongs
        // BEFORE it (smaller index sits further right).
        if(mouseX > mid) return i;
    }
    int last = L.visN>0 ? visToVec(L.visN-1, L) : -1;
    if(last<0) return (int)s_rd->tabs.size();
    return last+1;   // dropped past the left-most VISIBLE tab
}

// ------------------------------------------------------------- reception ---
static LRESULT CALLBACK recProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE:
        s_rd = new RecData();
        // v1.60.0: «پذیرش بیمار» / «تب جدید» live in the FRAME action bar
        // HEADER (main.cpp). The reception info-bar no longer owns any action
        // buttons, so the tab strip is clean and uncluttered.
        s_rd->bNewPat = NULL;
        s_rd->bNewTab = NULL;
        s_rd->bCalc   = NULL;   // calculator also in the frame header
        s_rd->lastUnseen = unseenMessageCount(g_session.user.username);
        SetTimer(h, 77, 5000, NULL);   // poll the cartable for new messages
        return 0;
    case WM_APP_THEME:
        InvalidateRect(h,NULL,TRUE);
        // v1.93: propagate theme change to all embedded HTML surfaces so
        // dark/neon themes apply everywhere, not just native C++ controls.
        {
            std::wstring tn=getSetting(L"theme",L"light");
            std::string ts=std::string("{\"theme\":\"")+
                (tn==L"dark"?"dark":tn==L"neon"?"neon":"light")+"\"}";
            for(auto* tp : s_rd->tabs)
                if(tp->web) WebAdmission_PushEventTo(tp->web,"theme.changed",ts);
        }
        return 0;
    case WM_TIMER:
        if(w==77 && s_rd){
            int n=unseenMessageCount(g_session.user.username);
            if(n>s_rd->lastUnseen){
                // new message → Windows notification sound + repaint cartable
                if(getSetting(L"notify",L"1")==L"1")
                    MessageBeep(MB_ICONASTERISK);
                InvalidateRect(h,NULL,FALSE);
                // repaint the cartable page itself if visible
                for(auto* tp : s_rd->tabs)
                    if(tp->kind==TK_PORTAL && tp->page) InvalidateRect(tp->page,NULL,FALSE);
                // v1.89: bottom-right toast for the newest unseen message
                {
                    auto msgs=loadMessages(g_session.user.username);
                    const KMsg* latest=NULL;
                    for(auto& mm:msgs) if(!mm.seen) latest=&mm;
                    if(latest) MsgToast_Show(h, *latest, n);
                }
            }
            // v1.89: keep the dashboard's envelope badge in sync (both ways)
            if(n!=s_rd->lastUnseen){
                std::string js = std::string("{\"n\":") + std::to_string(n) + "}";
                WebAdmission_PushEvent("dash.unread", js);
                // v1.93: tell the portal (cartable) HTML surface a new message
                // arrived so it can refresh its list without a manual reload.
                if(n>s_rd->lastUnseen)
                    WebAdmission_PushEvent("portal.changed","{\"unseen\":"+std::to_string(n)+"}");
            }
            s_rd->lastUnseen=n;
        }
        // v1.89.0: the message toast is a transient notification and must stay
        // above every tab page for its whole 9 s life. Any path that shows or
        // raises a page (tab switch, web-view re-creation, detach/reattach)
        // would otherwise sink it behind a full-screen page. The repaint also
        // keeps it fresh if a page painted over it (seen under Wine).
        if(HWND tw=FindWindowExW(h,NULL,AZ_TOAST_CLS,NULL))
            if(IsWindowVisible(tw)){
                BringWindowToTop(tw);
                InvalidateRect(tw,NULL,TRUE);
                UpdateWindow(tw);
            }
        return 0;
    case WM_NCDESTROY:
        KillTimer(h,77);
        if(s_rd){
            for(auto* t : s_rd->tabs){
                if(t->detached){
                    HWND det=GetParent(t->page);
                    DestroyWindow(t->page);
                    if(det){ SetWindowLongPtrW(det,GWLP_USERDATA,0); DestroyWindow(det); }
                }
                delete t;
            }
            delete s_rd; s_rd=NULL;
        }
        break;
    case WM_SIZE: {
        if(!s_rd) return 0;
        // v1.7.0: action buttons moved to the frame header — only the tab
        // strip needs re-laying out here.
        recLayoutTabs(h);
        return 0; }
    case WM_COMMAND: {
        int id=LOWORD(w);
        if(id==ID_RC_CALC) openCalculator(g_hFrame);
        else if(id==ID_RC_NEWTAB) addTabKind(h, TK_EMPTY);   // new-tab → empty
        else if(id==ID_RC_NEWPAT){
            // «پذیرش بیمار» — v1.89.0: routed through the shared helper (the
            // dashboard app icon + the ui.openTab verb use the same path, so
            // the reuse-empty-or-fresh behaviour can never drift apart).
            Reception_OpenAdmissionNew();
            return 0;
        }
        return 0; }
    case WM_MOUSEMOVE: {
        if(!s_rd) return 0;
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int chv=hitChevron(h,pt);
        if(chv!=s_rd->hotChevron){
            s_rd->hotChevron=chv;
            RECT bar={0,infoBarH(),S(2000),infoBarH()+tabBarH()};
            InvalidateRect(h,&bar,FALSE);
        }
        if(chv){
            SetCursor(LoadCursor(NULL,IDC_HAND));
            TRACKMOUSEEVENT te={sizeof(te),TME_LEAVE,h,0}; TrackMouseEvent(&te);
            return 0;
        }
        // v1.7.0: drag-reorder in progress (or arming) ----------------------
        if(s_rd->dragArmed){
            int dx=pt.x-s_rd->dragStart.x;
            if(!s_rd->dragging && (dx>S(6)||dx<-S(6)))
                s_rd->dragging=true;            // crossed the threshold
            if(s_rd->dragging){
                s_rd->dragX=pt.x;
                s_rd->dropIdx=dropIndexForX(h,pt.x);
                SetCursor(LoadCursor(NULL,IDC_SIZEWE));
                RECT bar={0,infoBarH(),S(2000),infoBarH()+tabBarH()};
                InvalidateRect(h,&bar,FALSE);
                return 0;
            }
        }
        int part=0, hit=hitTab(h,pt,&part);
        int hc = (part==1)?hit:-1;
        if(hit!=s_rd->hotTab || hc!=s_rd->hotClose){
            s_rd->hotTab=hit; s_rd->hotClose=hc;
            RECT bar={0,infoBarH(),S(2000),infoBarH()+tabBarH()};
            InvalidateRect(h,&bar,FALSE);
        }
        TRACKMOUSEEVENT t={sizeof(t),TME_LEAVE,h,0}; TrackMouseEvent(&t);
        return 0; }
    case WM_MOUSELEAVE:
        // don't clear hover while a drag (with capture) is active
        if(s_rd && !s_rd->dragging && (s_rd->hotTab!=-1 || s_rd->hotChevron)){
            s_rd->hotTab=s_rd->hotClose=-1;
            s_rd->hotChevron=0;
            RECT bar={0,infoBarH(),S(2000),infoBarH()+tabBarH()};
            InvalidateRect(h,&bar,FALSE);
        }
        return 0;
    case WM_CAPTURECHANGED:
        // capture was lost (e.g. alt-tab) — cancel any pending/active drag
        if(s_rd && (s_rd->dragArmed||s_rd->dragging)){
            s_rd->dragArmed=false; s_rd->dragging=false;
            s_rd->dragIdx=-1; s_rd->dropIdx=-1;
            RECT bar={0,infoBarH(),S(2000),infoBarH()+tabBarH()};
            InvalidateRect(h,&bar,FALSE);
        }
        return 0;
    case WM_LBUTTONDOWN: {
        if(!s_rd) return 0;
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int chv=hitChevron(h,pt);
        if(chv){
            TabStripLayout L=computeTabStrip(h);
            if(chv==1 && s_rd->tabOff>0) s_rd->tabOff--;
            else if(chv==2){
                int maxOff=L.dockN-L.visN;
                if(maxOff<0) maxOff=0;
                if(s_rd->tabOff<maxOff) s_rd->tabOff++;
            }
            RECT bar={0,infoBarH(),S(2000),infoBarH()+tabBarH()};
            InvalidateRect(h,&bar,FALSE);
            return 0;
        }
        int part=0, hit=hitTab(h,pt,&part);
        if(hit>=0 && hit<(int)s_rd->tabs.size()){
            TabPage* t=s_rd->tabs[hit];
            // v1.89: the DASHBOARD tab is permanent — it has no close control.
            // The cartable (کارتابل) is now CLOSABLE; while it carries an
            // unread badge the badge slot acts as «open + mark seen» instead.
            // v1.92: the کارتابل (TK_PORTAL) tab is always closable via its
            // close × — even when it carries an unread badge. The badge moved
            // to the title area so the close slot is never obscured.
            if(part==1 && t->kind==TK_DASH){ /* permanent — nothing to close */ }
            else if(part==1) closeTab(t);
            else {
                s_rd->active=hit; recLayoutTabs(h);
                if(t->kind==TK_PORTAL){
                    markMessagesSeen(g_session.user.username);
                    if(s_rd) s_rd->lastUnseen=0;
                    if(t->page) InvalidateRect(t->page,NULL,FALSE);
                }
                InvalidateRect(h,NULL,FALSE);
                // v1.7.0: arm a potential drag-reorder on the tab BODY. It only
                // becomes a real drag once the cursor moves past a threshold,
                // so a plain click still just activates the tab.
                s_rd->dragArmed=true; s_rd->dragging=false;
                s_rd->dragIdx=hit; s_rd->dragStart=pt; s_rd->dragX=pt.x;
                s_rd->dropIdx=hit;
                SetCapture(h);
            }
        }
        return 0; }
    case WM_LBUTTONUP: {
        if(!s_rd) return 0;
        bool wasDragging=s_rd->dragging;
        int from=s_rd->dragIdx, to=s_rd->dropIdx;
        if(GetCapture()==h) ReleaseCapture();
        s_rd->dragArmed=false; s_rd->dragging=false;
        if(wasDragging && from>=0 && from<(int)s_rd->tabs.size()){
            // RTL drop-index → list insertion. dropIndexForX returns the slot
            // the dragged tab should occupy; normalise when moving rightward.
            if(to<0) to=0; if(to>(int)s_rd->tabs.size()) to=(int)s_rd->tabs.size();
            int adj = (to>from)? to-1 : to;
            if(adj!=from && adj>=0 && adj<(int)s_rd->tabs.size()){
                TabPage* moved=s_rd->tabs[from];
                s_rd->tabs.erase(s_rd->tabs.begin()+from);
                s_rd->tabs.insert(s_rd->tabs.begin()+adj, moved);
                // keep the moved tab active and persist the new order
                s_rd->active=adj;
                saveTabOrder();
                recLayoutTabs(h);
            }
            s_rd->dragIdx=-1; s_rd->dropIdx=-1;
            InvalidateRect(h,NULL,FALSE);
        }
        return 0; }
    case WM_MOUSEWHEEL: {
        if(!s_rd) return 0;
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        ScreenToClient(h,&pt);
        if(pt.y<infoBarH() || pt.y>infoBarH()+tabBarH()) break;
        TabStripLayout L=computeTabStrip(h);
        if(!L.overflow) return 0;
        int delta=GET_WHEEL_DELTA_WPARAM(w);
        if(delta<0){
            int maxOff=L.dockN-L.visN;
            if(maxOff<0) maxOff=0;
            if(s_rd->tabOff<maxOff) s_rd->tabOff++;
        } else if(s_rd->tabOff>0) s_rd->tabOff--;
        RECT bar={0,infoBarH(),S(2000),infoBarH()+tabBarH()};
        InvalidateRect(h,&bar,FALSE);
        return 0; }
    case WM_KEYDOWN:
        if(w==VK_F8){ printLastReceipt(h); return 0; }
        break;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);

        // ------------------------------------------------------------------
        //  v1.63.0 RECEPTION CHROME REDESIGN.
        //  The bars were three flat FillRect() slabs separated by one 1-px
        //  GDI line — visually indistinguishable from a 1990s toolbar. They
        //  now render as layered surfaces: the info + tab strip is one soft
        //  vertical gradient sitting on the page, closed by a two-tone
        //  separator (a hairline border over a lighter highlight) which is
        //  what gives a bar an actual edge instead of a drawn line.
        // ------------------------------------------------------------------
        RECT bd={0,infoBarH()+tabBarH(),rc.right,rc.bottom};
        FillRect(dc,&bd,g_brBg);
        RECT strip={0,0,rc.right,infoBarH()+tabBarH()};
        gpGradRoundRect(dc,strip,0,
            blendColor(g_theme.surfaceTop,g_theme.surface2,35),
            g_theme.surface, CLR_INVALID);
        // two-tone closing edge: darker rule + 1 px light lip beneath it
        int sepY = infoBarH()+tabBarH()-1;
        gpLine(dc,0,sepY,rc.right,sepY,g_theme.border,1.0f,255);
        gpLine(dc,0,sepY+1,rc.right,sepY+1,
            blendColor(g_theme.bg,RGB(255,255,255),g_dark?8:60),1.0f,200);

        SetBkMode(dc,TRANSPARENT);
        // v1.10.0: the old "میز پذیرش بیمار" title row was removed — it was an
        // empty header that only pushed the form down and caused scrolling. The
        // logged-in person's name/role and the live clock already live in the
        // frame's Layer-1 header, so nothing is lost here.

        // ---- tabs ----
        if(s_rd){
            for(size_t i=0;i<s_rd->tabs.size();i++){
                TabPage* t=s_rd->tabs[i];
                if(t->detached) continue;
                RECT r=tabRect(h,(int)i);
                if(r.right<=r.left) continue;   // scrolled out of the strip
                bool act = ((int)i==s_rd->active);
                bool hov = ((int)i==s_rd->hotTab);
                // ------------------------------------------------------
                //  v1.75.0 TAB REDESIGN. The strip used to be plain text with a
                //  close + detach glyph; tabs were hard to tell apart and looked
                //  basic. Each tab now carries a distinct vector icon in a
                //  rounded medallion at the leading (right/RTL) edge — a solid
                //  accent medallion marks the ACTIVE tab, a quiet tint marks the
                //  rest — so the strip reads as a set of labelled, iconified
                //  tabs. The detach control is gone; tabs always stay in-app.
                //    * ACTIVE  — raised page card merging into the body below
                //      (browser-tab idiom) + a 3 px accent indicator on top,
                //    * HOVER   — lifts an inactive tab with a soft shadow and an
                //      accent-warmed hairline,
                //    * cartable keeps its glowing unread counter.
                // ------------------------------------------------------
                int trad = S(10);
                if(act){
                    gpShadowColor(dc,r,trad,S(7),70,g_theme.accent);
                    gpGradRoundRect(dc,r,trad,
                        blendColor(g_theme.bg,g_theme.surfaceTop,45),
                        g_theme.bg,
                        blendColor(g_theme.border,g_theme.accent,55));
                    // v1.87.0: frosted inner light rim inside the raised tab
                    { RECT itr=r; InflateRect(&itr,-S(1),-S(1));
                      gpRoundRect(dc, itr, trad-S(1)>2?trad-S(1):trad, CLR_INVALID,
                                  RGB(255,255,255), g_dark?26:85); }
                    // merge with the body: paint over the bottom 4 px
                    RECT mb={r.left+S(2),r.bottom-S(4),r.right-S(2),r.bottom+S(2)};
                    HBRUSH mbr=CreateSolidBrush(g_theme.bg);
                    FillRect(dc,&mb,mbr); DeleteObject(mbr);
                    // accent indicator across the top of the active tab —
                    // v1.87.0: gradialism sweep indigo→sky instead of a flat bar
                    RECT ind2={r.left+S(9),r.top+S(2),r.right-S(9),r.top+S(2)+S(3)};
                    gpGradRoundRectBgH(dc,ind2,S(2),g_theme.accent,g_theme.accent2,
                                       CLR_INVALID,CLR_INVALID);
                } else if(hov){
                    // v1.89: hover = a lifted frosted chip with a light inner rim
                    gpShadowColor(dc,r,trad,S(6),52,g_theme.accent);
                    gpGradRoundRect(dc,r,trad,
                        g_theme.surfaceTop,
                        blendColor(g_theme.hover,g_theme.surface,35),
                        blendColor(g_theme.border,g_theme.accent,45));
                    RECT itr=r; InflateRect(&itr,-S(1),-S(1));
                    gpRoundRect(dc,itr,trad>S(1)?trad-S(1):trad,CLR_INVALID,
                                RGB(255,255,255),g_dark?26:85);
                } else {
                    // v1.89: resting = a quiet frosted-glass chip (not flat grey)
                    gpGradRoundRect(dc,r,trad,
                        blendColor(g_theme.surfaceTop,RGB(255,255,255),35),
                        blendColor(g_theme.surface,g_theme.surface2,35),
                        blendColor(g_theme.border,g_theme.accent,16));
                    RECT itr=r; InflateRect(&itr,-S(1),-S(1));
                    gpRoundRect(dc,itr,trad>S(1)?trad-S(1):trad,CLR_INVALID,
                                RGB(255,255,255),g_dark?18:60);
                }
                // icon medallion at the leading (right/RTL) edge — solid accent
                // on the active tab, a quiet accent tint on the others.
                int ico = tabIconFor(t->kind);
                RECT ic={r.right-S(36),r.top+S(4),r.right-S(12),r.top+S(28)};
                int mrad=S(7);
                if(act){
                    gpGradRoundRect(dc,ic,mrad,g_theme.accent,g_theme.accent2,CLR_INVALID);
                } else if(hov){
                    gpGradRoundRect(dc,ic,mrad,
                        blendColor(g_theme.surface,g_theme.accent,30),
                        blendColor(g_theme.surface,g_theme.accent,46),
                        blendColor(g_theme.border,g_theme.accent,42));
                } else {
                    gpGradRoundRect(dc,ic,mrad,
                        blendColor(g_theme.surfaceTop,g_theme.surface,50),
                        g_theme.surface,
                        blendColor(g_theme.border,g_theme.accent,26));
                }
                RECT ici={ic.left+S(5),ic.top+S(5),ic.right-S(5),ic.bottom-S(5)};
                drawIcon(dc,ico,ici,
                    act?g_theme.accentText
                       : (hov?g_theme.accent:blendColor(g_theme.accent,g_theme.textDim,45)),
                    S(2));
                // title (right-aligned, RTL) fills the space between the close
                // slot and the icon medallion.
                SetTextColor(dc, act?g_theme.text:g_theme.textDim);
                SelectObject(dc, act?g_fUIB:g_fUI);
                RECT tr2={r.left+S(38),r.top,r.right-S(42),r.bottom};
                DrawTextW(dc,t->title.c_str(),-1,&tr2,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                if(t->kind==TK_DASH) continue;   // v1.89: permanent, no close slot
                // unseen badge on the cartable tab (sits in the close slot)
                if(t->kind==TK_PORTAL){
                    int n=unseenMessageCount(g_session.user.username);
                    if(n>0){
                        // v1.92: unread badge sits on the title area (left of
                        // the close slot) so the close × stays available even
                        // when there are unread messages. The user must always
                        // be able to close the کارتابل tab.
                        wchar_t nb[8]; swprintf(nb,8,L"%d",n);
                        std::wstring fa=toFaDigits(nb);
                        SelectObject(dc,g_fSmall);
                        SIZE sz={0,0};
                        GetTextExtentPoint32W(dc,fa.c_str(),(int)fa.size(),&sz);
                        int bw=sz.cx+S(14);
                        RECT bd={r.right-S(44)-bw, r.top+S(8),
                                 r.right-S(44), r.bottom-S(8)};
                        gpShadowColor(dc,bd,S(8),S(4),80,g_theme.danger);
                        gpGradRoundRect(dc,bd,S(8),
                            blendColor(g_theme.danger,RGB(255,255,255),24),
                            g_theme.danger,CLR_INVALID);
                        SetTextColor(dc,RGB(255,255,255)); SelectObject(dc,g_fSmall);
                        DrawTextW(dc,fa.c_str(),-1,&bd,
                            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
                        // fall through — close × is always painted below
                    }
                }
                // close ×  (v1.63.0: hover raises a soft glowing danger pill
                // instead of a flat red square)
                RECT cl={r.left+S(8),r.top+S(6),r.left+S(30),r.bottom-S(6)};
                bool hcl=((int)i==s_rd->hotClose);
                if(hcl){
                    gpShadowColor(dc,cl,S(7),S(5),90,g_theme.danger);
                    gpGradRoundRect(dc,cl,S(7),
                        blendColor(g_theme.danger,RGB(255,255,255),20),
                        g_theme.danger,CLR_INVALID);
                }
                RECT cli={cl.left+S(5),cl.top+S(5),cl.right-S(5),cl.bottom-S(5)};
                drawIcon(dc,ICO_X,cli,
                    hcl?RGB(255,255,255):g_theme.textDim,S(2));
            }
            // v1.90.0: overflow chevrons on the LEFT of the RTL strip
            {
                TabStripLayout L = computeTabStrip(h);
                if(L.overflow){
                    for(int which=1; which<=2; which++){
                        RECT cr=chevronRect(h,which);
                        bool hot = (s_rd->hotChevron==which);
                        bool en = (which==1) ? (L.off>0)
                                             : (L.off < L.dockN-L.visN);
                        gpGradRoundRect(dc,cr,S(8),
                            hot?blendColor(g_theme.surfaceTop,g_theme.accent,22)
                               :g_theme.surfaceTop,
                            hot?blendColor(g_theme.hover,g_theme.surface,30)
                               :g_theme.surface,
                            blendColor(g_theme.border,g_theme.accent,hot?50:22));
                        COLORREF icol = en
                            ? (hot?g_theme.accent:g_theme.sectionInk)
                            : g_theme.textDim;
                        int cx=(cr.left+cr.right)/2, cy=(cr.top+cr.bottom)/2;
                        int dx=S(5), dy=S(7);
                        HPEN pn=CreatePen(PS_SOLID, S(2)>1?S(2):2, icol);
                        HGDIOBJ op=SelectObject(dc,pn);
                        HGDIOBJ ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
                        if(which==2){ // next: point left (later tabs live left)
                            MoveToEx(dc,cx+dx,cy-dy,NULL);
                            LineTo(dc,cx-dx,cy); LineTo(dc,cx+dx,cy+dy);
                        } else {      // prev: point right
                            MoveToEx(dc,cx-dx,cy-dy,NULL);
                            LineTo(dc,cx+dx,cy); LineTo(dc,cx-dx,cy+dy);
                        }
                        SelectObject(dc,ob); SelectObject(dc,op); DeleteObject(pn);
                    }
                }
            }
            // v1.7.0: drag-reorder drop indicator — a bright accent bar at the
            // boundary where the tab will be inserted, plus a subtle highlight
            // of the tab being dragged.
            if(s_rd->dragging && s_rd->dragIdx>=0){
                int di=s_rd->dropIdx;
                if(di<0) di=0; if(di>(int)s_rd->tabs.size()) di=(int)s_rd->tabs.size();
                int barX;
                if(di>=(int)s_rd->tabs.size()){
                    RECT last=tabRect(h,(int)s_rd->tabs.size()-1);
                    barX=last.left-S(3);
                } else {
                    RECT r=tabRect(h,di);
                    barX=r.right+S(3);
                }
                int top=infoBarH()+S(4), bot=infoBarH()+tabBarH()-S(2);
                RECT ind={barX-S(2),top,barX+S(2),bot};
                // v1.63.0: the insertion caret glows so it is unmissable, and
                // the lifted tab gets a real elevation shadow.
                gpShadowColor(dc,ind,S(2),S(6),150,g_theme.accent);
                gpRoundRect(dc,ind,S(2),g_theme.accent,CLR_INVALID);
                RECT dr=tabRect(h,s_rd->dragIdx);
                gpShadowColor(dc,dr,S(10),S(11),110,g_theme.accent);
                gpGradRoundRect(dc,dr,S(10),
                    blendColor(g_theme.hover,g_theme.surfaceTop,40),
                    g_theme.hover,g_theme.accent);
            }
            if(s_rd->tabs.empty()){
                // ----------------------------------------------------------
                //  v1.63.0 EMPTY STATE. A single line of grey text floating in
                //  a void looked like a bug, not a state. It is now a proper
                //  empty-state card: a large tinted icon medallion, a headline
                //  and a dimmer hint line, centred as one block.
                // ----------------------------------------------------------
                int cy=(infoBarH()+tabBarH()+rc.bottom)/2;
                int med=S(96), gap=S(22), hl=S(34), sl=S(26);
                int blockH=med+gap+hl+S(6)+sl;
                int by=cy-blockH/2;
                RECT mr={rc.right/2-med/2,by,rc.right/2+med/2,by+med};
                gpFillAlpha(dc,mr,med/2,
                    blendColor(g_theme.accent,g_theme.bg,58),g_dark?70:52);
                gpRoundRect(dc,mr,med/2,CLR_INVALID,
                    blendColor(g_theme.border,g_theme.accent,40));
                int isz=(med*44)/100;
                RECT ir2={rc.right/2-isz/2,by+(med-isz)/2,
                          rc.right/2+isz/2,by+(med-isz)/2+isz};
                drawIcon(dc,ICO_USER,ir2,
                    blendColor(g_theme.accent,g_theme.text,15),S(2)+1);
                SelectObject(dc,g_fTitle);
                SetTextColor(dc,g_theme.text);
                RECT hr={S(20),by+med+gap,rc.right-S(20),by+med+gap+hl};
                DrawTextW(dc,L"در حال بارگذاری داشبورد…",-1,&hr,
                    DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
                SelectObject(dc,g_fUI);
                SetTextColor(dc,g_theme.textDim);
                RECT sr2={S(20),hr.bottom+S(6),rc.right-S(20),hr.bottom+S(6)+sl};
                DrawTextW(dc,
                    L"لطفاً چند لحظه صبر کنید",
                    -1,&sr2,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
            }
        }
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}
// v1.7.0: public routing used by the frame header (main.cpp) ----------------
HWND receptionWindow(){
    return FindWindowExW(g_hFrame,NULL,RC_CLASS,NULL);
}
void receptionAction(RecAction a){
    HWND rec=receptionWindow();
    if(!rec || !IsWindow(rec)) return;
    //  v1.79.0: permission gate — defence in depth on top of the hidden header
    //  button, so no shortcut/path can open an admission tab without the
    //  «پذیرش بیمار» access tick.
    if(a==RA_NEWPAT && !userHasPerm(g_session.user, L"admission")){
        MessageBoxW(g_hFrame,
            L"این حساب کاربری دسترسی «پذیرش بیمار» ندارد.\n"
            L"برای فعال‌سازی، در مدیریت ← تعریف حساب کاربری تیک دسترسی را بزنید.",
            L"عدم دسترسی", MB_OK|MB_ICONWARNING|MB_TOPMOST|MB_SETFOREGROUND);
        return;
    }
    //  v1.60.0: RA_APPOINTMENT no longer exists — only پذیرش بیمار / تب جدید.
    int id = a==RA_NEWTAB ? ID_RC_NEWTAB
           :                ID_RC_NEWPAT;
    SendMessageW(rec, WM_COMMAND, MAKEWPARAM(id,0), 0);
}

void receptionPrintAction(RecPrintAction a){
    if(!s_rd || s_rd->active<0 || s_rd->active>=(int)s_rd->tabs.size()) return;
    TabPage* t=s_rd->tabs[s_rd->active];
    if(a==RPA_LAST){ printLastReceipt(g_hFrame); return; }
    if(t && t->web){
        const char* kind=(a==RPA_INSURANCE)?"insurance":"rx";
        WebAdmission_PushEventTo(t->web,"native.print",std::string("{\"kind\":\"")+kind+"\"}");
        return;
    }
    if(!t) return;
    ReceptionRecord r;
    collect(t,r);
    printReceipt(r,a==RPA_INSURANCE?0:1,g_hFrame);
}

static void activateKind(HWND h, int kind){
    if(!s_rd || !h) return;
    for(size_t i=0;i<s_rd->tabs.size();i++){
        TabPage* t=s_rd->tabs[i];
        if(!t || t->detached || t->kind!=kind) continue;
        s_rd->active=(int)i;
        recLayoutTabs(h);
        InvalidateRect(h,NULL,TRUE);
        return;
    }
    addTabKind(h, kind);
}

void Reception_OpenTools(){
    HWND h=recWnd(); if(!h) return;
    activateKind(h, TK_TOOLS);
}
void Reception_OpenCashier(){
    if(!userHasPerm(g_session.user, L"cashier_view")) return;
    HWND h=recWnd(); if(!h) return;
    activateKind(h, TK_CASHIER);
}
void Reception_OpenAdmissionTicket(const std::string& ticketJson){
    HWND h=recWnd(); if(!h) return;
    addTabKind(h, TK_RECEPTION, ticketJson);
}
static void closeActiveKind(int kind){
    if(!s_rd) return;
    if(s_rd->active<0 || s_rd->active>=(int)s_rd->tabs.size()) return;
    TabPage* t=s_rd->tabs[s_rd->active];
    if(t && t->kind==kind) closeTab(t);
}
void Reception_OpenQueue(){
    HWND h=recWnd(); if(!h) return;
    activateKind(h, TK_QUEUE);
}
// v1.88.0: «جستجوی قبض» gets its own native tab — opening it from the tools
// grid spawns a tab, and its «بازگشت به ابزارها» closes that tab again, which
// naturally lands the user back on the still-open tools tab (state kept).
void Reception_OpenReceipts(){
    HWND h=recWnd(); if(!h) return;
    activateKind(h, TK_RECEIPTS);
}
// v1.94.0: «لیست سیاه بیماران» — opens the blacklist HTML surface as its own
// closable tab. Auto-creates the reception screen if it isn't up yet, then
// focuses (or spawns) the blacklist tab.
void Reception_OpenBlacklist(){
    HWND h=recWnd();
    if(!h) h=createReceptionScreen(g_hFrame);
    if(!h) return;
    activateKind(h, TK_BLACKLIST);
}
// v2.02: «تفکیک خدمات» — per-service breakdown report (own HTML tab).
void Reception_OpenSvReport(){
    HWND h=recWnd();
    if(!h) h=createReceptionScreen(g_hFrame);
    if(!h) return;
    activateKind(h, TK_SVREPORT);
}
void Reception_CloseSvReport(){ closeActiveKind(TK_SVREPORT); }
void Reception_CycleTab(){
    HWND h=recWnd(); if(!h || !s_rd || s_rd->tabs.empty()) return;
    int n=(int)s_rd->tabs.size();
    int start=s_rd->active;
    if(start<0) start=0;
    for(int k=1;k<=n;k++){
        int i=(start+k)%n;
        TabPage* t=s_rd->tabs[i];
        if(!t || t->detached) continue;
        s_rd->active=i;
        recLayoutTabs(h);
        InvalidateRect(h,NULL,TRUE);
        return;
    }
}
// v1.89.0: dashboard app-icon actions — portal / new empty tab / fresh
// admission (the «پذیرش بیمار» behaviour that used to live in the removed
// header action bar: reuse an EMPTY tab if focused, else open a fresh one).
void Reception_OpenPortal(){
    HWND h=recWnd(); if(!h) return;
    if(!userHasPerm(g_session.user, L"worklist")) return;   // v1.89: tick gate
    activateKind(h, TK_PORTAL);
}
void Reception_OpenNewTab(){
    HWND h=recWnd(); if(!h) return;
    addTabKind(h, TK_EMPTY);
}
void Reception_OpenAdmissionNew(){
    HWND h=recWnd(); if(!h) return;
    if(!userHasPerm(g_session.user, L"admission")) return;  // v1.89: tick gate
    if(s_rd && s_rd->active>=0 && s_rd->active<(int)s_rd->tabs.size()
       && s_rd->tabs[s_rd->active]->kind==TK_EMPTY)
        addTab(h);          // reuse the empty placeholder
    else
        addTab(h);          // fresh reception tab
}
void Reception_CloseTools(){ closeActiveKind(TK_TOOLS); }
void Reception_CloseCashier(){ closeActiveKind(TK_CASHIER); }
void Reception_CloseQueue(){ closeActiveKind(TK_QUEUE); }
void Reception_CloseReceipts(){ closeActiveKind(TK_RECEIPTS); }
void Reception_ClosePortal(){ closeActiveKind(TK_PORTAL); }
void Reception_ResetZoom(){
    // v2.02: zoom is session-scoped and defaults to 100% so the page fills
    // the C++ host. Each tab is its own browser instance — reset is per-tab.
    WebAdmission_PushEvent("reception.settings", "{\"zoom\":80}");
}

HWND createReceptionScreen(HWND frame){
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc={0};
        wc.lpfnWndProc=recProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.lpszClassName=RC_CLASS;
        RegisterClassW(&wc); reg=true;
    }
    RECT rc=frameContentRect();
    HWND h=CreateWindowExW(0,RC_CLASS,L"",
        WS_CHILD|WS_CLIPCHILDREN,
        rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,frame,NULL,g_hInst,NULL);
    // v1.89.0 — the reception account lands on the DASHBOARD (TK_DASH).
    // v2.04: create hidden, add the dash tab, THEN show — so the empty
    // «هیچ پذیرشی باز نیست» paint never flashes before the tab exists.
    addTabKind(h, TK_DASH);
    if(s_rd){
        s_rd->active = 0;
        recLayoutTabs(h);
    }
    ShowWindow(h, SW_SHOW);
    // v1.97: coming online → toast unseen کارتابل count (offline → login).
    {
        int n=unseenMessageCount(g_session.user.username);
        if(n>0){
            KMsg syn;
            syn.from=L"\u06a9\u0627\u0631\u062a\u0627\u0628\u0644";
            syn.text=L"\u0634\u0645\u0627 "+toFaDigits(std::to_wstring(n))+
                     L" \u067e\u06cc\u0627\u0645 \u062e\u0648\u0627\u0646\u062f\u0647\u200c\u0646\u0634\u062f\u0647 \u062f\u0627\u0631\u06cc\u062f";
            syn.type=KMSG_NORMAL;
            MsgToast_Show(h, syn, n);
        }
    }
#ifdef AZ_DEBUG_BUILD
    {   // headless screenshot helper: open a specific tab for inspection
        wchar_t dbg[32]={0};
        GetEnvironmentVariableW(L"AZ_DEBUG_TAB", dbg, 32);
        if(dbg[0]){
            if(!wcscmp(dbg,L"reception")) addTabKind(h, TK_RECEPTION);
            recLayoutTabs(h);
            InvalidateRect(h,NULL,TRUE);
            // optional initial scroll offset for screenshot verification
            wchar_t scr[16]={0};
            GetEnvironmentVariableW(L"AZ_DEBUG_SCROLL", scr, 16);
            if(scr[0] && s_rd && !s_rd->tabs.empty()){
                TabPage* t=s_rd->tabs.back();
                if(t && t->page){
                    t->scrollY=_wtoi(scr);
                    tabPageLayout(t->page,t);
                    InvalidateRect(t->page,NULL,TRUE);
                }
            }
        }
    }
#endif
    return h;
}
