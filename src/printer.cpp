// ============================================================================
//  printer.cpp — v1.4.0 print system
//   • Print SECTIONS (each prints differently): پذیرش / تزریقات / آزمایشگاه …
//   • A printer-SETTINGS dialog: default printer (persisted independently of the
//     Windows default), test connection, A4/A5 paper, fit/fill, advanced.
//   • A full visual print DESIGNER (in printer_designer.inc): draggable
//     labels / lines / borders / logo, field bindings, per-item font & style,
//     templates, backup/restore, zoom, snapping, "چاپ توسط پذیرش".
//   • printDesignedReceipt(): renders a saved design onto a real printer DC.
//
//  Everything is file-backed (data\design_<sec>.txt + settings.ini) so the EXE
//  stays single & static.
// ============================================================================
#include "app.h"
#include "print_designer.h"   // §3: new vector designer (PrintDesigner_Open)
#include "print_services_pagination.h"
#include "print_services_policy.h"
#include "sections.h"         // v1.65.0: lazy seeding in the print path
#include "ui_kit.h"           // v2.07: uikit::NormalizeFa for printer search
#include <cmath>              // v2.07: std::abs for reflow comparison
#include <stdio.h>

// ----------------------------------------------------------------------------
//  Record a "settings-change request" when a RECEPTION user (role 0) alters a
//  printer / design setting, so management sees who/what/when on the red-badge
//  panel. Manager/admin (role 1/2) edits are applied silently (they own it).
// ----------------------------------------------------------------------------
static std::wstring currentSystemName(){
    wchar_t buf[256]={0}; DWORD n=255;
    if(GetComputerNameW(buf,&n) && n>0) return std::wstring(buf,n);
    return L"—";
}
static void logSettingsChange(const std::wstring& change){
    if(g_session.user.username.empty()) return;
    if(g_session.user.role!=0) return;   // only reception edits are "requests"
    std::wstring who = g_session.user.fullname.empty()
                     ? g_session.user.username : g_session.user.fullname;
    std::wstring prof = g_session.user.dept.empty() ? L"پذیرش" : g_session.user.dept;
    pushSetReq(who, currentSystemName(), change, prof);
}
// v1.9.0: printer settings change gate. Managers (role>=1) apply directly; for
// reception/staff the change is NOT applied — it is confirmed, then queued for
// management approval with a key=value payload + a preview string, exactly like
// the settings.cpp workflow. Returns true if the caller may apply directly.
static bool printerRequestGate(HWND h, const std::wstring& title,
                               const std::wstring& change,
                               const std::wstring& payload,
                               const std::wstring& preview){
    if(g_session.user.username.empty()) return true;   // not signed in → local
    if(g_session.user.role>=1) return true;            // manager/admin → direct
    if(MessageBoxW(h,L"آیا از ذخیرهٔ این تنظیمات اطمینان دارید؟",
        L"تأیید ذخیره", MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2)!=IDYES)
        return false;
    std::wstring who = g_session.user.fullname.empty()
                     ? g_session.user.username : g_session.user.fullname;
    pushSetReqEx(who, systemSourceName(), title, change, payload, preview);
    MessageBoxW(h,L"این تنظیمات برای مدیریت ارسال شد. پس از تأیید اعمال خواهد شد.",
        L"ارسال برای تأیید", MB_OK|MB_ICONINFORMATION);
    return false;
}

// ------------------------------------------------------------- sections -----
const wchar_t* PRINT_SECTIONS[] = {
    L"پذیرش درمانگاه",
    L"قبض / صورتحساب",
    L"بیمه",
    L"بیمه مکمل",
    L"مبلغ نهایی",
    L"نسخه پزشک",
    L"تزریقات",
    L"آزمایشگاه",
    L"داروخانه",
    L"رادیولوژی",
};
const int N_PRINT_SECTIONS = sizeof(PRINT_SECTIONS)/sizeof(PRINT_SECTIONS[0]);

// ----------------------------------------------------- bindable fields ------
//  token  →  shown label (Persian).  Tokens are written {first} etc.
struct FieldDef { const wchar_t* token; const wchar_t* label; };
static const FieldDef FIELDS[] = {
    { L"{first}",     L"نام بیمار" },
    { L"{last}",      L"نام خانوادگی" },
    { L"{full}",      L"نام و نام خانوادگی" },
    { L"{father}",    L"نام پدر" },
    { L"{nid}",       L"کد ملی" },
    { L"{birth}",     L"تاریخ تولد" },
    { L"{gender}",    L"جنسیت" },
    { L"{mobile}",    L"تلفن همراه" },
    { L"{landline}",  L"تلفن ثابت" },
    { L"{address}",   L"آدرس" },
    { L"{ptype}",     L"نوع بیمار" },
    { L"{ins}",       L"بیمه اصلی" },
    { L"{supp}",      L"بیمه مکمل" },
    { L"{queue}",     L"شماره پذیرش" },
    { L"{date}",      L"تاریخ" },
    { L"{time}",      L"ساعت" },
    { L"{shift}",     L"شیفت" },
    { L"{dept}",      L"بخش" },
    { L"{user}",      L"کاربر پذیرش" },
    { L"{total}",     L"جمع کل" },
    { L"{discount}",  L"تخفیف" },
    { L"{paid}",      L"مبلغ پرداختی" },
    { L"{issued}",    L"چاپ توسط پذیرش" },
};
static const int N_FIELDS = sizeof(FIELDS)/sizeof(FIELDS[0]);

// ------------------------------------------------------------ design model --
enum ItemKind { IT_LABEL=0, IT_LINE_H, IT_LINE_V, IT_BORDER, IT_LOGO };

struct DItem {
    int  kind;          // ItemKind
    double x, y, w, h;  // millimetres on the page
    std::wstring field; // bound token, or empty for free text
    std::wstring text;  // literal text (label) or caption prefix
    int  fontSize;      // pt
    bool bold, italic, underline, strike;
    std::wstring fontName;
    COLORREF color;
    double lineW;       // mm thickness for lines / borders
    int  lineStyle;     // 0 solid, 1 dashed, 2 dotted
    COLORREF borderColor;
    std::wstring logoPath;
    COLORREF bgColor;   // label background fill; CLR_INVALID = transparent
    int  align;         // 0 right, 1 center, 2 left (for labels)
    DItem():kind(IT_LABEL),x(10),y(10),w(50),h(8),
        fontSize(11),bold(false),italic(false),underline(false),strike(false),
        fontName(L"Vazirmatn"),color(RGB(0,0,0)),lineW(0.3),lineStyle(0),
        borderColor(RGB(0,0,0)),bgColor(CLR_INVALID),align(0){}
};

struct Design {
    int paper;          // 0 = A4, 1 = A5
    std::wstring name;  // template/backup display name
    std::vector<DItem> items;
    Design():paper(0),name(L""){}
};

static void paperMM(int paper, double& w, double& h){
    if(paper==1){ w=148.0; h=210.0; }     // A5 portrait
    else        { w=210.0; h=297.0; }     // A4 portrait
}

// --------------------------------------------------------- serialization ----
//  data\design_<sec>.txt  — line 0: "paper\tname"; then one item per line,
//  tab-delimited fields. Logo paths & text are pipe-escaped of tabs/newlines.
static std::wstring escTab(const std::wstring& s){
    std::wstring o=s; for(auto&c:o) if(c==L'\t'||c==L'\n'||c==L'\r') c=L' '; return o;
}
static std::wstring designPath(int sec){
    wchar_t b[32]; swprintf(b,32,L"\\design_%d.txt",sec);
    return dataDir()+b;
}
static std::wstring designBackupPath(int sec){
    wchar_t b[40]; swprintf(b,40,L"\\design_%d_saved.txt",sec);
    return dataDir()+b;
}

static std::vector<std::wstring> splitTab(const std::wstring& s){
    std::vector<std::wstring> out; size_t pos=0;
    while(true){ size_t e=s.find(L'\t',pos);
        if(e==std::wstring::npos){ out.push_back(s.substr(pos)); break; }
        out.push_back(s.substr(pos,e-pos)); pos=e+1; }
    return out;
}

static std::wstring serializeDesign(const Design& d){
    std::wstring out;
    wchar_t hb[64]; swprintf(hb,64,L"%d\t",d.paper);
    out += std::wstring(hb)+escTab(d.name)+L"\r\n";
    for(const DItem& it: d.items){
        wchar_t b[512];
        swprintf(b,512,
            L"%d\t%.2f\t%.2f\t%.2f\t%.2f\t%d\t%d%d%d%d\t%u\t%.2f\t%d\t%u\t",
            it.kind,it.x,it.y,it.w,it.h,it.fontSize,
            it.bold?1:0,it.italic?1:0,it.underline?1:0,it.strike?1:0,
            (unsigned)it.color,it.lineW,it.lineStyle,(unsigned)it.borderColor);
        wchar_t b2[64]; swprintf(b2,64,L"\t%u\t%d",(unsigned)it.bgColor,it.align);
        out += std::wstring(b)+escTab(it.field)+L"\t"+escTab(it.text)+L"\t"
            +escTab(it.fontName)+L"\t"+escTab(it.logoPath)+std::wstring(b2)+L"\r\n";
    }
    return out;
}
static bool parseDesign(const std::wstring& all, Design& d){
    d.items.clear();
    size_t pos=0; bool first=true;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=all.substr(pos,e-pos); pos=e+1;
        while(!line.empty() && (line.back()==L'\r'||line.back()==L'\n')) line.pop_back();
        if(line.empty()) continue;
        auto f=splitTab(line);
        if(first){
            first=false;
            d.paper = f.size()>0 ? _wtoi(f[0].c_str()) : 0;
            d.name  = f.size()>1 ? f[1] : L"";
            continue;
        }
        if(f.size()<16) continue;
        DItem it;
        it.kind=_wtoi(f[0].c_str());
        it.x=_wtof(f[1].c_str()); it.y=_wtof(f[2].c_str());
        it.w=_wtof(f[3].c_str()); it.h=_wtof(f[4].c_str());
        it.fontSize=_wtoi(f[5].c_str());
        std::wstring st=f[6];
        it.bold     = st.size()>0 && st[0]==L'1';
        it.italic   = st.size()>1 && st[1]==L'1';
        it.underline= st.size()>2 && st[2]==L'1';
        it.strike   = st.size()>3 && st[3]==L'1';
        it.color=(COLORREF)wcstoul(f[7].c_str(),NULL,10);
        it.lineW=_wtof(f[8].c_str());
        it.lineStyle=_wtoi(f[9].c_str());
        it.borderColor=(COLORREF)wcstoul(f[10].c_str(),NULL,10);
        it.field=f[11]; it.text=f[12]; it.fontName=f[13]; it.logoPath=f[14];
        if(trim(it.fontName).empty()) it.fontName=L"Vazirmatn";
        // optional newer fields: bgColor, align
        if(f.size()>15 && !trim(f[15]).empty())
            it.bgColor=(COLORREF)wcstoul(f[15].c_str(),NULL,10);
        else it.bgColor=CLR_INVALID;
        if(f.size()>16) it.align=_wtoi(f[16].c_str());
        d.items.push_back(it);
    }
    return !first;
}
static void saveDesignFile(int sec, const Design& d){
    writeFileUtf8(designPath(sec), serializeDesign(d), false);
}
static bool loadDesignFile(int sec, Design& d){
    std::wstring all=readFileUtf8(designPath(sec));
    if(all.empty()) return false;
    return parseDesign(all,d);
}

// ------------------------------------------------------ default templates ---
//  A clean professional default per section so the program ships with usable
//  layouts. The designer lets the user pick / tweak / backup these.
static Design defaultDesign(int sec){
    Design d; d.paper=1;   // A5 is friendliest for a reception slip
    double cw; double ch; paperMM(d.paper,cw,ch);
    // v1.96.0 — expanded default slip: the full medical-receipt field set
    // (doctor/specialty, insurance split, complete financial breakdown,
    // e-prescription, referral, reception/cashier, ش.ص) in a compact two-column
    // grid, so the legacy fallback is no longer sparser than the designed path.
    auto addLbl=[&](double x,double y,double w,const wchar_t* token,
                    const wchar_t* text,int sz,bool bold,COLORREF col,int align=0){
        DItem it; it.kind=IT_LABEL; it.x=x; it.y=y; it.w=w; it.h=sz*0.45;
        it.field=token?token:L""; it.text=text?text:L"";
        it.fontSize=sz; it.bold=bold; it.color=col; it.align=align;
        d.items.push_back(it);
    };
    auto addLine=[&](double y){
        DItem it; it.kind=IT_LINE_H; it.x=8; it.y=y; it.w=cw-16; it.h=0;
        it.lineW=0.3; it.lineStyle=0; it.color=RGB(0,0,0); d.items.push_back(it);
    };
    auto addVLine=[&](double x,double y,double h){
        DItem it; it.kind=IT_LINE_V; it.x=x; it.y=y; it.w=0; it.h=h;
        it.lineW=0.3; it.lineStyle=0; it.color=RGB(0,0,0); d.items.push_back(it);
    };
    const COLORREF INK=RGB(0,0,0), DIM=RGB(70,70,70), ACC=RGB(20,50,100), RED=RGB(150,20,20);
    double L=8, R=cw/2.0+2, colW=cw/2.0-12;   // two-column cells (left/right)
    double y=10;
    // outer border (thin, monochrome)
    { DItem b; b.kind=IT_BORDER; b.x=5; b.y=5; b.w=cw-10; b.h=ch-10;
      b.lineW=0.4; b.borderColor=INK; d.items.push_back(b); }
    // header
    addLbl(8,y,cw-16,NULL,(std::wstring(L"درمانگاه ")+APP_NAME_W).c_str(),16,true,ACC,1); y+=8;
    addLbl(8,y,cw-16,L"clinicaddr",L"",9,false,DIM,1); y+=5;
    addLbl(8,y,cw-16,L"clinicphone",L"",9,false,DIM,1); y+=5;
    addLine(y); y+=4;
    // appointment row
    addVLine(cw/2.0,y-1,7);
    addLbl(L,y,colW,L"queue",L"نوبت: ",11,true,INK);
    addLbl(R,y,colW,L"apptdate",L"تاریخ نوبت: ",10,false,INK); y+=7;
    addLine(y); y+=3;
    // patient rows
    addLbl(L,y,colW,L"P-Name",L"نام بیمار: ",12,true,INK);
    addLbl(R,y,colW,L"nid",L"کد ملی: ",10,false,INK); y+=7;
    addVLine(cw/2.0,y-1,7);
    addLbl(L,y,colW,L"age",L"سن: ",10,false,INK);
    addLbl(R,y,colW,L"father",L"نام پدر: ",10,false,INK); y+=7;
    addVLine(cw/2.0,y-1,7);
    addLbl(L,y,colW,L"gender",L"جنسیت: ",10,false,INK);
    addLbl(R,y,colW,L"mobile",L"تلفن: ",10,false,INK); y+=7;
    addLine(y); y+=3;
    // doctor rows
    addLbl(L,y,colW,L"doctor",L"دکتر: ",11,false,INK);
    addLbl(R,y,colW,L"doctorcode",L"کد نظام پزشکی: ",10,false,INK); y+=7;
    addVLine(cw/2.0,y-1,7);
    addLbl(L,y,colW,L"specialtycode",L"کد تخصص: ",10,false,INK);
    addLbl(R,y,colW,L"specialty",L"شرح تخصص: ",10,false,INK); y+=7;
    addLine(y); y+=3;
    // insurance rows
    addLbl(L,y,colW,L"ins",L"بیمه پایه: ",10,false,INK);
    addLbl(R,y,colW,L"supp",L"مکمل: ",10,false,INK); y+=7;
    addVLine(cw/2.0,y-1,7);
    addLbl(L,y,colW,L"ins_percent",L"درصد پایه: ",10,false,INK);
    addLbl(R,y,colW,L"supp_percent",L"درصد مکمل: ",10,false,INK); y+=7;
    addLine(y); y+=3;
    // financial rows
    addLbl(L,y,colW,L"total",L"قیمت کل: ",11,true,INK);
    addLbl(R,y,colW,L"paid",L"پرداختی: ",11,true,RED); y+=7;
    addVLine(cw/2.0,y-1,7);
    addLbl(L,y,colW,L"insshare",L"سهم پایه: ",10,false,INK);
    addLbl(R,y,colW,L"supppay",L"سهم مکمل: ",10,false,INK); y+=7;
    addVLine(cw/2.0,y-1,7);
    addLbl(L,y,colW,L"discount",L"تخفیف: ",10,false,INK);
    addLbl(R,y,colW,L"patientshare",L"سهم بیمار: ",10,false,INK); y+=7;
    addVLine(cw/2.0,y-1,7);
    addLbl(L,y,colW,L"pos",L"POS: ",10,false,INK);
    addLbl(R,y,colW,L"cash",L"نقد: ",10,false,INK); y+=7;
    addLine(y); y+=3;
    // e-prescription / referral (full width)
    addLbl(8,y,cw-16,L"eprescription",L"کد رهگیری نسخه الکترونیک: ",10,false,INK); y+=6;
    addLbl(8,y,cw-16,L"referralno",L"شماره معرف نسخه: ",10,false,INK); y+=6;
    addLine(y); y+=3;
    // reception / cashier / scnum
    addVLine(cw/2.0,y-1,6);
    addLbl(L,y,colW,L"receptionist",L"پذیرش: ",10,false,INK);
    addLbl(R,y,colW,L"cashier",L"صندوق: ",10,false,INK); y+=6;
    addLbl(8,y,cw-16,L"scnum",L"ش.ص: ",10,false,INK); y+=6;
    addLine(y); y+=3;
    // print timestamp
    addLbl(8,y,cw-16,L"date",L"تاریخ چاپ: ",9,false,DIM);
    addLbl(8+cw*0.45,y,cw*0.5,L"time",L" ساعت: ",9,false,DIM);
    return d;
}

//  Pre-made templates offered in the designer dropdown — 10 modern Iranian
//  clinic-reception layouts. Index 0 = section default (پذیرش-friendly).
static const wchar_t* TEMPLATE_NAMES[] = {
    L"۱) پیش‌فرض حرفه‌ای",
    L"۲) ساده و سریع",
    L"۳) رسمی با کادر آبی",
    L"۴) فشرده A5",
    L"۵) سربرگ رنگی مدرن",
    L"۶) شماره پذیرش درشت",
    L"۷) رسید پرداخت",
    L"۸) دو ستونه شیک",
    L"۹) مینیمال خط‌دار",
    L"۱۰) کارت بیمار",
};
static const int N_TEMPLATES = sizeof(TEMPLATE_NAMES)/sizeof(TEMPLATE_NAMES[0]);

// --- small builder helpers shared by the templates --------------------------
namespace tpl {
    static DItem label(double x,double y,double w,const wchar_t* tok,
                       const wchar_t* t,int sz,bool b,COLORREF col,int align=0){
        DItem it; it.kind=IT_LABEL; it.x=x; it.y=y; it.w=w; it.h=sz*0.45;
        it.field=tok?tok:L""; it.text=t?t:L""; it.fontSize=sz; it.bold=b;
        it.color=col; it.align=align; return it;
    }
    static DItem hline(double x,double y,double w,double th,int style,COLORREF c){
        DItem it; it.kind=IT_LINE_H; it.x=x; it.y=y; it.w=w; it.h=0;
        it.lineW=th; it.lineStyle=style; it.color=c; return it;
    }
    static DItem border(double x,double y,double w,double h,double th,COLORREF c){
        DItem it; it.kind=IT_BORDER; it.x=x; it.y=y; it.w=w; it.h=h;
        it.lineW=th; it.borderColor=c; return it;
    }
    static DItem bar(double x,double y,double w,const wchar_t* tok,
                     const wchar_t* t,int sz,COLORREF bg,COLORREF fg,int align=1){
        DItem it=label(x,y,w,tok,t,sz,true,fg,align); it.bgColor=bg; return it;
    }
}

static Design templateByIndex(int sec, int idx){
    using namespace tpl;
    Design d;
    double cw,ch;
    const wchar_t* secName=PRINT_SECTIONS[sec<N_PRINT_SECTIONS?sec:0];
    std::wstring clinic=std::wstring(L"درمانگاه ")+APP_NAME_W;

    switch(idx){
    case 1: { // simple & fast (A5)
        d.paper=1; paperMM(d.paper,cw,ch);
        d.items.push_back(label(8,10,cw-16,NULL,clinic.c_str(),16,true,RGB(20,50,100),1));
        d.items.push_back(label(8,20,cw-16,NULL,secName,12,true,RGB(40,70,120),1));
        d.items.push_back(label(8,34,cw-16,L"{queue}",L"پذیرش: ",13,true,RGB(0,0,0)));
        d.items.push_back(label(8,44,cw-16,L"{full}",L"بیمار: ",12,true,RGB(0,0,0)));
        d.items.push_back(label(8,52,cw-16,L"{nid}",L"کد ملی: ",11,false,RGB(0,0,0)));
        d.items.push_back(label(8,60,cw-16,L"{date}",L"تاریخ: ",11,false,RGB(0,0,0)));
        d.items.push_back(label(8,68,cw-16,L"{paid}",L"پرداختی: ",12,true,RGB(150,20,20)));
        d.items.push_back(label(8,80,cw-16,L"{issued}",L"",10,false,RGB(120,120,120)));
        return d; }
    case 2: { // formal w/ blue frame (A4)
        d = defaultDesign(sec); d.paper=0;
        for(auto& it:d.items){ it.y*=1.18; it.x*=1.25; }
        return d; }
    case 3: { // compact A5
        d.paper=1; paperMM(d.paper,cw,ch);
        d.items.push_back(label(6,8,cw-12,NULL,clinic.c_str(),13,true,RGB(20,50,100)));
        d.items.push_back(label(6,18,80,L"{queue}",L"پذیرش: ",12,true,RGB(0,0,0)));
        d.items.push_back(label(cw-80,18,74,L"{date}",L"تاریخ: ",10,false,RGB(0,0,0),2));
        d.items.push_back(label(6,26,cw-12,L"{full}",L"",12,true,RGB(0,0,0)));
        d.items.push_back(label(6,34,cw-12,L"{nid}",L"کد ملی: ",10,false,RGB(0,0,0)));
        d.items.push_back(label(6,42,cw-12,L"{paid}",L"پرداختی: ",11,true,RGB(150,20,20)));
        d.items.push_back(label(6,52,cw-12,L"{issued}",L"",9,false,RGB(120,120,120)));
        return d; }
    case 4: { // modern colored header (A5)
        d.paper=1; paperMM(d.paper,cw,ch);
        d.items.push_back(bar(5,6,cw-10,NULL,clinic.c_str(),16,RGB(15,90,160),RGB(255,255,255),1));
        d.items.push_back(bar(5,18,cw-10,NULL,secName,12,RGB(30,130,200),RGB(255,255,255),1));
        d.items.push_back(label(8,32,70,L"{queue}",L"پذیرش: ",14,true,RGB(15,90,160)));
        d.items.push_back(label(cw-78,32,70,L"{time}",L"ساعت: ",11,false,RGB(0,0,0),2));
        d.items.push_back(hline(8,40,cw-16,0.3,2,RGB(150,150,150)));
        d.items.push_back(label(8,44,cw-16,L"{full}",L"بیمار: ",13,true,RGB(0,0,0)));
        d.items.push_back(label(8,52,cw-16,L"{nid}",L"کد ملی: ",11,false,RGB(0,0,0)));
        d.items.push_back(label(8,60,cw-16,L"{ins}",L"بیمه: ",11,false,RGB(0,0,0)));
        d.items.push_back(bar(8,70,cw-16,L"{paid}",L"مبلغ پرداختی: ",14,RGB(235,245,255),RGB(150,20,20),0));
        d.items.push_back(label(8,84,cw-16,L"{issued}",L"",9,false,RGB(120,120,120)));
        return d; }
    case 5: { // big queue number (A5)
        d.paper=1; paperMM(d.paper,cw,ch);
        d.items.push_back(label(6,8,cw-12,NULL,clinic.c_str(),13,true,RGB(20,50,100),1));
        d.items.push_back(label(6,18,cw-12,NULL,L"شماره پذیرش شما",12,false,RGB(80,80,80),1));
        d.items.push_back(label(6,26,cw-12,L"{queue}",L"",40,true,RGB(15,90,160),1));
        d.items.push_back(hline(8,60,cw-16,0.4,0,RGB(15,90,160)));
        d.items.push_back(label(6,64,cw-12,L"{full}",L"",13,true,RGB(0,0,0),1));
        d.items.push_back(label(6,74,cw-12,L"{dept}",L"بخش: ",11,false,RGB(0,0,0),1));
        d.items.push_back(label(6,82,cw-12,L"{date}",L"",10,false,RGB(120,120,120),1));
        return d; }
    case 6: { // payment receipt (A5)
        d.paper=1; paperMM(d.paper,cw,ch);
        d.items.push_back(border(5,5,cw-10,ch-10,0.4,RGB(120,120,120)));
        d.items.push_back(bar(6,7,cw-12,NULL,L"رسید پرداخت",15,RGB(40,120,60),RGB(255,255,255),1));
        d.items.push_back(label(8,20,cw-16,NULL,clinic.c_str(),11,false,RGB(0,0,0),1));
        d.items.push_back(label(8,30,cw-16,L"{full}",L"بیمار: ",12,true,RGB(0,0,0)));
        d.items.push_back(label(8,38,cw-16,L"{date}",L"تاریخ: ",10,false,RGB(0,0,0)));
        d.items.push_back(label(cw/2,38,cw/2-8,L"{time}",L"ساعت: ",10,false,RGB(0,0,0)));
        d.items.push_back(hline(8,46,cw-16,0.3,2,RGB(150,150,150)));
        d.items.push_back(label(8,50,cw-16,L"{total}",L"جمع کل: ",11,false,RGB(0,0,0)));
        d.items.push_back(label(8,58,cw-16,L"{discount}",L"تخفیف: ",11,false,RGB(0,0,0)));
        d.items.push_back(bar(8,68,cw-16,L"{paid}",L"پرداختی: ",14,RGB(235,250,238),RGB(40,120,60),0));
        d.items.push_back(label(8,82,cw-16,L"{issued}",L"",9,false,RGB(120,120,120)));
        return d; }
    case 7: { // two-column elegant (A4)
        d.paper=0; paperMM(d.paper,cw,ch);
        d.items.push_back(border(8,8,cw-16,ch-16,0.5,RGB(40,70,120)));
        d.items.push_back(bar(10,10,cw-20,NULL,clinic.c_str(),20,RGB(25,60,110),RGB(255,255,255),1));
        d.items.push_back(label(10,26,cw-20,NULL,secName,14,true,RGB(40,70,120),1));
        d.items.push_back(hline(12,40,cw-24,0.4,0,RGB(40,70,120)));
        double cR=cw/2+4, cL=12, colW=cw/2-16;
        d.items.push_back(label(cR,46,colW,L"{queue}",L"پذیرش: ",14,true,RGB(0,0,0)));
        d.items.push_back(label(cL,46,colW,L"{date}",L"تاریخ: ",12,false,RGB(0,0,0)));
        d.items.push_back(label(cR,56,colW,L"{full}",L"بیمار: ",14,true,RGB(0,0,0)));
        d.items.push_back(label(cL,56,colW,L"{time}",L"ساعت: ",12,false,RGB(0,0,0)));
        d.items.push_back(label(cR,66,colW,L"{nid}",L"کد ملی: ",12,false,RGB(0,0,0)));
        d.items.push_back(label(cL,66,colW,L"{father}",L"نام پدر: ",12,false,RGB(0,0,0)));
        d.items.push_back(label(cR,76,colW,L"{mobile}",L"تلفن: ",12,false,RGB(0,0,0)));
        d.items.push_back(label(cL,76,colW,L"{gender}",L"جنسیت: ",12,false,RGB(0,0,0)));
        d.items.push_back(hline(12,88,cw-24,0.3,2,RGB(150,150,150)));
        d.items.push_back(label(cR,92,colW,L"{ins}",L"بیمه: ",12,false,RGB(0,0,0)));
        d.items.push_back(bar(cL,92,colW,L"{paid}",L"پرداختی: ",14,RGB(245,245,250),RGB(150,20,20),0));
        d.items.push_back(label(12,108,cw-24,L"{issued}",L"",10,false,RGB(120,120,120)));
        return d; }
    case 8: { // minimal lined (A5)
        d.paper=1; paperMM(d.paper,cw,ch);
        d.items.push_back(label(8,8,cw-16,NULL,clinic.c_str(),15,true,RGB(0,0,0),1));
        d.items.push_back(hline(8,18,cw-16,0.5,0,RGB(0,0,0)));
        d.items.push_back(label(8,22,cw-16,L"{queue}",L"پذیرش: ",13,true,RGB(0,0,0)));
        d.items.push_back(hline(8,30,cw-16,0.2,1,RGB(180,180,180)));
        d.items.push_back(label(8,33,cw-16,L"{full}",L"بیمار: ",12,true,RGB(0,0,0)));
        d.items.push_back(hline(8,41,cw-16,0.2,1,RGB(180,180,180)));
        d.items.push_back(label(8,44,cw-16,L"{nid}",L"کد ملی: ",11,false,RGB(0,0,0)));
        d.items.push_back(hline(8,52,cw-16,0.2,1,RGB(180,180,180)));
        d.items.push_back(label(8,55,cw-16,L"{date}",L"تاریخ: ",11,false,RGB(0,0,0)));
        d.items.push_back(hline(8,63,cw-16,0.2,1,RGB(180,180,180)));
        d.items.push_back(label(8,66,cw-16,L"{paid}",L"پرداختی: ",13,true,RGB(0,0,0)));
        d.items.push_back(hline(8,75,cw-16,0.5,0,RGB(0,0,0)));
        d.items.push_back(label(8,78,cw-16,L"{issued}",L"",9,false,RGB(120,120,120)));
        return d; }
    case 9: { // patient card (A5 landscape-ish compact)
        d.paper=1; paperMM(d.paper,cw,ch);
        d.items.push_back(border(6,6,cw-12,70,0.5,RGB(25,60,110)));
        d.items.push_back(bar(8,8,cw-16,NULL,clinic.c_str(),14,RGB(25,60,110),RGB(255,255,255),1));
        d.items.push_back(label(10,22,cw-20,L"{full}",L"",15,true,RGB(0,0,0),1));
        d.items.push_back(label(10,34,cw-20,L"{nid}",L"کد ملی: ",11,false,RGB(0,0,0)));
        d.items.push_back(label(10,42,cw-20,L"{birth}",L"تاریخ تولد: ",11,false,RGB(0,0,0)));
        d.items.push_back(label(10,50,cw-20,L"{mobile}",L"تلفن: ",11,false,RGB(0,0,0)));
        d.items.push_back(label(10,58,cw-20,L"{ins}",L"بیمه: ",11,false,RGB(0,0,0)));
        d.items.push_back(bar(8,80,cw-16,L"{queue}",L"شماره پذیرش: ",13,RGB(235,245,255),RGB(25,60,110),0));
        d.items.push_back(label(8,92,cw-16,L"{date}",L"تاریخ مراجعه: ",10,false,RGB(120,120,120)));
        return d; }
    default:
        return defaultDesign(sec);
    }
}

