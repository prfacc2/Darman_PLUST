# WORK ORDER 2.07.0 — Deterministic Manifest

> Single source of truth for every address this release touches. Every row
> quotes the exact file/symbol that exists in the repository at commit
> `cffa65e` (v2.06.0, branch `genspark_ai_developer`). New identifiers created
> by this release are frozen in §2 and are used verbatim by later phases.
> No TBD, no guessed ids.

---

## 1. Existing addresses (verified by grep)

### `تنظیمات.ردیف‌ها` — settings rows enumeration + row-drawing loop

The **live** settings panel opened by the header gear button is
`src/user_settings.cpp` (`OpenSettings()` at user_settings.cpp:1204 is called
from main.cpp:973 `ID_FR_SETTINGS`). The legacy `src/settings.cpp`
`openSettingsPanel()` (settings.cpp:656) has no callers, but is still compiled;
both are kept untouched except for the one appended row below.

- Row enumeration: `enum SettingsRow` — user_settings.cpp:53–68
  (`ROW_PROFILE, ROW_THEME, ROW_RECEPTION, ROW_BLACKLIST, ROW_DESIGNER,
  ROW_BACKUP, ROW_EMP_SECT, ROW_SAVED_MSG, ROW_UPDATE, ROW_CONTACT, ROW_ABOUT,
  ROW_LOGOUT, ROW__COUNT`)
- Page ids: `enum PageId` — user_settings.cpp:133–138
- Row table: `homeRows(int mode)` — user_settings.cpp:188–221 (static
  `RowDef ALL[]`, order = display order, filtered by `canAccess`)
- Access gate: `canAccess(int row,int mode)` — user_settings.cpp:74–96
  (`ROW_DESIGNER` → true for all non-guest modes)
- Row geometry: `rowH()` S(70), `rowGap()` S(10), `homeRowRect(sw,idx)` —
  user_settings.cpp:257–268
- Row paint loop: `paintHome()` — user_settings.cpp:743–781
  (`gpShadow`, `gpRoundRectBg`, icon right `r.right-S(48)`, chevron left,
  title `g_fUIB` + subtitle `g_fSmall` in `g_theme.textDim`)
- Row click: `activateRow(sw,idx)` — user_settings.cpp:909–924
- Sub-page builder + buttons: `buildLauncherPage()` user_settings.cpp:515–528
  (`createFlatButton(parent,cmdId,btnText,icon,BS_PRIMARY,x,y,w,S(38))`),
  `buildDesignerPage()` user_settings.cpp:529–551
  (`IDC_PANEL_BASE+60` «باز کردن طراح چاپ» BS_PRIMARY,
   `IDC_PANEL_BASE+65` «تنظیمات چاپ» ICO_GEAR BS_OUTLINE)
- Command dispatch: user_settings.cpp:1097–1098
- Legacy enum `ROW_PRINTER` case: settings.cpp:588 → `openPrinterSettings(g_hFrame)`

### `تنظیمات.ردیف_چاپگر` — existing printer row and dialog call

There is **no printer-link row in the live panel** today. The closest live
entries are `ROW_DESIGNER` (user_settings.cpp:198) with buttons
`IDC_PANEL_BASE+60` → `PrintDesigner_Open(mw)` and `IDC_PANEL_BASE+65` →
`PrintCfg_Open(mw)` (user_settings.cpp:1097–1098). Legacy:
settings.cpp:588 `case ROW_PRINTER: closeSettingsPanel(); openPrinterSettings(g_hFrame); break;`.

### `چاپگر.کلاس` — printer dialog class/state/commands

- `#define PS_CLASS L"AzPrinterCfg"` — printer.cpp:532
- `struct PrnState` — printer.cpp:545–557
- `enum { PSB_CLOSE=1, PSB_TEST, PSB_ADV, PSB_DESIGN, PSB_A4, PSB_A5,
  PSB_P80, PSB_F58, PSB_FIT, PSB_FILL, PSB_SEC_PREV, PSB_SEC_NEXT,
  PSB_COPIES_DN, PSB_COPIES_UP, PSB_SECEN, PSB_AUTOPRINT, PSB_DRAWER,
  PSB_LOGO, PSB_PRINTER_BASE=200 }` — printer.cpp:533–543
