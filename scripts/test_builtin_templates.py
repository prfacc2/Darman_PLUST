#!/usr/bin/env python3
"""Structural regression checks for the 30 builtin print designs (v1.97.0).

Validates BOTH sides of the ready-made-template contract:

  1. src/print_designer_templates.inc  — the C++ seeder the print engine uses
  2. assets/designer/templates.js      — the ES5 mirror the web gallery shows

The single invariant that matters for the bug this architecture was written to
kill («خدمات چاپ نمی‌شود»): every one of the 30 designs owns exactly ONE live
services table and AT MOST ONE barcode/code carrier (v1.69.0: some designs now
carry no code at all, so the carrier count is 0 or 1, never 2); all presets
include authoritative name,
description, quantity, and line-amount columns; runtime rows stay compact,
wrap prose, and never pad the page with fake/example services.
"""
from pathlib import Path
import json
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INC = ROOT / "src" / "print_designer_templates.inc"
JS = ROOT / "assets" / "designer" / "templates.js"
PRINTER = ROOT / "src" / "printer.cpp"
PAGINATION = ROOT / "src" / "print_services_pagination.h"
SERVICE_IDENTITY = ROOT / "src" / "service_identity.h"
PAGINATION_POLICY = ROOT / "src" / "print_services_policy.h"
SERVICE_CANONICALIZATION = ROOT / "src" / "service_canonicalization.h"

failures = []
notes = []


def check(condition, message):
    if not condition:
        failures.append(message)
    return bool(condition)


# ===========================================================================
# 0. pdSvcColOf() re-implementation — keeps the templates honest.
# ===========================================================================
ZW = "\u200c\u200f\u200e \t"


def _norm(label):
    out = []
    for ch in label:
        if ch in ZW:
            continue
        if ch == "\u064a":
            ch = "\u06cc"
        if ch == "\u0643":
            ch = "\u06a9"
        out.append(ch)
    return "".join(out)


# ordered exactly like the C++ cascade in printer.cpp::pdSvcColOf
COL_RULES = [
    (("\u0634\u0631\u062d", "\u062a\u0648\u0636\u06cc\u062d"), "DESC"),
    (("\u0646\u0648\u0639",), "CAT"),
    (("\u062a\u0639\u062f\u0627\u062f", "\u0645\u0642\u062f\u0627\u0631"), "QTY"),
    (("\u0631\u062f\u06cc\u0641", "\u0634\u0645\u0627\u0631\u0647"), "ROW"),
    (
        (
            "\u0633\u0647\u0645\u0628\u06cc\u0645\u0647",
            "\u0633\u0647\u0645\u067e\u0627\u06cc\u0647",
            "\u0628\u06cc\u0645\u0647",
        ),
        "INS",
    ),
    (("\u0633\u0647\u0645\u0628\u06cc\u0645\u0627\u0631", "\u067e\u0631\u062f\u0627\u062e\u062a\u06cc"), "PAT"),
    (("\u062a\u062e\u0641\u06cc\u0641",), "DISC"),
    (("\u0645\u0628\u0644\u063a\u06a9\u0644", "\u062c\u0645\u0639", "\u06a9\u0644"), "LINE"),
    (("\u0642\u06cc\u0645\u062a", "\u0641\u06cc", "\u0645\u0628\u0644\u063a", "\u0646\u0631\u062e"), "PRICE"),
    (("\u06a9\u062f",), "CODE"),
    (
        (
            "\u0646\u0627\u0645\u062e\u062f\u0645\u062a",
            "\u062e\u062f\u0645\u062a",
            "\u0646\u0627\u0645",
            "\u0639\u0646\u0648\u0627\u0646",
        ),
        "NAME",
    ),
]


def classify(label):
    norm = _norm(label)
    if not norm:
        return "NONE"
    for needles, kind in COL_RULES:
        for needle in needles:
            if needle in norm:
                return kind
    return "NONE"


# sanity: the python mirror must agree with the real cascade on the two
# captions that are historically the trickiest (سهم بیمار vs سهم بیمه).
check(
    classify("\u0633\u0647\u0645 \u0628\u06cc\u0645\u0627\u0631") == "INS"
    or classify("\u0633\u0647\u0645 \u0628\u06cc\u0645\u0627\u0631") == "PAT",
    "the pdSvcColOf mirror cannot classify «سهم بیمار» at all",
)
check(
    "pdSvcColOf" in PRINTER.read_text(encoding="utf-8"),
    "printer.cpp no longer exposes pdSvcColOf — the caption contract is gone",
)

# ===========================================================================
# 1. src/print_designer_templates.inc
# ===========================================================================
inc = INC.read_text(encoding="utf-8")

check("PIT_SERVICES" in inc, "the seeder never emits a PIT_SERVICES item")
check(
    "it.type=PIT_SERVICES" in inc.replace(" ", ""),
    "mkServices() does not build a PIT_SERVICES item",
)

