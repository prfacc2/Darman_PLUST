// ============================================================================
//  billing.cpp — Iranian insurance definitions, reception persistence,
//                last receipt, printing (real GDI printer output)
// ============================================================================
#include "app.h"
#include "sections.h"
#include <stdio.h>

// ----------------------------------------------------- Iranian insurances --
//  pct = سهم سازمان بیمه‌گر (درصد پوشش پایه برای خدمات سرپایی دولتی)
const InsuranceDef INSURANCES[] = {
    { L"آزاد (بدون بیمه)",          0  },
    { L"تأمین اجتماعی",             70 },
    { L"بیمه سلامت ایرانیان",        70 },
    { L"بیمه سلامت روستایی",         90 },
    { L"بیمه سلامت کارکنان دولت",    70 },
    { L"نیروهای مسلح",              90 },
    { L"کمیته امداد امام خمینی",     100},
};
const int N_INSURANCES = sizeof(INSURANCES)/sizeof(INSURANCES[0]);

const InsuranceDef SUPP_INSURANCES[] = {
    { L"ندارد",            0  },
    { L"بیمه ایران",        70 },
    { L"بیمه آسیا",         70 },
    { L"بیمه دانا",         70 },
    { L"بیمه البرز",        70 },
    { L"بیمه پاسارگاد",     80 },
    { L"بیمه ملت",          70 },
    { L"بیمه کوثر",         80 },
    { L"بیمه دی",           90 },
    { L"بیمه SOS",          80 },
};
const int N_SUPP = sizeof(SUPP_INSURANCES)/sizeof(SUPP_INSURANCES[0]);

// ---------------------------------------------------------------------------
//  v1.55.0 — REAL coverage-percentage lookups used by the print designer's
//  {ins_percent} / {supp_percent} tokens. These read the SAME authoritative
//  tables the billing engine uses, so the printed percentage always equals the
//  percentage the bill was actually computed with. Out-of-range indices return
//  -1 (meaning «unknown») so the token prints an EMPTY string rather than a
//  misleading «۰٪». Nothing here is ever randomised or guessed.
// ---------------------------------------------------------------------------
int Ins_Percent(int idx){
    // v1.74: honour a user-defined سهم سازمان from the «تعریف بیمه» registry
    // when present, else fall back to the hardcoded table. Out-of-range → -1
    // so the {ins_percent} token prints empty rather than a fake 0٪.
    if(const InsDef* d=insDefByIndex(idx)){
        if(d->orgShare>=0) return d->orgShare;
    }
    if(idx<0 || idx>=N_INSURANCES) return -1;
    return INSURANCES[idx].pct;
}
int Supp_Percent(int idx){
    if(const SuppDef* d=suppDefByIndex(idx)){
        if(d->franchiseOrgPct>0) return d->franchiseOrgPct;
    }
    if(idx<0 || idx>=N_SUPP) return -1;
    return SUPP_INSURANCES[idx].pct;
}

// ------------------------------------------------------------- tariffs -----
//  Base service tariff per visit type (Rial). Editable; can later be loaded
//  from data\tariffs.ini for server-side configuration.
const long long VISIT_TARIFF[3] = {
    2'500'000,   // عادی   (ویزیت عمومی)
    3'500'000,   // سرپایی (خدمت سرپایی)
    8'000'000,   // بستری  (خدمت بستری پایه)
};
long long applyApptTariff(long long base, int apptType){
    switch(apptType){
        case 1: return base * 150 / 100;   // اورژانس: +۵۰٪
        case 2: return base *  50 / 100;   // پرسنلی: نصف تعرفه
        default:return base;               // عادی
    }
}
long long defaultServicePrice(int patientType, int apptType){
    int p = (patientType>=0 && patientType<3) ? patientType : 0;
    return applyApptTariff(VISIT_TARIFF[p], apptType);
}

