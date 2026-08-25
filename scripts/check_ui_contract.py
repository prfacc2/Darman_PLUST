#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
check_ui_contract.py — the UI contract guard for درمان پلاس (DarmanPlus).

WHY THIS EXISTS
===============
This program has many screens, and it is developed almost entirely by AI models
working one release at a time. Two failure modes kept happening:

  1. A release that only meant to touch «پذیرش بیمار» silently deleted or broke
     an element, a screen or a behaviour somewhere else.
  2. Every release appended a new "final, wins-the-cascade" CSS layer on top of
     the previous one. By v1.90 `admission.css` carried SIXTEEN stacked layers
     (~4400 lines) fighting each other, and nobody could predict what a colour
     change would actually do.

So the contract is now executable. This script is the machine-readable half of
`AGENTS.md` + `docs/DESIGN_SYSTEM.md`, and `build.sh` runs it on every build.
If you are an AI model working on this repository: making this script pass is
not optional, and "deleting the assertion" is never the fix.

WHAT IT ENFORCES
================
  1. ASSET REGISTRY   — the embedded-asset ids in src/app.rc, the inliner table
                        in src/web_admission_embed.inc and the <link>/<script>
                        markers in index.html all agree. The inliner replaces
                        those markers by exact string match exactly once, so a
                        drifted marker produces a blank screen on a customer's
                        machine, not a build error.
  2. LAYER DISCIPLINE — admission.css stays the structure layer and does not
                        grow a new trailing "polish pass"; every theme colour
                        lives in exactly one owned file under css/.
  3. DOM CONTRACT     — every element id each surface's JavaScript depends on
                        still exists, per surface. This is the check that stops
                        an unrelated release from deleting a screen's guts.
  4. DUAL-ENGINE CSS  — the page must render on WebView2 (Chromium) AND on the
                        MSHTML/Trident engine that ships with older Windows.
                        No var(), no grid, no flex gap, no backdrop-filter, no
                        logical properties, -ms- flex twins required.
  5. PALETTE          — the colour families the product owner rejected (violet
                        / purple / magenta / pink / cream / beige / clay) may
                        not reappear anywhere in the theme layer.
  6. ES5 ONLY         — admission.js must stay ES5; Trident has no let/const/
                        arrow functions/template literals.
  7. BEHAVIOUR ANCHORS— a few load-bearing invariants that were regressed
                        before (queue overlay geometry, invoice ordering,
                        service-table column count, retired controls staying
                        retired).

Run:  python3 scripts/check_ui_contract.py
Exit: 0 = contract holds, 1 = contract violated (build must fail).
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ADM = ROOT / "assets" / "admission"
CSSDIR = ADM / "css"
HTML_PATH = ADM / "index.html"
JS_PATH = ADM / "admission.js"
STRUCT_CSS = ADM / "admission.css"
APP_RC = ROOT / "src" / "app.rc"
EMBED_INC = ROOT / "src" / "web_admission_embed.inc"

fails = []
notes = []


def need(cond, msg):
    if not cond:
        fails.append(msg)
    return bool(cond)


def strip_css_comments(text):
    return re.sub(r"/\*.*?\*/", "", text, flags=re.S)


def strip_js_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"(?m)^\s*//.*$", "", text)


def strip_html_comments(text):
    return re.sub(r"<!--.*?-->", "", text, flags=re.S)


def declaration_values(css_body):
    """Yield only the VALUE side of each declaration.

    Colour literals live in values; `#foo` in a selector is an id, not a
    colour, so scanning whole rules would produce nonsense failures.
    """
    for rule in re.findall(r"\{([^{}]*)\}", css_body):
        for decl in rule.split(";"):
            if ":" in decl:
                yield decl.split(":", 1)[1]


for p in (HTML_PATH, JS_PATH, STRUCT_CSS, APP_RC, EMBED_INC):
    if not p.exists():
        print("FAIL — missing required file: %s" % p.relative_to(ROOT))
        sys.exit(1)

