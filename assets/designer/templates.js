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
        widths: [0.32, 0.10, 0.20, 0.30, 0.08],
        labels: ["نام خدمت", "تعداد", "مبلغ کل", "شرح خدمت", "ردیف"] };
      case SVC4_CAT: return { cols: 5, header: true,
        widths: [0.26, 0.12, 0.10, 0.22, 0.30],
        labels: ["نام خدمت", "نوع خدمت", "تعداد", "مبلغ کل", "شرح خدمت"] };
      case SVC5: return { cols: 4, header: true,
        widths: [0.32, 0.12, 0.24, 0.32],
        labels: ["نام خدمت", "تعداد", "مبلغ کل", "شرح خدمت"] };
      case SVC5_CODE: return { cols: 5, header: true,
        widths: [0.26, 0.12, 0.10, 0.22, 0.30],
        labels: ["نام خدمت", "کد خدمت", "تعداد", "مبلغ کل", "شرح خدمت"] };
      case SVC6_FIN: return { cols: 6, header: true,
        widths: [0.24, 0.08, 0.14, 0.10, 0.18, 0.26],
        labels: ["نام خدمت", "تعداد", "مبلغ واحد", "تخفیف", "مبلغ کل", "شرح خدمت"] };
      case SVC6_INS: return { cols: 6, header: true,
        widths: [0.24, 0.08, 0.16, 0.14, 0.14, 0.24],
        labels: ["نام خدمت", "تعداد", "مبلغ کل", "سهم بیمه", "سهم بیمار", "شرح خدمت"] };
      case SVC7: return { cols: 7, header: true,
        widths: [0.20, 0.10, 0.08, 0.12, 0.16, 0.26, 0.08],
        labels: ["نام خدمت", "کد خدمت", "تعداد", "مبلغ واحد", "مبلغ کل",
                 "شرح خدمت", "ردیف"] };
      default: return { cols: 4, header: true,
        widths: [0.34, 0.10, 0.22, 0.34],
        labels: ["نام خدمت", "تعداد", "مبلغ کل", "شرح خدمت"] };
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
  /* v1.96.0 — pure monochrome line-art: hf=0 (no coloured backgrounds). The ten
     families stay distinct through LAYOUT, not colour. */
  var TPL = [
    { f: 0, v: 0, s: SVC3,      a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 6.2, fr: false },
    { f: 0, v: 1, s: SVC4_ROW,  a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 6.2, fr: false },
    { f: 0, v: 2, s: SVC5,      a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 6.2, fr: false },
    { f: 1, v: 0, s: SVC3,      a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 6.0, fr: false },
    { f: 1, v: 1, s: SVC4_CAT,  a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 6.0, fr: false },
    { f: 1, v: 2, s: SVC5_CODE, a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 6.0, fr: false },
    { f: 2, v: 0, s: SVC6_FIN,  a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 5.6, fr: true  },
    { f: 2, v: 1, s: SVC6_INS,  a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 5.6, fr: true  },
    { f: 2, v: 2, s: SVC7,      a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 5.6, fr: true  },
    { f: 3, v: 0, s: SVC3,      a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 5.8, fr: false },
    { f: 3, v: 1, s: SVC4_ROW,  a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 5.8, fr: false },
    { f: 3, v: 2, s: SVC5,      a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 5.8, fr: false },
    { f: 4, v: 0, s: SVC3,      a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.25, rh: 6.0, fr: false },
    { f: 4, v: 1, s: SVC4_ROW,  a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.25, rh: 6.0, fr: true  },
    { f: 4, v: 2, s: SVC5,      a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.25, rh: 6.0, fr: false },
    { f: 5, v: 0, s: SVC3,      a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 6.0, fr: false },
    { f: 5, v: 1, s: SVC4_CAT,  a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 6.0, fr: false },
    { f: 5, v: 2, s: SVC6_INS,  a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 6.0, fr: false },
    { f: 6, v: 0, s: SVC3,      a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 6.0, fr: false },
    { f: 6, v: 1, s: SVC4_CAT,  a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 6.0, fr: false },
    { f: 6, v: 2, s: SVC5,      a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 6.0, fr: false },
    { f: 7, v: 0, s: SVC6_FIN,  a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 5.6, fr: false },
    { f: 7, v: 1, s: SVC6_INS,  a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 5.6, fr: false },
    { f: 7, v: 2, s: SVC7,      a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 5.6, fr: true  },
    { f: 8, v: 0, s: SVC3,      a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 5.2, fr: false },
    { f: 8, v: 1, s: SVC4_ROW,  a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 5.2, fr: false },
    { f: 8, v: 2, s: SVC5,      a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 5.2, fr: false },
    { f: 9, v: 0, s: SVC3,      a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 6.0, fr: false },
    { f: 9, v: 1, s: SVC6_FIN,  a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 6.0, fr: false },
    { f: 9, v: 2, s: SVC5_CODE, a: "#000000", t: "#FFFFFF", hf: 0, bw: 0.30, rh: 6.0, fr: true  }
  ];

  var NAMES = [
    "۰۱) قبض پزشک — کلاسیک",
    "۰۲) رسید بیمه — کلاسیک خط‌کشی",
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
    "۱۶) قبض پزشک — کارتی خط‌کشی",
    "۱۷) رسید بیمه — کارتی خط‌کشی",
    "۱۸) قبض و رسید — کارتی خط‌کشی",
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

  /* v1.96.0 — SPLIT LABEL / VALUE (CRITICAL RULE): caption = its own PIT_LABEL
     (right-aligned at the cell's right edge), bound value = its own PIT_FIELD
     with an EMPTY prefix (right-aligned just left of the label), so the row reads
     «label:  value» with NO hardcoded example text in the field. lwFrac tunes the
     label band so long Persian captions never clip. field==null → label only. */
  function putLF(d, x, y, w, h, label, field, pt, lblBold, valBold, lwFrac) {
    var lw = w * (lwFrac == null ? 0.42 : lwFrac), gap = 1.2, vw = w - lw - gap;
    if (vw < 6) vw = 6;
    d.push(L(x + w - lw, y, lw, h, label, pt, lblBold, 0));
    if (field) {
      if (valBold) d.push(FB(x, y, vw, h, field, "", pt, 0));
      else         d.push(F (x, y, vw, h, field, "", pt, 0));
    }
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
      /* v1.96.0: line-art emphasis (rule + larger bold name) — was a gray band */
      d.push(HL(x, y + 0.4, w, 0.3));
      d.push(L(x + 1, y + 0.8, w - 2, 6.4, FA_CLINIC, namePt + 1.0, true, 1));
      yy = y + 0.4 + 7.4 + 0.6;
    } else {
      d.push(L(x, yy, tw, 6.4, FA_CLINIC, namePt, true, 1)); yy += 7.0;
    }
    d.push(F(x, yy, tw, 4.4, "clinicaddr",  "", pt - 2.0, 1)); yy += 5.0;
    d.push(F(x, yy, tw, 4.2, "clinicphone", "", pt - 2.0, 1));
    if (photo) d.push(PHOTO(x + w - pw, y + 0.8, pw, ph));
    var bottom = y + (photo ? 19.0 : 18.0);
    d.push(HL(x, bottom, w, 0.3));
    if (hdrMode === 2 && !photo) { d.push(HL(x, bottom + 1.3, w, 0.3)); bottom += 1.3; }
    return bottom;
  }

  /* Section 2 — wide, short barcode (centered, STANDARD ~72×7 mm) + divider. */
  function medBarcode(d, x, y, w) {
    var bcW = 72.0, bcH = 7.0;
    d.push(BARCODE(x + (w - bcW) / 2.0, y + 0.4, bcW, bcH));
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

  /* Section 4 — the ruled info box: 6 rows × 2 columns, split by a center
     vertical line and separated by horizontal rules. v1.96.0: every row now
     carries SPLIT label (PIT_LABEL) + bound value (PIT_FIELD, empty prefix). */
  function medInfo(d, x, y, w, rH, pt, bw) {
    var half = w / 2.0, vx = x + half, fh = rH - 0.6;
    var rx = x + half + 1.0, rw = half - 2.0, lx = x + 1.0, lw = half - 2.0, cy = y;
    function vline() { d.push(VL(vx, cy, rH, bw)); }
    function rule()  { cy += rH; d.push(HL(x, cy, w, bw)); }

    /* a: تاریخ نوبت: {apptdate} {appttime}  |  نوبت: {queue} */
    /* RTL cell order right→left: [label][apptdate][appttime] */
    vline();
    var lblW = rw * 0.34, g = 1.0, rem = rw - lblW - 2 * g, f1 = rem * 0.58, f2 = rem * 0.42;
    d.push(F (rx,         cy + 0.3, f2,    fh, "appttime", "", pt, 0));
    d.push(FB(rx + f2 + g, cy + 0.3, f1,    fh, "apptdate", "", pt, 0));
    d.push(L (rx + rw - lblW, cy + 0.3, lblW, fh, FA_APPT, pt, true, 0));
    putLF(d, lx, cy + 0.3, lw, fh, FA_QUEUE, "queue", pt, false, false, 0.26);
    rule();
    /* b: کد ملی: {nid}  |  بارکد: {barcode} */
    vline(); putLF(d, rx, cy + 0.3, rw, fh, FA_NID, "nid", pt, true, false, 0.42);
             putLF(d, lx, cy + 0.3, lw, fh, FA_BARCODE, "barcode", pt, false, false, 0.30); rule();
    /* c: بیمه پایه: {ins}  |  مکمل: {supp} {supp_percent} */
    /* RTL cell order right→left: [label][supp][supp_percent] */
    vline(); putLF(d, rx, cy + 0.3, rw, fh, FA_BASEINS, "ins", pt, false, false, 0.40);
    lblW = lw * 0.22; g = 1.0; rem = lw - lblW - 2 * g; f1 = rem * 0.58; f2 = rem * 0.42;
    d.push(F (lx,         cy + 0.3, f2,    fh, "supp_percent", "", pt, 0));
    d.push(F (lx + f2 + g, cy + 0.3, f1,    fh, "supp",         "", pt, 0));
    d.push(L (lx + lw - lblW, cy + 0.3, lblW, fh, FA_SUPP, pt, false, 0));
    rule();
    /* d: نام بیمار: {P-Name}  |  سن: {age}  (10Y/24Y) */
    vline(); putLF(d, rx, cy + 0.3, rw, fh, FA_FULL, "P-Name", pt, true, false, 0.40);
             putLF(d, lx, cy + 0.3, lw, fh, FA_AGE, "age", pt, false, false, 0.20); rule();
    /* e: دکتر: {doctor}  |  کد نظام پزشکی: {doctorcode} */
    vline(); putLF(d, rx, cy + 0.3, rw, fh, FA_DOCTOR, "doctor", pt, false, false, 0.22);
             putLF(d, lx, cy + 0.3, lw, fh, FA_DOCCODE, "doctorcode", pt, false, false, 0.46); rule();
    /* f: کد تخصص: {specialtycode}  |  شرح تخصص: {specialty}   (no bottom rule) */
    vline(); putLF(d, rx, cy + 0.3, rw, fh, FA_SPECCODE, "specialtycode", pt, false, false, 0.30);
             putLF(d, lx, cy + 0.3, lw, fh, FA_SPECIALTY, "specialty", pt, false, false, 0.30);
    cy += rH;
    return cy;
  }

  /* Family 4 — two-column info: patient LEFT / visit RIGHT. Same field set. */
  function medInfoSplit(d, x, y, w, rH, pt, bw) {
    var gap = 3.0, colW = (w - gap) / 2.0;
    var lx = x, rx = x + colW + gap, fh = rH - 0.6, n = 6, i;
    for (i = 0; i < n - 1; i++) {
      d.push(HL(lx, y + (i + 1) * rH, colW, bw));
      d.push(HL(rx, y + (i + 1) * rH, colW, bw));
    }
    d.push(RECT(lx, y, colW, n * rH, bw, 0));
    d.push(RECT(rx, y, colW, n * rH, bw, 0));
    function cell(cx, row, lab, fld, lb, vb, frac) {
      putLF(d, cx + 1.0, y + row * rH + 0.3, colW - 2.0, fh, lab, fld, pt, lb, vb, frac);
    }
    cell(lx, 0, FA_FULL, "P-Name", true, false, 0.40);
    cell(lx, 1, FA_AGE, "age", false, false, 0.22);
    cell(lx, 2, FA_NID, "nid", true, false, 0.42);
    cell(lx, 3, FA_BARCODE, "barcode", false, false, 0.30);
    cell(lx, 4, FA_BASEINS, "ins", false, false, 0.40);
    var cw = colW - 2.0, xx = lx + 1.0, yy = y + 5 * rH + 0.3;
    var lblW = cw * 0.22, g = 1.0, rem = cw - lblW - 2 * g, f1 = rem * 0.58, f2 = rem * 0.42;
    d.push(F(xx, yy, f2, fh, "supp_percent", "", pt, 0));
    d.push(F(xx + f2 + g, yy, f1, fh, "supp", "", pt, 0));
    d.push(L(xx + cw - lblW, yy, lblW, fh, FA_SUPP, pt, false, 0));
    cw = colW - 2.0; xx = rx + 1.0; yy = y + 0.3;
    lblW = cw * 0.34; g = 1.0; rem = cw - lblW - 2 * g; f1 = rem * 0.58; f2 = rem * 0.42;
    d.push(F(xx, yy, f2, fh, "appttime", "", pt, 0));
    d.push(FB(xx + f2 + g, yy, f1, fh, "apptdate", "", pt, 0));
    d.push(L(xx + cw - lblW, yy, lblW, fh, FA_APPT, pt, true, 0));
    cell(rx, 1, FA_QUEUE, "queue", false, false, 0.26);
    cell(rx, 2, FA_DOCTOR, "doctor", false, false, 0.22);
    cell(rx, 3, FA_DOCCODE, "doctorcode", false, false, 0.50);
    cell(rx, 4, FA_SPECCODE, "specialtycode", false, false, 0.30);
    cell(rx, 5, FA_SPECIALTY, "specialty", false, false, 0.30);
    return y + n * rH;
  }

  /* Family 8 — shorter info (4×3) so the tear-off stub still fits. */
  function medInfoCompact(d, x, y, w, rH, pt, bw) {
    var cols = 3, rows = 4, cw = w / cols, fh = rH - 0.6, r, c;
    for (r = 0; r < rows; r++) {
      if (r > 0) d.push(HL(x, y + r * rH, w, bw));
      for (c = 1; c < cols; c++) d.push(VL(x + c * cw, y + r * rH, rH, bw));
    }
    function cell(rr, cc, lab, fld, lb, vb, frac) {
      putLF(d, x + cc * cw + 0.6, y + rr * rH + 0.3, cw - 1.2, fh, lab, fld, pt, lb, vb, frac);
    }
    cell(0, 0, FA_FULL, "P-Name", true, false, 0.42);
    cell(0, 1, FA_AGE, "age", false, false, 0.28);
    cell(0, 2, FA_NID, "nid", true, false, 0.42);
    cell(1, 0, FA_QUEUE, "queue", false, false, 0.30);
    var xx = x + 1 * cw + 0.6, yy = y + 1 * rH + 0.3, ww = cw - 1.2;
    var lblW = ww * 0.38, g = 0.6, rem = ww - lblW - 2 * g, f1 = rem * 0.58, f2 = rem * 0.42;
    d.push(F(xx, yy, f2, fh, "appttime", "", pt, 0));
    d.push(FB(xx + f2 + g, yy, f1, fh, "apptdate", "", pt, 0));
    d.push(L(xx + ww - lblW, yy, lblW, fh, FA_APPT, pt, true, 0));
    cell(1, 2, FA_BARCODE, "barcode", false, false, 0.34);
    cell(2, 0, FA_BASEINS, "ins", false, false, 0.42);
    xx = x + 1 * cw + 0.6; yy = y + 2 * rH + 0.3; ww = cw - 1.2;
    lblW = ww * 0.26; g = 0.6; rem = ww - lblW - 2 * g; f1 = rem * 0.58; f2 = rem * 0.42;
    d.push(F(xx, yy, f2, fh, "supp_percent", "", pt, 0));
    d.push(F(xx + f2 + g, yy, f1, fh, "supp", "", pt, 0));
    d.push(L(xx + ww - lblW, yy, lblW, fh, FA_SUPP, pt, false, 0));
    cell(2, 2, FA_DOCTOR, "doctor", false, false, 0.28);
    cell(3, 0, FA_DOCCODE, "doctorcode", false, false, 0.56);
    cell(3, 1, FA_SPECCODE, "specialtycode", false, false, 0.42);
    cell(3, 2, FA_SPECIALTY, "specialty", false, false, 0.42);
    return y + rows * rH;
  }

  /* Section 6 — financial section. v1.96.0: split label/value, line-art only.
     mode 0: 4 rows; mode 1: full; mode 2: dual-block (line-art caption labels). */
  function medFinancial(d, x, y, w, rH, pt, bw, mode) {
    var half = w / 2.0, vx = x + half, fh = rH - 0.6, cy = y;
    var rx = x + half + 1.0, rw = half - 2.0, lx = x + 1.0, lw = half - 2.0;
    if (mode === 2) {
      var gap = 3.0, bw2 = (w - gap) / 2.0, lx2 = x, rx2 = x + bw2 + gap, capH = 5.5;
      var iy = y + capH;
      /* line-art caption labels (bold, centered) replacing the old gray bands */
      d.push(L(lx2 + 1, y + 0.5, bw2 - 2, capH - 0.7, FA_PAYLBL, pt - 0.5, true, 1));
      d.push(L(rx2 + 1, y + 0.5, bw2 - 2, capH - 0.7, FA_SHARELBL, pt - 0.5, true, 1));
      d.push(HL(lx2, y + capH, bw2, bw));
      d.push(HL(rx2, y + capH, bw2, bw));
      /* left block: paid / total / discount */
      putLF(d, lx2 + 1, iy + 0.3, bw2 - 2, fh, FA_PAID, "paid", pt, true, false, 0.40);
      d.push(HL(lx2, iy + rH, bw2, bw));
      putLF(d, lx2 + 1, iy + rH + 0.3, bw2 - 2, fh, FA_TOTAL, "total", pt, false, false, 0.40);
      d.push(HL(lx2, iy + 2 * rH, bw2, bw));
      putLF(d, lx2 + 1, iy + 2 * rH + 0.3, bw2 - 2, fh, FA_DISCOUNT, "discount", pt, false, false, 0.40);
      /* right block: insshare / supppay / pos / cash */
      putLF(d, rx2 + 1, iy + 0.3, bw2 - 2, fh, FA_INSSHARE, "insshare", pt, false, false, 0.40);
      d.push(HL(rx2, iy + rH, bw2, bw));
      putLF(d, rx2 + 1, iy + rH + 0.3, bw2 - 2, fh, FA_SUPPPAY, "supppay", pt, false, false, 0.40);
      d.push(HL(rx2, iy + 2 * rH, bw2, bw));
      putLF(d, rx2 + 1, iy + 2 * rH + 0.3, bw2 - 2, fh, FA_POS, "pos", pt, false, false, 0.30);
      d.push(HL(rx2, iy + 3 * rH, bw2, bw));
      putLF(d, rx2 + 1, iy + 3 * rH + 0.3, bw2 - 2, fh, FA_CASH, "cash", pt, true, false, 0.30);
      var bottom = y + capH + 4 * rH;
      d.push(RECT(lx2, y, bw2, capH + 4 * rH, bw, 0));
      d.push(RECT(rx2, y, bw2, capH + 4 * rH, bw, 0));
      d.push(HL(x, bottom, w, bw));                 /* fin/epres divider */
      return bottom;
    }
    function vline() { d.push(VL(vx, cy, rH, bw)); }
    function rule()  { cy += rH; d.push(HL(x, cy, w, bw)); }
    function row2(l1, f1, b1, l2, f2, b2) {
      vline(); putLF(d, rx, cy + 0.3, rw, fh, l1, f1, pt, b1, b2, 0.40);
               putLF(d, lx, cy + 0.3, lw, fh, l2, f2, pt, b2, false, 0.40); rule();
    }
    function row1(l, f, b) { putLF(d, x + 1.0, cy + 0.3, w - 2.0, fh, l, f, pt, b, false, 0.30); rule(); }
    row2(FA_PAID, "paid", true, FA_TOTAL, "total", false);                 /* a */
    row2(FA_INSSHARE, "insshare", false, FA_SUPPPAY, "supppay", false);     /* b */
    row1(FA_DISCOUNT, "discount", false);                                  /* c (full width) */
    row2(FA_POS, "pos", false, FA_CASH, "cash", false);                     /* d */
    if (mode === 1) {
      row2(FA_PATSHARE, "patientshare", false, FA_FINAL, "finaltotal", true); /* e */
      row1("سهم بیمه پایه: ", "basepay", false);                              /* f */
    }
    return cy;
  }

  /* Sections 7 & 8 — e-prescription + referral (full-width split label/value). */
  function medEpresReferral(d, x, y, w, rH, pt, bw) {
    var fh = rH - 0.6, cy = y;
    putLF(d, x + 1.0, cy + 0.3, w - 2.0, fh, FA_EPRESC, "eprescription", pt, false, false, 0.46);
    cy += rH; d.push(HL(x, cy, w, bw));
    putLF(d, x + 1.0, cy + 0.3, w - 2.0, fh, FA_REFERRAL, "referralno", pt, false, false, 0.32);
    cy += rH;
    return cy;
  }

  /* Sections 9 / 10 / 11 — below the box: پذیرش | صندوق (two columns split by a
     vertical line), ش.ص , print timestamp. v1.96.0: split label/value items. */
  function medBelow(d, x, y, w, pt) {
    var fh = 5.5, gap = 2.2, half = w / 2.0, vx = x + half;
    d.push(HL(x, y, w, 0.25));
    d.push(VL(vx, y + 0.5, fh, 0.25));
    putLF(d, x + half + 1.0, y + 1.0, half - 2.0, fh, FA_RECEPTION, "receptionist", pt - 1.0, false, false, 0.30);
    putLF(d, x + 1.0, y + 1.0, half - 2.0, fh, FA_CASHIER, "cashier", pt - 1.0, false, false, 0.30);
    y += fh + gap;
    putLF(d, x + 1.0, y, w - 2.0, fh, FA_SC, "scnum", pt - 1.0, false, false, 0.14);
    y += fh + gap;
    putLF(d, x + 1.0, y, w * 0.50, fh, FA_PRINTTS, "date", pt - 1.0, false, false, 0.46);
    putLF(d, x + 1.0 + w * 0.52, y, w * 0.46, fh, " ساعت: ", "time", pt - 1.0, false, false, 0.30);
    y += fh + gap;
    return y;
  }

  /* Family 8 — compact tear-off: a rule + a mini «نسخهٔ بیمار» stub (name /
     receipt / total / insshare / paid), split label/value. No second barcode. */
  function medStub(d, x, y, w, pt) {
    d.push(HL(x, y, w, 0.3));
    d.push(L(x, y + 0.6, w, 4, FA_TEAR, pt - 1.0, false, 1));
    y += 6.0;
    d.push(LC(x, y, w, 6, FA_STUB, pt + 1.0, true, 1, R_INK));
    y += 7.0;
    var rH = 5.0, fh = 4.4, half = w / 2.0;
    d.push(RECT(x, y, w, 3 * rH, 0.3, 0));
    d.push(VL(x + half, y, 3 * rH, 0.3));
    putLF(d, x + half + 1.0, y + 0.3, half - 2.0, fh, FA_FULL, "P-Name", pt - 1.0, true, false, 0.36);
    putLF(d, x + 1.0, y + 0.3, half - 2.0, fh, FA_RECEIPT, "receiptNo", pt - 1.0, false, false, 0.40);
    d.push(HL(x, y + rH, w, 0.3));
    putLF(d, x + half + 1.0, y + rH + 0.3, half - 2.0, fh, FA_TOTAL, "total", pt - 1.0, false, false, 0.36);
    putLF(d, x + 1.0, y + rH + 0.3, half - 2.0, fh, FA_INSSHARE, "insshare", pt - 1.0, false, false, 0.36);
    d.push(HL(x, y + 2 * rH, w, 0.3));
    putLF(d, x + 1.0, y + 2 * rH + 0.3, w - 2.0, fh, FA_FINAL, "paid", pt, true, true, 0.34);
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
    d.push(F(sbX + 1.0, sy, sbW - 2.0, 10.0, "clinicaddr", "", pt - 2.0, 1)); sy += 11.0;
    d.push(F(sbX + 1.0, sy, sbW - 2.0, 5.0, "clinicphone", "", pt - 2.0, 1)); sy += 6.0;
    d.push(HL(sbX + 1.0, sy, sbW - 2.0, 0.3)); sy += 3.0;
    var sbBottom = sy + 2.0;
    d.push(RECT(sbX, sbTop, sbW, sbBottom - sbTop, bw, 0));
    /* main column: title + info + services + barcode BELOW the table + totals */
    var mx = mainX, mw = mainW, boxTop = R_M, y = boxTop + 1.2;
    d.push(L(mx, y, mw, 6.0, title, pt + 2.0, true, 1)); y += 7.5;
    d.push(HL(mx, y, mw, 0.3)); y += 2.0;
    y = medInfo(d, mx, y, mw, rH, pt, bw);
    d.push(SERVICES(mx, y, mw, svcH, pt - 0.5, sp.s, 0, bw, rH - 1.0, rH - 0.8));
    y += svcH;
    y = medBarcode(d, mx, y, mw);
    y = medFinancial(d, mx, y, mw, rH, pt, bw, 0);
    y = medEpresReferral(d, mx, y, mw, rH, pt, bw);
    d.push(RECT(mx, boxTop, mw, y - boxTop, bw, 0));
    medBelow(d, mx, y + 2.0, mw, pt);
  }

  /* ============== renderReceipt — one medical receipt (v1.99 orders) === */
  /* Basic insurance is {ins} name only — never {ins_percent}. Supplementary
     is {supp} + {supp_percent}. One Code128 sits below the services table. */
  function renderReceipt(d, sp) {
    var fam = sp.f;
    var pt = (fam === 2 || fam === 7) ? 9.0 : (fam === 8) ? 8.5 : 9.5;
    var rH = sp.rh, bw = sp.bw;
    var svcH = (fam === 2 || fam === 7) ? 40.0 : (fam === 3) ? 38.0
            : (fam === 8) ? 30.0 : (fam === 5) ? 32.0 : 34.0;
    if (sp.v === 1) svcH += 2.0;
    else if (sp.v === 2) svcH += 4.0;
    var finMode = (fam === 7) ? 1 : (fam === 9) ? 2 : 0;
    var photo = (fam === 6), sidebar = (fam === 3), tearoff = (fam === 8), capBars = (fam === 5);
    var svcHdr = sp.hf; if (fam === 4) svcHdr = 0;
    var base = (fam === 1 || fam === 5) ? 1 : (fam === 2) ? 2 : 0;
    var hdrMode = (base + sp.v) % 3;
    if (fam === 4) hdrMode = 0;
    if (photo && hdrMode === 1) hdrMode = 2;

    var title = (sp.v === 0) ? FA_DOC_REC : (sp.v === 1) ? FA_INS_REC : FA_DOC_BOTH;
    if (sidebar) { renderSidebar(d, sp, pt, rH, bw, svcH, title); return; }

    var x = R_M, w = R_CW, boxTop = R_M, y = boxTop + 1.2;
    if (sp.fr) d.push(FRAME(PG_W, PG_H, FR_M, R_INK, 0.3));

    function caption(txt) {
      d.push(HL(x, y, w, 0.3));
      d.push(L(x + 1, y + 0.3, w - 2, 4.4, txt, pt - 1.0, true, 1));
      d.push(HL(x, y + 4.9, w, 0.3));
      y += 5.4;
    }
    function emitSvc() {
      d.push(SERVICES(x, y, w, svcH, pt - 0.5, sp.s, svcHdr, bw, rH - 1.0, rH - 0.8));
      y += svcH;
    }
    function emitBc() {
      y = medBarcode(d, x, y, w);
    }

    y = medHeader(d, x, y, w, pt, hdrMode, photo);
    y = medTitle(d, x, y, w, pt, title);

    if (fam === 1) {
      /* services-high */
      emitSvc(); emitBc();
      y = medInfo(d, x, y, w, rH, pt, bw);
      y = medFinancial(d, x, y, w, rH, pt, bw, finMode);
      y = medEpresReferral(d, x, y, w, rH, pt, bw);
    } else if (fam === 2) {
      /* services-low */
      y = medInfo(d, x, y, w, rH, pt, bw);
      y = medFinancial(d, x, y, w, rH, pt, bw, finMode);
      y = medEpresReferral(d, x, y, w, rH, pt, bw);
      emitSvc(); emitBc();
    } else if (fam === 4) {
      /* two-column info then services then barcode */
      y = medInfoSplit(d, x, y, w, rH, pt, bw);
      emitSvc(); emitBc();
      y = medFinancial(d, x, y, w, rH, pt, bw, finMode);
      y = medEpresReferral(d, x, y, w, rH, pt, bw);
    } else if (fam === 5) {
      /* ruled captions; services after info */
      if (capBars) caption("مشخصات بیمار");
      y = medInfo(d, x, y, w, rH, pt, bw);
      if (capBars) caption(FA_SVCLIST);
      emitSvc(); emitBc();
      if (capBars) caption("مالی و پرداخت");
      y = medFinancial(d, x, y, w, rH, pt, bw, finMode);
      y = medEpresReferral(d, x, y, w, rH, pt, bw);
    } else if (fam === 7) {
      /* finance-first */
      y = medFinancial(d, x, y, w, rH, pt, bw, finMode);
      y = medInfo(d, x, y, w, rH, pt, bw);
      emitSvc(); emitBc();
      y = medEpresReferral(d, x, y, w, rH, pt, bw);
    } else {
      /* 0 classic / 6 photo / 8 compact tear-off / 9 dual-block */
      if (tearoff) y = medInfoCompact(d, x, y, w, rH, pt, bw);
      else y = medInfo(d, x, y, w, rH, pt, bw);
      emitSvc(); emitBc();
      y = medFinancial(d, x, y, w, rH, pt, bw, finMode);
      y = medEpresReferral(d, x, y, w, rH, pt, bw);
    }

    d.push(RECT(x, boxTop, w, y - boxTop, bw, 0));
    y = medBelow(d, x, y + 2.0, w, pt);
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