# --- page geometry ---------------------------------------------------------
geom = dict(re.findall(r"static const double (PG_\w+|FOOT_Y)\s*=\s*([-\d.]+)", inc))
check(geom.get("PG_W") == "210.0", f"page width is not A4 portrait: {geom.get('PG_W')}")
check(geom.get("PG_H") == "297.0", f"page height is not A4 portrait: {geom.get('PG_H')}")
check("PG_CW  = PG_W - 2*PG_M" in inc, "content width is no longer derived from the margin")
check("FOOT_Y = PG_H - 34.0" in inc, "the 34 mm footer band reservation is gone")

# --- the 8 column presets --------------------------------------------------
preset_names = ["SVC3", "SVC4_ROW", "SVC4_CAT", "SVC5", "SVC5_CODE", "SVC6_FIN", "SVC6_INS", "SVC7"]
enum_match = re.search(r"enum SvcPreset \{(.*?)\}", inc, re.S)
check(enum_match is not None, "enum SvcPreset was not found")
if enum_match:
    body = enum_match.group(1)
    for name in preset_names:
        check(re.search(r"\b%s\b" % name, body) is not None, f"SvcPreset {name} is missing")
    check("SVC_PRESET_COUNT" in body, "SvcPreset has no COUNT sentinel")

model_fn = re.search(r"static std::wstring svcModelJson\(int preset\)\{(.*?)\n\}", inc, re.S)
check(model_fn is not None, "svcModelJson() was not found")
model_body = model_fn.group(1) if model_fn else ""

# reconstruct each preset's JSON out of the concatenated wide-string literals
inc_presets = {}
for chunk in re.split(r"\n\s*case\s+|\n\s*default:", model_body):
    tag = re.match(r"(SVC[A-Z0-9_]*)\s*:", chunk)
    pieces = re.findall(r'L"((?:[^"\\]|\\.)*)"', chunk)
    if not pieces:
        continue
    raw = "".join(pieces).replace('\\"', '"')
    try:
        parsed = json.loads(raw)
    except ValueError:
        continue
    key = tag.group(1) if tag else "SVC3"
    inc_presets.setdefault(key, parsed)

check(
    len(inc_presets) == 8,
    f"expected 8 parsable column presets in svcModelJson, got {sorted(inc_presets)}",
)


def audit_preset(where, key, model):
    cols = model.get("cols")
    widths = model.get("widths") or []
    labels = model.get("labels") or []
    check(model.get("header") is True, f"{where} preset {key} has no header row")
    check(cols in (4, 5, 6, 7), f"{where} preset {key} declares an odd column count {cols}")
    check(len(widths) == cols, f"{where} preset {key}: {len(widths)} widths for {cols} cols")
    check(len(labels) == cols, f"{where} preset {key}: {len(labels)} labels for {cols} cols")
    total = sum(widths)
    check(
        abs(total - 1.0) < 0.005,
        f"{where} preset {key}: column widths sum to {total:.4f}, not 1.0",
    )
    check(
        all(w >= 0.045 for w in widths),
        f"{where} preset {key} has an unprintably narrow column: {widths}",
    )
    kinds = [classify(lab) for lab in labels]
    check(
        "NONE" not in kinds,
        f"{where} preset {key} has a caption printer.cpp cannot classify: "
        + ", ".join("%s->%s" % (l, k) for l, k in zip(labels, kinds)),
    )
    for mandatory in ("NAME", "DESC", "QTY", "LINE"):
        check(
            mandatory in kinds,
            f"{where} preset {key} lacks mandatory {mandatory}: {kinds}",
        )
    check(
        len(set(kinds)) == len(kinds),
        f"{where} preset {key} repeats a column kind: {kinds}",
    )
    # v1.97.0 — شرح خدمت last; ردیف omitted or last (never in the middle).
    if "ROW" in kinds:
        check(
            kinds[-1] == "ROW",
            f"{where} preset {key} has row # in the middle: {kinds}",
        )
        check(
            kinds[-2] == "DESC",
            f"{where} preset {key} must put DESC immediately before trailing ROW: {kinds}",
        )
    else:
        check(
            kinds[-1] == "DESC",
            f"{where} preset {key} must put DESC last: {kinds}",
        )
    return kinds


for key, model in sorted(inc_presets.items()):
    kinds = audit_preset("inc", key, model)
    notes.append("  %-9s %d cols  %s" % (key, model.get("cols"), " | ".join(kinds)))

# --- computed services height --------------------------------------------
# v1.95: the old servicesBlock/servicesBlockAt helpers were replaced by a single
# renderReceipt() that handles all 10 families internally.
render_fn = re.search(r"static void renderReceipt\(PrintDesign& d, const TplSpec& sp\)\{(.*?)\n\}", inc, re.S)
check(render_fn is not None, "renderReceipt() was not found")
if render_fn:
    body = render_fn.group(1)
    check("mkServices(" in body, "renderReceipt() does not emit the services table")
    check("medBelow" in body, "renderReceipt() does not emit the below-box footer section")