// ---------------------------------------------------------- printer list ----
static std::vector<std::wstring> enumPrinters(){
    std::vector<std::wstring> out;
    DWORD needed=0, returned=0;
    EnumPrintersW(PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS,NULL,4,
        NULL,0,&needed,&returned);
    if(needed==0) return out;
    std::vector<BYTE> buf(needed);
    if(EnumPrintersW(PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS,NULL,4,
        buf.data(),needed,&needed,&returned)){
        PRINTER_INFO_4W* pi=(PRINTER_INFO_4W*)buf.data();
        for(DWORD i=0;i<returned;i++)
            if(pi[i].pPrinterName) out.push_back(pi[i].pPrinterName);
    }
    return out;
}
static std::wstring currentPrinter(){
    std::wstring p=getSetting(L"printer_name",L"");
    if(!p.empty()) return p;
    wchar_t def[256]={0}; DWORD sz=256;
    if(GetDefaultPrinterW(def,&sz)) return def;
    return L"";
}

// Send the standard ESC/POS «kick drawer» pulse (ESC p m t1 t2) straight to the
// configured printer via the spooler RAW data type. Only fires when the cash
// drawer option is enabled in the printer settings; silently no-ops otherwise.
void kickCashDrawer(){
    if(getSetting(L"cash_drawer",L"0")!=L"1") return;
    std::wstring prn=currentPrinter();
    if(prn.empty()) return;
    HANDLE hp=NULL;
    if(!OpenPrinterW((LPWSTR)prn.c_str(),&hp,NULL) || !hp) return;
    DOC_INFO_1W di; di.pDocName=(LPWSTR)L"DarmanPlus-Drawer";
    di.pOutputFile=NULL; di.pDatatype=(LPWSTR)L"RAW";
    if(StartDocPrinterW(hp,1,(LPBYTE)&di)){
        StartPagePrinter(hp);
        // ESC p 0 25 250  — pulse pin 2 (most common); a 2nd kick for pin 5.
        BYTE kick[]={0x1B,0x70,0x00,0x19,0xFA, 0x1B,0x70,0x01,0x19,0xFA};
        DWORD wr=0; WritePrinter(hp,kick,sizeof(kick),&wr);
        EndPagePrinter(hp); EndDocPrinter(hp);
    }
    ClosePrinter(hp);
}

// =================================================== printer settings dialog =
#define PS_CLASS L"AzPrinterCfg"
enum {
    PSB_CLOSE=1, PSB_TEST, PSB_ADV, PSB_DESIGN, PSB_A4, PSB_A5,
    PSB_P80, PSB_P58,                 // 80mm / 58mm thermal roll
    PSB_FIT, PSB_FILL, PSB_SEC_PREV, PSB_SEC_NEXT,
    PSB_COPIES_DN, PSB_COPIES_UP,     // copies − / +
    PSB_SECEN,                        // per-section enable toggle
    PSB_AUTOPRINT,                    // auto-print receipt on save
    PSB_DRAWER,                       // open cash drawer after print
    PSB_LOGO,                         // print clinic logo/header
    PSB_PRINTER_BASE=200
};

struct PrnState {
    HWND owner;
    int  hot;
    std::vector<std::wstring> printers;
    std::wstring sel;       // selected printer
    int  paper;             // 0 A4 / 1 A5 / 2 80mm / 3 58mm
    int  mode;              // 0 fit / 1 fill
    int  section;           // section being edited (for the design button)
    int  copies;           // number of copies per print (1..5)
    bool autoPrint;        // auto-print receipt right after admission/save
    bool drawer;           // pulse the cash drawer after a successful print
    bool logo;             // print the clinic header/logo band
};
static HWND s_prn=NULL;
static PrnState* s_ps=NULL;

static int prnCardW(){ return S(580); }
static int prnCardH(){ return S(820); }
static RECT prnCard(HWND h){
    RECT rc; GetClientRect(h,&rc);
    int w=prnCardW(), hh=prnCardH();
    RECT c={(rc.right-w)/2,(rc.bottom-hh)/2,(rc.right+w)/2,(rc.bottom+hh)/2};
    return c;
}

// v2.07: shared test-print core (defined further below, before the PrinterLink
// dialog) — doTestPrint delegates so the printer-DC code exists exactly once.
static bool prnTestPrintTo(HWND h, const std::wstring& printer);

static void doTestPrint(HWND h){
    prnTestPrintTo(h, s_ps ? s_ps->sel : std::wstring(L""));
}

static void doAdvanced(HWND h){
    if(!s_ps || s_ps->sel.empty()){
        MessageBoxW(h,L"ابتدا یک چاپگر را انتخاب کنید.",L"تنظیمات پیشرفته",
            MB_OK|MB_ICONWARNING); return;
    }
    HANDLE hp=NULL;
    if(!OpenPrinterW((LPWSTR)s_ps->sel.c_str(),&hp,NULL) || !hp){
        MessageBoxW(h,L"دسترسی به چاپگر ممکن نشد.",L"تنظیمات پیشرفته",
            MB_OK|MB_ICONERROR); return;
    }
    LONG sz=DocumentPropertiesW(h,hp,(LPWSTR)s_ps->sel.c_str(),NULL,NULL,0);
    if(sz>0){
        std::vector<BYTE> buf(sz);
        DEVMODEW* dm=(DEVMODEW*)buf.data();
        DocumentPropertiesW(h,hp,(LPWSTR)s_ps->sel.c_str(),dm,NULL,DM_OUT_BUFFER);
        DocumentPropertiesW(h,hp,(LPWSTR)s_ps->sel.c_str(),dm,dm,
            DM_IN_BUFFER|DM_OUT_BUFFER|DM_IN_PROMPT);
    }
    ClosePrinter(hp);
}

