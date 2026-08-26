/* fields.js — bindable data fields, grouped by category.
   The `key` values mirror the C++ printer tokens so a saved design prints
   real data. New keys here must also be handled in printer.cpp fieldValue(). */
window.AZ_FIELD_CATS = [
  { title:"تاریخ و ساعت", items:[
    { key:"{date}",  label:"تاریخ", sample:"۱۴۰۵/۰۴/۰۶" },
    { key:"{time}",  label:"ساعت", sample:"۱۰:۳۰" },
    { key:"{shift}", label:"شیفت", sample:"صبح" },
    { key:"{datetime}", label:"تاریخ و ساعت", sample:"۱۴۰۵/۰۴/۰۶ - ۱۰:۳۰" },
  ]},
  { title:"اطلاعات بیمار", items:[
    { key:"{first}",    label:"نام", sample:"علی" },
    { key:"{last}",     label:"نام خانوادگی", sample:"رضایی" },
    { key:"{full}",     label:"نام و نام خانوادگی", sample:"[P-Name]" },
    { key:"{P-Name}",   label:"P-Name", sample:"[P-Name]" },
    { key:"{father}",   label:"نام پدر", sample:"حسن" },
    { key:"{nid}",      label:"کد ملی", sample:"۰۰۱۲۳۴۵۶۷۸" },
    { key:"{nationalcard}", label:"شماره ملی (کارت)", sample:"۰۰۱۲۳۴۵۶۷۸" },
    { key:"{birth}",    label:"تاریخ تولد", sample:"۱۳۷۰/۰۵/۱۲" },
    { key:"{age}",      label:"سن (با واحد)", sample:"10Y" },
    { key:"{agenum}",   label:"سن (عدد — برای پسوند Y)", sample:"۳۵" },
    { key:"{gender}",   label:"جنسیت", sample:"مرد" },
    { key:"{mobile}",   label:"تلفن همراه", sample:"۰۹۱۲۰۰۰۰۰۰۰" },
    { key:"{landline}", label:"تلفن ثابت", sample:"۰۲۱۰۰۰۰۰۰۰" },
    { key:"{address}",  label:"آدرس", sample:"تهران، خیابان…" },
    { key:"{ptype}",    label:"نوع بیمار", sample:"سرپایی" },
    { key:"{barcode}",  label:"بارکد/کد ملی", sample:"۰۰۱۲۳۴۵۶۷۸" },
  ]},
  { title:"بیمه", items:[
    { key:"{ins}",      label:"بیمه اصلی", sample:"تأمین اجتماعی" },
    { key:"{supp}",     label:"بیمه مکمل", sample:"دانا" },
    { key:"{insno}",    label:"شماره دفترچه", sample:"۱۲۳۴۵۶" },
    { key:"{insexp}",   label:"اعتبار بیمه", sample:"۱۴۰۵/۱۲/۲۹" },
    { key:"{insidx}",   label:"کد بیمه", sample:"۲" },
  ]},
  { title:"پزشک و پذیرش", items:[
    { key:"{doctor}",   label:"پزشک معالج", sample:"دکتر احمدی" },
    { key:"{refdoctor}",label:"پزشک ارجاع‌دهنده", sample:"دکتر کریمی" },
    { key:"{dept}",     label:"بخش / دپارتمان", sample:"دندانپزشکی" },
    { key:"{room}",     label:"شماره اتاق / واحد", sample:"۳" },
    { key:"{queue}",    label:"شماره پذیرش", sample:"۱۲" },
    { key:"{apptdate}", label:"تاریخ پذیرش", sample:"۱۴۰۵/۰۴/۰۷" },
    { key:"{appttime}", label:"ساعت پذیرش", sample:"۱۱:۰۰" },
    { key:"{appttype}", label:"نوع مراجعه", sample:"عادی" },
    { key:"{visittype}",label:"نوع ویزیت", sample:"سرپایی" },
    { key:"{nextvisit}",label:"مراجعه بعدی", sample:"۱۴۰۵/۰۵/۰۱" },
    { key:"{regdate}",  label:"تاریخ ثبت", sample:"۱۴۰۵/۰۴/۰۶" },
    { key:"{regtime}",  label:"ساعت ثبت", sample:"۱۰:۳۰" },
  ]},
  { title:"بالینی و علائم حیاتی", items:[
    { key:"{weight}",    label:"وزن (kg)", sample:"۷۲" },
    { key:"{height}",    label:"قد (cm)", sample:"۱۷۵" },
    { key:"{bp}",        label:"فشار خون", sample:"۱۲۰/۸۰" },
    { key:"{temp}",      label:"دما (°C)", sample:"۳۷" },
    { key:"{pulse}",     label:"ضربان", sample:"۷۲" },
    { key:"{allergy}",   label:"حساسیت‌ها", sample:"—" },
    { key:"{diagnosis}", label:"تشخیص", sample:"—" },
  ]},
  { title:"خدمات", items:[
    { key:"{services}",      label:"لیست خدمات (جدول پویا)", sample:"جدول خدمات" },
    { key:"{servicescount}", label:"تعداد خدمات", sample:"۴" },
    { key:"{servicestotal}", label:"جمع خدمات", sample:"۲٬۵۰۰٬۰۰۰ ریال" },
  ]},
  { title:"مالی و صورتحساب", items:[
    { key:"{total}",    label:"جمع کل", sample:"۲٬۵۰۰٬۰۰۰ ریال" },
    { key:"{totalonly}",label:"جمع کل (بدون واحد)", sample:"۲٬۵۰۰٬۰۰۰" },
    { key:"{insshare}", label:"سهم بیمه", sample:"۱٬۰۰۰٬۰۰۰ ریال" },
    { key:"{insshareonly}", label:"سهم بیمه (بدون واحد)", sample:"۱٬۰۰۰٬۰۰۰" },
    { key:"{patientshare}", label:"سهم بیمار", sample:"۱٬۵۰۰٬۰۰۰ ریال" },
    { key:"{discount}", label:"مبلغ تخفیف", sample:"۲۰۰٬۰۰۰ ریال" },
    { key:"{finaltotal}", label:"مبلغ نهایی", sample:"۱٬۳۰۰٬۰۰۰ ریال" },
    { key:"{paid}",     label:"مبلغ پرداختی", sample:"۱٬۳۰۰٬۰۰۰ ریال" },
    { key:"{paidonly}", label:"پرداختی (بدون واحد)", sample:"۱٬۳۰۰٬۰۰۰" },
    { key:"{service}",  label:"شرح خدمت", sample:"ویزیت" },
    { key:"{servicecode}", label:"کد خدمت", sample:"۹۰۱۲۳۴" },
    { key:"{visitfee}", label:"حق ویزیت", sample:"۸۵۰٬۰۰۰ ریال" },
    { key:"{paytype}",  label:"نوع پرداخت", sample:"نقدی" },
    { key:"{cashier}",  label:"صندوقدار", sample:"صندوق ۱" },
  ]},
  { title:"درمانگاه / سامانه", items:[
    { key:"{clinic}",     label:"نام درمانگاه", sample:"درمانگاه درمان پلاس" },
    { key:"{clinicaddr}", label:"آدرس درمانگاه", sample:"تهران، میدان آزادی…" },
    { key:"{clinicphone}",label:"تلفن درمانگاه", sample:"۰۲۱۶۶۰۰۰۰۰۰" },
    { key:"{clinicmgr}",  label:"مسئول فنی", sample:"دکتر …" },
    { key:"{cliniclic}",  label:"شماره پروانه", sample:"۱۲۳۴۵" },
    { key:"{receiptNo}",  label:"شماره قبض", sample:"۱۰۰۲۳" },
    { key:"{user}",       label:"کاربر پذیرش", sample:"پذیرش ۱" },
    { key:"{shiftuser}",  label:"شیفت و کاربر", sample:"صبح — پذیرش ۱" },
    { key:"{issued}",     label:"چاپ توسط پذیرش", sample:"چاپ توسط: پذیرش ۱" },
  ]},

  /* =====================================================================
     v1.55.0 — «فیلدهای رسید واقعی» (بازطراحی بر اساس رسید کاغذی درمانگاه
     شبانه‌روزی ثامن‌الائمه). همهٔ کلیدهای زیر در printer.cpp → pdFieldValue()
     پاسخ داده می‌شوند و مقدارشان از رکورد زندهٔ پذیرش، نشست جاری یا
     جداول مرجع می‌آید. هیچ‌کدام هرگز تصادفی نیست: اگر کاربر مقداری را وارد
     نکرده باشد، توکن خالی چاپ می‌شود (نه یک عدد ساختگی).
     ⚠️ هیچ فیلد قدیمی حذف نشده است — فقط افزوده شده.
     ===================================================================== */
  { title:"زمان‌بندی رسید (v1.55)", items:[
    { key:"{apptdatetime}", label:"تاریخ و ساعت نوبت", sample:"۱۴۰۵/۰۴/۰۶  ۱۰:۳۰" },
    { key:"{apptsec}",      label:"ساعت نوبت (با ثانیه)", sample:"۱۰:۳۰:۴۵" },
    { key:"{reg_ts}",       label:"تاریخ و ساعت ثبت پذیرش", sample:"۱۴۰۵/۰۴/۰۶  ۱۰:۳۱" },
    { key:"{regdate}",      label:"تاریخ ثبت", sample:"۱۴۰۵/۰۴/۰۶" },
    { key:"{regtime}",      label:"ساعت ثبت", sample:"۱۰:۳۱" },
  ]},
  { title:"شناسهٔ رسید و بارکد (v1.55)", items:[
    { key:"{receiptbarcode}", label:"بارکد رسید (عدد قابل اسکن)", sample:"۴۶۴۹۰۰۱" },
    { key:"{receiptcode}",    label:"کد کوتاه رسید", sample:"۵۶Y" },
    { key:"{eprescription}",  label:"کد رهگیری نسخهٔ الکترونیک", sample:"۸۸۷۷۶۶۵" },
    { key:"{referralno}",     label:"شماره معرفی‌نامه", sample:"۱۴۲۵۳۶" },
    { key:"{scnum}",          label:"ش.ص (شمارهٔ صندوق)", sample:"۱" },
  ]},
  { title:"بیمه + درصد (v1.55)", items:[
    { key:"{ins_percent}",  label:"درصد بیمهٔ پایه", sample:"۷۰٪" },
    { key:"{supp_percent}", label:"درصد بیمهٔ مکمل", sample:"۹۰٪" },
    { key:"{ins_full}",     label:"بیمهٔ پایه + درصد", sample:"تأمین اجتماعی (۷۰٪)" },
    { key:"{supp_full}",    label:"بیمهٔ مکمل + درصد", sample:"دانا (۹۰٪)" },
  ]},
  { title:"پزشک، تخصص و انجام‌دهنده (v1.55)", items:[
    { key:"{doctorcode}",    label:"کد پزشک معالج", sample:"۱۲" },
    { key:"{performer}",     label:"نام انجام‌دهنده", sample:"دکتر احمدی" },
    { key:"{performercode}", label:"کد انجام‌دهنده", sample:"۱۲" },
    { key:"{specialty}",     label:"شرح تخصص", sample:"داخلی" },
    { key:"{specialtycode}", label:"کد تخصص", sample:"۱۲" },
  ]},
  { title:"خدمت — نام/شرح/نوع (v1.55)", items:[
    { key:"{servicename}", label:"نام خدمت", sample:"ویزیت پزشک عمومی" },
    { key:"{servicedesc}", label:"شرح خدمت", sample:"معاینهٔ سرپایی" },
    { key:"{servicetype}", label:"نوع خدمت", sample:"عمومی" },
  ]},
  { title:"مالی تفصیلی رسید (v1.55)", items:[
    { key:"{basepay}",       label:"سهم پایه (بیمهٔ اصلی)", sample:"۱٬۰۰۰٬۰۰۰ ریال" },
    { key:"{supppay}",       label:"سهم مکمل (سازمان)", sample:"۳۰۰٬۰۰۰ ریال" },
    { key:"{cash}",          label:"نقدی (صندوق)", sample:"۵۰۰٬۰۰۰ ریال" },
    { key:"{pos}",           label:"کارتخوان / POS", sample:"۸۰۰٬۰۰۰ ریال" },
    { key:"{discount_from}", label:"تخفیف (خالی اگر صفر)", sample:"۲۰۰٬۰۰۰ ریال" },
  ]},
  { title:"کارکنان پذیرش (v1.55)", items:[
    { key:"{receptionist}",  label:"نام اکانت پذیرشگر", sample:"پذیرش ۱" },
    { key:"{cashier_name}",  label:"نام صندوق‌دار", sample:"پذیرش ۱" },
  ]},
];

/* flat lookup by key -> {label,sample} */
window.AZ_FIELDS = (function(){
  var m = {};
  window.AZ_FIELD_CATS.forEach(function(c){ c.items.forEach(function(it){ m[it.key]=it; }); });
  return m;
})();
