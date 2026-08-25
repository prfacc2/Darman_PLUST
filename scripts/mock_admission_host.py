#!/usr/bin/env python3
"""Mock loopback host that mimics the C++ /api bridge for the Patient-Admission
surface so its HTML/CSS/JS can be verified (layout, themes, services table,
invoice, queue mini-page) exactly as the embedded WebView host would serve it.

Run:  python3 scripts/mock_admission_host.py [port]
Then: http://127.0.0.1:8788/index.html
"""
import json
import os
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
ADM = os.path.join(ROOT, "assets", "admission")
SHELL = os.path.join(ROOT, "assets", "shell")

FILES = {
    "/": (ADM, "index.html", "text/html; charset=utf-8"),
    "/index.html": (ADM, "index.html", "text/html; charset=utf-8"),
    "/admission.css": (ADM, "admission.css", "text/css; charset=utf-8"),
    # v1.91.0 ordered theme layer (mirrors src/app.rc ids 420..424)
    "/css/core.css": (ADM, "css/core.css", "text/css; charset=utf-8"),
    "/css/surface-dash.css": (ADM, "css/surface-dash.css", "text/css; charset=utf-8"),
    "/css/surface-tools.css": (ADM, "css/surface-tools.css", "text/css; charset=utf-8"),
    "/css/surface-admission.css": (ADM, "css/surface-admission.css", "text/css; charset=utf-8"),
    "/css/surface-cashier.css": (ADM, "css/surface-cashier.css", "text/css; charset=utf-8"),
    "/admission.js": (ADM, "admission.js", "application/javascript; charset=utf-8"),
    "/bridge.js": (ADM, "bridge.js", "application/javascript; charset=utf-8"),
    "/contextmenu.js": (ADM, "contextmenu.js", "application/javascript; charset=utf-8"),
    "/vazir.ttf": (ADM, "vazir.ttf", "font/ttf"),
    "/common.css": (SHELL, "common.css", "text/css; charset=utf-8"),
    "/common.js": (SHELL, "common.js", "application/javascript; charset=utf-8"),
}

