# درمان پلاس — Design System «Clinical Slate»

Authoritative visual system for `Darman_PLUST` (Persian / RTL clinic reception & management,
Windows desktop). Every future UI change — HTML surfaces **and** native GDI screens — must be
derived from this document. Values are literal hex; nothing here needs `var()`, `gap`,
`backdrop-filter` or logical properties.

Design intent, in one line: **a premium professional medical product — tinted, layered, lit
surfaces with near-black ink; depth comes from light and shadow, colour comes from one hue per
panel, never from a rainbow.**

**Status:** authoritative since v1.91.0. This document is the single source of truth for
colour, elevation, typography and component recipes across the whole product — the embedded
HTML surfaces (`assets/admission/`, `assets/crm/`) *and* the native GDI screens (`src/*.cpp`).

**Where the values live in code**
| Layer | File |
|---|---|
| shared HTML design system | `assets/admission/css/core.css` |
| dashboard | `assets/admission/css/surface-dash.css` |
| tools + receipt search | `assets/admission/css/surface-tools.css` |
| admission form | `assets/admission/css/surface-admission.css` |
| cashier «صندوق» | `assets/admission/css/surface-cashier.css` |
| native GDI palette | `src/theme.cpp` (`applyTheme`) |

`assets/admission/admission.css` carries **structure and layout only** — no theme colours.
Never append a new dated "polish pass" layer anywhere; edit the one layer that owns the thing
you are changing. `scripts/check_ui_contract.py` enforces this and runs as part of `./build.sh`.

---

## 0. Non-negotiable engine constraints (FORBIDDEN list)

Both engines must render identically: **WebView2 (Chromium)** on Win10/11 and **MSHTML /
Trident (IE11-era)** on older Windows.

Never use:

| Forbidden | Use instead |
|---|---|
| `var()` / CSS custom properties | literal hex / px values (this doc is the token registry) |
| flex/grid `gap` | real `margin` on children |
| `backdrop-filter`, `filter: blur()` | the **faked-glass recipe** (§5) |
| CSS logical properties (`padding-inline-*`, `margin-inline-*`, `inset-*`) | physical properties, authored for RTL |
| CSS grid for layout | flexbox with `-ms-` prefixes (`-ms-flexbox`, `-ms-flex`, `-ms-flex-align`, `-ms-flex-pack`) |
| webfonts, `@import`, CDN, network assets, base64 images | bundled `Vazirmatn` only (`vazir.ttf`), CSS/inline-SVG graphics |
| SVG filters, `mix-blend-mode`, `clip-path`, `mask` | layered gradients + borders + shadows |
| `hsl()`, `color-mix()`, relative colour syntax | literal hex, `rgba()` for translucency only |
| CSS transitions on `box-shadow` chains longer than 2 layers | transition `border-color`, `background`, `transform` only |
| violet / purple / magenta / pink / cream / beige anywhere | the six approved hues (§2) |

Always: declare a **flat hex `background:` first, then the gradient** (`-webkit-linear-gradient`,
then standard `linear-gradient`) so Trident keeps a sane fill if it drops a gradient.

---

## 1. Neutral palette

### 1.1 Canvas layers (page background — never plain white, never dark)

Each surface family has its own three-stop canvas. Canvas is *tinted and lit*, ink stays dark.

| Token | Hex | Use |
|---|---|---|
| `canvas-adm-hi` | `#E6EDF7` | admission page gradient, light corner (top-right in RTL) |
| `canvas-adm-mid` | `#D6E0EF` | admission page gradient, middle |
| `canvas-adm-lo` | `#C4D2E5` | admission page gradient, deep corner |
| `canvas-dash-hi` | `#DCE6F4` | dashboard gradient, light corner |
| `canvas-dash-mid` | `#C6D4EA` | dashboard gradient, middle |
| `canvas-dash-lo` | `#B2C3DE` | dashboard gradient, deep corner |
| `canvas-tools-hi` | `#D2DBE6` | tools/cashier gradient, light corner (cool steel, **no warmth**) |
| `canvas-tools-mid` | `#BCC8D8` | tools/cashier gradient, middle |
| `canvas-tools-lo` | `#A8B7CB` | tools/cashier gradient, deep corner |

Canvas decoration (all three canvases, low-alpha only — this is what makes the page look
*designed* instead of a white sheet):

| Token | Value | Use |
|---|---|---|
| `glow-indigo` | `rgba(53,80,216,.16)` | radial glow, top-right |
| `glow-azure` | `rgba(14,127,192,.14)` | radial glow, bottom-left (dashboard/admission) |
| `glow-teal` | `rgba(15,139,141,.14)` | radial glow, bottom-left (tools/cashier) |
| `weave` | `rgba(19,38,72,.045)` | 1px diagonal hairline weave, `repeating-linear-gradient(135deg, …0 0 1px, transparent 1px 13px)` |
| `scrim-photo` | `rgba(228,236,248,.34)` | scrim over the welcome photograph |

