#!/usr/bin/env python3
"""
Regression guard for the embedded Patient-Admission surface (assets/admission).

The page is rendered by WebView2 (Chromium) *and* by the MSHTML/Trident (IE11)
fallback that ships with every Windows, so a single unsupported construct makes
the whole screen degrade to raw HTML on customer machines. There is no browser
in the build environment, so this script encodes the contract statically:

  1. admission.css parses: balanced braces, every colour literal is real hex.
  2. Trident-safety: no CSS custom properties, no grid, no flex `gap`, and every
     `display:flex` / `flex-direction` / `align-items` / `justify-content` /
     `flex:` declaration is paired with its `-ms-` equivalent in the same rule.
  3. The v1.64.0 (درمان پلاس) requirements are actually present:
        - the صندوق نرفته‌ها / صف پذیرش panel is a FULL-SCREEN overlay
          (position:fixed, inset-0, flex column) sized to the user's monitor,
        - a dim backdrop layer exists beneath it,
        - the services card is bottom-anchored and centred,
        - تاریخ پذیرش + شیفت are a flexible side-by-side pair,
        - the in-page print cluster is bottom-anchored in the right rail,
        - ثبت قبض is blue, never green.
  4. index.html structure: profile card first in the right rail, no zoom
     controls, no «اطلاعات تکمیلی بیمه» card, صورت حساب above مبلغ نهایی,
     the three print buttons in-page, the full-screen queue overlay + backdrop,
     the removed controls (عدم پرداخت / پذیرش جدید / انصراف) are gone, and the
     two navigation buttons (صندوق نرفته‌ها / صف پذیرش) are present.
  5. admission.js: ES5 only (no let/const/arrow/template literals), the queue
     overlay opens through the helper, and no retired drag plumbing remains.
"""
import re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CSS  = ROOT / "assets" / "admission" / "admission.css"
HTML = ROOT / "assets" / "admission" / "index.html"
JS   = ROOT / "assets" / "admission" / "admission.js"

fails = []
def need(cond, msg):
    if not cond:
        fails.append(msg)

def strip_comments(text):
    return re.sub(r"/\*.*?\*/", "", text, flags=re.S)

# --------------------------------------------------------------- 1. CSS parse
css_raw = CSS.read_text(encoding="utf-8")
css = strip_comments(css_raw)

need(css.count("{") == css.count("}"),
     "admission.css brace mismatch: %d '{' vs %d '}'" % (css.count("{"), css.count("}")))

# Parse the sheet into (selector, body) pairs so selectors (#id) are never
# mistaken for colour literals, and so cascade order can be reasoned about.
RULES = []                       # [(selector, raw_body)]
for m in re.finditer(r"([^{}]+)\{([^{}]*)\}", css):
    sel = " ".join(m.group(1).split())
    if sel.startswith("@"):      # at-rule preludes carry no declarations
        continue
    RULES.append((sel, m.group(2)))

# colour literals only ever follow ':' or ',' or '(' INSIDE a declaration body.
bad_hex = []
for sel, body in RULES:
    for m in re.finditer(r"[:,(]\s*#([0-9a-zA-Z]+)", body):
        v = m.group(1)
        if len(v) not in (3, 4, 6, 8) or not re.fullmatch(r"[0-9a-fA-F]+", v):
            bad_hex.append((sel, "#" + v))
need(not bad_hex, "admission.css malformed colour literals: %r" % (bad_hex,))

# ------------------------------------------------------- 2. Trident-safety
need("var(--" not in css, "admission.css uses CSS custom properties (Trident cannot read them)")
need(not re.search(r"display\s*:\s*grid", css), "admission.css uses display:grid")
need(not re.search(r"(^|[;{\s])gap\s*:", css), "admission.css uses flex/grid `gap`")
need("padding-inline" not in css and "margin-inline" not in css,
     "admission.css uses logical properties")