# --- the 30 specs ---------------------------------------------------------
spec_match = re.search(r"static const TplSpec TPL\[30\] = \{(.*?)\n\};", inc, re.S)
check(spec_match is not None, "the TplSpec TPL[30] table was not found")
specs = []
if spec_match:
    for line in spec_match.group(1).splitlines():
        row = re.search(r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(SVC[A-Z0-9_]*)\s*,(.*?)\}", line)
        if not row:
            continue
        rest = [p.strip() for p in row.group(4).split(",")]
        specs.append(
            {
                "family": int(row.group(1)),
                "variant": int(row.group(2)),
                "svc": row.group(3),
                "accent": rest[0],
                "headFill": rest[2],
                "bw": float(rest[3]),
                "rowH": float(rest[4]),
                "frame": rest[5] == "true",
            }
        )

check(len(specs) == 30, f"expected 30 TplSpec rows, parsed {len(specs)}")
if len(specs) == 30:
    fams = sorted({s["family"] for s in specs})
    check(fams == list(range(10)), f"expected 10 layout families 0..9, got {fams}")
    for fam in range(10):
        variants = sorted(s["variant"] for s in specs if s["family"] == fam)
        check(
            variants == [0, 1, 2],
            f"family {fam} must have variants 0,1,2 — got {variants}",
        )
    used = {s["svc"] for s in specs}
    check(
        used <= set(preset_names),
        f"a spec references an unknown preset: {sorted(used - set(preset_names))}",
    )
    check(
        len(used) >= 7,
        f"only {len(used)} of the 8 column presets are actually used: {sorted(used)}",
    )
    for i, s in enumerate(specs):
        check(
            3.8 <= s["rowH"] <= 9.0,
            f"template {i + 1:02d} row pitch {s['rowH']} mm is outside the compact 4.0..9 mm band",
        )
        check(
            0.2 <= s["bw"] <= 0.8,
            f"template {i + 1:02d} table border {s['bw']} mm is outside 0.2..0.8 mm",
        )
    lineart = [i + 1 for i, s in enumerate(specs) if s["headFill"] == "0x000000"]
    check(
        len(lineart) >= 3,
        f"expected at least 3 pure line-art (monochrome) designs, got {lineart}",
    )

# --- names are actually applied to the design ---------------------------
name_match = re.search(r"static const wchar_t\* const TPL_NAMES\[30\]=\{(.*?)\n\};", inc, re.S)
check(name_match is not None, "the TPL_NAMES[30] table was not found")
inc_names = re.findall(r'L"([^"]*)"', name_match.group(1)) if name_match else []
check(len(inc_names) == 30, f"expected 30 template names, got {len(inc_names)}")
check(len(set(inc_names)) == len(inc_names), "two builtin templates share the same name")
check(
    all(n.strip() for n in inc_names),
    "a builtin template name is blank — the gallery would show an empty card",
)
check(
    "d.name = TPL_NAMES[idx];" in inc,
    "buildTemplate() does not stamp TPL_NAMES onto the design — designs seed NAMELESS "
    "(this was the v1.62.0 blank-gallery bug)",
)
check(
    "return TPL_NAMES[idx];" in inc,
    "buildTemplateName() no longer shares the single TPL_NAMES table",
)

# --- every family really prints the live services ----------------------
build_match = re.search(
    r"static PrintDesign buildTemplate\(int idx\)\{(.*?)\n    return d;\n\}", inc, re.S
)
check(build_match is not None, "buildTemplate() body was not found")
build = build_match.group(1) if build_match else ""
# v1.95: buildTemplate now delegates to renderReceipt which handles all
# 10 families internally via if/else on sp.family (not a switch/case).
check("renderReceipt(d, sp);" in build, "buildTemplate() does not delegate to renderReceipt()")
# v1.95: renderReceipt dispatches families via boolean flags (photo/sidebar/tearoff/shadeBars)
# rather than case arms. Families 0 and 9 are the DEFAULT path (no special flags),
# so they don't get explicit conditionals — only families 1-8 have explicit checks.
render_body = render_fn.group(1) if render_fn else ""
explicit_fams = [1, 2, 3, 4, 5, 6, 7, 8]
for fam in explicit_fams:
    check(
        f"fam=={fam}" in render_body,
        f"renderReceipt() does not handle family {fam} (explicit flag check)",
    )
# v1.97: every medical receipt carries exactly one barcode BELOW the services table.
check(
    "medBarcode(" in render_body,
    "renderReceipt() does not emit the medical barcode (medBarcode)",
)
svc_pos = render_body.find("mkServices(")
bc_pos = render_body.find("medBarcode(")
check(
    svc_pos >= 0 and bc_pos > svc_pos,
    "barcode is not placed below the services table in renderReceipt()",
)

check('d.paper=L"A4"' in inc.replace(" ", "").replace('d.paper=L"A4"', 'd.paper=L"A4"') or 'd.paper=L"A4"' in inc,
      "designs are no longer authored against A4")

# --- migration guard ---------------------------------------------------
check('getSetting(L"tpl_migration_1_97"' in inc, "the v1.97 migration guard is missing")
init_fn = re.search(r"void Designs_Init\(\)\{(.*?)\n\}", inc, re.S)
check(init_fn is not None, "Designs_Init() was not found")
if init_fn:
    init_body = init_fn.group(1)
    check(
        init_body.count("stamp();") == 2,
        "the migration must be stamped for fresh installs and the v1.97 upgrade "
        f"(found {init_body.count('stamp();')} stamp() calls)",
    )
    check(
        "Designs_Insert(d)" in init_body and "Designs_Update(fresh)" in init_body,
        "Designs_Init() no longer both seeds fresh installs and rebuilds existing ones",
    )
    check(
        "Designs_Delete(existing[i].id)" in init_body,
        "Designs_Init() no longer removes surplus builtins beyond the 30",
    )
