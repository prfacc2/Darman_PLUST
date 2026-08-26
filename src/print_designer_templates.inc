// ============================================================================
//  print_designer_templates.inc — 30 built-in A4 reception templates
//  (v1.62.0 — FULL REWRITE: SERVICES-FIRST, TEN DISTINCT LAYOUT FAMILIES)
//
//  Included into print_designer.cpp. Every template is authored in A4 mm
//  coordinates (210×297).
//
//  WHAT CHANGED IN v1.62.0 (the whole point of this rewrite)
//  --------------------------------------------------------
//  * Previously ALL 30 builtins shared ONE cookie-cutter body, so the gallery
//    was 30 near-identical pages and several of them left almost no room for
//    the services table — the operator's #1 complaint («در ۳۰ طرح خدمات درست
//    وارد نمی‌شود / جا برای خدمات نیست / چند ردیف خدمت جا نمی‌شود»).
//  * Now there are TEN genuinely different layout families (classic, colour
//    band, invoice, sidebar, line-art, card, photo-ID, financial, compact
//    tear-off, dual-block), each with three variants → 30 distinct designs.
//  * EVERY single one of the 30 carries the dynamic services table
//    (PIT_SERVICES) bound to the live ReceptionRecord.services vector, and its
//    box height is COMPUTED from the safe space between patient details and
//    totals. The renderer paints only real runtime rows, leaving the rest blank.
//  * The table is a REAL TABLE (ruled grid, pinned header/row heights, RTL
//    column classification by caption) — never a stack of labels.
//  * Eight different, correctly-classified column presets are used across the
//    30 designs (4→7 columns: ردیف / نام خدمت / شرح خدمت / نوع خدمت / کد خدمت /
//    تعداد / مبلغ واحد / تخفیف / سهم بیمه / سهم بیمار / مبلغ کل), so a clinic
//    can pick the level of detail it wants and still get real data in it.
//
//  WHAT CHANGED IN v1.69.0 (per-variant visual distinctness)
//  --------------------------------------------------------
//  * The three variants inside each family used to share ONE cookie-cutter
//    meta + patient + totals + footer (only header / colour / column-preset
//    differed), so the gallery read as ten triplets of near-identical pages.
//    Now every variant also gets its OWN meta strip, patient block, totals
//    treatment and footer — so all 30 are fundamentally different in layout,
//    table framing and appearance, not colour swaps. Each family keeps its
//    signature (band / sidebar / card / photo / tear-off / dual-block …).
//  * Code-carrier variety: 21 designs carry one deterministic Code128
//    barcode (in four footer styles: right / centred / tinted-dual / mini),
//    9 carry NO code at all (the new footClean). One code per page remains
//    the rule (0 or 1, never 2 — the «بارکد دوبار» bug stays dead).
//  * Responsive: designs stay compact (modest fonts, 4.0–4.6 mm rows, all
//    items inside the A4 printable area) so the engine's proportional pscale
//    shrinks them gracefully onto A5/A6/receipt paper without clipping.
//
//  If the operator switches the paper size (A5/A6/receipt) the print engine
//  auto-scales the whole design proportionally (pscale), so nothing is
//  clipped. Seeded on first run; existing installs are migrated once.
// ============================================================================

