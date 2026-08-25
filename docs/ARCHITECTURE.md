# معماری برنامه — Architecture Map

Navigation map for `Darman_PLUST` («درمان پلاس»). Read `AGENTS.md` first for the
rules; this document is the *where*. `docs/DESIGN_SYSTEM.md` is the *how it
looks*. `docs/PROJECT_GUIDE.md` holds the longer per-feature narrative.

> **فارسی:** این نقشهٔ راه برنامه است. هر بخش، کدام فایل، و مالکیت هر لایه.
> پیش از تغییر، مالک همان لایه را پیدا کنید و فقط همان را عوض کنید.

---

## 1. Shape of the product

One statically-linked Windows EXE, no runtime dependencies, no server, no
database engine. Data is flat files under a per-machine data directory
(`dataDir()` in `src/util.cpp`).

Two presentation technologies live side by side:

| | Technology | Screens |
|---|---|---|
| **Native shell** | Win32 + GDI/GDI+, manual RTL | welcome / account select, login, window frame (header · footer · tab strip), settings, calculator, print designer, backup, dialogs |
| **Embedded web** | one HTML page per family, rendered by WebView2 (Chromium) on Win10/11, MSHTML/Trident fallback elsewhere | patient admission, staff dashboard, tools, receipt search, cashier, queue · and separately the management/CRM panel |

The web pages are **fully inlined from embedded resources** and handed to the
engine as a single string — no local server, no ports, no loose files.

---

## 2. Native layer (`src/`)

| Area | Files |
|---|---|
| Entry point, window frame, header/footer, welcome screen, hotkeys | `main.cpp` |
| Theme palette + brushes (`applyTheme`, `g_theme`, `g_dark`) | `theme.cpp`, struct in `app.h` |
| GDI+ drawing helpers (`gpGradRoundRect`, `gpShadow`, `gpLine`, …) | `gdiplus.cpp`, declared in `app.h` |
| Reusable native widgets | `ui_kit.cpp/.h` |
| Reception window + the C++ tab strip (open/close/reorder/overflow) | `reception.cpp` |
| Billing math (authoritative) | `billing.cpp` |
| Cashier tickets, shifts, receipts, department scope | `clinic_ops.cpp/.h` |
| Departments / sections registry | `sections.cpp/.h` |
| Users, permissions, personnel, employees | `users.cpp`, `persons.cpp`, `employees.cpp` |
| Insurance definitions, services catalog | `insurance_defs.cpp`, `services.cpp` |
| Settings (native + per-user) | `settings.cpp`, `user_settings.cpp` |
| Printing + print designer | `printer.cpp`, `print_designer.cpp`, `printer_designer.inc` |
| Backup import/analysis/log | `backup*.cpp/.h` |
| Updater | `update.cpp`, feed at `update/version.txt` |
| Blacklist, saved messages, profile requests, net sync | `blacklist.cpp`, `saved_messages.cpp`, `profile_requests.cpp`, `net_sync.cpp` |

### Global invariants
- `S(v)` is the DPI scale helper — never hard-code pixels in native paint code.
- Paint paths use dirty-rect double buffering; the frame header repaints twice a
  second, so that path must stay allocation-free.
