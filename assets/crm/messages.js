/* ============================================================================
   messages.js — Cartable / inbox (کارتابل). ES5-only.
   List / send / mark-seen / delete / pin messages via crm.messages.* verbs.
   The on-disk store is owned by C++ (loadMessages / pushMessageT /
   pinMessage / seenOneMessage / deleteOneMessage); the list is newest-first and
   idx matches the C++ indexNewestFirst convention.

   v1.76: the compose card now targets a PERSON, a SECTION, or a GROUP (broadcast):
     · شخص   — live search by personnel code (User.id) or username; matching
                users surface in a selectable list, the chosen one is the recipient.
     · بخش   — pick an organizational section; the message is fanned out to every
                user whose dept matches that section (one push per recipient).
     · گروه  — broadcast to every user on the network (to == "*").
   Recipients are resolved from crm.employees.list (users + depts). Each send
   round-trips through crm.messages.send so the C++ store (data\messages.dat)
   stays the single source of truth and the recipient workstation is notified.
   ============================================================================ */
(function (global) {
  'use strict';
  var Crm = global.Crm;

  var users = [];          /* cached system users (id, username, fullname, dept…) */
  var depts = [];          /* cached organizational sections */
  var selUser = null;      /* chosen person recipient ({username,fullname,id,dept}) */

  function load(host) {
    /* fetch the user/section directory first (drives the recipient picker),
       then the inbox — both failures degrade gracefully. */
    Crm.call('crm.employees.list', {}).then(function (d) {
      users = (d && d.rows) || [];
      depts = (d && d.depts) || [];
      loadMessages(host);
    }, function () {
      users = []; depts = [];
      loadMessages(host);
    });
  }

  function loadMessages(host) {
    Crm.call('crm.messages.list', {}).then(function (d) {
      render(host, d.rows || [], d.unseen || 0);
      /* mark all seen shortly after viewing */
      Crm.call('crm.messages.seen', {}).then(function () {
        var badge = Crm.$('navMsgBadge');
        if (badge) badge.style.display = 'none';
      });
    }, function () {
      host.innerHTML = '';
      Crm.head(host, 'کارتابل', 'صندوق پیام‌های دریافتی');
      host.appendChild(Crm.el('div', 'crm-banner err', 'بارگذاری پیام‌ها ناموفق بود.'));
    });
  }

  function typeLabel(t) {
    if (t === 2) return 'بحرانی';
    if (t === 1) return 'فوری';
    return 'عادی';
  }
  function typeKind(t) {
    if (t === 2) return 'off';
    if (t === 1) return 'info';
    return 'on';
  }

  function render(host, rows, unseen) {
    host.innerHTML = '';
    Crm.head(host, 'کارتابل', 'صندوق پیام‌های دریافتی' + (unseen ? ' (' + Crm.faDigits('' + unseen) + ' جدید)' : ''));
    selUser = null;

    /* ---------------- compose card ---------------- */
    var cc = Crm.el('div', 'crm-card');
    cc.innerHTML = '<div class="crm-card-title"><span class="dot"></span>ارسال پیام جدید</div>';

    var form = Crm.el('div', 'crm-form');
    form.innerHTML =
      '<div class="crm-field full"><label class="crm-label">نوع گیرنده</label>' +
        '<div class="crm-seg" id="mMode">' +
          '<button type="button" data-v="person" class="on">شخص</button>' +
          '<button type="button" data-v="section">بخش</button>' +
          '<button type="button" data-v="group">گروه (همه)</button>' +
        '</div></div>' +

      '<div class="crm-field full" id="mPersonWrap">' +
        '<label class="crm-label">جستجوی گیرنده — کد پرسنلی یا نام کاربری</label>' +
        '<input class="crm-input" id="mPersonQ" placeholder="مثال: ۱۰۲۳ یا ali…" />' +
        '<div class="crm-pick" id="mPersonList"></div>' +
        '<div class="crm-pick-sel" id="mPersonSel">گیرنده‌ای انتخاب نشده است.</div>' +
      '</div>' +

      '<div class="crm-field full" id="mSectionWrap" style="display:none">' +
        '<label class="crm-label">بخش مقصد</label>' +
        '<select class="crm-select" id="mSection">' + deptOptions('') + '</select>' +
      '</div>' +

      '<div class="crm-field full" id="mGroupWrap" style="display:none">' +
        '<span class="crm-banner info" style="margin:0">پیام برای تمام کاربران شبکه ارسال می‌شود.</span>' +
      '</div>' +

      '<div class="crm-field"><label class="crm-label">اولویت</label>' +
        '<select class="crm-select" id="mType"><option value="0">عادی</option>' +
        '<option value="1">فوری</option><option value="2">بحرانی</option></select></div>' +

      '<div class="crm-field full"><label class="crm-label">متن پیام</label>' +
        '<textarea class="crm-textarea" id="mText"></textarea></div>';
    cc.appendChild(form);

    var foot = Crm.el('div', 'crm-modal-foot');
    foot.style.borderTop = '0';
    foot.style.paddingTop = '0';
    foot.style.marginTop = '0';
    foot.innerHTML = '<button class="crm-btn primary" id="mSend">ارسال پیام</button>';
    cc.appendChild(foot);
    host.appendChild(cc);

    wireMode(host);
    var qEl = Crm.$('mPersonQ');
    if (qEl) qEl.onkeyup = function () { renderPersonList(); };
    renderPersonList();           /* seed the list with every user (filter as you type) */
    Crm.$('mSend').onclick = function () { send(host); };

    /* ---------------- inbox card ---------------- */
    var lc = Crm.el('div', 'crm-card');
    lc.innerHTML = '<div class="crm-card-title"><span class="dot"></span>پیام‌های دریافتی</div>';
    if (!rows.length) {
      lc.appendChild(Crm.el('div', 'crm-banner info', 'پیامی در کارتابل نیست.'));
    } else {
      for (var i = 0; i < rows.length; i++) {
        lc.appendChild(msgRow(host, rows[i]));
      }
    }
    host.appendChild(lc);
  }

  function deptOptions(sel) {
    var o = '<option value="">— انتخاب بخش —</option>';
    for (var i = 0; i < depts.length; i++) {
      o += '<option value="' + Crm.esc(depts[i].name) + '"' +
           (depts[i].name === sel ? ' selected' : '') + '>' + Crm.esc(depts[i].name) + '</option>';
    }
    return o;
  }

  /* ---- recipient-type segmented control ---- */
  function segValue() {
    var box = Crm.$('mMode');
    if (!box) return 'person';
    var btns = box.getElementsByTagName('button');
    for (var i = 0; i < btns.length; i++) {
      if (btns[i].className.indexOf('on') >= 0) return btns[i].getAttribute('data-v');
    }
    return btns.length ? btns[0].getAttribute('data-v') : 'person';
  }
  function wireMode(host) {
    var box = Crm.$('mMode');
    if (!box) return;
    var btns = box.getElementsByTagName('button');
    for (var i = 0; i < btns.length; i++) {
      (function (btn) {
        btn.onclick = function () {
          for (var k = 0; k < btns.length; k++) btns[k].className = '';
          btn.className = 'on';
          showMode();
        };
      })(btns[i]);
    }
    showMode();
  }
  function showMode() {
    var mode = segValue();
    var p = Crm.$('mPersonWrap'), s = Crm.$('mSectionWrap'), g = Crm.$('mGroupWrap');
    if (p) p.style.display = (mode === 'person') ? '' : 'none';
    if (s) s.style.display = (mode === 'section') ? '' : 'none';
    if (g) g.style.display = (mode === 'group') ? '' : 'none';
  }

  /* ---- live person search (by personnel code / username / fullname) ---- */
  function renderPersonList() {
    var box = Crm.$('mPersonList');
    if (!box) return;
    var raw = Crm.$('mPersonQ') ? Crm.$('mPersonQ').value : '';
    var fq = Crm.enDigits(raw || '').toLowerCase();
    box.innerHTML = '';
    var n = 0;
    for (var i = 0; i < users.length; i++) {
      var u = users[i];
      if (fq) {
        var id = '' + (u.id == null ? '' : u.id);
        var hit = id.indexOf(fq) >= 0 ||
                  ('' + u.username).toLowerCase().indexOf(fq) >= 0 ||
                  ('' + (u.fullname || '')).toLowerCase().indexOf(fq) >= 0;
        if (!hit) continue;
      }
      if (n >= 40) break;
      box.appendChild(personRow(u));
      n++;
    }
    if (!n) {
      box.appendChild(Crm.el('div', 'crm-pick-empty', fq ? 'کاربری مطابق جستجو یافت نشد.' : 'برای جستجو نام کاربری یا کد پرسنلی را وارد کنید.'));
    }
  }
  function personRow(u) {
    var r = Crm.el('button', 'crm-pick-item' + (selUser && selUser.username === u.username ? ' sel' : ''));
    r.setAttribute('type', 'button');
    r.innerHTML =
      '<span class="crm-pick-code">' + Crm.faDigits('' + (u.id == null ? '—' : u.id)) + '</span>' +
      '<span class="crm-pick-main">' +
        '<span class="crm-pick-name">' + Crm.esc(u.fullname || u.username) + '</span>' +
        '<span class="crm-pick-user">' + Crm.esc(u.username) + (u.dept ? ' · ' + Crm.esc(u.dept) : '') + '</span>' +
      '</span>' +
      '<span class="crm-pick-state">' + Crm.pill(u.online ? 'آنلاین' : 'آفلاین', u.online ? 'on' : 'off') + '</span>';
    r.onclick = function () { selectPerson(u); };
    return r;
  }
  function selectPerson(u) {
    selUser = u;
    var sel = Crm.$('mPersonSel');
    if (sel) {
      sel.innerHTML =
        '<span class="crm-pick-chip">' +
          '<span class="crm-pick-chip-name">' + Crm.esc(u.fullname || u.username) + '</span>' +
          '<span class="crm-pick-chip-user">@' + Crm.esc(u.username) + '</span>' +
        '</span>';
    }
    renderPersonList();
  }

  /* ---- send: resolve recipients by mode, fan out, reload ---- */
  function send(host) {
    var mode = segValue();
    var text = Crm.$('mText') ? Crm.$('mText').value : '';
    var type = Crm.$('mType') ? +Crm.$('mType').value : 0;
    if (!text) { Crm.toast('متن پیام خالی است.', 'err'); return; }

    var recipients = [];
    var sec = '';
    if (mode === 'person') {
      if (!selUser) { Crm.toast('یک گیرنده انتخاب کنید.', 'err'); return; }
      recipients = [selUser.username];
    } else if (mode === 'section') {
      sec = Crm.$('mSection') ? Crm.$('mSection').value : '';
      if (!sec) { Crm.toast('یک بخش انتخاب کنید.', 'err'); return; }
      for (var i = 0; i < users.length; i++) {
        if (users[i].dept === sec) recipients.push(users[i].username);
      }
      if (!recipients.length) { Crm.toast('کاربری در بخش «' + sec + '» یافت نشد.', 'err'); return; }
    } else {
      recipients = ['*'];
    }
    fanOut(host, recipients, text, type, function (ok) {
      var msg;
      if (mode === 'person') {
        msg = 'پیام به «' + (selUser.fullname || selUser.username) + '» ارسال شد.';
      } else if (mode === 'section') {
        msg = 'پیام به بخش «' + sec + '» ارسال شد (' + Crm.faDigits('' + ok) + ' گیرنده).';
      } else {
        msg = 'پیام گروهی برای همه کاربران شبکه ارسال شد.';
      }
      Crm.toast(msg, 'ok');
      load(host);
    });
  }

  function fanOut(host, recipients, text, type, onDone) {
    var total = recipients.length, done = 0, ok = 0;
    if (!total) { if (onDone) onDone(0); return; }
    for (var i = 0; i < total; i++) {
      (function (to) {
        Crm.call('crm.messages.send', { to: to, text: text, type: type }).then(function () {
          ok++; finish();
        }, function () { finish(); });
      })(recipients[i]);
    }
    function finish() {
      done++;
      if (done < total) return;
      if (onDone) onDone(ok);
    }
  }

  function msgRow(host, m) {
    var cls = 'crm-msg';
    /* v2.06: sent-by-me rows get their own style so the operator sees their
       own messages in the inbox (previously only the manager's side showed). */
    if (m.mine === true) cls += ' mine';
    if (!m.seen && m.mine !== true) cls += ' unseen';
    if (m.pinned) cls += ' pinned';
    var row = Crm.el('div', cls);
    var body = Crm.el('div', 'crm-msg-body');
    body.innerHTML =
      '<div><span class="crm-msg-from">' + Crm.esc(m.from) +
        (m.mine === true ? ' <span class="crm-msg-to">← ' + Crm.esc((m.to === '*' ? 'همه' : m.to) || '') + '</span>' : '') +
      '</span>' +
      '<span class="crm-msg-time">' + Crm.esc(m.time || '') + '</span> ' +
      Crm.pill(typeLabel(m.type), typeKind(m.type)) + '</div>' +
      '<div class="crm-msg-text">' + Crm.esc(m.text) + '</div>';
    row.appendChild(body);
    var acts = Crm.el('div', 'crm-msg-actions');
    if (m.mine === true) {
      /* v2.06: sent rows — informational only (no pin/delete on the copy the
         recipient owns). */
      acts.innerHTML = '<span class="crm-msg-sent-tag">ارسال‌شده</span>';
    } else {
      acts.innerHTML =
        '<button class="crm-row-btn" data-act="pin">' + (m.pinned ? 'برداشتن پین' : 'پین') + '</button>' +
        '<button class="crm-row-btn danger" data-act="del">حذف</button>';
      acts.childNodes[0].onclick = function () {
        Crm.call('crm.messages.pin', { idx: m.idx, pin: !m.pinned }).then(function () { load(host); },
          function () { Crm.toast('عملیات ناموفق بود.', 'err'); });
      };
      acts.childNodes[1].onclick = function () {
        Crm.confirm('حذف این پیام؟', function () {
          Crm.call('crm.messages.delete', { idx: m.idx }).then(function () { Crm.toast('پیام حذف شد.', 'ok'); load(host); },
            function () { Crm.toast('حذف ناموفق بود.', 'err'); });
        }, { danger: true });
      };
    }
    row.appendChild(acts);
    return row;
  }

  Crm.pages.messages = {
    title: 'کارتابل',
    render: function (host) { load(host); }
  };
})(window);
