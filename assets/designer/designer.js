/* ===========================================================================
   designer.js — DarmanPlus professional print designer engine (v1.21.0)
   Dependency-free, RTL-aware WYSIWYG editor. Talks to C++ over the loopback
   HTTP host (/api/*) using ASYNCHRONOUS XHR.

   v1.21.0 changes:
     • Right-side tools panel, LEFT live preview (matches app spec).
     • Real DOWNLOAD: writes a .aztpl file via a browser Blob (works because the
       designer now opens in the default browser). No more silent "sent" toast.
     • Robust SAVE with explicit console diagnostics on every failure.
     • TABLE designer (rows/cols/header + per-cell text & {field} bindings).
       Stored in the item's `text` field as compact JSON so it round-trips
       through C++ unchanged and prints/previews identically (true WYSIWYG).
     • WYSIWYG text rendering: top-aligned, pt-based font sizes — identical to
       the GDI print path (DT_TOP, lf = pt*dpi/72).
     • Paper-size dropdown populated from a shared PAPER table (incl. small /
       laser sizes) so designer + printer agree.
   =========================================================================== */
(function () {
  "use strict";

  /* ------------------------------------------------------------- bridge --- */
  var Bridge = {
    _on: (location.protocol === "http:" || location.protocol === "https:"),
    has: function () { return this._on; },
    request: function (verb, args, cb) {
      cb = cb || function () {};
      if (!this._on) { cb(null); return; }
      try {
        var xhr = new XMLHttpRequest();
        xhr.open("POST", "api/" + verb, true);   // ASYNC
        xhr.setRequestHeader("Content-Type", "application/json;charset=utf-8");
        xhr.onreadystatechange = function () {
          if (xhr.readyState !== 4) return;
          if (xhr.status >= 200 && xhr.status < 300) {
            var r = null;
            try { r = xhr.responseText ? JSON.parse(xhr.responseText) : {}; }
            catch (e) { console.error("[designer] bad JSON from /api/" + verb, e, xhr.responseText); r = null; }
            cb(r);
          } else { console.error("[designer] /api/" + verb + " HTTP " + xhr.status); cb(null); }
        };
        xhr.onerror = function () { console.error("[designer] network error on /api/" + verb); cb(null); };
        xhr.send(JSON.stringify(args || {}));
      } catch (e) { console.error("[designer] request threw on /api/" + verb, e); cb(null); }
    }
  };

  /* --------------------------------------------------------- paper data --- */
  // Shared with C++ Paper_Dims(). Portrait mm. Includes small / laser sizes.
  var PAPER = {
    A3: [297, 420], A4: [210, 297], A5: [148, 210], A6: [105, 148],
    A7: [74, 105], B4: [250, 353], B5: [176, 250], B6: [125, 176],
    Letter: [215.9, 279.4], Legal: [215.9, 355.6], HalfLetter: [139.7, 215.9],
    R80: [80, 200], R80L: [80, 297], R72: [72, 200], R58: [58, 200], R58L: [58, 297],
    R44: [44, 150], L90: [90, 130], L100: [100, 150], L102: [102, 152], L130: [130, 180]
  };
  var PAPER_LABELS = {
    A3: "A3 (۲۹۷×۴۲۰)", A4: "A4 (۲۱۰×۲۹۷)", A5: "A5 (۱۴۸×۲۱۰)", A6: "A6 (۱۰۵×۱۴۸)",
    A7: "A7 (۷۴×۱۰۵)", B4: "B4 (۲۵۰×۳۵۳)", B5: "B5 (۱۷۶×۲۵۰)", B6: "B6 (۱۲۵×۱۷۶)",
    Letter: "Letter (نامه)", Legal: "Legal (حقوقی)", HalfLetter: "نیم‌نامه (۱۴×۲۱.۶)",
    R80: "رول حرارتی ۸ سانت", R80L: "رول حرارتی ۸ سانت بلند",
    R72: "رول حرارتی ۷.۲ سانت", R58: "رول حرارتی ۵.۸ سانت",
    R58L: "رول حرارتی ۵.۸ سانت بلند", R44: "رول حرارتی ۴.۴ سانت",
    L90: "لیبل/لیزری ۹×۱۳", L100: "لیبل/لیزری ۱۰×۱۵",
    L102: "لیبل ۱۰.۲×۱۵.۲", L130: "لیبل بزرگ ۱۳×۱۸"
  };
  function paperDims(p, orient) {
    var d = PAPER[p] || [148, 210], w = d[0], h = d[1];
    if (orient === 1) { var t = w; w = h; h = t; }
    return [w, h];
  }

  // v1.22.0 RESPONSIVE: proportionally reflow every item from one page size to
  // another so a design keeps its relative layout when the paper changes. Font
  // sizes and border widths scale by the geometric-mean factor; clamped so they
  // never run off the new page.
  function reflowItems(fromW, fromH, toW, toH) {
    if (!S.design || !S.design.items) return;
    if (fromW <= 0 || fromH <= 0 || toW <= 0 || toH <= 0) return;
    var sx = toW / fromW, sy = toH / fromH;
    var sf = Math.sqrt(sx * sy);            // uniform factor for fonts/strokes
    S.design.items.forEach(function (it) {
      it.x = Math.round(it.x * sx * 100) / 100;
      it.y = Math.round(it.y * sy * 100) / 100;
      // v1.24.0: square media (logo / QR / photo) must keep their aspect ratio
      // when the page is rescaled — scale both sides by the SAME (uniform)
      // factor so a logo never gets stretched into an oblong blob.
      if (it.type === "logo" || it.type === "qr") {
        it.w = Math.round(it.w * sf * 100) / 100;
        it.h = Math.round(it.h * sf * 100) / 100;
      } else {
        it.w = Math.round(it.w * sx * 100) / 100;
        it.h = Math.round(it.h * sy * 100) / 100;
      }
      // v1.24.0 FIX: the font size property is `pt` (was wrongly scaling a
      // non-existent `fontSize`, so text never resized → on a bigger/smaller
      // page the type overflowed or looked tiny). Scale pt, padding and corner
      // by the uniform factor too, so the whole layout is truly responsive.
      if (typeof it.pt === "number" && it.pt > 0)
        it.pt = Math.max(4, Math.round(it.pt * sf * 10) / 10);
      if (typeof it.padding === "number" && it.padding > 0)
        it.padding = Math.round(it.padding * sf * 100) / 100;
      if (typeof it.corner === "number" && it.corner > 0)
        it.corner = Math.round(it.corner * sf * 100) / 100;
      if (typeof it.borderWidth === "number" && it.borderWidth > 0)
        it.borderWidth = Math.max(0.1, Math.round(it.borderWidth * sf * 100) / 100);
      // v1.55.0: explicit row / header heights are VERTICAL measures — they must
      // follow the page's vertical scale, otherwise a resized table keeps its
      // old row pitch and either overflows or leaves a big gap at the bottom.
      if (typeof it.rowH === "number" && it.rowH > 0)
        it.rowH = Math.max(3, Math.round(it.rowH * sy * 100) / 100);
      if (typeof it.headerH === "number" && it.headerH > 0)
        it.headerH = Math.max(3, Math.round(it.headerH * sy * 100) / 100);
      // keep inside new page
      if (it.w > toW) it.w = toW;
      if (it.h > toH) it.h = toH;
      if (it.x < 0) it.x = 0; if (it.y < 0) it.y = 0;
      if (it.x + it.w > toW) it.x = Math.max(0, toW - it.w);
      if (it.y + it.h > toH) it.y = Math.max(0, toH - it.h);
    });
  }

  function changePaper(newPaper, newOrient) {
    if (!S.design) return;
    var oldDims = paperDims(S.design.paper, S.design.orientation);
    if (typeof newPaper === "string") S.design.paper = newPaper;
    if (typeof newOrient === "number") S.design.orientation = newOrient;
    var newDims = paperDims(S.design.paper, S.design.orientation);
    pushUndo();
    // v1.99: always scale item x,y,w,h by newW/oldW and newH/oldH so the
    // relative layout is kept. Print-side calcPscale still fits the authored
    // design onto the physical printer paper.
    reflowItems(oldDims[0], oldDims[1], newDims[0], newDims[1]);
    compactForNarrow(S.design);
    renderAll(); fitZoom();
  }

  /* ---------------------------------------------------------- app state --- */
  var S = {
    design: null, selId: 0, selIds: [],
    tool: "select",                       // v1.99: select | hand  (item drag is transient)
    pxPerMM: 3.7795, scale: 1,
    undo: [], redo: [], dirty: false,
    reflowOnResize: true,                 // v1.22.0 responsive paper resize
    pageMargin: 8,                        // v1.96.0 safe-print margin guide (mm)
    templates: window.AZ_TEMPLATES || []
  };

  var $paper, $scroll, $selBox, $stage;

  /* ------------------------------------------------------------ helpers --- */
  function faDigits(s) {
    s = String(s); var f = "۰۱۲۳۴۵۶۷۸۹";
    return s.replace(/[0-9]/g, function (d) { return f[+d]; });
  }
  function clone(o) { return JSON.parse(JSON.stringify(o)); }
  function genId() { var m = 0; (S.design.items || []).forEach(function (it) { if (it.id > m) m = it.id; }); return m + 1; }
  function findItem(id) { return (S.design.items || []).find(function (it) { return it.id === id; }); }
  function selItem() { return S.selId ? findItem(S.selId) : null; }
  function toast(msg, kind) {
    var t = document.getElementById("toast"); if (!t) return;
    t.textContent = msg;
    t.className = "toast-msg show" + (kind ? (" " + kind) : "");
    clearTimeout(toast._t);
    toast._t = setTimeout(function () { t.className = "toast-msg"; }, 2400);
  }

  /* --------------------------------------------------------- undo stack --- */
  function pushUndo() {
    S.undo.push(clone(S.design)); if (S.undo.length > 100) S.undo.shift();
    S.redo.length = 0; S.dirty = true; updateUndoButtons();
  }
  function doUndo() {
    if (!S.undo.length) return;
    S.redo.push(clone(S.design)); S.design = S.undo.pop(); S.selId = 0; S.selIds = [];
    renderAll(); updateUndoButtons(); toast("بازگشت");
  }
  function doRedo() {
    if (!S.redo.length) return;
    S.undo.push(clone(S.design)); S.design = S.redo.pop(); S.selId = 0; S.selIds = [];
    renderAll(); updateUndoButtons(); toast("جلو");
  }
  function updateUndoButtons() {
    var u = document.getElementById("btnUndo"), r = document.getElementById("btnRedo");
    if (u) u.disabled = !S.undo.length;
    if (r) r.disabled = !S.redo.length;
  }

  /* --------------------------------------------------------- rendering ---- */
  var ITEM_LABELS = {
    label: "متن ثابت", field: "فیلد داده", hline: "خط افقی", vline: "خط عمودی",
    rect: "کادر", frame: "حاشیه صفحه", logo: "لوگو", photo: "عکس بیمار",
    qr: "بارکد / QR", image: "تصویر", table: "جدول",
    services: "لیست خدمات", barcode: "بارکد خطی (قابل اسکن)"
  };
  var ITEM_ICONS = {
    label: "T", field: "{}", hline: "—", vline: "│", rect: "▭", frame: "⬚",
    logo: "★", photo: "👤", qr: "▦", image: "🖼", table: "▦",
    services: "☰", barcode: "|||"
  };

  function mm(v) { return v * S.pxPerMM * S.scale; }
  // Font size in px so that on screen it equals the printed point size.
  // 1pt = 1/72 inch; CSS px = 96/inch ⇒ 1pt = 96/72 px = 1.3333px, then * zoom.
  function ptPx(pt) { return (pt || 10) * (96 / 72) * S.scale; }

  function styleItem(el, it) {
    el.style.left = mm(it.x) + "px";
    el.style.top = mm(it.y) + "px";
    el.style.width = mm(it.w) + "px";
    el.style.height = mm(it.h) + "px";
    el.style.transform = it.rot ? "rotate(" + it.rot + "deg)" : "";
    el.style.opacity = (it.opacity == null ? 1 : it.opacity);
    el.style.zIndex = it.z || 0;
  }

  function fieldTokenLabel(field) {
    var raw = String(field || "").replace(/^\{|\}$/g, "");
    var low = raw.toLowerCase();
    if (low === "full" || low === "fullname" || low === "p-name" ||
        low === "pname" || low === "p_name") return "P-Name";
    var f = window.AZ_FIELDS[field] || window.AZ_FIELDS["{" + raw + "}"];
    return f ? f.label : (raw || "فیلد");
  }
  function displayText(it) {
    if (it.type === "label") return it.text || "";
    if (it.type === "field") {
      // v1.99: field items always show [token], never a live sample name.
      return (it.prefix || "") + "[" + fieldTokenLabel(it.field) + "]" + (it.suffix || "");
    }
    // v1.55.0: a barcode item keeps its symbology model (JSON) in it.text — never
    // show that raw JSON on the canvas; show the bound field / literal payload.
    if (it.type === "barcode") {
      if (it.field) {
        var bf = window.AZ_FIELDS[it.field];
        return bf ? bf.label : it.field;
      }
      return it.prefix || "";
    }
    return it.text || "";
  }

  /* -------------------------------------------------- table data helpers -- */
  // A table item stores its model as JSON inside it.text:
  //   {cols:n, rows:n, header:true, widths:[..], cells:[[..]]}
  function parseTable(it) {
    try {
      var t = JSON.parse(it.text || "");
      if (t && t.cells) {
        if (!t.widths || t.widths.length !== t.cols) {
          t.widths = []; for (var i = 0; i < t.cols; i++) t.widths.push(1);
        }
        return t;
      }
    } catch (e) {}
    return { cols: 3, rows: 3, header: true, widths: [1, 1, 1],
      cells: [["ستون ۱", "ستون ۲", "ستون ۳"], ["", "", ""], ["", "", ""]] };
  }
  function tableHtml(it, forThumb) {
    var t = parseTable(it);
    var sum = 0; t.widths.forEach(function (w) { sum += (w || 1); });
    var tdir = (it.dir === 1) ? "ltr" : "rtl";
    var html = "<table style='font-size:" + (forThumb ? "100%" : ptPx(it.pt || 9) + "px") +
      ";color:" + (it.textColor || "#000") + ";direction:" + tdir + "'>";
    for (var r = 0; r < t.rows; r++) {
      html += "<tr>";
      for (var c = 0; c < t.cols; c++) {
        var isHd = (t.header && r === 0);
        var wpc = ((t.widths[c] || 1) / sum * 100).toFixed(3);
        var v = (t.cells[r] && t.cells[r][c] != null) ? t.cells[r][c] : "";
        // show field labels for {tokens}
        v = String(v).replace(/\{[a-zA-Z]+\}/g, function (tok) {
          var f = window.AZ_FIELDS[tok]; return f ? ("［" + f.label + "］") : tok;
        });
        html += "<td class='" + (isHd ? "th" : "") + "' style='width:" + wpc + "%;border-color:" +
          (it.borderColor || "#333") + "'>" + escapeHtml(v) + "</td>";
      }
      html += "</tr>";
    }
    html += "</table>";
    return html;
  }
  function escapeHtml(s) {
    return String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
  }

  /* ---------------------------------------------- services list helpers --- */
  // §1.53.0 (Bug A): a `services` item is a LIVE table whose header comes from
  // the design JSON and whose body rows are filled at print time from the
  // ReceptionRecord.services vector. The model stored in it.text is the SAME
  // shape the C++ pdParseServicesModel() reads:
  //   {"cols":n,"header":bool,"widths":[..],"labels":[..]}
  function parseServices(it) {
    try {
      var m = JSON.parse(it.text || "");
      if (m && typeof m.cols === "number") {
        if (!m.widths || m.widths.length !== m.cols) {
          m.widths = []; for (var i = 0; i < m.cols; i++) m.widths.push(1 / m.cols);
        }
        if (!m.labels) m.labels = [];
        if (typeof m.header !== "boolean") m.header = true;
        return m;
      }
    } catch (e) {}
    // v1.97.0: canonical services table RTL:
    // نام خدمت | تعداد | مبلغ کل | شرح خدمت last. Row # omitted.
    return { cols: 4, header: true, widths: SVC_DEF_WIDTHS.slice(),
      labels: SVC_DEF_LABELS.slice() };
  }

  // v1.97.0 canonical 4-column services model (mirrors printer.cpp svcModelJson SVC3)
  var SVC_DEF_LABELS = ["نام خدمت", "تعداد", "مبلغ کل", "شرح خدمت"];
  var SVC_DEF_WIDTHS = [0.34, 0.10, 0.22, 0.34];

  /* --- label-driven column resolution (mirrors C++ pdSvcColOf) ------------ *
   * The CAPTION decides which live datum a column shows — never the index.
   * That way a user can reorder / rename columns in the designer and the print
   * engine and this preview stay in perfect agreement.                       */
  var PSC = { NAME: 0, DESC: 1, QTY: 2, CODE: 3, ROW: 4, PRICE: 5, LINE: 6,
              DISC: 7, INS: 8, PAT: 9, CAT: 10, NONE: 11 };
  function svcNormLabel(s) {
    return String(s == null ? "" : s)
      .replace(/[\s\u200c\u200f\u200e\t]/g, "")
      .replace(/\u064a/g, "\u06cc").replace(/\u0643/g, "\u06a9");
  }
  function svcColOf(label, idx) {
    var L = svcNormLabel(label);
    if (!L) return (idx === 0) ? PSC.NAME : (idx === 1) ? PSC.DESC : (idx === 2) ? PSC.QTY : PSC.NONE;
    function has(x) { return L.indexOf(x) >= 0; }
    if (has("ردیف") || has("شماره") || L === "ر") return PSC.ROW;
    if (L === "#" || L === "№") return PSC.ROW;        // v1.96.0 row-no column
    if (has("شرح") || has("توضیح")) return PSC.DESC;
    if (has("نوع")) return PSC.CAT;
    if (has("تعداد") || has("مقدار") || L === "تع") return PSC.QTY;
    if (has("کد")) return PSC.CODE;
    if (has("تخفیف")) return PSC.DISC;
    if (has("بیمه") || has("سهم‌بیمه") || has("سهمبیمه")) return PSC.INS;
    if (has("بیمار") || has("پرداختی") || has("سهمبیمار")) return PSC.PAT;
    if (has("جمع") || has("کل") || has("مبلغکل")) return PSC.LINE;
    if (has("مبلغ") || has("قیمت") || has("تعرفه") || has("ریال")) return PSC.PRICE;
    if (has("نام") || has("خدمت") || has("عنوان")) return PSC.NAME;
    return PSC.NONE;
  }
  // DESIGN-TIME preview uses field tokens, never example service names
  // («ویزیت»). Print time still fills live ReceptionRecord.services.
  // v1.99: exactly TWO placeholder rows; every cell is a [token], never blank.
  var SVC_PLACEHOLDER_ROWS = 1;
  function svcSample(kind, label) {
    switch (kind) {
      case PSC.NAME:  return "[نام خدمت]";
      case PSC.DESC:  return "[شرح خدمت]";
      case PSC.CAT:   return "[نوع خدمت]";
      case PSC.QTY:   return "[تعداد]";
      case PSC.CODE:  return "[کد خدمت]";
      case PSC.ROW:   return "[#]";
      case PSC.PRICE: return "[مبلغ واحد]";
      case PSC.LINE:  return "[مبلغ کل]";
      case PSC.DISC:  return "[تخفیف]";
      case PSC.INS:   return "[سهم بیمه]";
      case PSC.PAT:   return "[سهم بیمار]";
      default:
        if (label) return "[" + label + "]";
        return "[نام خدمت]";
    }
  }
  // preview: header row (from labels) + sample rows so the designer looks real.
  // v1.55.0: monochrome, RTL, label-driven, and it honours rowH / headerH (mm)
  // exactly like the GDI engine so what you design is what prints.
  function servicesHtml(it) {
    var m = parseServices(it);
    var sum = 0; m.widths.forEach(function (w) { sum += (w || 0); }); if (sum <= 0) sum = m.cols;
    var ink = it.textColor || "#000000";
    var rule = it.borderColor || "#000000";
    // header band: only when the item actually has a fill (monochrome designs
    // use a light grey band; a transparent item stays plain white).
    var band = (!it.fillTransparent && it.fillColor) ? it.fillColor : "";
    var headTxt = ink;
    if (band) {
      var h = band.replace("#", "");
      if (h.length === 6) {
        var lum = (parseInt(h.substr(0, 2), 16) * 30 + parseInt(h.substr(2, 2), 16) * 59 +
                   parseInt(h.substr(4, 2), 16) * 11) / 100;
        if (lum < 140) headTxt = "#ffffff";
      }
    }
    var kinds = [];
    for (var k = 0; k < m.cols; k++) kinds.push(svcColOf(m.labels[k], k));

    var boxH = mm(it.h || 0);
    var hHpx = (it.headerH > 0) ? mm(it.headerH) : Math.max(14, boxH * 0.16);
    var rHpx = (it.rowH > 0) ? mm(it.rowH) : Math.max(12, Math.min(22, boxH - hHpx));
    /* v2.03: match C++ preview — header + one data row, NEVER stretch the
       table to 100% of the item box (that made HTML look much taller). */
    var nRows = SVC_PLACEHOLDER_ROWS;

    var html = "<table class='svc-tbl' style='font-size:" + ptPx(it.pt || 8.5) +
      "px;color:" + ink + ";direction:rtl;table-layout:fixed;width:100%;height:auto'>";
    if (m.header) {
      html += "<tr" + (hHpx > 0 ? " style='height:" + hHpx.toFixed(2) + "px'" : "") + ">";
      for (var c = 0; c < m.cols; c++) {
        var wpc = ((m.widths[c] || 1) / sum * 100).toFixed(3);
        var lb = m.labels[c] != null ? m.labels[c] : "";
        html += "<td class='th' style='width:" + wpc + "%;" +
          (band ? "background:" + band + ";" : "") + "color:" + headTxt +
          ";border-color:" + rule + ";text-align:center'>" +
          escapeHtml(faDigits(lb)) + "</td>";
      }
      html += "</tr>";
    }
    for (var r = 0; r < nRows; r++) {
      // subtle zebra banding only when the item is not transparent (monochrome tint)
      var bg = (band && (r % 2 === 1)) ? svcTint(band, 0.78) : "";
      html += "<tr" + (rHpx > 0 ? " style='height:" + rHpx.toFixed(2) + "px'" : "") + ">";
      for (var cc = 0; cc < m.cols; cc++) {
        var wpc2 = ((m.widths[cc] || 1) / sum * 100).toFixed(3);
        var v = svcSample(kinds[cc], m.labels[cc]);
        if (!v) v = "[" + (m.labels[cc] || "نام خدمت") + "]";
        var prose = (kinds[cc] === PSC.NAME || kinds[cc] === PSC.DESC || kinds[cc] === PSC.CAT);
        html += "<td style='width:" + wpc2 + "%;" + (bg ? "background:" + bg + ";" : "") +
          "border-color:" + rule + ";text-align:" + (prose ? "right" : "center") + "'>" +
          escapeHtml(v) + "</td>";
      }
      html += "</tr>";
    }
    html += "</table>";
    return html;
  }
  // mix a colour toward white by `k` (0..1) — used for the monochrome zebra band
  function svcTint(hex, k) {
    var h = String(hex).replace("#", "");
    if (h.length !== 6) return "#ffffff";
    function ch(i) {
      var v = parseInt(h.substr(i, 2), 16);
      v = Math.round(v + (255 - v) * k);
      return ("0" + v.toString(16)).slice(-2);
    }
    return "#" + ch(0) + ch(2) + ch(4);
  }

  /* ------------------------------------------------- barcode helpers ------ *
   * v1.55.0: a REAL scannable linear barcode. The model lives in it.text:
   *   {"sym":"code128"|"code39"|"ean13","hri":bool,"quiet":mm}
   * The canvas preview draws a faithful Code128-B / Code39 / EAN-13 module
   * pattern computed from the payload — the same algorithm the C++ engine uses,
   * so nothing here is decorative or random.                                  */
  function parseBarcode(it) {
    var m = { sym: "code128", hri: true, quiet: 2 };
    try {
      var o = JSON.parse(it.text || "");
      if (o && typeof o === "object") {
        if (o.sym) m.sym = String(o.sym).toLowerCase();
        if (typeof o.hri === "boolean") m.hri = o.hri;
        if (o.quiet != null && isFinite(o.quiet)) m.quiet = Math.max(0, +o.quiet);
      }
    } catch (e) {}
    if (m.sym !== "code39" && m.sym !== "ean13") m.sym = "code128";
    return m;
  }
  // ---- Code128 (subset B) -------------------------------------------------
  var C128_PAT = [
    "11011001100","11001101100","11001100110","10010011000","10010001100","10001001100",
    "10011001000","10011000100","10001100100","11001001000","11001000100","11000100100",
    "10110011100","10011011100","10011001110","10111001100","10011101100","10011100110",
    "11001110010","11001011100","11001001110","11011100100","11001110100","11101101110",
    "11101001100","11100101100","11100100110","11101100100","11100110100","11100110010",
    "11011011000","11011000110","11000110110","10100011000","10001011000","10001000110",
    "10110001000","10001101000","10001100010","11010001000","11000101000","11000100010",
    "10110111000","10110001110","10001101110","10111011000","10111000110","10001110110",
    "11101110110","11010001110","11000101110","11011101000","11011100010","11011101110",
    "11101011000","11101000110","11100010110","11101101000","11101100010","11100011010",
    "11101111010","11001000010","11110001010","10100110000","10100001100","10010110000",
    "10010000110","10000101100","10000100110","10110010000","10110000100","10011010000",
    "10011000010","10000110100","10000110010","11000010010","11001010000","11110111010",
    "11000010100","10001111010","10100111100","10010111100","10010011110","10111100100",
    "10011110100","10011110010","11110100100","11110010100","11110010010","11011011110",
    "11011110110","11110110110","10101111000","10100011110","10001011110","10111101000",
    "10111100010","11110101000","11110100010","10111011110","10111101110","11101011110",
    "11110101110","11010000100","11010010000","11010011100","1100011101011"
  ];   // index 106 = STOP (13 modules) — identical to PD_C128[106] in printer.cpp
  function bc128Modules(data) {
    var s = "", sum = 104, n = 0, i;   // 104 = START B
    s += C128_PAT[104];
    for (i = 0; i < data.length; i++) {
      var v = data.charCodeAt(i) - 32;
      if (v < 0 || v > 94) v = 0;      // unsupported → space
      s += C128_PAT[v]; n++; sum += v * n;
    }
    s += C128_PAT[sum % 103];
    s += C128_PAT[106];                // STOP
    return s;
  }
  // ---- Code39 -------------------------------------------------------------
  var C39_SET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-. $/+%*";
  var C39_PAT = [
    "101001101101","110100101011","101100101011","110110010101","101001101011",
    "110100110101","101100110101","101001011011","110100101101","101100101101",
    "110101001011","101101001011","110110100101","101011001011","110101100101",
    "101101100101","101010011011","110101001101","101101001101","101011001101",
    "110101010011","101101010011","110110101001","101011010011","110101101001",
    "101101101001","101010110011","110101011001","101101011001","101011011001",
    "110010101011","100110101011","110011010101","100101101011","110010110101",
    "100110110101","100101011011","110010101101","100110101101","100100100101",
    "100100101001","100101001001","101001001001","100101101101"
  ];
  function bc39Modules(data) {
    var s = "", i;
    var chars = "*" + data.toUpperCase() + "*";
    for (i = 0; i < chars.length; i++) {
      var idx = C39_SET.indexOf(chars[i]);
      if (idx < 0) idx = C39_SET.indexOf(" ");
      s += C39_PAT[idx];
      if (i + 1 < chars.length) s += "0";   // inter-character gap
    }
    return s;
  }
  // ---- EAN-13 -------------------------------------------------------------
  var EAN_A = ["0001101","0011001","0010011","0111101","0100011","0110001","0101111","0111011","0110111","0001011"];
  var EAN_B = ["0100111","0110011","0011011","0100001","0011101","0111001","0000101","0010001","0001001","0010111"];
  var EAN_C = ["1110010","1100110","1101100","1000010","1011100","1001110","1010000","1000100","1001000","1110100"];
  var EAN_PAR = ["AAAAAA","AABABB","AABBAB","AABBBA","ABAABB","ABBAAB","ABBBAA","ABABAB","ABABBA","ABBABA"];
  function bcEan13Modules(digits) {
    var d = digits.replace(/\D/g, "");
    while (d.length < 12) d = "0" + d;
    d = d.substr(0, 12);
    var sum = 0, i;
    for (i = 0; i < 12; i++) sum += (+d[i]) * ((i % 2) ? 3 : 1);
    var chk = (10 - (sum % 10)) % 10;
    var full = d + String(chk);
    var par = EAN_PAR[+full[0]];
    var s = "101";
    for (i = 1; i <= 6; i++) s += (par[i - 1] === "A" ? EAN_A : EAN_B)[+full[i]];
    s += "01010";
    for (i = 7; i <= 12; i++) s += EAN_C[+full[i]];
    s += "101";
    return { modules: s, text: full };
  }
  // normalize a payload: fold Persian/Arabic digits to ASCII, drop separators
  function bcNormPayload(s) {
    var out = "", i;
    s = String(s == null ? "" : s);
    for (i = 0; i < s.length; i++) {
      var ch = s[i], cc = s.charCodeAt(i);
      if (cc >= 0x06f0 && cc <= 0x06f9) out += String.fromCharCode(48 + cc - 0x06f0);
      else if (cc >= 0x0660 && cc <= 0x0669) out += String.fromCharCode(48 + cc - 0x0660);
      else if (cc === 0x066c || cc === 0x060c) continue;     // ٬ ، separators
      else if (cc >= 32 && cc <= 126) out += ch;
    }
    return out;
  }
  function barcodeHtml(it) {
    var m = parseBarcode(it);
    // design-time payload: the bound field's demo value, else the literal prefix
    var bf = it.field ? window.AZ_FIELDS[it.field] : null;
    var pay = bcNormPayload(bf ? (bf.sample || "") : (it.prefix || ""));
    if (!pay) pay = "2500410037";        // fixed placeholder (deterministic)
    var mods = "", hri = pay;
    if (m.sym === "ean13") { var e = bcEan13Modules(pay); mods = e.modules; hri = e.text; }
    else if (m.sym === "code39") { mods = bc39Modules(pay); hri = pay.toUpperCase(); }
    else { mods = bc128Modules(pay); }

    var ink = it.textColor || "#000000";
    var quietPc = 0, total = mods.length;
    var qmm = m.quiet || 0;
    var wmm = it.w || 40;
    if (wmm > 0 && qmm > 0) quietPc = Math.min(20, (qmm / wmm) * 100);
    var barsPc = 100 - quietPc * 2;
    var barsH = m.hri ? "76%" : "100%";

    // group consecutive '1' modules into single bars (fewer DOM nodes)
    var html = "<div class='bc-wrap' style='direction:ltr'>";
    html += "<div class='bc-bars' style='height:" + barsH + ";margin:0 " + quietPc.toFixed(3) + "%'>";
    var i2 = 0;
    while (i2 < total) {
      if (mods[i2] === "1") {
        var j = i2; while (j < total && mods[j] === "1") j++;
        var left = (i2 / total * 100).toFixed(4), wdt = ((j - i2) / total * 100).toFixed(4);
        html += "<i style='left:" + left + "%;width:" + wdt + "%;background:" + ink + "'></i>";
        i2 = j;
      } else i2++;
    }
    html += "</div>";
    if (m.hri) {
      html += "<div class='bc-hri' style='color:" + ink + ";font-size:" + ptPx(it.pt || 8) +
        "px'>" + escapeHtml(hri) + "</div>";
    }
    html += "</div>";
    // remember the effective total for the width grid (harmless metadata)
    void barsPc;
    return html;
  }

  /* ------------------------------------------- live table grips (v1.55.0) -- *
   * Professional, interactive editing: thin drag handles overlaid on the
   * selected table / services item let the user resize COLUMN WIDTHS and ROW
   * HEIGHTS directly on the canvas, in real time. Everything is written back
   * into the item's JSON model, so the print engine renders exactly what the
   * designer shows. Numeric entry for the same values lives in the inspector.  */
  function svcWriteModel(it, m) {
    it.text = JSON.stringify(m);
  }
  function gripModelOf(it) {
    return (it.type === "services") ? parseServices(it) : parseTable(it);
  }
  function addTableGrips(el, it) {
    if (it.id !== S.selId) return;               // only the selected item
    if (S.selIds && S.selIds.length > 1) return; // v1.99: no grips while multi-select
    var m = gripModelOf(it);
    var cols = m.cols || 1;
    var sum = 0; (m.widths || []).forEach(function (w) { sum += (w || 0); });
    if (sum <= 0) return;
    var Wpx = mm(it.w), Hpx = mm(it.h);
    if (Wpx < 12 || Hpx < 8) return;             // too small to grab safely

    var host = document.createElement("div");
    host.className = "tbl-grips";

    // ---- column boundaries (RTL: col 0 is the RIGHT-most column) ----------
    var acc = 0;
    for (var c = 0; c < cols - 1; c++) {
      acc += (m.widths[c] || 0);
      var g = document.createElement("div");
      g.className = "col-grip";
      g.style.right = (acc / sum * Wpx).toFixed(2) + "px";
      g.dataset.col = String(c);
      g.title = "کشیدن برای تغییر عرض ستون";
      host.appendChild(g);
    }

    // ---- header height + row height ---------------------------------------
    var hasHeader = (it.type === "services") ? (m.header !== false) : (m.header !== false);
    var hHpx = (it.headerH > 0) ? mm(it.headerH) : (hasHeader ? Hpx * 0.16 : 0);
    if (hasHeader && hHpx > 3 && hHpx < Hpx - 3) {
      var gh = document.createElement("div");
      gh.className = "row-grip row-grip-head";
      gh.style.top = hHpx.toFixed(2) + "px";
      gh.dataset.kind = "headerH";
      gh.title = "کشیدن برای تغییر ارتفاع سرستون";
      host.appendChild(gh);
    }
    var nData = (it.type === "services") ? SVC_PLACEHOLDER_ROWS
                                         : Math.max(1, (m.rows || 1) - (hasHeader ? 1 : 0));
    var rHpx = (it.rowH > 0) ? mm(it.rowH) : Math.max(3, (Hpx - hHpx) / Math.max(1, nData));
    var rowY = hHpx + rHpx;
    if (rowY > 4 && rowY < Hpx - 2) {
      var gr = document.createElement("div");
      gr.className = "row-grip";
      gr.style.top = rowY.toFixed(2) + "px";
      gr.dataset.kind = "rowH";
      gr.title = "کشیدن برای تغییر ارتفاع سطر";
      host.appendChild(gr);
    }

    host.addEventListener("mousedown", function (e) {
      var g2 = e.target.closest(".col-grip, .row-grip");
      if (!g2) return;
      e.preventDefault(); e.stopPropagation();
      pushUndo();
      var isCol = g2.classList.contains("col-grip");
      var col = +g2.dataset.col;
      var kind = g2.dataset.kind;
      var startX = e.clientX, startY = e.clientY;
      var mm0 = gripModelOf(it);
      var w0 = (mm0.widths || []).slice();
      var s0 = 0; w0.forEach(function (w) { s0 += (w || 0); });
      var head0 = (it.headerH > 0) ? it.headerH : +( (it.h * 0.16).toFixed(2) );
      var row0 = (it.rowH > 0) ? it.rowH
        : +(Math.max(3, (it.h - head0) / Math.max(1, nData)).toFixed(2));

      function onMove(ev) {
        var pxmm = S.pxPerMM * S.scale;
        if (isCol) {
          // RTL: dragging LEFT widens the column on the right of the boundary
          var df = (-(ev.clientX - startX) / pxmm) / it.w * s0;
          var minW = s0 * 0.04;
          var a = w0[col] + df, b = w0[col + 1] - df;
          if (a < minW) { b -= (minW - a); a = minW; }
          if (b < minW) { a -= (minW - b); b = minW; }
          if (a < minW || b < minW) return;
          var mm2 = gripModelOf(it);
          mm2.widths = w0.slice();
          mm2.widths[col] = Math.round(a * 1000) / 1000;
          mm2.widths[col + 1] = Math.round(b * 1000) / 1000;
          svcWriteModel(it, mm2);
        } else {
          var dmm = (ev.clientY - startY) / pxmm;
          if (kind === "headerH") it.headerH = Math.max(3, Math.round((head0 + dmm) * 2) / 2);
          else it.rowH = Math.max(3, Math.round((row0 + dmm) * 2) / 2);
        }
        renderAll();
      }
      function onUp() {
        document.removeEventListener("mousemove", onMove);
        document.removeEventListener("mouseup", onUp);
        renderInspector();
      }
      document.addEventListener("mousemove", onMove);
      document.addEventListener("mouseup", onUp);
    });

    el.appendChild(host);
  }

  function itemIsSelected(id) {
    if (S.selIds && S.selIds.length) {
      var i;
      for (i = 0; i < S.selIds.length; i++) if (S.selIds[i] === id) return true;
      return false;
    }
    return id === S.selId;
  }
  function selectedItems() {
    var out = [], i, it;
    if (S.selIds && S.selIds.length) {
      for (i = 0; i < S.selIds.length; i++) {
        it = findItem(S.selIds[i]);
        if (it) out.push(it);
      }
      return out;
    }
    it = selItem();
    if (it) out.push(it);
    return out;
  }

  function buildItemEl(it) {
    var el = document.createElement("div");
    el.className = "pi pi-" + it.type + (itemIsSelected(it.id) ? " sel" : "") +
      (it.fmt === "nowrap" ? " nowrap" : "");
    el.dataset.id = it.id;
    styleItem(el, it);

    if (it.type === "label" || it.type === "field") {
      var t = document.createElement("div");
      t.className = "pi-text" + (it.type === "field" ? " pi-fieldtext" : "");
      t.textContent = displayText(it);
      t.style.color = it.textColor || "#000";
      t.style.fontSize = ptPx(it.pt) + "px";
      t.style.fontWeight = it.bold ? "700" : "400";
      t.style.fontStyle = it.italic ? "italic" : "normal";
      t.style.fontFamily = (it.font || "Vazirmatn") + ",Tahoma,sans-serif";
      t.style.lineHeight = (it.lineSpacing && it.lineSpacing > 0) ? it.lineSpacing : 1.25;
      // v1.24.0: BLOCK layout (not flex) so wrapped RTL text aligns reliably and
      // never hugs the wrong edge. Direction drives the default side; align can
      // override. dir 0=RTL 1=LTR 2=center. This mirrors the GDI print engine
      // (DT_RIGHT|DT_RTLREADING for RTL, DT_LEFT for LTR, DT_CENTER for center).
      var dir = (it.dir == null) ? 0 : it.dir;
      var al = it.align;                          // 0=right 1=center 2=left (may be undefined)
      if (dir === 2) { t.style.direction = "rtl"; t.style.textAlign = "center"; }
      else if (dir === 1) {                       // LTR
        t.style.direction = "ltr";
        t.style.textAlign = (al === 1) ? "center" : (al === 0) ? "right" : "left";
      } else {                                    // RTL (default)
        t.style.direction = "rtl";
        t.style.textAlign = (al === 1) ? "center" : (al === 2) ? "left" : "right";
      }
      // vertical placement inside the box (0=top 1=middle 2=bottom) — matches GDI valign
      t.style.display = "block";
      t.style.alignItems = "";
      t.style.justifyContent = "";
      el.appendChild(t);
    } else if (it.type === "table") {
      el.innerHTML = tableHtml(it, false);
      if (!it.locked) addTableGrips(el, it);
    } else if (it.type === "services") {
      el.innerHTML = servicesHtml(it);
      if (!it.locked) addTableGrips(el, it);
    } else if (it.type === "barcode") {
      el.innerHTML = barcodeHtml(it);
    } else if (it.type === "hline") {
      el.style.height = Math.max(1, mm(it.borderWidth || 0.4)) + "px";
      el.style.background = it.borderColor || "#222";
    } else if (it.type === "vline") {
      el.style.width = Math.max(1, mm(it.borderWidth || 0.4)) + "px";
      el.style.background = it.borderColor || "#222";
    } else if (it.type === "rect" || it.type === "frame") {
      el.style.border = Math.max(1, mm(it.borderWidth || 0.4)) + "px solid " + (it.borderColor || "#222");
      el.style.borderRadius = mm(it.corner || 0) + "px";
      if (!it.fillTransparent && it.fillColor) el.style.background = it.fillColor;
      // v1.22.0 fix: a frame / transparent box must NOT capture clicks on its
      // empty center — otherwise clicking the blank page selects the frame.
      // We make the element click-through and rely on edge hit-testing
      // (hitItemAt) so it is only selected when its border/handle is clicked.
      if (it.type === "frame" || it.fillTransparent) el.style.pointerEvents = "none";
    } else if (it.type === "logo" || it.type === "photo" || it.type === "image" || it.type === "qr") {
      el.style.borderRadius = mm(it.corner || 0) + "px";
      if (it.imgPath && /^data:/.test(it.imgPath)) {
        var img = document.createElement("img");
        img.src = it.imgPath; img.className = "pi-img";
        el.appendChild(img);
      } else {
        el.style.border = "1px dashed #9aa7c2";
        var ph = document.createElement("div");
        ph.className = "pi-ph";
        ph.textContent = ITEM_ICONS[it.type] + " " + (ITEM_LABELS[it.type] || it.type);
        el.appendChild(ph);
      }
    }
    return el;
  }

  // v1.96.0 — draw the safe-print margin guide (dashed inset) on the page so the
  // operator sees the printable area while laying out items.
  function renderMarginGuide() {
    var g = document.getElementById("marginGuide"); if (!g) return;
    var m = S.pageMargin || 0;
    if (!(m > 0)) { g.classList.add("hidden"); return; }
    g.classList.remove("hidden");
    g.style.left = mm(m) + "px";
    g.style.top = mm(m) + "px";
    g.style.width = mm(S.design.paperW - 2 * m) + "px";
    g.style.height = mm(S.design.paperH - 2 * m) + "px";
  }

  function renderAll() {
    if (!S.design) return;
    var dims = paperDims(S.design.paper, S.design.orientation);
    S.design.paperW = dims[0]; S.design.paperH = dims[1];

    $paper.style.width = mm(dims[0]) + "px";
    $paper.style.height = mm(dims[1]) + "px";
    renderMarginGuide();

    Array.prototype.slice.call($paper.querySelectorAll(".pi")).forEach(function (e) { e.remove(); });

    var items = (S.design.items || []).slice().sort(function (a, b) { return (a.z || 0) - (b.z || 0); });
    items.forEach(function (it) { $paper.appendChild(buildItemEl(it)); });

    // v1.24.0: auto-fit text so a value never overflows / gets clipped (mirrors
    // the GDI print engine's shrink-to-fit). Runs after layout so we can measure.
    autoFitTexts();

    updateSelBox();
    var pl = document.getElementById("paperLbl");
    if (pl) pl.textContent = (PAPER_LABELS[S.design.paper] || S.design.paper) + " · " +
      faDigits(Math.round(dims[0])) + "×" + faDigits(Math.round(dims[1])) + " mm";
  }

  // v1.24.0: shrink any text element whose content overflows its box (height or
  // width) so letters are never sheared/clipped — matches the print auto-fit.
  function autoFitTexts() {
    var nodes = $paper.querySelectorAll(".pi-text");
    Array.prototype.forEach.call(nodes, function (t) {
      var box = t.parentNode; if (!box) return;
      var bw = box.clientWidth, bh = box.clientHeight;
      if (bw <= 0 || bh <= 0) return;
      var px = parseFloat(t.style.fontSize) || 12;
      var floor = Math.max(4.5, px * 0.55);
      var guard = 0;
      while ((t.scrollHeight > bh + 1 || t.scrollWidth > bw + 1) && px > floor && guard < 20) {
        px = px * 0.92; if (px < floor) px = floor;
        t.style.fontSize = px + "px";
        guard++;
      }
    });
  }

  function updateSelBox() {
    var it = selItem();
    var multi = S.selIds && S.selIds.length > 1;
    if (!it || multi) { $selBox.classList.add("hidden"); return; }
    $selBox.classList.remove("hidden");
    $selBox.style.left = mm(it.x) + "px";
    $selBox.style.top = mm(it.y) + "px";
    $selBox.style.width = mm(it.w) + "px";
    $selBox.style.height = mm(it.h) + "px";
    $selBox.style.transform = it.rot ? "rotate(" + it.rot + "deg)" : "";
  }

  function select(id) {
    S.selId = id;
    S.selIds = id ? [id] : [];
    syncSelClass();
    updateSelBox();
    renderInspector(); renderLayers();
    if (id) switchTab("inspector");
  }
  function selectMany(ids) {
    S.selIds = ids ? ids.slice() : [];
    S.selId = S.selIds.length ? S.selIds[S.selIds.length - 1] : 0;
    syncSelClass();
    updateSelBox();
    renderInspector(); renderLayers();
    if (S.selId) switchTab("inspector");
  }
  function syncSelClass() {
    if (!$paper) return;
    Array.prototype.slice.call($paper.querySelectorAll(".pi")).forEach(function (e) {
      e.classList.toggle("sel", itemIsSelected(+e.dataset.id));
    });
  }
  function setTool(tool) {
    S.tool = (tool === "hand") ? "hand" : "select";
    if ($scroll) {
      if (S.tool === "hand") {
        $scroll.classList.add("tool-hand");
        $scroll.classList.remove("tool-select");
      } else {
        $scroll.classList.add("tool-select");
        $scroll.classList.remove("tool-hand");
      }
    }
    var bs = document.getElementById("btnToolSelect");
    var bh = document.getElementById("btnToolHand");
    if (bs) { if (S.tool === "select") bs.classList.add("active"); else bs.classList.remove("active"); }
    if (bh) { if (S.tool === "hand") bh.classList.add("active"); else bh.classList.remove("active"); }
  }

  /* -------------------------------------------------------- new items ----- */
  function defaultItem(type) {
    var it = {
      id: genId(), type: type, x: 10, y: 10, w: 40, h: 8, rot: 0, z: (S.design.items || []).length + 1,
      locked: false, isFrame: false, text: "", field: "", prefix: "", suffix: "",
      font: "Vazirmatn", pt: 11, bold: false, italic: false, align: 0, dir: 0, lineSpacing: 1.25,
      textColor: "#111111", fillColor: "#ffffff", fillTransparent: true,
      borderColor: "#333333", borderWidth: 0.4, corner: 0, padding: 1, opacity: 1,
      visibility: 0, imgPath: "",
      // v1.55.0: explicit row / header heights in mm (0 = auto-distribute)
      rowH: 0, headerH: 0
    };
    if (type === "label") { it.text = "متن"; it.w = 40; it.h = 8; }
    else if (type === "field") { it.w = 50; it.h = 8; }
    else if (type === "hline") { it.w = 80; it.h = 1; it.borderWidth = 0.4; }
    else if (type === "vline") { it.w = 1; it.h = 40; it.borderWidth = 0.4; }
    else if (type === "rect") { it.w = 50; it.h = 25; }
    else if (type === "frame") {
      var dm = paperDims(S.design.paper, S.design.orientation);
      it.x = 4; it.y = 4; it.w = dm[0] - 8; it.h = dm[1] - 8; it.isFrame = true; it.borderWidth = 0.6;
    }
    else if (type === "logo") { it.w = 28; it.h = 28; }
    else if (type === "photo") { it.w = 25; it.h = 32; }
    else if (type === "image") { it.w = 40; it.h = 25; }
    else if (type === "qr") { it.w = 24; it.h = 24; }
    else if (type === "barcode") {
      // v1.55.0: real scannable Code128-B with the numeric code printed below
      it.w = 52; it.h = 16; it.pt = 8; it.align = 1; it.dir = 1;
      it.textColor = "#000000"; it.fillTransparent = true; it.borderWidth = 0;
      it.field = "{receiptbarcode}";
      it.text = JSON.stringify({ sym: "code128", hri: true, quiet: 2 });
    }
    else if (type === "table") {
      it.w = 90; it.h = 30; it.pt = 9; it.borderWidth = 0.4;
      it.text = JSON.stringify({ cols: 3, rows: 4, header: true, widths: [1, 1, 1],
        cells: [["ردیف", "شرح", "مبلغ"], ["", "", ""], ["", "", ""], ["", "", ""]] });
    }
    else if (type === "services") {
      // §1.53.0 (Bug A): default size adapts to the section's paper — wide sheet
      // gets a full-width table, a narrow thermal roll gets a compact one.
      var dm2 = paperDims(S.design.paper, S.design.orientation);
      var narrow = dm2[0] < 90;
      it.w = narrow ? 60 : 130; it.h = narrow ? 30 : 40;
      it.pt = narrow ? 7.5 : 8.5; it.borderWidth = 0.4;
      // v1.97.0: monochrome line-art + canonical name/qty/line/desc columns
      it.borderColor = "#000000"; it.fillColor = "#ffffff"; it.textColor = "#000000";
      it.fillTransparent = true;
      it.align = 0; it.dir = 0;
      it.headerH = narrow ? 6 : 7.5; it.rowH = narrow ? 5.5 : 6.5;
      it.text = JSON.stringify({ cols: 4, header: true, widths: SVC_DEF_WIDTHS.slice(),
        labels: SVC_DEF_LABELS.slice() });
    }
    return it;
  }

  function addItem(type, field) {
    pushUndo();
    var it = defaultItem(type);
    if (type === "field" && field) {
      it.field = field;
      var dm = paperDims(S.design.paper, S.design.orientation);
      it.x = Math.round((dm[0] - it.w) / 2);
      it.y = 20 + ((S.design.items.length * 3) % 60);
    }
    S.design.items.push(it);
    renderAll(); select(it.id);
    toast("افزوده شد: " + (ITEM_LABELS[type] || type));
  }

  function deleteItem(id) {
    var i = S.design.items.findIndex(function (x) { return x.id === id; });
    if (i < 0) return;
    pushUndo(); S.design.items.splice(i, 1);
    if (S.selId === id) S.selId = 0;
    if (S.selIds && S.selIds.length) {
      S.selIds = S.selIds.filter(function (x) { return x !== id; });
      if (!S.selId && S.selIds.length) S.selId = S.selIds[S.selIds.length - 1];
    }
    renderAll(); renderInspector(); renderLayers();
  }
  function duplicateItem(id) {
    var it = findItem(id); if (!it) return;
    pushUndo(); var c = clone(it); c.id = genId(); c.x += 4; c.y += 4; c.z = S.design.items.length + 1;
    S.design.items.push(c); renderAll(); select(c.id);
  }

  /* v1.96.0 — clipboard copy/paste for the keyboard shortcuts (Ctrl+C / Ctrl+V). */
  var clipItem = null;
  function copyItem() {
    var it = selItem(); if (!it) return;
    clipItem = clone(it);
    toast("کپی شد: " + (ITEM_LABELS[it.type] || it.type));
  }
  function pasteItem() {
    if (!clipItem) { toast("حافظه خالی است", "err"); return; }
    pushUndo();
    var c = clone(clipItem); c.id = genId(); c.x += 6; c.y += 6; c.z = S.design.items.length + 1;
    S.design.items.push(c); renderAll(); select(c.id);
    toast("چسبانده شد");
  }

  /* ------------------------------------------------------------ palette --- */
  function buildPalette() {
    var host = document.getElementById("paletteList");
    if (!host) return; host.innerHTML = "";

    var objs = [
      ["label", "متن ثابت"], ["field", "فیلد داده"], ["services", "لیست خدمات"], ["table", "جدول"],
      ["hline", "خط افقی"], ["vline", "خط عمودی"], ["rect", "کادر"],
      ["frame", "حاشیه صفحه"], ["logo", "لوگو"], ["photo", "عکس بیمار"],
      ["image", "تصویر"], ["qr", "بارکد / QR"], ["barcode", "بارکد خطی"]
    ];
    var sec = document.createElement("div"); sec.className = "pl-cat";
    sec.innerHTML = "<div class='pl-cat-h'>عناصر طراحی</div>";
    var grid = document.createElement("div"); grid.className = "pl-grid";
    objs.forEach(function (o) {
      var b = document.createElement("button");
      b.className = "pl-tile"; b.dataset.kind = "obj"; b.dataset.type = o[0];
      b.innerHTML = "<span class='pl-ic'>" + (ITEM_ICONS[o[0]] || "•") + "</span><span class='pl-lb'>" + o[1] + "</span>";
      b.title = o[1];
      b.addEventListener("click", function () {
        if (o[0] === "table") { openTableBuilder(null); return; }
        if (o[0] === "field") { switchTab("palette"); toast("یک فیلد از فهرست زیر را انتخاب کنید"); return; }
        addItem(o[0]);
      });
      grid.appendChild(b);
    });
    sec.appendChild(grid); host.appendChild(sec);

    (window.AZ_FIELD_CATS || []).forEach(function (cat) {
      var c = document.createElement("div"); c.className = "pl-cat";
      c.innerHTML = "<div class='pl-cat-h'>" + cat.title + "</div>";
      var g = document.createElement("div"); g.className = "pl-fields";
      cat.items.forEach(function (f) {
        var b = document.createElement("button");
        b.className = "pl-field"; b.dataset.kind = "field"; b.dataset.label = f.label;
        b.textContent = f.label;
        b.title = "افزودن فیلد: " + f.label;
        b.addEventListener("click", function () {
          // §1.53.0 (Bug D): the {services} marker must insert a LIVE services
          // list item (not a plain {field}), so the dynamic table is authored.
          if (f.key === "{services}") { addItem("services"); return; }
          addItem("field", f.key);
        });
        g.appendChild(b);
      });
      c.appendChild(g); host.appendChild(c);
    });
  }

  function filterPalette(q) {
    q = (q || "").trim();
    Array.prototype.slice.call(document.querySelectorAll("#paletteList .pl-field,#paletteList .pl-tile")).forEach(function (b) {
      var txt = (b.dataset.label || (b.querySelector(".pl-lb") && b.querySelector(".pl-lb").textContent) || "");
      b.style.display = (!q || txt.indexOf(q) >= 0) ? "" : "none";
    });
  }

  /* ---------------------------------------------------------- inspector --- */
  function row(label, ctrl) {
    var r = document.createElement("div"); r.className = "insp-row";
    var l = document.createElement("label"); l.textContent = label;
    r.appendChild(l); r.appendChild(ctrl); return r;
  }
  function numInput(val, min, max, step, onCh) {
    var i = document.createElement("input"); i.type = "number";
    i.className = "form-control form-control-sm"; i.value = (val == null ? 0 : val);
    if (min != null) i.min = min; if (max != null) i.max = max; if (step != null) i.step = step;
    i.addEventListener("change", function () { onCh(parseFloat(i.value) || 0); });
    return i;
  }
  function textInput(val, onCh, ph) {
    var i = document.createElement("input"); i.type = "text";
    i.className = "form-control form-control-sm"; i.value = val || ""; if (ph) i.placeholder = ph;
    i.addEventListener("input", function () { onCh(i.value); });
    return i;
  }
  function colorInput(val, onCh) {
    var i = document.createElement("input"); i.type = "color";
    i.className = "form-color"; i.value = val || "#000000";
    i.addEventListener("input", function () { onCh(i.value); });
    return i;
  }
  function checkInput(val, onCh) {
    var i = document.createElement("input"); i.type = "checkbox"; i.className = "form-check";
    i.checked = !!val; i.addEventListener("change", function () { onCh(i.checked); });
    return i;
  }
  function selectInput(opts, val, onCh) {
    var s = document.createElement("select"); s.className = "form-select form-select-sm";
    opts.forEach(function (o) {
      var op = document.createElement("option"); op.value = o[0]; op.textContent = o[1];
      if (String(o[0]) === String(val)) op.selected = true; s.appendChild(op);
    });
    s.addEventListener("change", function () { onCh(s.value); });
    return s;
  }

  function renderInspector() {
    var empty = document.getElementById("inspectorEmpty");
    var body = document.getElementById("inspectorBody");
    var it = selItem();
    if (!it) { empty.classList.remove("hidden"); body.classList.add("hidden"); body.innerHTML = ""; return; }
    empty.classList.add("hidden"); body.classList.remove("hidden"); body.innerHTML = "";

    function up(noUndo) { if (!noUndo) pushUndo(); renderAll(); updateSelBox(); }
    function grp(title) { var g = document.createElement("div"); g.className = "insp-grp"; g.innerHTML = "<div class='insp-grp-h'>" + title + "</div>"; body.appendChild(g); return g; }

    var hd = document.createElement("div"); hd.className = "insp-head";
    hd.innerHTML = "<span class='insp-type'>" + (ITEM_LABELS[it.type] || it.type) + "</span>";
    var del = document.createElement("button"); del.className = "btn btn-sm btn-danger"; del.textContent = "حذف";
    del.addEventListener("click", function () { deleteItem(it.id); });
    var dup = document.createElement("button"); dup.className = "btn btn-sm btn-outline"; dup.textContent = "تکثیر";
    dup.addEventListener("click", function () { duplicateItem(it.id); });
    hd.appendChild(dup); hd.appendChild(del);
    body.appendChild(hd);

    if (it.type === "label") {
      var gc = grp("متن");
      gc.appendChild(row("متن", textInput(it.text, function (v) { it.text = v; up(true); }, "متن دلخواه")));
    }
    if (it.type === "field") {
      var gf = grp("فیلد داده");
      var opts = [];
      (window.AZ_FIELD_CATS || []).forEach(function (c) { c.items.forEach(function (f) { opts.push([f.key, c.title + " › " + f.label]); }); });
      gf.appendChild(row("نوع داده", selectInput(opts, it.field, function (v) { it.field = v; up(); })));
      gf.appendChild(row("پیشوند", textInput(it.prefix, function (v) { it.prefix = v; up(true); }, "مثلاً: نام بیمار: ")));
      gf.appendChild(row("پسوند", textInput(it.suffix, function (v) { it.suffix = v; up(true); }, "")));
      gf.appendChild(row("فقط وقتی پر است", checkInput(it.visibility === 1, function (v) { it.visibility = v ? 1 : 0; up(); })));
    }
    if (it.type === "table") {
      var gtb = grp("جدول");
      var edit = document.createElement("button"); edit.className = "btn btn-sm btn-primary"; edit.style.width = "100%";
      edit.textContent = "ویرایش محتوای جدول…";
      edit.addEventListener("click", function () { openTableBuilder(it); });
      gtb.appendChild(edit);
      // v1.55.0: live row / header heights for static tables too
      gtb.appendChild(row("ارتفاع سرستون (mm)", numInput(it.headerH || 0, 0, 40, 0.5, function (v) {
        it.headerH = Math.max(0, v); up(); })));
      gtb.appendChild(row("ارتفاع سطر (mm)", numInput(it.rowH || 0, 0, 40, 0.5, function (v) {
        it.rowH = Math.max(0, v); up(); })));
      var tnote = document.createElement("div");
      tnote.style.cssText = "font-size:11px;color:#7a879c;margin:4px 0 0;line-height:1.5";
      tnote.textContent = "۰ = توزیع خودکار در ارتفاع کادر. لبهٔ ستون/سطر را روی صفحه بکشید.";
      gtb.appendChild(tnote);
    }
    if (it.type === "barcode") {
      // v1.55.0: a REAL scannable linear barcode — never decorative, never random
      var gbc = grp("بارکد خطی (قابل اسکن)");
      var bm = parseBarcode(it);
      function commitBc() { it.text = JSON.stringify(bm); up(true); renderInspector(); }
      var bopts = [];
      (window.AZ_FIELD_CATS || []).forEach(function (c) {
        c.items.forEach(function (f) { bopts.push([f.key, c.title + " › " + f.label]); });
      });
      gbc.appendChild(row("دادهٔ بارکد", selectInput(bopts, it.field, function (v) { it.field = v; up(); })));
      gbc.appendChild(row("نوع بارکد", selectInput([
        ["code128", "Code 128 (پیشنهادی — عدد و حروف)"],
        ["code39", "Code 39 (عدد و حروف بزرگ)"],
        ["ean13", "EAN-13 (۱۳ رقمی)"]
      ], bm.sym, function (v) { bm.sym = v; commitBc(); })));
      gbc.appendChild(row("نمایش عدد زیر بارکد", checkInput(bm.hri, function (v) { bm.hri = !!v; commitBc(); })));
      gbc.appendChild(row("حاشیهٔ سفید (mm)", numInput(bm.quiet, 0, 10, 0.5, function (v) { bm.quiet = Math.max(0, v); commitBc(); })));
      gbc.appendChild(row("متن جایگزین (اختیاری)", textInput(it.prefix, function (v) { it.prefix = v; up(true); }, "وقتی فیلد خالی باشد")));
      var bnote = document.createElement("div");
      bnote.style.cssText = "font-size:11px;color:#7a879c;margin:4px 0 0;line-height:1.5";
      bnote.textContent = "بارکد از دادهٔ واقعی پرونده ساخته می‌شود و با اسکنر خوانده می‌شود. برای برگهٔ بیمه از Code 128 استفاده کنید.";
      gbc.appendChild(bnote);
    }
    if (it.type === "services") {
      // §1.53.0 (Bug A): live-services inspector — columns, header, widths, labels.
      var gsv = grp("لیست خدمات (پویا)");
      var m = parseServices(it);
      function commitSvc() { it.text = JSON.stringify(m); up(true); renderInspector(); }
      gsv.appendChild(row("تعداد ستون", numInput(m.cols, 2, 6, 1, function (v) {
        v = Math.max(2, Math.min(6, v | 0));
        // resize widths + labels to match the new column count
        var nw = [], nl = [];
        for (var i = 0; i < v; i++) { nw.push(m.widths[i] != null ? m.widths[i] : (1 / v)); nl.push(m.labels[i] != null ? m.labels[i] : ""); }
        // renormalise widths to sum≈1
        var s = 0; nw.forEach(function (x) { s += x; }); if (s > 0) nw = nw.map(function (x) { return Math.round(x / s * 1000) / 1000; });
        m.cols = v; m.widths = nw; m.labels = nl; commitSvc();
      })));
      gsv.appendChild(row("سطر عنوان", checkInput(m.header, function (v) { m.header = !!v; commitSvc(); })));
      // v1.55.0: live, numeric row / header heights (mm). 0 = توزیع خودکار.
      gsv.appendChild(row("ارتفاع سرستون (mm)", numInput(it.headerH || 0, 0, 40, 0.5, function (v) {
        it.headerH = Math.max(0, v); up(); })));
      gsv.appendChild(row("ارتفاع سطر (mm)", numInput(it.rowH || 0, 0, 40, 0.5, function (v) {
        it.rowH = Math.max(0, v); up(); })));
      // one-click return to the real receipt's canonical monochrome layout
      var rst = document.createElement("button");
      rst.className = "btn btn-sm btn-outline"; rst.style.cssText = "width:100%;margin:6px 0 2px";
      rst.textContent = "بازگردانی به ستون‌های استاندارد رسید";
      rst.title = "نام خدمت | تعداد | مبلغ کل | شرح خدمت";
      rst.addEventListener("click", function () {
        pushUndo();
        m.cols = 5; m.header = true;
        m.widths = SVC_DEF_WIDTHS.slice(); m.labels = SVC_DEF_LABELS.slice();
        it.align = 0; it.dir = 0;
        it.text = JSON.stringify(m);
        up(true); renderInspector();
        toast("ستون‌ها بازگردانی شد", "ok");
      });
      gsv.appendChild(rst);
      var note = document.createElement("div"); note.style.cssText = "font-size:11px;color:#7a879c;margin:4px 0 6px;line-height:1.5";
      note.textContent = "عنوان و عرض هر ستون را تنظیم کنید — عنوانِ ستون تعیین می‌کند چه داده‌ای در آن چاپ شود. سطرهای داده هنگام چاپ به‌صورت خودکار از خدمات واقعی پذیرش پر می‌شوند. برای تغییر سریع، لبهٔ ستون‌ها و سطرها را روی صفحه بکشید.";
      gsv.appendChild(note);
      for (var ci = 0; ci < m.cols; ci++) {
        (function (idx) {
          var lbl = document.createElement("div"); lbl.style.cssText = "font-size:11px;color:#45506a;margin-top:5px";
          lbl.textContent = "ستون " + faDigits(idx + 1);
          gsv.appendChild(lbl);
          gsv.appendChild(row("عنوان", textInput(m.labels[idx] || "", function (v) { m.labels[idx] = v; commitSvc(); }, "نام ستون")));
          gsv.appendChild(row("عرض (٪)", numInput(Math.round((m.widths[idx] || 0) * 100), 3, 90, 1, function (v) {
            m.widths[idx] = Math.max(0.03, v / 100); commitSvc();
          })));
        })(ci);
      }
    }
    if (it.type === "logo" || it.type === "photo" || it.type === "image") {
      var gi = grp("تصویر");
      var up2 = document.createElement("button"); up2.className = "btn btn-sm btn-primary"; up2.style.width = "100%";
      up2.textContent = it.imgPath ? "تغییر تصویر…" : "بارگذاری تصویر…";
      up2.addEventListener("click", function () { pickImageFor(it); });
      gi.appendChild(up2);
      if (it.imgPath) {
        var clr = document.createElement("button"); clr.className = "btn btn-sm btn-outline"; clr.style.width = "100%"; clr.style.marginTop = "6px";
        clr.textContent = "حذف تصویر";
        clr.addEventListener("click", function () { pushUndo(); it.imgPath = ""; up(true); renderInspector(); });
        gi.appendChild(clr);
      }
    }

    if (it.type === "label" || it.type === "field" || it.type === "table" ||
        it.type === "services" || it.type === "barcode") {
      var gt = grp("قلم و متن");
      if (it.type !== "table" && it.type !== "services" && it.type !== "barcode")
        gt.appendChild(row("فونت", selectInput([["Vazirmatn", "وزیر"], ["Tahoma", "تاهوما"], ["IRANSans", "ایران‌سنس"]], it.font, function (v) { it.font = v; up(); })));
      gt.appendChild(row("اندازه (pt)", numInput(it.pt, 5, 96, 0.5, function (v) { it.pt = v; up(); })));
      if (it.type === "services") {
        gt.appendChild(row("بدون سایهٔ سرستون", checkInput(it.fillTransparent, function (v) { it.fillTransparent = v; up(); })));
        gt.appendChild(row("سایهٔ سرستون", colorInput(it.fillColor, function (v) { it.fillColor = v; it.fillTransparent = false; up(); renderInspector(); })));
      }
      if (it.type === "label" || it.type === "field" || it.type === "table" || it.type === "services") {
        gt.appendChild(row("شکستن متن طولانی", checkInput(it.fmt !== "nowrap", function (v) {
          it.fmt = v ? "wrap" : "nowrap"; up();
        })));
      }
      if (it.type !== "table" && it.type !== "services" && it.type !== "barcode") {
        gt.appendChild(row("ضخیم", checkInput(it.bold, function (v) { it.bold = v; up(); })));
        gt.appendChild(row("کج", checkInput(it.italic, function (v) { it.italic = v; up(); })));
        gt.appendChild(row("چینش", selectInput([["0", "راست"], ["1", "وسط"], ["2", "چپ"]], it.align, function (v) { it.align = +v; up(); })));
        gt.appendChild(row("جهت متن", selectInput([["0", "راست‌به‌چپ (RTL)"], ["1", "چپ‌به‌راست (LTR)"], ["2", "وسط‌چین (Center)"]], it.dir || 0, function (v) {
          it.dir = +v;
          // v1.24.0: choosing a direction also moves the text to the natural
          // side so the toggle has a *visible* effect (was: dir changed reading
          // order only, text stayed where it was → users thought RTL "did
          // nothing"). RTL → right-align, LTR → left-align, Center → center.
          if (it.dir === 0) it.align = 0;        // RTL  → راست
          else if (it.dir === 1) it.align = 2;   // LTR  → چپ
          else if (it.dir === 2) it.align = 1;   // وسط
          up(); renderInspector();
        })));
      }
      gt.appendChild(row(it.type === "barcode" ? "رنگ بارکد" : "رنگ متن",
        colorInput(it.textColor, function (v) { it.textColor = v; up(); })));
      if (it.type !== "table" && it.type !== "services" && it.type !== "barcode")
        gt.appendChild(row("فاصله خطوط", numInput(it.lineSpacing, 1, 3, 0.05, function (v) { it.lineSpacing = v; up(); })));
    }

    if (it.type === "rect" || it.type === "frame" || it.type === "hline" || it.type === "vline" ||
        it.type === "qr" || it.type === "logo" || it.type === "photo" || it.type === "image" || it.type === "table" || it.type === "services") {
      var gb = grp("کادر و خط");
      gb.appendChild(row("رنگ خط", colorInput(it.borderColor, function (v) { it.borderColor = v; up(); })));
      gb.appendChild(row("ضخامت (mm)", numInput(it.borderWidth, 0, 5, 0.1, function (v) { it.borderWidth = v; up(); })));
      if (it.type === "rect" || it.type === "frame") {
        gb.appendChild(row("گردی گوشه", numInput(it.corner, 0, 30, 0.5, function (v) { it.corner = v; up(); })));
        gb.appendChild(row("بدون پُرکننده", checkInput(it.fillTransparent, function (v) { it.fillTransparent = v; up(); })));
        gb.appendChild(row("رنگ پُرکننده", colorInput(it.fillColor, function (v) { it.fillColor = v; up(); })));
      }
    }

    var gg = grp("اندازه و موقعیت (mm)");
    gg.appendChild(row("X", numInput(it.x, 0, 1000, 0.5, function (v) { it.x = v; up(); })));
    gg.appendChild(row("Y", numInput(it.y, 0, 1000, 0.5, function (v) { it.y = v; up(); })));
    gg.appendChild(row("عرض", numInput(it.w, 1, 1000, 0.5, function (v) { it.w = v; up(); })));
    gg.appendChild(row("ارتفاع", numInput(it.h, 1, 1000, 0.5, function (v) { it.h = v; up(); })));
    gg.appendChild(row("چرخش°", numInput(it.rot, -180, 180, 1, function (v) { it.rot = v; up(); })));
    gg.appendChild(row("شفافیت", numInput(it.opacity, 0, 1, 0.05, function (v) { it.opacity = v; up(); })));
  }

  /* ------------------------------------------------------------- layers --- */
  function renderLayers() {
    var host = document.getElementById("layerList"); if (!host) return; host.innerHTML = "";
    var items = (S.design.items || []).slice().sort(function (a, b) { return (b.z || 0) - (a.z || 0); });
    items.forEach(function (it) {
      var r = document.createElement("div");
      r.className = "lyr" + (itemIsSelected(it.id) ? " sel" : "");
      var nm = it.type === "label" ? (it.text || "متن") :
        it.type === "field" ? (window.AZ_FIELDS[it.field] ? window.AZ_FIELDS[it.field].label : "فیلد") :
          (ITEM_LABELS[it.type] || it.type);
      r.innerHTML = "<span class='lyr-ic'>" + (ITEM_ICONS[it.type] || "•") + "</span><span class='lyr-nm'>" + escapeHtml(nm) + "</span>";
      var up = document.createElement("button"); up.className = "lyr-b"; up.textContent = "▲"; up.title = "بالا";
      up.addEventListener("click", function (e) { e.stopPropagation(); pushUndo(); it.z = (it.z || 0) + 1; renderAll(); renderLayers(); });
      var dn = document.createElement("button"); dn.className = "lyr-b"; dn.textContent = "▼"; dn.title = "پایین";
      dn.addEventListener("click", function (e) { e.stopPropagation(); pushUndo(); it.z = Math.max(0, (it.z || 0) - 1); renderAll(); renderLayers(); });
      var del = document.createElement("button"); del.className = "lyr-b del"; del.textContent = "✕";
      del.addEventListener("click", function (e) { e.stopPropagation(); deleteItem(it.id); });
      r.appendChild(up); r.appendChild(dn); r.appendChild(del);
      r.addEventListener("click", function () { select(it.id); });
      host.appendChild(r);
    });
  }

  /* ----------------------------------------------------------- tabs ------- */
  function switchTab(name) {
    Array.prototype.slice.call(document.querySelectorAll(".rp-tab")).forEach(function (t) {
      t.classList.toggle("active", t.dataset.tab === name);
    });
    Array.prototype.slice.call(document.querySelectorAll(".rp-pane")).forEach(function (p) {
      p.classList.toggle("active", p.id === "pane-" + name);
    });
  }

  /* ------------------------------------------------ zoom / pan / fit ------ */
  function setScale(s) {
    S.scale = Math.max(0.2, Math.min(4, s));
    var z = document.getElementById("zoomLbl"); if (z) z.textContent = faDigits(Math.round(S.scale * 100)) + "٪";
    renderAll();
  }
  function fitZoom() {
    var dm = paperDims(S.design.paper, S.design.orientation);
    var availW = $scroll.clientWidth - 80, availH = $scroll.clientHeight - 90;
    var sw = availW / (dm[0] * S.pxPerMM), sh = availH / (dm[1] * S.pxPerMM);
    setScale(Math.max(0.2, Math.min(sw, sh)));
    centerPaper();
  }
  function centerPaper() {
    $scroll.scrollLeft = ($stage.clientWidth - $scroll.clientWidth) / 2;
    $scroll.scrollTop = 40;
  }

  /* --------------------------------------------- canvas interactions ------ */
  function clientToMM(clientX, clientY) {
    var r = $paper.getBoundingClientRect();
    return { x: (clientX - r.left) / (S.pxPerMM * S.scale), y: (clientY - r.top) / (S.pxPerMM * S.scale) };
  }

  // v1.22.0: topmost item hit at a paper coordinate (mm). For frames and
  // transparent rectangles only the BORDER band counts as a hit, so their
  // empty interior is click-through (lets you click items/page behind them).
  function hitItemAt(mmx, mmy) {
    var items = (S.design.items || []).slice().sort(function (a, b) { return (b.z || 0) - (a.z || 0); });
    for (var i = 0; i < items.length; i++) {
      var it = items[i];
      if (it.locked) continue;
      if (mmx < it.x || mmx > it.x + it.w || mmy < it.y || mmy > it.y + it.h) continue;
      var hollow = (it.type === "frame") || ((it.type === "rect") && it.fillTransparent);
      if (hollow) {
        // border band thickness in mm (min 1.5mm so it's easy to grab)
        var band = Math.max(1.5, (it.borderWidth || 0.4) + 1.2);
        var inside = (mmx > it.x + band && mmx < it.x + it.w - band &&
                      mmy > it.y + band && mmy < it.y + it.h - band);
        if (inside) continue;   // clicked the hollow center → pass through
      }
      return it;
    }
    return null;
  }

  function wireCanvas() {
    var panning = false, panSX = 0, panSY = 0, scL = 0, scT = 0, moved = false;
    var drag = null;
    var rubber = false, rubberA = null, rubberB = null, rubberMoved = false;
    var $rb = document.getElementById("rubberBand");

    function hideRubber() {
      rubber = false; rubberA = null; rubberB = null; rubberMoved = false;
      if ($rb) $rb.classList.add("hidden");
    }
    function showRubberMM(a, b) {
      if (!$rb || !a || !b) return;
      var x = Math.min(a.x, b.x), y = Math.min(a.y, b.y);
      var w = Math.abs(b.x - a.x), h = Math.abs(b.y - a.y);
      $rb.classList.remove("hidden");
      $rb.style.left = mm(x) + "px";
      $rb.style.top = mm(y) + "px";
      $rb.style.width = mm(w) + "px";
      $rb.style.height = mm(h) + "px";
    }
    function idsInRect(a, b) {
      var L = Math.min(a.x, b.x), R = Math.max(a.x, b.x);
      var T = Math.min(a.y, b.y), B = Math.max(a.y, b.y);
      var out = [], i, it, items = (S.design && S.design.items) || [];
      for (i = 0; i < items.length; i++) {
        it = items[i];
        if (it.locked) continue;
        if (it.isFrame || it.type === "frame") continue;
        if (it.x + it.w < L || it.x > R || it.y + it.h < T || it.y > B) continue;
        out.push(it.id);
      }
      return out;
    }
    function beginPan(e) {
      panning = true; moved = false;
      panSX = e.clientX; panSY = e.clientY;
      scL = $scroll.scrollLeft; scT = $scroll.scrollTop;
      $scroll.classList.add("panning");
      e.preventDefault();
    }
    function beginRubber(p) {
      rubber = true; rubberMoved = false; rubberA = p; rubberB = p;
      showRubberMM(p, p);
    }

    $scroll.addEventListener("mousedown", function (e) {
      if (e.button) return;
      var pi = e.target.closest && e.target.closest(".pi");
      var handle = e.target.closest && e.target.closest(".handle");
      if (handle) return;
      if (S.tool === "hand") { beginPan(e); return; }
      if (pi) return;
      // v1.22.0: frames/transparent-rects have pointer-events:none in their
      // hollow center, so e.target won't be a .pi even when the click lands on
      // their border band. Hit-test by coordinate so the border is selectable.
      var p = clientToMM(e.clientX, e.clientY);
      var hit = hitItemAt(p.x, p.y);
      if (hit) {
        select(hit.id);
        if (!hit.locked) drag = { mode: "move", it: hit, start: p, o: clone(hit) };
        pushUndo(); e.preventDefault(); return;
      }
      // v1.99 select tool: empty canvas starts a rubber-band rect
      beginRubber(p);
      e.preventDefault();
    });
    window.addEventListener("mousemove", function (e) {
      if (panning) {
        var dx = e.clientX - panSX, dy = e.clientY - panSY;
        if (Math.abs(dx) + Math.abs(dy) > 3) moved = true;
        $scroll.scrollLeft = scL - dx; $scroll.scrollTop = scT - dy;
        return;
      }
      if (rubber && rubberA) {
        rubberB = clientToMM(e.clientX, e.clientY);
        if (Math.abs(rubberB.x - rubberA.x) + Math.abs(rubberB.y - rubberA.y) > 0.4) rubberMoved = true;
        showRubberMM(rubberA, rubberB);
      }
    });
    window.addEventListener("mouseup", function () {
      if (panning) {
        panning = false; $scroll.classList.remove("panning");
        if (!moved && S.tool === "select") select(0);
      }
      if (rubber) {
        if (rubberMoved && rubberA && rubberB) {
          var ids = idsInRect(rubberA, rubberB);
          if (ids.length) selectMany(ids); else select(0);
        } else {
          select(0);
        }
        hideRubber();
      }
    });

    $scroll.addEventListener("wheel", function (e) {
      if (e.ctrlKey) { e.preventDefault(); setScale(S.scale * (e.deltaY < 0 ? 1.1 : 0.9)); }
    }, { passive: false });

    $paper.addEventListener("mousedown", function (e) {
      if (e.button) return;
      if (S.tool === "hand") { beginPan(e); e.stopPropagation(); return; }
      var handle = e.target.closest(".handle");
      var piEl = e.target.closest(".pi");
      if (handle) {
        var it = selItem(); if (!it) return;
        drag = { mode: "resize", dir: handle.className.replace("handle ", "").trim(), it: it, start: clientToMM(e.clientX, e.clientY), o: clone(it) };
        if (handle.classList.contains("h-rot")) drag.mode = "rotate";
        pushUndo(); e.stopPropagation(); e.preventDefault(); return;
      }
      if (piEl) {
        var id = +piEl.dataset.id;
        var it2 = findItem(id); if (!it2 || it2.locked) { select(id); return; }
        var already = itemIsSelected(id);
        if (!already) select(id);
        drag = { mode: "move", it: it2, start: clientToMM(e.clientX, e.clientY), o: clone(it2) };
        if (already && S.selIds && S.selIds.length > 1) {
          drag.group = [];
          var gi, git;
          for (gi = 0; gi < S.selIds.length; gi++) {
            git = findItem(S.selIds[gi]);
            if (git) drag.group.push({ it: git, o: clone(git) });
          }
        }
        pushUndo(); e.preventDefault(); return;
      }
      // v1.22.0: no .pi under cursor — could be a hollow frame/rect border.
      var p = clientToMM(e.clientX, e.clientY);
      var hit = hitItemAt(p.x, p.y);
      if (hit) {
        select(hit.id);
        if (!hit.locked) drag = { mode: "move", it: hit, start: p, o: clone(hit) };
        pushUndo(); e.preventDefault();
      }
    });
    window.addEventListener("mousemove", function (e) {
      if (!drag) return;
      var p = clientToMM(e.clientX, e.clientY);
      var dx = p.x - drag.start.x, dy = p.y - drag.start.y;
      var it = drag.it, o = drag.o;
      if (drag.mode === "move") {
        if (drag.group && drag.group.length) {
          var g;
          for (g = 0; g < drag.group.length; g++) {
            drag.group[g].it.x = Math.max(0, Math.round((drag.group[g].o.x + dx) * 2) / 2);
            drag.group[g].it.y = Math.max(0, Math.round((drag.group[g].o.y + dy) * 2) / 2);
          }
        } else {
          it.x = Math.max(0, Math.round((o.x + dx) * 2) / 2);
          it.y = Math.max(0, Math.round((o.y + dy) * 2) / 2);
        }
      } else if (drag.mode === "rotate") {
        var cx = o.x + o.w / 2, cy = o.y + o.h / 2;
        it.rot = Math.round(Math.atan2(p.y - cy, p.x - cx) * 180 / Math.PI + 90);
      } else {
        var d = drag.dir;
        if (d.indexOf("e") >= 0) it.w = Math.max(2, o.w + dx);
        if (d.indexOf("s") >= 0) it.h = Math.max(2, o.h + dy);
        if (d.indexOf("w") >= 0) { it.w = Math.max(2, o.w - dx); it.x = o.x + dx; }
        if (d.indexOf("n") >= 0) { it.h = Math.max(2, o.h - dy); it.y = o.y + dy; }
      }
      renderAll();
    });
    window.addEventListener("mouseup", function () { if (drag) { drag = null; renderInspector(); } });

    window.addEventListener("keydown", function (e) {
      if (/INPUT|TEXTAREA|SELECT/.test(document.activeElement.tagName)) return;
      var ctrl = e.ctrlKey || e.metaKey;
      // undo / redo (Ctrl+Z and Ctrl+Shift+Z or Ctrl+Y), save (Ctrl+S),
      // copy / paste (Ctrl+C / Ctrl+V / Ctrl+P). Redo must be checked BEFORE undo.
      if (ctrl && e.shiftKey && e.key.toLowerCase() === "z") { e.preventDefault(); doRedo(); return; }
      if (ctrl && e.key.toLowerCase() === "z") { e.preventDefault(); doUndo(); return; }
      if (ctrl && e.key.toLowerCase() === "y") { e.preventDefault(); doRedo(); return; }
      if (ctrl && e.key.toLowerCase() === "s") { e.preventDefault(); saveDesign(); return; }
      if (ctrl && e.key.toLowerCase() === "c") { e.preventDefault(); copyItem(); return; }
      if (ctrl && e.key.toLowerCase() === "v") { e.preventDefault(); pasteItem(); return; }
      if (ctrl && e.key.toLowerCase() === "p") { e.preventDefault(); pasteItem(); return; }
      var list = selectedItems();
      if (!list.length) return;
      var step = e.shiftKey ? 5 : 1;
      var i;
      if (e.key === "Delete") {
        e.preventDefault();
        var ids = [];
        for (i = 0; i < list.length; i++) ids.push(list[i].id);
        pushUndo();
        for (i = 0; i < ids.length; i++) {
          var ix = S.design.items.findIndex(function (x) { return x.id === ids[i]; });
          if (ix >= 0) S.design.items.splice(ix, 1);
        }
        S.selId = 0; S.selIds = [];
        renderAll(); renderInspector(); renderLayers();
      } else if (e.key === "ArrowLeft") {
        e.preventDefault(); pushUndo();
        for (i = 0; i < list.length; i++) list[i].x -= step;
        renderAll();
      } else if (e.key === "ArrowRight") {
        e.preventDefault(); pushUndo();
        for (i = 0; i < list.length; i++) list[i].x += step;
        renderAll();
      } else if (e.key === "ArrowUp") {
        e.preventDefault(); pushUndo();
        for (i = 0; i < list.length; i++) list[i].y -= step;
        renderAll();
      } else if (e.key === "ArrowDown") {
        e.preventDefault(); pushUndo();
        for (i = 0; i < list.length; i++) list[i].y += step;
        renderAll();
      }
    });
  }

  /* ----------------------------------------------------- image upload ----- */
  function pickImageFor(it) {
    var inp = document.createElement("input");
    inp.type = "file"; inp.accept = "image/*";
    inp.addEventListener("change", function () {
      var f = inp.files[0]; if (!f) return;
      var rd = new FileReader();
      rd.onload = function () {
        pushUndo(); it.imgPath = rd.result; renderAll(); renderInspector();
        toast("تصویر بارگذاری شد", "ok");
      };
      rd.readAsDataURL(f);
    });
    inp.click();
  }

  /* -------------------------------------------------- table builder ------- */
  var tblTarget = null;        // existing item being edited, or null = new
  function openTableBuilder(it) {
    tblTarget = it;
    tblSel = { r: -1, c: -1 };
    var model = it ? parseTable(it) : { cols: 3, rows: 4, header: true, widths: [1, 1, 1],
      cells: [["ردیف", "شرح", "مبلغ"], ["", "", ""], ["", "", ""], ["", "", ""]] };
    document.getElementById("tblCols").value = model.cols;
    document.getElementById("tblRows").value = model.rows;
    document.getElementById("tblHeader").checked = !!model.header;
    // v1.55.0: live row / header heights (mm). 0 = distribute over the box height.
    var hh = document.getElementById("tblHeaderH"), rh = document.getElementById("tblRowH");
    if (hh) hh.value = (it && it.headerH > 0) ? it.headerH : 0;
    if (rh) rh.value = (it && it.rowH > 0) ? it.rowH : 0;
    // v1.55.0: keep the column widths the user set with the on-canvas grips
    tblWidths = (model.widths || []).slice();
    renderTableEditor(model);
    document.getElementById("tblOverlay").classList.remove("hidden");
  }
  // v1.55.0: column widths survive a rebuild / add / delete round-trip
  var tblWidths = [];
  // v1.22.0: track the currently selected cell (row,col) in the table editor.
  var tblSel = { r: -1, c: -1 };
  function readTableModel() {
    var cols = Math.max(1, Math.min(10, +document.getElementById("tblCols").value || 3));
    var rows = Math.max(1, Math.min(40, +document.getElementById("tblRows").value || 4));
    var header = document.getElementById("tblHeader").checked;
    var cells = [], widths = [];
    var inputs = document.querySelectorAll("#tblEditor input.cell");
    var k = 0;
    for (var r = 0; r < rows; r++) { cells[r] = []; for (var c = 0; c < cols; c++) { cells[r][c] = inputs[k] ? inputs[k].value : ""; k++; } }
    // v1.55.0: preserve any custom column widths instead of flattening them to 1
    for (var c2 = 0; c2 < cols; c2++) widths[c2] = (tblWidths[c2] > 0) ? tblWidths[c2] : 1;
    return { cols: cols, rows: rows, header: header, widths: widths, cells: cells };
  }
  function renderTableEditor(model) {
    var host = document.getElementById("tblEditor");
    var html = "<table><tbody>";
    for (var r = 0; r < model.rows; r++) {
      html += "<tr>";
      for (var c = 0; c < model.cols; c++) {
        var v = (model.cells[r] && model.cells[r][c] != null) ? model.cells[r][c] : "";
        var hd = (model.header && r === 0) ? "hd" : "";
        var selCls = (r === tblSel.r && c === tblSel.c) ? " cellsel" : "";
        html += "<td data-r='" + r + "' data-c='" + c + "'><input class='cell " + hd + selCls +
          "' data-r='" + r + "' data-c='" + c + "' value=\"" + escapeHtml(v).replace(/"/g, "&quot;") + "\"></td>";
      }
      html += "</tr>";
    }
    html += "</tbody></table>";
    host.innerHTML = html;
    // wire cell focus → remember selection (so add/del row/col + field-insert target it)
    Array.prototype.slice.call(host.querySelectorAll("input.cell")).forEach(function (inp) {
      inp.addEventListener("focus", function () {
        tblSel = { r: +this.dataset.r, c: +this.dataset.c };
        Array.prototype.slice.call(host.querySelectorAll("input.cell")).forEach(function (x) { x.classList.remove("cellsel"); });
        this.classList.add("cellsel");
      });
    });
  }

  // build the field dropdown for in-cell insertion
  function populateTblFieldSelect() {
    var sel = document.getElementById("tblFieldSel");
    if (!sel) return;
    sel.innerHTML = "<option value=''>— انتخاب فیلد —</option>";
    (window.AZ_FIELD_CATS || []).forEach(function (cat) {
      var og = document.createElement("optgroup"); og.label = cat.title;
      cat.items.forEach(function (f) {
        var op = document.createElement("option"); op.value = f.key; op.textContent = f.label + "  " + f.key;
        og.appendChild(op);
      });
      sel.appendChild(og);
    });
  }

  function wireTableBuilder() {
    document.getElementById("tblClose").addEventListener("click", function () { document.getElementById("tblOverlay").classList.add("hidden"); });
    document.getElementById("tblCancel").addEventListener("click", function () { document.getElementById("tblOverlay").classList.add("hidden"); });
    populateTblFieldSelect();
    function rebuild(model) {
      document.getElementById("tblCols").value = model.cols;
      document.getElementById("tblRows").value = model.rows;
      renderTableEditor(model);
    }
    document.getElementById("tblRebuild").addEventListener("click", function () {
      var cur = readTableModel();
      var cols = Math.max(1, Math.min(10, +document.getElementById("tblCols").value || 3));
      var rows = Math.max(1, Math.min(40, +document.getElementById("tblRows").value || 4));
      var m = { cols: cols, rows: rows, header: document.getElementById("tblHeader").checked, widths: [], cells: [] };
      for (var r = 0; r < rows; r++) { m.cells[r] = []; for (var c = 0; c < cols; c++) { m.cells[r][c] = (cur.cells[r] && cur.cells[r][c]) || ""; } }
      renderTableEditor(m);
    });
    // ---- v1.22.0 add/delete row & column ----
    document.getElementById("tblAddRow").addEventListener("click", function () {
      var m = readTableModel();
      var at = (tblSel.r >= 0 ? tblSel.r + 1 : m.rows);
      var nr = []; for (var c = 0; c < m.cols; c++) nr.push("");
      m.cells.splice(at, 0, nr); m.rows++;
      tblSel = { r: at, c: 0 }; rebuild(m);
    });
    document.getElementById("tblAddCol").addEventListener("click", function () {
      var m = readTableModel();
      var at = (tblSel.c >= 0 ? tblSel.c + 1 : m.cols);
      for (var r = 0; r < m.rows; r++) m.cells[r].splice(at, 0, "");
      m.cols++; m.widths.splice(at, 0, 1); tblWidths = m.widths.slice();
      tblSel = { r: 0, c: at }; rebuild(m);
    });
    document.getElementById("tblDelRow").addEventListener("click", function () {
      var m = readTableModel(); if (m.rows <= 1) { toast("حداقل یک ردیف لازم است", "err"); return; }
      var at = (tblSel.r >= 0 ? tblSel.r : m.rows - 1);
      m.cells.splice(at, 1); m.rows--;
      tblSel = { r: Math.min(at, m.rows - 1), c: tblSel.c }; rebuild(m);
    });
    document.getElementById("tblDelCol").addEventListener("click", function () {
      var m = readTableModel(); if (m.cols <= 1) { toast("حداقل یک ستون لازم است", "err"); return; }
      var at = (tblSel.c >= 0 ? tblSel.c : m.cols - 1);
      for (var r = 0; r < m.rows; r++) m.cells[r].splice(at, 1);
      m.cols--; m.widths.splice(at, 1); tblWidths = m.widths.slice();
      tblSel = { r: tblSel.r, c: Math.min(at, m.cols - 1) }; rebuild(m);
    });
    // ---- v1.22.0 insert a {field} token into the selected cell ----
    document.getElementById("tblFieldInsert").addEventListener("click", function () {
      var key = document.getElementById("tblFieldSel").value;
      if (!key) { toast("ابتدا یک فیلد را انتخاب کنید", "err"); return; }
      if (tblSel.r < 0 || tblSel.c < 0) { toast("ابتدا یک سلول را انتخاب کنید", "err"); return; }
      var inp = document.querySelector("#tblEditor input.cell[data-r='" + tblSel.r + "'][data-c='" + tblSel.c + "']");
      if (!inp) { toast("سلول یافت نشد", "err"); return; }
      inp.value = (inp.value ? inp.value + " " : "") + key;
      inp.focus();
      toast("فیلد در سلول درج شد", "ok");
    });
    document.getElementById("tblInsert").addEventListener("click", function () {
      var model = readTableModel();
      var hhEl = document.getElementById("tblHeaderH"), rhEl = document.getElementById("tblRowH");
      var newHH = hhEl ? Math.max(0, +hhEl.value || 0) : 0;
      var newRH = rhEl ? Math.max(0, +rhEl.value || 0) : 0;
      if (tblTarget) {
        pushUndo();
        tblTarget.text = JSON.stringify(model);
        tblTarget.headerH = newHH; tblTarget.rowH = newRH;
        renderAll(); renderInspector();
      } else {
        pushUndo();
        var it = defaultItem("table");
        it.text = JSON.stringify(model);
        it.headerH = newHH; it.rowH = newRH;
        var dm = paperDims(S.design.paper, S.design.orientation);
        it.w = Math.min(dm[0] - 16, 120); it.x = 8; it.y = 40;
        // size the box from the explicit heights when the user supplied them
        var autoH = newRH > 0
          ? (newHH > 0 ? newHH : newRH) + newRH * Math.max(0, model.rows - (model.header ? 1 : 0)) + 1
          : model.rows * 8 + 2;
        it.h = Math.min(dm[1] - 50, autoH);
        S.design.items.push(it); renderAll(); select(it.id);
      }
      document.getElementById("tblOverlay").classList.add("hidden");
      toast("جدول اعمال شد", "ok");
    });
  }

  /* -------------------------------------------------- templates gallery --- */
  var TPL_GROUPS = [
    { key: "reception", title: "پذیرش و صورتحساب" },
    { key: "lab", title: "آزمایشگاه و تصویربرداری" }
  ];
  function tplGroupOf(t) {
    var g = (t.group || "").toLowerCase();
    if (g) return g;
    var n = (t.name || "") + (t.kind || "");
    if (/آزمایش|رادیو|lab|radio/i.test(n)) return "lab";
    return "reception";
  }
  function openTemplateGallery() {
    var ov = document.getElementById("tplOverlay");
    var grid = document.getElementById("tplGrid");
    var tabs = document.getElementById("tplTabs");
    grid.innerHTML = ""; tabs.innerHTML = "";

    var current = "reception";
    function renderTab() {
      Array.prototype.slice.call(tabs.children).forEach(function (b) { b.classList.toggle("active", b.dataset.g === current); });
      grid.innerHTML = "";
      var list = (S.templates || []).filter(function (t) { return tplGroupOf(t) === current; });
      if (!list.length) { grid.innerHTML = "<div class='muted' style='padding:24px'>طرحی در این دسته نیست.</div>"; return; }
      list.forEach(function (t) {
        var card = document.createElement("div"); card.className = "tpl-card";
        var thumb = document.createElement("div"); thumb.className = "tpl-thumb";
        thumb.appendChild(buildThumb(t));
        var nm = document.createElement("div"); nm.className = "tpl-nm"; nm.textContent = t.name || "طرح";
        var meta = document.createElement("div"); meta.className = "tpl-meta"; meta.textContent = (PAPER_LABELS[t.paper] || t.paper || "A5") + " · " + faDigits((t.items || []).length) + " آیتم";
        card.appendChild(thumb); card.appendChild(nm); card.appendChild(meta);
        card.addEventListener("click", function () { applyTemplate(t); ov.classList.add("hidden"); });
        grid.appendChild(card);
      });
    }
    TPL_GROUPS.forEach(function (g) {
      var b = document.createElement("button"); b.className = "tpl-tab"; b.dataset.g = g.key;
      var cnt = (S.templates || []).filter(function (t) { return tplGroupOf(t) === g.key; }).length;
      b.textContent = g.title + " (" + faDigits(cnt) + ")";
      b.addEventListener("click", function () { current = g.key; renderTab(); });
      tabs.appendChild(b);
    });
    renderTab();
    ov.classList.remove("hidden");
  }

  // v1.22.0 "طراحی‌های من" — list user-saved designs from the C++ DB.
  function openMyDesigns() {
    var ov = document.getElementById("myOverlay");
    var grid = document.getElementById("myGrid");
    grid.innerHTML = "<div class='muted' style='padding:24px'>در حال بارگذاری…</div>";
    ov.classList.remove("hidden");

    function render(list) {
      grid.innerHTML = "";
      if (!list || !list.length) {
        grid.innerHTML = "<div class='muted' style='padding:24px'>هنوز طرحی ذخیره نکرده‌اید. پس از طراحی، دکمهٔ «ذخیره و اعمال» را بزنید و نامی برای طرح وارد کنید تا اینجا نمایش داده شود.</div>";
        return;
      }
      list.forEach(function (t) {
        var card = document.createElement("div"); card.className = "tpl-card my-card";
        var thumb = document.createElement("div"); thumb.className = "tpl-thumb";
        thumb.appendChild(buildThumb(t));
        var nm = document.createElement("div"); nm.className = "tpl-nm"; nm.textContent = t.name || "طرح من";
        var meta = document.createElement("div"); meta.className = "tpl-meta";
        meta.textContent = (PAPER_LABELS[t.paper] || t.paper || "A5") + " · " + faDigits((t.items || []).length) + " آیتم";
        var del = document.createElement("button"); del.className = "my-del"; del.title = "حذف طرح"; del.textContent = "🗑";
        del.addEventListener("click", function (e) {
          e.stopPropagation();
          if (!confirm("این طرح حذف شود؟ «" + (t.name || "") + "»")) return;
          Bridge.request("delete", { id: t.id }, function (res) {
            if (res && res.ok) { card.remove(); toast("طرح حذف شد", "ok"); }
            else toast("خطا در حذف طرح", "err");
          });
        });
        card.appendChild(del);
        card.appendChild(thumb); card.appendChild(nm); card.appendChild(meta);
        card.addEventListener("click", function () { applyTemplate(t); ov.classList.add("hidden"); });
        grid.appendChild(card);
      });
    }

    if (Bridge.has()) {
      Bridge.request("mydesigns", {}, function (res) {
        render((res && res.designs) || []);
      });
    } else {
      var local = [];
      try { var raw = localStorage.getItem("az_design"); if (raw) local = [JSON.parse(raw)]; } catch (e) {}
      render(local);
    }
  }

  // High-fidelity mini preview: render real text/lines/boxes/tables.
  function buildThumb(t) {
    var dm = paperDims(t.paper || "A5", t.orientation || 0);
    var maxH = 184, maxW = 168;
    var scale = Math.min(maxH / dm[1], maxW / dm[0]);
    var p = document.createElement("div"); p.className = "mini-paper";
    p.style.width = (dm[0] * scale) + "px"; p.style.height = (dm[1] * scale) + "px";
    var items = (t.items || []).slice().sort(function (a, b) { return (a.z || 0) - (b.z || 0); });
    items.forEach(function (it) {
      var e = document.createElement("div"); e.className = "mini-it";
      e.style.left = (it.x * scale) + "px"; e.style.top = (it.y * scale) + "px";
      e.style.width = (it.w * scale) + "px"; e.style.height = Math.max(0.6, it.h * scale) + "px";
      if (it.type === "hline") { e.style.borderTop = "0.7px solid " + (it.borderColor || "#444"); }
      else if (it.type === "vline") { e.style.borderRight = "0.7px solid " + (it.borderColor || "#444"); }
      else if (it.type === "rect" || it.type === "frame" || it.type === "table") {
        e.style.border = "0.6px solid " + (it.borderColor || "#789");
        if (!it.fillTransparent && it.fillColor && it.type === "rect") e.style.background = it.fillColor;
      }
      else if (it.type === "services") {
        var band = (!it.fillTransparent && it.fillColor) ? it.fillColor : "#e9e9e9";
        e.style.border = "0.6px solid " + (it.borderColor || "#000");
        e.style.background = "linear-gradient(" + band + " 0 22%, #fff 22% 100%)";
      }
      else if (it.type === "barcode") {
        // v1.55.0: miniature bar pattern so the thumbnail reads as a barcode
        e.style.background = "repeating-linear-gradient(90deg," +
          (it.textColor || "#000") + " 0 0.7px, transparent 0.7px 1.9px," +
          (it.textColor || "#000") + " 1.9px 2.6px, transparent 2.6px 4.2px)";
      }
      else if (it.type === "logo" || it.type === "photo" || it.type === "image" || it.type === "qr") {
        e.style.border = "0.6px dashed #9aa7c2"; e.style.background = "#f3f6fb";
      }
      else if (it.type === "label" || it.type === "field") {
        e.className += " mini-tx";
        var txt = it.type === "label" ? (it.text || "") :
          ((it.prefix || "") + (window.AZ_FIELDS[it.field] ? window.AZ_FIELDS[it.field].label : "···"));
        e.textContent = txt;
        e.style.color = it.textColor || "#222";
        e.style.fontSize = Math.max(3.5, (it.pt || 9) * scale * 1.05) + "px";
        e.style.fontWeight = it.bold ? "700" : "400";
        e.style.justifyContent = it.align === 1 ? "center" : (it.align === 2 ? "flex-start" : "flex-end");
        if (it.type === "field") e.style.background = "rgba(207,224,255,.5)";
      }
      p.appendChild(e);
    });
    return p;
  }

  // §1.53.0 (Bug B): on very narrow rolls (R80/R58 …) compact the layout so a
  // template authored in A4 stays legible — stack side-by-side pairs into single
  // column, shrink the header band and drop QR/logo blocks, clamp to page edges.
  function compactForNarrow(d) {
    var dm = paperDims(d.paper, d.orientation);
    var pw = dm[0], ph = dm[1];
    if (pw >= 80) return;                    // only compact truly narrow rolls
    var items = d.items || [];
    // drop decorative media that eats precious width on a thin roll.
    // v1.55.0: a `barcode` is NOT decorative — it carries the scannable receipt
    // identifier, so it always survives (it is merely narrowed by the clamp).
    if (pw < 80) {
      d.items = items = items.filter(function (it) {
        return !(it.type === "qr" || it.type === "logo" || it.type === "photo");
      });
    }
    // v1.55.0 RESPONSIVE TABLES: on a thin roll a three-column services table
    // becomes unreadable, so drop the widest prose column (شرح خدمت) and give
    // its width to the service name. The remaining columns keep their meaning
    // because the print engine resolves data by column CAPTION, not by index.
    items.forEach(function (it) {
      if (it.type !== "services") return;
      var m = parseServices(it);
      if (m.cols <= 2) return;
      var keepIdx = [], i2;
      for (i2 = 0; i2 < m.cols; i2++) {
        var kind = svcColOf(m.labels[i2], i2);
        if (kind !== PSC.DESC && kind !== PSC.CAT && kind !== PSC.ROW) keepIdx.push(i2);
      }
      if (!keepIdx.length || keepIdx.length === m.cols) return;
      var nl = [], nw = [], s2 = 0;
      keepIdx.forEach(function (ix) { nl.push(m.labels[ix] || ""); nw.push(m.widths[ix] || 0); });
      nw.forEach(function (x) { s2 += x; });
      if (s2 > 0) nw = nw.map(function (x) { return Math.round(x / s2 * 1000) / 1000; });
      m.cols = keepIdx.length; m.labels = nl; m.widths = nw;
      it.text = JSON.stringify(m);
    });
    // group items into rows by their y band, then detect side-by-side pairs and
    // stack them vertically (single column) so nothing runs off the edge.
    items.sort(function (a, b) { return (a.y - b.y) || (b.x - a.x); });
    var margin = 3, colW = pw - margin * 2;
    for (var i = 0; i < items.length; i++) {
      var it = items[i];
      // find a horizontal neighbour that shares the same band
      for (var j = i + 1; j < items.length; j++) {
        var jt = items[j];
        if (Math.abs(jt.y - it.y) > Math.max(it.h, 4)) break;   // different band
        var overlapX = !(jt.x + jt.w <= it.x || it.x + it.w <= jt.x);
        if (overlapX) continue;
        // stack jt beneath it
        jt.y = it.y + it.h + 1.5;
      }
    }
    // final clamp: every item full-width-safe and inside the page
    items.forEach(function (it) {
      if (it.type === "frame") { it.x = 2; it.y = 2; it.w = pw - 4; it.h = ph - 4; return; }
      if (it.x < margin) it.x = margin;
      if (it.x + it.w > pw - margin) {
        // shrink to the usable width if it started near the right, else move left
        if (it.w > colW) it.w = colW;
        it.x = Math.max(margin, pw - margin - it.w);
      }
      if (it.y < 0) it.y = 0;
      if (it.y + it.h > ph) it.y = Math.max(0, ph - it.h);
    });
  }

  function applyTemplate(t) {
    pushUndo();
    // §1.53.0 (Bug B): remember the section's CURRENT paper/orientation so an
    // A4-authored template is reflowed to whatever paper the section uses,
    // instead of hijacking the section into the template's own paper.
    var prevPaper = (S.design && S.design.paper) || null;
    var prevOrient = (S.design && typeof S.design.orientation === "number") ? S.design.orientation : 0;

    var d = clone(t);
    d.id = (S.design && S.design.id) || 0;   // keep binding so save UPDATES current
    d.kind = "user";
    if (!d.name) d.name = t.name || "طرح";
    var k = 1; (d.items || []).forEach(function (it) { it.id = k++; });
    S.design = d; S.selId = 0; S.selIds = [];

    if (S.reflowOnResize !== false && prevPaper && prevPaper !== d.paper) {
      // reflow the freshly-cloned template from its authored paper (t.paper) to
      // the section's paper, then adopt the section's paper/orientation.
      var tDims = paperDims(t.paper || d.paper, t.orientation || 0);
      d.paper = prevPaper; d.orientation = prevOrient;
      var nDims = paperDims(prevPaper, prevOrient);
      reflowItems(tDims[0], tDims[1], nDims[0], nDims[1]);
      compactForNarrow(d);
    } else if (S.reflowOnResize === false && prevPaper && prevPaper !== d.paper) {
      toast("واکنش‌گرا خاموش است — کاغذ طرح (" + (d.paper) + ") حفظ شد", "warn");
    }

    document.getElementById("paperSel").value = S.design.paper;
    document.getElementById("orientSel").value = S.design.orientation || 0;
    renderAll(); fitZoom(); renderLayers(); renderInspector();
    toast("طرح اعمال شد: " + (t.name || ""), "ok");
  }

  /* ------------------------------------------------------------ save ------ */
  function saveDesign() {
    if (!S.design) return;
    if (!S.design.name || S.design.kind === "builtin") {
      var nm = prompt("نام طرح را وارد کنید:", S.design.name || "طرح سفارشی");
      if (nm === null) return;
      S.design.name = nm || "طرح سفارشی"; S.design.kind = "user";
    }
    if (Bridge.has()) {
      var btn = document.getElementById("btnSave"); if (btn) { btn.disabled = true; btn.textContent = "در حال ذخیره…"; }
      Bridge.request("save", { design: S.design }, function (res) {
        if (btn) { btn.disabled = false; btn.innerHTML = "💾 ذخیره و اعمال"; }
        if (res && res.ok) {
          if (res.id) S.design.id = res.id;
          S.dirty = false;
          toast("ذخیره شد و بر بخش اعمال گردید ✓", "ok");
        } else {
          var why = (res && res.err) ? (" — " + res.err) : "";
          console.error("[designer] save failed", res);
          toast("خطا در ذخیره" + why + " (کنسول را ببینید)", "err");
        }
      });
    } else {
      try { localStorage.setItem("az_design", JSON.stringify(S.design)); } catch (e) { console.error(e); }
      S.dirty = false; toast("ذخیره شد (حالت آزمایشی)", "ok");
    }
  }

  // Real download: build a .aztpl file and let the browser save it. Works in
  // the default browser the designer now opens in.
  function downloadDesign() {
    try {
      var data = JSON.stringify(S.design, null, 0);
      var blob = new Blob([data], { type: "application/octet-stream" });
      var url = URL.createObjectURL(blob);
      var a = document.createElement("a");
      a.href = url;
      a.download = (S.design.name || "design").replace(/[\\/:*?"<>|]/g, "_") + ".aztpl";
      document.body.appendChild(a); a.click(); document.body.removeChild(a);
      setTimeout(function () { URL.revokeObjectURL(url); }, 1000);
      toast("فایل طرح دانلود شد", "ok");
    } catch (e) {
      console.error("[designer] download failed", e);
      toast("دانلود ناموفق بود (کنسول را ببینید)", "err");
    }
  }
  function uploadDesign() { document.getElementById("fileInput").click(); }

  /* ------------------------------------------------------------ wire ------ */
  function populatePaperSelect() {
    var sel = document.getElementById("paperSel");
    sel.innerHTML = "";
    Object.keys(PAPER).forEach(function (k) {
      var op = document.createElement("option"); op.value = k; op.textContent = PAPER_LABELS[k] || k;
      sel.appendChild(op);
    });
  }

  function wire() {
    populatePaperSelect();
    document.getElementById("paperSel").addEventListener("change", function () { changePaper(this.value, undefined); });
    document.getElementById("orientSel").addEventListener("change", function () { changePaper(undefined, +this.value); });
    var bSel = document.getElementById("btnToolSelect");
    var bHand = document.getElementById("btnToolHand");
    if (bSel) bSel.addEventListener("click", function () { setTool("select"); });
    if (bHand) bHand.addEventListener("click", function () { setTool("hand"); });
    var rf = document.getElementById("chkReflow");
    if (rf) { rf.checked = S.reflowOnResize; rf.addEventListener("change", function () { S.reflowOnResize = this.checked; }); }
    var mi = document.getElementById("marginInp");
    if (mi) { mi.value = S.pageMargin; mi.addEventListener("change", function () {
      S.pageMargin = Math.max(0, Math.min(30, parseFloat(mi.value) || 0)); mi.value = S.pageMargin;
      renderMarginGuide();
    }); }
    document.getElementById("btnUndo").addEventListener("click", doUndo);
    document.getElementById("btnRedo").addEventListener("click", doRedo);
    document.getElementById("btnZoomIn").addEventListener("click", function () { setScale(S.scale * 1.15); });
    document.getElementById("btnZoomOut").addEventListener("click", function () { setScale(S.scale / 1.15); });
    document.getElementById("btnZoomFit").addEventListener("click", fitZoom);
    document.getElementById("btnSave").addEventListener("click", saveDesign);
    document.getElementById("btnDownload").addEventListener("click", downloadDesign);
    document.getElementById("btnUpload").addEventListener("click", uploadDesign);
    document.getElementById("btnTemplates").addEventListener("click", openTemplateGallery);
    document.getElementById("tplClose").addEventListener("click", function () { document.getElementById("tplOverlay").classList.add("hidden"); });
    document.getElementById("tplOverlay").addEventListener("click", function (e) { if (e.target.id === "tplOverlay") this.classList.add("hidden"); });
    var bMy = document.getElementById("btnMyDesigns");
    if (bMy) bMy.addEventListener("click", openMyDesigns);
    var myClose = document.getElementById("myClose");
    if (myClose) myClose.addEventListener("click", function () { document.getElementById("myOverlay").classList.add("hidden"); });
    var myOv = document.getElementById("myOverlay");
    if (myOv) myOv.addEventListener("click", function (e) { if (e.target.id === "myOverlay") this.classList.add("hidden"); });
    document.getElementById("btnExit").addEventListener("click", function () {
      if (S.dirty && !confirm("تغییرات ذخیره‌نشده دارید. خارج می‌شوید؟")) return;
      if (Bridge.has()) Bridge.request("exit", {}, function () {});
      toast("جلسهٔ طراحی پایان یافت");
      setTimeout(function () { try { window.close(); } catch (e) {} }, 400);
    });
    Array.prototype.slice.call(document.querySelectorAll(".rp-tab")).forEach(function (t) {
      t.addEventListener("click", function () {
        switchTab(t.dataset.tab);
        if (t.dataset.tab === "inspector") renderInspector();
        if (t.dataset.tab === "layers") renderLayers();
      });
    });
    document.getElementById("paletteSearch").addEventListener("input", function () { filterPalette(this.value); });

    document.getElementById("fileInput").addEventListener("change", function (e) {
      var f = e.target.files[0]; if (!f) return;
      var r = new FileReader();
      r.onload = function () {
        try {
          var d = JSON.parse(r.result);
          pushUndo(); d.id = (S.design && S.design.id) || 0; S.design = d; S.selId = 0; S.selIds = [];
          if (!S.design.paper) S.design.paper = "A5"; if (!S.design.items) S.design.items = [];
          document.getElementById("paperSel").value = S.design.paper;
          document.getElementById("orientSel").value = S.design.orientation || 0;
          renderAll(); fitZoom(); renderLayers(); toast("طرح بارگذاری شد", "ok");
        } catch (err) { console.error("[designer] invalid file", err); toast("فایل نامعتبر است", "err"); }
      };
      r.readAsText(f); e.target.value = "";
    });

    wireTableBuilder();
  }

  /* ------------------------------------------------------------ init ------ */
  function loadInitial(cb) {
    if (Bridge.has()) {
      Bridge.request("init", {}, function (res) {
        var initial = null, secName = "";
        if (res) {
          if (res.design) initial = res.design;
          if (res.sectionName) secName = res.sectionName;
        }
        // Always prefer the rich JS gallery for the gallery itself.
        if (!S.templates || !S.templates.length) {
          Bridge.request("templates", {}, function (tr) {
            if (tr && tr.templates && tr.templates.length) S.templates = tr.templates;
            finish(initial, secName);
          });
        } else { finish(initial, secName); }
      });
    } else { finish(null, ""); }

    function finish(initial, secName) {
      if (!initial || !(initial.items && initial.items.length)) {
        initial = clone((S.templates && S.templates[0]) || { paper: "A5", orientation: 0, items: [] });
        var k = 1; (initial.items || []).forEach(function (it) { it.id = k++; });
      }
      S.design = initial;
      if (!S.design.paper) S.design.paper = "A5";
      if (!S.design.items) S.design.items = [];
      var sn = document.getElementById("secName");
      if (sn) sn.textContent = secName ? ("— " + secName) : "";
      document.getElementById("paperSel").value = S.design.paper;
      document.getElementById("orientSel").value = S.design.orientation || 0;
      cb && cb();
    }
  }

  function boot() {
    $paper = document.getElementById("paper");
    $scroll = document.getElementById("canvasScroll");
    $selBox = document.getElementById("selBox");
    $stage = document.getElementById("canvasStage");
    buildPalette(); wire(); wireCanvas(); setTool("select");
    loadInitial(function () {
      renderAll(); fitZoom(); renderLayers(); updateUndoButtons();
      if (Bridge.has()) Bridge.request("ready", {}, function () {});
    });
  }
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", boot);
  else boot();

  // Public surface. The v1.55.0 additions expose the pure model/encoder helpers
  // so the print contract (3-column services, label-driven columns, real
  // barcode symbologies) can be asserted automatically against the C++ engine.
  window.AZDesigner = {
    S: S, render: renderAll, save: saveDesign,
    // services model
    parseServices: parseServices, servicesHtml: servicesHtml,
    svcColOf: svcColOf, svcSample: svcSample, PSC: PSC,
    SVC_DEF_LABELS: SVC_DEF_LABELS, SVC_DEF_WIDTHS: SVC_DEF_WIDTHS,
    // barcode engine (must stay bit-identical to printer.cpp)
    parseBarcode: parseBarcode, barcodeHtml: barcodeHtml,
    bc128Modules: bc128Modules, bc39Modules: bc39Modules,
    bcEan13Modules: bcEan13Modules, bcNormPayload: bcNormPayload,
    C128_PAT: C128_PAT, C39_PAT: C39_PAT, C39_SET: C39_SET,
    EAN_A: EAN_A, EAN_B: EAN_B, EAN_C: EAN_C, EAN_PAR: EAN_PAR,
    // items / tables
    defaultItem: defaultItem, parseTable: parseTable, tableHtml: tableHtml,
    reflowItems: reflowItems, compactForNarrow: compactForNarrow,
    displayText: displayText, setTool: setTool, selectMany: selectMany,
    SVC_PLACEHOLDER_ROWS: SVC_PLACEHOLDER_ROWS
  };
})();