- The dirty-rect buffer is pre-filled with `g_theme.bg` before gradients (fixes a
  1px black scanline at the header's bottom edge).
- Shadow loops are capped at 12 iterations.

---

## 3. Embedded web layer (`assets/`)

### 3.1 The admission bundle — six surfaces, one page

`assets/admission/index.html` is a single document that hosts **six** screens.
Which one is shown is decided by C++ injecting `window.__azSurface`, which
`applySurfaceClass()` turns into exactly one body class:

| Surface string | Body class | Subtree | Opened by |
|---|---|---|---|
| `admission` | `surface-adm` | `#appBody` (three columns) | `TK_RECEPTION` |
| `dash` | `surface-dash` | `#dashPanel` | `TK_DASH` — the permanent staff landing tab |
| `tools` | `surface-tools` | `#toolsPanel` → `#toolsHome` | `TK_TOOLS` |
| `receipts` | `surface-rc` | `#toolsPanel` → `#toolsReceiptsView` | `TK_RECEIPTS` |
| `cashier` | `surface-cash` | `#cashPanel` | `TK_CASHIER` |
| `queue` | `surface-queue` | `#queuePanel` | `TK_QUEUE` |

The surface is chosen in `src/reception.cpp` and injected in
`src/web_admission_embed.inc`. Shared across all six: the loader, the toast, the
confirm dialog, the cancel dialog and the block modal.

### 3.2 Stylesheet ownership (the part that used to rot)

```
common.css  →  admission.css  →  css/core.css  →  css/surface-*.css
 (shell)       (STRUCTURE only)   (design system)   (one file per surface)
```

`admission.css` carries structure and layout only. All colour, elevation and
surface treatment lives in the `css/` layer, one exclusively-owned file per
surface. See `AGENTS.md` §4. **Never append a new override layer.**

### 3.3 Scripts

| File | Role |
|---|---|
| `assets/shell/common.js` | shared runtime (`AzBoot`/`AzBridge`/`AzUi`/`AzNav`/`AzPerf`) |
| `assets/admission/bridge.js` | transport: WebView2 `postMessage` → Trident `window.external.azCall` → dev HTTP |
| `assets/admission/admission.js` | all six surfaces' behaviour. **ES5 only.** |
| `assets/admission/contextmenu.js` | right-click menu |

### 3.4 Management / CRM bundle

`assets/crm/` is a separate inlined page (`src/web_crm*.inc`, RCDATA 800…816)
with its own module per screen: sections, subsections, persons, accounts,
patients, doctors, services, insurance, messages, calendar, backup, settings.
Departments are created here — which is why nothing may hard-code a department
name.

---

## 4. Resource registry (`src/app.rc`)

Every embedded asset has a reserved id range. Ranges must not collide.

| Ids | Contents |
|---|---|
| 1 | application icon |
| 101–104 | Vazirmatn fonts, welcome background art (light/dark) |
| 201–207 | raster action icons + brand logo |
| 300–305 | print designer bundle |
| **400–405** | admission page: html · admission.css · bridge.js · admission.js · font · contextmenu.js |
| **420–424** | **the ordered theme layer** — core, dash, tools, admission, cashier |
| 500–502 | shared shell assets |
| 600–602 | demo «ping» page |
| 700 | WebView2 loader |
| 800–816 | CRM bundle |

Adding a stylesheet means updating **three** places in sync: `app.rc`, the
`kAdThemeLayers` table in `src/web_admission_embed.inc`, and the `<link>` order
in `index.html`. `scripts/check_ui_contract.py` verifies all three agree.

---

## 5. C++ ↔ page bridge

- **Page → C++**: `Bridge.call(verb, payload)`. Verbs are dispatched in
  `src/web_admission_api.inc` (`admissionApi`). Families: `init`, `patient.*`,
  `service.*`, `doctor.*`, `bill.compute`, `admission.save`, `print.*`,
  `queue.*`, `receipt.*`, `cashier.*`, `ui.*`, `portal.unread`.
- **C++ → page**: `WebAdmission_PushEvent(name, json)`; the page subscribes with
  `Bridge.on(name, fn)`. Events: `patient.load`, `services.update`,
  `queue.update`, `ps.update`, `reception.settings`, `native.print`,
  `clock.update`, `insurance.update`, `catalog.update`, `hotkey`, `dash.unread`.
- Theme reaches the page as a **string only** (`theme: "light"|"dark"` on
  `init`) — there is no colour channel from C++ to CSS, and no live re-theme
  event, so an open tab keeps its palette until it reloads.

---

## 6. Data model quick reference

| Concept | Struct | Store |
|---|---|---|
| Saved admission | `ReceptionRecord` (`app.h`) | daily CSV + `last_receipt.dat` |
| Cashier ticket / receipt | `CashTicket` (`clinic_ops.h`) | `cashier_tickets.dat` |
| Cashier till session | `CashShift` (`clinic_ops.h`) | `shifts.dat` |
| Department / sub-department | `Section` (`sections.h`) — `parent_id > 0` = sub | `sections.dat` |
| Login account | `User` (`app.h`) — `dept` is a **name** | `users.dat` |
| Personnel | `PersonDef` (`app.h`) — `deptId`/`subId` are **ids** | `persons.dat` |
| Parked unpaid / admission queues | `AdQueueRow` | `unpaid_queue.dat`, `recept_queue_web.dat` |

Traps worth knowing before touching this area:
- **«صندوق نرفته‌ها» means two different things.** On the cashier page it is
  computed (`CashTicket.paid == 0`); in the queue overlay it is a hand-parked
  list in a different file. They can legitimately disagree.
- **`shift` is overloaded.** `ReceptionRecord::shift` / `CashTicket::shift` are
  the work-period label (صبح/عصر/شب); `CashShift` is the cashier till session.
- A `CashTicket`'s department comes from the **operator's** home section, not
  from the admission form — `ReceptionRecord` has no department id, only a
  free-text name.
- The cashier page only ever shows a **24-hour** window.
- An empty `perms` field means full access (legacy accounts).

---

## 7. Build, guards, release

```
./build.sh                        # resources → C++ → strip → sha256 → UI contract
AZ_SMOKE=1 ./build.sh             # + bounded Wine smokes (designer, admission probe, keys)
python3 scripts/check_ui_contract.py   # the UI contract on its own
./shot.sh home /var/tmp/home.png 1440x900   # native screen screenshot under Wine
python3 scripts/mock_admission_host.py 8788  # serve the HTML surfaces for a real browser
```

Version lives in four places and they must agree: `src/app.h`, `src/app.rc`,
`update/version.txt`, `docs/PROJECT_GUIDE.md`.
