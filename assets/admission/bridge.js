/* ============================================================================
   bridge.js — bidirectional C++ <-> JS IPC layer for the Admission UI.

   IMPORTANT: written in ES5-only syntax (no arrow functions, no const/let, no
   template literals, no Map/Set, no fetch, no spread/destructuring) so it parses
   and runs correctly on BOTH engines:
     (A) WebView2 (Chromium) — preferred, native postMessage transport.
     (B) MSHTML / Trident    — universal fallback that ships with every Windows;
                                it does NOT support ES2015+ syntax, which is why
                                every earlier version threw "Syntax error".

   Three transports, auto-selected at runtime:
     (A) WebView2  — window.chrome.webview.postMessage / addEventListener
     (B) EXTERNAL  — window.external.azCall(verb, payloadJson, page): the
                     v1.66.0 SERVERLESS bridge for the embedded MSHTML host
                     (synchronous IDispatch on the UI thread; push events
                     arrive via execScript -> window.azAdmissionReceive).
     (C) HTTP      — XMLHttpRequest POST /api/<verb> (dev harness only; the
                     in-app loopback host was removed in v1.66.0).

   Public API:
     Bridge.ready(cb)               -> called once the transport is up
     Bridge.call(verb, payload)     -> returns a thenable ({then,catch})
     Bridge.on(event, handler)      -> subscribe to C++ -> JS push events
   Messages are structured JSON with request/response IDs (script requirement).
   ============================================================================ */
