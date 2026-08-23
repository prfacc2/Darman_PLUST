/* ============================================================================
   crm_menu.js — CRM hamburger nav drawer (v1.77 module, split out of crm.js).

   ES5-ONLY (no const/let/arrow/template literals/class) so it parses and runs
   on BOTH WebView2 (Chromium) and the MSHTML/Trident (IE11) fallback.

   This module owns the categorized slide-out drawer: it builds the drawer body
   from Crm.categories (published by dashboard.js) and attaches Crm.openMenu /
   Crm.closeMenu / Crm.toggleMenu onto the shared window.Crm object. It must load
   AFTER crm.js (which creates window.Crm and exposes Crm.el / Crm.esc / Crm.$)
   and BEFORE the AzBoot.ready boot callback fires — both guaranteed by the
   <script> order in index.html, since these handlers are only ever called from a
   user click or the Esc key, never at load time. No behaviour changed from the
   original inline implementation; the code was moved verbatim.
   ============================================================================ */
(function (global) {
  'use strict';
  var Crm = global.Crm;
  /* local aliases to the helpers crm.js exposes, so the moved body stays verbatim */
  var el = Crm.el, esc = Crm.esc, $ = Crm.$;

  /* ---- hamburger nav drawer -------------------------------------------- */
  /* The categorized navigation lives in a slide-out drawer (Crm.categories is
     published by dashboard.js, which owns the CATEGORIES table + icons). The
     drawer body is rebuilt on every open so the active item stays in sync. */
  function menuIcon(t, size) {
    return '<svg viewBox="0 0 24 24" width="' + size + '" height="' + size + '">' +
           '<path fill="currentColor" d="' + t.ic + '"/></svg>';
  }
  function menuItem(t) {
    var b = el('button', 'crm-menu-item');
    b.setAttribute('data-page', t.page);
    if (Crm.state.page === t.page) b.className += ' active';
    b.innerHTML =
      '<span class="crm-menu-ic ' + esc(t.color || '') + '">' + menuIcon(t, 20) + '</span>' +
      '<span class="crm-menu-lbl">' + esc(t.label) + '</span>';
    b.onclick = function () { Crm.nav(t.page); Crm.closeMenu(); };
    return b;
  }
  function buildMenu() {
    var body = $('crmMenuBody');
    if (!body) return;
    var cats = Crm.categories;
    if (!cats || !cats.length) return;
    body.innerHTML = '';
    /* dashboard / home entry first */
    var home = el('button', 'crm-menu-item');
    home.setAttribute('data-page', 'dashboard');
    if (Crm.state.page === 'dashboard') home.className += ' active';
    home.innerHTML =
      '<span class="crm-menu-ic home"><svg viewBox="0 0 24 24" width="20" height="20">' +
      '<path fill="currentColor" d="M10 20v-6h4v6h5v-8h3L12 3 2 12h3v8z"/></svg></span>' +
      '<span class="crm-menu-lbl">داشبورد</span>';
    home.onclick = function () { Crm.nav('dashboard'); Crm.closeMenu(); };
    body.appendChild(home);
    /* then every category and its tiles, grouped under a heading */
    for (var c = 0; c < cats.length; c++) {
      var cat = cats[c];
      var head = el('div', 'crm-menu-cat');
      head.innerHTML = '<span class="crm-menu-dot" style="background:' + cat.dot + '"></span>' +
                       '<span>' + esc(cat.title) + '</span>';
      body.appendChild(head);
      for (var i = 0; i < cat.tiles.length; i++) body.appendChild(menuItem(cat.tiles[i]));
    }
    /* v1.82.0: backup is always the last hamburger item (not a header button). */
    body.appendChild(menuItem({
      page: 'backup',
      label: 'پشتیبان‌گیری',
      color: 'slate',
      ic: 'M19 12v7H5v-7H3v7a2 2 0 002 2h14a2 2 0 002-2v-7h-2zm-6 .67l2.59-2.58L17 11.5 12 16.5 7 11.5l1.41-1.41L11 12.67V3h2v9.67z'
    }));
  }
  Crm.openMenu = function () {
    buildMenu();
    var m = $('crmMenu'), b = $('crmMenuBackdrop');
    if (m) { m.className = 'crm-menu open'; m.setAttribute('aria-hidden', 'false'); }
    if (b) b.className = 'crm-menu-backdrop open';
  };
  Crm.closeMenu = function () {
    var m = $('crmMenu'), b = $('crmMenuBackdrop');
    if (m) { m.className = 'crm-menu'; m.setAttribute('aria-hidden', 'true'); }
    if (b) b.className = 'crm-menu-backdrop';
  };
  Crm.toggleMenu = function () {
    var m = $('crmMenu');
    if (m && (' ' + m.className + ' ').indexOf(' open ') >= 0) Crm.closeMenu();
    else Crm.openMenu();
  };
})(window);