html_raw = HTML_PATH.read_text(encoding="utf-8")
html = strip_html_comments(html_raw)
js_raw = JS_PATH.read_text(encoding="utf-8")
js = strip_js_comments(js_raw)
rc = APP_RC.read_text(encoding="utf-8")
embed = EMBED_INC.read_text(encoding="utf-8")

# ===========================================================================
# 1. ASSET REGISTRY — app.rc ids  ==  inliner table  ==  index.html markers
# ===========================================================================
# The theme layer: one exclusively-owned file per layer, in a fixed load order.
THEME_LAYERS = [
    ("css/core.css", 420),
    ("css/surface-dash.css", 421),
    ("css/surface-tools.css", 422),
    ("css/surface-admission.css", 423),
    ("css/surface-cashier.css", 424),
]

# Assets that are inlined by exact-string marker replacement. Drift here is
# invisible at build time and fatal at runtime, so it is checked hard.
BASE_MARKERS = [
    ('<link rel="stylesheet" href="common.css" />', 500),
    ('<link rel="stylesheet" href="admission.css" />', 401),
    ('<script src="common.js"></script>', 501),
    ('<script src="bridge.js"></script>', 402),
    ('<script src="contextmenu.js"></script>', 405),
    ('<script src="admission.js"></script>', 403),
]

for marker, _rid in BASE_MARKERS:
    n = html_raw.count(marker)
    need(n == 1,
         "asset marker must appear EXACTLY once in index.html (found %d): %s"
         % (n, marker))

prev_pos = -1
for href, rid in THEME_LAYERS:
    path = ADM / href
    need(path.exists(), "theme layer file is missing: assets/admission/%s" % href)
    if path.exists():
        body = strip_css_comments(path.read_text(encoding="utf-8")).strip()
        # The inliner treats an empty layer as a hard failure and ships a blank
        # page, so an all-comments layer must not reach a release.
        need(len(body) > 0,
             "theme layer has no rules — the inliner would ship a blank page: "
             "assets/admission/%s" % href)

    marker = '<link rel="stylesheet" href="%s" />' % href
    n = html_raw.count(marker)
    need(n == 1,
         "theme layer <link> must appear EXACTLY once in index.html (found %d): %s"
         % (n, href))
    if n == 1:
        pos = html_raw.index(marker)
        need(pos > prev_pos,
             "theme layer load order in index.html does not match the contract "
             "at: %s" % href)
        prev_pos = pos

    need(re.search(r"^%d\s+RCDATA\s+\"\.\./assets/admission/%s\"" %
                   (rid, re.escape(href)), rc, re.M) is not None,
         "src/app.rc is missing the RCDATA entry %d for %s" % (rid, href))

    need(re.search(r"\{\s*\"%s\"\s*,\s*%d\s*\}" % (re.escape(href), rid),
                   embed) is not None,
         "src/web_admission_embed.inc kAdThemeLayers is missing { \"%s\", %d }"
         % (href, rid))

# The load order in the inliner table must match the order in index.html.
tbl_order = re.findall(r"\{\s*\"(css/[a-z0-9.\-]+)\"\s*,\s*\d+\s*\}", embed)
need(tbl_order == [h for h, _ in THEME_LAYERS],
     "src/web_admission_embed.inc load order %r does not match the contract %r"
     % (tbl_order, [h for h, _ in THEME_LAYERS]))

# No id may be registered twice in app.rc.
rc_ids = re.findall(r"^(\d+)\s+RCDATA\s", rc, re.M)
dupes = sorted(set(i for i in rc_ids if rc_ids.count(i) > 1))
need(not dupes, "duplicate RCDATA ids in src/app.rc: %s" % ", ".join(dupes))

# ===========================================================================
# 2. LAYER DISCIPLINE
# ===========================================================================
struct_raw = STRUCT_CSS.read_text(encoding="utf-8")
need("END OF THE STRUCTURE LAYER" in struct_raw,
     "assets/admission/admission.css lost its 'END OF THE STRUCTURE LAYER' "
     "marker — that marker is what keeps the file from growing a new "
     "trailing override layer")