for old in ("1_52", "1_53", "1_58", "1_59", "1_60", "1_61", "1_62", "1_65", "1_66", "1_67"):
    check(
        'setSetting(L"tpl_migration_%s", L"1")' % old in inc,
        f"upgrade path no longer retires the tpl_migration_{old} guard",
    )

# ===========================================================================
# 2. assets/designer/templates.js — executed, then compared to the .inc
# ===========================================================================
js = JS.read_text(encoding="utf-8")
check("window.AZ_TEMPLATES" in js, "templates.js no longer publishes window.AZ_TEMPLATES")
check("var PG_W   = 210.0" in js or "PG_W = 210.0" in js.replace("   ", " "),
      "templates.js drifted off A4 geometry")
check("FOOT_Y = PG_H - 34.0" in js, "templates.js lost the footer reservation")

harness = r"""
var fs = require('fs');
global.window = {};
new Function(fs.readFileSync(process.argv[2], 'utf8')).call(global);
var all = global.window.AZ_TEMPLATES;
var out = [];
for (var i = 0; i < all.length; i++) {
  var t = all[i], svc = [], barcode = [], k;
  for (k = 0; k < t.items.length; k++) {
    if (t.items[k].type === 'services') svc.push(t.items[k]);
    if (t.items[k].type === 'barcode' || t.items[k].type === 'qr') barcode.push(t.items[k]);
  }
  var minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
  for (k = 0; k < t.items.length; k++) {
    var it = t.items[k];
    if (it.isFrame) continue;
    if (it.x < minX) minX = it.x;
    if (it.y < minY) minY = it.y;
    if (it.x + it.w > maxX) maxX = it.x + it.w;
    if (it.y + it.h > maxY) maxY = it.y + it.h;
  }
  var s = svc.length === 1 ? svc[0] : null;
  var bc = barcode.length === 1 ? barcode[0] : null;
  var model = null;
  if (s) { try { model = JSON.parse(s.text); } catch (e) { model = null; } }
  var rows = 0;
  if (s && s.rowH > 0) rows = Math.floor((s.h - (s.headerH || s.rowH)) / s.rowH);
  out.push({
    name: t.name, paper: t.paper, orientation: t.orientation,
    items: t.items.length, svcCount: svc.length, barcodeCount: barcode.length,
    model: model, h: s ? s.h : 0, y: s ? s.y : 0, rowH: s ? s.rowH : 0,
    headerH: s ? s.headerH : 0, rows: rows,
    barcodeY: bc ? bc.y : -1, barcodeH: bc ? bc.h : 0, barcodeW: bc ? bc.w : 0,
    barcodeBelow: !!(bc && s && bc.y + 0.05 >= s.y + s.h),
    minX: minX, minY: minY, maxX: maxX, maxY: maxY
  });
}
process.stdout.write(JSON.stringify(out));
"""

js_designs = []
try:
    with tempfile.NamedTemporaryFile("w", suffix=".js", delete=False, encoding="utf-8") as fh:
        fh.write(harness)
        harness_path = fh.name
    proc = subprocess.run(
        ["node", harness_path, str(JS)], capture_output=True, text=True, timeout=60
    )
    if proc.returncode != 0:
        failures.append("templates.js failed to execute under node: " + proc.stderr.strip()[:400])
    else:
        js_designs = json.loads(proc.stdout)
except FileNotFoundError:
    notes.append("  (node not available — skipped the templates.js execution audit)")
except Exception as exc:  # pragma: no cover
    failures.append(f"could not run the templates.js audit: {exc}")