### 1.2 Surface / elevation tiers

| Token | Hex | Gradient | Use |
|---|---|---|---|
| `surface-1` | `#FFFFFF` | `#FFFFFF → #F5F8FD` | cards, panels — the reading surface |
| `surface-2` | `#F2F6FC` | `#F7FAFE → #EEF3FB` | nested wells, table body, inner bordered regions |
| `surface-3` | `#E8EFF9` | `#EEF4FC → #E2EAF6` | strips: table head, totals foot, counter bars |
| `surface-glass` | `#EEF3FB` | see §5 | headers, trays, launchers floating over canvas |
| `surface-sunken` | `#DFE7F3` | — | scroll gutters, empty-state plates |

### 1.3 Ink tiers (dark ink on light surfaces — legibility beats decoration)

| Token | Hex | Role |
|---|---|---|
| `ink-title` | `#10203A` | screen titles, hero numbers, table numeric values |
| `ink-body` | `#1D2B42` | field values, table text |
| `ink-label` | `#3B4C69` | field labels, table head, kickers that must be read |
| `ink-muted` | `#67768F` | meta, counters, footer, placeholder-adjacent text |
| `ink-placeholder` | `#8C99AC` | input placeholder only |
| `ink-disabled` | `#93A0B4` | disabled control text |
| `ink-on-accent` | `#FFFFFF` | text on a filled accent/semantic button |

Minimum contrast: label/value ink on `surface-1` or `surface-2` only. Never place `ink-muted`
on a hue tint deeper than the `-tint` step.

### 1.4 Border tiers

| Token | Hex | Use |
|---|---|---|
| `border-hair` | `#E1E8F2` | dividers *inside* a card (row separators, head bottom on white) |
| `border-line` | `#C6D2E4` | card/panel edge, table grid lines |
| `border-strong` | `#9FB1CD` | input edge, emphasised container edge, tray ring |
| `border-ring` | `rgba(255,255,255,.92)` | inner light rim (1px, inside a glass/tray edge) |
| `border-hue` | hue `-base` at 34% over the tint | edge of a hue-tinted chip: use the listed `-tintEdge` hex |

---

## 2. Accent and semantic hues

Every hue ships **tint → tintEdge → base → deep**. A panel tinted with its own hue uses
`-tint` for the header wash, `-tintEdge` for the chip border, `-base` for the accent line and
icon chip glyph, `-deep` for the header title ink. Because all four steps come from one hue,
a tinted panel can never clash with itself.

| Hue | tint | tintEdge | base | deep | Meaning |
|---|---|---|---|---|---|
| **indigo** (primary) | `#E7ECFD` | `#BCC8F4` | `#3550D8` | `#22349C` | brand, primary action, patient identity, services workspace, «عملیات پذیرش» |
| **azure** (info) | `#E3F1FB` | `#AFD4EE` | `#0E7FC0` | `#0A4A72` | informational panels: insurance, performer, read-only reference |
| **teal** (clinical) | `#DEF1F1` | `#A7D8D9` | `#0F8B8D` | `#095A5C` | clinical actors: treating doctor, department/section identity, «تب جدید» |
| **emerald** (money & success) | `#E1F4EA` | `#A9DCC6` | `#0E9C77` | `#075E45` | invoice, cash, totals, paid/confirmed state |
| **amber** (warning / pending) | `#FBF1DB` | `#E6CE93` | `#C4890F` | `#6D4A05` | not-yet-paid, waiting queue, expiring |
| **red** (danger) | `#FBE8E8` | `#EEBDBD` | `#D24343` | `#8B2020` | destructive, blocked, refund |
| **slate** (neutral) | `#E7EDF6` | `#C0CDE0` | `#55688A` | `#2C3D5C` | panels with no semantic meaning (print, status, misc) |

Money emphasis extras (only for financial figures):

| Token | Hex | Use |
|---|---|---|
| `money-hero-ink` | `#064E3B` | the hero «مبلغ نهایی» number |
| `money-surface` | `#F1FAF5` | hero card body |
| `money-surface-edge` | `#8FD1B4` | hero card 2px border |
| `money-rule` | `#0E9C77` | hero card accent line and separators |

**Forbidden hues:** violet/purple (`#7C56E4`, `#8B5CF6`, …), magenta, pink, cream/beige
(`#F5D90A` washes, `#E8DCC8`, `#D4C4AE`). These are the exact families the owner rejected.
`g_infoAccent` in `theme.cpp` must move off violet to **azure** `#0E7FC0` (see §11).

### 2.1 The one-hue-per-panel rule (this is why «عملیات پذیرش» can never be purple-on-green)

1. **A panel owns exactly one hue.** Its header wash (`-tint`), its accent mini line (`-base`),
   its icon chip (tint fill + `-tintEdge` border + `-base` glyph) and its header title ink
   (`-deep`) are all derived from that single hue. No second hue may enter the header.
