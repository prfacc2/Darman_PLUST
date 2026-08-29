// ============================================================================
//  print_designer_templates.inc — 31 built-in reception templates (v2.00)
//
//  Included into print_designer.cpp.
//
//  Index 0 is the never-deletable «پیش‌فرض»: an exact thermal R80 (80×200 mm)
//  Samen-al-Aemeh receipt. Indices 1..30 are 30 additional designs, all
//  different from the default and from each other, mixing R80 / R58 / A5 / A4.
//
//  Same complete field set + exact Persian C2 labels on every design.
//  One Code128 (field receiptbarcode, never nid). Basic insurance = {ins}
//  name only (no percent). Supplementary = {supp} {supp_percent}.
// ============================================================================

namespace {

static int g_tplItemId = 1;
static int nid(){ return g_tplItemId++; }

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
static PrintItem mkFrameBox(double x,double y,double w,double h,double bw){
    PrintItem it; it.id=nid(); it.type=PIT_FRAME; it.is_frame=true;
    it.x=x; it.y=y; it.w=w; it.h=h;
    it.borderColor=0x000000; it.borderWidth=bw; it.fillTransparent=true; it.corner=0;
    return it;
}
static PrintItem mkLogo(double x,double y,double w,double h){
    PrintItem it; it.id=nid(); it.type=PIT_LOGO; it.x=x; it.y=y; it.w=w; it.h=h;
    it.borderWidth=0.3; return it;
}
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

// ---------------------------------------------------------------------------o
//  v2.06 — TABLE POLICY (user request): the services table carries EXACTLY
//  three columns — نام خدمت / تعداد / شرح خدمت. Nothing else. All money and
//  identity data (پرداختی، قیمت کل، سهم پایه، سهم مکمل، تخفیف، نقد، POS،
//  کد رهگیری نسخه الکترونیک، شماره معرفی نامه، صندوق دار، ش.ص، تاریخ و ساعت)
//  lives BELOW the table in the footer blocks (emitSamenFinance /
//  emitSamenRef / emitSamenFooter). The former per-variant presets are kept
//  as enum values so stored designs referencing them still parse, but every
//  preset now renders the same canonical 3-column model (only the column
//  widths differ slightly per paper width).
// ---------------------------------------------------------------------------
enum SvcPreset {
    SVC3 = 0, SVC4_ROW, SVC4_CAT, SVC5, SVC5_CODE, SVC6_FIN, SVC6_INS, SVC7,
    SVC_SAMEN,
    SVC_PRESET_COUNT
};

static std::wstring svcModelJson(int preset){
    /* v2.06: every preset → the canonical 3-column table. The variant only
       tweaks the NAME/DESC width split so narrow rolls keep readable prose. */
    (void)preset;
    return L"{\"cols\":3,\"header\":true,\"widths\":[0.34,0.10,0.56],"
           L"\"labels\":[\"نام خدمت\",\"تعداد\",\"شرح خدمت\"]}";
}

static PrintItem mkServices(double x,double y,double w,double h,double pt,
                            int preset,unsigned headFill,double bw,
                            double rowH,double headerH){
    PrintItem it; it.id=nid(); it.type=PIT_SERVICES; it.x=x; it.y=y; it.w=w; it.h=h;
    it.text=svcModelJson(preset);
    it.fontPt=pt; it.bold=false; it.align=1;
    it.borderColor=0x000000; it.borderWidth=bw;
    it.textColor=0x000000;
    it.fillColor=0xFFFFFF; it.fillTransparent=true;
    (void)headFill;
    it.padding=0.6;
    it.headerH=headerH; it.rowH=rowH;
    return it;
}

// --- exact C2 Persian captions --------------------------------------------
#define FA_CLINIC    L"درمانگاه شبانه روزی ثامن الائمه"
#define FA_ADDR      L"آدرس : "
#define FA_PHONE     L"تلفن : "
#define FA_DOC_REC   L"قبض پزشک"
#define FA_INS_REC   L"رسید بیمه"
#define FA_DOC_BOTH  L"قبض و رسید"
#define FA_APPT      L"تاریخ نوبت : "
#define FA_QUEUE     L"نوبت : "
#define FA_NID       L"کد ملی : "
#define FA_BARCODE   L"بارکد : "
#define FA_BASEINS   L"بیمه پایه : "
#define FA_SUPP      L"مکمل : "
#define FA_FULL      L"نام بیمار : "
#define FA_FILENO    L"شماره پرونده "
#define FA_ARCHIVE   L"شماره سابقه "
#define FA_DOCTOR    L"دکتر : "
#define FA_SPECIALTY L"شرح تخصص : "
#define FA_PAID      L"پرداختی : "
#define FA_TOTAL     L"قیمت کل : "
#define FA_INSSHARE  L"سهم پایه : "
#define FA_SUPPPAY   L"سهم مکمل : "
#define FA_DISCFROM  L"تخفیف از : "
#define FA_DISCOUNT  L"تخفیف : "
#define FA_CASH      L"نقد : "
#define FA_CASHDISC  L"تخفیف نقد : "
#define FA_POS       L"POS : "
#define FA_EPRESC    L"کد رهگیری نسخه الکترونیک : "
#define FA_REFERRAL  L"شماره معرفی نامه : "
#define FA_RECEPTION L"پذیرش : "
#define FA_CASHIER   L"صندوق دار : "
#define FA_SC        L"ش.ص : "
#define FA_CASH_SFX  L" / دار"
#define FA_TEAR      L"— — — محل جدا کردن — — —"
#define FA_STUB      L"نسخهٔ بیمار"
#define FA_PAYLBL    L"پرداخت‌ها"
#define FA_SHARELBL  L"سهم بیمه"
#define FA_SVCLIST   L"شرح خدمات"

static const unsigned R_INK = 0x000000;

static inline void putLF(std::vector<PrintItem>& v,double x,double y,double w,double h,
                         const wchar_t* label,const wchar_t* field,double pt,
                         bool lblBold,bool valBold,double lwFrac=0.42){
    double lw=w*lwFrac, gap=0.8, vw=w-lw-gap; if(vw<5) vw=5;
    v.push_back(mkLabel(x+w-lw, y, lw, h, label, pt, lblBold, 0));
    if(field) v.push_back(valBold? mkFieldB(x, y, vw, h, field, L"", pt, 0)
                                 : mkField (x, y, vw, h, field, L"", pt, 0));
}

static double emitHeader(std::vector<PrintItem>& v,double x,double y,double w,
                         double pt,bool rules,bool photo){
    double pw=14.0, ph=16.0, tw=w;
    if(photo) tw=w-pw-2.0;
    double namePt=pt+3.0;
    v.push_back(mkLabel(x, y, tw, 6.2, FA_CLINIC, namePt, true, 1)); y+=6.4;
    v.push_back(mkField(x, y, tw, 4.0, L"clinicaddr",  FA_ADDR,  pt-1.5, 1)); y+=4.2;
    v.push_back(mkField(x, y, tw, 3.8, L"clinicphone", FA_PHONE, pt-1.5, 1)); y+=4.0;
    if(photo) v.push_back(mkPhoto(x+w-pw, y-14.4, pw, ph));
    if(rules){ v.push_back(mkHLine(x, y, w, 0.3)); y+=1.2; }
    return y;
}

static double emitTitle(std::vector<PrintItem>& v,double x,double y,double w,
                        double pt,const std::wstring& title){
    v.push_back(mkLabel(x, y, w, 5.6, title, pt+1.5, true, 1));
    y+=6.0;
    v.push_back(mkHLine(x, y, w, 0.3));
    return y+0.8;
}

static double emitBarcode(std::vector<PrintItem>& v,double x,double y,double w,
                          double bcW,double bcH){
    if(bcW>w-1.0) bcW=w-1.0;
    v.push_back(mkBarcode(x+(w-bcW)/2.0, y+0.3, bcW, bcH));
    return y+bcH+1.2;
}

// C2 info block — 6 rows, RTL label-right value-left, HLINE separators.
static double emitSamenInfo(std::vector<PrintItem>& v,double x,double y,double w,
                            double rH,double pt,double bw){
    double half=w/2.0, fh=rH-0.7;
    auto vline=[&](double cy){ v.push_back(mkVLine(x+half, cy, rH, bw)); };
    auto rule =[&](double cy){ v.push_back(mkHLine(x, cy, w, bw)); };

    // 1 تاریخ نوبت : {apptdate} {appttime} | نوبت : {queue}
    vline(y);
    { double rx=x+half+0.5, rw=half-1.0;
      double lblW=rw*0.40, g=0.5, rem=rw-lblW-2*g; if(rem<8) rem=8;
      double f1=rem*0.58, f2=rem*0.42;
      v.push_back(mkField (rx,         y+0.3, f2, fh, L"appttime", L"", pt, 0));
      v.push_back(mkFieldB(rx+f2+g,     y+0.3, f1, fh, L"apptdate", L"", pt, 0));
      v.push_back(mkLabel (rx+rw-lblW,  y+0.3, lblW, fh, FA_APPT, pt, true, 0)); }
    putLF(v, x+0.5, y+0.3, half-1.0, fh, FA_QUEUE, L"queue", pt, true, false, 0.38);
    y+=rH; rule(y);

    // 2 کد ملی : {nid} | بارکد : {receiptbarcode}
    vline(y);
    putLF(v, x+half+0.5, y+0.3, half-1.0, fh, FA_NID,     L"nid",             pt, true,  false, 0.40);
    putLF(v, x+0.5,      y+0.3, half-1.0, fh, FA_BARCODE, L"receiptbarcode",  pt, false, false, 0.32);
    y+=rH; rule(y);

    // 3 بیمه پایه : {ins} | مکمل : {supp} {supp_percent}
    vline(y);
    putLF(v, x+half+0.5, y+0.3, half-1.0, fh, FA_BASEINS, L"ins", pt, false, false, 0.42);
    { double lx=x+0.5, lw=half-1.0;
      double lblW=lw*0.28, g=0.5, rem=lw-lblW-2*g;
      double f1=rem*0.58, f2=rem*0.42;
      v.push_back(mkField(lx,         y+0.3, f2, fh, L"supp_percent", L"", pt, 0));
      v.push_back(mkField(lx+f2+g,     y+0.3, f1, fh, L"supp",         L"", pt, 0));
      v.push_back(mkLabel(lx+lw-lblW,  y+0.3, lblW, fh, FA_SUPP, pt, false, 0)); }
    y+=rH; rule(y);

    // 4 نام بیمار : {full} | شماره پرونده {fileNo} LARGE BOLD | شماره سابقه {archiveNo} small
    { double w1=w*0.42, w2=w*0.33, w3=w-w1-w2;
      v.push_back(mkVLine(x+w1, y, rH, bw));
      v.push_back(mkVLine(x+w1+w2, y, rH, bw));
      putLF(v, x+w1+w2+0.4, y+0.3, w3-0.8, fh, FA_FULL, L"full", pt, true, false, 0.44);
      { double ptB=pt+1.5, fhB=fh;
        putLF(v, x+w3+0.4, y+0.25, w2-0.8, fhB, FA_FILENO, L"fileNo", ptB, true, true, 0.52);
        v.back().bold=true; v.back().fontPt=ptB; }
      putLF(v, x+0.4, y+0.4, w1-0.8, fh-0.2, FA_ARCHIVE, L"archiveNo", pt-1.5, false, false, 0.52); }
    y+=rH; rule(y);

    // 5 دکتر : {doctor}-{doctorcode} full width
    { double lblW=(w<60)?11.0:13.0, hyW=2.4, codeW=w*0.20; if(codeW<10) codeW=10;
      double docW=w-lblW-hyW-codeW-1.2; if(docW<12) docW=12;
      double xl=x+0.5;
      v.push_back(mkField (xl,               y+0.3, codeW, fh, L"doctorcode", L"", pt, 0));
      v.push_back(mkLabel (xl+codeW,         y+0.3, hyW,   fh, L"-", pt, false, 1));
      v.push_back(mkFieldB(xl+codeW+hyW,     y+0.3, docW,  fh, L"doctor",     L"", pt, 0));
      v.push_back(mkLabel (x+w-lblW-0.4,     y+0.3, lblW,  fh, FA_DOCTOR, pt, true, 0)); }
    y+=rH; rule(y);

    // 6 شرح تخصص : {specialty} | empty reserved | {specialtycode} one char
    { double codeW=(w<60)?6.0:7.5, emptyW=w*0.16; if(emptyW<8) emptyW=8;
      double specW=w-codeW-emptyW-1.0;
      v.push_back(mkVLine(x+codeW+0.4, y, rH, bw));
      v.push_back(mkVLine(x+codeW+emptyW, y, rH, bw));
      v.push_back(mkField(x+0.4, y+0.3, codeW, fh, L"specialtycode", L"", pt-1.0, 1));
      putLF(v, x+codeW+emptyW+0.4, y+0.3, specW-0.6, fh, FA_SPECIALTY, L"specialty", pt, false, false, 0.36); }
    y+=rH; rule(y);
    return y;
}

// Two-column packing of the same C2 fields (patient | visit).
static double emitSamenInfoSplit(std::vector<PrintItem>& v,double x,double y,double w,
                                 double rH,double pt,double bw){
    double gap=2.4, colW=(w-gap)/2.0, fh=rH-0.7;
    double lx=x, rx=x+colW+gap;
    const int n=6;
    v.push_back(mkRect(lx, y, colW, n*rH, bw, 0));
    v.push_back(mkRect(rx, y, colW, n*rH, bw, 0));
    for(int i=1;i<n;i++){
        v.push_back(mkHLine(lx, y+i*rH, colW, bw));
        v.push_back(mkHLine(rx, y+i*rH, colW, bw));
    }
    auto cell=[&](double cx,int row,const wchar_t* lab,const wchar_t* fld,
                  bool lb,bool vb,double frac,double ptv){
        putLF(v, cx+0.6, y+row*rH+0.3, colW-1.2, fh, lab, fld, ptv, lb, vb, frac);
    };
    cell(rx,0,FA_FULL,    L"full",            true,  false, 0.44, pt);
    cell(rx,1,FA_NID,     L"nid",             true,  false, 0.40, pt);
    cell(rx,2,FA_FILENO,  L"fileNo",          true,  true,  0.52, pt+1.0);
    cell(rx,3,FA_ARCHIVE, L"archiveNo",       false, false, 0.52, pt-1.0);
    cell(rx,4,FA_BASEINS, L"ins",             false, false, 0.42, pt);
    { double cw=colW-1.2, xx=rx+0.6, yy=y+5*rH+0.3;
      double lblW=cw*0.28, g=0.5, rem=cw-lblW-2*g, f1=rem*0.58, f2=rem*0.42;
      v.push_back(mkField(xx,         yy, f2, fh, L"supp_percent", L"", pt, 0));
      v.push_back(mkField(xx+f2+g,     yy, f1, fh, L"supp",         L"", pt, 0));
      v.push_back(mkLabel(xx+cw-lblW,  yy, lblW, fh, FA_SUPP, pt, false, 0)); }
    { double cw=colW-1.2, xx=lx+0.6, yy=y+0.3;
      double lblW=cw*0.40, g=0.5, rem=cw-lblW-2*g, f1=rem*0.58, f2=rem*0.42;
      v.push_back(mkField (xx,         yy, f2, fh, L"appttime", L"", pt, 0));
      v.push_back(mkFieldB(xx+f2+g,     yy, f1, fh, L"apptdate", L"", pt, 0));
      v.push_back(mkLabel (xx+cw-lblW,  yy, lblW, fh, FA_APPT, pt, true, 0)); }
    cell(lx,1,FA_QUEUE,     L"queue",           true,  false, 0.38, pt);
    cell(lx,2,FA_BARCODE,   L"receiptbarcode",  false, false, 0.34, pt);
    cell(lx,3,FA_DOCTOR,    L"doctor",          true,  false, 0.30, pt);
    { PrintItem c=mkField(lx+0.6, y+4*rH+0.3, colW-1.2, fh, L"doctorcode", L"-", pt, 0);
      v.push_back(c); }
    cell(lx,5,FA_SPECIALTY, L"specialty",       false, false, 0.40, pt);
    v.push_back(mkField(lx+colW-8.0, y+5*rH+0.3, 7.0, fh, L"specialtycode", L"", pt-1.0, 1));
    return y+n*rH;
}

static double emitSamenFinance(std::vector<PrintItem>& v,double x,double y,double w,
                               double rH,double pt,double bw){
    double half=w/2.0, fh=rH-0.7;
    auto row2=[&](const wchar_t* l1,const wchar_t* f1,bool b1,
                  const wchar_t* l2,const wchar_t* f2){
        v.push_back(mkVLine(x+half, y, rH, bw));
        putLF(v, x+half+0.5, y+0.3, half-1.0, fh, l1, f1, pt, b1, false, 0.44);
        putLF(v, x+0.5,      y+0.3, half-1.0, fh, l2, f2, pt, false, false, 0.44);
        y+=rH; v.push_back(mkHLine(x, y, w, bw));
    };
    /* v2.06: the complete financial footer block requested by the user —
       پرداختی، قیمت کل، سهم پایه، سهم مکمل، تخفیف از، تخفیف، تخفیف نقد،
       POS — all BELOW the services table, never as table columns. */
    row2(FA_PAID,     L"paid",          true,  FA_TOTAL,    L"total");
    row2(FA_INSSHARE, L"insshare",      false, FA_SUPPPAY,  L"supppay");
    row2(FA_DISCFROM, L"discount_from", false, FA_DISCOUNT, L"discount");
    row2(FA_CASHDISC, L"cash",          false, FA_POS,      L"pos");
    return y;
}

static double emitDualFinance(std::vector<PrintItem>& v,double x,double y,double w,
                              double rH,double pt,double bw){
    double gap=2.5, bw2=(w-gap)/2.0, capH=5.2, fh=rH-0.7;
    double lx=x, rx=x+bw2+gap;
    v.push_back(mkLabel(lx+0.5, y+0.3, bw2-1.0, capH-0.6, FA_PAYLBL,   pt-0.5, true, 1));
    v.push_back(mkLabel(rx+0.5, y+0.3, bw2-1.0, capH-0.6, FA_SHARELBL, pt-0.5, true, 1));
    v.push_back(mkHLine(lx, y+capH, bw2, bw));
    v.push_back(mkHLine(rx, y+capH, bw2, bw));
    double iy=y+capH;
    /* v2.06: same complete footer field set as emitSamenFinance — the whole
       money block lives below the table, never inside it. */
    putLF(v, lx+0.5, iy+0.3,        bw2-1.0, fh, FA_PAID,     L"paid",          pt, true,  false, 0.44);
    v.push_back(mkHLine(lx, iy+rH, bw2, bw));
    putLF(v, lx+0.5, iy+rH+0.3,     bw2-1.0, fh, FA_TOTAL,    L"total",         pt, false, false, 0.44);
    v.push_back(mkHLine(lx, iy+2*rH, bw2, bw));
    putLF(v, lx+0.5, iy+2*rH+0.3,   bw2-1.0, fh, FA_DISCFROM, L"discount_from", pt, false, false, 0.44);
    v.push_back(mkHLine(lx, iy+3*rH, bw2, bw));
    putLF(v, lx+0.5, iy+3*rH+0.3,   bw2-1.0, fh, FA_DISCOUNT, L"discount",      pt, false, false, 0.44);
    putLF(v, rx+0.5, iy+0.3,        bw2-1.0, fh, FA_INSSHARE, L"insshare",      pt, false, false, 0.44);
    v.push_back(mkHLine(rx, iy+rH, bw2, bw));
    putLF(v, rx+0.5, iy+rH+0.3,     bw2-1.0, fh, FA_SUPPPAY,  L"supppay",       pt, false, false, 0.44);
    v.push_back(mkHLine(rx, iy+2*rH, bw2, bw));
    putLF(v, rx+0.5, iy+2*rH+0.3,   bw2-1.0, fh, FA_CASHDISC, L"cash",          pt, true,  false, 0.36);
    v.push_back(mkHLine(rx, iy+3*rH, bw2, bw));
    putLF(v, rx+0.5, iy+3*rH+0.3,   bw2-1.0, fh, FA_POS,      L"pos",           pt, false, false, 0.30);
    double bottom=y+capH+4*rH;
    v.push_back(mkRect(lx, y, bw2, capH+4*rH, bw, 0));
    v.push_back(mkRect(rx, y, bw2, capH+4*rH, bw, 0));
    v.push_back(mkHLine(x, bottom, w, bw));
    return bottom;
}

static double emitSamenRef(std::vector<PrintItem>& v,double x,double y,double w,
                           double rH,double pt,double bw){
    double fh=rH-0.7;
    putLF(v, x+0.5, y+0.3, w-1.0, fh, FA_EPRESC,   L"eprescription", pt-0.5, false, false, 0.56);
    y+=rH; v.push_back(mkHLine(x, y, w, bw));
    putLF(v, x+0.5, y+0.3, w-1.0, fh, FA_REFERRAL, L"referralno",    pt-0.5, false, false, 0.42);
    y+=rH;
    return y;
}

static double emitSamenFooter(std::vector<PrintItem>& v,double x,double y,double w,
                              double pt){
    double fh=4.6, half=w/2.0;
    v.push_back(mkHLine(x, y, w, 0.25));
    v.push_back(mkVLine(x+half, y+0.3, fh, 0.25));
    putLF(v, x+half+0.4, y+0.5, half-0.8, fh, FA_RECEPTION, L"receptionist", pt-1.0, false, false, 0.36);
    putLF(v, x+0.4,      y+0.5, half-0.8, fh, FA_CASHIER,   L"cashier",      pt-1.0, false, false, 0.36);
    if(!v.empty()) v.back().suffix=FA_CASH_SFX;
    y+=fh+0.8;
    putLF(v, x+half+0.4, y, half-0.8, fh, FA_SC, L"scnum", pt-1.0, false, false, 0.28);
    v.push_back(mkField(x+0.4, y, half-0.8, fh, L"reg_ts", L"", pt-1.5, 0));
    return y+fh+0.4;
}

static double emitStub(std::vector<PrintItem>& v,double x,double y,double w,double pt){
    v.push_back(mkHLine(x, y, w, 0.3));
    v.push_back(mkLabel(x, y+0.4, w, 3.6, FA_TEAR, pt-1.5, false, 1));
    y+=4.6;
    v.push_back(mkLabel(x, y, w, 4.8, FA_STUB, pt, true, 1));
    y+=5.2;
    double rH=4.8, fh=4.2, half=w/2.0;
    v.push_back(mkRect(x, y, w, 2*rH, 0.3, 0));
    v.push_back(mkVLine(x+half, y, 2*rH, 0.3));
    putLF(v, x+half+0.5, y+0.3, half-1.0, fh, FA_FULL,  L"full",  pt-1.0, true, false, 0.40);
    putLF(v, x+0.5,      y+0.3, half-1.0, fh, FA_QUEUE, L"queue", pt-1.0, false, false, 0.36);
    v.push_back(mkHLine(x, y+rH, w, 0.3));
    putLF(v, x+0.5, y+rH+0.3, w-1.0, fh, FA_PAID, L"paid", pt, true, true, 0.28);
    return y+2*rH+1.0;
}

static double paperMargin(double pw){
    if(pw<=80.0) return 3.0;
    if(pw<=148.0) return 6.0;
    return 8.0;
}

// ===========================================================================
//  Index 0 — NEVER-deletable «پیش‌فرض» exact thermal R80 Samen receipt.
// ===========================================================================
static void buildSamenDefault(PrintDesign& d){
    const double PW=80.0, PH=200.0, M=3.0, bw=0.30, pt=8.0, rH=5.2;
    d.paper=L"R80"; d.paperW=PW; d.paperH=PH; d.orientation=0;
    std::vector<PrintItem>& v=d.items;
    double x=M, w=PW-2*M, y=M;

    // HEADER (no border, centered)
    v.push_back(mkLabel(x, y, w, 6.4, FA_CLINIC, 11.0, true, 1)); y+=6.6;
    v.push_back(mkField(x, y, w, 4.0, L"clinicaddr",  FA_ADDR,  7.0, 1)); y+=4.2;
    v.push_back(mkField(x, y, w, 3.8, L"clinicphone", FA_PHONE, 7.0, 1)); y+=4.2;

    // TITLE+BARCODE row: right title, left Code128 ~50×8 mm
    { double bcW=50.0, bcH=8.0;
      v.push_back(mkBarcode(x, y+0.2, bcW, bcH));
      v.push_back(mkLabel(x+bcW+1.0, y+1.0, w-bcW-1.0, 6.4, FA_DOC_REC, 9.5, true, 0));
      y+=bcH+1.0; }

    double footH=10.5;
    double boxTop=y, boxBot=PH-M-footH, boxH=boxBot-boxTop;
    double ix=x+0.8, iw=w-1.6, iy=boxTop+0.8;
    iy=emitSamenInfo(v, ix, iy, iw, rH, pt, bw);
    double finRefH=6*rH;
    double svcH=boxBot-0.8-iy-finRefH;
    if(svcH<20.0) svcH=20.0;
    v.push_back(mkServices(ix, iy, iw, svcH, 7.0, SVC_SAMEN, 0, bw, 4.6, 5.0));
    iy+=svcH;
    iy=emitSamenFinance(v, ix, iy, iw, rH, pt, bw);
    iy=emitSamenRef(v, ix, iy, iw, rH, pt, bw);
    (void)iy;
    v.push_back(mkFrameBox(x, boxTop, w, boxH, bw));
    emitSamenFooter(v, x, boxBot+0.4, w, pt);
}

// ===========================================================================
//  Indices 1..30 — 10 layout families × 3 variants, mixed papers.
// ===========================================================================
struct ExtraSpec {
    int layout;     // 0..9
    int variant;    // 0..2
    int svc;
    const wchar_t* paper;
    double bw;
    double rowH;
    bool pageFrame;
    bool photo;
    bool logo;
    bool tear;
};

static const ExtraSpec EXTRA[30] = {
  /* 01 */ {0,0, SVC3,      L"R80", 0.30, 5.2, false, false, false, false},
  /* 02 */ {0,1, SVC4_ROW,  L"R80", 0.30, 5.0, true,  false, false, false},
  /* 03 */ {0,2, SVC5,      L"A5",  0.30, 5.6, true,  false, false, false},
  /* 04 */ {1,0, SVC3,      L"R80", 0.30, 5.0, false, false, false, false},
  /* 05 */ {1,1, SVC4_CAT,  L"R58", 0.28, 4.8, false, false, false, false},
  /* 06 */ {1,2, SVC5_CODE, L"A5",  0.30, 5.6, true,  false, false, false},
  /* 07 */ {2,0, SVC6_FIN,  L"R80", 0.30, 5.0, false, false, false, false},
  /* 08 */ {2,1, SVC6_INS,  L"A5",  0.30, 5.4, true,  false, false, false},
  /* 09 */ {2,2, SVC7,      L"A4",  0.30, 5.8, true,  false, false, false},
  /* 10 */ {3,0, SVC3,      L"A5",  0.30, 5.4, false, false, true,  false},
  /* 11 */ {3,1, SVC4_ROW,  L"A4",  0.30, 5.8, true,  false, true,  false},
  /* 12 */ {3,2, SVC5,      L"A4",  0.30, 6.0, false, true,  true,  false},
  /* 13 */ {4,0, SVC3,      L"A5",  0.25, 5.4, false, false, false, false},
  /* 14 */ {4,1, SVC4_ROW,  L"A4",  0.25, 5.8, true,  false, false, false},
  /* 15 */ {4,2, SVC5,      L"A4",  0.25, 6.0, false, true,  false, false},
  /* 16 */ {5,0, SVC3,      L"R80", 0.30, 5.0, false, false, false, false},
  /* 17 */ {5,1, SVC4_CAT,  L"A5",  0.30, 5.4, false, false, false, false},
  /* 18 */ {5,2, SVC6_INS,  L"A4",  0.30, 5.8, true,  false, false, false},
  /* 19 */ {6,0, SVC3,      L"R58", 0.28, 4.8, false, false, false, false},
  /* 20 */ {6,1, SVC5,      L"R80", 0.30, 5.2, false, false, false, false},
  /* 21 */ {6,2, SVC5_CODE, L"A5",  0.30, 5.6, true,  false, false, false},
  /* 22 */ {7,0, SVC3,      L"R58", 0.25, 4.6, false, false, false, false},
  /* 23 */ {7,1, SVC4_ROW,  L"R58", 0.28, 4.8, true,  false, false, false},
  /* 24 */ {7,2, SVC5,      L"R80", 0.30, 5.0, false, false, false, false},
  /* 25 */ {8,0, SVC3,      L"A5",  0.30, 5.2, false, false, false, true },
  /* 26 */ {8,1, SVC4_ROW,  L"A4",  0.30, 5.4, true,  false, false, true },
  /* 27 */ {8,2, SVC5,      L"A4",  0.30, 5.6, false, false, false, true },
  /* 28 */ {9,0, SVC3,      L"A5",  0.30, 5.4, false, false, false, false},
  /* 29 */ {9,1, SVC6_FIN,  L"A4",  0.30, 5.8, true,  false, false, false},
  /* 30 */ {9,2, SVC5_CODE, L"A4",  0.30, 6.0, true,  false, false, false},
};

static const wchar_t* const TPL_NAMES[31]={
    L"پیش‌فرض",
    L"۰۱) قبض پزشک — رول ۸۰ کلاسیک",
    L"۰۲) رسید بیمه — رول ۸۰ قاب‌دار",
    L"۰۳) قبض و رسید — A5 کلاسیک",
    L"۰۴) قبض پزشک — رول ۸۰ خدمات‌بالا",
    L"۰۵) رسید بیمه — رول ۵۸ خدمات‌بالا",
    L"۰۶) قبض و رسید — A5 خدمات‌بالا",
    L"۰۷) قبض پزشک — رول ۸۰ مالی‌اول",
    L"۰۸) رسید بیمه — A5 مالی‌اول",
    L"۰۹) قبض و رسید — A4 مالی‌اول",
    L"۱۰) قبض پزشک — A5 ستون کناری",
    L"۱۱) رسید بیمه — A4 ستون کناری",
    L"۱۲) قبض و رسید — A4 ستون و عکس",
    L"۱۳) قبض پزشک — A5 دو ستونه",
    L"۱۴) رسید بیمه — A4 دو ستونه",
    L"۱۵) قبض و رسید — A4 دو ستونه عکس",
    L"۱۶) قبض پزشک — رول ۸۰ با عنوان بخش",
    L"۱۷) رسید بیمه — A5 با عنوان بخش",
    L"۱۸) قبض و رسید — A4 با عنوان بخش",
    L"۱۹) قبض پزشک — رول ۵۸ بارکد پایین",
    L"۲۰) رسید بیمه — رول ۸۰ بارکد پایین",
    L"۲۱) قبض و رسید — A5 بارکد پایین",
    L"۲۲) قبض پزشک — رول ۵۸ فشرده",
    L"۲۳) رسید بیمه — رول ۵۸ فشرده قاب‌دار",
    L"۲۴) قبض و رسید — رول ۸۰ فشرده",
    L"۲۵) قبض پزشک — A5 ته‌برگ",
    L"۲۶) رسید بیمه — A4 ته‌برگ",
    L"۲۷) قبض و رسید — A4 ته‌برگ",
    L"۲۸) قبض پزشک — A5 دو بلوک مالی",
    L"۲۹) رسید بیمه — A4 دو بلوک مالی",
    L"۳۰) قبض و رسید — A4 دو بلوک مالی قاب‌دار",
};

static void buildExtra(PrintDesign& d, int extraIdx){
    if(extraIdx<0||extraIdx>=30) extraIdx=0;
    const ExtraSpec& sp=EXTRA[extraIdx];
    double PW=210.0, PH=297.0;
    Paper_Dims(sp.paper, PW, PH);
    d.paper=sp.paper; d.paperW=PW; d.paperH=PH; d.orientation=0;
    std::vector<PrintItem>& v=d.items;
    const int lay=sp.layout, var=sp.variant;
    double M=paperMargin(PW);
    double bw=sp.bw, rH=sp.rowH;
    double pt=(PW<=58.0)?7.2:(PW<=80.0)?8.2:(PW<=148.0)?9.0:9.5;
    double bcW=(PW<=58.0)?44.0:(PW<=80.0)?52.0:(PW<=148.0)?64.0:72.0;
    double bcH=(PW<=80.0)?7.0:7.0;
    double footH=(PW<=80.0)?11.0:13.5;
    double stubH=sp.tear?16.0:0.0;
    double x=M, w=PW-2*M, y=M;
    double yMax=PH-M-footH-stubH;
    const std::wstring title=(var==0)?FA_DOC_REC:(var==1)?FA_INS_REC:FA_DOC_BOTH;
    if(sp.pageFrame) v.push_back(mkFrame(PW, PH, (M>3.0)?(M-1.5):2.0, R_INK, 0.3));

    auto caption=[&](const wchar_t* txt){
        v.push_back(mkHLine(x, y, w, 0.3));
        v.push_back(mkLabel(x+0.6, y+0.2, w-1.2, 4.0, txt, pt-1.0, true, 1));
        v.push_back(mkHLine(x, y+4.4, w, 0.3));
        y+=4.8;
    };
    auto emitSvcAt=[&](double svcH){
        v.push_back(mkServices(x, y, w, svcH, pt-0.8, sp.svc, 0, bw, rH-0.8, rH-0.5));
        y+=svcH;
    };
    auto emitBc=[&](){ y=emitBarcode(v, x, y, w, bcW, bcH); };

    if(lay==3 && sp.logo){
        double sbW=(PW>=210.0)?40.0:30.0;
        double sbX=x, mainX=x+sbW+3.0, mainW=w-sbW-3.0;
        double sy=y;
        v.push_back(mkLogo(sbX+(sbW-18.0)/2.0, sy, 18.0, 18.0)); sy+=20.0;
        v.push_back(mkLabel(sbX+0.4, sy, sbW-0.8, 10.0, FA_CLINIC, pt, true, 1)); sy+=11.0;
        v.push_back(mkField(sbX+0.4, sy, sbW-0.8, 10.0, L"clinicaddr", FA_ADDR, pt-2.0, 1)); sy+=11.0;
        v.push_back(mkField(sbX+0.4, sy, sbW-0.8, 5.0, L"clinicphone", FA_PHONE, pt-2.0, 1)); sy+=6.0;
        sy=emitBarcode(v, sbX+0.4, sy, sbW-0.8, (sbW-2.0<bcW)?(sbW-2.0):bcW, bcH);
        v.push_back(mkRect(sbX, y, sbW, sy-y+1.0, bw, 0));
        double mx=mainX, mw=mainW, my=y;
        if(sp.photo){ v.push_back(mkPhoto(mx+mw-16.0, my, 15.0, 18.0)); }
        v.push_back(mkLabel(mx, my, mw-(sp.photo?17.0:0), 6.0, title, pt+1.5, true, 1)); my+=7.0;
        my=emitSamenInfo(v, mx, my, mw, rH, pt, bw);
        double remain=yMax-my-6*rH; if(remain<22.0) remain=22.0;
        if(var==1) remain+=2.0; else if(var==2) remain+=4.0;
        v.push_back(mkServices(mx, my, mw, remain, pt-0.8, sp.svc, 0, bw, rH-0.8, rH-0.5));
        my+=remain;
        my=emitSamenFinance(v, mx, my, mw, rH, pt, bw);
        my=emitSamenRef(v, mx, my, mw, rH, pt, bw);
        v.push_back(mkRect(mx, y, mw, my-y, bw, 0));
        emitSamenFooter(v, mx, (my+1.0<PH-M-stubH)?my+1.0:PH-M-stubH-footH, mw, pt);
        return;
    }

    y=emitHeader(v, x, y, w, pt, true, sp.photo);
    y=emitTitle(v, x, y, w, pt, title);

    double svcH=(PW<=58.0)?22.0:(PW<=80.0)?28.0:(PW<=148.0)?36.0:42.0;
    if(var==1) svcH+=2.0; else if(var==2) svcH+=4.0;

    if(lay==1){
        emitSvcAt(svcH); emitBc();
        y=emitSamenInfo(v, x, y, w, rH, pt, bw);
        y=emitSamenFinance(v, x, y, w, rH, pt, bw);
        y=emitSamenRef(v, x, y, w, rH, pt, bw);
    } else if(lay==2){
        y=emitSamenFinance(v, x, y, w, rH, pt, bw);
        y=emitSamenInfo(v, x, y, w, rH, pt, bw);
        emitSvcAt(svcH); emitBc();
        y=emitSamenRef(v, x, y, w, rH, pt, bw);
    } else if(lay==4){
        y=emitSamenInfoSplit(v, x, y, w, rH, pt, bw);
        emitSvcAt(svcH); emitBc();
        y=emitSamenFinance(v, x, y, w, rH, pt, bw);
        y=emitSamenRef(v, x, y, w, rH, pt, bw);
    } else if(lay==5){
        caption(L"مشخصات بیمار");
        y=emitSamenInfo(v, x, y, w, rH, pt, bw);
        caption(FA_SVCLIST);
        emitSvcAt(svcH); emitBc();
        caption(L"مالی و پرداخت");
        y=emitSamenFinance(v, x, y, w, rH, pt, bw);
        y=emitSamenRef(v, x, y, w, rH, pt, bw);
    } else if(lay==6){
        y=emitSamenInfo(v, x, y, w, rH, pt, bw);
        emitSvcAt(svcH);
        y=emitSamenFinance(v, x, y, w, rH, pt, bw);
        y=emitSamenRef(v, x, y, w, rH, pt, bw);
        emitBc();
    } else if(lay==7){
        y=emitSamenInfo(v, x, y, w, rH, pt-0.4, bw);
        emitSvcAt(svcH);
        y=emitSamenFinance(v, x, y, w, rH, pt-0.4, bw);
        y=emitSamenRef(v, x, y, w, rH, pt-0.4, bw);
        emitBc();
    } else if(lay==9){
        y=emitSamenInfo(v, x, y, w, rH, pt, bw);
        emitSvcAt(svcH); emitBc();
        y=emitDualFinance(v, x, y, w, rH, pt, bw);
        y=emitSamenRef(v, x, y, w, rH, pt, bw);
    } else {
        // 0 classic / 8 tear-off: info, services, barcode, finance, ref
        y=emitSamenInfo(v, x, y, w, rH, pt, bw);
        emitSvcAt(svcH); emitBc();
        y=emitSamenFinance(v, x, y, w, rH, pt, bw);
        y=emitSamenRef(v, x, y, w, rH, pt, bw);
    }

    double footY=y+1.0;
    double footLimit=PH-M-stubH-11.0;
    if(footY>footLimit) footY=footLimit;
    y=emitSamenFooter(v, x, footY, w, pt);
    if(sp.tear) emitStub(v, x, y+0.6, w, pt);
}

static PrintDesign buildTemplate(int idx){
    g_tplItemId = 1;
    PrintDesign d; d.kind=L"builtin"; d.orientation=0;
    if(idx<0||idx>=31) idx=0;
    d.name = TPL_NAMES[idx];
    if(idx==0) buildSamenDefault(d);
    else buildExtra(d, idx-1);
    return d;
}

} // namespace