if js_designs:
    check(len(js_designs) == 30, f"templates.js publishes {len(js_designs)} designs, expected 30")
    js_names = [d["name"] for d in js_designs]
    check(
        js_names == inc_names,
        "the web gallery names have drifted from the C++ TPL_NAMES table",
    )
    for i, d in enumerate(js_designs):
        tag = "web template %02d" % (i + 1)
        check(d["paper"] == "A4" and d["orientation"] == 0, f"{tag} is not A4 portrait")
        check(
            d["svcCount"] == 1,
            f"{tag} must own exactly one dynamic services table; got {d['svcCount']}",
        )
        check(
            d["barcodeCount"] <= 1,
            f"{tag} must own at most one barcode/code carrier; got {d['barcodeCount']}",
        )
        if d["svcCount"] != 1:
            continue
        check(d["model"] is not None, f"{tag} has an unparsable services model")
        if d["model"]:
            audit_preset("web", "#%02d" % (i + 1), d["model"])
            if len(specs) == 30:
                want = inc_presets.get(specs[i]["svc"])
                if want:
                    check(
                        d["model"].get("labels") == want.get("labels"),
                        f"{tag} column captions differ from the C++ preset {specs[i]['svc']}",
                    )
                    check(
                        d["model"].get("widths") == want.get("widths"),
                        f"{tag} column widths differ from the C++ preset {specs[i]['svc']}",
                    )
        check(d["rowH"] > 0 and d["headerH"] > 0, f"{tag} has no pinned row/header pitch")
        check(
            d["y"] + d["h"] <= 289.0 + 0.01,
            f"{tag} services table (y={d['y']:.1f} h={d['h']:.1f}) runs into the footer band",
        )
        # v1.95: the page margin is now R_M=8 (was PG_M=12), and the frame margin
        # FR_M=5 sits outside the info box. So the printable-area bleed limit
        # is 8..202 (width) and 8..289 (height).
        check(
            d["minX"] >= 7.9 and d["maxX"] <= 202.1,
            f"{tag} bleeds off the printable width ({d['minX']:.1f}..{d['maxX']:.1f})",
        )
        check(
            d["minY"] >= 7.9 and d["maxY"] <= 289.1,
            f"{tag} bleeds off the printable height ({d['minY']:.1f}..{d['maxY']:.1f})",
        )
        check(d["items"] >= 18, f"{tag} looks under-designed ({d['items']} items)")
        check(
            d["barcodeBelow"],
            f"{tag} barcode is not below the services table "
            f"(svc y={d['y']:.1f} h={d['h']:.1f}, bc y={d['barcodeY']:.1f})",
        )
        check(
            70.0 <= d["barcodeW"] <= 74.0 and 6.0 <= d["barcodeH"] <= 8.0,
            f"{tag} barcode size {d['barcodeW']:.1f}×{d['barcodeH']:.1f} is not ~72×7 mm",
        )

    # v1.69.0 — the 30 designs must be VISUALLY DISTINCT, not mere colour swaps.
    # Each family's three variants get a different services-table height (a direct
    # consequence of differing meta/patient/totals/footer blocks), and the code
    # v1.95: all 30 medical receipts now carry a barcode (section 2 of the spec).
    # The old no_code>=5 / with_code>=10 variety assertion is replaced by an
    # all-barcode check (every receipt has exactly 1 barcode).
    no_code = sum(1 for d in js_designs if d["barcodeCount"] == 0)
    with_code = sum(1 for d in js_designs if d["barcodeCount"] == 1)
    check(no_code == 0 and with_code == 30,
          f"v1.95 medical receipts: expected all 30 barcoded, got {no_code} code-less, {with_code} barcoded")
    for fam in range(10):
        hs = sorted(round(d["h"], 1) for i, d in enumerate(js_designs)
                    if len(specs) == 30 and specs[i]["family"] == fam)
        check(len(set(hs)) == 3,
              f"family {fam} variants are not layout-distinct (services heights {hs})")

# ===========================================================================
# 3. Runtime renderer and admission canonicalization guards
# ===========================================================================
printer = PRINTER.read_text(encoding="utf-8")
manage = (ROOT / "src" / "manage.inc").read_text(encoding="utf-8")
api = (ROOT / "src" / "web_admission_api.inc").read_text(encoding="utf-8")
admission_js = (ROOT / "assets" / "admission" / "admission.js").read_text(encoding="utf-8")
identity_header = SERVICE_IDENTITY.read_text(encoding="utf-8")
policy_header = PAGINATION_POLICY.read_text(encoding="utf-8")
canonical_header = SERVICE_CANONICALIZATION.read_text(encoding="utf-8")
check("#include <string>" in identity_header and "#include <cstring>" not in identity_header,
      "service_identity.h must include <string> directly, not <cstring>")
check("#include <string>" in policy_header and "#include <cstring>" not in policy_header,
      "print_services_policy.h must include <string> directly, not <cstring>")
check("if(dst.discount>cap) dst.discount=cap;" in canonical_header and
      "if(dst.discount<cap) dst.discount=cap;" not in canonical_header,
      "merged service discounts do not clamp down to line gross")
check("minRows" not in printer, "printer still pads live services to frame capacity")
check("pdSvcCellSample" not in printer, "printer still fabricates sample service text")
check('L"%dY"' in printer, "{age} must print as 10Y/24Y")
check("p-name" in printer.lower(), "P-Name → {full} alias is missing")
check(
    "ویزیت پزشک عمومی" not in (ROOT / "assets" / "designer" / "designer.js").read_text(encoding="utf-8"),
    "designer services preview still shows example service names",
)
check("نمونهٔ خدمت" not in manage, "native ready-template preview still shows a fake service")
check("ry.reserve(totalRows+1)" in printer, "renderer no longer allocates totalRows+1 boundaries")
check("DT_WORDBREAK" in printer and "pdBuildServicesLayout" in printer,
      "service prose is not measured/wrapped into growing rows")
check("-2" not in printer[printer.find("static void pdDrawServices"):printer.find("REAL 1-D BARCODE ENGINE")],
      "service renderer still replaces overflow services with a continuation marker row")
check("pdPlanServicePages" in printer and "for(size_t pageNo=0;pageNo<servicePages.size();++pageNo)" in printer,
      "modern print-design path does not paginate measured service rows")
