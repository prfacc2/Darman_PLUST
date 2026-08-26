/* ============================================================================
   crm.js — CRM Management panel controller (v1.70.0)

   ES5-ONLY (no const/let/arrow/template literals/class) so it parses and runs
   on BOTH WebView2 (Chromium) and the MSHTML/Trident (IE11) fallback that ships
   with every Windows. Uses the shared shell runtime (common.js) as its single
   C++<->JS transport: AzBridge.call(verb, payload) -> window.external.azCall /
   chrome.webview.postMessage. Page modules register on window.Crm.pages and are
   routed by Crm.nav(). RTL is handled with direction:rtl (no layout flipping).
   ============================================================================ */
(function (global) {
  'use strict';

  var Crm = {
    pages: {},          /* pageId -> {title, render(host)} */
    state: { page: 'dashboard', data: {} }
  };

  /* ---- tiny helpers ----------------------------------------------------- */
  function esc(s) {
    s = (s == null) ? '' : ('' + s);
    return s.replace(/&/g, '&amp;').replace(/</g, '&lt;')
            .replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }
  function faDigits(s) {
    s = '' + (s == null ? '' : s);
    var map = ['۰','۱','۲','۳','۴','۵','۶','۷','۸','۹'];
    var out = '';
    for (var i = 0; i < s.length; i++) {
      var c = s.charAt(i);
      out += (c >= '0' && c <= '9') ? map[+c] : c;
    }
    return out;
  }
  function enDigits(s) {
    s = '' + (s == null ? '' : s);
    var out = '';
    for (var i = 0; i < s.length; i++) {
      var c = s.charCodeAt(i);
      if (c >= 0x06F0 && c <= 0x06F9) out += String.fromCharCode(c - 0x06F0 + 48);      /* ۰-۹ */
      else if (c >= 0x0660 && c <= 0x0669) out += String.fromCharCode(c - 0x0660 + 48);  /* ٠-٩ */
      else out += s.charAt(i);
    }
    return out;
  }
  function fmtMoney(n) {
    n = +n || 0;
    var neg = n < 0; if (neg) n = -n;
    var s = '' + Math.floor(n);
    var grp = '';
    while (s.length > 3) { grp = ',' + s.substr(s.length - 3) + grp; s = s.substr(0, s.length - 3); }
    return (neg ? '-' : '') + s + grp;
  }
  function el(tag, cls, html) {
    var e = document.createElement(tag);
    if (cls) e.className = cls;
    if (html != null) e.innerHTML = html;
    return e;
  }
  function $(id) { return document.getElementById(id); }

  Crm.esc = esc;
  Crm.faDigits = faDigits;
  Crm.enDigits = enDigits;
  Crm.fmtMoney = fmtMoney;
  Crm.el = el;
  Crm.$ = $;

  /* ---- bridge call ------------------------------------------------------ */
  Crm.call = function (verb, payload) {
    return global.AzBridge.call(verb, payload || {});
  };
  Crm.toast = function (text, kind, ms) {
    if (global.AzUi) global.AzUi.toast(text, kind, ms);
  };

  /* ---- navigation ------------------------------------------------------- */
  /* In-memory back stack so the «بازگشت» button can step to the previous page.
     A plain nav pushes where we came from; a back nav is flagged silent so it
     does not re-push and loop. */
  Crm._hist = [];

  Crm.nav = function (pageId, opts) {
    opts = opts || {};
    var p = Crm.pages[pageId];
    if (!p) { Crm.toast('صفحه یافت نشد: ' + pageId, 'err'); return; }
    if (!opts.silent && Crm.state.page && Crm.state.page !== pageId) {
      Crm._hist.push(Crm.state.page);
    }
    Crm.state.page = pageId;
    /* Active state is toggled on every nav target that carries data-page —
       the quick-access tiles, the definitions-hub buttons AND the hamburger
       drawer items, so the redesign needs no special-casing here. */
    var items = document.querySelectorAll('[data-page]');
    for (var i = 0; i < items.length; i++) {
      var on = items[i].getAttribute('data-page') === pageId;
      var base = items[i].className.replace(/\s+active/g, '');
      items[i].className = base + (on ? ' active' : '');
    }
    var host = $('crmPage');
    if (!host) return;
    host.innerHTML = '';
    try { p.render(host); }
    catch (e) { if (global.console) console.error(e); host.innerHTML = '<div class="crm-banner err">خطا در بارگذاری صفحه.</div>'; }
    host.scrollTop = 0;
    /* keep the top-bar search in sync with whatever page we land on */
    var s = $('navSearch');
    if (s && pageId !== 'dashboard') { s.value = ''; }
  };

  /* step back to the previous page (or home when the stack is empty) */
  Crm.back = function () {
    var prev = Crm._hist.pop();
    Crm.nav(prev || 'dashboard', { silent: true });
  };

  /* jump back to the categorized home (dashboard) — used by the top-bar logo */
  Crm.home = function () { Crm.nav('dashboard'); };

  /* ---- shared page header builder --------------------------------------- */
  Crm.head = function (host, title, sub) {
    var h = el('div', 'crm-page-head');
    /* بازگشت — a back button is prepended on every page except the dashboard
       (home). It pops the in-memory nav stack so the operator returns to
       whatever page/category they came from. The chevron points right because
       the panel is RTL (back = towards the reading start). */
    if (Crm.state.page !== 'dashboard') {
      var back = el('button', 'crm-back');
      back.setAttribute('type', 'button');
      back.innerHTML =
        '<span class="crm-back-ic"><svg viewBox="0 0 24 24" width="18" height="18">' +
        '<path fill="currentColor" d="M10 6L8.59 7.41 13.17 12l-4.58 4.59L10 18l6-6z"/></svg></span>' +
        '<span class="crm-back-lbl">بازگشت</span>';
      back.onclick = function () { Crm.back(); };
      h.appendChild(back);
    }
    h.appendChild(el('h2', 'crm-page-title', esc(title)));
    if (sub) h.appendChild(el('span', 'crm-page-sub', esc(sub)));
    host.appendChild(h);
    return h;
  };

  /* ---- hamburger nav drawer -------------------------------------------- */
  /* v1.77: the categorized slide-out drawer builder + open/close/toggle live
     in crm_menu.js (a focused module). They attach Crm.openMenu / Crm.closeMenu /
     Crm.toggleMenu here, on this same Crm object, at load time — before the
     AzBoot.ready boot callback ever fires — so wireNav() finds them in place. */

  /* ---- simple modal helper (returns the card + a close fn) -------------- */
  /* v1.77: Crm.modal / Crm.confirm / Crm.alert live in crm_dialogs.js (a
     focused module). They attach onto this same Crm object at load time, before
     any page renders, so every page module that calls Crm.modal / Crm.confirm
     keeps working unchanged. */

  /* ---- table helper ----------------------------------------------------- */
  Crm.table = function (columns, rows) {
    /* columns: [{key,label,cls,render(row)}]; rows: array */
    var wrap = el('div', 'crm-table-wrap');
    var tbl = el('table', 'crm-tbl');
    var thead = el('thead'); var tr = el('tr');
    for (var c = 0; c < columns.length; c++) {
      var th = el('th', columns[c].cls || '', esc(columns[c].label));
      tr.appendChild(th);
    }
    thead.appendChild(tr); tbl.appendChild(thead);
    var tbody = el('tbody');
    if (!rows || !rows.length) {
      var empty = el('tr'); var td = el('td', 'empty', 'موردی یافت نشد');
      td.setAttribute('colspan', '' + columns.length);
      empty.appendChild(td); tbody.appendChild(empty);
    } else {
      for (var r = 0; r < rows.length; r++) {
        var row = rows[r]; var trow = el('tr');
        for (var k = 0; k < columns.length; k++) {
          var col = columns[k];
          var cell = el('td', col.cls || '');
          if (typeof col.render === 'function') {
            var html = col.render(row, r);
            if (html && html.nodeType) cell.appendChild(html);
            else cell.innerHTML = html == null ? '' : html;
          } else {
            cell.innerHTML = esc(row[col.key]);
          }
          trow.appendChild(cell);
        }
        tbody.appendChild(trow);
      }
    }
    tbl.appendChild(tbody); wrap.appendChild(tbl);
    return wrap;
  };

  /* ---- a pill span ------------------------------------------------------ */
  /* v1.80.0: shared clinical-tree helpers (بخش/زیربخش) — used by the personnel
     and accounts pages (and any future page) so the indented dropdown build
     exists in exactly one place. */
  Crm.sectTop = function (list) {
    var t = [], i;
    for (i = 0; i < list.length; i++) if (!list[i].parentId) t.push(list[i]);
    return t;
  };
  Crm.sectSubs = function (list, parentId) {
    var t = [], i;
    for (i = 0; i < list.length; i++) if (+list[i].parentId === +parentId) t.push(list[i]);
    return t;
  };
  /* «همه» / «در حالت تعلیق» / بخش‌ها with indented زیربخش‌ها */
  Crm.sectFilterOptions = function (list, sel) {
    var o = '<option value="">همه بخش‌ها</option>' +
            '<option value="__none__"' + (sel === '__none__' ? ' selected' : '') + '>در حالت تعلیق</option>';
    var tops = Crm.sectTop(list), i, j, subs;
    for (i = 0; i < tops.length; i++) {
      o += '<option value="' + esc(tops[i].id) + '"' + (tops[i].id === sel ? ' selected' : '') + '>' +
           esc(tops[i].name) + '</option>';
      subs = Crm.sectSubs(list, tops[i].id);
      for (j = 0; j < subs.length; j++)
        o += '<option value="' + esc(subs[j].id) + '"' + (subs[j].id === sel ? ' selected' : '') + '>' +
             '↳ ' + esc(subs[j].name) + '</option>';
    }
    return o;
  };

  Crm.pill = function (text, kind) {
    return '<span class="crm-pill ' + (kind || 'info') + '">' + esc(text) + '</span>';
  };

  /* ---- init / boot ------------------------------------------------------ */
  function applyInit(d) {
    /* The CRM top bar (clock, user label, theme button) was removed — date/time
       and appearance are owned by the C++ shell settings, so showing them here
       was redundant. We still honour the persisted theme C++ sends on crm.init
       so the page matches the operator's chosen appearance. */
    if (typeof d.theme === 'string' && d.theme) {
      Crm._theme = d.theme;
      Crm._dark = d.theme === 'dark' || d.theme === 'neon';
      if (global.AzBoot) global.AzBoot.applyTheme(d.theme);
    }
    /* v1.76: the brand label next to the hamburger shows the logged-in
       management account's display name (first + last name, else username) —
       the account they signed in with — instead of a static clinic title.
       v1.77: the identity chip now also shows the access LEVEL (نقش) beside the
       name — 'مدیریت' for role 1, 'پذیرش' otherwise — so the header reads as one
       clean "user name + access level" element instead of loose parts. */
    var who = $('navUserName');
    if (who) {
      var nm = (typeof d.user === 'string' && d.user) ? d.user
             : (typeof d.username === 'string' ? d.username : '');
      who.innerHTML = esc(nm || 'مدیریت درمانگاه');
    }
    var roleEl = $('navUserRole');
    if (roleEl) {
      roleEl.innerHTML = esc(d.role === 1 ? 'مدیریت' : 'پذیرش');
    }
    var badge = $('navMsgBadge');
    if (badge) {
      var n = +d.messages || 0;
      if (n > 0) { badge.style.display = ''; badge.innerHTML = faDigits('' + n); }
      else badge.style.display = 'none';
    }
    Crm.state.data = d;
  }

  function hideLoader() {
    var ldr = $('loader');
    var app = $('app');
    if (app) app.setAttribute('aria-hidden', 'false');
    if (app) app.className = 'crm-app ready';
    if (ldr) {
      ldr.className = 'crm-loader fade';
      setTimeout(function () { if (ldr.parentNode) ldr.parentNode.removeChild(ldr); }, 280);
    }
  }

  function boot() {
    Crm.call('crm.init', {}).then(function (d) {
      applyInit(d || {});
      hideLoader();
      Crm.nav('dashboard');
    }, function (e) {
      if (global.console) console.error(e);
      var lt = $('loaderText');
      if (lt) lt.innerHTML = 'اتصال به پنل مدیریت ناموفق بود.';
      /* still try to show the shell so the operator sees the structure */
      hideLoader();
      Crm.nav('dashboard');
    });
  }

  /* ---- wire static controls --------------------------------------------- */
  function wireNav() {
    /* Every element that carries data-page navigates — covers the new
       quick-access tiles AND any legacy sidebar items. */
    var items = document.querySelectorAll('[data-page]');
    for (var i = 0; i < items.length; i++) {
      (function (btn) {
        btn.onclick = function () { Crm.nav(btn.getAttribute('data-page')); };
      })(items[i]);
    }
    var home = $('navHome');
    if (home) home.onclick = function () { Crm.home(); };
    /* v1.82.0: backup lives on its own HTML page (last hamburger item). */
    /* hamburger drawer: the top-bar button toggles it; the in-drawer close
       button, the backdrop and the Esc key all dismiss it. The drawer body
       itself is populated from Crm.categories when it first opens. */
    var burger = $('navMenu');
    if (burger) burger.onclick = function () { Crm.toggleMenu(); };
    var mClose = $('crmMenuClose');
    if (mClose) mClose.onclick = function () { Crm.closeMenu(); };
    var mBackdrop = $('crmMenuBackdrop');
    if (mBackdrop) mBackdrop.onclick = function () { Crm.closeMenu(); };
    function onKey(ev) {
      ev = ev || window.event;
      if (ev.keyCode === 27) Crm.closeMenu();      /* Esc */
    }
    if (document.addEventListener) document.addEventListener('keydown', onKey);
    else if (document.attachEvent) document.attachEvent('onkeydown', onKey);
    /* top-bar global search: jump home and filter the tile grid live */
    var search = $('navSearch');
    if (search) {
      search.onkeyup = function () {
        if (Crm.state.page !== 'dashboard') {
          Crm.nav('dashboard');           /* nav renders the (filtered) home */
        } else {
          var host = $('crmPage');
          if (host) { host.innerHTML = ''; Crm.pages.dashboard.render(host); }
        }
        var s2 = $('navSearch');
        if (s2) s2.focus();
      };
    }
  }

  /* ---- listen for C++ push events --------------------------------------- */
  if (global.AzBridge && global.AzBridge.on) {
    global.AzBridge.on('crm.refresh', function () {
      var p = Crm.pages[Crm.state.page];
      var host = $('crmPage');
      if (p && host && typeof p.render === 'function') {
        host.innerHTML = ''; p.render(host);
      }
    });
    global.AzBridge.on('theme.changed', function (d) {
      var th = (d && d.theme) ? d.theme : 'light';
      Crm._theme = th;
      Crm._dark = th === 'dark' || th === 'neon';
      if (global.AzBoot) global.AzBoot.applyTheme(th);
    });
  }

  /* ---- start once the shell bridge transport is up ---------------------- */
  if (global.AzBoot) {
    global.AzBoot.ready(function () {
      wireNav();
      boot();
    });
  } else {
    /* standalone dev harness (no native bridge) — still render the shell */
    wireNav();
    hideLoader();
    Crm.nav('dashboard');
  }

  global.Crm = Crm;
})(window);