- `prnCardW()` S(580) / `prnCardH()` S(820) / `prnCard(HWND)` — printer.cpp:561–568
- Theming: `prnPaint()` printer.cpp:625–824 uses `g_theme`, `g_fTitle/g_fUI/g_fUIB/g_fSmall`,
  `S()`, `gpShadow`, `gpGradRoundRect`, `gpRoundRect`, `gpFillAlpha`, `drawIcon`,
  local `toggleRow` lambda (printer.cpp:690–709), local `chip`/`btn` lambdas.
  `WM_APP_THEME` → `InvalidateRect` (printer.cpp:895).
- `openPrinterSettings(HWND owner)` — printer.cpp:1003–1028 (declared app.h:668)
- `printerRequestGate(h,title,change,payload,preview)` — printer.cpp:43–58
  (returns true → apply directly; false → queued to management via `pushSetReqEx`)

### `چاپگر.شمارش` — printer enumeration / current printer / setting key

- `static std::vector<std::wstring> enumPrinters()` — printer.cpp:487–501
- `static std::wstring currentPrinter()` — printer.cpp:502–508
  (reads `getSetting(L"printer_name",L"")`, else `GetDefaultPrinterW`)
- Setting key: `printer_name` (printer.cpp:503, 918; manage.inc:1124)

### `چاپ.موتور` — design rendering onto a printer DC

- `bool printPrintDesign(const ReceptionRecord& r,int sectionId,HWND owner)` —
  printer.cpp:2555–2834 (the current engine; design resolved via
  `MachineDesign_Resolve()` else `Design_BuiltinTemplate(0)`)
- Paper/DEVMODE: `pdPaperCode()` printer.cpp:2504–2516,
  `pdCreatePrinterDC()` printer.cpp:2517–2553
- mm→device: `sx=dpiX/25.4`, `sy=dpiY/25.4`, `pscale` auto-fit,
  `mmX/mmY` lambdas — printer.cpp:2605–2638
- Pagination: `pdEnsureServicesFrame` (printer.cpp:2683),
  `pdPlanServicePages` (printer.cpp:2060–2069 → `pdSliceServiceRows`)
- Continuation whitelist: `pdContinuationRepeatAllowed()` — print_services_policy.h:11–34
- Item dispatch loop: printer.cpp:2702–2829 (PIT_TABLE, PIT_SERVICES,
  PIT_BARCODE, PIT_HLINE, PIT_VLINE, PIT_RECT/FRAME/LOGO/PHOTO/QR/IMAGE, text)
- Services table: `pdParseServicesModel` printer.cpp:1871–1920,
  `pdBuildServicesLayout` printer.cpp:1997–2059, `pdDrawServices`
  printer.cpp:2152–2275, `pdSvcColOf` printer.cpp:1838–1869
- Barcode: `pdParseBarcodeModel` printer.cpp:2395–2419, `pdDrawBarcode`
  printer.cpp:2421–2502 (HRI text drawn under the bars)
- **No raster stage exists on the print path.** `CreateCompatibleDC`/`BitBlt`
  occur only inside `prnPaint()` (printer.cpp:627, 822) — the on-screen
  settings card, not the receipt engine. There is no `StretchBlt` and no
  `Gdiplus::Bitmap` intermediate in `src/` on the print path.

### `چاپ.توکن‌ها` — field tokens

- `static std::wstring pdNormalizeField(const std::wstring& f)` — printer.cpp:1322–1415
  (bare-name → `{token}` map; `{P-Name}`→`{full}`; unknown → wrapped)
- `static std::wstring pdFieldValue(const ReceptionRecord& r,const std::wstring& tokIn)`
  — printer.cpp:1473–1688 (95 tokens; includes `{specialty}` → `r.specialty`,
  `{doctorcode}` → `r.doctorCode`, `{receiptbarcode}`, `{age}`,
  `{clinicaddr}` → `getSetting(L"clinic_address")`, `{clinicphone}` →
  `getSetting(L"clinic_phone")`)
- Legacy resolver `fieldValue()` — printer.cpp:1042–1113 (subset, same normalizer)
- Bare aliases `clinicaddr`/`clinicphone` already map to the braced forms:
  printer.cpp:1366–1367

### `چاپ.طرح‌های_آماده` — builtin templates

- File: `src/print_designer_templates.inc` (728 lines, included into
  print_designer.cpp)
- `static PrintDesign buildTemplate(int idx)` — templates .inc:636–644
- `void Designs_Init()` — templates .inc:656–727 (idempotent seeding +
  migrations `tpl_migration_2_00`, `tpl_migration_2_06`)