tail = struct_raw.split("END OF THE STRUCTURE LAYER", 1)[1]
need("{" not in strip_css_comments(tail),
     "a new CSS layer was appended to the bottom of admission.css. Do not "
     "stack another 'polish pass' there — edit the css/ layer that owns the "
     "thing you are changing (see docs/DESIGN_SYSTEM.md)")

css_files = [STRUCT_CSS] + sorted(CSSDIR.glob("*.css"))
owned = set(h for h, _ in THEME_LAYERS)
for extra in sorted(CSSDIR.glob("*.css")):
    rel = "css/" + extra.name
    need(rel in owned,
         "assets/admission/%s is not part of the registered theme layer. Every "
         "stylesheet must be registered in app.rc + the inliner + index.html, "
         "otherwise it is dead weight that never reaches the EXE." % rel)

# Each surface layer must stay inside its own surface (a layer that restyles
# another surface is how the cascade wars started).
SURFACE_SCOPE = {
    "surface-dash.css": ["surface-dash"],
    "surface-tools.css": ["surface-tools", "surface-rc"],
    "surface-admission.css": ["surface-adm"],
    "surface-cashier.css": ["surface-cash", "surface-queue"],
}
for fname, allowed in SURFACE_SCOPE.items():
    p = CSSDIR / fname
    if not p.exists():
        continue
    body = strip_css_comments(p.read_text(encoding="utf-8"))
    used = set(re.findall(r"surface-(?:adm|dash|tools|cash|queue|rc)", body))
    stray = sorted(u for u in used if u not in allowed)
    need(not stray,
         "assets/admission/css/%s styles a surface it does not own: %s "
         "(allowed: %s)" % (fname, ", ".join(stray), ", ".join(allowed)))

