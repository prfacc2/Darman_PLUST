# گزارش تحویل برای مدل بعدی — درمان پلاس (DarmanPlus) v1.63.0

> این فایل را **قبل از هر کاری** بخوان. وضعیت دقیق پروژه، کارهای انجام‌شده،
> کارهای باقی‌مانده، و نقطهٔ دقیق ادامه را توضیح می‌دهد.

**تاریخ آخرین به‌روزرسانی:** ۲۰۲۶-۰۸-۰۳ — **نسخهٔ فعلی:** ۱.۶۳.۰

---

## ۱) معرفی پروژه

- **درمان پلاس**: نرم‌افزار دسکتاپ پذیرش/مدیریت درمانگاه، فارسی و راست‌به‌چپ (RTL).
- **زبان/فناوری**: C++17 خالص با Win32 API + GDI/GDI+ (بدون Qt/MFC/.NET) + صفحهٔ پذیرش HTML تعبیه‌شده (MSHTML/WebView2 هیبرید).
- **خروجی**: یک فایل EXE استاتیک (PE32 i686) برای ویندوز ۷ تا ۱۱، در `build/DarmanPlus.exe` (~۴.۳ مگابایت).
- **کامپایل**: `./build.sh` → `i686-w64-mingw32-g++` با `-std=c++17 -O2 -s -municode -mwindows -static -Wall -Wextra -Werror`، سپس strip و تولید `build/DarmanPlus.exe.sha256`.
- **مخزن**: https://github.com/prfacc2/AZADI_TEB — برنچ‌ها: `main` (اصلی)، `dev` (آینه)، `genspark_ai_developer` (کاری).

### قواعد الزامی کاربر (تغییرناپذیر)

1. بعد از **هر بخش**، فوراً کامیت + پوش روی گیت‌هاب (نه به‌صورت دسته‌ای).
2. توضیح/سؤال اضافی به کاربر نده؛ کار را تا ۱۰۰٪ کامل کن.
3. EXE جدید باید جایگزین `build/DarmanPlus.exe` شود.
4. `docs/CHANGELOG.md` را برای هر نسخه به‌روز کن.
5. RTL دستی است (بدون `WS_EX_LAYOUTRTL`).
6. چرخهٔ انتشار کامل: build → push به `main` و `dev` → PR + اشتراک لینک → ریلیز جدید با EXE → **حذف ریلیز/تگ قبلی** → گزارش ۴ بخشی (CHANGED / UNCHANGED / STILL NEEDING WORK / REMAINING).

---

## ۲) نکات فنی حیاتی (اینها را ندانی، کد را می‌شکنی)

### مقیاس و فونت
- `inline int S(int v)` در `src/app.h:61` — **هر** بعد پیکسلی باید از این عبور کند.
- `fitFont(px, weight, f)` برای ساخت فونت مقیاس‌شده.

### تم
- ساختار `g_theme` با فیلدهای: `bg, bg2, surface, surfaceTop, surface2, border, text, textDim, accent, accent2, accentHover, accentText, danger, dangerHover, success, warn, hover, inputBg, headerTop, headerBot, sectionInk, labelInk`.
- `g_dark`, `applyTheme(bool)`, `broadcastThemeChange()`, `blendColor(a, b, pct)`.
- ⚠️ `broadcastThemeChange()` **باید** اول `gpFreeBackgroundCache()` را صدا بزند، وگرنه سوئیچ تم کش کهنه را نشان می‌دهد.

### کمکی‌های GDI+ (`src/gdiplus.cpp`)
- `gpRoundRect`, `gpGradRoundRect`, `gpGradRoundRectBg`, `gpFillAlpha`, `gpLine`, `fillRoundRect`.
- `gpShadow(dc, rc, rad, spread, alpha)` — سایهٔ خنثی. حلقه‌ها روی **۱۲** سقف دارند (آلفا با گام جبران می‌شود). این سقف را برندار؛ عامل اصلی افت FPS بود.
- `gpShadowColor(dc, rc, rad, spread, alpha, tint)` — سایهٔ ته‌رنگی (tint تا ۳۲٪ شید می‌شود). موتور «نور رنگی» زبان طراحی ۱.۶۳.۰ است.
- `gpDrawBackground` نتیجهٔ نهایی (هنر + اسکریم) را در بیت‌مپ کش می‌کند؛ کلید کش `(W, H, dark, scrim, alpha)`. آزادسازی با `gpFreeBackgroundCache()`.