static std::wstring buildTemplateName(int idx){
    if(idx<0||idx>=31) return L"";
    return TPL_NAMES[idx];
}

// Seed 31 builtins (index 0 = undeletable Default) and migrate existing
// installs. The v2.00 one-shot flag rebuilds every builtin from
// buildTemplate(idx) while preserving stored ids / bindings.
void Designs_Init(){
    std::vector<PrintDesign> existing; Designs_Builtins(existing);
    const int N = 31;
    auto stamp=[](){
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
    // v2.00 migration: rebuild all 31 builtins (Default + 30 extras).
    if(getSetting(L"tpl_migration_2_00", L"0") != L"1"){
        for(int i=0;i<N;i++){
            PrintDesign fresh=buildTemplate(i);
            if(i<(int)existing.size()){ fresh.id=existing[i].id; Designs_Update(fresh); }
            else Designs_Insert(fresh);
        }
        for(size_t i=N;i<existing.size();++i) Designs_Delete(existing[i].id);
        stamp();
    }
    // v2.06 migration — the services-table policy changed: the table now has
    // EXACTLY three columns (نام خدمت / تعداد / شرح خدمت) and all money data
    // moved to the footer blocks. Rebuild every builtin AND migrate any
    // user-saved design whose services model still carries the old columns.
    if(getSetting(L"tpl_migration_2_06", L"0") != L"1"){
        for(int i=0;i<N;i++){
            PrintDesign fresh=buildTemplate(i);
            if(i<(int)existing.size()){ fresh.id=existing[i].id; Designs_Update(fresh); }
            else Designs_Insert(fresh);
        }
        /* migrate user-saved designs (id >= 31) that still hold an old model */
        for(size_t i=N;i<existing.size();++i){
            PrintDesign d=existing[i];
            bool changed=false;
            for(auto& it:d.items){
                if(it.type!=PIT_SERVICES) continue;
                std::wstring t=it.text;
                if(t.find(L"نام خدمت")!=std::wstring::npos &&
                   (t.find(L"مبلغ کل")!=std::wstring::npos ||
                    t.find(L"قیمت کل")!=std::wstring::npos ||
                    t.find(L"سهم پایه")!=std::wstring::npos ||
                    t.find(L"سهم بیمه")!=std::wstring::npos ||
                    t.find(L"مبلغ واحد")!=std::wstring::npos ||
                    t.find(L"کد خدمت")!=std::wstring::npos ||
                    t.find(L"ردیف")!=std::wstring::npos ||
                    t.find(L"نوع خدمت")!=std::wstring::npos)){
                    it.text=svcModelJson(SVC3);
                    changed=true;
                }
            }
            if(changed) Designs_Update(d);
        }
        stamp();
    }
}

PrintDesign Design_BuiltinTemplate(int idx){ return buildTemplate(idx); }
