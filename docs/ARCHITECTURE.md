# DarmanPlus Architecture & File Map — v1.92.0

> **راهنمای معماری و ساختار فایل‌ها برای مدل‌های هوش مصنوعی.**
> This document is the **operating manual** for any AI model working on
> DarmanPlus. Read it before touching code. The **Critical Safety Rules** below
> are the most important section — they exist specifically to stop destructive
> updates from breaking pages, surfaces, or features you were not asked to touch.

---

## 1. Project Overview

DarmanPlus is a **C++17 Win32 / GDI+ clinic-management desktop application**.

- **Form factor:** a single self-contained **PE32 (i686) `.exe`**, ~5.5 MB,
  fully static (no shipped DLLs). Runs on Windows 7 → 11, x86 **and** x64.
- **Toolchain:** cross-compiled from Linux with **MinGW-w64**
  (`i686-w64-mingw32-g++`), `-Wall -Wextra -Werror`.
- **Native UI:** owner-drawn GDI+ controls themed by `src/theme.cpp`.
- **Embedded surfaces:** admission, tools, cashier, queue, dashboard and
  receipts are **HTML / CSS / JS** pages rendered inside the app
  (WebView2 preferred → MSHTML/Trident fallback → native GDI last resort).
- **Localization:** Persian (RTL). Font: embedded **Vazirmatn**. Digits shown
  to the user run through `toFaDigits()`.

---

## 2. ⚠️ CRITICAL SAFETY RULES FOR AI MODELS

> **قوانین بحرانی — رعایت این قوانین برای جلوگیری از تخریب اجباری است.**
> The single most common destructive mistake is editing one page and
> accidentally deleting / renaming / restyling elements that belong to another
> page. These rules prevent exactly that.

```text
RULE 1 — ONE page, ONE scope. Do NOT touch other pages.
   Every embedded HTML surface (admission, tools, cashier, queue, dash,
   receipts) has its OWN DOM section inside assets/admission/index.html with
   distinct IDs. When you are assigned to ONE page/section, ONLY edit the DOM
   elements inside that section. NEVER delete, rename, or modify elements of
   OTHER pages — even if they look unused to you.

RULE 2 — CSS is namespaced by surface. Do NOT touch other surfaces' styles.
   Body classes scope every surface:
       surface-adm    (admission)
       surface-tools  (tools)
       surface-cash   (cashier)
       surface-queue  (queue)
       surface-dash   (dashboard)
       surface-rc     (receipts)
   When restyling a surface, ONLY modify rules prefixed with that surface's
   body class. NEVER delete global rules (.tbl, .btn, .card, .inp, …) — other
   surfaces depend on them.

RULE 3 — C++ files are modular. Stay in your file.
   Each .cpp has a clear, single responsibility (see the File Map). When
   editing one file, do NOT modify functions in another file unless the task
   explicitly requires it.

RULE 4 — Bridge verbs are shared and stable. APPEND only.
   The C++↔JS verb dispatch lives in src/web_admission_api.inc
   (pattern: if(verb=="…")). Existing JS handlers depend on verb NAMES staying
   stable. When ADDING a verb, APPEND a new if-branch — NEVER remove, rename,
   or reorder existing verbs.
   v2.07.0 appended two verbs at the END of the chain (both still active):
   «بیمه_تکمیلی_فهرست» and «بیمه_تکمیلی_انتخاب» (supplementary-insurance
   wiring for the admission surface).

RULE 5 — RCDATA resource IDs are FIXED. Never renumber.
       400–405 = admission bundle
         400 index.html   401 admission.css   402 bridge.js
         403 admission.js 404 vazir.ttf        405 contextmenu.js
       500–502 = shared shell (common.css / common.js / vazir.ttf)
       600–602 = ping page (index.html / ping.css / ping.js)
   Already allocated beyond 600: 700 = WebView2Loader (reserved),
   800–802 = CRM bundle. When adding a NEW resource, pick an unused ID block
   ≥ 700 and avoid collisions with the allocations above.
   v2.07.0: block 900–909 is RESERVED for future use. No resource in this
   release was added; existing allocations are unchanged.

RULE 6 — Never delete a feature, control, or handler.
   "Improve" / "بهبود" means RESTYLE or ENHANCE — never remove. Do not delete a
   button, input, tab, JS handler, or bridge verb unless the user EXPLICITLY
   asks for its removal.

RULE 7 — The build is -Werror. Zero warnings.
   Every change must compile cleanly under -Wall -Wextra -Werror. An unused
   variable, a sign-compare, or a missing override is a BUILD FAILURE. Verify
   with ./build.sh before considering the task done.

RULE 8 — Version is set in THREE places. Update all three together.
       1. src/app.h        → APP_VERSION_W   (e.g. L"1.92.0")
       2. src/app.rc       → FILEVERSION / PRODUCTVERSION (1,92,0,0)
                            AND the "FileVersion" / "ProductVersion" strings
       3. update/version.txt → single line version + release download URL
   A version bump that misses any of the three desynchronizes the release.
```