2. **Hue is assigned by meaning, not by variety.** Reusing one hue on three panels is correct;
   inventing a fourth hue to "separate" panels is wrong. Separation comes from elevation,
   border and spacing — not from colour.
3. **The panel body stays neutral.** `surface-1` / `surface-2` with neutral ink. Hue appears
   only in: header wash, mini line, icon chip, focused/active states, and hue-coded values
   (money in emerald ink, warning counts in amber ink).
4. **A panel's primary button must be the panel's hue** — or the global indigo primary. It may
   never be a third hue. `«ثبت قبض و صدور»` is always indigo, therefore
   **«عملیات پذیرش» is an indigo panel** (indigo header, indigo mini line, indigo chip, indigo
   submit). Its secondary buttons are neutral outline; its destructive button is red *because
   red is a state, not a decoration* — destructive is the single allowed exception, and only on
   the button itself (never on the header, chip or line).
5. **Fixed assignments (do not re-hue without updating this table):**

| Panel | Hue |
|---|---|
| کارت بیمار (profile) · مشخصات بیمار · خدمات پذیرش · عملیات پذیرش · اقدامات اصلی | indigo |
| اطلاعات بیمه و نوع پذیرش · انجام دهنده · جستجوی قبض | azure |
| پزشک معالج · بخش/دپارتمان · تب جدید | teal |
| صورتحساب · مبلغ نهایی · صندوق · شیفت باز | emerald |
| صندوق نرفته‌ها · صف پذیرش · پرداخت‌نشده | amber |
| حذف/لغو/استرداد/مسدود | red |
| چاپ اسناد · وضعیت اتصال · ابزارهای عمومی | slate |

---

## 3. Elevation & shadow recipes

Multi-layer `box-shadow` is safe on both engines (IE9+). Always emit the `-webkit-box-shadow`
twin first. Never animate more than the listed pairs.

| Token | Value |
|---|---|
| `sh-1` (resting card) | `0 1px 2px rgba(16,32,58,.06), 0 3px 8px rgba(16,32,58,.07)` |
| `sh-2` (raised panel / tray) | `0 2px 4px rgba(16,32,58,.07), 0 10px 22px rgba(16,32,58,.12)` |
| `sh-3` (floating header / hero) | `0 3px 6px rgba(16,32,58,.08), 0 20px 40px rgba(16,32,58,.16)` |
| `sh-4` (modal / welcome hero) | `0 6px 12px rgba(16,32,58,.10), 0 34px 64px rgba(16,32,58,.22)` |
| `sh-inset-top` (top light) | `inset 0 1px 0 #FFFFFF` |
| `sh-inset-rim` (glass rim) | `inset 0 1px 0 rgba(255,255,255,.95), inset 0 -1px 0 rgba(19,38,72,.06)` |
| `sh-icon` (app-icon badge) | `0 8px 18px rgba(16,32,58,.26), inset 0 2px 2px rgba(255,255,255,.45), inset 0 -3px 6px rgba(0,0,0,.14)` |
| `sh-glow-tray` (welcome tray only) | `0 0 0 1px rgba(255,255,255,.80), 0 24px 50px rgba(18,38,74,.26), 0 0 60px rgba(166,198,240,.55)` |
| `sh-focus` | `0 0 0 3px rgba(53,80,216,.22)` |
| `sh-row-hover` | `0 2px 6px rgba(16,32,58,.10)` |

Elevation ladder (what sits on what): canvas → `sh-2` glass header / tray → `sh-1` cards inside
a tray → `surface-2` wells inside a card → `sh-3` only for the one floating element per screen
(page header) → `sh-4` only for modals and the welcome hero.

**Glow is reserved.** Coloured glow (`sh-glow-tray`) appears on the welcome tray and nowhere
else. App icons get `sh-icon` (a shadow), never a coloured halo/plate.

---

## 4. Section-header recipe (with the thin short accent line)

The owner wants a *designed* header per panel with a **mini accent line**, not a full-width
rule across the card top. `inset 0 3px 0 <hue>` across the whole head is **banned**.

```html
<div class="ph ph-indigo">
  <span class="ph-side">
    <span class="ph-chip"><svg viewBox="0 0 24 24" width="15" height="15">…</svg></span>
    <span class="ph-stack">
      <span class="ph-title">مشخصات بیمار</span>
      <i class="ph-line" aria-hidden="true"></i>
    </span>
  </span>
  <span class="ph-tools">…</span>
</div>
```