# every rule that uses a modern flex prop must also carry the -ms- form
PAIRS = [(r"display\s*:\s*flex",              "display:-ms-flexbox"),
         (r"flex-direction\s*:",              "-ms-flex-direction:"),
         (r"align-items\s*:",                 "-ms-flex-align:"),
         (r"justify-content\s*:",             "-ms-flex-pack:"),
         (r"flex-wrap\s*:",                   "-ms-flex-wrap:"),
         (r"(^|[;\s])flex\s*:",               "-ms-flex:")]
missing_ms = 0
for _sel, body in RULES:
    compact = body.replace(" ", "")
    for pat, ms in PAIRS:
        if re.search(pat.replace(r"\s*", ""), compact) and ms.replace(" ", "") not in compact:
            missing_ms += 1
need(missing_ms == 0,
     "admission.css has %d flex declaration(s) without their -ms- fallback" % missing_ms)

# ------------------------------------------- 3. v1.64.0 requirements in CSS
def rule_of(selector, exclude=("theme-dark", "theme-calm", "theme-warm")):
    """Merge every declaration block whose selector list contains `selector` as a
    whole comma-separated selector, in source order (so the later, winning
    declarations appear last). Theme variants are excluded by default so a dark
    override cannot be mistaken for the base rule."""
    want = " ".join(selector.split())
    out = []
    for sel, body in RULES:
        parts = [" ".join(p.split()) for p in sel.split(",")]
        if want not in parts:
            continue
        if any(x in sel for x in exclude):
            continue
        out.append(body)
    return "".join(out).replace(" ", "").replace("\n", "")

# full-screen queue overlay (replaces the v1.62.0 draggable mini-page)
ov = rule_of(".queue-overlay")
need("position:fixed" in ov,      "queue overlay is not position:fixed")
need("top:0" in ov and "left:0" in ov and "right:0" in ov and "bottom:0" in ov,
     "queue overlay is not full-screen (inset-0)")
need("-ms-flex-direction:column" in ov,
      "queue overlay is not a flex column")
zso = re.findall(r"z-index:(\d+)", ov)
zom = zso[-1] if zso else None
need(zom and int(zom) >= 1600,
     "queue overlay z-index too low — it can slide under another layer")
need(".queue-backdrop" in css, "no dim backdrop beneath the queue overlay")
bd = rule_of(".queue-backdrop")
need("position:fixed" in bd, "queue backdrop is not position:fixed")
zbs = re.findall(r"z-index:(\d+)", bd)
zb = zbs[-1] if zbs else None
need(zb and zom and int(zb) < int(zom),
     "queue backdrop must sit BELOW the overlay")

svc = rule_of(".col-center > .svc-card")
need("margin-top:auto" in svc, "services card is not bottom-anchored in the workspace")
need("align-self:center" in svc, "services card is not centred")
need("max-width:" in svc, "services card has no max-width, so it cannot read as centred")

dt = rule_of(".datetime-card .dt-half")
need(re.search(r"-ms-flex:11\d+px", dt), "تاریخ/شیفت halves are not flexibly sized")
need("min-width:" in dt, "تاریخ/شیفت halves have no min-width guard")
need("-ms-flex-wrap:wrap" in rule_of(".datetime-card"),
     "the تاریخ/شیفت card cannot wrap, so it squashes instead of responding")

pc = rule_of(".print-card")
need("margin-top:auto" in pc, "the in-page print cluster is not bottom-anchored")
need("order:" in pc.replace("-ms-flex-order:", "order:"),
     "the in-page print cluster has no explicit rail order")

sub = rule_of(".btn-submit")
need("#2f6fe4" in sub or "#3d81f5" in sub or "#1e57c4" in sub,
     "ثبت قبض is not blue")
for green in ("#16a34a", "#22c55e", "#16c47f", "#0f7a4e"):
    need(green not in sub, "ثبت قبض still carries a green tone (%s)" % green)

# ------------------------------------------------------ 4. index.html shape
html = HTML.read_text(encoding="utf-8")
right = html.split('id="colRight"', 1)
need(len(right) == 2, "index.html has no right action rail (#colRight)")
if len(right) == 2:
    rail = right[1].split("</aside>", 1)[0]
    need(rail.index("profile-card") < rail.index("action-card"),
         "the profile card is not FIRST in the right rail")