// --- small builder helpers --------------------------------------------------
namespace {

static int g_tplItemId = 1;
static int nid(){ return g_tplItemId++; }

// A4 page geometry shared by all 30 designs.
static const double PG_W   = 210.0;
static const double PG_H   = 297.0;
static const double PG_M   = 12.0;                 // page margin
static const double PG_CW  = PG_W - 2*PG_M;        // 186 mm content width
static const double FOOT_Y = PG_H - 34.0;          // 263 mm — footer band top

static PrintItem mkLabel(double x,double y,double w,double h,
                         const std::wstring& text,double pt,bool bold,int align){
    PrintItem it; it.id=nid(); it.type=PIT_LABEL; it.x=x; it.y=y; it.w=w; it.h=h;
    it.text=text; it.fontPt=pt; it.bold=bold; it.align=align; return it;
}
static PrintItem mkLabelC(double x,double y,double w,double h,
                          const std::wstring& text,double pt,bool bold,int align,
                          unsigned colour){
    PrintItem it=mkLabel(x,y,w,h,text,pt,bold,align); it.textColor=colour; return it;
}
static PrintItem mkField(double x,double y,double w,double h,
                         const std::wstring& field,const std::wstring& prefix,
                         double pt,int align){
    PrintItem it; it.id=nid(); it.type=PIT_FIELD; it.x=x; it.y=y; it.w=w; it.h=h;
    it.field=field; it.prefix=prefix; it.fontPt=pt; it.align=align; return it;
}
static PrintItem mkFieldB(double x,double y,double w,double h,
                          const std::wstring& field,const std::wstring& prefix,
                          double pt,int align){
    PrintItem it=mkField(x,y,w,h,field,prefix,pt,align); it.bold=true; return it;
}
static PrintItem mkHLine(double x,double y,double w,double bw){
    PrintItem it; it.id=nid(); it.type=PIT_HLINE; it.x=x; it.y=y; it.w=w; it.h=0.2;
    it.borderWidth=bw; return it;
}
static PrintItem mkHLineC(double x,double y,double w,double bw,unsigned col){
    PrintItem it=mkHLine(x,y,w,bw); it.borderColor=col; return it;
}
static PrintItem mkVLine(double x,double y,double h,double bw){
    PrintItem it; it.id=nid(); it.type=PIT_VLINE; it.x=x; it.y=y; it.w=0.2; it.h=h;
    it.borderWidth=bw; return it;
}
static PrintItem mkRect(double x,double y,double w,double h,double bw,double corner){
    PrintItem it; it.id=nid(); it.type=PIT_RECT; it.x=x; it.y=y; it.w=w; it.h=h;
    it.borderWidth=bw; it.corner=corner; it.fillTransparent=true; return it;
}
static PrintItem mkRectC(double x,double y,double w,double h,double bw,double corner,
                         unsigned border){
    PrintItem it=mkRect(x,y,w,h,bw,corner); it.borderColor=border; return it;
}
static PrintItem mkBand(double x,double y,double w,double h,unsigned fill,double bw){
    PrintItem it; it.id=nid(); it.type=PIT_RECT; it.x=x; it.y=y; it.w=w; it.h=h;
    it.fillColor=fill; it.fillTransparent=false; it.borderWidth=bw;
    it.borderColor=fill; return it;
}
static PrintItem mkTint(double x,double y,double w,double h,unsigned fill,
                        unsigned border,double bw,double corner){
    PrintItem it; it.id=nid(); it.type=PIT_RECT; it.x=x; it.y=y; it.w=w; it.h=h;
    it.fillColor=fill; it.fillTransparent=false;
    it.borderColor=border; it.borderWidth=bw; it.corner=corner; return it;
}
static PrintItem mkFrame(double pw,double ph,double margin,unsigned col,double bw){
    PrintItem it; it.id=nid(); it.type=PIT_FRAME; it.is_frame=true;
    it.x=margin; it.y=margin; it.w=pw-2*margin; it.h=ph-2*margin;
    it.borderColor=col; it.borderWidth=bw; it.fillTransparent=true; return it;
}
static PrintItem mkLogo(double x,double y,double w,double h){
    PrintItem it; it.id=nid(); it.type=PIT_LOGO; it.x=x; it.y=y; it.w=w; it.h=h; return it;
}
static PrintItem mkQR(double x,double y,double s){
    PrintItem it; it.id=nid(); it.type=PIT_QR; it.x=x; it.y=y; it.w=s; it.h=s;
    it.field=L"receiptNo"; return it;
}
// REAL scannable 1-D barcode (Code128-B) bound to the deterministic,
// patient-linked {receiptbarcode} token — reprints are identical.
static PrintItem mkBarcode(double x,double y,double w,double h){
    PrintItem it; it.id=nid(); it.type=PIT_BARCODE; it.x=x; it.y=y; it.w=w; it.h=h;
    it.field=L"receiptbarcode";
    it.text=L"{\"sym\":\"code128\",\"hri\":true,\"quiet\":2}";
    it.fontPt=8.0; it.align=1; it.textColor=0x000000; return it;
}
static PrintItem mkPhoto(double x,double y,double w,double h){
    PrintItem it; it.id=nid(); it.type=PIT_PHOTO; it.x=x; it.y=y; it.w=w; it.h=h;
    it.borderWidth=0.3; return it;
}

// ---------------------------------------------------------------------------
//  THE CORE: dynamic services table CONNECTED to the live reception record.
//
//  The model JSON describes the header (cols / labels / widths); at print time
//  printer.cpp::pdDrawServices fills ONE data row per entered service from
//  ReceptionRecord.services — real نام/شرح/تعداد/مبلغ، never test text. The
//  caption of each column decides its meaning (pdSvcColOf), so a clinic may
//  reorder or delete columns in the designer and the data still lands right.
//
//  v1.67.0 — every preset contains the four authoritative columns required on
//  every receipt: نام خدمت | شرح خدمت | تعداد | مبلغ کل. Optional columns may
//  add row/code/category/unit/insurance detail but never replace those four.
// ---------------------------------------------------------------------------
enum SvcPreset {
    SVC3 = 0, SVC4_ROW, SVC4_CAT, SVC5, SVC5_CODE, SVC6_FIN, SVC6_INS, SVC7,
    SVC_PRESET_COUNT
};

static std::wstring svcModelJson(int preset){
    switch(preset){
    case SVC4_ROW:
        return L"{\"cols\":5,\"header\":true,\"widths\":[0.07,0.36,0.25,0.10,0.22],"
               L"\"labels\":[\"ردیف\",\"نام خدمت\",\"شرح خدمت\",\"تعداد\",\"مبلغ کل\"]}";
    case SVC4_CAT:
        return L"{\"cols\":5,\"header\":true,\"widths\":[0.32,0.18,0.22,0.09,0.19],"
               L"\"labels\":[\"نام خدمت\",\"نوع خدمت\",\"شرح خدمت\",\"تعداد\",\"مبلغ کل\"]}";
    case SVC5:
        return L"{\"cols\":5,\"header\":true,\"widths\":[0.07,0.36,0.25,0.10,0.22],"
               L"\"labels\":[\"ردیف\",\"نام خدمت\",\"شرح خدمت\",\"تعداد\",\"مبلغ کل\"]}";
    case SVC5_CODE:
        return L"{\"cols\":6,\"header\":true,\"widths\":[0.06,0.12,0.29,0.22,0.09,0.22],"
               L"\"labels\":[\"ردیف\",\"کد خدمت\",\"نام خدمت\",\"شرح خدمت\",\"تعداد\",\"مبلغ کل\"]}";
    case SVC6_FIN:
        return L"{\"cols\":7,\"header\":true,\"widths\":[0.05,0.27,0.20,0.08,0.14,0.11,0.15],"
               L"\"labels\":[\"ردیف\",\"نام خدمت\",\"شرح خدمت\",\"تعداد\",\"مبلغ واحد\","
               L"\"تخفیف\",\"مبلغ کل\"]}";
    case SVC6_INS:
        return L"{\"cols\":7,\"header\":true,\"widths\":[0.05,0.26,0.20,0.08,0.15,0.13,0.13],"
               L"\"labels\":[\"ردیف\",\"نام خدمت\",\"شرح خدمت\",\"تعداد\",\"مبلغ کل\","
               L"\"سهم بیمه\",\"سهم بیمار\"]}";
    case SVC7:
        return L"{\"cols\":7,\"header\":true,\"widths\":[0.05,0.11,0.27,0.20,0.08,0.14,0.15],"
               L"\"labels\":[\"ردیف\",\"کد خدمت\",\"نام خدمت\",\"شرح خدمت\",\"تعداد\","
               L"\"مبلغ واحد\",\"مبلغ کل\"]}";
    case SVC3:
    default:
        return L"{\"cols\":4,\"header\":true,\"widths\":[0.39,0.29,0.10,0.22],"
               L"\"labels\":[\"نام خدمت\",\"شرح خدمت\",\"تعداد\",\"مبلغ کل\"]}";
    }
}

// Build the live services table. `headFill` == 0 keeps a monochrome line-art
// header. rowH/headerH are the compact initial pitch; the print renderer grows
// individual rows for wrapped prose without filling the unused page area.
static PrintItem mkServices(double x,double y,double w,double h,double pt,
                            int preset,unsigned headFill,double bw,
                            double rowH,double headerH){
    PrintItem it; it.id=nid(); it.type=PIT_SERVICES; it.x=x; it.y=y; it.w=w; it.h=h;
    it.text=svcModelJson(preset);
    it.fontPt=pt; it.bold=false; it.align=1;
    it.borderColor=0x000000; it.borderWidth=bw;
    it.textColor=0x000000;
    if(headFill){ it.fillColor=headFill; it.fillTransparent=false; }
    else        { it.fillColor=0xFFFFFF; it.fillTransparent=true;  }
    it.padding=0.8;
    it.headerH=headerH; it.rowH=rowH;
    return it;
}

// ===========================================================================
//  v1.95.0 — THERMAL MEDICAL RECEIPT redesign.
//  All 30 builtins now share ONE 11-section medical-receipt structure
//  (clinic header → barcode → title → ruled info box → services table →
//  financial section → e-prescription → referral → reception/cashier →
//  ش.ص → print timestamp), drawn with thin BLACK lines only — no shadows,
//  gradients, rounded corners or coloured backgrounds. The TEN layout
//  families vary the VISUAL treatment (classic / gray band / dense invoice /
//  sidebar / line-art / shaded caption bars / patient photo / full financial /
//  compact tear-off / dual-block totals) while keeping the SAME data fields
//  in the SAME order. Authored in A4 mm; the print engine's pscale shrinks
//  them proportionally onto A5 / 58 mm / 80 mm thermal rolls.
//
//  One deterministic Code128 barcode per page (section 2) — never two.
// ===========================================================================

// --- localized captions for the medical receipt ----------------------------
#define FA_CLINIC    L"درمانگاه شبانه‌روزی درمان پلاس"
#define FA_SUBTITLE  L"سامانه پذیرش و مدیریت درمانگاه"
#define FA_PHONE     L"تلفن: "
#define FA_ADDR      L"نشانی: "
#define FA_APPT      L"تاریخ نوبت: "
#define FA_QUEUE     L"نوبت: "
#define FA_NID       L"کد ملی: "
#define FA_BARCODE   L"بارکد: "
#define FA_BASEINS   L"بیمه پایه: "
#define FA_SUPP      L"مکمل: "
#define FA_FULL      L"نام بیمار: "
#define FA_AGE       L"سن: "
#define FA_DOCTOR    L"دکتر: "
#define FA_DOCCODE   L"کد نظام پزشکی: "
#define FA_SPECCODE  L"کد تخصص: "
#define FA_SPECIALTY L"شرح تخصص: "
#define FA_PAID      L"پرداختی: "
#define FA_TOTAL     L"قیمت کل: "
#define FA_INSSHARE  L"سهم پایه: "
#define FA_SUPPPAY   L"سهم مکمل: "
#define FA_DISCOUNT  L"تخفیف از: "
#define FA_POS       L"POS: "
#define FA_CASH      L"نقد: "
#define FA_PATSHARE  L"سهم بیمار: "
#define FA_FINAL     L"مبلغ نهایی: "
#define FA_EPRESC    L"کد رهگیری نسخه الکترونیک: "
#define FA_REFERRAL  L"شماره معرف نسخه: "
#define FA_RECEPTION L"پذیرش: "
#define FA_CASHIER   L"صندوق: "
#define FA_SC        L"ش.ص: "
#define FA_PRINTTS   L"تاریخ چاپ: "
#define FA_SVCLIST   L"شرح خدمات"
#define FA_TEAR      L"— — — محل جدا کردن — — —"
#define FA_STUB      L"نسخهٔ بیمار"
#define FA_PAYLBL    L"پرداخت‌ها"
#define FA_SHARELBL  L"سهم بیمه"
#define FA_DOC_REC   L"قبض پزشک"
#define FA_INS_REC   L"رسید بیمه"
#define FA_DOC_BOTH  L"قبض و رسید پزشکی"
#define FA_RECEIPT   L"شماره قبض: "

// ===========================================================================
//  TplSpec — per-template style. v1.95.0: monochrome medical receipt, so the
//  brand accent is black, fills are white / light gray and borders are thin.
//  `svc` selects the services column preset; `frame` toggles a page border;
//  family/variant select the layout signature in renderReceipt().
// ===========================================================================
struct TplSpec {
    int      family;      // 0..9 layout family
    int      variant;     // 0..2
    int      svc;         // SvcPreset (services column set)
    unsigned accent;      // ink colour (black)
    unsigned tint;        // light fill (white / light gray)
    unsigned headFill;    // services header fill (0 = line-art)
    double   bw;          // thin border width (mm)
    double   rowH;        // compact row pitch (mm)
    bool     frame;       // page frame
};

static const TplSpec TPL[30] = {
  /* 01 */ {0,0, SVC3     , 0x000000, 0xFFFFFF, 0x000000, 0.30, 6.2, false},
  /* 02 */ {0,1, SVC4_ROW , 0x000000, 0xFFFFFF, 0xECECEC, 0.30, 6.2, false},
  /* 03 */ {0,2, SVC5     , 0x000000, 0xFFFFFF, 0x000000, 0.30, 6.2, false},
  /* 04 */ {1,0, SVC3     , 0x000000, 0xFFFFFF, 0xECECEC, 0.30, 6.0, false},
  /* 05 */ {1,1, SVC4_CAT , 0x000000, 0xFFFFFF, 0xECECEC, 0.30, 6.0, false},
  /* 06 */ {1,2, SVC5_CODE, 0x000000, 0xFFFFFF, 0xECECEC, 0.30, 6.0, false},
  /* 07 */ {2,0, SVC6_FIN , 0x000000, 0xFFFFFF, 0x000000, 0.30, 5.6, true },
  /* 08 */ {2,1, SVC6_INS , 0x000000, 0xFFFFFF, 0x000000, 0.30, 5.6, true },
  /* 09 */ {2,2, SVC7     , 0x000000, 0xFFFFFF, 0x000000, 0.30, 5.6, true },
  /* 10 */ {3,0, SVC3     , 0x000000, 0xFFFFFF, 0x000000, 0.30, 5.8, false},
  /* 11 */ {3,1, SVC4_ROW , 0x000000, 0xFFFFFF, 0x000000, 0.30, 5.8, false},
  /* 12 */ {3,2, SVC5     , 0x000000, 0xFFFFFF, 0x000000, 0.30, 5.8, false},
  /* 13 */ {4,0, SVC3     , 0x000000, 0xFFFFFF, 0x000000, 0.25, 6.0, false},
  /* 14 */ {4,1, SVC4_ROW , 0x000000, 0xFFFFFF, 0x000000, 0.25, 6.0, true },
  /* 15 */ {4,2, SVC5     , 0x000000, 0xFFFFFF, 0x000000, 0.25, 6.0, false},
  /* 16 */ {5,0, SVC3     , 0x000000, 0xECECEC, 0xECECEC, 0.30, 6.0, false},
  /* 17 */ {5,1, SVC4_CAT , 0x000000, 0xECECEC, 0xECECEC, 0.30, 6.0, false},
  /* 18 */ {5,2, SVC6_INS , 0x000000, 0xECECEC, 0xECECEC, 0.30, 6.0, false},
  /* 19 */ {6,0, SVC3     , 0x000000, 0xFFFFFF, 0x000000, 0.30, 6.0, false},
  /* 20 */ {6,1, SVC4_CAT , 0x000000, 0xFFFFFF, 0xECECEC, 0.30, 6.0, false},
  /* 21 */ {6,2, SVC5     , 0x000000, 0xFFFFFF, 0x000000, 0.30, 6.0, false},
  /* 22 */ {7,0, SVC6_FIN , 0x000000, 0xFFFFFF, 0xECECEC, 0.30, 5.6, false},
  /* 23 */ {7,1, SVC6_INS , 0x000000, 0xFFFFFF, 0xECECEC, 0.30, 5.6, false},
  /* 24 */ {7,2, SVC7     , 0x000000, 0xFFFFFF, 0x000000, 0.30, 5.6, true },
  /* 25 */ {8,0, SVC3     , 0x000000, 0xFFFFFF, 0x000000, 0.30, 5.2, false},
  /* 26 */ {8,1, SVC4_ROW , 0x000000, 0xFFFFFF, 0xECECEC, 0.30, 5.2, false},
  /* 27 */ {8,2, SVC5     , 0x000000, 0xFFFFFF, 0x000000, 0.30, 5.2, false},
  /* 28 */ {9,0, SVC3     , 0x000000, 0xFFFFFF, 0xECECEC, 0.30, 6.0, false},
  /* 29 */ {9,1, SVC6_FIN , 0x000000, 0xFFFFFF, 0xECECEC, 0.30, 6.0, false},
  /* 30 */ {9,2, SVC5_CODE, 0x000000, 0xFFFFFF, 0xECECEC, 0.30, 6.0, true },
};

// v1.95.0 — the 30 Persian gallery names. The document title cycles by variant
// (قبض پزشک / رسید بیمه / قبض و رسید پزشکی) and the family signature follows.
static const wchar_t* const TPL_NAMES[30]={
    L"۰۱) قبض پزشک — کلاسیک",
    L"۰۲) رسید بیمه — کلاسیک باند خاکستری",
    L"۰۳) قبض و رسید — کلاسیک دوخطی",
    L"۰۴) قبض پزشک — باند سرتیتر",
    L"۰۵) رسید بیمه — باند سرتیتر",
    L"۰۶) قبض و رسید — باند سرتیتر",
    L"۰۷) قبض پزشک — فاکتور فشرده",
    L"۰۸) رسید بیمه — فاکتور فشرده",
    L"۰۹) قبض و رسید — فاکتور فشرده",
    L"۱۰) قبض پزشک — ستون کناری",
    L"۱۱) رسید بیمه — ستون کناری",
    L"۱۲) قبض و رسید — ستون کناری",
    L"۱۳) قبض پزشک — خط‌کشی تک‌رنگ",
    L"۱۴) رسید بیمه — خط‌کشی قاب‌دار",
    L"۱۵) قبض و رسید — خط‌کشی",
    L"۱۶) قبض پزشک — کارتی سایه‌دار",
    L"۱۷) رسید بیمه — کارتی سایه‌دار",
    L"۱۸) قبض و رسید — کارتی سایه‌دار",
    L"۱۹) قبض پزشک — عکس بیمار",
    L"۲۰) رسید بیمه — عکس بیمار",
    L"۲۱) قبض و رسید — عکس بیمار",
    L"۲۲) قبض پزشک — مالی کامل",
    L"۲۳) رسید بیمه — مالی کامل",
    L"۲۴) قبض و رسید — مالی کامل قاب‌دار",
    L"۲۵) قبض پزشک — فشرده ته‌برگ",
    L"۲۶) رسید بیمه — فشرده ته‌برگ",
    L"۲۷) قبض و رسید — فشرده ته‌برگ",
    L"۲۸) قبض پزشک — دو بلوکی",
    L"۲۹) رسید بیمه — دو بلوکی",
    L"۳۰) قبض و رسید — دو بلوکی قاب‌دار",
};

// --- medical receipt geometry (8 mm margins, portrait A4) ------------------
static const double R_M   = 8.0;
static const double R_CW  = PG_W - 2*R_M;            // 194 mm content width
static const double FR_M  = 5.0;                     // page-frame margin (outside box)
static const unsigned R_INK   = 0x000000;            // black ink
static const unsigned R_SHADE = 0xECECEC;            // very light gray (monochrome)

// push a labelled data field into a cell; align 0 = right (RTL cell start)
static inline void putF(std::vector<PrintItem>& v,double x,double y,double w,double h,
                        const wchar_t* field,const wchar_t* prefix,double pt,
                        bool bold,int align=0){
    v.push_back(bold? mkFieldB(x,y,w,h,field,prefix,pt,align)
                    : mkField (x,y,w,h,field,prefix,pt,align));
}

// ---------------------------------------------------------------------------
//  SHARED MEDICAL-RECEIPT SECTION BUILDERS
//  Each appends to the item vector and returns the next free y (mm). The whole
//  receipt body sits inside ONE bordered box (drawn last by renderReceipt) so
//  every internal rule meets a continuous thin outline.
// ---------------------------------------------------------------------------

// Section 1 — clinic header (centered name + address + phone).
// hdrMode: 0 plain, 1 gray band behind the name, 2 double rule below.
// photo: a small patient-photo box tucked into the top-right corner.
static double medHeader(std::vector<PrintItem>& v,double x,double y,double w,
                        double pt,int hdrMode,bool photo){
    double pw=15.0, ph=18.0, tw=w;
    if(photo) tw=w-pw-3.0;
    double yy=y+0.8, namePt=pt+3.0;
    if(hdrMode==1 && !photo){
        v.push_back(mkBand(x, y+0.4, w, 7.0, R_SHADE, 0));
        v.push_back(mkLabel(x+1, y+0.7, w-2, 6.2, FA_CLINIC, namePt, true, 1));
        yy=y+0.4+7.0+0.6;
    } else {
        v.push_back(mkLabel(x, yy, tw, 6.4, FA_CLINIC, namePt, true, 1)); yy+=7.0;
    }
    v.push_back(mkField(x, yy, tw, 4.4, L"clinicaddr",  FA_ADDR,  pt-2.0, 1)); yy+=5.0;
    v.push_back(mkField(x, yy, tw, 4.2, L"clinicphone", FA_PHONE, pt-2.0, 1));
    if(photo) v.push_back(mkPhoto(x+w-pw, y+0.8, pw, ph));
    double bottom = y + (photo?19.0:18.0);
    v.push_back(mkHLine(x, bottom, w, 0.3));
    if(hdrMode==2 && !photo){ v.push_back(mkHLine(x, bottom+1.3, w, 0.3)); bottom+=1.3; }
    return bottom;
}

// Section 2 — wide, short barcode (centered) + divider rule.
static double medBarcode(std::vector<PrintItem>& v,double x,double y,double w,double){
    double bcW=w*0.82, bcH=11.0;
    v.push_back(mkBarcode(x+(w-bcW)/2.0, y+0.5, bcW, bcH));
    double bottom = y + bcH + 1.5;
    v.push_back(mkHLine(x, bottom, w, 0.3));
    return bottom;
}

// Section 3 — document title (bold, centered) + divider rule.
static double medTitle(std::vector<PrintItem>& v,double x,double y,double w,
                       double pt,const std::wstring& title){
    v.push_back(mkLabel(x, y+0.4, w, 6.0, title, pt+2.0, true, 1));
    double bottom = y + 7.5;
    v.push_back(mkHLine(x, bottom, w, 0.3));
    return bottom;
}

// Section 4 — the ruled info box: 6 rows × 2 columns, split by a center
// vertical line and separated by horizontal rules. The last row leaves its
// bottom rule to the services table's top border. Right cell = first (right-
// most) field; rows a & c carry two concatenated fields in one cell.
static double medInfo(std::vector<PrintItem>& v,double x,double y,double w,
                      double rH,double pt,double bw){
    double half=w/2.0, vx=x+half, fh=rH-0.6;
    double RR=x+w-1.0, LR=x+half-1.0, cy=y;
    auto vline=[&](){ v.push_back(mkVLine(vx, cy, rH, bw)); };
    auto rule =[&](){ cy+=rH; v.push_back(mkHLine(x, cy, w, bw)); };
    auto r1=[&](const wchar_t* f,const wchar_t* p,bool b){
        putF(v, x+half+1.0, cy+0.3, half-2.0, fh, f, p, pt, b, 0); };
    auto l1=[&](const wchar_t* f,const wchar_t* p,bool b){
        putF(v, x+1.0, cy+0.3, half-2.0, fh, f, p, pt, b, 0); };

    // a: تاریخ نوبت: {apptdate} {appttime}  |  نوبت: {queue}
    vline();
    { double sw=half*0.50, pw2=half*0.28, g=1.0;
      putF(v, RR-sw,       cy+0.3, sw,  fh, L"apptdate", FA_APPT, pt, true,  0);
      putF(v, RR-sw-g-pw2, cy+0.3, pw2, fh, L"appttime", L"",     pt, false, 0);
      l1(L"queue", FA_QUEUE, false); }
    rule();
    // b: کد ملی: {nid}  |  بارکد: {barcode}
    vline(); r1(L"nid", FA_NID, true); l1(L"barcode", FA_BARCODE, false); rule();
    // c: بیمه پایه: {ins}  |  مکمل: {supp} {supp_percent}
    vline(); r1(L"ins", FA_BASEINS, false);
    { double sw=half*0.52, pw2=half*0.30, g=1.0;
      putF(v, LR-sw,       cy+0.3, sw,  fh, L"supp",         FA_SUPP, pt, false, 0);
      putF(v, LR-sw-g-pw2, cy+0.3, pw2, fh, L"supp_percent", L"",     pt, false, 0); }
    rule();
    // d: نام بیمار: {full}  |  سن: {age}
    vline(); r1(L"full", FA_FULL, true); l1(L"age", FA_AGE, false); rule();
    // e: دکتر: {doctor}  |  کد نظام پزشکی: {doctorcode}
    vline(); r1(L"doctor", FA_DOCTOR, false); l1(L"doctorcode", FA_DOCCODE, false); rule();
    // f: کد تخصص: {specialtycode}  |  شرح تخصص: {specialty}   (no bottom rule)
    vline(); r1(L"specialtycode", FA_SPECCODE, false); l1(L"specialty", FA_SPECIALTY, false);
    cy+=rH;
    return cy;
}
// Section 6 — financial section. mode 0: 4 rows (a 2col, b 2col, c 1col,
// d 2col); mode 1: full (adds سهم بیمار / مبلغ نهایی / سهم بیمه پایه);
// mode 2: dual-block — two side-by-side sub-boxes with gray caption strips.
// Ends with a full-width rule that divides financial from e-prescription.
static double medFinancial(std::vector<PrintItem>& v,double x,double y,double w,
                           double rH,double pt,double bw,int mode){
    double half=w/2.0, vx=x+half, fh=rH-0.6, cy=y;
    if(mode==2){
        double gap=3.0, bw2=(w-gap)/2.0, lx=x, rx=x+bw2+gap, capH=5.0;
        double iy=y+capH;
        v.push_back(mkBand(lx, y, bw2, capH, R_SHADE, 0));
        v.push_back(mkLabel(lx+1, y+0.4, bw2-2, capH-0.6, FA_PAYLBL,   pt-0.5, true, 1));
        v.push_back(mkBand(rx, y, bw2, capH, R_SHADE, 0));
        v.push_back(mkLabel(rx+1, y+0.4, bw2-2, capH-0.6, FA_SHARELBL, pt-0.5, true, 1));
        // left block: paid / total / discount
        putF(v, lx+1, iy+0.3,      bw2-2, fh, L"paid",     FA_PAID,     pt, true,  0);
        v.push_back(mkHLine(lx, iy+rH, bw2, bw));
        putF(v, lx+1, iy+rH+0.3,   bw2-2, fh, L"total",    FA_TOTAL,    pt, false, 0);
        v.push_back(mkHLine(lx, iy+2*rH, bw2, bw));
        putF(v, lx+1, iy+2*rH+0.3, bw2-2, fh, L"discount", FA_DISCOUNT, pt, false, 0);
        // right block: insshare / supppay / pos / cash
        putF(v, rx+1, iy+0.3,      bw2-2, fh, L"insshare", FA_INSSHARE, pt, false, 0);
        v.push_back(mkHLine(rx, iy+rH, bw2, bw));
        putF(v, rx+1, iy+rH+0.3,   bw2-2, fh, L"supppay",  FA_SUPPPAY,  pt, false, 0);
        v.push_back(mkHLine(rx, iy+2*rH, bw2, bw));
        putF(v, rx+1, iy+2*rH+0.3, bw2-2, fh, L"pos",      FA_POS,      pt, false, 0);
        v.push_back(mkHLine(rx, iy+3*rH, bw2, bw));
        putF(v, rx+1, iy+3*rH+0.3, bw2-2, fh, L"cash",     FA_CASH,     pt, true,  0);
        double bottom = y + capH + 4*rH;
        v.push_back(mkRect(lx, y, bw2, capH+4*rH, bw, 0));
        v.push_back(mkRect(rx, y, bw2, capH+4*rH, bw, 0));
        v.push_back(mkHLine(x, bottom, w, bw));          // fin/epres divider
        return bottom;
    }
    auto vline=[&](){ v.push_back(mkVLine(vx, cy, rH, bw)); };
    auto rule =[&](){ cy+=rH; v.push_back(mkHLine(x, cy, w, bw)); };
    auto r1=[&](const wchar_t* f,const wchar_t* p,bool b){
        putF(v, x+half+1.0, cy+0.3, half-2.0, fh, f, p, pt, b, 0); };
    auto l1=[&](const wchar_t* f,const wchar_t* p,bool b){
        putF(v, x+1.0, cy+0.3, half-2.0, fh, f, p, pt, b, 0); };
    auto row2=[&](const wchar_t* f1,const wchar_t* p1,
                  const wchar_t* f2,const wchar_t* p2,bool b1,bool b2){
        vline(); r1(f1,p1,b1); l1(f2,p2,b2); rule(); };
    auto row1=[&](const wchar_t* f,const wchar_t* p,bool b){
        putF(v, x+1.0, cy+0.3, w-2.0, fh, f, p, pt, b, 0); rule(); };
    row2(L"paid",     FA_PAID,     L"total",    FA_TOTAL,    true,  false);  // a
    row2(L"insshare", FA_INSSHARE, L"supppay",  FA_SUPPPAY,  false, false);  // b
    row1(L"discount", FA_DISCOUNT, false);                                  // c (full width)
    row2(L"pos",      FA_POS,      L"cash",     FA_CASH,     false, false);  // d
    if(mode==1){
        row2(L"patientshare", FA_PATSHARE, L"finaltotal", FA_FINAL, false, true); // e
        row1(L"basepay",      L"سهم بیمه پایه: ", false);                          // f
    }
    return cy;
}

// Sections 7 & 8 — e-prescription + referral (full-width ruled rows, the last
// one leaving its bottom rule to the outer box border).
static double medEpresReferral(std::vector<PrintItem>& v,double x,double y,double w,
                               double rH,double pt,double bw){
    double fh=rH-0.6, cy=y;
    putF(v, x+1.0, cy+0.3, w-2.0, fh, L"eprescription", FA_EPRESC, pt, false, 0);
    cy+=rH; v.push_back(mkHLine(x, cy, w, bw));
    putF(v, x+1.0, cy+0.3, w-2.0, fh, L"referralno",    FA_REFERRAL, pt, false, 0);
    cy+=rH;
    return cy;
}

// Sections 9 / 10 / 11 — below the box: پذیرش | صندوق , ش.ص , print timestamp.
static double medBelow(std::vector<PrintItem>& v,double x,double y,double w,double pt){
    double fh=5.5, gap=2.2, half=w/2.0;
    v.push_back(mkHLine(x, y, w, 0.25));
    putF(v, x+half+1.0, y+1.0, half-2.0, fh, L"receptionist", FA_RECEPTION, pt-1.0, false, 0);
    putF(v, x+1.0,      y+1.0, half-2.0, fh, L"cashier",      FA_CASHIER,   pt-1.0, false, 0);
    y += fh + gap;
    putF(v, x+1.0, y, w-2.0, fh, L"scnum", FA_SC, pt-1.0, false, 0);
    y += fh + gap;
    { double sw=w*0.30, pw2=w*0.22, g=1.0;
      putF(v, x+w-1.0-sw,        y, sw,  fh, L"date", FA_PRINTTS,    pt-1.0, false, 0);
      putF(v, x+w-1.0-sw-g-pw2,  y, pw2, fh, L"time", L" ساعت: ",    pt-1.0, false, 0); }
    y += fh + gap;
    return y;
}

// Family 8 — compact tear-off: a dotted rule + a mini «نسخهٔ بیمار» stub
// (name / receipt / total / insshare / paid). No second barcode: the main
// receipt already carries the one deterministic Code128 in section 2.
static double medStub(std::vector<PrintItem>& v,double x,double y,double w,double pt){
    v.push_back(mkHLine(x, y, w, 0.3));
    v.push_back(mkLabel(x, y+0.6, w, 4, FA_TEAR, pt-1.0, false, 1));
    y += 6.0;
    v.push_back(mkLabelC(x, y, w, 6, FA_STUB, pt+1.0, true, 1, R_INK));
    y += 7.0;
    double rH=5.0, fh=4.4, half=w/2.0;
    v.push_back(mkRect(x, y, w, 3*rH, 0.3, 0));
    v.push_back(mkVLine(x+half, y, 3*rH, 0.3));
    putF(v, x+half+1.0, y+0.3,      half-2.0, fh, L"full",      FA_FULL,      pt-1.0, true,  0);
    putF(v, x+1.0,      y+0.3,      half-2.0, fh, L"receiptNo", FA_RECEIPT,  pt-1.0, false, 0);
    v.push_back(mkHLine(x, y+rH, w, 0.3));
    putF(v, x+half+1.0, y+rH+0.3,   half-2.0, fh, L"total",     FA_TOTAL,     pt-1.0, false, 0);
    putF(v, x+1.0,      y+rH+0.3,   half-2.0, fh, L"insshare",  FA_INSSHARE,  pt-1.0, false, 0);
    v.push_back(mkHLine(x, y+2*rH, w, 0.3));
    v.push_back(mkFieldB(x+1.0, y+2*rH+0.3, w-2.0, fh, L"paid", FA_FINAL, pt, 0));
    y += 3*rH + 2.0;
    v.push_back(mkHLine(x+w*0.55, y, w*0.45, 0.3));
    v.push_back(mkLabel(x+w*0.55, y+0.4, w*0.45, 4, L"امضا و مهر صندوق", pt-2.0, false, 1));
    return y + 6.0;
}

// Family 3 — sidebar: a narrow left panel (logo + clinic identity + barcode)
// and a main column carrying the title + the ruled receipt box + below lines.
static void renderSidebar(PrintDesign& d,const TplSpec& sp,double pt,double rH,
                          double bw,double svcH,const std::wstring& title){
    std::vector<PrintItem>& v=d.items;
    double sbW=42.0, sbX=R_M, mainX=sbX+sbW+4.0, mainW=R_CW-sbW-4.0;
    if(sp.frame) d.items.push_back(mkFrame(PG_W,PG_H,FR_M,R_INK,0.3));
    // sidebar panel: sections 1 (clinic header) + 2 (barcode)
    double sbTop=R_M, sy=sbTop+1.2;
    v.push_back(mkLogo(sbX+(sbW-22.0)/2.0, sy, 22.0, 22.0)); sy+=24.0;
    v.push_back(mkLabel(sbX+1.0, sy, sbW-2.0, 8.0, FA_CLINIC, pt+1.0, true, 1)); sy+=9.0;
    v.push_back(mkField(sbX+1.0, sy, sbW-2.0, 10.0, L"clinicaddr",  FA_ADDR,  pt-2.0, 1)); sy+=11.0;
    v.push_back(mkField(sbX+1.0, sy, sbW-2.0, 5.0,  L"clinicphone", FA_PHONE, pt-2.0, 1)); sy+=6.0;
    v.push_back(mkHLine(sbX+1.0, sy, sbW-2.0, 0.3)); sy+=3.0;
    double bcH=12.0;
    v.push_back(mkBarcode(sbX+2.0, sy, sbW-4.0, bcH));
    double sbBottom=sy+bcH+2.0;
    v.push_back(mkRect(sbX, sbTop, sbW, sbBottom-sbTop, bw, 0));
    // main column: section 3 title + sections 4..8 box + sections 9..11
    double mx=mainX, mw=mainW, boxTop=R_M, y=boxTop+1.2;
    v.push_back(mkLabel(mx, y, mw, 6.0, title, pt+2.0, true, 1)); y+=7.5;
    v.push_back(mkHLine(mx, y, mw, 0.3)); y+=2.0;
    y=medInfo(v, mx, y, mw, rH, pt, bw);
    v.push_back(mkServices(mx, y, mw, svcH, pt-0.5, sp.svc, 0, bw, rH-1.0, rH-0.8));
    y+=svcH;
    y=medFinancial(v, mx, y, mw, rH, pt, bw, 0);
    y=medEpresReferral(v, mx, y, mw, rH, pt, bw);
    v.push_back(mkRect(mx, boxTop, mw, y-boxTop, bw, 0));
    medBelow(v, mx, y+2.0, mw, pt);
}

// ===========================================================================
//  renderReceipt — build one medical receipt for TPL[idx] (non-sidebar path).
// ===========================================================================
static void renderReceipt(PrintDesign& d, const TplSpec& sp){
    std::vector<PrintItem>& v = d.items;
    const int fam = sp.family;
    // ---- style ----
    double pt   = (fam==2||fam==7)?9.0 : (fam==8)?8.5 : 9.5;
    double rH   = sp.rowH;
    double bw   = sp.bw;
    double svcH = (fam==2||fam==7)?40.0 : (fam==3)?38.0 : (fam==8)?30.0
                : (fam==5)?32.0 : 34.0;
    // v1.95: variants within each family get distinct service-table heights so the
    // gallery looks visually different (not just a colour swap).
    if(sp.variant==1) svcH += 2.0;
    else if(sp.variant==2) svcH += 4.0;
    int    finMode = (fam==7)?1 : (fam==9)?2 : 0;
    bool   photo    = (fam==6);
    bool   sidebar  = (fam==3);
    bool   tearoff  = (fam==8);
    bool   shadeBars= (fam==5);
    unsigned svcHdr = sp.headFill; if(fam==4) svcHdr=0;
    int    base = (fam==1||fam==5)?1 : (fam==2)?2 : 0;
    int    hdrMode = (base + sp.variant) % 3;
    if(fam==4) hdrMode=0;                       // line-art: plain header, hairline
    if(photo && hdrMode==1) hdrMode=2;          // never a gray band over the photo

    const std::wstring title = (sp.variant==0)? FA_DOC_REC
                                :(sp.variant==1)? FA_INS_REC
                                                 : FA_DOC_BOTH;
    if(sidebar){ renderSidebar(d, sp, pt, rH, bw, svcH, title); return; }

    double x=R_M, w=R_CW, boxTop=R_M, y=boxTop+1.2;
    if(sp.frame) d.items.push_back(mkFrame(PG_W,PG_H,FR_M,R_INK,0.3));

    // 1 clinic header
    y = medHeader(v, x, y, w, pt, hdrMode, photo);
    // 2 barcode
    y = medBarcode(v, x, y, w, pt);
    // 3 document title
    y = medTitle(v, x, y, w, pt, title);

    // optional shaded section caption bars (card family)
    if(shadeBars){
        v.push_back(mkBand(x, y, w, 5.0, R_SHADE, 0));
        v.push_back(mkLabel(x+1, y+0.4, w-2, 4.4, L"مشخصات بیمار", pt-1.0, true, 1));
        y += 5.0;
    }
    // 4 ruled info box
    y = medInfo(v, x, y, w, rH, pt, bw);

    if(shadeBars){
        v.push_back(mkBand(x, y, w, 5.0, R_SHADE, 0));
        v.push_back(mkLabel(x+1, y+0.4, w-2, 4.4, FA_SVCLIST, pt-1.0, true, 1));
        y += 5.0;
    }
    // 5 services table (dynamic, PIT_SERVICES)
    v.push_back(mkServices(x, y, w, svcH, pt-0.5, sp.svc, svcHdr, bw, rH-1.0, rH-0.8));
    y += svcH;

    if(shadeBars){
        v.push_back(mkBand(x, y, w, 5.0, R_SHADE, 0));
        v.push_back(mkLabel(x+1, y+0.4, w-2, 4.4, L"مالی و پرداخت", pt-1.0, true, 1));
        y += 5.0;
    }
    // 6 financial section
    y = medFinancial(v, x, y, w, rH, pt, bw, finMode);
    // 7 e-prescription  +  8 referral
    y = medEpresReferral(v, x, y, w, rH, pt, bw);

    // the ONE bordered box around the whole receipt body (drawn last; content
    // is inset from every edge so the outline never crosses the text)
    v.push_back(mkRect(x, boxTop, w, y-boxTop, bw, 0));

    // 9 / 10 / 11 below the box
    y = medBelow(v, x, y+2.0, w, pt);

    // compact tear-off stub
    if(tearoff) medStub(v, x, y+2.0, w, pt);
}

static PrintDesign buildTemplate(int idx){
    g_tplItemId = 1;
    PrintDesign d; d.kind=L"builtin";
    d.paper=L"A4"; d.orientation=0; d.paperW=PG_W; d.paperH=PG_H;
    if(idx<0||idx>=30) idx=0;
    d.name = TPL_NAMES[idx];
    const TplSpec& sp = TPL[idx];
    renderReceipt(d, sp);
    return d;
}

} // namespace