---

## 3. File Map (v1.92.0)

| File | Responsibility |
|------|----------------|
| `src/theme.cpp` | Color palette (`applyTheme`), the `AzFlatBtn` owner-drawn button class, vector icons (`drawIcon` / `ICO_*`) |
| `src/gdiplus.cpp` | GDI+ helpers: `gpShadow`/`gpShadowColor`, `gpGradRoundRect`/`gpGradRoundRectBg`, `gpDrawBackground` (+ cached scaled artwork) |
| `src/main.cpp` | Main frame, home screen, header/footer, screen routing |
| `src/reception.cpp` | Tab system, native admission form, hosting of dashboard / cashier / tools / queue tabs |
| `src/clinic_ops.cpp` | Cash register data model (shifts, tickets, payments) |
| `src/ui_kit.cpp` | Reusable UI primitives (`RoundedPanel`, `Card`, `InputWell`, `AzSwitch`) |
| `src/util.cpp` | Shared utilities incl. `toFaDigits()` (Persian digit conversion) |
| `assets/admission/admission.css` | ALL embedded HTML styling — Trident-safe, NO CSS vars / NO calc() |
| `assets/admission/index.html` | ALL HTML surfaces (admission / tools / cashier / queue / dash / receipts) in one file |
| `assets/admission/admission.js` | ALL JS logic for the embedded surfaces |
| `assets/admission/bridge.js` | C++↔JS IPC transport |
| `assets/admission/contextmenu.js` | Right-click context menu for embedded surfaces |
| `src/web_admission_embed.inc` | Serverless page inlining (RCDATA → one fully-inlined HTML string) |
| `src/web_admission_api.inc` | Bridge verb dispatch (C++ handlers for JS calls) |
| `src/web_admission_host.inc` | WebView2 control hosting inside the reception tab |
| `src/web_admission_mshtml.inc` | MSHTML / Trident OLE host (fallback renderer) |
| `src/web_admission_webview2.inc` | WebView2 engine binding |
| `src/web_admission.cpp` | Engine selector / lifecycle (WebView2 → MSHTML → native fallback) |
| `src/web_pages.cpp` | Generic multi-page verb dispatcher (ping, client.log, metrics) |
| `src/app.h` | Central header: `struct Theme`, `enum BtnStyle`, `enum IconId`, `S()`, `blendColor()` |
| `src/app.rc` | Win32 resource script (RCDATA blobs + version info) |
| `update/version.txt` | Released version string + download URL |
| `src/sections.h` / `src/sections.cpp` | Sections registry — v2.07 adds the optional 12th column `recept_sub` (زیربخش پذیرش), `Sections_IsReceptionSub()`, `Sections_AccountRoleLabel()` |
| `src/print_designer_templates.inc` | v2.07: the 30 builtin templates (T01–T30) + TB1, all produced by the single composer `بساز_طرح(طرح_پارامتر)` |

> `src/printer.cpp` additionally owns the v2.07 «ارتباط با چاپگر» dialog:
> window class `AzPrinterLink` (`PL_CLASS`), entry point `PrinterLink_Open(HWND)`
> (declared in app.h next to openPrinterSettings), command enum `PLB_*`
> (PLB_بستن=400, PLB_تأیید, PLB_انصراف, PLB_تست_اتصال, PLB_بازخوانی,
> PLB_پیش‌فرض_ویندوز, PLB_ITEM_BASE=500), and the shared test-print core
> `prnTestPrintTo`. It also hosts the print engine
> (`printPrintDesign` / `printPrintDesignWith`) with the 3-column services
> clamp, the elastic «بلوک_پایانی» band and the ink-saturation policy.
>
> Other `src/*.cpp` (billing, persons, employees, printer, backup, settings,
> admin, …) are feature modules. Apply RULE 3 to all of them.

---

## 4. Embedded Surface Architecture

```text
                ┌───────────────────────────────────────────────┐
                │  ONE index.html  (RCDATA 400)                 │
                │  body.surface-<name> selects the active page   │
                ├───────────────────────────────────────────────┤
                │  ONE admission.css (RCDATA 401)                │
                │  rules scoped by body.surface-<name>           │
                ├───────────────────────────────────────────────┤
                │  admission.js (403) + bridge.js (402) +        │
                │  contextmenu.js (405) + shell common.js (501)  │
                └───────────────────────────────────────────────┘
                          ▲                          ▲
                          │ window.__azSurface       │ verb IPC
                          │ (injected by C++)        │ (azCall / postMessage)
                          │                          │
                ┌─────────┴──────────────────────────┴──────────┐
                │  C++ host (web_admission_*.inc)                │
                │  WebView2 ── fail ──► MSHTML ── fail ──►       │
                │  native GDI reception form (deterministic)     │
                └────────────────────────────────────────────────┘
```

