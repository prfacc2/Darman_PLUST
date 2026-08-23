/* ============================================================================
   settings.js — Settings + backup (تنظیمات و پشتیبان). ES5-only.
   Read / write application settings via crm.settings.* verbs. Backup opens
   the HTML page (Crm.nav('backup')), not the native C++ manager. Print
   settings still go through crm.printConfig → PrintCfg_Open. The on-disk
   settings store (data\settings) is owned by C++ (getSetting / setSetting).
   ============================================================================ */
(function (global) {
  'use strict';
  var Crm = global.Crm;

  function load(host) {
    Crm.call('crm.settings.get', {}).then(function (d) {
      render(host, d || {});
    }, function () {
      host.innerHTML = '';
      Crm.head(host, 'تنظیمات و پشتیبان', 'پیکربندی سیستم و پشتیبان‌گیری');
      host.appendChild(Crm.el('div', 'crm-banner err', 'بارگذاری تنظیمات ناموفق بود.'));
    });
  }

  function render(host, s) {
    host.innerHTML = '';
    Crm.head(host, 'تنظیمات و پشتیبان', 'پیکربندی سیستم و پشتیبان‌گیری');

    /* general settings card */
    var gc = Crm.el('div', 'crm-card');
    gc.innerHTML = '<div class="crm-card-title"><span class="dot"></span>تنظیمات عمومی</div>';
    var form = Crm.el('div', 'crm-form');
    form.innerHTML =
      '<div class="crm-field"><label class="crm-label">نام درمانگاه</label>' +
        '<input class="crm-input" id="sClinic" value="' + Crm.esc(s.clinicName || 'درمان پلاس') + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">حالت پذیرش</label>' +
        '<select class="crm-select" id="sMode"><option value="simple"' + (s.receptionMode !== 'full' ? ' selected' : '') + '>ساده</option>' +
        '<option value="full"' + (s.receptionMode === 'full' ? ' selected' : '') + '>کامل</option></select></div>' +
        /* v1.77: the «پالت رنگی» (calm/warm) picker was removed — only light/dark remain. */
        '<div class="crm-field"><label class="crm-label">تعداد کپی چاپ</label>' +
        '<input class="crm-input" id="sCopies" value="' + Crm.esc(s.printCopies || '1') + '" /></div>' +
      '<div class="crm-field full"><label class="crm-check"><input type="checkbox" id="sSavedMsgs" ' + (s.savedMsgs === '1' ? 'checked' : '') + ' />ذخیرهٔ پیام‌های آرشیوشده</label></div>';
    gc.appendChild(form);
    var foot = Crm.el('div', 'crm-modal-foot');
    foot.innerHTML = '<button class="crm-btn primary" id="sSave">ذخیره تنظیمات</button>';
    gc.appendChild(foot);
    host.appendChild(gc);

    Crm.$('sSave').onclick = function () {
      var payload = {
        clinicName: Crm.$('sClinic').value,
        receptionMode: Crm.$('sMode').value,
        printCopies: Crm.$('sCopies').value,
        savedMsgs: Crm.$('sSavedMsgs').checked ? '1' : '0',
        theme: Crm._dark ? 'dark' : 'light'
      };
      Crm.call('crm.settings.save', payload).then(function () {
        Crm.toast('تنظیمات ذخیره شد.', 'ok');
      }, function () { Crm.toast('ذخیره تنظیمات ناموفق بود.', 'err'); });
    };

    /* system / backup card */
    var bc = Crm.el('div', 'crm-card');
    bc.innerHTML = '<div class="crm-card-title"><span class="dot"></span>پشتیبان‌گیری و چاپ</div>' +
      '<div class="crm-banner info">برای پشتیبان‌گیری از داده‌های بیماران یا تنظیمات چاپ، از دکمه‌های زیر استفاده کنید.</div>';
    var row = Crm.el('div', 'crm-toolbar');
    row.appendChild(Crm.el('div', 'spacer', ''));
    var bPrint = Crm.el('button', 'crm-btn outline', 'تنظیمات چاپ');
    var bBackup = Crm.el('button', 'crm-btn success', 'پشتیبان‌گیری');
    row.appendChild(bPrint);
    row.appendChild(bBackup);
    bc.appendChild(row);
    host.appendChild(bc);

    bPrint.onclick = function () {
      Crm.toast('در حال باز کردن تنظیمات چاپ…', 'info');
      Crm.call('crm.printConfig', {}).then(function () {}, function () { Crm.toast('عملیات ناموفق بود.', 'err'); });
    };
    bBackup.onclick = function () { Crm.nav('backup'); };
  }

  Crm.pages.settings = {
    title: 'تنظیمات و پشتیبان',
    render: function (host) { load(host); }
  };
})(window);
