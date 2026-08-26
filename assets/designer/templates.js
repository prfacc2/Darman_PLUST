/* ===========================================================================
   templates.js — DarmanPlus ready-made print designs  (v1.62.0 — FULL REWRITE)

   ★ این فایل دقیقاً همان ۳۰ قالب داخلی C++ (src/print_designer_templates.inc)
     است تا گالری دیزاینر وب با آنچه موتور چاپ واقعاً seed می‌کند یکی باشد.
     هر تغییری در .inc باید عیناً اینجا هم اعمال شود.

   مرکز هر ۳۰ قالب: جدول پویای خدمات (type:"services") که هنگام چاپ از
   ReceptionRecord.services پر می‌شود — ردیف/نام خدمت/شرح/تعداد/مبلغ، سطر به
   سطر و زنده، مستقیم از مدیریت ← خدمات. نه لیبل ثابت، نه متن آزمایشی.

   v1.62.0:
     • ده خانوادهٔ چیدمان کاملاً متفاوت × ۳ گونه = ۳۰ طرح واقعاً متمایز
     • قاب امن جدول از فضای آزاد صفحه محاسبه می‌شود؛ موتور چاپ فقط سطرهای
       واقعی را می‌کشد و فضای استفاده‌نشدهٔ صفحه را خالی می‌گذارد
     • هشت پیش‌تنظیم ستونی (۴ تا ۷ ستون) با عنوان‌های تأییدشده در pdSvcColOf

   v1.69.0 (تمایز بصری هر ۳۰ طرح):
     • هر گونه در هر خانواده حالا متا/بلاک بیمار/جمع‌بندی/پاورقی مخصوص خود
       را دارد تا هر ۳۰ طرح اساسی متفاوت باشند، نه فقط تعویض رنگ.
     • تنوع بارکد: ۲۱ طرح یک بارکد Code128 (چهار سبک پاورقی)، ۹ طرح بدون
       کد (footClean). هر صفحه نهایتًا یک کد (۰ یا ۱، هرگز دو).
     •compact و درون کادر A4 تا pscale موتور چاپ آن را روی A5/A6/رول کوچک
       متناسب و بدون برش کوچک کند.
   =========================================================================== */