- **One HTML file, many surfaces.** All surfaces share `index.html`. The active
  surface is chosen by a `body.surface-<name>` class.
- **`window.__azSurface`** is injected by C++ (a `<script>` right after
  `<body>`) to tell JS which surface to render. Valid values:
  `admission`, `tools`, `cashier`, `queue`, `receipts`, `dash`.
- **One CSS file.** All styling lives in `admission.css`, scoped by surface
  body class (RULE 2).
- **Serverless.** Pages are **fully inlined** from RCDATA into a single HTML
  string — no HTTP, no socket, no loopback. WebView2 gets
  `NavigateToString`; MSHTML gets `about:blank` + `IHTMLDocument2::write`.
- **Renderer cascade.** WebView2 is preferred; if it cannot host, MSHTML
  (Trident) is used; if neither works, the native GDI reception form is kept as
  a deterministic last resort. **Because MSHTML is the guaranteed fallback,
  every CSS rule must be Trident-safe (RULE D-9 in DESIGN_SYSTEM.md).**

---

## 5. How to Add a New Page — اضافه کردن صفحه جدید

Follow every step. **Do not touch other surfaces' code at any step.**

```text
1. HTML  — add a new <section> in assets/admission/index.html with UNIQUE ids
           (e.g. id="myPageRoot"). Do not reuse or rename existing ids.

2. CSS   — add rules in assets/admission/admission.css scoped to
           body.surface-<name>. Use ONLY DESIGN_SYSTEM.md palette tokens,
           literal hex (Trident-safe). Do not edit other surfaces' rules.

3. JS    — in assets/admission/admission.js, gate your logic with
           if (state.surface === '<name>') { … }.
           Do not modify other surfaces' branches.

4. C++   — register the surface string in src/reception.cpp WM_CREATE:
           surf="<name>". The surface whitelist in web_admission_embed.inc
           (AdmissionPageForHost) must also accept "<name>".

5. Body class — add the mapping in applySurfaceClass() in admission.js:
           else if (surf === '<name>') cls += ' surface-<name>';

6. VERIFY — node --check assets/admission/admission.js  (JS syntax)
           visual inspection of the new CSS
           ./build.sh  (C++ must compile with -Werror, zero warnings)
```

---

## 6. How to Restyle a Page Safely — تغییر استایل بدون تخریب

> **قاعده طلایی: فقط همان صفحه را تغییر بده.** (Golden rule: change ONLY that page.)

```text
1. SCOPE   — only modify CSS rules scoped to that surface's body class
             (body.surface-<name>). Leave global rules (.tbl, .btn, .card, .inp)
             and every other surface's rules untouched.

2. HTML    — only modify elements INSIDE that surface's <section>. Do not
             delete/rename ids that other surfaces or JS reference.

3. COLORS  — update colors to match the docs/DESIGN_SYSTEM.md palette. If a
             token value changes, also update theme.cpp + admission.css + the
             design doc (File-to-Theme Sync Rule, RULE S-1..S-4).

4. VERIFY  — node --check on any edited .js
             visual inspection of edited CSS
             ./build.sh to verify C++ compilation (zero warnings, -Werror)

5. NEVER   — delete a control, handler, or feature (RULE 6). "Improve" =
             restyle/enhance, not remove.
```

---

## 7. Build & Release

```text
BUILD
  ./build.sh
    → cross-compiles with i686-w64-mingw32-g++
    → -static -static-libgcc -static-libstdc++ -Wall -Wextra -Werror
    → i686-w64-mingw32-strip
    → build/DarmanPlus.exe   (PE32 i686, static, ~5.5 MB)
    → build/DarmanPlus.exe.sha256

VERSION BUMP (before build — RULE 8, all THREE together)
  1. src/app.h          APP_VERSION_W          L"1.92.0"
  2. src/app.rc         FILEVERSION / PRODUCTVERSION  1,92,0,0
                       + VALUE "FileVersion" / "ProductVersion" strings
  3. update/version.txt  version line + release download URL

RELEASE
  commit + push + PR, then publish a GitHub release attaching
  build/DarmanPlus.exe (and the .sha256). The version.txt URL must point at
  that release asset.
```

---

## Cross-reference

- **Colors, spacing, components, input style** → `docs/DESIGN_SYSTEM.md`
- **Historical UI decision (MSHTML rationale)** → `docs/UI_ARCHITECTURE_DECISION.md`
- **General project guide** → `docs/PROJECT_GUIDE.md`

When in doubt, the **Safety Rules (§2)** and the **Design Rules (DESIGN_SYSTEM.md §2)**
win. They exist to keep every page intact across AI-assisted updates.