// ------------------------------------------------------------ persistence --
static std::wstring recPath(){
    SYSTEMTIME st = iranNow();
    int jy,jm,jd; gregToJalali(st.wYear,st.wMonth,st.wDay,jy,jm,jd);
    wchar_t f[64]; swprintf(f,64,L"\\receptions_%04d-%02d-%02d.csv",jy,jm,jd);
    return dataDir()+f;
}
int countTodayReceptions(){
    std::wstring all = readFileUtf8(recPath());
    int n=0;
    for(wchar_t c : all) if(c==L'\n') n++;
    return n>0 ? n-1 : 0;   // minus header
}
int countTodayDoctorReceptions(const std::wstring& doctor, bool paidOnly){
    std::wstring want=trim(doctor);
    if(want.empty()) return 0;
    std::wstring all=readFileUtf8(recPath());
    int count=0; size_t pos=0; bool first=true;
    while(pos<all.size()){
        size_t e=all.find(L'\n',pos); if(e==std::wstring::npos) e=all.size();
        std::wstring line=all.substr(pos,e-pos); pos=e+1;
        if(first){ first=false; continue; }
        while(!line.empty()&&line.back()==L'\r') line.pop_back();
        if(line.empty()) continue;
        std::vector<std::wstring> f; std::wstring cur;
        for(wchar_t c:line){ if(c==L','){ f.push_back(cur); cur.clear(); } else cur+=c; }
        f.push_back(cur);
        // v1.56 appends treating doctor after the original 25 columns, preserving
        // every legacy financial index (paid remains field 24).
        if(f.size()<26 || trim(f[25])!=want) continue;
        if(paidOnly && _wtoi64(f[24].c_str())<=0) continue;
        count++;
    }
    return count;
}
static std::wstring csvEsc(const std::wstring& s){
    std::wstring o = s;
    for(auto& c : o) if(c==L',') c=L'،';
    return o;
}
int saveReception(ReceptionRecord& r){
    SYSTEMTIME st = iranNow();
    r.apptDate = jalaliDateShort(st);
    r.apptTime = iranTimeStr(st, true);
    bool isNew = GetFileAttributesW(recPath().c_str())==INVALID_FILE_ATTRIBUTES;
    r.queueNo = countTodayReceptions() + 1;
    std::wstring row;
    if(isNew)
        row += L"\uFEFFنوبت,نام,نام خانوادگی,کد ملی,نام پدر,تاریخ تولد,جنسیت,تلفن,ثابت,آدرس,"
               L"نوع بیمار,بیمه,بیمه مکمل,تاریخ,ساعت,شیفت,بخش,کاربر,"
               L"جمع کل,سهم بیمه,سهم بیمار,مابه‌التفاوت,سهم سازمان,تخفیف,پرداختی,پزشک معالج\r\n";
    wchar_t nums[256];
    swprintf(nums,256,L"%lld,%lld,%lld,%lld,%lld,%lld,%lld",
        r.total,r.mainShare,r.patientShare,r.baseDiff,r.orgShare,r.discount,r.paid);
    wchar_t qn[16]; swprintf(qn,16,L"%d",r.queueNo);
    row += std::wstring(qn)+L","+csvEsc(r.firstName)+L","+csvEsc(r.lastName)+L","
        + csvEsc(r.nationalId)+L","+csvEsc(r.fatherName)+L","+csvEsc(r.birthDate)+L","
        + csvEsc(r.gender)+L","+csvEsc(r.mobile)+L","+csvEsc(r.landline)+L","
        + csvEsc(r.address)+L","+csvEsc(r.patientType)+L","+csvEsc(r.insurance)+L","
        + csvEsc(r.suppInsurance)+L","+r.apptDate+L","+r.apptTime+L","
        + csvEsc(r.shift)+L","+csvEsc(r.dept)+L","+csvEsc(r.userName)+L","+nums+L","+
        csvEsc(r.treatingDoctor)+L"\r\n";
    writeFileUtf8(recPath(), row, true);
    logLine(L"reception saved #" + std::wstring(qn) + L" " + r.firstName + L" " + r.lastName);
    saveLastReceipt(r);
    return r.queueNo;
}

// -------------------------------------------------------- last receipt -----
static std::wstring lastPath(){ return dataDir()+L"\\last_receipt.dat"; }

