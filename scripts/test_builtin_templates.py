#!/usr/bin/env python3
"""Structural regression checks for the 31 builtin print designs (v2.00).

Validates BOTH sides of the ready-made-template contract:

  1. src/print_designer_templates.inc  — the C++ seeder the print engine uses
  2. assets/designer/templates.js      — the ES5 mirror the web gallery shows

Index 0 is the never-deletable «پیش‌فرض» exact thermal R80 Samen receipt.
Indices 1..30 are 30 additional designs, all different, mixing R80/R58/A5/A4.
Every design owns exactly ONE live services table and exactly ONE Code128
barcode bound to receiptbarcode (never nid). Default services are
نام خدمت | # | شرح خدمت (NAME+ROW+DESC). Extra 4+ column presets still
carry NAME+DESC+QTY+LINE.
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
    (("\u0631\u062f\u06cc\u0641", "\u0634\u0645\u0627\u0631\u0647", "#", "№"), "ROW"),
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
    raw = (label or "").strip()
    if raw in ("#", "№"):
        return "ROW"
    norm = _norm(label)
    if not norm:
        return "NONE"
    for needles, kind in COL_RULES:
        for needle in needles:
            if needle in norm:
                return kind
    return "NONE"


check(
    classify("\u0633\u0647\u0645 \u0628\u06cc\u0645\u0627\u0631") == "INS"
    or classify("\u0633\u0647\u0645 \u0628\u06cc\u0645\u0627\u0631") == "PAT",
    "the pdSvcColOf mirror cannot classify «سهم بیمار» at all",
)
check(classify("#") == "ROW", "pdSvcColOf mirror does not treat «#» as ROW")
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
check("buildSamenDefault" in inc, "the Samen default builder is missing")
check('d.paper=L"R80"' in inc, "default design is not authored against R80")
check("درمانگاه شبانه روزی ثامن الائمه" in inc, "exact Samen clinic title is missing from the seeder")
check("آدرس : " in inc and "تلفن : " in inc, "exact آدرس/تلفن prefixes are missing")
check("شماره معرفی نامه" in inc, "referral caption is not the C2 «شماره معرفی نامه» string")
check("شماره پرونده" in inc and "شماره سابقه" in inc, "fileNo/archiveNo captions are missing")
check('L"receiptbarcode"' in inc, "barcode is not bound to receiptbarcode")
check(
    'it.field=L"receiptbarcode"' in inc.replace(" ", ""),
    "PIT_BARCODE is not bound to receiptbarcode",
)

preset_names = [
    "SVC3", "SVC4_ROW", "SVC4_CAT", "SVC5", "SVC5_CODE",
    "SVC6_FIN", "SVC6_INS", "SVC7", "SVC_SAMEN",
]
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
    len(inc_presets) >= 8,
    f"expected at least 8 parsable column presets in svcModelJson, got {sorted(inc_presets)}",
)
check("SVC_SAMEN" in inc_presets, "SVC_SAMEN (نام خدمت|#|شرح خدمت) preset is missing")


def audit_preset(where, key, model, defaultish=False):
    cols = model.get("cols")
    widths = model.get("widths") or []
    labels = model.get("labels") or []
    check(model.get("header") is True, f"{where} preset {key} has no header row")
    check(cols in (3, 4, 5, 6, 7), f"{where} preset {key} declares an odd column count {cols}")
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
    check("NAME" in kinds, f"{where} preset {key} lacks mandatory NAME: {kinds}")
    check("DESC" in kinds, f"{where} preset {key} lacks mandatory DESC: {kinds}")
    if cols >= 4:
        for mandatory in ("QTY", "LINE"):
            check(
                mandatory in kinds,
                f"{where} preset {key} lacks mandatory {mandatory}: {kinds}",
            )
    check(
        len(set(kinds)) == len(kinds),
        f"{where} preset {key} repeats a column kind: {kinds}",
    )
    if kinds[-1] != "DESC" and "ROW" in kinds:
        check(
            kinds[-1] == "ROW" and kinds[-2] == "DESC",
            f"{where} preset {key} must put DESC last (or before trailing ROW): {kinds}",
        )
    else:
        check(
            kinds[-1] == "DESC",
            f"{where} preset {key} must put DESC last: {kinds}",
        )
    if defaultish:
        check(
            kinds == ["NAME", "ROW", "DESC"],
            f"{where} default services must be NAME+ROW+DESC, got {kinds}",
        )
    return kinds


for key, model in sorted(inc_presets.items()):
    kinds = audit_preset("inc", key, model, defaultish=(key == "SVC_SAMEN"))
    notes.append("  %-9s %d cols  %s" % (key, model.get("cols"), " | ".join(kinds)))

# --- names ----------------------------------------------------------------
name_match = re.search(r"static const wchar_t\* const TPL_NAMES\[31\]=\{(.*?)\n\};", inc, re.S)
check(name_match is not None, "the TPL_NAMES[31] table was not found")
inc_names = re.findall(r'L"([^"]*)"', name_match.group(1)) if name_match else []
check(len(inc_names) == 31, f"expected 31 template names, got {len(inc_names)}")
check(len(set(inc_names)) == len(inc_names), "two builtin templates share the same name")
check(
    all(n.strip() for n in inc_names),
    "a builtin template name is blank — the gallery would show an empty card",
)
check(inc_names and inc_names[0] == "پیش‌فرض", "index 0 is not named «پیش‌فرض»")
check(
    "d.name = TPL_NAMES[idx];" in inc,
    "buildTemplate() does not stamp TPL_NAMES onto the design",
)
check(
    "return TPL_NAMES[idx];" in inc,
    "buildTemplateName() no longer shares the single TPL_NAMES table",
)

build_match = re.search(
    r"static PrintDesign buildTemplate\(int idx\)\{(.*?)\n    return d;\n\}", inc, re.S
)
check(build_match is not None, "buildTemplate() body was not found")
build = build_match.group(1) if build_match else ""
check("buildSamenDefault(d)" in build, "buildTemplate() does not emit the Samen default")
check("buildExtra(d, idx-1)" in build, "buildTemplate() does not emit the 30 extras")

# --- extra spec table -----------------------------------------------------
extra_match = re.search(r"static const ExtraSpec EXTRA\[30\] = \{(.*?)\n\};", inc, re.S)
check(extra_match is not None, "the ExtraSpec EXTRA[30] table was not found")
extras = []
if extra_match:
    for line in extra_match.group(1).splitlines():
        row = re.search(
            r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(SVC[A-Z0-9_]*)\s*,\s*L\"([^\"]+)\"\s*,(.*?)\}",
            line,
        )
        if not row:
            continue
        rest = [p.strip() for p in row.group(5).split(",")]
        extras.append(
            {
                "layout": int(row.group(1)),
                "variant": int(row.group(2)),
                "svc": row.group(3),
                "paper": row.group(4),
                "bw": float(rest[0]),
                "rowH": float(rest[1]),
            }
        )
check(len(extras) == 30, f"expected 30 ExtraSpec rows, parsed {len(extras)}")
if len(extras) == 30:
    papers = {e["paper"] for e in extras}
    check("R80" in papers and "R58" in papers, f"extras are missing thermal papers: {papers}")
    check("A5" in papers and "A4" in papers, f"extras are missing sheet papers: {papers}")
    fams = sorted({e["layout"] for e in extras})
    check(fams == list(range(10)), f"expected 10 extra layout families 0..9, got {fams}")
    for i, e in enumerate(extras):
        check(
            3.8 <= e["rowH"] <= 9.0,
            f"extra {i + 1:02d} row pitch {e['rowH']} mm is outside 3.8..9 mm",
        )
        check(
            0.2 <= e["bw"] <= 0.8,
            f"extra {i + 1:02d} table border {e['bw']} mm is outside 0.2..0.8 mm",
        )

# --- migration guard ---------------------------------------------------
check(
    'getSetting(L"tpl_migration_2_00"' in inc or 'getSetting(L"tpl_migration_1_200"' in inc,
    "the v2.00 migration guard is missing",
)
init_fn = re.search(r"void Designs_Init\(\)\{(.*?)\n\}", inc, re.S)
check(init_fn is not None, "Designs_Init() was not found")
if init_fn:
    init_body = init_fn.group(1)
    check(
        init_body.count("stamp();") == 2,
        "the migration must be stamped for fresh installs and the v2.00 upgrade "
        f"(found {init_body.count('stamp();')} stamp() calls)",
    )
    check(
        "Designs_Insert(d)" in init_body and "Designs_Update(fresh)" in init_body,
        "Designs_Init() no longer both seeds fresh installs and rebuilds existing ones",
    )
    check(
        "Designs_Delete(existing[i].id)" in init_body,
        "Designs_Init() no longer removes surplus builtins beyond the 31",
    )
    check("const int N = 31;" in init_body, "Designs_Init() does not seed 31 builtins")
for old in ("1_52", "1_53", "1_58", "1_59", "1_60", "1_61", "1_62", "1_65", "1_66", "1_67", "1_98", "1_99"):
    check(
        'setSetting(L"tpl_migration_%s", L"1")' % old in inc,
        f"upgrade path no longer retires the tpl_migration_{old} guard",
    )

# ===========================================================================
# 2. assets/designer/templates.js — executed, then compared to the .inc
# ===========================================================================
js = JS.read_text(encoding="utf-8")
check("window.AZ_TEMPLATES" in js, "templates.js no longer publishes window.AZ_TEMPLATES")
check("buildSamenDefault" in js, "templates.js is missing the Samen default builder")
check("درمانگاه شبانه روزی ثامن الائمه" in js, "templates.js is missing the Samen clinic title")
check("آدرس : " in js and "تلفن : " in js, "templates.js is missing exact آدرس/تلفن prefixes")

PAPER = {"R80": (80.0, 200.0), "R58": (58.0, 200.0), "A5": (148.0, 210.0), "A4": (210.0, 297.0)}
REQUIRED_FIELDS = [
    "clinicaddr", "clinicphone", "apptdate", "appttime", "queue", "nid",
    "receiptbarcode", "ins", "supp", "supp_percent", "full", "fileNo",
    "archiveNo", "doctor", "doctorcode", "specialty", "specialtycode",
    "paid", "total", "insshare", "supppay", "discount_from", "discount",
    "cash", "pos", "eprescription", "referralno", "receptionist", "cashier",
    "scnum", "reg_ts",
]

harness = r"""
var fs = require('fs');
global.window = {};
new Function(fs.readFileSync(process.argv[2], 'utf8')).call(global);
var all = global.window.AZ_TEMPLATES;
var out = [];
for (var i = 0; i < all.length; i++) {
  var t = all[i], svc = [], barcode = [], k;
  var fields = {}, labels = [];
  for (k = 0; k < t.items.length; k++) {
    if (t.items[k].type === 'services') svc.push(t.items[k]);
    if (t.items[k].type === 'barcode' || t.items[k].type === 'qr') barcode.push(t.items[k]);
  }
  var minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
  var hasPhoto = 0, hasLogo = 0, hasStub = 0, hasCap = 0, hasPayLbl = 0, hasFrame = 0;
  var gray = 0, round = 0, nidBarcode = 0;
  var nameIt = null, paidIt = null;
  for (k = 0; k < t.items.length; k++) {
    var it = t.items[k];
    if (it.field) fields[it.field] = (fields[it.field] || 0) + 1;
    if (it.type === 'label' && it.text) labels.push(it.text);
    if (it.isFrame || it.type === 'frame') hasFrame++;
    if (!it.isFrame && it.type !== 'frame') {
      if (it.x < minX) minX = it.x;
      if (it.y < minY) minY = it.y;
      if (it.x + it.w > maxX) maxX = it.x + it.w;
      if (it.y + it.h > maxY) maxY = it.y + it.h;
    }
    if (it.field === 'full' && !nameIt) nameIt = it;
    if (it.field === 'paid' && !paidIt) paidIt = it;
    if (it.type === 'photo') hasPhoto++;
    if (it.type === 'logo') hasLogo++;
    if (it.text === 'مشخصات بیمار') hasCap++;
    if (it.text === 'پرداخت‌ها') hasPayLbl++;
    if (it.text === 'نسخهٔ بیمار' || it.text === '— — — محل جدا کردن — — —') hasStub++;
    if (it.fillTransparent === false && it.fillColor && it.fillColor !== '#ffffff' && it.fillColor !== '#FFFFFF') gray++;
    if ((it.corner || 0) > 0.01) round++;
    if ((it.type === 'barcode' || it.type === 'qr') && it.field === 'nid') nidBarcode++;
  }
  var s = svc.length === 1 ? svc[0] : null;
  var bc = barcode.length === 1 ? barcode[0] : null;
  var model = null;
  if (s) { try { model = JSON.parse(s.text); } catch (e) { model = null; } }
  out.push({
    name: t.name, paper: t.paper, orientation: t.orientation,
    items: t.items.length, svcCount: svc.length, barcodeCount: barcode.length,
    model: model, h: s ? s.h : 0, y: s ? s.y : 0, rowH: s ? s.rowH : 0,
    headerH: s ? s.headerH : 0,
    barcodeY: bc ? bc.y : -1, barcodeH: bc ? bc.h : 0, barcodeW: bc ? bc.w : 0,
    barcodeField: bc ? bc.field : '',
    minX: minX, minY: minY, maxX: maxX, maxY: maxY,
    nameY: nameIt ? nameIt.y : -1, paidY: paidIt ? paidIt.y : -1,
    hasPhoto: hasPhoto, hasLogo: hasLogo, hasStub: hasStub,
    hasCap: hasCap, hasPayLbl: hasPayLbl, hasFrame: hasFrame,
    gray: gray, round: round, nidBarcode: nidBarcode,
    fields: fields, labels: labels,
    prefixes: t.items.filter(function (it) { return it.prefix; }).map(function (it) { return it.prefix; })
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
    check(len(js_designs) == 31, f"templates.js publishes {len(js_designs)} designs, expected 31")
    js_names = [d["name"] for d in js_designs]
    check(
        js_names == inc_names,
        "the web gallery names have drifted from the C++ TPL_NAMES table",
    )
    fps = []
    papers_seen = set()
    for i, d in enumerate(js_designs):
        tag = "web template %02d" % i
        papers_seen.add(d["paper"])
        check(d["orientation"] == 0, f"{tag} is not portrait")
        check(
            d["paper"] in PAPER,
            f"{tag} uses unexpected paper {d['paper']}",
        )
        pw, ph = PAPER.get(d["paper"], (210.0, 297.0))
        check(
            d["svcCount"] == 1,
            f"{tag} must own exactly one dynamic services table; got {d['svcCount']}",
        )
        check(
            d["barcodeCount"] == 1,
            f"{tag} must own exactly one barcode/code carrier; got {d['barcodeCount']}",
        )
        check(d["nidBarcode"] == 0, f"{tag} barcode is bound to nid")
        check(
            d["barcodeField"] == "receiptbarcode",
            f"{tag} barcode field is {d['barcodeField']!r}, not receiptbarcode",
        )
        if d["svcCount"] != 1:
            continue
        check(d["model"] is not None, f"{tag} has an unparsable services model")
        if d["model"]:
            audit_preset("web", "#%02d" % i, d["model"], defaultish=(i == 0))
        check(d["rowH"] > 0 and d["headerH"] > 0, f"{tag} has no pinned row/header pitch")
        check(
            d["minX"] >= 1.5 and d["maxX"] <= pw - 1.5 + 0.05,
            f"{tag} bleeds off the printable width ({d['minX']:.1f}..{d['maxX']:.1f} of {pw})",
        )
        check(
            d["minY"] >= 1.5 and d["maxY"] <= ph + 0.2,
            f"{tag} bleeds off the printable height ({d['minY']:.1f}..{d['maxY']:.1f} of {ph})",
        )
        check(d["items"] >= 18, f"{tag} looks under-designed ({d['items']} items)")
        missing = [f for f in REQUIRED_FIELDS if f not in d["fields"]]
        check(not missing, f"{tag} missing fields: {missing}")
        check("ins_percent" not in d["fields"], f"{tag} still binds ins_percent")
        check(
            "درمانگاه شبانه روزی ثامن الائمه" in d["labels"],
            f"{tag} is missing the Samen clinic title label",
        )
        fp = (
            d["paper"],
            round(d["y"], 1),
            round(d["h"], 1),
            round(d["barcodeY"], 1),
            round(d["barcodeW"], 1),
            round(d["nameY"], 1),
            round(d["paidY"], 1),
            d["items"],
            d["hasPhoto"],
            d["hasLogo"],
            d["hasStub"],
            d["hasCap"],
            d["hasPayLbl"],
            d["hasFrame"],
        )
        fps.append(fp)

    # default exact thermal receipt
    if js_designs:
        d0 = js_designs[0]
        check(d0["name"] == "پیش‌فرض", "index 0 is not «پیش‌فرض»")
        check(d0["paper"] == "R80", f"default paper is {d0['paper']}, not R80")
        check(
            48.0 <= d0["barcodeW"] <= 52.0 and 7.0 <= d0["barcodeH"] <= 9.0,
            f"default barcode size {d0['barcodeW']:.1f}×{d0['barcodeH']:.1f} is not ~50×8 mm",
        )
        check(d0["hasFrame"] >= 1, "default is missing the body PIT_FRAME")
        check(d0["gray"] == 0, "default uses gray fills")
        check(d0["round"] == 0, "default uses rounded corners")
        check(d0["hasPhoto"] == 0 and d0["hasLogo"] == 0, "default must not carry photo/logo")
        check("آدرس : " in d0["prefixes"], "default clinicaddr prefix is not «آدرس : »")
        check("تلفن : " in d0["prefixes"], "default clinicphone prefix is not «تلفن : »")
        if d0["model"]:
            check(
                d0["model"].get("labels") == ["نام خدمت", "#", "شرح خدمت"],
                f"default services columns are {d0['model'].get('labels')}",
            )

    check("R80" in papers_seen and "R58" in papers_seen, f"gallery papers missing thermal: {papers_seen}")
    check("A4" in papers_seen and "A5" in papers_seen, f"gallery papers missing sheets: {papers_seen}")
    check(len(set(fps)) == len(fps), "two builtin designs share the same layout fingerprint")
    no_code = sum(1 for d in js_designs if d["barcodeCount"] == 0)
    with_code = sum(1 for d in js_designs if d["barcodeCount"] == 1)
    check(
        no_code == 0 and with_code == len(js_designs),
        f"expected every design barcoded, got {no_code} code-less, {with_code} barcoded",
    )

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
designer_js = (ROOT / "assets" / "designer" / "designer.js").read_text(encoding="utf-8")
fields_js = (ROOT / "assets" / "designer" / "fields.js").read_text(encoding="utf-8")
index_html = (ROOT / "assets" / "designer" / "index.html").read_text(encoding="utf-8")
check(
    "ویزیت پزشک عمومی" not in designer_js,
    "designer services preview still shows example service names",
)
check("SVC_PLACEHOLDER_ROWS = 2" in designer_js,
      "designer PIT_SERVICES preview is not locked to 2 placeholder rows")
check("[نام خدمت]" in designer_js and "[تعداد]" in designer_js and
      "[مبلغ کل]" in designer_js and "[شرح خدمت]" in designer_js,
      "designer services preview is missing the required [token] placeholders")
check('id="btnToolSelect"' in index_html and 'id="btnToolHand"' in index_html,
      "select/hand toolbar buttons are missing from the designer")
check("بارکد/کد ملی" not in fields_js, "fields.js still labels {barcode} as بارکد/کد ملی")
check("بارکد قبض" in fields_js,
      "fields.js does not label {barcode}/{receiptbarcode} as بارکد قبض")
check('L"ins_percent"' not in inc and '"ins_percent"' not in js,
      "builtin templates still bind basic insurance as a percent field")
check("درصد بیمه پایه" not in inc and "درصد بیمه پایه" not in js,
      "builtin templates still caption basic insurance as a percent")
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

print("PASS: service presets include NAME+DESC (default NAME+ROW+DESC; extras keep QTY+LINE)")
for note in notes:
    print(note)
print("PASS: 31 designs = Default R80 Samen receipt + 30 extras, mixed R80/R58/A5/A4")
print("PASS: all 31 designs are layout-distinct and carry unique Persian names")
print("PASS: default is undeletable پیش‌فرض with exact C2 labels / receiptbarcode / 3-col services")
print("PASS: every design emits exactly one live PIT_SERVICES table + one Code128")
print("PASS: runtime service rows are compact, wrapped, bounded, and never sample-padded")
print("PASS: assets/designer/templates.js mirrors the C++ seeder exactly")
print("PASS: tpl_migration_2_00 guard stamped for fresh installs and upgrades")
print("PASS: toggled-back normal rows charge 170 in both orders through production canonicalization")
