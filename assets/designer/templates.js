/* ===========================================================================
   templates.js — DarmanPlus ready-made print designs  (v2.00)

   ★ این فایل دقیقاً همان ۳۱ قالب داخلی C++ (src/print_designer_templates.inc)
     است تا گالری دیزاینر وب با آنچه موتور چاپ واقعاً seed می‌کند یکی باشد.
     هر تغییری در .inc باید عیناً اینجا هم اعمال شود.

   ایندکس ۰: «پیش‌فرض» — رسید حرارتی R80 درمانگاه شبانه روزی ثامن الائمه
   ایندکس ۱..۳۰: سی طرح اضافی، همه متفاوت، مخلوط R80/R58/A5/A4
   =========================================================================== */
(function () {
  "use strict";

  var INK      = "#000000";
  var RULE     = "#000000";
  var PAPER_BG = "#ffffff";

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

  function L(x, y, w, h, text, pt, bold, align, o) {
    return mk(extend({ type: "label", x: x, y: y, w: w, h: h, text: text,
      pt: pt, bold: !!bold, align: align, dir: (align === 1 ? 2 : 0) }, o || {}));
  }
  function F(x, y, w, h, field, prefix, pt, align, o) {
    return mk(extend({ type: "field", x: x, y: y, w: w, h: h,
      field: field, prefix: prefix || "", pt: pt, align: align,
      dir: (align === 1 ? 2 : 0) }, o || {}));
  }
  function FB(x, y, w, h, field, prefix, pt, align, o) {
    return F(x, y, w, h, field, prefix, pt, align, extend({ bold: true }, o || {}));
  }
  function HL(x, y, w, bw) {
    return mk({ type: "hline", x: x, y: y, w: w, h: 0.2, borderWidth: bw,
      borderColor: RULE });
  }
  function VL(x, y, h, bw) {
    return mk({ type: "vline", x: x, y: y, w: 0.2, h: h, borderWidth: bw });
  }
  function RECT(x, y, w, h, bw) {
    return mk({ type: "rect", x: x, y: y, w: w, h: h, borderWidth: bw,
      corner: 0, fillTransparent: true, borderColor: RULE });
  }
  function FRAME(pw, ph, m, bw) {
    return mk({ type: "frame", x: m, y: m, w: pw - 2 * m, h: ph - 2 * m,
      isFrame: true, borderColor: RULE, borderWidth: bw, fillTransparent: true,
      corner: 0 });
  }
  function FRAME_BOX(x, y, w, h, bw) {
    return mk({ type: "frame", x: x, y: y, w: w, h: h,
      isFrame: true, borderColor: RULE, borderWidth: bw, fillTransparent: true,
      corner: 0 });
  }
  function LOGO(x, y, w, h) {
    return mk({ type: "logo", x: x, y: y, w: w, h: h,
      borderColor: RULE, borderWidth: 0.3 });
  }
  function PHOTO(x, y, w, h) {
    return mk({ type: "photo", x: x, y: y, w: w, h: h,
      borderColor: RULE, borderWidth: 0.3 });
  }
  function BARCODE(x, y, w, h) {
    return mk({ type: "barcode", x: x, y: y, w: w, h: h,
      field: "receiptbarcode",
      text: JSON.stringify({ sym: "code128", hri: true, quiet: 2 }),
      pt: 8, align: 1, dir: 1, textColor: INK, borderWidth: 0 });
  }

  var SVC3 = 0, SVC4_ROW = 1, SVC4_CAT = 2, SVC5 = 3, SVC5_CODE = 4,
      SVC6_FIN = 5, SVC6_INS = 6, SVC7 = 7, SVC_SAMEN = 8;

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
      case SVC_SAMEN: return { cols: 3, header: true,
        widths: [0.58, 0.10, 0.32],
        labels: ["نام خدمت", "#", "شرح خدمت"] };
      default: return { cols: 4, header: true,
        widths: [0.34, 0.10, 0.22, 0.34],
        labels: ["نام خدمت", "تعداد", "مبلغ کل", "شرح خدمت"] };
    }
  }

  function SERVICES(x, y, w, h, pt, preset, bw, rowH, headerH) {
    return mk({
      type: "services", x: x, y: y, w: w, h: h, pt: pt, align: 1, dir: 2,
      borderColor: RULE, borderWidth: bw, textColor: INK,
      fillColor: PAPER_BG, fillTransparent: true,
      padding: 0.6, headerH: headerH, rowH: rowH,
      text: JSON.stringify(svcModel(preset))
    });
  }

  var FA_CLINIC    = "درمانگاه شبانه روزی ثامن الائمه";
  var FA_ADDR      = "آدرس : ";
  var FA_PHONE     = "تلفن : ";
  var FA_DOC_REC   = "قبض پزشک";
  var FA_INS_REC   = "رسید بیمه";
  var FA_DOC_BOTH  = "قبض و رسید";
  var FA_APPT      = "تاریخ نوبت : ";
  var FA_QUEUE     = "نوبت : ";
  var FA_NID       = "کد ملی : ";
  var FA_BARCODE   = "بارکد : ";
  var FA_BASEINS   = "بیمه پایه : ";
  var FA_SUPP      = "مکمل : ";
  var FA_FULL      = "نام بیمار : ";
  var FA_FILENO    = "شماره پرونده ";
  var FA_ARCHIVE   = "شماره سابقه ";
  var FA_DOCTOR    = "دکتر : ";
  var FA_SPECIALTY = "شرح تخصص : ";
  var FA_PAID      = "پرداختی : ";
  var FA_TOTAL     = "قیمت کل : ";
  var FA_INSSHARE  = "سهم پایه : ";
  var FA_SUPPPAY   = "سهم مکمل : ";
  var FA_DISCFROM  = "تخفیف از : ";
  var FA_DISCOUNT  = "تخفیف : ";
  var FA_CASH      = "نقد : ";
  var FA_POS       = "POS : ";
  var FA_EPRESC    = "کد رهگیری نسخه الکترونیک : ";
  var FA_REFERRAL  = "شماره معرفی نامه : ";
  var FA_RECEPTION = "پذیرش : ";
  var FA_CASHIER   = "صندوق : ";
  var FA_SC        = "ش.ص : ";
  var FA_CASH_SFX  = " / دار";
  var FA_TEAR      = "— — — محل جدا کردن — — —";
  var FA_STUB      = "نسخهٔ بیمار";
  var FA_PAYLBL    = "پرداخت‌ها";
  var FA_SHARELBL  = "سهم بیمه";
  var FA_SVCLIST   = "شرح خدمات";

  function putLF(d, x, y, w, h, label, field, pt, lblBold, valBold, lwFrac) {
    var lw = w * (lwFrac == null ? 0.42 : lwFrac), gap = 0.8, vw = w - lw - gap;
    if (vw < 5) vw = 5;
    d.push(L(x + w - lw, y, lw, h, label, pt, lblBold, 0));
    if (field) {
      if (valBold) d.push(FB(x, y, vw, h, field, "", pt, 0));
      else         d.push(F (x, y, vw, h, field, "", pt, 0));
    }
  }

  function emitHeader(d, x, y, w, pt, rules, photo) {
    var pw = 14.0, ph = 16.0, tw = w;
    if (photo) tw = w - pw - 2.0;
    var namePt = pt + 3.0;
    d.push(L(x, y, tw, 6.2, FA_CLINIC, namePt, true, 1)); y += 6.4;
    d.push(F(x, y, tw, 4.0, "clinicaddr",  FA_ADDR,  pt - 1.5, 1)); y += 4.2;
    d.push(F(x, y, tw, 3.8, "clinicphone", FA_PHONE, pt - 1.5, 1)); y += 4.0;
    if (photo) d.push(PHOTO(x + w - pw, y - 14.4, pw, ph));
    if (rules) { d.push(HL(x, y, w, 0.3)); y += 1.2; }
    return y;
  }

  function emitTitle(d, x, y, w, pt, title) {
    d.push(L(x, y, w, 5.6, title, pt + 1.5, true, 1));
    y += 6.0;
    d.push(HL(x, y, w, 0.3));
    return y + 0.8;
  }

  function emitBarcode(d, x, y, w, bcW, bcH) {
    if (bcW > w - 1.0) bcW = w - 1.0;
    d.push(BARCODE(x + (w - bcW) / 2.0, y + 0.3, bcW, bcH));
    return y + bcH + 1.2;
  }

  function emitSamenInfo(d, x, y, w, rH, pt, bw) {
    var half = w / 2.0, fh = rH - 0.7;
    function vline(cy) { d.push(VL(x + half, cy, rH, bw)); }
    function rule(cy)  { d.push(HL(x, cy, w, bw)); }

    vline(y);
    var rx = x + half + 0.5, rw = half - 1.0;
    var lblW = rw * 0.40, g = 0.5, rem = rw - lblW - 2 * g; if (rem < 8) rem = 8;
    var f1 = rem * 0.58, f2 = rem * 0.42;
    d.push(F (rx,          y + 0.3, f2, fh, "appttime", "", pt, 0));
    d.push(FB(rx + f2 + g,  y + 0.3, f1, fh, "apptdate", "", pt, 0));
    d.push(L (rx + rw - lblW, y + 0.3, lblW, fh, FA_APPT, pt, true, 0));
    putLF(d, x + 0.5, y + 0.3, half - 1.0, fh, FA_QUEUE, "queue", pt, true, false, 0.38);
    y += rH; rule(y);

    vline(y);
    putLF(d, x + half + 0.5, y + 0.3, half - 1.0, fh, FA_NID, "nid", pt, true, false, 0.40);
    putLF(d, x + 0.5,      y + 0.3, half - 1.0, fh, FA_BARCODE, "receiptbarcode", pt, false, false, 0.32);
    y += rH; rule(y);

    vline(y);
    putLF(d, x + half + 0.5, y + 0.3, half - 1.0, fh, FA_BASEINS, "ins", pt, false, false, 0.42);
    var lx = x + 0.5, lw = half - 1.0;
    lblW = lw * 0.28; g = 0.5; rem = lw - lblW - 2 * g;
    f1 = rem * 0.58; f2 = rem * 0.42;
    d.push(F(lx,          y + 0.3, f2, fh, "supp_percent", "", pt, 0));
    d.push(F(lx + f2 + g,  y + 0.3, f1, fh, "supp",         "", pt, 0));
    d.push(L(lx + lw - lblW, y + 0.3, lblW, fh, FA_SUPP, pt, false, 0));
    y += rH; rule(y);

    var w1 = w * 0.42, w2 = w * 0.33, w3 = w - w1 - w2;
    d.push(VL(x + w1, y, rH, bw));
    d.push(VL(x + w1 + w2, y, rH, bw));
    putLF(d, x + w1 + w2 + 0.4, y + 0.3, w3 - 0.8, fh, FA_FULL, "full", pt, true, false, 0.44);
    var ptB = pt + 1.5;
    putLF(d, x + w3 + 0.4, y + 0.25, w2 - 0.8, fh, FA_FILENO, "fileNo", ptB, true, true, 0.52);
    d[d.length - 1].bold = true; d[d.length - 1].pt = ptB;
    putLF(d, x + 0.4, y + 0.4, w1 - 0.8, fh - 0.2, FA_ARCHIVE, "archiveNo", pt - 1.5, false, false, 0.52);
    y += rH; rule(y);

    var lblWd = (w < 60) ? 11.0 : 13.0, hyW = 2.4, codeW = w * 0.20;
    if (codeW < 10) codeW = 10;
    var docW = w - lblWd - hyW - codeW - 1.2; if (docW < 12) docW = 12;
    var xl = x + 0.5;
    d.push(F (xl,           y + 0.3, codeW, fh, "doctorcode", "", pt, 0));
    d.push(L (xl + codeW,   y + 0.3, hyW,   fh, "-", pt, false, 1));
    d.push(FB(xl + codeW + hyW, y + 0.3, docW, fh, "doctor", "", pt, 0));
    d.push(L (x + w - lblWd - 0.4, y + 0.3, lblWd, fh, FA_DOCTOR, pt, true, 0));
    y += rH; rule(y);

    var codeWs = (w < 60) ? 6.0 : 7.5, emptyW = w * 0.16;
    if (emptyW < 8) emptyW = 8;
    var specW = w - codeWs - emptyW - 1.0;
    d.push(VL(x + codeWs + 0.4, y, rH, bw));
    d.push(VL(x + codeWs + emptyW, y, rH, bw));
    d.push(F(x + 0.4, y + 0.3, codeWs, fh, "specialtycode", "", pt - 1.0, 1));
    putLF(d, x + codeWs + emptyW + 0.4, y + 0.3, specW - 0.6, fh, FA_SPECIALTY, "specialty", pt, false, false, 0.36);
    y += rH; rule(y);
    return y;
  }

  function emitSamenInfoSplit(d, x, y, w, rH, pt, bw) {
    var gap = 2.4, colW = (w - gap) / 2.0, fh = rH - 0.7;
    var lx = x, rx = x + colW + gap, n = 6, i;
    d.push(RECT(lx, y, colW, n * rH, bw));
    d.push(RECT(rx, y, colW, n * rH, bw));
    for (i = 1; i < n; i++) {
      d.push(HL(lx, y + i * rH, colW, bw));
      d.push(HL(rx, y + i * rH, colW, bw));
    }
    function cell(cx, row, lab, fld, lb, vb, frac, ptv) {
      putLF(d, cx + 0.6, y + row * rH + 0.3, colW - 1.2, fh, lab, fld, ptv, lb, vb, frac);
    }
    cell(rx, 0, FA_FULL,    "full",           true,  false, 0.44, pt);
    cell(rx, 1, FA_NID,     "nid",            true,  false, 0.40, pt);
    cell(rx, 2, FA_FILENO,  "fileNo",         true,  true,  0.52, pt + 1.0);
    cell(rx, 3, FA_ARCHIVE, "archiveNo",      false, false, 0.52, pt - 1.0);
    cell(rx, 4, FA_BASEINS, "ins",            false, false, 0.42, pt);
    var cw = colW - 1.2, xx = rx + 0.6, yy = y + 5 * rH + 0.3;
    var lblW = cw * 0.28, g = 0.5, rem = cw - lblW - 2 * g, f1 = rem * 0.58, f2 = rem * 0.42;
    d.push(F(xx,         yy, f2, fh, "supp_percent", "", pt, 0));
    d.push(F(xx + f2 + g, yy, f1, fh, "supp",         "", pt, 0));
    d.push(L(xx + cw - lblW, yy, lblW, fh, FA_SUPP, pt, false, 0));
    cw = colW - 1.2; xx = lx + 0.6; yy = y + 0.3;
    lblW = cw * 0.40; g = 0.5; rem = cw - lblW - 2 * g; f1 = rem * 0.58; f2 = rem * 0.42;
    d.push(F (xx,         yy, f2, fh, "appttime", "", pt, 0));
    d.push(FB(xx + f2 + g, yy, f1, fh, "apptdate", "", pt, 0));
    d.push(L (xx + cw - lblW, yy, lblW, fh, FA_APPT, pt, true, 0));
    cell(lx, 1, FA_QUEUE,     "queue",          true,  false, 0.38, pt);
    cell(lx, 2, FA_BARCODE,   "receiptbarcode", false, false, 0.34, pt);
    cell(lx, 3, FA_DOCTOR,    "doctor",         true,  false, 0.30, pt);
    d.push(F(lx + 0.6, y + 4 * rH + 0.3, colW - 1.2, fh, "doctorcode", "-", pt, 0));
    cell(lx, 5, FA_SPECIALTY, "specialty",      false, false, 0.40, pt);
    d.push(F(lx + colW - 8.0, y + 5 * rH + 0.3, 7.0, fh, "specialtycode", "", pt - 1.0, 1));
    return y + n * rH;
  }

  function emitSamenFinance(d, x, y, w, rH, pt, bw) {
    var half = w / 2.0, fh = rH - 0.7;
    function row2(l1, f1, b1, l2, f2) {
      d.push(VL(x + half, y, rH, bw));
      putLF(d, x + half + 0.5, y + 0.3, half - 1.0, fh, l1, f1, pt, b1, false, 0.44);
      putLF(d, x + 0.5,      y + 0.3, half - 1.0, fh, l2, f2, pt, false, false, 0.44);
      y += rH; d.push(HL(x, y, w, bw));
    }
    row2(FA_PAID,     "paid",          true,  FA_TOTAL,    "total");
    row2(FA_INSSHARE, "insshare",      false, FA_SUPPPAY,  "supppay");
    row2(FA_DISCFROM, "discount_from", false, FA_DISCOUNT, "discount");
    row2(FA_CASH,     "cash",          true,  FA_POS,      "pos");
    return y;
  }

  function emitDualFinance(d, x, y, w, rH, pt, bw) {
    var gap = 2.5, bw2 = (w - gap) / 2.0, capH = 5.2, fh = rH - 0.7;
    var lx = x, rx = x + bw2 + gap;
    d.push(L(lx + 0.5, y + 0.3, bw2 - 1.0, capH - 0.6, FA_PAYLBL,   pt - 0.5, true, 1));
    d.push(L(rx + 0.5, y + 0.3, bw2 - 1.0, capH - 0.6, FA_SHARELBL, pt - 0.5, true, 1));
    d.push(HL(lx, y + capH, bw2, bw));
    d.push(HL(rx, y + capH, bw2, bw));
    var iy = y + capH;
    putLF(d, lx + 0.5, iy + 0.3,      bw2 - 1.0, fh, FA_PAID,     "paid",          pt, true,  false, 0.44);
    d.push(HL(lx, iy + rH, bw2, bw));
    putLF(d, lx + 0.5, iy + rH + 0.3, bw2 - 1.0, fh, FA_TOTAL,    "total",         pt, false, false, 0.44);
    d.push(HL(lx, iy + 2 * rH, bw2, bw));
    putLF(d, lx + 0.5, iy + 2 * rH + 0.3, bw2 - 1.0, fh, FA_DISCFROM, "discount_from", pt, false, false, 0.44);
    d.push(HL(lx, iy + 3 * rH, bw2, bw));
    putLF(d, lx + 0.5, iy + 3 * rH + 0.3, bw2 - 1.0, fh, FA_DISCOUNT, "discount",      pt, false, false, 0.44);
    putLF(d, rx + 0.5, iy + 0.3,      bw2 - 1.0, fh, FA_INSSHARE, "insshare",      pt, false, false, 0.44);
    d.push(HL(rx, iy + rH, bw2, bw));
    putLF(d, rx + 0.5, iy + rH + 0.3, bw2 - 1.0, fh, FA_SUPPPAY,  "supppay",       pt, false, false, 0.44);
    d.push(HL(rx, iy + 2 * rH, bw2, bw));
    putLF(d, rx + 0.5, iy + 2 * rH + 0.3, bw2 - 1.0, fh, FA_CASH,     "cash",          pt, true,  false, 0.36);
    d.push(HL(rx, iy + 3 * rH, bw2, bw));
    putLF(d, rx + 0.5, iy + 3 * rH + 0.3, bw2 - 1.0, fh, FA_POS,      "pos",           pt, false, false, 0.30);
    var bottom = y + capH + 4 * rH;
    d.push(RECT(lx, y, bw2, capH + 4 * rH, bw));
    d.push(RECT(rx, y, bw2, capH + 4 * rH, bw));
    d.push(HL(x, bottom, w, bw));
    return bottom;
  }

  function emitSamenRef(d, x, y, w, rH, pt, bw) {
    var fh = rH - 0.7;
    putLF(d, x + 0.5, y + 0.3, w - 1.0, fh, FA_EPRESC,   "eprescription", pt - 0.5, false, false, 0.56);
    y += rH; d.push(HL(x, y, w, bw));
    putLF(d, x + 0.5, y + 0.3, w - 1.0, fh, FA_REFERRAL, "referralno",    pt - 0.5, false, false, 0.42);
    y += rH;
    return y;
  }

  function emitSamenFooter(d, x, y, w, pt) {
    var fh = 4.6, half = w / 2.0;
    d.push(HL(x, y, w, 0.25));
    d.push(VL(x + half, y + 0.3, fh, 0.25));
    putLF(d, x + half + 0.4, y + 0.5, half - 0.8, fh, FA_RECEPTION, "receptionist", pt - 1.0, false, false, 0.36);
    putLF(d, x + 0.4,      y + 0.5, half - 0.8, fh, FA_CASHIER,   "cashier",      pt - 1.0, false, false, 0.36);
    if (d.length) d[d.length - 1].suffix = FA_CASH_SFX;
    y += fh + 0.8;
    putLF(d, x + half + 0.4, y, half - 0.8, fh, FA_SC, "scnum", pt - 1.0, false, false, 0.28);
    d.push(F(x + 0.4, y, half - 0.8, fh, "reg_ts", "", pt - 1.5, 0));
    return y + fh + 0.4;
  }

  function emitStub(d, x, y, w, pt) {
    d.push(HL(x, y, w, 0.3));
    d.push(L(x, y + 0.4, w, 3.6, FA_TEAR, pt - 1.5, false, 1));
    y += 4.6;
    d.push(L(x, y, w, 4.8, FA_STUB, pt, true, 1));
    y += 5.2;
    var rH = 4.8, fh = 4.2, half = w / 2.0;
    d.push(RECT(x, y, w, 2 * rH, 0.3));
    d.push(VL(x + half, y, 2 * rH, 0.3));
    putLF(d, x + half + 0.5, y + 0.3, half - 1.0, fh, FA_FULL,  "full",  pt - 1.0, true, false, 0.40);
    putLF(d, x + 0.5,      y + 0.3, half - 1.0, fh, FA_QUEUE, "queue", pt - 1.0, false, false, 0.36);
    d.push(HL(x, y + rH, w, 0.3));
    putLF(d, x + 0.5, y + rH + 0.3, w - 1.0, fh, FA_PAID, "paid", pt, true, true, 0.28);
    return y + 2 * rH + 1.0;
  }

  function paperSize(p) {
    if (p === "R80") return [80, 200];
    if (p === "R58") return [58, 200];
    if (p === "A5")  return [148, 210];
    return [210, 297];
  }
  function paperMargin(pw) {
    if (pw <= 80.0) return 3.0;
    if (pw <= 148.0) return 6.0;
    return 8.0;
  }

  function buildSamenDefault() {
    var PW = 80.0, PH = 200.0, M = 3.0, bw = 0.30, pt = 8.0, rH = 5.2;
    var d = [], x = M, w = PW - 2 * M, y = M;
    d.push(L(x, y, w, 6.4, FA_CLINIC, 11.0, true, 1)); y += 6.6;
    d.push(F(x, y, w, 4.0, "clinicaddr",  FA_ADDR,  7.0, 1)); y += 4.2;
    d.push(F(x, y, w, 3.8, "clinicphone", FA_PHONE, 7.0, 1)); y += 4.2;
    var bcW = 50.0, bcH = 8.0;
    d.push(BARCODE(x, y + 0.2, bcW, bcH));
    d.push(L(x + bcW + 1.0, y + 1.0, w - bcW - 1.0, 6.4, FA_DOC_REC, 9.5, true, 0));
    y += bcH + 1.0;
    var footH = 10.5, boxTop = y, boxBot = PH - M - footH, boxH = boxBot - boxTop;
    var ix = x + 0.8, iw = w - 1.6, iy = boxTop + 0.8;
    iy = emitSamenInfo(d, ix, iy, iw, rH, pt, bw);
    var finRefH = 6 * rH;
    var svcH = boxBot - 0.8 - iy - finRefH;
    if (svcH < 20.0) svcH = 20.0;
    d.push(SERVICES(ix, iy, iw, svcH, 7.0, SVC_SAMEN, bw, 4.6, 5.0));
    iy += svcH;
    iy = emitSamenFinance(d, ix, iy, iw, rH, pt, bw);
    iy = emitSamenRef(d, ix, iy, iw, rH, pt, bw);
    d.push(FRAME_BOX(x, boxTop, w, boxH, bw));
    emitSamenFooter(d, x, boxBot + 0.4, w, pt);
    return { paper: "R80", items: d };
  }

  var EXTRA = [
    { lay: 0, v: 0, s: SVC3,      paper: "R80", bw: 0.30, rh: 5.2, fr: false, photo: false, logo: false, tear: false },
    { lay: 0, v: 1, s: SVC4_ROW,  paper: "R80", bw: 0.30, rh: 5.0, fr: true,  photo: false, logo: false, tear: false },
    { lay: 0, v: 2, s: SVC5,      paper: "A5",  bw: 0.30, rh: 5.6, fr: true,  photo: false, logo: false, tear: false },
    { lay: 1, v: 0, s: SVC3,      paper: "R80", bw: 0.30, rh: 5.0, fr: false, photo: false, logo: false, tear: false },
    { lay: 1, v: 1, s: SVC4_CAT,  paper: "R58", bw: 0.28, rh: 4.8, fr: false, photo: false, logo: false, tear: false },
    { lay: 1, v: 2, s: SVC5_CODE, paper: "A5",  bw: 0.30, rh: 5.6, fr: true,  photo: false, logo: false, tear: false },
    { lay: 2, v: 0, s: SVC6_FIN,  paper: "R80", bw: 0.30, rh: 5.0, fr: false, photo: false, logo: false, tear: false },
    { lay: 2, v: 1, s: SVC6_INS,  paper: "A5",  bw: 0.30, rh: 5.4, fr: true,  photo: false, logo: false, tear: false },
    { lay: 2, v: 2, s: SVC7,      paper: "A4",  bw: 0.30, rh: 5.8, fr: true,  photo: false, logo: false, tear: false },
    { lay: 3, v: 0, s: SVC3,      paper: "A5",  bw: 0.30, rh: 5.4, fr: false, photo: false, logo: true,  tear: false },
    { lay: 3, v: 1, s: SVC4_ROW,  paper: "A4",  bw: 0.30, rh: 5.8, fr: true,  photo: false, logo: true,  tear: false },
    { lay: 3, v: 2, s: SVC5,      paper: "A4",  bw: 0.30, rh: 6.0, fr: false, photo: true,  logo: true,  tear: false },
    { lay: 4, v: 0, s: SVC3,      paper: "A5",  bw: 0.25, rh: 5.4, fr: false, photo: false, logo: false, tear: false },
    { lay: 4, v: 1, s: SVC4_ROW,  paper: "A4",  bw: 0.25, rh: 5.8, fr: true,  photo: false, logo: false, tear: false },
    { lay: 4, v: 2, s: SVC5,      paper: "A4",  bw: 0.25, rh: 6.0, fr: false, photo: true,  logo: false, tear: false },
    { lay: 5, v: 0, s: SVC3,      paper: "R80", bw: 0.30, rh: 5.0, fr: false, photo: false, logo: false, tear: false },
    { lay: 5, v: 1, s: SVC4_CAT,  paper: "A5",  bw: 0.30, rh: 5.4, fr: false, photo: false, logo: false, tear: false },
    { lay: 5, v: 2, s: SVC6_INS,  paper: "A4",  bw: 0.30, rh: 5.8, fr: true,  photo: false, logo: false, tear: false },
    { lay: 6, v: 0, s: SVC3,      paper: "R58", bw: 0.28, rh: 4.8, fr: false, photo: false, logo: false, tear: false },
    { lay: 6, v: 1, s: SVC5,      paper: "R80", bw: 0.30, rh: 5.2, fr: false, photo: false, logo: false, tear: false },
    { lay: 6, v: 2, s: SVC5_CODE, paper: "A5",  bw: 0.30, rh: 5.6, fr: true,  photo: false, logo: false, tear: false },
    { lay: 7, v: 0, s: SVC3,      paper: "R58", bw: 0.25, rh: 4.6, fr: false, photo: false, logo: false, tear: false },
    { lay: 7, v: 1, s: SVC4_ROW,  paper: "R58", bw: 0.28, rh: 4.8, fr: true,  photo: false, logo: false, tear: false },
    { lay: 7, v: 2, s: SVC5,      paper: "R80", bw: 0.30, rh: 5.0, fr: false, photo: false, logo: false, tear: false },
    { lay: 8, v: 0, s: SVC3,      paper: "A5",  bw: 0.30, rh: 5.2, fr: false, photo: false, logo: false, tear: true  },
    { lay: 8, v: 1, s: SVC4_ROW,  paper: "A4",  bw: 0.30, rh: 5.4, fr: true,  photo: false, logo: false, tear: true  },
    { lay: 8, v: 2, s: SVC5,      paper: "A4",  bw: 0.30, rh: 5.6, fr: false, photo: false, logo: false, tear: true  },
    { lay: 9, v: 0, s: SVC3,      paper: "A5",  bw: 0.30, rh: 5.4, fr: false, photo: false, logo: false, tear: false },
    { lay: 9, v: 1, s: SVC6_FIN,  paper: "A4",  bw: 0.30, rh: 5.8, fr: true,  photo: false, logo: false, tear: false },
    { lay: 9, v: 2, s: SVC5_CODE, paper: "A4",  bw: 0.30, rh: 6.0, fr: true,  photo: false, logo: false, tear: false }
  ];

  var NAMES = [
    "پیش‌فرض",
    "۰۱) قبض پزشک — رول ۸۰ کلاسیک",
    "۰۲) رسید بیمه — رول ۸۰ قاب‌دار",
    "۰۳) قبض و رسید — A5 کلاسیک",
    "۰۴) قبض پزشک — رول ۸۰ خدمات‌بالا",
    "۰۵) رسید بیمه — رول ۵۸ خدمات‌بالا",
    "۰۶) قبض و رسید — A5 خدمات‌بالا",
    "۰۷) قبض پزشک — رول ۸۰ مالی‌اول",
    "۰۸) رسید بیمه — A5 مالی‌اول",
    "۰۹) قبض و رسید — A4 مالی‌اول",
    "۱۰) قبض پزشک — A5 ستون کناری",
    "۱۱) رسید بیمه — A4 ستون کناری",
    "۱۲) قبض و رسید — A4 ستون و عکس",
    "۱۳) قبض پزشک — A5 دو ستونه",
    "۱۴) رسید بیمه — A4 دو ستونه",
    "۱۵) قبض و رسید — A4 دو ستونه عکس",
    "۱۶) قبض پزشک — رول ۸۰ با عنوان بخش",
    "۱۷) رسید بیمه — A5 با عنوان بخش",
    "۱۸) قبض و رسید — A4 با عنوان بخش",
    "۱۹) قبض پزشک — رول ۵۸ بارکد پایین",
    "۲۰) رسید بیمه — رول ۸۰ بارکد پایین",
    "۲۱) قبض و رسید — A5 بارکد پایین",
    "۲۲) قبض پزشک — رول ۵۸ فشرده",
    "۲۳) رسید بیمه — رول ۵۸ فشرده قاب‌دار",
    "۲۴) قبض و رسید — رول ۸۰ فشرده",
    "۲۵) قبض پزشک — A5 ته‌برگ",
    "۲۶) رسید بیمه — A4 ته‌برگ",
    "۲۷) قبض و رسید — A4 ته‌برگ",
    "۲۸) قبض پزشک — A5 دو بلوک مالی",
    "۲۹) رسید بیمه — A4 دو بلوک مالی",
    "۳۰) قبض و رسید — A4 دو بلوک مالی قاب‌دار"
  ];

  function buildExtra(extraIdx) {
    var sp = EXTRA[extraIdx];
    var sz = paperSize(sp.paper), PW = sz[0], PH = sz[1];
    var d = [];
    var lay = sp.lay, vari = sp.v;
    var M = paperMargin(PW);
    var bw = sp.bw, rH = sp.rh;
    var pt = (PW <= 58.0) ? 7.2 : (PW <= 80.0) ? 8.2 : (PW <= 148.0) ? 9.0 : 9.5;
    var bcW = (PW <= 58.0) ? 44.0 : (PW <= 80.0) ? 52.0 : (PW <= 148.0) ? 64.0 : 72.0;
    var bcH = 7.0;
    var footH = (PW <= 80.0) ? 11.0 : 13.5;
    var stubH = sp.tear ? 16.0 : 0.0;
    var x = M, w = PW - 2 * M, y = M;
    var yMax = PH - M - footH - stubH;
    var title = (vari === 0) ? FA_DOC_REC : (vari === 1) ? FA_INS_REC : FA_DOC_BOTH;
    if (sp.fr) d.push(FRAME(PW, PH, (M > 3.0) ? (M - 1.5) : 2.0, 0.3));

    function caption(txt) {
      d.push(HL(x, y, w, 0.3));
      d.push(L(x + 0.6, y + 0.2, w - 1.2, 4.0, txt, pt - 1.0, true, 1));
      d.push(HL(x, y + 4.4, w, 0.3));
      y += 4.8;
    }
    function emitSvcAt(svcH) {
      d.push(SERVICES(x, y, w, svcH, pt - 0.8, sp.s, bw, rH - 0.8, rH - 0.5));
      y += svcH;
    }
    function emitBc() { y = emitBarcode(d, x, y, w, bcW, bcH); }

    if (lay === 3 && sp.logo) {
      var sbW = (PW >= 210.0) ? 40.0 : 30.0;
      var sbX = x, mainX = x + sbW + 3.0, mainW = w - sbW - 3.0;
      var sy = y;
      d.push(LOGO(sbX + (sbW - 18.0) / 2.0, sy, 18.0, 18.0)); sy += 20.0;
      d.push(L(sbX + 0.4, sy, sbW - 0.8, 10.0, FA_CLINIC, pt, true, 1)); sy += 11.0;
      d.push(F(sbX + 0.4, sy, sbW - 0.8, 10.0, "clinicaddr", FA_ADDR, pt - 2.0, 1)); sy += 11.0;
      d.push(F(sbX + 0.4, sy, sbW - 0.8, 5.0, "clinicphone", FA_PHONE, pt - 2.0, 1)); sy += 6.0;
      sy = emitBarcode(d, sbX + 0.4, sy, sbW - 0.8, (sbW - 2.0 < bcW) ? (sbW - 2.0) : bcW, bcH);
      d.push(RECT(sbX, y, sbW, sy - y + 1.0, bw));
      var mx = mainX, mw = mainW, my = y;
      if (sp.photo) d.push(PHOTO(mx + mw - 16.0, my, 15.0, 18.0));
      d.push(L(mx, my, mw - (sp.photo ? 17.0 : 0), 6.0, title, pt + 1.5, true, 1)); my += 7.0;
      my = emitSamenInfo(d, mx, my, mw, rH, pt, bw);
      var remain = yMax - my - 6 * rH; if (remain < 22.0) remain = 22.0;
      if (vari === 1) remain += 2.0; else if (vari === 2) remain += 4.0;
      d.push(SERVICES(mx, my, mw, remain, pt - 0.8, sp.s, bw, rH - 0.8, rH - 0.5));
      my += remain;
      my = emitSamenFinance(d, mx, my, mw, rH, pt, bw);
      my = emitSamenRef(d, mx, my, mw, rH, pt, bw);
      d.push(RECT(mx, y, mw, my - y, bw));
      emitSamenFooter(d, mx, (my + 1.0 < PH - M - stubH) ? my + 1.0 : PH - M - stubH - footH, mw, pt);
      return { paper: sp.paper, items: d };
    }

    y = emitHeader(d, x, y, w, pt, true, sp.photo);
    y = emitTitle(d, x, y, w, pt, title);

    var svcH = (PW <= 58.0) ? 22.0 : (PW <= 80.0) ? 28.0 : (PW <= 148.0) ? 36.0 : 42.0;
    if (vari === 1) svcH += 2.0; else if (vari === 2) svcH += 4.0;

    if (lay === 1) {
      emitSvcAt(svcH); emitBc();
      y = emitSamenInfo(d, x, y, w, rH, pt, bw);
      y = emitSamenFinance(d, x, y, w, rH, pt, bw);
      y = emitSamenRef(d, x, y, w, rH, pt, bw);
    } else if (lay === 2) {
      y = emitSamenFinance(d, x, y, w, rH, pt, bw);
      y = emitSamenInfo(d, x, y, w, rH, pt, bw);
      emitSvcAt(svcH); emitBc();
      y = emitSamenRef(d, x, y, w, rH, pt, bw);
    } else if (lay === 4) {
      y = emitSamenInfoSplit(d, x, y, w, rH, pt, bw);
      emitSvcAt(svcH); emitBc();
      y = emitSamenFinance(d, x, y, w, rH, pt, bw);
      y = emitSamenRef(d, x, y, w, rH, pt, bw);
    } else if (lay === 5) {
      caption("مشخصات بیمار");
      y = emitSamenInfo(d, x, y, w, rH, pt, bw);
      caption(FA_SVCLIST);
      emitSvcAt(svcH); emitBc();
      caption("مالی و پرداخت");
      y = emitSamenFinance(d, x, y, w, rH, pt, bw);
      y = emitSamenRef(d, x, y, w, rH, pt, bw);
    } else if (lay === 6) {
      y = emitSamenInfo(d, x, y, w, rH, pt, bw);
      emitSvcAt(svcH);
      y = emitSamenFinance(d, x, y, w, rH, pt, bw);
      y = emitSamenRef(d, x, y, w, rH, pt, bw);
      emitBc();
    } else if (lay === 7) {
      y = emitSamenInfo(d, x, y, w, rH, pt - 0.4, bw);
      emitSvcAt(svcH);
      y = emitSamenFinance(d, x, y, w, rH, pt - 0.4, bw);
      y = emitSamenRef(d, x, y, w, rH, pt - 0.4, bw);
      emitBc();
    } else if (lay === 9) {
      y = emitSamenInfo(d, x, y, w, rH, pt, bw);
      emitSvcAt(svcH); emitBc();
      y = emitDualFinance(d, x, y, w, rH, pt, bw);
      y = emitSamenRef(d, x, y, w, rH, pt, bw);
    } else {
      y = emitSamenInfo(d, x, y, w, rH, pt, bw);
      emitSvcAt(svcH); emitBc();
      y = emitSamenFinance(d, x, y, w, rH, pt, bw);
      y = emitSamenRef(d, x, y, w, rH, pt, bw);
    }

    var footY = y + 1.0;
    var footLimit = PH - M - stubH - 11.0;
    if (footY > footLimit) footY = footLimit;
    y = emitSamenFooter(d, x, footY, w, pt);
    if (sp.tear) emitStub(d, x, y + 0.6, w, pt);
    return { paper: sp.paper, items: d };
  }

  var ALL = [];
  var i, built, items, j;
  for (i = 0; i < 31; i++) {
    _uid = 0;
    built = (i === 0) ? buildSamenDefault() : buildExtra(i - 1);
    items = built.items;
    for (j = 0; j < items.length; j++) { items[j].id = j + 1; items[j].z = j + 1; }
    ALL.push({ id: 0, name: NAMES[i], kind: "builtin", group: "reception",
      paper: built.paper, orientation: 0, items: items });
  }

  window.AZ_TEMPLATES = ALL;
})();
