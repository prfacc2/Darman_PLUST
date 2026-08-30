// ============================================================================
//  print_designer_templates.inc — 30 builtin templates + TB1 (v2.07.0)
//
//  Included into print_designer.cpp.
//
//  THE COMPOSER MODEL (§4.4): every builtin is produced by ONE deterministic
//  composer, بساز_طرح(طرح_پارامتر), which always emits the 13 mandatory
//  blocks of §4.3 in order. Templates differ ONLY in the knobs carried by
//  طرح_پارامتر: paper, orientation, layout arrangement, colour emphasis and
//  base font. No loops over random values, no generated names — the 30 rows
//  of the parameter table are literal constants.
//
//  The 13 mandatory blocks (identical on every template, top to bottom):
//   1. لیبل نام درمانگاه {clinicname}
//   2. لیبل آدرس {clinicaddr}
//   3. لیبل تلفن {clinicphone}
//   4. لیبل معرف رسید {receipttitle} — right side
//   5. بارکد PIT_BARCODE hri:true — left side (value ONLY under the graphic)
//   6. تاریخ نوبت {apptdate} · ساعت نوبت {appttime} · نوبت {queue}
//   7. کد ملی {nid} · بیمه پایه {ins_full} · بیمه مکمل {supp_full}
//   8. نام بیمار {P-Name} single-line · سن {age} · شماره شناسنامه {certno}
//   9. انجام‌دهنده {performer} + {performercode}
//  10. پزشک معالج {doctor} + شرح تخصص {specialty} + کد نظام پزشکی {doctorcode}
//  11. جدول خدمات PIT_SERVICES — exactly نام خدمت | تعداد | شرح خدمت
//  12. خلاصهٔ پرداخت: پرداختی/قیمت کل/سهم پایه/سهم مکمل/تخفیف از/تخفیف/نقد/POS
//  13. پذیرش {receptionist} · صندوق‌دار {cashier_name} · ش.ص {scnum} ·
//      تاریخ و ساعت {datetime}
//
//  Removed from every layout per §3.4/§3.6: the duplicated بارکد text row,
//  شماره پرونده (FA_FILENO) and شماره سابقه (FA_ARCHIVE). Their tokens and
//  renderers remain available for user designs (RULE 6) — the composer simply
//  never emits them.
// ============================================================================