(function (global) {
  'use strict';

  /* ---------------------------------------------------------------------- */
  /*  Tiny Promise-like thenable (ES5). Enough for our request/response flow */
  /*  without depending on a native Promise (MSHTML has none).               */
  /* ---------------------------------------------------------------------- */
  function Deferred() {
    this._st = 0;            /* 0 pending, 1 resolved, 2 rejected */
    this._val = null;
    this._ok = [];
    this._err = [];
  }
  Deferred.prototype.resolve = function (v) {
    if (this._st !== 0) return;
    this._st = 1; this._val = v;
    var i, a = this._ok;
    for (i = 0; i < a.length; i++) { this._safe(a[i], v); }
    this._ok = []; this._err = [];
  };
  Deferred.prototype.reject = function (e) {
    if (this._st !== 0) return;
    this._st = 2; this._val = e;
    var i, a = this._err;
    for (i = 0; i < a.length; i++) { this._safe(a[i], e); }
    this._ok = []; this._err = [];
  };
  Deferred.prototype._safe = function (fn, v) {
    try { fn(v); } catch (ex) { if (global.console) console.error(ex); reportSelf(ex, 'deferred.then'); }
  };
  /* returns a thenable object */
  Deferred.prototype.promise = function () {
    var self = this;
    var api = {
      then: function (onOk, onErr) {
        if (typeof onOk === 'function') {
          if (self._st === 1) self._safe(onOk, self._val);
          else if (self._st === 0) self._ok.push(onOk);
        }
        if (typeof onErr === 'function') {
          if (self._st === 2) self._safe(onErr, self._val);
          else if (self._st === 0) self._err.push(onErr);
        }
        return api;
      },
      'catch': function (onErr) { return api.then(null, onErr); }
    };
    return api;
  };

  /* ---------------------------------------------------------------------- */
  /*  State                                                                  */
  /* ---------------------------------------------------------------------- */
  var pending = {};            /* id -> Deferred */
  var listeners = {};          /* event -> [handlers] */
  var seq = 1;
  var transport = null;        /* 'webview' | 'external' | 'http' */
  var readyCbs = [];
  var isReady = false;
  var allowHttp = global.__AZADI_DEV_ALLOW_HTTP__ === true;

  /* v1.92.0: forward caught callback errors to C++ via 'client.error' so they
     reach the DEDICATED HTML error log (logs\html errors\), not the normal
     activity log. Re-entrancy guarded so a fault inside the reporter itself can
     never loop forever. `Bridge` (assigned below) is in scope by the time any
     callback catch ever runs. */
  var _bridgeErrGuard = false;
  function reportSelf(ex, where) {
    if (_bridgeErrGuard) return;
    _bridgeErrGuard = true;
    try {
      if (Bridge && typeof Bridge.call === 'function') {
        Bridge.call('client.error', {
          message: String((ex && ex.message) ? ex.message : ex).substring(0, 300),
          source: String(where || ''),
          line: 0,
          column: 0,
          stack: (ex && ex.stack) ? String(ex.stack).substring(0, 500) : ''
        });
      }
    } catch (x) { /* never let the reporter throw again */ }
    _bridgeErrGuard = false;
  }

  function fireReady() {
    if (isReady) return;
    isReady = true;
    var i;
    for (i = 0; i < readyCbs.length; i++) {
      try { readyCbs[i](); } catch (e) { if (global.console) console.error(e); reportSelf(e, 'bridge.ready'); }
    }
    readyCbs = [];
  }

  function dispatchEvent(name, data) {
    var hs = listeners[name];
    if (!hs) return;
    var i;
    for (i = 0; i < hs.length; i++) {
      try { hs[i](data); } catch (e) { if (global.console) console.error(e); reportSelf(e, 'bridge.event:' + name); }
    }
  }

  function parseJson(s) {
    if (typeof s !== 'string') return s;
    try { return JSON.parse(s); } catch (e) { return null; }
  }

  /* handle an inbound message object (from either transport) */
  function handleInbound(msg) {
    if (!msg || typeof msg !== 'object') return;
    if (msg.id && pending[msg.id]) {
      var d = pending[msg.id];
      delete pending[msg.id];
      if (msg.error) d.reject(new Error(msg.error));
      else d.resolve(msg.result != null ? msg.result : {});
      return;
    }
    if (msg.event) dispatchEvent(msg.event, msg.data || {});
  }

  /* ---------------------------------------------------------------- WebView2 */
  function initWebView() {
    var wv = global.chrome && global.chrome.webview;
    if (!wv) return false;
    transport = 'webview';
    wv.addEventListener('message', function (ev) {
      var d = ev.data;
      if (typeof d === 'string') { d = parseJson(d); if (!d) return; }
      handleInbound(d);
    });
    try { wv.postMessage({ verb: 'ready', id: 'ready' }); } catch (e) {}
    fireReady();
    return true;
  }

  function callWebView(verb, payload, timeoutMs) {
    var id = 'r' + (seq++);
    var d = new Deferred();
    pending[id] = d;
    try {
      global.chrome.webview.postMessage(
        { verb: verb, id: id, payload: payload || {} });
    } catch (e) { delete pending[id]; d.reject(e); return d.promise(); }
    var ms = timeoutMs || 15000;
    if (verb === 'svreport.data' && ms < 120000) ms = 120000;
    setTimeout(function () {
      if (pending[id]) { delete pending[id]; d.reject(new Error('timeout: ' + verb)); }
    }, ms);
    return d.promise();
  }

  /* ---------------------------------------------------------------- EXTERNAL */
  /* v1.66.0 serverless MSHTML bridge: window.external.azCall(verb, payload,
     page) returns the JSON reply synchronously. NOTE: Trident reports host
     methods as typeof 'unknown' (not 'function'), so feature-detect with a
     guarded probe call instead of a typeof check. */
  function externalUsable() {
    var ext = global.external;
    if (!ext) return false;
    try {
      if (typeof ext.azCall === 'undefined') return false;
      var r = ext.azCall('bridge.probe', '{}', 'admission');
      return typeof r === 'string' && r.indexOf('ok') >= 0;
    } catch (e) { return false; }
  }

  function initExternal() {
    if (!externalUsable()) return false;
    transport = 'external';
    fireReady();
    return true;
  }

  function callExternal(verb, payload) {
    var d = new Deferred();
    var body = '';
    try { body = JSON.stringify(payload || {}); } catch (e0) { body = '{}'; }
    /* defer so the thenable is returned before resolution (keeps the async
       contract admission.js expects even though azCall is synchronous). */
    setTimeout(function () {
      var txt = null;
      try { txt = global.external.azCall(verb, body, 'admission'); }
      catch (e1) { d.reject(e1); return; }
      var j = parseJson(txt);
      if (j && j.error) { d.reject(new Error(j.error)); return; }
      d.resolve(j != null ? j : {});
    }, 0);
    return d.promise();
  }

  /* -------------------------------------------------------------------- HTTP */
  /* Uses XMLHttpRequest (present in every engine incl. MSHTML). */
  function xhrPost(url, body, onOk, onErr) {
    var x;
    try { x = new XMLHttpRequest(); }
    catch (e) {
      try { x = new global.ActiveXObject('Microsoft.XMLHTTP'); }
      catch (e2) { if (onErr) onErr(new Error('no XHR')); return; }
    }
    try {
      x.open('POST', url, true);
      try { x.setRequestHeader('Content-Type', 'application/json'); } catch (e3) {}
      x.onreadystatechange = function () {
        if (x.readyState !== 4) return;
        if (x.status >= 200 && x.status < 300) {
          if (onOk) onOk(x.responseText);
        } else {
          if (onErr) onErr(new Error('HTTP ' + x.status));
        }
      };
      x.send(body || '{}');
    } catch (e4) { if (onErr) onErr(e4); }
  }

  function initHttp() {
    transport = 'http';
    fireReady();
    /* poll for pushed events (C++ -> JS) via /api/poll */
    function poll() {
      xhrPost('/api/poll', '{}',
        function (txt) {
          var j = parseJson(txt);
          if (j && j.events) {
            var i;
            for (i = 0; i < j.events.length; i++) handleInbound(j.events[i]);
          }
          setTimeout(poll, 900);
        },
        function () { setTimeout(poll, 1500); });
    }
    poll();
    return true;
  }

  function callHttp(verb, payload) {
    var d = new Deferred();
    xhrPost('/api/' + verb, JSON.stringify(payload || {}),
      function (txt) {
        var j = parseJson(txt);
        if (j && j.error) { d.reject(new Error(j.error)); return; }
        d.resolve(j != null ? j : {});
      },
      function (err) { d.reject(err); });
    return d.promise();
  }

  /* --------------------------------------------------------------- public API */
  var Bridge = {
    transport: function () { return transport; },
    ready: function (cb) { if (isReady) cb(); else readyCbs.push(cb); },
    on: function (event, handler) {
      if (!listeners[event]) listeners[event] = [];
      listeners[event].push(handler);
    },
    call: function (verb, payload, timeoutMs) {
      if (transport === 'webview') return callWebView(verb, payload, timeoutMs);
      if (transport === 'external') return callExternal(verb, payload);
      if (transport === 'http' && allowHttp) return callHttp(verb, payload);
      var d = new Deferred();
      d.reject(new Error('native bridge unavailable'));
      return d.promise();
    }
  };

  /* Production embedded pages must have a native transport. A standalone
     development harness may opt into HTTP before loading this script with
     window.__AZADI_DEV_ALLOW_HTTP__ = true. */
  if (!initWebView() && !initExternal() && allowHttp) initHttp();
  if (!transport) {
    try { global.__AZADI_BRIDGE_FATAL__ = 'native bridge unavailable'; } catch (e) {}
  }

  /* Native host can target one embedded admission view without broadcasting to
     every open tab. The payload uses the same inbound envelope as WebView2. */
  global.azAdmissionReceive = function (msg) {
    if (typeof msg === 'string') msg = parseJson(msg);
    handleInbound(msg);
  };
  global.Bridge = Bridge;
})(window);