(function () {
  "use strict";

  var INK      = "#000000";
  var RULE     = "#000000";
  var RULE_DIM = "#8a8a8a";
  var PAPER_BG = "#ffffff";

  /* ------------------------------------------------- A4 page geometry (mm) */
  var PG_W   = 210.0;
  var PG_H   = 297.0;
  var PG_M   = 12.0;
  var PG_CW  = PG_W - 2 * PG_M;      /* 186 */
  var FOOT_Y = PG_H - 34.0;          /* 263 */

  var _uid = 0;
  function nid() { return ++_uid; }

  function base() {
    return {
      id: nid(), type: "label", x: 10, y: 10, w: 40, h: 7, rot: 0, z: 1,
      locked: false, isFrame: false, text: "", field: "", prefix: "", suffix: "",
      font: "Vazirmatn", pt: 10, bold: false, italic: false,
      align: 0, dir: 0, valign: 1, lineSpacing: 1.25,
      textColor: INK, fillColor: PAPER_BG, fillTransparent: true,
      borderColor: RULE, borderWidth: 0.3, corner: 0, padding: 0.8, opacity: 1,
      visibility: 0, imgPath: "",
      rowH: 0, headerH: 0
    };
  }
  function extend(dst, src) {
    var k;
    if (!src) return dst;
    for (k in src) if (Object.prototype.hasOwnProperty.call(src, k)) dst[k] = src[k];
    return dst;
  }
  function mk(o) { return extend(base(), o); }

  /* ---------------------------------------------------------- primitives */
  function L(x, y, w, h, text, pt, bold, align, o) {
    return mk(extend({ type: "label", x: x, y: y, w: w, h: h, text: text,
      pt: pt, bold: !!bold, align: align, dir: (align === 1 ? 2 : 0) }, o || {}));
  }
  function LC(x, y, w, h, text, pt, bold, align, colour) {
    return L(x, y, w, h, text, pt, bold, align, { textColor: colour });
  }
  function F(x, y, w, h, field, prefix, pt, align, o) {
    return mk(extend({ type: "field", x: x, y: y, w: w, h: h,
      field: field, prefix: prefix || "", pt: pt, align: align,
      dir: (align === 1 ? 2 : 0) }, o || {}));
  }
  function FB(x, y, w, h, field, prefix, pt, align, o) {
    return F(x, y, w, h, field, prefix, pt, align, extend({ bold: true }, o || {}));
  }
  function HL(x, y, w, bw, col) {
    return mk({ type: "hline", x: x, y: y, w: w, h: 0.2, borderWidth: bw,
      borderColor: col || RULE });
  }
  function VL(x, y, h, bw) {
    return mk({ type: "vline", x: x, y: y, w: 0.2, h: h, borderWidth: bw });
  }
  function RECT(x, y, w, h, bw, corner, border) {
    return mk({ type: "rect", x: x, y: y, w: w, h: h, borderWidth: bw,
      corner: corner, fillTransparent: true, borderColor: border || RULE });
  }
  function BAND(x, y, w, h, fill) {
    return mk({ type: "rect", x: x, y: y, w: w, h: h, fillColor: fill,
      fillTransparent: false, borderColor: fill, borderWidth: 0 });
  }
  function TINT(x, y, w, h, fill, border, bw, corner) {
    return mk({ type: "rect", x: x, y: y, w: w, h: h, fillColor: fill,
      fillTransparent: false, borderColor: border, borderWidth: bw,
      corner: corner });
  }
  function FRAME(pw, ph, m, col, bw) {
    return mk({ type: "frame", x: m, y: m, w: pw - 2 * m, h: ph - 2 * m,
      isFrame: true, borderColor: col, borderWidth: bw, fillTransparent: true });
  }
  function LOGO(x, y, w, h) {
    return mk({ type: "logo", x: x, y: y, w: w, h: h,
      borderColor: RULE_DIM, borderWidth: 0.3 });
  }
  function QR(x, y, s) {
    return mk({ type: "qr", x: x, y: y, w: s, h: s, field: "receiptNo",
      borderColor: RULE_DIM, borderWidth: 0.3 });
  }
  function PHOTO(x, y, w, h) {
    return mk({ type: "photo", x: x, y: y, w: w, h: h,
      borderColor: RULE_DIM, borderWidth: 0.3 });
  }
  function BARCODE(x, y, w, h) {
    return mk({ type: "barcode", x: x, y: y, w: w, h: h,
      field: "receiptbarcode",
      text: JSON.stringify({ sym: "code128", hri: true, quiet: 2 }),
      pt: 8, align: 1, dir: 1, textColor: INK, borderWidth: 0 });
  }

  /* ------------------------------- THE CORE: live services table presets */
  /*  عنوان هر ستون معنایش را تعیین می‌کند (printer.cpp::pdSvcColOf)، پس
      کاربر می‌تواند ستون‌ها را جابه‌جا یا حذف کند و داده باز هم درست بنشیند. */
  var SVC3 = 0, SVC4_ROW = 1, SVC4_CAT = 2, SVC5 = 3, SVC5_CODE = 4,
      SVC6_FIN = 5, SVC6_INS = 6, SVC7 = 7;

  function svcModel(preset) {
    switch (preset) {
      case SVC4_ROW: return { cols: 5, header: true,
        widths: [0.07, 0.36, 0.25, 0.10, 0.22],
        labels: ["ردیف", "نام خدمت", "شرح خدمت", "تعداد", "مبلغ کل"] };
      case SVC4_CAT: return { cols: 5, header: true,
        widths: [0.32, 0.18, 0.22, 0.09, 0.19],
        labels: ["نام خدمت", "نوع خدمت", "شرح خدمت", "تعداد", "مبلغ کل"] };
      case SVC5: return { cols: 5, header: true,
        widths: [0.07, 0.36, 0.25, 0.10, 0.22],
        labels: ["ردیف", "نام خدمت", "شرح خدمت", "تعداد", "مبلغ کل"] };
      case SVC5_CODE: return { cols: 6, header: true,
        widths: [0.06, 0.12, 0.29, 0.22, 0.09, 0.22],
        labels: ["ردیف", "کد خدمت", "نام خدمت", "شرح خدمت", "تعداد", "مبلغ کل"] };
      case SVC6_FIN: return { cols: 7, header: true,
        widths: [0.05, 0.27, 0.20, 0.08, 0.14, 0.11, 0.15],
        labels: ["ردیف", "نام خدمت", "شرح خدمت", "تعداد", "مبلغ واحد", "تخفیف", "مبلغ کل"] };
      case SVC6_INS: return { cols: 7, header: true,
        widths: [0.05, 0.26, 0.20, 0.08, 0.15, 0.13, 0.13],
        labels: ["ردیف", "نام خدمت", "شرح خدمت", "تعداد", "مبلغ کل", "سهم بیمه", "سهم بیمار"] };
      case SVC7: return { cols: 7, header: true,
        widths: [0.05, 0.11, 0.27, 0.20, 0.08, 0.14, 0.15],
        labels: ["ردیف", "کد خدمت", "نام خدمت", "شرح خدمت", "تعداد",
                 "مبلغ واحد", "مبلغ کل"] };
      default: return { cols: 4, header: true,
        widths: [0.39, 0.29, 0.10, 0.22],
        labels: ["نام خدمت", "شرح خدمت", "تعداد", "مبلغ کل"] };
    }
  }

  function SERVICES(x, y, w, h, pt, preset, headFill, bw, rowH, headerH) {
    return mk({
      type: "services", x: x, y: y, w: w, h: h, pt: pt, align: 1, dir: 2,
      borderColor: RULE, borderWidth: bw, textColor: INK,
      fillColor: headFill ? headFill : PAPER_BG,
      fillTransparent: !headFill,
      padding: 0.8, headerH: headerH, rowH: rowH,
      text: JSON.stringify(svcModel(preset))
    });
  }

  /* ------------------------------------------------------------- captions */
  /* v1.95.0 — thermal medical receipt captions (mirror the C++ .inc).       */
  var FA_CLINIC    = "درمانگاه شبانه‌روزی درمان پلاس";
  var FA_SUBTITLE  = "سامانه پذیرش و مدیریت درمانگاه";
  var FA_PHONE     = "تلفن: ";
  var FA_ADDR      = "نشانی: ";
  var FA_APPT      = "تاریخ نوبت: ";
  var FA_QUEUE     = "نوبت: ";
  var FA_NID       = "کد ملی: ";
  var FA_BARCODE   = "بارکد: ";
  var FA_BASEINS   = "بیمه پایه: ";
  var FA_SUPP      = "مکمل: ";
  var FA_FULL      = "نام بیمار: ";
  var FA_AGE       = "سن: ";
  var FA_DOCTOR    = "دکتر: ";
  var FA_DOCCODE   = "کد نظام پزشکی: ";
  var FA_SPECCODE  = "کد تخصص: ";
  var FA_SPECIALTY = "شرح تخصص: ";
  var FA_PAID      = "پرداختی: ";
  var FA_TOTAL     = "قیمت کل: ";
  var FA_INSSHARE  = "سهم پایه: ";
  var FA_SUPPPAY   = "سهم مکمل: ";
  var FA_DISCOUNT  = "تخفیف از: ";
  var FA_POS       = "POS: ";
  var FA_CASH      = "نقد: ";
  var FA_PATSHARE  = "سهم بیمار: ";
  var FA_FINAL     = "مبلغ نهایی: ";
  var FA_EPRESC    = "کد رهگیری نسخه الکترونیک: ";
  var FA_REFERRAL  = "شماره معرف نسخه: ";
  var FA_RECEPTION = "پذیرش: ";
  var FA_CASHIER   = "صندوق: ";
  var FA_SC        = "ش.ص: ";
  var FA_PRINTTS   = "تاریخ چاپ: ";
  var FA_SVCLIST   = "شرح خدمات";
  var FA_TEAR      = "— — — محل جدا کردن — — —";
  var FA_STUB      = "نسخهٔ بیمار";
  var FA_PAYLBL    = "پرداخت‌ها";
  var FA_SHARELBL  = "سهم بیمه";
  var FA_DOC_REC   = "قبض پزشک";
  var FA_INS_REC   = "رسید بیمه";
  var FA_DOC_BOTH  = "قبض و رسید پزشکی";
  var FA_RECEIPT   = "شماره قبض: ";

  /* ----- medical receipt geometry (8 mm margins, portrait A4) ------------- */
  var R_M   = 8.0;
  var R_CW  = PG_W - 2 * R_M;        /* 194 */
  var FR_M  = 5.0;                   /* page-frame margin (outside the box) */
  var R_INK   = "#000000";
  var R_SHADE = "#ECECEC";           /* very light gray (monochrome shading) */

  /* ============================================ 30 designs / 10 families == */
  /* family, variant, svc preset, accent, tint, headFill(0=line-art), bw, rowH, frame */
  var TPL = [
    { f: 0, v: 0, s: SVC3,      a: "#000000", t: "#FFFFFF", hf: 0,          bw: 0.30, rh: 6.2, fr: false },
    { f: 0, v: 1, s: SVC4_ROW,  a: "#000000", t: "#FFFFFF", hf: "#ECECEC",  bw: 0.30, rh: 6.2, fr: false },
    { f: 0, v: 2, s: SVC5,      a: "#000000", t: "#FFFFFF", hf: 0,          bw: 0.30, rh: 6.2, fr: false },
    { f: 1, v: 0, s: SVC3,      a: "#000000", t: "#FFFFFF", hf: "#ECECEC",  bw: 0.30, rh: 6.0, fr: false },
    { f: 1, v: 1, s: SVC4_CAT,  a: "#000000", t: "#FFFFFF", hf: "#ECECEC",  bw: 0.30, rh: 6.0, fr: false },
    { f: 1, v: 2, s: SVC5_CODE, a: "#000000", t: "#FFFFFF", hf: "#ECECEC",  bw: 0.30, rh: 6.0, fr: false },
    { f: 2, v: 0, s: SVC6_FIN,  a: "#000000", t: "#FFFFFF", hf: 0,          bw: 0.30, rh: 5.6, fr: true  },
    { f: 2, v: 1, s: SVC6_INS,  a: "#000000", t: "#FFFFFF", hf: 0,          bw: 0.30, rh: 5.6, fr: true  },
    { f: 2, v: 2, s: SVC7,      a: "#000000", t: "#FFFFFF", hf: 0,          bw: 0.30, rh: 5.6, fr: true  },
    { f: 3, v: 0, s: SVC3,      a: "#000000", t: "#FFFFFF", hf: 0,          bw: 0.30, rh: 5.8, fr: false },
    { f: 3, v: 1, s: SVC4_ROW,  a: "#000000", t: "#FFFFFF", hf: 0,          bw: 0.30, rh: 5.8, fr: false },
    { f: 3, v: 2, s: SVC5,      a: "#000000", t: "#FFFFFF", hf: 0,          bw: 0.30, rh: 5.8, fr: false },
    { f: 4, v: 0, s: SVC3,      a: "#000000", t: "#FFFFFF", hf: 0,          bw: 0.25, rh: 6.0, fr: false },
    { f: 4, v: 1, s: SVC4_ROW,  a: "#000000", t: "#FFFFFF", hf: 0,          bw: 0.25, rh: 6.0, fr: true  },
    { f: 4, v: 2, s: SVC5,      a: "#000000", t: "#FFFFFF", hf: 0,          bw: 0.25, rh: 6.0, fr: false },
    { f: 5, v: 0, s: SVC3,      a: "#000000", t: "#ECECEC", hf: "#ECECEC",  bw: 0.30, rh: 6.0, fr: false },
    { f: 5, v: 1, s: SVC4_CAT,  a: "#000000", t: "#ECECEC", hf: "#ECECEC",  bw: 0.30, rh: 6.0, fr: false },
    { f: 5, v: 2, s: SVC6_INS,  a: "#000000", t: "#ECECEC", hf: "#ECECEC",  bw: 0.30, rh: 6.0, fr: false },
    { f: 6, v: 0, s: SVC3,      a: "#000000", t: "#FFFFFF", hf: 0,          bw: 0.30, rh: 6.0, fr: false },
    { f: 6, v: 1, s: SVC4_CAT,  a: "#000000", t: "#FFFFFF", hf: "#ECECEC",  bw: 0.30, rh: 6.0, fr: false },
    { f: 6, v: 2, s: SVC5,      a: "#000000", t: "#FFFFFF", hf: 0,          bw: 0.30, rh: 6.0, fr: false },
    { f: 7, v: 0, s: SVC6_FIN,  a: "#000000", t: "#FFFFFF", hf: "#ECECEC",  bw: 0.30, rh: 5.6, fr: false },
    { f: 7, v: 1, s: SVC6_INS,  a: "#000000", t: "#FFFFFF", hf: "#ECECEC",  bw: 0.30, rh: 5.6, fr: false },
    { f: 7, v: 2, s: SVC7,      a: "#000000", t: "#FFFFFF", hf: 0,          bw: 0.30, rh: 5.6, fr: true  },
    { f: 8, v: 0, s: SVC3,      a: "#000000", t: "#FFFFFF", hf: 0,          bw: 0.30, rh: 5.2, fr: false },
    { f: 8, v: 1, s: SVC4_ROW,  a: "#000000", t: "#FFFFFF", hf: "#ECECEC",  bw: 0.30, rh: 5.2, fr: false },
    { f: 8, v: 2, s: SVC5,      a: "#000000", t: "#FFFFFF", hf: 0,          bw: 0.30, rh: 5.2, fr: false },
    { f: 9, v: 0, s: SVC3,      a: "#000000", t: "#FFFFFF", hf: "#ECECEC",  bw: 0.30, rh: 6.0, fr: false },
    { f: 9, v: 1, s: SVC6_FIN,  a: "#000000", t: "#FFFFFF", hf: "#ECECEC",  bw: 0.30, rh: 6.0, fr: false },
    { f: 9, v: 2, s: SVC5_CODE, a: "#000000", t: "#FFFFFF", hf: "#ECECEC",  bw: 0.30, rh: 6.0, fr: true  }
  ];

  var NAMES = [
    "۰۱) قبض پزشک — کلاسیک",
    "۰۲) رسید بیمه — کلاسیک باند خاکستری",
    "۰۳) قبض و رسید — کلاسیک دوخطی",
    "۰۴) قبض پزشک — باند سرتیتر",
    "۰۵) رسید بیمه — باند سرتیتر",
    "۰۶) قبض و رسید — باند سرتیتر",
    "۰۷) قبض پزشک — فاکتور فشرده",
    "۰۸) رسید بیمه — فاکتور فشرده",
    "۰۹) قبض و رسید — فاکتور فشرده",
    "۱۰) قبض پزشک — ستون کناری",
    "۱۱) رسید بیمه — ستون کناری",
    "۱۲) قبض و رسید — ستون کناری",
    "۱۳) قبض پزشک — خط‌کشی تک‌رنگ",
    "۱۴) رسید بیمه — خط‌کشی قاب‌دار",
    "۱۵) قبض و رسید — خط‌کشی",
    "۱۶) قبض پزشک — کارتی سایه‌دار",
    "۱۷) رسید بیمه — کارتی سایه‌دار",
    "۱۸) قبض و رسید — کارتی سایه‌دار",
    "۱۹) قبض پزشک — عکس بیمار",
    "۲۰) رسید بیمه — عکس بیمار",
    "۲۱) قبض و رسید — عکس بیمار",
    "۲۲) قبض پزشک — مالی کامل",
    "۲۳) رسید بیمه — مالی کامل",
    "۲۴) قبض و رسید — مالی کامل قاب‌دار",
    "۲۵) قبض پزشک — فشرده ته‌برگ",
    "۲۶) رسید بیمه — فشرده ته‌برگ",
    "۲۷) قبض و رسید — فشرده ته‌برگ",
    "۲۸) قبض پزشک — دو بلوکی",
    "۲۹) رسید بیمه — دو بلوکی",
    "۳۰) قبض و رسید — دو بلوکی قاب‌دار"
  ];

  /* push a labelled data field into a cell; align 0 = right (RTL cell start) */
  function putF(d, x, y, w, h, field, prefix, pt, bold, align) {
    if (bold) d.push(FB(x, y, w, h, field, prefix, pt, align || 0));
    else      d.push(F (x, y, w, h, field, prefix, pt, align || 0));
  }

  /* ============== SHARED MEDICAL-RECEIPT SECTION BUILDERS ================= */
  /* Each appends to the item array and returns the next free y (mm). The    */
  /* whole receipt body sits inside ONE bordered box (drawn last by          */
  /* renderReceipt) so every internal rule meets a continuous thin outline.  */

  /* Section 1 — clinic header (centered name + address + phone).            */
  /* hdrMode: 0 plain, 1 gray band behind the name, 2 double rule below.     */
  /* photo: a small patient-photo box tucked into the top-right corner.      */
  function medHeader(d, x, y, w, pt, hdrMode, photo) {
    var pw = 15.0, ph = 18.0, tw = w;
    if (photo) tw = w - pw - 3.0;
    var yy = y + 0.8, namePt = pt + 3.0;
    if (hdrMode === 1 && !photo) {
      d.push(BAND(x, y + 0.4, w, 7.0, R_SHADE));
      d.push(L(x + 1, y + 0.7, w - 2, 6.2, FA_CLINIC, namePt, true, 1));
      yy = y + 0.4 + 7.0 + 0.6;
    } else {
      d.push(L(x, yy, tw, 6.4, FA_CLINIC, namePt, true, 1)); yy += 7.0;
    }
    d.push(F(x, yy, tw, 4.4, "clinicaddr",  FA_ADDR,  pt - 2.0, 1)); yy += 5.0;
    d.push(F(x, yy, tw, 4.2, "clinicphone", FA_PHONE, pt - 2.0, 1));
    if (photo) d.push(PHOTO(x + w - pw, y + 0.8, pw, ph));
    var bottom = y + (photo ? 19.0 : 18.0);
    d.push(HL(x, bottom, w, 0.3));
    if (hdrMode === 2 && !photo) { d.push(HL(x, bottom + 1.3, w, 0.3)); bottom += 1.3; }
    return bottom;
  }

  /* Section 2 — wide, short barcode (centered) + divider rule. */
  function medBarcode(d, x, y, w) {
    var bcW = w * 0.82, bcH = 11.0;
    d.push(BARCODE(x + (w - bcW) / 2.0, y + 0.5, bcW, bcH));
    var bottom = y + bcH + 1.5;
    d.push(HL(x, bottom, w, 0.3));
    return bottom;
  }

  /* Section 3 — document title (bold, centered) + divider rule. */
  function medTitle(d, x, y, w, pt, title) {
    d.push(L(x, y + 0.4, w, 6.0, title, pt + 2.0, true, 1));
    var bottom = y + 7.5;
    d.push(HL(x, bottom, w, 0.3));
    return bottom;
  }

  /* Section 4 — the ruled info box: 6 rows × 2 columns, split by a center   */
  /* vertical line and separated by horizontal rules. The last row leaves its */
  /* bottom rule to the services table's top border. Right cell = first       */
  /* (right-most) field; rows a & c carry two concatenated fields in one cell.*/
  function medInfo(d, x, y, w, rH, pt, bw) {
    var half = w / 2.0, vx = x + half, fh = rH - 0.6;
    var RR = x + w - 1.0, LR = x + half - 1.0, cy = y;
    function vline() { d.push(VL(vx, cy, rH, bw)); }
    function rule()  { cy += rH; d.push(HL(x, cy, w, bw)); }
    function r1(f, p, b) { putF(d, x + half + 1.0, cy + 0.3, half - 2.0, fh, f, p, pt, b, 0); }
    function l1(f, p, b) { putF(d, x + 1.0, cy + 0.3, half - 2.0, fh, f, p, pt, b, 0); }

    /* a: تاریخ نوبت: {apptdate} {appttime}  |  نوبت: {queue} */
    vline();
    var sw = half * 0.50, pw2 = half * 0.28, g = 1.0;
    putF(d, RR - sw, cy + 0.3, sw, fh, "apptdate", FA_APPT, pt, true, 0);
    putF(d, RR - sw - g - pw2, cy + 0.3, pw2, fh, "appttime", "", pt, false, 0);
    l1("queue", FA_QUEUE, false);
    rule();
    /* b: کد ملی: {nid}  |  بارکد: {barcode} */
    vline(); r1("nid", FA_NID, true); l1("barcode", FA_BARCODE, false); rule();
    /* c: بیمه پایه: {ins}  |  مکمل: {supp} {supp_percent} */
    vline(); r1("ins", FA_BASEINS, false);
    sw = half * 0.52; pw2 = half * 0.30; g = 1.0;
    putF(d, LR - sw, cy + 0.3, sw, fh, "supp", FA_SUPP, pt, false, 0);
    putF(d, LR - sw - g - pw2, cy + 0.3, pw2, fh, "supp_percent", "", pt, false, 0);
    rule();
    /* d: نام بیمار: {full}  |  سن: {age} */
    vline(); r1("full", FA_FULL, true); l1("age", FA_AGE, false); rule();
    /* e: دکتر: {doctor}  |  کد نظام پزشکی: {doctorcode} */
    vline(); r1("doctor", FA_DOCTOR, false); l1("doctorcode", FA_DOCCODE, false); rule();
    /* f: کد تخصص: {specialtycode}  |  شرح تخصص: {specialty}   (no bottom rule) */
    vline(); r1("specialtycode", FA_SPECCODE, false); l1("specialty", FA_SPECIALTY, false);
    cy += rH;
    return cy;
  }
  /* Section 6 — financial section. mode 0: 4 rows (a 2col, b 2col, c 1col,  */
  /* d 2col); mode 1: full (adds سهم بیمار / مبلغ نهایی / سهم بیمه پایه);     */
  /* mode 2: dual-block — two side-by-side sub-boxes with gray caption strips.*/
  /* Ends with a full-width rule dividing financial from e-prescription.     */
  function medFinancial(d, x, y, w, rH, pt, bw, mode) {
    var half = w / 2.0, vx = x + half, fh = rH - 0.6, cy = y;
    if (mode === 2) {
      var gap = 3.0, bw2 = (w - gap) / 2.0, lx = x, rx = x + bw2 + gap, capH = 5.0;
      var iy = y + capH;
      d.push(BAND(lx, y, bw2, capH, R_SHADE));
      d.push(L(lx + 1, y + 0.4, bw2 - 2, capH - 0.6, FA_PAYLBL, pt - 0.5, true, 1));
      d.push(BAND(rx, y, bw2, capH, R_SHADE));
      d.push(L(rx + 1, y + 0.4, bw2 - 2, capH - 0.6, FA_SHARELBL, pt - 0.5, true, 1));
      /* left block: paid / total / discount */
      putF(d, lx + 1, iy + 0.3, bw2 - 2, fh, "paid", FA_PAID, pt, true, 0);
      d.push(HL(lx, iy + rH, bw2, bw));
      putF(d, lx + 1, iy + rH + 0.3, bw2 - 2, fh, "total", FA_TOTAL, pt, false, 0);
      d.push(HL(lx, iy + 2 * rH, bw2, bw));
      putF(d, lx + 1, iy + 2 * rH + 0.3, bw2 - 2, fh, "discount", FA_DISCOUNT, pt, false, 0);
      /* right block: insshare / supppay / pos / cash */
      putF(d, rx + 1, iy + 0.3, bw2 - 2, fh, "insshare", FA_INSSHARE, pt, false, 0);
      d.push(HL(rx, iy + rH, bw2, bw));
      putF(d, rx + 1, iy + rH + 0.3, bw2 - 2, fh, "supppay", FA_SUPPPAY, pt, false, 0);
      d.push(HL(rx, iy + 2 * rH, bw2, bw));
      putF(d, rx + 1, iy + 2 * rH + 0.3, bw2 - 2, fh, "pos", FA_POS, pt, false, 0);
      d.push(HL(rx, iy + 3 * rH, bw2, bw));
      putF(d, rx + 1, iy + 3 * rH + 0.3, bw2 - 2, fh, "cash", FA_CASH, pt, true, 0);
      var bottom = y + capH + 4 * rH;
      d.push(RECT(lx, y, bw2, capH + 4 * rH, bw, 0));
      d.push(RECT(rx, y, bw2, capH + 4 * rH, bw, 0));
      d.push(HL(x, bottom, w, bw));                 /* fin/epres divider */
      return bottom;
    }
    function vline() { d.push(VL(vx, cy, rH, bw)); }
    function rule()  { cy += rH; d.push(HL(x, cy, w, bw)); }
    function r1(f, p, b) { putF(d, x + half + 1.0, cy + 0.3, half - 2.0, fh, f, p, pt, b, 0); }
    function l1(f, p, b) { putF(d, x + 1.0, cy + 0.3, half - 2.0, fh, f, p, pt, b, 0); }
    function row2(f1, p1, f2, p2, b1, b2) { vline(); r1(f1, p1, b1); l1(f2, p2, b2); rule(); }
    function row1(f, p, b) { putF(d, x + 1.0, cy + 0.3, w - 2.0, fh, f, p, pt, b, 0); rule(); }
    row2("paid", FA_PAID, "total", FA_TOTAL, true, false);                 /* a */
    row2("insshare", FA_INSSHARE, "supppay", FA_SUPPPAY, false, false);     /* b */
    row1("discount", FA_DISCOUNT, false);                                   /* c (full width) */
    row2("pos", FA_POS, "cash", FA_CASH, false, false);                     /* d */
    if (mode === 1) {
      row2("patientshare", FA_PATSHARE, "finaltotal", FA_FINAL, false, true); /* e */
      row1("basepay", "سهم بیمه پایه: ", false);                              /* f */
    }
    return cy;
  }

  /* Sections 7 & 8 — e-prescription + referral (full-width ruled rows, the  */
  /* last one leaving its bottom rule to the outer box border).              */
  function medEpresReferral(d, x, y, w, rH, pt, bw) {
    var fh = rH - 0.6, cy = y;
    putF(d, x + 1.0, cy + 0.3, w - 2.0, fh, "eprescription", FA_EPRESC, pt, false, 0);
    cy += rH; d.push(HL(x, cy, w, bw));
    putF(d, x + 1.0, cy + 0.3, w - 2.0, fh, "referralno", FA_REFERRAL, pt, false, 0);
    cy += rH;
    return cy;
  }

  /* Sections 9 / 10 / 11 — below the box: پذیرش | صندوق , ش.ص , print ts.   */
  function medBelow(d, x, y, w, pt) {
    var fh = 5.5, gap = 2.2, half = w / 2.0;
    d.push(HL(x, y, w, 0.25));
    putF(d, x + half + 1.0, y + 1.0, half - 2.0, fh, "receptionist", FA_RECEPTION, pt - 1.0, false, 0);
    putF(d, x + 1.0, y + 1.0, half - 2.0, fh, "cashier", FA_CASHIER, pt - 1.0, false, 0);
    y += fh + gap;
    putF(d, x + 1.0, y, w - 2.0, fh, "scnum", FA_SC, pt - 1.0, false, 0);
    y += fh + gap;
    var sw = w * 0.30, pw2 = w * 0.22, g = 1.0;
    putF(d, x + w - 1.0 - sw, y, sw, fh, "date", FA_PRINTTS, pt - 1.0, false, 0);
    putF(d, x + w - 1.0 - sw - g - pw2, y, pw2, fh, "time", " ساعت: ", pt - 1.0, false, 0);
    y += fh + gap;
    return y;
  }

  /* Family 8 — compact tear-off: a rule + a mini «نسخهٔ بیمار» stub (name /  */
  /* receipt / total / insshare / paid). No second barcode: the main receipt  */
  /* already carries the one deterministic Code128 in section 2.              */
  function medStub(d, x, y, w, pt) {
    d.push(HL(x, y, w, 0.3));
    d.push(L(x, y + 0.6, w, 4, FA_TEAR, pt - 1.0, false, 1));
    y += 6.0;
    d.push(LC(x, y, w, 6, FA_STUB, pt + 1.0, true, 1, R_INK));
    y += 7.0;
    var rH = 5.0, fh = 4.4, half = w / 2.0;
    d.push(RECT(x, y, w, 3 * rH, 0.3, 0));
    d.push(VL(x + half, y, 3 * rH, 0.3));
    putF(d, x + half + 1.0, y + 0.3, half - 2.0, fh, "full", FA_FULL, pt - 1.0, true, 0);
    putF(d, x + 1.0, y + 0.3, half - 2.0, fh, "receiptNo", FA_RECEIPT, pt - 1.0, false, 0);
    d.push(HL(x, y + rH, w, 0.3));
    putF(d, x + half + 1.0, y + rH + 0.3, half - 2.0, fh, "total", FA_TOTAL, pt - 1.0, false, 0);
    putF(d, x + 1.0, y + rH + 0.3, half - 2.0, fh, "insshare", FA_INSSHARE, pt - 1.0, false, 0);
    d.push(HL(x, y + 2 * rH, w, 0.3));
    d.push(FB(x + 1.0, y + 2 * rH + 0.3, w - 2.0, fh, "paid", FA_FINAL, pt, 0));
    y += 3 * rH + 2.0;
    d.push(HL(x + w * 0.55, y, w * 0.45, 0.3));
    d.push(L(x + w * 0.55, y + 0.4, w * 0.45, 4, "امضا و مهر صندوق", pt - 2.0, false, 1));
    return y + 6.0;
  }

  /* Family 3 — sidebar: a narrow left panel (logo + clinic identity +       */
  /* barcode) and a main column carrying the title + the ruled receipt box +  */
  /* below lines.                                                             */
  function renderSidebar(d, sp, pt, rH, bw, svcH, title) {
    var sbW = 42.0, sbX = R_M, mainX = sbX + sbW + 4.0, mainW = R_CW - sbW - 4.0;
    if (sp.fr) d.push(FRAME(PG_W, PG_H, FR_M, R_INK, 0.3));
    /* sidebar panel: sections 1 (clinic header) + 2 (barcode) */
    var sbTop = R_M, sy = sbTop + 1.2;
    d.push(LOGO(sbX + (sbW - 22.0) / 2.0, sy, 22.0, 22.0)); sy += 24.0;
    d.push(L(sbX + 1.0, sy, sbW - 2.0, 8.0, FA_CLINIC, pt + 1.0, true, 1)); sy += 9.0;
    d.push(F(sbX + 1.0, sy, sbW - 2.0, 10.0, "clinicaddr", FA_ADDR, pt - 2.0, 1)); sy += 11.0;
    d.push(F(sbX + 1.0, sy, sbW - 2.0, 5.0, "clinicphone", FA_PHONE, pt - 2.0, 1)); sy += 6.0;
    d.push(HL(sbX + 1.0, sy, sbW - 2.0, 0.3)); sy += 3.0;
    var bcH = 12.0;
    d.push(BARCODE(sbX + 2.0, sy, sbW - 4.0, bcH));
    var sbBottom = sy + bcH + 2.0;
    d.push(RECT(sbX, sbTop, sbW, sbBottom - sbTop, bw, 0));
    /* main column: section 3 title + sections 4..8 box + sections 9..11 */
    var mx = mainX, mw = mainW, boxTop = R_M, y = boxTop + 1.2;
    d.push(L(mx, y, mw, 6.0, title, pt + 2.0, true, 1)); y += 7.5;
    d.push(HL(mx, y, mw, 0.3)); y += 2.0;
    y = medInfo(d, mx, y, mw, rH, pt, bw);
    d.push(SERVICES(mx, y, mw, svcH, pt - 0.5, sp.s, 0, bw, rH - 1.0, rH - 0.8));
    y += svcH;
    y = medFinancial(d, mx, y, mw, rH, pt, bw, 0);
    y = medEpresReferral(d, mx, y, mw, rH, pt, bw);
    d.push(RECT(mx, boxTop, mw, y - boxTop, bw, 0));
    medBelow(d, mx, y + 2.0, mw, pt);
  }

  /* ===================== renderReceipt — one medical receipt ============== */
  function renderReceipt(d, sp) {
    var fam = sp.f;
    var pt = (fam === 2 || fam === 7) ? 9.0 : (fam === 8) ? 8.5 : 9.5;
    var rH = sp.rh, bw = sp.bw;
    var svcH = (fam === 2 || fam === 7) ? 40.0 : (fam === 3) ? 38.0
            : (fam === 8) ? 30.0 : (fam === 5) ? 32.0 : 34.0;
    /* v1.95: variants within each family get distinct service-table heights */
    if (sp.v === 1) svcH += 2.0;
    else if (sp.v === 2) svcH += 4.0;
    var finMode = (fam === 7) ? 1 : (fam === 9) ? 2 : 0;
    var photo = (fam === 6), sidebar = (fam === 3), tearoff = (fam === 8), shadeBars = (fam === 5);
    var svcHdr = sp.hf; if (fam === 4) svcHdr = 0;
    var base = (fam === 1 || fam === 5) ? 1 : (fam === 2) ? 2 : 0;
    var hdrMode = (base + sp.v) % 3;
    if (fam === 4) hdrMode = 0;                  /* line-art: plain header, hairline */
    if (photo && hdrMode === 1) hdrMode = 2;     /* never a gray band over the photo */

    var title = (sp.v === 0) ? FA_DOC_REC : (sp.v === 1) ? FA_INS_REC : FA_DOC_BOTH;
    if (sidebar) { renderSidebar(d, sp, pt, rH, bw, svcH, title); return; }

    var x = R_M, w = R_CW, boxTop = R_M, y = boxTop + 1.2;
    if (sp.fr) d.push(FRAME(PG_W, PG_H, FR_M, R_INK, 0.3));

    /* 1 clinic header */
    y = medHeader(d, x, y, w, pt, hdrMode, photo);
    /* 2 barcode */
    y = medBarcode(d, x, y, w);
    /* 3 document title */
    y = medTitle(d, x, y, w, pt, title);

    /* optional shaded section caption bars (card family) */
    if (shadeBars) {
      d.push(BAND(x, y, w, 5.0, R_SHADE));
      d.push(L(x + 1, y + 0.4, w - 2, 4.4, "مشخصات بیمار", pt - 1.0, true, 1));
      y += 5.0;
    }
    /* 4 ruled info box */
    y = medInfo(d, x, y, w, rH, pt, bw);

    if (shadeBars) {
      d.push(BAND(x, y, w, 5.0, R_SHADE));
      d.push(L(x + 1, y + 0.4, w - 2, 4.4, FA_SVCLIST, pt - 1.0, true, 1));
      y += 5.0;
    }
    /* 5 services table (dynamic, PIT_SERVICES) */
    d.push(SERVICES(x, y, w, svcH, pt - 0.5, sp.s, svcHdr, bw, rH - 1.0, rH - 0.8));
    y += svcH;

    if (shadeBars) {
      d.push(BAND(x, y, w, 5.0, R_SHADE));
      d.push(L(x + 1, y + 0.4, w - 2, 4.4, "مالی و پرداخت", pt - 1.0, true, 1));
      y += 5.0;
    }
    /* 6 financial section */
    y = medFinancial(d, x, y, w, rH, pt, bw, finMode);
    /* 7 e-prescription  +  8 referral */
    y = medEpresReferral(d, x, y, w, rH, pt, bw);

    /* the ONE bordered box around the whole receipt body (drawn last; content */
    /* is inset from every edge so the outline never crosses the text) */
    d.push(RECT(x, boxTop, w, y - boxTop, bw, 0));

    /* 9 / 10 / 11 below the box */
    y = medBelow(d, x, y + 2.0, w, pt);

    /* compact tear-off stub */
    if (tearoff) medStub(d, x, y + 2.0, w, pt);
  }

  function buildTemplate(idx) {
    if (idx < 0 || idx >= 30) idx = 0;
    var sp = TPL[idx], d = [];
    renderReceipt(d, sp);
    return d;
  }



  var ALL = [];
  for (var i = 0; i < 30; i++) {
    _uid = 0;
    var items = buildTemplate(i);
    for (var j = 0; j < items.length; j++) { items[j].id = j + 1; items[j].z = j + 1; }
    ALL.push({ id: 0, name: NAMES[i], kind: "builtin", group: "reception",
      paper: "A4", orientation: 0, items: items });
  }

  window.AZ_TEMPLATES = ALL;
})();