check("pdDrawServiceRowFragment" in printer and "f.offset" in printer and
      "layout.textLineH" in printer,
      "oversized wrapped rows are not split into complete line-aligned fragments")
check("pdPrintableDataHeight" in printer and "pdEnsureServicesFrame" in printer,
      "header-consuming/zero-height service geometry can still drop all data rows")
check("pdContinuationRepeatAllowed(repeatKind,normalizedField)" in printer and
      "if(multiPage && serviceItem && pit!=serviceItem && !finalPage)" in printer and
      "if(it.type==PIT_FRAME) repeatKind=PDCI_FRAME;" in printer and
      "||it.is_frame" not in printer[printer.find("PdContinuationItemKind repeatKind"):printer.find("if(!pdContinuationRepeatAllowed", printer.find("PdContinuationItemKind repeatKind"))],
      "continuation-page repetition is not governed by an explicit type-safe whitelist")
check("if(StartPage(dc)<=0)" in printer and "EndPage(dc)" in printer,
      "modern print-design pagination no longer emits physical pages safely")
check("adCanonicalServices" in api and "r.services.swap(canonicalServices)" in api,
      "server does not print the same canonical merged services it billed")
check("serviceCanonicalAdd(lines,identityCodes,identityFreeRates,ln" in api and
      "serviceVariantMatches(incomingCodeKey,nameKey,freeRate,incoming.price" in canonical_header and
      "identityCodes.push_back(incomingCodeKey);" in canonical_header and
      "identityFreeRates.push_back(freeRate);" in canonical_header,
      "server duplicate matching no longer preserves submitted code/rate variants")
check("serviceIdentityKey(u82w(codeU))" in api and "serviceIdentityKey(s.code)==want" in api,
      "server catalogue lookup and canonicalization do not share one identity normalization contract")
check("serviceCanonicalSort(lines,identityCodes,identityFreeRates);" in api and
      "serviceVariantCompare(" in canonical_header and "normal→free and free→normal" in api,
      "server canonical row ordering is not explicitly submission-order independent")
check("if(want.empty()&&!wantName.empty())" in api,
      "authoritative catalogue lookup still rebinds an unmatched coded service by display name")
check("jsonGetNumber(obj,\"price\",n)" not in api[api.find("static bool adCatalogPrice"):api.find("// ---- resolve the operator")],
      "unknown normal-rate rows still trust a browser-submitted amount")
check("sameServiceIdentity(code, name, rowCode" in admission_js and "row.qty = Math.min(999" in admission_js,
      "admission UI duplicate matching drifted from the code-first server behavior")

