/* ============================================================================
   contextmenu.js — right-click Copy / Cut / Paste for text fields only.
   ES5-only (runs on WebView2 AND MSHTML/Trident). Persian labels, RTL.

   Behaviour:
     * Right-click on an <input type=text/…> or <textarea>  -> Copy / Cut / Paste
       (+ "انتخاب همه"). Copy/Cut are disabled when there is no selection.
     * Right-click anywhere on the admission surface -> same menu plus F2
       (پاک کردن فیلدهای بیمار). The browser menu is always suppressed.
   Uses document.execCommand which is supported by both engines. On MSHTML,
   clipboardData is also used as a fallback for paste when execCommand('paste')
   is blocked.
   ============================================================================ */
(function (global) {
  'use strict';

  var menu = null;

  function isTextField(el) {
    if (!el || !el.tagName) return false;
    var tag = el.tagName.toLowerCase();
    if (tag === 'textarea') return !el.readOnly && !el.disabled;
    if (tag === 'input') {
      var t = (el.type || 'text').toLowerCase();
      var ok = (t === 'text' || t === 'search' || t === 'tel' || t === 'url' ||
                t === 'email' || t === 'number' || t === 'password' || t === '');
      return ok && !el.readOnly && !el.disabled;
    }
    return false;
  }

  function hasSelection(el) {
    try {
      if (typeof el.selectionStart === 'number') {
        return el.selectionEnd > el.selectionStart;
      }
    } catch (e) {}
    /* MSHTML fallback */
    try {
      var sel = document.selection;
      if (sel && sel.createRange) return sel.createRange().text.length > 0;
    } catch (e2) {}
    return false;
  }

  function buildMenu() {
    if (menu) return menu;
    menu = document.createElement('div');
    menu.id = 'ctxMenu';
    menu.className = 'ctx-menu';
    menu.style.display = 'none';
    menu.setAttribute('dir', 'rtl');
    document.body.appendChild(menu);
    return menu;
  }

  function addItem(label, key, enabled, onClick) {
    var it = document.createElement('div');
    it.className = 'ctx-item' + (enabled ? '' : ' disabled');
    var l = document.createElement('span'); l.className = 'ctx-label';
    l.appendChild(document.createTextNode(label));
    var s = document.createElement('span'); s.className = 'ctx-shortcut';
    s.appendChild(document.createTextNode(key || ''));
    it.appendChild(l); it.appendChild(s);
    if (enabled) {
      it.onmousedown = function (e) {
        e = e || window.event;
        if (e.preventDefault) e.preventDefault();
        if (e.stopPropagation) e.stopPropagation();
        hideMenu();
        onClick();
        return false;
      };
    }
    menu.appendChild(it);
  }

  function addSep() {
    var d = document.createElement('div');
    d.className = 'ctx-sep';
    menu.appendChild(d);
  }

  function hideMenu() {
    if (menu) menu.style.display = 'none';
  }

  function doExec(cmd) {
    try { document.execCommand(cmd, false, null); } catch (e) {}
  }

  function pasteInto(el) {
    /* Preferred: execCommand paste (works in MSHTML, some Chromium builds) */
    var ok = false;
    try { ok = document.execCommand('paste', false, null); } catch (e) {}
    if (ok) return;
    /* MSHTML clipboardData fallback */
    try {
      var w = global.clipboardData || (global.window && global.window.clipboardData);
      if (w && w.getData) {
        var txt = w.getData('Text');
        if (txt != null) insertAtCaret(el, txt);
        return;
      }
    } catch (e2) {}
    /* WebView2 async clipboard fallback */
    try {
      if (global.navigator && global.navigator.clipboard && global.navigator.clipboard.readText) {
        global.navigator.clipboard.readText().then(function (txt) {
          insertAtCaret(el, txt);
        });
      }
    } catch (e3) {}
  }

  function insertAtCaret(el, text) {
    if (text == null) return;
    try {
      if (typeof el.selectionStart === 'number') {
        var s = el.selectionStart, e = el.selectionEnd;
        var v = el.value;
        el.value = v.substring(0, s) + text + v.substring(e);
        var pos = s + text.length;
        el.selectionStart = el.selectionEnd = pos;
        el.focus();
        return;
      }
    } catch (ex) {}
    /* MSHTML range fallback */
    try {
      el.focus();
      var r = document.selection.createRange();
      r.text = text;
    } catch (ex2) {
      el.value = (el.value || '') + text;
    }
  }

  function resetPatient() {
    try {
      if (global.AzAdmission && global.AzAdmission.newPatient) {
        global.AzAdmission.newPatient();
      }
    } catch (e) {}
  }

  function showMenu(x, y, el) {
    buildMenu();
    menu.innerHTML = '';
    var text = isTextField(el);
    var sel = text && hasSelection(el);
    addItem('کپی', 'Ctrl+C', !!sel, function () { doExec('copy'); });
    addItem('برش', 'Ctrl+X', !!sel, function () { doExec('cut'); });
    addItem('چسباندن', 'Ctrl+V', text, function () { pasteInto(el); });
    if (text) {
      addSep();
      addItem('انتخاب همه', 'Ctrl+A', true, function () {
        try { el.focus(); el.select(); } catch (e) {}
      });
    }
    addSep();
    addItem('پاک کردن فیلدهای بیمار', 'F2', true, resetPatient);

    menu.style.display = 'block';
    /* position within viewport */
    var mw = menu.offsetWidth || 160, mh = menu.offsetHeight || 120;
    var vw = document.documentElement.clientWidth || global.innerWidth;
    var vh = document.documentElement.clientHeight || global.innerHeight;
    if (x + mw > vw) x = vw - mw - 4;
    if (y + mh > vh) y = vh - mh - 4;
    if (x < 0) x = 4; if (y < 0) y = 4;
    menu.style.left = x + 'px';
    menu.style.top = y + 'px';
  }

  function onContextMenu(e) {
    e = e || window.event;
    var el = e.target || e.srcElement;
    if (e.preventDefault) e.preventDefault();
    e.returnValue = false;
    if (isTextField(el)) {
      try { el.focus(); } catch (ex) {}
    }
    var x = (e.clientX != null) ? e.clientX : 0;
    var y = (e.clientY != null) ? e.clientY : 0;
    showMenu(x, y, el);
    return false;
  }

  function bind() {
    if (document.addEventListener) {
      document.addEventListener('contextmenu', onContextMenu, false);
      document.addEventListener('mousedown', function (e) {
        e = e || window.event;
        var t = e.target || e.srcElement;
        if (menu && menu.style.display === 'block') {
          /* click outside the menu closes it */
          var n = t;
          while (n) { if (n === menu) return; n = n.parentNode; }
          hideMenu();
        }
      }, false);
      document.addEventListener('scroll', hideMenu, true);
      global.addEventListener && global.addEventListener('resize', hideMenu, false);
    } else if (document.attachEvent) {
      document.attachEvent('oncontextmenu', onContextMenu);
      document.attachEvent('onmousedown', function () {
        if (menu && menu.style.display === 'block') hideMenu();
      });
    }
  }

  if (document.readyState === 'complete' || document.readyState === 'interactive') {
    setTimeout(bind, 1);
  } else if (document.addEventListener) {
    document.addEventListener('DOMContentLoaded', bind, false);
  } else {
    document.attachEvent('onreadystatechange', function () {
      if (document.readyState === 'complete') bind();
    });
  }
})(window);