- `PrintDesign Design_BuiltinTemplate(int idx){ return buildTemplate(idx); }`
  — templates .inc:729
- Names: `TPL_NAMES[31]` — templates .inc:484–516 (index 0 = `L"پیش‌فرض"`,
  1..30 extras), `buildTemplateName(idx)` templates .inc:646–650
- Composers: `mkLabel/mkField/mkFieldB/mkHLine/mkVLine/mkRect/mkFrame/
  mkFrameBox/mkLogo/mkBarcode/mkPhoto/mkServices` templates .inc:20–114,
  emit blocks `emitHeader/emitTitle/emitBarcode/emitSamenInfo/
  emitSamenInfoSplit/emitSamenFinance/emitDualFinance/emitSamenRef/
  emitSamenFooter/emitStub` templates .inc:165–391
- Services model JSON: `svcModelJson(int)` templates .inc:92–98
  (`{"cols":3,"header":true,"widths":[0.34,0.10,0.56],
    "labels":["نام خدمت","تعداد","شرح خدمت"]}`)
- Barcode model: `mkBarcode` templates .inc:64–69
  (`{"sym":"code128","hri":true,"quiet":2}`)
- Default builtin: `buildSamenDefault()` templates .inc:402–438 (R80 80×200)

### `چاپ.جدول_خدمات` — services table model + renderer

- Model struct `PdServicesModel { int cols=4; bool header=true;
  std::vector<double> widths; std::vector<std::wstring> labels; }` —
  printer.cpp:1809–1815
- `enum PdSvcCol` — printer.cpp:1820–1836; `pdSvcColOf` printer.cpp:1838–1869
- `pdParseServicesModel` printer.cpp:1871–1920; layout
  `pdBuildServicesLayout` printer.cpp:1997–2059; renderer
  `pdDrawServices` printer.cpp:2152–2275
- Policy header: `src/print_services_policy.h` (34 lines)
- Pagination header: `src/print_services_pagination.h` (102 lines)

### `پذیرش.سطح_html` — admission surface DOM ids

- Surface class applied at runtime: `applySurfaceClass()` admission.js:4738–4757
  (`body.surface-adm`); markup root `#app`/`#appBody` index.html:26–355
- «نوع بیمه» label + select: index.html:172
  `<label class="fld"><span>نوع بیمه</span><select id="insType" class="inp" data-nav>…</select></label>`
  (inside `.ins-card` index.html:166–176; sibling `#insMain` «بیمه پایه»
  index.html:171, `#ptype` «نوع پذیرش» :173, `#insSuppPct` «درصد بیمه تکمیلی» :174)
- Hidden supplementary select: `#insSupp` index.html:298
  (inside `#insExtraHidden` index.html:293–299)
- پزشک معالج combo: `<select id="doc2name" class="inp" data-nav>` index.html:191
  (`.doc-card` index.html:178–194; search input `#doc2code` :185,
  suggestions `#docResults` :189)
- انجام‌دهنده search input: `#perfcode` index.html:203 (`.perf-card`
  index.html:196–212; suggestions `#perfResults` :207; select `#perfname` :209)
- خدمات پذیرش header: `.card-head` block index.html:242–254
  (title `.card-title` + inline SVG `.ct-ico` :243–245, tools `.head-tools` :246,
  add button `#svcAddBtn` :251, toggle `#svcToggle` :253)
- خدمات پذیرش search input: `#svcSearch` index.html:248
  (`.svc-search-wrap` container :247)
- خدمات پذیرش list body: `#svcBody` index.html:275
  (table wrapper `#svcTblWrap` :258, suggestions `#svcSuggest` :256,
  totals `#svcFoot` :278)

### `پذیرش.css` — admission-scoped rules

- Doctor/performer selects: `select.inp#doc2name,select.inp#perfname{overflow:auto;
  scrollbar-width:none;…}` admission.css:281–286; size-mode height
  `#doc2name[size…] { height:auto; z-index:8; }` admission.css:7801–7804
- Shared input metrics: `.inp{display:block;width:100%;height:38px;padding:0 11px;…}`
  admission.css:250–…; `.inp-sm{height:33px;…}` :270; compact overrides
  `.inp{height:39px;padding:0 12px;…}` :1194–1200, `.inp-sm{height:34px;…}` :1210,
  `.inp{height:34px;font-size:13px;}` :2291
