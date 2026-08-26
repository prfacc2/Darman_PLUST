/* ============================================================================
   admission.js — Patient Admission UI logic (AZADI_TEB).

   ES5-only syntax (no arrow functions / const / let / template literals / Map /
   spread / destructuring / fetch) so it runs identically on the WebView2
   (Chromium) engine AND the universal MSHTML / Trident (IE11) fallback.

   Talks to C++ ONLY through Bridge (structured JSON messages). C++ is the
   single source of truth — the UI never fabricates prices or identities.

   Behaviours implemented per the Management spec:
     · National-ID + Enter  → C++ lookup, auto-fills every patient field and
       auto-selects the verified insurance, WITHOUT resetting other fields.
     · Enter moves focus to the NEXT logical field (data-nav order). Tab too.
     · Birth-date field auto-inserts "/", Ctrl+A selects the whole value and
       typing replaces cleanly from the start.
     · Invoice is ZERO on open; numbers appear only after a real service (with
       a Management-defined name + price) is added.
     · Billing: آزاد / no insurance → full net base price (no reduction);
       insured → reduced per the base + supplementary insurance percentages.
     · Services searched by code/name; price ALWAYS from the catalog; quantity
       editable (pencil / double-click). List updates live, no refresh.
     · Queue is «صندوق نرفته‌ها»; receptionist can add the current admission to
       it; search + recent-minutes filter + collapse/expand.
     · ثبت پذیرش و صدور قبض → C++ saves + prints per the Management print design.
   ============================================================================ */