// v1.61: keep the original 29 scalar lines byte-compatible, then append a
// versioned, escaped service block. Old installations can still read the scalar
// prefix; new builds recover every billed service for deterministic reprints.
static std::wstring lrEscape(const std::wstring& in){
    std::wstring out;
    for(wchar_t c:in){
        if(c==L'\\') out+=L"\\\\";
        else if(c==L'\t') out+=L"\\t";
        else if(c==L'\n') out+=L"\\n";
        else if(c==L'\r') out+=L"\\r";
        else out+=c;
    }
    return out;
}
static std::wstring lrUnescape(const std::wstring& in){
    std::wstring out;
    for(size_t i=0;i<in.size();++i){
        if(in[i]!=L'\\' || i+1>=in.size()){ out+=in[i]; continue; }
        wchar_t n=in[++i];
        if(n==L't') out+=L'\t';
        else if(n==L'n') out+=L'\n';
        else if(n==L'r') out+=L'\r';
        else out+=n;
    }
    return out;
}
static std::vector<std::wstring> lrSplitTabs(const std::wstring& line){
    std::vector<std::wstring> out; std::wstring cur; bool escaped=false;
    for(wchar_t c:line){
        if(c==L'\t' && !escaped){ out.push_back(cur); cur.clear(); continue; }
        cur+=c;
        if(escaped) escaped=false; else escaped=(c==L'\\');
    }
    out.push_back(cur); return out;
}
void saveLastReceipt(const ReceptionRecord& r){
    wchar_t nums[512];
    swprintf(nums,512,L"%lld\n%lld\n%lld\n%lld\n%lld\n%lld\n%lld\n%lld\n%d",
        r.total,r.mainShare,r.patientShare,r.baseDiff,r.orgShare,
        r.finalTotal,r.discount,r.paid,r.queueNo);
    std::wstring s = r.firstName+L"\n"+r.lastName+L"\n"+r.nationalId+L"\n"
        +r.fatherName+L"\n"+r.birthDate+L"\n"+r.gender+L"\n"+r.mobile+L"\n"
        +r.landline+L"\n"+r.address+L"\n"+r.patientType+L"\n"+r.insurance+L"\n"
        +r.suppInsurance+L"\n"+r.apptDate+L"\n"+r.apptTime+L"\n"+r.shift+L"\n"
        +r.dept+L"\n"+r.userName+L"\n"+nums+L"\n"+r.insuranceType+L"\n"+
        r.treatingDoctor+L"\n"+r.doctorCode+L"\nAZT_LAST_RECEIPT_V2\n";
    wchar_t count[32]; swprintf(count,32,L"%u",(unsigned)r.services.size());
    s+=count; s+=L"\n";
    for(const auto& line:r.services){
        wchar_t values[160];
        swprintf(values,160,L"%lld\t%d\t%lld\t%lld\t%lld",
                 line.price,line.qty,line.discount,line.insShare,line.patShare);
        s+=lrEscape(line.code)+L"\t"+lrEscape(line.name)+L"\t"+
           lrEscape(line.category)+L"\t"+lrEscape(line.desc)+L"\t"+values+L"\n";
    }
    writeFileUtf8(lastPath(), s, false);
}
bool loadLastReceipt(ReceptionRecord& r){
    std::wstring all = readFileUtf8(lastPath());
    if(all.empty()) return false;
    std::vector<std::wstring> f; size_t pos=0;
    while(pos <= all.size()){
        size_t e = all.find(L'\n',pos);
        std::wstring line=(e==std::wstring::npos)?all.substr(pos):all.substr(pos,e-pos);
        if(!line.empty() && line.back()==L'\r') line.pop_back();
        f.push_back(line);
        if(e==std::wstring::npos) break;
        pos=e+1;
    }
    if(f.size() < 26) return false;
    r.firstName=trim(f[0]); r.lastName=trim(f[1]); r.nationalId=trim(f[2]); r.fatherName=trim(f[3]);
    r.birthDate=trim(f[4]); r.gender=trim(f[5]); r.mobile=trim(f[6]); r.landline=trim(f[7]);
    r.address=trim(f[8]); r.patientType=trim(f[9]); r.insurance=trim(f[10]); r.suppInsurance=trim(f[11]);
    r.apptDate=trim(f[12]); r.apptTime=trim(f[13]); r.shift=trim(f[14]); r.dept=trim(f[15]); r.userName=trim(f[16]);
    r.total=_wtoi64(f[17].c_str()); r.mainShare=_wtoi64(f[18].c_str());
    r.patientShare=_wtoi64(f[19].c_str()); r.baseDiff=_wtoi64(f[20].c_str());
    r.orgShare=_wtoi64(f[21].c_str()); r.finalTotal=_wtoi64(f[22].c_str());
    r.discount=_wtoi64(f[23].c_str()); r.paid=_wtoi64(f[24].c_str());
    r.queueNo=_wtoi(f[25].c_str());
    if(f.size()>26) r.insuranceType=trim(f[26]);
    if(f.size()>27) r.treatingDoctor=trim(f[27]);
    if(f.size()>28) r.doctorCode=trim(f[28]);
    r.services.clear();
    if(f.size()>30 && trim(f[29])==L"AZT_LAST_RECEIPT_V2"){
        int count=_wtoi(f[30].c_str());
        if(count<0) count=0; if(count>1000) count=1000;
        for(int i=0;i<count && 31+(size_t)i<f.size();++i){
            auto cols=lrSplitTabs(f[31+i]);
            if(cols.size()<9) continue;
            ServiceLine line;
            line.code=lrUnescape(cols[0]); line.name=lrUnescape(cols[1]);
            line.category=lrUnescape(cols[2]); line.desc=lrUnescape(cols[3]);
            line.price=_wtoi64(cols[4].c_str()); line.qty=_wtoi(cols[5].c_str());
            line.discount=_wtoi64(cols[6].c_str()); line.insShare=_wtoi64(cols[7].c_str());
            line.patShare=_wtoi64(cols[8].c_str()); if(line.qty<1) line.qty=1;
            r.services.push_back(line);
        }
    }
    return true;
}

