# AGENTS.md — rules for anyone (human or AI) changing this repository

> **فارسی — خلاصه:** این فایل قانون کار روی این مخزن است. این برنامه بخش‌های
> زیادی دارد و هر نسخه معمولاً فقط روی یک بخش کار می‌کند. قانون اصلی این است:
> **هیچ صفحه، آیتم یا قابلیتِ بخش‌های دیگر را حذف، جابه‌جا یا از کار نیندازید.**
> رنگ‌ها و ظاهر برنامه از `docs/DESIGN_SYSTEM.md` می‌آید و اختیاری نیست؛ هیچ
> رنگ دلبخواهی اضافه نکنید. پیش از تحویل، `./build.sh` باید سبز باشد — این
> اسکریپت خودش قرارداد UI را بررسی می‌کند و اگر چیزی از بخش‌های دیگر خراب شده
> باشد، بیلد را رد می‌کند.

`Darman_PLUST` («درمان پلاس») is a Persian, right-to-left clinic reception and
management application for Windows: pure Win32 C++17, cross-compiled from Linux
into **one static EXE** that must run on x86 and x64 Windows 7 → 11+ with no
external dependencies.

Read this file first, then `docs/ARCHITECTURE.md`, then
`docs/DESIGN_SYSTEM.md`. `docs/CHANGELOG.md` is the release history.

---

## 1. The rule that matters most: additive, never destructive

This application has many screens (welcome, staff dashboard, patient admission,
tools, receipt search, cashier, queue, management/CRM, settings, print designer,
backup). A typical release touches **one** of them. Historically, releases that
only meant to change «پذیرش بیمار» silently deleted elements, screens or
behaviour elsewhere — the single most damaging failure mode in this codebase.

Therefore:

- **Do not delete or disable anything you were not asked to change.** Not an
  element, not a field, not a button, not a CSS rule that another screen needs,
  not a bridge verb, not a permission check.
- **Do not "clean up" or refactor unrelated code** in a feature release.
- If a change genuinely requires removing something, say so explicitly in your
  report and in `docs/CHANGELOG.md`, and remove its consumers in the same
  commit. Silent removal is the thing this rule exists to prevent.
- The executable half of this rule is **`scripts/check_ui_contract.py`**, which
  `build.sh` runs on every build. It asserts that every element id each screen
  depends on still exists. If it fails, you broke another screen.
- **Never "fix" a contract failure by deleting the assertion.** If an assertion
  is genuinely obsolete, change it deliberately and explain why in your report.

## 2. Never break money or records

- Billing math lives in `src/billing.cpp` and `adComputeBill` in
  `src/web_admission_api.inc`. The server-side computation is authoritative.
  Do not reimplement it, do not "optimise" it, do not let the page's local
  preview become the source of truth.
- On-disk formats are **append-only**. `cashier_tickets.dat`, `shifts.dat`,
  `sections.dat`, `users.dat`, `persons.dat`, the daily reception CSV and
  `last_receipt.dat` all have readers that tolerate older, shorter rows. Adding
  a trailing column is safe; inserting or reordering one corrupts existing
  clinic data.
- Permission gates are checked on **both** sides (page and C++). Keep both.

## 3. Visual work goes through the design system

`docs/DESIGN_SYSTEM.md` is the authoritative palette and component library for
the whole product — the embedded HTML surfaces *and* the native GDI screens.

- Every colour must come from it. **No ad-hoc hex values.**
- One hue per panel: header wash, accent mini line, icon chip and title ink all
  derive from that panel's single hue. A panel may never mix two hues.
- The rejected colour families — violet/purple, magenta, pink, cream/beige/clay
  — must not reappear. `check_ui_contract.py` fails the build on unregistered
  colours from those families.
- The section-header accent is a **short 26×3px mini line** under the title.
  A full-width heavy rule across a card top is banned.
- Fields are crisp flat white plates with a real border. Inset / concave /
  neumorphic wells are banned.
- If the change is a real redesign rather than a tweak, update
  `docs/DESIGN_SYSTEM.md` in the same commit — the document and the code must
  never disagree.

## 4. CSS layering — never stack another "polish pass"

The single worst piece of technical debt in this repo's history: every release
from v1.65 to v1.90 appended a new "final, wins-the-cascade" layer to the bottom
of `admission.css`. Sixteen layers, ~4400 lines, all fighting each other. Nobody
could predict what a colour change would do, and the UI was rejected three
times in a row.

The structure is now fixed:

| File | Owns |
|---|---|
| `assets/admission/admission.css` | structure and layout **only** — no theme colour |
| `assets/admission/css/core.css` | shared design system: cards, section heads + mini line, fields, buttons, tables, chrome, dark parity |
| `assets/admission/css/surface-dash.css` | the staff dashboard (`body.surface-dash`) |
| `assets/admission/css/surface-tools.css` | tools + receipt search (`body.surface-tools`, `body.surface-rc`) |
| `assets/admission/css/surface-admission.css` | the admission form (`body.surface-adm`) |
| `assets/admission/css/surface-cashier.css` | the cashier «صندوق» (`body.surface-cash`, `body.surface-queue`) |

