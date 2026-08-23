/* ============================================================================
   calendar.js — تقویم کاری (v1.82.0). ES5-only.
   Who started / ended which section+subsection, day, hours, shift income.
   Calls crm.calendar.list { from, to, user }.
   ============================================================================ */
(function (global) {
  'use strict';
  var Crm = global.Crm;

  var curFrom = '', curTo = '', curUser = '';

  function load(host) {
    Crm.call('crm.calendar.list', { from: curFrom, to: curTo, user: curUser }).then(function (d) {
      render(host, (d && d.rows) || []);
    }, function () {
      host.innerHTML = '';
      Crm.head(host, 'تقویم کاری', 'شروع و پایان شیفت صندوق');
      host.appendChild(Crm.el('div', 'crm-banner err', 'بارگذاری تقویم کاری ناموفق بود.'));
    });
  }

  function statusPill(r) {
    if (r.open || r.status === 'open') return Crm.pill('باز', 'on');
    return Crm.pill('بسته', 'off');
  }

  function hoursCell(r) {
    var a = (r.startJalali || '') + (r.startTime ? ' ' + r.startTime : '');
    var b = (r.endJalali || '') + (r.endTime ? ' ' + r.endTime : '');
    if (!a) return '—';
    return Crm.esc(a) + (b.replace(/\s+/g, '') ? '<br/><span class="crm-muted">تا ' + Crm.esc(b) + '</span>' : '');
  }

  function sectCell(r) {
    var t = Crm.esc(r.sectionName || '—');
    if (r.subName) t += ' <span class="crm-sub-arrow">←</span> <b>' + Crm.esc(r.subName) + '</b>';
    return t;
  }

  function render(host, rows) {
    host.innerHTML = '';
    Crm.head(host, 'تقویم کاری', 'چه کسی در کدام بخش/زیربخش شیفت را شروع یا پایان داده است');

    var tb = Crm.el('div', 'crm-toolbar');
    var fFrom = Crm.el('div', 'crm-search');
    fFrom.style.maxWidth = '160px';
    fFrom.innerHTML = '<input class="crm-input" id="calFrom" placeholder="از تاریخ" value="' + Crm.esc(curFrom) + '" />';
    var fTo = Crm.el('div', 'crm-search');
    fTo.style.maxWidth = '160px';
    fTo.innerHTML = '<input class="crm-input" id="calTo" placeholder="تا تاریخ" value="' + Crm.esc(curTo) + '" />';
    var fUser = Crm.el('div', 'crm-search');
    fUser.style.maxWidth = '180px';
    fUser.innerHTML = '<input class="crm-input" id="calUser" placeholder="نام کاربری" value="' + Crm.esc(curUser) + '" />';
    tb.appendChild(fFrom);
    tb.appendChild(fTo);
    tb.appendChild(fUser);
    var go = Crm.el('button', 'crm-btn primary', 'نمایش');
    tb.appendChild(go);
    host.appendChild(tb);

    host.appendChild(Crm.table([
      { key: 'i', label: 'ردیف', render: function (r, i) { return Crm.faDigits('' + (i + 1)); } },
      { key: 'who', label: 'پرسنل', render: function (r) {
          return '<b>' + Crm.esc(r.fullname || r.username || '—') + '</b>' +
                 (r.username ? '<br/><span class="crm-muted">' + Crm.esc(r.username) + '</span>' : '');
        } },
      { key: 'sect', label: 'بخش / زیربخش', render: sectCell },
      { key: 'day', label: 'روز', render: function (r) { return Crm.faDigits(r.startJalali || '—'); } },
      { key: 'hrs', label: 'ساعت شروع / پایان', render: hoursCell },
      { key: 'inc', label: 'درآمد شیفت', cls: 'c-mono', render: function (r) {
          return '<b>' + Crm.faDigits(Crm.fmtMoney(r.income || 0)) + '</b> ریال';
        } },
      { key: 'st', label: 'وضعیت', render: statusPill }
    ], rows));

    if (!rows.length)
      host.appendChild(Crm.el('div', 'crm-banner info', 'شیفتی در این بازه ثبت نشده است.'));

    go.onclick = function () {
      curFrom = Crm.$('calFrom').value;
      curTo = Crm.$('calTo').value;
      curUser = Crm.$('calUser').value;
      load(host);
    };
  }

  Crm.pages.calendar = {
    title: 'تقویم کاری',
    render: function (host) { curFrom = ''; curTo = ''; curUser = ''; load(host); }
  };
})(window);