### دکمه‌ها
- کلاس سفارشی `AzFlatBtn` در `src/theme.cpp` (`btnProc`)، سبک‌ها: `BS_PRIMARY / BS_DANGER / BS_INFO / BS_OUTLINE / BS_CARD / BS_GHOST`.
- از ۱.۶۳.۰: شعاع گوشه **متناسب با ارتفاع** (`hgt/3`، سقف `S(14)`، کف `S(6)`) و لامبدای مشترک `solidBody(top, bot, glow)` برای PRIMARY/DANGER/INFO.
- `BtnBgToken` برای حل معنایی پس‌زمینه؛ `setFlatButtonBg`.

### آیکون‌ها
- **وکتور**: `drawIcon(HDC, int icon, RECT, COLORREF, int thick)` با enum `ICO_*` در `src/app.h:120-133` — `ICO_USER, ICO_SHIELD, ICO_PLUS, ICO_LOGOUT, ICO_DETACH, ICO_CROSS_MED, ICO_X, ICO_CHEVRON, ICO_TAB, ICO_ID, ICO_BELL, ICO_GEAR, ICO_SAVE, ICO_REFRESH, ICO_RECEIPT, ICO_TRASH`.
- **رستری**: PNG های RCDATA با شناسه‌های ۲۰۱–۲۰۶، **سفید روی آلفا** ذخیره می‌شوند و در زمان ترسیم با `ColorMatrix` در `gpDrawTintedImageRes()` رنگ می‌گیرند. اگر آیکون جدید ساختی، باید همین قاعده را رعایت کند (`scripts/make_icons.py` را اجرا کن، دستی نساز).
- نقشهٔ RCDATA در `src/app.rc`: `103` bg_light.jpg، `104` bg_dark.jpg، `201` ic_printer، `202` ic_receipt، `203` ic_shield، `204` ic_last، `205` ic_settings، `206` ic_calc.

### تلهٔ مختصات `WM_PAINT` (مهم)
`src/main.cpp` بافر دوگانه را فقط به اندازهٔ `ps.rcPaint` می‌سازد و با `SetViewportOrgEx(dc, -d.left, -d.top, NULL)` مبدأ را جابه‌جا می‌کند. **`SelectClipRgn` در واحد دیوایس کار می‌کند**، پس ناحیه باید `CreateRectRgn(0, 0, dw, dh)` باشد نه مستطیل منطقی. اگر این را به مستطیل منطقی برگردانی، هر dirty-rect غیرمبدأ تمام ترسیم را کلیپ می‌کند و صفحه خالی می‌شود.

### دیالوگ‌های مودال
`runModal` در `src/dialogs.cpp` **static است (export نشده)** — حلقهٔ پیام تودرتو + `EnableWindow`. الگوهای مرجع: `showLoginDialog`, `showShiftDialog`, `showProfileDialog`.

### تقویم و داده
- جلالی: `iranNow` (UTC+3:30)، `gregToJalali`، `jalaliDateShort`، `toFaDigits`.
- لایهٔ داده فایل‌محور در `data/` (UTF-8، جداکنندهٔ `|`).
- بیمه‌ها: `INSURANCES[7]` و `SUPP_INSURANCES[10]` در `src/billing.cpp`.

---

## ۳) زبان طراحی ۱.۶۳.۰ — از این خارج نشو

هر سطح جدیدی که می‌سازی باید این هفت عنصر را رعایت کند، وگرنه ظاهر برنامه دریفت می‌کند:

1. **سایهٔ ارتفاع** — `gpShadow` برای کارت خنثی، `gpShadowColor` برای کنترل رنگی.
2. **گرادیان سطح** از بالا به پایین — `gpGradRoundRect` / `gpGradRoundRectBg` با `surfaceTop → surface`.
3. **مدال آیکون** — دیسک `gpFillAlpha` با ته‌رنگ accent + رینگ مویی.
4. **خط جداکنندهٔ محوشو** — پررنگ در لبهٔ راست (RTL)، محوشونده به چپ (`gpLine` قطعه‌قطعه).
5. **نشانگر ۳ پیکسلی لبهٔ RTL** برای آیتم انتخاب‌شده/hover.
6. **چالهٔ فرورفتهٔ ورودی** — گرادیان معکوس + هالهٔ accent در فوکوس + هالهٔ خطر در نامعتبر.
7. **پیل درخشان** برای تب فعال، بج و کلید تاگل.