```css
.ph{                                 /* header strip */
  display:-ms-flexbox;display:flex;-ms-flex-align:center;align-items:center;
  -ms-flex-pack:justify;justify-content:space-between;
  padding:9px 13px 8px;border-bottom:1px solid #E1E8F2;
  background:#F3F7FD;                                   /* flat fallback first */
  background:-webkit-linear-gradient(top,#FFFFFF 0%,#EFF4FC 100%);
  background:linear-gradient(180deg,#FFFFFF 0%,#EFF4FC 100%);
}
.ph-chip{                            /* icon chip — hue tint + hue edge + hue glyph */
  display:-ms-inline-flexbox;display:inline-flex;
  -ms-flex-align:center;align-items:center;-ms-flex-pack:center;justify-content:center;
  width:26px;height:26px;border-radius:8px;margin-left:8px;   /* RTL: chip on the right */
  -webkit-box-shadow:inset 0 1px 0 #FFFFFF;box-shadow:inset 0 1px 0 #FFFFFF;
}
.ph-title{display:block;font-size:13.5px;font-weight:800;letter-spacing:.1px;}
.ph-line{                            /* THE MINI LINE — short, thin, hue, never full width */
  display:block;width:26px;height:3px;border-radius:2px;margin-top:4px;
}
```

Per-hue variant (indigo shown; repeat verbatim per hue with §2 values):

```css
.ph-indigo{
  background:#EFF3FE;
  background:-webkit-linear-gradient(top,#FFFFFF 0%,#E7ECFD 100%);
  background:linear-gradient(180deg,#FFFFFF 0%,#E7ECFD 100%);
  border-bottom:1px solid #BCC8F4;
}
.ph-indigo .ph-chip{background:#E7ECFD;border:1px solid #BCC8F4;color:#3550D8;}
.ph-indigo .ph-title{color:#22349C;}
.ph-indigo .ph-line{
  background:#3550D8;
  background:-webkit-linear-gradient(right,#3550D8 0%,#22349C 100%);
  background:linear-gradient(270deg,#3550D8 0%,#22349C 100%);
}
```

Rules: mini line width **24–32px**, height **3px**, radius 2px, sits directly under the title
and is aligned to the title's RTL start (right). One mini line per header. The header wash is a
*tint*, max two stops, and its bottom border is the hue's `-tintEdge`. Header height 40–44px.

---

## 5. Faked-glass recipe (no `backdrop-filter`)

Glass = layered translucency + a light rim + a double edge + a real shadow. It must read as
glass **over a visibly tinted canvas** — that is what makes it visible at all.

```css
.glass{
  position:relative;border-radius:16px;
  background:#EEF3FB;                                                  /* fallback */
  background:-webkit-linear-gradient(top left,rgba(255,255,255,.94) 0%,rgba(240,245,252,.72) 52%,rgba(255,255,255,.86) 100%);
  background:linear-gradient(150deg,rgba(255,255,255,.94) 0%,rgba(240,245,252,.72) 52%,rgba(255,255,255,.86) 100%);
  border:1px solid rgba(255,255,255,.92);      /* inner light edge */
  outline:1px solid #9FB1CD;                   /* outer definition edge (Trident-safe) */
  -webkit-box-shadow:0 2px 4px rgba(16,32,58,.07),0 10px 22px rgba(16,32,58,.12),inset 0 1px 0 rgba(255,255,255,.95),inset 0 -1px 0 rgba(19,38,72,.06);
  box-shadow:0 2px 4px rgba(16,32,58,.07),0 10px 22px rgba(16,32,58,.12),inset 0 1px 0 rgba(255,255,255,.95),inset 0 -1px 0 rgba(19,38,72,.06);
}
.glass:after{                                  /* specular sweep — the "lit pane" cue */
  content:"";position:absolute;top:1px;right:1px;left:1px;height:42%;
  border-radius:15px 15px 20px 20px;
  background:-webkit-linear-gradient(top,rgba(255,255,255,.55) 0%,rgba(255,255,255,0) 100%);
  background:linear-gradient(180deg,rgba(255,255,255,.55) 0%,rgba(255,255,255,0) 100%);
  pointer-events:none;
}
```

Notes: `outline` is the second edge (Trident renders it reliably and it never affects layout).
Glass is only for **shell** elements (page header, tray, launcher, hero panel) — never for a
data card, never behind a table, never behind body text.

---

## 6. Input recipe (replaces the disliked concave / inset field)

Fields read as **crisp, clearly bounded, comfortable** — a flat white plate with a real edge
and a hairline base weight. No `inset` shadow, no neumorphic well, no gradient fill.

