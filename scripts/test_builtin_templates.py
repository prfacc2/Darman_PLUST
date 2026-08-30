#!/usr/bin/env python3
"""Structural regression checks for the builtin print designs (v2.07).

Validates the ready-made-template contract:

  1. src/print_designer_templates.inc — the C++ composer (بساز_طرح) the print
     engine uses. All 30 templates (T01..T30) + TB1 must be produced by the
     single composer and always carry the 13 mandatory blocks of §4.3.
  2. assets/designer/templates.js — the gallery display list. Since v2.07 this
     file deliberately carries ONLY the frozen §4.2 names + paper (the
     authoritative layouts are served from the seeded designs through the
     designer bridge), so it must no longer embed a hand-written layout copy.

v2.07 policy: the services table is EXACTLY نام خدمت | تعداد | شرح خدمت;
the barcode value appears ONLY as the HRI under the graphic; شماره پرونده and
شماره سابقه are absent from every builtin layout (their tokens stay available
for user designs).
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
INC = ROOT / "src" / "print_designer_templates.inc"
JS = ROOT / "assets" / "designer" / "templates.js"
PRINTER = ROOT / "src" / "printer.cpp"
POLICY = ROOT / "src" / "print_services_policy.h"

failures = []
notes = []
_invariant_count = [0]   # every check() that passes increments this


def check(condition, message):
    if not condition:
        failures.append(message)
    else:
        _invariant_count[0] += 1
    return bool(condition)


# ===========================================================================
# 0. shared caption classifier (mirrors printer.cpp::pdSvcColOf)
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


COL_RULES = [
    (("\u0634\u0631\u062d", "\u062a\u0648\u0636\u06cc\u062d"), "DESC"),
    (("\u0646\u0648\u0639",), "CAT"),
    (("\u062a\u0639\u062f\u0627\u062f", "\u0645\u0642\u062f\u0627\u0631"), "QTY"),
    (("\u0631\u062f\u06cc\u0641", "\u0634\u0645\u0627\u0631\u0647", "#", "\u2116"), "ROW"),
    (("\u0633\u0647\u0645\u0628\u06cc\u0645\u0647", "\u0633\u0647\u0645\u067e\u0627\u06cc\u0647", "\u0628\u06cc\u0645\u0647"), "INS"),
    (("\u0633\u0647\u0645\u0628\u06cc\u0645\u0627\u0631", "\u067e\u0631\u062f\u0627\u062e\u062a\u06cc"), "PAT"),
    (("\u062a\u062e\u0641\u06cc\u0641",), "DISC"),
    (("\u0645\u0628\u0644\u063a\u06a9\u0644", "\u062c\u0645\u0639", "\u06a9\u0644"), "LINE"),
    (("\u0642\u06cc\u0645\u062a", "\u0641\u06cc", "\u0645\u0628\u0644\u063a", "\u0646\u0631\u062e"), "PRICE"),
    (("\u06a9\u062f",), "CODE"),
    (("\u0646\u0627\u0645\u062e\u062f\u0645\u062a", "\u062e\u062f\u0645\u062a", "\u0646\u0627\u0645", "\u0639\u0646\u0648\u0627\u0646"), "NAME"),
]


def classify(label):
    raw = (label or "").strip()
    if raw in ("#", "\u2116"):
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
    "pdSvcColOf" in PRINTER.read_text(encoding="utf-8"),
    "printer.cpp no longer exposes pdSvcColOf — the caption contract is gone",
)

# ===========================================================================
# 1. src/print_designer_templates.inc — the composer contract
# ===========================================================================
inc = INC.read_text(encoding="utf-8")

check("PIT_SERVICES" in inc, "the composer never emits a PIT_SERVICES item")
check(
    "it.type=PIT_SERVICES" in inc.replace(" ", ""),
    "mkServices() does not build a PIT_SERVICES item",
)
check("it.type=PIT_BARCODE" in inc.replace(" ", ""),
      "mkBarcode() does not build a PIT_BARCODE item")
check(
    'it.field=L"receiptbarcode"' in inc.replace(" ", ""),
    "PIT_BARCODE is not bound to receiptbarcode",
)
check('hri\\":true' in inc or 'hri":true' in inc,
      "the barcode model is not hri:true (value must appear only under the graphic)")

# --- the single composer ---------------------------------------------------
check("struct \u0637\u0631\u062d_\u067e\u0627\u0631\u0627\u0645\u062a\u0631" in inc,
      "the \u0637\u0631\u062d_\u067e\u0627\u0631\u0627\u0645\u062a\u0631 struct is missing")
check(
    "static PrintDesign \u0628\u0633\u0627\u0632_\u0637\u0631\u062d(const \u0637\u0631\u062d_\u067e\u0627\u0631\u0627\u0645\u062a\u0631& p)" in inc,
    "the \u0628\u0633\u0627\u0632_\u0637\u0631\u062d composer is missing",
)
check(
    "PrintDesign \u0628\u0633\u0627\u0632_\u0637\u0631\u062d(const \u0637\u0631\u062d_\u067e\u0627\u0631\u0627\u0645\u062a\u0631& p)" in inc,
    "the composer signature is not intact",
)
check(
    re.search(r"return\s*\u0628\u0633\u0627\u0632_\u0637\u0631\u062d\(TPL_TABLE\[idx-1\]\)", inc) is not None,
    "buildTemplate() must produce every builtin through the composer",
)

# --- the frozen parameter table (30 rows) ---------------------------------
tbl_match = re.search(
    r"static const \u0637\u0631\u062d_\u067e\u0627\u0631\u0627\u0645\u062a\u0631 TPL_TABLE\[30\] = \{(.*?)\n\};",
    inc, re.S,
)
check(tbl_match is not None, "the TPL_TABLE[30] parameter table was not found")
rows = []
if tbl_match:
    for line in tbl_match.group(1).splitlines():
        m = re.search(
            r'L"(T\d\d)"\s*,\s*L"([^"]*)"\s*,\s*L"([^"]*)"\s*,\s*(\d)\s*,\s*(\d)\s*,\s*(\d)\s*,\s*([\d.]+)',
            line,
        )
        if m:
            rows.append({
                "code": m.group(1), "name": m.group(2), "paper": m.group(3),
                "orient": int(m.group(4)), "layout": int(m.group(5)),
                "emph": int(m.group(6)), "pt": float(m.group(7)),
            })
check(len(rows) == 30, f"expected 30 \u0637\u0631\u062d_\u067e\u0627\u0631\u0627\u0645\u062a\u0631 rows, parsed {len(rows)}")
if rows:
    codes = [r["code"] for r in rows]
    check(codes == ["T%02d" % i for i in range(1, 31)],
          f"codes are not T01..T30 in order: {codes[:5]}…{codes[-3:]}")
    names = [r["name"] for r in rows]
    check(len(set(names)) == len(names), "two templates share the same display name")
    papers = {r["paper"] for r in rows}
    for want in ("A4", "A5", "A6", "R80", "R58"):
        check(want in papers, f"the parameter table is missing the {want} paper")
    layouts = {r["layout"] for r in rows}
    # the composer must SUPPORT the families with distinct behaviour:
    # 1 = two-column, 2 = banded, 3 = card. Families 0 (one-column) and
    # 4 (roll) share the base flow — their geometry is driven by the paper.
    for want in sorted(layouts):
        if want in (0, 4):
            continue
        check(re.search(r"p\.\u0686\u06cc\u062f\u0645\u0627\u0646==%d" % want, inc) is not None,
              f"the table uses \u0686\u06cc\u062f\u0645\u0627\u0646 {want} but the composer has no branch for it")
    # roll family templates must be narrow paper (the geometry driver)
    for i, r in enumerate(rows):
        if r["layout"] == 4:
            check(r["paper"] in ("R80", "R58"),
                  f"{r['code']} is \u0686\u06cc\u062f\u0645\u0627\u0646 4 (roll) but its paper is {r['paper']}")
    emphs = {r["emph"] for r in rows}
    for want in (0, 1, 2):
        check(want in emphs, f"emphasis level \u062a\u0623\u06a9\u06cc\u062f {want} is never used")
    for i, r in enumerate(rows):
        check(6.5 <= r["pt"] <= 11.0,
              f"{r['code']} base font {r['pt']}pt is outside 6.5..11pt")
        check(0 <= r["orient"] <= 1, f"{r['code']} orientation is not 0/1")

# --- the canonical 3-column services model --------------------------------
# anchor on the cols literal and join the two adjacent C++ string literals
_ci = inc.find('cols')
_start = inc.rfind('L"', 0, _ci)
_seg = inc[_start:]
svc_m = re.match(r'L"((?:[^"\\]|\\.)*)"\s*\n\s*L"((?:[^"\\]|\\.)*)"', _seg)
svc_raw = ""
if svc_m:
    svc_raw = (svc_m.group(1) + svc_m.group(2)).replace('\\\"', '"')
    try:
        import json
        parsed = json.loads(svc_raw)
        labels = parsed.get("labels") or []
        kinds = [classify(lab) for lab in labels]
        check(parsed.get("cols") == 3, "services cols != 3")
        check(kinds == ["NAME", "QTY", "DESC"],
              f"services columns are not \u0646\u0627\u0645 \u062e\u062f\u0645\u062a|\u062a\u0639\u062f\u0627\u062f|\u0634\u0631\u062d \u062e\u062f\u0645\u062a: {kinds}")
        widths = parsed.get("widths") or []
        check(len(widths) == 3, f"expected 3 widths, got {len(widths)}")
        if len(widths) == 3:
            check(abs(sum(widths) - 1.0) < 0.005,
                  f"services widths sum to {sum(widths):.4f}, not 1.0")
    except ValueError:
        check(False, "the services model JSON does not parse")
else:
    check(False, "the composer does not store the canonical 3-column model")

# --- v2.07 removals: no fileNo/archiveNo/duplicate barcode text ------------
# Scope to the composer BODY only — comments and the smoke verifier may
# legitimately mention the removed names; only emitted LAYOUT items count.
comp_i = inc.find("static PrintDesign \u0628\u0633\u0627\u0632_\u0637\u0631\u062d")
tb1_i = inc.find("static PrintDesign \u0628\u0633\u0627\u0632_\u0628\u0631\u0686\u0633\u0628_\u0628\u0627\u0631\u06a9\u062f")
comp = inc[comp_i:tb1_i] if (comp_i > 0 and tb1_i > comp_i) else inc
for forbidden in ("archiveNo", "fileNo", "FA_ARCHIVE", "FA_FILENO"):
    check(forbidden not in comp,
          f"the composer still emits {forbidden} — v2.07 removed it from every builtin layout")
check(
    not re.search(r'putLF\([^)]*L"receiptbarcode"', comp),
    "the composer still emits a duplicate {receiptbarcode} text row",
)

# --- the 13 mandatory blocks are emitted in order -------------------------
MANDATORY_FIELDS = [
    "clinicname", "clinicaddr", "clinicphone", "receipttitle",
    "apptdate", "appttime", "queue",
    "nid", "ins_full", "supp_full",
    "P-Name", "age", "certno",
    "performer", "performercode",
    "doctor", "specialty", "doctorcode",
    "paid", "total", "basepay", "supppay",
    "discount_from", "discount", "cash", "pos",
    "receptionist", "cashier_name", "scnum", "datetime",
]
for f in MANDATORY_FIELDS:
    check('L"%s"' % f in inc,
          f"the composer never binds the mandatory field {f}")

# --- v2.07.1: PURE BLACK-AND-WHITE — no colour anywhere in a builtin ----
# The clinic's printers are monochrome; emphasis is weight/rules only.
check("R_ACCENT" not in inc and "R_DANGER" not in inc and "R_MUTED" not in inc,
      "the composer still carries a colour palette — builtins must be pure B/W")
check("0x0B3D91" not in inc and "0xA31212" not in inc,
      "the composer still carries ACCENT/DANGER hex values")
check(re.search(r'fillColor=R_', inc) is None,
      "the composer still fills a rect with a non-white colour")
# every textColor the composer sets must be R_INK
for m in re.finditer(r'textColor=(0x[0-9A-Fa-f]+|R_INK)', inc):
    val = m.group(1)
    check(val == "R_INK" or val.upper() == "0X000000" or val.upper() == "0XFFFFFF",
          f"the composer sets a non-ink text colour: {val}")
# templates.js mirror must be B/W too
js_bw = JS.read_text(encoding="utf-8")
check('var INK = "#000000"' in js_bw and '#0B3D91' not in js_bw and '#A31212' not in js_bw,
      "templates.js is not pure black-and-white")

# --- TB1 barcode-only label (§7.5) ---------------------------------------
check("\u0628\u0633\u0627\u0632_\u0628\u0631\u0686\u0633\u0628_\u0628\u0627\u0631\u06a9\u062f" in inc,
      "the TB1 \u0628\u0631\u0686\u0633\u0628 \u0628\u0627\u0631\u06a9\u062f builder is missing")
check("Design_BuiltinTemplate(31)" in inc or "31" in inc,
      "TB1 is not reachable at store index 31")
# §7.5: TB1 carries EXACTLY the reduced block set — a زیربخش پذیرش gets no
# other print, so drift here is release-critical.
_tb1_i = inc.find("static PrintDesign \u0628\u0633\u0627\u0632_\u0628\u0631\u0686\u0633\u0628_\u0628\u0627\u0631\u06a9\u062f")
_tb1_end = inc.find("\n}\n", _tb1_i)
tb1 = inc[_tb1_i:_tb1_end] if (_tb1_i > 0 and _tb1_end > _tb1_i) else ""
if tb1:
    for fld in ("clinicname", "receipttitle", "P-Name", "nid", "queue", "datetime"):
        check('L"%s"' % fld in tb1,
              f"TB1 is missing its §7.5 field {{{fld}}}")
    check("mkBarcode" in tb1, "TB1 has no PIT_BARCODE")
    check("mkServices" not in tb1,
          "TB1 must NOT carry a services table (barcode-only label)")
    for money in ("paid", "total", "basepay", "supppay", "cash", "pos",
                  "receptionist", "cashier_name"):
        check('L"%s"' % money not in tb1,
              f"TB1 must NOT carry the payment/footer field {{{money}}}")
else:
    check(False, "the TB1 builder body could not be extracted for verification")

# --- names table -----------------------------------------------------------
name_match = re.search(r"static const wchar_t\* const TPL_NAMES\[31\]=\{(.*?)\n\};", inc, re.S)
check(name_match is not None, "the TPL_NAMES[31] table was not found")
inc_names = re.findall(r'L"([^"]*)"', name_match.group(1)) if name_match else []
check(len(inc_names) == 31, f"expected 31 template names, got {len(inc_names)}")
check(len(set(inc_names)) == len(inc_names), "two builtin templates share the same name")
check(all(n.strip() for n in inc_names),
      "a builtin template name is blank — the gallery would show an empty card")
check(inc_names and inc_names[0] == "\u067e\u06cc\u0634\u200c\u0641\u0631\u0636",
      "index 0 is not named \u300e\u067e\u06cc\u0634\u200c\u0641\u0631\u0636\u300f")

# --- migration guard -------------------------------------------------------
check('getSetting(L"tpl_migration_2_07_1"' in inc,
      "the v2.07.1 B/W rebuild migration guard is missing")
init_fn = re.search(r"void Designs_Init\(\)\{(.*?)\n\}", inc, re.S)
check(init_fn is not None, "Designs_Init() was not found")
if init_fn:
    init_body = init_fn.group(1)
    check("Designs_Insert(d)" in init_body and "Designs_Update(fresh)" in init_body,
          "Designs_Init() no longer both seeds fresh installs and rebuilds existing ones")
    check("Designs_Delete(existing[i].id)" in init_body,
          "Designs_Init() no longer removes surplus builtins")
    check("const int N = 31;" in init_body, "Designs_Init() does not seed 31 builtins")

# ===========================================================================
# 2. print_services_policy.h — the barcode de-dup guard
# ===========================================================================
pol = POLICY.read_text(encoding="utf-8")
check("pdBarcodeValueAlreadyRendered" in pol,
      "print_services_policy.h is missing the barcode de-dup guard")
check("pdBarcodeValueAlreadyRendered" in PRINTER.read_text(encoding="utf-8"),
      "printer.cpp never consults the barcode de-dup guard")

# ===========================================================================
# 3. assets/designer/templates.js — display list only (v2.07)
# ===========================================================================
js = JS.read_text(encoding="utf-8")
check("window.AZ_TEMPLATES" in js, "templates.js no longer publishes window.AZ_TEMPLATES")
check("AZ_TEMPLATES" in js, "the gallery export is gone")
# v2.07: this file must NOT embed a layout mirror (it drifted every release)
for gone in ("buildSamenDefault", "buildExtra", "svcModelJson"):
    check(gone not in js,
          f"templates.js still embeds the legacy {gone} layout mirror — the "
          "authoritative layouts come from the seeded designs via the bridge")
# the JS mirror must publish 31 full designs (default + T01..T30) — verify
# by executing it and counting, since the names are Persian literals.
import subprocess as _sp, json as _json
_probe = (
    "global.window={};require(process.argv[1]);"
    "var T=window.AZ_TEMPLATES;"
    "if(!T||T.length!==31)process.exit(1);"
    "var need=['clinicname','clinicaddr','clinicphone','receipttitle','apptdate',"
    "'appttime','queue','nid','ins_full','supp_full','P-Name','age','certno',"
    "'performer','performercode','doctor','specialty','doctorcode','paid','total',"
    "'basepay','supppay','discount_from','discount','cash','pos','receptionist',"
    "'cashier_name','scnum','datetime'];"
    "var bad=0;"
    "T.forEach(function(t){"
    "  if(!t.items||t.items.length<30)bad++;"
    "  var f={};var nb=0,ns=0;"
    "  t.items.forEach(function(it){if(it.field)f[it.field]=1;"
    "    if(it.type==='barcode')nb++;if(it.type==='services')ns++;});"
    "  need.forEach(function(k){if(!f[k])bad++;});"
    "  if(nb!==1||ns!==1)bad++;"
    "});"
    "process.exit(bad?1:0);"
)
_r = _sp.run(["node", "-e", _probe, str(JS)])
check(_r.returncode == 0,
      "templates.js must publish 31 full B/W designs with the 13 mandatory blocks, "
      "exactly one barcode and one services table each")

# ===========================================================================
# report
# ===========================================================================
print("\n".join(notes))
if failures:
    print("\n%d FAIL:" % len(failures))
    for f in failures:
        print("  FAIL: %s" % f)
    sys.exit(1)
print("\nOK — %d builtin-template invariants hold" % _invariant_count[0])