need("btnZoomIn" not in html and "btnZoomOut" not in html and "view-tools" not in html,
     "zoom in/out controls are still present")
need("insExtraHidden" in html and "اطلاعات تکمیلی بیمه" not in
     re.sub(r"<!--.*?-->", "", html, flags=re.S),
     "the «اطلاعات تکمیلی بیمه» card is still rendered")
need(html.index("invoiceCard") < html.index("payableCard"),
     "صورت حساب must sit ABOVE مبلغ نهایی")
need("محاسبه قطعی" not in html, "the removed «محاسبه قطعی…» caption is back")
for bid in ("btnPrtLast", "btnPrtRx", "btnPrtIns"):
    need(bid in html, "the in-page print button %s is missing" % bid)
need('id="queueBackdrop"' in html, "index.html has no full-screen queue backdrop element")
need('id="queuePanel"' in html and "queue-overlay" in html,
     "index.html has no full-screen queue overlay")
# v1.64.0 removed controls
need('id="noPay"' not in html, "the removed «عدم پرداخت فعلی» checkbox is still present")
need('id="btnNew"' not in html, "the removed «پذیرش جدید» button is still present")
need('id="btnCancel"' not in html, "the removed «انصراف» button is still present")
# v1.79.0: the separate nav buttons were consolidated into the tools page
# launcher — verify the consolidated queue overlay + its tabs are present.
need('id="tabQueue"' in html, "the «صندوق نرفته‌ها» queue tab is missing")
need('id="tabAdmQ"' in html, "the «صف پذیرش» queue tab is missing")
# v1.64.0 unblock button on the block modal
need('id="blockUnblock"' in html, "the «رفع مسدودی» button is missing on the block modal")
need('class="btn btn-submit' in html or "btn-submit" in html, "ثبت قبض lost its btn-submit class")
need("btn-success" not in html, "a green btn-success survives on the page")

# ----------------------------------------------------------- 5. admission.js
js = JS.read_text(encoding="utf-8")
js_code = re.sub(r"/\*.*?\*/", "", js, flags=re.S)
js_code = re.sub(r"^\s*//.*$", "", js_code, flags=re.M)
need(not re.search(r"\b(let|const)\s+\w+\s*=", js_code), "admission.js uses let/const (not ES5)")
need("=>" not in js_code, "admission.js uses arrow functions (not ES5)")
need("`" not in js_code, "admission.js uses template literals (not ES5)")
need("queuePanel" in js_code and "queueBackdrop" in js_code,
     "the queue overlay no longer opens through its helper")
need("queueBackdrop" in js_code, "admission.js does not drive the full-screen queue backdrop")
need("wireDrag" not in js_code and "queueDrag" not in js_code,
     "retired draggable queue plumbing is still present")
need("blacklist.remove" in js_code, "admission.js does not call blacklist.remove (رفع مسدودی)")

# ------------------------------------------------------------------- report
if fails:
    print("FAIL — %d problem(s):" % len(fails))
    for f in fails:
        print("  -", f)
    sys.exit(1)

print("PASS: admission.css parses (%d rules), all colour literals valid" % len(RULES))
print("PASS: Trident-safe — no custom properties / grid / gap, every flex has its -ms- pair")
print("PASS: صندوق نرفته‌ها queue overlay is full-screen, flex-column, z-%s over a backdrop"
      % zom)
print("PASS: services card bottom-anchored and centred; تاریخ+شیفت flexible side-by-side")
print("PASS: in-page print cluster bottom-anchored in the right rail; ثبت قبض is blue, not green")
print("PASS: index.html — profile first, no zoom, no insurance-extra card, صورت حساب above مبلغ نهایی")
print("PASS: index.html — full-screen queue overlay, removed controls gone, nav + رفع مسدودی buttons present")
print("PASS: admission.js is ES5 and drives the full-screen overlay + blacklist.remove")