### کمکی‌های مشترکی که برای همین ساخته شده‌اند — از نو ننویس
- `src/reception.cpp` → داخل `tabPageProc`'s `WM_PAINT` (~خط ۳۲۸۱): لامبداهای `cardShell(cr)`، `fadeRule(x0, x1, y)`، `medallion(hi)`. همهٔ کارت‌های پایین‌تر در همان هندلر باید از اینها استفاده کنند.
- `src/manage.inc` → بعد از `mgContentRect` (~خط ۲۶۴۰): `mgRowShell(dc, r, hot, rad, accent)` و `mgEmptyState(dc, a, icon, head, hint)`.
- `src/theme.cpp` → `solidBody(top, bot, glow)` داخل `btnProc`.

---

## ۴) وضعیت فعلی — چه چیزی کامل است

### تحویل‌شده در ۱.۶۲.۰ (نباید رگرس کند)
- چاپ خدمات روی **هر ۳۰ قالب آماده** به‌صورت جدول واقعی `PIT_SERVICES` (نه لیبل).
- حذف کامل «نوبت‌دهی» از همهٔ لایه‌ها.
- بازطراحی صفحهٔ خوش‌آمد (`HomeGeom`/`homeGeom` تنها منبع هندسه) و کارت‌های ورود.
- بازطراحی صفحهٔ پذیرش HTML: پروفایل بالا-راست، صورتحساب بالا-چپ با مبلغ نهایی زیر آن، «ثبت قبض» آبی (نه سبز)، رفع درگ «صندوق نرفته‌ها»، خوشهٔ چاپ درون‌صفحه پایین-راست، تاریخ+شیفت واکنش‌گرا، خدمات پایین-وسط.
- هنر پس‌زمینهٔ جدید روشن/تیره (`assets/bg_light.jpg`, `assets/bg_dark.jpg`، ۱۹۲۰×۱۰۸۰).

### تحویل‌شده در ۱.۶۳.۰ (این نسخه)
- ✅ رفع افت FPS تنظیمات — سه علت ریشه‌ای (کش پس‌زمینه، سقف حلقهٔ سایه، بافر dirty-rect) + رفع باگ نهفتهٔ واحد کلیپ.
- ✅ آیکون‌های حرفه‌ای — `scripts/make_icons.py` + هر ۶ PNG بازتولید و بازبینی شد.
- ✅ بازطراحی دکمه‌های صفحهٔ تنظیمات — ردیف، تاگل، چیپ مقدار، دکمهٔ بستن.
- ✅ مدرن‌سازی کل رابط نیتیو — پذیرش، مدیریت، ادمین، هر ۳ دیالوگ، جدول خدمات، پنل صف.
- ✅ بازبینی جدول خدمات چاپی (`printer.cpp::pdDrawServices`) — بدون نیاز به تغییر.

---

## ۵) کارهای باقی‌مانده برای مدل بعدی (به ترتیب اولویت)

1. **تست روی ویندوز واقعی** — تمام بازطراحی این نسخه تحت `-Werror` بیلد شده و منطقاً بازبینی شده، اما اسکرین‌شات واقعی از هر صفحه گرفته نشده. اولویت: تنظیمات (FPS)، جدول خدمات با ردیف پرشده، پنل مدیریت.
2. **بازطراحی طراح چاپ (`src/printer_designer.inc`)** — آخرین سطح نیتیوی است که زبان طراحی ۱.۶۳.۰ روی آن اعمال **نشده**. نوار ابزار، پنل ویژگی‌ها و گالری قالب باید همان سایه/گرادیان/مدال/خط محوشو را بگیرند.
3. **ماشین حساب (`src/calculator.cpp`)** — سطح نیتیو دیگری که هنوز ظاهر قدیمی دارد.
4. **پایگاه‌داده SQLite/سرور مرکزی** + همگام‌سازی چند ایستگاه پذیرش.
5. **ماژول‌های پنل مدیریت**: گزارش مالی، آمار پذیرش، مدیریت پزشکان و خدمات.
6. **چاپ روی پرینتر حرارتی ۸ سانتی فیش‌زن**.

---

## ۶) چرخهٔ کاری — دقیقاً همین ترتیب