```css
.f-lbl{display:block;font-size:12.5px;font-weight:700;color:#3B4C69;margin-bottom:5px;
  text-align:right;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
.f-req{color:#D24343;font-style:normal;font-weight:800;}
.inp{
  display:block;width:100%;height:36px;padding:0 11px;
  font-family:inherit;font-size:14px;font-weight:600;color:#1D2B42;text-align:right;
  background:#FFFFFF;                              /* flat white — crisp, not concave */
  border:1px solid #9FB1CD;border-bottom-color:#8699B8;   /* base weight = "a field" */
  border-radius:8px;
  -webkit-box-shadow:0 1px 1px rgba(16,32,58,.05);box-shadow:0 1px 1px rgba(16,32,58,.05);
  -webkit-transition:border-color .14s,box-shadow .14s;transition:border-color .14s,box-shadow .14s;
}
.inp:hover{border-color:#7B90B4;}
.inp:focus{
  outline:none;border-color:#3550D8;
  -webkit-box-shadow:0 0 0 3px rgba(53,80,216,.22);box-shadow:0 0 0 3px rgba(53,80,216,.22);
}
.inp[readonly],.inp:disabled{background:#F2F6FC;color:#67768F;border-color:#C6D2E4;
  -webkit-box-shadow:none;box-shadow:none;}
.inp.is-err{border-color:#D24343;-webkit-box-shadow:0 0 0 3px rgba(210,67,67,.18);box-shadow:0 0 0 3px rgba(210,67,67,.18);}
.inp::-webkit-input-placeholder{color:#8C99AC;font-weight:600;}
select.inp{padding-left:26px;}      /* room for the native arrow, RTL-safe */
```

Trailing icon inside a field: wrap in `.with-ico{position:relative}` and place
`.in-ico{position:absolute;top:9px;left:9px;color:#67768F}` (RTL: the icon sits on the LEFT so
it never covers the text caret, which starts at the right). Search fields flip to
`right:11px` only when the input is a pure search box with `padding-right:34px`.

Density: field height 36px, label→field 5px, field→field 10px vertical / 10px horizontal
(margins, never `gap`). Compact table-embedded inputs: 30px, font 13px.

Focus ring is **always indigo**, on every hue of panel — one focus colour keyboard-wide.

---

## 7. Typography scale (Vazirmatn, bundled)

`font-family:'Vazirmatn','Segoe UI',Tahoma,sans-serif;` Base `font-weight:600` (Vazirmatn's
heavier stems render sharper on both engines), `line-height:1.5`,
`text-rendering:optimizeSpeed`, `-webkit-font-smoothing:auto`.

| Role | Size | Weight | Ink | Notes |
|---|---|---|---|---|
| screen title | 20px | 800 | `#10203A` | one per screen |
| screen kicker / user line | 12px | 700 | `#67768F` | under the title |
| panel section head | 13.5px | 800 | hue `-deep` | + 26px mini line |
| panel kicker | 11px | 800 | `#67768F` | letter-spacing .3px |
| field label | 12.5px | 700 | `#3B4C69` | — |
| field value / input | 14px | 600 | `#1D2B42` | — |
| body text | 13px | 600 | `#1D2B42` | — |
| table head | 12px | 800 | `#3B4C69` | on `surface-3` |
| table cell | 13px | 600 | `#1D2B42` | — |
| numeric / money cell | 13.5px | 800 | `#10203A` | `direction:ltr;text-align:left` inside the cell for digit runs |
| section total (footer) | 15px | 800 | `#10203A` | — |
| money total (invoice line) | 17px | 800 | `#075E45` | — |
| **hero «مبلغ نهایی»** | **34px** | **900** | **`#064E3B`** | letter-spacing −.02em; suffix «ریال» 13px/700 `#0A6B52`; must be the largest number on the screen by ≥ 12px |
| big stat (cashier/shift) | 22px | 900 | `#10203A` | money stats in `#075E45` |
| button label | 13px | 800 | per button | 34px tall (sm 30px) |
| badge / chip | 11px | 800 | hue `-deep` | on hue `-tint` |
| footer / meta | 11.5px | 700 | `#67768F` | — |
| native clock | 22px | bold | `#16253C` | was 19px — **larger** |
| native Jalali date | 14px | semibold | `#3B4C69` | was 12px — **larger** |

Numbers: Persian digits (`۰۱۲۳۴۵۶۷۸۹`) with ASCII `,` group separators — exactly what
`money()` in `admission.js` already produces (e.g. `۱۲,۴۵۰,۰۰۰`). Currency word «ریال» is a
separate, smaller, lighter span — never the same weight as the figure.

---

## 8. Components

### 8.1 Card / panel
`background:#FFFFFF` (gradient `#FFFFFF → #F5F8FD`), `border:1px solid #C6D2E4`,
`border-radius:12px`, `sh-1`, `overflow:hidden`, `margin-bottom:12px` (last child 0).
Body padding 12px. A card never carries a hue on its body.

### 8.2 Inner bordered region (e.g. the خدمات پذیرش table container)
`margin:10px` (an **even inset from all four corners** — no fixed bottom gap),
`border:1px solid #C6D2E4`, `border-radius:10px`, `background:#F7FAFE`, `overflow:hidden`,
and it **flexes**: `-ms-flex:1 1 auto;flex:1 1 auto;min-height:0;` so the rows inside grow and
scroll with the container instead of leaving dead space at the bottom.

### 8.3 Buttons