# ===========================================================================
# 3. DOM CONTRACT — per surface
# ===========================================================================
# Every id here is depended on by admission.js or by the C++ bridge. If a
# release removes one, that screen breaks. Adding ids is fine; removing one
# means you must also remove its consumer and update this list deliberately.
DOM_CONTRACT = {
    "shell": [
        "loader", "loaderText", "app", "appBody", "toast",
        "opsDlg", "opsDlgMsg", "opsDlgNo", "opsDlgYes",
        "cancelDlg", "cancelAcct", "cancelReason", "cancelDlgNo", "cancelDlgYes",
        "blockModal", "blockReason", "blockRemaining", "blockClose",
        "blockOverride", "blockUnblock",
    ],
    "admission": [
        "colRight", "colCenter", "colLeft",
        "pfName", "pfFile", "profileState", "profileStateText", "btnErx",
        "apptDate", "apptShift", "btnSave", "btnClear",
        "queueLauncher", "toolsBtn",
        "btnPrtLast", "btnPrtRx", "btnPrtIns", "syncBadge", "syncText",
        "first", "last", "nid", "birth", "gender", "mobile", "father",
        "phone", "addr",
        "insMain", "insType", "ptype", "insSuppPct",
        "doc2code", "docResults", "doc2name", "perfcode", "perfname",
        "counterStrip", "psPVal", "psSVal", "tpReg", "tpWait",
        "svcCount", "svcSearch", "svcAddBtn", "svcToggle", "svcSuggest",
        "svcTblWrap", "svcBody", "svcFoot", "sfTotal", "sfDisc", "sfIns", "sfPat",
        "insExtraHidden", "hasIns", "insBooklet", "insValid", "rxDate", "insSupp",
        "invoiceCard", "invoiceToggle",
        "invGMain", "invGSupp", "invGFin",
        "invMainTotal", "invMainPat", "invMainOrg",
        "invSuppTotal", "invSuppShare", "invSuppPat",
        "invFinTotal", "invFinDisc", "invFinPaid", "invRemain",
        "payableCard", "tcVal", "paymentCard", "addToQueueBtn", "addToAdmQBtn",
    ],
    "dash": [
        "dashPanel", "dashBurger", "dashUser", "dashMail", "dashMailBadge",
        "dashQ", "dashGrid", "dashNewPat", "dashNewTab", "dashPortal",
        "dashEmpty", "dashDrawerBk", "dashDrawer", "dashDrawerClose",
        "dashDrawerQ", "dashDrawerBody",
    ],
    "tools": [
        "toolsPanel", "toolsHome", "toolsBurger", "toolsBack", "toolsQ",
        "toolsGrid", "toolsReceipts", "toolsCash", "toolsQueue", "toolsEmpty",
        "toolsDrawerBk", "toolsDrawer", "toolsDrawerClose", "toolsDrawerQ",
        "toolsDrawerBody",
    ],
    "receipts": [
        "toolsReceiptsView", "rcBack", "rcHelp", "rcPrint", "rcExcel",
        "rcDelete", "rcSect", "rcFrom", "rcTo", "rcFirst", "rcLast", "rcNid",
        "rcMobile", "rcFile", "rcArch", "rcBar", "rcDoc", "rcOnlyUser",
        "rcByAppt", "rcSearchBtn", "rcWrap", "rcTable", "rcAll", "rcBody",
        "rcPager", "rcPrev", "rcPageLbl", "rcNext",
    ],
    "queue": [
        "queueBackdrop", "queuePanel", "qovSubtitle", "queueTabs", "tabQueue",
        "tabAdmQ", "qMinutes", "qSearch", "queueToggle", "queueClose",
        "qCountSum", "qCount", "queueWrap", "queueBody",
    ],
    "cashier": [
        "cashBackdrop", "cashPanel", "cashSubtitle", "cashTabs", "cashIncome",
        "cashSearch", "cashClose", "cashShiftStart", "cashShiftEnd",
        "cashManualBtn", "cashManualBox", "cmNid", "cmFirst", "cmLast",
        "cmDoc", "cmAmt", "cmSave", "cmCancel", "cashSummary",
        "cashStatP", "cashStatPaid", "cashStatUnpaid", "cashStatQ",
        "cashShiftMeta", "cashWrap", "cashBody",
    ],
}

present = set(re.findall(r'\bid="([A-Za-z0-9_\-]+)"', html))
for surface, ids in sorted(DOM_CONTRACT.items()):
    missing = [i for i in ids if i not in present]
    need(not missing,
         "surface '%s' lost required element id(s): %s — a release must never "
         "delete another screen's elements" % (surface, ", ".join(missing)))

# The six surfaces must all still be reachable from the page runtime.
for surf in ("surface-adm", "surface-dash", "surface-tools", "surface-cash",
             "surface-queue", "surface-rc"):
    need(surf in js,
         "admission.js no longer knows the body class '%s' — a surface was "
         "dropped from applySurfaceClass()" % surf)

# Retired controls must stay retired (each of these came back by accident once).
for gone in ("btnZoomIn", "btnZoomOut", "noPay", "btnNew", "btnCancel",
             "navUnpaid", "navAdmQ"):
    need('id="%s"' % gone not in html,
         "the retired control '%s' is back in index.html" % gone)

# ===========================================================================
# 4. BEHAVIOUR ANCHORS
# ===========================================================================
# One concatenated view of every registered stylesheet, used by the anchors
# below and by the queue-overlay geometry check.
qcss = "\n".join(strip_css_comments(p.read_text(encoding="utf-8"))
                 for p in css_files)

# The services table row builder hard-codes 11 columns and writes cells[8]/[9];
# the empty state must use the same span or the table visibly breaks.
need('colspan="11"' in html or 'colspan="11"' in js,
     "the services table empty state no longer spans 11 columns — "
     "renderServices()/refreshRowCells() hard-code 11 service columns")