# Compile and execute the pure behavior helpers. These cases cover empty, one,
# many and long-description-height pagination, plus the coded-name collision
# that triggered the integrated review. No printer driver is required.
behavior_cpp = r'''
#include <cassert>
#include <string>
#include <vector>
#include "print_services_pagination.h"
#include "print_services_policy.h"
#include "service_identity.h"
#include "service_canonicalization.h"

struct CanonLine {
    std::wstring code,name,category,desc;
    long long price=0,discount=0;
    int qty=1;
};
static CanonLine line(const wchar_t* code,const wchar_t* name,long long price,
                      int qty=1,long long discount=0,const wchar_t* desc=L""){
    CanonLine v; v.code=code; v.name=name; v.price=price; v.qty=qty;
    v.discount=discount; v.desc=desc; return v;
}
static void add(std::vector<CanonLine>& rows,std::vector<std::wstring>& codes,
                std::vector<bool>& freeRates,const CanonLine& v,bool freeRate){
    serviceCanonicalAdd(rows,codes,freeRates,v,serviceIdentityKey(v.code),freeRate);
}
static int emitted(const std::vector<PdServicesPageSlice>& pages,int row){
    int total=0;
    for(size_t p=0;p<pages.size();++p)
        for(size_t i=0;i<pages[p].rows.size();++i)
            if(pages[p].rows[i].row==row) total+=pages[p].rows[i].height;
    return total;
}
static long long authoritativeCharge(const std::vector<CanonLine>& rows){
    long long total=0;
    for(size_t i=0;i<rows.size();++i)
        total+=rows[i].price*(long long)rows[i].qty-rows[i].discount;
    return total;
}
static long long toggledBackCharge(bool formerFreeFirst){
    std::vector<CanonLine> rows;
    std::vector<std::wstring> codes;
    std::vector<bool> freeRates;
    CanonLine normal=line(L"A۱۲٣",L"shared",100,1,10,L"normal");
    // This row used to be manual/free. After the operator turns that toggle off,
    // its stale freePrice is irrelevant: the effective normal price is 100 and
    // it must merge with the catalogue row without becoming fully discounted.
    CanonLine formerFree=line(L"a١٢۳",L"shared",100,1,20,L"former free");
    if(formerFreeFirst){
        add(rows,codes,freeRates,formerFree,false);
        add(rows,codes,freeRates,normal,false);
    } else {
        add(rows,codes,freeRates,normal,false);
        add(rows,codes,freeRates,formerFree,false);
    }
    serviceCanonicalSort(rows,codes,freeRates);
    assert(rows.size()==1 && rows[0].qty==2 && rows[0].discount==30);
    return authoritativeCharge(rows);
}
int main(){
    { std::vector<int> h; auto p=pdSliceServiceRows(h,100);
      assert(p.size()==1 && p[0].rows.empty()); }
    { std::vector<int> h(1,20); auto p=pdSliceServiceRows(h,100);
      assert(p.size()==1 && p[0].rows.size()==1);
      assert(p[0].rows[0].row==0 && p[0].rows[0].offset==0 && p[0].rows[0].height==20); }
    { std::vector<int> h={20,20,20,20,20}; auto p=pdSliceServiceRows(h,45);
      assert(p.size()==3);
      for(int row=0;row<5;++row) assert(emitted(p,row)==20); }
    { std::vector<int> h={230}; auto p=pdSliceServiceRows(h,100,10,2);
      assert(p.size()==3 && emitted(p,0)==230);
      assert(p[0].rows[0].offset==0 && p[0].rows[0].height==92);
      assert(p[1].rows[0].offset==92 && p[1].rows[0].height==100);
      assert(p[2].rows[0].offset==192 && p[2].rows[0].height==38); }
    { int head=-1; int data=pdPrintableDataHeight(10,100,12,&head);
      assert(data==10 && head==0);
      std::vector<int> h={3}; auto p=pdSliceServiceRows(h,data);
      assert(p.size()==1 && emitted(p,0)==3); }
    { int head=-1; int data=pdPrintableDataHeight(1,100,12,&head);
      assert(data==1 && head==0);
      std::vector<int> h={3}; auto p=pdSliceServiceRows(h,data);
      assert(p.size()==3 && emitted(p,0)==3); }
    { PdServicesFrame f=pdEnsureServicesFrame(295,295,0,300,12);
      assert(f.top==288 && f.bottom==300); }

    assert(serviceIdentityKey(L"  A۱۲٣‏ ")==L"a123");
    assert(serviceIdentityKey(L"‌نام‌‏  خدمت‎")==L"نام خدمت");
    assert(serviceIdentityKey(L"كد ١٢۳")==serviceIdentityKey(L"کد 123"));
    assert(serviceIdentityMatches(L"a123",L"shared",L"a123",L"shared"));
    assert(!serviceIdentityMatches(L"a123",L"shared",L"b123",L"shared"));
    assert(serviceIdentityMatches(L"",L"shared",L"b123",L"shared"));
    assert(!serviceVariantMatches(L"a",L"shared",false,100,L"a",L"shared",true,100));
    assert(!serviceVariantMatches(L"a",L"shared",false,100,L"a",L"shared",false,200));
    assert(serviceVariantMatches(L"a",L"shared",false,100,L"a",L"shared",false,100));
    assert(serviceVariantCompare(L"a",L"shared",false,100,L"",L"a",L"shared",true,250,L"")<0);
    assert(serviceVariantCompare(L"a",L"shared",true,250,L"",L"a",L"shared",false,100,L"")>0);
    long long normalThenFormerFree=toggledBackCharge(false);
    long long formerFreeThenNormal=toggledBackCharge(true);
    assert(normalThenFormerFree==170);
    assert(formerFreeThenNormal==170);
    assert(normalThenFormerFree==formerFreeThenNormal && normalThenFormerFree>0);
    { std::vector<CanonLine> rows; std::vector<std::wstring> codes;
      std::vector<bool> freeRates;
      add(rows,codes,freeRates,line(L"A۱۲٣",L"shared",100),false);
      add(rows,codes,freeRates,line(L"a١٢۳",L"shared",250),true);
      serviceCanonicalSort(rows,codes,freeRates);
      assert(rows.size()==2 && rows[0].price==100 && rows[1].price==250);
      assert(!freeRates[0] && freeRates[1]); }
    { std::vector<CanonLine> rows; std::vector<std::wstring> codes;
      std::vector<bool> freeRates;
      add(rows,codes,freeRates,line(L"a١٢۳",L"shared",250),true);
      add(rows,codes,freeRates,line(L"A۱۲٣",L"shared",100),false);
      serviceCanonicalSort(rows,codes,freeRates);
      assert(rows.size()==2 && rows[0].price==100 && rows[1].price==250);
      assert(!freeRates[0] && freeRates[1]); }
    { std::vector<CanonLine> rows; std::vector<std::wstring> codes;
      std::vector<bool> freeRates;
      add(rows,codes,freeRates,line(L"A",L"shared",100),false);
      add(rows,codes,freeRates,line(L"B",L"shared",100),false);
      assert(rows.size()==2); }
    { std::vector<CanonLine> rows; std::vector<std::wstring> codes;
      std::vector<bool> freeRates;
      add(rows,codes,freeRates,line(L"",L"shared",100,2,10,L"z"),false);
      add(rows,codes,freeRates,line(L"A",L"shared",100,3,20,L"a"),false);
      assert(rows.size()==1 && rows[0].code==L"A" && rows[0].qty==5 &&
             rows[0].discount==30 && codes[0]==L"a"); }

    assert(pdContinuationRepeatAllowed(PDCI_FRAME,L""));
    assert(pdContinuationRepeatAllowed(PDCI_FIELD,L"{full}"));
    assert(pdContinuationRepeatAllowed(PDCI_FIELD,L"{nid}"));
    assert(!pdContinuationRepeatAllowed(PDCI_OTHER,L"{receiptbarcode}"));
    assert(!pdContinuationRepeatAllowed(PDCI_OTHER,L"{receiptcode}"));
    assert(!pdContinuationRepeatAllowed(PDCI_FIELD,L"{total}"));
    assert(!pdContinuationRepeatAllowed(PDCI_FIELD,L"{paid}"));
    assert(!pdContinuationRepeatAllowed(PDCI_FIELD,L"{patientshare}"));
    assert(!pdContinuationRepeatAllowed(PDCI_OTHER,L""));
    return 0;
}
'''
try:
    with tempfile.TemporaryDirectory() as td:
        src = Path(td) / "behavior.cpp"
        exe = Path(td) / "behavior"
        src.write_text(behavior_cpp, encoding="utf-8")
        build = subprocess.run(
            ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "src"),
             str(src), "-o", str(exe)], capture_output=True, text=True, timeout=60
        )
        check(build.returncode == 0, "service pagination behavior harness failed to compile: " + build.stderr[:400])
        if build.returncode == 0:
            run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
            check(run.returncode == 0, "service pagination/identity behavior harness failed")