namespace {

static int g_tplItemId = 1;
static int nid(){ return g_tplItemId++; }

// ---------------------------------------------------------------------------
//  item builders (mm coordinates)
// ---------------------------------------------------------------------------
static PrintItem mkLabel(double x,double y,double w,double h,
                         const std::wstring& text,double pt,bool bold,int align){
    PrintItem it; it.id=nid(); it.type=PIT_LABEL; it.x=x; it.y=y; it.w=w; it.h=h;
    it.text=text; it.fontPt=pt; it.bold=bold; it.align=align; return it;
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
static PrintItem mkVLine(double x,double y,double h,double bw){
    PrintItem it; it.id=nid(); it.type=PIT_VLINE; it.x=x; it.y=y; it.w=0.2; it.h=h;
    it.borderWidth=bw; return it;
}
static PrintItem mkRect(double x,double y,double w,double h,double bw,double corner){
    PrintItem it; it.id=nid(); it.type=PIT_RECT; it.x=x; it.y=y; it.w=w; it.h=h;
    it.borderWidth=bw; it.corner=corner; it.fillTransparent=true; return it;
}
static PrintItem mkFrame(double pw,double ph,double margin,unsigned col,double bw){
    PrintItem it; it.id=nid(); it.type=PIT_FRAME; it.is_frame=true;
    it.x=margin; it.y=margin; it.w=pw-2*margin; it.h=ph-2*margin;
    it.borderColor=col; it.borderWidth=bw; it.fillTransparent=true; it.corner=0;
    return it;
}
// v2.07 §3.4: the barcode value appears ONLY as the HRI text under the graphic.
static PrintItem mkBarcode(double x,double y,double w,double h){
    PrintItem it; it.id=nid(); it.type=PIT_BARCODE; it.x=x; it.y=y; it.w=w; it.h=h;
    it.field=L"receiptbarcode";
    it.text=L"{\"sym\":\"code128\",\"hri\":true,\"quiet\":2}";
    it.fontPt=8.0; it.align=1; it.textColor=0x000000; return it;
}

// v2.07 §3.7: the canonical 3-column model (the renderer clamps builtins to
// this anyway; the composer stores the same canonical shape).
static PrintItem mkServices(double x,double y,double w,double h,double pt,
                            double bw,double rowH,double headerH){
    PrintItem it; it.id=nid(); it.type=PIT_SERVICES; it.x=x; it.y=y; it.w=w; it.h=h;
    it.text=L"{\"cols\":3,\"header\":true,\"widths\":[0.46,0.12,0.42],"
            L"\"labels\":[\"نام خدمت\",\"تعداد\",\"شرح خدمت\"]}";
    it.fontPt=pt; it.bold=false; it.align=1;
    it.borderColor=0x000000; it.borderWidth=bw;
    it.textColor=0x000000;
    it.fillColor=0xFFFFFF; it.fillTransparent=true;
    it.padding=0.6;
    it.headerH=headerH; it.rowH=rowH;
    return it;
}

// ---------------------------------------------------------------------------
//  exact C2 Persian captions (§4.3 order)
// ---------------------------------------------------------------------------
#define FA_CLINICLBL  L"نام درمانگاه : "
#define FA_ADDRLBL    L"آدرس : "
#define FA_PHONELBL   L"تلفن : "
#define FA_TITLELBL   L""                       // معرف رسید prefix (dynamic)
#define FA_APPTDATE   L"تاریخ نوبت : "
#define FA_APPTTIME   L"ساعت نوبت : "
#define FA_QUEUE      L"نوبت : "
#define FA_NID        L"کد ملی : "
#define FA_BASEINS    L"بیمه پایه : "
#define FA_SUPPINS    L"بیمه مکمل : "
#define FA_FULL       L"نام بیمار : "
#define FA_AGE        L"سن : "
#define FA_CERTNO     L"شماره شناسنامه : "
#define FA_PERF       L"انجام‌دهنده : "
#define FA_PERFCODE   L"کد نظام پزشکی : "
#define FA_DOCTOR     L"پزشک معالج : "
#define FA_SPECIALTY  L"شرح تخصص : "
#define FA_DOCCODE    L"کد نظام پزشکی : "
#define FA_SVCTITLE   L"جدول خدمات"
#define FA_PAID       L"پرداختی : "
#define FA_TOTAL      L"قیمت کل : "
#define FA_BASEPAY    L"سهم پایه : "
#define FA_SUPPPAY    L"سهم مکمل : "
#define FA_DISCFROM   L"تخفیف از : "
#define FA_DISCOUNT   L"تخفیف : "
#define FA_CASH       L"نقد : "
#define FA_POS        L"POS : "
#define FA_RECEPTION  L"پذیرش : "
#define FA_CASHIER    L"صندوق‌دار : "
#define FA_SC         L"ش.ص : "
#define FA_DATETIME   L"تاریخ و ساعت : "

// v2.07.1 — PURE BLACK-AND-WHITE. The clinic's printers are monochrome, so no
// builtin may carry any colour: every stroke, every glyph, every fill is
// #000000 ink on white paper. Emphasis (تأکید) is expressed ONLY through
// weight (bold), rule thickness and framing — never through hue.
static const unsigned R_INK = 0x000000;

// ---------------------------------------------------------------------------
//  label+field row helper (RTL: label on the right, value to its left)
// ---------------------------------------------------------------------------
static inline void putLF(std::vector<PrintItem>& v,double x,double y,double w,double h,
                         const wchar_t* label,const wchar_t* field,double pt,
                         bool lblBold,bool valBold,double lwFrac=0.42){
    double lw=w*lwFrac, gap=0.8, vw=w-lw-gap; if(vw<5) vw=5;
    v.push_back(mkLabel(x+w-lw, y, lw, h, label, pt, lblBold, 0));
    if(field) v.push_back(valBold? mkFieldB(x, y, vw, h, field, L"", pt, 0)
                                 : mkField (x, y, vw, h, field, L"", pt, 0));
}

// ===========================================================================
//  §4.4 — THE COMPOSER PARAMETER STRUCT (per-template variation knobs ONLY)
// ===========================================================================
struct طرح_پارامتر {
    const wchar_t* کد;         // "T01"…"T30"
    const wchar_t* نام;        // Persian display name from §4.2
    const wchar_t* کاغذ;       // "A4"|"A5"|"A6"|"B5"|"Letter"|"R80"|"R58"
    int   جهت;                 // 0 portrait 1 landscape
    int   چیدمان;              // 0 one-column 1 two-column 2 banded 3 card 4 roll
    int   تأکید;               // 0 mono 1 accent-header 2 full-color bands
    double قلم_پایه;           // base pt
};

// ===========================================================================
//  §4.4 — THE SINGLE COMPOSER. بساز_طرح is the ONLY producer of builtin
//  templates. It always emits the 13 mandatory blocks of §4.3 in order.
// ===========================================================================
static PrintDesign بساز_طرح(const طرح_پارامتر& p){
    g_tplItemId = 1;
    PrintDesign d;
    d.kind=L"builtin";
    d.name=p.نام;
    d.paper=p.کاغذ;
    double PW=210.0,PH=297.0;
    Paper_Dims(p.کاغذ,PW,PH);
    d.orientation=p.جهت;
    if(p.جهت==1) std::swap(PW,PH);
    d.paperW=PW; d.paperH=PH;
    d.baseW=PW; d.baseH=PH;      // §4.5: base = authored paper → reflow scales

    std::vector<PrintItem>& v=d.items;
    const double M = (PW<=60.0)?3.0 : (PW<=90.0)?4.0 : (PW<=150.0)?7.0 : 9.0;
    const double bw = 0.30;                       // §3.8 hairline minimum
    const double pt = p.قلم_پایه;
    const bool roll = (PW<=90.0);
    const bool card = (p.چیدمان==3);
    double x=M, w=PW-2*M, y=M;

    // B/W emphasis: 0 = plain, 1 = bold header + heavier rules,
    // 2 = full-page frame + heaviest rules. No colour anywhere.
    double ruleBw = (p.تأکید==2)? bw*2.0 : (p.تأکید==1)? bw*1.5 : bw;
    if(ruleBw < 0.30) ruleBw = 0.30;   // §3.8 hairline floor

    // ---------------- BLOCK 1-3: clinic name / address / phone --------------
    double namePt = pt + (roll?2.5:3.0);
    v.push_back(mkField(x, y, w, 5.6, L"clinicname", L"", namePt, 1));
    v.back().bold=true; v.back().textColor=R_INK;
    y+=5.8;
    v.push_back(mkField(x, y, w, 4.0, L"clinicaddr", FA_ADDRLBL, pt-1.5, 1));
    v.back().textColor=R_INK; y+=4.2;
    v.push_back(mkField(x, y, w, 3.8, L"clinicphone", FA_PHONELBL, pt-1.5, 1));
    v.back().textColor=R_INK; y+=4.4;
    // heavier rule under the letterhead for تأکید>=1
    v.push_back(mkHLine(x,y,w,ruleBw)); y+=1.6;

    // ---------------- BLOCK 4+5: receipt title (right) + barcode (left) -----
    {
        double bcW = roll? (w*0.62) : (w*0.42);
        double bcH = roll?7.5:9.0;
        if(bcW>w-30.0) bcW=w-30.0;
        double tW = w-bcW-2.0;
        // right: معرف رسید
        v.push_back(mkField(x+bcW+2.0, y+1.0, tW, 5.0, L"receipttitle", L"", pt+1.5, 0));
        v.back().bold=true; v.back().textColor=R_INK;
        // left: barcode (value ONLY as the HRI under the graphic)
        v.push_back(mkBarcode(x, y+0.2, bcW, bcH));
        y += bcH+1.6;
    }
    v.push_back(mkHLine(x,y,w,bw)); y+=1.6;

    // ---------------- BLOCKS 6-10: identity / visit / doctor ----------------
    // چیدمان 1 (two-column): blocks 6..10 pack into two columns, side by side.
    // چیدمان 2 (banded):     each block gets a caption bar above it.
    // چیدمان 0/3/4:          single-column rows (the classic receipt flow).
    const bool twoCol = (p.چیدمان==1);
    const bool banded = (p.چیدمان==2);
    auto captionBar=[&](const wchar_t* txt){
        v.push_back(mkLabel(x, y, w, 3.6, txt, pt-1.0, true, 1));
        v.back().textColor=R_INK;
        y+=3.8;
    };
    if(twoCol){
        // Two columns: visit+insurance on the RIGHT column, patient+doctors LEFT.
        double gap=2.0, colW=(w-gap)/2.0;
        double xR=x+colW+gap, xL=x;                 // RTL: right column first
        double yTop=y;
        // right column: blocks 6 + 7
        putLF(v, xR, y, colW, 4.4, FA_APPTDATE, L"apptdate", pt, true,false, 0.52); y+=4.6;
        putLF(v, xR, y, colW, 4.4, FA_APPTTIME, L"appttime", pt, false,false, 0.52); y+=4.6;
        putLF(v, xR, y, colW, 4.4, FA_QUEUE,    L"queue",    pt, false,true,  0.40); y+=4.6;
        putLF(v, xR, y, colW, 4.4, FA_NID,      L"nid",      pt, true,false, 0.46); y+=4.6;
        putLF(v, xR, y, colW, 4.4, FA_BASEINS,  L"ins_full", pt, false,false, 0.48); y+=4.6;
        putLF(v, xR, y, colW, 4.4, FA_SUPPINS,  L"supp_full",pt, false,false, 0.48); y+=4.6;
        double yR=y;
        // left column: blocks 8 + 9 + 10
        y=yTop;
        putLF(v, xL, y, colW, 4.6, FA_FULL, L"P-Name", pt+0.5, true, true, 0.34);
        v.back().dir=0; v.back().align=0; v.back().valign=1;   // §3.5 contract
        y+=5.0;
        putLF(v, xL, y, colW, 4.4, FA_AGE,     L"age",    pt, false,false, 0.40); y+=4.6;
        putLF(v, xL, y, colW, 4.4, FA_CERTNO,  L"certno", pt, false,false, 0.52); y+=4.6;
        putLF(v, xL, y, colW, 4.4, FA_PERF,    L"performer", pt, true,false, 0.34); y+=4.6;
        putLF(v, xL, y, colW, 4.4, FA_PERFCODE, L"performercode", pt, false,false, 0.56); y+=4.6;
        putLF(v, xL, y, colW, 4.4, FA_DOCTOR,  L"doctor",     pt, true,false, 0.36); y+=4.6;
        putLF(v, xL, y, colW, 4.4, FA_SPECIALTY, L"specialty", pt, false,false, 0.40); y+=4.6;
        putLF(v, xL, y, colW, 4.4, FA_DOCCODE,   L"doctorcode", pt, false,false, 0.56); y+=4.6;
        if(yR>y) y=yR;
        v.push_back(mkVLine(x+colW+gap/2.0, yTop, y-yTop, bw));
        v.push_back(mkHLine(x,y,w,bw)); y+=1.4;
    } else {
        if(banded) captionBar(L"مشخصات نوبت و بیمه");
        // ---------------- BLOCK 6: appt date · time · queue ----------------
        {
            double colW=(w-2*0.6)/3.0;
            putLF(v, x+2*(colW+0.6), y, colW, 4.4, FA_APPTDATE, L"apptdate", pt, true,false, 0.52);
            putLF(v, x+(colW+0.6),     y, colW, 4.4, FA_APPTTIME, L"appttime", pt, false,false, 0.52);
            putLF(v, x,                y, colW, 4.4, FA_QUEUE,    L"queue",    pt, false,true,  0.40);
            y+=4.8; v.push_back(mkHLine(x,y,w,bw)); y+=1.2;
        }
        // ---------------- BLOCK 7: nid · base ins · supp ins ---------------
        {
            double colW=(w-2*0.6)/3.0;
            putLF(v, x+2*(colW+0.6), y, colW, 4.4, FA_NID,     L"nid",      pt, true,false, 0.46);
            putLF(v, x+(colW+0.6),   y, colW, 4.4, FA_BASEINS, L"ins_full", pt, false,false, 0.48);
            putLF(v, x,              y, colW, 4.4, FA_SUPPINS, L"supp_full",pt, false,false, 0.48);
            y+=4.8; v.push_back(mkHLine(x,y,w,bw)); y+=1.2;
        }
        if(banded) captionBar(L"مشخصات بیمار و پزشک");
        // ---------------- BLOCK 8: patient name (single line) · age · certno
        {
            double nameW = w*0.46, ageW=w*0.22, certW=w-nameW-ageW-1.2;
            putLF(v, x+ageW+certW+1.2, y, nameW, 4.6, FA_FULL, L"P-Name", pt+0.5, true, true, 0.34);
            // §3.5 name item contract: dir 0 (RTL), align 0 (right), valign 1
            v.back().dir=0; v.back().align=0; v.back().valign=1;
            putLF(v, x+certW+0.6, y, ageW, 4.4, FA_AGE,    L"age",    pt, false,false, 0.40);
            putLF(v, x,            y, certW, 4.4, FA_CERTNO, L"certno", pt, false,false, 0.52);
            y+=5.0; v.push_back(mkHLine(x,y,w,bw)); y+=1.2;
        }
        // ---------------- BLOCK 9: انجام‌دهنده + کد -------------------------
        {
            double codeW=w*0.28;
            putLF(v, x+codeW+0.6, y, w-codeW-0.6, 4.4, FA_PERF, L"performer", pt, true,false, 0.34);
            putLF(v, x,            y, codeW,      4.4, FA_PERFCODE, L"performercode", pt, false,false, 0.56);
            y+=4.8; v.push_back(mkHLine(x,y,w,bw)); y+=1.2;
        }
        // ---------------- BLOCK 10: پزشک معالج + تخصص + کد نظام پزشکی ------
        {
            double codeW=w*0.24, specW=w*0.34, docW=w-codeW-specW-1.2;
            putLF(v, x+codeW+specW+1.2, y, docW,  4.4, FA_DOCTOR,   L"doctor",     pt, true,false, 0.36);
            putLF(v, x+codeW+0.6,       y, specW, 4.4, FA_SPECIALTY, L"specialty",  pt, false,false, 0.40);
            putLF(v, x,                 y, codeW, 4.4, FA_DOCCODE,   L"doctorcode", pt, false,false, 0.56);
            y+=4.8; v.push_back(mkHLine(x,y,w,bw)); y+=1.4;
        }
    }

    // ---------------- BLOCK 11: جدول خدمات (3 columns) ---------------------
    // The elastic item of the layout: everything below forms بلوک_پایانی.
    if(banded) captionBar(L"جدول خدمات");
    double footH = roll? 26.0 : 34.0;
    double moneyH = roll? 16.0 : 22.0;
    double svcY=y;
    double svcH = (PH - M - footH - moneyH) - y;
    if(svcH < 18.0) svcH = 18.0;
    if(!roll && svcH > (PH-2*M)*0.42) svcH=(PH-2*M)*0.42;
    v.push_back(mkServices(x, svcY, w, svcH, pt-0.8, bw,
                           roll?4.6:5.2, roll?4.8:5.4));
    y = svcY + svcH + 1.2;

    // ---------------- BLOCK 12: خلاصهٔ پرداخت --------------------------------
    {
        double colW=(w-0.6)/2.0;
        double rH = roll?3.8:4.6;
        auto money2=[&](const wchar_t* l1,const wchar_t* f1,
                        const wchar_t* l2,const wchar_t* f2,double yy,bool boldL){
            putLF(v, x+colW+0.6, yy, colW, rH, l1, f1, pt, boldL, false, 0.44);
            v.back().textColor=R_INK;
            putLF(v, x,           yy, colW, rH, l2, f2, pt, boldL, false, 0.44);
            v.back().textColor=R_INK;
        };
        money2(FA_PAID,     L"paid",          FA_TOTAL,    L"total",         y, true);
        y+=rH;
        money2(FA_BASEPAY,  L"basepay",       FA_SUPPPAY,  L"supppay",       y, false);
        y+=rH;
        money2(FA_DISCFROM, L"discount_from", FA_DISCOUNT, L"discount",      y, false);
        y+=rH;
        money2(FA_CASH,     L"cash",          FA_POS,      L"pos",           y, false);
        y+=rH+0.8;
        v.push_back(mkHLine(x,y,w,bw)); y+=1.2;
    }

    // ---------------- BLOCK 13: پذیرش · صندوق‌دار · ش.ص · تاریخ و ساعت ------
    {
        double colW=(w-0.6)/2.0;
        double rH = roll?3.6:4.2;
        putLF(v, x+colW+0.6, y, colW, rH, FA_RECEPTION, L"receptionist", pt-0.5, false,false, 0.34);
        v.back().textColor=R_INK;
        putLF(v, x,           y, colW, rH, FA_CASHIER,   L"cashier_name", pt-0.5, false,false, 0.36);
        v.back().textColor=R_INK;
        y+=rH;
        putLF(v, x+colW+0.6, y, colW, rH, FA_SC,        L"scnum",    pt-0.5, false,false, 0.30);
        v.back().textColor=R_INK;
        putLF(v, x,           y, colW, rH, FA_DATETIME, L"datetime", pt-0.5, false,false, 0.44);
        v.back().textColor=R_INK;
    }

    // Framing per layout family (never straddles the services bottom edge —
    // §3.9 invariant: frames stop ABOVE the table or wrap the whole page).
    if(p.چیدمان==0 && p.تأکید==2 && !roll){
        v.push_back(mkFrame(PW,PH,M-1.0,R_INK,0.30));
    } else if(card){
        v.push_back(mkFrame(PW,PH,M-1.0,R_INK,0.30));
    }
    return d;
}

// ===========================================================================
//  §7.5 — TB1: «برچسب بارکد زیربخش» — barcode-only label (NOT one of the 30).
//  Reduced block set: {clinicname}, {receipttitle}, PIT_BARCODE hri:true,
//  {P-Name}, {nid}, {queue}, {datetime}.
// ===========================================================================
static PrintDesign بساز_برچسب_بارکد(){
    g_tplItemId = 1;
    PrintDesign d;
    d.kind=L"builtin";
    d.name=L"برچسب بارکد زیربخش";
    d.paper=L"R80"; d.paperW=80.0; d.paperH=120.0;
    d.orientation=0; d.baseW=80.0; d.baseH=120.0;
    std::vector<PrintItem>& v=d.items;
    const double M=3.0, bw=0.30, pt=8.0;
    double x=M, w=80.0-2*M, y=M;
    v.push_back(mkField(x, y, w, 5.0, L"clinicname", L"", pt+2.0, 1));
    v.back().bold=true; y+=5.4;
    v.push_back(mkField(x, y, w, 4.2, L"receipttitle", L"", pt, 1));
    v.back().textColor=R_INK; y+=4.6;
    v.push_back(mkHLine(x,y,w,bw)); y+=1.4;
    v.push_back(mkBarcode(x, y, w, 12.0)); y+=13.4;
    putLF(v, x, y, w, 4.6, L"نام بیمار : ", L"P-Name", pt, true, true, 0.36); y+=5.0;
    putLF(v, x, y, w, 4.2, L"کد ملی : ",   L"nid",    pt, false,false, 0.34); y+=4.6;
    putLF(v, x, y, w, 4.2, L"نوبت : ",     L"queue",  pt, false,false, 0.30); y+=4.6;
    putLF(v, x, y, w, 4.2, L"تاریخ و ساعت : ", L"datetime", pt-0.5, false,false, 0.44);
    return d;
}

// ===========================================================================
//  §4.2 — THE FROZEN PARAMETER TABLE (30 rows, literal constants)
// ===========================================================================
static const طرح_پارامتر TPL_TABLE[30] = {
    {L"T01", L"طرح پیش‌فرض حرفه‌ای",      L"A4",  0, 0, 1, 9.5},
    {L"T02", L"طرح ساده و سریع",          L"A5",  0, 0, 0, 9.0},
    {L"T03", L"طرح رسمی کادردار",         L"A4",  0, 0, 2, 9.5},
    {L"T04", L"طرح فشردهٔ A5",            L"A5",  0, 0, 0, 8.5},
    {L"T05", L"طرح سربرگ رنگی مدرن",      L"A4",  0, 0, 2, 9.5},
    {L"T06", L"طرح شمارهٔ نوبت درشت",      L"A5",  0, 0, 1, 9.5},
    {L"T07", L"طرح رسید پرداخت",          L"A5",  0, 0, 1, 9.0},
    {L"T08", L"طرح دو ستونهٔ شیک",        L"A4",  0, 1, 1, 9.0},
    {L"T09", L"طرح مینیمال خط‌دار",        L"A5",  0, 0, 0, 9.0},
    {L"T10", L"طرح کارت بیمار",           L"A6",  0, 3, 1, 8.5},
    {L"T11", L"طرح نواری ۸۰ میلی‌متر",     L"R80", 0, 4, 0, 7.5},
    {L"T12", L"طرح نواری ۵۸ میلی‌متر",     L"R58", 0, 4, 0, 7.0},
    {L"T13", L"طرح افقی A5",             L"A5",  1, 0, 1, 9.0},
    {L"T14", L"طرح افقی A4",             L"A4",  1, 0, 1, 9.5},
    {L"T15", L"طرح بارکدمحور",           L"A6",  0, 3, 0, 8.5},
    {L"T16", L"طرح جدول‌محور",            L"A4",  0, 0, 1, 9.0},
    {L"T17", L"طرح دو ستونهٔ فشرده",      L"A5",  0, 1, 0, 8.5},
    {L"T18", L"طرح سربرگ‌دار بلند",        L"A4",  0, 0, 2, 9.5},
    {L"T19", L"طرح رسید نقدی",           L"A5",  0, 0, 1, 9.0},
    {L"T20", L"طرح رسید کارتخوان",        L"A5",  0, 0, 1, 9.0},
    {L"T21", L"طرح بیمهٔ پایه",           L"A5",  0, 0, 1, 9.0},
    {L"T22", L"طرح بیمهٔ تکمیلی",         L"A5",  0, 0, 1, 9.0},
    {L"T23", L"طرح آزمایشگاه",           L"A5",  0, 0, 1, 9.0},
    {L"T24", L"طرح رادیولوژی",           L"A5",  0, 0, 1, 9.0},
    {L"T25", L"طرح تزریقات",             L"A5",  0, 0, 1, 9.0},
    {L"T26", L"طرح داروخانه",            L"A5",  0, 0, 1, 9.0},
    {L"T27", L"طرح فیزیوتراپی",           L"A5",  0, 0, 1, 9.0},
    {L"T28", L"طرح نسخهٔ پزشک",           L"A5",  0, 0, 1, 9.0},
    {L"T29", L"طرح صورتحساب تفصیلی",       L"A4",  0, 0, 2, 9.0},
    {L"T30", L"طرح خلاصهٔ مدیریتی",        L"A4",  0, 0, 1, 9.0},
};

static const wchar_t* const TPL_NAMES[31]={
    L"پیش‌فرض",
    L"T01 طرح پیش‌فرض حرفه‌ای",
    L"T02 طرح ساده و سریع",
    L"T03 طرح رسمی کادردار",
    L"T04 طرح فشردهٔ A5",
    L"T05 طرح سربرگ رنگی مدرن",
    L"T06 طرح شمارهٔ نوبت درشت",
    L"T07 طرح رسید پرداخت",
    L"T08 طرح دو ستونهٔ شیک",
    L"T09 طرح مینیمال خط‌دار",
    L"T10 طرح کارت بیمار",
    L"T11 طرح نواری ۸۰ میلی‌متر",
    L"T12 طرح نواری ۵۸ میلی‌متر",
    L"T13 طرح افقی A5",
    L"T14 طرح افقی A4",
    L"T15 طرح بارکدمحور",
    L"T16 طرح جدول‌محور",
    L"T17 طرح دو ستونهٔ فشرده",
    L"T18 طرح سربرگ‌دار بلند",
    L"T19 طرح رسید نقدی",
    L"T20 طرح رسید کارتخوان",
    L"T21 طرح بیمهٔ پایه",
    L"T22 طرح بیمهٔ تکمیلی",
    L"T23 طرح آزمایشگاه",
    L"T24 طرح رادیولوژی",
    L"T25 طرح تزریقات",
    L"T26 طرح داروخانه",
    L"T27 طرح فیزیوتراپی",
    L"T28 طرح نسخهٔ پزشک",
    L"T29 طرح صورتحساب تفصیلی",
    L"T30 طرح خلاصهٔ مدیریتی",
};

static PrintDesign buildTemplate(int idx){
    if(idx==0){                     // never-deletable default = T01 shape on A4
        طرح_پارامتر p=TPL_TABLE[0];
        p.کد=L"T01"; p.نام=TPL_NAMES[0];
        return بساز_طرح(p);
    }
    if(idx>=1 && idx<=30) return بساز_طرح(TPL_TABLE[idx-1]);
    if(idx==31)           return بساز_برچسب_بارکد();     // TB1 (§7.5)
    return buildTemplate(0);
}

} // namespace

static std::wstring buildTemplateName(int idx){
    if(idx<0||idx>=31) return L"";
    return TPL_NAMES[idx];
}

// ---------------------------------------------------------------------------
//  §4.6 — COMPILE-TIME-STYLE SMOKE ASSERTION (runs in AZ_SMOKE debug builds).
//  Declared at file scope (outside the anonymous namespace) so the AZ_DEBUG
//  smoke dispatcher in main.cpp can call it. Each of the 30 designs must
//  contain all 13 mandatory blocks, exactly ONE PIT_BARCODE, exactly ONE
//  PIT_SERVICES with cols==3, ZERO items bound to شماره سابقه/شماره پرونده,
//  and at most ONE item bound to {receiptbarcode}.
// ---------------------------------------------------------------------------
bool pdVerifyBuiltinTemplates(){
    bool ok=true;
    for(int i=0;i<=30;++i){
        PrintDesign d=buildTemplate(i);
        int nBarcode=0,nServices=0,nRcptBarcode=0;
        bool hasClinic=false,hasAddr=false,hasPhone=false,hasTitle=false,
             hasAppt=false,hasIns=false,hasPatient=false,hasPerf=false,
             hasDoctor=false,hasMoney=false,hasFooter=false;
        std::wstring svcModel;
        for(const auto& it:d.items){
            if(it.type==PIT_BARCODE) ++nBarcode;
            if(it.type==PIT_SERVICES){ ++nServices; svcModel=it.text; }
            if(it.type==PIT_FIELD || it.type==PIT_LABEL){
                std::wstring f=it.field;
                if(f==L"clinicname") hasClinic=true;
                if(f==L"clinicaddr") hasAddr=true;
                if(f==L"clinicphone") hasPhone=true;
                if(f==L"receipttitle") hasTitle=true;
                if(f==L"apptdate"||f==L"appttime"||f==L"queue") hasAppt=true;
                if(f==L"nid"||f==L"ins_full"||f==L"supp_full") hasIns=true;
                if(f==L"P-Name"||f==L"full"||f==L"age"||f==L"certno") hasPatient=true;
                if(f==L"performer"||f==L"performercode") hasPerf=true;
                if(f==L"doctor"||f==L"specialty"||f==L"doctorcode") hasDoctor=true;
                if(f==L"paid"||f==L"total"||f==L"basepay"||f==L"supppay"||
                   f==L"discount_from"||f==L"discount"||f==L"cash"||f==L"pos") hasMoney=true;
                if(f==L"receptionist"||f==L"cashier_name"||f==L"scnum"||f==L"datetime") hasFooter=true;
                if(f==L"receiptbarcode") ++nRcptBarcode;
                // §3.6: شماره سابقه must be gone (and شماره پرونده is not §4.3)
                if(f==L"archiveNo"||f==L"fileNo"){
                    logLine(L"SMOKE FAIL: template "+std::to_wstring(i)+
                            L" still binds removed field "+f);
                    ok=false;
                }
            }
        }
        if(nBarcode!=1){ logLine(L"SMOKE FAIL: template "+std::to_wstring(i)+
            L" has "+std::to_wstring(nBarcode)+L" barcodes (want 1)"); ok=false; }
        if(nServices!=1){ logLine(L"SMOKE FAIL: template "+std::to_wstring(i)+
            L" has "+std::to_wstring(nServices)+L" services tables (want 1)"); ok=false; }
        if(nRcptBarcode>1){ logLine(L"SMOKE FAIL: template "+std::to_wstring(i)+
            L" binds {receiptbarcode} "+std::to_wstring(nRcptBarcode)+L" times"); ok=false; }
        if(svcModel.find(L"\"cols\":3")==std::wstring::npos){
            logLine(L"SMOKE FAIL: template "+std::to_wstring(i)+L" services cols != 3"); ok=false; }
        if(!(hasClinic&&hasAddr&&hasPhone&&hasTitle&&hasAppt&&hasIns&&hasPatient
             &&hasPerf&&hasDoctor&&hasMoney&&hasFooter)){
            logLine(L"SMOKE FAIL: template "+std::to_wstring(i)+L" missing mandatory blocks");
            ok=false;
        }
    }
    return ok;
}

// ---------------------------------------------------------------------------
//  §4.6 — idempotent seeding / repair. An existing user-modified copy is NEVER
//  overwritten; a missing or malformed builtin is regenerated from بساز_طرح.
// ---------------------------------------------------------------------------
void Designs_Init(){
    std::vector<PrintDesign> existing; Designs_Builtins(existing);
    const int N = 31;       // index 0 = default, 1..30 = T01..T30, 31st slot = TB1
    auto stamp=[](){
        setSetting(L"tpl_migration_2_07_1", L"1");   // v2.07.1 B/W rebuild
        setSetting(L"tpl_migration_2_07", L"1");
        setSetting(L"tpl_migration_2_06", L"1");
        setSetting(L"tpl_migration_2_00", L"1");
        setSetting(L"tpl_migration_1_200", L"1");
        setSetting(L"tpl_migration_1_99", L"1");
        setSetting(L"tpl_migration_1_98", L"1");
        setSetting(L"tpl_migration_1_97", L"1");
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
    // v2.07.1 migration: rebuild all builtins in PURE BLACK-AND-WHITE (the
    // clinic's printers are monochrome) and restore the full designer-gallery
    // layouts. The 2_07_1 flag is a NEW guard so installs that already ran the
    // 2.07 rebuild get this regeneration too. Stored ids / machine bindings
    // are preserved; user-modified copies keep kind=="user" and are untouched.
    if(getSetting(L"tpl_migration_2_07_1", L"0") != L"1"){
        for(int i=0;i<N;i++){
            PrintDesign fresh=buildTemplate(i);
            if(i<(int)existing.size()){ fresh.id=existing[i].id; Designs_Update(fresh); }
            else Designs_Insert(fresh);
        }
        for(size_t i=N;i<existing.size();++i) Designs_Delete(existing[i].id);
        stamp();
    }
}

PrintDesign Design_BuiltinTemplate(int idx){ return buildTemplate(idx); }