// Resolve just the NAME of template idx (migration matches by name).
// Shares the single TPL_NAMES table baked into buildTemplate(), so a design's
// stored name and its gallery caption can never drift apart again.
static std::wstring buildTemplateName(int idx){
    if(idx<0||idx>=30) return L"";
    return TPL_NAMES[idx];
}

// ----------------------------------------------------------------- seeding ---
// Seed the 30 built-in A4 templates AND migrate any previously-seeded
// builtins. The migration is idempotent: it ALWAYS rebuilds each builtin
// from buildTemplate(idx) on every version bump — so an existing install
// picks up the rebuilt services-table layouts automatically. A per-version
// one-shot setting flag guards re-runs. Stored design ids (and therefore
// section<->design bindings) are preserved.
void Designs_Init(){
    std::vector<PrintDesign> existing; Designs_Builtins(existing);
    const int N = 30;
    auto stamp=[](){
        setSetting(L"tpl_migration_1_67", L"1");
        setSetting(L"tpl_migration_1_66", L"1");
        setSetting(L"tpl_migration_1_65", L"1");
        setSetting(L"tpl_migration_1_62", L"1");
        setSetting(L"tpl_migration_1_61", L"1");
        setSetting(L"tpl_migration_1_60", L"1");
        setSetting(L"tpl_migration_1_59", L"1");
        setSetting(L"tpl_migration_1_58", L"1");
        setSetting(L"tpl_migration_1_53", L"1");
        setSetting(L"tpl_migration_1_52", L"1");
    };
    if(existing.empty()){
        for(int i=0;i<N;i++){ PrintDesign d=buildTemplate(i); Designs_Insert(d); }
        stamp();
        return;
    }
    // v1.67.0 migration: rebuild all builtins with the mandatory service
    // columns and runtime-sized rows. Preserve ids and section bindings.
    if(getSetting(L"tpl_migration_1_67", L"0") != L"1"){
        for(int i=0;i<N;i++){
            PrintDesign fresh=buildTemplate(i);
            if(i<(int)existing.size()){ fresh.id=existing[i].id; Designs_Update(fresh); }
            else Designs_Insert(fresh);
        }
        for(size_t i=N;i<existing.size();++i) Designs_Delete(existing[i].id);
        stamp();
    }
}

// v1.65.0 — public in-memory accessor for the print path's last-resort
// fallback (see printPrintDesign). No file I/O, no store mutation.
PrintDesign Design_BuiltinTemplate(int idx){ return buildTemplate(idx); }
