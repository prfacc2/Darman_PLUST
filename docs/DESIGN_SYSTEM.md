# DarmanPlus Design System — v1.92.0

> **سیستم طراحی استاندارد دارمن‌پلاس.**
> This document is the **single source of truth** for every color, spacing rule,
> and component style in DarmanPlus. The C++ native UI reads tokens live from
> `src/theme.cpp` (`applyTheme()`); the embedded HTML surfaces mirror the **exact
> same hex values** in `assets/admission/admission.css`. **When you change a
> color, update all three places: `theme.cpp`, `admission.css`, and this doc.**
>
> The palette below is named **"DarmanPlus Medical Glass"**. It replaced the
> retired calm/warm palettes (v1.77). There are exactly **two themes**: Light
> (روشن) and Dark (مشکی).

---

## 1. Color Palette

The tokens below are the **only** colors an AI model may use. Do not invent new
hex values. Every token exists in `struct Theme g_theme` (`src/app.h`) and is set
inside `applyTheme(bool dark)` (`src/theme.cpp`).

### 1.1 Light theme (روشن)

| Token          | Hex       | RGB            | Role                                           |
|----------------|-----------|----------------|------------------------------------------------|
| `bg`           | `#C3CDDD` | `195,205,221`  | Page body (deep blue-gray — gives the page depth) |
| `bg2`          | `#B5C1D4` | `181,193,212`  | Page gradient bottom (deepest step)             |
| `surface`      | `#F1F4F9` | `241,244,249`  | Cards — **off-white, NEVER pure `#FFFFFF`**      |
| `surfaceTop`   | `#F7F9FC` | `247,249,252`  | Soft top-light on cards (gradient start)         |
| `surface2`     | `#DDE4EF` | `221,228,239`  | Wells / bars / list backgrounds                 |
| `border`       | `#A8B5CC` | `168,181,204`  | Crisp hairline — **always clearly visible**      |
| `text`         | `#1A2435` | `26,36,53`     | Primary ink (high contrast)                     |
| `textDim`      | `#5C6B7E` | `92,107,126`   | Muted / secondary text                          |
| `labelInk`     | `#2E3D52` | `46,61,82`     | Readable field labels                            |
| `sectionInk`   | `#0D1726` | `13,23,38`     | Strong section titles                            |
| `accent`       | `#2B48CC` | `43,72,204`    | Royal blue — primary action / focus             |
| `accent2`      | `#4263E8` | `66,99,232`    | Accent gradient end / hover                      |
| `danger`       | `#DC2626` | `220,38,38`    | Delete / cancel / error (red)                   |
| `success`      | `#059669` | `5,150,105`    | Insurance / paid / final (green)                |
| `warn`         | `#D97706` | `217,119,6`    | Invoice / pending (amber)                       |
| `inputBg`      | `#E2E8F2` | `226,232,242`  | Input wells (recessed, tinted)                  |
| `hover`        | `#D6DEF0` | `214,222,240`  | Soft accent wash on hover                        |
| `headerTop`    | `#D0D9E8` | `208,217,232`  | Frosted header band (top)                        |
| `headerBot`    | `#BAC6D8` | `186,198,216`  | Frosted header band (bottom)                     |

### 1.2 Dark theme (مشکی)