- Performer search input = `.with-ico .inp{padding-left:32px;}` admission.css:291
- Services search box: `.svc-search-wrap{width:214px;}` admission.css:531;
  later passes `width:260px` :1913, `width:205px` :2092,
  `width:auto;flex:1 1 auto;` :2104; `.svc-search-wrap .inp{padding-left:30px;}` :533
- Surface-scoped input styling: `body.surface-adm .inp{…}` admission.css:4101
  and `body.surface-adm .inp,body.surface-tools .inp,…` :4391–4398;
  light-theme pass :5959–5963
- Card head accents: `… .surface-adm .svc-card > .card-head{…}` admission.css:6021
- Density mechanism: **no CSS variables** — literal px, append-only passes;
  zoom is JS-driven on `#appBody` (admission.js:2137–2186). Trident-safe rule:
  no `var()`, only one legacy `calc()` at admission.css:1865 (untouched).

### `پذیرش.js` — admission JS wiring

- Insurance select population: `init` verb → `fillSelect($('insMain'),r.insurances)`
  and `fillSelect($('insSupp'),r.supp)` admission.js:4884–4885; live push
  `Bridge.on('insurance.update',…)` admission.js:3462–3466
- `suppInsPct()` admission.js:256–266 (explicit `#insSuppPct` wins, else
  `state.supp[selectedIndex].pct`); `baseInsPct()` :250–255
- Per-row split `computeRow` admission.js:299–321; totals `recompute` :362–408;
  authoritative `bill.compute` :323–360
- Services search wiring: `svcSearch()` admission.js:1384–1391 (debounced 180 ms
  `service.search`), Enter resolve :1412–1474, `#svcAddBtn` :1485–1487,
  delegated `#svcSuggest` clicks :1499–1513, `renderSvcSuggest` :995–1013
- Save response/mini-kind handling: `doAdmissionSave` admission.js:3255–3295
  (`mainPay` → `openPayMini`, `subTicket`/`needsCashier` → `openTicketMini`)
- `openTicketMini` admission.js:2996–3009; `#ticketMiniPrint` handler
  admission.js:3094–3101 (`receipt.print` verb)

### `بیمه.مکمل` — supplementary insurance registry

- `struct SuppDef` — app.h:419–447 (fields incl. `idx`, `name`,
  `franchiseOrgPct`, `tariffType`, `franchise`, `ceiling`, `color`, `active`)
- `std::vector<SuppDef> loadSuppDefs()` — insurance_defs.cpp:118–160
  (store `data\suppdefs.dat`, pipe-delimited)
- `int Supp_Percent(int idx)` — billing.cpp:54–60
  (honours `SuppDef::franchiseOrgPct` before `SUPP_INSURANCES[idx].pct`)
- JSON builders: `insurancesJson()` / `suppInsurancesJson()` —
  web_admission_api.inc:71–113 (used by `init` verb, :661–665)
- CRM page: `assets/crm/insurance.js` (form ids `cTariff` :188, `cFranchise` :190,
  `cCeiling` :192, `cOrgPct` :198, `cColor` :203, `cActive` :224; save verb
  `crm.supp.save` payload :284–318)
- AdBill parse: `adComputeBill` web_admission_api.inc:507–519
  (`insSupp` → `b.suppIdx`, `insSuppPct` → `b.suppPct`)

### `بخش‌ها.زیربخش` — sections model

- `struct Section` — sections.h:11–39 (`id, code, name_fa, kind, is_active,
  created_at, updated_at, net_meta, parent_id(9th), cashier_tab(10th),
  has_pos(11th)`)
- Serializer column order — sections.cpp:44–67 (load) / 90–110 (save):
  `id|code|name_fa|kind|is_active|created_at|updated_at|net_meta|parent_id|
  cashier_tab|has_pos`
- `bool Sections_HasPos(int id)` — sections.cpp:275–280 (no parent inheritance)
- `Sections_Find` — sections.cpp:180–193 (uses `uikit::NormalizeFa`)
- `uikit::NormalizeFa` — ui_kit.h:97–101, ui_kit.cpp:74–107
- CRM sections page: `assets/crm/sections.js` — sub-section modal
  `openSubModal` :249–298 (ticks `gActive` :264, `gCashier` :265,
  `gHasPos` :266; payload keys `active/cashierTab/hasPos` :279–281);
  verbs `crm.sections.list/save/delete` (:17, :164, :293)

