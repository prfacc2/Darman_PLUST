/* ============================================================================
   accounts.js — «تعریف حساب کاربری» (v1.79.0, replaces the old «کاربران»).
   ES5-only.
   ----------------------------------------------------------------------------
   The account workshop, driven by the personnel registry («تعریف پرسنل»):
     1) pick a department (or «همه» / «در حالت تعلیق») → its persons list
        renders LIVE;
     2) search them by کد پرسنلی / نام / کد ملی (one box, server matches all);
     3) pick a person → assign username + password + ACCESS TICKS (دسترسی‌ها);
     4) the account lands in the accounts table — username visible, password
        NEVER shown; clicking a name opens the person's sheet, clicking a
        department opens the department sheet.
   The organisational-department manager (بخش‌های سازمانی) that used to live on
   the old «کاربران» page is preserved below (nothing is lost).
   ============================================================================ */
(function (global) {
  'use strict';
  var Crm = global.Crm;

  /* the access ticks available today (keys must match userHasPerm in C++) */
  var PERMS = [
    { key: 'admission',    label: 'پذیرش بیمار',     hint: 'ثبت و صدور قبض پذیرش' },
    { key: 'worklist',     label: 'کارتابل',          hint: 'مشاهده پیام‌های مدیریت' },
    { key: 'cashier_view', label: 'دیدن صندوق',       hint: 'مشاهده صندوق و زبانه بخش‌ها' },
    { key: 'cashier_edit', label: 'تغییر در صندوق',   hint: 'پرداخت، سند دستی و ویرایش پس از F4' },
    { key: 'settings',     label: 'تنظیمات',          hint: 'تنظیمات پذیرش' }
  ];
  function permsLabel(p) {
    if (!p) return 'دسترسی کامل';
    if (p === '-') return 'بدون دسترسی';
    var map = {
      admission: 'پذیرش', worklist: 'کارتابل', cashier: 'صندوق',
      cashier_view: 'دیدن صندوق', cashier_edit: 'تغییر صندوق', settings: 'تنظیمات'
    };
    var parts = ('' + p).split(','), out = [], i;
    for (i = 0; i < parts.length; i++) if (map[parts[i]]) out.push(map[parts[i]]);
    return out.length ? out.join('، ') : 'دسترسی کامل';
  }

  /* ---- section/dept info sheet (shared) -----------------------------------
     v1.80.0: personnel live on the clinical sections tree, so try that first;
     fall back to the organisational DeptCat registry for legacy rows. */
  Crm.viewDeptInfo = function (deptId) {
    Crm.call('crm.sections.info', { id: deptId }).then(function (d) {
      if (d && d.ok) { renderSectionInfo(d); return; }
      legacyInfo(deptId);
    }, function () { legacyInfo(deptId); });
  };
  function legacyInfo(deptId) {
    Crm.call('crm.depts.info', { id: deptId }).then(function (d2) {
      if (d2 && d2.ok) renderDeptInfo(d2);
      else Crm.toast('اطلاعات بخش پیدا نشد.', 'err');
    }, function () { Crm.toast('اطلاعات بخش پیدا نشد.', 'err'); });
  }
  function renderSectionInfo(d) {
    var dep = d.section || {}, i;
    var m = Crm.modal('اطلاعات بخش — ' + (dep.name || ''), null);
    var subs = d.subs || [], ps = d.persons || [], us = d.users || [];
    var h = '<div class="crm-printable" id="deptSheet">' +
      '<div class="crm-sheet-head"><span class="crm-sheet-id">' +
        '<b>' + Crm.esc(dep.name || '') + '</b>' +
        '<span>' + Crm.esc(dep.kindLabel || '') + (subs.length ? ' — ' + Crm.faDigits('' + subs.length) + ' زیربخش' : '') + '</span>' +
        '<span class="crm-sheet-code">' + Crm.esc(dep.code || '') + '</span>' +
      '</span></div>';
    if (subs.length) {
      h += '<div class="crm-sheet-sub">زیربخش‌ها</div><div class="crm-chips">';
      for (i = 0; i < subs.length; i++) h += '<span class="crm-codechip">' + Crm.esc(subs[i].name) + '</span> ';
      h += '</div>';
    }
    var chips = d.receptionChips || [];
    h += '<div class="crm-sheet-sub">پذیرش‌های این بخش (' + Crm.faDigits('' + (d.receptionCount || chips.length || 0)) + ')</div><div class="crm-chips">';
    if (chips.length) {
      for (i = 0; i < chips.length; i++)
        h += '<span class="crm-rec-chip" style="background:' + Crm.esc(chips[i].color || '#2563eb') + '">' +
             Crm.esc(chips[i].name) + '</span> ';
    } else {
      h += '<span class="crm-muted">پذیرش ثبت‌شده‌ای زیر این بخش نیست.</span>';
    }
    h += '</div>';
    h += '<div class="crm-sheet-sub">پرسنل این بخش (' + Crm.faDigits('' + ps.length) + ' نفر)</div>' +
      '<table class="crm-sheet-tbl"><tr><td>کد پرسنلی</td><td>نام</td><td>نقش</td><td>مقام/سمت</td><td>زیربخش</td><td>حساب</td></tr>';
    for (i = 0; i < ps.length; i++) {
      h += '<tr><td>' + Crm.esc(ps[i].code) + '</td><td><b>' + Crm.esc((ps[i].firstName || '') + ' ' + (ps[i].lastName || '')) + '</b></td>' +
           '<td>' + Crm.esc(ps[i].roleLabel || '') + '</td><td>' + Crm.esc(ps[i].position || '—') + '</td>' +
           '<td>' + Crm.esc(ps[i].subName || '—') + '</td>' +
           '<td>' + Crm.esc(ps[i].username || '—') + '</td></tr>';
    }
    if (!ps.length) h += '<tr><td colspan="6">پرسنلی در این بخش ثبت نشده است.</td></tr>';
    h += '</table><div class="crm-sheet-sub">حساب‌های کاربری بخش</div>' +
         '<table class="crm-sheet-tbl"><tr><td>نام کاربری</td><td>نام کامل</td><td>وضعیت</td></tr>';
    for (i = 0; i < us.length; i++) {
      h += '<tr><td>' + Crm.esc(us[i].username) + '</td><td><b>' + Crm.esc(us[i].fullname) + '</b></td>' +
           '<td>' + (us[i].online ? 'آنلاین' : '—') + '</td></tr>';
    }
    if (!us.length) h += '<tr><td colspan="3">حسابی برای این بخش نیست.</td></tr>';
    h += '</table></div>';
    m.body.innerHTML = h;
    var foot = Crm.el('div', 'crm-modal-foot');
    foot.innerHTML = '<button class="crm-btn ghost" id="shClose">بستن</button>' +
                     '<button class="crm-btn outline" id="shPrint">چاپ اطلاعات بخش</button>';
    m.card.appendChild(foot);
    Crm.$('shClose').onclick = m.close;
    Crm.$('shPrint').onclick = function () { Crm.printNode('deptSheet', 'اطلاعات بخش — ' + (dep.name || '')); };
  }
  /* legacy organisational-dept sheet (kept for the بخش‌های سازمانی card) —
     renders an already-fetched crm.depts.info response. */
  function renderDeptInfo(d) {
    if (!d || !d.ok) { Crm.toast('اطلاعات بخش پیدا نشد.', 'err'); return; }
    var dep = d.dept || {}, i;
    var m = Crm.modal('اطلاعات بخش — ' + (dep.name || ''), null);
    var h = '<div class="crm-printable" id="deptSheet">' +
      '<div class="crm-sheet-head"><span class="crm-sheet-id">' +
        '<b>' + Crm.esc(dep.name || '') + '</b>' +
        '<span>مدیر بخش: ' + Crm.esc(dep.manager || '—') + '</span>' +
        '<span class="crm-sheet-code">' + Crm.esc(dep.id || '') + '</span>' +
      '</span></div>' +
      '<div class="crm-sheet-sub">پرسنل این بخش (' + Crm.faDigits('' + ((d.persons || []).length)) + ' نفر)</div>' +
      '<table class="crm-sheet-tbl"><tr><td>کد پرسنلی</td><td>نام</td><td>نقش</td><td>مقام/سمت</td><td>حساب کاربری</td></tr>';
    var ps = d.persons || [];
    for (i = 0; i < ps.length; i++) {
      h += '<tr><td>' + Crm.esc(ps[i].code) + '</td><td><b>' + Crm.esc((ps[i].firstName || '') + ' ' + (ps[i].lastName || '')) + '</b></td>' +
           '<td>' + Crm.esc(ps[i].roleLabel || '') + '</td><td>' + Crm.esc(ps[i].position || '—') + '</td>' +
           '<td>' + Crm.esc(ps[i].username || '—') + '</td></tr>';
    }
    if (!ps.length) h += '<tr><td colspan="5">پرسنلی در این بخش ثبت نشده است.</td></tr>';
    h += '</table><div class="crm-sheet-sub">حساب‌های کاربری بخش</div>' +
         '<table class="crm-sheet-tbl"><tr><td>نام کاربری</td><td>نام کامل</td><td>وضعیت</td></tr>';
    var us = d.users || [];
    for (i = 0; i < us.length; i++) {
      h += '<tr><td>' + Crm.esc(us[i].username) + '</td><td><b>' + Crm.esc(us[i].fullname) + '</b></td>' +
           '<td>' + (us[i].online ? 'آنلاین' : '—') + '</td></tr>';
    }
    if (!us.length) h += '<tr><td colspan="3">حسابی برای این بخش نیست.</td></tr>';
    h += '</table></div>';
    m.body.innerHTML = h;
    var foot = Crm.el('div', 'crm-modal-foot');
    foot.innerHTML = '<button class="crm-btn ghost" id="shClose">بستن</button>' +
                     '<button class="crm-btn outline" id="shPrint">چاپ اطلاعات بخش</button>';
    m.card.appendChild(foot);
    Crm.$('shClose').onclick = m.close;
    Crm.$('shPrint').onclick = function () { Crm.printNode('deptSheet', 'اطلاعات بخش — ' + (dep.name || '')); };
  }
  Crm.viewOrgDeptInfo = function (deptId) {
    Crm.call('crm.depts.info', { id: deptId }).then(renderDeptInfo,
      function () { Crm.toast('بارگذاری اطلاعات بخش ناموفق بود.', 'err'); });
  };

  /* ---- page state ---------------------------------------------------------- */
  var st = { dept: '', q: '', picked: null,
             fDept: '', fPerm: '', fQ: '', users: [], depts: [], totalUsers: 0,
             legacyDepts: [] };

  /* v1.79.0 review fix: filter changes re-render ONLY the accounts table —
     the toolbar inputs keep their value AND focus (a full load() used to blank
     the search box mid-typing and snap the selects back to «همه»). */
  function renderAccountsTable(host) {
    var box = Crm.$('accTableWrap');
    if (!box) return;
    box.innerHTML = '';
    box.appendChild(buildAccountsTable(host));
  }

  function load(host) {
    Crm.call('crm.employees.list', {}).then(function (d) {
      st.users = d.rows || [];
      st.depts = d.sects || [];            /* v1.80.0: clinical sections tree */
      st.legacyDepts = d.depts || [];      /* organisational DeptCat (legacy) */
      st.totalUsers = d.totalUsers || st.users.length;
      function go() { render(host, st.users, st.depts); }
      if (!st.depts.length) {
        Crm.call('crm.sections.list', {}).then(function (s) {
          var rows = s.rows || [], i;
          st.depts = [];
          for (i = 0; i < rows.length; i++) {
            st.depts.push({
              id: rows[i].id, name: rows[i].name, code: rows[i].code,
              parentId: rows[i].parentId || 0, kindLabel: rows[i].kindLabel
            });
          }
          go();
        }, go);
        return;
      }
      go();
    }, function () {
      host.innerHTML = '';
      Crm.head(host, 'تعریف حساب کاربری', 'ساخت و مدیریت حساب‌های پرسنل');
      host.appendChild(Crm.el('div', 'crm-banner err', 'بارگذاری حساب‌ها ناموفق بود.'));
    });
  }

  /* personnel pick-list: search by name / personnel code / national ID from
     the first character. People who already have an account stay visible but
     marked. No 2-character / department gate. */
  function loadPersonPick(host) {
    var box = Crm.$('accPersonList');
    var params = { q: st.q || '' };
    if (st.dept) params.deptId = st.dept;
    Crm.call('crm.persons.list', params).then(function (d) {
      var rows = d.rows || [];
      var box = Crm.$('accPersonList');
      if (!box) return;
      if (!rows.length) {
        box.innerHTML = '<div class="crm-empty-line">پرسنلی یافت نشد — ابتدا در «تعریف پرسنل» معرفی کنید.</div>';
        return;
      }
      var h = '';
      for (var i = 0; i < rows.length; i++) {
        var p = rows[i];
        var sel = st.picked && st.picked.code === p.code;
        h += '<button type="button" class="crm-pickbtn' + (sel ? ' sel' : '') + (p.username ? ' has-acc' : '') + '" data-code="' + Crm.esc(p.code) + '">' +
          '<span class="crm-codechip">' + Crm.esc(p.code) + '</span>' +
          '<span class="crm-pickbtn-name"><b>' + Crm.esc((p.roleLabel || '') + ' ' + (p.firstName || '') + ' ' + (p.lastName || '')) + '</b>' +
          '<small>' + Crm.esc((p.deptName || 'در حالت تعلیق') + (p.subName ? ' — ' + p.subName : '')) + '</small></span>' +
          (p.username ? '<span class="crm-pill on">' + Crm.esc(p.username) + '</span>' : '<span class="crm-pill off">بدون حساب</span>') +
        '</button>';
      }
      box.innerHTML = h;
      var items = box.querySelectorAll('.crm-pickbtn');
      for (var k = 0; k < items.length; k++) {
        (function (btn) {
          btn.onclick = function () {
            var code = btn.getAttribute('data-code');
            for (var j = 0; j < rows.length; j++) if (rows[j].code === code) st.picked = rows[j];
            if (st.picked && st.picked.username)
              Crm.toast('این پرسنل قبلاً حساب کاربری دارد: «' + st.picked.username + '»', 'err');
            renderPick(host, rows);
          };
        })(items[k]);
      }
    }, function () {});
  }
  function renderPick(host, rows) {
    loadPersonPick(host);   /* re-render list with the new selection */
    var info = Crm.$('accPickedInfo');
    if (info) {
      if (st.picked) {
        info.innerHTML = 'پرسنل انتخاب‌شده: <b>' + Crm.esc((st.picked.firstName || '') + ' ' + (st.picked.lastName || '')) +
          '</b> <span class="crm-codechip">' + Crm.esc(st.picked.code) + '</span>' +
          (st.picked.username ? ' <span class="crm-pill on">دارای حساب: ' + Crm.esc(st.picked.username) + '</span>' : '');
      } else info.innerHTML = 'هنوز پرسنلی انتخاب نشده است.';
    }
    var dis = !st.picked || !!st.picked.username;
    var u = Crm.$('accUser'), ps = Crm.$('accPass'), sv = Crm.$('accSave');
    if (u) u.disabled = dis; if (ps) ps.disabled = dis; if (sv) sv.disabled = dis;
  }

  /* the accounts table — rebuilt ALONE on every filter change so the toolbar
     inputs keep value + focus (see renderAccountsTable). */
  function buildAccountsTable(host) {
    return Crm.table([
      { key: 'i', label: 'ردیف', render: function (r, i) { return Crm.faDigits('' + (i + 1)); } },
      { key: 'username', label: 'نام کاربری', cls: 'c-mono', render: function (r) { return '<b class="crm-codechip">' + Crm.esc(r.username) + '</b>'; } },
      { key: 'fullname', label: 'نام و نام خانوادگی', render: function (r) {
          return '<button class="crm-link" data-person="' + Crm.esc(r.username) + '"' +
            (r.personCode ? ' data-personcode="' + Crm.esc(r.personCode) + '"' : '') +
            '><b>' + Crm.esc(r.fullname) + '</b></button>';
        } },
      { key: 'dept', label: 'بخش', render: function (r) { return Crm.esc(r.dept || '—'); } },
      { key: 'position', label: 'مقام/سمت', render: function (r) { return Crm.esc(r.position || '—'); } },
      { key: 'perms', label: 'دسترسی‌ها', render: function (r) {
          var lbl = permsLabel(r.perms);
          return Crm.pill(lbl, (!r.perms) ? 'on' : (r.perms === '-' ? 'off' : 'info'));
        } },
      { key: 'online', label: 'وضعیت', render: function (r) { return Crm.pill(r.online ? 'آنلاین' : '—', r.online ? 'on' : 'off'); } },
      { key: 'ops', label: 'عملیات', render: function (r) {
          var b = Crm.el('span');
          b.innerHTML = '<button class="crm-row-btn" data-act="edit">دسترسی/رمز</button>' +
                        '<button class="crm-row-btn danger" data-act="del">حذف</button>';
          b.childNodes[0].onclick = function () { openPermsModal(host, r); };
          b.childNodes[1].onclick = function () { del(host, r); };
          return b;
        } }
    ], filterUsers(st.users, st.depts));
  }

  function render(host, users, depts) {
    host.innerHTML = '';
    Crm.head(host, 'تعریف حساب کاربری', 'ساخت حساب برای پرسنل + مدیریت دسترسی‌ها');

    /* ================= Card 1: ساخت حساب ================================== */
    var c1 = Crm.el('div', 'crm-card');
    c1.innerHTML = '<div class="crm-card-title"><span class="dot"></span>ساخت حساب کاربری</div>' +
      '<div class="crm-accmode"><button type="button" class="crm-accmode-btn on" id="accModeP">حساب پرسنل</button>' +
      '<button type="button" class="crm-accmode-btn" id="accModeM">حساب مدیریت</button></div>';
    var form = Crm.el('div', 'crm-form');
    form.innerHTML =
      /* personnel mode */
      '<div class="crm-field accmode-p"><label class="crm-label">بخش</label>' +
        '<input class="crm-input" id="accSectQ" placeholder="جستجوی بخش…" />' +
        '<select class="crm-select" id="accDept">' + deptFilterOpts(depts) + '</select></div>' +
      '<div class="crm-field accmode-p"><label class="crm-label">جستجو (کد پرسنلی / نام / کد ملی)</label>' +
        '<input class="crm-input" id="accQ" placeholder="نام، کد پرسنلی یا کد ملی — بدون حساب هم پیدا می‌شود" /></div>' +
      '<div class="crm-field full accmode-p"><div class="crm-picklist" id="accPersonList"></div></div>' +
      '<div class="crm-field full accmode-p"><div class="crm-banner" id="accPickedInfo">هنوز پرسنلی انتخاب نشده است.</div></div>' +
      /* management mode (kept from the old «کاربران» page — nothing is lost) */
      '<div class="crm-field accmode-m" style="display:none"><label class="crm-label">نام کامل</label>' +
        '<input class="crm-input" id="accFullM" placeholder="مثلاً: مدیر مالی" /></div>' +
      /* shared */
      '<div class="crm-field"><label class="crm-label">نام کاربری</label><input class="crm-input c-mono" id="accUser" disabled /></div>' +
      '<div class="crm-field"><label class="crm-label">رمز عبور</label><input class="crm-input" id="accPass" type="text" disabled /></div>' +
      '<div class="crm-field full accmode-p"><label class="crm-label">دسترسی‌ها (تیک ندارد = آن قسمت برای این حساب نمایش داده نمی‌شود)</label>' +
        '<div class="crm-permrow" id="accPerms">' + permChecks(null) + '</div></div>';
    c1.appendChild(form);
    var f1 = Crm.el('div', 'crm-modal-foot');
    f1.style.padding = '0';
    f1.innerHTML = '<span class="spacer"></span><button class="crm-btn primary" id="accSave" disabled>ساخت حساب کاربری</button>';
    c1.appendChild(f1);
    host.appendChild(c1);

    /* ================= Card 2: حساب‌های موجود ============================== */
    var c2 = Crm.el('div', 'crm-card');
    c2.innerHTML = '<div class="crm-card-title"><span class="dot"></span>حساب‌های کاربری</div>';
    var tb2 = Crm.el('div', 'crm-toolbar');
    var fDept = Crm.el('div', 'crm-search'); fDept.style.maxWidth = '190px';
    fDept.innerHTML = '<select class="crm-select" id="afDept">' + deptFilterOpts(depts) + '</select>';
    var fPerm = Crm.el('div', 'crm-search'); fPerm.style.maxWidth = '170px';
    fPerm.innerHTML = '<select class="crm-select" id="afPerm">' +
      '<option value="">همه دسترسی‌ها</option>' +
      '<option value="full">دسترسی کامل</option>' +
      '<option value="admission">پذیرش بیمار</option><option value="worklist">کارتابل</option>' +
      '<option value="cashier_view">دیدن صندوق</option><option value="cashier_edit">تغییر در صندوق</option>' +
      '<option value="settings">تنظیمات</option>' +
      '<option value="none">بدون دسترسی</option></select>';
    var fQ = Crm.el('div', 'crm-search');
    fQ.innerHTML = '<input class="crm-input" id="afQ" placeholder="جستجوی نام کاربری / نام…" />' +
      '<span class="crm-search-ic"><svg viewBox="0 0 24 24" width="16" height="16"><path fill="currentColor" d="M10 2a8 8 0 105.3 14L20 20.7 21.7 19l-4.7-4.7A8 8 0 0010 2zm0 2a6 6 0 110 12 6 6 0 010-12z"/></svg></span>';
    tb2.appendChild(fDept); tb2.appendChild(fPerm); tb2.appendChild(fQ);
    c2.appendChild(tb2);
    var tw = Crm.el('div'); tw.id = 'accTableWrap';
    tw.appendChild(buildAccountsTable(host));
    c2.appendChild(tw);
    if (st.totalUsers > st.users.length)
      c2.appendChild(Crm.el('div', 'crm-banner',
        'فقط ' + Crm.faDigits('' + st.users.length) + ' حساب اول نمایش داده شد (از ' + Crm.faDigits('' + st.totalUsers) + ') — با فیلترها دقیق‌تر کنید.'));
    host.appendChild(c2);

    /* ================= Card 3: بخش‌ها و زیربخش‌ها (clinical tree) ==========
       v1.80.0: personnel live on the clinical sections tree («تعریف بخش و
       زیربخش»), so this card lists THAT tree with a live اطلاعات بخش sheet;
       زیربخش‌ها are managed in the dedicated sections page. The legacy
       organisational-dept manager (بخش‌های سازمانی) is kept below it. */
    var c3 = Crm.el('div', 'crm-card');
    c3.innerHTML = '<div class="crm-card-title"><span class="dot"></span>بخش‌ها و زیربخش‌ها</div>';
    var dtb = Crm.el('div', 'crm-toolbar');
    var dsearch = Crm.el('div', 'crm-search');
    dsearch.style.maxWidth = '260px';
    dsearch.innerHTML = '<input class="crm-input" id="sectName" placeholder="نام بخش جدید (مثلاً: عمومی)" />';
    dtb.appendChild(dsearch);
    var hint = Crm.el('small', 'crm-hint', 'زیربخش‌ها را در «تعریف بخش و زیربخش» تعریف کنید.');
    hint.style.marginRight = '10px';
    dtb.appendChild(hint);
    dtb.appendChild(Crm.el('div', 'spacer', ''));
    var dAdd = Crm.el('button', 'crm-btn outline', 'افزودن بخش');
    dtb.appendChild(dAdd);
    c3.appendChild(dtb);
    var tops = [], i2;
    for (i2 = 0; i2 < depts.length; i2++) if (!depts[i2].parentId) tops.push(depts[i2]);
    c3.appendChild(Crm.table([
      { key: 'i', label: 'ردیف', render: function (r, i) { return Crm.faDigits('' + (i + 1)); } },
      { key: 'name', label: 'نام بخش', render: function (r) { return '<b>' + Crm.esc(r.name) + '</b>'; } },
      { key: 'subs', label: 'زیربخش‌ها', render: function (r) {
          var n = 0, i;
          for (i = 0; i < depts.length; i++) if (+depts[i].parentId === +r.id) n++;
          return n ? Crm.faDigits('' + n) + ' زیربخش' : '—';
        } },
      { key: 'ops', label: 'عملیات', render: function (r) {
          var b = Crm.el('span');
          b.innerHTML = '<button class="crm-row-btn" data-act="info">اطلاعات بخش</button>';
          b.childNodes[0].onclick = function () { Crm.viewDeptInfo(r.id); };
          return b;
        } }
    ], tops));
    host.appendChild(c3);

    /* ================= Card 4: بخش‌های سازمانی (legacy — DeptCat) ============ */
    var c4 = Crm.el('div', 'crm-card');
    c4.innerHTML = '<div class="crm-card-title"><span class="dot"></span>بخش‌های سازمانی <small class="crm-hint">(قدمی — برای سازگاری با حساب‌های قبلی)</small></div>';
    var dtb4 = Crm.el('div', 'crm-toolbar');
    var dsearch4 = Crm.el('div', 'crm-search');
    dsearch4.style.maxWidth = '240px';
    dsearch4.innerHTML = '<input class="crm-input" id="deptName" placeholder="نام بخش سازمانی جدید" />';
    dtb4.appendChild(dsearch4);
    dtb4.appendChild(Crm.el('div', 'spacer', ''));
    var dAdd4 = Crm.el('button', 'crm-btn ghost', 'افزودن');
    dtb4.appendChild(dAdd4);
    c4.appendChild(dtb4);
    var legacy = st.legacyDepts;
    var tw4 = Crm.el('div'); tw4.id = 'legacyDeptWrap';
      tw4.appendChild(Crm.table([
        { key: 'i', label: 'ردیف', render: function (r, i) { return Crm.faDigits('' + (i + 1)); } },
        { key: 'name', label: 'نام بخش', render: function (r) { return '<b>' + Crm.esc(r.name) + '</b>'; } },
        { key: 'manager', label: 'مدیر بخش', render: function (r) { return Crm.esc(r.manager || '—'); } },
        { key: 'ops', label: 'عملیات', render: function (r) {
            var b = Crm.el('span');
            b.innerHTML = '<button class="crm-row-btn" data-act="info">اطلاعات</button>';
            b.childNodes[0].onclick = function () { Crm.viewOrgDeptInfo(r.id); };
            return b;
          } }
      ], legacy));
    c4.appendChild(tw4);
    host.appendChild(c4);

    /* wire card 1 */
    Crm.$('accDept').onchange = function () { st.dept = this.value; st.picked = null; loadPersonPick(host); renderPickInfoOnly(host); };
    if (Crm.$('accSectQ')) {
      var accDeptAll = [];
      (function () {
        var sel0 = Crm.$('accDept');
        var oi;
        if (sel0) {
          for (oi = 0; oi < sel0.options.length; oi++)
            accDeptAll.push({ value: sel0.options[oi].value, text: sel0.options[oi].text });
        }
      })();
      Crm.$('accSectQ').oninput = function () {
        var q = (this.value || '').toLowerCase();
        var sel = Crm.$('accDept');
        if (!sel) return;
        var keep = sel.value;
        var html = '', i, o, show, found = false;
        for (i = 0; i < accDeptAll.length; i++) {
          o = accDeptAll[i];
          show = !q || i === 0 || ((o.text || '').toLowerCase().indexOf(q) >= 0);
          if (!show) continue;
          html += '<option value="' + Crm.esc(o.value) + '"' +
            (o.value === keep ? ' selected="selected"' : '') + '>' +
            Crm.esc(o.text) + '</option>';
          if (o.value === keep) found = true;
        }
        sel.innerHTML = html;
        if (!found) {
          sel.selectedIndex = 0;
          st.dept = sel.value;
          st.picked = null;
          loadPersonPick(host);
          renderPickInfoOnly(host);
        }
      };
    }
    var qT = null;
    Crm.$('accQ').oninput = function () {
      var v = this.value;
      if (qT) clearTimeout(qT);
      qT = setTimeout(function () { st.q = v; loadPersonPick(host); }, 240);
    };
    Crm.$('accSave').onclick = function () { createAccount(host); };
    /* mode toggle: personnel-linked (default) vs direct management account */
    function setMode(mgmt) {
      st.picked = null;
      Crm.$('accModeP').className = 'crm-accmode-btn' + (mgmt ? '' : ' on');
      Crm.$('accModeM').className = 'crm-accmode-btn' + (mgmt ? ' on' : '');
      var ps = host.querySelectorAll('.accmode-p'), ms = host.querySelectorAll('.accmode-m'), i;
      for (i = 0; i < ps.length; i++) ps[i].style.display = mgmt ? 'none' : '';
      for (i = 0; i < ms.length; i++) ms[i].style.display = mgmt ? '' : 'none';
      var u = Crm.$('accUser'), ps2 = Crm.$('accPass'), sv = Crm.$('accSave');
      if (u) u.disabled = false; if (ps2) ps2.disabled = false; if (sv) sv.disabled = false;
      if (!mgmt) renderPick(host);
      var info = Crm.$('accPickedInfo');
      if (info && mgmt) info.innerHTML = 'حساب مدیریت مستقیماً با نام کامل ساخته می‌شود (بدون پرسنل).';
    }
    Crm.$('accModeP').onclick = function () { setMode(false); };
    Crm.$('accModeM').onclick = function () { setMode(true); };
    /* wire card 2 filters */
    Crm.$('afDept').onchange = function () { st.fDept = this.value; renderAccountsTable(host); };
    Crm.$('afPerm').onchange = function () { st.fPerm = this.value; renderAccountsTable(host); };
    var fT = null;
    Crm.$('afQ').oninput = function () { var v = this.value; if (fT) clearTimeout(fT); fT = setTimeout(function () { st.fQ = v; renderAccountsTable(host); }, 240); };
    /* wire cards 3+4 */
    dAdd.onclick = function () {
      var nm = Crm.$('sectName').value;
      if (!nm) { Crm.toast('نام بخش الزامی است.', 'err'); return; }
      var mx = 0, ci;
      for (ci = 0; ci < depts.length; ci++) {
        var m = /^OTH(\d+)$/.exec(depts[ci].code || '');
        if (m && +m[1] > mx) mx = +m[1];
      }
      Crm.call('crm.sections.save', { name: nm, code: 'OTH' + (mx + 1), kind: 'other', active: true }).then(function () {
        Crm.toast('بخش اضافه شد.', 'ok'); load(host);
      }, function () { Crm.toast('افزودن بخش ناموفق بود.', 'err'); });
    };
    dAdd4.onclick = function () {
      var nm = Crm.$('deptName').value;
      if (!nm) { Crm.toast('نام بخش الزامی است.', 'err'); return; }
      Crm.call('crm.depts.save', { name: nm }).then(function () { Crm.toast('بخش اضافه شد.', 'ok'); load(host); },
        function () { Crm.toast('افزودن بخش ناموفق بود.', 'err'); });
    };
    /* person links in the accounts table */
    var pl = host.querySelectorAll('[data-person]');
    for (var i = 0; i < pl.length; i++) {
      (function (a) {
        a.onclick = function () {
          var un = a.getAttribute('data-person');
          var pc = a.getAttribute('data-personcode');
          if (pc && Crm.viewPerson) { Crm.viewPerson(pc); return; }
          /* legacy accounts carry no personCode — scan by username once */
          Crm.call('crm.persons.list', { q: '' }).then(function (d) {
            var rows = d.rows || [], j, hit = null;
            for (j = 0; j < rows.length; j++) if (rows[j].username === un) { hit = rows[j]; break; }
            if (hit && Crm.viewPerson) Crm.viewPerson(hit.code);
            else Crm.alert('برای این حساب پرسنلی ثبت نشده است (حساب قدیمی).');
          });
        };
      })(pl[i]);
    }
    /* first pick-list fill */
    loadPersonPick(host);
  }
  function renderPickInfoOnly(host) {
    var info = Crm.$('accPickedInfo');
    if (info && !st.picked) info.innerHTML = 'هنوز پرسنلی انتخاب نشده است.';
  }

  /* v1.80.0: pickers filter by the CLINICAL sections tree (بخش + زیربخش,
     indented) via the shared builder in crm.js (Crm.sectFilterOptions). */
  var deptFilterOpts = Crm.sectFilterOptions;
  function permChecks(perms) {
    /* empty/absent perms = full access → all boxes ticked; "-" = none ticked */
    var all = (perms == null || perms === '');
    var h = '', i;
    for (i = 0; i < PERMS.length; i++) {
      var key = PERMS[i].key;
      var csv = ',' + (perms || '') + ',';
      var on = all || (perms !== '-' && csv.indexOf(',' + key + ',') >= 0);
      if (!on && (key === 'cashier_view' || key === 'cashier_edit') && csv.indexOf(',cashier,') >= 0)
        on = true;
      h += '<label class="crm-check crm-perm"><input type="checkbox" data-perm="' + PERMS[i].key + '"' + (on ? ' checked' : '') + ' />' +
           '<b>' + PERMS[i].label + '</b><small>' + PERMS[i].hint + '</small></label>';
    }
    return h;
  }
  function readPermChecks(containerId) {
    /* v1.79.0 bugfix: read ONLY the given container — the page form (#accPerms)
       and the edit modal (#pmPerms) can coexist in the DOM, and mixing their
       ticks corrupts both. */
    var root = document.getElementById(containerId);
    if (!root) return '';
    var boxes = root.querySelectorAll('input[type=checkbox]');
    var keys = [], i, total = 0;
    for (i = 0; i < boxes.length; i++) { total++; if (boxes[i].checked) keys.push(boxes[i].getAttribute('data-perm')); }
    if (keys.length === total) return '';      /* all ticked = full access */
    if (!keys.length) return '-';              /* none ticked = NO access    */
    return keys.join(',');
  }

  function filterUsers(users, depts) {
    var out = [], i;
    var fq = (st.fQ || '').toLowerCase();
    for (i = 0; i < users.length; i++) {
      var u = users[i];
      if (st.fDept) {
        var dName = '';
        for (var k = 0; k < depts.length; k++) if (depts[k].id === st.fDept) dName = depts[k].name;
        /* a زیربخش filter matches its PARENT section's accounts — the account
           stores the top-level section name by design. */
        for (var k2 = 0; k2 < depts.length; k2++)
          if (depts[k2].id === st.fDept && +depts[k2].parentId > 0) {
            for (var k3 = 0; k3 < depts.length; k3++)
              if (+depts[k3].id === +depts[k2].parentId) dName = depts[k3].name;
          }
        if (st.fDept === '__none__') { if (u.dept) continue; }
        else if (u.dept !== dName) continue;
      }
      if (st.fPerm === 'full' && u.perms) continue;
      if (st.fPerm === 'none' && u.perms !== '-') continue;
      if (st.fPerm && st.fPerm !== 'full' && st.fPerm !== 'none') {
        if (!u.perms || (',' + u.perms + ',').indexOf(',' + st.fPerm + ',') < 0) continue;
      }
      if (fq && (('' + u.username).toLowerCase().indexOf(fq) < 0) &&
          (('' + u.fullname).toLowerCase().indexOf(fq) < 0)) continue;
      out.push(u);
    }
    return out;
  }

  function createAccount(host) {
    var un = Crm.$('accUser').value, pw = Crm.$('accPass').value;
    /* management mode: direct account (role=1), no personnel link — the same
       capability the old «کاربران» page had, so nothing is lost. */
    var modeBtn = Crm.$('accModeM');
    var mgmt = modeBtn && (' ' + modeBtn.className + ' ').indexOf(' on ') >= 0;
    if (mgmt) {
      var full = Crm.$('accFullM').value;
      if (!full) { Crm.toast('نام کامل الزامی است.', 'err'); return; }
      if (!un || !pw) { Crm.toast('نام کاربری و رمز عبور الزامی است.', 'err'); return; }
      Crm.call('crm.employees.save', {
        username: un, fullname: full, dept: '', role: 1, password: pw
      }).then(function (d) {
        if (d && d.ok === false) { Crm.toast(d.err || 'ساخت حساب ناموفق بود.', 'err'); return; }
        Crm.toast('حساب مدیریت «' + un + '» ساخته شد.', 'ok');
        load(host);
      }, function () { Crm.toast('ساخت حساب ناموفق بود.', 'err'); });
      return;
    }
    if (!st.picked) { Crm.toast('ابتدا پرسنل را از لیست انتخاب کنید.', 'err'); return; }
    if (!un || !pw) { Crm.toast('نام کاربری و رمز عبور الزامی است.', 'err'); return; }
    Crm.call('crm.accounts.create', {
      personCode: st.picked.code, username: un, password: pw, perms: readPermChecks('accPerms')
    }).then(function (d) {
      if (d && d.ok === false) { Crm.toast(d.err || 'ساخت حساب ناموفق بود.', 'err'); return; }
      Crm.toast('حساب «' + un + '» ساخته شد.', 'ok');
      st.picked = null; st.q = '';
      load(host);
    }, function () { Crm.toast('ساخت حساب ناموفق بود.', 'err'); });
  }

  /* edit access ticks + optional password reset (username immutable) */
  function openPermsModal(host, u) {
    var m = Crm.modal('دسترسی و رمز — ' + u.username, null);
    m.body.innerHTML =
      '<div class="crm-form">' +
      '<div class="crm-field full"><label class="crm-label">دسترسی‌ها</label>' +
        '<div class="crm-permrow" id="pmPerms">' + permChecks(u.perms) + '</div></div>' +
      '<div class="crm-field full"><label class="crm-label">رمز عبور جدید (خالی = بدون تغییر)</label>' +
        '<input class="crm-input" id="pmPass" type="text" /></div>' +
      '<div class="crm-field full"><small class="crm-hint">نام کاربری «' + Crm.esc(u.username) +
        '» ثابت است؛ رمز هرگز نمایش داده نمی‌شود.</small></div>' +
      '</div>';
    var foot = Crm.el('div', 'crm-modal-foot');
    foot.innerHTML = '<button class="crm-btn ghost" id="mCancel">انصراف</button><button class="crm-btn primary" id="mSave">ذخیره</button>';
    m.card.appendChild(foot);
    Crm.$('mCancel').onclick = m.close;
    Crm.$('mSave').onclick = function () {
      Crm.call('crm.accounts.update', {
        username: u.username, password: Crm.$('pmPass').value, perms: readPermChecks('pmPerms')
      }).then(function (d) {
        if (d && d.ok === false) { Crm.toast(d.err || 'ناموفق', 'err'); return; }
        Crm.toast('حساب به‌روزرسانی شد.', 'ok'); m.close(); load(host);
      }, function () { Crm.toast('به‌روزرسانی ناموفق بود.', 'err'); });
    };
  }

  function del(host, u) {
    Crm.confirm('حذف حساب «' + u.username + '»؟ (پرسنل باقی می‌ماند و بعداً می‌تواند حساب بگیرد)', function () {
      Crm.call('crm.employees.delete', { username: u.username }).then(function () {
        Crm.toast('حساب حذف شد.', 'ok'); load(host);
      }, function () { Crm.toast('حذف ناموفق بود.', 'err'); });
    }, { danger: true });
  }

  Crm.pages.employees = {
    title: 'تعریف حساب کاربری',
    render: function (host) { st.picked = null; load(host); }
  };
})(window);
