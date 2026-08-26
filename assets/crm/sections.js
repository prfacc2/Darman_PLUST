/* ============================================================================
   sections.js — Sections (بخش) + Subsections (زیربخش) + definitions hub.
   ES5-only. List / search / add / edit / delete via crm.sections.* verbs.
   v1.74: navigation redesign adds a «تعریف بخش» hub (sectionsHub) that branches
   to تعریف بخش (top-level sections, parent_id==0) and تعریف زیر بخش (subsections,
   parent_id>0). The subsection form carries a parent-section dropdown so every
   زیربخش records which section it belongs to. File format (data\sections.dat)
   is owned by the C++ layer (Sections_Upsert / Sections_Delete); the UI only
   sends fields. The section type field stays a free-text input (v1.73 change).
   ============================================================================ */
(function (global) {
  'use strict';
  var Crm = global.Crm;

  /* cached top-level sections for the subsection parent dropdown */
  function fetchSections(onOk, onErr) {
    Crm.call('crm.sections.list', {}).then(function (d) { onOk(d.rows || []); },
      function () { if (onErr) onErr(); });
  }
  function parentName(rows, pid) {
    for (var i = 0; i < rows.length; i++) if (rows[i].id === pid) return rows[i].name;
    return '—';
  }

  /* ---------------- definitions hub (تعریف بخش / تعریف زیر بخش) -------------- */
  Crm.pages.sectionsHub = {
    title: 'تعریف بخش',
    render: function (host) {
      host.innerHTML = '';
      Crm.head(host, 'تعریف بخش و زیربخش', 'مدیریت بخش‌ها و زیربخش‌های درمانگاه');
      var hub = Crm.el('div', 'crm-hub');
      hub.appendChild(hubBtn('sections', 'تعریف بخش', 'افزودن و ویرایش بخش‌های اصلی درمانگاه',
        'M4 4h7v7H4V4zm9 0h7v7h-7V4zM4 13h7v7H4v-7zm9 0h7v7h-7v-7z', ''));
      hub.appendChild(hubBtn('subsections', 'تعریف زیر بخش', 'زیربخش‌های وابسته به هر بخش',
        'M6 2h12a2 2 0 012 2v16a2 2 0 01-2 2H6a2 2 0 01-2-2V4a2 2 0 012-2zm0 4v4h12V6H6zm0 6v4h12v-4H6zm2 6v2h2v-2H8z', 'green'));
      host.appendChild(hub);
    }
  };
  function hubBtn(page, title, sub, ic, color) {
    var b = Crm.el('button', 'crm-hub-btn');
    b.setAttribute('data-page', page);
    b.innerHTML =
      '<span class="crm-hub-ic ' + Crm.esc(color || '') + '"><svg viewBox="0 0 24 24" width="28" height="28"><path fill="currentColor" d="' + ic + '"/></svg></span>' +
      '<span><div class="crm-hub-txt-title">' + Crm.esc(title) + '</div><div class="crm-hub-txt-sub">' + Crm.esc(sub) + '</div></span>';
    b.onclick = function () { Crm.nav(page); };
    return b;
  }

  /* ---------------- sections (top-level, parent_id == 0) -------------------- */
  function loadSections(host, q) {
    Crm.call('crm.sections.list', {}).then(function (d) {
      renderSections(host, d.rows || [], q);
    }, function () {
      host.innerHTML = '';
      Crm.head(host, 'تعریف بخش', 'افزودن و ویرایش بخش‌های اصلی درمانگاه');
      host.appendChild(Crm.el('div', 'crm-banner err', 'بارگذاری بخش‌ها ناموفق بود.'));
    });
  }

  function renderSections(host, rows, q) {
    host.innerHTML = '';
    Crm.head(host, 'تعریف بخش', 'افزودن و ویرایش بخش‌های اصلی درمانگاه');
    var top = [];
    for (var i = 0; i < rows.length; i++) if (!rows[i].parentId) top.push(rows[i]);

    var tb = Crm.el('div', 'crm-toolbar');
    var search = Crm.el('div', 'crm-search');
    search.innerHTML =
      '<input class="crm-input" id="secQ" placeholder="جستجوی نام یا کد بخش…" value="' + Crm.esc(q || '') + '" />' +
      '<span class="crm-search-ic"><svg viewBox="0 0 24 24" width="16" height="16"><path fill="currentColor" d="M10 2a8 8 0 105.3 14L20 20.7 21.7 19l-4.7-4.7A8 8 0 0010 2zm0 2a6 6 0 110 12 6 6 0 010-12z"/></svg></span>';
    tb.appendChild(search);
    tb.appendChild(Crm.el('div', 'spacer', ''));
    var addBtn = Crm.el('button', 'crm-btn primary', '+ افزودن بخش');
    tb.appendChild(addBtn);
    host.appendChild(tb);

    var fq = (q || '').toLowerCase();
    var view = [];
    for (var k = 0; k < top.length; k++) {
      var r = top[k];
      if (!fq || ('' + r.name).toLowerCase().indexOf(fq) >= 0 ||
          ('' + r.code).toLowerCase().indexOf(fq) >= 0) view.push(r);
    }

    function isRec(s) {
      var k = ('' + (s.kind || '')).toLowerCase();
      var n = s.name || '';
      return k === 'reception' || n.indexOf('پذیرش') >= 0;
    }
    function recInfo(id) {
      var PAL = ['#2563eb','#0f766e','#7c3aed','#be123c','#c2410c','#0369a1','#4d7c0f','#6d28d9'];
      var n = 0, h = '', j, s, col;
      for (j = 0; j < rows.length; j++) {
        s = rows[j];
        if (s.parentId !== id || !s.active || !isRec(s)) continue;
        n++;
        col = PAL[(((+s.id || 0) * 2654435761) >>> 0) % 8];
        h += '<span class="crm-rec-chip" style="background:' + col + '">' + Crm.esc(s.name) + '</span> ';
      }
      return { n: n, chips: h || '<span class="crm-muted">—</span>' };
    }

    host.appendChild(Crm.table([
      { key: 'i', label: 'ردیف', cls: 'c-row', render: function (r, i) { return Crm.faDigits('' + (i + 1)); } },
      { key: 'code', label: 'کد', cls: 'c-mono', render: function (r) { return Crm.esc(r.code || '—'); } },
      { key: 'name', label: 'نام بخش', render: function (r) { return '<b>' + Crm.esc(r.name) + '</b>'; } },
      { key: 'kind', label: 'نوع', render: function (r) { return Crm.esc(r.kindLabel || r.kind || '—'); } },
      { key: 'rec', label: 'پذیرش‌ها', render: function (r) {
          var info = recInfo(r.id);
          return Crm.faDigits('' + info.n) + '<div class="crm-chips">' + info.chips + '</div>';
        } },
      { key: 'active', label: 'وضعیت', render: function (r) { return Crm.pill(r.active ? 'فعال' : 'غیرفعال', r.active ? 'on' : 'off'); } },
      { key: 'ops', label: 'عملیات', render: function (r) {
          var b = Crm.el('span');
          b.innerHTML = '<button class="crm-row-btn" data-act="edit">ویرایش</button>' +
                        '<button class="crm-row-btn danger" data-act="del">حذف</button>';
          b.childNodes[0].onclick = function () { openSectionModal(host, r); };
          b.childNodes[1].onclick = function () { delSection(host, r); };
          return b;
        } }
    ], view));

    var qEl = Crm.$('secQ');
    if (qEl) {
      qEl.onkeyup = function () { renderSections(host, rows, qEl.value); qEl.focus(); };
      if (q) { var v = qEl.value; qEl.value = ''; qEl.value = v; }
    }
    addBtn.onclick = function () { openSectionModal(host, null); };
  }

  function openSectionModal(host, sec) {
    var adding = !sec;
    var m = Crm.modal(adding ? 'افزودن بخش' : 'ویرایش بخش', null);
    var body = m.body;
    body.innerHTML =
      '<div class="crm-form">' +
      '<input type="hidden" id="fId" value="' + (sec ? sec.id : 0) + '" />' +
      '<div class="crm-field"><label class="crm-label">کد بخش</label>' +
        '<input class="crm-input" id="fCode" value="' + Crm.esc(sec ? sec.code : '') + '" placeholder="مثال: REC01" /></div>' +
      '<div class="crm-field"><label class="crm-label">نام بخش</label>' +
        '<input class="crm-input" id="fName" value="' + Crm.esc(sec ? sec.name : '') + '" placeholder="نام فارسی بخش" /></div>' +
      '<div class="crm-field"><label class="crm-label">نوع بخش</label>' +
        '<input class="crm-input" id="fKind" value="' + Crm.esc(sec ? sec.kind : '') + '" placeholder="مثال: عمومی، تزریقات، آزمایشگاه" /></div>' +
      '<div class="crm-field full"><label class="crm-check"><input type="checkbox" id="fActive" ' + (!sec || sec.active ? 'checked' : '') + ' />فعال</label></div>' +
      '<div class="crm-field full"><label class="crm-check"><input type="checkbox" id="fCashier" ' + (sec && sec.cashierTab ? 'checked' : '') + ' />ثبت پذیرش در صندوق</label></div>' +
      '<div class="crm-field full"><label class="crm-check"><input type="checkbox" id="fHasPos" ' + (sec && sec.hasPos ? 'checked' : '') + ' />بخش دستگاه پوز دارد</label></div>' +
      '</div>';
    var foot = Crm.el('div', 'crm-modal-foot');
    foot.innerHTML = '<button class="crm-btn ghost" id="mCancel">انصراف</button><button class="crm-btn primary" id="mSave">ذخیره</button>';
    m.card.appendChild(foot);
    Crm.$('mCancel').onclick = m.close;
    Crm.$('mSave').onclick = function () {
      var payload = {
        id: +Crm.enDigits(Crm.$('fId').value) || 0,
        parentId: 0,
        code: Crm.$('fCode').value,
        name: Crm.$('fName').value,
        kind: Crm.$('fKind').value,
        active: Crm.$('fActive').checked,
        cashierTab: !!(Crm.$('fCashier') && Crm.$('fCashier').checked),
        hasPos: !!(Crm.$('fHasPos') && Crm.$('fHasPos').checked)
      };
      if (!payload.name) { Crm.toast('نام بخش الزامی است.', 'err'); return; }
      Crm.call('crm.sections.save', payload).then(function () {
        Crm.toast(adding ? 'بخش اضافه شد.' : 'بخش ویرایش شد.', 'ok');
        m.close(); loadSections(host, '');
      }, function () { Crm.toast('ذخیره ناموفق بود.', 'err'); });
    };
  }

  function delSection(host, sec) {
    Crm.confirm('حذف بخش «' + sec.name + '»؟', function () {
      Crm.call('crm.sections.delete', { id: sec.id }).then(function () {
        Crm.toast('بخش حذف شد.', 'ok'); loadSections(host, '');
      }, function () { Crm.toast('حذف ناموفق بود.', 'err'); });
    }, { danger: true });
  }

  /* ---------------- subsections (parent_id > 0) ----------------------------- */
  function loadSubs(host, q) {
    fetchSections(function (rows) { renderSubs(host, rows, q); },
      function () {
        host.innerHTML = '';
        Crm.head(host, 'تعریف زیر بخش', 'زیربخش‌های وابسته به هر بخش');
        host.appendChild(Crm.el('div', 'crm-banner err', 'بارگذاری بخش‌ها ناموفق بود.'));
      });
  }

  function renderSubs(host, rows, q) {
    host.innerHTML = '';
    Crm.head(host, 'تعریف زیر بخش', 'زیربخش‌های وابسته به هر بخش');
    var subs = [];
    for (var i = 0; i < rows.length; i++) if (rows[i].parentId) subs.push(rows[i]);

    var tb = Crm.el('div', 'crm-toolbar');
    var search = Crm.el('div', 'crm-search');
    search.innerHTML =
      '<input class="crm-input" id="subQ" placeholder="جستجوی نام یا کد زیربخش…" value="' + Crm.esc(q || '') + '" />' +
      '<span class="crm-search-ic"><svg viewBox="0 0 24 24" width="16" height="16"><path fill="currentColor" d="M10 2a8 8 0 105.3 14L20 20.7 21.7 19l-4.7-4.7A8 8 0 0010 2zm0 2a6 6 0 110 12 6 6 0 010-12z"/></svg></span>';
    tb.appendChild(search);
    tb.appendChild(Crm.el('div', 'spacer', ''));
    var addBtn = Crm.el('button', 'crm-btn primary', '+ افزودن زیربخش');
    tb.appendChild(addBtn);
    host.appendChild(tb);

    var fq = (q || '').toLowerCase();
    var view = [];
    for (var k = 0; k < subs.length; k++) {
      var r = subs[k];
      if (!fq || ('' + r.name).toLowerCase().indexOf(fq) >= 0 ||
          ('' + r.code).toLowerCase().indexOf(fq) >= 0) view.push(r);
    }

    host.appendChild(Crm.table([
      { key: 'i', label: 'ردیف', cls: 'c-row', render: function (r, i) { return Crm.faDigits('' + (i + 1)); } },
      { key: 'parent', label: 'بخش مربوطه', render: function (r) { return '<b>' + Crm.esc(parentName(rows, r.parentId)) + '</b>'; } },
      { key: 'code', label: 'کد', cls: 'c-mono', render: function (r) { return Crm.esc(r.code || '—'); } },
      { key: 'name', label: 'نام زیربخش', render: function (r) { return Crm.esc(r.name); } },
      { key: 'kind', label: 'نوع', render: function (r) { return Crm.esc(r.kindLabel || r.kind || '—'); } },
      { key: 'active', label: 'وضعیت', render: function (r) { return Crm.pill(r.active ? 'فعال' : 'غیرفعال', r.active ? 'on' : 'off'); } },
      { key: 'ops', label: 'عملیات', render: function (r) {
          var b = Crm.el('span');
          b.innerHTML = '<button class="crm-row-btn" data-act="edit">ویرایش</button>' +
                        '<button class="crm-row-btn danger" data-act="del">حذف</button>';
          b.childNodes[0].onclick = function () { openSubModal(host, r, rows); };
          b.childNodes[1].onclick = function () { delSub(host, r); };
          return b;
        } }
    ], view));

    var qEl = Crm.$('subQ');
    if (qEl) {
      qEl.onkeyup = function () { renderSubs(host, rows, qEl.value); qEl.focus(); };
      if (q) { var v = qEl.value; qEl.value = ''; qEl.value = v; }
    }
    addBtn.onclick = function () { openSubModal(host, null, rows); };
  }

  function parentOptions(rows, sel) {
    var o = '<option value="0">— انتخاب بخش —</option>';
    for (var i = 0; i < rows.length; i++) {
      if (rows[i].parentId) continue;       /* only top-level sections */
      o += '<option value="' + rows[i].id + '"' + (rows[i].id === sel ? ' selected' : '') + '>' +
           Crm.esc(rows[i].name) + '</option>';
    }
    return o;
  }

  function openSubModal(host, sub, rows) {
    var adding = !sub;
    var m = Crm.modal(adding ? 'افزودن زیربخش' : 'ویرایش زیربخش', null);
    var body = m.body;
    body.innerHTML =
      '<div class="crm-form">' +
      '<input type="hidden" id="gId" value="' + (sub ? sub.id : 0) + '" />' +
      '<div class="crm-field"><label class="crm-label">بخش مربوطه</label>' +
        '<select class="crm-select" id="gParent">' + parentOptions(rows, sub ? sub.parentId : 0) + '</select></div>' +
      '<div class="crm-field"><label class="crm-label">کد زیربخش</label>' +
        '<input class="crm-input" id="gCode" value="' + Crm.esc(sub ? sub.code : '') + '" placeholder="مثال: REC01-1" /></div>' +
      '<div class="crm-field"><label class="crm-label">نام زیربخش</label>' +
        '<input class="crm-input" id="gName" value="' + Crm.esc(sub ? sub.name : '') + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">نوع</label>' +
        '<input class="crm-input" id="gKind" value="' + Crm.esc(sub ? sub.kind : '') + '" placeholder="مثال: تزریقات، ویزیت" /></div>' +
      '<div class="crm-field full"><label class="crm-check"><input type="checkbox" id="gActive" ' + (!sub || sub.active ? 'checked' : '') + ' />فعال</label></div>' +
      '<div class="crm-field full"><label class="crm-check"><input type="checkbox" id="gCashier" ' + (sub && sub.cashierTab ? 'checked' : '') + ' />ثبت پذیرش در صندوق</label></div>' +
      '<div class="crm-field full"><label class="crm-check"><input type="checkbox" id="gHasPos" ' + (sub && sub.hasPos ? 'checked' : '') + ' />زیر بخش دستگاه پوز دارد</label></div>' +
      '</div>';
    var foot = Crm.el('div', 'crm-modal-foot');
    foot.innerHTML = '<button class="crm-btn ghost" id="mCancel">انصراف</button><button class="crm-btn primary" id="mSave">ذخیره</button>';
    m.card.appendChild(foot);
    Crm.$('mCancel').onclick = m.close;
    Crm.$('mSave').onclick = function () {
      var payload = {
        id: +Crm.enDigits(Crm.$('gId').value) || 0,
        parentId: +Crm.enDigits(Crm.$('gParent').value) || 0,
        code: Crm.$('gCode').value,
        name: Crm.$('gName').value,
        kind: Crm.$('gKind').value,
        active: Crm.$('gActive').checked,
        cashierTab: !!(Crm.$('gCashier') && Crm.$('gCashier').checked),
        hasPos: !!(Crm.$('gHasPos') && Crm.$('gHasPos').checked)
      };
      if (!payload.name) { Crm.toast('نام زیربخش الزامی است.', 'err'); return; }
      if (!payload.parentId) { Crm.toast('انتخاب بخش مربوطه الزامی است.', 'err'); return; }
      Crm.call('crm.sections.save', payload).then(function () {
        Crm.toast(adding ? 'زیربخش اضافه شد.' : 'زیربخش ویرایش شد.', 'ok');
        m.close(); loadSubs(host, '');
      }, function () { Crm.toast('ذخیره ناموفق بود.', 'err'); });
    };
  }

  function delSub(host, sub) {
    Crm.confirm('حذف زیربخش «' + sub.name + '»؟', function () {
      Crm.call('crm.sections.delete', { id: sub.id }).then(function () {
        Crm.toast('زیربخش حذف شد.', 'ok'); loadSubs(host, '');
      }, function () { Crm.toast('حذف ناموفق بود.', 'err'); });
    }, { danger: true });
  }

  Crm.pages.sections = {
    title: 'تعریف بخش',
    render: function (host) { loadSections(host, ''); }
  };
  Crm.pages.subsections = {
    title: 'تعریف زیر بخش',
    render: function (host) { loadSubs(host, ''); }
  };
})(window);