### `صندوق.مدل` — cash register model

- `struct CashScope` — clinic_ops.h:44–52; `CashTicket` :10–28; `CashShift` :30–42
- `Cash_ResolveScope()` — clinic_ops.cpp:101–166
- `Cash_CreateFromReception` — clinic_ops.cpp:296–361
  (POS decision :316–319, unpaid routing :347–351)
- `Cash_PayEx` — clinic_ops.cpp:534–591 (stamps `paidUser` = username :584,
  `shiftAddIncome` :589)
- `Receipt_BuildRecord` — clinic_ops.cpp:919–964
- Pay verb: `cashier.pay` web_admission_api.inc:1477–1503
- Ticket mini panel: `#ticketMini` index.html:888–905, `openTicketMini`
  admission.js:2996–3009

### `هدر.حساب_کاربری` — header identity painter + session title

- Header painter: main.cpp:1105–1153 (name `g_fUIB` + title line `g_fSmall`
  in `g_theme.textDim`, `mainBarH()` = S(60))
- `std::wstring resolveSessionTitle(const User& u)` — persons.cpp:171–201
  (precedence: `PersonDef.position` → `personRoleLabel(p)` → `EmpProfile.position`
  → role fallback → reception detection)
- Cache: `g_session.title=resolveSessionTitle(u)` set at login
  (main.cpp:506, 520, 533, 991)
- `struct Session { User user; int shift; SYSTEMTIME loginAt; std::wstring title; }`
  — app.h:567–575
- Repaint timer: `TIMER_CLOCK` 500/1000 ms — main.cpp:899, 924–933

### `نسخه` — version triple

- `#define APP_VERSION_W L"2.06.0"` — app.h:23
- `src/app.rc` FILEVERSION/PRODUCTVERSION + `"FileVersion"`/`"ProductVersion"`
- `update/version.txt` — `2.06.0` + release download URL

### Print entry points / scope (needed by Phase 7)

- `printPrintDesign(r, sectionId, owner)` callers: reception.cpp:3303,
  billing.cpp:506, web_admission_api.inc:1089, 1178, 1274, 1496
- Section resolution: `recResolveSectionId()` reception.cpp:2745–2768;
  `adResolveSectionId()` web_admission_api.inc:265–278
- Designer entry points: `PrintDesigner_Open` (print_designer.h:213,
  print_designer_ui.inc:1738) and `PrintCfg_Open` (print_designer.h:219,
  manage.inc:5422) — gated by `canAccess(ROW_DESIGNER, mode)`
  user_settings.cpp:89 and `PG_PRINTCFG` manage.inc:34
- Mini-kind routing: web_admission_api.inc:1299–1319
  (`mainRec`/`posHas` → `mainPay` / `subTicket`)
- Doctors store: `struct DoctorDef` app.h:1068–1106 (`specialty` :1070,
  `medicalId` نظام پزشکی :1078); `loadDoctors()` data_ext.cpp:525–551
- Specialty capture today: reception.cpp:1856–1868 (native path only);
  web path does NOT populate `r.specialty` (web_admission_api.inc:566–568)

---

## 2. Frozen new identifiers for release 2.07.0

All later phases use exactly these names. Nothing else is invented.

### 2.1 Phase 2 — «ارتباط با چاپگر» dialog