need("tr.cells.length < 11" in js or "cells.length<11" in js.replace(" ", ""),
     "refreshRowCells() no longer guards the 11-column service row shape")

# صورت حساب must stay above مبلغ نهایی.
if 'id="invoiceCard"' in html and 'id="payableCard"' in html:
    need(html.index('id="invoiceCard"') < html.index('id="payableCard"'),
         "«صورت حساب» must render above «مبلغ نهایی»")

# The patient profile card leads the right rail, above «عملیات پذیرش».
mright = re.search(r'id="colRight"(.*?)</aside>', html, re.S)
if mright:
    rail = mright.group(1)
    if "profile-card" in rail and "action-card" in rail:
        need(rail.index("profile-card") < rail.index("action-card"),
             "the patient profile card must lead the right rail, above "
             "«عملیات پذیرش»")

# The «اطلاعات تکمیلی بیمه» card was retired; its fields live hidden in
# #insExtraHidden so billing keeps working. The card must not come back.
need("اطلاعات تکمیلی بیمه" not in html,
     "the retired «اطلاعات تکمیلی بیمه» card is back — its fields belong in "
     "the hidden #insExtraHidden block")

# «ثبت قبض» is the primary submit and must read as the primary (indigo) action,
# never as a green/success button — that confusion was reported before.
submit_rules = re.findall(
    r"(?:^|\})([^{}]*\.btn-submit[^{}]*)\{([^{}]*)\}", qcss)
submit_body = " ".join(b for _s, b in submit_rules).upper()
if submit_body:
    need(not re.search(r"#(16A34A|22C55E|16C47F|0F7A4E|0E9C77|075E45)",
                       submit_body),
         "«ثبت قبض» (.btn-submit) is being painted green — it must stay the "
         "indigo primary action")

# The cashier's «صندوق نرفته‌ها» tab strip must NOT sit in the header any more
# (the owner asked for it directly above the table).
mcash = re.search(r'id="cashPanel"(.*?)</section>', html, re.S)
if mcash:
    panel = mcash.group(1)
    mhead = re.search(r'class="[^"]*queue-overlay-head[^"]*"(.*?)</div>\s*<div',
                      panel, re.S)
    if mhead:
        need('id="cashTabs"' not in mhead.group(1),
             "«صندوق نرفته‌ها» / the cashier tab strip is back in the cashier "
             "header — it must sit directly above the table")
    if 'id="cashTabs"' in panel and 'id="cashWrap"' in panel:
        need(panel.index('id="cashTabs"') < panel.index('id="cashWrap"'),
             "the cashier tab strip must render above the cashier table")

# The queue overlay is a real full-screen overlay sized to the monitor.
mq = re.findall(r"(?:^|\})([^{}]*\.queue-overlay[^{}]*)\{([^{}]*)\}", qcss)
qbody = " ".join(b for _s, b in mq)
need("position:fixed" in qbody.replace(" ", ""),
     ".queue-overlay lost position:fixed — the full-screen queue/cashier "
     "overlay must stay sized to the user's monitor")

# ---- SURFACE GATING -------------------------------------------------------
# index.html hosts six screens in one document, so exactly one subtree may be
# visible at a time. This gating is cross-surface STRUCTURE and therefore lives
# in admission.css — a per-surface file in css/ is not allowed to contain it
# (§2 above forbids cross-surface selectors), so if it goes missing from the
# structure layer it goes missing entirely and screens stack on top of each
# other. That is exactly what happened once; hence this check.
struct_css = strip_css_comments(struct_raw)
for surf in ("surface-dash", "surface-tools", "surface-rc", "surface-cash",
             "surface-queue"):
    need(re.search(r"body\.%s\s+#appBody" % surf, struct_css) is not None,
         "admission.css is missing the gating rule 'body.%s #appBody{display:"
         "none}' — without it that surface renders the admission form "
         "underneath itself" % surf)