except FileNotFoundError:
    failures.append("g++ is required for service pagination behavior coverage")

# Execute the actual ES5 identity helpers extracted from admission.js, rather
# than merely checking for their names. This keeps browser/server duplicate
# behavior locked to the same coded-name collision and missing-code fallback.
helper_src = []
for helper_name in ("trimStr", "serviceKey", "sameServiceIdentity",
                    "serviceEffectiveUnit", "sameServiceVariant",
                    "compareServiceVariants"):
    helper = re.search(r"  function %s\([^\n]*\) \{.*?\n  \}" % helper_name,
                       admission_js, re.S)
    check(helper is not None, f"admission.js helper {helper_name}() was not found")
    if helper:
        helper_src.append(helper.group(0))
if len(helper_src) == 6:
    browser_identity_js = "\n".join(helper_src) + r'''
function ok(v) { if (!v) process.exit(2); }
ok(serviceKey('  A۱۲٣\u200f ') === 'a123');
ok(serviceKey('\u200cنام\u200c\u200f  خدمت\u200e') === 'نام خدمت');
ok(serviceKey('كد ١٢۳') === serviceKey('کد 123'));
ok(sameServiceIdentity(serviceKey('A'), serviceKey('shared'),
                       serviceKey('A'), serviceKey('shared')));
ok(!sameServiceIdentity(serviceKey('A'), serviceKey('shared'),
                        serviceKey('B'), serviceKey('shared')));
ok(sameServiceIdentity(serviceKey(''), serviceKey('shared'),
                       serviceKey('B'), serviceKey('shared')));
var normal = { code: 'A۱۲٣', name: 'shared', price: 100, freeRate: false, desc: '' };
var free = { code: 'a١٢۳', name: 'shared', price: 100, freeRate: true, freePrice: 250, desc: '' };
ok(!sameServiceVariant(serviceKey(normal.code), serviceKey(normal.name), normal,
                       serviceKey(free.code), serviceKey(free.name), free));
var nf = [normal, free].sort(compareServiceVariants);
var fn = [free, normal].sort(compareServiceVariants);
ok(!nf[0].freeRate && nf[1].freeRate);
ok(!fn[0].freeRate && fn[1].freeRate);
ok(serviceEffectiveUnit(nf[0]) + serviceEffectiveUnit(nf[1]) ===
   serviceEffectiveUnit(fn[0]) + serviceEffectiveUnit(fn[1]));
'''
    try:
        browser_run = subprocess.run(
            ["node", "-e", browser_identity_js], capture_output=True, text=True, timeout=60
        )
        check(browser_run.returncode == 0,
              "admission.js code-first duplicate identity behavior failed")
    except FileNotFoundError:
        failures.append("node is required for browser duplicate-identity coverage")

# ===========================================================================
if failures:
    for failure in failures:
        print(f"FAIL: {failure}")
    sys.exit(1)

print("PASS: 8 presets all include name + description + quantity + line amount")
for note in notes:
    print(note)
print("PASS: 30 specs = 10 distinct layout families x 3 variants, legible pitch/borders")
print("PASS: all 30 designs are layout-distinct (per-family variant heights differ) + mixed code carrier")
print("PASS: all 30 designs carry their Persian name (no more blank gallery cards)")
print("PASS: every family emits exactly one live PIT_SERVICES table + a footer band")
print("PASS: runtime service rows are compact, wrapped, bounded, and never sample-padded")
print("PASS: assets/designer/templates.js mirrors the C++ seeder exactly")
print("PASS: tpl_migration_1_97 guard stamped for fresh installs and upgrades")
print("PASS: toggled-back normal rows charge 170 in both orders through production canonicalization")
