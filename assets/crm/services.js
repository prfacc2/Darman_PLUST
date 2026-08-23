/* ============================================================================
   services.js — Services (خدمات) management. ES5-only.
   v1.74 overhaul: the retired «دسته» (category) and free-text «بیمه» (insType)
   fields are gone. Each service now carries: کد، نام، بخش (section dropdown from
   crm.sections.list), نام بیمه (insurance dropdown), ضریب (multiplier) and six
   tariffs — قیمت آزاد / دولتی / بیمه, each with a «جدید» (new) companion. A
   professional-settings panel rounds, increases or decreases those tariffs in
   bulk (percent / numeric / multiplier modes, base + supplementary insurance
   scope, section + insurance filters) and persists through crm.services.adjust
   so the on-disk store (data\services.dat, owned by C++) stays the single source
   of truth that admission reads.
   ============================================================================ */
(function (global) {
  'use strict';
  var Crm = global.Crm;

  var ROUND_OPTS = [
    { v: 0, l: 'بدون گرد کردن' },
    { v: 5, l: '۵' }, { v: 50, l: '۵۰' }, { v: 500, l: '۵۰۰' },
    { v: 5000, l: '۵٬۰۰۰' }, { v: 50000, l: '۵۰٬۰۰۰' }
  ];

  var secCache = [];          /* top-level sections for the dropdowns */

  /* Live-format a Rial price input with thousand separators (Persian digits). */
  function fmtPriceInput(inp) {
    var digits = Crm.enDigits(inp.value || '').replace(/[^0-9]/g, '');
    inp.value = digits ? Crm.faDigits(Crm.fmtMoney(+digits)) : '';
  }
  function numVal(id) { return +Crm.enDigits((Crm.$(id) || { value: '' }).value.replace(/,/g, '')) || 0; }

  function fetchSections(onOk) {
    Crm.call('crm.sections.list', {}).then(function (d) {
      secCache = d.rows || [];
      var top = [];
      for (var i = 0; i < secCache.length; i++) if (!secCache[i].parentId) top.push(secCache[i]);
      onOk(top);
    }, function () { onOk([]); });
  }

  function sectionOptions(sel) {
    var o = '<option value="">— بدون بخش —</option>';
    for (var i = 0; i < secCache.length; i++) {
      if (secCache[i].parentId) continue;
      o += '<option value="' + Crm.esc(secCache[i].name) + '"' +
           (secCache[i].name === sel ? ' selected' : '') + '>' + Crm.esc(secCache[i].name) + '</option>';
    }
    return o;
  }

  /* insurance dropdown source depends on the scope (base vs supplementary).
     insName is stored as the insurance NAME, so the filter matches by name. */
  function insuranceOptions(scope, sel) {
    var list = (scope === 'supp')
      ? ((Crm.state.data && Crm.state.data.supp) || [])
      : ((Crm.state.data && Crm.state.data.insurances) || []);
    var o = '<option value="">— بدون بیمه —</option>';
    for (var i = 0; i < list.length; i++) {
      o += '<option value="' + Crm.esc(list[i].name) + '"' +
           (list[i].name === sel ? ' selected' : '') + '>' + Crm.esc(list[i].name) + '</option>';
    }
    return o;
  }

  function load(host, q) {
    fetchSections(function () {
      Crm.call('crm.services.list', { q: q || '' }).then(function (d) {
        render(host, d.rows || [], q);
      }, function () {
        host.innerHTML = '';
        Crm.head(host, 'خدمات', 'مدیریت فهرست خدمات و تعرفه‌ها');
        host.appendChild(Crm.el('div', 'crm-banner err', 'بارگذاری خدمات ناموفق بود.'));
      });
    });
  }

  /* ---- segmented control builder (returns html + wires clicks afterwards) -- */
  function segHtml(id, opts, sel) {
    var h = '<div class="crm-seg" id="' + id + '">';
    for (var i = 0; i < opts.length; i++) {
      h += '<button type="button" data-v="' + Crm.esc(opts[i].v) + '"' +
           (opts[i].v === sel ? ' class="on"' : '') + '>' + Crm.esc(opts[i].l) + '</button>';
    }
    return h + '</div>';
  }
  function segValue(id) {
    var box = Crm.$(id);
    if (!box) return '';
    var btns = box.getElementsByTagName('button');
    for (var i = 0; i < btns.length; i++) if (btns[i].className.indexOf('on') >= 0) return btns[i].getAttribute('data-v');
    return btns.length ? btns[0].getAttribute('data-v') : '';
  }
  function wireSeg(id, onCh) {
    var box = Crm.$(id);
    if (!box) return;
    var btns = box.getElementsByTagName('button');
    for (var i = 0; i < btns.length; i++) {
      (function (btn) {
        btn.onclick = function () {
          for (var k = 0; k < btns.length; k++) btns[k].className = '';
          btn.className = 'on';
          if (onCh) onCh();
        };
      })(btns[i]);
    }
  }

  function render(host, rows, q) {
    host.innerHTML = '';
    Crm.head(host, 'خدمات', 'مدیریت فهرست خدمات و تعرفه‌ها');

    /* ---------------- professional settings panel (collapsed advanced) -- */
    var adv = Crm.el('div', 'crm-card');
    var tog = Crm.el('button', 'crm-btn outline', 'تنظیمات پیشرفته قیمت‌گذاری ▾');
    tog.setAttribute('type', 'button');
    var panel = Crm.el('div', 'crm-panel');
    panel.style.display = 'none';
    panel.innerHTML =
      '<div class="crm-panel-title">تنظیمات حرفه‌ای قیمت‌گذاری</div>' +
      '<div class="crm-form">' +
      '<div class="crm-field"><label class="crm-label">گرد کردن قیمت</label>' +
        '<select class="crm-select" id="svRound">' + roundOptions(0) + '</select></div>' +
      '<div class="crm-field"><label class="crm-label">حالت محاسبه</label>' + segHtml('svMode', [
          { v: 'percent', l: 'درصد' }, { v: 'numeric', l: 'عددی' }, { v: 'multiplier', l: 'ضریب' }
        ], 'percent') + '</div>' +
      '<div class="crm-field"><label class="crm-label">جهت</label>' + segHtml('svDir', [
          { v: 'inc', l: 'افزایش' }, { v: 'dec', l: 'کاهش' }
        ], 'inc') + '</div>' +
      '<div class="crm-field"><label class="crm-label">مقدار</label>' +
        '<input class="crm-input" id="svAmount" value="" placeholder="مثال: ۱۰ یا ۱.۵" /></div>' +
      '<div class="crm-field"><label class="crm-label">قیمت هدف</label>' + segHtml('svTarget', [
          { v: 'free', l: 'آزاد' }, { v: 'gov', l: 'دولتی' }, { v: 'ins', l: 'بیمه' }
        ], 'free') + '</div>' +
      '<div class="crm-field"><label class="crm-label">دامنه بیمه</label>' + segHtml('svScope', [
          { v: 'base', l: 'بیمه پایه' }, { v: 'supp', l: 'بیمه تکمیلی' }
        ], 'base') + '</div>' +
      '<div class="crm-field"><label class="crm-label">بخش</label>' +
        '<select class="crm-select" id="svDept">' + sectionOptions('') + '</select></div>' +
      '<div class="crm-field"><label class="crm-label">بیمه</label>' +
        '<select class="crm-select" id="svIns">' + insuranceOptions('base', '') + '</select></div>' +
      '<div class="crm-field full"><span class="crm-banner info" id="svPreview" style="margin:0">پیش‌نمایش پس از وارد کردن مقدار و انتخاب فیلترها…</span></div>' +
      '</div>' +
      '<div class="crm-modal-foot" style="border-top:0;padding-top:0;margin-top:0">' +
        '<button class="crm-btn primary" id="svApply">اعمال روی خدمات</button>' +
      '</div>';
    tog.onclick = function () {
      var on = panel.style.display !== 'none';
      panel.style.display = on ? 'none' : 'block';
      tog.innerHTML = on ? 'تنظیمات پیشرفته قیمت‌گذاری ▾' : 'تنظیمات پیشرفته قیمت‌گذاری ▴';
    };
    adv.appendChild(tog);
    adv.appendChild(panel);
    host.appendChild(adv);
    wireSeg('svMode', updatePreview);
    wireSeg('svDir', updatePreview);
    wireSeg('svTarget', updatePreview);
    wireSeg('svScope', function () {
      var insSel = Crm.$('svIns');
      if (insSel) insSel.innerHTML = insuranceOptions(segValue('svScope'), '');
      updatePreview();
    });
    var amtEl = Crm.$('svAmount'); if (amtEl) amtEl.onkeyup = updatePreview;
    var deptEl = Crm.$('svDept'); if (deptEl) deptEl.onchange = updatePreview;
    var insEl = Crm.$('svIns'); if (insEl) insEl.onchange = updatePreview;
    Crm.$('svApply').onclick = function () { applyAdjust(host); };

    /* ---------------- toolbar + table ----------------------------------- */
    var tb = Crm.el('div', 'crm-toolbar');
    var search = Crm.el('div', 'crm-search');
    search.innerHTML =
      '<input class="crm-input" id="svQ" placeholder="جستجوی نام یا کد خدمت…" value="' + Crm.esc(q || '') + '" />' +
      '<span class="crm-search-ic"><svg viewBox="0 0 24 24" width="16" height="16"><path fill="currentColor" d="M10 2a8 8 0 105.3 14L20 20.7 21.7 19l-4.7-4.7A8 8 0 0010 2zm0 2a6 6 0 110 12 6 6 0 010-12z"/></svg></span>';
    tb.appendChild(search);
    tb.appendChild(Crm.el('div', 'spacer', ''));
    var addBtn = Crm.el('button', 'crm-btn primary', '+ افزودن خدمت');
    tb.appendChild(addBtn);
    host.appendChild(tb);

    host.appendChild(Crm.table([
      { key: 'i', label: 'ردیف', render: function (r, i) { return Crm.faDigits('' + (i + 1)); } },
      { key: 'code', label: 'کد', cls: 'c-mono', render: function (r) { return Crm.esc(r.code); } },
      { key: 'name', label: 'نام خدمت', render: function (r) { return '<b>' + Crm.esc(r.name) + '</b>'; } },
      { key: 'dept', label: 'بخش', render: function (r) { return Crm.esc(r.dept || '—'); } },
      { key: 'insName', label: 'بیمه', render: function (r) { return Crm.esc(r.insName || '—'); } },
      { key: 'multiplier', label: 'ضریب', cls: 'c-num', render: function (r) { return Crm.faDigits(Crm.esc(r.multiplier || '—')); } },
      { key: 'priceFree', label: 'آزاد', cls: 'c-num', render: function (r) { return Crm.faDigits(Crm.fmtMoney(r.priceFree)); } },
      { key: 'priceGov', label: 'دولتی', cls: 'c-num', render: function (r) { return Crm.faDigits(Crm.fmtMoney(r.priceGov)); } },
      { key: 'priceIns', label: 'بیمه', cls: 'c-num', render: function (r) { return Crm.faDigits(Crm.fmtMoney(r.priceIns)); } },
      { key: 'status', label: 'وضعیت', render: function (r) { return Crm.pill(r.status ? 'فعال' : 'غیرفعال', r.status ? 'on' : 'off'); } },
      { key: 'ops', label: 'عملیات', render: function (r) {
          var b = Crm.el('span');
          b.innerHTML = '<button class="crm-row-btn" data-act="edit">ویرایش</button>' +
                        '<button class="crm-row-btn danger" data-act="del">حذف</button>';
          b.childNodes[0].onclick = function () { openModal(host, r); };
          b.childNodes[1].onclick = function () { del(host, r); };
          return b;
        } }
    ], rows));

    var qEl = Crm.$('svQ');
    if (qEl) {
      qEl.onkeyup = function () {
        Crm.call('crm.services.list', { q: qEl.value }).then(function (d) { render(host, d.rows || [], qEl.value); qEl.focus(); });
      };
      if (q) { var v = qEl.value; qEl.value = ''; qEl.value = v; }
    }
    addBtn.onclick = function () { openModal(host, null); };
  }

  function roundOptions(sel) {
    var o = '';
    for (var i = 0; i < ROUND_OPTS.length; i++) {
      o += '<option value="' + ROUND_OPTS[i].v + '"' + (ROUND_OPTS[i].v === sel ? ' selected' : '') + '>' + Crm.esc(ROUND_OPTS[i].l) + '</option>';
    }
    return o;
  }

  /* mirror the C++ crm.services.adjust arithmetic so the preview is real */
  function computeNew(base, mode, dir, amount, round) {
    if (base <= 0) return null;
    var v = base;
    if (mode === 'percent') v = (dir === 'dec') ? v * (100 - amount) / 100 : v * (100 + amount) / 100;
    else if (mode === 'multiplier') v = v * amount;
    else v = (dir === 'dec') ? v - amount : v + amount;
    var r = Math.round(v);
    if (round > 0) { var m = r % round; if (m !== 0) r += (round - m); }
    if (r < 0) r = 0;
    return r;
  }

  function updatePreview() {
    var mode = segValue('svMode'), dir = segValue('svDir'), target = segValue('svTarget');
    var amount = +Crm.enDigits((Crm.$('svAmount') || { value: '' }).value) || 0;
    var round = +Crm.enDigits((Crm.$('svRound') || { value: '0' }).value) || 0;
    var dept = (Crm.$('svDept') || { value: '' }).value;
    var ins = (Crm.$('svIns') || { value: '' }).value;
    var prev = Crm.$('svPreview');
    if (!prev) return;
    if (!amount && mode !== 'numeric') { prev.innerHTML = 'مقدار را وارد کنید.'; return; }
    Crm.call('crm.services.list', { q: '', dept: dept, insName: ins }).then(function (d) {
      var rows = d.rows || [];
      var n = 0, sample = '';
      for (var i = 0; i < rows.length; i++) {
        var r = rows[i];
        var base = (target === 'gov') ? r.priceGov : (target === 'ins') ? r.priceIns : r.priceFree;
        var np = computeNew(base, mode, dir, amount, round);
        if (np !== null) { n++; if (!sample) sample = r.name; }
      }
      prev.innerHTML = 'پیش‌نمایش: ' + Crm.faDigits('' + n) + ' خدمت تحت تأثیر' +
        (sample ? ' (نمونه: ' + Crm.esc(sample) + ')' : '') + ' — هدف: ' +
        (target === 'gov' ? 'دولتی' : target === 'ins' ? 'بیمه' : 'آزاد');
    });
  }

  function applyAdjust(host) {
    var mode = segValue('svMode'), dir = segValue('svDir'), target = segValue('svTarget');
    var amount = (Crm.$('svAmount') || { value: '' }).value;
    var round = +Crm.enDigits((Crm.$('svRound') || { value: '0' }).value) || 0;
    var dept = (Crm.$('svDept') || { value: '' }).value;
    var ins = (Crm.$('svIns') || { value: '' }).value;
    if (!amount && mode !== 'numeric') { Crm.toast('مقدار را وارد کنید.', 'err'); return; }
    Crm.confirm('اعمال تغییرات قیمت روی خدمات مطابق فیلترها؟', function () {
      Crm.call('crm.services.adjust', {
        mode: mode, dir: dir, amount: amount, target: target,
        round: round, dept: dept, insName: ins
      }).then(function (d) {
        Crm.toast('تغییرات روی ' + Crm.faDigits('' + (d.changed || 0)) + ' خدمت اعمال شد.', 'ok');
        load(host, '');
      }, function () { Crm.toast('اعمال تغییرات ناموفق بود.', 'err'); });
    });
  }

  function priceField(id, label, val) {
    return '<div class="crm-field"><label class="crm-label">' + Crm.esc(label) + '</label>' +
           '<input class="crm-input" id="' + id + '" value="' + Crm.faDigits(Crm.fmtMoney(val || 0)) + '" /></div>';
  }

  function openModal(host, s) {
    var adding = !s;
    if (!s) s = { status: 1 };
    var m = Crm.modal(adding ? 'افزودن خدمت' : 'ویرایش خدمت', null);
    var body = m.body;
    body.innerHTML =
      '<div class="crm-form">' +
      '<input type="hidden" id="sOrig" value="' + Crm.esc(adding ? '' : s.code) + '" />' +
      '<div class="crm-field"><label class="crm-label">کد خدمت</label>' +
        '<input class="crm-input" id="sCode" value="' + Crm.esc(s.code || '') + '" placeholder="خالی = خودکار" /></div>' +
      '<div class="crm-field"><label class="crm-label">نام خدمت</label>' +
        '<input class="crm-input" id="sName" value="' + Crm.esc(s.name || '') + '" /></div>' +
      '<div class="crm-field"><label class="crm-label">بخش</label>' +
        '<select class="crm-select" id="sDept">' + sectionOptions(s.dept || '') + '</select></div>' +
      '<div class="crm-field"><label class="crm-label">نام بیمه</label>' +
        '<select class="crm-select" id="sIns">' + insuranceOptions('base', s.insName || '') + '</select></div>' +
      '<div class="crm-field"><label class="crm-label">ضریب</label>' +
        '<input class="crm-input" id="sMult" value="' + Crm.esc(s.multiplier || '') + '" placeholder="مثال: ۱.۵" /></div>' +
      priceField('sPriceFree', 'قیمت آزاد (ریال)', s.priceFree) +
      priceField('sPriceFreeNew', 'قیمت آزاد جدید (ریال)', s.priceFreeNew) +
      priceField('sPriceGov', 'قیمت دولتی (ریال)', s.priceGov) +
      priceField('sPriceGovNew', 'قیمت دولتی جدید (ریال)', s.priceGovNew) +
      priceField('sPriceIns', 'قیمت بیمه (ریال)', s.priceIns) +
      priceField('sPriceInsNew', 'قیمت بیمه جدید (ریال)', s.priceInsNew) +
      '<div class="crm-field full"><label class="crm-label">شرح / توضیحات</label>' +
        '<textarea class="crm-textarea" id="sDesc">' + Crm.esc(s.desc || '') + '</textarea></div>' +
      '<div class="crm-field full"><label class="crm-check"><input type="checkbox" id="sActive" ' + (s.status ? 'checked' : '') + ' />فعال</label></div>' +
      '</div>';
    var foot = Crm.el('div', 'crm-modal-foot');
    foot.innerHTML = '<button class="crm-btn ghost" id="mCancel">انصراف</button><button class="crm-btn primary" id="mSave">ذخیره</button>';
    m.card.appendChild(foot);
    Crm.$('mCancel').onclick = m.close;
    var pids = ['sPriceFree', 'sPriceFreeNew', 'sPriceGov', 'sPriceGovNew', 'sPriceIns', 'sPriceInsNew'];
    for (var i = 0; i < pids.length; i++) {
      var pe = Crm.$(pids[i]);
      if (pe) { (function (elx) { elx.onkeyup = function () { fmtPriceInput(elx); }; elx.onblur = function () { fmtPriceInput(elx); }; })(pe); }
    }
    Crm.$('mSave').onclick = function () {
      var payload = {
        originalCode: Crm.$('sOrig').value,
        code: Crm.$('sCode').value,
        name: Crm.$('sName').value,
        dept: Crm.$('sDept').value,
        insName: Crm.$('sIns').value,
        multiplier: Crm.enDigits(Crm.$('sMult').value),
        priceFree: numVal('sPriceFree'),
        priceFreeNew: numVal('sPriceFreeNew'),
        priceGov: numVal('sPriceGov'),
        priceGovNew: numVal('sPriceGovNew'),
        priceIns: numVal('sPriceIns'),
        priceInsNew: numVal('sPriceInsNew'),
        desc: Crm.$('sDesc').value,
        active: Crm.$('sActive').checked
      };
      if (!payload.name) { Crm.toast('نام خدمت الزامی است.', 'err'); return; }
      Crm.call('crm.services.save', payload).then(function () {
        Crm.toast(adding ? 'خدمت اضافه شد.' : 'خدمت ویرایش شد.', 'ok');
        m.close(); load(host, '');
      }, function () { Crm.toast('ذخیره ناموفق بود.', 'err'); });
    };
  }

  function del(host, s) {
    Crm.confirm('حذف خدمت «' + s.name + '» (' + s.code + ')؟', function () {
      Crm.call('crm.services.delete', { code: s.code }).then(function () {
        Crm.toast('خدمت حذف شد.', 'ok'); load(host, '');
      }, function () { Crm.toast('حذف ناموفق بود.', 'err'); });
    }, { danger: true });
  }

  Crm.pages.services = {
    title: 'خدمات',
    render: function (host) { load(host, ''); }
  };
})(window);