need(re.search(r"(^|\})\s*\.dash-panel\s*\{[^{}]*display\s*:\s*none",
               struct_css) is not None,
     "admission.css is missing the base '.dash-panel{display:none}' rule — "
     "without it the dashboard leaks onto every other surface")
for surf in ("surface-adm", "surface-cash", "surface-queue"):
    need(re.search(r"body\.%s\s+\.tools-panel" % surf, struct_css) is not None,
         "admission.css is missing the gating rule hiding .tools-panel on "
         "body.%s" % surf)
# Base hide rules: without these, a panel leaks onto every surface that does not
# explicitly hide it. Both of these went missing once and rendered the whole
# tools rail + receipt-search screen underneath the dashboard.
for cls in (".dash-panel", ".tools-panel", ".tools-home", ".tools-receipts"):
    need(re.search(r"(^|\})[^{}]*\%s\s*(,[^{}]*)?\{[^{}]*display\s*:\s*none"
                   % cls, struct_css) is not None,
         "admission.css is missing the base '%s{display:none}' rule — without "
         "it that panel leaks onto other surfaces" % cls)
# ...and the matching reveal, or the screen can never be shown at all.
for pair in (".tools-home", ".tools-receipts"):
    need(re.search(r"\%s\.show" % pair, struct_css) is not None,
         "admission.css is missing the '%s.show' reveal rule that pairs with "
         "its base display:none" % pair)

# ---- COMPONENT COVERAGE ---------------------------------------------------
# Every visual component below must be styled by SOME registered stylesheet.
# This is the second half of "a release must not break another screen": the DOM
# contract above catches a deleted element, and this catches a component whose
# styling was deleted, which is just as damaging and much easier to miss. It was
# added after the v1.91 CSS restructure silently left the whole queue-overlay
# row family (.q-*) unstyled: the markup was intact, so nothing else noticed.
COMPONENT_COVERAGE = {
    "shell": ["card", "card-head", "card-title", "ct-ico", "fld", "inp", "btn",
              "btn-primary", "btn-outline", "btn-danger", "btn-ico", "tbl",
              "tbl-wrap", "empty", "mini-list", "loader", "ctx-menu",
              "modal-card", "ops-dlg"],
    "admission": ["profile-card", "patient-card", "ins-card", "doc-card",
                  "perf-card", "svc-card", "svc-tbl", "svc-empty",
                  "invoice-card", "inv-line", "payable-card", "payable-value",
                  "payment-card", "action-card", "print-card", "datetime-card",
                  "counter-strip", "cs-item", "doc-results",
                  "doc-name-display", "queue-action-btns"],
    "dash": ["dash-panel", "dash-topbar", "dash-search", "dash-vessel",
             "dash-app", "dash-appico", "dash-appname", "dash-mail-badge",
             "dash-drawer", "dash-drawer-item", "dash-cat"],
    "tools": ["tools-panel", "tools-topbar", "tools-grid", "tools-tile",
              "tools-appico", "tools-tile-name", "tools-drawer",
              "tools-drawer-item", "tools-cat"],
    "receipts": ["tools-receipts", "rc-help-hdr"],
    "queue": ["queue-overlay", "queue-overlay-head", "qov-summary",
              "qov-sum-card", "qov-sum-hint", "q-pname", "q-avatar",
              "q-file-badge", "q-wait-pill", "q-act", "q-empty", "q-time",
              "q-date", "q-idx"],
    "cashier": ["cash-overlay", "cash-tab", "cash-income", "cash-summary",
                "cash-tbl", "cash-unpaid"],
}
all_css = "\n".join(strip_css_comments(p.read_text(encoding="utf-8"))
                    for p in css_files)
for surface, comps in sorted(COMPONENT_COVERAGE.items()):
    bare = [c for c in comps
            if not re.search(r"\.%s\b" % re.escape(c), all_css)]
    need(not bare,
         "surface '%s' has component(s) with NO styling in any registered "
         "stylesheet: %s — a component whose CSS was deleted renders as "
         "unstyled text; restore it in the layer that owns it"
         % (surface, ", ".join("." + c for c in bare)))