| Variant | Fill | Border | Ink |
|---|---|---|---|
| primary (indigo) | `#3550D8 → #2A41B8` | `#22349C` | `#FFFFFF` |
| primary hover | `#4A62E6 → #3550D8` | `#22349C` | `#FFFFFF` |
| hue-filled (emerald/azure/teal) | `-base → -deep` | `-deep` | `#FFFFFF` |
| outline (default) | `#FFFFFF → #F2F6FC` | `#9FB1CD` | `#1D2B42` |
| outline hover | `#FFFFFF → #EAF0FA` | `#7B90B4` | `#22349C` |
| soft (hue) | `-tint` | `-tintEdge` | `-deep` |
| danger | `#D24343 → #B62F2F` | `#8B2020` | `#FFFFFF` |
| disabled | `#EDF1F8` | `#CBD5E5` | `#93A0B4`, `cursor:default`, no shadow |

Height 34px (sm 30px, block 38px), radius 9px, `sh-1` on filled variants,
`inset 0 1px 0 rgba(255,255,255,.28)` top light on filled variants. Icon 15px, `margin-left:7px`
(RTL: icon leads on the right).

**Disabled must be obvious**: flat fill, no shadow, muted ink, plus `aria-disabled="true"`.
The cashier's «شروع شیفت» / «پایان شیفت» pair always shows exactly one enabled.

### 8.4 App icon (dashboard / launcher)
Squircle 84×84, `border-radius:24px`, hue gradient `-base+30% white → -base → -deep`,
`sh-icon`, a top gloss `:after` (top 3px, inset 9%, height 44%,
`rgba(255,255,255,.5) → transparent`), inner rim `inset 0 0 0 1px rgba(255,255,255,.42)`,
white 34px inline-SVG glyph. Label under it: 14px/800 `#10203A`. Hover: `scale(1.06)` only.
**No coloured halo, no plate, no glow** around the badge.

### 8.5 Table
Wrapper `surface-2`, head `surface-3` with `border-bottom:1px solid #C6D2E4`, sticky head where
supported, cell padding `7px 10px`, row divider `1px solid #E1E8F2`, zebra `#F7FAFE`,
hover `#EDF3FC` + `sh-row-hover`, numeric columns right-aligned in RTL with the digit run in
`direction:ltr`. Row state tints: paid `#E1F4EA`, unpaid `#FBF1DB`, cancelled `#FBE8E8`,
each with its `-deep` ink. Totals foot: `surface-3`, 15px/800.

### 8.6 Badge / count
Pill, height 20px, radius 999px, 11px/800, hue tint + tintEdge. Unread mail badge (header) is
the one filled badge: `#D24343 → #B62F2F`, white ink, `2px solid #FFFFFF` ring, top-left of the
envelope button (RTL).

### 8.7 Search box
Centred, **clearly narrower than full width** (dashboard 460px / max 42%), height 42px,
`border-radius:999px`, white fill, `1px solid #9FB1CD`, `sh-1`, magnifier 16px inset on the
right, placeholder `#8C99AC`. Focus = indigo ring (§6). Never inset/concave.

---

## 9. Surface shells (how the three screen families differ)

Same tokens, deliberately different **structure** — so Tools cannot be mistaken for the
dashboard, while both stay one product family.

| | Dashboard | Tools / Cashier | Admission |
|---|---|---|---|
| Canvas | `canvas-dash-*` + `glow-indigo` + `glow-azure` + `weave` | `canvas-tools-*` + `glow-teal` + `glow-indigo` + `weave` | `canvas-adm-*` (lighter, all-day) + faint `glow-indigo` |
| Header | floating rounded glass bar, inset 14px, radius 16, `sh-3` | full-bleed flat bar docked to the top edge, radius 0, 1px `#9FB1CD` bottom edge, `sh-2` | slim 46px context strip (title + counters), radius 12 |
| Navigation | centred search + 2 big app icons in a tray | right-docked category rail (208px) + list rows | three columns, no nav |
| Rhythm | airy, centred, few large targets | dense, left-aligned rows, list-first | dense forms + one table |
| Corner language | 16–24px radii | 12–14px radii, squarer | 10–12px radii |

---

## 10. Screen-specific rules

### 10.1 Dashboard (`mock-dashboard.html`)
- Header (glass): hamburger on the RIGHT, title «داشبورد» + user kicker, «کارتابل پیام» envelope
  button with unread badge on the LEFT. کارتابل پیام lives **only** in the header.
- Centred search, narrower than full width, rounded, visible border.
- «اقدامات اصلی» tray holds **exactly two** app icons: **پذیرش بیمار on the RIGHT** (indigo),
  **تب جدید on the LEFT** (teal). RTL order = پذیرش بیمار first.
- Footer meta strip, `ink-muted`.