| Identifier | Value / kind |
|---|---|
| Window class macro | `#define PL_CLASS L"AzPrinterLink"` (in `src/printer.cpp`) |
| State struct | `struct PrinterLinkState` with members `owner`, `hot`, `همه_چاپگرها` (`std::vector<std::wstring>`), `نتایج_جستجو` (`std::vector<int>`), `عبارت_جستجو` (`std::wstring`), `چاپگر_انتخابی` (`std::wstring`), `چاپگر_ویندوز` (`std::wstring`), `پیرو_ویندوز` (`bool`), `اسکرول` (`int`) |
| Command enum | `PLB_بستن = 400, PLB_تأیید, PLB_انصراف, PLB_تست_اتصال, PLB_بازخوانی, PLB_پیش‌فرض_ویندوز, PLB_ITEM_BASE = 500` |
| Entry point | `void PrinterLink_Open(HWND owner);` declared in `src/app.h` next to `openPrinterSettings` (app.h:668 block) |
| Settings row (live panel) | `ROW_PRINTER_LINK` appended to `enum SettingsRow` (user_settings.cpp:53–68) — never reordering existing rows |
| Row title | `L"ارتباط با چاپگر"` |
| Row subtitle | `L"انتخاب چاپگر پیش‌فرض برنامه"` |
| Row icon | `ICO_PRINT` (app.h:123 — reused, no new icon) |
| Settings keys | `printer_name` (existing) and `printer_follow_windows_default` (new; `"1"`/`"0"`, default `"1"`) |
| Approval gate | `printerRequestGate(h, L"ارتباط با چاپگر", L"تغییر چاپگر برنامه", L"printer_name=<name>;printer_follow_windows_default=<0|1>", L"چاپگر: <name>")` |
| Persian UI strings | `L"جستجوی چاپگر"`, `L"نام چاپگر را بنویسید…"`, `L"لیست چاپگرها"`, `L"پیش‌فرض ویندوز"`, `L"پیرو پیش‌فرض ویندوز"`, `L"تست اتصال"`, `L"بازخوانی"`, `L"تأیید"`, `L"انصراف"` |
| Normalizer | `uikit::NormalizeFa` (ui_kit.h:97–101) — case-insensitive + Persian-normalized substring filter |

### 2.2 Phase 3 — data model, tokens, policy

| Identifier | Value |
|---|---|
| `ReceptionRecord` appends (end of member list) | `std::wstring certNo;` `std::wstring receiptTitle;` `std::wstring clinicName, clinicAddr, clinicPhone;` |
| New tokens | `{certno}` (`r.certNo`), `{receipttitle}` (`r.receiptTitle`), `{clinicname}` (settings `clinic.name`, fallback `APP_NAME_W`), `{clinicaddr}` (existing, settings `clinic_address`), `{clinicphone}` (existing, settings `clinic_phone`) |
| Bare aliases to add | `certno`, `receipttitle`, `clinicname` in `pdNormalizeField` (the `clinicaddr`/`clinicphone` aliases already exist) |
| De-dup guard | `bool pdBarcodeValueAlreadyRendered(bool hriRendered, const std::wstring& normalizedField);` in `src/print_services_policy.h` |
| Services normalizer | `PdServicesModel pdNormalizeServicesModel(const PdServicesModel& m, bool builtin);` clamps builtins to exactly `{"cols":3,"header":true,"widths":[0.46,0.12,0.42],"labels":["نام خدمت","تعداد","شرح خدمت"]}` |
| Adaptive fractions | `نام خدمت 0.46`, `تعداد 0.12`, `شرح خدمت 0.42` (base); desc-empty → desc `0.18` (name grows); long desc → desc up to `0.52` (name only shrinks; `تعداد` fixed) |
| Ink palette | `INK #000000`, `ACCENT #0B3D91`, `DANGER #A31212`, `MUTED #333333`; text luminance ceiling `0.62`; hairline min `0.30 mm` / `#333333` |
| Elastic band | `بلوک_پایانی` (trailing block below `PIT_SERVICES`); breathing room `4.0 mm` (A4/A5/A6/B5/Letter), `2.5 mm` (R80/R58) |
| Name item contract | `dir = 0`, `align = 0`, `valign = 1`, single-line shrink-to-fit (`DT_SINGLELINE|DT_NOCLIP`), 0.5 pt steps, floor 70 % |

### 2.3 Phase 4 — 30 templates + composer

| Identifier | Value |
|---|---|
| Composer struct | `struct طرح_پارامتر { const wchar_t* کد; const wchar_t* نام; const wchar_t* کاغذ; int جهت; int چیدمان; int تأکید; double قلم_پایه; };` |
| Composer | `static PrintDesign بساز_طرح(const طرح_پارامتر& p);` — the ONLY producer of builtin templates |
| Codes | `T01`…`T30` (frozen; `T01` stays the fallback); `TB1` = «برچسب بارکد زیربخش» (not one of the 30) |
| Migration flag | `tpl_migration_2_07` |
| Display names | exactly the 30 rows of the §4.2 table (below) |

