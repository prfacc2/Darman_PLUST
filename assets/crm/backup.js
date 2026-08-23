/* ============================================================================
   backup.js — پشتیبان‌گیری / بازگردانی HTML (v1.82.0). ES5-only.
   Pick a .bak path, create or analyze-then-restore, poll progress, copyable
   error log. Does not open the native C++ backup manager.
   ============================================================================ */
(function (global) {
  'use strict';
  var Crm = global.Crm;
  var pollT = null;

  function stopPoll() {
    if (pollT) { clearTimeout(pollT); pollT = null; }
  }

  function setBusy(on) {
    var spin = Crm.$('bkSpin');
    if (spin) spin.style.display = on ? 'block' : 'none';
    var btns = document.getElementsByClassName('bk-act');
    var i;
    for (i = 0; i < btns.length; i++) btns[i].disabled = !!on;
  }

  function setBar(pct, status) {
    var bar = Crm.$('bkBar');
    var st = Crm.$('bkStatus');
    var n = +pct || 0;
    if (n < 0) n = 0;
    if (n > 100) n = 100;
    if (bar) bar.style.width = n + '%';
    if (st) st.innerHTML = Crm.esc(status || '') + (n ? ' — ' + Crm.faDigits('' + n) + '٪' : '');
  }

  function appendErr(s) {
    if (!s) return;
    var ta = Crm.$('bkErr');
    if (!ta) return;
    var prev = ta.value || '';
    if (prev && prev.indexOf(s) >= 0) return;
    ta.value = prev ? (prev + '\n' + s) : s;
  }

  function pollUntilDone(onDone) {
    stopPoll();
    function tick() {
      Crm.call('crm.backup.progress', {}).then(function (p) {
        p = p || {};
        setBar(p.pct, p.status);
        if (p.err) appendErr(p.err);
        if (p.busy) {
          pollT = setTimeout(tick, 450);
        } else {
          stopPoll();
          setBusy(false);
          if (onDone) onDone(p);
        }
      }, function () {
        pollT = setTimeout(tick, 800);
      });
    }
    pollT = setTimeout(tick, 200);
  }

  function runJob(verb, path, startMsg, failMsg, okMsg) {
    setBusy(true);
    setBar(0, startMsg);
    Crm.call(verb, { path: path }).then(function (r) {
      if (!r || !r.ok) {
        setBusy(false);
        appendErr((r && r.err) || failMsg);
        Crm.toast((r && r.err) || failMsg, 'err');
        return;
      }
      pollUntilDone(function (p) {
        if (p && p.err) {
          Crm.toast(failMsg, 'err');
        } else {
          setBar(100, 'تمام شد');
          Crm.toast(okMsg, 'ok');
        }
      });
    }, function () {
      setBusy(false);
      Crm.toast(failMsg, 'err');
    });
  }

  function pickBak(verb, emptyMsg, failMsg, onPath) {
    Crm.call(verb, {}).then(function (d) {
      if (!d || d.cancelled) return;
      if (!d.ok || !d.path) { Crm.toast((d && d.err) || emptyMsg, 'err'); return; }
      onPath(d.path);
    }, function () { Crm.toast(failMsg, 'err'); });
  }

  function doCreate() {
    pickBak('crm.backup.pickSave', 'مسیر انتخاب نشد.', 'انتخاب مسیر ناموفق بود.', function (path) {
      runJob('crm.backup.create', path, 'شروع پشتیبان‌گیری…',
             'پشتیبان‌گیری ناموفق بود.', 'پشتیبان با موفقیت ذخیره شد.');
    });
  }

  function showAnalyze(path, d) {
    var box = Crm.$('bkAnalyze');
    if (!box) return;
    var tables = (d && d.tables) || [];
    var h = '<div class="crm-banner info">فایل معتبر است — ' +
            Crm.faDigits('' + (d.files || 0)) + ' پرونده در بایگانی.</div>' +
            '<table class="crm-sheet-tbl"><tr><td>جدول</td><td>تعداد پرونده</td><td>حجم</td></tr>';
    var i;
    for (i = 0; i < tables.length; i++) {
      h += '<tr><td><b>' + Crm.esc(tables[i].name) + '</b></td><td>' +
           Crm.faDigits('' + (tables[i].files || 0)) + '</td><td>' +
           Crm.faDigits(Crm.fmtMoney(tables[i].bytes || 0)) + ' بایت</td></tr>';
    }
    if (!tables.length) h += '<tr><td colspan="3">جدولی تشخیص داده نشد.</td></tr>';
    h += '</table>';
    box.innerHTML = h;
    box.setAttribute('data-path', path || '');
    var apply = Crm.$('bkApply');
    if (apply) apply.style.display = 'inline-block';
  }

  function doAnalyze() {
    pickBak('crm.backup.pickOpen', 'فایل انتخاب نشد.', 'انتخاب فایل ناموفق بود.', function (path) {
      Crm.call('crm.backup.analyze', { path: path }).then(function (r) {
        if (!r || !r.ok) {
          appendErr((r && r.err) || 'تحلیل ناموفق بود.');
          Crm.toast((r && r.err) || 'تحلیل فایل ناموفق بود.', 'err');
          return;
        }
        showAnalyze(path, r);
        Crm.toast('تحلیل انجام شد — در صورت تأیید، بازگردانی کنید.', 'ok');
      }, function () { Crm.toast('تحلیل فایل ناموفق بود.', 'err'); });
    });
  }

  function doRestore() {
    var box = Crm.$('bkAnalyze');
    var path = box ? box.getAttribute('data-path') : '';
    if (!path) { Crm.toast('ابتدا فایل پشتیبان را تحلیل کنید.', 'err'); return; }
    Crm.confirm('بازگردانی این پشتیبان داده‌های جاری را جایگزین می‌کند. ادامه می‌دهید؟', function () {
      runJob('crm.backup.restore', path, 'شروع بازگردانی…',
             'بازگردانی ناموفق بود.', 'بازگردانی با موفقیت انجام شد.');
    }, { danger: true });
  }

  function copyErr() {
    var ta = Crm.$('bkErr');
    if (!ta || !ta.value) { Crm.toast('متن خطایی برای کپی نیست.', 'info'); return; }
    try {
      ta.focus();
      ta.select();
      if (global.document && global.document.execCommand)
        global.document.execCommand('copy');
      Crm.toast('گزارش خطا کپی شد.', 'ok');
    } catch (e) {
      Crm.toast('کپی ناموفق بود.', 'err');
    }
  }

  Crm.pages.backup = {
    title: 'پشتیبان‌گیری',
    render: function (host) {
      stopPoll();
      host.innerHTML = '';
      Crm.head(host, 'پشتیبان‌گیری و بازگردانی', 'بایگانی .bak از صندوق، بخش‌ها، پرسنل، خدمات، کاربران و تنظیمات');

      var gc = Crm.el('div', 'crm-card');
      gc.innerHTML =
        '<div class="crm-card-title"><span class="dot"></span>پشتیبان‌گیری</div>' +
        '<div class="crm-banner info">یک پوشه/فایل با پسوند <b>.bak</b> انتخاب کنید. صندوق، بخش و زیربخش، جای‌گذاری پرسنل، خدمات و تعرفه، کاربران، کارتابل، پرونده بیمار، طرح‌های چاپ و تنظیمات ذخیره می‌شوند.</div>' +
        '<div class="crm-toolbar"><div class="spacer"></div>' +
          '<button class="crm-btn success bk-act" id="bkCreate">پشتیبان‌گیری</button></div>';
      host.appendChild(gc);

      var rc = Crm.el('div', 'crm-card');
      rc.innerHTML =
        '<div class="crm-card-title"><span class="dot"></span>بازگردانی</div>' +
        '<div class="crm-banner info">ابتدا فایل تحلیل می‌شود (فهرست جدول‌ها) و پس از تأیید اعمال می‌گردد.</div>' +
        '<div class="crm-toolbar"><div class="spacer"></div>' +
          '<button class="crm-btn outline bk-act" id="bkOpen">انتخاب و تحلیل فایل</button>' +
          '<button class="crm-btn danger bk-act" id="bkApply" style="display:none;margin-right:8px">اعمال بازگردانی</button></div>' +
        '<div id="bkAnalyze"></div>';
      host.appendChild(rc);

      var pc = Crm.el('div', 'crm-card');
      pc.innerHTML =
        '<div class="crm-card-title"><span class="dot"></span>پیشرفت</div>' +
        '<div id="bkSpin" class="crm-loader-spin" style="display:none;margin:8px auto"></div>' +
        '<div class="bk-track"><div class="bk-bar" id="bkBar"></div></div>' +
        '<div class="crm-muted" id="bkStatus">آماده</div>';
      host.appendChild(pc);

      var ec = Crm.el('div', 'crm-card');
      ec.innerHTML =
        '<div class="crm-card-title"><span class="dot"></span>گزارش خطا</div>' +
        '<textarea class="crm-input" id="bkErr" readonly="readonly" rows="6" ' +
          'style="width:100%;min-height:120px;font-family:Tahoma,monospace;direction:ltr;text-align:left"></textarea>' +
        '<div class="crm-toolbar"><div class="spacer"></div>' +
          '<button class="crm-btn outline" id="bkCopy">کپی گزارش</button></div>';
      host.appendChild(ec);

      Crm.$('bkCreate').onclick = doCreate;
      Crm.$('bkOpen').onclick = doAnalyze;
      Crm.$('bkApply').onclick = doRestore;
      Crm.$('bkCopy').onclick = copyErr;
    }
  };
})(window);