(function () {
  'use strict';

  function $(id) { return document.getElementById(id); }

  /* ==========================================================================
     v1.92.0 — global front-end error reporting. Uncaught JS errors (and the
     caught ones below) are forwarded to C++ via the 'client.error' bridge verb
     so they land in the DEDICATED HTML error log (logs\html errors\errors.log)
     — NOT the normal activity log. Only real bugs/crashes/load failures are
     ever reported; normal info/debug logs are never sent here.
     A re-entrancy guard (_errReporting) makes sure an error thrown inside the
     reporter itself can never loop forever. window.onerror returns false so the
     engine's own default handler still runs. ========================================================================== */
  var _errReporting = false;
  function reportJsError(e, ctx) {
    if (_errReporting) return;
    _errReporting = true;
    try {
      if (window.Bridge && typeof window.Bridge.call === 'function') {
        window.Bridge.call('client.error', {
          message: String((e && e.message) ? e.message : e).substring(0, 300),
          source: String(ctx || ''),
          line: 0,
          column: 0,
          stack: (e && e.stack) ? String(e.stack).substring(0, 500) : ''
        });
      }
    } catch (x) { /* never let the reporter throw again */ }
    _errReporting = false;
  }
  window.onerror = function (msg, src, line, col, err) {
    if (_errReporting) return false;
    _errReporting = true;
    try {
      var stack = (err && err.stack) ? String(err.stack).substring(0, 500) : '';
      if (window.Bridge && typeof window.Bridge.call === 'function') {
        window.Bridge.call('client.error', {
          message: String(msg).substring(0, 300),
          source: String(src || ''),
          line: line || 0,
          column: col || 0,
          stack: stack
        });
      }
    } catch (e2) { /* prevent infinite loop */ }
    _errReporting = false;
    return false; /* let the default handler also run */
  };

  var state = {
    services: [],          /* current admission service rows */
    insurances: [],        /* {name,pct} base list */
    supp: [],              /* supplementary list */
    patient: null,         /* current loaded patient (or null) */
    catalog: [],           /* last service search results */
    queue: [],             /* active صندوق/پذیرش queue rows */
    queueKind: 'unpaid',   /* independent file-backed queue selected by tabs */
    doctors: [],
    performers: [],        /* v1.78.0: cached «انجام دهنده» list (isPerformer doctors) */
    ps: { P: 0, S: 0 },
    mode: 'simple',
    zoom: 80,
    overrideBlock: false,
    ready: false,
    formLocked: false,
    canCashView: true,
    canCashEdit: true,
    cashTab: 0,
    cashQ: '',
    cashStatus: '',
    cashRows: [],
    loadedTicketId: '',
    reprintId: '',
    payTicketId: '',
    payCtx: null,
    ticketMiniId: '',
    role: 0,
    userName: '',
    todayJalali: '',
    rcRows: [],
    rcSel: '',
    rcChecked: {},
    rcHits: [],
    rcPage: 'home',
    rcPageNo: 1,
    surface: 'admission'
  };

  /* ---- Persian digit helpers ---- */
  var FA = '۰۱۲۳۴۵۶۷۸۹';
  function toFa(n) {
    return String(n == null ? '' : n).replace(/[0-9]/g, function (d) { return FA.charAt(+d); });
  }
  function toEn(s) {
    return String(s == null ? '' : s)
      .replace(/[۰-۹]/g, function (d) { return String(FA.indexOf(d)); })
      .replace(/[٠-٩]/g, function (d) { return String('٠١٢٣٤٥٦٧٨٩'.indexOf(d)); });
  }
  function money(n) {
    var v = Number(n || 0);
    var str = String(Math.round(Math.abs(v)));
    var out = '', c = 0, i;
    for (i = str.length - 1; i >= 0; i--) {
      out = str.charAt(i) + out;
      if (++c % 3 === 0 && i > 0) out = ',' + out;
    }
    return toFa((v < 0 ? '-' : '') + out);
  }
  function trimStr(s) { return String(s == null ? '' : s).replace(/^\s+|\s+$/g, ''); }

  function setText(el, text) {
    if (!el) return;
    el.innerHTML = '';
    el.appendChild(document.createTextNode(text == null ? '' : String(text)));
  }
  function esc(s) {
    s = (s == null ? '' : String(s));
    return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }
  function on(el, ev, fn) {
    if (!el) return;
    if (el.addEventListener) el.addEventListener(ev, fn, false);
    else el.attachEvent('on' + ev, fn);
  }

  /* ==========================================================================
     v1.50.0 — FREEZE-PROOF HELPERS.
     Root cause of the whole-app freeze: the old per-row onclick handlers
     SYNCHRONOUSLY tore down (innerHTML='') the very DOM node that was still
     dispatching the click event. The in-process MSHTML/Trident engine shares
     the app's Win32 message pump; destroying the dispatching node wedges it and
     freezes the ENTIRE program (close button, tab switch, everything).
     Solution: every handler that rebuilds a list it lives inside MUST route the
     mutation through defer() so the DOM is only touched after the event has
     fully unwound. ========================================================================== */
  function defer(fn) {
    setTimeout(function () {
      try { fn(); } catch (e) { if (window.console) console.error(e); reportJsError(e, 'defer'); }
    }, 0);
  }

  /* nearest ancestor (or self) of `node` carrying attribute `attr`, stopping at
     `stop`. ES5/MSHTML-safe (no Element.closest). */
  function findUp(node, attr, stop) {
    while (node && node !== stop) {
      if (node.getAttribute && node.getAttribute(attr) != null) return node;
      node = node.parentNode;
    }
    return null;
  }

  function toast(msg, kind) {
    var t = $('toast');
    if (!t) return;
    setText(t, msg);
    t.className = 'toast show ' + (kind || '');
    if (toast._t) clearTimeout(toast._t);
    toast._t = setTimeout(function () { t.className = 'toast'; }, 2600);
  }

  function setSync(kind, text) {
    var b = $('syncBadge');
    if (b) b.className = 'sync-badge ' + (kind || '');
    setText($('syncText'), text);
  }

  /* The three reception zones are deliberately distinct. Whichever zone owns
     keyboard/mouse focus gets a restrained stronger outline and typography. */
  function setActiveZone(zone) {
    var ids = ['colRight', 'colCenter', 'colLeft'], i, el;
    for (i = 0; i < ids.length; i++) {
      el = $(ids[i]);
      if (!el) continue;
      el.className = String(el.className || '').replace(/\s*is-active/g, '');
      if (el === zone) el.className += ' is-active';
    }
  }
  function wireZoneFocus() {
    var ids = ['colRight', 'colCenter', 'colLeft'], i, zone;
    for (i = 0; i < ids.length; i++) {
      zone = $(ids[i]);
      if (!zone) continue;
      (function (z) {
        on(z, 'focusin', function () { setActiveZone(z); });
        on(z, 'mousedown', function () { setActiveZone(z); });
      })(zone);
    }
    setActiveZone($('colCenter'));
  }

  function applyPalette(name) {
    var body = document.body;
    if (!body) return;
    body.className = String(body.className || '')
      .replace(/\btheme-(calm|warm)\b/g, '').replace(/\s+/g, ' ');
    /* Light palettes must never override the dark surface selectors. Old
       settings.ini files can legitimately contain theme=dark + palette=warm. */
    if (/\btheme-dark\b/.test(body.className)) return;
    if (name === 'calm') body.className += ' theme-calm';
    else if (name === 'warm') body.className += ' theme-warm';
  }

  function clampPct(value) {
    var pct = Number(value);
    if (!isFinite(pct)) return 0;
    if (pct < 0) return 0;
    if (pct > 100) return 100;
    return pct;
  }

  /* ==========================================================================
     BILLING — computed entirely here from Management-defined prices.
     Base insurance pct is the organisation share of the covered amount;
     supplementary insurance pct then covers a share of the patient remainder.
     آزاد (pct 0) → patient pays the full net base price (no reduction).
     ========================================================================== */
  function hasIns() { return $('hasIns') && $('hasIns').checked; }

  function baseInsPct() {
    if (!hasIns()) return 0;
    var sel = $('insMain');
    var idx = sel ? sel.selectedIndex : -1;
    return clampPct((state.insurances[idx] && state.insurances[idx].pct) || 0);
  }
  function suppInsPct() {
    /* explicit percentage box wins; else the selected supplementary plan pct */
    var box = $('insSuppPct');
    if (box) {
      var v = parseInt(toEn(box.value), 10);
      if (!isNaN(v) && v > 0) return clampPct(v);
    }
    var sel = $('insSupp');
    var idx = sel ? sel.selectedIndex : -1;
    return clampPct((state.supp[idx] && state.supp[idx].pct) || 0);
  }

  /* per-row computation using current insurance selection */
  function computeRow(s) {
    var qty = Number(s.qty) || 1;
    if (qty < 1) qty = 1; else if (qty > 999) qty = 999;
    /* v1.75.0: pick unit price based on insurance type, matching the C++
     * adCatalogPrice logic. freeRate rows use freePrice. Otherwise:
     *   hasIns → priceIns (insurance tariff), else → priceFree (free tariff).
     * Falls back to s.price (legacy base) if the tariff field is 0. */
    var unitPrice;
    if (s.freeRate) {
      unitPrice = Number(s.freePrice) || 0;
    } else if (hasIns() && Number(s.priceIns) > 0) {
      unitPrice = Number(s.priceIns);
    } else if (!hasIns() && Number(s.priceFree) > 0) {
      unitPrice = Number(s.priceFree);
    } else {
      unitPrice = Number(s.price) || 0;
    }
    if (unitPrice < 0) unitPrice = 0;
    var gross = unitPrice * qty;
    var disc = Number(s.discount) || 0;
    if (disc < 0) disc = 0; else if (disc > gross) disc = gross;
    var net = gross - disc;
    var bPct = baseInsPct();
    var orgShare = Math.round(net * bPct / 100);      /* base insurer share */
    if (orgShare < 0) orgShare = 0; else if (orgShare > net) orgShare = net;
    var afterBase = net - orgShare;                    /* patient remainder  */
    var sPct = suppInsPct();
    var suppShare = Math.round(afterBase * sPct / 100);/* supplementary share */
    if (suppShare < 0) suppShare = 0; else if (suppShare > afterBase) suppShare = afterBase;
    var patShare = afterBase - suppShare;
    s._gross = gross; s._disc = disc; s._net = net;
    s._org = orgShare; s._supp = suppShare; s._pat = patShare;
    return s;
  }

  var _billSyncTimer = null;
  function authoritativeBillPayload() {
    return {
      hasIns: hasIns(),
      insMain: $('insMain') ? $('insMain').selectedIndex : -1,
      insSupp: $('insSupp') ? $('insSupp').selectedIndex : -1,
      insSuppPct: suppInsPct(),
      noPay: $('noPay') ? $('noPay').checked : false,
      services: state.services
    };
  }
  function applyAuthoritativeBill(b) {
    if (!b) return;
    var afterBase = (Number(b.gross) || 0) - (Number(b.disc) || 0) - (Number(b.org) || 0);
    if (afterBase < 0) afterBase = 0;
    setText($('sfTotal'), money(b.gross));
    setText($('sfDisc'), money(b.disc));
    setText($('sfIns'), money((Number(b.org) || 0) + (Number(b.supp) || 0)));
    setText($('sfPat'), money(b.pat));
    setText($('invMainTotal'), money(b.gross));
    setText($('invMainPat'), money(afterBase));
    setText($('invMainOrg'), money(b.org));
    setText($('invSuppTotal'), money(afterBase));
    setText($('invSuppShare'), money(b.supp));
    setText($('invSuppPat'), money(b.pat));
    setText($('invFinTotal'), money(b.pat));  // v1.69.0: final = patient share after insurance, NOT gross
    setText($('invFinDisc'), money(b.disc));
    setText($('invFinPaid'), money(b.paid));
    setText($('invRemain'), money((Number(b.pat) || 0) - (Number(b.paid) || 0)));
    setText($('tcVal'), money(b.pat));
  }
  function scheduleBillSync() {
    if (!state.ready || state.formLocked) return;
    // v1.70.0: sync immediately (was 100ms delay which caused a brief flash of
    // wrong values). The authoritative C++ recompute is fast enough to run
    // synchronously without freezing the UI.
    Bridge.call('bill.compute', authoritativeBillPayload()).then(applyAuthoritativeBill);
  }

  function recompute() {
    if (state.formLocked) return { gross: 0, disc: 0, org: 0, supp: 0, pat: 0, paid: 0 };
    var i, s;
    var sumGross = 0, sumDisc = 0, sumOrg = 0, sumSupp = 0, sumPat = 0;
    for (i = 0; i < state.services.length; i++) {
      s = computeRow(state.services[i]);
      sumGross += s._gross; sumDisc += s._disc;
      sumOrg += s._org; sumSupp += s._supp; sumPat += s._pat;
    }
    var netTotal = sumGross - sumDisc; if (netTotal < 0) netTotal = 0;
    var afterBase = netTotal - sumOrg;

    /* service footer strip */
    setText($('sfTotal'), money(sumGross));
    setText($('sfDisc'), money(sumDisc));
    setText($('sfIns'), money(sumOrg + sumSupp));
    setText($('sfPat'), money(sumPat));

    /* v1.75.0: show the active insurance percentages in the invoice group
       titles so the calculation breakdown is visible at a glance. */
    var bPct = baseInsPct();
    var sPct = suppInsPct();
    setText($('invGMain'), 'بیمه اصلی' + (bPct > 0 ? ' (' + toFa(bPct) + '٪)' : ''));
    setText($('invGSupp'), 'بیمه مکمل' + (sPct > 0 ? ' (' + toFa(sPct) + '٪)' : ''));
    setText($('invGFin'), 'مبلغ نهایی');

    /* invoice — بیمه اصلی */
    setText($('invMainTotal'), money(sumGross));
    setText($('invMainPat'), money(afterBase));
    setText($('invMainOrg'), money(sumOrg));
    /* invoice — بیمه مکمل */
    setText($('invSuppTotal'), money(afterBase));
    setText($('invSuppShare'), money(sumSupp));
    setText($('invSuppPat'), money(sumPat));
    /* invoice — مبلغ نهایی */
    setText($('invFinTotal'), money(sumPat));  // v1.69.0: final = patient share after insurance, NOT gross
    setText($('invFinDisc'), money(sumDisc));
    var paid = $('noPay') && $('noPay').checked ? 0 : sumPat;
    setText($('invFinPaid'), money(paid));
    setText($('invRemain'), money(sumPat - paid));

    /* total card — v1.69.0: payment status card removed, replaced with
       queue/unpaid action buttons in HTML. No paymentState element anymore. */
    setText($('tcVal'), money(sumPat));
    scheduleBillSync();
    return { gross: sumGross, disc: sumDisc, org: sumOrg, supp: sumSupp, pat: sumPat, paid: paid };
  }

  /* ==========================================================================
     SERVICE ROWS — v1.50.0 FREEZE FIX.
     The old code rebuilt the whole table (innerHTML) and re-bound per-node
     handlers SYNCHRONOUSLY inside the very event dispatch whose source node it
     was destroying. On the in-process MSHTML/Trident engine (which shares the
     app's UI thread + Win32 message pump), tearing down the node that is still
     dispatching the current event wedges the engine — and the WHOLE app froze.
     New design: addServiceRow only mutates state; ALL re-rendering goes through
     scheduleRender() (setTimeout 0) so the DOM is never torn down while an
     event is still being dispatched. Row actions use ONE delegated listener
     bound once in wire() — zero per-render handler rebinding.
     ========================================================================== */
  var _renderTimer = null;
  function scheduleRender() {
    if (_renderTimer) return;             /* coalesce bursts into one render */
    _renderTimer = setTimeout(function () {
      _renderTimer = null;
      try { renderServices(); } catch (e) { if (window.console) console.error(e); reportJsError(e, 'renderServices'); }
      try { recompute(); } catch (e2) { if (window.console) console.error(e2); reportJsError(e2, 'recompute'); }
    }, 0);
  }

  function serviceKey(v) {
    /* Keep byte-for-byte semantics with C++ serviceIdentityKey(): all three
       digit sets normalize to ASCII; ZWNJ/LRM/RLM/whitespace runs collapse to
       one interior space and vanish at boundaries. */
    return String(v == null ? '' : v)
      .replace(/[۰-۹]/g, function (d) { return String('۰۱۲۳۴۵۶۷۸۹'.indexOf(d)); })
      .replace(/[٠-٩]/g, function (d) { return String('٠١٢٣٤٥٦٧٨٩'.indexOf(d)); })
      .replace(/[ي]/g, 'ی').replace(/[ك]/g, 'ک')
      .replace(/[\u0009-\u000d \u00a0\u1680\u2000-\u200a\u2028\u2029\u202f\u205f\u3000\u200c\u200e\u200f]+/g, ' ')
      .replace(/^ +| +$/g, '')
      .replace(/[A-Z]/g, function (c) { return c.toLowerCase(); });
  }

  function sameServiceIdentity(code, name, rowCode, rowName) {
    var bothCoded = !!(code && rowCode);
    if (bothCoded) return rowCode === code;
    return !!(name && rowName && rowName === name);
  }

  function serviceEffectiveUnit(svc) {
    var unit = Number(svc && svc.freeRate ? svc.freePrice : svc && svc.price) || 0;
    if (unit < 0) unit = 0;
    return Math.round(unit);
  }

  function sameServiceVariant(code, name, svc, rowCode, rowName, row) {
    return sameServiceIdentity(code, name, rowCode, rowName) &&
      !!svc.freeRate === !!row.freeRate && serviceEffectiveUnit(svc) === serviceEffectiveUnit(row);
  }

  function preferredServiceText(a, b) {
    a = String(a || ''); b = String(b || '');
    if (!a) return b; if (!b) return a;
    var ak = serviceKey(a), bk = serviceKey(b);
    if (ak < bk) return a; if (bk < ak) return b;
    return b < a ? b : a;
  }

  function compareServiceVariants(a, b) {
    var ac = serviceKey(a.code || ''), bc = serviceKey(b.code || '');
    var an = serviceKey(a.name || ''), bn = serviceKey(b.name || '');
    var ad = serviceKey(a.desc || ''), bd = serviceKey(b.desc || '');
    var ak = ac || an, bk = bc || bn;
    if (ak < bk) return -1; if (ak > bk) return 1;
    if (!!a.freeRate !== !!b.freeRate) return a.freeRate ? 1 : -1;
    var ap = serviceEffectiveUnit(a), bp = serviceEffectiveUnit(b);
    if (ap !== bp) return ap < bp ? -1 : 1;
    return ad < bd ? -1 : (ad > bd ? 1 : 0);
  }

  function sortServiceVariants() { state.services.sort(compareServiceVariants); }

  function addServiceRow(svc) {
    /* Merge repeated catalogue selections immediately. The server performs the
       same canonicalization again and remains authoritative for every amount. */
    var code = serviceKey(svc.code || '');
    var name = serviceKey(svc.name || '');
    var incomingQty = Number(svc.qty) || 1;
    if (incomingQty < 1) incomingQty = 1; else if (incomingQty > 999) incomingQty = 999;
    var i, row, rowCode, bothCoded;
    for (i = 0; i < state.services.length; i++) {
      row = state.services[i];
      rowCode = serviceKey(row.code || '');
      bothCoded = !!(code && rowCode);
      if (sameServiceVariant(code, name, svc, rowCode, serviceKey(row.name || ''), row)) {
        row.qty = Math.min(999, (Number(row.qty) || 1) + incomingQty);
        row.discount = Math.min(serviceEffectiveUnit(row) * row.qty,
          Math.max(0, Number(row.discount) || 0) + Math.max(0, Number(svc.discount) || 0));
        /* Match the server's deterministic promotion rule: a coded catalogue
           row replaces an uncoded fallback wholesale, while an uncoded row can
           never overwrite an already-coded service's authoritative metadata or
           price. Equal codes may refresh from the same catalogue identity. */
        if (!rowCode && code) {
          row.code = svc.code || '';
          row.name = svc.name || row.name;
          row.desc = svc.desc || '';
          row.category = svc.category || '';
          row.price = Number(svc.price) || 0;
        } else if (rowCode && !code) {
          /* Keep the coded/catalogue variant authoritative. */
        } else {
          row.code = preferredServiceText(row.code, svc.code);
          row.name = preferredServiceText(row.name, svc.name);
          row.desc = preferredServiceText(row.desc, svc.desc);
          row.category = preferredServiceText(row.category, svc.category);
          if (bothCoded) row.price = Number(svc.price) || 0;
        }
        sortServiceVariants();
        scheduleRender();
        return false;
      }
    }
    state.services.push({
      code: svc.code || '', name: svc.name || '', desc: svc.desc || '', category: svc.category || '',
      qty: incomingQty, price: Number(svc.price) || 0, discount: Number(svc.discount) || 0,
      freeRate: !!svc.freeRate, freePrice: Number(svc.freePrice) || 0
    });
    sortServiceVariants();
    scheduleRender();
    return true;
  }

  function renderServices() {
    var body = $('svcBody');
    if (!body) return;
    setText($('svcCount'), toFa(state.services.length) + ' ردیف');
    if (!state.services.length) {
      /* v1.78.0: designed empty state — a soft tinted panel with icon, title
         and hint instead of one plain line on a white sheet. */
      body.innerHTML =
        '<tr class="svc-empty-row"><td colspan="11" class="empty svc-empty-cell">' +
          '<div class="svc-empty">' +
            '<span class="se-ico"><svg viewBox="0 0 24 24" width="30" height="30">' +
              '<path fill="currentColor" d="M19 3h-4.2C14.4 1.8 13.3 1 12 1s-2.4.8-2.8 2H5a2 2 0 00-2 2v15a2 2 0 002 2h14a2 2 0 002-2V5a2 2 0 00-2-2zm-7 0a1 1 0 110 2 1 1 0 010-2zm-1.5 14.5l-3-3 1.4-1.4 1.6 1.6 4.6-4.6 1.4 1.4-6 6z"/>' +
            '</svg></span>' +
            '<b>هنوز خدمتی اضافه نشده است</b>' +
            '<span class="se-sub">از «جستجوی خدمت» بالای جدول استفاده کنید یا روی «افزودن خدمت +» بزنید</span>' +
          '</div>' +
        '</td></tr>';
      return;
    }
    var html = '', i, s;
    for (i = 0; i < state.services.length; i++) {
      s = computeRow(state.services[i]);
      html +=
        '<tr data-row="' + i + '">' +
        '<td>' + toFa(i + 1) + '</td>' +
        '<td class="td-name">' + esc(s.name || '') + '</td>' +
        '<td>' + toFa(s.code || '—') + '</td>' +
        '<td><input class="inp desc-inp" type="text" value="' + esc(s.desc || '') + '" data-desc="' + i + '"/></td>' +
        '<td><input class="inp qty-inp" type="text" value="' + toFa(s.qty) + '" data-i="' + i + '"/></td>' +
        '<td><label class="free-rate"><input type="checkbox" data-free="' + i + '"' + (s.freeRate ? ' checked="checked"' : '') + '/><span>آزاد</span></label></td>' +
        '<td><input class="inp free-inp" type="text" value="' + money(s.freeRate ? s.freePrice : s.price) + '" data-free-price="' + i + '"' + (s.freeRate ? '' : ' disabled="disabled"') + '/></td>' +
        '<td><input class="inp disc-inp" type="text" value="' + money(s._disc) + '" data-discount="' + i + '" inputmode="numeric"/></td>' +
        '<td>' + money(s._org + s._supp) + '</td>' +
        '<td>' + money(s._pat) + '</td>' +
        '<td>' +
          '<button type="button" class="act-btn act-edit" data-edit="' + i + '" title="ویرایش تعداد">✎</button>' +
          '<button type="button" class="act-btn act-del" data-del="' + i + '" title="حذف">✕</button>' +
        '</td>' +
        '</tr>';
    }
    body.innerHTML = html;
    /* NO handler binding here — everything is delegated (wired once). */
  }

  /* live-refresh the computed cells of the row that owns `inputEl` WITHOUT
     rebuilding the table (rebuilding would destroy the input mid-keystroke). */
  function refreshRowCells(inputEl, idx) {
    if (!(idx >= 0 && idx < state.services.length)) return;
    var s = computeRow(state.services[idx]);
    var tr = inputEl;
    while (tr && String(tr.tagName || '').toLowerCase() !== 'tr') tr = tr.parentNode;
    if (!tr || !tr.cells || tr.cells.length < 11) return;
    /* cell 7 contains the live discount INPUT; never replace it while the user
       is typing. Only refresh the computed, read-only insurance/patient cells. */
    setText(tr.cells[8], money(s._org + s._supp));
    setText(tr.cells[9], money(s._pat));
  }

  /* ==========================================================================
     PATIENT — fill without disturbing anything the operator is typing.
     ========================================================================== */
  function fillPatient(p, opts) {
    if (!p) return;
    opts = opts || {};
    state.patient = p;
    function setIf(id, v) {
      var el = $(id); if (!el) return;
      /* never blank a field we have no value for (avoids "jumping") */
      if (v == null || v === '') return;
      el.value = v;
    }
    setIf('nid', toFa(p.nid || ''));
    setIf('first', p.first || '');
    setIf('last', p.last || '');
    setIf('father', p.father || '');
    setIf('birth', toFa(p.birth || ''));
    setIf('mobile', toFa(p.mobile || ''));
    setIf('phone', toFa(p.phone || ''));   /* B2: تلفن ثابت now really arrives */
    setIf('addr', p.addr || '');           /* B2: آدرس now really arrives */
    /* §1.53.0 FIX: gender auto-select was unreliable because the <select> has no
       explicit value= attributes (options carry only text), and a stored gender
       with stray whitespace / different unicode form silently failed to match.
       Normalise to the canonical مرد/زن and pick the option by TEXT so it always
       lands on the right row regardless of how the value was stored. */
    if ($('gender')) {
      var g = trimStr((p.gender || '') + '');
      var gn = g;
      if (g.indexOf('زن') >= 0 || g === 'female' || g === 'f' || g === '0') gn = 'زن';
      else if (g.indexOf('مرد') >= 0 || g === 'male' || g === 'm' || g === '1') gn = 'مرد';
      var sel = $('gender');
      var matched = false;
      for (var gi = 0; gi < sel.options.length; gi++) {
        if (trimStr(sel.options[gi].text) === gn || trimStr(sel.options[gi].value) === gn) {
          sel.selectedIndex = gi; matched = true; break;
        }
      }
      if (!matched) sel.selectedIndex = 0;
    }
    var full = trimStr((p.first || '') + ' ' + (p.last || ''));
    setText($('pfName'), full || 'بیمار جدید');
    setText($('pfFile'), toFa(p.file || p.nid || '----'));
    setText($('profileStateText'), full ? 'اطلاعات بیمار کامل و آماده پذیرش است' : 'برای شروع، مشخصات بیمار را وارد کنید');

    /* B2: auto-select the supplementary insurance when we recall one */
    if (p.suppIdx != null && p.suppIdx >= 0 && $('insSupp') &&
        p.suppIdx < state.supp.length) {
      $('insSupp').selectedIndex = p.suppIdx;
    }

    /* auto-select the verified base insurance (index into INSURANCES[]) */
    if (p.insurances && p.insurances.length && $('insMain')) {
      var idx = p.insurances[0];
      if (idx >= 0 && idx < state.insurances.length) {
        $('insMain').selectedIndex = idx;
      }
    }
    /* B3: only ever turn hasIns ON when we have POSITIVE insurance data;
       NEVER auto-uncheck it — the operator drives that. */
    if (p.insurances && p.insurances.length > 0 && $('hasIns')) {
      if (p.insurances[0] > 0) $('hasIns').checked = true;
    }
    recompute();
  }

  /* ==========================================================================
     RESULT LISTS
     ========================================================================== */
  function renderPatientResults(rows) {
    var box = $('patResults');
    if (!box) return;
    state.patResults = rows || [];
    if (!rows || !rows.length) { box.innerHTML = '<div class="empty">نتیجه‌ای نیست</div>'; return; }
    var html = '', i, p, lim = Math.min(rows.length, 25);
    for (i = 0; i < lim; i++) {
      p = rows[i];
      html += '<div class="row" data-p="' + i + '"><span class="r-name">' +
        esc(trimStr((p.first || '') + ' ' + (p.last || ''))) +
        '</span><span class="r-sub">' + toFa(p.nid || '') + '</span></div>';
    }
    box.innerHTML = html;   /* delegated click — wired once in wire() */
  }

  function renderDocResults(rows) {
    var box = $('docResults');
    if (!box) return;
    state.doctors = rows || [];
    /* v1.72.0: the list starts EMPTY and only shows matches while the operator
       is typing — no «نتیجه‌ای نیست» placeholder. */
    if (!rows || !rows.length) { box.innerHTML = ''; return; }
    var html = '', i, doc, lim = Math.min(rows.length, 25);
    /* v1.71.0: each suggestion shows name + specialty + medical-council ID,
       so the operator can confirm the match before clicking. */
    for (i = 0; i < lim; i++) {
      doc = rows[i];
      html += '<div class="row" data-d="' + i + '">' +
        '<span class="r-name">' + esc(doc.name || '') + '</span>' +
        '<span class="r-sub">' + esc(doc.specialty || '') + '</span>' +
        (doc.code ? '<span class="r-code">' + esc(toFa(doc.code)) + '</span>' : '') +
        '</div>';
    }
    box.innerHTML = html;   /* delegated click — wired once in wire() */
  }
  function selectDoctor(doc, ix) {
    /* v1.71.0: the <select id="doc2name"> dropdown is gone — the doctor name is
       shown in a read-only display field that mirrors the suggestion pick, and
       the medical-council ID (#doc2code) is auto-filled from the same row. */
    if ($('doc2name')) $('doc2name').value = doc.name || '';
    if ($('doc2code') && doc.code) $('doc2code').value = toFa(doc.code);
    refreshDoctorStats(doc.name || '');
  }
  function refreshDoctorStats(name) {
    name = name || val('doc2name');
    if (!name) { updatePS({ P: 0, S: 0 }); return; }
    Bridge.call('doctor.stats', { name: name }).then(function (r) {
      updatePS(r || { P: 0, S: 0 });
    });
  }

  /* v1.94: doctor search auto-select — mirrors resolvePerfCode() for the
     performer: when the search yields exactly one row, OR an exact medicalId/
     code match is found, auto-select it via selectDoctor() instead of just
     listing suggestions. The matched row/index are captured into locals before
     defer() so the setTimeout closure never sees a stale loop variable. */
  function autoSelectDoctor(rows, en) {
    if (!rows || !rows.length) return;
    if (rows.length === 1) {
      var d0 = rows[0];
      defer(function () { selectDoctor(d0, 0); });
      return;
    }
    var matched = null, mIdx = -1, i;
    for (i = 0; i < rows.length; i++) {
      if (toEn(rows[i].medicalId || '') === en || toEn(rows[i].code || '') === en) {
        matched = rows[i]; mIdx = i; break;
      }
    }
    if (matched) defer(function () { selectDoctor(matched, mIdx); });
  }

  /* ---------------------------------------------------------------------- *
     v1.78.0 — «انجام دهنده» (performer) wiring.
     داستان: بیمار از سمت «پزشک معالج» می‌آید (ارجاع — جستجو با کد نظام پزشکی
     در میان همه پزشکان)، اما «انجام دهنده» کسی است که بیمار پیش او ویزیت
     می‌شود. این فهرست فقط پزشکانی را نشان می‌دهد که در مدیریت تیک «تعریف به
     عنوان انجام دهنده» دارند (پیش‌فرض برای همه فعال است).
     قبل از این نسخه کمبوی #perfname هرگز پر نمی‌شد — از صفر وصل شده است.
     ---------------------------------------------------------------------- */
  /* v1.93: fillPerformers accepts optional folded rows from the init response.
     When rows are supplied we render directly (no bridge round-trip); with no
     argument we fall back to the standalone 'doctor.performers' call. */
  function fillPerformers(rows) {
    var sel = $('perfname'); if (!sel) return;
    function applyRows(rs) {
      state.performers = rs || [];
      var h = '<option value="">— انتخاب —</option>', i;
      for (i = 0; i < state.performers.length; i++) {
        var nm = state.performers[i].name || '';
        var label = nm + (state.performers[i].specialty ? ' — ' + state.performers[i].specialty : '');
        h += '<option value="' + esc(nm) + '">' + esc(label) + '</option>';
      }
      sel.innerHTML = h;
    }
    if (rows) { applyRows(rows); return; }
    try {
      Bridge.call('doctor.performers', {}).then(function (r) {
        applyRows((r && r.rows) || []);
      });
    } catch (e) { /* ignore — combo stays empty */ }
  }
  /* picking a performer name → mirror its code into the numeric code box.
     Convention matches selectDoctor() on this same page (and the native form):
     the 1-based list code goes in the box (medicalId is only a fallback). */
  function mirrorPerfCode() {
    var nm = $('perfname') ? trimStr($('perfname').value) : '';
    var box = $('perfcode'); if (!box) return;
    if (!nm) { box.value = ''; return; }
    var rows = state.performers || [], i;
    for (i = 0; i < rows.length; i++) {
      if ((rows[i].name || '') === nm) {
        box.value = toFa(rows[i].code || rows[i].medicalId || '');
        return;
      }
    }
  }
  /* typing a code → resolve to the matching performer and select it.
     While TYPING we only snap on an EXACT کد نظام پزشکی / code match (so
     «142536» never jumps after the first digit); Enter may take the first
     (best) partial row. */
  var perfTimer = null;
  function resolvePerfCode(immediate) {
    var box = $('perfcode'); if (!box) return;
    var q = toEn(trimStr(box.value)).replace(/\s+/g, '');
    if (!q) return;
    var run = function () {
      Bridge.call('doctor.search', { q: q, code: q, role: 'performer' }).then(function (r) {
        var rows = (r && r.rows) || [];
        if (!rows.length) return;
        var d = null, i;
        if (immediate) d = rows[0];
        else {
          for (i = 0; i < rows.length; i++) {
            if (toEn(rows[i].medicalId || '') === q || toEn(rows[i].code || '') === q) { d = rows[i]; break; }
          }
          if (!d) return;             /* no exact match yet — keep typing */
        }
        var sel = $('perfname');
        if (sel) {
          for (i = 0; i < sel.options.length; i++) {
            if (sel.options[i].value === (d.name || '')) { sel.selectedIndex = i; break; }
          }
        }
        /* same convention as selectDoctor(): the 1-based list code fills the box */
        box.value = toFa(d.code || d.medicalId || q);
      });
    };
    if (immediate) { run(); return; }
    if (perfTimer) clearTimeout(perfTimer);
    perfTimer = setTimeout(run, 220);
  }

  function renderSvcSuggest(rows) {
    var box = $('svcSuggest');
    if (!box) return;
    state.catalog = rows || [];
    if (!rows || !rows.length) { box.className = 'svc-suggest'; box.innerHTML = ''; return; }
    var html = '', i, s, lim = Math.min(rows.length, 40);
    for (i = 0; i < lim; i++) {
      s = rows[i];
      /* §1.53.0: professional dropdown — code shown as a distinct monospaced badge
         on the right of the name, price on a separate aligned column with a label,
         so the eye can scan code / name / price as three clear columns. */
      html += '<div class="s-row" data-s="' + i + '">' +
        '<span class="s-code">' + toFa(s.code || '') + '</span>' +
        '<span class="s-name">' + esc(s.name || '') + '</span>' +
        '<span class="s-price"><i>' + money(s.price) + '</i> ریال</span></div>';
    }
    box.innerHTML = html;   /* delegated click — wired once in wire() */
    box.className = 'svc-suggest open';
  }

  /* ==========================================================================
     QUEUE — صندوق نرفته‌ها
     ========================================================================== */
  /* v1.75.0: the overlay header subtitle + summary card reflect the active
     queue kind so the surface reads as a deliberate page, not a bare table. */
  function updateQueueChrome() {
    var isAdm = state.queueKind === 'admission';
    setText($('qovSubtitle'),
      isAdm ? 'بیمارانی که در صف پذیرش منتظر دریافت نوبت هستند'
            : 'پذیرش‌های ثبت‌شده‌ای که هنوز به صندوق نرفته‌اند');
    setText($('qovSumLbl'),
      isAdm ? 'مورد در صف پذیرش' : 'مورد در صندوق نرفته‌ها');
    setText($('qovHint'),
      isAdm ? 'برای صدور نوبت یا حذف هر ردیف از دکمه‌های سمت چپ استفاده کنید'
            : 'برای بازخوانی یا حذف هر ردیف از دکمه‌های سمت چپ استفاده کنید');
  }
  function renderQueue(rows) {
    state.queue = rows || state.queue || [];
    var body = $('queueBody');
    if (!body) return;
    var filter = filterQueue(state.queue);
    state.queueView = filter;             /* v1.50.0: delegated handlers read this */
    var total = state.queue.length;
    setText($('qCount'), toFa(total));
    setText($('qCountSum'), toFa(total));
    updateQueueChrome();
    if (!filter.length) {
      body.innerHTML = '<tr><td colspan="7" class="empty q-empty">' +
        (state.queueKind === 'admission' ? 'موردی در صف پذیرش نیست' : 'موردی در صندوق نیست') + '</td></tr>';
      return;
    }
    var html = '', i, q, lim = Math.min(filter.length, 60);
    for (i = 0; i < lim; i++) {
      q = filter[i];
      var nm = trimStr(String(q.name || '—'));
      var pinit = toFa(nm.charAt(0) || '؟');
      var file = toFa(q.barcode || q.nid || '—');
      var mins = (q.minsAgo != null)
        ? '<span class="q-wait-pill">' + toFa(q.minsAgo) + ' دقیقه</span>'
        : '—';
      html += '<tr data-qi="' + i + '">' +
        '<td class="q-idx">' + toFa(q.turn || (i + 1)) + '</td>' +
        '<td class="q-pname"><span class="q-avatar">' + esc(pinit) + '</span>' +
          '<span class="q-pname-text">' + esc(nm) + '</span></td>' +
        '<td class="q-file"><span class="q-file-badge">' + esc(file) + '</span></td>' +
        '<td class="q-date">' + toFa(q.date || '—') + '</td>' +
        '<td class="q-time">' + toFa(q.time || '—') + '</td>' +
        '<td class="q-wait">' + mins + '</td>' +
        '<td class="q-act">' +
          '<button type="button" class="q-act-btn q-recall" title="بازخوانی" data-q="' + i + '">بازخوانی</button>' +
          '<button type="button" class="q-act-btn q-xfer" title="انتقال" data-qxfer="' + i + '">' +
            (state.queueKind === 'admission' ? 'به صندوق' : 'به صف') + '</button>' +
          (state.queueKind === 'unpaid'
            ? '<button type="button" class="q-act-btn q-pay" title="پرداخت" data-qpay="' + i + '">پرداخت</button>'
            : '') +
          '<button type="button" class="q-act-btn q-cancel" title="لغو" data-qcancel="' + i + '">لغو</button>' +
          '<button type="button" class="q-act-btn q-remove" title="حذف" data-qdel="' + i + '">×</button>' +
        '</td>' +
      '</tr>';
    }
    body.innerHTML = html;
    /* NO handler binding here — everything is delegated (wired once). */
  }
  function filterQueue(rows) {
    var q = $('qSearch') ? trimStr($('qSearch').value) : '';
    var qd = toEn(q);
    var mins = $('qMinutes') ? parseInt($('qMinutes').value, 10) || 0 : 0;
    var out = [], i, r;
    for (i = 0; i < rows.length; i++) {
      r = rows[i];
      if (q) {
        var name = String(r.name || ''), nid = toEn(String(r.nid || '')), bc = toEn(String(r.barcode || ''));
        if (name.indexOf(q) < 0 && nid.indexOf(qd) < 0 && bc.indexOf(qd) < 0) continue;
      }
      if (mins > 0 && r.minsAgo != null && r.minsAgo > mins) continue;
      out.push(r);
    }
    return out;
  }
  function refreshQueue() {
    Bridge.call('queue.list', { kind: state.queueKind, hours: 24 }).then(function (r) { renderQueue(r.rows || []); updateTurnPreview(); });
  }

  /* ==========================================================================
     NAVIGATION (B1) — Enter / Tab advance through an EXPLICIT hard-coded id
     order that matches the secretary's real visual/logical workflow, NOT the
     raw DOM order (wrappers rearrange cells under RTL, so DOM order is wrong).
     v1.75.0: the order now follows the on-screen grid field-by-field without
     skipping — نام (first) → نام خانوادگی (last) → کد ملی (nid) → تاریخ تولد →
     جنسیت → موبایل → … — exactly the order the eye travels down the form. When
     no field is focused (page open / form cleared) the cursor auto-lands on the
     national-ID (کد ملی) field so a patient can be looked up immediately.
     Auxiliary search boxes and the table qty cells are deliberately NOT here —
     they keep their own dedicated Enter handlers.
     ========================================================================== */
  var NAV_ORDER = [
    'first', 'last', 'nid',
    'birth', 'gender', 'mobile', 'father', 'phone', 'addr',
    'insMain', 'insType', 'ptype', 'insSuppPct',
    'doc2code', 'doc2name', 'perfcode', 'perfname',
    'apptDate', 'apptShift',
    'insBooklet', 'insValid', 'rxDate', 'insSupp'
  ];

  function isVisible(el) {
    if (!el) return false;
    if (el.disabled) return false;
    if (el.type === 'hidden') return false;
    /* offsetParent is null for display:none elements (fast IE-safe check) */
    if (el.offsetParent === null && el.offsetWidth === 0 && el.offsetHeight === 0) return false;
    return true;
  }

  /* Map NAV_ORDER ids to live, visible + enabled DOM elements (in order). */
  function navElements() {
    var arr = [], i, el;
    for (i = 0; i < NAV_ORDER.length; i++) {
      el = $(NAV_ORDER[i]);
      if (!el) continue;
      if (!isVisible(el)) continue;
      arr.push(el);
    }
    return arr;
  }

  /* Index of `cur` inside NAV_ORDER (not DOM order). -1 when not a nav field. */
  function navIndexOf(cur) {
    if (!cur || !cur.id) return -1;
    var i;
    for (i = 0; i < NAV_ORDER.length; i++) if (NAV_ORDER[i] === cur.id) return i;
    return -1;
  }

  function focusEl(n) {
    if (!n) return;
    try { n.focus(); } catch (e) {}
    if (n.tagName === 'INPUT') {
      /* n.select() works on Chromium/WebView2; on some MSHTML builds it fails
         silently, so fall back to an explicit selection range. */
      var selected = false;
      if (n.select) { try { n.select(); selected = true; } catch (e2) { selected = false; } }
      if (n.setSelectionRange) {
        try { n.setSelectionRange(0, (n.value || '').length); } catch (e3) {}
      }
    }
  }

  /* Advance to the next live nav element AFTER `cur` (no wrap-around). */
  function focusNext(cur) {
    var arr = navElements();
    var oi = navIndexOf(cur), i, ci = -1;
    for (i = 0; i < arr.length; i++) if (arr[i] === cur) { ci = i; break; }
    if (ci >= 0) { if (ci + 1 < arr.length) focusEl(arr[ci + 1]); return; }
    /* cur not currently visible in the live list — fall back to NAV_ORDER pos */
    if (oi >= 0) {
      for (i = oi + 1; i < NAV_ORDER.length; i++) {
        var el = $(NAV_ORDER[i]); if (el && isVisible(el)) { focusEl(el); return; }
      }
      return;
    }
    if (arr.length) focusEl(arr[0]);
  }

  /* Go back to the previous live nav element BEFORE `cur` (no wrap-around). */
  function focusPrev(cur) {
    var arr = navElements();
    var oi = navIndexOf(cur), i, ci = -1;
    for (i = 0; i < arr.length; i++) if (arr[i] === cur) { ci = i; break; }
    if (ci >= 0) { if (ci - 1 >= 0) focusEl(arr[ci - 1]); return; }
    if (oi >= 0) {
      for (i = oi - 1; i >= 0; i--) {
        var el = $(NAV_ORDER[i]); if (el && isVisible(el)) { focusEl(el); return; }
      }
    }
  }

  /* v1.75.0: when NO field is currently focused (page just opened, or the form
     was just cleared), land the cursor on the national-ID (کد ملی) field so the
     receptionist can look up a patient straight away. If an input/select/textarea
     already owns focus we leave it untouched. */
  function autoFocusNid() {
    var ae = document.activeElement;
    var tag = ae && ae.tagName ? String(ae.tagName).toUpperCase() : '';
    if (tag === 'INPUT' || tag === 'SELECT' || tag === 'TEXTAREA') return;
    var n = $('nid');
    if (!n) return;
    try { n.focus(); } catch (e) {}
    if (n.select) { try { n.select(); } catch (e2) {} }
    else if (n.setSelectionRange) {
      try { n.setSelectionRange(0, (n.value || '').length); } catch (e3) {}
    }
  }

  /* ==========================================================================
     BIRTH-DATE (and appt-date) auto-slash + Ctrl+A + clean replace
     ========================================================================== */
  function wireDateField(el) {
    if (!el) return;
    on(el, 'keydown', function (e) {
      e = e || window.event;
      var key = e.keyCode || e.which;
      /* Ctrl+A → select whole value (default anyway, but ensure it) */
      if ((e.ctrlKey || e.metaKey) && (key === 65)) {
        try { el.select(); } catch (er) {}
      }
    });
    on(el, 'input', function () { formatDate(el); });
    on(el, 'keyup', function (e) {
      e = e || window.event; var key = e.keyCode || e.which;
      if (key === 8 || key === 46) return; /* don't re-add slash on delete */
      formatDate(el);
    });
  }
  function formatDate(el) {
    var raw = toEn(el.value).replace(/[^0-9]/g, '');
    if (raw.length > 8) raw = raw.substr(0, 8);
    var out = raw;
    if (raw.length > 4) out = raw.substr(0, 4) + '/' + raw.substr(4, 2) + (raw.length > 6 ? '/' + raw.substr(6) : '');
    else if (raw.length > 0) out = raw;
    /* keep caret comfortable: only rewrite if changed */
    var fa = toFa(out);
    if (el.value !== fa) el.value = fa;
  }

  /* ==========================================================================
     CLOCK (top bar)
     ========================================================================== */
  var FA_DAYS = ['یکشنبه', 'دوشنبه', 'سه‌شنبه', 'چهارشنبه', 'پنجشنبه', 'جمعه', 'شنبه'];
  function tickClock() {
    var d = new Date();
    function p(n) { return (n < 10 ? '0' : '') + n; }
    setText($('tbClock'), toFa(p(d.getHours()) + ':' + p(d.getMinutes()) + ':' + p(d.getSeconds())));
    /* date label is refreshed from C++ (Jalali) on init; keep placeholder here */
  }

  /* ==========================================================================
     WIRING
     ========================================================================== */
  function wire() {
    wireZoneFocus();
    /* --- B1: canonical Enter/Tab/Shift+Tab/Ctrl+A handler wired by id over
       NAV_ORDER (NOT a CSS selector). Direct per-element binding is the
       engine-reliable path (event delegation is unreliable on WebView2/MSHTML).
       On <select>, Enter must advance focus and NEVER open the dropdown. --- */
    function fieldKeydown(el) {
      return function (e) {
        e = e || window.event;
        var key = e.keyCode || e.which;

        /* Ctrl+A / Cmd+A → select the entire value inside an INPUT */
        if ((e.ctrlKey || e.metaKey) && (key === 65 || key === 97)) {
          if (el.tagName === 'INPUT') {
            try { el.select(); } catch (er) {}
            if (e.preventDefault) e.preventDefault(); else e.returnValue = false;
            if (e.stopPropagation) e.stopPropagation();
            return false;
          }
        }

        /* Enter (13) → advance; nid triggers lookup FIRST then advances.
           Tab (9) with no Shift → advance (normalise across engines). */
        if (key === 13 || (key === 9 && !e.shiftKey)) {
          if (e.preventDefault) e.preventDefault(); else e.returnValue = false;
          if (e.stopPropagation) e.stopPropagation();
          if (el.id === 'nid' && key === 13) {
            lookupNid(el);  /* lookupNid calls focusNext(el) on completion */
            return false;
          }
          focusNext(el);
          return false;
        }

        /* Shift+Tab → go back */
        if (key === 9 && e.shiftKey) {
          if (e.preventDefault) e.preventDefault(); else e.returnValue = false;
          if (e.stopPropagation) e.stopPropagation();
          focusPrev(el);
          return false;
        }
      };
    }
    function selectOnFocus(el) {
      return function () { if (el.tagName === 'INPUT') { try { el.select(); } catch (e) {} } };
    }
    /* Bind by id iteration over NAV_ORDER (once, after DOM is ready). */
    var _ni, _el;
    for (_ni = 0; _ni < NAV_ORDER.length; _ni++) {
      _el = $(NAV_ORDER[_ni]);
      if (!_el) continue;
      (function (el) {
        on(el, 'keydown', fieldKeydown(el));
        if (el.tagName === 'INPUT') on(el, 'focus', selectOnFocus(el));
        /* <select>: also attach keypress + a guarded keyup so Enter advances
           reliably on MSHTML (where preventDefault on keydown is unreliable and
           the dropdown may open before the handler runs). Space still opens. */
        if (el.tagName === 'SELECT') {
          on(el, 'keypress', fieldKeydown(el));
          on(el, 'keyup', function (e) {
            e = e || window.event;
            var k = e.keyCode || e.which;
            if (k === 13 && el.__navHandled !== true) {
              el.__navHandled = true;
              focusNext(el);
              setTimeout(function () { el.__navHandled = false; }, 100);
            }
          });
        }
      })(_el);
    }

    /* national id via the quick-search box too */
    on($('qsNid'), 'keydown', function (e) {
      e = e || window.event; var key = e.keyCode || e.which;
      if (key === 13) { if (e.preventDefault) e.preventDefault(); doQuickNid(); }
    });
    on($('qsNidBtn'), 'click', doQuickNid);
    on($('qsFileBtn'), 'click', doPatFileSearch);
    on($('qsFile'), 'keydown', function (e) { e = e || window.event; if ((e.keyCode || e.which) === 13) { if (e.preventDefault) e.preventDefault(); doPatFileSearch(); } });

    /* birth-date + appt-date behaviours */
    var dates = document.querySelectorAll('[data-date]'), di;
    for (di = 0; di < dates.length; di++) wireDateField(dates[di]);

    /* v1.72.0: doctor search — the separate «جستجوی پزشک» (#docSearch) box is
       gone; the «کد نظام پزشکی» field (#doc2code) is now the single top search
       field and accepts BOTH a doctor name (text) and a medical-council code
       (numeric). Search is LIVE as the operator types (debounced): a numeric
       value queries by medical ID (code), anything else queries by name. Enter
       re-runs it. Picking a suggestion auto-fills #doc2code (medical ID) and
       #doc2name (display name) via selectDoctor(). */
    on($('doc2code'), 'keydown', function (e) { e = e || window.event; if ((e.keyCode || e.which) === 13) { if (e.preventDefault) e.preventDefault(); doDocSearch(); } });

    var docTimer = null;
    function docLiveSearch() {
      if (docTimer) clearTimeout(docTimer);
      var q = $('doc2code') ? trimStr($('doc2code').value) : '';
      if (!q) { renderDocResults([]); return; }
      var en = toEn(q).replace(/\s+/g, '');
      var byCode = /^[0-9]+$/.test(en);          /* numeric → medical ID search */
      docTimer = setTimeout(function () {
        var params = byCode ? { q: en, code: en } : { q: q };
        Bridge.call('doctor.search', params).then(function (r) {
          var rows = r.rows || r.doctors || [];
          renderDocResults(rows);
          /* v1.94: auto-select single result / exact match (like performer) */
          autoSelectDoctor(rows, en);
        });
      }, 180);
    }
    on($('doc2code'), 'input', docLiveSearch);
    on($('doc2code'), 'keyup', docLiveSearch);

    /* v1.78.0: «انجام دهنده» — the combo lists ONLY performer-flagged doctors
       (filled by fillPerformers at boot / after clear). Name→code mirroring on
       selection; code→name resolution on Enter / typing (debounced). */
    on($('perfname'), 'change', function () { mirrorPerfCode(); });
    on($('perfcode'), 'keydown', function (e) {
      e = e || window.event;
      if ((e.keyCode || e.which) === 13) { if (e.preventDefault) e.preventDefault(); resolvePerfCode(true); }
    });
    on($('perfcode'), 'input', function () { resolvePerfCode(false); });

    /* service live search */
    var svcTimer = null;
    function svcSearch() {
      if (svcTimer) clearTimeout(svcTimer);
      var q = $('svcSearch') ? trimStr($('svcSearch').value) : '';
      if (!q) { renderSvcSuggest([]); return; }
      svcTimer = setTimeout(function () {
        Bridge.call('service.search', { q: q }).then(function (r) { renderSvcSuggest(r.rows || r.services || []); });
      }, 180);
    }
    on($('svcSearch'), 'input', svcSearch);
    on($('svcSearch'), 'keyup', svcSearch);
    /* --------------------------------------------------------------------
       v1.49.0 — ROBUST "enter a service code → pick the service" handler.
       When the operator types a value and confirms it (Enter or the
       «افزودن خدمت» button) we resolve it against the LIVE Management catalog
       in two stages, so an exact SERVICE CODE always wins over a fuzzy name
       match, and multiple services can be added one after another:

         1) service.resolve  — exact code lookup (SRV0001, 1001, …). This is
            the fast, unambiguous path when the operator knows the code.
         2) service.search   — free-text fallback (name / partial code). If it
            returns exactly one hit we add it; if several, we open the picker.

       Every path is guarded by _svcSearchInFlight so a burst of Enter presses
       can never stack overlapping bridge calls (which is what used to make the
       surface feel "stuck"). The bridge itself has a 15s timeout, so even a
       lost response can never wedge the box — the flag is always cleared in
       both then() and catch().
       -------------------------------------------------------------------- */
    var _svcSearchInFlight = false;   /* B4: debounce Enter/click re-entry */

    function clearSvcBox() {
      renderSvcSuggest([]);
      if ($('svcSearch')) $('svcSearch').value = '';
    }

    /* Resolve `q` and either add the matched service or show the picker.
       `preferPicker` (used by the button) shows the list when the query is
       ambiguous instead of silently adding the first hit. */
    function resolveAndAddService(q, preferPicker) {
      q = trimStr(q || '');
      if (!q) return;
      if (_svcSearchInFlight) return;
      _svcSearchInFlight = true;

      function done() { _svcSearchInFlight = false; }

      /* stage 1: exact service-code lookup */
      Bridge.call('service.resolve', { code: q }).then(function (r) {
        if (r && r.found && r.service) {
          addServiceRow(r.service);          /* price comes from the catalog */
          clearSvcBox();
          toast('خدمت «' + (r.service.name || '') + '» افزوده شد', 'ok');
          done();
          return;
        }
        /* stage 2: free-text search fallback */
        Bridge.call('service.search', { q: q }).then(function (sr) {
          done();
          var rows = (sr && (sr.rows || sr.services)) || [];
          state.catalog = rows;
          if (!rows.length) {
            renderSvcSuggest([]);
            toast('خدمتی با این عبارت یافت نشد', 'err');
            return;
          }
          if (rows.length === 1 || !preferPicker) {
            addServiceRow(rows[0]);
            clearSvcBox();
            toast('خدمت «' + (rows[0].name || '') + '» افزوده شد', 'ok');
          } else {
            renderSvcSuggest(rows);          /* several matches → let user pick */
          }
        })['catch'](function () { done(); });
      })['catch'](function () {
        /* resolve verb unavailable/failed → degrade to pure search */
        Bridge.call('service.search', { q: q }).then(function (sr) {
          done();
          var rows = (sr && (sr.rows || sr.services)) || [];
          state.catalog = rows;
          if (rows.length) {
            if (rows.length === 1 || !preferPicker) {
              addServiceRow(rows[0]); clearSvcBox();
              toast('خدمت «' + (rows[0].name || '') + '» افزوده شد', 'ok');
            } else { renderSvcSuggest(rows); }
          } else {
            renderSvcSuggest([]);
            toast('خدمتی با این عبارت یافت نشد', 'err');
          }
        })['catch'](function () { done(); });
      });
    }

    /* Enter confirms the current service selection. */
    on($('svcSearch'), 'keydown', function (e) {
      e = e || window.event;
      if ((e.keyCode || e.which) !== 13) return;
      if (e.preventDefault) e.preventDefault(); else e.returnValue = false;
      resolveAndAddService($('svcSearch') ? $('svcSearch').value : '', false);
      return false;
    });
    /* «افزودن خدمت» button: prefer showing the picker when ambiguous. */
    on($('svcAddBtn'), 'click', function () {
      resolveAndAddService($('svcSearch') ? $('svcSearch').value : '', true);
    });

    /* ====================================================================
       v1.50.0 — DELEGATED list/table handlers (bound ONCE, never rebound).
       Root cause of the whole-app freeze: the old per-node onclick handlers
       synchronously rebuilt (innerHTML) the very subtree that was still
       dispatching the click. In-process MSHTML wedges on that, and since it
       shares the app's UI thread, the ENTIRE program froze. Delegation +
       defer() guarantees the DOM is only mutated after dispatch unwinds.
       ==================================================================== */

    /* service suggestion dropdown → pick a service (deferred) */
    on($('svcSuggest'), 'click', function (e) {
      e = e || window.event;
      var box = $('svcSuggest');
      var row = findUp(e.target || e.srcElement, 'data-s', box);
      if (!row) return;
      var s = state.catalog[+row.getAttribute('data-s')];
      if (!s) return;
      if (e.preventDefault) e.preventDefault(); else e.returnValue = false;
      defer(function () {
        addServiceRow(s);                  /* schedules its own render */
        box.className = 'svc-suggest'; box.innerHTML = '';
        if ($('svcSearch')) $('svcSearch').value = '';
        toast('خدمت «' + (s.name || '') + '» افزوده شد', 'ok');
      });
    });

    /* services table → delete / edit buttons (delegated) */
    on($('svcBody'), 'click', function (e) {
      e = e || window.event;
      var body = $('svcBody');
      var tgt = e.target || e.srcElement;
      var del = findUp(tgt, 'data-del', body);
      if (del) {
        var di = +del.getAttribute('data-del');
        defer(function () {
          if (di >= 0 && di < state.services.length) state.services.splice(di, 1);
          scheduleRender();
        });
        return;
      }
      var ed = findUp(tgt, 'data-edit', body);
      if (ed) {
        var ei = +ed.getAttribute('data-edit');
        defer(function () {
          var inp = body.getElementsByClassName('qty-inp')[ei];
          if (inp) { try { inp.focus(); inp.select(); } catch (er) {} }
        });
      }
    });

    /* services table → qty edits (delegated). While typing we only refresh the
       computed cells of THAT row (never rebuild the table under the caret);
       the full re-render happens on blur/change. */
    function onQtyEvent(e) {
      e = e || window.event;
      var tgt = e.target || e.srcElement;
      if (!tgt || !/(^|\s)qty-inp(\s|$)/.test(tgt.className || '')) return;
      var idx = +tgt.getAttribute('data-i');
      if (!(idx >= 0 && idx < state.services.length)) return;
      var q = Math.max(1, parseInt(toEn(tgt.value), 10) || 1);
      state.services[idx].qty = q;
      refreshRowCells(tgt, idx);
      recompute();                          /* totals only — no DOM teardown */
    }
    on($('svcBody'), 'input', onQtyEvent);
    on($('svcBody'), 'keyup', onQtyEvent);
    on($('svcBody'), 'change', function (e) {
      e = e || window.event;
      var tgt = e.target || e.srcElement;
      if (!tgt || !/(^|\s)qty-inp(\s|$)/.test(tgt.className || '')) return;
      onQtyEvent(e);
      scheduleRender();                     /* safe: deferred out of dispatch */
    });
    on($('svcBody'), 'keydown', function (e) {
      e = e || window.event;
      var tgt = e.target || e.srcElement;
      if (!tgt || !/(^|\s)qty-inp(\s|$)/.test(tgt.className || '')) return;
      var k = e.keyCode || e.which;
      if (k === 13) {                       /* Enter in qty → back to search */
        if (e.preventDefault) e.preventDefault(); else e.returnValue = false;
        defer(function () {
          scheduleRender();
          var s = $('svcSearch');
          if (s) { try { s.focus(); if (s.select) s.select(); } catch (er) {} }
        });
        return false;
      }
    });
    on($('svcBody'), 'dblclick', function (e) {
      e = e || window.event;
      var tgt = e.target || e.srcElement;
      if (!tgt || !/(^|\s)qty-inp(\s|$)/.test(tgt.className || '')) return;
      try { tgt.focus(); tgt.select(); } catch (er) {}
    });
    /* Description, per-service discount and optional free-rate amount are
       explicit. The normal catalog rate stays read-only until «نرخ آزاد» is
       checked. Discount is bounded by C++ authoritatively and mirrored here for
       immediate feedback without rebuilding the row under the caret. */
    on($('svcBody'), 'input', function (e) {
      e = e || window.event; var t = e.target || e.srcElement, i, row;
      if (!t || !t.getAttribute) return;
      if (t.getAttribute('data-desc') != null) {
        i = +t.getAttribute('data-desc'); if (state.services[i]) state.services[i].desc = t.value;
      } else if (t.getAttribute('data-free-price') != null) {
        i = +t.getAttribute('data-free-price');
        if (state.services[i]) {
          state.services[i].freePrice = Math.max(0, parseInt(toEn(t.value).replace(/[^0-9]/g, ''), 10) || 0);
          refreshRowCells(t, i); recompute();
        }
      } else if (t.getAttribute('data-discount') != null) {
        i = +t.getAttribute('data-discount'); row = state.services[i];
        if (row) {
          var maxDiscount = (row.freeRate ? (Number(row.freePrice) || 0) : (Number(row.price) || 0)) * (row.qty || 1);
          row.discount = Math.min(maxDiscount, Math.max(0, parseInt(toEn(t.value).replace(/[^0-9]/g, ''), 10) || 0));
          refreshRowCells(t, i); recompute();
        }
      }
    });
    on($('svcBody'), 'change', function (e) {
      e = e || window.event; var t = e.target || e.srcElement;
      if (t && t.getAttribute && t.getAttribute('data-discount') != null) scheduleRender();
    });
    on($('svcBody'), 'change', function (e) {
      e = e || window.event; var t = e.target || e.srcElement;
      if (!t || !t.getAttribute || t.getAttribute('data-free') == null) return;
      var i = +t.getAttribute('data-free');
      if (state.services[i]) { state.services[i].freeRate = !!t.checked; if (!state.services[i].freePrice) state.services[i].freePrice = state.services[i].price; scheduleRender(); }
    });

    /* patient results list → load patient (delegated, deferred) */
    on($('patResults'), 'click', function (e) {
      e = e || window.event;
      var box = $('patResults');
      var row = findUp(e.target || e.srcElement, 'data-p', box);
      if (!row) return;
      var p = (state.patResults || [])[+row.getAttribute('data-p')];
      if (!p) return;
      defer(function () {
        fillPatient(p);
        box.innerHTML = '<div class="empty">نتیجه‌ای نیست</div>';
        toast('اطلاعات بیمار بارگذاری شد', 'ok');
      });
    });

    /* doctor results list → select doctor (delegated, deferred) */
    on($('docResults'), 'click', function (e) {
      e = e || window.event;
      var box = $('docResults');
      var row = findUp(e.target || e.srcElement, 'data-d', box);
      if (!row) return;
      var ix = +row.getAttribute('data-d');
      var doc = (state.doctors || [])[ix];
      if (!doc) return;
      defer(function () { selectDoctor(doc, ix); });
    });

    /* queue table → recall / remove (delegated, deferred) */
    on($('queueBody'), 'click', function (e) {
      e = e || window.event;
      var body = $('queueBody');
      var tgt = e.target || e.srcElement;
      var rep = findUp(tgt, 'data-q', body);
      if (rep) {
        var q = (state.queueView || [])[+rep.getAttribute('data-q')];
        if (q) defer(function () { queueRecall(q); });
        return;
      }
      var del = findUp(tgt, 'data-qdel', body);
      if (del) {
        var qd = (state.queueView || [])[+del.getAttribute('data-qdel')];
        if (qd) defer(function () {
          cashAsk('این ردیف حذف شود؟', function () {
            Bridge.call('queue.remove', { id: qd.id, kind: state.queueKind }).then(function () { refreshQueue(); });
          });
        });
        return;
      }
      var xf = findUp(tgt, 'data-qxfer', body);
      if (xf) {
        var qx = (state.queueView || [])[+xf.getAttribute('data-qxfer')];
        if (qx) defer(function () { queueTransfer(qx); });
        return;
      }
      var qp = findUp(tgt, 'data-qpay', body);
      if (qp) {
        var qpay = (state.queueView || [])[+qp.getAttribute('data-qpay')];
        if (qpay) defer(function () { queuePay(qpay); });
        return;
      }
      var qc = findUp(tgt, 'data-qcancel', body);
      if (qc) {
        var qcan = (state.queueView || [])[+qc.getAttribute('data-qcancel')];
        if (qcan) defer(function () { queueCancel(qcan); });
      }
    });
    on($('queueBody'), 'dblclick', function (e) {
      e = e || window.event;
      var body = $('queueBody');
      var tgt = e.target || e.srcElement;
      if (findUp(tgt, 'data-qdel', body) || findUp(tgt, 'data-qxfer', body) ||
          findUp(tgt, 'data-qpay', body) || findUp(tgt, 'data-qcancel', body)) return;
      var tr = findUp(tgt, 'data-qi', body);
      if (!tr) return;
      var qrow = (state.queueView || [])[+tr.getAttribute('data-qi')];
      if (!qrow) return;
      queueRecall(qrow);
    });

    /* insurance changes → recompute (never blanks patient fields) */
    on($('insMain'), 'change', function () { scheduleRender(); });
    on($('insSupp'), 'change', function () { scheduleRender(); });
    on($('doc2name'), 'change', function () { refreshDoctorStats(val('doc2name')); });
    on($('insSuppPct'), 'input', function () { scheduleRender(); });
    on($('insSuppPct'), 'keyup', function () { scheduleRender(); });
    on($('hasIns'), 'change', function () { scheduleRender(); });
    on($('noPay'), 'change', function () { recompute(); });

    /* collapse / expand lists */
    on($('svcToggle'), 'click', function () { toggleWrap('svcTblWrap', this); });
    on($('queueToggle'), 'click', function () { toggleWrap('queueWrap', this); });
    on($('invoiceToggle'), 'click', function () {
      var c = $('invoiceCard'); if (!c) return;
      c.className = /collapsed/.test(c.className) ? c.className.replace(/\s*collapsed/, '') : c.className + ' collapsed';
    });
    on($('queueLauncher'), 'click', function () {
      /* v1.85: the queue opens as its own native C++ tab, not an overlay. */
      if (state.surface === 'queue') { refreshQueue(); return; }
      Bridge.call('ui.openTab', { kind: 'queue' });
    });
    on($('queueClose'),    'click', function () {
      if (state.surface === 'queue') { Bridge.call('ui.closeTab', { kind: 'queue' }); return; }
      closeQueuePanel();
    });
    on($('toolsBtn'), 'click', function () { Bridge.call('ui.openTab', { kind: 'tools' }); });
    on($('toolsBack'), 'click', function () {
      Bridge.call('ui.closeTab', { kind: 'tools' });
    });
    /* v1.88: «جستجوی قبض» opens in its own native C++ tab (like صندوق), so the
       tools tab stays alive underneath with its state untouched. */
    on($('toolsReceipts'), 'click', function () {
      Bridge.call('ui.openTab', { kind: 'receipts' });
    });
    on($('toolsCash'), 'click', function () {
      if (!state.canCashView) { toast('دسترسی صندوق ندارید', 'err'); return; }
      Bridge.call('ui.openTab', { kind: 'cashier' });
    });
    on($('toolsQueue'), 'click', function () {
      Bridge.call('ui.openTab', { kind: 'queue' });
    });
    /* v1.87: tools hamburger drawer (categorised, searchable) + grid filter. */
    (function () {
      var drawer = $('toolsDrawer'), bk = $('toolsDrawerBk');
      function openDrawer() {
        if (drawer) drawer.className = 'tools-drawer open';
        if (bk) bk.className = 'tools-drawer-bk open';
      }
      function closeDrawer() {
        if (drawer) drawer.className = 'tools-drawer';
        if (bk) bk.className = 'tools-drawer-bk';
      }
      on($('toolsBurger'), 'click', function () {
        if (drawer && /(^|\s)open(\s|$)/.test(drawer.className)) closeDrawer();
        else openDrawer();
      });
      on($('toolsDrawerClose'), 'click', closeDrawer);
      on($('toolsDrawerBk'), 'click', closeDrawer);
      /* drawer item → activate the matching grid tile */
      var body = $('toolsDrawerBody');
      if (body) {
        var items = body.getElementsByClassName('tools-drawer-item');
        var ii;
        for (ii = 0; ii < items.length; ii++) {
          (function (it) {
            on(it, 'click', function () {
              var t = $(it.getAttribute('data-tool'));
              closeDrawer();
              if (t) t.click();
            });
          })(items[ii]);
        }
      }
      /* search — filters BOTH the grid tiles and the drawer items by their
         visible text (name + subtitle). */
      function norm(s) { return (s || '').replace(/\s+/g, ' ').toLowerCase(); }
      function applyToolsFilter(q) {
        q = norm(q);
        var grid = $('toolsGrid');
        var any = false;
        var i, tiles, txt;
        if (grid) {
          tiles = grid.getElementsByClassName('tools-tile');
          for (i = 0; i < tiles.length; i++) {
            txt = norm(tiles[i].textContent || tiles[i].innerText);
            var hide = q && txt.indexOf(q) < 0;
            tiles[i].style.display = hide ? 'none' : '';
            if (!hide) any = true;
          }
        }
        var emp = $('toolsEmpty');
        if (emp) emp.style.display = (q && !any) ? '' : 'none';
        if (body) {
          var dis = body.getElementsByClassName('tools-drawer-item');
          for (i = 0; i < dis.length; i++) {
            txt = norm(dis[i].textContent || dis[i].innerText);
            dis[i].style.display = (q && txt.indexOf(q) < 0) ? 'none' : '';
          }
          /* hide a category heading when every item under it is filtered out */
          var kids = body.childNodes, lastCat = null, lastVis = false;
          for (i = 0; i < kids.length; i++) {
            var k = kids[i];
            if (!k.className) continue;
            if (/(^|\s)tools-cat(\s|$)/.test(k.className)) {
              if (lastCat) lastCat.style.display = lastVis ? '' : 'none';
              lastCat = k; lastVis = false;
            } else if (/(^|\s)tools-drawer-item(\s|$)/.test(k.className)) {
              if (k.style.display !== 'none') lastVis = true;
            }
          }
          if (lastCat) lastCat.style.display = lastVis ? '' : 'none';
        }
      }
      on($('toolsQ'), 'keyup', function () { applyToolsFilter(this.value); });
      on($('toolsQ'), 'input', function () { applyToolsFilter(this.value); });
      on($('toolsDrawerQ'), 'keyup', function () { applyToolsFilter(this.value); });
      on($('toolsDrawerQ'), 'input', function () { applyToolsFilter(this.value); });
    })();
    on($('rcBack'), 'click', function () {
      if (state.surface === 'receipts') {
        Bridge.call('ui.closeTab', { kind: 'receipts' });   /* back to tools tab */
        return;
      }
      state.rcPage = 'home'; showToolsHome();
    });
    on($('rcSearchBtn'), 'click', searchReceipts);
    on($('rcExcel'), 'click', exportReceiptExcel);
    on($('rcPrint'), 'click', printSelectedReceipt);
    on($('rcDelete'), 'click', deleteSelectedReceipts);
    on($('rcPrev'), 'click', function () { if (state.rcPageNo > 1) { state.rcPageNo -= 1; renderReceipts(); } });
    on($('rcNext'), 'click', function () {
      var pages = receiptPageCount();
      if (state.rcPageNo < pages) { state.rcPageNo += 1; renderReceipts(); }
    });
    on($('rcAll'), 'click', function () {
      var on = this.checked, body = $('rcBody'); if (!body) return;
      if (!state.rcChecked) state.rcChecked = {};
      var boxes = body.getElementsByTagName('input'), i, id;
      for (i = 0; i < boxes.length; i++) {
        if (boxes[i].type !== 'checkbox') continue;
        boxes[i].checked = on;
        id = boxes[i].getAttribute('data-rchk');
        if (!id) continue;
        if (on) state.rcChecked[id] = true;
        else delete state.rcChecked[id];
      }
    });
    on($('rcBody'), 'click', function (e) {
      e = e || window.event;
      var tgt = e.target || e.srcElement;
      if (tgt && (tgt.tagName || '').toLowerCase() === 'input' && tgt.type === 'checkbox') {
        var cid = tgt.getAttribute('data-rchk');
        if (!state.rcChecked) state.rcChecked = {};
        if (cid) {
          if (tgt.checked) state.rcChecked[cid] = true;
          else delete state.rcChecked[cid];
        }
        return;
      }
      var tr = findUp(tgt, 'data-rid', $('rcBody'));
      if (!tr) return;
      state.rcSel = tr.getAttribute('data-rid');
      var rows = $('rcBody').getElementsByTagName('tr'), i, r, cls;
      for (i = 0; i < rows.length; i++) {
        r = rows[i];
        cls = String(r.className || '').replace(/\s*rc-sel/g, '');
        if (r.getAttribute('data-rid') === state.rcSel) cls += ' rc-sel';
        r.className = cls;
      }
    });
    on($('rcBody'), 'dblclick', function (e) {
      e = e || window.event;
      var tr = findUp(e.target || e.srcElement, 'data-rid', $('rcBody'));
      if (tr) openReceiptOnAdmission(tr.getAttribute('data-rid'));
    });
    on($('cashClose'),    'click', function () {
      if (state.surface === 'cashier') Bridge.call('ui.closeTab', { kind: 'cashier' });
    });
    on($('cashSearch'), 'keyup', function (e) {
      e = e || window.event;
      state.cashQ = this.value;
      var key = e.keyCode || e.which;
      if (key === 13) { lookupCashSearch(this.value); return; }
      refreshCash();
    });
    on($('cashStatusStrip'), 'click', function (e) {
      e = e || window.event;
      var btn = findUp(e.target || e.srcElement, 'data-status', $('cashStatusStrip'));
      if (!btn) return;
      var st = btn.getAttribute('data-status') || '';
      state.cashStatus = (state.cashStatus === st) ? '' : st;
      refreshCash();
    });
    on($('cashShiftStart'), 'click', function () {
      cashCall('شیفت صندوق شروع شود؟', 'cashier.shift.start', {}, 'شیفت شروع شد', 'شروع شیفت ناموفق بود');
    });
    on($('cashShiftEnd'), 'click', function () {
      cashCall('پایان شیفت ثبت شود؟', 'cashier.shift.end', {}, 'شیفت بسته شد', 'پایان شیفت ناموفق بود');
    });
    on($('cashManualBtn'), 'click', function () {
      if (!state.canCashEdit) { toast('دسترسی تغییر صندوق ندارید', 'err'); return; }
      var box = $('cashManualBox');
      if (box) box.style.display = box.style.display === 'none' ? 'block' : 'none';
    });
    on($('cmCancel'), 'click', function () {
      var box = $('cashManualBox'); if (box) box.style.display = 'none';
    });
    on($('cmSave'), 'click', function () {
      if (!state.canCashEdit) { toast('دسترسی تغییر صندوق ندارید', 'err'); return; }
      var amt = Number(toEn($('cmAmt') ? $('cmAmt').value : '').replace(/,/g, '')) || 0;
      cashCall('سند دستی به مبلغ ' + money(amt) + ' ریال ثبت شود؟', 'cashier.manual', {
        nid: toEn($('cmNid') ? $('cmNid').value : ''),
        first: $('cmFirst') ? $('cmFirst').value : '',
        last: $('cmLast') ? $('cmLast').value : '',
        doctor: $('cmDoc') ? $('cmDoc').value : '',
        amount: amt
      }, 'سند دستی ثبت شد', 'ثبت سند ناموفق بود', function () {
        var box = $('cashManualBox'); if (box) box.style.display = 'none';
      });
    });
    on($('cashTabs'), 'click', function (e) {
      e = e || window.event;
      var tgt = e.target || e.srcElement;
      var btn = findUp(tgt, 'data-tab', $('cashTabs'));
      if (!btn) return;
      state.cashTab = +btn.getAttribute('data-tab') || 0;
      state.cashStatus = '';
      refreshCash();
    });
    on($('cashBody'), 'dblclick', function (e) {
      e = e || window.event;
      var tgt = e.target || e.srcElement;
      var tr = findUp(tgt, 'data-cid', $('cashBody'));
      if (!tr) return;
      var paid = tr.getAttribute('data-paid') === '1';
      if (paid) { toast('این ردیف قبلاً صندوق شده است', 'info'); return; }
      openReceiptOnAdmission(tr.getAttribute('data-cid'));
    });
    on($('cashBody'), 'click', function (e) {
      e = e || window.event;
      var tgt = e.target || e.srcElement;
      var pay = findUp(tgt, 'data-pay', $('cashBody'));
      if (!pay) return;
      if (!state.canCashEdit) { toast('دسترسی تغییر صندوق ندارید', 'err'); return; }
      var id = pay.getAttribute('data-pay');
      openPayMini(cashRowById(id) || { id: id });
    });
    /* v1.79.0: the «صندوق نرفته‌ها» / «صف پذیرش» nav buttons were removed from
       the action card (they duplicated the launcher). The handlers stay out —
       the consolidated «queueLauncher» (now centred on the right rail) opens
       the same overlay. */
    /* clicking the dim backdrop dismisses the full-screen overlay, like a modal. */
    on($('queueBackdrop'),  'click', function () { closeQueuePanel(); });
    /* v1.65.0: the draggable mini-page (and its wireDrag/queueDrag plumbing)
       is fully retired — the queue is a FULL-SCREEN overlay since v1.64.0. */
    /* v1.60.0: zoom in/out buttons REMOVED — fixed, readable scale. */

    /* v1.60.0: print cluster moved OUT of the native bottom bar INTO the page. */
    on($('btnPrtLast'), 'click', function () { Bridge.call('print.last', {}); });
    on($('btnPrtRx'),  'click', function () { Bridge.call('print.rx', collectRecord()); });
    on($('btnPrtIns'), 'click', function () { Bridge.call('print.insurance', collectRecord()); });

    /* queue search / filter / add */
    on($('qSearch'), 'input', function () { renderQueue(state.queue); });
    on($('qSearch'), 'keyup', function () { renderQueue(state.queue); });
    on($('qMinutes'), 'change', function () { renderQueue(state.queue); });
    on($('tabQueue'), 'click', function () {
      state.queueKind = 'unpaid'; setActiveTab('tabQueue'); refreshQueue();
    });
    on($('tabAdmQ'), 'click', function () {
      state.queueKind = 'admission'; setActiveTab('tabAdmQ'); refreshQueue();
    });

    /* v1.69.0: two distinct queue/unpaid action buttons (replacing the old
       single context-dependent addToQueue + payment status card). */
    on($('addToQueueBtn'), 'click', function () {
      state.queueKind = 'unpaid'; addCurrentToQueue();
    });
    on($('addToAdmQBtn'), 'click', function () {
      state.queueKind = 'admission'; addCurrentToQueue();
    });

    /* save / clear / new — v1.64.0: «پذیرش جدید» and «انصراف» were removed;
       «پاک کردن» empties every field via clearForm(). */
    on($('btnSave'), 'click', saveAdmission);
    on($('hdrNewAdm'), 'click', function () { clearForm(); toast('پذیرش جدید', 'ok'); });
    on($('hdrNew'), 'click', function () { clearForm(); toast('پذیرش جدید', 'ok'); });
    on($('btnClear'), 'click', function () { clearForm(); Bridge.call('admission.clear', {}); });

    /* Printing is intentionally native-only; F8 remains a convenience shortcut. */
    on($('hdrSettings'), 'click', function () { Bridge.call('ui.settings', {}); });
    on($('btnErx'), 'click', function () { Bridge.call('rx.electronic', collectRecord()); });

    /* F8 = print last; F7 = cashier; F4 = unlock locked form; F5 = toggle edit
       mode; Ctrl+Enter = save. */
    on(document, 'keydown', function (e) {
      e = e || window.event; var key = e.keyCode || e.which;
      if (key === 118) { /* F7 */
        if (e.preventDefault) e.preventDefault(); else e.returnValue = false;
        if (state.surface !== 'cashier' && state.canCashView)
          Bridge.call('ui.openTab', { kind: 'cashier' });
      } else if (key === 113) { /* F2 */
        if (e.preventDefault) e.preventDefault(); else e.returnValue = false;
        newPatient();
      } else if (key === 115) { /* F4 */
        if (e.preventDefault) e.preventDefault(); else e.returnValue = false;
        unlockCashForm();
      } else if (key === 116) { /* F5 — toggle admission edit mode (req. v1.94) */
        /* preventDefault so F5 never refreshes the page; only act on the
           admission surface (edit mode is an admission-form concept). */
        if (state.surface === 'admission' || state.loadedTicketId) {
          if (e.preventDefault) e.preventDefault(); else e.returnValue = false;
          toggleEditMode();
        }
      } else if (key === 119) { Bridge.call('print.last', {}); }        /* F8 */
      else if (key === 13 && e.ctrlKey) {
        if (e.preventDefault) e.preventDefault(); else e.returnValue = false;
        saveAdmission();
      }
    });

    /* v1.70.0: Ctrl+scroll = zoom in/out. Persists the chosen zoom per-user
       so it survives app restarts. v1.77.0: default lowered 90% → 80%, and the
       unzoom floor lowered 80 → 50 so the user can zoom OUT to fit all items;
       applyZoom() adds a small font bump below 80% so text stays readable. */
    on(document, 'wheel', function (e) {
      e = e || window.event;
      if (!e.ctrlKey) return;
      if (state.surface !== 'admission') return;
      if (e.preventDefault) e.preventDefault(); else e.returnValue = false;
      var delta = e.wheelDelta || (e.detail ? -e.detail : 0);
      var z = state.zoom || 80;
      z += (delta > 0) ? 5 : -5;
      if (z < 50) z = 50; if (z > 200) z = 200;
      if (applyZoom(z)) Bridge.call('reception.zoom.save', { zoom: z });
    });
  }

  /* v1.79.0: TRIDENT SCROLL-REPAINT FIX. The clinic reported that after
     scrolling a few times (and across app restarts — i.e. whenever the
     persisted zoom != 100%), the page's items suddenly garble on scroll. That
     is the classic MSHTML/Trident repaint bug for `zoom`ed containers: the
     scrolled-in region paints stale pixels until a full reflow. WebView2 /
     Chromium never shows it. Fix: on the MSHTML bridge only, after each scroll
     burst settles, re-apply the zoom value — a cheap reflow that forces the
     whole workspace to repaint correctly. */
  (function () {
    if (window.chrome && window.chrome.webview) return;   /* WebView2: fine */
    var t = null;
    function nudge() {
      var host = $('appBody'); if (!host) return;
      host.style.zoom = '';
      host.style.zoom = (state.zoom || 80) + '%';
    }
    function onScroll() { if (t) clearTimeout(t); t = setTimeout(nudge, 150); }
    on(window, 'scroll', onScroll);
    on(document, 'scroll', onScroll);
    var tw = $('svcTblWrap'); if (tw) on(tw, 'scroll', onScroll);
  })();

  /* v1.73.0: ZOOM rework — content never overflows the web view at any zoom.
     The .app shell is a FIXED clip boundary (width:100%, height:100vh,
     overflow:hidden in admission.css) whose size JS NEVER touches, so the page
     can never spill outside the user's screen. Zoom is applied to the inner
     workspace (#appBody = .body) as the CSS `zoom` property ONLY — no container
     is ever resized (the old enlarge-by-1/scale trick that set width/height to
     10000/z caused left-side overflow in RTL and is gone). The browser scales
     the content inside the fixed shell and clips anything that would spill past
     it. Columns use percentage flex-basis that sums to 100%, so they scale with
     the workspace and never push each other off-screen. If a browser ignores
     `zoom` (some MSHTML builds), fall back to transform:scale() anchored at the
     centre so the content stays centred and the .app overflow:hidden still clips
     the overflow. We never apply both — that would double-scale.
     v1.77.0: default lowered 90% → 80%; unzoom floor lowered 80 → 50. When the
     user zooms OUT below the 80% default (to fit all items), text would shrink
     too far to read, so a small font bump is applied — see the .az-unzoom block
     in admission.css. Zoom-IN (>=80%) is unchanged (font stays normal). */
  function columnsWrapped() {
    var a = $('colRight'), b = $('colCenter'), c = $('colLeft');
    if (!a || !b || !c) return false;
    var t = a.offsetTop;
    return (b.offsetTop !== t) || (c.offsetTop !== t);
  }
  function applyZoom(z, force) {
    var prev = state.zoom || 80;
    if (z < 50) z = 50; if (z > 200) z = 200;
    state.zoom = z;
    var host = $('appBody');            /* the inner workspace (.body) */
    if (!host) return true;
    host.style.zoom = z + '%';          /* primary: CSS zoom scales content in place */
    /* Detect whether `zoom` actually reflowed the element. A width:100% element
       that honoured zoom reports a scaled offsetWidth; one that ignored it
       reports the shell's full width. Fall back to transform:scale() then. */
    var shell = $('app');
    var took = false;
    if (shell) {
      var sw = shell.offsetWidth, hw = host.offsetWidth;
      if (sw && hw) took = Math.round(hw) !== Math.round(sw);
    }
    host.style.webkitTransformOrigin = 'center center';
    host.style.msTransformOrigin = 'center center';
    host.style.transformOrigin = 'center center';
    if (took || z === 100) {
      host.style.webkitTransform = '';
      host.style.msTransform = '';
      host.style.transform = '';
    } else {
      var f = z / 100;
      host.style.webkitTransform = 'scale(' + f + ')';
      host.style.msTransform = 'scale(' + f + ')';
      host.style.transform = 'scale(' + f + ')';
    }
    /* v1.77.0: UNZOOM READABILITY — only when zoomed OUT below the 80% default.
       Nudge the workspace base font up a little (1-3px, graduated) so text stays
       legible even when shrunk to fit all items, and tag the workspace with the
       `az-unzoom` class so admission.css can bump the explicit-size fields,
       buttons, table cells and titles by ~1px as well. At >=80% (zoom-in / the
       default) the class and inline font are cleared so behaviour is unchanged. */
    if (z < 80) {
      var fsBump = z < 60 ? 3 : (z < 70 ? 2 : 1);
      host.style.fontSize = (14 + fsBump) + 'px';
      if ((' ' + host.className + ' ').indexOf(' az-unzoom ') < 0) {
        host.className = String(host.className || '') + ' az-unzoom';
      }
    } else {
      host.style.fontSize = '';
      host.className = String(host.className || '').replace(/\s*az-unzoom\b/g, '');
    }
    /* v1.84: first apply is wrap-checked too (do not skip when z === prev). */
    if (!force && columnsWrapped()) {
      applyZoom(prev, true);
      return false;
    }
    return true;
  }
  function applySavedZoom(saved) {
    var want = Number(saved);
    if (!(want >= 50 && want <= 200)) want = 80;
    applyZoom(80, true);
    var z = want;
    var used = 80;
    while (z >= 50) {
      if (applyZoom(z)) { used = state.zoom || z; break; }
      if (z <= 80) { applyZoom(80, true); used = 80; break; }
      z -= 5;
    }
    if (used !== want) Bridge.call('reception.zoom.save', { zoom: used });
  }
  function applyMode(mode) {
    state.mode = mode === 'full' ? 'full' : 'simple';
    document.body.className = (document.body.className || '').replace(/\bmode-(simple|full)\b/g, '') + ' mode-' + state.mode;
  }
  /* v1.64.0 (درمان پلاس) — the queue panel is now a FULL-SCREEN overlay sized
     to the user's monitor (not a fixed draggable box). open/close go through
     one pair of helpers so the overlay + its dim backdrop can never fall out of
     sync. `tab` optionally selects the صندوق/صف tab before showing. */
  /* v1.85: recall a queue patient. On the queue tab we open a fresh پذیرش tab
     (the queue document has no visible form); on the admission surface we fill
     the live form in place. Never fill the hidden queue form. */
  function queueRecall(q) {
    if (!q) return;
    if (state.surface === 'queue') {
      Bridge.call('ui.openQueuePatient', { id: q.id, kind: state.queueKind });
      return;
    }
    closeQueuePanel();
    fillPatient(q);
    toast('بیمار از صف بازخوانی شد', 'ok');
  }
  function closeQueuePanel() {
    /* v1.85: on the queue tab the panel IS the page — never collapse it. */
    if (state.surface === 'queue') return;
    setOverlay('queuePanel', 'queueBackdrop', false);
  }

  function cashAsk(msg, onYes) {
    var dlg = $('opsDlg');
    var yes = $('opsDlgYes');
    var no = $('opsDlgNo');
    var box = $('opsDlgMsg');
    if (!dlg || !yes) { if (onYes) onYes(); return; }
    setText(box, msg || '');
    dlg.style.display = 'block';
    yes.onclick = function () { dlg.style.display = 'none'; if (onYes) onYes(); };
    if (no) no.onclick = function () { dlg.style.display = 'none'; };
  }

  function cashCall(msg, verb, payload, okMsg, failMsg, afterOk) {
    cashAsk(msg, function () {
      Bridge.call(verb, payload || {}).then(function (r) {
        if (r && r.ok === false) { toast(r.err || failMsg, 'err'); return; }
        toast(okMsg, 'ok');
        if (afterOk) afterOk(r);
        refreshCash();
      }, function () { toast(failMsg, 'err'); });
    });
  }

  function queueCall(msg, verb, payload, okMsg, failMsg, afterOk) {
    cashAsk(msg, function () {
      Bridge.call(verb, payload || {}).then(function (r) {
        if (r && r.ok === false) { toast(r.err || failMsg, 'err'); return; }
        toast(okMsg, 'ok');
        if (afterOk) afterOk(r);
        refreshQueue();
      }, function () { toast(failMsg, 'err'); });
    });
  }

  function setFieldsLocked(on) {
    var ids = ['nid', 'first', 'last', 'father', 'birth', 'mobile', 'phone', 'addr',
      'doc2code', 'doc2name', 'perfcode', 'perfname', 'insBooklet', 'insValid',
      'rxDate', 'apptDate', 'svcSearch', 'gender', 'insMain', 'insSupp', 'insSuppPct',
      'ptype', 'insType', 'apptShift', 'hasIns', 'noPay'];
    var i, el;
    for (i = 0; i < ids.length; i++) {
      el = $(ids[i]);
      if (!el) continue;
      if (on) {
        el.setAttribute('disabled', 'disabled');
        if (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA')
          el.setAttribute('readonly', 'readonly');
      } else {
        el.removeAttribute('disabled');
        el.removeAttribute('readonly');
      }
    }
    if (on && document.activeElement && document.activeElement.blur) {
      try { document.activeElement.blur(); } catch (e) {}
    }
  }

  function setFormLocked(on) {
    state.formLocked = !!on;
    var root = document.body;
    var cls = String(root.className || '').replace(/\s*form-locked\b/g, '');
    if (on) cls += ' form-locked';
    root.className = cls;
    setFieldsLocked(on);
  }

  function unlockCashForm() {
    if (!state.formLocked) return;
    if (!state.canCashEdit) { toast('دسترسی تغییر صندوق ندارید', 'err'); return; }
    setFormLocked(false);
    recompute();
    toast('ویرایش باز شد', 'ok');
  }

  /* v1.94: F5 toggle for admission edit mode. Loaded receipts start read-only;
     pressing F5 unlocks editing (and locks again on the next press). */
  function toggleEditMode() {
    state.formLocked = !state.formLocked;
    setFormLocked(state.formLocked);
    toast(state.formLocked ? 'فرم قفل شد' : 'ویرایش فعال شد — F5 برای قفل', 'info');
  }

  function setOverlay(panelId, backId, open) {
    var p = $(panelId), b = $(backId);
    if (p) {
      p.className = (p.className || '').replace(/\s*open/g, '') + (open ? ' open' : '');
      p.setAttribute('aria-hidden', open ? 'false' : 'true');
    }
    if (b) b.className = open ? 'queue-backdrop open' : 'queue-backdrop';
  }

  function newPatient() {
    clearForm();
    toast('فرم بیمار پاک شد', 'ok');
  }

  function pad2(n) { return (n < 10 ? '0' : '') + n; }
  function jalaliYesterday(j) {
    var d = toEn(j || '').replace(/[^0-9]/g, '');
    if (d.length < 8) return '';
    var y = +d.substr(0, 4), m = +d.substr(4, 2), day = +d.substr(6, 2);
    day -= 1;
    if (day < 1) {
      m -= 1;
      if (m < 1) { m = 12; y -= 1; }
      day = (m <= 6) ? 31 : (m <= 11 ? 30 : 29);
    }
    return y + '/' + pad2(m) + '/' + pad2(day);
  }

  function setSelectText(id, text) {
    var sel = $(id); if (!sel || text == null || text === '') return;
    var i;
    for (i = 0; i < sel.options.length; i++) {
      if (trimStr(sel.options[i].text) === trimStr(text) ||
          trimStr(sel.options[i].value) === trimStr(text)) {
        sel.selectedIndex = i; return;
      }
    }
  }
  function applyTicketToForm(t) {
    /* v1.94: a loaded receipt starts read-only — data seated but not editable;
       F5 unlocks editing. clearForm keeps the lock (keepLock:true) so it survives
       the field reset, and JS .value assignment still populates locked fields. */
    state.formLocked = true;
    setFormLocked(true);
    clearForm({ keepLock: true, skipFocus: true });
    fillPatient({
      nid: t.nid, first: t.first, last: t.last, mobile: t.mobile,
      file: t.fileNo
    });
    if (t.doctor && $('doc2name')) $('doc2name').value = t.doctor;
    setSelectText('apptShift', t.shift);
    if (t.apptDate && $('apptDate')) $('apptDate').value = toFa(t.apptDate);
    else if (t.date && $('apptDate')) $('apptDate').value = toFa(t.date);
    state.services = [];
    var sv = t.services || [];
    var i;
    for (i = 0; i < sv.length; i++) addServiceRow(sv[i]);
    renderServices();
    applyTicketBill(t);
    /* v1.97: remember the loaded ticket. Paid → reprint via receipt.print;
       unpaid → صدور / صندوق شد opens the payment mini. */
    state.loadedTicketId = t.id || t.ticketId || '';
    var paidAmt = Number(t.paid) || 0;
    var st = String(t.status || '').toLowerCase();
    var unpaid = !paidAmt || st === 'unpaid' || st === 'waiting' || st === 'debtor';
    if (unpaid && state.loadedTicketId) {
      state.payTicketId = state.loadedTicketId;
      state.reprintId = '';
    } else {
      state.payTicketId = '';
      state.reprintId = state.loadedTicketId;
    }
    state.payCtx = {
      id: state.loadedTicketId,
      name: t.name || trimStr((t.first || '') + ' ' + (t.last || '')),
      barcode: t.barcode || '',
      payable: (t.payable != null) ? t.payable : 0,
      services: t.services || state.services || []
    };
  }
  function showToolsHome() {
    var h = $('toolsHome'), r = $('toolsReceiptsView');
    if (h) h.className = 'tools-home show';
    if (r) r.className = 'tools-receipts';
  }
  function showReceiptsPage() {
    var h = $('toolsHome'), r = $('toolsReceiptsView');
    if (h) h.className = 'tools-home';
    if (r) r.className = 'tools-receipts show';
    ensureReceiptDefaults();
    loadReceiptSections();
  }

  function ensureReceiptDefaults() {
    var today = state.todayJalali || '';
    if ($('rcTo') && !$('rcTo').value) $('rcTo').value = toFa(today);
    if ($('rcFrom') && !$('rcFrom').value) $('rcFrom').value = toFa(jalaliYesterday(today) || today);
  }
  function loadReceiptSections() {
    var sel = $('rcSect');
    if (!sel || sel.getAttribute('data-ready') === '1') return;
    Bridge.call('receipt.sections', {}).then(function (d) {
      if (!sel) return;
      var rows = (d && d.rows) || [];
      sel.innerHTML = '';
      var o0 = document.createElement('option');
      o0.value = '0'; o0.appendChild(document.createTextNode('همه بخش‌ها'));
      sel.appendChild(o0);
      var i;
      for (i = 0; i < rows.length; i++) {
        var o = document.createElement('option');
        o.value = String(rows[i].id || 0);
        o.appendChild(document.createTextNode(rows[i].name || ''));
        sel.appendChild(o);
      }
      sel.setAttribute('data-ready', '1');
    });
  }

  function receiptHits() {
    var parts = [];
    function add(id) {
      var el = $(id); var v = el ? trimStr(el.value) : '';
      if (v) parts.push(toEn(v));
    }
    add('rcFirst'); add('rcLast'); add('rcNid'); add('rcMobile');
    add('rcFile'); add('rcArch'); add('rcBar'); add('rcDoc');
    return parts;
  }
  function boldHits(text, hits) {
    var s = esc(text == null ? '' : text);
    if (!hits || !hits.length) return s;
    var i, h, re;
    for (i = 0; i < hits.length; i++) {
      h = hits[i];
      if (!h) continue;
      re = new RegExp('(' + h.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + ')', 'gi');
      s = s.replace(re, '<span class="rc-hit">$1</span>');
    }
    return s;
  }
  function receiptRowClass(r) {
    var st = String(r.status || '').toLowerCase();
    var paid = (+r.paid || 0) > 0;
    if (st === 'cancelled' || st === 'refund' || st.indexOf('استرداد') >= 0) return 'rc-red';
    if (st === 'insdel' || st === 'ins_del' || st.indexOf('بیمه') >= 0) return 'rc-orange';
    if (st === 'debtor' || st.indexOf('بدهکار') >= 0) return 'rc-purple';
    if (st === 'creditor' || st.indexOf('بستانکار') >= 0) return 'rc-soap';
    if (st === 'advance' || st.indexOf('نوبت') >= 0) return 'rc-green';
    if (st === 'answered' || st.indexOf('جواب') >= 0) return 'rc-blue';
    if (st === 'queue' || st === 'unpaid' || !paid) return 'rc-yellow';
    return 'rc-paid';
  }
  function searchReceipts() {
    var payload = {
      first: $('rcFirst') ? $('rcFirst').value : '',
      last: $('rcLast') ? $('rcLast').value : '',
      nid: $('rcNid') ? $('rcNid').value : '',
      mobile: $('rcMobile') ? $('rcMobile').value : '',
      fileNo: $('rcFile') ? $('rcFile').value : '',
      archive: $('rcArch') ? $('rcArch').value : '',
      barcode: $('rcBar') ? $('rcBar').value : '',
      doctor: $('rcDoc') ? $('rcDoc').value : '',
      from: $('rcFrom') ? toEn($('rcFrom').value) : '',
      to: $('rcTo') ? toEn($('rcTo').value) : '',
      sectionId: $('rcSect') ? +$('rcSect').value || 0 : 0,
      onlyUser: $('rcOnlyUser') ? !!$('rcOnlyUser').checked : false,
      byAppt: $('rcByAppt') ? !!$('rcByAppt').checked : false
    };
    Bridge.call('receipt.search', payload).then(function (d) {
      state.rcRows = (d && d.rows) || [];
      state.rcHits = receiptHits();
      state.rcChecked = {};
      state.rcPageNo = 1;
      if ($('rcAll')) $('rcAll').checked = false;
      renderReceipts();
    }, function () { toast('جستجوی قبض ناموفق بود', 'err'); });
  }
  var RC_PAGE = 30;
  function receiptPageCount() {
    var n = (state.rcRows || []).length;
    return n ? Math.ceil(n / RC_PAGE) : 1;
  }
  function receiptPageRows() {
    var rows = state.rcRows || [];
    var pages = receiptPageCount();
    if (state.rcPageNo < 1) state.rcPageNo = 1;
    if (state.rcPageNo > pages) state.rcPageNo = pages;
    var start = (state.rcPageNo - 1) * RC_PAGE;
    return rows.slice(start, start + RC_PAGE);
  }
  function updateReceiptPager() {
    var lbl = $('rcPageLbl');
    var pages = receiptPageCount();
    var n = (state.rcRows || []).length;
    if (lbl) lbl.innerHTML = n ? ('صفحه ' + toFa(state.rcPageNo) + ' از ' + toFa(pages)) : 'صفحه ۰ از ۰';
    if ($('rcPrev')) $('rcPrev').disabled = state.rcPageNo <= 1 || !n;
    if ($('rcNext')) $('rcNext').disabled = state.rcPageNo >= pages || !n;
  }
  function renderReceipts() {
    var body = $('rcBody'); if (!body) return;
    var all = state.rcRows || [];
    updateReceiptPager();
    if (!all.length) {
      body.innerHTML = '<tr><td colspan="13" class="empty">موردی یافت نشد</td></tr>';
      return;
    }
    var rows = receiptPageRows();
    var hits = state.rcHits || [];
    var html = '', i, r, cls, paid, sel;
    for (i = 0; i < rows.length; i++) {
      r = rows[i];
      cls = receiptRowClass(r);
      paid = (+r.paid || 0) > 0 && String(r.status || '') !== 'cancelled';
      sel = (state.rcSel && state.rcSel === r.id) ? ' rc-sel' : '';
      html += '<tr class="' + cls + sel + '" data-rid="' + esc(r.id) + '">' +
        '<td class="c-chk"><input type="checkbox" data-rchk="' + esc(r.id) + '"' +
          ((state.rcChecked && state.rcChecked[r.id]) ? ' checked="checked"' : '') + ' /></td>' +
        '<td>' + boldHits(r.barcode, hits) + '</td>' +
        '<td>' + boldHits(r.fileNo, hits) + '</td>' +
        '<td>' + boldHits(r.nid, hits) + '</td>' +
        '<td>' + boldHits(r.name || ((r.first || '') + ' ' + (r.last || '')), hits) + '</td>' +
        '<td>' + esc(r.insBase || '—') + '</td>' +
        '<td>' + esc(r.insSupp || '—') + '</td>' +
        '<td>' + boldHits(r.receiptNo, hits) + '</td>' +
        '<td>' + toFa(r.date || '') + '</td>' +
        '<td>' + toFa(r.apptDate || '') + '</td>' +
        '<td>' + toFa(r.turn || '') + '</td>' +
        '<td>' + esc(r.shift || '') + '</td>' +
        '<td>' + (paid ? '<span class="rc-tickmark">✓</span>پرداخت' : '—') + '</td></tr>';
    }
    body.innerHTML = html;
  }
  function selectedReceiptIds() {
    var ids = [], k;
    if (state.rcChecked) {
      for (k in state.rcChecked) {
        if (state.rcChecked.hasOwnProperty(k) && state.rcChecked[k]) ids.push(k);
      }
    }
    if (!ids.length && state.rcSel) ids.push(state.rcSel);
    return ids;
  }
  function openReceiptOnAdmission(id) {
    if (!id) return;
    Bridge.call('ui.openAdmission', { id: id });
  }
  function printSelectedReceipt() {
    var ids = selectedReceiptIds();
    if (!ids.length) { toast('قبضی انتخاب نشده', 'err'); return; }
    Bridge.call('receipt.print', { id: ids[0] }).then(function (r) {
      if (r && r.ok) toast('چاپ قبض با تاریخ اصلی', 'ok');
      else toast((r && r.err) || 'چاپ ناموفق بود', 'err');
    }, function () { toast('چاپ ناموفق بود', 'err'); });
  }
  function deleteSelectedReceipts() {
    if (state.role < 1) { toast('فقط مدیر می‌تواند قبض را حذف کند', 'err'); return; }
    var ids = selectedReceiptIds();
    if (!ids.length) { toast('قبضی انتخاب نشده', 'err'); return; }
    cashAsk('حذف ' + toFa(ids.length) + ' قبض؟ این عمل برگشت‌پذیر نیست.', function () {
      var verb = ids.length === 1 ? 'receipt.delete' : 'receipt.deleteMany';
      var payload = ids.length === 1 ? { id: ids[0] } : { ids: ids.join(',') };
      Bridge.call(verb, payload).then(function (r) {
        if (r && r.ok === false) { toast(r.err || 'حذف ناموفق بود', 'err'); return; }
        toast('حذف شد', 'ok');
        searchReceipts();
      }, function () { toast('حذف ناموفق بود', 'err'); });
    });
  }
  function csvCell(v) {
    var s = String(v == null ? '' : v).replace(/"/g, '""');
    return '"' + s + '"';
  }
  function exportReceiptExcel() {
    var rows = receiptPageRows();
    if (!rows.length) { toast('ردیفی برای خروجی نیست', 'err'); return; }
    var lines = ['بارکد,ش پرونده,کد ملی,نام بیمار,بیمه پایه,بیمه تکمیلی,ش قبض,تاریخ پذیرش,تاریخ نوبت,نوبت,شیفت,صندوق'];
    var i, r, paid;
    for (i = 0; i < rows.length; i++) {
      r = rows[i];
      paid = (+r.paid || 0) > 0 ? 'پرداخت' : '';
      lines.push([
        csvCell(r.barcode), csvCell(r.fileNo), csvCell(r.nid),
        csvCell(r.name || ((r.first || '') + ' ' + (r.last || ''))),
        csvCell(r.insBase), csvCell(r.insSupp), csvCell(r.receiptNo),
        csvCell(r.date), csvCell(r.apptDate), csvCell(r.turn),
        csvCell(r.shift), csvCell(paid)
      ].join(','));
    }
    var csv = '\ufeff' + lines.join('\r\n');
    try {
      var blob = new Blob([csv], { type: 'text/csv;charset=utf-8' });
      var a = document.createElement('a');
      a.href = (window.URL || window.webkitURL).createObjectURL(blob);
      a.download = 'receipts.csv';
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      toast('خروجی اکسل آماده شد', 'ok');
      return;
    } catch (e) {}
    try {
      var w = window.open('', '_blank');
      if (w && w.document) {
        w.document.write('<html><head><meta charset="utf-8"><title>قبوض</title></head><body dir="rtl"><pre>');
        w.document.write(esc(csv));
        w.document.write('</pre></body></html>');
        toast('خروجی در پنجره جدید — ذخیره کنید', 'ok');
        return;
      }
    } catch (e2) {}
    toast('خروجی اکسل در این موتور پشتیبانی نمی‌شود', 'err');
  }

  function storeWidths(key, map) {
    try { if (window.localStorage) window.localStorage.setItem(key, map); } catch (e) {}
  }
  function loadWidths(key) {
    try { return window.localStorage ? window.localStorage.getItem(key) : ''; } catch (e) { return ''; }
  }
  function applyStoredWidths(table, key) {
    if (!table) return;
    var raw = loadWidths(key); if (!raw) return;
    var parts = raw.split(','), i, kv, id, w, ths = table.getElementsByTagName('th');
    var map = {};
    for (i = 0; i < parts.length; i++) {
      kv = parts[i].split(':');
      if (kv.length === 2) map[kv[0]] = kv[1];
    }
    for (i = 0; i < ths.length; i++) {
      id = ths[i].getAttribute('data-col');
      if (id && map[id]) ths[i].style.width = map[id] + 'px';
    }
  }
  function saveTableWidths(table, key) {
    if (!table) return;
    var ths = table.getElementsByTagName('th'), i, id, out = [];
    for (i = 0; i < ths.length; i++) {
      id = ths[i].getAttribute('data-col');
      if (id) out.push(id + ':' + ths[i].offsetWidth);
    }
    storeWidths(key, out.join(','));
  }
  function wireColResize(table, key) {
    if (!table || table.getAttribute('data-resz') === '1') return;
    table.setAttribute('data-resz', '1');
    table.style.tableLayout = 'fixed';
    applyStoredWidths(table, key);
    var ths = table.getElementsByTagName('th'), i, th, grip;
    for (i = 0; i < ths.length; i++) {
      th = ths[i];
      if (!th.getAttribute('data-col')) continue;
      grip = document.createElement('span');
      grip.className = 'rc-col-resizer';
      th.appendChild(grip);
      (function (cell, tbl) {
        on(grip, 'mousedown', function (e) {
          e = e || window.event;
          if (e.preventDefault) e.preventDefault();
          if (e.stopPropagation) e.stopPropagation();
          e.cancelBubble = true;
          var startX = e.clientX, startW = cell.offsetWidth;
          function move(ev) {
            ev = ev || window.event;
            if (ev.stopPropagation) ev.stopPropagation();
            var w = startW - (ev.clientX - startX);
            if (w < 48) w = 48;
            cell.style.width = w + 'px';
          }
          function up() {
            if (document.removeEventListener) {
              document.removeEventListener('mousemove', move, false);
              document.removeEventListener('mouseup', up, false);
            } else {
              document.detachEvent('onmousemove', move);
              document.detachEvent('onmouseup', up);
            }
            saveTableWidths(tbl, key);
          }
          if (document.addEventListener) {
            document.addEventListener('mousemove', move, false);
            document.addEventListener('mouseup', up, false);
          } else {
            document.attachEvent('onmousemove', move);
            document.attachEvent('onmouseup', up);
          }
        });
      })(th, table);
    }
  }

  function queueTransfer(row) {
    if (!row || !row.id) return;
    var from = state.queueKind;
    var to = from === 'admission' ? 'unpaid' : 'admission';
    var msg = to === 'admission' ? 'انتقال به صف پذیرش؟' : 'انتقال به صندوق نرفته‌ها؟';
    queueCall(msg, 'queue.transfer', { id: row.id, from: from, to: to }, 'منتقل شد', 'انتقال ناموفق بود');
  }
  function queuePay(row) {
    if (!row || !row.id) return;
    queueCall('این ردیف پرداخت و از صندوق نرفته‌ها خارج شود؟', 'queue.pay', { id: row.id },
      'پرداخت شد', 'پرداخت ناموفق بود');
  }
  function queueCancel(row) {
    if (!row || !row.id) return;
    var dlg = $('cancelDlg');
    if (!dlg) return;
    if ($('cancelAcct')) $('cancelAcct').value = state.userName || '';
    if ($('cancelReason')) $('cancelReason').value = '';
    dlg.style.display = 'block';
    $('cancelDlgYes').onclick = function () {
      var reason = $('cancelReason') ? trimStr($('cancelReason').value) : '';
      if (!reason) { toast('علت لغو را بنویسید', 'err'); return; }
      dlg.style.display = 'none';
      cashAsk('لغو این پذیرش تأیید شود؟', function () {
        Bridge.call('queue.cancel', { id: row.id, reason: reason, kind: state.queueKind }).then(function (r) {
          if (r && r.ok === false) { toast(r.err || 'لغو ناموفق بود', 'err'); return; }
          toast('لغو شد', 'ok');
          refreshQueue();
        }, function () { toast('لغو ناموفق بود', 'err'); });
      });
    };
    $('cancelDlgNo').onclick = function () { dlg.style.display = 'none'; };
  }

  function refreshCash() {
    if (!state.canCashView) return;
    Bridge.call('cashier.page', {
      q: state.cashQ || '',
      tab: state.cashTab || 0,
      status: state.cashStatus || ''
    }).then(function (d) {
      renderCash(d || {});
    }, function () { toast('بارگذاری صندوق ناموفق بود', 'err'); });
  }

  function renderCash(d) {
    var tabs = d.tabs || [];
    /* Build the section tab buttons once. The same markup is mirrored into a
       container ABOVE the table (#cashTabsAbove) so «صندوق نرفته‌ها» and the
       section tabs sit right above the rows instead of inside the overlay
       header. The original header #cashTabs is hidden — the tabs now live
       above the table. (loop var renamed `on`->`act` so the on() event helper
       stays callable for the delegated handler bound below.) */
    var h = '', i, t, act;
    for (i = 0; i < tabs.length; i++) {
      t = tabs[i];
      act = (+t.id === +state.cashTab) ? ' active' : '';
      h += '<button type="button" class="cash-tab' + act + '" data-tab="' + esc(t.id) + '">' +
           esc(t.name || '') + '</button>';
    }
    var head = $('cashTabs');
    if (head) { head.innerHTML = h; head.style.display = 'none'; }
    var above = $('cashTabsAbove');
    if (!above) {
      above = document.createElement('div');
      above.id = 'cashTabsAbove';
      above.className = 'tabs cash-tabs';
      var wrap = $('cashWrap');
      var strip = $('cashStatusStrip');
      var before = strip || wrap;
      if (before && before.parentNode) before.parentNode.insertBefore(above, before);
      /* delegated tab handler — wired ONCE (same pattern as #cashTabs in
         wire()) so re-renders never double-bind. */
      on(above, 'click', function (e) {
        e = e || window.event;
        var tgt = e.target || e.srcElement;
        var btn = findUp(tgt, 'data-tab', above);
        if (!btn) return;
        state.cashTab = +btn.getAttribute('data-tab') || 0;
        state.cashStatus = '';
        refreshCash();
      });
    }
    if (above) above.innerHTML = h;
    paintCashStatus(d.statusCounts || {});
    var inc = (d.shift && d.shift.income != null) ? d.shift.income : (d.income || 0);
    var incEl = $('cashIncome');
    if (incEl) incEl.innerHTML = '<b>' + money(inc) + '</b> <span class="cash-cur">ریال</span>';
    var st = d.stats || {};
    setText($('cashStatP'), toFa(st.patients || 0));
    setText($('cashStatPaid'), toFa(st.paid || 0));
    setText($('cashStatUnpaid'), toFa(st.unpaid || 0));
    setText($('cashStatQ'), toFa(st.queue || 0));
    var sh = d.shift || {};
    /* v1.93: restructure the shift meta into TWO lines (date / time) and move
       #cashShiftMeta into the .cash-actions row so it sits beside the shift
       buttons. Done dynamically here because the JS owns layout on this surface. */
    var meta = $('cashShiftMeta');
    var actions = document.querySelector('.cash-actions');
    if (meta && actions && meta.parentNode !== actions) {
      actions.appendChild(meta);
      meta.className = 'cash-shift-meta-inline';
    }
    var dateLine = '', timeLine = '';
    if (sh.open) {
      dateLine = toFa(sh.startJalali || '');
      timeLine = 'شروع: ' + toFa(sh.startTime || '');
    } else if (sh.startTime) {
      dateLine = toFa(sh.startJalali || '');
      timeLine = toFa(sh.startTime || '') + ' — ' + toFa(sh.endTime || '');
    } else {
      dateLine = 'شیفت شروع نشده';
    }
    if (meta) meta.innerHTML = '<div class="cash-meta-date">' + dateLine + '</div><div class="cash-meta-time">' + timeLine + '</div>';

    /* Shift button enable/disable: when a shift is OPEN the start button is
       disabled and the end button enabled, and vice-versa when CLOSED. Only
       touch the buttons when the user has cashier_edit (the no-permission case
       hides them via hideIds in init — display:none — so we must NOT re-enable
       them here; detect that by checking the start button is still visible). */
    var ssBtn = $('cashShiftStart'), seBtn = $('cashShiftEnd');
    if (ssBtn && seBtn) {
      var canEdit = !ssBtn.style.display || ssBtn.style.display !== 'none';
      if (canEdit) {
        ssBtn.disabled = !!sh.open;
        seBtn.disabled = !sh.open;
      }
    }

    var rows = d.rows || [];
    state.cashRows = rows;
    var body = $('cashBody');
    if (!body) return;
    if (!rows.length) {
      body.innerHTML = '<tr><td colspan="8" class="empty">موردی در این زبانه نیست</td></tr>';
      return;
    }
    var html = '', r, paid;
    for (i = 0; i < rows.length; i++) {
      r = rows[i];
      paid = (+r.paid || 0) > 0;
      html += '<tr class="' + (paid ? '' : 'cash-unpaid') + '" data-cid="' + esc(r.id) +
              '" data-paid="' + (paid ? '1' : '0') + '">' +
        '<td class="c-mono">' + toFa(r.barcode || '—') + '</td>' +
        '<td>' + esc(r.name || ((r.first || '') + ' ' + (r.last || ''))) + '</td>' +
        '<td>' + money(r.payable) + '</td>' +
        '<td>' + esc(r.doctor || '—') + '</td>' +
        '<td>' + esc(r.section || '—') + '</td>' +
        '<td>' + toFa(r.time || '') + '</td>' +
        '<td>' + toFa(r.date || '') + '</td>' +
        '<td>' + (paid || !state.canCashEdit ? (paid ? 'صندوق‌شده' : '—') :
          '<button type="button" class="q-act-btn" data-pay="' + esc(r.id) + '">صندوق شد</button>') +
        '</td></tr>';
    }
    body.innerHTML = html;
  }

  function paintCashStatus(counts) {
    counts = counts || {};
    setText($('cashStRefund'), toFa(counts.refund || 0));
    setText($('cashStWait'), toFa(counts.waiting || 0));
    setText($('cashStDebt'), toFa(counts.debtor || 0));
    setText($('cashStCred'), toFa(counts.creditor || 0));
    var strip = $('cashStatusStrip');
    if (!strip) return;
    var btns = strip.getElementsByTagName('button'), i, b, st, cls;
    for (i = 0; i < btns.length; i++) {
      b = btns[i];
      st = b.getAttribute('data-status') || '';
      cls = String(b.className || '').replace(/\s*on\b/g, '');
      if (st && st === state.cashStatus) cls += ' on';
      b.className = cls;
    }
  }

  function cashRowById(id) {
    var rows = state.cashRows || [], i;
    for (i = 0; i < rows.length; i++) {
      if (String(rows[i].id) === String(id)) return rows[i];
    }
    return null;
  }

  function lookupCashSearch(q) {
    var raw = trimStr(toEn(q || '')).replace(/\s+/g, '');
    if (!raw) { refreshCash(); return; }
    if (!/^[0-9]{4,}$/.test(raw)) { refreshCash(); return; }
    Bridge.call('cashier.lookup', { barcode: raw, nid: raw }).then(function (r) {
      if (r && r.ok && r.id) openReceiptOnAdmission(r.id);
      else refreshCash();
    }, function () { refreshCash(); });
  }

  function barcodeHtml(code) {
    var s = toEn(String(code || ''));
    var h = '<span class="bc-quiet"></span>', i, d, w;
    for (i = 0; i < s.length; i++) {
      d = s.charCodeAt(i) - 48;
      if (d < 0 || d > 9) d = (s.charCodeAt(i) % 7) + 1;
      w = 1 + (d % 4);
      h += '<span class="bc-bar bc-w' + w + '"></span>';
      h += '<span class="bc-gap bc-g' + (1 + ((d + 1) % 3)) + '"></span>';
    }
    h += '<span class="bc-quiet"></span>';
    return h;
  }

  function fillMiniServices(tbodyId, services) {
    var body = $(tbodyId);
    if (!body) return;
    var sv = services || [], i, s, q, p, tot, html = '';
    if (!sv.length) {
      body.innerHTML = '<tr><td colspan="3" class="empty">خدمتی ثبت نشده</td></tr>';
      return;
    }
    for (i = 0; i < sv.length; i++) {
      s = sv[i] || {};
      q = Number(s.qty) || 1;
      if (q < 1) q = 1;
      p = Number(s.price) || 0;
      if (s.total != null) tot = Number(s.total) || 0;
      else if (s.patShare != null) tot = Number(s.patShare) || 0;
      else tot = p * q;
      html += '<tr><td>' + esc(s.name || s.code || '—') + '</td><td>' +
        toFa(q) + '</td><td>' + money(tot) + '</td></tr>';
    }
    body.innerHTML = html;
  }

  function setMiniOpen(panelId, backId, open) {
    var p = $(panelId), b = $(backId);
    if (p) {
      p.className = String(p.className || '').replace(/\s*open\b/g, '') + (open ? ' open' : '');
      p.setAttribute('aria-hidden', open ? 'false' : 'true');
    }
    if (b) b.className = String(b.className || '').replace(/\s*open\b/g, '') + (open ? ' open' : '');
  }

  function openTicketMini(r) {
    r = r || {};
    state.ticketMiniId = r.ticketId || r.id || '';
    var name = r.name || trimStr((r.first || '') + ' ' + (r.last || ''));
    var code = r.barcode || state.ticketMiniId || '';
    setText($('ticketMiniName'), name || '—');
    setText($('ticketMiniCode'), code ? toFa(code) : '—');
    if ($('ticketMiniBar')) $('ticketMiniBar').innerHTML = barcodeHtml(code);
    fillMiniServices('ticketMiniSvc', r.services || []);
    setMiniOpen('ticketMini', 'ticketMiniBack', true);
  }

  function closeTicketMini() {
    setMiniOpen('ticketMini', 'ticketMiniBack', false);
  }

  function openPayMini(ctx) {
    ctx = ctx || {};
    var id = ctx.id || ctx.ticketId || state.payTicketId || '';
    if (!id) { toast('بلیت صندوق مشخص نیست', 'err'); return; }
    var name = ctx.name || trimStr((ctx.first || '') + ' ' + (ctx.last || ''));
    if (!name && state.payCtx && state.payCtx.name) name = state.payCtx.name;
    var code = ctx.barcode || (state.payCtx && state.payCtx.barcode) || '';
    var sv = ctx.services || (state.payCtx && state.payCtx.services) || state.services || [];
    var payable = ctx.payable;
    if (payable == null && state.payCtx) payable = state.payCtx.payable;
    if (payable == null) payable = 0;
    state.payCtx = { id: id, name: name, barcode: code, payable: payable, services: sv };
    state.payTicketId = id;
    setText($('payMiniName'), name || '—');
    setText($('payMiniCode'), code ? toFa(code) : '—');
    if ($('payMiniBar')) $('payMiniBar').innerHTML = barcodeHtml(code);
    fillMiniServices('payMiniSvc', sv);
    if ($('payMiniAmt')) $('payMiniAmt').innerHTML = money(payable);
    var box = $('payMiniDiscBox');
    if (box) box.style.display = 'none';
    if ($('payMiniDisc')) $('payMiniDisc').value = '';
    setMiniOpen('payMini', 'payMiniBack', true);
  }

  function closePayMini() {
    setMiniOpen('payMini', 'payMiniBack', false);
  }

  function submitPayMini(method) {
    var ctx = state.payCtx || {};
    var id = ctx.id || state.payTicketId;
    if (!id) { toast('بلیت صندوق مشخص نیست', 'err'); return; }
    if (!state.canCashEdit) { toast('دسترسی تغییر صندوق ندارید', 'err'); return; }
    var payable = Number(ctx.payable) || 0;
    var disc = 0, amount = payable;
    if (method === 'discount') {
      var box = $('payMiniDiscBox');
      var inp = $('payMiniDisc');
      if (box && box.style.display === 'none') {
        box.style.display = 'block';
        if (inp) try { inp.focus(); } catch (e) {}
        return;
      }
      disc = Number(toEn(inp ? inp.value : '').replace(/,/g, '')) || 0;
      if (disc < 0) disc = 0;
      amount = payable - disc;
      if (amount < 0) amount = 0;
    } else if (method === 'free') {
      amount = 0;
      disc = 0;
    }
    Bridge.call('cashier.pay', { id: id, method: method, amount: amount, discount: disc }).then(function (r) {
      if (r && r.ok === false) { toast(r.err || 'پرداخت ناموفق بود', 'err'); return; }
      toast(method === 'test' ? 'تست موفق بود' : 'صندوق شد', 'ok');
      closePayMini();
      state.payTicketId = '';
      state.payCtx = null;
      refreshCash();
    }, function () { toast('پرداخت ناموفق بود', 'err'); });
  }

  function wireMinis() {
    on($('ticketMiniClose'), 'click', closeTicketMini);
    on($('ticketMiniBack'), 'click', closeTicketMini);
    on($('ticketMiniPrint'), 'click', function () {
      var id = state.ticketMiniId;
      if (!id) { toast('قبضی برای چاپ نیست', 'err'); return; }
      Bridge.call('receipt.print', { id: id }).then(function (r) {
        if (r && r.ok) toast('چاپ قبض', 'ok');
        else toast((r && r.err) || 'چاپ ناموفق بود', 'err');
      }, function () { toast('چاپ ناموفق بود', 'err'); });
    });
    on($('payMiniClose'), 'click', closePayMini);
    on($('payMiniBack'), 'click', closePayMini);
    function onPayClick(e) {
      e = e || window.event;
      var btn = findUp(e.target || e.srcElement, 'data-paym', $('payMini'));
      if (!btn) return;
      submitPayMini(btn.getAttribute('data-paym') || 'cash');
    }
    on($('payMini'), 'click', onPayClick);
  }

  function applyTicketBill(t) {
    var sv = (t && t.services) || [];
    var i, gross = 0, pat = 0, q, p;
    for (i = 0; i < sv.length; i++) {
      q = Number(sv[i].qty) || 1;
      p = Number(sv[i].price) || 0;
      if (q < 1) q = 1;
      gross += p * q;
      if (sv[i].patShare != null) pat += Number(sv[i].patShare) || 0;
    }
    if (t && t.payable != null) pat = Number(t.payable) || 0;
    applyAuthoritativeBill({
      gross: gross,
      disc: 0,
      org: Math.max(0, gross - pat),
      supp: 0,
      pat: pat,
      paid: 0
    });
  }



  function showBlock(block) {
    var m=$('blockModal'); if(!m)return;
    setText($('blockReason'), (block && block.reason) || 'علت مسدودی ثبت نشده است');
    setText($('blockRemaining'), (block && block.remaining) || 'برای همیشه');
    m.className='modal-overlay open'; m.setAttribute('aria-hidden','false');
  }
  function closeBlock(){var m=$('blockModal');if(m){m.className='modal-overlay';m.setAttribute('aria-hidden','true');}}

  function toggleWrap(id, btn) {
    var w = $(id);
    if (!w) return;
    if (/expanded/.test(w.className)) {
      w.className = w.className.replace(/\s*expanded/, '');
      if (btn) btn.innerHTML = '&#9662;';
    } else {
      w.className += ' expanded';
      if (btn) btn.innerHTML = '&#9652;';
    }
  }
  function setActiveTab(id) {
    var tabs = document.getElementsByClassName('tab'), i;
    for (i = 0; i < tabs.length; i++) tabs[i].className = 'tab';
    if ($(id)) $(id).className = 'tab active';
  }

  /* --- lookups --- */
  function lookupNid(el) {
    var nid = toEn(el.value).replace(/\s+/g, '');
    if (!nid) { focusNext(el); return; }
    setSync('', 'در حال استعلام…');
    Bridge.call('patient.lookup', { nid: nid }).then(function (r) {
      if (r && r.found) { fillPatient(r.patient || r); toast('اطلاعات بیمار بارگذاری شد', 'ok'); }
      else toast('بیماری با این کد ملی یافت نشد — لطفاً دستی وارد کنید', 'err');
      setSync('ok', 'همگام');
      focusNext(el);
    })['catch'](function (err) { toast('خطا در استعلام', 'err'); setSync('err', 'خطا'); focusNext(el); });
  }
  function doQuickNid() {
    var nid = toEn($('qsNid') ? $('qsNid').value : '').replace(/\s+/g, '');
    if (!nid) return;
    Bridge.call('patient.lookup', { nid: nid }).then(function (r) {
      if (r && r.found) { fillPatient(r.patient || r); toast('اطلاعات بیمار بارگذاری شد', 'ok'); }
      else toast('یافت نشد', 'err');
    });
  }
  function doPatFileSearch() {
    var q = $('qsFile') ? trimStr($('qsFile').value) : '';
    Bridge.call('patient.search', { q: q }).then(function (r) { renderPatientResults(r.rows || r.patients || []); });
  }
  function doDocSearch() {
    /* v1.72.0: reads the unified #doc2code field; numeric → medical ID search,
       otherwise → name search (same rule as the live search). */
    var q = $('doc2code') ? trimStr($('doc2code').value) : '';
    if (!q) { renderDocResults([]); return; }
    var en = toEn(q).replace(/\s+/g, '');
    var byCode = /^[0-9]+$/.test(en);
    var params = byCode ? { q: en, code: en } : { q: q };
    Bridge.call('doctor.search', params).then(function (r) {
      var rows = r.rows || r.doctors || [];
      renderDocResults(rows);
      /* v1.94: auto-select single result / exact match (like performer) */
      autoSelectDoctor(rows, en);
    });
  }

  /* --- add current admission to queue (صندوق نرفته‌ها) --- */
  function addCurrentToQueue() {
    var rec = collectRecord();
    if (!rec.patient.nid) { toast('ابتدا کد ملی بیمار را وارد کنید', 'err'); return; }
    rec.kind = state.queueKind;
    Bridge.call('queue.add', rec).then(function (r) {
      if (r && r.ok) {
        toast(state.queueKind === 'admission' ? 'به صف پذیرش افزوده شد' : 'به صندوق نرفته‌ها افزوده شد', 'ok');
        refreshQueue();
      }
      else toast('افزودن ناموفق بود', 'err');
    })['catch'](function () { toast('خطا در افزودن به صندوق', 'err'); });
  }

  /* --- save admission + print per Management design --- */
  function saveAdmission() {
    /* v1.97: unpaid loaded ticket → صدور opens the payment mini (even if locked). */
    if (state.payTicketId) { openPayMini(state.payCtx || { id: state.payTicketId }); return; }
    var reprint = state.reprintId || state.loadedTicketId;
    if (reprint) {
      Bridge.call('receipt.print', { id: reprint }).then(function (r) {
        if (r && r.ok) toast('چاپ قبض با تاریخ اصلی', 'ok');
        else toast((r && r.err) || 'چاپ ناموفق بود', 'err');
      }, function () { toast('چاپ ناموفق بود', 'err'); });
      return;
    }
    if (state.formLocked) { toast('فرم قفل است — F5 برای ویرایش', 'err'); return; }
    var rec = collectRecord();
    /* v1.69.0: SMART SUBMIT — if the patient fields are empty (no national ID
       AND no name), the operator is re-printing the PREVIOUS receipt (F8
       equivalent). If fields are filled, this is a NEW admission. */
    var hasPatient = rec.patient.nid || (rec.patient.first && rec.patient.last);
    if (!hasPatient) {
      /* empty form → print the last receipt instead of erroring */
      Bridge.call('print.last', {});
      return;
    }
    if (!rec.patient.nid) { toast('کد ملی بیمار الزامی است', 'err'); if ($('nid')) $('nid').focus(); return; }
    if (!rec.patient.first || !rec.patient.last) { toast('نام و نام خانوادگی الزامی است', 'err'); return; }
    if (!state.services.length) { toast('حداقل یک خدمت باید افزوده شود', 'err'); return; }
    setSync('', 'در حال ثبت…');
    Bridge.call('admission.save', rec).then(function (r) {
      if (r && r.blocked) { state.pendingBlockedRecord=rec; showBlock(r.block); setSync('err','بیمار مسدود'); return; }
      if (r && r.ok) {
        if (r.needsCashier) {
          /* non-POS: ticket is created unpaid — show mini, do NOT claim printed */
          toast('پذیرش ثبت شد' + (r.queueNo ? ' — نوبت ' + toFa(r.queueNo) : ''), 'ok');
          openTicketMini(r);
        } else {
          toast('پذیرش ثبت و قبض چاپ شد' + (r.queueNo ? ' — نوبت ' + toFa(r.queueNo) : ''), 'ok');
        }
        /* B5: warn when the classic-GDI fallback template was used because no
           print-design is bound to the operator's section. */
        if (r.printMode && String(r.printMode).indexOf('classic-') === 0) {
          setTimeout(function () {
            toast('هیچ دیزاین چاپی به بخش شما متصل نیست — قالب پیش‌فرض استفاده شد', 'warn');
          }, 700);
        }
        if (r.ps) updatePS(r.ps);
        if (r.cashWarn) {
          setTimeout(function () {
            toast('پذیرش ثبت شد ولی صندوق: ' + r.cashWarn, 'err');
          }, 600);
        }
        refreshQueue();
      } else {
        toast('ثبت ناموفق: ' + ((r && r.err) || 'نامشخص'), 'err');
      }
      setSync('ok', 'همگام');
    })['catch'](function (err) { toast('خطا در ثبت پذیرش', 'err'); setSync('err', 'خطا'); });
  }

  /* --- collect the full record for C++ --- */
  function val(id) { var e = $(id); return e ? e.value : ''; }
  function trimEn(id) { return toEn(val(id)).replace(/^\s+|\s+$/g, ''); }
  function trimFa(id) { return trimStr(val(id)); }

  function collectRecord() {
    var totals = recompute();
    return {
      patient: {
        nid: trimEn('nid'), first: trimFa('first'), last: trimFa('last'),
        father: trimFa('father'), gender: val('gender'),
        birth: trimEn('birth'), mobile: trimEn('mobile'),
        phone: trimEn('phone'), addr: trimFa('addr')
      },
      hasIns: hasIns(),
      insMain: $('insMain') ? $('insMain').selectedIndex : -1,
      insSupp: $('insSupp') ? $('insSupp').selectedIndex : -1,
      insSuppPct: suppInsPct(),
      insBooklet: trimEn('insBooklet'),
      insValid: trimEn('insValid'),
      ptype: val('ptype'), insType: val('insType'),
      doc2code: trimEn('doc2code'), doc2name: trimFa('doc2name'),
      perfcode: trimEn('perfcode'), perfname: trimFa('perfname'),
      apptDate: trimEn('apptDate'), apptShift: val('apptShift'),
      rxDate: trimEn('rxDate'),
      noPay: $('noPay') ? $('noPay').checked : false,
      overrideBlock: state.overrideBlock,
      services: state.services,
      totals: totals
    };
  }

  function clearForm(opts) {
    opts = opts || {};
    /* v1.72.0: #docSearch / #docCode are retired (the #doc2code field is now the
       single doctor search field), so they are no longer cleared here. */
    var ids = ['nid', 'first', 'last', 'father', 'birth', 'mobile', 'phone', 'addr',
      'doc2code', 'perfcode', 'perfname', 'insBooklet', 'insValid', 'rxDate', 'apptDate',
      'svcSearch', 'qsNid', 'qsFile'];
    var i;
    for (i = 0; i < ids.length; i++) { if ($(ids[i])) $(ids[i]).value = ''; }
    if ($('doc2name')) $('doc2name').value = '';
    /* v1.78.0: refill the performer combo (it was previously left with only the
       placeholder — after one clear it stayed empty for the whole session). */
    fillPerformers();
    if ($('insSuppPct')) $('insSuppPct').value = '0';
    if ($('insMain')) $('insMain').selectedIndex = 0;
    if ($('insSupp')) $('insSupp').selectedIndex = 0;
    if ($('ptype')) $('ptype').selectedIndex = 0;
    if ($('insType')) $('insType').selectedIndex = 0;
    if ($('apptShift')) $('apptShift').selectedIndex = 0;
    if ($('gender')) $('gender').selectedIndex = 0;
    if ($('hasIns')) $('hasIns').checked = false;
    if ($('noPay')) $('noPay').checked = false;
    state.services = []; state.patient = null; state.catalog = []; state.overrideBlock = false;
    if (!opts.keepLock) {
      setFormLocked(false);
      state.loadedTicketId = '';
      state.reprintId = '';
      state.payTicketId = '';
      state.payCtx = null;
    }
    setText($('pfName'), 'بیمار جدید');
    setText($('pfFile'), '----');
    setText($('profileStateText'), 'برای شروع، مشخصات بیمار را وارد کنید');
    if ($('patResults')) $('patResults').innerHTML = '<div class="empty">نتیجه‌ای نیست</div>';
    if ($('docResults')) $('docResults').innerHTML = '';
    renderSvcSuggest([]);
    renderServices(); recompute();
    /* v1.75.0: after clearing, no field is selected → auto-focus کد ملی (nid)
       so the next patient can be looked up immediately. */
    if (!opts.skipFocus) autoFocusNid();
  }

  function updatePS(ps) {
    if (ps.P != null) { state.ps.P = ps.P; setText($('psPVal'), toFa(ps.P)); }
    if (ps.S != null) { state.ps.S = ps.S; setText($('psSVal'), toFa(ps.S)); }
  }

  /* v1.69.0: پیش‌نمایش نوبت — ثبت‌شده (صندوق نرفته‌ها) و در انتظار (صف پذیرش).
     Counts come from the existing queue.list IPC (one call per kind); the
     bridge contract is unchanged. setText no-ops if the elements are absent. */
  function updateTurnPreview() {
    Bridge.call('queue.list', { kind: 'unpaid', hours: 24 }).then(function (r) {
      setText($('tpReg'), toFa((r && r.rows) ? r.rows.length : 0));
    });
    Bridge.call('queue.list', { kind: 'admission', hours: 24 }).then(function (r) {
      setText($('tpWait'), toFa((r && r.rows) ? r.rows.length : 0));
    });
  }

  /* ==========================================================================
     C++ → JS events
     ========================================================================== */
  function subscribeEvents() {
    Bridge.on('patient.load', function (d) { fillPatient(d); });
    Bridge.on('services.update', function (d) {
      if (d.rows) {
        state.services = [];
        for (var ui = 0; ui < d.rows.length; ui++) addServiceRow(d.rows[ui]);
        renderServices(); recompute();
      }
    });
    Bridge.on('hotkey', function (d) {
      var k = (d && d.key) ? ('' + d.key) : '';
      if (k === 'F4') unlockCashForm();
    });
    Bridge.on('queue.update', function (d) { renderQueue(d.rows || []); updateTurnPreview(); });
    Bridge.on('ps.update', function (d) { updatePS(d); });
    Bridge.on('reception.settings', function (d) {
      if (d && (d.mode === 'full' || d.mode === 'simple')) {
        applyMode(d.mode);
        toast('تنظیمات نمایش پذیرش اعمال شد', 'ok');
      }
      if (state.surface === 'admission' && d && d.zoom != null && d.zoom !== '')
        applySavedZoom(d.zoom);
    });
    Bridge.on('native.print', function (d) {
      var kind=d && d.kind;
      if(kind==='insurance') Bridge.call('print.insurance',collectRecord());
      else if(kind==='rx') Bridge.call('print.rx',collectRecord());
    });
    Bridge.on('clock.update', function (d) {
      if (d.time) setText($('tbClock'), toFa(d.time));
      if (d.date) setText($('tbDate'), toFa(d.date));
    });
    // v1.93: live theme switching — C++ pushes 'theme.changed' when the user
    // picks light/dark/neon in settings. Update the body class in place so
    // all HTML surfaces reflect the new theme without a page reload.
    Bridge.on('theme.changed', function (d) {
      var b = document.body;
      if (!b) return;
      b.className = String(b.className || '')
        .replace(/\btheme-(dark|calm|warm|neon)\b/g, '').replace(/\s+/g, ' ');
      if (d.theme === 'dark') b.className += ' theme-dark';
      else if (d.theme === 'neon') b.className += ' theme-neon';
    });
    Bridge.on('insurance.update', function (d) {
      if (d.main) { state.insurances = d.main; fillSelect($('insMain'), d.main); }
      if (d.supp) { state.supp = d.supp; fillSelect($('insSupp'), d.supp); }
      recompute();
    });
    /* Debug-only: lets the headless --smoke-admission-keys test place a value in
       #nid and focus it, so the synthesized Enter keystroke exercises the real
       keydown → lookupNid path end-to-end. No effect in normal operation. */
    Bridge.on('debug.focusNid', function (d) {
      var el = $('nid');
      if (!el) return;
      if (d && d.nid != null) el.value = '' + d.nid;
      try { el.focus(); } catch (e) {}
      if (el.setSelectionRange) { try { el.setSelectionRange(0, (el.value || '').length); } catch (e2) {} }
    });
    /* LIVE service catalog sync from Management (add / edit / delete a service).
       We keep the freshest catalog and, if the suggestion dropdown is currently
       open, re-run the active search so the operator sees the change instantly —
       with NO page reload. Prices of already-added rows are refreshed too, so an
       admission in progress reflects a Management price edit immediately. */
    Bridge.on('catalog.update', function (d) {
      var rows = (d && (d.services || d.rows)) || [];
      state.fullCatalog = rows;
      /* refresh prices of rows already in the current admission */
      if (state.services.length) {
        var i, j, changed = false;
        for (i = 0; i < state.services.length; i++) {
          for (j = 0; j < rows.length; j++) {
            if (rows[j].code && rows[j].code === state.services[i].code) {
              var np = Number(rows[j].price) || 0;
              if (state.services[i].price !== np) { state.services[i].price = np; changed = true; }
              if (rows[j].name) state.services[i].name = rows[j].name;
              break;
            }
          }
        }
        if (changed) { renderServices(); recompute(); }
      }
      /* if the suggestion list is open, re-run the current query live */
      var sug = $('svcSuggest');
      if (sug && /open/.test(sug.className)) {
        var q = $('svcSearch') ? trimStr($('svcSearch').value) : '';
        if (q) Bridge.call('service.search', { q: q }).then(function (r) { renderSvcSuggest(r.rows || r.services || []); });
      }
      toast('فهرست خدمات به‌روزرسانی شد', 'ok');
    });
  }

  function fillSelect(sel, arr) {
    if (!sel) return;
    var cur = sel.selectedIndex, i, h = '';
    for (i = 0; i < arr.length; i++) {
      h += '<option value="' + esc(String(arr[i].id || i)) + '">' + esc(arr[i].name || '') + '</option>';
    }
    sel.innerHTML = h;
    if (cur >= 0 && cur < sel.options.length) sel.selectedIndex = cur;
  }

  /* ==========================================================================
     v1.93 — PORTAL MESSAGE WORKDESK
     The portal surface lists clinic messages (from the C++ message store) with
     severity stripes, unread dots, pin/save/print/reply/delete actions, a
     hamburger drawer (inbox / archive / pinned) and live search. Every bridge
     verb is wrapped in try-catch so a missing C++ handler never crashes the UI.
     ES5-only (var/function/string concat). Drives the #portalPanel markup the
     CSS agent adds to index.html; if those elements are absent yet, every
     helper no-ops safely ($ returns null -> guarded).
     ========================================================================== */
  var portal = {
    rows: [],        /* inbox messages (portal.messages.list) */
    saved: [],       /* saved+archive (portal.messages.list_saved) */
    view: 'inbox',   /* 'inbox' | 'archive' | 'saved' | 'pinned' */
    detailIdx: -1,   /* idx of the message open in #portalDetail */
    ctxIdx: -1,      /* idx targeted by the list context menu */
    peer: '',        /* conversation counterpart (from) */
    q: '',           /* current search filter (lower-cased) */
    wired: false     /* one-time DOM/bridge wiring guard */
  };

  /* severity: 0 = normal, 1 = urgent (amber), 2 = critical (red) */
  function portalStripe(type) {
    var t = Number(type) || 0;
    if (t === 2) return 'portal-sev-crit';
    if (t === 1) return 'portal-sev-urgent';
    return 'portal-sev-normal';
  }
  function portalStripeColor(type) {
    var t = Number(type) || 0;
    if (t === 2) return '#e53935';
    if (t === 1) return '#ffb300';
    return '#3b82f6';
  }
  function portalSeverityLabel(type) {
    var t = Number(type) || 0;
    if (t === 2) return 'بحرانی';
    if (t === 1) return 'فوری';
    return 'عادی';
  }

  /* unseen badge (#portalBadge) — fed by list responses and 'dash.unread' */
  function portalSetUnread(n) {
    var fa = 0; try { fa = (n | 0); } catch (e) { fa = 0; }
    var b = $('portalBadge');
    if (b) { b.textContent = toFa(fa); b.style.display = fa > 0 ? '' : 'none'; }
  }

  function portalTileHtml(m) {
    /* v1.97: WhatsApp-style list tile — priority BACKGROUND + date AND time. */
    var pv = String(m.text || '').replace(/\s+/g, ' ');
    pv = pv.replace(/^\s+|\s+$/g, '');
    if (pv.indexOf('\u21a9') === 0) pv = pv.substring(1);
    pv = pv.replace(/^\s+|\s+$/g, '');
    if (pv.length > 140) pv = pv.substring(0, 140) + '\u2026';
    var pin = m.pinned
      ? '<span class="portal-tile-pin" title="سنجاق‌شده">\u2605</span>'
      : '';
    var dot = (!m.seen) ? '<span class="portal-unread-dot"></span>' : '';
    var when = trimStr((m.date || '') + (m.time ? '  ' + m.time : ''));
    var act = (Number(m.idx) === Number(portal.detailIdx)) ? ' active' : '';
    return '<div class="portal-tile ' + portalStripe(m.type) + act + '" data-idx="' + esc(m.idx) + '">' +
      '<span class="portal-stripe"></span>' +
      '<div class="portal-tile-main">' +
        '<div class="portal-tile-row">' + dot +
          '<span class="portal-tile-from">' + esc(m.from || '—') + '</span>' + pin +
        '</div>' +
        '<div class="portal-tile-date">' + esc(when || '—') + '</div>' +
        '<div class="portal-tile-prev">' + esc(pv) + '</div>' +
      '</div>' +
    '</div>';
  }

  function portalSrc() {
    if (portal.view === 'archive' || portal.view === 'saved') return portal.saved;
    return portal.rows;
  }

  /* render the active view (inbox / archive / pinned) into #portalList, applying
     the live search filter. No bridge call — works on cached rows. */
  function portalRender() {
    var list = $('portalList'); if (!list) return;
    var src = portalSrc();
    var q = portal.q, out = [], i, m, hay;
    for (i = 0; i < src.length; i++) {
      m = src[i];
      if (portal.view === 'pinned' && !m.pinned) continue;
      if (q) {
        hay = String(m.from || '') + ' ' + String(m.text || '');
        if (hay.toLowerCase().indexOf(q) < 0) continue;
      }
      out.push(m);
    }
    /* v1.94: pinned messages sort to the top (pin = star = bring to top) */
    out.sort(function (a, b) { return (b.pinned ? 1 : 0) - (a.pinned ? 1 : 0); });
    if (!out.length) {
      list.innerHTML = '<div class="portal-empty">پیامی یافت نشد</div>';
      return;
    }
    var h = '', k;
    for (k = 0; k < out.length; k++) h += portalTileHtml(out[k]);
    list.innerHTML = h;
  }

  /* fetch the active view from C++ then render. inbox/pinned read the live list
     (pinned filters locally); archive reads the saved list. */
  function portalLoad() {
    if (portal.view === 'archive' || portal.view === 'saved') {
      try {
        Bridge.call('portal.messages.list_saved', {}).then(function (r) {
          portal.saved = (r && r.rows) || [];
          portalRender();
          if (r && r.unseen != null) portalSetUnread(r.unseen);
          if (portal.detailIdx >= 0) portalDetail(portal.detailIdx);
        }, function () { portalRender(); });
      } catch (e) { portalRender(); }
      return;
    }
    try {
      Bridge.call('portal.messages.list', {}).then(function (r) {
        portal.rows = (r && r.rows) || [];
        portalRender();
        if (r && r.unseen != null) portalSetUnread(r.unseen);
        if (portal.detailIdx >= 0) portalDetail(portal.detailIdx);
      }, function () { portalRender(); });
    } catch (e) { portalRender(); }
  }

  function portalCurrentMsg() {
    var src = portalSrc();
    var i;
    for (i = 0; i < src.length; i++) {
      if (Number(src[i].idx) === Number(portal.detailIdx)) return src[i];
    }
    return null;
  }

  function portalCloseDetail() {
    portal.detailIdx = -1;
    portal.peer = '';
    var det = $('portalDetail');
    if (det) det.innerHTML = '<div class="portal-empty">یک گفتگو را از فهرست انتخاب کنید</div>';
  }

  function portalToggleReply() {
    var box = $('portalReplyBox'); if (!box) return;
    var show = box.style.display === 'none' || !box.style.display;
    box.style.display = show ? '' : 'none';
    var ta = $('portalReplyText');
    if (ta && show) { try { ta.focus(); } catch (e) {} }
  }

  function portalIsOurs(m) {
    if (!m) return false;
    var txt = String(m.text || '');
    if (txt.indexOf('\u21a9') === 0) return true;
    var from = String(m.from || '');
    var us = state.userName || '';
    if (us && from === us) return true;
    return false;
  }

  function portalThread(seed) {
    var src = portalSrc();
    var who = String((seed && seed.from) || portal.peer || '');
    var us = state.userName || '';
    var out = [], i, m, from, to;
    for (i = 0; i < src.length; i++) {
      m = src[i];
      from = String(m.from || '');
      to = String(m.to || '');
      if (who && from === who) out.push(m);
      else if (portalIsOurs(m) && (from === us || to === who || from === who)) out.push(m);
    }
    if (!out.length && seed) out.push(seed);
    return out;
  }

  function portalSendReply() {
    var ta = $('portalReplyText'); if (!ta) return;
    var text = trimStr(ta.value);
    if (!text) { toast('متن پاسخ خالی است', 'err'); return; }
    cashAsk('آیا مطمئن هستید پیام را ارسال می‌کنید؟', function () {
      try {
        Bridge.call('portal.message.reply', { idx: portal.detailIdx, text: text }).then(function () {
          toast('پاسخ ارسال شد', 'ok');
          ta.value = '';
          portalLoad();
        }, function () { toast('ارسال پاسخ ناموفق بود', 'err'); });
      } catch (e) { toast('ارسال پاسخ ناموفق بود', 'err'); }
    });
  }

  /* open a full message in #portalDetail and mark it seen immediately (req. H).
     v1.94: redesigned as a messenger chat detail — avatar (initial letter),
     sender name + severity badge in the header, body in a chat bubble, a meta
     row (date / time / to), an action-button row, and a toggled reply box. */
  function portalDetail(idx) {
    var det = $('portalDetail'); if (!det) return;
    var m = null, i, src = portalSrc();
    for (i = 0; i < src.length; i++) { if (Number(src[i].idx) === Number(idx)) { m = src[i]; break; } }
    portal.detailIdx = m ? Number(m.idx) : -1;
    portal.peer = m ? String(m.from || '') : '';
    if (!m) { det.innerHTML = '<div class="portal-empty">پیام یافت نشد</div>'; return; }
    var initial = String(m.from || '؟').replace(/^\s+|\s+$/g, '').charAt(0) || '؟';
    var thread = portalThread(m);
    var bubbles = '', k, row, ours, body, when;
    for (k = 0; k < thread.length; k++) {
      row = thread[k];
      ours = portalIsOurs(row);
      body = String(row.text || '');
      if (body.indexOf('\u21a9') === 0) body = body.substring(1);
      when = trimStr((row.date || '') + (row.time ? '  ' + row.time : ''));
      bubbles += '<div class="portal-bub ' + (ours ? 'portal-bub-out' : 'portal-bub-in') + '">' +
        '<div class="portal-bub-txt">' + esc(body) + '</div>' +
        (when ? '<div class="portal-bub-meta">' + esc(when) + '</div>' : '') +
      '</div>';
    }
    var html = '<div class="portal-chat">' +
      '<div class="portal-chat-head">' +
        '<span class="portal-avatar">' + esc(initial) + '</span>' +
        '<span class="portal-detail-name">' + esc(m.from || '—') + '</span>' +
        '<span class="portal-detail-sev ' + portalStripe(m.type) + '">' + esc(portalSeverityLabel(m.type)) + '</span>' +
      '</div>' +
      '<div class="portal-chat-thread" id="portalThread">' + bubbles + '</div>' +
      '<div class="portal-chat-composer">' +
        '<textarea class="portal-reply-text" id="portalReplyText" rows="2" placeholder="نوشتن پیام…"></textarea>' +
        '<button type="button" class="portal-act portal-send" data-act="send">ارسال</button>' +
      '</div>' +
    '</div>';
    det.innerHTML = html;
    try { Bridge.call('portal.messages.seenone', { idx: portal.detailIdx }); } catch (e) {}
    m.seen = true;
    var threadEl = $('portalThread');
    if (threadEl) threadEl.scrollTop = threadEl.scrollHeight;
    portalRender(); /* refresh active tile */
  }

  /* delegated click handler for the detail action buttons */
  function portalDetailClick(e) {
    e = e || window.event;
    var det = $('portalDetail'); if (!det) return;
    var tgt = e.target || e.srcElement;
    var btn = findUp(tgt, 'data-act', det);
    if (!btn) return;
    var act = btn.getAttribute('data-act');
    var idx = portal.detailIdx;
    if (act === 'back') { portalCloseDetail(); }
    else if (act === 'reply') { portalToggleReply(); }
    else if (act === 'send') { portalSendReply(); }
    else if (act === 'pin') {
      var cur = portalCurrentMsg();
      var pin = cur ? !cur.pinned : true;
      try { Bridge.call('portal.messages.pin', { idx: idx, pin: pin }); } catch (e2) {}
      if (cur) cur.pinned = pin;
      defer(portalLoad);
    } else if (act === 'save') {
      try { Bridge.call('portal.messages.save', { idx: idx }); } catch (e2) {}
      toast('پیام ذخیره شد', 'ok');
    } else if (act === 'print') {
      try { Bridge.call('portal.message.print', { idx: idx }); } catch (e2) {}
    } else if (act === 'delete') {
      try { Bridge.call('portal.messages.delete', { idx: idx }); } catch (e2) {}
      toast('پیام حذف شد', 'ok');
      portalCloseDetail();
      defer(portalLoad);
    }
  }

  function portalToggleDrawer(force) {
    /* v1.94: optional `force` (true=open, false=close) so the click-outside
       handler can force-close. The right-side slide is driven by the `open`
       class (the CSS agent positions the drawer on the right). */
    var dr = $('portalDrawer'); if (!dr) return;
    var cn = String(dr.className || '');
    var isOpen = /(^|\s)open(\s|$)/.test(cn);
    var open;
    if (force === true) open = true;
    else if (force === false) open = false;
    else open = !isOpen;
    if (open && !isOpen) dr.className = cn + ' open';
    else if (!open && isOpen) dr.className = cn.replace(/\s*open/g, '');
  }

  function portalMarkView(view) {
    var dr = $('portalDrawer'); if (!dr) return;
    var items = dr.getElementsByTagName('button'), i, it, v, cls;
    for (i = 0; i < items.length; i++) {
      it = items[i];
      v = it.getAttribute('data-view');
      if (v == null) continue;
      cls = String(it.className || '').replace(/\s*active\b/g, '');
      if (v === view) cls += ' active';
      it.className = cls;
    }
  }

  function portalSwitchView(view) {
    portal.view = view || 'inbox';
    portal.q = '';
    var s = $('portalSearch'); if (s) s.value = '';
    var dr = $('portalDrawer'); if (dr) dr.className = String(dr.className || '').replace(/\s*open/g, '');
    portalMarkView(portal.view);
    portalCloseDetail();
    portalLoad();
  }

  function portalOnSearch() {
    var s = $('portalSearch'); if (!s) return;
    portal.q = trimStr(s.value).toLowerCase();
    portalRender();
  }

  function portalHideCtx() {
    var m = $('portalCtx');
    if (m) m.style.display = 'none';
    portal.ctxIdx = -1;
  }

  function portalShowCtx(e, idx) {
    var menu = $('portalCtx'); if (!menu) return;
    portal.ctxIdx = Number(idx);
    var cur = null, src = portalSrc(), i;
    for (i = 0; i < src.length; i++) {
      if (Number(src[i].idx) === portal.ctxIdx) { cur = src[i]; break; }
    }
    var pinBtn = null, items = menu.getElementsByTagName('button'), k;
    for (k = 0; k < items.length; k++) {
      if (items[k].getAttribute('data-pact') === 'pin') pinBtn = items[k];
    }
    if (pinBtn) setText(pinBtn, (cur && cur.pinned) ? 'برداشتن سنجاق' : 'سنجاق');
    menu.style.display = 'block';
    var x = (e.clientX != null) ? e.clientX : 0;
    var y = (e.clientY != null) ? e.clientY : 0;
    var mw = menu.offsetWidth || 160, mh = menu.offsetHeight || 140;
    var vw = document.documentElement.clientWidth || 800;
    var vh = document.documentElement.clientHeight || 600;
    if (x + mw > vw) x = vw - mw - 8;
    if (y + mh > vh) y = vh - mh - 8;
    if (x < 8) x = 8; if (y < 8) y = 8;
    menu.style.left = x + 'px';
    menu.style.top = y + 'px';
  }

  function portalCtxAct(act) {
    var idx = portal.ctxIdx;
    portalHideCtx();
    if (idx < 0) return;
    var cur = null, src = portalSrc(), i;
    for (i = 0; i < src.length; i++) {
      if (Number(src[i].idx) === Number(idx)) { cur = src[i]; break; }
    }
    if (act === 'pin') {
      var pin = cur ? !cur.pinned : true;
      try { Bridge.call('portal.messages.pin', { idx: idx, pin: pin }); } catch (e2) {}
      if (cur) cur.pinned = pin;
      defer(portalLoad);
    } else if (act === 'save') {
      try { Bridge.call('portal.messages.save', { idx: idx }); } catch (e2) {}
      toast('پیام ذخیره شد', 'ok');
    } else if (act === 'print') {
      try { Bridge.call('portal.message.print', { idx: idx }); } catch (e2) {}
    } else if (act === 'delete') {
      try { Bridge.call('portal.messages.delete', { idx: idx }); } catch (e2) {}
      toast('پیام حذف شد', 'ok');
      if (Number(portal.detailIdx) === Number(idx)) portalCloseDetail();
      defer(portalLoad);
    }
  }

  /* one-time wiring of burger / list / detail / search / drawer + live events */
  function portalWire() {
    if (portal.wired) return;
    portal.wired = true;
    try { on($('portalBurger'), 'click', portalToggleDrawer); } catch (e) {}
    try {
      on($('portalList'), 'click', function (e) {
        e = e || window.event;
        var tgt = e.target || e.srcElement;
        var tile = findUp(tgt, 'data-idx', $('portalList'));
        if (tile) portalDetail(tile.getAttribute('data-idx'));
      });
    } catch (e) {}
    /* capture-phase so the shared contextmenu.js never steals the list tile */
    try {
      var plist = $('portalList');
      function onListCtx(e) {
        e = e || window.event;
        var tile = findUp(e.target || e.srcElement, 'data-idx', plist);
        if (!tile) return;
        if (e.preventDefault) e.preventDefault();
        if (e.stopPropagation) e.stopPropagation();
        e.cancelBubble = true;
        e.returnValue = false;
        portalShowCtx(e, tile.getAttribute('data-idx'));
        return false;
      }
      if (plist && plist.addEventListener) plist.addEventListener('contextmenu', onListCtx, true);
      else on(plist, 'contextmenu', onListCtx);
    } catch (e) {}
    try {
      on($('portalCtx'), 'click', function (e) {
        e = e || window.event;
        var btn = findUp(e.target || e.srcElement, 'data-pact', $('portalCtx'));
        if (btn) portalCtxAct(btn.getAttribute('data-pact'));
      });
    } catch (e) {}
    try {
      on(document, 'mousedown', function (e) {
        e = e || window.event;
        var menu = $('portalCtx');
        if (!menu || menu.style.display === 'none') return;
        var n = e.target || e.srcElement;
        while (n) { if (n === menu) return; n = n.parentNode; }
        portalHideCtx();
      });
    } catch (e) {}
    try { on($('portalDetail'), 'click', portalDetailClick); } catch (e) {}
    try {
      on($('portalSearch'), 'input', portalOnSearch);
      on($('portalSearch'), 'keyup', portalOnSearch);
    } catch (e) {}
    /* drawer items: any [data-view] inside #portalDrawer (inbox/archive/pinned) */
    try {
      var dr = $('portalDrawer');
      if (dr && dr.querySelectorAll) {
        var items = dr.querySelectorAll('[data-view]');
        var k;
        for (k = 0; k < items.length; k++) {
          (function (it) {
            on(it, 'click', function (e) {
              if (e) { try { e.preventDefault(); } catch (x) {} }
              portalSwitchView(it.getAttribute('data-view') || 'inbox');
            });
          })(items[k]);
        }
      }
    } catch (e) {}
    /* v1.94: click-outside-to-close for the right-side drawer. The drawer
       signals visibility via the `open` class (not inline display), so we test
       that class instead of style.display. Clicks inside the drawer or on the
       burger are ignored; everything else force-closes it. */
    try {
      on(document, 'click', function (e) {
        e = e || window.event;
        var tgt = e.target || e.srcElement;
        var dr = $('portalDrawer');
        var bg = $('portalBurger');
        if (!dr || !tgt) return;
        if (!/(^|\s)open(\s|$)/.test(String(dr.className || ''))) return; /* closed -> ignore */
        var p = tgt;
        while (p) { if (p === dr || p === bg) return; p = p.parentNode; }
        portalToggleDrawer(false);
      });
    } catch (e) {}
    /* live refresh (req. G): reload on new messages; keep the badge in sync */
    try {
      Bridge.on('portal.changed', function () { defer(portalLoad); });
      Bridge.on('dash.unread', function (d) { if (d && d.n != null) portalSetUnread(d.n); });
    } catch (e) {}
  }

  /* ==========================================================================
     v1.94 — BLACKLIST MANAGEMENT SURFACE
     Drives the #blPanel markup the CSS agent adds to index.html. Lists blocked
     patients (nid / name / reason / duration / date / status / unblock), adds
     new entries, unblocks by row or by NID, and live-filters the table. Every
     bridge verb is wrapped in try-catch so a missing C++ handler never crashes
     the UI. ES5-only (var/function/string concat). All element refs are guarded
     so the helpers no-op safely if the markup is absent.
     ========================================================================== */
  function blacklistRowHtml(r) {
    var nid = esc(r.nid || '');
    var active = true;
    if (r.status) active = String(r.status).toLowerCase() === 'active';
    else if (r.expired) active = false;
    return '<tr data-nid="' + nid + '">' +
      '<td class="bl-nid">' + toFa(nid) + '</td>' +
      '<td class="bl-name">' + esc((r.first || '') + ' ' + (r.last || '')) + '</td>' +
      '<td class="bl-reason">' + esc(r.reason || '') + '</td>' +
      '<td class="bl-dur">' + esc(r.duration || '') + '</td>' +
      '<td class="bl-date">' + esc(r.date || '') + '</td>' +
      '<td class="bl-status">' + (active ? 'فعال' : 'منقضی') + '</td>' +
      '<td class="bl-act"><button type="button" class="btn bl-unblock" data-act="unblock" data-nid="' + nid + '">رفع مسدودی</button></td>' +
    '</tr>';
  }

  function blacklistLoad() {
    var body = $('blBody');
    function fail() { if (body) body.innerHTML = '<tr><td colspan="7" class="bl-empty">بارگذاری ناموفق بود</td></tr>'; }
    if (!body) return;
    try {
      Bridge.call('blacklist.list', {}).then(function (r) {
        var rows = (r && r.rows) || (r && r.items) || [];
        if (!rows.length) {
          body.innerHTML = '<tr><td colspan="7" class="bl-empty">رکوردی یافت نشد</td></tr>';
          return;
        }
        var h = '', i;
        for (i = 0; i < rows.length; i++) h += blacklistRowHtml(rows[i]);
        body.innerHTML = h;
      }, fail);
    } catch (e) { fail(); }
  }

  function blacklistClearForm() {
    var ids = ['blNid', 'blFirst', 'blLast', 'blMobile', 'blDuration', 'blReason'], i, el;
    for (i = 0; i < ids.length; i++) { el = $(ids[i]); if (el) el.value = ''; }
  }

  function blacklistAdd() {
    var nid = trimStr(val('blNid'));
    if (!nid) { toast('کد ملی را وارد کنید', 'err'); return; }
    var rec = {
      nid: toEn(nid),
      first: trimStr(val('blFirst')),
      last: trimStr(val('blLast')),
      mobile: trimStr(val('blMobile')),
      duration: trimStr(val('blDuration')),
      reason: trimStr(val('blReason'))
    };
    try {
      Bridge.call('blacklist.add', rec).then(function (res) {
        if (res && res.ok) {
          toast('به لیست سیاه افزوده شد', 'ok');
          blacklistClearForm();
          blacklistLoad();
        } else {
          toast('افزودن ناموفق بود', 'err');
        }
      }, function () { toast('افزودن ناموفق بود', 'err'); });
    } catch (e) { toast('افزودن ناموفق بود', 'err'); }
  }

  function blacklistUnblock() {
    var nid = trimStr(val('blUnblockNid'));
    if (!nid) { toast('کد ملی را وارد کنید', 'err'); return; }
    nid = toEn(nid);
    try {
      Bridge.call('blacklist.remove', { nid: nid }).then(function (res) {
        if (res && res.ok) {
          toast('مسدودی رفع شد', 'ok');
          var f = $('blUnblockNid'); if (f) f.value = '';
          blacklistLoad();
        } else {
          toast('رفع مسدودی ناموفق بود', 'err');
        }
      }, function () { toast('رفع مسدودی ناموفق بود', 'err'); });
    } catch (e) { toast('رفع مسدودی ناموفق بود', 'err'); }
  }

  function blacklistSearch() {
    var s = $('blSearch'); if (!s) return;
    var q = trimStr(s.value).toLowerCase();
    var body = $('blBody'); if (!body) return;
    var rows = body.getElementsByTagName('tr'), i, hay;
    for (i = 0; i < rows.length; i++) {
      hay = (rows[i].textContent || rows[i].innerText || '').toLowerCase();
      rows[i].style.display = (q && hay.indexOf(q) < 0) ? 'none' : '';
    }
  }

  var _blWired = false;
  function blacklistWire() {
    if (_blWired) return;
    _blWired = true;
    try { on($('blAddBtn'), 'click', blacklistAdd); } catch (e) {}
    try { on($('blUnblockBtn'), 'click', blacklistUnblock); } catch (e) {}
    try { on($('blSearch'), 'input', blacklistSearch); } catch (e) {}
    try { on($('blSearch'), 'keyup', blacklistSearch); } catch (e) {}
    /* per-row unblock buttons — delegated on #blBody so re-renders stay wired */
    try {
      on($('blBody'), 'click', function (e) {
        e = e || window.event;
        var tgt = e.target || e.srcElement;
        var body = $('blBody');
        var btn = findUp(tgt, 'data-act', body);
        if (!btn || btn.getAttribute('data-act') !== 'unblock') return;
        var nid = btn.getAttribute('data-nid') || '';
        if (!nid) return;
        try {
          Bridge.call('blacklist.remove', { nid: nid }).then(function (res) {
            if (res && res.ok) { toast('مسدودی رفع شد', 'ok'); blacklistLoad(); }
            else toast('رفع مسدودی ناموفق بود', 'err');
          }, function () { toast('رفع مسدودی ناموفق بود', 'err'); });
        } catch (e2) { toast('رفع مسدودی ناموفق بود', 'err'); }
      });
    } catch (e) {}
  }

  /* ==========================================================================
     LOADER + BOOT
     ========================================================================== */
  var _loaderHidden = false;
  function hideLoader() {
    if (_loaderHidden) return; _loaderHidden = true;
    var el = $('loader'); if (!el) return;
    if (el.className.indexOf('hide') < 0) el.className += ' hide';
    setTimeout(function () { if (el) el.style.display = 'none'; }, 420);
  }
  function loaderText(t) { setText($('loaderText'), t); }

  function applySurfaceClass() {
    var surf = window.__azSurface || 'admission';
    /* v1.88: 'receipts' is its own native C++ tab now (opened from the tools
       grid); the same HTML file renders it full-page via body.surface-rc. */
    if (surf !== 'tools' && surf !== 'cashier' && surf !== 'queue' && surf !== 'receipts' && surf !== 'dash' && surf !== 'portal' && surf !== 'blacklist') surf = 'admission';
    state.surface = surf;
    var b = document.body;
    if (!b) return;
    var cls = String(b.className || '').replace(/\bsurface-(adm|tools|cash|queue|rc|dash|portal|bl)\b/g, '').replace(/\s+/g, ' ');
    if (surf === 'tools') cls += ' surface-tools';
    else if (surf === 'cashier') cls += ' surface-cash';
    else if (surf === 'queue') cls += ' surface-queue';
    else if (surf === 'receipts') cls += ' surface-rc';
    else if (surf === 'dash') cls += ' surface-dash';
    else if (surf === 'portal') cls += ' surface-portal';
    else if (surf === 'blacklist') cls += ' surface-bl';
    else cls += ' surface-adm';
    b.className = cls;
  }

  function boot() {
    applySurfaceClass();
    /* v1.92.0: load-failure detection. boot() runs after DOM ready, so the app
       shell (document.body + #app) MUST exist by now. If either is missing the
       HTML/CSS bundle failed to load or parse — report it as an HTML error
       (never a normal activity log) so it lands in the dedicated error log. */
    if (!document.body || !$('app')) {
      reportJsError(new Error('admission shell missing: body or #app not found at boot'), 'boot');
    }
    /* v1.89: reception DASHBOARD — app-icon actions, envelope unread badge,
       drawer with categories + search. Wired ONLY on the dash surface so the
       broadcast «dash.unread» event never touches other surfaces. */
    (function () {
      if ((window.__azSurface || '') !== 'dash') return;
      function dashSetUnread(n) {
        var fa = 0; try { fa = n | 0; } catch (e) { fa = 0; }
        var b1 = $('dashMailBadge'), b2 = $('dashTileBadge');
        var txt = toFa(fa);
        if (b1) {
          b1.textContent = txt;
          b1.className = 'dash-mail-badge' + (fa > 0 ? ' has' : '');
        }
        if (b2) {
          b2.textContent = txt;
          b2.className = 'dash-app-badge' + (fa > 0 ? ' has' : '');
        }
      }
      function openApp(kind) { Bridge.call('ui.openTab', { kind: kind }); }
      on($('dashNewPat'), 'click', function () { openApp('admission'); });
      on($('dashNewTab'), 'click', function () { openApp('empty'); });
      on($('dashPortal'), 'click', function () { openApp('portal'); });
      on($('dashMail'), 'click', function () { openApp('portal'); });
      /* v1.94: dashboard blacklist launcher */
      on($('dashBlacklist'), 'click', function () { openApp('blacklist'); });
      /* initial count + live sync from the native poll */
      Bridge.call('portal.unread', {}).then(function (d) {
        if (d && d.ok) dashSetUnread(d.n);
      });
      Bridge.on('dash.unread', function (d) { if (d && d.n != null) dashSetUnread(d.n); });
      /* drawer */
      var drawer = $('dashDrawer'), bk = $('dashDrawerBk');
      function openD() { if (drawer) drawer.className = 'dash-drawer open'; if (bk) bk.className = 'dash-drawer-bk open'; }
      function closeD() { if (drawer) drawer.className = 'dash-drawer'; if (bk) bk.className = 'dash-drawer-bk'; }
      on($('dashBurger'), 'click', function () {
        if (drawer && /(^|\s)open(\s|$)/.test(drawer.className)) closeD(); else openD();
      });
      on($('dashDrawerClose'), 'click', closeD);
      on($('dashDrawerBk'), 'click', closeD);
      var body = $('dashDrawerBody');
      if (body) {
        var items = body.getElementsByClassName('dash-drawer-item');
        for (var ii = 0; ii < items.length; ii++) {
          (function (it) {
            on(it, 'click', function () {
              var id = it.getAttribute('data-app');
              closeD();
              if (id === 'dashPortal') {
                var mail = $('dashMail');
                if (mail && mail.style.display !== 'none') mail.click();
                return;
              }
              var t = $(id);
              if (t && t.style.display !== 'none') t.click();
            });
          })(items[ii]);
        }
      }
      /* search — filters the app icons AND the drawer items */
      function norm(x) { return (x || '').replace(/\s+/g, ' ').toLowerCase(); }
      function dashFilter(q) {
        q = norm(q);
        var grid = $('dashGrid'), any = false, i, txt;
        if (grid) {
          var apps = grid.getElementsByClassName('dash-app');
          for (i = 0; i < apps.length; i++) {
            txt = norm(apps[i].getAttribute('data-name') + ' ' + (apps[i].textContent || apps[i].innerText));
            var hide = !!(q && txt.indexOf(q) < 0);
            apps[i].style.display = hide ? 'none' : '';
            if (!hide) any = true;
          }
        }
        var emp = $('dashEmpty');
        if (emp) emp.style.display = (q && !any) ? '' : 'none';
        if (body) {
          var dis = body.getElementsByClassName('dash-drawer-item');
          for (i = 0; i < dis.length; i++) {
            txt = norm(dis[i].textContent || dis[i].innerText);
            dis[i].style.display = (q && txt.indexOf(q) < 0) ? 'none' : '';
          }
          /* hide a category heading when every item under it is filtered out */
          var kids = body.childNodes, lastCat = null, lastVis = false;
          for (i = 0; i < kids.length; i++) {
            var k = kids[i];
            if (!k.className) continue;
            if (/(^|\s)dash-cat(\s|$)/.test(k.className)) {
              if (lastCat) lastCat.style.display = lastVis ? '' : 'none';
              lastCat = k; lastVis = false;
            } else if (/(^|\s)dash-drawer-item(\s|$)/.test(k.className)) {
              if (k.style.display !== 'none') lastVis = true;
            }
          }
          if (lastCat) lastCat.style.display = lastVis ? '' : 'none';
        }
      }
      on($('dashQ'), 'keyup', function () { dashFilter(this.value); });
      on($('dashQ'), 'input', function () { dashFilter(this.value); });
      on($('dashDrawerQ'), 'keyup', function () { dashFilter(this.value); });
      on($('dashDrawerQ'), 'input', function () { dashFilter(this.value); });
      /* permission gating — same ticks as the rest of the app */
      Bridge.ready(function () { /* bridge already up at boot */ });
    })();

    /* v1.93: portal message workdesk — wire the burger / search / drawer once
       on the portal surface (same one-time pattern as the dash IIFE above). */
    if ((window.__azSurface || '') === 'portal') portalWire();

    loaderText('در حال همگام‌سازی با برنامه…');
    Bridge.call('init', {}).then(function (r) {
      /* v1.90: dash/tools skip the admission form fill (selects, performers,
         queue, services, zoom) — those surfaces hide #appBody and the extra
         work was the main reason they hung on open. Theme/user/perms stay. */
      var light = (state.surface === 'dash' || state.surface === 'tools' || state.surface === 'portal' || state.surface === 'blacklist');
      if (!light) {
        if (r.insurances) { state.insurances = r.insurances; fillSelect($('insMain'), r.insurances); }
        if (r.supp) { state.supp = r.supp; fillSelect($('insSupp'), r.supp); }
      }
      if (r.date) setText($('tbDate'), toFa(r.date));
      if (r.time) setText($('tbClock'), toFa(r.time));
      if (!light) {
        if (r.patient) fillPatient(r.patient);
        if (r.services) {
          state.services = [];
          for (var ri = 0; ri < r.services.length; ri++) addServiceRow(r.services[ri]);
        }
        if (r.ps) updatePS(r.ps);
        applyMode(r.mode || 'simple');
        if (state.surface === 'admission') applySavedZoom(r.zoom || 80);
      }
      document.body.className = String(document.body.className || '')
        .replace(/\btheme-(dark|calm|warm|neon)\b/g, '').replace(/\s+/g, ' ');
      if (r.theme === 'dark') document.body.className += ' theme-dark';
      else if (r.theme === 'neon') document.body.className += ' theme-neon';
      else applyPalette(r.palette || 'blue');
      if (!light) {
        if ($('apptDate')) $('apptDate').value = toFa(r.date || '');
        if ($('rxDate') && !$('rxDate').value) $('rxDate').value = toFa(r.date || '');
        if ($('apptShift') && r.shift) { var si; for(si=0;si<$('apptShift').options.length;si++) if($('apptShift').options[si].text===r.shift){$('apptShift').selectedIndex=si;break;} }
      }
      state.todayJalali = r.date || '';
      state.role = Number(r.role) || 0;
      state.userName = r.user || r.username || '';
      if (state.role >= 1 && $('rcDelete')) $('rcDelete').style.display = '';
      if (!light) {
        /* invoice starts at ZERO until a service is added */
        renderServices(); recompute();
        /* v1.93: performers folded into init to eliminate a round-trip; the
           standalone call stays as a fallback when the folded data is absent. */
        if (r.performers) fillPerformers(r.performers);
        else fillPerformers();
      }
      /* v1.79.0 / v1.82.0: hide launchers when the matching tick is OFF
         (the button must not exist — not merely disabled). */
      if (r.perms && r.perms.cashier === false) {
        var ql = $('queueLauncher'); if (ql) ql.style.display = 'none';
      }
      /* v1.89: dashboard tiles honour the same access ticks */
      if (r.perms && r.perms.admission === false) {
        var dnp = $('dashNewPat'); if (dnp) dnp.style.display = 'none';
      }
      if (r.perms && r.perms.worklist === false) {
        var dpt = $('dashPortal'); if (dpt) dpt.style.display = 'none';
        var dml = $('dashMail'); if (dml) dml.style.display = 'none';
      }
      state.canCashView = !r.perms || r.perms.cashier_view !== false;
      state.canCashEdit = !r.perms || r.perms.cashier_edit !== false;
      if (!state.canCashView) {
        var tc = $('toolsCash'); if (tc) tc.style.display = 'none';
      }
      if (!state.canCashEdit) {
        var hideIds = ['cashManualBtn', 'cashShiftStart', 'cashShiftEnd', 'cashManualBox'];
        var hi;
        for (hi = 0; hi < hideIds.length; hi++) {
          if ($(hideIds[hi])) $(hideIds[hi]).style.display = 'none';
        }
      }
      if (!light) {
        /* v1.93: queue folded into init to eliminate a round-trip; refreshQueue
           stays as a fallback when the folded data is absent. */
        if (r.queue) { renderQueue(r.queue.rows || []); updateTurnPreview(); }
        else refreshQueue();
        wireColResize($('rcTable'), 'az.rc.widths');
        wireColResize(document.getElementById('cashWrap') ? document.getElementById('cashWrap').getElementsByTagName('table')[0] : null, 'az.cash.widths');
      }
      setSync('ok', 'همگام با برنامه');
      state.ready = true;
      hideLoader();
      if (state.surface === 'portal') {
        /* v1.93: portal message workdesk — fetch the inbox on surface boot */
        portalMarkView(portal.view || 'inbox');
        portalCloseDetail();
        portalLoad();
        return;
      }
      if (state.surface === 'blacklist') {
        /* v1.94: blacklist management surface — wire buttons + load the table */
        blacklistWire();
        blacklistLoad();
        return;
      }
      if (state.surface === 'dash') {
        /* dashboard: greet with the user name; badges sync via portal.unread */
        var du = $('dashUser');
        if (du && r.user) du.textContent = r.user;
      } else if (state.surface === 'tools') {
        showToolsHome();
      } else if (state.surface === 'receipts') {
        state.rcPage = 'receipts';
        showReceiptsPage();
      } else if (state.surface === 'queue') {
        setOverlay('queuePanel', 'queueBackdrop', true);
        refreshQueue();
        toast('صندوق نرفته‌ها و صف پذیرش', 'ok');
      } else if (state.surface === 'cashier') {
        if (state.canCashView) {
          refreshCash();
          var cs = $('cashSearch'); if (cs) try { cs.focus(); } catch (e0) {}
        }
        toast('صندوق آماده است', 'ok');
      } else {
        var ticket = window.__azTicket;
        if (ticket && ticket.ok === false) ticket = null;
        if (ticket && ticket.ticket) ticket = ticket.ticket;
        if (ticket && (ticket.nid || ticket.first || ticket.id || ticket.barcode)) {
          clearForm();
          applyTicketToForm(ticket);
          toast('قبض در زبانه پذیرش باز شد', 'ok');
        } else {
          toast('پذیرش بیمار آماده است', 'ok');
        }
        setTimeout(autoFocusNid, 60);
      }
    })['catch'](function (err) {
      setSync('err', 'قطع ارتباط با برنامه');
      renderServices(); recompute();
      hideLoader();
      if (window.console) console.error('init failed', err);
      reportJsError(err, 'init');
    });
  }

  function domReady(fn) {
    if (document.readyState === 'complete' || document.readyState === 'interactive') setTimeout(fn, 1);
    else if (document.addEventListener) document.addEventListener('DOMContentLoaded', fn, false);
    else document.attachEvent('onreadystatechange', function () { if (document.readyState === 'complete') fn(); });
  }

  /* v1.50.0: idempotence guard — if any engine fires the ready event more than
     once (MSHTML readystatechange quirks), we must NEVER wire the delegated
     handlers twice (that would double-add services on every click). */
  var _wired = false;
  domReady(function () {
    if (_wired) return;
    _wired = true;
    wire();
    wireMinis();
    subscribeEvents();
    on($('blockClose'),'click',closeBlock);
    on($('blockOverride'),'click',function(){closeBlock();state.overrideBlock=true;saveAdmission();});
    /* v1.64.0 (درمان پلاس): «رفع مسدودی» — permanently remove the blacklist
       entry for the entered national id so the patient can be admitted normally
       from now on. The C++ side answers blacklist.remove. */
    on($('blockUnblock'),'click',function(){
      var rec = collectRecord();
      var nid = (rec && rec.nid) ? rec.nid : '';
      if(!nid){ toast('کد ملی وارد نشده است','err'); return; }
      Bridge.call('blacklist.remove', { nid: nid }).then(function (res) {
        if (res && res.ok) {
          closeBlock();
          toast('مسدودی این بیمار رفع شد', 'ok');
        } else {
          toast('رفع مسدودی ناموفق بود', 'err');
        }
      });
    });
    setActiveTab('tabQueue');
    renderServices();
    recompute();               /* zero invoice on open */
    tickClock();
    setInterval(tickClock, 1000);
    Bridge.ready(boot);
    setTimeout(hideLoader, 8000);   /* safety net */
  });

  window.AzAdmission = { newPatient: newPatient };
})();