### 10.2 Admission (`mock-admission.html`)
- Right rail: patient profile card · مشخصات بیمار · اطلاعات بیمه · پزشک معالج · انجام دهنده.
- Centre: خدمات پذیرش — table inside the §8.2 inner bordered region (even 10px inset, flexible
  rows), search + «افزودن خدمت» in the header, totals foot.
- Left rail: صورتحساب (emerald) with «مانده قابل پرداخت» then the hero «مبلغ نهایی»;
  «عملیات پذیرش» (indigo) beneath it.
- Hues exactly per §2.1 table. «عملیات پذیرش» is indigo end-to-end.

### 10.3 Tools (`mock-tools.html`)
- Docked flat header (title right, inline search, بازگشت left) + right category rail + list rows.
- **No helper subtitles anywhere** — no explanatory sentence under any title (owner request).
- Canvas = cool steel; pink/cream/clay is banned.

### 10.4 Cashier (`mock-cashier.html`, `mock-cashier-shift-closed.html`)
- Departments (بخش) are runtime data from Management; the layout must read the same with 2 or
  20 of them. **Chosen layout: one row per department in a single scrolling table with a sticky
  head and a totals foot** (see the plan summary for why).
- Shift panel: state chip, designed start/end date-time blocks, shift income, and the
  «شروع شیفت» / «پایان شیفت» pair with exactly one enabled per state.
- «صندوق نرفته‌ها» summary strip sits **directly above the table**, never in the header.

### 10.5 Welcome (`mock-welcome.html`) — reference for the native GDI screen
- Photographic background + `scrim-photo`.
- Hero panel: white-leaning glass, radius 24, `sh-4`, 3px top ribbon indigo→azure (no violet).
- Tray: **width identical to the hero panel width** (`tray.left == panel.left`,
  `tray.right == panel.right`), light/white-leaning fill `#F7FAFE → #E8EFF9`,
  `1px solid #B9C8DF` + inner white rim, `sh-glow-tray` (shadow **and** soft outer glow).
- Two account icons inside the tray: **حساب پرسنل on the RIGHT** (indigo), **حساب مدیریت on the
  LEFT** (teal), separated by a 1px faint vertical divider `#C6D2E4` at 55% alpha, inset 26px
  top and bottom. **No glow/halo/plate around the icons** — badge + `sh-icon` only.

---

## 11. Native (GDI) palette port — `src/theme.cpp` light branch

Replace the light-theme block with these values so the native shell and the web surfaces are
one product. `RGB()` order is (R,G,B).

| `Theme` field | Hex | `RGB()` |
|---|---|---|
| `bg` | `#D6E0EF` | `RGB(0xD6,0xE0,0xEF)` |
| `bg2` | `#C4D2E5` | `RGB(0xC4,0xD2,0xE5)` |
| `surface` | `#FFFFFF` | `RGB(0xFF,0xFF,0xFF)` |
| `surfaceTop` | `#F5F8FD` | `RGB(0xF5,0xF8,0xFD)` |
| `surface2` | `#E8EFF9` | `RGB(0xE8,0xEF,0xF9)` |
| `border` | `#C6D2E4` | `RGB(0xC6,0xD2,0xE4)` |
| `text` | `#1D2B42` | `RGB(0x1D,0x2B,0x42)` |
| `textDim` | `#67768F` | `RGB(0x67,0x76,0x8F)` |
| `labelInk` | `#3B4C69` | `RGB(0x3B,0x4C,0x69)` |
| `sectionInk` | `#10203A` | `RGB(0x10,0x20,0x3A)` |
| `accent` | `#3550D8` | `RGB(0x35,0x50,0xD8)` |
| `accent2` | `#4A62E6` | `RGB(0x4A,0x62,0xE6)` |
| `accentHover` | `#4A62E6` | `RGB(0x4A,0x62,0xE6)` |
| `accentText` | `#FFFFFF` | `RGB(0xFF,0xFF,0xFF)` |
| `danger` | `#D24343` | `RGB(0xD2,0x43,0x43)` |
| `dangerHover` | `#E05B5B` | `RGB(0xE0,0x5B,0x5B)` |
| `success` | `#0E9C77` | `RGB(0x0E,0x9C,0x77)` |
| `warn` | `#C4890F` | `RGB(0xC4,0x89,0x0F)` |
| `inputBg` | `#FFFFFF` | `RGB(0xFF,0xFF,0xFF)` |
| `inputText` | `#1D2B42` | `RGB(0x1D,0x2B,0x42)` |
| `hover` | `#EAF0FA` | `RGB(0xEA,0xF0,0xFA)` |
| `headerTop` | `#F2F6FD` | `RGB(0xF2,0xF6,0xFD)` |
| `headerBot` | `#D8E2F1` | `RGB(0xD8,0xE2,0xF1)` |
| `g_infoAccent` | `#0E7FC0` | `RGB(0x0E,0x7F,0xC0)` — **was violet; violet is banned** |
| `g_infoAccent2` | `#0A4A72` | `RGB(0x0A,0x4A,0x72)` |