// =============================================================== PRINTING ==
//  Real GDI printing to the system default / chosen printer.
static void pLine(HDC dc, int& y, int x, int w, const std::wstring& s,
                  HFONT f, bool center=false){
    HGDIOBJ of = SelectObject(dc, f);
    RECT rc = {x, y, x+w, y+1000};
    DrawTextW(dc, s.c_str(), -1, &rc,
        DT_CALCRECT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
    int hgt = rc.bottom - rc.top;
    rc = {x, y, x+w, y+hgt};
    DrawTextW(dc, s.c_str(), -1, &rc,
        (center?DT_CENTER:DT_RIGHT)|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
    y += hgt + 8;
    SelectObject(dc, of);
}
static void pSep(HDC dc, int& y, int x, int w){
    HPEN p = CreatePen(PS_DOT,1,RGB(0,0,0));
    HGDIOBJ o = SelectObject(dc,p);
    MoveToEx(dc,x,y,0); LineTo(dc,x+w,y);
    SelectObject(dc,o); DeleteObject(p);
    y += 14;
}
bool printReceipt(const ReceptionRecord& r, int kind, HWND owner){
    PRINTDLGW pd = {0};
    pd.lStructSize = sizeof(pd);
    pd.hwndOwner   = owner;
    pd.Flags = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION | PD_USEDEVMODECOPIES;
    if(!PrintDlgW(&pd)){
        // fallback: default printer without dialog
        wchar_t prn[256]; DWORD sz=256;
        if(!GetDefaultPrinterW(prn,&sz)){
            MessageBoxW(owner, L"هیچ پرینتری روی سیستم پیدا نشد.",
                L"چاپ", MB_OK|MB_ICONWARNING);
            return false;
        }
        pd.hDC = CreateDCW(L"WINSPOOL", prn, NULL, NULL);
        if(!pd.hDC) return false;
    }
    HDC dc = pd.hDC;

    int dpiY = GetDeviceCaps(dc, LOGPIXELSY);
    int pw   = GetDeviceCaps(dc, HORZRES);
    int marg = dpiY/2;
    int w    = pw - 2*marg;

    HFONT fT = CreateFontW(-(dpiY*16/72),0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,
        0,0,CLEARTYPE_QUALITY,0,L"Vazirmatn");
    HFONT fN = CreateFontW(-(dpiY*11/72),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
        0,0,CLEARTYPE_QUALITY,0,L"Vazirmatn");
    HFONT fB = CreateFontW(-(dpiY*12/72),0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,
        0,0,CLEARTYPE_QUALITY,0,L"Vazirmatn");

    DOCINFOW di = { sizeof(di) };
    const wchar_t* names[3] = { L"رسید بیمه", L"نسخه پزشک", L"قبض پذیرش" };
    std::wstring docName = std::wstring(APP_NAME_W) + L" — " + names[kind];
    di.lpszDocName = docName.c_str();

    if(StartDocW(dc,&di) <= 0){
        // Selected/default printer rejected the job — typically a virtual "app"
        // device (PDF/photo app → "doesn't support print preview"). Re-open the
        // dialog so the operator can choose a real printer.
        DeleteDC(dc); dc=NULL;
        PRINTDLGW pd2={0}; pd2.lStructSize=sizeof(pd2); pd2.hwndOwner=owner;
        pd2.Flags=PD_RETURNDC|PD_NOPAGENUMS|PD_NOSELECTION|PD_USEDEVMODECOPIES;
        if(!PrintDlgW(&pd2)){
            DeleteObject(fT); DeleteObject(fN); DeleteObject(fB); return false; }
        dc=pd2.hDC; if(!dc){ DeleteObject(fT); DeleteObject(fN); DeleteObject(fB); return false; }
        if(StartDocW(dc,&di) <= 0){
            MessageBoxW(owner,L"چاپگر انتخاب‌شده از چاپ این سند پشتیبانی نمی‌کند.\n"
                L"لطفاً یک چاپگر واقعی (نه «Microsoft Print to PDF» یا برنامهٔ عکس) انتخاب کنید.",
                L"چاپ",MB_OK|MB_ICONWARNING);
            DeleteDC(dc); DeleteObject(fT); DeleteObject(fN); DeleteObject(fB); return false; }
    }
    StartPage(dc);
    SetBkMode(dc, TRANSPARENT);
    SetTextAlign(dc, TA_RIGHT|TA_RTLREADING);

    int y = marg, x = marg;
    wchar_t buf[512];

    pLine(dc,y,x,w, std::wstring(L"درمانگاه ") + APP_NAME_W, fT, true);
    pLine(dc,y,x,w, names[kind], fB, true);
    pSep(dc,y,x,w);
    swprintf(buf,512,L"شماره پذیرش: %d        تاریخ ثبت: %s        ساعت ثبت: %s",
        r.queueNo, toFaDigits(r.apptDate).c_str(), toFaDigits(r.apptTime).c_str());
    pLine(dc,y,x,w, buf, fN);
    swprintf(buf,512,L"شیفت: %s        بخش: %s        کاربر: %s",
        r.shift.c_str(), r.dept.c_str(), r.userName.c_str());
    pLine(dc,y,x,w, buf, fN);
    pSep(dc,y,x,w);
    swprintf(buf,512,L"بیمار: %s %s        کد ملی: %s",
        r.firstName.c_str(), r.lastName.c_str(), toFaDigits(r.nationalId).c_str());
    pLine(dc,y,x,w, buf, fB);
    swprintf(buf,512,L"نام پدر: %s        تاریخ تولد: %s        جنسیت: %s",
        r.fatherName.c_str(), toFaDigits(r.birthDate).c_str(), r.gender.c_str());
    pLine(dc,y,x,w, buf, fN);
    swprintf(buf,512,L"تلفن: %s        ثابت: %s",
        toFaDigits(r.mobile).c_str(), toFaDigits(r.landline).c_str());
    pLine(dc,y,x,w, buf, fN);
    if(!r.address.empty())
        pLine(dc,y,x,w, L"آدرس: " + r.address, fN);
    pSep(dc,y,x,w);
    pLine(dc,y,x,w, L"نوع بیمار: " + r.patientType, fN);
    pLine(dc,y,x,w, L"بیمه اصلی: " + r.insurance +
                    L"        بیمه مکمل: " + r.suppInsurance, fN);
    pSep(dc,y,x,w);

    // =====================================================================
    //  v1.61.0 — SERVICES TABLE on the classic fallback receipt.
    //  Until now this last-resort printer emitted only money and identity
    //  lines, so whenever a section had no bound design the services the
    //  operator entered vanished from the paper. It now prints a real ruled
    //  table: ردیف / نام خدمت / شرح / تعداد / مبلغ کل.
    // =====================================================================
    {
        pLine(dc,y,x,w, L"خدمات ارائه‌شده", fB, true);
        int colW[5]; // RTL: rightmost first
        colW[0]=(int)(w*0.07);   // ردیف
        colW[1]=(int)(w*0.34);   // نام خدمت
        colW[2]=(int)(w*0.27);   // شرح
        colW[3]=(int)(w*0.10);   // تعداد
        colW[4]=w-colW[0]-colW[1]-colW[2]-colW[3]; // مبلغ
        const wchar_t* hdr[5]={L"ردیف",L"نام خدمت",L"شرح خدمت",L"تعداد",L"مبلغ (ریال)"};
        int rowH=(int)(dpiY*0.26); if(rowH<18) rowH=18;
        int nRows=(int)r.services.size();
        int tableTop=y;
        // header row
        {
            HGDIOBJ of=SelectObject(dc,fB);
            UINT oldAl=GetTextAlign(dc);
            SetTextAlign(dc,TA_TOP|TA_LEFT|TA_RTLREADING);
            int cxr=x+w;
            for(int c=0;c<5;++c){
                RECT cr={cxr-colW[c]+4, y+2, cxr-4, y+rowH-2};
                DrawTextW(dc,hdr[c],-1,&cr,
                    DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
                cxr-=colW[c];
            }
            SetTextAlign(dc,oldAl);
            SelectObject(dc,of);
            y+=rowH;
        }
        if(nRows==0){
            HGDIOBJ of=SelectObject(dc,fN);
            UINT oldAl=GetTextAlign(dc);
            SetTextAlign(dc,TA_TOP|TA_LEFT|TA_RTLREADING);
            RECT cr={x+4,y+2,x+w-4,y+rowH-2};
            DrawTextW(dc,L"خدمتی ثبت نشده است",-1,&cr,
                DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
            SetTextAlign(dc,oldAl);
            SelectObject(dc,of);
            y+=rowH;
        } else {
            HGDIOBJ of=SelectObject(dc,fN);
            UINT oldAl=GetTextAlign(dc);
            SetTextAlign(dc,TA_TOP|TA_LEFT|TA_RTLREADING);
            for(int i=0;i<nRows;++i){
                const ServiceLine& sl=r.services[i];
                int q = sl.qty>0? sl.qty : 1;
                std::wstring cells[5];
                wchar_t nb[32];
                swprintf(nb,32,L"%d",i+1);            cells[0]=toFaDigits(nb);
                cells[1]= sl.name.empty()? L"—" : sl.name;
                cells[2]= sl.desc.empty()? (sl.category.empty()? std::wstring(L"—") : sl.category) : sl.desc;
                swprintf(nb,32,L"%d",q);              cells[3]=toFaDigits(nb);
                cells[4]= toFaDigits(formatMoney(sl.price*(long long)q - sl.discount));
                int cxr=x+w;
                for(int c=0;c<5;++c){
                    RECT cr={cxr-colW[c]+4, y+2, cxr-4, y+rowH-2};
                    UINT al=(c==1||c==2)? DT_RIGHT : DT_CENTER;
                    /* v2.06 — NO ELLIPSIS in the classic receipt either
                       («کلا هیچی نباید ۳ نقطه بشه»): wrap long service
                       names/descriptions instead of truncating them. */
                    RECT mr={0,0,cr.right-cr.left,1000000};
                    DrawTextW(dc,cells[c].c_str(),-1,&mr,
                        al|DT_TOP|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX|DT_CALCRECT);
                    int mh=mr.bottom-mr.top;
                    RECT dr=cr;
                    int off=((cr.bottom-cr.top)-mh)/2; if(off>0) dr.top+=off;
                    DrawTextW(dc,cells[c].c_str(),-1,&dr,
                        al|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX|DT_NOCLIP);
                    cxr-=colW[c];
                }
                y+=rowH;
            }
            SetTextAlign(dc,oldAl);
            SelectObject(dc,of);
        }
        // grid
        {
            int rows=(nRows>0? nRows : 1)+1;
            HPEN gp=CreatePen(PS_SOLID,1,RGB(0,0,0));
            HGDIOBJ op=SelectObject(dc,gp);
            for(int rr=0;rr<=rows;++rr){
                int yy=tableTop+rr*rowH;
                MoveToEx(dc,x,yy,0); LineTo(dc,x+w,yy);
            }
            int cxr=x+w;
            MoveToEx(dc,cxr,tableTop,0); LineTo(dc,cxr,tableTop+rows*rowH);
            for(int c=0;c<5;++c){
                cxr-=colW[c];
                MoveToEx(dc,cxr,tableTop,0); LineTo(dc,cxr,tableTop+rows*rowH);
            }
            SelectObject(dc,op); DeleteObject(gp);
        }
        y+=10;
        pSep(dc,y,x,w);
    }

    if(kind != 1){  // مالی — رسید بیمه و قبض
        pLine(dc,y,x,w, L"جمع کل: " + toFaDigits(formatMoney(r.total)) + L" ریال", fN);
        pLine(dc,y,x,w, L"سهم بیمه اصلی: " + toFaDigits(formatMoney(r.mainShare)) + L" ریال", fN);
        pLine(dc,y,x,w, L"مابه‌التفاوت پایه: " + toFaDigits(formatMoney(r.baseDiff)) + L" ریال", fN);
        pLine(dc,y,x,w, L"سهم سازمان (مکمل): " + toFaDigits(formatMoney(r.orgShare)) + L" ریال", fN);
        pLine(dc,y,x,w, L"سهم بیمار: " + toFaDigits(formatMoney(r.patientShare)) + L" ریال", fN);
        pLine(dc,y,x,w, L"تخفیف: " + toFaDigits(formatMoney(r.discount)) + L" ریال", fN);
        pSep(dc,y,x,w);
        pLine(dc,y,x,w, L"مبلغ قابل پرداخت: " + toFaDigits(formatMoney(r.paid)) + L" ریال", fB);
    } else {        // نسخه — فضای نسخه‌نویسی
        pLine(dc,y,x,w, L"شرح نسخه / دستور پزشک:", fB);
        for(int i=0;i<8;i++){ y += dpiY/3; pSep(dc,y,x,w); }
        pLine(dc,y,x,w, L"امضا و مهر پزشک", fN);
    }
    y += 10;
    pLine(dc,y,x,w, L"نرم‌افزار درمان پلاس — این رسید را نزد خود نگه دارید", fN, true);

    EndPage(dc);
    EndDoc(dc);
    DeleteObject(fT); DeleteObject(fN); DeleteObject(fB);
    DeleteDC(dc);
    logLine(L"printed: " + std::wstring(names[kind]));
    return true;
}
bool printLastReceipt(HWND owner){
    ReceptionRecord r;
    if(!loadLastReceipt(r)){
        MessageBoxW(owner, L"هنوز قبضی ثبت نشده است.", L"چاپ آخرین قبض",
            MB_OK|MB_ICONINFORMATION);
        return false;
    }
    // Resolve the stored department again so a reprint uses the same section-
    // bound design and receives the restored live service rows.
    int sectionId=0; std::vector<Section> sections; Sections_All(sections);
    for(const auto& s:sections){
        if(s.is_active && (s.name_fa==r.dept || s.code==r.dept)){ sectionId=s.id; break; }
    }
    if(sectionId==0){
        for(const auto& s:sections){ if(s.is_active && s.kind==L"reception"){ sectionId=s.id; break; } }
    }
    if(sectionId==0){ for(const auto& s:sections){ if(s.is_active){ sectionId=s.id; break; } } }
    // v1.65.0: attempt the services-capable print-design even when the stored
    // department no longer matches a section (sectionId==0 → first builtin).
    if(printPrintDesign(r,sectionId,owner)) return true;
    if(printDesignedReceipt(r,0,owner)) return true;
    return printReceipt(r,2,owner);
}