INSURANCES = [
    {"name": "آزاد (بدون بیمه)", "pct": 0},
    {"name": "تأمین اجتماعی", "pct": 70},
    {"name": "بیمه سلامت ایران", "pct": 70},
    {"name": "نیروهای مسلح", "pct": 80},
    {"name": "بیمه سلامت روستایی", "pct": 70},
    {"name": "بیمه سلامت همگانی", "pct": 60},
    {"name": "سایر", "pct": 50},
]
SUPP = [
    {"name": "بدون مکمل", "pct": 0},
    {"name": "بیمه دی", "pct": 80},
    {"name": "بیمه آسیا", "pct": 80},
    {"name": "بیمه ایران", "pct": 70},
    {"name": "بیمه پارسیان", "pct": 75},
]
CATALOG = [
    {"code": "90101", "name": "ویزیت پزشک عمومی", "category": "عمومی",
     "desc": "معاینه و ویزیت سرپایی", "price": 950000},
    {"code": "90214", "name": "تزریق عضلانی", "category": "خدمات پرستاری",
     "desc": "تزریق دارو به‌صورت عضلانی", "price": 320000},
    {"code": "90307", "name": "نوار قلب (ECG)", "category": "تشخیصی",
     "desc": "الکتروکاردیوگرام ۱۲ لید", "price": 780000},
    {"code": "90422", "name": "سرم‌تراپی", "category": "خدمات پرستاری",
     "desc": "تزریق سرم و مایعات وریدی", "price": 1450000},
    {"code": "90533", "name": "پانسمان ساده", "category": "خدمات پرستاری",
     "desc": "پانسمان زخم سطحی", "price": 410000},
    {"code": "90644", "name": "آزمایش قند خون ناشتا", "category": "آزمایشگاه",
     "desc": "FBS", "price": 260000},
]
DOCTORS = [
    {"code": "112233", "name": "دکتر مریم رستمی", "specialty": "پزشک عمومی"},
    {"code": "445566", "name": "دکتر امیر کاظمی", "specialty": "داخلی"},
    {"code": "778899", "name": "دکتر سارا موسوی", "specialty": "اطفال"},
]
QUEUE = []


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def _send(self, code, ctype, body):
        if isinstance(body, str):
            body = body.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        p = self.path.split("?")[0]
        ent = FILES.get(p)
        if not ent:
            self._send(404, "text/plain", "not found")
            return
        d, name, ct = ent
        with open(os.path.join(d, name), "rb") as f:
            body = f.read()
        if name == "index.html":
            marker = b'<script src="common.js"></script>'
            dev = b'<script>window.__AZADI_DEV_ALLOW_HTTP__ = true;</script>\n  ' + marker
            body = body.replace(marker, dev, 1)
            # index.html hosts SIX screens; the real app picks one by injecting
            # window.__azSurface from C++. Mirror that here so every surface can
            # be reviewed in a browser: /index.html?surface=dash|tools|receipts|
            # cashier|queue|admission  (default: admission).
            qs = self.path.split("?", 1)[1] if "?" in self.path else ""
            surface = ""
            for part in qs.split("&"):
                if part.startswith("surface="):
                    surface = part[len("surface="):]
            if surface in ("dash", "tools", "receipts", "cashier", "queue",
                           "admission"):
                inject = ('<script>window.__azSurface=%r;</script>'
                          % surface).encode("utf-8")
                body = body.replace(b"<body>", b"<body>" + inject, 1)
        self._send(200, ct, body)

    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        raw = self.rfile.read(n).decode("utf-8") if n else "{}"
        verb = self.path[5:].split("?")[0] if self.path.startswith("/api/") else ""
        try:
            args = json.loads(raw) if raw else {}
        except Exception:
            args = {}
        theme = os.environ.get("AZ_MOCK_THEME", "light")
        out = {"ok": True}
        if verb in ("init", "ready"):
            out = {
                "ok": True,
                "insurances": INSURANCES, "supp": SUPP,
                "date": "۱۴۰۵/۰۵/۱۲", "time": "۱۰:۲۴:۵۱", "shift": "صبح",
                "mode": os.environ.get("AZ_MOCK_MODE", "full"),
                "zoom": 100,
                "theme": theme,
                "palette": "blue",
                # C++ sends `user` as a plain display STRING (see the
                # `init` handler in src/web_admission_api.inc), so the
                # mock must too — an object here rendered as
                # "[object Object]" on the dashboard.
                "user": "زهرا احمدی",
                "userDept": "پذیرش عمومی",
                "ps": {"s": 37, "p": 29},
                "services": [],
            }
        elif verb == "poll":
            out = {"events": []}
        elif verb in ("service.search", "services.list", "service.list"):
            q = str(args.get("q", "")).strip()
            rows = [s for s in CATALOG if not q or q in s["name"] or q in s["code"]]
            out = {"ok": True, "rows": rows}
        elif verb in ("doctor.search", "doctors.list"):
            out = {"ok": True, "rows": DOCTORS}
        elif verb in ("patient.find", "patient.search", "patient.inquiry"):
            out = {"ok": True, "patient": {
                "nid": "0021548796", "first": "علی", "last": "محمدی",
                "father": "حسین", "gender": "مرد", "birth": "۱۳۷۲/۰۴/۱۱",
                "mobile": "09121234567", "phone": "02155667788",
                "addr": "تهران، خیابان آزادی، پلاک ۱۲",
                "file": "۱۰۲۴۸"}}
        elif verb == "admission.save":
            out = {"ok": True, "queueNo": 42, "printMode": "design-12",
                   "ps": {"s": 38, "p": 30}}
        elif verb in ("queue.list", "unpaid.list"):
            out = {"ok": True, "rows": QUEUE}
        elif verb in ("queue.add", "unpaid.add"):
            QUEUE.append({"id": str(len(QUEUE) + 1), "name": "علی محمدی",
                          "barcode": "10248", "date": "۱۴۰۵/۰۵/۱۲",
                          "time": "۱۰:۲۴", "minsAgo": 3, "amount": 950000})
            out = {"ok": True, "rows": QUEUE}
        elif verb in ("print.last", "print.rx", "print.ins"):
            out = {"ok": True}
        self._send(200, "application/json; charset=utf-8",
                   json.dumps(out, ensure_ascii=False))


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8788
    srv = ThreadingHTTPServer(("0.0.0.0", port), H)
    print("mock admission host on http://127.0.0.1:%d/index.html" % port)
    srv.serve_forever()
