/* ============================================================================
   persons.js — «تعریف پرسنل» (v1.79.0). ES5-only.
   ----------------------------------------------------------------------------
   The personnel registry: a PERSON is defined here — full identity, education,
   photo, role (پرسنل/پزشک/پرستار/کارآموز/سایر+متن آزاد), position and the
   department they work in. Every person receives a کد پرسنلی (auto-generated
   from the department name's first two letters — آزمایشگاه→AZ_0001 — or typed
   manually). A person with NO department is «در حالت تعلیق» until assigned.
   Login accounts are attached later on the «تعریف حساب کاربری» page.
   ============================================================================ */
(function (global) {
  'use strict';
  var Crm = global.Crm;

  var ROLE_KINDS = ['پرسنل', 'پزشک', 'پرستار', 'کارآموز', 'سایر'];

  /* ---- shared bits --------------------------------------------------------
     v1.80.0: «بخش» = the clinical sections tree («تعریف بخش و زیربخش»); each
     row is {id,name,parentId}. Top-level rows are بخش; children are زیربخش. */
  var topSections = Crm.sectTop, subSections = Crm.sectSubs;
  function sectOptions(depts, sel) {
    var o = '<option value="">— بدون بخش (تعلیق) —</option>', tops = topSections(depts), i;
    for (i = 0; i < tops.length; i++)
      o += '<option value="' + Crm.esc(tops[i].id) + '"' + (tops[i].id === sel ? ' selected' : '') + '>' +
           Crm.esc(tops[i].name) + '</option>';
    return o;
  }
  function subOptions(depts, parentId, sel) {
    var o = '<option value="">— مستقیم زیر بخش —</option>', subs = subSections(depts, parentId), i;
    for (i = 0; i < subs.length; i++)
      o += '<option value="' + Crm.esc(subs[i].id) + '"' + (subs[i].id === sel ? ' selected' : '') + '>' +
           Crm.esc(subs[i].name) + '</option>';
    return o;
  }
  /* dropdown for FILTERS: «همه» / «در حالت تعلیق» / بخش‌ها + indented زیربخش‌ها
     (shared builder lives in crm.js — Crm.sectFilterOptions) */
  var deptFilterOptions = Crm.sectFilterOptions;
  function fullName(p) { return ((p.firstName || '') + ' ' + (p.lastName || '')).replace(/^\s+|\s+$/g, ''); }
  function deptCell(p) {
    if (!p.deptId) return Crm.pill('در حالت تعلیق', 'warn');
    var t = Crm.esc(p.deptName || p.deptId);
    if (p.subName) t += ' <span class="crm-sub-arrow">←</span> <b>' + Crm.esc(p.subName) + '</b>';
    return '<button class="crm-link" data-dept="' + Crm.esc(p.deptId) + '">' + t + '</button>';
  }

  /* ---- page --------------------------------------------------------------- */
  var curDept = '', curQ = '';
  function load(host) {
    /* v1.80.0: «__none__» is a server-side filter now (the 200-row cap can
       never hide suspended persons from this view). */
    Crm.call('crm.persons.list', { deptId: curDept, q: curQ }).then(function (d) {
      render(host, d.rows || [], d.depts || [], d);
    }, function () {
      host.innerHTML = '';
      Crm.head(host, 'تعریف پرسنل', 'معرفی کامل پرسنل مرکز');
      host.appendChild(Crm.el('div', 'crm-banner err', 'بارگذاری پرسنل ناموفق بود.'));
    });
  }

  function render(host, persons, depts, meta) {
    host.innerHTML = '';
    Crm.head(host, 'تعریف پرسنل', 'معرفی کامل پرسنل — کد پرسنلی خودکار از نام بخش/زیربخش ساخته می‌شود');

    var tb = Crm.el('div', 'crm-toolbar');
    var search = Crm.el('div', 'crm-search');
    search.innerHTML =
      '<input class="crm-input" id="pQ" placeholder="جستجو: کد پرسنلی، نام یا کد ملی…" value="' + Crm.esc(curQ) + '" />' +
      '<span class="crm-search-ic"><svg viewBox="0 0 24 24" width="16" height="16"><path fill="currentColor" d="M10 2a8 8 0 105.3 14L20 20.7 21.7 19l-4.7-4.7A8 8 0 0010 2zm0 2a6 6 0 110 12 6 6 0 010-12z"/></svg></span>';
    tb.appendChild(search);
    var df = Crm.el('div', 'crm-search');
    df.style.maxWidth = '220px';
    df.innerHTML = '<select class="crm-select" id="pDeptF">' + deptFilterOptions(depts, curDept) + '</select>';
    tb.appendChild(df);
    tb.appendChild(Crm.el('div', 'spacer', ''));
    var addBtn = Crm.el('button', 'crm-btn primary', '+ افزودن پرسنل');
    tb.appendChild(addBtn);
    host.appendChild(tb);

    host.appendChild(Crm.table([
      { key: 'i', label: 'ردیف', render: function (r, i) { return Crm.faDigits('' + (i + 1)); } },
      { key: 'code', label: 'کد پرسنلی', cls: 'c-mono', render: function (r) { return '<b class="crm-codechip">' + Crm.esc(r.code) + '</b>'; } },
      { key: 'name', label: 'نام و نام خانوادگی', render: function (r) {
          return '<button class="crm-link" data-code="' + Crm.esc(r.code) + '"><b>' + Crm.esc(fullName(r)) + '</b></button>';
        } },
      { key: 'role', label: 'نقش', render: function (r) { return Crm.pill(r.roleLabel || 'پرسنل', 'info'); } },
      { key: 'position', label: 'مقام/سمت', render: function (r) { return Crm.esc(r.position || '—'); } },
      { key: 'dept', label: 'بخش / زیربخش', render: deptCell },
      { key: 'acc', label: 'حساب کاربری', render: function (r) {
          return r.username ? Crm.pill(r.username, 'on') : Crm.pill('ندارد', 'off');
        } },
      { key: 'ops', label: 'عملیات', render: function (r) {
          var b = Crm.el('span');
          b.innerHTML = '<button class="crm-row-btn" data-act="view">مشاهده</button>' +
                        '<button class="crm-row-btn" data-act="assign">تعریف بخش برای پرسنل</button>' +
                        '<button class="crm-row-btn" data-act="edit">ویرایش</button>' +
                        '<button class="crm-row-btn danger" data-act="del">حذف</button>';
          b.childNodes[0].onclick = function () { viewPerson(r.code); };
          b.childNodes[1].onclick = function () { openAssign(host, r, depts); };
          b.childNodes[2].onclick = function () { openModal(host, r, depts); };
          b.childNodes[3].onclick = function () { del(host, r); };
          return b;
        } }
    ], persons));
    /* v1.80.0: hard cap note — never dump the whole registry into the DOM */
    if (meta && meta.capped)
      host.appendChild(Crm.el('div', 'crm-banner',
        'فقط ' + Crm.faDigits('' + persons.length) + ' مورد اول نمایش داده شد (از ' + Crm.faDigits('' + (meta.total || 0)) + ') — با جستجو یا فیلتر بخش دقیق‌تر کنید.'));

    /* live search (server-side) */
    var qT = null;
    Crm.$('pQ').oninput = function () {
      var v = this.value;
      if (qT) clearTimeout(qT);
      qT = setTimeout(function () { curQ = v; load(host); }, 260);
    };
    Crm.$('pDeptF').onchange = function () { curDept = this.value; load(host); };
    addBtn.onclick = function () { openModal(host, null, depts); };

    /* clickable person/dept links */
    var links = host.querySelectorAll('.crm-link');
    for (var i = 0; i < links.length; i++) {
      (function (a) {
        a.onclick = function () {
          var dc = a.getAttribute('data-code'), dd = a.getAttribute('data-dept');
          if (dc) viewPerson(dc);
          else if (dd && global.Crm.viewDeptInfo) global.Crm.viewDeptInfo(dd);
        };
      })(links[i]);
    }
  }

  /* ---- person photo (shared loader — the bridge returns a data: URL) ------ */
  function loadPhotoInto(code, imgEl, fallbackEl) {
    if (!code) return;
    Crm.call('crm.persons.photo', { code: code }).then(function (d) {
      if (d && d.ok && d.data && imgEl) {
        imgEl.src = d.data;
        imgEl.style.display = 'block';
        if (fallbackEl) fallbackEl.style.display = 'none';
      }
    }, function () {});
  }


  /* ---- view sheet (click on a name) + print -------------------------------- */
  function viewPerson(code) {
    /* v1.80.0: single-person verb — a 200-row-capped list could never lie
       about «not found» for someone beyond the cap. */
    Crm.call('crm.persons.get', { code: code }).then(function (d) {
      var p = d && d.ok ? d.person : null, i;
      if (!p) { Crm.toast('پرسنل پیدا نشد.', 'err'); return; }
      var m = Crm.modal('پروفایل پرسنل — ' + fullName(p), null);
      var rows2 = [
        ['کد پرسنلی', p.code], ['نام', p.firstName], ['نام خانوادگی', p.lastName],
        ['نام پدر', p.fatherName], ['کد ملی', Crm.faDigits(p.nationalId || '—')],
        ['تاریخ تولد', p.birthDate], ['موبایل', Crm.faDigits(p.mobile || '—')],
        ['تلفن', Crm.faDigits(p.phone || '—')], ['ایمیل', p.email],
        ['مدرک تحصیلی', p.education], ['شاخه تحصیلی', p.field], ['عنوان مدرک', p.degree],
        ['نقش', p.roleLabel], ['مقام/سمت', p.position],
        ['بخش', p.deptName || '—'], ['زیربخش', p.subName || '—'],
        ['وضعیت', p.deptId ? 'فعال در بخش' : 'در حالت تعلیق'],
        ['حساب کاربری', p.username || '—'], ['تاریخ ثبت', p.created]
      ];
      var h = '<div class="crm-printable" id="personSheet">' +
        '<div class="crm-sheet-head">' +
          '<span class="crm-sheet-photo"><img id="personSheetImg" alt="" style="display:none" />' +
          '<span class="crm-sheet-photo-ph" id="personSheetPh">' +
            Crm.esc((p.firstName || ' ').charAt(0)) + '</span></span>' +
          '<span class="crm-sheet-id"><b>' + Crm.esc(fullName(p)) + '</b>' +
          '<span>' + Crm.esc(p.roleLabel || '') + (p.position ? ' — ' + Crm.esc(p.position) : '') + '</span>' +
          '<span>' + Crm.esc(p.deptName || '—') + (p.subName ? ' — ' + Crm.esc(p.subName) : '') + '</span>' +
          '<span class="crm-sheet-code">' + Crm.esc(p.code) + '</span></span>' +
        '</div><table class="crm-sheet-tbl">';
      for (i = 0; i < rows2.length; i++) {
        if (rows2[i][1] === '' || rows2[i][1] == null) continue;
        h += '<tr><td>' + Crm.esc(rows2[i][0]) + '</td><td><b>' + Crm.esc(rows2[i][1]) + '</b></td></tr>';
      }
      h += '</table></div>';
      m.body.innerHTML = h;
      var foot = Crm.el('div', 'crm-modal-foot');
      foot.innerHTML = '<button class="crm-btn ghost" id="shClose">بستن</button>' +
                       '<button class="crm-btn outline" id="shPrint">چاپ مشخصات</button>';
      m.card.appendChild(foot);
      Crm.$('shClose').onclick = m.close;
      Crm.$('shPrint').onclick = function () { Crm.printNode('personSheet', 'مشخصات پرسنل — ' + fullName(p)); };
      loadPhotoInto(p.code, Crm.$('personSheetImg'), Crm.$('personSheetPh'));
    });
  }
  Crm.viewPerson = viewPerson;   /* shared with accounts.js */

  /* ---- add / edit modal ---------------------------------------------------- */
  function openModal(host, p, depts) {
    var adding = !p;
    if (!p) p = { roleKind: 0 };
    var m = Crm.modal(adding ? 'افزودن پرسنل' : 'ویرایش پرسنل — ' + fullName(p), null);
    var body = m.body;
    var roleOpts = '';
    for (var i = 0; i < ROLE_KINDS.length; i++)
      roleOpts += '<option value="' + i + '"' + (p.roleKind === i ? ' selected' : '') + '>' + ROLE_KINDS[i] + '</option>';

    body.innerHTML =
      '<div class="crm-form">' +
      /* photo picker */
      '<div class="crm-field full crm-photo-row">' +
        '<span class="crm-photo-frame"><img id="ppImg" alt="" style="display:none" />' +
          '<span id="ppPh">' + Crm.esc(((p.firstName || '') + ' ').charAt(0) || '+') + '</span></span>' +
        '<span class="crm-photo-side">' +
          '<label class="crm-label">عکس پرسنلی</label>' +
          '<input type="file" id="ppPhoto" accept="image/*" class="crm-input" />' +
          '<small class="crm-hint">حداکثر ۲ مگابایت — PNG/JPG</small>' +
        '</span>' +
      '</div>' +
      /* identity */
      '<div class="crm-field"><label class="crm-label">نام *</label><input class="crm-input" id="ppFirst" value="' + Crm.esc(p.firstName || '') + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">نام خانوادگی *</label><input class="crm-input" id="ppLast" value="' + Crm.esc(p.lastName || '') + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">نام پدر</label><input class="crm-input" id="ppFather" value="' + Crm.esc(p.fatherName || '') + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">کد ملی</label><input class="crm-input" id="ppNid" inputmode="numeric" value="' + Crm.esc(p.nationalId || '') + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">تاریخ تولد</label><input class="crm-input" id="ppBirth" placeholder="۱۳۷۰/۰۱/۰۱" value="' + Crm.esc(p.birthDate || '') + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">موبایل</label><input class="crm-input" id="ppMobile" inputmode="numeric" value="' + Crm.esc(p.mobile || '') + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">تلفن ثابت</label><input class="crm-input" id="ppPhone" inputmode="numeric" value="' + Crm.esc(p.phone || '') + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">ایمیل</label><input class="crm-input" id="ppEmail" value="' + Crm.esc(p.email || '') + '" /></div>' +
      '<div class="crm-field full"><label class="crm-label">آدرس</label><input class="crm-input" id="ppAddr" value="' + Crm.esc(p.address || '') + '" /></div>' +
      /* education */
      '<div class="crm-field"><label class="crm-label">مدرک تحصیلی</label><input class="crm-input" id="ppEdu" placeholder="دیپلم، لیسانس، دکترا…" value="' + Crm.esc(p.education || '') + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">شاخه تحصیلی</label><input class="crm-input" id="ppField" placeholder="پرستاری، پزشکی، مدیریت…" value="' + Crm.esc(p.field || '') + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">عنوان مدرک</label><input class="crm-input" id="ppDegree" value="' + Crm.esc(p.degree || '') + '" /></div>' +
      /* role + position */
      '<div class="crm-field"><label class="crm-label">نقش</label><select class="crm-select" id="ppRole">' + roleOpts + '</select></div>' +
      '<div class="crm-field" id="ppRoleCustomWrap" style="display:' + (p.roleKind === 4 ? 'block' : 'none') + '">' +
        '<label class="crm-label">نقش (دستی)</label><input class="crm-input" id="ppRoleCustom" placeholder="مثلاً: مسئول آزمایشگاه" value="' + Crm.esc(p.roleCustom || '') + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">مقام/سمت</label><input class="crm-input" id="ppPos" placeholder="مثلاً: سرپرستار شیفت" value="' + Crm.esc(p.position || '') + '" /></div>' +
      /* dept + subsection + code */
      '<div class="crm-field"><label class="crm-label">بخش (خالی = در حالت تعلیق)</label><select class="crm-select" id="ppDept">' + sectOptions(depts, p.deptId) + '</select></div>' +
      '<div class="crm-field"><label class="crm-label">زیربخش</label><select class="crm-select" id="ppSub">' + subOptions(depts, p.deptId, p.subId) + '</select></div>' +
      '<div class="crm-field"><label class="crm-label">کد پرسنلی' + (adding ? ' (خالی = خودکار)' : ' — ثابت') + '</label>' +
        '<input class="crm-input c-mono" id="ppCode" value="' + Crm.esc(p.code || '') + '" placeholder="AZ_0001"' + (adding ? '' : ' readonly') + ' /></div>' +
      '</div>';

    var photoData = '';
    Crm.$('ppRole').onchange = function () {
      Crm.$('ppRoleCustomWrap').style.display = (+this.value === 4) ? 'block' : 'none';
    };
    /* live code preview from the chosen بخش/زیربخش (while the box is empty);
       the زیربخش name wins when one is picked (آزمایشگاه → AZ_0002 …) */
    function refreshSub() {
      Crm.$('ppSub').innerHTML = subOptions(depts, Crm.$('ppDept').value, '');
    }
    function refreshCodePreview() {
      if (Crm.$('ppCode').value) return;
      Crm.call('crm.persons.nextcode', { deptId: Crm.$('ppDept').value, subId: Crm.$('ppSub').value }).then(function (d) {
        if (d && d.code) Crm.$('ppCode').placeholder = d.code;
      });
    }
    Crm.$('ppDept').onchange = function () { refreshSub(); refreshCodePreview(); };
    Crm.$('ppSub').onchange = refreshCodePreview;
    Crm.$('ppDept').onchange();   /* seed subs + the preview for the initial dept */
    /* photo: preview + 2MB cap before it ever crosses the bridge */
    Crm.$('ppPhoto').onchange = function () {
      var f = this.files && this.files[0];
      if (!f) return;
      if (f.size > 2 * 1024 * 1024) { Crm.toast('حجم عکس بیش از ۲ مگابایت است.', 'err'); this.value = ''; return; }
      var rd = new FileReader();
      rd.onload = function () {
        photoData = '' + rd.result;
        var img = Crm.$('ppImg');
        img.src = photoData; img.style.display = 'block';
        Crm.$('ppPh').style.display = 'none';
      };
      rd.readAsDataURL(f);
    };
    if (p.hasPhoto && p.code) loadPhotoInto(p.code, Crm.$('ppImg'), Crm.$('ppPh'));

    var foot = Crm.el('div', 'crm-modal-foot');
    foot.innerHTML = '<button class="crm-btn ghost" id="mCancel">انصراف</button><button class="crm-btn primary" id="mSave">ذخیره پرسنل</button>';
    m.card.appendChild(foot);
    Crm.$('mCancel').onclick = m.close;
    Crm.$('mSave').onclick = function () {
      var payload = {
        origCode: adding ? '' : (p.code || ''),
        code: Crm.$('ppCode').value,
        firstName: Crm.$('ppFirst').value, lastName: Crm.$('ppLast').value,
        fatherName: Crm.$('ppFather').value, nationalId: Crm.$('ppNid').value,
        birthDate: Crm.$('ppBirth').value, mobile: Crm.$('ppMobile').value,
        phone: Crm.$('ppPhone').value, email: Crm.$('ppEmail').value,
        address: Crm.$('ppAddr').value,
        education: Crm.$('ppEdu').value, field: Crm.$('ppField').value,
        degree: Crm.$('ppDegree').value,
        roleKind: +Crm.$('ppRole').value, roleCustom: Crm.$('ppRoleCustom').value,
        position: Crm.$('ppPos').value,
        deptId: Crm.$('ppDept').value,
        subId: Crm.$('ppSub').value
      };
      if (!payload.firstName && !payload.lastName) { Crm.toast('نام و نام خانوادگی الزامی است.', 'err'); return; }
      if (photoData) payload.photoData = photoData;
      Crm.call('crm.persons.save', payload).then(function (d) {
        Crm.toast(adding ? ('پرسنل با کد ' + (d.code || '') + ' ثبت شد.') : 'پرسنل ویرایش شد.', 'ok');
        m.close(); load(host);
      }, function (e) { Crm.toast('ذخیره ناموفق بود.', 'err'); });
    };
  }

  function openAssign(host, p, depts) {
    var m = Crm.modal('تعریف بخش برای پرسنل — ' + fullName(p), null);
    m.body.innerHTML =
      '<div class="crm-form">' +
      '<div class="crm-field"><label class="crm-label">بخش</label><select class="crm-select" id="asDept">' +
        sectOptions(depts, p.deptId) + '</select></div>' +
      '<div class="crm-field"><label class="crm-label">زیربخش</label><select class="crm-select" id="asSub">' +
        subOptions(depts, p.deptId, p.subId) + '</select></div>' +
      '</div>';
    Crm.$('asDept').onchange = function () {
      Crm.$('asSub').innerHTML = subOptions(depts, Crm.$('asDept').value, '');
    };
    var foot = Crm.el('div', 'crm-modal-foot');
    foot.innerHTML = '<button class="crm-btn ghost" id="asCancel">انصراف</button>' +
                     '<button class="crm-btn primary" id="asSave">ثبت بخش</button>';
    m.card.appendChild(foot);
    Crm.$('asCancel').onclick = m.close;
    Crm.$('asSave').onclick = function () {
      Crm.call('crm.persons.assign', {
        code: p.code,
        deptId: Crm.$('asDept').value,
        subId: Crm.$('asSub').value
      }).then(function () {
        Crm.toast('بخش پرسنل ثبت شد.', 'ok');
        m.close(); load(host);
      }, function () { Crm.toast('ثبت بخش ناموفق بود.', 'err'); });
    };
  }

  function del(host, p) {
    Crm.confirm('حذف «' + fullName(p) + '»؟' + (p.username ? '\nحساب کاربری «' + p.username + '» هم بسته می‌شود.' : ''), function () {
      Crm.call('crm.persons.delete', { code: p.code }).then(function () {
        Crm.toast('پرسنل حذف شد.', 'ok'); load(host);
      }, function () { Crm.toast('حذف ناموفق بود.', 'err'); });
    }, { danger: true });
  }

  Crm.pages.persons = {
    title: 'تعریف پرسنل',
    render: function (host) { curDept = ''; curQ = ''; load(host); }
  };
})(window);