Rules:
- **Edit the one layer that owns the thing you are changing.** Never append a
  dated override block anywhere.
- A surface layer may only style its own surface. The guard enforces this.
- Every stylesheet must be registered in **three** places that must stay in
  sync: `src/app.rc` (RCDATA id), the `kAdThemeLayers` table in
  `src/web_admission_embed.inc`, and the `<link>` order in `index.html`. The
  guard enforces all three.
- An **empty** layer is a hard runtime failure: the inliner refuses to build the
  page and the customer sees a blank screen. The guard catches it first.

## 5. The dual-engine CSS contract (this is why the odd restrictions exist)

The admission/CRM pages render in **two** engines: WebView2 (Chromium) on
Windows 10/11, and the MSHTML/Trident (IE11-era) engine that ships with every
older Windows. One unsupported construct degrades the whole screen to raw HTML
on a customer's machine. Therefore, in every stylesheet under
`assets/admission/`:

| Forbidden | Use instead |
|---|---|
| `var()` / custom properties | literal hex and px |
| CSS grid | flexbox with `-ms-` prefixes |
| flex/grid `gap` | real `margin` on children |
| `backdrop-filter`, `filter:blur()` | the faked-glass recipe (DESIGN_SYSTEM §5) |
| logical properties (`padding-inline-*`, `inset-*`) | physical properties, authored for RTL |
| webfonts, `@import`, `url()`, network assets | the bundled Vazirmatn only |

Every `display:flex`, `flex-direction:`, `align-items:`, `justify-content:`,
`flex-wrap:` and `flex:` must carry its `-ms-` twin **in the same rule body**.
Declare a flat hex `background:` before any gradient, and the
`-webkit-linear-gradient` twin before the standard one.

**`assets/admission/admission.js` is ES5 only** — no `let`/`const`, no arrow
functions, no template literals, no classes. Trident cannot parse them and the
page dies with a syntax error.

## 6. Performance rules that were bought with real bugs

- Native screens paint through **dirty-rect double buffering**. Keep it. The
  frame header repaints twice a second; do not add allocations, timers,
  animations or extra shadow passes to that path.
- The dirty-rect buffer must be pre-filled with `g_theme.bg` before gradients
  are drawn, or a 1px black scanline returns at the header's bottom edge.
- GDI+ shadow loops are capped at 12 iterations. Leave the cap.
- The dashboard and tools surfaces deliberately **skip** the admission form's
  data fill (selects, performers, queue, services, zoom) on boot. That is why
  they open fast. Do not undo it.
- Never add a spinner/shimmer animation to the welcome screen; it caused a
  measurable FPS drop and was removed.

## 7. Workflow

1. Work on a branch prefixed `vorflux/`.
2. Build with `./build.sh` from the repo root. It compiles the resources and the
   C++, strips the EXE, writes `build/DarmanPlus.exe` + its `.sha256`, and runs
   `scripts/check_ui_contract.py`. **A red build is not deliverable.**
   The compiler runs with `-Wall -Wextra -Werror`: an unused variable fails it.
3. Bump the version in **all four** places or the updater breaks:
   `src/app.h` (`APP_VERSION_W`), `src/app.rc` (`FILEVERSION`,
   `PRODUCTVERSION`, `FileVersion`, `ProductVersion`), `update/version.txt`
   (version + download URL), and the header line of `docs/PROJECT_GUIDE.md`.
4. Add a `docs/CHANGELOG.md` entry: what changed, why, and in which files.
5. Ship the rebuilt `build/DarmanPlus.exe`.

## 8. Testing on this Linux sandbox

- Native GDI screens: `./shot.sh <screen> <out.png> [WxH]` under Wine
  (`home`, `settings`, `login`, …). **Use `shot.sh` (singular)** —
  `shots.sh` (plural) is stale and fails to link.
- HTML surfaces: `python3 scripts/mock_admission_host.py 8788`, then open
  `http://127.0.0.1:8788/index.html` in a real browser. This is the only way to
  see those screens here.
- The `reception` and `manage` screens **crash under Wine** with
  `0xC0000005` inside `WebAdmission_CreateView`. That is a Wine OLE-hosting
  limitation, **not an app bug**. Do not chase it; use the mock host.
- `python3 scripts/check_ui_contract.py` runs standalone too — run it early and
  often, not just at the end.

## 9. Language and conventions

- All user-facing text is Persian, right-to-left. Persian digits
  (`۰۱۲۳۴۵۶۷۸۹`) with `,` group separators for numbers; Jalali dates.
- RTL is done **manually** — `WS_EX_LAYOUTRTL` is deliberately not used.
- Departments, sections, services, insurers and users are **runtime data**
  defined in the Management screen. **Never hard-code a clinic's department or
  service name** (no literal «آزمایشگاه», «رادیولوژی», …). Route on the stable
  id/code, never on `name_fa`.
- Comments in code explain *why*, and cite the version that introduced the
  decision — that history is how the rules above were learned.
