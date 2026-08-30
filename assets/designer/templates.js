/* ===========================================================================
   templates.js — DarmanPlus ready-made print designs  (v2.07.1)

   ★ این فایل آینهٔ دقیق آهنگساز C++ (src/print_designer_templates.inc) است:
     همان تابع بساز_طرح(طرح_پارامتر)، همان ۱۳ بلوک اجباری، همان ۳۰ ردیفِ
     پارامتر. هر تغییری در .inc باید عیناً اینجا هم اعمال شود.

   ★ سیاه‌وسفیدِ خالص: هیچ طرحی رنگ ندارد — چاپگر درمانگاه مونوکروم است.
     هر متن، خط و کادر #000000 روی کاغذ سفید است. تأکید فقط از طریق
     ضخامت قلم/خط و کادر است، نه رنگ.
   =========================================================================== */
(function () {
  "use strict";

  var INK = "#000000";

  /* paper dimensions in mm (mirrors print_designer.cpp Paper_Dims) */
  var PAPER_MM = {
    A3: [297, 420], A4: [210, 297], A5: [148, 210], A6: [105, 148],
    B5: [176, 250], Letter: [216, 279]
  };
  function rollMm(name) {
    if (name === "R80") return [80, 200];
    if (name === "R58") return [58, 200];
    return null;
  }

  var _uid = 0;
  function nid() { return ++_uid; }

  /* ---------------------------------------------------------------- items */
  function mkLabel(x, y, w, h, text, pt, bold, align) {
    return { id: nid(), type: "label", x: x, y: y, w: w, h: h, rot: 0, z: 0,
      locked: false, isFrame: false, text: text, field: "", prefix: "", suffix: "",
      font: "Vazirmatn", pt: pt, bold: !!bold, italic: false,
      align: align || 0, dir: 0, valign: 0, lineSpacing: 1,
      objectFit: "contain", textColor: INK, fillColor: "#ffffff",
      fillTransparent: true, borderColor: INK, borderWidth: 0, borderStyle: 0,
      corner: 0, padding: 1, opacity: 1, visibility: 0,
      rowH: 0, headerH: 0, imgPath: "" };
  }
  function mkField(x, y, w, h, field, prefix, pt, align, bold) {
    var it = mkLabel(x, y, w, h, "", pt, !!bold, align);
    it.type = "field"; it.field = field; it.prefix = prefix || "";
    return it;
  }
  function mkHLine(x, y, w, bw) {
    var it = mkLabel(x, y, w, 0.2, "", 8, false, 0);
    it.type = "hline"; it.borderWidth = bw;
    return it;
  }
  function mkVLine(x, y, h, bw) {
    var it = mkLabel(x, y, 0.2, h, "", 8, false, 0);
    it.type = "vline"; it.borderWidth = bw;
    return it;
  }
  function mkFrame(pw, ph, margin, bw) {
    var it = mkLabel(margin, margin, pw - 2 * margin, ph - 2 * margin, "", 8, false, 0);
    it.type = "frame"; it.isFrame = true; it.borderWidth = bw;
    return it;
  }
  /* §3.4: the barcode value appears ONLY as the HRI text under the graphic */
  function mkBarcode(x, y, w, h) {
    var it = mkLabel(x, y, w, h, "", 8, false, 1);
    it.type = "barcode"; it.field = "receiptbarcode";
    it.text = '{"sym":"code128","hri":true,"quiet":2}';
    it.pt = 8; it.align = 1; it.textColor = INK;
    return it;
  }
  /* §3.7: the canonical 3-column services model */
  function mkServices(x, y, w, h, pt, bw, rowH, headerH) {
    var it = mkLabel(x, y, w, h, "", pt, false, 1);
    it.type = "services";
    it.text = '{"cols":3,"header":true,"widths":[0.46,0.12,0.42],'
            + '"labels":["نام خدمت","تعداد","شرح خدمت"]}';
    it.fontPt = pt; it.align = 1; it.bold = false;
    it.borderColor = INK; it.borderWidth = bw; it.textColor = INK;
    it.fillColor = "#ffffff"; it.fillTransparent = true; it.padding = 0.6;
    it.headerH = headerH; it.rowH = rowH;
    return it;
  }

  /* label+field row (RTL: label right, value left) — mirrors putLF */
  function putLF(v, x, y, w, h, label, field, pt, lblBold, valBold, lwFrac) {
    var lw = w * (lwFrac == null ? 0.42 : lwFrac), gap = 0.8;
    var vw = w - lw - gap; if (vw < 5) vw = 5;
    v.push(mkLabel(x + w - lw, y, lw, h, label, pt, lblBold, 0));
    v.push(mkField(x, y, vw, h, field, "", pt, 0, valBold));
  }

  /* =========================================================================
     THE COMPOSER — exact JS mirror of بساز_طرح(طرح_پارامتر).
     Always emits the 13 mandatory blocks of §4.3, in order.
     ========================================================================= */
  function بساز_طرح(p) {
    _uid = 0;
    var v = [];
    var dims = rollMm(p.کاغذ) || PAPER_MM[p.کاغذ] || PAPER_MM.A5;
    var PW = dims[0], PH = dims[1];
    if (p.جهت === 1) { var t = PW; PW = PH; PH = t; }

    var M = (PW <= 60) ? 3.0 : (PW <= 90) ? 4.0 : (PW <= 150) ? 7.0 : 9.0;
    var bw = 0.30;                       /* §3.8 hairline minimum */
    var pt = p.قلم_پایه;
    var roll = (PW <= 90);
    var x = M, w = PW - 2 * M, y = M;

    /* B/W emphasis: 0 plain, 1 heavier letterhead rule, 2 heaviest + frame */
    var ruleBw = (p.تأکید === 2) ? bw * 2.0 : (p.تأکید === 1) ? bw * 1.5 : bw;
    if (ruleBw < 0.30) ruleBw = 0.30;

    /* ---------------- BLOCK 1-3: clinic name / address / phone ---------- */
    var namePt = pt + (roll ? 2.5 : 3.0);
    v.push(mkField(x, y, w, 5.6, "clinicname", "", namePt, 1, true));
    y += 5.8;
    v.push(mkField(x, y, w, 4.0, "clinicaddr", "آدرس : ", pt - 1.5, 1));
    y += 4.2;
    v.push(mkField(x, y, w, 3.8, "clinicphone", "تلفن : ", pt - 1.5, 1));
    y += 4.4;
    v.push(mkHLine(x, y, w, ruleBw)); y += 1.6;

    /* ---------------- BLOCK 4+5: receipt title (right) + barcode (left) - */
    (function () {
      var bcW = roll ? (w * 0.62) : (w * 0.42);
      var bcH = roll ? 7.5 : 9.0;
      if (bcW > w - 30) bcW = w - 30;
      var tW = w - bcW - 2.0;
      v.push(mkField(x + bcW + 2.0, y + 1.0, tW, 5.0, "receipttitle", "", pt + 1.5, 0, true));
      v.push(mkBarcode(x, y + 0.2, bcW, bcH));
      y += bcH + 1.6;
    })();
    v.push(mkHLine(x, y, w, bw)); y += 1.6;

    /* ---------------- BLOCKS 6-10: identity / visit / doctor ------------ */
    var twoCol = (p.چیدمان === 1);
    var banded = (p.چیدمان === 2);
    function captionBar(txt) {
      v.push(mkLabel(x, y, w, 3.6, txt, pt - 1.0, true, 1));
      y += 3.8;
    }
    if (twoCol) {
      var gap = 2.0, colW = (w - gap) / 2.0;
      var xR = x + colW + gap, xL = x;
      var yTop = y;
      putLF(v, xR, y, colW, 4.4, "تاریخ نوبت : ", "apptdate", pt, true, false, 0.52); y += 4.6;
      putLF(v, xR, y, colW, 4.4, "ساعت نوبت : ", "appttime", pt, false, false, 0.52); y += 4.6;
      putLF(v, xR, y, colW, 4.4, "نوبت : ", "queue", pt, false, true, 0.40); y += 4.6;
      putLF(v, xR, y, colW, 4.4, "کد ملی : ", "nid", pt, true, false, 0.46); y += 4.6;
      putLF(v, xR, y, colW, 4.4, "بیمه پایه : ", "ins_full", pt, false, false, 0.48); y += 4.6;
      putLF(v, xR, y, colW, 4.4, "بیمه مکمل : ", "supp_full", pt, false, false, 0.48); y += 4.6;
      var yR = y;
      y = yTop;
      var nm = mkField(xL, y, colW, 4.6, "P-Name", "", pt + 0.5, 0, true);
      nm.dir = 0; nm.align = 0; nm.valign = 1;           /* §3.5 no-wrap name */
      v.push(nm); y += 5.0;
      putLF(v, xL, y, colW, 4.4, "سن : ", "age", pt, false, false, 0.40); y += 4.6;
      putLF(v, xL, y, colW, 4.4, "شماره شناسنامه : ", "certno", pt, false, false, 0.52); y += 4.6;
      putLF(v, xL, y, colW, 4.4, "انجام‌دهنده : ", "performer", pt, true, false, 0.34); y += 4.6;
      putLF(v, xL, y, colW, 4.4, "کد نظام پزشکی : ", "performercode", pt, false, false, 0.56); y += 4.6;
      putLF(v, xL, y, colW, 4.4, "پزشک معالج : ", "doctor", pt, true, false, 0.36); y += 4.6;
      putLF(v, xL, y, colW, 4.4, "شرح تخصص : ", "specialty", pt, false, false, 0.40); y += 4.6;
      putLF(v, xL, y, colW, 4.4, "کد نظام پزشکی : ", "doctorcode", pt, false, false, 0.56); y += 4.6;
      if (yR > y) y = yR;
      v.push(mkVLine(x + colW + gap / 2.0, yTop, y - yTop, bw));
      v.push(mkHLine(x, y, w, bw)); y += 1.4;
    } else {
      if (banded) captionBar("مشخصات نوبت و بیمه");
      /* BLOCK 6 */
      (function () {
        var colW = (w - 2 * 0.6) / 3.0;
        putLF(v, x + 2 * (colW + 0.6), y, colW, 4.4, "تاریخ نوبت : ", "apptdate", pt, true, false, 0.52);
        putLF(v, x + (colW + 0.6), y, colW, 4.4, "ساعت نوبت : ", "appttime", pt, false, false, 0.52);
        putLF(v, x, y, colW, 4.4, "نوبت : ", "queue", pt, false, true, 0.40);
        y += 4.8; v.push(mkHLine(x, y, w, bw)); y += 1.2;
      })();
      /* BLOCK 7 */
      (function () {
        var colW = (w - 2 * 0.6) / 3.0;
        putLF(v, x + 2 * (colW + 0.6), y, colW, 4.4, "کد ملی : ", "nid", pt, true, false, 0.46);
        putLF(v, x + (colW + 0.6), y, colW, 4.4, "بیمه پایه : ", "ins_full", pt, false, false, 0.48);
        putLF(v, x, y, colW, 4.4, "بیمه مکمل : ", "supp_full", pt, false, false, 0.48);
        y += 4.8; v.push(mkHLine(x, y, w, bw)); y += 1.2;
      })();
      if (banded) captionBar("مشخصات بیمار و پزشک");
      /* BLOCK 8 */
      (function () {
        var nameW = w * 0.46, ageW = w * 0.22, certW = w - nameW - ageW - 1.2;
        putLF(v, x + ageW + certW + 1.2, y, nameW, 4.6, "نام بیمار : ", "P-Name", pt + 0.5, true, true, 0.34);
        var nm2 = v[v.length - 1];
        nm2.dir = 0; nm2.align = 0; nm2.valign = 1;       /* §3.5 no-wrap name */
        putLF(v, x + certW + 0.6, y, ageW, 4.4, "سن : ", "age", pt, false, false, 0.40);
        putLF(v, x, y, certW, 4.4, "شماره شناسنامه : ", "certno", pt, false, false, 0.52);
        y += 5.0; v.push(mkHLine(x, y, w, bw)); y += 1.2;
      })();
      /* BLOCK 9 */
      (function () {
        var codeW = w * 0.28;
        putLF(v, x + codeW + 0.6, y, w - codeW - 0.6, 4.4, "انجام‌دهنده : ", "performer", pt, true, false, 0.34);
        putLF(v, x, y, codeW, 4.4, "کد نظام پزشکی : ", "performercode", pt, false, false, 0.56);
        y += 4.8; v.push(mkHLine(x, y, w, bw)); y += 1.2;
      })();
      /* BLOCK 10 */
      (function () {
        var codeW = w * 0.24, specW = w * 0.34, docW = w - codeW - specW - 1.2;
        putLF(v, x + codeW + specW + 1.2, y, docW, 4.4, "پزشک معالج : ", "doctor", pt, true, false, 0.36);
        putLF(v, x + codeW + 0.6, y, specW, 4.4, "شرح تخصص : ", "specialty", pt, false, false, 0.40);
        putLF(v, x, y, codeW, 4.4, "کد نظام پزشکی : ", "doctorcode", pt, false, false, 0.56);
        y += 4.8; v.push(mkHLine(x, y, w, bw)); y += 1.4;
      })();
    }

    /* ---------------- BLOCK 11: جدول خدمات (3 columns) ------------------ */
    if (banded) captionBar("جدول خدمات");
    var footH = roll ? 26.0 : 34.0;
    var moneyH = roll ? 16.0 : 22.0;
    var svcY = y;
    var svcH = (PH - M - footH - moneyH) - y;
    if (svcH < 18.0) svcH = 18.0;
    if (!roll && svcH > (PH - 2 * M) * 0.42) svcH = (PH - 2 * M) * 0.42;
    v.push(mkServices(x, svcY, w, svcH, pt - 0.8, bw, roll ? 4.6 : 5.2, roll ? 4.8 : 5.4));
    y = svcY + svcH + 1.2;

    /* ---------------- BLOCK 12: خلاصهٔ پرداخت ---------------------------- */
    (function () {
      var colW = (w - 0.6) / 2.0;
      var rH = roll ? 3.8 : 4.6;
      function money2(l1, f1, l2, f2, boldL) {
        putLF(v, x + colW + 0.6, y, colW, rH, l1, f1, pt, boldL, false, 0.44);
        putLF(v, x, y, colW, rH, l2, f2, pt, boldL, false, 0.44);
      }
      money2("پرداختی : ", "paid", "قیمت کل : ", "total", true); y += rH;
      money2("سهم پایه : ", "basepay", "سهم مکمل : ", "supppay", false); y += rH;
      money2("تخفیف از : ", "discount_from", "تخفیف : ", "discount", false); y += rH;
      money2("نقد : ", "cash", "POS : ", "pos", false); y += rH + 0.8;
      v.push(mkHLine(x, y, w, bw)); y += 1.2;
    })();

    /* ---------------- BLOCK 13: پذیرش · صندوق‌دار · ش.ص · تاریخ و ساعت --- */
    (function () {
      var colW = (w - 0.6) / 2.0;
      var rH = roll ? 3.6 : 4.2;
      putLF(v, x + colW + 0.6, y, colW, rH, "پذیرش : ", "receptionist", pt - 0.5, false, false, 0.34);
      putLF(v, x, y, colW, rH, "صندوق‌دار : ", "cashier_name", pt - 0.5, false, false, 0.36);
      y += rH;
      putLF(v, x + colW + 0.6, y, colW, rH, "ش.ص : ", "scnum", pt - 0.5, false, false, 0.30);
      putLF(v, x, y, colW, rH, "تاریخ و ساعت : ", "datetime", pt - 0.5, false, false, 0.44);
    })();

    /* framing per layout family (never straddles the services bottom edge) */
    if (p.چیدمان === 0 && p.تأکید === 2 && !roll) {
      v.push(mkFrame(PW, PH, M - 1.0, 0.30));
    } else if (p.چیدمان === 3) {
      v.push(mkFrame(PW, PH, M - 1.0, 0.30));
    }

    var i;
    for (i = 0; i < v.length; i++) { v[i].id = i + 1; v[i].z = i + 1; }
    return { paper: p.کاغذ, orientation: p.جهت, items: v };
  }

  /* =========================================================================
     §4.2 — THE FROZEN PARAMETER TABLE (30 rows, literal constants — exact
     mirror of TPL_TABLE in print_designer_templates.inc)
     ========================================================================= */
  var TPL_TABLE = [
    { کد: "T01", نام: "طرح پیش‌فرض حرفه‌ای",      کاغذ: "A4",  جهت: 0, چیدمان: 0, تأکید: 1, قلم_پایه: 9.5 },
    { کد: "T02", نام: "طرح ساده و سریع",          کاغذ: "A5",  جهت: 0, چیدمان: 0, تأکید: 0, قلم_پایه: 9.0 },
    { کد: "T03", نام: "طرح رسمی کادردار",         کاغذ: "A4",  جهت: 0, چیدمان: 0, تأکید: 2, قلم_پایه: 9.5 },
    { کد: "T04", نام: "طرح فشردهٔ A5",            کاغذ: "A5",  جهت: 0, چیدمان: 0, تأکید: 0, قلم_پایه: 8.5 },
    { کد: "T05", نام: "طرح سربرگ رنگی مدرن",      کاغذ: "A4",  جهت: 0, چیدمان: 0, تأکید: 2, قلم_پایه: 9.5 },
    { کد: "T06", نام: "طرح شمارهٔ نوبت درشت",      کاغذ: "A5",  جهت: 0, چیدمان: 0, تأکید: 1, قلم_پایه: 9.5 },
    { کد: "T07", نام: "طرح رسید پرداخت",          کاغذ: "A5",  جهت: 0, چیدمان: 0, تأکید: 1, قلم_پایه: 9.0 },
    { کد: "T08", نام: "طرح دو ستونهٔ شیک",        کاغذ: "A4",  جهت: 0, چیدمان: 1, تأکید: 1, قلم_پایه: 9.0 },
    { کد: "T09", نام: "طرح مینیمال خط‌دار",        کاغذ: "A5",  جهت: 0, چیدمان: 0, تأکید: 0, قلم_پایه: 9.0 },
    { کد: "T10", نام: "طرح کارت بیمار",           کاغذ: "A6",  جهت: 0, چیدمان: 3, تأکید: 1, قلم_پایه: 8.5 },
    { کد: "T11", نام: "طرح نواری ۸۰ میلی‌متر",     کاغذ: "R80", جهت: 0, چیدمان: 4, تأکید: 0, قلم_پایه: 7.5 },
    { کد: "T12", نام: "طرح نواری ۵۸ میلی‌متر",     کاغذ: "R58", جهت: 0, چیدمان: 4, تأکید: 0, قلم_پایه: 7.0 },
    { کد: "T13", نام: "طرح افقی A5",             کاغذ: "A5",  جهت: 1, چیدمان: 0, تأکید: 1, قلم_پایه: 9.0 },
    { کد: "T14", نام: "طرح افقی A4",             کاغذ: "A4",  جهت: 1, چیدمان: 0, تأکید: 1, قلم_پایه: 9.5 },
    { کد: "T15", نام: "طرح بارکدمحور",           کاغذ: "A6",  جهت: 0, چیدمان: 3, تأکید: 0, قلم_پایه: 8.5 },
    { کد: "T16", نام: "طرح جدول‌محور",            کاغذ: "A4",  جهت: 0, چیدمان: 0, تأکید: 1, قلم_پایه: 9.0 },
    { کد: "T17", نام: "طرح دو ستونهٔ فشرده",      کاغذ: "A5",  جهت: 0, چیدمان: 1, تأکید: 0, قلم_پایه: 8.5 },
    { کد: "T18", نام: "طرح سربرگ‌دار بلند",        کاغذ: "A4",  جهت: 0, چیدمان: 0, تأکید: 2, قلم_پایه: 9.5 },
    { کد: "T19", نام: "طرح رسید نقدی",           کاغذ: "A5",  جهت: 0, چیدمان: 0, تأکید: 1, قلم_پایه: 9.0 },
    { کد: "T20", نام: "طرح رسید کارتخوان",        کاغذ: "A5",  جهت: 0, چیدمان: 0, تأکید: 1, قلم_پایه: 9.0 },
    { کد: "T21", نام: "طرح بیمهٔ پایه",           کاغذ: "A5",  جهت: 0, چیدمان: 0, تأکید: 1, قلم_پایه: 9.0 },
    { کد: "T22", نام: "طرح بیمهٔ تکمیلی",         کاغذ: "A5",  جهت: 0, چیدمان: 0, تأکید: 1, قلم_پایه: 9.0 },
    { کد: "T23", نام: "طرح آزمایشگاه",           کاغذ: "A5",  جهت: 0, چیدمان: 0, تأکید: 1, قلم_پایه: 9.0 },
    { کد: "T24", نام: "طرح رادیولوژی",           کاغذ: "A5",  جهت: 0, چیدمان: 0, تأکید: 1, قلم_پایه: 9.0 },
    { کد: "T25", نام: "طرح تزریقات",             کاغذ: "A5",  جهت: 0, چیدمان: 0, تأکید: 1, قلم_پایه: 9.0 },
    { کد: "T26", نام: "طرح داروخانه",            کاغذ: "A5",  جهت: 0, چیدمان: 0, تأکید: 1, قلم_پایه: 9.0 },
    { کد: "T27", نام: "طرح فیزیوتراپی",           کاغذ: "A5",  جهت: 0, چیدمان: 0, تأکید: 1, قلم_پایه: 9.0 },
    { کد: "T28", نام: "طرح نسخهٔ پزشک",           کاغذ: "A5",  جهت: 0, چیدمان: 0, تأکید: 1, قلم_پایه: 9.0 },
    { کد: "T29", نام: "طرح صورتحساب تفصیلی",       کاغذ: "A4",  جهت: 0, چیدمان: 0, تأکید: 2, قلم_پایه: 9.0 },
    { کد: "T30", نام: "طرح خلاصهٔ مدیریتی",        کاغذ: "A4",  جهت: 0, چیدمان: 0, تأکید: 1, قلم_پایه: 9.0 }
  ];

  var TPL_NAMES = ["پیش‌فرض"];
  var i, built;
  for (i = 0; i < TPL_TABLE.length; i++) TPL_NAMES.push(TPL_TABLE[i].کد + " " + TPL_TABLE[i].نام);

  var ALL = [];
  /* index 0 = the never-deletable default (same T01 shape, name «پیش‌فرض») */
  _uid = 0;
  built = بساز_طرح(TPL_TABLE[0]);
  ALL.push({ id: 0, name: TPL_NAMES[0], kind: "builtin", group: "reception",
    paper: built.paper, orientation: built.orientation, items: built.items });
  /* indices 1..30 = T01..T30 */
  for (i = 0; i < TPL_TABLE.length; i++) {
    _uid = 0;
    built = بساز_طرح(TPL_TABLE[i]);
    ALL.push({ id: 0, name: TPL_NAMES[i + 1], kind: "builtin", group: "reception",
      paper: built.paper, orientation: built.orientation, items: built.items });
  }

  window.AZ_TEMPLATES = ALL;
})();