Template display names (frozen):
```
T01 طرح پیش‌فرض حرفه‌ای        T16 طرح جدول‌محور
T02 طرح ساده و سریع            T17 طرح دو ستونهٔ فشرده
T03 طرح رسمی کادردار           T18 طرح سربرگ‌دار بلند
T04 طرح فشردهٔ A5              T19 طرح رسید نقدی
T05 طرح سربرگ رنگی مدرن        T20 طرح رسید کارتخوان
T06 طرح شمارهٔ نوبت درشت        T21 طرح بیمهٔ پایه
T07 طرح رسید پرداخت            T22 طرح بیمهٔ تکمیلی
T08 طرح دو ستونهٔ شیک          T23 طرح آزمایشگاه
T09 طرح مینیمال خط‌دار          T24 طرح رادیولوژی
T10 طرح کارت بیمار             T25 طرح تزریقات
T11 طرح نواری ۸۰ میلی‌متر       T26 طرح داروخانه
T12 طرح نواری ۵۸ میلی‌متر       T27 طرح فیزیوتراپی
T13 طرح افقی A5                T28 طرح نسخهٔ پزشک
T14 طرح افقی A4                T29 طرح صورتحساب تفصیلی
T15 طرح بارکدمحور              T30 طرح خلاصهٔ مدیریتی
```

### 2.4 Phase 5 — admission surface

| Identifier | Value |
|---|---|
| Label text change | `نوع بیمه` → `بیمه تکمیلی` (element id `insType` unchanged) |
| New bridge verb 1 | `بیمه_تکمیلی_فهرست` — returns per active `SuppDef`: `idx`, `name`, `franchiseOrgPct`, `tariffType`, `franchise`, `ceiling`, `color` |
| New bridge verb 2 | `بیمه_تکمیلی_انتخاب` — body `{idx}`; returns the resolved `pct` (via `Supp_Percent(idx)`) + name so the form can display the authoritative percentage live. The persisted `suppIdx`/`suppPercent` flow through the save payload (`insSupp`/`insSuppPct`) exactly as before — this verb is the live-echo half of the one path. |
| CSS scoping | only `body.surface-adm`-prefixed rules; Trident-safe literals |
| Services search width | `+28px` from the right edge at base density (appended `body.surface-adm` rule) |
| Header click-to-focus | listener on the خدمات پذیرش `.card-head` root only; `cursor: text` on background/title |

### 2.5 Phase 6 — header account type

| Identifier | Value |
|---|---|
| Label function | `std::wstring Sections_AccountRoleLabel(int sectionId);` (Phase 7 helper, used by the header) |
| Precedence | personnel `position` → employee-profile role → `Sections_AccountRoleLabel` → access-level fallback (`پذیرش`/`مدیریت`/`کارآموز`) |
| Paint contract | name `g_fUIB` line 1, title `g_fSmall` `g_theme.textDim` line 2; resolved once at login into `Session::title` |

### 2.6 Phase 7 — زیربخش پذیرش

| Identifier | Value |
|---|---|
| `Section` 12th column | `int زیربخش_پذیرش;` (after `has_pos`), serializer key `recept_sub`, default 0 |
| Accessor | `bool Sections_IsReceptionSub(int id);` (no parent inheritance) |
| Label helper | `std::wstring Sections_AccountRoleLabel(int sectionId);` → `پذیرش <parent name_fa>` for a reception sub, `پذیرش` for a top-level reception section |
| CRM tick | «زیربخش پذیرش» in `openSubModal` (assets/crm/sections.js:249–298), visible only when `parentId > 0`, default OFF; payload key `receptSub` |
| Barcode-only builtin | code `TB1`, name `L"برچسب بارکد زیربخش"`, reduced block set: `{clinicname}`, `{receipttitle}`, `PIT_BARCODE` hri:true, `{P-Name}`, `{nid}`, `{queue}`, `{datetime}` |
| Inline notice | `L"پرداخت در بخش پذیرش انجام می‌شود"` |
| Routing rule | in `printPrintDesign`: acting scope = reception-sub without POS ⇒ select `TB1` regardless of machine/section binding |

### 2.7 Phase 8 — version

| Place | New value |
|---|---|
| `src/app.h` | `APP_VERSION_W L"2.07.0"` |
| `src/app.rc` | `FILEVERSION 2,7,0,0`, `PRODUCTVERSION 2,7,0,0`, `"FileVersion" "2.07.0"`, `"ProductVersion" "2.07.0"` |
| `update/version.txt` | `2.07.0` + `https://github.com/prfacc2/Darman_PLUST/releases/download/v2.07.0/DarmanPlus.exe` |

No RCDATA resources are added by this release; the reserved 900–909 block is
recorded in `docs/ARCHITECTURE.md` §3 as free for future use. No existing
RCDATA id changes.