# ===========================================================================
# 5. DUAL-ENGINE CSS CONTRACT
# ===========================================================================
FLEX_TWINS = [
    (r"display\s*:\s*flex", "display:-ms-flexbox"),
    (r"flex-direction\s*:", "-ms-flex-direction:"),
    (r"align-items\s*:", "-ms-flex-align:"),
    (r"justify-content\s*:", "-ms-flex-pack:"),
    (r"flex-wrap\s*:", "-ms-flex-wrap:"),
]

for path in css_files:
    rel = path.relative_to(ROOT)
    raw = path.read_text(encoding="utf-8")
    body = strip_css_comments(raw)

    need(body.count("{") == body.count("}"),
         "%s has unbalanced braces (%d '{' vs %d '}')"
         % (rel, body.count("{"), body.count("}")))

    need("var(--" not in body,
         "%s uses CSS custom properties — Trident cannot resolve var()" % rel)
    need(not re.search(r"display\s*:\s*(inline-)?grid", body),
         "%s uses CSS grid — Trident has no modern grid support" % rel)
    need(not re.search(r"grid-template", body),
         "%s uses grid-template-* — not supported on Trident" % rel)
    need(not re.search(r"(^|[;{\s])(grid-)?gap\s*:", body),
         "%s uses flex/grid gap — use real margins instead" % rel)
    need("backdrop-filter" not in body,
         "%s uses backdrop-filter — Trident ignores it, so 'glass' must be "
         "faked with layered gradients (docs/DESIGN_SYSTEM.md §5)" % rel)
    need(not re.search(r"(padding|margin|border|inset)-(inline|block)", body),
         "%s uses CSS logical properties — use physical RTL properties" % rel)

    for rule in re.findall(r"\{([^{}]*)\}", body):
        for pattern, twin in FLEX_TWINS:
            if re.search(pattern, rule) and twin not in rule:
                fails.append("%s: a rule uses '%s' without its '%s' twin — "
                             "Trident needs the -ms- prefix in the same rule: "
                             "%s" % (rel, pattern, twin,
                                     " ".join(rule.split())[:120]))
                break
        if re.search(r"(^|[;\s])flex\s*:", rule) and "-ms-flex:" not in rule:
            fails.append("%s: a rule uses 'flex:' without '-ms-flex:' — %s"
                         % (rel, " ".join(rule.split())[:120]))

    # Colour literals are only meaningful inside a declaration VALUE. Scanning
    # the whole file would trip over id selectors such as `#cashPanel`.
    for value in declaration_values(body):
        for hexlit in re.findall(r"#([0-9A-Za-z]+)", value):
            if len(hexlit) not in (3, 4, 6, 8) or \
                    re.search(r"[^0-9A-Fa-f]", hexlit):
                fails.append("%s: '#%s' is not a valid hex colour" % (rel, hexlit))

# ===========================================================================
# 6. PALETTE — the rejected colour families may not come back
# ===========================================================================
# Anything listed in docs/DESIGN_SYSTEM.md is approved by definition: that
# document IS the palette registry. This check therefore only fires on a colour
# that is BOTH unregistered AND in one of the families the product owner
# explicitly rejected (violet/purple/magenta/pink and cream/beige/clay). That
# keeps the guard from second-guessing the approved amber and red hues while
# still stopping the rejected families from creeping back in.
DESIGN_DOC = ROOT / "docs" / "DESIGN_SYSTEM.md"
need(DESIGN_DOC.exists(),
     "docs/DESIGN_SYSTEM.md is missing — it is the palette registry this "
     "check validates against")
approved = set()
if DESIGN_DOC.exists():
    for h in re.findall(r"#([0-9A-Fa-f]{6})\b",
                        DESIGN_DOC.read_text(encoding="utf-8")):
        approved.add(h.upper())