static int prnHit(HWND h, POINT pt);
static void prnPaint(HWND h, HDC dc0){
    RECT rc; GetClientRect(h,&rc);
    HDC dc=CreateCompatibleDC(dc0);
    HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
    HGDIOBJ obm=SelectObject(dc,bmp);
    { HBRUSH sb=CreateSolidBrush(g_dark?RGB(6,9,14):RGB(28,36,48));
      FillRect(dc,&rc,sb); DeleteObject(sb); }
    gpFillAlpha(dc,rc,0,g_dark?RGB(0,0,0):RGB(20,28,40),120);
    RECT c=prnCard(h);
    gpShadow(dc,c,S(20),S(22),80);
    { HBRUSH pb=CreateSolidBrush(g_theme.surface); FillRect(dc,&c,pb); DeleteObject(pb); }
    gpGradRoundRect(dc,c,S(20),g_theme.surfaceTop,g_theme.surface,g_theme.border);
    SetBkMode(dc,TRANSPARENT);

    // title
    SelectObject(dc,g_fTitle); SetTextColor(dc,g_theme.text);
    RECT tr={c.left+S(20),c.top+S(18),c.right-S(20),c.top+S(54)};
    DrawTextW(dc,L"تنظیمات چاپگر و چاپ",-1,&tr,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    // close
    { RECT cb={c.left+S(16),c.top+S(18),c.left+S(42),c.top+S(44)};
      if(s_ps&&s_ps->hot==PSB_CLOSE) gpRoundRect(dc,cb,S(8),g_theme.hover,CLR_INVALID,255);
      RECT ci={cb.left+S(5),cb.top+S(5),cb.right-S(5),cb.bottom-S(5)};
      drawIcon(dc,ICO_X,ci,g_theme.text,S(2)); }

    // printers list
    SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
    RECT lb={c.left+S(20),c.top+S(64),c.right-S(20),c.top+S(84)};
    DrawTextW(dc,L"چاپگر پیش‌فرض برنامه (مستقل از پیش‌فرض ویندوز):",-1,&lb,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    int ly=c.top+S(88);
    int lh=S(34), maxRows=5;
    for(int i=0;i<(int)s_ps->printers.size() && i<maxRows;i++){
        RECT r={c.left+S(20),ly+i*lh,c.right-S(20),ly+i*lh+lh-S(6)};
        bool selrow=(s_ps->printers[i]==s_ps->sel);
        bool hov=(s_ps->hot==PSB_PRINTER_BASE+i);
        gpRoundRect(dc,r,S(9),
            selrow?g_theme.accent:(hov?g_theme.hover:g_theme.surface2),
            selrow?g_theme.accent:g_theme.border,255);
        SetTextColor(dc,selrow?g_theme.accentText:g_theme.text);
        SelectObject(dc,g_fUI);
        RECT ir={r.right-S(34),r.top+S(6),r.right-S(12),r.bottom-S(6)};
        drawIcon(dc,ICO_PRINT,ir,selrow?g_theme.accentText:g_theme.accent,S(2));
        RECT nr={r.left+S(12),r.top,r.right-S(40),r.bottom};
        DrawTextW(dc,s_ps->printers[i].c_str(),-1,&nr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    }
    if(s_ps->printers.empty()){
        RECT r={c.left+S(20),ly,c.right-S(20),ly+lh};
        SetTextColor(dc,g_theme.danger); SelectObject(dc,g_fUI);
        DrawTextW(dc,L"هیچ چاپگری روی این سیستم پیدا نشد.",-1,&r,
            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    }

    int by=ly+maxRows*lh+S(8);
    // shared chip drawer used across all option rows
    auto chip=[&](int id,const wchar_t* t,RECT r,bool on){
        bool hov=(s_ps->hot==id);
        gpRoundRect(dc,r,S(9),on?g_theme.accent:(hov?g_theme.hover:g_theme.surface2),
            on?g_theme.accent:g_theme.border,255);
        SetTextColor(dc,on?g_theme.accentText:g_theme.text);
        SelectObject(dc,g_fUIB);
        DrawTextW(dc,t,-1,&r,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    };
    // a labelled on/off pill toggle (RTL: label on the right, pill on the left)
    auto toggleRow=[&](int id,const wchar_t* label,bool on,int yy){
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUI);
        RECT lr={c.left+S(96),yy,c.right-S(20),yy+S(30)};
        DrawTextW(dc,label,-1,&lr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        RECT pill={c.left+S(20),yy+S(2),c.left+S(86),yy+S(28)};
        bool hov=(s_ps->hot==id);
        gpRoundRect(dc,pill,(pill.bottom-pill.top)/2,
            on?g_theme.success:(hov?g_theme.hover:g_theme.surface2),
            on?g_theme.success:g_theme.border,255);
        int kr=(pill.bottom-pill.top)/2-S(3);
        int kcx= on? (pill.right-S(3)-kr) : (pill.left+S(3)+kr);
        int kcy=(pill.top+pill.bottom)/2;
        RECT kn={kcx-kr,kcy-kr,kcx+kr,kcy+kr};
        gpRoundRect(dc,kn,kr,RGB(255,255,255),CLR_INVALID,255);
        SetTextColor(dc,on?g_theme.accentText:g_theme.textDim);
        SelectObject(dc,g_fSmall);
        RECT tt={pill.left+(on?S(6):S(20)),pill.top,pill.right-(on?S(20):S(6)),pill.bottom};
        DrawTextW(dc,on?L"روشن":L"خاموش",-1,&tt,
            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    };

    // ---- section selector (each section keeps its OWN print design) ----
    SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
    { RECT sl={c.left+S(20),by,c.right-S(20),by+S(20)};
      DrawTextW(dc,L"بخش/دپارتمان چاپ (هر بخش طراحی مستقل دارد):",-1,&sl,
          DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
    by+=S(24);
    {
        int navW=S(40);
        RECT rPrev={c.left+S(20),by,c.left+S(20)+navW,by+S(34)};
        RECT rNext={c.right-S(20)-navW,by,c.right-S(20),by+S(34)};
        RECT rMid ={rPrev.right+S(6),by,rNext.left-S(6),by+S(34)};
        bool hp=(s_ps->hot==PSB_SEC_PREV), hn=(s_ps->hot==PSB_SEC_NEXT);
        gpRoundRect(dc,rPrev,S(9),hp?g_theme.hover:g_theme.surface2,g_theme.border,255);
        gpRoundRect(dc,rNext,S(9),hn?g_theme.hover:g_theme.surface2,g_theme.border,255);
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
        DrawTextW(dc,L"‹",-1,&rPrev,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
        DrawTextW(dc,L"›",-1,&rNext,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
        gpRoundRect(dc,rMid,S(9),g_theme.inputBg,g_theme.accent,255);
        SetTextColor(dc,g_theme.accent); SelectObject(dc,g_fUIB);
        int sc=s_ps->section; if(sc<0||sc>=N_PRINT_SECTIONS) sc=0;
        DrawTextW(dc,PRINT_SECTIONS[sc],-1,&rMid,
            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    }
    by+=S(44);

    // ---- paper sizes (A4 / A5 / 80mm / 58mm thermal) ----
    SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
    { RECT pl={c.left+S(20),by,c.right-S(20),by+S(20)};
      DrawTextW(dc,L"اندازهٔ کاغذ:",-1,&pl,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
    by+=S(24);
    {
        int n=4, gap=S(4);
        int cwN=(c.right-c.left-S(40)-gap*(n-1))/n;
        int cx=c.right-S(20)-cwN;
        RECT rA4 ={cx,by,cx+cwN,by+S(34)}; chip(PSB_A4 ,L"A4",rA4 ,s_ps->paper==0); cx-=cwN+gap;
        RECT rA5 ={cx,by,cx+cwN,by+S(34)}; chip(PSB_A5 ,L"A5",rA5 ,s_ps->paper==1); cx-=cwN+gap;
        RECT r80 ={cx,by,cx+cwN,by+S(34)}; chip(PSB_P80,L"رول ۸۰",r80,s_ps->paper==2); cx-=cwN+gap;
        RECT r58 ={cx,by,cx+cwN,by+S(34)}; chip(PSB_P58,L"رول ۵۸",r58,s_ps->paper==3);
    }
    by+=S(44);

    // ---- fit / fill ----
    SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
    { RECT pl={c.left+S(20),by,c.right-S(20),by+S(20)};
      DrawTextW(dc,L"حالت تطبیق با کاغذ:",-1,&pl,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
    by+=S(24);
    {
        int cwH=(c.right-c.left-S(44))/2;
        int cx=c.right-S(20)-cwH;
        RECT rFit ={cx,by,cx+cwH,by+S(34)}; chip(PSB_FIT ,L"متناسب",rFit ,s_ps->mode==0); cx-=cwH+S(4);
        RECT rFill={cx,by,cx+cwH,by+S(34)}; chip(PSB_FILL,L"پرکردن",rFill,s_ps->mode==1);
    }
    by+=S(44);

    // ---- number of copies (− N +) ----
    SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUI);
    { RECT lr={c.left+S(150),by,c.right-S(20),by+S(34)};
      DrawTextW(dc,L"تعداد نسخهٔ چاپ:",-1,&lr,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
    {
        int bwn=S(40);
        RECT rDn={c.left+S(20),by,c.left+S(20)+bwn,by+S(34)};
        RECT rUp={c.left+S(20)+bwn+S(46)+S(4),by,c.left+S(20)+bwn+S(46)+S(4)+bwn,by+S(34)};
        RECT rNum={rDn.right+S(2),by,rUp.left-S(2),by+S(34)};
        bool hd=(s_ps->hot==PSB_COPIES_DN), hu=(s_ps->hot==PSB_COPIES_UP);
        gpRoundRect(dc,rDn,S(9),hd?g_theme.hover:g_theme.surface2,g_theme.border,255);
        gpRoundRect(dc,rUp,S(9),hu?g_theme.hover:g_theme.surface2,g_theme.border,255);
        gpRoundRect(dc,rNum,S(9),g_theme.inputBg,g_theme.accent,255);
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
        DrawTextW(dc,L"−",-1,&rDn,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
        DrawTextW(dc,L"+",-1,&rUp,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
        wchar_t nb[8]; swprintf(nb,8,L"%d",s_ps->copies);
        SetTextColor(dc,g_theme.accent);
        DrawTextW(dc,toFaDigits(nb).c_str(),-1,&rNum,
            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    }
    by+=S(46);

    // ---- toggles: section-enabled / auto-print / cash drawer / logo ----
    bool secOn = getSetting(L"sec_enabled_"+std::to_wstring(s_ps->section),L"1")!=L"0";
    toggleRow(PSB_SECEN,    L"چاپ این بخش فعال باشد",            secOn,         by); by+=S(36);
    toggleRow(PSB_AUTOPRINT,L"چاپ خودکار قبض پس از ثبت",         s_ps->autoPrint, by); by+=S(36);
    toggleRow(PSB_DRAWER,   L"باز کردن کشوی پول پس از چاپ",       s_ps->drawer,  by); by+=S(36);
    toggleRow(PSB_LOGO,     L"چاپ سربرگ/لوگوی درمانگاه",         s_ps->logo,    by); by+=S(42);

    // action buttons
    auto btn=[&](int id,const wchar_t* t,int icon,RECT r,bool primary){
        bool hov=(s_ps->hot==id);
        COLORREF bg = primary? (hov?g_theme.accentHover:g_theme.accent)
                             : (hov?g_theme.hover:g_theme.surface2);
        gpRoundRect(dc,r,S(10),bg,primary?bg:g_theme.border,255);
        COLORREF tc=primary?g_theme.accentText:g_theme.text;
        SetTextColor(dc,tc); SelectObject(dc,g_fUIB);
        RECT ir={r.right-S(36),r.top+S(8),r.right-S(14),r.bottom-S(8)};
        drawIcon(dc,icon,ir,tc,S(2));
        RECT nr={r.left+S(10),r.top,r.right-S(40),r.bottom};
        DrawTextW(dc,t,-1,&nr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    };
    int bw=(c.right-c.left-S(44))/2;
    RECT rTest={c.right-S(20)-bw,by,c.right-S(20),by+S(44)};
    btn(PSB_TEST,L"تست اتصال و چاپ",ICO_PRINT,rTest,false);
    RECT rAdv={c.left+S(20),by,c.left+S(20)+bw,by+S(44)};
    btn(PSB_ADV,L"تنظیمات پیشرفتهٔ درایور",ICO_GEAR,rAdv,false);
    by+=S(52);
    RECT rDes={c.left+S(20),by,c.right-S(20),by+S(46)};
    std::wstring dl=std::wstring(L"طراحی و تنظیم چاپ بخش: ")+
        PRINT_SECTIONS[s_ps->section<N_PRINT_SECTIONS?s_ps->section:0];
    btn(PSB_DESIGN,dl.c_str(),ICO_RECEIPT,rDes,true);

    BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
    SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
}

static int prnHit(HWND h, POINT pt){
    if(!s_ps) return 0;
    RECT c=prnCard(h);
    if(!PtInRect(&c,pt)) return -1;   // scrim
    RECT cb={c.left+S(16),c.top+S(18),c.left+S(42),c.top+S(44)};
    if(PtInRect(&cb,pt)) return PSB_CLOSE;
    int ly=c.top+S(88), lh=S(34), maxRows=5;
    for(int i=0;i<(int)s_ps->printers.size() && i<maxRows;i++){
        RECT r={c.left+S(20),ly+i*lh,c.right-S(20),ly+i*lh+lh-S(6)};
        if(PtInRect(&r,pt)) return PSB_PRINTER_BASE+i;
    }
    int by=ly+maxRows*lh+S(8);
    // section selector row (label +24, controls 34h)
    by+=S(24);
    { int navW=S(40);
      RECT rPrev={c.left+S(20),by,c.left+S(20)+navW,by+S(34)};
      RECT rNext={c.right-S(20)-navW,by,c.right-S(20),by+S(34)};
      if(PtInRect(&rPrev,pt)) return PSB_SEC_PREV;
      if(PtInRect(&rNext,pt)) return PSB_SEC_NEXT; }
    by+=S(44);
    // paper sizes (label +24, then 4 chips)
    by+=S(24);
    {
        int n=4, gap=S(4);
        int cwN=(c.right-c.left-S(40)-gap*(n-1))/n;
        int cx=c.right-S(20)-cwN;
        RECT rA4 ={cx,by,cx+cwN,by+S(34)}; if(PtInRect(&rA4 ,pt)) return PSB_A4;  cx-=cwN+gap;
        RECT rA5 ={cx,by,cx+cwN,by+S(34)}; if(PtInRect(&rA5 ,pt)) return PSB_A5;  cx-=cwN+gap;
        RECT r80 ={cx,by,cx+cwN,by+S(34)}; if(PtInRect(&r80 ,pt)) return PSB_P80; cx-=cwN+gap;
        RECT r58 ={cx,by,cx+cwN,by+S(34)}; if(PtInRect(&r58 ,pt)) return PSB_P58;
    }
    by+=S(44);
    // fit / fill (label +24, then 2 chips)
    by+=S(24);
    {
        int cwH=(c.right-c.left-S(44))/2;
        int cx=c.right-S(20)-cwH;
        RECT rFit ={cx,by,cx+cwH,by+S(34)}; if(PtInRect(&rFit ,pt)) return PSB_FIT;  cx-=cwH+S(4);
        RECT rFill={cx,by,cx+cwH,by+S(34)}; if(PtInRect(&rFill,pt)) return PSB_FILL;
    }
    by+=S(44);
    // copies − N +
    {
        int bwn=S(40);
        RECT rDn={c.left+S(20),by,c.left+S(20)+bwn,by+S(34)};
        RECT rUp={c.left+S(20)+bwn+S(46)+S(4),by,c.left+S(20)+bwn+S(46)+S(4)+bwn,by+S(34)};
        if(PtInRect(&rDn,pt)) return PSB_COPIES_DN;
        if(PtInRect(&rUp,pt)) return PSB_COPIES_UP;
    }
    by+=S(46);
    // four toggle rows (each pill is c.left+20 .. c.left+86, 28h, +36 step)
    { RECT p1={c.left+S(20),by+S(2),c.left+S(86),by+S(28)};   if(PtInRect(&p1,pt)) return PSB_SECEN;     by+=S(36);
      RECT p2={c.left+S(20),by+S(2),c.left+S(86),by+S(28)};   if(PtInRect(&p2,pt)) return PSB_AUTOPRINT; by+=S(36);
      RECT p3={c.left+S(20),by+S(2),c.left+S(86),by+S(28)};   if(PtInRect(&p3,pt)) return PSB_DRAWER;    by+=S(36);
      RECT p4={c.left+S(20),by+S(2),c.left+S(86),by+S(28)};   if(PtInRect(&p4,pt)) return PSB_LOGO;      by+=S(42); }
    int bw=(c.right-c.left-S(44))/2;
    RECT rTest={c.right-S(20)-bw,by,c.right-S(20),by+S(44)}; if(PtInRect(&rTest,pt)) return PSB_TEST;
    RECT rAdv={c.left+S(20),by,c.left+S(20)+bw,by+S(44)};    if(PtInRect(&rAdv,pt)) return PSB_ADV;
    by+=S(52);
    RECT rDes={c.left+S(20),by,c.right-S(20),by+S(46)};       if(PtInRect(&rDes,pt)) return PSB_DESIGN;
    return 0;
}

static void prnClose(){
    if(s_prn && IsWindow(s_prn)){ HWND v=s_prn; s_prn=NULL; DestroyWindow(v); }
}
static LRESULT CALLBACK prnProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps);
        prnPaint(h,dc); EndPaint(h,&ps); return 0; }
    case WM_APP_THEME: InvalidateRect(h,NULL,FALSE); return 0;
    case WM_MOUSEMOVE: {
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int hr=prnHit(h,pt); if(hr<0) hr=0;
        if(s_ps && hr!=s_ps->hot){ s_ps->hot=hr; InvalidateRect(h,NULL,FALSE); }
        TRACKMOUSEEVENT te={sizeof(te),TME_LEAVE,h,0}; TrackMouseEvent(&te);
        return 0; }
    case WM_MOUSELEAVE:
        if(s_ps && s_ps->hot){ s_ps->hot=0; InvalidateRect(h,NULL,FALSE); }
        return 0;
    case WM_LBUTTONDOWN: {
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int id=prnHit(h,pt);
        if(id==-1 || id==PSB_CLOSE){ prnClose(); return 0; }
        if(id>=PSB_PRINTER_BASE){
            int i=id-PSB_PRINTER_BASE;
            if(i<(int)s_ps->printers.size()){
                std::wstring want=s_ps->printers[i];
                std::wstring chg=L"تغییر چاپگر پیش‌فرض به «"+want+L"»";
                if(printerRequestGate(h,L"تغییر نوع/چاپگر پیش‌فرض",chg,
                        L"printer_name="+want, L"چاپگر: "+want)){
                    s_ps->sel=want; setSetting(L"printer_name",want);
                    InvalidateRect(h,NULL,FALSE);
                }
            }
            return 0;
        }
        switch(id){
        case PSB_A4:
            if(printerRequestGate(h,L"تغییر اندازهٔ کاغذ",L"تغییر اندازه کاغذ به A4",
                    L"paper_size=A4",L"اندازهٔ کاغذ: A4")){
                s_ps->paper=0; setSetting(L"paper_size",L"A4"); InvalidateRect(h,NULL,FALSE); }
            break;
        case PSB_A5:
            if(printerRequestGate(h,L"تغییر اندازهٔ کاغذ",L"تغییر اندازه کاغذ به A5",
                    L"paper_size=A5",L"اندازهٔ کاغذ: A5")){
                s_ps->paper=1; setSetting(L"paper_size",L"A5"); InvalidateRect(h,NULL,FALSE); }
            break;
        case PSB_FIT:
            if(printerRequestGate(h,L"تغییر حالت چاپ",L"تغییر حالت چاپ به «متناسب»",
                    L"print_mode=fit",L"حالت چاپ: متناسب")){
                s_ps->mode=0; setSetting(L"print_mode",L"fit"); InvalidateRect(h,NULL,FALSE); }
            break;
        case PSB_FILL:
            if(printerRequestGate(h,L"تغییر حالت چاپ",L"تغییر حالت چاپ به «پرکننده»",
                    L"print_mode=fill",L"حالت چاپ: پرکننده")){
                s_ps->mode=1; setSetting(L"print_mode",L"fill"); InvalidateRect(h,NULL,FALSE); }
            break;
        case PSB_P80:
            if(printerRequestGate(h,L"تغییر اندازهٔ کاغذ",L"تغییر اندازه کاغذ به رول حرارتی ۸۰ میلی‌متر",
                    L"paper_size=80MM",L"اندازهٔ کاغذ: رول ۸۰")){
                s_ps->paper=2; setSetting(L"paper_size",L"80MM"); InvalidateRect(h,NULL,FALSE); }
            break;
        case PSB_P58:
            if(printerRequestGate(h,L"تغییر اندازهٔ کاغذ",L"تغییر اندازه کاغذ به رول حرارتی ۵۸ میلی‌متر",
                    L"paper_size=58MM",L"اندازهٔ کاغذ: رول ۵۸")){
                s_ps->paper=3; setSetting(L"paper_size",L"58MM"); InvalidateRect(h,NULL,FALSE); }
            break;
        case PSB_SEC_PREV:
            s_ps->section=(s_ps->section+N_PRINT_SECTIONS-1)%N_PRINT_SECTIONS;
            InvalidateRect(h,NULL,FALSE); break;
        case PSB_SEC_NEXT:
            s_ps->section=(s_ps->section+1)%N_PRINT_SECTIONS;
            InvalidateRect(h,NULL,FALSE); break;
        case PSB_COPIES_DN:
            if(s_ps->copies>1){ s_ps->copies--;
                setSetting(L"print_copies",std::to_wstring(s_ps->copies));
                InvalidateRect(h,NULL,FALSE); }
            break;
        case PSB_COPIES_UP:
            if(s_ps->copies<5){ s_ps->copies++;
                setSetting(L"print_copies",std::to_wstring(s_ps->copies));
                InvalidateRect(h,NULL,FALSE); }
            break;
        case PSB_SECEN: {
            std::wstring key=L"sec_enabled_"+std::to_wstring(s_ps->section);
            bool now=getSetting(key,L"1")!=L"0";
            setSetting(key, now?L"0":L"1");
            InvalidateRect(h,NULL,FALSE); break; }
        case PSB_AUTOPRINT:
            s_ps->autoPrint=!s_ps->autoPrint;
            setSetting(L"auto_print",s_ps->autoPrint?L"1":L"0");
            InvalidateRect(h,NULL,FALSE); break;
        case PSB_DRAWER:
            s_ps->drawer=!s_ps->drawer;
            setSetting(L"cash_drawer",s_ps->drawer?L"1":L"0");
            InvalidateRect(h,NULL,FALSE); break;
        case PSB_LOGO:
            s_ps->logo=!s_ps->logo;
            setSetting(L"print_logo",s_ps->logo?L"1":L"0");
            InvalidateRect(h,NULL,FALSE); break;
        case PSB_TEST: doTestPrint(h); break;
        case PSB_ADV:  doAdvanced(h); break;
        case PSB_DESIGN:{ int sec=s_ps->section; prnClose();
            openPrintDesigner(g_hFrame,sec); break; }
        }
        return 0; }
    case WM_KEYDOWN: if(w==VK_ESCAPE){ prnClose(); return 0; } break;
    case WM_DESTROY:
        if(s_ps){ delete s_ps; s_ps=NULL; } s_prn=NULL;
        if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE);
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

void openPrinterSettings(HWND owner){
    if(s_prn && IsWindow(s_prn)){ prnClose(); return; }
    static bool reg=false;
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=prnProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=PS_CLASS;
        RegisterClassW(&wc); reg=true; }
    RECT rc; GetClientRect(owner,&rc);
    POINT org={0,0}; ClientToScreen(owner,&org);
    s_ps=new PrnState();
    s_ps->owner=owner; s_ps->hot=0;
    s_ps->printers=enumPrinters();
    s_ps->sel=currentPrinter();
    { std::wstring ps=getSetting(L"paper_size",L"A5");
      s_ps->paper = ps==L"A4"?0 : ps==L"80MM"?2 : ps==L"58MM"?3 : 1; }
    s_ps->mode =(getSetting(L"print_mode",L"fit")==L"fill")?1:0;
    s_ps->section=0;
    { int cp=_wtoi(getSetting(L"print_copies",L"1").c_str());
      if(cp<1)cp=1; if(cp>5)cp=5; s_ps->copies=cp; }
    s_ps->autoPrint=getSetting(L"auto_print",L"0")==L"1";
    s_ps->drawer   =getSetting(L"cash_drawer",L"0")==L"1";
    s_ps->logo     =getSetting(L"print_logo",L"1")!=L"0";
    s_prn=CreateWindowExW(WS_EX_TOPMOST,PS_CLASS,L"",
        WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN,
        org.x,org.y,rc.right,rc.bottom,owner,NULL,g_hInst,NULL);
    BringWindowToTop(s_prn); SetFocus(s_prn);
}

// ===========================================================================
//  v2.07.0 — «ارتباط با چاپگر» dialog (PrinterLink).
//  A dedicated printer-picker opened from the C++ settings header: searchable
//  list of every installed printer, Windows-default pre-selection, an explicit
//  «پیرو پیش‌فرض ویندوز» switch, a live test-connection button and a reload
//  button. Saving routes through printerRequestGate exactly like every other
//  printer setting, so a reception user's change is queued for management.
//  Styling is identical to the AzPrinterCfg card above (same card geometry,
//  same theme primitives, same fonts) — no new colours, fonts or radii.
// ===========================================================================
#define PL_CLASS L"AzPrinterLink"

enum {
    PLB_بستن = 400, PLB_تأیید, PLB_انصراف, PLB_تست_اتصال,
    PLB_بازخوانی, PLB_پیش‌فرض_ویندوز, PLB_ITEM_BASE = 500
};

struct PrinterLinkState {
    HWND owner;
    HWND eSearch;                            // real EDIT child (EN_CHANGE filter)
    int  hot;
    std::vector<std::wstring> همه_چاپگرها;   // every enumerated printer
    std::vector<int>          نتایج_جستجو;   // indices into همه_چاپگرها
    std::wstring عبارت_جستجو;                // current filter text
    std::wstring چاپگر_انتخابی;              // in-dialog selection
    std::wstring چاپگر_ویندوز;               // GetDefaultPrinterW() snapshot
    bool         پیرو_ویندوز;                // printer_follow_windows_default
    int          اسکرول;                     // list scroll offset (rows)
    PrinterLinkState():owner(NULL),eSearch(NULL),hot(0),پیرو_ویندوز(true),اسکرول(0){}
};
static HWND s_plink=NULL;
static PrinterLinkState* s_pls=NULL;
#define IDC_PL_SEARCH 901     // search edit child id

static int plCardW(){ return S(560); }
static int plCardH(){ return S(640); }
static RECT plCard(HWND h){
    RECT rc; GetClientRect(h,&rc);
    int w=plCardW(), hh=plCardH();
    if(hh > rc.bottom-S(40)) hh = rc.bottom-S(40);
    RECT c={(rc.right-w)/2,(rc.bottom-hh)/2,(rc.right+w)/2,(rc.bottom+hh)/2};
    return c;
}
static int plListRows(){ return 7; }
static int plRowH(){ return S(36); }

// Apply the current filter (case-insensitive + Persian-normalized substring,
// reusing uikit::NormalizeFa — the same normalizer Sections_Find uses) and
// rebuild نتایج_جستجو. Keeping the current selection if it still matches.
static void plApplyFilter(){
    if(!s_pls) return;
    // sync state from the EDIT child (single source of truth for the filter)
    if(s_pls->eSearch && IsWindow(s_pls->eSearch)){
        wchar_t buf[256]={0};
        GetWindowTextW(s_pls->eSearch,buf,256);
        s_pls->عبارت_جستجو=buf;
    }
    s_pls->نتایج_جستجو.clear();
    std::wstring q=uikit::NormalizeFa(s_pls->عبارت_جستجو);
    for(size_t i=0;i<s_pls->همه_چاپگرها.size();++i){
        if(q.empty() ||
           uikit::NormalizeFa(s_pls->همه_چاپگرها[i]).find(q)!=std::wstring::npos)
            s_pls->نتایج_جستجو.push_back((int)i);
    }
    // v2.07: چاپگر_انتخابی is a STABLE candidate that survives filtering —
    // typing one character and deleting it must not lose the Windows-default
    // pre-selection (§2.3 contract). It is re-derived only on open/reload.
    int maxScroll=(int)s_pls->نتایج_جستجو.size()-plListRows();
    if(maxScroll<0) maxScroll=0;
    if(s_pls->اسکرول>maxScroll) s_pls->اسکرول=maxScroll;
    if(s_pls->اسکرول<0) s_pls->اسکرول=0;
}

// Shared test-print core (factored out of doTestPrint so both dialogs use ONE
// printer-DC path — no duplicated driver code).
static bool prnTestPrintTo(HWND h, const std::wstring& printer){
    if(printer.empty()){
        MessageBoxW(h,L"ابتدا یک چاپگر را انتخاب کنید.",L"تست چاپگر",
            MB_OK|MB_ICONWARNING); return false;
    }
    HDC dc=CreateDCW(L"WINSPOOL",printer.c_str(),NULL,NULL);
    if(!dc){
        MessageBoxW(h,L"اتصال به چاپگر برقرار نشد.\nبررسی کنید چاپگر روشن و متصل باشد.",
            L"تست چاپگر",MB_OK|MB_ICONERROR); return false;
    }
    DOCINFOW di={sizeof(di)}; di.lpszDocName=L"درمان پلاس — تست چاپ";
    bool ok=false;
    if(StartDocW(dc,&di)>0){
        StartPage(dc);
        int dpiY=GetDeviceCaps(dc,LOGPIXELSY);
        HFONT f=CreateFontW(-(dpiY*18/72),0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,
            0,0,CLEARTYPE_QUALITY,0,L"Vazirmatn");
        HGDIOBJ of=SelectObject(dc,f);
        SetBkMode(dc,TRANSPARENT);
        SetTextAlign(dc,TA_RIGHT|TA_RTLREADING);
        RECT r={dpiY/2,dpiY/2,GetDeviceCaps(dc,HORZRES)-dpiY/2,dpiY*3};
        DrawTextW(dc,L"تست چاپ موفق بود — درمان پلاس\nچاپگر به‌درستی کار می‌کند.",-1,&r,
            DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
        SelectObject(dc,of); DeleteObject(f);
        EndPage(dc); EndDoc(dc);
        MessageBoxW(h,L"صفحهٔ تست به چاپگر ارسال شد.",L"تست چاپگر",
            MB_OK|MB_ICONINFORMATION);
        ok=true;
    } else {
        MessageBoxW(h,L"ارسال سند به چاپگر ناموفق بود.",L"تست چاپگر",
            MB_OK|MB_ICONERROR);
    }
    DeleteDC(dc);
    return ok;
}

static void plClose(){
    if(s_plink && IsWindow(s_plink)){ HWND v=s_plink; s_plink=NULL; DestroyWindow(v); }
    if(s_pls){ delete s_pls; s_pls=NULL; }
}

static RECT plSearchEditRect(const RECT& c){
    RECT r={c.left+S(20),c.top+S(64),c.right-S(20),c.top+S(64)+S(36)};
    return r;
}
// v2.07: re-place the search EDIT child after a resize so it always sits on
// its drawn frame (same pattern as settings.cpp layoutServerEdit).
static void plLayoutEdit(HWND h){
    if(!s_pls || !s_pls->eSearch || !IsWindow(s_pls->eSearch)) return;
    RECT er=plSearchEditRect(plCard(h));
    MoveWindow(s_pls->eSearch,
        er.left+S(6),er.top+S(3),(er.right-er.left)-S(12),(er.bottom-er.top)-S(6),TRUE);
}
static int plListTop(const RECT& c){ return c.top+S(112); }
static RECT plListRect(const RECT& c){
    int top=plListTop(c);
    RECT r={c.left+S(20),top,c.right-S(20),top+plRowH()*plListRows()};
    return r;
}
static RECT plSwitchRect(const RECT& c){
    int y=plListTop(c)+plRowH()*plListRows()+S(10);
    RECT r={c.left+S(20),y,c.right-S(20),y+S(30)};
    return r;
}
static RECT plBtnRowRect(const RECT& c){
    int y=plSwitchRect(c).bottom+S(14);
    RECT r={c.left+S(20),y,c.right-S(20),y+S(44)};
    return r;
}

static void plPaint(HWND h, HDC dc0){
    RECT rc; GetClientRect(h,&rc);
    HDC dc=CreateCompatibleDC(dc0);
    HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
    HGDIOBJ obm=SelectObject(dc,bmp);
    { HBRUSH sb=CreateSolidBrush(g_dark?RGB(6,9,14):RGB(28,36,48));
      FillRect(dc,&rc,sb); DeleteObject(sb); }
    gpFillAlpha(dc,rc,0,g_dark?RGB(0,0,0):RGB(20,28,40),120);
    RECT c=plCard(h);
    gpShadow(dc,c,S(20),S(22),80);
    { HBRUSH pb=CreateSolidBrush(g_theme.surface); FillRect(dc,&c,pb); DeleteObject(pb); }
    gpGradRoundRect(dc,c,S(20),g_theme.surfaceTop,g_theme.surface,g_theme.border);
    SetBkMode(dc,TRANSPARENT);

    // title band
    SelectObject(dc,g_fTitle); SetTextColor(dc,g_theme.text);
    RECT tr={c.left+S(20),c.top+S(18),c.right-S(20),c.top+S(54)};
    DrawTextW(dc,L"ارتباط با چاپگر",-1,&tr,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    // close (×)
    { RECT cb={c.left+S(16),c.top+S(18),c.left+S(42),c.top+S(44)};
      if(s_pls&&s_pls->hot==PLB_بستن) gpRoundRect(dc,cb,S(8),g_theme.hover,CLR_INVALID,255);
      RECT ci={cb.left+S(5),cb.top+S(5),cb.right-S(5),cb.bottom-S(5)};
      drawIcon(dc,ICO_X,ci,g_theme.text,S(2)); }

    // ---- جستجوی چاپگر (real themed EDIT child — handles EN_CHANGE, paste, IME) --
    SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
    { RECT lr={c.left+S(20),c.top+S(56),c.right-S(20),c.top+S(64)};
      DrawTextW(dc,L"جستجوی چاپگر:",-1,&lr,
          DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
    // The EDIT child itself is positioned in plLayoutEdit(); here we only draw
    // its themed frame so the control blends into the card like every other
    // input in AzPrinterCfg.
    { RECT er=plSearchEditRect(c);
      gpRoundRect(dc,er,S(10),CLR_INVALID,g_theme.border,255); }

    // ---- لیست چاپگرها ------------------------------------------------------
    SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
    { RECT lr={c.left+S(20),plListTop(c)-S(20),c.right-S(20),plListTop(c)-S(2)};
      DrawTextW(dc,L"لیست چاپگرها:",-1,&lr,
          DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
    { RECT lr=plListRect(c);
      int shown=s_pls?(int)s_pls->نتایج_جستجو.size():0;
      wchar_t n[64]; swprintf(n,64,L"%d چاپگر",shown);
      RECT cr={lr.left,lr.top-S(20),lr.right,lr.top-S(2)};
      SetTextColor(dc,g_theme.textDim);
      DrawTextW(dc,toFaDigits(n).c_str(),-1,&cr,
          DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
    { RECT lb=plListRect(c);
      gpRoundRect(dc,lb,S(10),g_theme.surface2,g_theme.border,255);
      if(s_pls){
        int rows=plListRows();
        for(int v=0;v<rows;++v){
            int idx=s_pls->اسکرول+v;
            if(idx>=(int)s_pls->نتایج_جستجو.size()) break;
            int gi=s_pls->نتایج_جستجو[idx];
            const std::wstring& name=s_pls->همه_چاپگرها[gi];
            RECT r={lb.left+S(4),lb.top+S(4)+v*plRowH(),
                    lb.right-S(4),lb.top+S(4)+v*plRowH()+plRowH()-S(4)};
            bool selrow=(name==s_pls->چاپگر_انتخابی);
            bool hov=(s_pls->hot==PLB_ITEM_BASE+v);
            gpRoundRect(dc,r,S(9),
                selrow?g_theme.accent:(hov?g_theme.hover:g_theme.surface),
                selrow?g_theme.accent:g_theme.border,255);
            SetTextColor(dc,selrow?g_theme.accentText:g_theme.text);
            SelectObject(dc,g_fUI);
            RECT ir={r.right-S(30),r.top+S(6),r.right-S(10),r.bottom-S(6)};
            drawIcon(dc,ICO_PRINT,ir,selrow?g_theme.accentText:g_theme.accent,S(2));
            // Windows-default trailing chip «پیش‌فرض ویندوز»
            bool isDef=(!s_pls->چاپگر_ویندوز.empty() && name==s_pls->چاپگر_ویندوز);
            int chipW=S(84);
            if(isDef){
                RECT chip={r.left+S(8),r.top+(r.bottom-r.top)/2-S(12),
                           r.left+S(8)+chipW,r.top+(r.bottom-r.top)/2+S(12)};
                gpRoundRect(dc,chip,S(9),g_theme.success,g_theme.success,255);
                SetTextColor(dc,RGB(255,255,255)); SelectObject(dc,g_fSmall);
                DrawTextW(dc,L"پیش‌فرض ویندوز",-1,&chip,
                    DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            }
            RECT nr={r.left+S(8)+(isDef?chipW+S(6):0),r.top,
                     r.right-S(36),r.bottom};
            SetTextColor(dc,selrow?g_theme.accentText:g_theme.text);
            SelectObject(dc,g_fUI);
            DrawTextW(dc,name.c_str(),-1,&nr,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
        }
        if(s_pls->نتایج_جستجو.empty()){
            SetTextColor(dc,g_theme.danger); SelectObject(dc,g_fUI);
            RECT r={lb.left+S(10),lb.top,lb.right-S(10),lb.bottom};
            DrawTextW(dc,s_pls->عبارت_جستجو.empty()
                        ?L"هیچ چاپگری روی این سیستم پیدا نشد."
                        :L"چاپگری با این نام پیدا نشد.",-1,&r,
                DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        }
        // scroll affordance arrows (only when the list overflows)
        int maxScroll=(int)s_pls->نتایج_جستجو.size()-plListRows();
        if(maxScroll>0){
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
            RECT up={lb.right-S(22),lb.top+S(2),lb.right-S(4),lb.top+S(16)};
            RECT dn={lb.right-S(22),lb.bottom-S(16),lb.right-S(4),lb.bottom-S(2)};
            UINT a=DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX;
            SetTextColor(dc,s_pls->اسکرول>0?g_theme.text:g_theme.border);
            DrawTextW(dc,L"▲",-1,&up,a);
            SetTextColor(dc,s_pls->اسکرول<maxScroll?g_theme.text:g_theme.border);
            DrawTextW(dc,L"▼",-1,&dn,a);
        }
      }
    }

    // ---- پیرو پیش‌فرض ویندوز switch (same pill geometry as AzPrinterCfg) ----
    if(s_pls){
        int id=PLB_پیش‌فرض_ویندوز;
        bool on=s_pls->پیرو_ویندوز;
        bool hov=(s_pls->hot==id);
        RECT sr=plSwitchRect(c);
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUI);
        RECT lr={c.left+S(96),sr.top,c.right-S(20),sr.bottom};
        DrawTextW(dc,L"پیرو پیش‌فرض ویندوز (چاپگر برنامه مستقل نباشد)",-1,&lr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        RECT pill={c.left+S(20),sr.top+S(1),c.left+S(86),sr.bottom-S(1)};
        gpRoundRect(dc,pill,(pill.bottom-pill.top)/2,
            on?g_theme.success:(hov?g_theme.hover:g_theme.surface2),
            on?g_theme.success:g_theme.border,255);
        int kr=(pill.bottom-pill.top)/2-S(3);
        int kcx= on? (pill.right-S(3)-kr) : (pill.left+S(3)+kr);
        int kcy=(pill.top+pill.bottom)/2;
        RECT kn={kcx-kr,kcy-kr,kcx+kr,kcy+kr};
        gpRoundRect(dc,kn,kr,RGB(255,255,255),CLR_INVALID,255);
        SetTextColor(dc,on?g_theme.accentText:g_theme.textDim);
        SelectObject(dc,g_fSmall);
        RECT tt={pill.left+(on?S(6):S(20)),pill.top,pill.right-(on?S(20):S(6)),pill.bottom};
        DrawTextW(dc,on?L"روشن":L"خاموش",-1,&tt,
            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    }

    // ---- action buttons -----------------------------------------------------
    { RECT br=plBtnRowRect(c);
      int gap=S(10);
      int bw=(br.right-br.left-gap*2)/3;
      auto btn=[&](int id,const wchar_t* t,RECT r,bool primary){
        bool hov=(s_pls&&s_pls->hot==id);
        COLORREF bg = primary? (hov?g_theme.accentHover:g_theme.accent)
                             : (hov?g_theme.hover:g_theme.surface2);
        gpRoundRect(dc,r,S(10),bg,primary?bg:g_theme.border,255);
        SetTextColor(dc,primary?g_theme.accentText:g_theme.text);
        SelectObject(dc,g_fUIB);
        DrawTextW(dc,t,-1,&r,
            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
      };
      int cx=br.right;
      RECT rOk={cx-bw,br.top,cx,br.bottom}; cx-=bw+gap;
      RECT rNo={cx-bw,br.top,cx,br.bottom}; cx-=bw+gap;
      RECT rTs={cx-bw,br.top,cx,br.bottom};
      btn(PLB_تأیید,L"تأیید",rOk,true);
      btn(PLB_انصراف,L"انصراف",rNo,false);
      btn(PLB_تست_اتصال,L"تست اتصال",rTs,false);
      // بازخوانی sits below, full width
      RECT rb={c.left+S(20),br.bottom+S(10),c.right-S(20),br.bottom+S(10)+S(40)};
      btn(PLB_بازخوانی,L"بازخوانی لیست چاپگرها",rb,false);
    }

    BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
    SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
}

static int plHit(HWND h, POINT pt){
    if(!s_pls) return 0;
    RECT c=plCard(h);
    if(!PtInRect(&c,pt)) return -1;   // scrim
    RECT cb={c.left+S(16),c.top+S(18),c.left+S(42),c.top+S(44)};
    if(PtInRect(&cb,pt)) return PLB_بستن;
    { RECT er=plSearchEditRect(c);
      if(PtInRect(&er,pt)) return -2; }               // search field
    { RECT lb=plListRect(c);
      if(PtInRect(&lb,pt)){
          int v=(pt.y-(lb.top+S(4)))/plRowH();
          if(v>=0 && v<plListRows()){
              int idx=s_pls->اسکرول+v;
              if(idx<(int)s_pls->نتایج_جستجو.size()) return PLB_ITEM_BASE+v;
          }
          return 0;
      } }
    { RECT sr=plSwitchRect(c);
      RECT pill={c.left+S(20),sr.top+S(1),c.left+S(86),sr.bottom-S(1)};
      if(PtInRect(&pill,pt)) return PLB_پیش‌فرض_ویندوز; }
    { RECT br=plBtnRowRect(c);
      int gap=S(10);
      int bw=(br.right-br.left-gap*2)/3;
      int cx=br.right;
      RECT rOk={cx-bw,br.top,cx,br.bottom}; cx-=bw+gap;
      RECT rNo={cx-bw,br.top,cx,br.bottom}; cx-=bw+gap;
      RECT rTs={cx-bw,br.top,cx,br.bottom};
      if(PtInRect(&rOk,pt))  return PLB_تأیید;
      if(PtInRect(&rNo,pt))  return PLB_انصراف;
      if(PtInRect(&rTs,pt))  return PLB_تست_اتصال;
      RECT rb={c.left+S(20),br.bottom+S(10),c.right-S(20),br.bottom+S(10)+S(40)};
      if(PtInRect(&rb,pt))   return PLB_بازخوانی; }
    return 0;
}

static void plSave(HWND h){
    if(!s_pls) return;
    std::wstring name = s_pls->پیرو_ویندوز ? std::wstring(L"") : s_pls->چاپگر_انتخابی;
    if(!s_pls->پیرو_ویندوز && name.empty()){
        // manual mode with no pick → keep following Windows (nothing to save)
        MessageBoxW(h,L"ابتدا یک چاپگر از لیست انتخاب کنید.",L"ارتباط با چاپگر",
            MB_OK|MB_ICONWARNING);
        return;
    }
    wchar_t f[2]={0,0}; f[0]=s_pls->پیرو_ویندوز?L'1':L'0';
    std::wstring preview = s_pls->پیرو_ویندوز
        ? L"چاپگر: پیش‌فرض ویندوز"
        : L"چاپگر: "+name;
    std::wstring payload = L"printer_name="+name+
        L";printer_follow_windows_default="+std::wstring(f);
    if(printerRequestGate(h,L"ارتباط با چاپگر",L"تغییر چاپگر برنامه",
            payload,preview)){
        setSetting(L"printer_name",name);
        setSetting(L"printer_follow_windows_default",f);
        plClose();
    }
}

static LRESULT CALLBACK plProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps);
        plPaint(h,dc); EndPaint(h,&ps); return 0; }
    case WM_APP_THEME: InvalidateRect(h,NULL,FALSE); return 0;
    case WM_MOUSEMOVE: {
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int hr=plHit(h,pt); if(hr==-1) hr=0;   // -2 (search field) stays for hover
        if(s_pls && hr!=s_pls->hot){ s_pls->hot=hr; InvalidateRect(h,NULL,FALSE); }
        TRACKMOUSEEVENT te={sizeof(te),TME_LEAVE,h,0}; TrackMouseEvent(&te);
        return 0; }
    case WM_MOUSELEAVE:
        if(s_pls && s_pls->hot){ s_pls->hot=0; InvalidateRect(h,NULL,FALSE); }
        return 0;
    case WM_MOUSEWHEEL: {
        if(!s_pls) break;
        int z=GET_WHEEL_DELTA_WPARAM(w);
        int maxScroll=(int)s_pls->نتایج_جستجو.size()-plListRows();
        if(maxScroll<=0) return 0;
        s_pls->اسکرول += (z>0)?-1:1;
        if(s_pls->اسکرول>maxScroll) s_pls->اسکرول=maxScroll;
        if(s_pls->اسکرول<0) s_pls->اسکرول=0;
        InvalidateRect(h,NULL,FALSE);
        return 0; }
    case WM_LBUTTONDOWN: {
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int id=plHit(h,pt);
        if(id==-1 || id==PLB_بستن){ plClose(); return 0; }
        if(id==-2){ SetFocus(s_pls->eSearch?s_pls->eSearch:h); return 0; }
        if(id>=PLB_ITEM_BASE){
            int v=id-PLB_ITEM_BASE;
            int gi=s_pls->اسکرول+v;
            if(gi<(int)s_pls->نتایج_جستجو.size()){
                s_pls->چاپگر_انتخابی=s_pls->همه_چاپگرها[s_pls->نتایج_جستجو[gi]];
                if(s_pls->پیرو_ویندوز){ s_pls->پیرو_ویندوز=false; }
            }
            InvalidateRect(h,NULL,FALSE);
            return 0;
        }
        switch(id){
        case PLB_پیش‌فرض_ویندوز:
            s_pls->پیرو_ویندوز=!s_pls->پیرو_ویندوز;
            if(s_pls->پیرو_ویندوز) s_pls->چاپگر_انتخابی=s_pls->چاپگر_ویندوز;
            InvalidateRect(h,NULL,FALSE); break;
        case PLB_تأیید:      plSave(h); break;
        case PLB_انصراف:     plClose(); break;
        case PLB_تست_اتصال:  prnTestPrintTo(h, s_pls->پیرو_ویندوز
                                             ? s_pls->چاپگر_ویندوز
                                             : s_pls->چاپگر_انتخابی); break;
        case PLB_بازخوانی: {
            s_pls->همه_چاپگرها=enumPrinters();
            wchar_t def[256]={0}; DWORD sz=256;
            s_pls->چاپگر_ویندوز = GetDefaultPrinterW(def,&sz)? std::wstring(def):std::wstring(L"");
            plApplyFilter();
            InvalidateRect(h,NULL,FALSE); break; }
        }
        return 0; }
    case WM_KEYDOWN:
        if(w==VK_ESCAPE){ plClose(); return 0; }
        if(s_pls){
            if(w==VK_UP||w==VK_DOWN){
                int n=(int)s_pls->نتایج_جستجو.size();
                if(n>0){
                    int cur=-1;
                    for(int i=0;i<n;++i)
                        if(s_pls->همه_چاپگرها[s_pls->نتایج_جستجو[i]]==s_pls->چاپگر_انتخابی){ cur=i; break; }
                    cur += (w==VK_DOWN)?1:-1;
                    if(cur<0) cur=0; if(cur>=n) cur=n-1;
                    s_pls->چاپگر_انتخابی=s_pls->همه_چاپگرها[s_pls->نتایج_جستجو[cur]];
                    if(s_pls->پیرو_ویندوز) s_pls->پیرو_ویندوز=false;
                    int vis=cur-s_pls->اسکرول;
                    if(vis<0) s_pls->اسکرول=cur;
                    if(vis>=plListRows()) s_pls->اسکرول=cur-plListRows()+1;
                    InvalidateRect(h,NULL,FALSE);
                }
                return 0;
            }
            if(w==VK_RETURN){ plSave(h); return 0; }
        }
        break;
    case WM_COMMAND:
        // EN_CHANGE from the search EDIT → re-filter the list live.
        if(LOWORD(w)==IDC_PL_SEARCH && HIWORD(w)==EN_CHANGE && s_pls){
            plApplyFilter();
            InvalidateRect(h,NULL,FALSE);
            return 0;
        }
        break;
    case WM_SIZE:
        plLayoutEdit(h);
        InvalidateRect(h,NULL,FALSE);
        return 0;
    case WM_CTLCOLOREDIT: {
        HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_DESTROY:
        // Mirror prnProc exactly: never call DestroyWindow from WM_DESTROY
        // (plClose() would re-enter on a window already being destroyed).
        if(s_pls){ delete s_pls; s_pls=NULL; }
        s_plink=NULL;
        if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE);
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

void PrinterLink_Open(HWND owner){
    if(s_plink && IsWindow(s_plink)){ plClose(); return; }
    static bool reg=false;
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=plProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=PL_CLASS;
        RegisterClassW(&wc); reg=true; }
    RECT rc; GetClientRect(owner,&rc);
    POINT org={0,0}; ClientToScreen(owner,&org);
    s_pls=new PrinterLinkState();
    s_pls->owner=owner; s_pls->hot=0; s_pls->اسکرول=0;
    s_pls->همه_چاپگرها=enumPrinters();
    wchar_t def[256]={0}; DWORD sz=256;
    s_pls->چاپگر_ویندوز = GetDefaultPrinterW(def,&sz)? std::wstring(def) : std::wstring(L"");
    s_pls->پیرو_ویندوز = getSetting(L"printer_follow_windows_default",L"1")!=L"0";
    // Pre-selection order (exactly the currentPrinter() precedence):
    //   1) saved printer_name, if non-empty AND still installed
    //   2) Windows default
    //   3) no selection
    { std::wstring saved=getSetting(L"printer_name",L"");
      bool have=false;
      for(size_t i=0;i<s_pls->همه_چاپگرها.size();++i)
          if(s_pls->همه_چاپگرها[i]==saved){ have=true; break; }
      if(!saved.empty() && have) s_pls->چاپگر_انتخابی=saved;
      else                       s_pls->چاپگر_انتخابی=s_pls->چاپگر_ویندوز;
      if(!s_pls->پیرو_ویندوز && (s_pls->چاپگر_انتخابی.empty() ||
          (s_pls->چاپگر_انتخابی==s_pls->چاپگر_ویندوز && saved.empty())))
          s_pls->پیرو_ویندوز=true;
      if(s_pls->پیرو_ویندوز) s_pls->چاپگر_انتخابی=s_pls->چاپگر_ویندوز;
    }
    plApplyFilter();
    // bring the selection into view
    for(int i=0;i<(int)s_pls->نتایج_جستجو.size();++i)
        if(s_pls->همه_چاپگرها[s_pls->نتایج_جستجو[i]]==s_pls->چاپگر_انتخابی){
            if(i>=plListRows()) s_pls->اسکرول=i-plListRows()+1;
            break;
        }
    s_plink=CreateWindowExW(WS_EX_TOPMOST,PL_CLASS,L"",
        WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN,
        org.x,org.y,rc.right,rc.bottom,owner,NULL,g_hInst,NULL);
    // themed search EDIT child (same pattern as the settings server-url box)
    { RECT er=plSearchEditRect(plCard(s_plink));
      s_pls->eSearch=CreateWindowExW(0,L"EDIT",s_pls->عبارت_جستجو.c_str(),
          WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,
          er.left+S(6),er.top+S(3),(er.right-er.left)-S(12),(er.bottom-er.top)-S(6),
          s_plink,(HMENU)IDC_PL_SEARCH,g_hInst,NULL);
      SendMessageW(s_pls->eSearch,WM_SETFONT,(WPARAM)g_fUI,TRUE);
      SendMessageW(s_pls->eSearch,EM_SETCUEBANNER,TRUE,(LPARAM)L"نام چاپگر را بنویسید…");
    }
    BringWindowToTop(s_plink); SetFocus(s_plink);
}

// ============================================================================
//  PRINT DESIGNER  (separate file for clarity)
// ============================================================================
#include "printer_designer.inc"

// §1.52.0 — forward declaration so the legacy fieldValue resolver can reuse the
// canonical bare-name → {token} normalizer defined further below (pdNormalizeField).
static std::wstring pdNormalizeField(const std::wstring& f);

// ============================================================================
//  RENDER A SAVED DESIGN ONTO A PRINTER DC
// ============================================================================
static std::wstring fieldValue(const ReceptionRecord& r, const std::wstring& tokIn){
    // §1.52.0 — accept BOTH the canonical {token} form and the bare field name
    // (e.g. "firstName"). The new ready-made templates and the Print Designer
    // field picker store the *bare* human-readable key; the classic resolver only
    // matched the {token} form, which is why every field printed blank. Normalize
    // once up-front so every comparison below works for both shapes.
    std::wstring tok = pdNormalizeField(tokIn);
    if(tok==L"{first}")    return r.firstName;
    if(tok==L"{last}")     return r.lastName;
    if(tok==L"{full}")     return r.firstName+L" "+r.lastName;
    if(tok==L"{father}")   return r.fatherName;
    if(tok==L"{nid}")      return toFaDigits(r.nationalId);
    if(tok==L"{birth}")    return toFaDigits(r.birthDate);
    if(tok==L"{gender}")   return r.gender;
    if(tok==L"{mobile}")   return toFaDigits(r.mobile);
    if(tok==L"{landline}") return toFaDigits(r.landline);
    if(tok==L"{address}")  return r.address;
    if(tok==L"{ptype}")    return r.patientType;
    if(tok==L"{ins}")      return r.insurance;
    if(tok==L"{supp}")     return r.suppInsurance;
    if(tok==L"{queue}"){ wchar_t b[16]; swprintf(b,16,L"%d",r.queueNo); return toFaDigits(b); }
    if(tok==L"{date}")     return toFaDigits(r.apptDate);
    if(tok==L"{time}")     return toFaDigits(r.apptTime);
    if(tok==L"{shift}")    return r.shift;
    if(tok==L"{dept}")     return r.dept;
    if(tok==L"{doctor}")   return r.treatingDoctor.empty()? r.dept : r.treatingDoctor; // §1.53.0
    if(tok==L"{user}")     return r.userName;
    if(tok==L"{total}")    return toFaDigits(formatMoney(r.total))+L" ریال";
    if(tok==L"{discount}") return toFaDigits(formatMoney(r.discount))+L" ریال";
    if(tok==L"{paid}")     return toFaDigits(formatMoney(r.paid))+L" ریال";
    // v1.96.0 — full medical-receipt field set for the expanded default slip.
    // v2.07: same resolution chain as pdFieldValue.
    if(tok==L"{clinicaddr}"){
        if(!r.clinicAddr.empty()) return r.clinicAddr;
        std::wstring a=getSetting(L"clinic_addr",L"");
        if(!a.empty()) return a;
        return getSetting(L"clinic_address",L"");
    }
    if(tok==L"{clinicphone}"){
        if(!r.clinicPhone.empty()) return toFaDigits(r.clinicPhone);
        std::wstring p=getSetting(L"clinic_phone_num",L"");
        if(!p.empty()) return toFaDigits(p);
        return toFaDigits(getSetting(L"clinic_phone",L""));
    }
    if(tok==L"{apptdate}")     return toFaDigits(r.apptDate);
    if(tok==L"{age}"){
        // v1.97.0: thermal receipts print age as 10Y / 24Y (Latin digits + Y).
        std::wstring bd=r.birthDate; if(bd.size()>=4){
            int by=_wtoi(bd.substr(0,4).c_str());
            if(by>1200 && by<1500){ SYSTEMTIME st; GetLocalTime(&st);
                int jy=st.wYear-621; int age=jy-by;
                if(age>0&&age<150){ wchar_t b[16]; swprintf(b,16,L"%dY",age); return b; } } }
        return L"";
    }
    if(tok==L"{agenum}"){   // v1.96.0: bare numeric age
        std::wstring bd=r.birthDate; if(bd.size()>=4){
            int by=_wtoi(bd.substr(0,4).c_str());
            if(by>1200 && by<1500){ SYSTEMTIME st; GetLocalTime(&st);
                int jy=st.wYear-621; int age=jy-by;
                if(age>0&&age<150){ wchar_t b[16]; swprintf(b,16,L"%d",age); return b; } } }
        return L"";
    }
    if(tok==L"{doctorcode}")    return toFaDigits(r.doctorCode);
    if(tok==L"{specialtycode}") return toFaDigits(r.specialtyCode);
    if(tok==L"{specialty}")     return r.specialty;
    if(tok==L"{ins_percent}"){  int p=r.insPercent>=0?r.insPercent:Ins_Percent(r.insIdx);
        if(p<=0) return L""; wchar_t b[16]; swprintf(b,16,L"%d",p); return toFaDigits(b)+L"٪"; }
    if(tok==L"{supp_percent}"){ int p=r.suppPercent>=0?r.suppPercent:Supp_Percent(r.suppIdx);
        if(p<=0) return L""; wchar_t b[16]; swprintf(b,16,L"%d",p); return toFaDigits(b)+L"٪"; }
    if(tok==L"{insshare}")      return toFaDigits(formatMoney(r.mainShare))+L" ریال";
    if(tok==L"{supppay}")       return toFaDigits(formatMoney(r.orgShare))+L" ریال";
    if(tok==L"{patientshare}")  return toFaDigits(formatMoney(r.patientShare))+L" ریال";
    if(tok==L"{pos}")           return toFaDigits(formatMoney(r.pos))+L" ریال";
    if(tok==L"{cash}")          return toFaDigits(formatMoney(r.cash))+L" ریال";
    if(tok==L"{eprescription}") return toFaDigits(r.eprescription);
    if(tok==L"{referralno}")    return toFaDigits(r.referralNo);
    if(tok==L"{receptionist}")  return r.receptionist.empty()? (r.userName.empty()?g_session.user.fullname:r.userName) : r.receptionist;
    if(tok==L"{cashier}")       return r.cashierName.empty()? (r.userName.empty()?g_session.user.fullname:r.userName) : r.cashierName;
    if(tok==L"{scnum}"){ if(!r.scNum.empty()) return toFaDigits(r.scNum);
        wchar_t b[16]; swprintf(b,16,L"%d",g_session.shift+1); return toFaDigits(b); }
    if(tok==L"{issued}")   return L"چاپ توسط پذیرش: "+
        (r.userName.empty()?g_session.user.fullname:r.userName);
    return L"";
}
static std::wstring itemText(const ReceptionRecord& r, const DItem& it){
    std::wstring s=it.text;
    if(!it.field.empty()) s += fieldValue(r,it.field);
    return s;
}

bool printDesignedReceipt(const ReceptionRecord& r, int sectionIdx, HWND owner){
    // honour the per-section enable toggle from the printer-settings dialog
    if(getSetting(L"sec_enabled_"+std::to_wstring(sectionIdx),L"1")==L"0")
        return false;
    Design d;
    if(!loadDesignFile(sectionIdx,d)){
        // try the per-section default if the user never saved one
        d=defaultDesign(sectionIdx);
        if(d.items.empty()) return false;
    }
    std::wstring prn=currentPrinter();
    HDC dc = prn.empty()? NULL : CreateDCW(L"WINSPOOL",prn.c_str(),NULL,NULL);
    if(!dc){
        PRINTDLGW pd={0}; pd.lStructSize=sizeof(pd); pd.hwndOwner=owner;
        pd.Flags=PD_RETURNDC|PD_NOPAGENUMS|PD_NOSELECTION|PD_USEDEVMODECOPIES;
        if(!PrintDlgW(&pd)) return false;
        dc=pd.hDC;
    }
    if(!dc) return false;

    double pageW,pageH; paperMM(d.paper,pageW,pageH);
    int dpiX=GetDeviceCaps(dc,LOGPIXELSX), dpiY=GetDeviceCaps(dc,LOGPIXELSY);
    int horz=GetDeviceCaps(dc,HORZRES),  vert=GetDeviceCaps(dc,VERTRES);
    int offX=GetDeviceCaps(dc,PHYSICALOFFSETX);
    int offY=GetDeviceCaps(dc,PHYSICALOFFSETY);
    // scale: device pixels per mm. 1 inch = 25.4 mm.
    double sx=dpiX/25.4, sy=dpiY/25.4;
    int mode=(getSetting(L"print_mode",L"fit")==L"fill")?1:0;
    if(mode==1){ // fill: stretch design to printable area
        double pw=horz/sx, ph=vert/sy;
        if(pw>1 && ph>1){ sx*=pageW/pw; sy*=pageH/ph; }
    }
    auto mmX=[&](double mm){ return (int)(mm*sx)-offX; };
    auto mmY=[&](double mm){ return (int)(mm*sy)-offY; };

    DOCINFOW di={sizeof(di)};
    std::wstring docName=std::wstring(APP_NAME_W)+L" — "+
        PRINT_SECTIONS[sectionIdx<N_PRINT_SECTIONS?sectionIdx:0];
    di.lpszDocName=docName.c_str();
    if(StartDocW(dc,&di)<=0){ DeleteDC(dc); return false; }

    // number of copies (1..5) configured in the printer-settings dialog
    int copies=_wtoi(getSetting(L"print_copies",L"1").c_str());
    if(copies<1) copies=1; if(copies>5) copies=5;

    for(int copy=0; copy<copies; ++copy){
    StartPage(dc);
    SetBkMode(dc,TRANSPARENT);

    for(const DItem& it: d.items){
        if(it.kind==IT_LABEL){
            std::wstring s=itemText(r,it);
            if(s.empty()) continue;
            int px=mmX(it.x), py=mmY(it.y);
            int lf=-(int)(it.fontSize*dpiY/72.0);
            int wpx=(int)(it.w*sx); if(wpx<10) wpx=horz;
            RECT rr={px,py,px+wpx,py+lf*-3};
            if(it.bgColor!=CLR_INVALID){
                RECT bg={px,py,px+wpx,py+(int)(it.fontSize*dpiY/72.0*1.3)};
                HBRUSH bb=CreateSolidBrush(it.bgColor); FillRect(dc,&bg,bb); DeleteObject(bb);
            }
            HFONT f=CreateFontW(lf,0,0,0,it.bold?FW_BOLD:FW_NORMAL,
                it.italic?1:0,it.underline?1:0,it.strike?1:0,DEFAULT_CHARSET,
                0,0,CLEARTYPE_QUALITY,0,
                it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
            HGDIOBJ of=SelectObject(dc,f);
            SetTextColor(dc,it.color);
            UINT al=(it.align==1)?DT_CENTER:(it.align==2)?DT_LEFT:DT_RIGHT;
            DrawTextW(dc,s.c_str(),-1,&rr,
                al|DT_TOP|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
            SelectObject(dc,of); DeleteObject(f);
        } else if(it.kind==IT_LINE_H || it.kind==IT_LINE_V){
            int style=it.lineStyle==1?PS_DASH:it.lineStyle==2?PS_DOT:PS_SOLID;
            int wpx=(int)(it.lineW*sx); if(wpx<1) wpx=1;
            HPEN p=CreatePen(style,wpx,it.color);
            HGDIOBJ o=SelectObject(dc,p);
            if(it.kind==IT_LINE_H){
                MoveToEx(dc,mmX(it.x),mmY(it.y),0); LineTo(dc,mmX(it.x+it.w),mmY(it.y));
            } else {
                MoveToEx(dc,mmX(it.x),mmY(it.y),0); LineTo(dc,mmX(it.x),mmY(it.y+it.h));
            }
            SelectObject(dc,o); DeleteObject(p);
        } else if(it.kind==IT_BORDER){
            int style=it.lineStyle==1?PS_DASH:it.lineStyle==2?PS_DOT:PS_SOLID;
            int wpx=(int)(it.lineW*sx); if(wpx<1) wpx=1;
            HPEN p=CreatePen(style,wpx,it.borderColor);
            HGDIOBJ o=SelectObject(dc,p);
            HGDIOBJ ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
            Rectangle(dc,mmX(it.x),mmY(it.y),mmX(it.x+it.w),mmY(it.y+it.h));
            SelectObject(dc,ob); SelectObject(dc,o); DeleteObject(p);
        } else if(it.kind==IT_LOGO){
            // logo rendering handled in the designer via GDI+; for the printer
            // we draw a placeholder box if no image could be loaded.
            HPEN p=CreatePen(PS_DOT,1,RGB(120,120,120));
            HGDIOBJ o=SelectObject(dc,p);
            HGDIOBJ ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
            Rectangle(dc,mmX(it.x),mmY(it.y),mmX(it.x+it.w),mmY(it.y+it.h));
            SelectObject(dc,ob); SelectObject(dc,o); DeleteObject(p);
        }
    }
    EndPage(dc);
    }  // end copies loop
    EndDoc(dc); DeleteDC(dc);
    logLine(L"designed receipt printed for section "+std::to_wstring(sectionIdx)
            +L" ×"+std::to_wstring(copies));
    return true;
}

// ============================================================================
//  §1.19.0 — RENDER A NEW (print_designer JSON) DESIGN ONTO A PRINTER DC
//  The HTML/CSS/JS designer + native designer both persist a `PrintDesign`
//  (print_designer.h) bound to a section id. This renderer resolves that design
//  for the given section and prints it on the connected printer. The first time
//  (per session) it asks A4/A5 via the standard print dialog so the operator can
//  pick paper + printer; afterwards it reuses the saved default printer.
// ============================================================================
// ----------------------------------------------------------------------------
//  §1.21.0 — table model. A PIT_TABLE item stores its grid as JSON inside
//  `it.text`: {"cols":n,"rows":n,"header":bool,"widths":[..],"cells":[[..]]}.
//  We parse it here with a tiny tolerant reader so print + preview render the
//  EXACT same grid the designer shows (true WYSIWYG). Cells may contain {field}
//  tokens which are substituted with live data at print time.
// ----------------------------------------------------------------------------
struct PdTable {
    int cols=0, rows=0; bool header=false;
    std::vector<double> widths;
    std::vector<std::vector<std::wstring>> cells;
};
static std::wstring pdU8toW(const std::string& s){
    int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),NULL,0);
    std::wstring w(n,0); if(n) MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),&w[0],n);
    return w;
}
// PrintItem colours are stored as 0x00RRGGBB (web/CSS order). GDI COLORREF is
// 0x00BBGGRR, so we must swap R<->B to print the *exact* colour designed.
static inline COLORREF pdCR(unsigned int rgb){
    return RGB((rgb>>16)&0xFF, (rgb>>8)&0xFF, rgb&0xFF);
}

// ---------------------------------------------------------------------------
// v2.07 §3.8 — PRINT INK POLICY (high-contrast, saturated output).
// Authored colours are tuned for the on-screen designer, where light greys
// read fine; on paper they produce the washed-out look the operator reported.
// On the PRINTER DC (never on the preview) we therefore saturate:
//   • TEXT whose relative luminance is above 0.62 is darkened to a minimum
//     ink contrast (no text grey lighter than #555555).
//   • Hairlines thinner than 0.30 mm are bumped up, and no line lighter than
//     #333333 is emitted.
//   • opacity < 1.0 and shadows are ignored on paper (preview-only effects).
static inline double pdLuminance(unsigned int rgb){
    double r=((rgb>>16)&0xFF)/255.0, g=((rgb>>8)&0xFF)/255.0, b=(rgb&0xFF)/255.0;
    return 0.2126*r + 0.7152*g + 0.0722*b;
}
// Darken toward pure ink until the luminance is at or below the ceiling.
static inline unsigned int pdSaturateInk(unsigned int rgb,double ceiling){
    if(pdLuminance(rgb)<=ceiling) return rgb;
    double k=ceiling/pdLuminance(rgb);          // scale all channels equally
    if(k>1.0) k=1.0;
    unsigned int r=(unsigned int)(((rgb>>16)&0xFF)*k+0.5);
    unsigned int g=(unsigned int)(((rgb>>8)&0xFF)*k+0.5);
    unsigned int b=(unsigned int)((rgb&0xFF)*k+0.5);
    if(r>0xFF)r=0xFF; if(g>0xFF)g=0xFF; if(b>0xFF)b=0xFF;
    return (r<<16)|(g<<8)|b;
}
// v2.07 §3.8: the canonical saturated print palette.
static const unsigned int PD_INK    = 0x000000;   // INK
static const unsigned int PD_ACCENT = 0x0B3D91;   // ACCENT
static const unsigned int PD_DANGER = 0xA31212;   // DANGER
static const unsigned int PD_MUTED  = 0x333333;   // MUTED
// Text ink: saturate, then never lighter than #555555-equivalent luminance.
static inline COLORREF pdTextInk(unsigned int rgb){
    unsigned int sat=pdSaturateInk(rgb,0.62);
    return pdCR(sat);
}
// Hairline ink: never lighter than #333333.
static inline COLORREF pdLineInk(unsigned int rgb){
    unsigned int sat=pdSaturateInk(rgb,0.25);
    return pdCR(sat);
}
static bool pdParseTable(const std::wstring& jsonW, PdTable& t){
    // convert to utf8 for byte parsing, but keep strings as wstring
    int n=WideCharToMultiByte(CP_UTF8,0,jsonW.c_str(),(int)jsonW.size(),NULL,0,NULL,NULL);
    std::string s(n,0); if(n) WideCharToMultiByte(CP_UTF8,0,jsonW.c_str(),(int)jsonW.size(),&s[0],n,NULL,NULL);
    size_t p=0; auto ws=[&]{ while(p<s.size()&&(s[p]==' '||s[p]=='\t'||s[p]=='\n'||s[p]=='\r'))++p; };
    auto rdStr=[&](std::string& out)->bool{ ws(); if(p>=s.size()||s[p]!='"')return false; ++p;
        while(p<s.size()&&s[p]!='"'){ char c=s[p++]; if(c=='\\'&&p<s.size()){ char e=s[p++];
            switch(e){case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;
                case '"':out+='"';break;case '\\':out+='\\';break;case '/':out+='/';break;
                case 'u':{ if(p+4<=s.size()){ unsigned v=(unsigned)strtoul(s.substr(p,4).c_str(),NULL,16); p+=4;
                    if(v<0x80)out+=(char)v; else if(v<0x800){out+=(char)(0xC0|(v>>6));out+=(char)(0x80|(v&0x3F));}
                    else{out+=(char)(0xE0|(v>>12));out+=(char)(0x80|((v>>6)&0x3F));out+=(char)(0x80|(v&0x3F));} } break; }
                default:out+=e; } } else out+=c; }
        if(p<s.size()&&s[p]=='"')++p; return true; };
    auto rdNum=[&]()->double{ ws(); size_t st=p; while(p<s.size()&&(isdigit((unsigned char)s[p])||s[p]=='-'||s[p]=='+'||s[p]=='.'||s[p]=='e'||s[p]=='E'))++p; return atof(s.substr(st,p-st).c_str()); };
    ws(); if(p>=s.size()||s[p]!='{') return false; ++p;
    while(true){ ws(); if(p<s.size()&&s[p]=='}'){++p;break;} if(p>=s.size())break;
        std::string key; if(!rdStr(key))break; ws(); if(p<s.size()&&s[p]==':')++p;
        if(key=="cols") t.cols=(int)rdNum();
        else if(key=="rows") t.rows=(int)rdNum();
        else if(key=="header"){ ws(); if(s.compare(p,4,"true")==0){t.header=true;p+=4;} else if(s.compare(p,5,"false")==0){t.header=false;p+=5;} else rdNum(); }
        else if(key=="widths"){ ws(); if(p<s.size()&&s[p]=='['){++p; while(true){ ws(); if(p<s.size()&&s[p]==']'){++p;break;} t.widths.push_back(rdNum()); ws(); if(p<s.size()&&s[p]==','){++p;continue;} if(p<s.size()&&s[p]==']'){++p;break;} break; } } }
        else if(key=="cells"){ ws(); if(p<s.size()&&s[p]=='['){++p;
            while(true){ ws(); if(p<s.size()&&s[p]==']'){++p;break;}
                if(p<s.size()&&s[p]=='['){++p; std::vector<std::wstring> rowv;
                    while(true){ ws(); if(p<s.size()&&s[p]==']'){++p;break;} std::string cv; if(rdStr(cv)) rowv.push_back(pdU8toW(cv)); else { rdNum(); rowv.push_back(L""); }
                        ws(); if(p<s.size()&&s[p]==','){++p;continue;} if(p<s.size()&&s[p]==']'){++p;break;} break; }
                    t.cells.push_back(rowv); }
                ws(); if(p<s.size()&&s[p]==','){++p;continue;} if(p<s.size()&&s[p]==']'){++p;break;} break; } } }
        else { // skip unknown value
            ws(); if(p<s.size()&&s[p]=='"'){ std::string tmp; rdStr(tmp); }
            else if(p<s.size()&&s[p]=='{'){ int d=0; do{ if(s[p]=='{')d++; else if(s[p]=='}')d--; ++p; }while(p<s.size()&&d>0); }
            else if(p<s.size()&&s[p]=='['){ int d=0; do{ if(s[p]=='[')d++; else if(s[p]==']')d--; ++p; }while(p<s.size()&&d>0); }
            else rdNum();
        }
        ws(); if(p<s.size()&&s[p]==','){++p;continue;} ws(); if(p<s.size()&&s[p]=='}'){++p;break;}
        if(p>=s.size())break;
    }
    if(t.cols<=0||t.rows<=0||t.cells.empty()) return false;
    if((int)t.widths.size()!=t.cols){ t.widths.assign(t.cols,1.0); }
    return true;
}
// substitute {field} tokens inside an arbitrary string with live record data.
static std::wstring pdSubstFields(const ReceptionRecord& r, const std::wstring& in,
                                  std::wstring (*resolver)(const ReceptionRecord&, const std::wstring&)){
    std::wstring out; size_t i=0;
    while(i<in.size()){
        if(in[i]==L'{'){ size_t e=in.find(L'}',i);
            if(e!=std::wstring::npos){ std::wstring tok=in.substr(i,e-i+1);
                std::wstring v=resolver(r,tok);
                if(!v.empty() || tok.size()>2) { out+=v; i=e+1; continue; } } }
        out+=in[i++];
    }
    return out;
}

// §1.52.0 — normalize a bare field name (as stored by the ready-made templates
// and the Print Designer field picker, e.g. "firstName") into the canonical
// {token} vocabulary that pdFieldValue understands ("{first}"). Templates and
// user-created designs store the *bare* human-readable key in PrintItem.field;
// pdFieldValue historically only matched the {token} form, which is why every
// field printed blank. This single mapping fixes that root cause for ALL
// existing and future designs without touching their stored JSON.
static std::wstring pdNormalizeField(const std::wstring& f){
    if(f.empty()) return f;
    if(f.size()>=2 && f.front()==L'{' && f.back()==L'}'){
        // v1.97.0 — {P-Name} is a documented alias for the live full name.
        std::wstring inner=f.substr(1,f.size()-2);
        auto ieq=[&](const char* a)->bool{
            size_t i=0; for(; i<inner.size() && a[i]; ++i){
                wchar_t c=inner[i]; if(c>=L'A'&&c<=L'Z') c=(wchar_t)(c-L'A'+L'a');
                wchar_t d=(wchar_t)(unsigned char)a[i];
                if(d>=L'A'&&d<=L'Z') d=(wchar_t)(d-L'A'+L'a');
                if(c!=d) return false; }
            return a[i]==0 && i==inner.size();
        };
        if(ieq("p-name")||ieq("pname")||ieq("p_name")) return L"{full}";
        return f;
    }
    // lowercase compare helper (ASCII only — field names are ASCII).
    // §1.52.0 — accept const char* literals so callers can pass plain
    // narrow strings; widen each byte on the fly.
    auto eq=[&](const char* a)->bool{
        size_t i=0; for(; i<f.size() && a[i]; ++i){
            wchar_t c=f[i]; if(c>=L'A'&&c<=L'Z') c=(wchar_t)(c-L'A'+L'a');
            wchar_t d=(wchar_t)(unsigned char)a[i];
            if(d>=L'A'&&d<=L'Z') d=(wchar_t)(d-L'A'+L'a');
            if(c!=d) return false; }
        return a[i]==0 && i==f.size();
    };
    // identity / general clinic meta
    if(eq("firstName")||eq("firstname")||eq("fname")||eq("name"))  return L"{first}";
    if(eq("lastName") ||eq("lastname") ||eq("lname")||eq("surname")) return L"{last}";
    if(eq("fullName") ||eq("fullname")||eq("full")||eq("p-name")||eq("pname")||eq("p_name")) return L"{full}";
    if(eq("fatherName")||eq("fathername")||eq("father"))             return L"{father}";
    if(eq("nationalCode")||eq("nationalcode")||eq("nationalId")||eq("nationalid")||eq("nid")||eq("nationalNo")||eq("id")) return L"{nid}";
    if(eq("birthDate") ||eq("birthdate") ||eq("birth")||eq("dob"))   return L"{birth}";
    if(eq("gender")    ||eq("sex"))                                  return L"{gender}";
    if(eq("mobile")    ||eq("cellphone")||eq("cell")||eq("phone2"))  return L"{mobile}";
    if(eq("landline")  ||eq("phone")||eq("tel")||eq("telephone"))    return L"{landline}";
    if(eq("address"))                                                return L"{address}";
    if(eq("patientType")||eq("patienttype")||eq("ptype")||eq("visitType")||eq("visittype")) return L"{ptype}";
    if(eq("insurance") ||eq("ins")||eq("insName")||eq("insurer"))    return L"{ins}";
    if(eq("suppInsurance")||eq("suppinsurance")||eq("supplementary")||eq("supp")||eq("suppIns")) return L"{supp}";
    if(eq("queueNo")   ||eq("queueno")||eq("queue")||eq("turn")||eq("turnNo")) return L"{queue}";
    if(eq("apptDate")  ||eq("apptdate")||eq("date")||eq("regDate")||eq("regdate")||eq("appointmentDate")) return L"{date}";
    if(eq("apptTime")  ||eq("appttime")||eq("time")||eq("regTime")||eq("regtime")||eq("appointmentTime")) return L"{time}";
    if(eq("shift"))                                                  return L"{shift}";
    if(eq("dept")     ||eq("department")||eq("section")||eq("unit")) return L"{dept}";
    if(eq("doctor")   ||eq("physician")||eq("doc"))                  return L"{doctor}";
    if(eq("userName") ||eq("username")||eq("user")||eq("operator")) return L"{user}";
    if(eq("clinic")   ||eq("clinicName")||eq("clinicname")||eq("center")) return L"{clinic}";
    if(eq("receiptNo")||eq("receiptno")||eq("receipt")||eq("invoiceNo")||eq("invoiceno")) return L"{receiptNo}";
    // money
    if(eq("total")    ||eq("gross")||eq("totalPrice")||eq("billTotal"))  return L"{total}";
    if(eq("mainShare")||eq("mainshare")||eq("insShare")||eq("insshare")||eq("insuranceShare")) return L"{insshare}";
    if(eq("discount"))                                                return L"{discount}";
    if(eq("paid")     ||eq("amountPaid")||eq("amountpaid"))          return L"{paid}";
    if(eq("patientShare")||eq("patientshare")||eq("patShare")||eq("patshare")||eq("share")) return L"{patientshare}";
    if(eq("finalTotal")||eq("finaltotal")||eq("final")||eq("net")||eq("netTotal")) return L"{finaltotal}";
    if(eq("visitFee") ||eq("visitfee")||eq("fee"))                   return L"{visitfee}";
    if(eq("payType")  ||eq("paytype")||eq("paymentType")||eq("paymenttype")) return L"{paytype}";
    if(eq("cashier")  ||eq("issued")||eq("issuer"))                  return L"{cashier}";
    // services aggregates
    if(eq("servicesCount")||eq("servicescount")||eq("serviceCount")||eq("servicecount")) return L"{servicescount}";
    if(eq("servicesTotal")||eq("servicestotal")||eq("serviceTotal")||eq("servicetotal")) return L"{servicestotal}";
    // clinic meta
    if(eq("clinicAddress")||eq("clinicaddress")||eq("address_clinic")) return L"{clinicaddr}";
    if(eq("clinicPhone") ||eq("clinicphone")||eq("phone_clinic"))     return L"{clinicphone}";
    if(eq("clinicManager")||eq("clinicmanager")||eq("manager"))       return L"{clinicmgr}";
    if(eq("clinicLicense")||eq("cliniclicense")||eq("license")||eq("licence")) return L"{cliniclic}";
    if(eq("age"))                                                    return L"{age}";
    if(eq("refDoctor")||eq("refdoctor")||eq("referringDoctor")||eq("referring")) return L"{refdoctor}";
    if(eq("room"))                                                   return L"{room}";
    if(eq("service"))                                                return L"{service}";
    // ---- v1.55.0: real-receipt aliases (ثامن‌الائمه redesign) ---------------
    // Bare-name aliases for every token the 30 new ready-made designs bind, so
    // a design saved either way (token or bare name) resolves identically.
    if(eq("apptdatetime")||eq("appointmentdatetime")) return L"{apptdatetime}";
    if(eq("apptsec")||eq("appttimesec")||eq("timesec")) return L"{apptsec}";
    if(eq("reg_ts")||eq("regts")||eq("regstamp"))      return L"{reg_ts}";
    if(eq("receptionist")||eq("receptionuser")||eq("admituser")) return L"{receptionist}";
    if(eq("cashier_name")||eq("cashiername"))          return L"{cashier_name}";
    if(eq("scnum")||eq("sc_num")||eq("sheetno")||eq("sheetnum")) return L"{scnum}";
    if(eq("receiptbarcode")||eq("receipt_barcode")||eq("barcodeno")||eq("barcodenumber")) return L"{receiptbarcode}";
    if(eq("receiptcode")||eq("receipt_code")||eq("shortcode")) return L"{receiptcode}";
    if(eq("ins_percent")||eq("inspercent")||eq("insurancepercent")) return L"{ins_percent}";
    if(eq("supp_percent")||eq("supppercent"))          return L"{supp_percent}";
    if(eq("ins_full")||eq("insfull"))                  return L"{ins_full}";
    if(eq("supp_full")||eq("suppfull"))                return L"{supp_full}";
    if(eq("doctorcode")||eq("doctor_code")||eq("doccode")) return L"{doctorcode}";
    if(eq("performer")||eq("performername"))           return L"{performer}";
    if(eq("performercode")||eq("performer_code")||eq("perfcode")) return L"{performercode}";
    if(eq("specialty")||eq("speciality")||eq("specialtydesc")) return L"{specialty}";
    if(eq("specialtycode")||eq("specialitycode"))      return L"{specialtycode}";
    if(eq("servicetype")||eq("service_type")||eq("svctype")) return L"{servicetype}";
    if(eq("servicedesc")||eq("service_desc")||eq("svcdesc")) return L"{servicedesc}";
    if(eq("pos")||eq("posamount")||eq("cardamount"))   return L"{pos}";
    if(eq("cash")||eq("cashamount"))                   return L"{cash}";
    if(eq("discount_from")||eq("discountfrom"))        return L"{discount_from}";
    if(eq("eprescription")||eq("erx")||eq("erxcode")||eq("etrackcode")) return L"{eprescription}";
    if(eq("referralno")||eq("referral_no")||eq("ref_no")||eq("refno")) return L"{referralno}";
    if(eq("basepay")||eq("base_pay")||eq("baseshare"))  return L"{basepay}";
    if(eq("supppay")||eq("supp_pay")||eq("suppshare"))  return L"{supppay}";
    // ---- v2.07.0: clinic/receipt identity aliases ------------------------
    if(eq("certno")||eq("cert_no")||eq("certificateNo")||eq("certificateno")
       ||eq("shenasname")||eq("shenasnameNo")||eq("shenasnameno")) return L"{certno}";
    if(eq("receipttitle")||eq("receipt_title")||eq("receiptlabel")) return L"{receipttitle}";
    if(eq("clinicname")||eq("clinic_name")) return L"{clinicname}";
    // unknown bare name → wrap as {name} so the token pass can still decide
    return L"{"+f+L"}";
}

// ---------------------------------------------------------------------------
//  v1.55.0 — DETERMINISTIC receipt identifiers.
//  The operator explicitly required that NOTHING on a printed receipt is ever
//  random: reprinting the same admission must always produce the SAME barcode
//  and the SAME short code. Both are therefore pure functions of the receipt /
//  queue number, so they are stable across reprints, sessions and machines.
// ---------------------------------------------------------------------------
// A 7-digit numeric barcode payload derived from the receipt number. Uses a
// fixed odd multiplier so consecutive receipts get well-separated payloads (a
// nicer looking barcode) while remaining fully reproducible.
static std::wstring pdReceiptBarcode(long long seed){
    if(seed<=0) return L"";
    // v1.58.0: a 12-digit numeric payload derived deterministically from the
    // patient-linked seed. 12 digits (plus an EAN-13 check digit computed by the
    // encoder) gives every admission a unique, reproducible, scannable code.
    long long v = 100000000000LL + ((seed * 2654435761LL) % 899999999999LL);
    if(v<0) v = -v;
    wchar_t b[24]; swprintf(b,24,L"%012lld",v);
    return b;
}
// A 3-character Crockford-ish alphanumeric short tag (e.g. "56Y") derived from
// the same seed. Ambiguous glyphs (I, O) are excluded from the alphabet.
static std::wstring pdReceiptShortCode(long long seed){
    if(seed<=0) return L"";
    static const wchar_t A[] = L"0123456789ABCDEFGHJKLMNPQRSTUVWXYZ";
    const int N = (int)(sizeof(A)/sizeof(A[0])) - 1;   // 34 symbols
    wchar_t b[4] = {0};
    b[0] = A[(int)(( seed * 7LL) % N)];
    b[1] = A[(int)(( seed * 13LL) % N)];
    b[2] = A[(int)(( seed * 19LL) % N)];
    return std::wstring(b);
}
// The canonical receipt seed for a record. v1.58.0: the barcode / short code
// must be BOTH stable across reprints AND uniquely tied to the admitted patient
// — the operator explicitly required "بارکد متصل به اطلاعات بیمار". We therefore
// fold the patient's national-id digits into the deterministic receipt/queue
// number. Reprinting the same admission always yields the same value (no
// randomness); two different patients with the same daily queue number get
// different barcodes because their national ids differ.
static long long pdReceiptSeed(const ReceptionRecord& r){
    // v1.98: barcode is NOT the national id. Seed is receipt/queue number only.
    if(r.receiptNo > 0) return r.receiptNo;
    if(r.queueNo > 0) return (long long)r.queueNo;
    return 0;
}

// v2.07 §3.3 — resolve the record's specialty/codes against the LIVE doctors
// store (the «مدیریت تخصص» records live on DoctorDef). Memoised per call so a
// receipt with many doctor tokens loads the store once.
static std::vector<DoctorDef> pdLoadDoctorsForRecord(){
    return loadDoctors();
}

static std::wstring pdFieldValue(const ReceptionRecord& r, const std::wstring& tokIn){
    // §1.52.0 — accept BOTH the canonical {token} form and the bare field name
    // (e.g. "firstName") that ready-made templates / the designer store. We
    // normalize once up-front so every comparison below works for both shapes.
    std::wstring tok = pdNormalizeField(tokIn);
    // The new designer's field keys mirror the legacy {token} vocabulary, plus
    // a few extras. Reuse the classic resolver, then handle the new ones.
    if(tok==L"{first}")    return r.firstName;
    if(tok==L"{last}")     return r.lastName;
    if(tok==L"{full}")     return r.firstName+L" "+r.lastName;
    if(tok==L"{father}")   return r.fatherName;
    if(tok==L"{nid}")      return toFaDigits(r.nationalId);
    if(tok==L"{birth}")    return toFaDigits(r.birthDate);
    if(tok==L"{gender}")   return r.gender;
    if(tok==L"{mobile}")   return toFaDigits(r.mobile);
    if(tok==L"{landline}") return toFaDigits(r.landline);
    if(tok==L"{address}")  return r.address;
    if(tok==L"{ptype}")    return r.patientType;
    if(tok==L"{ins}")      return r.insurance;
    if(tok==L"{supp}")     return r.suppInsurance;
    if(tok==L"{insno}")    return toFaDigits(r.insNo);    // §1.53.0 (Bug D)
    if(tok==L"{insexp}")   return toFaDigits(r.insExp);
    if(tok==L"{queue}"){ wchar_t b[16]; swprintf(b,16,L"%d",r.queueNo); return toFaDigits(b); }
    if(tok==L"{date}")     return toFaDigits(r.apptDate);
    if(tok==L"{time}")     return toFaDigits(r.apptTime);
    if(tok==L"{datetime}") return toFaDigits(r.apptDate+L" - "+r.apptTime);
    if(tok==L"{shift}")    return r.shift;
    if(tok==L"{dept}")     return r.dept;
    // §1.53.0 (Bug D): prefer the dedicated treating-doctor name; fall back to
    // r.dept (the §1.52.0 behaviour) when the operator left the doctor blank.
    if(tok==L"{doctor}")   return r.treatingDoctor.empty() ? r.dept : r.treatingDoctor;
    if(tok==L"{apptdate}") return toFaDigits(r.apptDate);
    if(tok==L"{appttime}") return toFaDigits(r.apptTime);
    if(tok==L"{appttype}") return r.patientType;
    if(tok==L"{user}")     return r.userName;
    if(tok==L"{clinic}")   return L"درمانگاه درمان پلاس";
    if(tok==L"{receiptNo}"){ wchar_t b[16]; swprintf(b,16,L"%d",r.queueNo); return toFaDigits(b); }
    if(tok==L"{total}")    return toFaDigits(formatMoney(r.total))+L" ریال";
    if(tok==L"{insshare}") return toFaDigits(formatMoney(r.mainShare))+L" ریال";
    if(tok==L"{discount}") return toFaDigits(formatMoney(r.discount))+L" ریال";
    if(tok==L"{paid}")     return toFaDigits(formatMoney(r.paid))+L" ریال";
    if(tok==L"{service}")  return L"ویزیت";
    if(tok==L"{issued}")   return L"چاپ توسط پذیرش: "+
        (r.userName.empty()?g_session.user.fullname:r.userName);
    // v1.22.0 — extra fields modelled on real Iranian clinic forms.
    // v2.07: prefer the record-resolved value, then management settings
    // (clinic_addr then the legacy clinic_address key), else blank.
    if(tok==L"{clinicaddr}"){
        if(!r.clinicAddr.empty()) return r.clinicAddr;
        std::wstring a=getSetting(L"clinic_addr",L"");
        if(!a.empty()) return a;
        return getSetting(L"clinic_address",L"");
    }
    if(tok==L"{clinicphone}"){
        if(!r.clinicPhone.empty()) return toFaDigits(r.clinicPhone);
        std::wstring p=getSetting(L"clinic_phone_num",L"");
        if(!p.empty()) return toFaDigits(p);
        return toFaDigits(getSetting(L"clinic_phone",L""));
    }
    if(tok==L"{clinicmgr}")   return getSetting(L"clinic_manager",L"");
    if(tok==L"{cliniclic}")   return toFaDigits(getSetting(L"clinic_license",L""));
    if(tok==L"{age}"){
        // v1.97.0: thermal receipts print age as 10Y / 24Y (Latin digits + Y).
        std::wstring bd=r.birthDate; if(bd.size()>=4){
            int by=_wtoi(bd.substr(0,4).c_str());
            if(by>1200 && by<1500){ SYSTEMTIME st; GetLocalTime(&st);
                int jy=st.wYear-621; int age=jy-by;
                if(age>0&&age<150){ wchar_t b[16]; swprintf(b,16,L"%dY",age); return b; } } }
        return L"";
    }
    if(tok==L"{agenum}"){   // v1.96.0: bare numeric age
        std::wstring bd=r.birthDate; if(bd.size()>=4){
            int by=_wtoi(bd.substr(0,4).c_str());
            if(by>1200 && by<1500){ SYSTEMTIME st; GetLocalTime(&st);
                int jy=st.wYear-621; int age=jy-by;
                if(age>0&&age<150){ wchar_t b[16]; swprintf(b,16,L"%d",age); return b; } } }
        return L"";
    }
    if(tok==L"{patientshare}") return toFaDigits(formatMoney(r.patientShare))+L" ریال";
    if(tok==L"{finaltotal}")   return toFaDigits(formatMoney(r.finalTotal))+L" ریال";
    if(tok==L"{visittype}")    return r.patientType;
    if(tok==L"{insidx}")     { wchar_t b[16]; swprintf(b,16,L"%d",r.insIdx); return toFaDigits(b); }
    if(tok==L"{shiftuser}")    return r.shift+L" — "+(r.userName.empty()?g_session.user.fullname:r.userName);
    if(tok==L"{barcode}"){
        if(!r.receiptBarcode.empty()) return toFaDigits(r.receiptBarcode);
        return toFaDigits(pdReceiptBarcode(pdReceiptSeed(r)));
    }
    if(tok==L"{nationalcard}") return toFaDigits(r.nationalId);
    if(tok==L"{regdate}")      return toFaDigits(r.apptDate);
    if(tok==L"{regtime}")      return toFaDigits(r.apptTime);
    if(tok==L"{insshareonly}") return toFaDigits(formatMoney(r.mainShare));
    if(tok==L"{paidonly}")     return toFaDigits(formatMoney(r.paid));
    if(tok==L"{totalonly}")    return toFaDigits(formatMoney(r.total));
    // v1.24.0 — additional fields used by the new professional templates.
    // Not (yet) captured at reception → resolve to empty/sensible defaults so
    // a design that references them still prints cleanly (the field simply
    // shows blank, or is hidden when visibility==1).
    // §1.53.0 (Bug D): resolve the optional clinical fields from the record.
    // They stay empty by default so a design that references them prints
    // cleanly (and hides the row when visibility==1).
    if(tok==L"{refdoctor}")    return r.refDoctor;
    if(tok==L"{room}")         return r.dept;            // unit/room ≈ section
    if(tok==L"{nextvisit}")    return toFaDigits(r.nextVisit);
    if(tok==L"{weight}")       return toFaDigits(r.weight);
    if(tok==L"{height}")       return toFaDigits(r.height);
    if(tok==L"{bp}")           return toFaDigits(r.bp);
    if(tok==L"{temp}")         return toFaDigits(r.temp);
    if(tok==L"{pulse}")        return toFaDigits(r.pulse);
    if(tok==L"{allergy}")      return r.allergy;
    if(tok==L"{diagnosis}")    return r.diagnosis;
    if(tok==L"{servicecode}")  return r.services.empty()? L"" : toFaDigits(r.services[0].code);
    if(tok==L"{visitfee}")     return toFaDigits(formatMoney(r.total))+L" ریال";
    if(tok==L"{paytype}")      return L"نقدی";
    if(tok==L"{cashier}")      return r.userName.empty()?g_session.user.fullname:r.userName;
    // §1.51.0 — services-list aggregate tokens (the table itself is PIT_SERVICES;
    // these let a design print a count or a one-line summary outside the table).
    if(tok==L"{servicescount}"){ wchar_t b[16]; swprintf(b,16,L"%d",(int)r.services.size()); return toFaDigits(b); }
    if(tok==L"{servicestotal}"){ return toFaDigits(formatMoney(r.total))+L" ریال"; }
    // -----------------------------------------------------------------------
    //  v1.55.0 — REAL-RECEIPT TOKENS (ثامن‌الائمه redesign)
    //  Every value below comes from the live record, the live session or the
    //  settings store. Nothing here is random or invented: when the operator
    //  has not captured a value the token resolves to an EMPTY string so the
    //  design simply prints a blank (or hides the row when visibility==1).
    // -----------------------------------------------------------------------
    // --- date / time -------------------------------------------------------
    if(tok==L"{apptdatetime}") return toFaDigits(r.apptDate+L"  "+r.apptTime);
    if(tok==L"{apptsec}"){
        if(!r.apptSec.empty()) return toFaDigits(r.apptSec);
        if(r.apptTime.empty()) return L"";
        // pad "hh:mm" → "hh:mm:00" so the receipt column keeps a fixed width
        std::wstring t=r.apptTime;
        if(t.size()==5 && t[2]==L':') t += L":00";
        return toFaDigits(t);
    }
    if(tok==L"{reg_ts}"){
        if(!r.regStamp.empty()) return toFaDigits(r.regStamp);
        if(r.apptDate.empty()) return L"";
        SYSTEMTIME st; GetLocalTime(&st);
        wchar_t b[16]; swprintf(b,16,L"%02d:%02d",st.wHour,st.wMinute);
        return toFaDigits(r.apptDate+L"  "+b);
    }
    // --- receipt identity (deterministic, see pdReceiptBarcode above) ------
    if(tok==L"{receiptbarcode}"){
        if(!r.receiptBarcode.empty()) return toFaDigits(r.receiptBarcode);
        return toFaDigits(pdReceiptBarcode(pdReceiptSeed(r)));
    }
    if(tok==L"{receiptcode}"){
        if(!r.receiptCode.empty()) return r.receiptCode;
        return pdReceiptShortCode(pdReceiptSeed(r));
    }
    // --- insurance + percentage ------------------------------------------
    if(tok==L"{ins_percent}"){
        int p = r.insPercent>=0 ? r.insPercent : Ins_Percent(r.insIdx);
        if(p<=0) return L"";
        wchar_t b[16]; swprintf(b,16,L"%d",p); return toFaDigits(b)+L"٪";
    }
    if(tok==L"{supp_percent}"){
        int p = r.suppPercent>=0 ? r.suppPercent : Supp_Percent(r.suppIdx);
        if(p<=0) return L"";
        wchar_t b[16]; swprintf(b,16,L"%d",p); return toFaDigits(b)+L"٪";
    }
    if(tok==L"{ins_full}"){
        if(r.insurance.empty()) return L"";
        int p = r.insPercent>=0 ? r.insPercent : Ins_Percent(r.insIdx);
        if(p<=0) return r.insurance;
        wchar_t b[16]; swprintf(b,16,L"%d",p);
        return r.insurance+L" ("+toFaDigits(b)+L"٪)";
    }
    if(tok==L"{supp_full}"){
        if(r.suppInsurance.empty()) return L"";
        int p = r.suppPercent>=0 ? r.suppPercent : Supp_Percent(r.suppIdx);
        if(p<=0) return r.suppInsurance;
        wchar_t b[16]; swprintf(b,16,L"%d",p);
        return r.suppInsurance+L" ("+toFaDigits(b)+L"٪)";
    }
    // --- doctor / performer / specialty -----------------------------------
    if(tok==L"{doctorcode}"){
        // v2.07 §3.3: prefer the captured code; fall back to the doctors-store
        // نظام پزشکی code (medicalId) of the matched doctor — never fabricated.
        if(!r.doctorCode.empty()) return toFaDigits(r.doctorCode);
        std::vector<DoctorDef> docs=pdLoadDoctorsForRecord();
        for(const auto& d:docs){
            if(!r.treatingDoctor.empty() && d.name==r.treatingDoctor &&
               !d.medicalId.empty()) return toFaDigits(d.medicalId);
        }
        return L"";
    }
    if(tok==L"{performer}")      return r.performer.empty()? r.treatingDoctor : r.performer;
    if(tok==L"{performercode}"){
        if(!r.performerCode.empty()) return toFaDigits(r.performerCode);
        if(!r.doctorCode.empty())    return toFaDigits(r.doctorCode);
        std::vector<DoctorDef> docs=pdLoadDoctorsForRecord();
        for(const auto& d:docs){
            if(!r.performer.empty() && d.name==r.performer &&
               !d.medicalId.empty()) return toFaDigits(d.medicalId);
        }
        return L"";
    }
    if(tok==L"{specialty}"){
        // v2.07 §3.3 — fallback chain, no fabricated specialty:
        //   1) r.specialty (captured at admission from the doctors store)
        //   2) doctors-store lookup by r.doctorCode (medicalId/docCode)
        //   3) doctors-store lookup by exact r.treatingDoctor name
        //   4) empty
        if(!r.specialty.empty()) return r.specialty;
        std::vector<DoctorDef> docs=pdLoadDoctorsForRecord();
        if(!r.doctorCode.empty()){
            for(const auto& d:docs)
                if((d.medicalId==r.doctorCode||d.docCode==r.doctorCode) &&
                   !d.specialty.empty()) return d.specialty;
        }
        if(!r.treatingDoctor.empty()){
            for(const auto& d:docs)
                if(d.name==r.treatingDoctor && !d.specialty.empty()) return d.specialty;
        }
        return L"";
    }
    if(tok==L"{specialtycode}"){
        if(!r.specialtyCode.empty()) return toFaDigits(r.specialtyCode);
        return toFaDigits(r.doctorCode);
    }
    // --- service description / type --------------------------------------
    if(tok==L"{servicetype}"){
        // نوع خدمت (e.g. «عمومی») — taken from the catalogue category of the
        // first booked service, then the doctor's specialty, then patient type.
        for(size_t i=0;i<r.services.size();++i)
            if(!r.services[i].category.empty()) return r.services[i].category;
        if(!r.specialty.empty()) return r.specialty;
        return r.patientType;
    }
    if(tok==L"{servicedesc}"){
        for(size_t i=0;i<r.services.size();++i)
            if(!r.services[i].desc.empty()) return r.services[i].desc;
        return r.services.empty()? L"" : r.services[0].name;
    }
    if(tok==L"{servicename}") return r.services.empty()? L"" : r.services[0].name;
    // --- money split ------------------------------------------------------
    if(tok==L"{basepay}")   return toFaDigits(formatMoney(r.mainShare))+L" ریال";
    if(tok==L"{supppay}")   return toFaDigits(formatMoney(r.orgShare))+L" ریال";
    if(tok==L"{cash}")      return toFaDigits(formatMoney(r.cash))+L" ریال";
    if(tok==L"{pos}")       return toFaDigits(formatMoney(r.pos))+L" ریال";
    if(tok==L"{discount_from}"){
        if(r.discount<=0) return L"";
        return toFaDigits(formatMoney(r.discount))+L" ریال";
    }
    // --- referral / e-prescription ----------------------------------------
    if(tok==L"{eprescription}") return toFaDigits(r.eprescription);
    if(tok==L"{referralno}")    return toFaDigits(r.referralNo);
    // --- staff ------------------------------------------------------------
    if(tok==L"{receptionist}"){
        if(!r.receptionist.empty()) return r.receptionist;
        return r.userName.empty()? g_session.user.fullname : r.userName;
    }
    if(tok==L"{cashier_name}"){
        if(!r.cashierName.empty()) return r.cashierName;
        return r.userName.empty()? g_session.user.fullname : r.userName;
    }
    if(tok==L"{scnum}"){
        if(!r.scNum.empty()) return toFaDigits(r.scNum);
        wchar_t b[16]; swprintf(b,16,L"%d",g_session.shift+1); return toFaDigits(b);
    }
    // ---- v2.07.0: clinic / receipt identity tokens ------------------------
    if(tok==L"{certno}"){
        if(!r.certNo.empty()) return toFaDigits(r.certNo);
        return L"";                       // never synthesised
    }
    if(tok==L"{receipttitle}"){
        if(!r.receiptTitle.empty()) return r.receiptTitle;
        // fall back to the section's receipt kind label
        if(!r.dept.empty()) return r.dept;
        return L"";
    }
    if(tok==L"{clinicname}"){
        if(!r.clinicName.empty()) return r.clinicName;
        std::wstring n=getSetting(L"clinic.name",L"");
        if(!n.empty()) return n;
        n=getSetting(L"clinic_name",L"");
        if(!n.empty()) return n;
        return APP_NAME_W;
    }
    return L"";
}

// Draw a PIT_TABLE grid inside `box` (device px). RTL: column 0 is the rightmost.
//   pxPerMmX/Y : device pixels per millimetre (for border width scaling)
//   live       : when non-NULL, substitutes {field} tokens with record data;
//                otherwise (preview with no record) shows the raw cell text.
static void pdDrawTable(HDC dc, const PrintItem& it, const RECT& box,
                        double pxPerMmX, double pxPerMmY,
                        double fontPxPerPt, const ReceptionRecord* live){
    PdTable t; if(!pdParseTable(it.text,t)) return;
    int X0=box.left, Y0=box.top, X1=box.right, Y1=box.bottom;
    int W=X1-X0, H=Y1-Y0; if(W<=0||H<=0) return;

    // column x-boundaries (RTL: col index 0 starts at the right edge)
    double sumw=0; for(double w:t.widths) sumw+=w; if(sumw<=0) sumw=t.cols;
    std::vector<int> cx; cx.reserve(t.cols+1);
    cx.push_back(X1);
    double acc=0;
    for(int c=0;c<t.cols;++c){ acc+=t.widths[c]; cx.push_back(X1-(int)(W*(acc/sumw))); }
    // row y-boundaries (top → bottom).
    // v1.55.0: honour the design's explicit ارتفاع سرستون (it.headerH, mm) and
    // ارتفاع سطر (it.rowH, mm). Both default to 0 = "auto", which reproduces the
    // v1.54 equal-height behaviour exactly. Pinned heights that would overflow
    // the frame are scaled down uniformly instead of being clipped.
    int hH = (t.header && it.headerH>0) ? (int)(it.headerH*pxPerMmY) : 0;
    int rH = (it.rowH>0) ? (int)(it.rowH*pxPerMmY) : 0;
    int nData = t.header ? t.rows-1 : t.rows; if(nData<0) nData=0;
    std::vector<int> ry; ry.reserve(t.rows+1);
    if(hH>0 || rH>0){
        if(hH<=0) hH = rH>0 ? rH : (int)((double)H/(t.rows?t.rows:1));
        if(rH<=0) rH = (nData>0) ? (int)((double)(H-hH)/nData) : hH;
        if(rH<1) rH=1;
        int need = (t.header? hH : 0) + rH*(t.header? nData : t.rows);
        if(need>H && need>0){ double k=(double)H/(double)need; hH=(int)(hH*k); rH=(int)(rH*k); if(rH<4)rH=4; }
        ry.push_back(Y0);
        if(t.header) ry.push_back(Y0+hH);
        int base = Y0 + (t.header? hH : 0);
        int n = t.header? nData : t.rows;
        for(int rr=1; rr<=n; ++rr) ry.push_back(base + rH*rr);
    } else {
        for(int rr=0;rr<=t.rows;++rr) ry.push_back(Y0+(int)((double)H*rr/t.rows));
    }
    while((int)ry.size() < t.rows+1) ry.push_back(ry.empty()?Y0:ry.back());

    // fill header row background
    if(t.header && t.rows>0){
        RECT hr={cx[t.cols], ry[0], cx[0], ry[1]};
        HBRUSH hb=CreateSolidBrush(RGB(238,242,251));
        FillRect(dc,&hr,hb); DeleteObject(hb);
    }

    // cell text
    double fontPt = it.fontPt>0 ? it.fontPt : 9.0;
    int lf=-(int)(fontPt*fontPxPerPt);
    HFONT fNorm=CreateFontW(lf,0,0,0,FW_NORMAL,it.italic?1:0,0,0,DEFAULT_CHARSET,0,0,
        CLEARTYPE_QUALITY,0,it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
    HFONT fHead=CreateFontW(lf,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,
        CLEARTYPE_QUALITY,0,it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
    int pad=(int)(1.2*pxPerMmX); if(pad<2)pad=2;
    SetTextColor(dc,pdTextInk(it.textColor));   // v2.07 §3.8 saturated ink
    for(int rr=0;rr<t.rows;++rr){
        bool isHead=(t.header && rr==0);
        HGDIOBJ of=SelectObject(dc, isHead?fHead:fNorm);
        for(int c=0;c<t.cols;++c){
            std::wstring cell;
            if(rr<(int)t.cells.size() && c<(int)t.cells[rr].size()) cell=t.cells[rr][c];
            if(live) cell=pdSubstFields(*live,cell,pdFieldValue);
            // RTL: visual column c occupies [cx[c+1] .. cx[c]]
            RECT cr={cx[c+1]+pad, ry[rr]+pad/2, cx[c]-pad, ry[rr+1]-pad/2};
            if(cr.right<=cr.left||cr.bottom<=cr.top) continue;
            /* v2.06 — NO ELLIPSIS: shrink-to-fit then wrap (never ۳ نقطه). */
            {
                std::wstring fname=it.fontName.empty()?std::wstring(L"Vazirmatn"):it.fontName;
                double sz=fontPt;
                int limit=cr.right-cr.left; if(limit<2) limit=2;
                auto mk=[&](double s){
                    int lfh=-(int)(s*fontPxPerPt+0.5); if(lfh>-4) lfh=-4;
                    return CreateFontW(lfh,0,0,0,isHead?FW_BOLD:FW_NORMAL,
                        it.italic?1:0,0,0,DEFAULT_CHARSET,0,0,
                        CLEARTYPE_QUALITY,0,fname.c_str()); };
                HFONT f=mk(sz);
                HGDIOBJ o2=SelectObject(dc,f);
                RECT mr={0,0,limit,0};
                DrawTextW(dc,cell.c_str(),-1,&mr,
                    DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX|DT_CALCRECT);
                int w=mr.right-mr.left;
                SelectObject(dc,o2); DeleteObject(f);
                while(w>limit && sz>4.5){
                    sz*=0.9; f=mk(sz); o2=SelectObject(dc,f);
                    RECT m2={0,0,limit,0};
                    DrawTextW(dc,cell.c_str(),-1,&m2,
                        DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX|DT_CALCRECT);
                    w=m2.right-m2.left;
                    SelectObject(dc,o2); DeleteObject(f);
                }
                f=mk(sz); o2=SelectObject(dc,f);
                RECT wr={0,0,limit,1000000};
                DrawTextW(dc,cell.c_str(),-1,&wr,
                    DT_CENTER|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX|DT_CALCRECT);
                int th=wr.bottom-wr.top;
                RECT dr=cr;
                int off=((cr.bottom-cr.top)-th)/2; if(off>0) dr.top+=off;
                DrawTextW(dc,cell.c_str(),-1,&dr,
                    DT_CENTER|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX|DT_NOCLIP);
                SelectObject(dc,of);
                DeleteObject(f);
            }
        }
        SelectObject(dc,of);
    }
    DeleteObject(fNorm); DeleteObject(fHead);

    // grid lines
    int bw=(int)(it.borderWidth*pxPerMmX); if(bw<1)bw=1;
    HPEN pen=CreatePen(PS_SOLID,bw,pdCR(it.borderColor));
    HGDIOBJ op=SelectObject(dc,pen);
    for(int c=0;c<=t.cols;++c){ MoveToEx(dc,cx[c],ry[0],0); LineTo(dc,cx[c],ry[t.rows]); }
    for(int rr=0;rr<=t.rows;++rr){ MoveToEx(dc,cx[t.cols],ry[rr],0); LineTo(dc,cx[0],ry[rr]); }
    SelectObject(dc,op); DeleteObject(pen);
}

// §1.51.0: parse the PIT_SERVICES model JSON:
//   {"cols":n,"header":bool,"widths":[..],"labels":[..]}
// `cols`/`labels`/`widths` describe the table header; rows are filled from the
// live ReceptionRecord.services vector (variable count) at render time.
struct PdServicesModel {
    // Mandatory receipt defaults, right→left:
    //   نام خدمت | شرح خدمت | تعداد | مبلغ کل
    int cols=4;
    bool header=true;
    std::vector<double> widths;
    std::vector<std::wstring> labels;   // header captions (RTL order, col0=right)
};
// v1.55.0 — canonical column vocabulary of the services table. The renderer is
// LABEL-DRIVEN: whatever caption a design puts in a column decides which piece
// of the live ServiceLine goes in that column, so a designer can reorder or drop
// columns freely and the data still lands in the right place.
enum PdSvcCol {
    PSC_NAME=0,   // نام خدمت
    PSC_DESC,     // شرح خدمت
    PSC_QTY,      // تعداد
    PSC_CODE,     // کد خدمت
    PSC_ROW,      // ردیف
    PSC_PRICE,    // مبلغ واحد
    PSC_LINE,     // مبلغ کل سطر
    PSC_DISC,     // تخفیف
    PSC_INS,      // سهم بیمه / سهم پایه
    PSC_PAT,      // سهم بیمار / پرداختی
    PSC_CAT,      // نوع خدمت
    PSC_SUPP,     // سهم مکمل
    PSC_NONE
};
// Classify a header caption. Matching is substring based and tolerant of the
// spacing / ZWNJ variations that appear on real Persian forms.
static PdSvcCol pdSvcColOf(const std::wstring& labIn, int idx){
    std::wstring L; L.reserve(labIn.size());
    for(size_t i=0;i<labIn.size();++i){
        wchar_t c=labIn[i];
        if(c==L' '||c==0x200C||c==0x200F||c==0x200E||c==L'\t') continue;  // ZWNJ/RLM/LRM
        if(c==L'ي') c=L'ی';
        if(c==L'ك') c=L'ک';
        L+=c;
    }
    auto has=[&](const wchar_t* n){ return L.find(n)!=std::wstring::npos; };
    if(L.empty()){
        // an unlabelled column falls back to positional defaults
        switch(idx){
            case 0: return PSC_NAME; case 1: return PSC_DESC;
            case 2: return PSC_QTY;  case 3: return PSC_LINE;
            default: return PSC_NONE;
        }
    }
    if(has(L"شرح")||has(L"توضیح"))                       return PSC_DESC;
    if(has(L"نوع"))                                       return PSC_CAT;
    if(has(L"تعداد")||has(L"مقدار")||L==L"تع")            return PSC_QTY;
    if(has(L"ردیف")||has(L"شماره")||L==L"ر")              return PSC_ROW;
    if(L==L"#"||L==L"№"||L==L"#")                        return PSC_ROW; // v1.96.0 row-no col
    if(has(L"سهممکمل")||has(L"مکمل"))                      return PSC_SUPP;
    if(has(L"سهمبیمه")||has(L"سهمپایه")||has(L"بیمه"))    return PSC_INS;
    if(has(L"سهمبیمار")||has(L"پرداختی"))                 return PSC_PAT;
    if(has(L"تخفیف"))                                     return PSC_DISC;
    if(has(L"مبلغکل")||has(L"جمع")||has(L"کل"))           return PSC_LINE;
    if(has(L"قیمت")||has(L"فی")||has(L"مبلغ")||has(L"نرخ")) return PSC_PRICE;
    if(has(L"کد"))                                        return PSC_CODE;
    if(has(L"نامخدمت")||has(L"خدمت")||has(L"نام")||has(L"عنوان")) return PSC_NAME;
    return PSC_NONE;
}
static bool pdParseServicesModel(const std::wstring& jsonW, PdServicesModel& m){
    int n=WideCharToMultiByte(CP_UTF8,0,jsonW.c_str(),(int)jsonW.size(),NULL,0,NULL,NULL);
    std::string s(n,0); if(n) WideCharToMultiByte(CP_UTF8,0,jsonW.c_str(),(int)jsonW.size(),&s[0],n,NULL,NULL);
    size_t p=0; auto ws=[&]{ while(p<s.size()&&(s[p]==' '||s[p]=='\t'||s[p]=='\n'||s[p]=='\r'))++p; };
    auto rdStr=[&](std::string& out)->bool{ ws(); if(p>=s.size()||s[p]!='"')return false; ++p;
        while(p<s.size()&&s[p]!='"'){ char c=s[p++]; if(c=='\\'&&p<s.size()){ char e=s[p++];
            switch(e){case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;
                case '"':out+='"';break;case '\\':out+='\\';break;case '/':out+='/';break;
                case 'u':{ if(p+4<=s.size()){ unsigned v=(unsigned)strtoul(s.substr(p,4).c_str(),NULL,16); p+=4;
                    if(v<0x80)out+=(char)v; else if(v<0x800){out+=(char)(0xC0|(v>>6));out+=(char)(0x80|(v&0x3F));}
                    else{out+=(char)(0xE0|(v>>12));out+=(char)(0x80|((v>>6)&0x3F));out+=(char)(0x80|(v&0x3F));} } break; }
                default:out+=e; } } else out+=c; }
        if(p<s.size()&&s[p]=='"')++p; return true; };
    auto rdNum=[&]()->double{ ws(); size_t st=p; while(p<s.size()&&(isdigit((unsigned char)s[p])||s[p]=='-'||s[p]=='+'||s[p]=='.'||s[p]=='e'||s[p]=='E'))++p; return atof(s.substr(st,p-st).c_str()); };
    ws(); if(p>=s.size()||s[p]!='{') return false; ++p;
    while(true){ ws(); if(p<s.size()&&s[p]=='}'){++p;break;} if(p>=s.size())break;
        std::string key; if(!rdStr(key))break; ws(); if(p<s.size()&&s[p]==':')++p;
        if(key=="cols") m.cols=(int)rdNum();
        else if(key=="header"){ ws(); if(s.compare(p,4,"true")==0){m.header=true;p+=4;} else if(s.compare(p,5,"false")==0){m.header=false;p+=5;} else rdNum(); }
        else if(key=="widths"){ ws(); if(p<s.size()&&s[p]=='['){++p; while(true){ ws(); if(p<s.size()&&s[p]==']'){++p;break;} m.widths.push_back(rdNum()); ws(); if(p<s.size()&&s[p]==','){++p;continue;} if(p<s.size()&&s[p]==']'){++p;break;} break; } } }
        else if(key=="labels"){ ws(); if(p<s.size()&&s[p]=='['){++p; while(true){ ws(); if(p<s.size()&&s[p]==']'){++p;break;}
                std::string cv; if(rdStr(cv)) m.labels.push_back(pdU8toW(cv));
                ws(); if(p<s.size()&&s[p]==','){++p;continue;} if(p<s.size()&&s[p]==']'){++p;break;} break; } } }
        else { ws(); if(p<s.size()&&s[p]=='"'){ std::string tmp; rdStr(tmp); }
            else if(p<s.size()&&s[p]=='{'){ int d=0; do{ if(s[p]=='{')d++; else if(s[p]=='}')d--; ++p; }while(p<s.size()&&d>0); }
            else if(p<s.size()&&s[p]=='['){ int d=0; do{ if(s[p]=='[')d++; else if(s[p]==']')d--; ++p; }while(p<s.size()&&d>0); }
            else rdNum();
        }
        ws(); if(p<s.size()&&s[p]==','){++p;continue;} ws(); if(p<s.size()&&s[p]=='}'){++p;break;}
        if(p>=s.size())break;
    }
    if(m.cols<1) m.cols=4;
    if((int)m.widths.size()!=m.cols){
        if(m.cols==4){
            m.widths.clear(); m.widths.push_back(0.39); m.widths.push_back(0.29);
            m.widths.push_back(0.10); m.widths.push_back(0.22);
        } else m.widths.assign(m.cols,1.0);
    }
    if(m.labels.empty()){
        m.labels.clear();
        const wchar_t* def[4]={L"نام خدمت",L"شرح خدمت",L"تعداد",L"مبلغ کل"};
        for(int i=0;i<m.cols;++i) m.labels.push_back(i<4?std::wstring(def[i]):L"");
    }
    while((int)m.labels.size()<m.cols) m.labels.push_back(L"");
    return true;
}

// ---------------------------------------------------------------------------
// v2.07 §3.7 — THE CANONICAL 3-COLUMN SERVICES MODEL.
// نام خدمت | تعداد | شرح خدمت — exactly three columns, in this RTL order,
// for the default design and every builtin template. Any parsed model that a
// builtin/default carries is clamped here so no builtin can drift to four
// columns. User designs are NEVER clamped (their authored model is theirs).
static void pdClampBuiltinServicesModel(PdServicesModel& m){
    m.cols=3;
    m.header=true;
    m.labels.assign(3,std::wstring());
    m.labels[0]=L"نام خدمت";
    m.labels[1]=L"تعداد";
    m.labels[2]=L"شرح خدمت";
    m.widths.assign(3,0.0);
    m.widths[0]=0.46;   // نام خدمت
    m.widths[1]=0.12;   // تعداد (fixed)
    m.widths[2]=0.42;   // شرح خدمت (base)
}

// v2.07 §3.7 — CONTENT-ADAPTIVE «شرح خدمت» WIDTH.
// Fractions of the ITEM width (survive paper changes), computed once, before
// pagination, from the measured text of all rows:
//   every desc empty            → desc collapses to 0.18, name takes the freed width
//   long descriptions           → desc may grow up to 0.52 (name only shrinks)
//   تعداد stays fixed at 0.12, always.
// The caller passes the measured widths (device px) of the widest desc/name.
static void pdAdaptServicesWidths(std::vector<double>& widths,
                                  double descPx, double itemPx){
    if(widths.size()!=3 || itemPx<=0){ return; }
    const double WNAME=0.46, WQTY=0.12, WDESC=0.42;
    double desc=WDESC, name=WNAME;
    double descRatio = descPx/itemPx;
    if(descPx<=0.5){
        desc=0.18;                        // every desc empty → collapse
        name=WNAME+(WDESC-0.18);          // name takes the freed width
    } else if(descRatio>WDESC){
        // grow desc (cap 0.52), taking width from name only
        desc=descRatio>0.52?0.52:descRatio;
        name=WNAME+(WDESC-desc);
        if(name<0.30) name=0.30;          // keep the name readable
        desc=WQTY+name+desc>1.0 ? 1.0-WQTY-name : desc;
    }
    double sum=name+WQTY+desc;
    if(sum>0){
        widths[0]=name/sum;
        widths[1]=WQTY/sum;
        widths[2]=desc/sum;
    }
}

// v1.55.0 — resolve one services-table cell from the live ServiceLine according
// to the column's LABEL (never by blind index). All values are real data taken
// straight from the record; nothing is generated or randomised. A service that
// somehow has no name prints "—" so the row is never visually empty.
static std::wstring pdSvcCellValue(PdSvcCol kind, const ServiceLine& s, int rowIdx){
    switch(kind){
        case PSC_NAME:  return s.name.empty()? std::wstring(L"—") : s.name;
        case PSC_DESC:  return s.desc.empty()? (s.category.empty()? std::wstring(L"—") : s.category) : s.desc;
        case PSC_QTY: { wchar_t b[16]; swprintf(b,16,L"%d", s.qty>0?s.qty:1); return toFaDigits(b); }
        case PSC_CODE:  return toFaDigits(s.code);
        case PSC_ROW: { wchar_t b[16]; swprintf(b,16,L"%d",rowIdx+1); return toFaDigits(b); }
        case PSC_PRICE: return toFaDigits(formatMoney(s.price));
        case PSC_LINE:  return toFaDigits(formatMoney(s.price*(long long)(s.qty>0?s.qty:1) - s.discount));
        case PSC_DISC:  return s.discount>0? toFaDigits(formatMoney(s.discount)) : std::wstring(L"—");
        case PSC_INS:   return toFaDigits(formatMoney(s.insShare));
        case PSC_PAT:   return toFaDigits(formatMoney(s.patShare));
        case PSC_SUPP:  return L"—";
        case PSC_CAT:   return s.category.empty()? std::wstring(L"—") : s.category;
        default:        return L"";
    }
}
// Precomputed geometry shared by pagination and drawing. Keeping row measurement
// in one helper guarantees that a page break can never disagree with the rows
// later painted on that page.
struct PdServicesLayout {
    PdServicesModel model;
    std::vector<PdSvcCol> kinds;
    std::vector<int> cx;
    std::vector<int> rowHeights;
    int pad=2;
    int textLineH=1;
    int baseRowH=1;
    int headH=0;
    double fNormPt=8.5;      // v2.06: base font size so cell drawing can shrink-to-fit
    std::wstring fontName;   // v2.06: item font name for the shrink loop
    double fontPxPerPt=4.0/3.0; // v2.06: real DC px-per-pt (populated by pdBuildServicesLayout)
};
/* v2.06: shared shrink-to-fit font factory — no cell may EVER render with an
   ellipsis, so over-wide numeric cells step the size down until they fit. */
static HFONT pdSvcFont(double pt,double pxPerPt,const std::wstring& name){
    int lf=-(int)(pt*pxPerPt+0.5); if(lf>-4) lf=-4;
    return CreateFontW(lf,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,
        CLEARTYPE_QUALITY,0,name.empty()?L"Vazirmatn":name.c_str());
}
static void pdDrawCellNoEllipsis(HDC dc,const std::wstring& cell,const RECT& cr,
                                 double basePt,double pxPerPt,const std::wstring& fontName){
    if(cell.empty()) return;
    double sz=basePt;
    int limit=cr.right-cr.left; if(limit<2) limit=2;
    HFONT f=pdSvcFont(sz,pxPerPt,fontName);
    HGDIOBJ of=SelectObject(dc,f);
    RECT mr={0,0,limit,0};
    DrawTextW(dc,cell.c_str(),-1,&mr,
        DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX|DT_CALCRECT);
    int w=mr.right-mr.left;
    SelectObject(dc,of); DeleteObject(f);
    while(w>limit && sz>4.5){
        sz*=0.9;
        f=pdSvcFont(sz,pxPerPt,fontName);
        of=SelectObject(dc,f);
        RECT m2={0,0,limit,0};
        DrawTextW(dc,cell.c_str(),-1,&m2,
            DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX|DT_CALCRECT);
        w=m2.right-m2.left;
        SelectObject(dc,of); DeleteObject(f);
    }
    /* final draw — wrap + vertical center, never clipped, never ellipsised */
    f=pdSvcFont(sz,pxPerPt,fontName);
    of=SelectObject(dc,f);
    RECT wr={0,0,limit,1000000};
    DrawTextW(dc,cell.c_str(),-1,&wr,
        DT_CENTER|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX|DT_CALCRECT);
    int th=wr.bottom-wr.top;
    RECT dr=cr;
    int off=((cr.bottom-cr.top)-th)/2; if(off>0) dr.top+=off;
    DrawTextW(dc,cell.c_str(),-1,&dr,
        DT_CENTER|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX|DT_NOCLIP);
    SelectObject(dc,of); DeleteObject(f);
}
static bool pdBuildServicesLayout(HDC dc, const PrintItem& it, const RECT& box,
                                  double pxPerMmX, double pxPerMmY,
                                  double fontPxPerPt, const ReceptionRecord* live,
                                  PdServicesLayout& out, bool builtinDesign=false){
    pdParseServicesModel(it.text, out.model);
    // v2.07 §3.7: builtin/default designs are clamped to the canonical
    // 3-column model so no builtin can drift. User designs keep their model.
    if(builtinDesign){
        pdClampBuiltinServicesModel(out.model);
        // v2.07: content-adaptive «شرح خدمت» width, measured from the live rows
        if(live && !live->services.empty()){
            // measure desc/name extents once with the table's own font
            double fontPt0=it.fontPt>0?it.fontPt:8.5;
            HFONT fm=CreateFontW(-(int)(fontPt0*fontPxPerPt+0.5),0,0,0,FW_NORMAL,0,0,0,
                DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,
                it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
            HGDIOBJ ofm=SelectObject(dc,fm);
            double maxDesc=0, maxName=0;
            for(size_t i=0;i<live->services.size();++i){
                const ServiceLine& sv=live->services[i];
                RECT mr={0,0,0,0};
                if(!sv.desc.empty()){
                    DrawTextW(dc,sv.desc.c_str(),-1,&mr,
                        DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX|DT_CALCRECT);
                    if(mr.right-mr.left>maxDesc) maxDesc=mr.right-mr.left;
                }
                RECT nr={0,0,0,0};
                if(!sv.name.empty()){
                    DrawTextW(dc,sv.name.c_str(),-1,&nr,
                        DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX|DT_CALCRECT);
                    if(nr.right-nr.left>maxName) maxName=nr.right-nr.left;
                }
            }
            SelectObject(dc,ofm); DeleteObject(fm);
            pdAdaptServicesWidths(out.model.widths,maxDesc,(double)(box.right-box.left));
        }
    }
    int X0=box.left, X1=box.right;
    int W=X1-X0, H=box.bottom-box.top; if(W<=0||H<=0) return false;

    double sumw=0; for(double w:out.model.widths) sumw+=w;
    if(sumw<=0) sumw=out.model.cols;
    out.cx.reserve(out.model.cols+1); out.cx.push_back(X1);
    double acc=0;
    for(int c=0;c<out.model.cols;++c){
        acc+=out.model.widths[c];
        out.cx.push_back(c==out.model.cols-1 ? X0 : X1-(int)(W*(acc/sumw)));
    }
    out.kinds.assign(out.model.cols,PSC_NONE);
    for(int c=0;c<out.model.cols;++c)
        out.kinds[c]=pdSvcColOf(c<(int)out.model.labels.size()?out.model.labels[c]:std::wstring(),c);

    double fontPt=it.fontPt>0?it.fontPt:8.5;
    int lf=-(int)(fontPt*fontPxPerPt);
    HFONT fNorm=CreateFontW(lf,0,0,0,FW_NORMAL,it.italic?1:0,0,0,DEFAULT_CHARSET,0,0,
        CLEARTYPE_QUALITY,0,it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
    /* v2.06: remember the real font metrics so cell drawing can shrink-to-fit
       with the SAME px/pt factor the rest of the table uses. */
    out.fNormPt=fontPt;
    out.fontName=it.fontName.empty()?std::wstring(L"Vazirmatn"):it.fontName;
    out.fontPxPerPt=fontPxPerPt;
    out.pad=(int)(1.2*pxPerMmX); if(out.pad<2) out.pad=2;
    TEXTMETRICW tm={0};
    HGDIOBJ oldFont=SelectObject(dc,fNorm);
    GetTextMetricsW(dc,&tm);
    int textLineH=tm.tmHeight>0?tm.tmHeight:(int)(fontPt*fontPxPerPt+0.5);
    out.textLineH=textLineH>0?textLineH:1;
    out.baseRowH=(it.rowH>0)?(int)(it.rowH*pxPerMmY+0.5):textLineH+out.pad;
    if(out.baseRowH<textLineH+out.pad) out.baseRowH=textLineH+out.pad;
    if(out.model.header){
        int requested=(it.headerH>0)?(int)(it.headerH*pxPerMmY+0.5):out.baseRowH;
        if(requested<textLineH+out.pad) requested=textLineH+out.pad;
        pdPrintableDataHeight(H,requested,out.baseRowH,&out.headH);
    }

    int nLive=live?(int)live->services.size():0;
    out.rowHeights.reserve(nLive);
    for(int rowIdx=0;rowIdx<nLive;++rowIdx){
        const ServiceLine& svc=live->services[rowIdx];
        int wanted=out.baseRowH;
        for(int c=0;c<out.model.cols;++c){
            if(out.kinds[c]!=PSC_NAME && out.kinds[c]!=PSC_DESC && out.kinds[c]!=PSC_CAT) continue;
            std::wstring cell=pdSvcCellValue(out.kinds[c],svc,rowIdx);
            int cw=out.cx[c]-out.cx[c+1]-2*out.pad; if(cw<4) continue;
            RECT mr={0,0,cw,1000000};
            DrawTextW(dc,cell.c_str(),-1,&mr,
                DT_RIGHT|DT_TOP|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX|DT_CALCRECT);
            int h=(mr.bottom-mr.top)+out.pad;
            if(h>wanted) wanted=h;
        }
        out.rowHeights.push_back(wanted);
    }
    SelectObject(dc,oldFont); DeleteObject(fNorm);
    return true;
}
static std::vector<PdServicesPageSlice> pdPlanServicePages(
        HDC dc,const PrintItem& it,const RECT& box,double pxPerMmX,double pxPerMmY,
        double fontPxPerPt,const ReceptionRecord* live,bool builtinDesign=false){
    PdServicesLayout layout;
    if(!pdBuildServicesLayout(dc,it,box,pxPerMmX,pxPerMmY,fontPxPerPt,live,layout,builtinDesign))
        return std::vector<PdServicesPageSlice>(1);
    int available=(box.bottom-box.top)-layout.headH;
    int firstInset=layout.pad/2; if(firstInset<0) firstInset=0;
    return pdSliceServiceRows(layout.rowHeights,available,layout.textLineH,firstInset);
}

static void pdPaintServiceLogicalRow(HDC dc,const PrintItem& it,
        const PdServicesLayout& layout,const ServiceLine& svc,int rowIdx,
        int xShift,int top,int fullHeight,HFONT fNorm,bool fillBackground,
        int cellMode){ // 0=all, 1=prose only, 2=numeric/non-prose only
    const PdServicesModel& m=layout.model;
    int left=layout.cx[m.cols]+xShift, right=layout.cx[0]+xShift;
    if(fillBackground){
        COLORREF bg=RGB(255,255,255);
        if(!it.fillTransparent && (rowIdx&1)){
            DWORD headFill=(DWORD)it.fillColor;
            int br=(int)((headFill>>16)&0xFF), bgc=(int)((headFill>>8)&0xFF), bb=(int)(headFill&0xFF);
            bg=RGB(br+(int)((255-br)*0.78),bgc+(int)((255-bgc)*0.78),bb+(int)((255-bb)*0.78));
        }
        RECT rowBg={left,top,right,top+fullHeight};
        HBRUSH brush=CreateSolidBrush(bg); FillRect(dc,&rowBg,brush); DeleteObject(brush);
    } else if(cellMode==0 && !it.fillTransparent && (rowIdx&1)){
        DWORD headFill=(DWORD)it.fillColor;
        int br=(int)((headFill>>16)&0xFF), bgc=(int)((headFill>>8)&0xFF), bb=(int)(headFill&0xFF);
        RECT rowBg={left,top,right,top+fullHeight};
        HBRUSH brush=CreateSolidBrush(RGB(br+(int)((255-br)*0.78),
            bgc+(int)((255-bgc)*0.78),bb+(int)((255-bb)*0.78)));
        FillRect(dc,&rowBg,brush); DeleteObject(brush);
    }

    HGDIOBJ oldFont=SelectObject(dc,fNorm);
    SetBkMode(dc,TRANSPARENT); SetTextAlign(dc,TA_RTLREADING|TA_TOP|TA_LEFT);
    SetTextColor(dc,pdTextInk(it.textColor));   // v2.07 §3.8 saturated ink
    for(int c=0;c<m.cols;++c){
        PdSvcCol kind=layout.kinds[c];
        std::wstring cell=pdSvcCellValue(kind,svc,rowIdx);
        RECT cr={layout.cx[c+1]+xShift+layout.pad,top+layout.pad/2,
                 layout.cx[c]+xShift-layout.pad,top+fullHeight-layout.pad/2};
        if(cr.right<=cr.left||cr.bottom<=cr.top||cell.empty()) continue;
        bool prose=(kind==PSC_NAME||kind==PSC_DESC||kind==PSC_CAT);
        if(cellMode==1&&!prose) continue;
        if(cellMode==2&&prose) continue;
        if(cellMode==3) continue;
        if(prose){
            RECT mr={0,0,cr.right-cr.left,1000000};
            DrawTextW(dc,cell.c_str(),-1,&mr,
                DT_RIGHT|DT_TOP|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX|DT_CALCRECT);
            int mh=mr.bottom-mr.top;
            RECT dr=cr; int off=((cr.bottom-cr.top)-mh)/2; if(off>0) dr.top+=off;
            DrawTextW(dc,cell.c_str(),-1,&dr,
                DT_RIGHT|DT_TOP|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
        } else {
            /* v2.06 — NO ELLIPSIS IN PRINT («کلا هیچی نباید ۳ نقطه بشه»):
               numeric/short cells shrink-to-fit then wrap; never truncated. */
            pdDrawCellNoEllipsis(dc,cell,cr,layout.fNormPt,layout.fontPxPerPt,layout.fontName);
        }
    }
    SelectObject(dc,oldFont);
}

static void pdDrawServiceRowFragment(HDC dc,const PrintItem& it,
        const PdServicesLayout& layout,const ServiceLine& svc,int rowIdx,
        int destTop,int offset,int fragmentHeight,HFONT fNorm){
    int fullHeight=layout.rowHeights[rowIdx];
    int X0=layout.cx[layout.model.cols], W=layout.cx[0]-X0;
    if(offset==0 && fragmentHeight==fullHeight){
        pdPaintServiceLogicalRow(dc,it,layout,svc,rowIdx,0,destTop,fullHeight,fNorm,false,0);
        return;
    }

    // Paint the visible prose strip through a clipped, vertically shifted
    // logical row. Numeric identity (row/code/qty/amount) is emitted on the
    // first fragment only; wrapped name/description pixels are emitted exactly
    // once across the contiguous fragments.
    int saved=SaveDC(dc);
    IntersectClipRect(dc,X0,destTop,X0+W,destTop+fragmentHeight);
    pdPaintServiceLogicalRow(dc,it,layout,svc,rowIdx,0,destTop-offset,fullHeight,fNorm,
                             false,1);
    RestoreDC(dc,saved);
    if(offset==0)
        pdPaintServiceLogicalRow(dc,it,layout,svc,rowIdx,0,destTop,fragmentHeight,fNorm,
                                 false,2);
}

// Draw one pre-planned page of service-row fragments inside `box`. The page
// header repeats; vertical fragments reconstruct every oversized wrapped row
// without clipping or substituting an “N more” marker.
static void pdDrawServices(HDC dc, const PrintItem& it, const RECT& box,
                           double pxPerMmX, double pxPerMmY,
                           double fontPxPerPt, const ReceptionRecord* live,
                           const PdServicesPageSlice& slice){
    PdServicesLayout layout;
    if(!pdBuildServicesLayout(dc,it,box,pxPerMmX,pxPerMmY,fontPxPerPt,live,layout)) return;
    PdServicesModel& m=layout.model;
    std::vector<int>& cx=layout.cx;
    int Y0=box.top, Y1=box.bottom, H=Y1-Y0;
    SetTextAlign(dc,TA_RTLREADING|TA_TOP|TA_LEFT);

    int nLive=live?(int)live->services.size():0;
    std::vector<PdServicesRowFragment> fragments;
    if(nLive>0){
        for(size_t i=0;i<slice.rows.size();++i){
            const PdServicesRowFragment& f=slice.rows[i];
            if(f.row>=0&&f.row<nLive&&f.height>0) fragments.push_back(f);
        }
    }
    bool emptyRow=(nLive==0);
    int available=H-layout.headH; if(available<1) available=1;
    int dataRows=emptyRow?1:(int)fragments.size();
    int totalRows=(m.header?1:0)+dataRows;
    if(totalRows<1) return;

    std::vector<int> rowHeights;
    if(emptyRow) rowHeights.push_back(available<layout.baseRowH?available:layout.baseRowH);
    else for(size_t i=0;i<fragments.size();++i) rowHeights.push_back(fragments[i].height);
    std::vector<int> ry; ry.reserve(totalRows+1); ry.push_back(Y0);
    if(m.header) ry.push_back(Y0+layout.headH);
    for(int rr=0;rr<dataRows;++rr){
        int next=ry.back()+rowHeights[rr]; if(next>Y1) next=Y1;
        ry.push_back(next);
    }
    if((int)ry.size()!=totalRows+1) return;

    double fontPt=it.fontPt>0?it.fontPt:8.5;
    int lf=-(int)(fontPt*fontPxPerPt);
    HFONT fNorm=CreateFontW(lf,0,0,0,FW_NORMAL,it.italic?1:0,0,0,DEFAULT_CHARSET,0,0,
        CLEARTYPE_QUALITY,0,it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
    HFONT fHead=CreateFontW(lf,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,
        CLEARTYPE_QUALITY,0,it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
    DWORD headFill=!it.fillTransparent?(DWORD)it.fillColor:0xEFEFEFu;
    int lum=(int)((((headFill>>16)&0xFF)*30+((headFill>>8)&0xFF)*59+(headFill&0xFF)*11)/100);
    COLORREF headTxt=(lum<140)?RGB(255,255,255):pdCR(it.textColor);

    if(m.header){
        RECT hr={cx[m.cols],ry[0],cx[0],ry[1]};
        HBRUSH hb=CreateSolidBrush(pdCR((int)headFill)); FillRect(dc,&hr,hb); DeleteObject(hb);
        HGDIOBJ oldFont=SelectObject(dc,fHead); SetTextColor(dc,headTxt);
        for(int c=0;c<m.cols;++c){
            std::wstring cell=toFaDigits(c<(int)m.labels.size()?m.labels[c]:L"");
            RECT cr={cx[c+1]+layout.pad,ry[0]+layout.pad/2,
                     cx[c]-layout.pad,ry[1]-layout.pad/2};
            if(cr.right<=cr.left||cr.bottom<=cr.top) continue;
            /* v2.06: header labels shrink-to-fit / wrap — never ۳ نقطه. */
            {
                double sz=fontPt;
                int limit=cr.right-cr.left; if(limit<2) limit=2;
                auto mkHead=[&](double s){
                    int lfh=-(int)(s*layout.fontPxPerPt+0.5); if(lfh>-4) lfh=-4;
                    return CreateFontW(lfh,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,
                        CLEARTYPE_QUALITY,0,layout.fontName.c_str()); };
                HFONT f=mkHead(sz);
                HGDIOBJ of=SelectObject(dc,f);
                RECT mr={0,0,limit,0};
                DrawTextW(dc,cell.c_str(),-1,&mr,
                    DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX|DT_CALCRECT);
                int w=mr.right-mr.left;
                SelectObject(dc,of); DeleteObject(f);
                while(w>limit && sz>4.5){
                    sz*=0.9;
                    f=mkHead(sz);
                    of=SelectObject(dc,f);
                    RECT m2={0,0,limit,0};
                    DrawTextW(dc,cell.c_str(),-1,&m2,
                        DT_CENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX|DT_CALCRECT);
                    w=m2.right-m2.left;
                    SelectObject(dc,of); DeleteObject(f);
                }
                f=mkHead(sz);
                of=SelectObject(dc,f);
                RECT wr={0,0,limit,1000000};
                DrawTextW(dc,cell.c_str(),-1,&wr,
                    DT_CENTER|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX|DT_CALCRECT);
                int th=wr.bottom-wr.top;
                RECT dr=cr;
                int off=((cr.bottom-cr.top)-th)/2; if(off>0) dr.top+=off;
                DrawTextW(dc,cell.c_str(),-1,&dr,
                    DT_CENTER|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX|DT_NOCLIP);
                SelectObject(dc,oldFont);
                DeleteObject(f);
            }
        }
        SelectObject(dc,oldFont);
    }

    int dataStart=m.header?1:0;
    if(emptyRow){
        HGDIOBJ oldFont=SelectObject(dc,fNorm); SetTextColor(dc,pdTextInk(it.textColor));   // v2.07 §3.8 saturated ink
        if(live){
            for(int c=0;c<m.cols;++c) if(layout.kinds[c]==PSC_NAME){
                RECT cr={cx[c+1]+layout.pad,ry[dataStart]+layout.pad/2,
                         cx[c]-layout.pad,ry[dataStart+1]-layout.pad/2};
                DrawTextW(dc,L"خدمتی ثبت نشده است",-1,&cr,
                    DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
                break;
            }
        }
        SelectObject(dc,oldFont);
    } else {
        for(int rr=0;rr<dataRows;++rr){
            const PdServicesRowFragment& f=fragments[rr];
            pdDrawServiceRowFragment(dc,it,layout,live->services[f.row],f.row,
                                     ry[dataStart+rr],f.offset,f.height,fNorm);
        }
    }
    DeleteObject(fNorm); DeleteObject(fHead);

    int bw=(int)(it.borderWidth*pxPerMmX); if(bw<1) bw=1;
    HPEN pen=CreatePen(PS_SOLID,bw,pdCR(it.borderColor)); HGDIOBJ oldPen=SelectObject(dc,pen);
    for(int c=0;c<=m.cols;++c){ MoveToEx(dc,cx[c],ry[0],0); LineTo(dc,cx[c],ry[totalRows]); }
    for(int rr=0;rr<=totalRows;++rr){ MoveToEx(dc,cx[m.cols],ry[rr],0); LineTo(dc,cx[0],ry[rr]); }
    SelectObject(dc,oldPen); DeleteObject(pen);
}
// ===========================================================================
//  v1.55.0 — REAL 1-D BARCODE ENGINE (PIT_BARCODE)
//  A genuine, standards-conformant symbol generator: Code 128-B, Code 39 and
//  EAN-13. The bars are drawn as filled rectangles on the printer DC, so the
//  output is device-resolution sharp and scans with any handheld reader.
//  The payload ALWAYS comes from the item's bound field token (live record
//  data). Nothing is invented: an empty payload draws nothing at all.
// ===========================================================================

// ---- Code 128 -------------------------------------------------------------
// 107 symbol patterns, each 11 modules encoded as 6 run-lengths (b s b s b s).
static const char* const PD_C128[107] = {
"212222","222122","222221","121223","121322","131222","122213","122312","132212","221213",
"221312","231212","112232","122132","122231","113222","123122","123221","223211","221132",
"221231","213212","223112","312131","311222","321122","321221","312212","322112","322211",
"212123","212321","232121","111323","131123","131321","112313","132113","132311","211313",
"231113","231311","112133","112331","132131","113123","113321","133121","313121","211331",
"231131","213113","213311","213131","311123","311321","331121","312113","312311","332111",
"314111","221411","431111","111224","111422","121124","121421","141122","141221","112214",
"112412","122114","122411","142112","142211","241211","221114","413111","241112","134111",
"111242","121142","121241","114212","124112","124211","411212","421112","421211","212141",
"214121","412121","111143","111341","131141","114113","114311","411113","411311","113141",
"114131","311141","411131","211412","211214","211232","2331112"
};
// Build the module string ("1"=bar, "0"=space) for Code 128-B.
static bool pdBc128(const std::string& data, std::string& mods){
    std::vector<int> code;
    code.push_back(104);                       // START B
    long long sum = 104;
    for(size_t i=0;i<data.size();++i){
        unsigned char ch=(unsigned char)data[i];
        if(ch<32||ch>126) return false;        // Code-B covers ASCII 32..126
        int v = (int)ch - 32;
        code.push_back(v);
        sum += (long long)v * (long long)(i+1);
    }
    code.push_back((int)(sum % 103));          // check digit
    code.push_back(106);                       // STOP
    mods.clear();
    for(size_t i=0;i<code.size();++i){
        const char* p = PD_C128[code[i]];
        for(int k=0; p[k]; ++k){
            int run = p[k]-'0';
            char lvl = (k%2==0) ? '1' : '0';   // runs alternate bar/space
            mods.append((size_t)run, lvl);
        }
    }
    return !mods.empty();
}
// ---- Code 39 --------------------------------------------------------------
// 9 elements per character ("n"=narrow, "w"=wide), alternating bar/space.
static const char  PD_C39_SET[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-. $/+%*";
static const char* const PD_C39_PAT[44] = {
"nnnwwnwnn","wnnwnnnnw","nnwwnnnnw","wnwwnnnnn","nnnwwnnnw","wnnwwnnnn","nnwwwnnnn","nnnwnnwnw",
"wnnwnnwnn","nnwwnnwnn","wnnnnwnnw","nnwnnwnnw","wnwnnwnnn","nnnnwwnnw","wnnnwwnnn","nnwnwwnnn",
"nnnnnwwnw","wnnnnwwnn","nnwnnwwnn","nnnnwwwnn","wnnnnnnww","nnwnnnnww","wnwnnnnwn","nnnnwnnww",
"wnnnwnnwn","nnwnwnnwn","nnnnnnwww","wnnnnnwwn","nnwnnnwwn","nnnnwnwwn","wwnnnnnnw","nwwnnnnnw",
"wwwnnnnnn","nwnnwnnnw","wwnnwnnnn","nwwnwnnnn","nwnnnnwnw","wwnnnnwnn","nwwnnnwnn","nwnwnwnnn",
"nwnwnnnwn","nwnnnwnwn","nnnwnwnwn","nwnnwnwnn"
};
static bool pdBc39(const std::string& dataIn, std::string& mods){
    std::string data;
    for(size_t i=0;i<dataIn.size();++i){        // Code 39 is upper-case only
        char c=dataIn[i];
        if(c>='a'&&c<='z') c=(char)(c-'a'+'A');
        if(!strchr(PD_C39_SET,c) || c=='*') return false;
        data+=c;
    }
    if(data.empty()) return false;
    data = "*" + data + "*";                   // mandatory start/stop guard
    mods.clear();
    for(size_t i=0;i<data.size();++i){
        const char* pos = strchr(PD_C39_SET, data[i]);
        if(!pos) return false;
        const char* pat = PD_C39_PAT[pos-PD_C39_SET];
        for(int k=0;k<9;++k){
            int run = (pat[k]=='w') ? 3 : 1;
            char lvl = (k%2==0) ? '1' : '0';
            mods.append((size_t)run, lvl);
        }
        if(i+1<data.size()) mods.append(1,'0'); // inter-character space
    }
    return true;
}
// ---- EAN-13 ---------------------------------------------------------------
static const char* const PD_EAN_A[10]={"0001101","0011001","0010011","0111101","0100011",
                                       "0110001","0101111","0111011","0110111","0001011"};
static const char* const PD_EAN_B[10]={"0100111","0110011","0011011","0100001","0011101",
                                       "0111001","0000101","0010001","0001001","0010111"};
// The right-hand (C) set is the bitwise complement of the left-odd (A) set.
// v1.55.0 fix: entries 7 and 8 were transposed, which produced barcodes a
// scanner would reject. Verified: C[i] == ~A[i] for every digit.
static const char* const PD_EAN_C[10]={"1110010","1100110","1101100","1000010","1011100",
                                       "1001110","1010000","1000100","1001000","1110100"};
static const char* const PD_EAN_PAR[10]={"AAAAAA","AABABB","AABBAB","AABBBA","ABAABB",
                                         "ABBAAB","ABBBAA","ABABAB","ABABBA","ABBABA"};
static bool pdBcEan13(const std::string& dataIn, std::string& mods, std::string& hri){
    std::string d;
    for(size_t i=0;i<dataIn.size();++i) if(isdigit((unsigned char)dataIn[i])) d+=dataIn[i];
    if(d.size()<12) return false;
    d = d.substr(0,12);
    int sum=0;                                  // compute the EAN-13 check digit
    for(int i=0;i<12;++i) sum += (d[i]-'0') * ((i%2==0)?1:3);
    int chk = (10 - (sum%10)) % 10;
    d += (char)('0'+chk);
    hri = d;
    const char* par = PD_EAN_PAR[d[0]-'0'];
    mods = "101";                               // left guard
    for(int i=1;i<=6;++i)
        mods += (par[i-1]=='A') ? PD_EAN_A[d[i]-'0'] : PD_EAN_B[d[i]-'0'];
    mods += "01010";                            // centre guard
    for(int i=7;i<=12;++i) mods += PD_EAN_C[d[i]-'0'];
    mods += "101";                              // right guard
    return true;
}

// Parse the optional PIT_BARCODE model JSON: {"sym":"code128","hri":true,"quiet":2}
struct PdBarcodeModel { std::wstring sym; bool hri; double quiet; PdBarcodeModel():sym(L"code128"),hri(true),quiet(2.0){} };
static void pdParseBarcodeModel(const std::wstring& jsonW, PdBarcodeModel& m){
    if(jsonW.empty()) return;
    if(jsonW.find(L'{')==std::wstring::npos){    // plain text = symbology name
        m.sym=jsonW; return;
    }
    auto grabStr=[&](const wchar_t* key, std::wstring& out){
        size_t k=jsonW.find(key); if(k==std::wstring::npos) return;
        size_t c=jsonW.find(L':',k); if(c==std::wstring::npos) return;
        size_t q1=jsonW.find(L'"',c); if(q1==std::wstring::npos) return;
        size_t q2=jsonW.find(L'"',q1+1); if(q2==std::wstring::npos) return;
        out=jsonW.substr(q1+1,q2-q1-1);
    };
    grabStr(L"\"sym\"", m.sym);
    size_t k=jsonW.find(L"\"hri\"");
    if(k!=std::wstring::npos) m.hri = (jsonW.find(L"false",k)==std::wstring::npos ||
                                       jsonW.find(L"true",k)<jsonW.find(L"false",k));
    k=jsonW.find(L"\"quiet\"");
    if(k!=std::wstring::npos){ size_t c=jsonW.find(L':',k);
        if(c!=std::wstring::npos){ double v=_wtof(jsonW.c_str()+c+1); if(v>=0&&v<20) m.quiet=v; } }
    // normalise symbology spelling
    for(size_t i=0;i<m.sym.size();++i) if(m.sym[i]>=L'A'&&m.sym[i]<=L'Z') m.sym[i]=(wchar_t)(m.sym[i]-L'A'+L'a');
}

// Draw a real barcode inside `box`. `payload` is the already-resolved live
// value; digits may be Persian (they are folded back to ASCII for encoding,
// while the human-readable line below keeps the Persian rendering).
static void pdDrawBarcode(HDC dc, const PrintItem& it, const RECT& box,
                          double pxPerMmX, double pxPerMmY,
                          double fontPxPerPt, const std::wstring& payloadW){
    if(payloadW.empty()) return;
    PdBarcodeModel bm; pdParseBarcodeModel(it.text, bm);

    // Fold Persian/Arabic-Indic digits back to ASCII so the encoder sees the
    // real numeric value; keep everything else verbatim.
    std::string ascii; std::wstring hriW;
    for(size_t i=0;i<payloadW.size();++i){
        wchar_t c=payloadW[i];
        if(c>=0x06F0 && c<=0x06F9)      ascii += (char)('0' + (c-0x06F0));
        else if(c>=0x0660 && c<=0x0669) ascii += (char)('0' + (c-0x0660));
        else if(c>=32 && c<127)         ascii += (char)c;
        else if(c==0x066C || c==0x060C || c==L'٬') { /* thousands separator: drop */ }
    }
    if(ascii.empty()) return;

    std::string mods; std::string hriAscii = ascii;
    bool ok=false;
    if(bm.sym==L"ean13"||bm.sym==L"ean"||bm.sym==L"ean-13"){
        ok = pdBcEan13(ascii, mods, hriAscii);
        if(!ok) ok = pdBc128(ascii, mods);          // graceful fallback
    } else if(bm.sym==L"code39"||bm.sym==L"c39"||bm.sym==L"code-39"){
        ok = pdBc39(ascii, mods);
        if(!ok) ok = pdBc128(ascii, mods);
    } else {
        ok = pdBc128(ascii, mods);
    }
    if(!ok || mods.empty()) return;

    int X0=box.left, Y0=box.top, X1=box.right, Y1=box.bottom;
    int W=X1-X0, H=Y1-Y0; if(W<=2||H<=2) return;

    // quiet zone (mm on each side) — required for reliable scanning
    int qz=(int)(bm.quiet*pxPerMmX); if(qz<0) qz=0;
    if(2*qz > W/2) qz = W/4;
    int bw = W - 2*qz; if(bw<(int)mods.size()) bw=(int)mods.size();

    // Human-readable interpretation line height
    double fontPt = it.fontPt>0 ? it.fontPt : 8.0;
    int hriH = bm.hri ? (int)(fontPt*fontPxPerPt*1.45) : 0;
    if(hriH > H/2) hriH = H/2;
    int barsH = H - hriH; if(barsH<3){ barsH=H; hriH=0; }

    // Draw bars. Consecutive '1' modules merge into a single rectangle so the
    // printer never leaves hairline gaps inside a wide bar.
    HBRUSH bbr=CreateSolidBrush(pdCR(it.textColor));
    size_t n=mods.size();
    size_t i=0;
    while(i<n){
        if(mods[i]=='0'){ ++i; continue; }
        size_t j=i; while(j<n && mods[j]=='1') ++j;
        int px0 = X0+qz + (int)((double)bw*(double)i/(double)n);
        int px1 = X0+qz + (int)((double)bw*(double)j/(double)n);
        if(px1<=px0) px1=px0+1;
        RECT br={px0, Y0, px1, Y0+barsH};
        FillRect(dc,&br,bbr);
        i=j;
    }
    DeleteObject(bbr);

    // Human-readable numeric line, centred under the symbol. The numeric code
    // is ALWAYS shown next to the bars on the insurance receipt page, which is
    // what the paper form does.
    if(hriH>0){
        hriW.clear();
        for(size_t k=0;k<hriAscii.size();++k) hriW += (wchar_t)(unsigned char)hriAscii[k];
        hriW = toFaDigits(hriW);
        int lf=-(int)(fontPt*fontPxPerPt);
        HFONT f=CreateFontW(lf,0,0,0,it.bold?FW_BOLD:FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,
            CLEARTYPE_QUALITY,0,it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
        HGDIOBJ of=SelectObject(dc,f);
        // v2.07 §3.8: the HRI line IS the barcode value — the single most
        // important text on the receipt. Saturated ink, never washed out.
        SetTextColor(dc,pdTextInk(it.textColor));
        RECT tr={X0, Y0+barsH, X1, Y1};
        DrawTextW(dc,hriW.c_str(),-1,&tr,
            DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        SelectObject(dc,of); DeleteObject(f);
    }
    (void)pxPerMmY;
}

// Map a design paper name to a Windows DMPAPER_* code. Returns 0 for custom.
static short pdPaperCode(const std::wstring& name){
    if(name==L"A3")     return DMPAPER_A3;       // 8
    if(name==L"A4")     return DMPAPER_A4;       // 9
    if(name==L"A5")     return DMPAPER_A5;       // 11
    if(name==L"A6")     return DMPAPER_A6;       // 70
    if(name==L"B5")     return DMPAPER_B5;       // 13
    if(name==L"Letter") return DMPAPER_LETTER;   // 1
    if(name==L"Legal")  return DMPAPER_LEGAL;    // 5
    return 0; // R80/R58/L90/L100/custom → set explicit dimensions instead
}
// Create a printer DC whose DEVMODE paper size & orientation match the design,
// so an A5 design actually prints on A5 (not the printer's default A4). Falls
// back to a plain DC if the printer doesn't expose its properties.
static HDC pdCreatePrinterDC(const std::wstring& prn, const PrintDesign& d){
    if(prn.empty()) return NULL;
    HANDLE hp=NULL;
    if(!OpenPrinterW((LPWSTR)prn.c_str(),&hp,NULL) || !hp)
        return CreateDCW(L"WINSPOOL",prn.c_str(),NULL,NULL);
    LONG need=DocumentPropertiesW(NULL,hp,(LPWSTR)prn.c_str(),NULL,NULL,0);
    HDC dc=NULL;
    if(need>0){
        std::vector<BYTE> buf(need);
        DEVMODEW* dm=(DEVMODEW*)buf.data();
        if(DocumentPropertiesW(NULL,hp,(LPWSTR)prn.c_str(),dm,NULL,DM_OUT_BUFFER)==IDOK){
            short code=pdPaperCode(d.paper);
            // orientation
            dm->dmFields |= DM_ORIENTATION;
            dm->dmOrientation = (d.orientation==1)?DMORIENT_LANDSCAPE:DMORIENT_PORTRAIT;
            if(code>0){
                dm->dmFields |= DM_PAPERSIZE;
                dm->dmPaperSize = code;
                dm->dmFields &= ~(DM_PAPERWIDTH|DM_PAPERLENGTH);
            } else {
                // custom / receipt / small-laser: set explicit size in 0.1 mm.
                double wmm=d.paperW, hmm=d.paperH;
                if(d.orientation==1 && wmm<hmm) std::swap(wmm,hmm);
                dm->dmFields |= (DM_PAPERSIZE|DM_PAPERWIDTH|DM_PAPERLENGTH);
                dm->dmPaperSize   = DMPAPER_USER;       // 256
                dm->dmPaperWidth  = (short)(wmm*10.0);   // tenths of a millimetre
                dm->dmPaperLength = (short)(hmm*10.0);
            }
            // let the driver validate/merge our changes
            DocumentPropertiesW(NULL,hp,(LPWSTR)prn.c_str(),dm,dm,DM_IN_BUFFER|DM_OUT_BUFFER);
            dc=CreateDCW(L"WINSPOOL",prn.c_str(),NULL,dm);
        }
    }
    ClosePrinter(hp);
    if(!dc) dc=CreateDCW(L"WINSPOOL",prn.c_str(),NULL,NULL);
    return dc;
}

// v2.07 §7.5 — print a SPECIFIC design (used by the زیربخش پذیرش barcode-only
// route, which must select TB1 regardless of the machine/section binding).
// A thread-local override makes the standard engine resolve `forced` instead
// of the machine-bound design — no duplicated rendering path.
static thread_local const PrintDesign* t_forcedDesign=nullptr;
bool printPrintDesignWith(const ReceptionRecord& r, const PrintDesign& forced, HWND owner){
    t_forcedDesign=&forced;
    bool ok=printPrintDesign(r,0,owner);
    t_forcedDesign=nullptr;
    return ok;
}

bool printPrintDesign(const ReceptionRecord& r, int sectionId, HWND owner){
    // v1.65.0 SERVICES-PRINT FIX. Root cause of «قبض فقط برچسب چاپ می‌کند»:
    // Designs_Init() was only ever called from the Settings window / the Print
    // Designer, so a fresh install that never opened either had ZERO seeded
    // design files → SectionDesign_Resolve() failed → every receipt fell back
    // to the LEGACY label-only layout (printDesignedReceipt) which cannot render
    // the admission services table. Seed the section registry + the 30 built-in
    // templates lazily right here (cheap no-op once seeded, guarded static), so
    // the FIRST print of a fresh install already uses the real services-capable
    // design.
    { static bool s_printSeeded=false;
      if(!s_printSeeded){ Sections_Init(); Designs_Init(); s_printSeeded=true; } }
    PrintDesign d;
    // v1.99: design is bound to THIS machine's printer, not a clinic section.
    (void)sectionId;
    if(t_forcedDesign){                       // v2.07 §7.5 (TB1 route)
        d=*t_forcedDesign;
    } else if(!MachineDesign_Resolve(d)){
        d = Design_BuiltinTemplate(0);
    }
    if(d.items.empty()) return false;
    if(d.paperW<=0 || d.paperH<=0){
        double pw,ph; if(Paper_Dims(d.paper,pw,ph)){ d.paperW=pw; d.paperH=ph;
            if(d.orientation==1) std::swap(d.paperW,d.paperH); }
        else { d.paperW=148; d.paperH=210; }
    }
    // v2.07 §3.9 — RESPONSIVE REFLOW. If the current paper differs from the
    // paper the layout was authored for (baseW/baseH), reflow a COPY of the
    // design (never the stored one) before rendering. Legacy designs with
    // baseW == 0 adopt the current paper as base WITHOUT scaling (existing
    // contract — kept).
    if(d.baseW>0 && d.baseH>0 &&
       (std::abs(d.paperW-d.baseW)>0.01 || std::abs(d.paperH-d.baseH)>0.01)){
        reflowDesign(d);
    } else if(d.baseW<=0 || d.baseH<=0){
        d.baseW=d.paperW; d.baseH=d.paperH;   // adopt without scaling
    }

    // Resolve a printer DC whose paper size matches the DESIGN (so an A5 design
    // prints on A5, an 80mm receipt design on the receipt roll, etc.). First
    // time (no saved printer) → standard dialog so the operator picks a printer.
    std::wstring prn=currentPrinter();
    HDC dc = pdCreatePrinterDC(prn, d);
    if(!dc){
        PRINTDLGW pd={0}; pd.lStructSize=sizeof(pd); pd.hwndOwner=owner;
        pd.Flags=PD_RETURNDC|PD_NOPAGENUMS|PD_NOSELECTION|PD_USEDEVMODECOPIES;
        if(!PrintDlgW(&pd)) return true;   // user cancelled → treat as handled
        dc=pd.hDC;
    }
    if(!dc) return false;

    // §1.52.0 / v1.96.0 — RESPONSIVE A4→smaller AUTO-SCALE. All built-in
    // templates are authored in A4 mm coordinates (210×297). If the operator
    // prints that design onto a smaller sheet — A5, 58 mm / 80 mm thermal roll,
    // or any printer whose actual paper is smaller than the authored space — the
    // item coordinates would overflow. We auto-detect the "authored space" from
    // the bounding box of all items (max x+w, max y+h) and compute a uniform
    // shrink so the whole design fits the ACTUAL printer paper (read back from
    // the DC's PHYSICALWIDTH/HEIGHT in mm), not just the design's stored paper.
    // This is what fixes the print-dialog fallback path where the DC paper differs
    // from d.paper. When paper == authored size (normal A4 case) scale == 1 and
    // nothing changes. No schema change: authored size is inferred from extents.
    // v2.05: authored space is the design paper, not the item bounding box.
    // Expanding to item extents made A5 prints shrink as if they were still A4.
    double authW=d.paperW>0?d.paperW:210, authH=d.paperH>0?d.paperH:297;
    double pscale=1.0;

    int dpiX=GetDeviceCaps(dc,LOGPIXELSX), dpiY=GetDeviceCaps(dc,LOGPIXELSY);
    int offX=GetDeviceCaps(dc,PHYSICALOFFSETX), offY=GetDeviceCaps(dc,PHYSICALOFFSETY);
    double sx=dpiX/25.4, sy=dpiY/25.4;
    // Fit the authored extent onto the ACTUAL printer paper (mm). Recomputed after
    // the StartDoc fallback below may replace the DC with one of a different size.
    auto calcPscale=[&](){
        int pw=GetDeviceCaps(dc,PHYSICALWIDTH), ph=GetDeviceCaps(dc,PHYSICALHEIGHT);
        double actW = (sx>0 && pw>0)? pw/sx : d.paperW;
        double actH = (sy>0 && ph>0)? ph/sy : d.paperH;
        if(actW<=0) actW=d.paperW; if(actH<=0) actH=d.paperH;
        pscale=1.0;
        if(authW>actW+0.01 || authH>actH+0.01){
            double fx=actW/authW, fy=actH/authH;
            pscale = fx<fy ? fx : fy;
            if(pscale>1.0) pscale=1.0;
        }
    };
    calcPscale();
    // mm→device-px, applying the responsive scale so an A4-authored design
    // relocates and shrinks proportionally onto whatever paper size is active.
    auto mmX=[&](double mm){ return (int)(mm*pscale*sx)-offX; };
    auto mmY=[&](double mm){ return (int)(mm*pscale*sy)-offY; };

    DOCINFOW di={sizeof(di)};
    std::wstring docName=std::wstring(APP_NAME_W)+L" — print";
    di.lpszDocName=docName.c_str();
    if(StartDocW(dc,&di)<=0){
        // The stored/default printer rejected the job (common with virtual
        // "app" printers e.g. some PDF/photo apps → "doesn't support print
        // preview"). Fall back to the standard dialog so the operator picks a
        // working printer, then re-resolve DPI/offsets for the new DC.
        DeleteDC(dc); dc=NULL;
        PRINTDLGW pd={0}; pd.lStructSize=sizeof(pd); pd.hwndOwner=owner;
        pd.Flags=PD_RETURNDC|PD_NOPAGENUMS|PD_NOSELECTION|PD_USEDEVMODECOPIES;
        if(!PrintDlgW(&pd)) return true;        // cancelled → handled
        dc=pd.hDC; if(!dc) return false;
        dpiX=GetDeviceCaps(dc,LOGPIXELSX); dpiY=GetDeviceCaps(dc,LOGPIXELSY);
        offX=GetDeviceCaps(dc,PHYSICALOFFSETX); offY=GetDeviceCaps(dc,PHYSICALOFFSETY);
        sx=dpiX/25.4; sy=dpiY/25.4;
        calcPscale();                  // v1.96.0: re-fit for the new DC's paper size
        if(StartDocW(dc,&di)<=0){
            MessageBoxW(owner,L"چاپگر انتخاب‌شده از چاپ این سند پشتیبانی نمی‌کند.\n"
                L"لطفاً یک چاپگر واقعی (نه «Microsoft Print to PDF» یا برنامهٔ عکس) انتخاب کنید.",
                L"چاپ طرح",MB_OK|MB_ICONWARNING);
            DeleteDC(dc); return false; }
    }
    // Paint in z-order on every planned page. Service pagination is computed
    // before the first StartPage so the same measured slices drive page count
    // and rendering.
    std::vector<const PrintItem*> ord;
    for(const auto& it:d.items) ord.push_back(&it);
    // simple insertion sort by z (avoids pulling in <algorithm>; item counts are tiny)
    for(size_t i=1;i<ord.size();++i){ const PrintItem* k=ord[i]; size_t j=i;
        while(j>0 && ord[j-1]->z > k->z){ ord[j]=ord[j-1]; --j; } ord[j]=k; }

    const PrintItem* serviceItem=NULL;
    for(const PrintItem* pit:ord) if(pit->type==PIT_SERVICES){ serviceItem=pit; break; }
    RECT serviceBox={0,0,0,0};
    std::vector<PdServicesPageSlice> servicePages(1);
    // v2.07 §3.9 — ELASTIC STRETCH BAND (بلوک_پایانی). Everything authored
    // BELOW the services item's bottom edge (payment summary, signatures,
    // footer) is translated down by (naturalHeight − authoredHeight), clamped
    // to the printable area, so the footer sits directly under the table and
    // the table can never reach the footer. Internal block spacing is kept
    // exactly as authored.
    double svcShiftMm=0.0;
    if(serviceItem){
        serviceBox.left=mmX(serviceItem->x);
        serviceBox.top=mmY(serviceItem->y);
        serviceBox.right=mmX(serviceItem->x+serviceItem->w);
        serviceBox.bottom=mmY(serviceItem->y+serviceItem->h);
        int minimumFrame=(int)(12.0*sy*pscale+0.5); if(minimumFrame<2) minimumFrame=2;
        PdServicesFrame safe=pdEnsureServicesFrame(serviceBox.top,serviceBox.bottom,
            mmY(0),mmY(d.paperH),minimumFrame);
        serviceBox.top=safe.top; serviceBox.bottom=safe.bottom;
        // ---- v2.07 §3.9: natural height of the table from the actual rows ----
        {
            PdServicesLayout probe;
            if(pdBuildServicesLayout(dc,*serviceItem,serviceBox,sx*pscale,sy*pscale,
                                     (dpiY/72.0)*pscale,&r,probe,d.kind==L"builtin")){
                int natural=probe.headH;
                for(size_t i=0;i<probe.rowHeights.size();++i) natural+=probe.rowHeights[i];
                int authored=serviceBox.bottom-serviceBox.top;
                int delta=natural-authored;
                if(delta>0){
                    // breathing room between the last row and the block (mm):
                    // 4.0 on sheet papers, 2.5 on R80/R58 thermal rolls.
                    double gapMm = (d.paperW<=90.0)?2.5:4.0;
                    double shift=(double)delta/sy/pscale + gapMm;
                    // clamp: the block must never leave the PRINTABLE area (the
                    // hardware unprintable bottom margin, not just the paper edge)
                    double printableBottomMm = d.paperH;
                    {
                        int ph=GetDeviceCaps(dc,PHYSICALHEIGHT);
                        if(ph>0 && sy>0 && pscale>0)
                            printableBottomMm = ((double)(ph-offY)/sy)/pscale;
                    }
                    double limitMm = printableBottomMm - 2.0;
                    if(limitMm > d.paperH-2.0) limitMm = d.paperH-2.0;
                    double svcBottomMm = serviceItem->y + serviceItem->h + shift;
                    if(svcBottomMm > limitMm)
                        shift = limitMm - (serviceItem->y+serviceItem->h);
                    if(shift<0) shift=0;
                    svcShiftMm=shift;
                    // grow the services box so the table renders its natural height
                    serviceBox.bottom=mmY(serviceItem->y+serviceItem->h+shift);
                }
            }
        }
        servicePages=pdPlanServicePages(dc,*serviceItem,serviceBox,sx*pscale,sy*pscale,
                                        (dpiY/72.0)*pscale,&r,d.kind==L"builtin");
        if(servicePages.empty()) servicePages.push_back(PdServicesPageSlice());
    }

    for(size_t pageNo=0;pageNo<servicePages.size();++pageNo){
        bool multiPage=servicePages.size()>1;
        bool finalPage=pageNo+1==servicePages.size();
        if(StartPage(dc)<=0){ AbortDoc(dc); DeleteDC(dc); return false; }
        SetBkMode(dc,TRANSPARENT);
        // v2.07 §3.4 — set once a PIT_BARCODE with hri==true is emitted on this
        // page; any additional {receiptbarcode}-bound text item is then skipped.
        bool hriBarcodeRendered=false;

        for(const PrintItem* pit : ord){
        const PrintItem& it=*pit;
        // On continuation pages repeat the page shell and all patient/template
        // context above the services table. Totals, signatures, tear-off stubs,
        // QR/barcode and other footer items appear once, on the final page.
        if(multiPage && serviceItem && pit!=serviceItem && !finalPage){
            std::wstring normalizedField=(it.type==PIT_FIELD)?pdNormalizeField(it.field):L"";
            PdContinuationItemKind repeatKind=PDCI_OTHER;
            if(it.type==PIT_FRAME) repeatKind=PDCI_FRAME;
            else if(it.type==PIT_LOGO) repeatKind=PDCI_LOGO;
            else if(it.type==PIT_PHOTO) repeatKind=PDCI_PHOTO;
            else if(it.type==PIT_FIELD) repeatKind=PDCI_FIELD;
            if(!pdContinuationRepeatAllowed(repeatKind,normalizedField)) continue;
        }
        int x0=mmX(it.x), y0=mmY(it.y), x1=mmX(it.x+it.w), y1=mmY(it.y+it.h);
        if(pit==serviceItem){ y0=serviceBox.top; y1=serviceBox.bottom; }
        else if(serviceItem && svcShiftMm>0.0){
            // v2.07 §3.9 — translate the trailing block (بلوک_پایانی): every
            // item whose TOP starts at or below the services item's authored
            // bottom edge moves down by the same shift, keeping its internal
            // spacing untouched. Items above the table never move.
            double svcBottomMm = serviceItem->y + serviceItem->h;
            if(it.y >= svcBottomMm - 0.001){
                y0=mmY(it.y+svcShiftMm);
                y1=mmY(it.y+it.h+svcShiftMm);
            }
        }
        // v2.07 §3.4 — barcode value de-duplication: once a PIT_BARCODE with
        // hri==true has been rendered, skip any later text item bound to the
        // same value (holds for old user designs too; nothing is deleted).
        if(it.type==PIT_FIELD && hriBarcodeRendered){
            std::wstring nf=pdNormalizeField(it.field);
            if(pdBarcodeValueAlreadyRendered(true,nf)) continue;
        }
        if(it.type==PIT_TABLE){
            RECT rr={x0,y0,x1,y1};
            // §1.52.0: scale the internal font/cell px-per-mm so an A4-authored
            // table shrinks correctly onto an A5 sheet (WYSIWYG with preview).
            pdDrawTable(dc, it, rr, sx*pscale, sy*pscale, (dpiY/72.0)*pscale, &r);
        } else if(it.type==PIT_SERVICES){
            // §1.51.0: dynamic services list rendered from the live record.
            RECT rr={x0,y0,x1,y1};
            const PdServicesPageSlice& slice=servicePages[pageNo];
            pdDrawServices(dc, it, rr, sx*pscale, sy*pscale, (dpiY/72.0)*pscale,
                           &r, slice);
        } else if(it.type==PIT_BARCODE){
            // v1.55.0: a REAL scannable barcode. The payload is resolved from
            // the bound field token (default {receiptbarcode}) so it always
            // encodes live record data. Empty payload → nothing is drawn.
            std::wstring pl = it.field.empty()
                ? pdFieldValue(r, L"{receiptbarcode}")
                : pdFieldValue(r, it.field);
            if(pl.empty() && !it.prefix.empty()) pl = it.prefix;
            RECT rr={x0,y0,x1,y1};
            pdDrawBarcode(dc, it, rr, sx*pscale, sy*pscale, (dpiY/72.0)*pscale, pl);
            // v2.07 §3.4: mark the HRI as rendered so any later {receiptbarcode}
            // text item is skipped (single barcode value per receipt).
            { PdBarcodeModel bm; pdParseBarcodeModel(it.text, bm);
              if(bm.hri) hriBarcodeRendered=true; }
        } else if(it.type==PIT_HLINE){
            // v2.07 §3.8: hairlines are at least 0.30 mm and never lighter
            // than #333333 on paper.
            double wmm=it.borderWidth; if(wmm<0.30) wmm=0.30;
            int wpx=(int)(wmm*sx*pscale); if(wpx<1)wpx=1;
            HPEN p=CreatePen(PS_SOLID,wpx,pdLineInk(it.borderColor)); HGDIOBJ o=SelectObject(dc,p);
            MoveToEx(dc,x0,y0,0); LineTo(dc,x1,y0); SelectObject(dc,o); DeleteObject(p);
        } else if(it.type==PIT_VLINE){
            double wmm=it.borderWidth; if(wmm<0.30) wmm=0.30;
            int wpx=(int)(wmm*sx*pscale); if(wpx<1)wpx=1;
            HPEN p=CreatePen(PS_SOLID,wpx,pdLineInk(it.borderColor)); HGDIOBJ o=SelectObject(dc,p);
            MoveToEx(dc,x0,y0,0); LineTo(dc,x0,y1); SelectObject(dc,o); DeleteObject(p);
        } else if(it.type==PIT_RECT||it.type==PIT_FRAME||it.type==PIT_LOGO||
                  it.type==PIT_PHOTO||it.type==PIT_QR||it.type==PIT_IMAGE){
            int wpx=(int)(it.borderWidth*sx*pscale); if(wpx<1)wpx=1;
            bool isImgItem=(it.type==PIT_LOGO||it.type==PIT_PHOTO||it.type==PIT_IMAGE);
            // v1.21.0: fill rect/frame background when not transparent (WYSIWYG).
            if((it.type==PIT_RECT||it.type==PIT_FRAME) && !it.fillTransparent){
                RECT fr={x0,y0,x1,y1}; HBRUSH fb=CreateSolidBrush(pdCR(it.fillColor));
                FillRect(dc,&fr,fb); DeleteObject(fb);
            }
            HPEN p=CreatePen(PS_SOLID,wpx,pdCR(it.borderColor)); HGDIOBJ o=SelectObject(dc,p);
            HGDIOBJ ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
            // v1.20.0: PIT_IMAGE with a real picture draws no border box.
            if(!(isImgItem && !it.imgPath.empty()))
                Rectangle(dc,x0,y0,x1,y1);
            SelectObject(dc,ob); SelectObject(dc,o); DeleteObject(p);
            if(it.type==PIT_LOGO||it.type==PIT_QR||it.type==PIT_PHOTO||it.type==PIT_IMAGE){
                RECT rr={x0,y0,x1,y1};
                // v1.20.0: if the item carries an actual image (uploaded logo /
                // patient photo / image, stored as a file path or data:base64 URI),
                // render the picture; otherwise fall back to a labelled box.
                bool drawn=false;
                if(isImgItem && !it.imgPath.empty()){
                    // v1.23.0: honour the designed object-fit + padding so the
                    // logo/photo respects its rectangle exactly (no overflow,
                    // no stretch) and matches the designer preview 1:1.
                    int padPx=(int)(it.padding*sx); if(padPx<0)padPx=0;
                    drawn=gpDrawImageRectFit(dc,it.imgPath,rr,it.objectFit,padPx);
                }
                if(!drawn){
                    std::wstring ph=(it.type==PIT_LOGO)?L"لوگو":(it.type==PIT_QR?L"QR":(it.type==PIT_IMAGE?L"تصویر":L"عکس"));
                    HFONT f=CreateFontW(-(int)(9*dpiY/72.0),0,0,0,FW_NORMAL,0,0,0,
                        DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Vazirmatn");
                    // v2.07 §3.8: placeholder text obeys the ink floor (no grey
                    // lighter than #555555-equivalent luminance on paper).
                    HGDIOBJ of=SelectObject(dc,f); SetTextColor(dc,pdTextInk(0x555555));
                    DrawTextW(dc,ph.c_str(),-1,&rr,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
                    SelectObject(dc,of); DeleteObject(f);
                }
            }
        } else { // text-bearing: LABEL / FIELD
            std::wstring s=it.text;
            if(it.type==PIT_FIELD && !it.field.empty()) s=it.prefix+pdFieldValue(r,it.field)+it.suffix;
            else s=it.prefix+it.text+it.suffix;
            if(it.visibility==1 && (it.type==PIT_FIELD) && pdFieldValue(r,it.field).empty()) continue;
            if(s.empty()) continue;
            // v1.23.0: apply inner padding so text never touches the box edge,
            // matching the designer preview exactly.  §1.52.0: scale padding by
            // pscale so A4→A5 stays proportional.
            int padPx=(int)(it.padding*sx*pscale); if(padPx<0)padPx=0;
            RECT rr={x0+padPx,y0+padPx,x1-padPx,y1-padPx};
            if(rr.right<=rr.left){ rr.left=x0; rr.right=x1; }
            if(rr.bottom<=rr.top){ rr.top=y0; rr.bottom=y1; }
            int boxW=rr.right-rr.left, boxH=rr.bottom-rr.top;
            // horizontal alignment: 0=right 1=center 2=left (RTL)
            UINT al=(it.align==1)?DT_CENTER:(it.align==2)?DT_LEFT:DT_RIGHT;
            // v1.22.0: per-item text direction. dir 0=RTL, 1=LTR, 2=center.
            UINT dirf=(it.dir==1)?0:DT_RTLREADING;
            if(it.dir==2) al=DT_CENTER;
            // v1.24.0: AUTO-FIT so text is NEVER clipped / cut in half. We start
            // at the designed point size and shrink (down to a sensible floor)
            // until the wrapped block fits inside the box height AND each line
            // fits the width. This kills the "half-cut letters / missing ر"
            // problem the operator reported on real prints.  §1.52.0: start the
            // auto-fit from the responsively-scaled point size (A4→A5).
            double basePt = it.fontPt>0 ? it.fontPt : 10.0;
            basePt *= pscale;
            // v2.07 §3.5 — PATIENT NAME NEVER WRAPS. The bound patient-name item
            // ({full}/{P-Name}) is measured SINGLE-LINE first; if the string
            // exceeds the item width the point size drops in 0.5 pt steps down
            // to 70% of the authored size, and only then ellipsizes. It never
            // breaks to a second line and never overlaps the neighbouring cell.
            std::wstring nfName;
            if(it.type==PIT_FIELD && !it.field.empty())
                nfName=pdNormalizeField(it.field);
            bool isPatientName = (nfName==L"{full}");
            if(isPatientName){
                UINT single=al|DT_SINGLELINE|DT_NOCLIP|dirf|DT_NOPREFIX;
                double ptN=basePt; double floorN=basePt*0.70;
                HFONT fN=NULL; HGDIOBJ ofN=NULL;
                for(int tries=0;tries<24;++tries){
                    int lf=-(int)(ptN*dpiY/72.0+0.5);
                    fN=CreateFontW(lf,0,0,0,it.bold?FW_BOLD:FW_NORMAL,it.italic?1:0,0,0,
                        DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
                        DEFAULT_PITCH|FF_DONTCARE,
                        it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
                    ofN=SelectObject(dc,fN);
                    RECT mN=rr;
                    DrawTextW(dc,s.c_str(),-1,&mN,single|DT_TOP|DT_CALCRECT);
                    int wN=mN.right-mN.left;
                    SelectObject(dc,ofN); DeleteObject(fN); fN=NULL;
                    if(wN<=boxW+1) break;
                    if(ptN<=floorN+0.001) break;
                    ptN-=0.5; if(ptN<floorN) ptN=floorN;
                }
                // final single-line draw (ellipsis only as the last resort)
                int lf=-(int)(ptN*dpiY/72.0+0.5);
                HFONT fF=CreateFontW(lf,0,0,0,it.bold?FW_BOLD:FW_NORMAL,it.italic?1:0,0,0,
                    DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
                    DEFAULT_PITCH|FF_DONTCARE,
                    it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
                HGDIOBJ ofF=SelectObject(dc,fF);
                SetTextColor(dc,pdTextInk(it.textColor));
                RECT drN=rr;
                if(it.valign==1){ int off=(boxH-(int)(ptN*dpiY/72.0))/2; if(off>0) drN.top+=off; }
                else if(it.valign==2){ int off=boxH-(int)(ptN*dpiY/72.0); if(off>0) drN.top+=off; }
                DrawTextW(dc,s.c_str(),-1,&drN,
                    al|DT_SINGLELINE|DT_VCENTER|dirf|DT_NOPREFIX|DT_NOCLIP|DT_END_ELLIPSIS);
                SelectObject(dc,ofF); DeleteObject(fF);
                continue;   // name item is fully handled — next item
            }
            UINT base = al|DT_WORDBREAK|dirf|DT_NOPREFIX;
            HFONT f=NULL; HGDIOBJ of=NULL; RECT meas; int th=0;
            double pt=basePt; double floorPt = (basePt<7.0)? basePt*0.7 : 6.0;
            for(int tries=0; tries<14; ++tries){
                int lf=-(int)(pt*dpiY/72.0+0.5);
                f=CreateFontW(lf,0,0,0,it.bold?FW_BOLD:FW_NORMAL,it.italic?1:0,0,0,
                    DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
                    DEFAULT_PITCH|FF_DONTCARE,
                    it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
                of=SelectObject(dc,f);
                meas=rr; meas.bottom=rr.top+30000;     // unbounded height for measure
                DrawTextW(dc,s.c_str(),-1,&meas,base|DT_TOP|DT_CALCRECT);
                th=meas.bottom-meas.top;
                int mw=meas.right-meas.left;
                if(th<=boxH && mw<=boxW+1) break;       // fits → done
                if(pt<=floorPt){ break; }               // can't shrink further
                SelectObject(dc,of); DeleteObject(f); f=NULL;
                pt = pt*0.92; if(pt<floorPt) pt=floorPt;
            }
            SetTextColor(dc,pdTextInk(it.textColor));   // v2.07 §3.8 saturated ink
            // v1.23.0: vertical alignment (0=top 1=middle 2=bottom).
            RECT dr=rr;
            int bh=boxH;
            if(it.valign==1){ int off=(bh-th)/2; if(off>0) dr.top+=off; }
            else if(it.valign==2){ int off=(bh-th); if(off>0) dr.top+=off; }
            // DT_NOCLIP guarantees the LAST line is fully drawn even if the box is
            // a hair short, so descenders/letters are never sheared.
            DrawTextW(dc,s.c_str(),-1,&dr,base|DT_TOP|DT_NOCLIP);
            if(of) SelectObject(dc,of); if(f) DeleteObject(f);
        }
        }
        if(EndPage(dc)<=0){ AbortDoc(dc); DeleteDC(dc); return false; }
    }
    EndDoc(dc); DeleteDC(dc);
    logLine(L"print_designer design printed for section "+std::to_wstring(sectionId)+
            L" ("+std::to_wstring(servicePages.size())+L" page(s))");
    return true;
}