| Token          | Hex       | RGB            | Role                                           |
|----------------|-----------|----------------|------------------------------------------------|
| `bg`           | `#0B0F15` | `11,15,21`     | Near-black page                                 |
| `bg2`          | `#0E131B` | `14,19,27`     | Page gradient bottom                            |
| `surface`      | `#151B25` | `21,27,37`     | Cards (off-black)                               |
| `surfaceTop`   | `#1D2531` | `29,37,49`     | Card gradient top                               |
| `surface2`     | `#0D1218` | `13,18,24`     | Bars / wells                                    |
| `border`       | `#2C3645` | `44,54,69`     | Visible separators                              |
| `text`         | `#E8EEF6` | `232,238,246`  | Bright primary text                             |
| `textDim`      | `#95A3B8` | `149,163,184`  | Readable dim text                               |
| `labelInk`     | `#C2CCDC` | `194,204,220`  | Light-gray labels                               |
| `sectionInk`   | `#E8EEF6` | `232,238,246`  | Bright section titles                           |
| `accent`       | `#3B82F6` | `59,130,246`   | Vivid blue                                      |
| `accent2`      | `#2563EB` | `37,99,235`    | Gradient end / hover                            |
| `danger`       | `#F87171` | `248,113,113`  | Delete / cancel / error (red)                   |
| `success`      | `#34D399` | `52,211,153`   | Insurance / paid / final (green)                |
| `warn`         | `#FBBF24` | `251,191,36`   | Invoice / pending (amber)                       |
| `inputBg`      | `#181F28` | `24,31,40`     | Input wells (distinctly lighter than card)      |
| `hover`        | `#1C2430` | `28,36,48`     | Hover wash                                       |
| `headerTop`    | `#0D131C` | `13,19,28`     | Header band (top)                                |
| `headerBot`    | `#070A0F` | `7,10,15`      | Header band (bottom)                             |

### 1.3 Derived / secondary tokens (set alongside the table above)

These are computed in `applyTheme()` from the core tokens and exist on `g_theme`.
Use them rather than hard-coding:

| Token           | Light               | Dark                | Notes                                   |
|-----------------|---------------------|---------------------|-----------------------------------------|
| `accentHover`   | `#4263E8`           | `#60A5FA`           | Hover state of accent buttons           |
| `accentText`    | `#FFFFFF`           | `#FFFFFF`           | Text on accent fill (always white)      |
| `dangerHover`   | `#EF4444`           | `#FCA5A5`           | Hover state of danger buttons           |
| `inputText`     | `#1A2435`           | `#E8EEF6`           | Typed text inside an input well         |
| `g_infoAccent`  | `#6D4DD6` (violet)  | `#A58AF0` (violet)  | **Non-red "attention"** — change-requests only; never an admission card |

> `g_infoAccent` is a **violet** reserved for the change-request / attention flow.
> It is intentionally distinct from `danger` (red) so users never read a
> change-request as an error. **It is NOT a general accent — do not use it for
> admission section cards.**

---

## 2. Design Rules — قواعد اجباری طراحی

> **این قوانین برای تمام کارهای رابط کاربری الزامی است. هیچ المان جدیدی نباید
> آن‌ها را نقض کند.** (These rules are mandatory for ALL UI work. No new element
> may violate them.)