def rgb_of(h):
    if len(h) == 3:
        h = "".join(c * 2 for c in h)
    if len(h) == 8:
        h = h[:6]
    if len(h) != 6:
        return None
    try:
        return int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)
    except ValueError:
        return None


def hsv_of(r, g, b):
    r, g, b = r / 255.0, g / 255.0, b / 255.0
    mx, mn = max(r, g, b), min(r, g, b)
    d = mx - mn
    if d == 0:
        h = 0.0
    elif mx == r:
        h = (60 * ((g - b) / d)) % 360
    elif mx == g:
        h = 60 * ((b - r) / d) + 120
    else:
        h = 60 * ((r - g) / d) + 240
    s = 0.0 if mx == 0 else d / mx
    return h, s, mx


def banned_family(h, s, v):
    """The exact families the product owner rejected."""
    if s < 0.10:
        return None                      # neutral grey — always fine
    if 255 <= h <= 340 and s >= 0.14:
        return "violet/purple/magenta/pink"
    if 340 < h <= 360 or 0 <= h < 12:
        if s >= 0.18 and v >= 0.82:
            return "pink"
    if 20 <= h <= 62 and 0.12 <= s <= 0.55 and v >= 0.78:
        return "cream/beige/clay"
    return None


for path in sorted(CSSDIR.glob("*.css")):
    body = strip_css_comments(path.read_text(encoding="utf-8"))
    seen = {}
    for value in declaration_values(body):
        for hexlit in re.findall(r"#([0-9A-Fa-f]{3,8})\b", value):
            norm = hexlit.upper()
            if len(norm) == 3:
                norm = "".join(c * 2 for c in norm)
            if norm[:6] in approved:
                continue                 # registered in DESIGN_SYSTEM.md
            rgbv = rgb_of(hexlit)
            if not rgbv:
                continue
            fam = banned_family(*hsv_of(*rgbv))
            if fam:
                seen.setdefault(fam, set()).add("#" + hexlit.upper())
    for fam, cols in sorted(seen.items()):
        fails.append("assets/admission/css/%s introduces an unregistered "
                     "colour from the rejected %s family: %s — use an approved "
                     "hue from docs/DESIGN_SYSTEM.md §2"
                     % (path.name, fam, ", ".join(sorted(cols))))

# ===========================================================================
# 7. ES5-ONLY JAVASCRIPT
# ===========================================================================
need(not re.search(r"(^|[^\w.$])(let|const)\s+[A-Za-z_$]", js),
     "admission.js uses let/const — Trident needs ES5 (use var)")
need("=>" not in js,
     "admission.js uses an arrow function — Trident needs ES5")
need("`" not in js,
     "admission.js uses a template literal — Trident needs ES5")
need(not re.search(r"\bclass\s+[A-Z]", js),
     "admission.js uses an ES6 class — Trident needs ES5")

# The bridge + block-list plumbing other screens depend on.
for anchor in ("applySurfaceClass", "setOverlay", "queueBackdrop",
               "blacklist.remove", "cashier.page", "cashier.shift.start",
               "cashier.shift.end", "bill.compute"):
    need(anchor in js,
         "admission.js no longer references '%s' — that path was load-bearing"
         % anchor)

# Retired plumbing must stay retired.
for gone in ("wireDrag", "queueDrag"):
    need(gone not in js, "the retired '%s' plumbing is back in admission.js" % gone)

# ===========================================================================
# Report
# ===========================================================================
if fails:
    print("UI CONTRACT FAILED — %d problem(s):" % len(fails))
    for f in fails:
        print("  - %s" % f)
    print("")
    print("Read AGENTS.md and docs/DESIGN_SYSTEM.md before changing this.")
    print("Do NOT 'fix' a failure by deleting the assertion.")
    sys.exit(1)

for n in notes:
    print("note: %s" % n)
print("UI contract OK — %d surfaces, %d theme layers, %d stylesheets checked."
      % (len(DOM_CONTRACT), len(THEME_LAYERS), len(css_files)))
sys.exit(0)