### محیط
اگر سندباکس بازیافت شده، ابتدا زنجیرهٔ ابزار را نصب کن:
```bash
sudo apt-get install -y g++-mingw-w64-i686 binutils-mingw-w64-i686
```

### بیلد و تست
```bash
cd /home/user/webapp
./build.sh                                     # باید Build OK بدهد
strings build/DarmanPlus.exe | grep 1.63.0       # تأیید نسخه در باینری
python3 scripts/check_ui_contract.py           # UI contract OK
python3 scripts/test_builtin_templates.py      # all PASS
node --check assets/admission/admission.js     # OK
```

### انتشار
```bash
git add -A && git commit -m "..."
git fetch origin main
git rebase origin/main                          # تعارض؟ کد ریموت اولویت دارد
git reset --soft origin/main && git commit -m "..."   # اسکواش
git push -f origin genspark_ai_developer
gh pr create --base main --head genspark_ai_developer --title "..." --body "..."
# لینک PR را به کاربر بده
gh pr merge --merge
git push origin main:dev                        # آینه
gh release create v1.63.0 build/DarmanPlus.exe build/DarmanPlus.exe.sha256 -t "..." -n "..."
gh release delete v1.62.0 --cleanup-tag -y      # حذف نسخهٔ قبلی
```

### به‌روزرسانی نسخه — هر ۳ فایل، هیچ‌کدام را جا نگذار
- `src/app.h` خط ۲۳ → `#define APP_VERSION_W L"x.y.z"`
- `src/app.rc` خطوط ۶۳/۶۴ (`FILEVERSION`/`PRODUCTVERSION x,y,z,0`) و ~۷۷/~۸۱ (رشته‌های `FileVersion`/`ProductVersion`)
- `update/version.txt` — خط ۱ نسخه، خط ۲ لینک دانلود asset ریلیز

---

## ۷) نقشهٔ فایل‌ها

| فایل | نقش |
|---|---|
| `src/main.cpp` | قاب اصلی، صفحهٔ خوش‌آمد (`HomeGeom`/`homeGeom`)، `WM_PAINT` با بافر dirty-rect |
| `src/app.h` | نسخه، `S()`, `fitFont`, enum `ICO_*`, پروتوتایپ همهٔ کمکی‌ها |
| `src/theme.cpp` | پالت `g_theme`, `applyTheme`, `btnProc` (نقاش دکمه‌ها), کمبوی تم‌دار |
| `src/gdiplus.cpp` | همهٔ کمکی‌های GDI+، کش پس‌زمینه، `gpShadow`/`gpShadowColor` |
| `src/reception.cpp` | صفحهٔ پذیرش نیتیو: تب‌ها، فرم، پنل بیمار، جدول خدمات، صف، صورتحساب، کارتابل |
| `src/manage.inc` | پنل مدیریت: shell، ریل ناوبری، داشبورد KPI، بخش‌ها، کارمندان، پیام‌ها، یادداشت‌ها |
| `src/settings.cpp` | صفحهٔ تنظیمات (کش‌شده، ردیف/تاگل/چیپ) |
| `src/admin.cpp` | پنل مخفی ادمین |
| `src/dialogs.cpp` | `runModal` + دیالوگ‌های ورود/شیفت/پروفایل |
| `src/printer.cpp` | چاپ واقعی GDI؛ `pdDrawServices` در خط ۱۸۱۹ |
| `src/print_designer_templates.inc` | ۳۰ قالب آمادهٔ چاپ |
| `src/printer_designer.inc` | طراح چاپ (⚠️ هنوز مدرن نشده) |
| `src/billing.cpp` | محاسبهٔ مبلغ + `INSURANCES`/`SUPP_INSURANCES` |
| `src/ui_kit.cpp` | `namespace uikit`: `RoundedPanel`, `Card`, `Chip`, `InputWell`, `AzSwitch`, … |
| `scripts/make_icons.py` | مولد آیکون رستری (۲۵۶px، ۸× SS، شبکهٔ ۲۴ واحدی) |
| `build.sh` | بیلد کراس MinGW + strip + SHA-256 + smoke اختیاری (`AZ_SMOKE`) |

---

**نقطهٔ ادامه:** مورد ۲ فهرست بالا — بازطراحی `src/printer_designer.inc` با زبان طراحی ۱.۶۳.۰ و کمکی‌های مشترک موجود.