```text
RULE D-1  surface is NEVER pure white (#FFFFFF).
          Light surface = #F1F4F9 (off-white). The off-white gives cards real
          glassmorphism depth against the deep #C3CDDD page.

RULE D-2  border must be ≥ 40 lightness points from surface.
          The hairline must always be clearly visible — never an invisible edge.
          (Light: border 168 vs surface 241 = ~73 pts. Dark: 44 vs 21 = ~23 pts
           but high contrast on near-black, verified visible.)

RULE D-3  text must be ≥ 140 lightness points from surface (WCAG AA+, near AAA
          on card surface). High contrast is non-negotiable.

RULE D-4  accent is royal blue #2B48CC.
          NEVER use purple / violet for admission section cards. Violet
          (g_infoAccent) is reserved for the change-request attention flow only.

RULE D-5  Only THREE semantic colors exist:
            success (green  #059669)
            danger  (red    #DC2626)
            warn    (amber  #D97706)
          Do not introduce a fourth semantic color.

RULE D-6  Semantic color usage is FIXED:
            insurance      → success (green)
            invoice        → warn    (amber)
            payable / final→ success, BOLD
            delete / cancel→ danger  (red)
          Apply these consistently across every surface.

RULE D-7  Cashier has its own identity color: teal #0F766E — HEADER ONLY.
          It appears on the cashier overlay header (.cash-overlay-head). Do not
          spread teal into cashier cards, inputs, or other surfaces.

RULE D-8  Glassmorphism: every card has
            • off-white surface fill
            • a subtle surfaceTop → surface vertical gradient
            • a soft drop shadow
            • a crisp 1px border
          (C++: gpGradRoundRectBg(surfaceTop, surface, border). CSS: card rules
           in admission.css.)

RULE D-9  CSS is Trident-safe.
            • NO custom properties  (no var(--x))
            • NO calc()
            • ALL colors are literal hex / rgba()
          MSHTML (Trident) is the guaranteed fallback renderer and supports none
          of the above. The whole admission.css must run on it.

RULE D-10 RTL is MANUAL.
            • C++ main UI: no window-level WS_EX_LAYOUTRTL (it triggers GDI
              mirroring bugs). RTL is laid out by hand.
              EXCEPTION: WS_EX_LAYOUTRTL|WS_EX_RTLREADING is used ONLY on
              individual ListView controls (admin user tables) where it is
              safe — do NOT remove those.
            • HTML surfaces: <html lang="fa" dir="rtl">. Do not add layout
              mirroring in C++ for the embedded surfaces.

RULE D-11 Font: Vazirmatn (embedded as vazir.ttf, RCDATA 404).
            • WebView2 path: injected as a base64 data: URI @font-face.
            • MSHTML path: resolved from the process-installed memory font
              (no @font-face in the page).
            • In C++, run EVERY displayed number through toFaDigits() so digits
              render in Persian. Never print raw ASCII digits to the user.

RULE D-12 Every new UI element MUST:
            • use S() scaling for all pixel sizes in C++  (S(v) = v * g_scale)
            • use ONLY the palette tokens above (no invented colors)
            • compile under -Werror with zero warnings
```

---

## 3. Concave Input Style — استایل ورودی مقعر

Inputs are **recessed wells** (concave), not raised fields. This is the one
mandatory input look across native and HTML surfaces.

### 3.1 Native (C++ / GDI+)

```text
Fill        : inputBg (#E2E8F2 light / #181F28 dark) — a solid recessed well.
Inset gradient (top → bottom):
              top    = blendColor(inputBg, border, 34%)   // slightly darker rim
              bottom = inputBg                            // well floor
Border      : border at rest; switches to accent on focus.
Focus halo  : gpShadowColor(dc, rc, rad, spread, alpha, accent-tint)
              — a soft accent glow just outside the well.
Invalid     : red glow (gpShadowColor with danger tint) + red border (danger).
```

### 3.2 Embedded (CSS, Trident-safe)

```css
/* recessed well — literal colors, NO var(), NO calc() */
.inp{
  background:#e2e8f2;                       /* inputBg */
  border:1px solid #a8b5cc;                  /* border  */
  box-shadow:inset 0 2px 4px rgba(16,24,40,.10);
}
.inp:focus{
  border-color:#2b48cc;                     /* accent  */
  box-shadow:0 0 0 3px rgba(43,72,204,.18);  /* accent halo */
  background:#fff;                           /* lift on focus */
}
.inp.invalid{
  border-color:#dc2626;                      /* danger  */
  box-shadow:0 0 0 3px rgba(220,38,38,.18);  /* red glow */
}
```

> The exact rgba alphas above are the reference; mirror them in dark theme with
> the dark token hexes (`#181F28`, `#2C3645`, `#3B82F6`, `#F87171`).

---

## 4. Component Rules — قواعد کامپوننت‌ها

### 4.1 Buttons — `AzFlatBtn`

Owner-drawn flat button class (`src/theme.cpp`). Style is selected via the
`BtnStyle` enum (`src/app.h`):

| Style        | Value | Use                                          |
|--------------|-------|----------------------------------------------|
| `BS_GHOST`   | 0     | Borderless bar / toolbar button (rest = transparent) |
| `BS_PRIMARY` | 1     | Filled accent (`accent → accent2` gradient)  |
| `BS_DANGER`  | 2     | Filled danger (delete / cancel)              |
| `BS_OUTLINE` | 3     | Accent outline on surface                    |
| `BS_CARD`    | 4     | Big claymorphic card button (chunkier radius, optional brand accent) |