### 11.1 Native window header (lighter + larger clock/date)
- Band: three-stop `#F2F6FD` → mid `#E9EFF9` → `#D8E2F1` (today's `#E8EEF8 → #C8D6E8` is too
  dark). Bottom hairline `#C6D2E4`, plus a 2px indigo underline at 18% alpha.
- Top ribbon 3px: `#3550D8 → #4A62E6 → #0E7FC0` (indigo → indigo-light → azure; **no violet**).
- Clock: `mkFont(22, FW_BOLD)` (from 19), ink `#16253C`.
- Jalali date: `mkFont(14, FW_SEMIBOLD/FW_NORMAL)` (from 12), ink `#3B4C69`.
- Framing hairlines beside the clock zone: `#B9C8DF` at 110 alpha; keep the small accent
  diamonds but paint them `#3550D8` at 70% blend toward the band.
- Identity block ink: name `#10203A` (bold 15), role `#67768F` (12.5).

### 11.2 Native welcome hero + tray
| Element | Value |
|---|---|
| photo scrim | `#E4ECF8` at alpha 86/255 (lighter than today's 58 over a darker tint) |
| hero panel top | `#FFFFFF` |
| hero panel bottom | `#EEF3FB` |
| hero panel border | `#B9C8DF` |
| hero inner rim | `#FFFFFF` at alpha 120 |
| hero ribbon | `#3550D8 → #4A62E6 → #0E7FC0`, 3px, inset by the corner radius |
| tray top | `#F9FBFE` |
| tray bottom | `#E8EFF9` |
| tray border | `#B9C8DF` |
| tray inner rim | `#FFFFFF` at alpha 150 |
| tray shadow | spread `S(18)`, alpha 54, neutral `#16253C` |
| tray glow | spread `S(30)`, alpha 40, tint `#A6C6F0` (soft outer glow, light-leaning) |
| tray divider | `#C6D2E4` at alpha 140, 1px, inset `S(26)` top/bottom |
| icon badge (پرسنل) | `#5A72E6 → #3550D8 → #22349C` |
| icon badge (مدیریت) | `#35A8AA → #0F8B8D → #095A5C` |
| icon badge shadow | spread `S(9)`, alpha 70, neutral — **remove `gpShadowColor` halo and the tinted plate under the badge** |
| icon label | `#10203A`, bold 15 |
- Geometry: `tray.left = panel.left; tray.right = panel.right;` (tray width == hero width).

---

## 12. Dark theme parity (token mapping)

Light is the product default and the subject of this redesign; dark keeps structure identical
and swaps tokens:

| Light token | Dark value |
|---|---|
| canvas `-hi/-mid/-lo` | `#121A28 / #0E1522 / #0A1019` |
| `surface-1` | `#182234` (gradient `#1C2739 → #151F2F`) |
| `surface-2` | `#141D2C` |
| `surface-3` | `#1B2537` |
| `surface-glass` | `rgba(28,39,57,.86)` over canvas |
| `ink-title / body / label / muted` | `#EEF3FB / #DCE5F2 / #B4C2D6 / #8B9BB3` |
| `border-hair / line / strong` | `#25314A / #33425C / #45577A` |
| indigo tint / base / deep | `#232F55 / #6A83F7 / #A9B8FF` |
| azure tint / base / deep | `#12324A / #3FA9E6 / #9BD3F5` |
| teal tint / base / deep | `#0F3436 / #2FB3B5 / #93DEDF` |
| emerald tint / base / deep | `#0F3A2C / #2CC191 / #8FE7C4` |
| amber tint / base / deep | `#3A2E12 / #E0A83A / #F4D79A` |
| red tint / base / deep | `#3A1C1C / #E86A6A / #F6B4B4` |
| shadows | same geometry, alpha ×1.6, tint `rgba(0,0,0,…)` |
| glass rim | `rgba(255,255,255,.10)` |
Ink on dark hue tints uses the `-deep` (light) value listed above as the *text* colour.

---

## 13. Review checklist (every PR touching UI)

1. No `var()`, no `gap`, no `backdrop-filter`, no logical properties, no webfont, no network asset.
2. Every flex container carries its `-ms-` prefixes; spacing is margin-based.
3. Every colour is a literal hex from §1/§2 (or an `rgba()` of one of them).
4. Every panel: one hue, header wash + 26px mini line + icon chip + `-deep` title, no second hue.
5. No full-width heavy rule on a card top; no violet; no cream/pink.
6. Inputs use §6 (flat white, crisp border, indigo focus ring) — no inset/concave fields.
7. The hero «مبلغ نهایی» is the largest number on its screen.
8. Disabled controls are unmistakably disabled and carry `aria-disabled`.
9. Interactive elements carry a stable `data-component-id` and an accessible label.
10. Ink is dark on light: no `ink-muted` on anything deeper than a `-tint`.
