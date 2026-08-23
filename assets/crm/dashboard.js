/* ============================================================================
   dashboard.js — CRM Dashboard / home (KPIs + categorized quick-access tiles).
   ES5-only. Registers on window.Crm.pages.dashboard. Calls crm.dashboard.
   v1.74: the retired right sidebar is replaced by this categorized tile grid;
   the top-bar search (#navSearch) filters the tiles live.
   ============================================================================ */
(function (global) {
  'use strict';
  var Crm = global.Crm;

  /* quick-access tiles grouped by category. Each tile navigates to its page.
     'ic' is an inline SVG path drawn inside a coloured badge. */
  var CATEGORIES = [
    { title: 'تعاریف', dot: '#7c3aed', tiles: [
      { page: 'sectionsHub', label: 'تعریف بخش و زیربخش', sub: 'بخش‌ها و زیربخش‌های درمانگاه', color: 'violet',
        ic: 'M4 4h7v7H4V4zm9 0h7v7h-7V4zM4 13h7v7H4v-7zm9 0h7v7h-7v-7z' },
      { page: 'insurance', label: 'تعریف بیمه', sub: 'بیمه پایه و تکمیلی', color: 'teal',
        ic: 'M12 2L4 5v6c0 5 3.4 9.5 8 11 4.6-1.5 8-6 8-11V5l-8-3zm-1 13l-3-3 1.4-1.4L11 12.2l4.6-4.6L17 9l-6 6z' }
    ] },
    { title: 'مدیریت روزمره', dot: '#2f6fe4', tiles: [
      { page: 'patients', label: 'بیماران', sub: 'جستجو و پرونده بیماران', color: '',
        ic: 'M12 12a5 5 0 100-10 5 5 0 000 10zm0 2c-4 0-9 2-9 6v2h18v-2c0-4-5-6-9-6z' },
      { page: 'doctors', label: 'پزشکان و پرستاران', sub: 'نیروی درمانی و قراردادها', color: 'green',
        ic: 'M19 3h-4.5v3H12V3H7a2 2 0 00-2 2v14a2 2 0 002 2h12a2 2 0 002-2V5a2 2 0 00-2-2zm-6 14h-2v-2h2v2zm0-4h-2V8h2v5z' },
      { page: 'services', label: 'خدمات و تعرفه‌ها', sub: 'تعریف خدمات و قیمت‌ها', color: 'amber',
        ic: 'M5 3h14a2 2 0 012 2v14a2 2 0 01-2 2H5a2 2 0 01-2-2V5a2 2 0 012-2zm2 5v2h10V8H7zm0 4v2h10v-2H7zm0 4v2h6v-2H7z' },
      { page: 'calendar', label: 'تقویم کاری', sub: 'شروع و پایان شیفت صندوق هر بخش', color: 'amber',
        ic: 'M19 4h-1V2h-2v2H8V2H6v2H5a2 2 0 00-2 2v14a2 2 0 002 2h14a2 2 0 002-2V6a2 2 0 00-2-2zm0 16H5V10h14v10zM7 12h2v2H7v-2zm4 0h2v2h-2v-2z' }
    ] },
    { title: 'سیستم', dot: '#475569', tiles: [
      /* v1.79.0: the personnel registry + the rebuilt account workshop */
      { page: 'persons', label: 'تعریف پرسنل', sub: 'معرفی کامل پرسنل + کد پرسنلی خودکار', color: 'teal',
        ic: 'M12 12a5 5 0 100-10 5 5 0 000 10zm0 2c-4 0-9 2-9 6v2h18v-2c0-4-5-6-9-6zM19 8h-2V6h-1V4h1V2h2v2h1v2h-1v2h-2V8z' },
      { page: 'employees', label: 'تعریف حساب کاربری', sub: 'حساب‌های پرسنل، دسترسی‌ها و بخش‌ها', color: 'rose',
        ic: 'M16 11c1.66 0 3-1.34 3-3s-1.34-3-3-3-3 1.34-3 3 1.34 3 3 3zm-8 0c1.66 0 3-1.34 3-3S9.66 5 8 5 5 6.34 5 8s1.34 3 3 3zm0 2c-2.67 0-8 1.34-8 4v2h10v-2c0-1.1.45-2.1 1.18-2.83C10.5 13.4 8.9 13 8 13zm8 0c-.62 0-1.4.12-2.22.34A4.01 4.01 0 0116 17v2h8v-2c0-2.66-5.33-4-8-4z' },
      { page: 'messages', label: 'کارتابل', sub: 'صندوق پیام‌های دریافتی', color: 'slate',
        ic: 'M4 4h16v12H7l-3 3V4zm2 3v2h12V7H6zm0 4v2h8v-2H6z' },
      { page: 'settings', label: 'تنظیمات و پشتیبان', sub: 'پیکربندی و پشتیبان‌گیری', color: 'slate',
        ic: 'M19.4 13a7.8 7.8 0 000-2l2-1.6-2-3.4-2.5 1a7.8 7.8 0 00-1.7-1l-.4-2.6H9.2l-.4 2.6a7.8 7.8 0 00-1.7 1l-2.5-1-2 3.4L4.6 11a7.8 7.8 0 000 2l-2 1.6 2 3.4 2.5-1c.5.4 1.1.7 1.7 1l.4 2.6h4.4l.4-2.6c.6-.3 1.2-.6 1.7-1l2.5 1 2-3.4-2-1.6zM12 15.5A3.5 3.5 0 1112 8.5a3.5 3.5 0 010 7z' }
    ] }
  ];

  /* publish the categorized nav so crm.js can build the hamburger drawer */
  Crm.categories = CATEGORIES;

  function kpi(label, value, foot, kind) {
    var card = Crm.el('div', 'crm-kpi' + (kind ? ' ' + kind : ''));
    card.innerHTML =
      '<div class="crm-kpi-label">' + Crm.esc(label) + '</div>' +
      '<div class="crm-kpi-value">' + Crm.faDigits(value) + '</div>' +
      (foot ? '<div class="crm-kpi-foot">' + Crm.esc(foot) + '</div>' : '');
    return card;
  }

  function tileNode(t) {
    var b = Crm.el('button', 'crm-tile');
    b.setAttribute('data-page', t.page);
    if (Crm.state.page === t.page) b.className += ' active';
    b.innerHTML =
      '<span class="crm-tile-ic ' + Crm.esc(t.color || '') + '">' +
        '<svg viewBox="0 0 24 24" width="24" height="24"><path fill="currentColor" d="' + t.ic + '"/></svg>' +
      '</span>' +
      '<span class="crm-tile-lbl">' + Crm.esc(t.label) + '</span>' +
      '<span class="crm-tile-sub">' + Crm.esc(t.sub) + '</span>';
    b.onclick = function () { Crm.nav(t.page); };
    return b;
  }

  function renderTiles(host, q) {
    var fq = (q || '').toLowerCase();
    for (var c = 0; c < CATEGORIES.length; c++) {
      var cat = CATEGORIES[c];
      /* match the category NAME itself — when the query hits a category title,
         every tile under it is shown so the whole group surfaces. */
      var catHit = !fq || ('' + cat.title).toLowerCase().indexOf(fq) >= 0;
      var matched = [];
      for (var i = 0; i < cat.tiles.length; i++) {
        var t = cat.tiles[i];
        if (!fq || catHit ||
            ('' + t.label).toLowerCase().indexOf(fq) >= 0 ||
            ('' + t.sub).toLowerCase().indexOf(fq) >= 0 ||
            ('' + t.page).toLowerCase().indexOf(fq) >= 0) matched.push(t);
      }
      if (!matched.length) continue;
      var wrap = Crm.el('div', 'crm-cat');
      var title = Crm.el('div', 'crm-cat-title');
      title.innerHTML = '<span class="dot" style="background:' + cat.dot + '"></span>' + Crm.esc(cat.title);
      wrap.appendChild(title);
      var grid = Crm.el('div', 'crm-tiles');
      for (var k = 0; k < matched.length; k++) grid.appendChild(tileNode(matched[k]));
      wrap.appendChild(grid);
      host.appendChild(wrap);
    }
    if (!host.childNodes.length) {
      host.appendChild(Crm.el('div', 'crm-banner info', 'موردی مطابق جستجو یافت نشد.'));
    }
  }

  Crm.pages.dashboard = {
    title: 'داشبورد',
    render: function (host) {
      var q = '';
      var s = Crm.$('navSearch');
      if (s) q = s.value;
      var fq = (q || '').toLowerCase();
      /* SEARCH MODE — while a query is typed, the dashboard KPIs and the full
         tile grid are HIDDEN; only categories / pages whose NAME or DESCRIPTION
         match the query are shown as search results. Clearing the box restores
         the normal KPI + tile dashboard. */
      if (fq) {
        Crm.head(host, 'نتایج جستجو', 'دسترسی‌ها و دسته‌های مطابق «' + q + '»');
        renderTiles(host, q);
        return;
      }
      Crm.head(host, 'داشبورد مدیریت', 'نمای کلی و دسترسی سریع');
      var kpis = Crm.el('div', 'crm-kpis');
      host.appendChild(kpis);
      kpis.appendChild(kpi('بیماران', '…', '', ''));
      kpis.appendChild(kpi('پزشکان و پرستاران', '…', '', 'k-green'));
      kpis.appendChild(kpi('خدمات', '…', '', 'k-amber'));
      kpis.appendChild(kpi('بخش‌ها', '…', '', 'k-violet'));

      Crm.call('crm.dashboard', {}).then(function (d) {
        kpis.innerHTML = '';
        kpis.appendChild(kpi('بیماران ثبت‌شده', d.patients || 0, 'مجموع پرونده‌ها', ''));
        kpis.appendChild(kpi('پزشکان و پرستاران', d.doctors || 0, 'نیروی درمانی', 'k-green'));
        kpis.appendChild(kpi('خدمات فعال', d.services || 0, 'فهرست خدمات درمانگاه', 'k-amber'));
        kpis.appendChild(kpi('بخش‌ها', d.sections || 0, 'بخش‌های تعریف‌شده', 'k-violet'));
        kpis.appendChild(kpi('کاربران سیستم', d.employees || 0, 'پذیرش و مدیریت', 'k-rose'));
        kpis.appendChild(kpi('پذیرش امروز', d.today || 0, 'تعداد پذیرش روز جاری', ''));
        kpis.appendChild(kpi('پیام‌های جدید', d.messages || 0, 'کارتابل خوانده‌نشده', 'k-green'));
        kpis.appendChild(kpi('درخواست‌های در انتظار', d.pendingReqs || 0, 'تنظیمات و پروفایل', 'k-amber'));

        renderTiles(host, q);
      }, function () {
        kpis.innerHTML = '';
        kpis.appendChild(Crm.el('div', 'crm-banner err', 'بارگذاری آمار ناموفق بود.'));
        renderTiles(host, q);
      });
    }
  };
})(window);