- A `BS_CARD` button may carry a **per-button brand accent** (border, badge,
  halo) via its button data — 0 means "use the theme accent".
- Corner radius: `S(10)` for normal buttons, `S(22)` for `BS_CARD`.
- Always scale with `S()`.

### 4.2 Cards

```text
C++ : gpGradRoundRectBg(dc, rc, rad, surfaceTop, surface, border)
      + a soft drop shadow (gpShadowColor).
CSS : .card { border; border-radius; box-shadow; surfaceTop→surface gradient }
```
Cards are off-white (light) / off-black (dark), never pure white, with a crisp
visible border and a subtle top-light gradient.

### 4.3 Section headers

```text
Ink   : g_theme.sectionInk (bold)
Rule  : a fadeRule UNDER the title (gradient hairline that fades out),
        NOT a full solid line above the header.
```

### 4.4 Icons

- Use the **vector** `drawIcon(dc, icon, rc, col, thick)` with the `ICO_*` enum
  (`src/app.h`: `ICO_X, ICO_CALC, ICO_PRINT, ICO_UPDATE, ICO_MOON, ICO_SUN,
  ICO_USER, ICO_SHIELD, ICO_PLUS, ICO_LOGOUT, ICO_DETACH, ICO_CROSS_MED,
  ICO_CHECK, ICO_TRASH, ICO_SAVE, ICO_BACK, ICO_ID, ICO_PHONE, ICO_CAL,
  ICO_PIN, ICO_RECEIPT, ICO_CLOCK, ICO_REFRESH, ICO_GEAR, ICO_BELL, ICO_TAB,
  ICO_CHEVRON, ICO_SAVED_MSG, ICO_PALETTE, ICO_INFO, ICO_PEOPLE, ICO_WALLET,
  ICO_LETTER, ICO_USER_ADD, ICO_HOME`).
- **Never** use colored emoji or bitmap stickers where a vector icon exists.

### 4.5 Tabs

- Responsive **shrink-to-fit** tabs.
- On overflow, render chevrons (`ICO_CHEVRON`) for scroll — do not silently
  clip or drop tabs.

---

## 5. File-to-Theme Sync Rule — همگام‌سازی رنگ‌ها

> **هنگام تغییر رنگ، سه فایل باید همزمان به‌روز شوند.**

```text
RULE S-1  When you change a color token in src/theme.cpp applyTheme(),
          you MUST update the matching hex value in
          assets/admission/admission.css.

RULE S-2  When you change a color token, you MUST update the matching
          hex value in this document (docs/DESIGN_SYSTEM.md) tables.

RULE S-3  When you ADD a new color token, add it to ALL THREE places in
          the same change:
            1. src/theme.cpp   (applyTheme — both dark and light branches)
            2. assets/admission/admission.css  (Trident-safe literal hex)
            3. docs/DESIGN_SYSTEM.md           (this palette table)
          A token that exists in only one or two files is a bug.

RULE S-4  Never rename an existing token. C++ code, CSS rules, and other
          docs reference token names by string. Renaming breaks lookups.
```

---

## Quick reference — "can I use this color?"

| You want to…                         | Use token            | Notes                                  |
|---------------------------------------|----------------------|----------------------------------------|
| Paint a card                          | `surface`            | off-white, never `#FFFFFF`             |
| Paint a page section bar / well       | `surface2`           |                                        |
| Draw a hairline                       | `border`             | always visible                         |
| A primary button / link / focus       | `accent` / `accent2` | royal blue, never purple on admission  |
| Mark insurance / paid / final         | `success`            | green, bold for payable/final          |
| Mark an invoice / pending             | `warn`               | amber                                  |
| Delete / cancel / error               | `danger`             | red                                    |
| Cashier header                        | `#0F766E`            | teal, HEADER ONLY                      |
| Change-request attention              | `g_infoAccent`       | violet, not for admission cards        |
| An input field                        | `inputBg` + `border` | concave well, accent halo on focus     |
