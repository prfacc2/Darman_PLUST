// ============================================================================
//  manage.inc — management panel (v1.8.0 dashboard redesign)
//  #included into admin.cpp.  Owner-drawn, theme-aware, RTL.
//
//  The panel is now a structured DASHBOARD with a left navigation rail and
//  separate content pages instead of one flat, crowded screen:
//
//    PG_DASH   داشبورد           — summary cards (sections / employees / online
//                                  / pending requests) + quick actions
//    PG_SECTS  بخش‌ها             — existing sections grid + «ایجاد بخش» page
//    PG_DOCS   پزشکان             — doctor/nurse registry: searchable table +
//                                  full add/edit form (identity, contact, contract,
//                                  accounting) with delete (v1.69.0)
//    PG_EMPS   کارمندان           — employee list with combinable filters +
//                                  «افزودن کارمند» (full new-employee form)
//    PG_MSG    پیام به کارمندان   — compose with live search, dept filters,
//                                  message routing and media attachment
//    PG_REQS   درخواست‌ها         — settings + profile change requests
//                                  (distinct violet "info" category, never red)
//    PG_SAVED  پیام‌های ذخیره‌شده  — archived messages (when enabled in settings)
//
//  All heavy lifting (employee details / print, request approval modals) is
//  preserved.  Every rounded surface uses the v1.8.0 *Bg corner-patch helpers
//  so corners always match the theme background.
// ============================================================================
#define MG_CLASS L"AzManage"

// navigation pages
// v1.3.0: the «بیماران» (patients) page now lives in the clinic-management
// panel — index 1, right after the dashboard (RTL: visually first after it).
// It was previously only reachable from the hidden-admin panel, which was the
// wrong surface. The hidden-admin patients tab is removed (see admin.cpp).
enum { PG_DASH=0, PG_PATIENTS=1, PG_SECTS=2, PG_SERVICES=3, PG_DOCS=4, PG_EMPS=5,
       PG_MSG=6, PG_REQS=7, PG_SAVED=8, PG_BACKUP=9, PG_PRINTCFG=10, PG_SHIFTS=11, PG_COUNT=12 };

// control ids
enum {
    MG_ADD_CAT=2001, MG_E_NAME, MG_E_MGR, MG_E_ID, MG_CB_AUTOID,
    MG_FILTER_ABC, MG_FILTER_NEW, MG_FILTER_CREATED, MG_BACK,
    MG_ADD_EMP, MG_DESIGN,
    MG_MSG, MG_REQS, MG_PROFREQS, MG_BACKUP,
    MG_NOTE_ADD, MG_E_NOTE, MG_NOTE_ATTACH, MG_NOTE_SEND,   // saved messages
    MG_CB_DEPT,            // department filter combo (employees page)
    MG_DESIGN_SEC,         // print-design for the selected section
    MG_PT_SEARCH=2050,     // patients page: search box
    MG_PT_LIST,            // patients page: virtualized ListView
    MG_PT_ADD,             // patients page: «افزودن بیمار» button (§1.19.0)
    MG_NAV_BASE=2100,      // sidebar nav buttons (MG_NAV_BASE+page)
    MG_CARD_BASE=2200,     // clicking a section card
    MG_DETAIL_BASE=2400,   // clicking an employee row
    MG_QA_BASE=2600,       // dashboard quick-action tiles
    // ---- v1.28.0: مدیریت خدمات (Service Management) page ----
    MG_SV_CODE=2700,       // کد خدمت
    MG_SV_NAME,            // نام خدمت
    MG_SV_CAT,             // دسته
    MG_SV_DEPT,            // بخش (combo)
    MG_SV_PRICE,           // مبلغ پایه
    MG_SV_INS,             // نوع بیمه (combo)
    MG_SV_STATUS,          // وضعیت (combo: فعال/غیرفعال)
    MG_SV_DESC,            // توضیحات
    MG_SV_SHORT,           // v2.01: نام اختصار
    MG_SV_LOVING,          // v2.01: کد لوینگ
    MG_SV_EQUIV,           // v2.01: کد معادل
    MG_SV_SRVID,           // v2.01: شناسه خدمت
    MG_SV_REVGRP,          // v2.01: گروه درآمد
    MG_SV_HNID,            // v2.01: کد ملی سلامت
    MG_SV_FREE,            // v2.01: نرخ آزاد
    MG_SV_GOV,             // v2.01: نرخ دولتی
    MG_SV_FREEOLD,         // v2.01: نرخ آزاد قدیم
    MG_SV_GOVCOLD,         // v2.01: نرخ دولتی قدیم
    MG_SV_SAVE,            // ذخیره / افزودن
    MG_SV_EDIT,            // ویرایش (بارگذاری ردیف انتخابی در فرم)
    MG_SV_DELETE,          // حذف
    MG_SV_CLEAR,           // فرم جدید (پاک کردن)
    MG_SV_SEARCH,          // جستجو
    MG_SV_LIST,            // جدول خدمات (ListView)
    // ---- v1.69.0: مدیریت پزشکان (Doctors Management) page ----
    MG_DC_DEPT=2800,       // بخش (combo, from loadDepts())
    MG_DC_TYPE,            // نوع (combo: پزشک/پرستار)
    MG_DC_CODE,            // کد پزشک
    MG_DC_ACTIVE,          // فعال/غیرفعال (toggle button)
    MG_DC_MEDID,           // کد نظام پزشکی
    MG_DC_PREFIX,          // پیشوند نام
    MG_DC_FIRST,           // نام
    MG_DC_LAST,            // نام خانوادگی
    MG_DC_NID,             // کد ملی
    MG_DC_MOBILE,          // تلفن همراه
    MG_DC_EMAIL,           // ایمیل
    MG_DC_ADDRESS,         // آدرس
    MG_DC_FRANCHISE,       // فرانشیز
    MG_DC_PRINT,           // چاپ در قبض (toggle button)
    MG_DC_PERF,            // v1.78.0: تعریف به عنوان انجام دهنده (toggle button)
    MG_DC_SITE,            // v2.01: امکان نمایش در لیست پزشکان سایت (toggle)
    MG_DC_ONLINE,          // v2.01: امکان نوبت‌دهی اینترنتی (toggle)
    MG_DC_ANES,            // v2.01: پزشک بیهوشی می‌باشد (toggle)
    MG_DC_SPEC,            // تخصص
    MG_DC_INSSPEC,         // تخصص بیمه
    MG_DC_DEGREE,          // مدرک
    MG_DC_CONTRACT,        // نوع قرار داد (combo)
    MG_DC_EMERG,           // قرار پزشک اورژانس
    MG_DC_ACCT,            // حسابداری
    MG_DC_SAVE,            // ذخیره / افزودن
    MG_DC_EDIT,            // ویرایش
    MG_DC_DELETE,          // حذف
    MG_DC_CLEAR,           // فرم جدید (پاک کردن)
    MG_DC_SEARCH,          // جستجو
    MG_DC_LIST,            // جدول پزشکان (ListView)
    // v2.01 (Part F2): shift management page controls
    MG_SH_NAME=2900, MG_SH_START, MG_SH_END, MG_SH_SAVE, MG_SH_DELETE, MG_SH_CLEAR,
    MG_SH_LIST,
};

struct EmpRow {
    User user;
    EmpProfile prof;
    bool online;
    std::wstring deptName;
};

struct ManageState {
    int page;                   // PG_*
    int hot;                    // hot-tracked element id
    int navHot;                 // hot nav item (-1 none)
    // sections
    std::vector<DeptCat> cats;
    bool secAddMode;            // showing the «ایجاد بخش» add-form?
    HWND eName, eMgr, eId, bAdd, bAutoId, bSecNew, bSecBack;
    bool autoId;
    std::wstring autoIdPreview;
    // employees
    std::vector<EmpRow> emps;
    int  empSort;               // 0=abc, 1=newest, 2=created(oldest-first)
    std::wstring empDeptFilter; // dept id filter ("" = all)
    HWND bAbc, bNew, bCreated, bAddEmp, cDept;
    // messages / requests buttons (page bodies use owner-draw)
    HWND bMsg, bReqs, bProfReqs;
    // saved messages (local notes) page
    HWND eNote, bNoteAdd, bNoteAttach, bNoteSend;
    std::wstring noteAttach;    // pending attachment path for the next note
    int  noteHot;               // hot-tracked local-note row (-1 none)
    std::wstring curCatName;    // for personnel card "section" line
    std::wstring msg; COLORREF msgCol;
    // ---- patients page (v1.3.0): moved here from the hidden-admin panel ----
    HWND ptSearch=nullptr;      // debounced search box
    HWND ptList=nullptr;        // LVS_OWNERDATA virtualized list
    HWND ptAdd=nullptr;         // «افزودن بیمار» button (§1.19.0)
    std::vector<PatientRow>* ptAll=nullptr;   // owned full set (heap)
    std::vector<int> ptView;    // indices into *ptAll after filtering
    std::wstring ptQuery;       // last applied search text
    bool ptLoading=false, ptLoaded=false;
    // ---- services page (v1.28.0) ----
    HWND svCode=nullptr, svName=nullptr, svCat=nullptr, svDept=nullptr,
         svPrice=nullptr, svIns=nullptr, svStatus=nullptr, svDesc=nullptr,
         svShort=nullptr, svLoving=nullptr, svEquiv=nullptr, svSrvId=nullptr,
         svRevGrp=nullptr, svHnId=nullptr, svFree=nullptr, svGov=nullptr,
         svFreeOld=nullptr, svGovOld=nullptr,
         svSave=nullptr, svEdit=nullptr, svDelete=nullptr, svClear=nullptr,
         svSearch=nullptr, svList=nullptr;
    std::vector<ServiceDef> svAll;      // all services (loaded)
    std::vector<int>        svView;     // filtered indices into svAll
    std::wstring            svQuery;    // last search text
    std::wstring            svEditCode; // code currently loaded in the form ("" = new)
    bool                    svLoaded=false;
    // ---- doctors page (v1.69.0) ----
    //  In-memory model + form <-> record helpers, mirroring the services page.
    //  Doctor data is loaded ONCE on page entry (mgDocsReload) and cached here so
    //  WM_PAINT never touches the doctors.dat file.
    HWND dcDept=nullptr, dcType=nullptr, dcCode=nullptr, dcActiveChk=nullptr,
         dcMedId=nullptr, dcPrefix=nullptr, dcFirst=nullptr, dcLast=nullptr,
         dcNid=nullptr, dcMobile=nullptr, dcEmail=nullptr, dcAddress=nullptr,
         dcFranchise=nullptr, dcPrintChk=nullptr, dcSpec=nullptr, dcInsSpec=nullptr,
         dcDegree=nullptr, dcContract=nullptr, dcEmerg=nullptr, dcAcct=nullptr,
         dcSave=nullptr, dcEdit=nullptr, dcDelete=nullptr, dcClear=nullptr,
         dcSearch=nullptr, dcList=nullptr, dcPerfChk=nullptr,
         dcSiteChk=nullptr, dcOnlineChk=nullptr, dcAnesChk=nullptr;
    std::vector<DoctorDef>  dcAll;      // all doctors (loaded + cached)
    std::vector<int>        dcView;     // filtered indices into dcAll
    std::wstring            dcQuery;    // last search text
    int                     dcEditIdx=-1; // index into dcAll loaded in form (-1 = new)
    bool                    dcActiveVal=true, dcPrintVal=true; // toggle-button state
    bool                    dcPerfVal=true;   // v1.78.0: «تعریف به عنوان انجام دهنده» (default ON)
    bool                    dcSiteVal=true, dcOnlineVal=false, dcAnesVal=false; // v2.01 Part B
    bool                    dcLoaded=false;
    // v2.01 (Part F2): shift management page state
    std::vector<ShiftDef>    shAll;       // all shift definitions (cached)
    HWND                    shList=nullptr; // shift list ListView
    HWND                    shName=nullptr, shStart=nullptr, shEnd=nullptr;
    HWND                    shSave=nullptr, shDelete=nullptr, shClear=nullptr;
    int                     shEditId=-1;     // id being edited (-1 = new)
    bool                    shLoaded=false;
    // v1.66.0: dashboard summary cache — the dash calls loadAllPatients(),
    // loadUsers(), loadDepts(), countTodayReceptions(), unseen*Count() on
    // every WM_PAINT (several times per frame with GDI+), and every call
    // re-parses a potentially large JSON file. Compute once and reuse
    // across paints; invalidate only on page-switch back to dash.
    int  dashSec, dashEmp, dashOn, dashReq, dashPat, dashToday;
    bool dashValid;
    // v1.69.0: subtle staggered fade-in + count-up entrance for the dashboard.
    // Advances 0→1 over ~350 ms via a WM_TIMER; jumps to 1 instantly on weak
    // hardware (g_lowSpec). At steady state (==1) every colour is drawn at full
    // strength, so the entrance adds ZERO per-paint GDI+ cost once it settles.
    float dashAnim;        // 0..1 entrance progress
    bool  dashAnimOn;      // entrance timer running?
};
// background patient loader → UI thread marshal (no HWND access off-thread)
#define WM_MG_PATIENTS_READY (WM_APP+74)
// v1.69.0 dashboard entrance-animation timer id (kept clear of 0xB1 debounce).
#define MG_DASH_ANIM_TIMER 0xD1
static ManageState* s_mg=NULL;
static HWND g_hManage=NULL;
static void mgRefreshReqBadge();
static void openProfReqs(HWND owner);
static void mgGoPage(HWND h, int page);

// ====================================================== PATIENTS PAGE =======
//  v1.3.0 — a virtualized (LVS_OWNERDATA) patient list in the clinic-management
//  panel. Reuses the SAME data layer as the (now-removed) hidden-admin tab:
//  loadAllPatients() + uikit::NormalizeFa search. Loaded ONCE on a worker
//  thread so the UI never blocks; only visible rows are materialized so it
//  stays smooth with very large datasets.
static const wchar_t* mgGenderText(const std::wstring& g){
    if(g==L"1"||g==L"مرد"||g==L"M"||g==L"m") return L"مرد";
    if(g==L"0"||g==L"زن"||g==L"F"||g==L"f")  return L"زن";
    return L"—";
}
static void mgPatientsFilter(){
    if(!s_mg) return;
    s_mg->ptView.clear();
    if(!s_mg->ptAll) return;
    std::wstring q=uikit::NormalizeFa(s_mg->ptQuery);
    const auto& all=*s_mg->ptAll;
    s_mg->ptView.reserve(all.size());
    if(q.empty()){
        for(int i=0;i<(int)all.size();++i) s_mg->ptView.push_back(i);
    } else {
        for(int i=0;i<(int)all.size();++i){
            const PatientRow& p=all[i];
            std::wstring name=uikit::NormalizeFa(p.first+L" "+p.last);
            std::wstring nid =uikit::NormalizeFa(p.nid);
            std::wstring mob =uikit::NormalizeFa(p.mobile);
            if(name.find(q)!=std::wstring::npos ||
               nid.find(q)!=std::wstring::npos  ||
               mob.find(q)!=std::wstring::npos)
                s_mg->ptView.push_back(i);
        }
    }
    if(s_mg->ptList){
        ListView_SetItemCountEx(s_mg->ptList,(int)s_mg->ptView.size(),
            LVSICF_NOINVALIDATEALL|LVSICF_NOSCROLL);
        InvalidateRect(s_mg->ptList,NULL,FALSE);
    }
}
static void mgPatientsLoadAsync(HWND h){
    if(!s_mg || s_mg->ptLoaded || s_mg->ptLoading) return;
    s_mg->ptLoading=true;
    HWND target=h;
    std::thread([target](){
        auto* vec=new std::vector<PatientRow>(loadAllPatients());
        if(IsWindow(target)) PostMessageW(target,WM_MG_PATIENTS_READY,0,(LPARAM)vec);
        else delete vec;
    }).detach();
}

// ================================================= SERVICES PAGE (v1.28.0) ==
//  In-memory model, filter, and form <-> record helpers for «مدیریت خدمات».
static void mgServicesFilter(){
    if(!s_mg) return;
    std::wstring q=uikit::NormalizeFa(trim(s_mg->svQuery));
    s_mg->svView.clear();
    s_mg->svView.reserve(s_mg->svAll.size());
    for(int i=0;i<(int)s_mg->svAll.size();++i){
        const ServiceDef& s=s_mg->svAll[i];
        if(q.empty()){ s_mg->svView.push_back(i); continue; }
        // v2.01 (Part C): search across every visible catalogue column.
        std::wstring code  =uikit::NormalizeFa(s.code);
        std::wstring name  =uikit::NormalizeFa(s.name);
        std::wstring sh    =uikit::NormalizeFa(s.shortName);
        std::wstring loving=uikit::NormalizeFa(s.lovingCode);
        std::wstring equiv =uikit::NormalizeFa(s.equivCode);
        std::wstring sid   =uikit::NormalizeFa(s.serviceId);
        std::wstring rev   =uikit::NormalizeFa(s.revenueGroup);
        std::wstring hnid  =uikit::NormalizeFa(s.healthNationalId);
        if(code.find(q)!=std::wstring::npos   ||
           name.find(q)!=std::wstring::npos   ||
           sh.find(q)!=std::wstring::npos     ||
           loving.find(q)!=std::wstring::npos ||
           equiv.find(q)!=std::wstring::npos  ||
           sid.find(q)!=std::wstring::npos    ||
           rev.find(q)!=std::wstring::npos    ||
           hnid.find(q)!=std::wstring::npos)
            s_mg->svView.push_back(i);
    }
    if(s_mg->svList){
        ListView_SetItemCountEx(s_mg->svList,(int)s_mg->svView.size(),
            LVSICF_NOINVALIDATEALL|LVSICF_NOSCROLL);
        InvalidateRect(s_mg->svList,NULL,FALSE);
    }
}
//  (Re)populate the department combo for the services form from the live
//  section list, then apply the current filter and refresh the table.
static void mgServicesReload(){
    if(!s_mg) return;
    s_mg->svAll=loadServices();
    s_mg->svLoaded=true;
    // department combo: «—»/blank first, then each section + subsection from
    // the live section tree (v2.01 Part B/C — same pattern as the doctor form).
    if(s_mg->svDept){
        std::wstring cur; { wchar_t b[128]={0}; GetWindowTextW(s_mg->svDept,b,128); cur=b; }
        SendMessageW(s_mg->svDept,CB_RESETCONTENT,0,0);
        SendMessageW(s_mg->svDept,CB_ADDSTRING,0,(LPARAM)L"—");
        std::vector<Section> secs; Sections_All(secs);
        for(const auto& s : secs){
            if(!s.is_active) continue;
            std::wstring label=s.name_fa;
            if(s.parent_id>0){
                for(const auto& p : secs)
                    if(p.id==s.parent_id){ label=p.name_fa+L" ← "+s.name_fa; break; }
            }
            SendMessageW(s_mg->svDept,CB_ADDSTRING,0,(LPARAM)label.c_str());
        }
        int idx=0;
        if(!cur.empty()){
            int n=(int)SendMessageW(s_mg->svDept,CB_GETCOUNT,0,0);
            for(int i=1;i<n;i++){ wchar_t b[128]={0};
                SendMessageW(s_mg->svDept,CB_GETLBTEXT,i,(LPARAM)b);
                if(cur==b){ idx=i; break; } }
        }
        SendMessageW(s_mg->svDept,CB_SETCURSEL,idx,0);
    }
    mgServicesFilter();
}
//  Clear the form for a NEW service.
static void mgServicesClearForm(){
    if(!s_mg) return;
    SetWindowTextW(s_mg->svCode,L"");
    SetWindowTextW(s_mg->svName,L"");
    SetWindowTextW(s_mg->svCat, L"");
    SetWindowTextW(s_mg->svPrice,L"");
    SetWindowTextW(s_mg->svDesc,L"");
    // v2.01 (Part C) fields
    SetWindowTextW(s_mg->svShort,L"");
    SetWindowTextW(s_mg->svLoving,L"");
    SetWindowTextW(s_mg->svEquiv,L"");
    SetWindowTextW(s_mg->svSrvId,L"");
    SetWindowTextW(s_mg->svRevGrp,L"");
    SetWindowTextW(s_mg->svHnId,L"");
    SetWindowTextW(s_mg->svFree,L"");
    SetWindowTextW(s_mg->svGov,L"");
    SetWindowTextW(s_mg->svFreeOld,L"");
    SetWindowTextW(s_mg->svGovOld,L"");
    if(s_mg->svDept)   SendMessageW(s_mg->svDept,CB_SETCURSEL,0,0);
    if(s_mg->svIns)    SendMessageW(s_mg->svIns,CB_SETCURSEL,0,0);
    if(s_mg->svStatus) SendMessageW(s_mg->svStatus,CB_SETCURSEL,0,0);
    s_mg->svEditCode.clear();
}
//  §1.53.0: live Rials formatting — as the operator types digits into the
//  «قیمت» box we re-format the value with thousand separators (1,234,567) and
//  keep the caret at the end. Non-digit characters are silently dropped so the
//  field behaves like ES_NUMBER but shows grouping. Re-entrant safe: we bail
//  out while we're already rewriting to avoid a feedback storm.
static bool s_svPriceFormatting=false;
static void mgServicesFormatPriceLive(){
    if(!s_mg || !s_mg->svPrice) return;
    if(s_svPriceFormatting) return;
    s_svPriceFormatting=true;
    wchar_t b[64]={0}; GetWindowTextW(s_mg->svPrice,b,64);
    long long v=parseMoney(b);
    std::wstring fmt=formatMoney(v);
    if(fmt!=b){
        DWORD st=GetWindowLongW(s_mg->svPrice,GWL_STYLE);
        int sl0=0, sl1=0; SendMessageW(s_mg->svPrice,EM_GETSEL,(WPARAM)&sl0,(LPARAM)&sl1);
        SetWindowTextW(s_mg->svPrice,fmt.c_str());
        int len=(int)fmt.size();
        SendMessageW(s_mg->svPrice,EM_SETSEL,len,len);
    }
    s_svPriceFormatting=false;
}
//  v2.01 (Part C): the four tariff boxes (نرخ آزاد / دولتی + قدیم) get the same
//  live thousands-separator formatting as «قیمت». Each box owns a static
//  re-entrancy guard so the EN_CHANGE fired while rewriting does not loop.
static bool s_svFreeFmt=false, s_svGovFmt=false, s_svFreeOldFmt=false, s_svGovOldFmt=false;
static void mgServicesFormatMoneyLive(HWND box,bool& guard){
    if(!s_mg || !box) return;
    if(guard) return;
    guard=true;
    wchar_t b[64]={0}; GetWindowTextW(box,b,64);
    long long v=parseMoney(b);
    std::wstring fmt=formatMoney(v);
    if(fmt!=b){
        int sl0=0, sl1=0; SendMessageW(box,EM_GETSEL,(WPARAM)&sl0,(LPARAM)&sl1);
        SetWindowTextW(box,fmt.c_str());
        int len=(int)fmt.size();
        SendMessageW(box,EM_SETSEL,len,len);
    }
    guard=false;
}
static void mgServicesFormatFreeLive()   { if(s_mg) mgServicesFormatMoneyLive(s_mg->svFree,   s_svFreeFmt); }
static void mgServicesFormatGovLive()    { if(s_mg) mgServicesFormatMoneyLive(s_mg->svGov,    s_svGovFmt); }
static void mgServicesFormatFreeOldLive(){ if(s_mg) mgServicesFormatMoneyLive(s_mg->svFreeOld,s_svFreeOldFmt); }
static void mgServicesFormatGovOldLive() { if(s_mg) mgServicesFormatMoneyLive(s_mg->svGovOld, s_svGovOldFmt); }
//  Load a record into the form (for editing).
static void mgServicesLoadForm(const ServiceDef& s){
    if(!s_mg) return;
    SetWindowTextW(s_mg->svCode,s.code.c_str());
    SetWindowTextW(s_mg->svName,s.name.c_str());
    SetWindowTextW(s_mg->svCat, s.category.c_str());
    { std::wstring ps=formatMoney(s.price); SetWindowTextW(s_mg->svPrice,ps.c_str()); }
    SetWindowTextW(s_mg->svDesc,s.desc.c_str());
    // v2.01 (Part C) fields
    SetWindowTextW(s_mg->svShort, s.shortName.c_str());
    SetWindowTextW(s_mg->svLoving,s.lovingCode.c_str());
    SetWindowTextW(s_mg->svEquiv, s.equivCode.c_str());
    SetWindowTextW(s_mg->svSrvId, s.serviceId.c_str());
    SetWindowTextW(s_mg->svRevGrp,s.revenueGroup.c_str());
    SetWindowTextW(s_mg->svHnId,  s.healthNationalId.c_str());
    { std::wstring f=formatMoney(s.priceFree);     SetWindowTextW(s_mg->svFree,   f.c_str()); }
    { std::wstring g=formatMoney(s.priceGov);      SetWindowTextW(s_mg->svGov,    g.c_str()); }
    { std::wstring fo=formatMoney(s.priceFreeOld); SetWindowTextW(s_mg->svFreeOld,fo.c_str()); }
    { std::wstring go=formatMoney(s.priceGovOld);  SetWindowTextW(s_mg->svGovOld, go.c_str()); }
    // department combo by sectionId: build the «parent ← sub» label for the
    // matching section (or its plain name) and select that combo entry.
    if(s_mg->svDept){
        int sel=0;
        if(s.sectionId>0){
            std::vector<Section> secs; Sections_All(secs);
            std::wstring want;
            for(const auto& sec:secs){
                if(sec.id!=s.sectionId) continue;
                want=sec.name_fa;
                if(sec.parent_id>0){
                    for(const auto& p:secs)
                        if(p.id==sec.parent_id){ want=p.name_fa+L" ← "+sec.name_fa; break; }
                }
                break;
            }
            if(!want.empty()){
                int n=(int)SendMessageW(s_mg->svDept,CB_GETCOUNT,0,0);
                for(int i=1;i<n;i++){ wchar_t b[128]={0};
                    SendMessageW(s_mg->svDept,CB_GETLBTEXT,i,(LPARAM)b);
                    if(want==b){ sel=i; break; } }
            }
        }
        SendMessageW(s_mg->svDept,CB_SETCURSEL,sel,0);
    }
    if(s_mg->svIns){
        int n=(int)SendMessageW(s_mg->svIns,CB_GETCOUNT,0,0), sel=0;
        for(int i=0;i<n;i++){ wchar_t b[128]={0};
            SendMessageW(s_mg->svIns,CB_GETLBTEXT,i,(LPARAM)b);
            if(s.insType==b){ sel=i; break; } }
        SendMessageW(s_mg->svIns,CB_SETCURSEL,sel,0);
    }
    if(s_mg->svStatus) SendMessageW(s_mg->svStatus,CB_SETCURSEL,s.status?0:1,0);
    s_mg->svEditCode=s.code;
}
//  Read the form into a ServiceDef.
//  v1.49.0: only three fields survive in the UI — name, code, price. The other
//  columns (category / dept / insurance / description) are no longer edited; we
//  leave them empty for a NEW service and preserve the previous values when
//  EDITING (mgServicesSave copies them back from the stored record). Status is
//  always «فعال» (1) so simplified services stay usable in admission.
static ServiceDef mgServicesReadForm(){
    ServiceDef s;
    if(!s_mg) return s;
    wchar_t b[512]={0};
    GetWindowTextW(s_mg->svCode,b,512);   s.code=trim(b);
    GetWindowTextW(s_mg->svName,b,512);   s.name=trim(b);
    GetWindowTextW(s_mg->svPrice,b,512);  s.price=parseMoney(b);   // legacy base price
    // v2.01 (Part C) fields
    GetWindowTextW(s_mg->svShort,b,512);  s.shortName=trim(b);
    GetWindowTextW(s_mg->svLoving,b,512); s.lovingCode=trim(b);
    GetWindowTextW(s_mg->svEquiv,b,512);  s.equivCode=trim(b);
    GetWindowTextW(s_mg->svSrvId,b,512);  s.serviceId=trim(b);
    GetWindowTextW(s_mg->svRevGrp,b,512); s.revenueGroup=trim(b);
    GetWindowTextW(s_mg->svHnId,b,512);   s.healthNationalId=trim(b);
    // tariff money fields — parseMoney already strips thousand separators.
    GetWindowTextW(s_mg->svFree,b,64);    s.priceFree   =parseMoney(b);
    GetWindowTextW(s_mg->svGov,b,64);     s.priceGov    =parseMoney(b);
    GetWindowTextW(s_mg->svFreeOld,b,64); s.priceFreeOld=parseMoney(b);
    GetWindowTextW(s_mg->svGovOld,b,64);  s.priceGovOld =parseMoney(b);
    s.category.clear();
    s.dept.clear();
    s.insType.clear();
    s.desc.clear();
    // department: resolve the selected «parent ← sub» label back to a sectionId.
    s.sectionId=0;
    if(s_mg->svDept){
        int sel=(int)SendMessageW(s_mg->svDept,CB_GETCURSEL,0,0);
        if(sel>0){
            wchar_t lb[128]={0}; SendMessageW(s_mg->svDept,CB_GETLBTEXT,sel,(LPARAM)lb);
            std::wstring label=lb;
            std::vector<Section> secs; Sections_All(secs);
            for(const auto& sec:secs){
                if(!sec.is_active) continue;
                std::wstring lab=sec.name_fa;
                if(sec.parent_id>0){
                    for(const auto& p:secs)
                        if(p.id==sec.parent_id){ lab=p.name_fa+L" ← "+sec.name_fa; break; }
                }
                if(lab==label){ s.sectionId=sec.id; break; }
            }
        }
    }
    // status combo (فعال / غیرفعال)
    s.status = 1;
    if(s_mg->svStatus){
        int sel=(int)SendMessageW(s_mg->svStatus,CB_GETCURSEL,0,0);
        if(sel==1) s.status=0;
    }
    return s;
}
//  «ذخیره» — insert a new service or update the one loaded in the form.
static void mgServicesSave(HWND h){
    if(!s_mg) return;
    ServiceDef s=mgServicesReadForm();
    // ---- required-field validation (v2.01 Part C) ----
    if(s.shortName.empty()){
        s_mg->msg=L"نام اختصار الزامی است."; s_mg->msgCol=g_theme.danger;
        InvalidateRect(h,NULL,TRUE); return;
    }
    if(s.name.empty()){
        s_mg->msg=L"نام کامل الزامی است."; s_mg->msgCol=g_theme.danger;
        InvalidateRect(h,NULL,TRUE); return;
    }
    if(s.code.empty()){
        s_mg->msg=L"کد خدمت الزامی است."; s_mg->msgCol=g_theme.danger;
        InvalidateRect(h,NULL,TRUE); return;
    }
    // code must be unique (exclude the record currently loaded for editing)
    for(const auto& e:s_mg->svAll){
        if(!s_mg->svEditCode.empty() && e.code==s_mg->svEditCode) continue;
        if(e.code==s.code){
            s_mg->msg=L"این کد خدمت تکراری است."; s_mg->msgCol=g_theme.danger;
            InvalidateRect(h,NULL,TRUE); return;
        }
    }
    // ---- historical-rate migration (قدیم) ----
    // When the operator changes «نرخ آزاد» / «نرخ دولتی» and the matching قدیم
    // box is still empty, offer to archive the previous value there.
    const ServiceDef* prev=nullptr;
    for(const auto& e:s_mg->svAll) if(e.code==s_mg->svEditCode){ prev=&e; break; }
    if(prev){
        if(s.priceFree!=prev->priceFree && s.priceFreeOld==0 && prev->priceFree!=0){
            if(MessageBoxW(h,L"نرخ آزاد تغییر کرد. مقدار قبلی به «نرخ آزاد قدیم» منتقل شود؟",
                L"بایگانی نرخ قدیم",MB_YESNO|MB_ICONQUESTION)==IDYES)
                s.priceFreeOld=prev->priceFree;
        }
        if(s.priceGov!=prev->priceGov && s.priceGovOld==0 && prev->priceGov!=0){
            if(MessageBoxW(h,L"نرخ دولتی تغییر کرد. مقدار قبلی به «نرخ دولتی قدیم» منتقل شود؟",
                L"بایگانی نرخ قدیم",MB_YESNO|MB_ICONQUESTION)==IDYES)
                s.priceGovOld=prev->priceGov;
        }
    }
    std::wstring err; bool ok;
    if(!s_mg->svEditCode.empty() && s.code==s_mg->svEditCode){
        // EDIT: preserve the hidden/legacy columns from the existing record so a
        // service that already carried a category/dept/insurance/description or
        // the professional tariff columns does not silently lose them. Status is
        // now edited via the combo, so it is NOT overwritten here.
        for(const auto& e:s_mg->svAll){
            if(e.code==s.code){
                s.category=e.category; s.dept=e.dept;
                s.insType =e.insType;  s.desc=e.desc;
                s.created =e.created;
                s.insName=e.insName; s.multiplier=e.multiplier;
                s.priceFreeNew=e.priceFreeNew; s.priceGovNew=e.priceGovNew;
                s.priceIns=e.priceIns; s.priceInsNew=e.priceInsNew;
                s.extra=e.extra;
                break;
            }
        }
        ok=updateService(s,err);
        if(ok){ s_mg->msg=L"خدمت «"+s.name+L"» ویرایش شد."; s_mg->msgCol=g_theme.success; }
    } else {
        ok=addService(s,err);
        if(ok){ s_mg->msg=L"خدمت «"+s.name+L"» اضافه شد."; s_mg->msgCol=g_theme.success; }
    }
    if(!ok){ s_mg->msg=err; s_mg->msgCol=g_theme.danger; }
    else {
        mgServicesClearForm();
        // LIVE SYNC: push the fresh catalog to any open embedded Admission view
        // so the change is reflected immediately, without a page reload.
        WebAdmission_NotifyCatalogChanged();
    }
    mgServicesReload();
    InvalidateRect(h,NULL,TRUE);
}
//  «حذف» — delete the selected row (with confirm).
static void mgServicesDelete(HWND h){
    if(!s_mg || !s_mg->svList) return;
    int sel=ListView_GetNextItem(s_mg->svList,-1,LVNI_SELECTED);
    if(sel<0 || sel>=(int)s_mg->svView.size()){
        s_mg->msg=L"ابتدا یک خدمت را از جدول انتخاب کنید."; s_mg->msgCol=g_theme.danger;
        InvalidateRect(h,NULL,TRUE); return;
    }
    const ServiceDef& s=s_mg->svAll[s_mg->svView[sel]];
    std::wstring q=L"حذف خدمت «"+s.name+L"»؟";
    if(MessageBoxW(h,q.c_str(),L"تأیید حذف",MB_YESNO|MB_ICONWARNING)!=IDYES) return;
    removeService(s.code);
    s_mg->msg=L"خدمت حذف شد."; s_mg->msgCol=g_theme.success;
    mgServicesClearForm();
    // LIVE SYNC: reflect the deletion in any open embedded Admission view.
    WebAdmission_NotifyCatalogChanged();
    mgServicesReload();
    InvalidateRect(h,NULL,TRUE);
}
//  «ویرایش» / double-click — load the selected row into the form.
static void mgServicesEditSelected(HWND h){
    if(!s_mg || !s_mg->svList) return;
    int sel=ListView_GetNextItem(s_mg->svList,-1,LVNI_SELECTED);
    if(sel<0 || sel>=(int)s_mg->svView.size()){
        s_mg->msg=L"ابتدا یک خدمت را از جدول انتخاب کنید."; s_mg->msgCol=g_theme.danger;
        InvalidateRect(h,NULL,TRUE); return;
    }
    mgServicesLoadForm(s_mg->svAll[s_mg->svView[sel]]);
    InvalidateRect(h,NULL,TRUE);
    SetFocus(s_mg->svName);
}

// ================================================= DOCTORS PAGE (v1.69.0) ====
//  In-memory model, filter and form <-> record helpers for «مدیریت پزشکان».
//  Mirrors the services page: doctors are loaded once into s_mg->dcAll (cached,
//  no per-WM_PAINT file reads), filtered into dcView, and rendered by an
//  LVS_OWNERDATA ListView. The form composes the display `name` from prefix +
//  first + last on save (the data layer keeps the parts too).
//  NOTE on deptId: reception only consumes a doctor's name/specialty, and a
//  DeptCat.id may be empty (auto ids), so the form stores the SECTION NAME in
//  deptId (human-readable, round-trips safely) — same convention the services
//  page uses for its department field.
static void mgDocsRefreshChecks(){
    if(!s_mg) return;
    SetWindowTextW(s_mg->dcActiveChk, s_mg->dcActiveVal?L"\u2611 فعال":L"\u2610 غیرفعال");
    SetWindowTextW(s_mg->dcPrintChk,  s_mg->dcPrintVal ?L"\u2611 بله":L"\u2610 خیر");
    // v1.78.0: «تعریف به عنوان انجام دهنده» toggle (default بله)
    if(s_mg->dcPerfChk)
        SetWindowTextW(s_mg->dcPerfChk, s_mg->dcPerfVal?L"\u2611 بله":L"\u2610 خیر");
    // v2.01 (Part B): complementary option checkboxes
    if(s_mg->dcSiteChk)
        SetWindowTextW(s_mg->dcSiteChk, s_mg->dcSiteVal?L"\u2611 بله":L"\u2610 خیر");
    if(s_mg->dcOnlineChk)
        SetWindowTextW(s_mg->dcOnlineChk, s_mg->dcOnlineVal?L"\u2611 بله":L"\u2610 خیر");
    if(s_mg->dcAnesChk)
        SetWindowTextW(s_mg->dcAnesChk, s_mg->dcAnesVal?L"\u2611 بله":L"\u2610 خیر");
}
static void mgDocsFilter(){
    if(!s_mg) return;
    std::wstring q=uikit::NormalizeFa(trim(s_mg->dcQuery));
    s_mg->dcView.clear();
    s_mg->dcView.reserve(s_mg->dcAll.size());
    for(int i=0;i<(int)s_mg->dcAll.size();++i){
        const DoctorDef& d=s_mg->dcAll[i];
        if(q.empty()){ s_mg->dcView.push_back(i); continue; }
        std::wstring name=uikit::NormalizeFa(d.name);
        std::wstring spec =uikit::NormalizeFa(d.specialty);
        std::wstring code=uikit::NormalizeFa(d.docCode);
        if(name.find(q)!=std::wstring::npos ||
           spec.find(q)!=std::wstring::npos  ||
           code.find(q)!=std::wstring::npos)
            s_mg->dcView.push_back(i);
    }
    if(s_mg->dcList){
        ListView_SetItemCountEx(s_mg->dcList,(int)s_mg->dcView.size(),
            LVSICF_NOINVALIDATEALL|LVSICF_NOSCROLL);
        InvalidateRect(s_mg->dcList,NULL,FALSE);
    }
}
//  (Re)load doctors + repopulate the department combo from the live SECTION
//  tree (sections.dat with subsections), then re-apply the filter and refresh
//  the table. v2.01 (Part B): switched from loadDepts() to Sections_All() so
//  the doctor's «بخش» can be a section OR a subsection.
static void mgDocsReload(){
    if(!s_mg) return;
    s_mg->dcAll=loadDoctors();
    s_mg->dcLoaded=true;
    if(s_mg->dcDept){
        std::wstring cur; { wchar_t b[128]={0}; GetWindowTextW(s_mg->dcDept,b,128); cur=b; }
        SendMessageW(s_mg->dcDept,CB_RESETCONTENT,0,0);
        SendMessageW(s_mg->dcDept,CB_ADDSTRING,0,(LPARAM)L"—");
        // v2.01: enumerate sections + subsections from the live section tree.
        std::vector<Section> secs; Sections_All(secs);
        for(const auto& s : secs){
            if(!s.is_active) continue;
            std::wstring label=s.name_fa;
            if(s.parent_id>0){
                // subsection: prefix with parent name for clarity
                for(const auto& p : secs)
                    if(p.id==s.parent_id){ label=p.name_fa+L" ← "+s.name_fa; break; }
            }
            SendMessageW(s_mg->dcDept,CB_ADDSTRING,0,(LPARAM)label.c_str());
        }
        int idx=0;
        if(!cur.empty()){
            int n=(int)SendMessageW(s_mg->dcDept,CB_GETCOUNT,0,0);
            for(int i=1;i<n;i++){ wchar_t b[128]={0};
                SendMessageW(s_mg->dcDept,CB_GETLBTEXT,i,(LPARAM)b);
                if(cur==b){ idx=i; break; } }
        }
        SendMessageW(s_mg->dcDept,CB_SETCURSEL,idx,0);
    }
    mgDocsFilter();
}
//  Clear the form for a NEW doctor (checkboxes reset to their defaults).
static void mgDocsClearForm(){
    if(!s_mg) return;
    SetWindowTextW(s_mg->dcCode,L"");
    SetWindowTextW(s_mg->dcMedId,L"");
    SetWindowTextW(s_mg->dcPrefix,L"");
    SetWindowTextW(s_mg->dcFirst,L"");
    SetWindowTextW(s_mg->dcLast,L"");
    SetWindowTextW(s_mg->dcNid,L"");
    SetWindowTextW(s_mg->dcMobile,L"");
    SetWindowTextW(s_mg->dcEmail,L"");
    SetWindowTextW(s_mg->dcAddress,L"");
    SetWindowTextW(s_mg->dcFranchise,L"");
    SetWindowTextW(s_mg->dcSpec,L"");
    SetWindowTextW(s_mg->dcInsSpec,L"");
    SetWindowTextW(s_mg->dcDegree,L"");
    SetWindowTextW(s_mg->dcEmerg,L"");
    SetWindowTextW(s_mg->dcAcct,L"");
    if(s_mg->dcDept)     SendMessageW(s_mg->dcDept,CB_SETCURSEL,0,0);
    if(s_mg->dcType)     SendMessageW(s_mg->dcType,CB_SETCURSEL,0,0);
    if(s_mg->dcContract) SendMessageW(s_mg->dcContract,CB_SETCURSEL,0,0);
    s_mg->dcActiveVal=true; s_mg->dcPrintVal=true;
    s_mg->dcPerfVal=true;              // v1.78.0: performer flag defaults ON for new doctors
    s_mg->dcSiteVal=true; s_mg->dcOnlineVal=false; s_mg->dcAnesVal=false; // v2.01 Part B defaults
    s_mg->dcEditIdx=-1;
    mgDocsRefreshChecks();
}
//  Load a record into the form (for editing).
static void mgDocsLoadForm(const DoctorDef& d){
    if(!s_mg) return;
    SetWindowTextW(s_mg->dcCode,d.docCode.c_str());
    SetWindowTextW(s_mg->dcMedId,d.medicalId.c_str());
    SetWindowTextW(s_mg->dcPrefix,d.namePrefix.c_str());
    SetWindowTextW(s_mg->dcFirst,d.firstName.c_str());
    SetWindowTextW(s_mg->dcLast,d.lastName.c_str());
    SetWindowTextW(s_mg->dcNid,d.nationalId.c_str());
    SetWindowTextW(s_mg->dcMobile,d.mobile.c_str());
    SetWindowTextW(s_mg->dcEmail,d.email.c_str());
    SetWindowTextW(s_mg->dcAddress,d.address.c_str());
    SetWindowTextW(s_mg->dcFranchise,d.franchise.c_str());
    SetWindowTextW(s_mg->dcSpec,d.specialty.c_str());
    SetWindowTextW(s_mg->dcInsSpec,d.insSpecialty.c_str());
    SetWindowTextW(s_mg->dcDegree,d.degree.c_str());
    SetWindowTextW(s_mg->dcEmerg,d.emergencyContract.c_str());
    SetWindowTextW(s_mg->dcAcct,d.accounting.c_str());
    if(s_mg->dcDept){                       // dept combo by stored section name
        int n=(int)SendMessageW(s_mg->dcDept,CB_GETCOUNT,0,0), sel=0;
        for(int i=0;i<n;i++){ wchar_t b[128]={0};
            SendMessageW(s_mg->dcDept,CB_GETLBTEXT,i,(LPARAM)b);
            if(d.deptId==b){ sel=i; break; } }
        SendMessageW(s_mg->dcDept,CB_SETCURSEL,sel,0);
    }
    if(s_mg->dcType) SendMessageW(s_mg->dcType,CB_SETCURSEL,d.docType==0?0:1,0);
    if(s_mg->dcContract){
        int n=(int)SendMessageW(s_mg->dcContract,CB_GETCOUNT,0,0);
        int ct=d.contractType; if(ct<0)ct=0; if(n>0&&ct>=n)ct=0;
        SendMessageW(s_mg->dcContract,CB_SETCURSEL,ct,0);
    }
    s_mg->dcActiveVal=d.active; s_mg->dcPrintVal=d.printOnReceipt;
    s_mg->dcPerfVal=d.isPerformer;   // v1.78.0: edit form round-trips the performer flag
    s_mg->dcSiteVal=d.showOnSiteList; s_mg->dcOnlineVal=d.onlineAppointment; s_mg->dcAnesVal=d.isAnesthesiologist; // v2.01 Part B
    mgDocsRefreshChecks();
}
//  Read the form into a DoctorDef. The display `name` is composed from the
//  prefix / first / last parts; `services` is left empty (preserved on edit).
static DoctorDef mgDocsReadForm(){
    DoctorDef d;
    if(!s_mg) return d;
    wchar_t b[512]={0};
    GetWindowTextW(s_mg->dcCode,b,512);      d.docCode=trim(b);
    GetWindowTextW(s_mg->dcMedId,b,512);     d.medicalId=trim(b);
    GetWindowTextW(s_mg->dcPrefix,b,512);    d.namePrefix=trim(b);
    GetWindowTextW(s_mg->dcFirst,b,512);     d.firstName=trim(b);
    GetWindowTextW(s_mg->dcLast,b,512);      d.lastName=trim(b);
    GetWindowTextW(s_mg->dcNid,b,512);       d.nationalId=trim(b);
    GetWindowTextW(s_mg->dcMobile,b,512);    d.mobile=trim(b);
    GetWindowTextW(s_mg->dcEmail,b,512);     d.email=trim(b);
    GetWindowTextW(s_mg->dcAddress,b,512);   d.address=trim(b);
    GetWindowTextW(s_mg->dcFranchise,b,512); d.franchise=trim(b);
    GetWindowTextW(s_mg->dcSpec,b,512);      d.specialty=trim(b);
    GetWindowTextW(s_mg->dcInsSpec,b,512);   d.insSpecialty=trim(b);
    GetWindowTextW(s_mg->dcDegree,b,512);    d.degree=trim(b);
    GetWindowTextW(s_mg->dcEmerg,b,512);     d.emergencyContract=trim(b);
    GetWindowTextW(s_mg->dcAcct,b,512);      d.accounting=trim(b);
    if(s_mg->dcDept){                        // deptId stores the section NAME
        int sel=(int)SendMessageW(s_mg->dcDept,CB_GETCURSEL,0,0);
        if(sel>0){ wchar_t nb[128]={0};
            SendMessageW(s_mg->dcDept,CB_GETLBTEXT,sel,(LPARAM)nb); d.deptId=nb; }
        else d.deptId.clear();
    }
    if(s_mg->dcType) d.docType = (SendMessageW(s_mg->dcType,CB_GETCURSEL,0,0)==1)?1:0;
    if(s_mg->dcContract){
        int sel=(int)SendMessageW(s_mg->dcContract,CB_GETCURSEL,0,0);
        d.contractType = sel<0?0:sel;
    }
    d.active=s_mg->dcActiveVal; d.printOnReceipt=s_mg->dcPrintVal;
    d.isPerformer=s_mg->dcPerfVal;   // v1.78.0: the form owns the flag (no silent reset)
    d.showOnSiteList=s_mg->dcSiteVal; d.onlineAppointment=s_mg->dcOnlineVal; d.isAnesthesiologist=s_mg->dcAnesVal; // v2.01 Part B
    std::wstring nm;
    if(!d.namePrefix.empty()) nm += d.namePrefix + L" ";
    if(!d.firstName.empty())  nm += d.firstName  + L" ";
    if(!d.lastName.empty())   nm += d.lastName;
    d.name=trim(nm);
    return d;
}
//  «ذخیره» — insert a new doctor or update the one loaded in the form.
//  v2.01 (Part B2): full validation — required fields, national-ID checksum,
//  mobile format, email format, duplicate docCode/medicalId/nationalId.
//  On edit the existing `services` list is preserved.
static void mgDocsSave(HWND h){
    if(!s_mg) return;
    DoctorDef d=mgDocsReadForm();
    // Required: first name OR last name (so name is never empty).
    if(d.name.empty()){
        s_mg->msg=L"نام و نام خانوادگی پزشک الزامی است."; s_mg->msgCol=g_theme.danger;
        InvalidateRect(h,NULL,TRUE); return;
    }
    // Required: docCode (کد پزشک) — numeric, unique.
    if(d.docCode.empty()){
        s_mg->msg=L"کد پزشک الزامی است."; s_mg->msgCol=g_theme.danger;
        InvalidateRect(h,NULL,TRUE); return;
    }
    for(wchar_t c : d.docCode) if(c<L'0'||c>L'9'){
        if(c>=L'\u06F0'&&c<=L'\u06F9') continue; // Persian digits OK
        s_mg->msg=L"کد پزشک باید عددی باشد."; s_mg->msgCol=g_theme.danger;
        InvalidateRect(h,NULL,TRUE); return;
    }
    // Required: medicalId (کد نظام پزشکی) — unique.
    if(d.medicalId.empty()){
        s_mg->msg=L"کد نظام پزشکی الزامی است."; s_mg->msgCol=g_theme.danger;
        InvalidateRect(h,NULL,TRUE); return;
    }
    // National ID validation (if provided): 10 digits, checksum, unique.
    if(!d.nationalId.empty()){
        if(!validateNationalId(d.nationalId)){
            s_mg->msg=L"کد ملی نامعتبر است."; s_mg->msgCol=g_theme.danger;
            InvalidateRect(h,NULL,TRUE); return;
        }
    }
    // Mobile validation (if provided).
    if(!d.mobile.empty() && !validateMobile(d.mobile)){
        s_mg->msg=L"تلفن همراه نامعتبر است. (مثال: ۰۹۱۲۳۴۵۶۷۸۹)"; s_mg->msgCol=g_theme.danger;
        InvalidateRect(h,NULL,TRUE); return;
    }
    // Email validation (if provided).
    if(!d.email.empty() && !validateEmail(d.email)){
        s_mg->msg=L"ایمیل نامعتبر است."; s_mg->msgCol=g_theme.danger;
        InvalidateRect(h,NULL,TRUE); return;
    }
    // Franchise validation: numeric percentage 0-100 (if provided).
    if(!d.franchise.empty()){
        int f=_wtoi(d.franchise.c_str());
        if(f<0||f>100){
            s_mg->msg=L"فرانشیز باید عددی بین ۰ تا ۱۰۰ باشد."; s_mg->msgCol=g_theme.danger;
            InvalidateRect(h,NULL,TRUE); return;
        }
    }
    // Duplicate checks against all other doctors.
    int editIdx=s_mg->dcEditIdx;
    for(int i=0;i<(int)s_mg->dcAll.size();++i){
        if(i==editIdx) continue;
        const DoctorDef& e=s_mg->dcAll[i];
        if(!d.docCode.empty() && e.docCode==d.docCode){
            s_mg->msg=L"کد پزشک تکراری است. این کد برای پزشک «"+e.name+L"» ثبت شده است.";
            s_mg->msgCol=g_theme.danger; InvalidateRect(h,NULL,TRUE); return;
        }
        if(!d.medicalId.empty() && e.medicalId==d.medicalId){
            s_mg->msg=L"کد نظام پزشکی تکراری است. این کد برای پزشک «"+e.name+L"» ثبت شده است.";
            s_mg->msgCol=g_theme.danger; InvalidateRect(h,NULL,TRUE); return;
        }
        if(!d.nationalId.empty() && e.nationalId==d.nationalId){
            s_mg->msg=L"کد ملی تکراری است. این کد برای پزشک «"+e.name+L"» ثبت شده است.";
            s_mg->msgCol=g_theme.danger; InvalidateRect(h,NULL,TRUE); return;
        }
    }
    bool ok;
    if(editIdx>=0 && editIdx<(int)s_mg->dcAll.size()){
        d.services=s_mg->dcAll[editIdx].services;   // keep services list
        s_mg->dcAll[editIdx]=d;
        ok=saveDoctors(s_mg->dcAll);
        if(ok){ s_mg->msg=L"پزشک «"+d.name+L"» ویرایش شد."; s_mg->msgCol=g_theme.success; }
    } else {
        s_mg->dcAll.push_back(d);
        ok=saveDoctors(s_mg->dcAll);
        if(ok){ s_mg->msg=L"پزشک «"+d.name+L"» اضافه شد."; s_mg->msgCol=g_theme.success; }
    }
    if(!ok){ s_mg->msg=L"ذخیرهٔ پزشک ناموفق بود."; s_mg->msgCol=g_theme.danger; }
    else { mgDocsClearForm(); WebAdmission_NotifyCatalogChanged(); }
    mgDocsReload();
    InvalidateRect(h,NULL,TRUE);
}
//  «حذف» — delete the selected row (with confirm). v2.01 (Part B2): if the
//  doctor is already used on an existing receipt/ticket, deletion is blocked
//  with a clear explanation and the user is offered «غیرفعال کردن» instead.
static void mgDocsDelete(HWND h){
    if(!s_mg || !s_mg->dcList) return;
    int sel=ListView_GetNextItem(s_mg->dcList,-1,LVNI_SELECTED);
    if(sel<0 || sel>=(int)s_mg->dcView.size()){
        s_mg->msg=L"ابتدا یک پزشک را از جدول انتخاب کنید."; s_mg->msgCol=g_theme.danger;
        InvalidateRect(h,NULL,TRUE); return;
    }
    int idx=s_mg->dcView[sel];
    const DoctorDef& d=s_mg->dcAll[idx];
    // Check if this doctor is used on any cashier ticket (by name).
    bool inUse=false;
    {
        // Load tickets via Receipt_SearchJson by doctor name — but the simplest
        // check is to scan cashier_tickets.dat for the doctor name. We use the
        // public Receipt_SearchJson which returns JSON; an empty rows array
        // means no usage.
        ReceiptQuery rq; rq.doctor=d.name;
        std::string result=Receipt_SearchJson(rq);
        if(result.find("\"rows\":[")!=std::string::npos &&
           result.find("\"rows\":[]")==std::string::npos)
            inUse=true;
    }
    if(inUse){
        std::wstring msg=L"پزشک «"+d.name+L"» در قبض‌های صادرشده استفاده شده است و قابل حذف نیست.\n"
                         L"می‌توانید آن را غیرفعال کنید تا در پذیرش‌های جدید نمایش داده نشود.\n\n"
                         L"آیا می‌خواهید این پزشک را غیرفعال کنید؟";
        if(MessageBoxW(h,msg.c_str(),L"حذف ممکن نیست",MB_YESNO|MB_ICONQUESTION)==IDYES){
            s_mg->dcAll[idx].active=false;
            saveDoctors(s_mg->dcAll);
            s_mg->msg=L"پزشک «"+d.name+L"» غیرفعال شد."; s_mg->msgCol=g_theme.success;
            mgDocsClearForm();
            mgDocsReload();
            InvalidateRect(h,NULL,TRUE);
        }
        return;
    }
    std::wstring q=L"حذف پزشک «"+d.name+L"»؟";
    if(MessageBoxW(h,q.c_str(),L"تأیید حذف",MB_YESNO|MB_ICONWARNING)!=IDYES) return;
    s_mg->dcAll.erase(s_mg->dcAll.begin()+idx);
    saveDoctors(s_mg->dcAll);
    s_mg->msg=L"پزشک حذف شد."; s_mg->msgCol=g_theme.success;
    mgDocsClearForm();
    mgDocsReload();
    WebAdmission_NotifyCatalogChanged();
    InvalidateRect(h,NULL,TRUE);
}
//  «ویرایش» / double-click — load the selected row into the form.
static void mgDocsEditSelected(HWND h){
    if(!s_mg || !s_mg->dcList) return;
    int sel=ListView_GetNextItem(s_mg->dcList,-1,LVNI_SELECTED);
    if(sel<0 || sel>=(int)s_mg->dcView.size()){
        s_mg->msg=L"ابتدا یک پزشک را از جدول انتخاب کنید."; s_mg->msgCol=g_theme.danger;
        InvalidateRect(h,NULL,TRUE); return;
    }
    s_mg->dcEditIdx=s_mg->dcView[sel];
    mgDocsLoadForm(s_mg->dcAll[s_mg->dcEditIdx]);
    InvalidateRect(h,NULL,TRUE);
    SetFocus(s_mg->dcFirst);
}

// ---- data refresh ----------------------------------------------------------
static std::wstring deptNameById(const std::wstring& id){
    if(!s_mg) return L"";
    for(auto& c:s_mg->cats) if(c.id==id) return c.name;
    return L"";
}
static void mgLoadCats(){
    if(!s_mg) return;
    s_mg->cats=loadDepts();
}
static void mgLoadEmps(){
    if(!s_mg) return;
    s_mg->emps.clear();
    auto users=loadUsers();
    int order=0;
    for(auto& u:users){
        EmpProfile p=loadEmpProfile(u.username);
        // department filter (combines with the sort below)
        if(!s_mg->empDeptFilter.empty()){
            std::wstring did = p.deptId.empty()? L"" : p.deptId;
            // match by deptId OR by legacy dept-name fallback
            bool match = (did==s_mg->empDeptFilter);
            if(!match && did.empty() && u.dept==deptNameById(s_mg->empDeptFilter)) match=true;
            if(!match) continue;
        }
        EmpRow r; r.user=u; r.prof=p; r.online=isUserOnline(u.username);
        r.deptName = p.deptId.empty()? u.dept : deptNameById(p.deptId);
        if(r.deptName.empty()) r.deptName=u.dept;
        s_mg->emps.push_back(r);
        (void)order;
    }
    // sort (combines with the dept filter above)
    if(s_mg->empSort==0){            // alphabetical (by full name)
        std::sort(s_mg->emps.begin(),s_mg->emps.end(),
            [](const EmpRow&a,const EmpRow&b){ return a.user.fullname<b.user.fullname; });
    } else if(s_mg->empSort==1){     // newest first (file order is oldest→newest)
        std::reverse(s_mg->emps.begin(),s_mg->emps.end());
    }
    // empSort==2 → created order (oldest first) = natural load order, no change
}
// ====================== employee details modal ==============================
// v1.68.0 PERF — cached card drop-shadow for modal popups.
// The seven management dialogs each draw gpShadow(spread 22) → 11 GDI+ path
// fills per WM_PAINT; mgdProc/mrProc repaint on WM_MOUSEMOVE hover, so those
// 11 fills ran per mouse-move → FPS drop. The shadow depends only on
// (window size, card rect, dark, rad, spread, alpha) — all static between
// paints unless the theme/size changes — so it is rendered once into a
// transparent backing DC and blitted on subsequent paints. Only one modal is
// open at a time, so a single shared cache suffices.
static HDC        s_mgShDC  = NULL;
static HBITMAP    s_mgShBmp = NULL, s_mgShOld = NULL;
static int        s_mgShW=0, s_mgShH=0, s_mgShDark=-1, s_mgShRad=0, s_mgShSp=0, s_mgShA=0;
static RECT       s_mgShCard{};

static void mgFreeShadowCache(){
    if(s_mgShDC && s_mgShOld) SelectObject(s_mgShDC,s_mgShOld);
    if(s_mgShBmp){ DeleteObject(s_mgShBmp); s_mgShBmp=NULL; }
    if(s_mgShDC){ DeleteDC(s_mgShDC); s_mgShDC=NULL; }
    s_mgShW=s_mgShH=0; s_mgShDark=-1;
}
// Render the card shadow into a cache (transparent background) once, then blit
// it onto the paint DC on every subsequent paint. Falls back to a direct
// gpShadow if allocation fails (low-resource hosts).
static void gpShadowCached(HDC ref, HDC dc, int W, int H, const RECT& card,
                           int rad, int spread, int alpha){
    bool same = s_mgShDC && s_mgShW==W && s_mgShH==H && s_mgShDark==(g_dark?1:0)
                && s_mgShRad==rad && s_mgShSp==spread && s_mgShA==alpha
                && memcmp(&s_mgShCard,&card,sizeof(RECT))==0;
    if(!same){
        mgFreeShadowCache();
        s_mgShDC=CreateCompatibleDC(ref);
        s_mgShBmp=CreateCompatibleBitmap(ref,W,H);
        if(!s_mgShDC||!s_mgShBmp){ mgFreeShadowCache();
            gpShadow(dc,card,rad,spread,alpha); return; }
        s_mgShOld=(HBITMAP)SelectObject(s_mgShDC,s_mgShBmp);
        // Clear the cache to the page scrim colour + scrim wash, then render
        // the card shadow once. Subsequent paints just blit this surface (the
        // 11 GDI+ path fills run once, not per hover repaint). The blit covers
        // the same scrim pixels the dialog already painted, so it is seamless.
        RECT all={0,0,W,H};
        { HBRUSH sb=CreateSolidBrush(g_dark?RGB(6,9,14):RGB(28,36,48));
          FillRect(s_mgShDC,&all,sb); DeleteObject(sb); }
        gpFillAlpha(s_mgShDC,all,0,g_dark?RGB(0,0,0):RGB(20,28,40),120);
        gpShadow(s_mgShDC,card,rad,spread,alpha);
        s_mgShW=W; s_mgShH=H; s_mgShDark=g_dark?1:0; s_mgShRad=rad;
        s_mgShSp=spread; s_mgShA=alpha; s_mgShCard=card;
    }
    BitBlt(dc,0,0,W,H,s_mgShDC,0,0,SRCCOPY);
}

#define MGD_CLASS L"AzEmpDetail"
struct DetailState { EmpRow row; int hot; int paper; };
static HWND s_mgd=NULL; static DetailState* s_dt=NULL;

static int mgdCardW(){ return S(740); }
static int mgdCardH(){ return S(660); }
static RECT mgdCard(HWND h){
    RECT rc; GetClientRect(h,&rc);
    int w=mgdCardW(),hh=mgdCardH();
    RECT c={(rc.right-w)/2,(rc.bottom-hh)/2,(rc.right+w)/2,(rc.bottom+hh)/2};
    return c;
}

// render the personnel card onto a DC (preview AND printer share this)
static void renderPersonnelCard(HDC dc, RECT area, const EmpRow& r, bool forPrint){
    SetBkMode(dc,TRANSPARENT);
    COLORREF ink = forPrint?RGB(0,0,0):g_theme.text;
    COLORREF dim = forPrint?RGB(90,90,90):g_theme.textDim;
    COLORREF acc = forPrint?RGB(20,50,100):g_theme.accent;
    // border
    { HPEN p=CreatePen(PS_SOLID,forPrint?2:1,acc); HGDIOBJ o=SelectObject(dc,p);
      HGDIOBJ ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
      Rectangle(dc,area.left,area.top,area.right,area.bottom);
      SelectObject(dc,ob); SelectObject(dc,o); DeleteObject(p); }
    int pad=S(20);
    // title
    SelectObject(dc,g_fTitle); SetTextColor(dc,acc);
    RECT tr={area.left+pad,area.top+pad,area.right-pad,area.top+pad+S(40)};
    DrawTextW(dc,(std::wstring(L"شناسنامهٔ پرسنلی — درمانگاه ")+APP_NAME_W).c_str(),-1,&tr,
        DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    // photo box top-left
    RECT ph={area.left+pad, area.top+pad+S(50), area.left+pad+S(120), area.top+pad+S(200)};
    { HPEN p=CreatePen(PS_DOT,1,dim); HGDIOBJ o=SelectObject(dc,p);
      HGDIOBJ ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
      Rectangle(dc,ph.left,ph.top,ph.right,ph.bottom);
      SelectObject(dc,ob); SelectObject(dc,o); DeleteObject(p); }
    SelectObject(dc,g_fSmall); SetTextColor(dc,dim);
    DrawTextW(dc,L"عکس پرسنلی",-1,&ph,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    // info lines (right side)
    int rx=area.right-pad, infoTop=area.top+pad+S(56);
    int lh=S(30); int y=infoTop;
    auto line=[&](const wchar_t* lbl, const std::wstring& val, bool bold){
        SelectObject(dc, bold?g_fUIB:g_fUI); SetTextColor(dc,ink);
        std::wstring s=std::wstring(lbl)+L"  "+ (val.empty()?L"—":val);
        RECT lr={area.left+S(150),y,rx,y+lh};
        DrawTextW(dc,s.c_str(),-1,&lr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        y+=lh;
    };
    line(L"نام و نام خانوادگی:", r.user.fullname, true);
    line(L"نام کاربری:",        r.user.username, false);
    line(L"کد پرسنلی:",         (r.prof.empId.empty()?L"—":toFaDigits(r.prof.empId)), false);
    line(L"شناسه یکتا:",        (r.prof.uniqueId.empty()?L"—":toFaDigits(r.prof.uniqueId)), false);
    line(L"کد ملی:",            toFaDigits(r.prof.nationalId), false);
    line(L"نام پدر:",           r.prof.fatherName, false);
    line(L"سمت:",               (r.prof.position.empty()?L"—":r.prof.position), false);
    line(L"موبایل:",            toFaDigits(r.prof.mobile), false);
    line(L"تلفن ثابت:",         toFaDigits(r.prof.landline), false);
    line(L"بخش:",               (r.deptName.empty()?r.user.dept:r.deptName), false);
    line(L"شیفت کاری:",         (r.prof.shiftFrom.empty()?L"—":(r.prof.shiftFrom+L" تا "+r.prof.shiftTo)), false);
    line(L"ساعات کاری هفتگی:",  (r.prof.workHours.empty()?L"—":toFaDigits(r.prof.workHours)), false);
    line(L"تاریخ استخدام:",     (r.prof.hireDate.empty()?L"—":toFaDigits(r.prof.hireDate)), false);
    line(L"نوع دسترسی:",        (r.user.role==0?L"پذیرش":r.user.role==1?L"مدیریت":L"مدیر سامانه"), false);
    line(L"وضعیت:",             (r.online?L"برخط (آنلاین)":L"غیرفعال"), false);
    // address (wrapped)
    SelectObject(dc,g_fUI); SetTextColor(dc,ink);
    RECT ar={area.left+pad,y+S(6),rx,y+S(70)};
    DrawTextW(dc,(L"آدرس:  "+(r.prof.address.empty()?std::wstring(L"—"):r.prof.address)).c_str(),
        -1,&ar,DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
    // national-card photo box bottom
    RECT nc={area.left+pad, area.bottom-pad-S(110), area.left+pad+S(220), area.bottom-pad};
    { HPEN p=CreatePen(PS_DOT,1,dim); HGDIOBJ o=SelectObject(dc,p);
      HGDIOBJ ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
      Rectangle(dc,nc.left,nc.top,nc.right,nc.bottom);
      SelectObject(dc,ob); SelectObject(dc,o); DeleteObject(p); }
    SelectObject(dc,g_fSmall); SetTextColor(dc,dim);
    DrawTextW(dc,L"تصویر کارت ملی",-1,&nc,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
}

static bool printPersonnelCard(HWND owner, const EmpRow& r, int paper){
    std::wstring prn=getSetting(L"printer_name",L"");
    if(prn.empty()){ wchar_t def[256]={0}; DWORD sz=256;
        if(GetDefaultPrinterW(def,&sz)) prn=def; }
    HDC dc = prn.empty()?NULL:CreateDCW(L"WINSPOOL",prn.c_str(),NULL,NULL);
    if(!dc){
        PRINTDLGW pd={0}; pd.lStructSize=sizeof(pd); pd.hwndOwner=owner;
        pd.Flags=PD_RETURNDC|PD_NOPAGENUMS|PD_NOSELECTION;
        if(!PrintDlgW(&pd)) return false; dc=pd.hDC;
    }
    if(!dc) return false;
    DOCINFOW di={sizeof(di)}; di.lpszDocName=L"درمان پلاس — شناسنامهٔ پرسنلی";
    if(StartDocW(dc,&di)<=0){ DeleteDC(dc); return false; }
    StartPage(dc);
    int horz=GetDeviceCaps(dc,HORZRES), vert=GetDeviceCaps(dc,VERTRES);
    int dpiY=GetDeviceCaps(dc,LOGPIXELSY);
    int marg=dpiY/2;
    RECT area={marg,marg,horz-marg, (paper==1)? (vert/2) : (vert-marg)};
    renderPersonnelCard(dc,area,r,true);
    EndPage(dc); EndDoc(dc); DeleteDC(dc);
    return true;
}

static void mgdClose(){ if(s_mgd&&IsWindow(s_mgd)){ HWND v=s_mgd; s_mgd=NULL; DestroyWindow(v);} }
static LRESULT CALLBACK mgdProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:{ PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        { HBRUSH sb=CreateSolidBrush(g_dark?RGB(6,9,14):RGB(28,36,48));
          FillRect(dc,&rc,sb); DeleteObject(sb); }
        gpFillAlpha(dc,rc,0,g_dark?RGB(0,0,0):RGB(20,28,40),120);
        RECT c=mgdCard(h);
        // v1.68.0: cached card shadow (11 GDI+ path fills) instead of per-paint.
        gpShadowCached(dc0,dc,rc.right,rc.bottom,c,S(20),S(22),80);
        gpGradRoundRectBg(dc,c,S(18),g_theme.surfaceTop,g_theme.surface,g_theme.border,
            g_dark?RGB(6,9,14):RGB(28,36,48));
        SetBkMode(dc,TRANSPARENT);
        // close
        { RECT cb={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)};
          if(s_dt&&s_dt->hot==1) gpRoundRect(dc,cb,S(8),g_theme.hover,CLR_INVALID,255);
          RECT ci={cb.left+S(5),cb.top+S(5),cb.right-S(5),cb.bottom-S(5)};
          drawIcon(dc,ICO_X,ci,g_theme.text,S(2)); }
        // preview area
        RECT pv={c.left+S(24),c.top+S(54),c.right-S(24),c.bottom-S(70)};
        { HBRUSH wb=CreateSolidBrush(RGB(255,255,255)); FillRect(dc,&pv,wb); DeleteObject(wb); }
        if(s_dt) renderPersonnelCard(dc,pv,s_dt->row,false);
        // buttons
        auto btn=[&](int id,const wchar_t* t,RECT r,bool primary){
            bool hov=(s_dt&&s_dt->hot==id);
            COLORREF bg=primary?(hov?g_theme.accentHover:g_theme.accent):(hov?g_theme.hover:g_theme.surface2);
            gpRoundRect(dc,r,S(9),bg,primary?bg:g_theme.border,255);
            SetTextColor(dc,primary?g_theme.accentText:g_theme.text); SelectObject(dc,g_fUIB);
            DrawTextW(dc,t,-1,&r,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        };
        int by=c.bottom-S(58), bw=S(150);
        RECT r4={c.right-S(24)-bw,by,c.right-S(24),by+S(40)};       btn(2,L"چاپ A4",r4,true);
        RECT r5={r4.left-S(8)-bw,by,r4.left-S(8),by+S(40)};         btn(3,L"چاپ A5",r5,false);
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps); return 0; }
    case WM_MOUSEMOVE:{ POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        RECT c=mgdCard(h); int hr=0;
        RECT cb={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)};
        int by=c.bottom-S(58), bw=S(150);
        RECT r4={c.right-S(24)-bw,by,c.right-S(24),by+S(40)};
        RECT r5={r4.left-S(8)-bw,by,r4.left-S(8),by+S(40)};
        if(PtInRect(&cb,pt))hr=1; else if(PtInRect(&r4,pt))hr=2; else if(PtInRect(&r5,pt))hr=3;
        if(s_dt&&s_dt->hot!=hr){ s_dt->hot=hr; InvalidateRect(h,NULL,FALSE); } return 0; }
    case WM_LBUTTONDOWN:{ POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        RECT c=mgdCard(h);
        if(!PtInRect(&c,pt)){ mgdClose(); return 0; }
        RECT cb={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)};
        if(PtInRect(&cb,pt)){ mgdClose(); return 0; }
        int by=c.bottom-S(58), bw=S(150);
        RECT r4={c.right-S(24)-bw,by,c.right-S(24),by+S(40)};
        RECT r5={r4.left-S(8)-bw,by,r4.left-S(8),by+S(40)};
        if(PtInRect(&r4,pt)){ printPersonnelCard(h,s_dt->row,0); }
        else if(PtInRect(&r5,pt)){ printPersonnelCard(h,s_dt->row,1); }
        return 0; }
    case WM_KEYDOWN: if(w==VK_ESCAPE){ mgdClose(); return 0; } break;
    case WM_DESTROY: if(s_dt){ delete s_dt; s_dt=NULL; } s_mgd=NULL;
        if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE); return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
static void openEmpDetails(HWND owner, const EmpRow& r){
    if(s_mgd&&IsWindow(s_mgd)) mgdClose();
    static bool reg=false;
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=mgdProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=MGD_CLASS;
        RegisterClassW(&wc); reg=true; }
    RECT rc; GetClientRect(g_hFrame,&rc);
    POINT org={0,0}; ClientToScreen(g_hFrame,&org);
    s_dt=new DetailState(); s_dt->row=r; s_dt->hot=0; s_dt->paper=0;
    s_mgd=CreateWindowExW(WS_EX_TOPMOST,MGD_CLASS,L"",
        WS_POPUP|WS_VISIBLE,org.x,org.y,rc.right,rc.bottom,g_hFrame,NULL,g_hInst,NULL);
    BringWindowToTop(s_mgd); SetFocus(s_mgd);
}
// ============== «درخواست‌های تغییر تنظیمات» (settings-change requests) =======
//  v1.9.0 redesign — a clean, interactive request centre:
//    • each card shows title, detailed action, employee, network/system source,
//      time, and (if present) a preview string.
//    • clicking a card SELECTS it → a VERY THIN GREEN border (the whole card,
//      including its «جزئیات» detail area, shares this selected state).
//    • a selected request is automatically marked READ → it turns GRAY.
//    • per-card actions: تأیید (approve+apply) / رد (reject) / حذف (delete with
//      confirmation).
//    • «علامت‌گذاری همه به‌عنوان خوانده‌شده» works; the unread counter and the
//      nav badge update live and immediately.
#define MGREQ_CLASS L"AzMgReq"
//  v1.9.0: a thin green selection border colour.
static COLORREF mrGreen(){ return RGB(0x2E,0xB8,0x6B); }
struct ReqState { std::vector<SetReq> reqs; int hot; int sel; int scroll; };
static HWND s_mr=NULL; static ReqState* s_rs=NULL;
static void mrClose(){ if(s_mr&&IsWindow(s_mr)){ HWND v=s_mr; s_mr=NULL; DestroyWindow(v);} }
static RECT mrCard(HWND h){ RECT rc; GetClientRect(h,&rc);
    int w=S(780),hh=S(600);
    RECT c={(rc.right-w)/2,(rc.bottom-hh)/2,(rc.right+w)/2,(rc.bottom+hh)/2}; return c; }
static int  mrRowH(){ return S(104); }
static int  mrListTop(HWND h){ RECT c=mrCard(h); return c.top+S(62); }
static int  mrListBot(HWND h){ RECT c=mrCard(h); return c.bottom-S(60); }
static RECT mrRowRect(HWND h,int i){
    RECT c=mrCard(h); int y=mrListTop(h)+i*(mrRowH()+S(8));
    RECT r={c.left+S(20),y,c.right-S(20),y+mrRowH()}; return r; }
// per-row action buttons (LEFT side)
static RECT mrApprove(RECT row){ RECT b={row.left+S(12),row.bottom-S(40),row.left+S(98),row.bottom-S(10)}; return b; }
static RECT mrReject (RECT row){ RECT b={row.left+S(106),row.bottom-S(40),row.left+S(176),row.bottom-S(10)}; return b; }
static RECT mrDelete (RECT row){ RECT b={row.left+S(184),row.bottom-S(40),row.left+S(254),row.bottom-S(10)}; return b; }
static void mrRefreshLive(HWND h){
    if(s_rs) s_rs->reqs=loadSetReqs();
    mgRefreshReqBadge();
    InvalidateRect(h,NULL,FALSE);
    if(g_hManage) InvalidateRect(g_hManage,NULL,FALSE);
}
static LRESULT CALLBACK mrProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE: s_rs=new ReqState(); s_rs->reqs=loadSetReqs();
        s_rs->hot=-1; s_rs->sel=-1; s_rs->scroll=0; return 0;
    case WM_NCDESTROY: if(s_rs){ delete s_rs; s_rs=NULL; } s_mr=NULL;
        if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEMOVE:{ if(!s_rs) break; POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int oh=s_rs->hot; s_rs->hot=-1;
        for(int i=0;i<(int)s_rs->reqs.size();i++){ RECT r=mrRowRect(h,i);
            if(r.top>=mrListBot(h)) break;
            if(PtInRect(&r,pt)){ s_rs->hot=i; break; } }
        if(oh!=s_rs->hot) InvalidateRect(h,NULL,FALSE);
        return 0; }
    case WM_LBUTTONDOWN:{ if(!s_rs) break; POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        RECT c=mrCard(h);
        RECT cb={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)};
        if(!PtInRect(&c,pt)){ mrClose(); return 0; }
        if(PtInRect(&cb,pt)){ mrClose(); return 0; }
        // «علامت‌گذاری همه به‌عنوان خوانده‌شده»
        int by=c.bottom-S(50), bw=S(250);
        RECT mb={c.left+S(24),by,c.left+S(24)+bw,by+S(36)};
        if(PtInRect(&mb,pt)){ markSetReqsSeen(); mrRefreshLive(h); return 0; }
        // per-row hit testing
        for(int i=0;i<(int)s_rs->reqs.size();i++){
            RECT r=mrRowRect(h,i); if(r.top>=mrListBot(h)) break;
            if(!PtInRect(&r,pt)) continue;
            SetReq& q=s_rs->reqs[i];
            RECT ap=mrApprove(r), rj=mrReject(r), dl=mrDelete(r);
            if(q.status==0 && PtInRect(&ap,pt)){ setSetReqStatus(i,1,L""); mrRefreshLive(h); return 0; }
            if(q.status==0 && PtInRect(&rj,pt)){ setSetReqStatus(i,2,L""); mrRefreshLive(h); return 0; }
            if(PtInRect(&dl,pt)){
                if(MessageBoxW(h,L"این درخواست حذف شود؟",L"حذف درخواست",
                    MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2)==IDYES){
                    deleteSetReq(i); if(s_rs->sel==i) s_rs->sel=-1; mrRefreshLive(h); }
                return 0; }
            // clicking the card body (incl. details area) selects + marks read
            s_rs->sel=i;
            if(!q.seen){ markOneSetReqSeen(i); }
            mrRefreshLive(h);
            return 0;
        }
        return 0; }
    case WM_RBUTTONDOWN:{ if(!s_rs) break; POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        for(int i=0;i<(int)s_rs->reqs.size();i++){
            RECT r=mrRowRect(h,i); if(r.top>=mrListBot(h)) break;
            if(!PtInRect(&r,pt)) continue;
            s_rs->sel=i; InvalidateRect(h,NULL,FALSE);
            HMENU pm=CreatePopupMenu();
            AppendMenuW(pm,MF_STRING,1,L"ارسال به پیام‌های ذخیره‌شده");
            POINT sp=pt; ClientToScreen(h,&sp);
            int cmd=TrackPopupMenu(pm,TPM_RIGHTBUTTON|TPM_RETURNCMD,sp.x,sp.y,0,h,NULL);
            DestroyMenu(pm);
            if(cmd==1){
                SetReq& q=s_rs->reqs[i];
                std::wstring title=q.title.empty()?q.change:q.title;
                std::wstring body=title+L"\r\nاقدام: "+q.change+
                    L"\r\nکارمند: "+(q.user.empty()?std::wstring(L"—"):q.user)+
                    L"   سامانه: "+(q.system.empty()?std::wstring(L"—"):q.system)+
                    L"   زمان: "+q.time;
                if(!q.preview.empty()) body+=L"\r\nپیش‌نمایش: "+q.preview;
                pushLocalNote(g_session.user.username,body,L"");
                MessageBoxW(h,L"درخواست به «پیام‌های ذخیره‌شده» شما اضافه شد.",
                    L"پیام‌های ذخیره‌شده",MB_ICONINFORMATION|MB_OK);
                if(g_hManage) InvalidateRect(g_hManage,NULL,FALSE);
            }
            return 0;
        }
        return 0; }
    case WM_KEYDOWN: if(w==VK_ESCAPE){ mrClose(); return 0; } break;
    case WM_PAINT:{ PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        { HBRUSH sb=CreateSolidBrush(g_dark?RGB(6,9,14):RGB(28,36,48));
          FillRect(dc,&rc,sb); DeleteObject(sb); }
        gpFillAlpha(dc,rc,0,g_dark?RGB(0,0,0):RGB(20,28,40),120);
        RECT c=mrCard(h);
        gpShadowCached(dc0,dc,rc.right,rc.bottom,c,S(20),S(22),80);
        gpGradRoundRectBg(dc,c,S(18),g_theme.surfaceTop,g_theme.surface,g_theme.border,
            g_dark?RGB(6,9,14):RGB(28,36,48));
        SetBkMode(dc,TRANSPARENT);
        // close
        { RECT cb={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)};
          RECT ci={cb.left+S(5),cb.top+S(5),cb.right-S(5),cb.bottom-S(5)};
          drawIcon(dc,ICO_X,ci,g_theme.text,S(2)); }
        SelectObject(dc,g_fTitle); SetTextColor(dc,g_infoAccent);
        RECT tr={c.left+S(24),c.top+S(14),c.right-S(150),c.top+S(48)};
        DrawTextW(dc,L"درخواست‌های تغییر تنظیمات کارمندان",-1,&tr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        // live unread counter chip (top-left)
        { int unread=0; for(auto&q:s_rs->reqs) if(!q.seen) unread++;
          RECT cc={c.left+S(50),c.top+S(18),c.left+S(140),c.top+S(44)};
          gpRoundRect(dc,cc,(cc.bottom-cc.top)/2, unread>0?g_infoAccent:g_theme.surface2,
              unread>0?g_infoAccent:g_theme.border,255);
          SelectObject(dc,g_fSmall); SetTextColor(dc, unread>0?RGB(255,255,255):g_theme.textDim);
          std::wstring ct=L"خوانده‌نشده: "+toFaDigits(std::to_wstring(unread));
          DrawTextW(dc,ct.c_str(),-1,&cc,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
        // list
        if(!s_rs || s_rs->reqs.empty()){
            SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
            RECT er={c.left+S(24),mrListTop(h)+S(20),c.right-S(24),mrListTop(h)+S(60)};
            DrawTextW(dc,L"درخواستی ثبت نشده است.",-1,&er,
                DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        } else {
            for(int i=0;i<(int)s_rs->reqs.size();i++){
                RECT row=mrRowRect(h,i); if(row.top>=mrListBot(h)) break;
                SetReq& q=s_rs->reqs[i];
                bool seld=(s_rs->sel==i), hot=(s_rs->hot==i);
                // body: GRAY when read; subtle attention tint when unread.
                COLORREF body = q.seen ? blendColor(g_theme.surface,g_theme.textDim,8)
                                       : g_theme.surface2;
                COLORREF bord = seld ? mrGreen()
                              : hot  ? g_infoAccent : g_theme.border;
                gpRoundRectBg(dc,row,S(11),body,bord,g_theme.surface,255);
                // VERY THIN green selection border: draw a single hairline inset
                if(seld){ RECT in={row.left+1,row.top+1,row.right-1,row.bottom-1};
                    gpRoundRect(dc,in,S(10),CLR_INVALID,mrGreen(),255); }
                // unread dot
                if(!q.seen){ RECT dot={row.right-S(26),row.top+S(12),row.right-S(14),row.top+S(24)};
                    gpRoundRect(dc,dot,S(6),g_infoAccent,CLR_INVALID,255); }
                // status badge (approved/rejected) top-left of card
                if(q.status!=0){
                    RECT sb2={row.left+S(12),row.top+S(10),row.left+S(110),row.top+S(34)};
                    bool ok=(q.status==1);
                    gpRoundRect(dc,sb2,(sb2.bottom-sb2.top)/2, ok?mrGreen():g_theme.danger,
                        ok?mrGreen():g_theme.danger,255);
                    SelectObject(dc,g_fSmall); SetTextColor(dc,RGB(255,255,255));
                    DrawTextW(dc, ok?L"✓ تأیید شد":L"✕ رد شد",-1,&sb2,
                        DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
                }
                // title (bold)
                SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
                RECT l1={row.left+S(14),row.top+S(8),row.right-S(34),row.top+S(30)};
                std::wstring title=q.title.empty()?q.change:q.title;
                DrawTextW(dc,title.c_str(),-1,&l1,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                // detailed action
                SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
                RECT l2={row.left+S(14),row.top+S(30),row.right-S(14),row.top+S(50)};
                DrawTextW(dc,(L"اقدام: "+q.change).c_str(),-1,&l2,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                // employee • system • time
                SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
                RECT l3={row.left+S(14),row.top+S(50),row.right-S(14),row.top+S(68)};
                std::wstring meta=L"کارمند: "+(q.user.empty()?std::wstring(L"—"):q.user)+
                    L"   •   سامانه: "+(q.system.empty()?std::wstring(L"—"):q.system)+
                    L"   •   "+q.time;
                DrawTextW(dc,meta.c_str(),-1,&l3,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                // preview (if any) — right side above the action buttons
                if(!q.preview.empty()){
                    SelectObject(dc,g_fSmall); SetTextColor(dc,g_infoAccent2);
                    RECT pv={row.left+S(270),row.bottom-S(38),row.right-S(14),row.bottom-S(12)};
                    DrawTextW(dc,(L"پیش‌نمایش: "+q.preview).c_str(),-1,&pv,
                        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                }
                // action buttons (only when pending)
                if(q.status==0){
                    RECT ap=mrApprove(row), rj=mrReject(row);
                    gpRoundRect(dc,ap,S(8),mrGreen(),mrGreen(),255);
                    SelectObject(dc,g_fSmall); SetTextColor(dc,RGB(255,255,255));
                    DrawTextW(dc,L"تأیید",-1,&ap,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
                    gpRoundRect(dc,rj,S(8),g_theme.danger,g_theme.danger,255);
                    DrawTextW(dc,L"رد",-1,&rj,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
                }
                // delete (always)
                RECT dl=mrDelete(row);
                gpRoundRect(dc,dl,S(8),g_theme.surface2,g_theme.border,255);
                SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.danger);
                DrawTextW(dc,L"حذف",-1,&dl,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            }
        }
        // mark-all button
        int by=c.bottom-S(50), bw=S(250);
        RECT mb={c.left+S(24),by,c.left+S(24)+bw,by+S(36)};
        gpRoundRect(dc,mb,S(9),g_theme.accent,g_theme.accent,255);
        SetTextColor(dc,g_theme.accentText); SelectObject(dc,g_fUIB);
        DrawTextW(dc,L"علامت‌گذاری همه به‌عنوان خوانده‌شده",-1,&mb,
            DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps); return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}
static void openSetReqs(HWND owner){
    if(s_mr&&IsWindow(s_mr)) mrClose();
    static bool reg=false;
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=mrProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=MGREQ_CLASS;
        RegisterClassW(&wc); reg=true; }
    RECT rc; GetClientRect(g_hFrame,&rc);
    POINT org={0,0}; ClientToScreen(g_hFrame,&org);
    s_mr=CreateWindowExW(WS_EX_TOPMOST,MGREQ_CLASS,L"",WS_POPUP|WS_VISIBLE,
        org.x,org.y,rc.right,rc.bottom,g_hFrame,NULL,g_hInst,NULL);
    BringWindowToTop(s_mr); SetFocus(s_mr);
}

// ============== «درخواست‌های تغییر پروفایل» (profile-change requests) ========
//  Each pending request shows old→new name and whether a photo was attached.
//  Manager can «تأیید» (approve → applies override + green cartable) or
//  «رد» (reject → red cartable with the optional reason from the textbox).
#define MGPRF_CLASS L"AzMgProf"
#define ID_PR_REASON 3101
struct ProfReqState {
    std::vector<ProfReq> reqs;
    int sel;                 // selected row (for the reason box), -1 none
    HWND eReason;
};
static HWND s_pr=NULL; static ProfReqState* s_prs=NULL;
static void prClose(){ if(s_pr&&IsWindow(s_pr)){ HWND v=s_pr; s_pr=NULL; DestroyWindow(v);} }
static RECT prCard(HWND h){ RECT rc; GetClientRect(h,&rc);
    int w=S(780),hh=S(600);
    RECT c={(rc.right-w)/2,(rc.bottom-hh)/2,(rc.right+w)/2,(rc.bottom+hh)/2}; return c; }
static int  prRowH(){ return S(84); }
static int  prListTop(HWND h){ RECT c=prCard(h); return c.top+S(64); }
static RECT prRowRect(HWND h, int i){
    RECT c=prCard(h); int y=prListTop(h)+i*(prRowH()+S(8));
    RECT r={c.left+S(20),y,c.right-S(20),y+prRowH()}; return r; }
// the two action buttons live on the LEFT side of each row
static RECT prApproveRect(RECT row){ RECT b={row.left+S(12),row.top+S(20),row.left+S(120),row.bottom-S(20)}; return b; }
static RECT prRejectRect (RECT row){ RECT b={row.left+S(128),row.top+S(20),row.left+S(220),row.bottom-S(20)}; return b; }
//  v1.9.0: a square photo thumbnail on the RIGHT edge of each profile request
//  row — clicking it enlarges the photo (system viewer, where it can be saved).
static RECT prPhotoRect(RECT row){ int s2=S(64); RECT b={row.right-S(14)-s2,row.top+(row.bottom-row.top-s2)/2,row.right-S(14),row.top+(row.bottom-row.top+s2)/2}; return b; }
//  Look up an employee's department from the user store (by full name or login).
static std::wstring prEmpDept(const std::wstring& who){
    if(who.empty()) return L"";
    for(auto& u:loadUsers())
        if(u.fullname==who || u.username==who) return u.dept;
    return L"";
}
static void prLayoutReason(HWND h){
    if(!s_prs||!s_prs->eReason) return;
    RECT c=prCard(h);
    int ry=c.bottom-S(52), rw=c.right-c.left-S(48)-S(150);
    MoveWindow(s_prs->eReason, c.left+S(24), ry, rw, S(34), TRUE);
    ShowWindow(s_prs->eReason, SW_SHOW);
}
static void prDoApprove(HWND h, int i){
    if(!s_prs || i<0 || i>=(int)s_prs->reqs.size()) return;
    if(s_prs->reqs[i].status!=0) return;
    setProfReqStatus(i, 1, L"");
    s_prs->reqs=loadProfReqs(); s_prs->sel=-1;
    SetWindowTextW(s_prs->eReason,L"");
    mgRefreshReqBadge();
    if(g_hManage) InvalidateRect(g_hManage,NULL,FALSE);
    InvalidateRect(h,NULL,FALSE);
}
static void prDoReject(HWND h, int i){
    if(!s_prs || i<0 || i>=(int)s_prs->reqs.size()) return;
    if(s_prs->reqs[i].status!=0) return;
    wchar_t rb[512]={0}; if(s_prs->eReason) GetWindowTextW(s_prs->eReason,rb,512);
    setProfReqStatus(i, 2, trim(rb));   // reason may be empty
    s_prs->reqs=loadProfReqs(); s_prs->sel=-1;
    SetWindowTextW(s_prs->eReason,L"");
    mgRefreshReqBadge();
    if(g_hManage) InvalidateRect(g_hManage,NULL,FALSE);
    InvalidateRect(h,NULL,FALSE);
}
static LRESULT CALLBACK prProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE: {
        s_prs=new ProfReqState(); s_prs->reqs=loadProfReqs(); s_prs->sel=-1;
        DWORD es=WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL;
        s_prs->eReason=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_PR_REASON,g_hInst,0);
        SendMessageW(s_prs->eReason,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        SendMessageW(s_prs->eReason,EM_SETCUEBANNER,TRUE,(LPARAM)L"دلیل رد (اختیاری)");
        prLayoutReason(h);
        return 0; }
    case WM_NCDESTROY: if(s_prs){ delete s_prs; s_prs=NULL; } s_pr=NULL;
        if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_CTLCOLOREDIT:{ HDC dc=(HDC)w; SetTextColor(dc,g_theme.inputText);
        SetBkColor(dc,g_theme.inputBg); return (LRESULT)g_brInput; }
    case WM_KEYDOWN: if(w==VK_ESCAPE){ prClose(); return 0; } break;
    case WM_LBUTTONDOWN:{ POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        RECT c=prCard(h);
        RECT cb={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)};
        if(!PtInRect(&c,pt)){ prClose(); return 0; }
        if(PtInRect(&cb,pt)){ prClose(); return 0; }
        if(s_prs){
            for(int i=0;i<(int)s_prs->reqs.size();i++){
                if(s_prs->reqs[i].status!=0) continue;
                RECT row=prRowRect(h,i);
                if(row.top>c.bottom-S(80)) break;
                RECT ab=prApproveRect(row), rb=prRejectRect(row);
                if(PtInRect(&ab,pt)){ prDoApprove(h,i); return 0; }
                if(PtInRect(&rb,pt)){ s_prs->sel=i; prDoReject(h,i); return 0; }
                // photo thumbnail → enlarge / download in the system viewer
                RECT prr=prPhotoRect(row);
                std::wstring photo = !s_prs->reqs[i].newPhoto.empty()
                                   ? s_prs->reqs[i].newPhoto : s_prs->reqs[i].oldPhoto;
                if(!photo.empty() && PtInRect(&prr,pt)){
                    ShellExecuteW(h,L"open",photo.c_str(),NULL,NULL,SW_SHOWNORMAL);
                    s_prs->sel=i; InvalidateRect(h,NULL,FALSE); return 0;
                }
                if(PtInRect(&row,pt)){ s_prs->sel=i; SetFocus(s_prs->eReason);
                    InvalidateRect(h,NULL,FALSE); return 0; }
            }
        }
        return 0; }
    case WM_PAINT:{ PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        { HBRUSH sb=CreateSolidBrush(g_dark?RGB(6,9,14):RGB(28,36,48));
          FillRect(dc,&rc,sb); DeleteObject(sb); }
        gpFillAlpha(dc,rc,0,g_dark?RGB(0,0,0):RGB(20,28,40),120);
        RECT c=prCard(h);
        gpShadowCached(dc0,dc,rc.right,rc.bottom,c,S(20),S(22),80);
        gpGradRoundRectBg(dc,c,S(18),g_theme.surfaceTop,g_theme.surface,g_theme.border,
            g_dark?RGB(6,9,14):RGB(28,36,48));
        SetBkMode(dc,TRANSPARENT);
        { RECT cb={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)};
          RECT ci={cb.left+S(5),cb.top+S(5),cb.right-S(5),cb.bottom-S(5)};
          drawIcon(dc,ICO_X,ci,g_theme.text,S(2)); }
        SelectObject(dc,g_fTitle); SetTextColor(dc,g_infoAccent);
        RECT tr={c.left+S(24),c.top+S(16),c.right-S(54),c.top+S(50)};
        DrawTextW(dc,L"درخواست‌های تغییر پروفایل کارمندان",-1,&tr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        // build pending list count
        int pending=0; if(s_prs) for(auto&q:s_prs->reqs) if(q.status==0) pending++;
        if(!s_prs || pending==0){
            SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
            RECT er={c.left+S(24),prListTop(h),c.right-S(24),prListTop(h)+S(40)};
            DrawTextW(dc,L"درخواست در انتظار تأییدی وجود ندارد.",-1,&er,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            // still show recently processed (history) below, dimmed
        }
        if(s_prs){
            for(int i=0;i<(int)s_prs->reqs.size();i++){
                ProfReq& q=s_prs->reqs[i];
                if(q.status!=0) continue;   // only pending in the actionable list
                RECT row=prRowRect(h,i);
                if(row.top>c.bottom-S(80)) break;
                bool selected=(s_prs->sel==i);
                gpRoundRectBg(dc,row,S(10),selected?g_theme.surface2:g_theme.surface,
                    selected?g_theme.accent:g_infoAccent, g_theme.surface,255);
                // photo thumbnail (RIGHT edge) — prefer the NEW photo, else old.
                std::wstring photo = !q.newPhoto.empty()? q.newPhoto : q.oldPhoto;
                int txtR = row.right-S(14);
                if(!photo.empty()){
                    RECT pr=prPhotoRect(row);
                    gpRoundRectBg(dc,pr,S(8),g_theme.surface2,g_infoAccent,g_theme.surface,255);
                    if(!gpDrawImageFileCircle(dc,photo,pr)){
                        RECT ii={pr.left+S(16),pr.top+S(16),pr.right-S(16),pr.bottom-S(16)};
                        drawIcon(dc,ICO_USER,ii,g_theme.textDim,S(2));
                    }
                    // tiny "enlarge" hint glyph
                    SelectObject(dc,g_fSmall); SetTextColor(dc,g_infoAccent2);
                    RECT eh={pr.left,pr.bottom+S(1),pr.right,pr.bottom+S(15)};
                    DrawTextW(dc,L"بزرگ‌نمایی",-1,&eh,DT_CENTER|DT_SINGLELINE|DT_NOPREFIX);
                    txtR = pr.left-S(10);
                }
                int txtL = row.left+S(230);
                // text (RIGHT side)
                SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
                RECT l1={txtL,row.top+S(6),txtR,row.top+S(28)};
                std::wstring head=L"کارمند: "+(q.user.empty()?std::wstring(L"—"):q.user);
                std::wstring dep=prEmpDept(q.user);
                if(!dep.empty()) head+=L"   •   بخش: "+dep;
                DrawTextW(dc,head.c_str(),-1,&l1,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
                RECT lm={txtL,row.top+S(28),txtR,row.top+S(46)};
                std::wstring meta=L"سامانه: "+systemSourceName()+L"   •   "+q.time;
                DrawTextW(dc,meta.c_str(),-1,&lm,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
                RECT l2={txtL,row.top+S(46),txtR,row.top+S(68)};
                std::wstring nm=L"نام: "+q.oldName+L"  ←  "+q.newName;
                DrawTextW(dc,nm.c_str(),-1,&l2,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                RECT l3={txtL,row.top+S(66),txtR,row.bottom-S(6)};
                std::wstring ph = q.newPhoto.empty()? std::wstring(L"عکس: بدون تغییر")
                                                    : std::wstring(L"عکس: جدید پیوست شد (برای بزرگ‌نمایی/دانلود کلیک کنید)");
                DrawTextW(dc,ph.c_str(),-1,&l3,
                    DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                // action buttons (LEFT)
                RECT ab=prApproveRect(row);
                gpRoundRect(dc,ab,S(8),g_theme.success,g_theme.success,255);
                SetTextColor(dc,RGB(255,255,255)); SelectObject(dc,g_fUIB);
                DrawTextW(dc,L"تأیید",-1,&ab,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
                RECT rb=prRejectRect(row);
                gpRoundRect(dc,rb,S(8),g_theme.danger,g_theme.danger,255);
                SetTextColor(dc,RGB(255,255,255));
                DrawTextW(dc,L"رد",-1,&rb,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            }
        }
        // reason hint label (above the textbox, LEFT-aligned in RTL = right edge)
        SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
        RECT hr={c.left+S(24),c.bottom-S(80),c.right-S(24),c.bottom-S(58)};
        DrawTextW(dc,L"برای «رد»، ابتدا ردیف را انتخاب کنید و در صورت تمایل دلیل را بنویسید (اختیاری).",-1,&hr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps); return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}
static void openProfReqs(HWND owner){
    if(s_pr&&IsWindow(s_pr)) prClose();
    static bool reg=false;
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=prProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=MGPRF_CLASS;
        RegisterClassW(&wc); reg=true; }
    RECT rc; GetClientRect(g_hFrame,&rc);
    POINT org={0,0}; ClientToScreen(g_hFrame,&org);
    s_pr=CreateWindowExW(WS_EX_TOPMOST,MGPRF_CLASS,L"",WS_POPUP|WS_VISIBLE,
        org.x,org.y,rc.right,rc.bottom,g_hFrame,NULL,g_hInst,NULL);
    BringWindowToTop(s_pr); SetFocus(s_pr);
}

// ====================== «پیام به کارمندان» compose modal =====================
//  v1.8.0 full redesign:
//    • live SEARCH box (by id / unique-id / name) with suggestion list
//    • department filter chips (multi-select, combinable) for message routing
//    • severity (عادی/فوری/بحرانی)
//    • MEDIA attachment (image / video / gif / jpg / png / word / any) — copied
//      into the local attachments store and sent with the message so the
//      recipient can download it; content is preserved permanently.
#define MGMSG_CLASS L"AzMgMsg"
#define MM_E_SEARCH 3201
#define MM_E_TEXT   3202
#define MM_R_NORMAL 3203
#define MM_R_URGENT 3204
#define MM_R_CRIT   3205
#define MM_B_SEND   3206
#define MM_B_CANCEL 3207
#define MM_B_ATTACH 3208
#define MM_LB_SUG   3209
#define MM_B_SAVE   3210
#define MM_B_VIEWEMP 3211   // «مشاهدهٔ کارمندان» toggle for the right-side panel
#define MM_E_EMPSRCH 3212   // employee-panel search box
struct MsgState {
    HWND eSearch, eText, rNormal, rUrgent, rCrit, bSend, bCancel, bAttach, lbSug, bSave;
    HWND bViewEmp, eEmpSrch;          // employee panel toggle + its search box
    std::vector<User> users;          // all users (recipient pool)
    std::vector<EmpProfile> profs;    // parallel profiles (for id/uid match)
    std::vector<int> matchIdx;        // current search matches → users index
    std::vector<DeptCat> depts;       // dept filter chips
    std::vector<bool> deptOn;         // which dept chips are active
    int selUser;                      // chosen single recipient (-1 = use depts/all)
    std::wstring attachPath;          // local copied attachment path
    std::wstring attachName;          // display name
    bool sendToSaved;                 // also archive to saved messages?
    // right-side employee panel (CLOSED by default)
    bool empPanelOpen;
    std::wstring empQuery;            // filter text for the panel
    int empHot;                       // hot-tracked panel row (-1 none)
    int empScroll;                    // vertical scroll offset (px)
    // §E: PRow carries a code too — for section headers it is the section code,
    // for employees it is the personnel code (empId). kind 0=dept header,1=emp.
    struct PRow { int kind; int idx; std::wstring name; std::wstring code; };
    std::vector<PRow> empRows;
};
static HWND s_mm=NULL; static MsgState* s_ms=NULL;
static void mmClose(){ if(s_mm&&IsWindow(s_mm)){ HWND v=s_mm; s_mm=NULL; DestroyWindow(v);} }
static int  mmPanelW(){ return S(280); }
static RECT mmCardOuter(HWND h){ RECT rc; GetClientRect(h,&rc);
    bool pan = s_ms && s_ms->empPanelOpen;
    int w=S(640)+(pan?mmPanelW()+S(12):0), hh=S(620);
    RECT c={(rc.right-w)/2,(rc.bottom-hh)/2,(rc.right+w)/2,(rc.bottom+hh)/2}; return c; }
// the compose card (right part) — fixed width; the panel sits to its LEFT.
static RECT mmCard(HWND h){ RECT o=mmCardOuter(h);
    RECT c={o.right-S(640),o.top,o.right,o.bottom}; return c; }
static RECT mmPanelRect(HWND h){ RECT o=mmCardOuter(h);
    RECT p={o.left,o.top,o.left+mmPanelW(),o.bottom}; return p; }
static int  mmPanelListTop(HWND h){ RECT p=mmPanelRect(h); return p.top+S(96); }
static int  mmPanelRowH(){ return S(40); }
// dept chip geometry (left→right wrap inside the card)
static int  mmChipsTop(HWND h){ RECT c=mmCard(h); return c.top+S(150); }
static RECT mmChipRect(HWND h, int i){
    RECT c=mmCard(h); int x=c.right-S(28), y=mmChipsTop(h);
    int cw=S(150), ch=S(30), gap=S(8), perRow=(c.right-c.left-S(56))/(cw+gap);
    if(perRow<1) perRow=1;
    int row=i/perRow, col=i%perRow;
    int rx=x-(col+1)*cw-col*gap;
    RECT r={rx,y+row*(ch+gap),rx+cw,y+row*(ch+gap)+ch}; return r;
}
static int mmChipRows(HWND h){
    if(!s_ms||s_ms->depts.empty()) return 0;
    RECT c=mmCard(h); int cw=S(150),gap=S(8);
    int perRow=(c.right-c.left-S(56))/(cw+gap); if(perRow<1) perRow=1;
    return ((int)s_ms->depts.size()+perRow-1)/perRow;
}
static void mmRunSearch(HWND h){
    if(!s_ms) return;
    wchar_t buf[128]={0}; GetWindowTextW(s_ms->eSearch,buf,128);
    std::wstring q=trim(buf);
    s_ms->matchIdx.clear();
    SendMessageW(s_ms->lbSug,LB_RESETCONTENT,0,0);
    if(q.empty()){ ShowWindow(s_ms->lbSug,SW_HIDE); return; }
    std::wstring ql=q; for(auto&ch:ql) ch=towlower(ch);
    for(size_t i=0;i<s_ms->users.size() && s_ms->matchIdx.size()<8;i++){
        std::wstring nm=s_ms->users[i].fullname; for(auto&ch:nm) ch=towlower(ch);
        std::wstring un=s_ms->users[i].username; for(auto&ch:un) ch=towlower(ch);
        const EmpProfile& p=s_ms->profs[i];
        bool hit = nm.find(ql)!=std::wstring::npos || un.find(ql)!=std::wstring::npos
                || p.empId.find(q)!=std::wstring::npos
                || p.uniqueId.find(q)!=std::wstring::npos
                || p.nationalId.find(q)!=std::wstring::npos;
        if(hit){
            s_ms->matchIdx.push_back((int)i);
            std::wstring it=(s_ms->users[i].fullname.empty()?s_ms->users[i].username
                            :s_ms->users[i].fullname)+L"  (@"+s_ms->users[i].username+L")";
            if(!p.empId.empty()) it+=L"  ["+p.empId+L"]";
            SendMessageW(s_ms->lbSug,LB_ADDSTRING,0,(LPARAM)it.c_str());
        }
    }
    ShowWindow(s_ms->lbSug, s_ms->matchIdx.empty()?SW_HIDE:SW_SHOW);
}
// Rebuild the grouped employee list for the right-side panel:
//   • grouped by department; ONLINE first (green dot) then OFFLINE (gray dot)
//   • honours the panel search box (name / username / dept).
//  §E (v1.10.0): grouped by SECTION, ONLINE first; each section header shows
//  the section CODE, each employee row shows the personnel CODE. The list is
//  CAPPED at 20 sections and 20 employees per section (lazy virtualization:
//  rows beyond the cap are summarised with a «… و N مورد دیگر» line so the
//  panel stays light on weak hardware and never balloons). Online presence is
//  preserved; sorting keeps online employees at the top of each section.
#define MM_MAX_SECTIONS 20
#define MM_MAX_EMPS_PER_SECTION 20
static void mmBuildEmpRows(HWND h){
    if(!s_ms) return;
    s_ms->empRows.clear();
    std::wstring q=s_ms->empQuery; for(auto&ch:q) ch=towlower(ch);
    auto deptNameOf=[&](int i)->std::wstring{
        std::wstring did=s_ms->profs[i].deptId;
        for(auto&d:s_ms->depts) if(d.id==did) return d.name;
        return s_ms->users[i].dept.empty()?L"بدون بخش":s_ms->users[i].dept;
    };
    auto deptCodeOf=[&](const std::wstring& name)->std::wstring{
        for(auto&d:s_ms->depts) if(d.name==name) return d.id;
        return L"\u2014";   // —
    };
    // build the section ordering, capped at MM_MAX_SECTIONS
    std::vector<std::wstring> groups;
    for(auto&d:s_ms->depts){ if((int)groups.size()>=MM_MAX_SECTIONS) break;
        groups.push_back(d.name); }
    groups.push_back(L"بدون بخش");
    auto matches=[&](int i)->bool{
        if(q.empty()) return true;
        std::wstring nm=s_ms->users[i].fullname; for(auto&ch:nm) ch=towlower(ch);
        std::wstring un=s_ms->users[i].username; for(auto&ch:un) ch=towlower(ch);
        std::wstring dn=deptNameOf(i);            for(auto&ch:dn) ch=towlower(ch);
        std::wstring cd=s_ms->profs[i].empId;     for(auto&ch:cd) ch=towlower(ch);
        return nm.find(q)!=std::wstring::npos||un.find(q)!=std::wstring::npos
             ||dn.find(q)!=std::wstring::npos||cd.find(q)!=std::wstring::npos;
    };
    int sectionsShown=0;
    for(auto&g:groups){
        if(sectionsShown>=MM_MAX_SECTIONS) break;
        std::vector<int> on, off;
        for(int i=0;i<(int)s_ms->users.size();i++){
            if(deptNameOf(i)!=g) continue;
            if(!matches(i)) continue;
            if(isUserOnline(s_ms->users[i].username)) on.push_back(i);
            else off.push_back(i);
        }
        if(on.empty()&&off.empty()) continue;
        std::wstring code = (g==L"بدون بخش") ? std::wstring(L"\u2014") : deptCodeOf(g);
        s_ms->empRows.push_back({0,-1,g,code});
        sectionsShown++;
        // ONLINE first, then OFFLINE — capped per section.
        int emitted=0; int totalInSection=(int)on.size()+(int)off.size();
        auto emit=[&](const std::vector<int>& v){
            for(int i:v){ if(emitted>=MM_MAX_EMPS_PER_SECTION) return;
                s_ms->empRows.push_back({1,i,L"",s_ms->profs[i].empId}); emitted++; }
        };
        emit(on); emit(off);
        if(totalInSection>emitted){
            wchar_t mb[64]; swprintf(mb,64,L"\u2026 \u0648 %d \u0645\u0648\u0631\u062f \u062f\u06cc\u06af\u0631",
                totalInSection-emitted);
            s_ms->empRows.push_back({2,-1,mb,L""});   // kind 2 = "more" summary
        }
    }
}
static void mmLayout(HWND h){
    if(!s_ms) return; RECT c=mmCard(h);
    int x=c.left+S(28), r=c.right-S(28), y=c.top+S(64), fh=S(34);
    // search box (full width)
    MoveWindow(s_ms->eSearch, x, y, r-x, fh, TRUE);
    // suggestion list floats just under the search box
    MoveWindow(s_ms->lbSug, x, y+fh+S(2), r-x, S(140), TRUE);
    // dept chips occupy mmChipsTop region (owner-drawn) — push the rest below
    int rows=mmChipRows(h); int chipsH = rows>0 ? rows*(S(30)+S(8))+S(8) : 0;
    int yy = mmChipsTop(h)+chipsH+S(28);
    // severity radios
    MoveWindow(s_ms->rNormal, r-S(110), yy, S(110), fh, TRUE);
    MoveWindow(s_ms->rUrgent, r-S(110)-S(120), yy, S(120), fh, TRUE);
    MoveWindow(s_ms->rCrit,   r-S(110)-S(240), yy, S(120), fh, TRUE);
    // attach button (left of radios)
    MoveWindow(s_ms->bAttach, x, yy, S(170), fh, TRUE);
    yy+=fh+S(12);
    // message body
    int bodyBottom=c.bottom-S(96);
    MoveWindow(s_ms->eText, x, yy, r-x, bodyBottom-yy, TRUE);
    // "send to saved" toggle (shown only when saved messages enabled)
    MoveWindow(s_ms->bSave, x, c.bottom-S(86), S(260), S(30), TRUE);
    ShowWindow(s_ms->bSave, savedMsgsEnabled()?SW_SHOW:SW_HIDE);
    // action buttons
    int by=c.bottom-S(50), bw=S(150);
    MoveWindow(s_ms->bSend,   r-bw,         by, bw, S(38), TRUE);
    MoveWindow(s_ms->bCancel, r-bw-S(8)-bw, by, bw, S(38), TRUE);
}
static void mmPickAttachment(HWND h){
    if(!s_ms) return;
    wchar_t file[1024]={0};
    OPENFILENAMEW ofn={0}; ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=h;
    ofn.lpstrFilter=L"همهٔ رسانه‌ها\0*.jpg;*.jpeg;*.png;*.gif;*.bmp;*.mp4;*.avi;*.mov;*.mkv;*.doc;*.docx;*.pdf\0"
                    L"تصاویر (JPG/PNG/GIF)\0*.jpg;*.jpeg;*.png;*.gif;*.bmp\0"
                    L"ویدئو\0*.mp4;*.avi;*.mov;*.mkv\0"
                    L"اسناد (Word/PDF)\0*.doc;*.docx;*.pdf\0"
                    L"همهٔ فایل‌ها\0*.*\0\0";
    ofn.lpstrFile=file; ofn.nMaxFile=1024;
    ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
    if(GetOpenFileNameW(&ofn)){
        std::wstring stored=copyAttachmentLocal(file);
        if(!stored.empty()){
            s_ms->attachPath=stored;
            size_t sl=stored.find_last_of(L"\\/");
            s_ms->attachName=(sl==std::wstring::npos)?stored:stored.substr(sl+1);
            SetWindowTextW(s_ms->bAttach,(L"📎 "+s_ms->attachName).c_str());
        } else {
            MessageBoxW(h,L"کپی پیوست ناموفق بود.",L"پیام",MB_OK|MB_ICONWARNING);
        }
        InvalidateRect(h,NULL,FALSE);
    }
}
static void mmDoSend(HWND h){
    if(!s_ms) return;
    wchar_t buf[4096]={0}; GetWindowTextW(s_ms->eText,buf,4096);
    std::wstring text=trim(buf);
    if(text.empty()){
        MessageBoxW(h,L"متن پیام را وارد کنید.",L"پیام",MB_OK|MB_ICONINFORMATION); return; }
    int type=KMSG_NORMAL;
    if(SendMessageW(s_ms->rUrgent,BM_GETCHECK,0,0)==BST_CHECKED) type=KMSG_URGENT;
    else if(SendMessageW(s_ms->rCrit,BM_GETCHECK,0,0)==BST_CHECKED) type=KMSG_CRITICAL;
    std::wstring body=text;
    if(!s_ms->attachPath.empty())
        body += L"\n\n📎 پیوست: "+s_ms->attachName+L"\n["+s_ms->attachPath+L"]";
    std::wstring from=g_session.user.fullname.empty()?g_session.user.username
                                                     :g_session.user.fullname;
    // routing: a single chosen recipient overrides; else union of selected
    // departments; else broadcast to everyone (*).
    int sent=0;
    if(s_ms->selUser>=0 && s_ms->selUser<(int)s_ms->users.size()){
        pushMessageT(from,s_ms->users[s_ms->selUser].username,body,type); sent=1;
    } else {
        bool anyDept=false;
        for(size_t d=0;d<s_ms->depts.size();d++) if(s_ms->deptOn[d]) anyDept=true;
        if(anyDept){
            for(size_t i=0;i<s_ms->users.size();i++){
                std::wstring did=s_ms->profs[i].deptId;
                for(size_t d=0;d<s_ms->depts.size();d++){
                    if(s_ms->deptOn[d] && did==s_ms->depts[d].id){
                        pushMessageT(from,s_ms->users[i].username,body,type); sent++;
                        break;
                    }
                }
            }
            if(sent==0){ MessageBoxW(h,L"در بخش‌های انتخاب‌شده کارمندی یافت نشد.",
                L"پیام",MB_OK|MB_ICONINFORMATION); return; }
        } else {
            pushMessageT(from,L"*",body,type); sent=1;   // broadcast
        }
    }
    // optionally archive to saved messages
    if(savedMsgsEnabled() &&
       SendMessageW(s_ms->bSave,BM_GETCHECK,0,0)==BST_CHECKED){
        std::wstring to = s_ms->selUser>=0 ? s_ms->users[s_ms->selUser].username : L"*";
        pushSavedMsg(from,to,text,type,s_ms->attachPath);
    }
    MessageBoxW(h,(L"پیام برای "+toFaDigits(std::to_wstring(sent))+
                  L" گیرنده ارسال شد.").c_str(),
                L"ارسال موفق",MB_OK|MB_ICONINFORMATION);
    mmClose();
    if(g_hManage) InvalidateRect(g_hManage,NULL,TRUE);
}
static LRESULT CALLBACK mmProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE:{
        s_ms=new MsgState(); s_ms->selUser=-1; s_ms->sendToSaved=false;
        s_ms->empPanelOpen=false; s_ms->empHot=-1; s_ms->empScroll=0;
        DWORD es=WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL;
        s_ms->eSearch=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MM_E_SEARCH,g_hInst,0);
        SendMessageW(s_ms->eSearch,EM_SETCUEBANNER,TRUE,
            (LPARAM)L"جستجوی گیرنده: نام / نام‌کاربری / کد پرسنلی / شناسه یکتا");
        s_ms->lbSug=CreateWindowExW(0,L"LISTBOX",L"",
            WS_CHILD|WS_BORDER|WS_VSCROLL|LBS_NOTIFY,0,0,10,10,h,(HMENU)MM_LB_SUG,g_hInst,0);
        s_ms->eText=CreateWindowExW(0,L"EDIT",L"",
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL,
            0,0,10,10,h,(HMENU)MM_E_TEXT,g_hInst,0);
        DWORD rb=WS_CHILD|WS_VISIBLE|BS_AUTORADIOBUTTON|BS_RIGHTBUTTON|BS_RIGHT;
        s_ms->rNormal=CreateWindowExW(0,L"BUTTON",L"عادی",rb|WS_GROUP,0,0,10,10,h,(HMENU)MM_R_NORMAL,g_hInst,0);
        s_ms->rUrgent=CreateWindowExW(0,L"BUTTON",L"فوری",rb,0,0,10,10,h,(HMENU)MM_R_URGENT,g_hInst,0);
        s_ms->rCrit  =CreateWindowExW(0,L"BUTTON",L"بحرانی",rb,0,0,10,10,h,(HMENU)MM_R_CRIT,g_hInst,0);
        s_ms->bAttach=createFlatButton(h,MM_B_ATTACH,L"📎 افزودن رسانه",ICO_NONE,BS_OUTLINE,0,0,10,10);
        s_ms->bSave  =CreateWindowExW(0,L"BUTTON",L"ذخیره در پیام‌های ذخیره‌شده",
            WS_CHILD|BS_AUTOCHECKBOX|BS_RIGHTBUTTON|BS_RIGHT,0,0,10,10,h,(HMENU)MM_B_SAVE,g_hInst,0);
        s_ms->bSend  =createFlatButton(h,MM_B_SEND,L"ارسال پیام",ICO_BELL,BS_PRIMARY,0,0,10,10);
        s_ms->bCancel=createFlatButton(h,MM_B_CANCEL,L"انصراف",ICO_X,BS_OUTLINE,0,0,10,10);
        s_ms->bViewEmp=createFlatButton(h,MM_B_VIEWEMP,L"مشاهدهٔ کارمندان",ICO_USER,BS_OUTLINE,0,0,10,10);
        HWND cardBtns[]={s_ms->bAttach,s_ms->bSend,s_ms->bCancel,s_ms->bViewEmp};
        for(HWND b:cardBtns) setFlatButtonBg(b,g_theme.surface);
        s_ms->eEmpSrch=CreateWindowExW(0,L"EDIT",L"",
            WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL,0,0,10,10,h,(HMENU)MM_E_EMPSRCH,g_hInst,0);
        SendMessageW(s_ms->eEmpSrch,EM_SETCUEBANNER,TRUE,(LPARAM)L"جستجوی کارمند…");
        SendMessageW(s_ms->eEmpSrch,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        s_ms->users=loadUsers();
        for(auto&u:s_ms->users) s_ms->profs.push_back(loadEmpProfile(u.username));
        s_ms->depts=loadDepts();
        s_ms->deptOn.assign(s_ms->depts.size(),false);
        SendMessageW(s_ms->rNormal,BM_SETCHECK,BST_CHECKED,0);
        HWND fc[7]={s_ms->eSearch,s_ms->eText,s_ms->rNormal,s_ms->rUrgent,s_ms->rCrit,s_ms->lbSug,0};
        for(int i=0;fc[i];i++) SendMessageW(fc[i],WM_SETFONT,(WPARAM)g_fUI,TRUE);
        SendMessageW(s_ms->bSave,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        mmLayout(h);
        return 0; }
    case WM_SIZE: mmLayout(h); return 0;
    case WM_NCDESTROY: if(s_ms){ delete s_ms; s_ms=NULL; } s_mm=NULL;
        if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX:{ HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_CTLCOLORSTATIC:{ HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.text); SetBkColor(dc,g_theme.surface);
        return (LRESULT)g_brSurface; }
    case WM_LBUTTONDOWN:{ POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        RECT c=mmCard(h);
        RECT cb={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)};
        // employee panel: pick a recipient by clicking a row
        if(s_ms && s_ms->empPanelOpen){
            RECT p=mmPanelRect(h);
            if(PtInRect(&p,pt)){
                int top=mmPanelListTop(h)-s_ms->empScroll, rh=mmPanelRowH();
                for(int i=0;i<(int)s_ms->empRows.size();i++){
                    if(s_ms->empRows[i].kind!=1) continue;
                    int ry=top+i*rh; RECT rr={p.left+S(8),ry,p.right-S(8),ry+rh};
                    if(PtInRect(&rr,pt)){
                        int u=s_ms->empRows[i].idx;
                        s_ms->selUser=u;
                        std::wstring nm=s_ms->users[u].fullname.empty()
                            ?s_ms->users[u].username:s_ms->users[u].fullname;
                        SetWindowTextW(s_ms->eSearch,nm.c_str());
                        for(size_t d=0;d<s_ms->deptOn.size();d++) s_ms->deptOn[d]=false;
                        ShowWindow(s_ms->lbSug,SW_HIDE);
                        InvalidateRect(h,NULL,FALSE);
                        return 0;
                    }
                }
                return 0;   // clicks inside the panel never close the modal
            }
        }
        if(!PtInRect(&c,pt)){ mmClose(); return 0; }
        if(PtInRect(&cb,pt)){ mmClose(); return 0; }
        // dept chip toggles
        if(s_ms) for(int i=0;i<(int)s_ms->depts.size();i++){
            RECT r=mmChipRect(h,i);
            if(PtInRect(&r,pt)){ s_ms->deptOn[i]=!s_ms->deptOn[i];
                s_ms->selUser=-1;   // dept routing clears single-recipient
                InvalidateRect(h,NULL,FALSE); return 0; }
        }
        return 0; }
    case WM_MOUSEMOVE:{ if(!s_ms||!s_ms->empPanelOpen) break;
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int oh=s_ms->empHot; s_ms->empHot=-1;
        RECT p=mmPanelRect(h);
        int top=mmPanelListTop(h)-s_ms->empScroll, rh=mmPanelRowH();
        for(int i=0;i<(int)s_ms->empRows.size();i++){
            if(s_ms->empRows[i].kind!=1) continue;
            int ry=top+i*rh; RECT rr={p.left+S(8),ry,p.right-S(8),ry+rh};
            if(PtInRect(&rr,pt)){ s_ms->empHot=i; break; }
        }
        if(oh!=s_ms->empHot) InvalidateRect(h,NULL,FALSE);
        return 0; }
    case WM_MOUSEWHEEL:{ if(!s_ms||!s_ms->empPanelOpen) break;
        int d=GET_WHEEL_DELTA_WPARAM(w);
        s_ms->empScroll-=d/2; if(s_ms->empScroll<0) s_ms->empScroll=0;
        int contentH=(int)s_ms->empRows.size()*mmPanelRowH();
        RECT p=mmPanelRect(h); int viewH=p.bottom-mmPanelListTop(h)-S(12);
        int maxS=contentH-viewH; if(maxS<0) maxS=0;
        if(s_ms->empScroll>maxS) s_ms->empScroll=maxS;
        InvalidateRect(h,NULL,FALSE); return 0; }
    case WM_COMMAND:{ int id=LOWORD(w), code=HIWORD(w);
        if(id==MM_E_SEARCH && code==EN_CHANGE){ mmRunSearch(h); }
        else if(id==MM_E_EMPSRCH && code==EN_CHANGE){
            wchar_t b[128]={0}; GetWindowTextW(s_ms->eEmpSrch,b,128);
            s_ms->empQuery=trim(b); s_ms->empScroll=0;
            mmBuildEmpRows(h); InvalidateRect(h,NULL,FALSE);
        }
        else if(id==MM_B_VIEWEMP){
            s_ms->empPanelOpen=!s_ms->empPanelOpen;
            SetWindowTextW(s_ms->bViewEmp, s_ms->empPanelOpen?L"بستن کارمندان":L"مشاهدهٔ کارمندان");
            if(s_ms->empPanelOpen){ s_ms->empScroll=0; mmBuildEmpRows(h); }
            mmLayout(h); InvalidateRect(h,NULL,TRUE);
        }
        else if(id==MM_LB_SUG && code==LBN_SELCHANGE){
            int sel=(int)SendMessageW(s_ms->lbSug,LB_GETCURSEL,0,0);
            if(sel>=0 && sel<(int)s_ms->matchIdx.size()){
                s_ms->selUser=s_ms->matchIdx[sel];
                std::wstring nm=s_ms->users[s_ms->selUser].fullname.empty()
                    ?s_ms->users[s_ms->selUser].username:s_ms->users[s_ms->selUser].fullname;
                SetWindowTextW(s_ms->eSearch,nm.c_str());
                ShowWindow(s_ms->lbSug,SW_HIDE);
                for(size_t d=0;d<s_ms->deptOn.size();d++) s_ms->deptOn[d]=false;
                InvalidateRect(h,NULL,FALSE);
            }
        }
        else if(id==MM_B_ATTACH) mmPickAttachment(h);
        else if(id==MM_B_SEND) mmDoSend(h);
        else if(id==MM_B_CANCEL) mmClose();
        return 0; }
    case WM_KEYDOWN: if(w==VK_ESCAPE){ mmClose(); return 0; } break;
    case WM_PAINT:{ PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        { HBRUSH sb=CreateSolidBrush(g_dark?RGB(6,9,14):RGB(28,36,48));
          FillRect(dc,&rc,sb); DeleteObject(sb); }
        gpFillAlpha(dc,rc,0,g_dark?RGB(0,0,0):RGB(20,28,40),120);
        RECT c=mmCard(h);
        gpShadowCached(dc0,dc,rc.right,rc.bottom,c,S(20),S(22),80);
        gpGradRoundRectBg(dc,c,S(18),g_theme.surfaceTop,g_theme.surface,g_theme.border,
            g_dark?RGB(6,9,14):RGB(28,36,48));
        SetBkMode(dc,TRANSPARENT);
        { RECT cb={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)};
          RECT ci={cb.left+S(5),cb.top+S(5),cb.right-S(5),cb.bottom-S(5)};
          drawIcon(dc,ICO_X,ci,g_theme.text,S(2)); }
        SelectObject(dc,g_fTitle); SetTextColor(dc,g_theme.accent);
        RECT tr={c.left+S(28),c.top+S(16),c.right-S(54),c.top+S(46)};
        DrawTextW(dc,L"پیام به کارمندان",-1,&tr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        // dept-filter label + chips
        SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
        RECT dl={c.left+S(28),mmChipsTop(h)-S(22),c.right-S(28),mmChipsTop(h)-S(4)};
        DrawTextW(dc,L"مسیردهی بر اساس بخش (چندانتخابی) — یا یک گیرنده از جستجو:",-1,&dl,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        if(s_ms) for(int i=0;i<(int)s_ms->depts.size();i++){
            RECT r=mmChipRect(h,i); bool on=s_ms->deptOn[i];
            gpRoundRectBg(dc,r,S(14),on?g_theme.accent:g_theme.surface2,
                on?g_theme.accent:g_theme.border, g_theme.surface,255);
            SetTextColor(dc,on?g_theme.accentText:g_theme.text); SelectObject(dc,g_fSmall);
            DrawTextW(dc,s_ms->depts[i].name.c_str(),-1,&r,
                DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
        }
        // selected single recipient indicator (top, under title)
        if(s_ms && s_ms->selUser>=0){
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.accent);
            RECT sr={c.left+S(190),c.top+S(20),c.right-S(54),c.top+S(44)};
            std::wstring nm=s_ms->users[s_ms->selUser].fullname.empty()
                ?s_ms->users[s_ms->selUser].username:s_ms->users[s_ms->selUser].fullname;
            DrawTextW(dc,(L"گیرنده: "+nm).c_str(),-1,&sr,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
        }
        // ---- right-side employee panel (when open) ----
        if(s_ms && s_ms->empPanelOpen){
            RECT p=mmPanelRect(h);
            gpGradRoundRectBg(dc,p,S(16),g_theme.surfaceTop,g_theme.surface,g_theme.border,
                g_dark?RGB(6,9,14):RGB(28,36,48));
            SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.text);
            RECT ph={p.left+S(14),p.top+S(14),p.right-S(14),p.top+S(40)};
            DrawTextW(dc,L"کارمندان",-1,&ph,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
            RECT clip={p.left+S(2),mmPanelListTop(h),p.right-S(2),p.bottom-S(8)};
            HRGN rgn=CreateRectRgn(clip.left,clip.top,clip.right,clip.bottom);
            SelectClipRgn(dc,rgn);
            int top=mmPanelListTop(h)-s_ms->empScroll, rh=mmPanelRowH();
            for(int i=0;i<(int)s_ms->empRows.size();i++){
                int ry=top+i*rh;
                if(ry+rh<clip.top||ry>clip.bottom) continue;
                auto& row=s_ms->empRows[i];
                if(row.kind==0){
                    // §E: section header — name on the right, section code badge on the left.
                    SelectObject(dc,g_fSmall); SetTextColor(dc,g_infoAccent2);
                    RECT hr={p.left+S(14),ry+S(6),p.right-S(14),ry+rh-S(4)};
                    DrawTextW(dc,row.name.c_str(),-1,&hr,
                        DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                    if(!row.code.empty()){
                        // §G: section code in monospace so digits align in a column.
                        SelectObject(dc,g_fCode);
                        SetTextColor(dc,blendColor(g_theme.textDim,g_theme.surface,20));
                        RECT cr={p.left+S(14),ry+S(6),p.left+S(120),ry+rh-S(4)};
                        DrawTextW(dc,row.code.c_str(),-1,&cr,
                            DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
                        SelectObject(dc,g_fSmall);
                    }
                    gpLine(dc,p.left+S(14),ry+rh-S(2),p.right-S(14),ry+rh-S(2),g_theme.border,1);
                } else if(row.kind==2){
                    // §E: "… و N مورد دیگر" lazy-overflow summary row (caps reached).
                    SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
                    RECT mr={p.left+S(16),ry+S(2),p.right-S(16),ry+rh-S(2)};
                    DrawTextW(dc,row.name.c_str(),-1,&mr,
                        DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                } else {
                    int u=row.idx; bool online=isUserOnline(s_ms->users[u].username);
                    bool sel=(s_ms->selUser==u), hot=(s_ms->empHot==i);
                    RECT rr={p.left+S(8),ry+S(2),p.right-S(8),ry+rh-S(2)};
                    if(sel) gpRoundRectBg(dc,rr,S(9),blendColor(g_theme.surface,g_theme.accent,18),g_theme.accent,g_theme.surface);
                    else if(hot) gpRoundRectBg(dc,rr,S(9),g_theme.surface2,g_theme.border,g_theme.surface);
                    RECT dot={rr.right-S(22),rr.top+S(13),rr.right-S(10),rr.top+S(25)};
                    gpRoundRect(dc,dot,S(6), online?RGB(0x2E,0xB8,0x6B):blendColor(g_theme.textDim,g_theme.surface,30),
                        CLR_INVALID,255);
                    SelectObject(dc,g_fUI); SetTextColor(dc, sel?g_theme.accent:(online?g_theme.text:g_theme.textDim));
                    // §E: employee code shown on the left of each row; name on the right.
                    if(!row.code.empty()){
                        // §G: personnel code in monospace (Consolas → Courier New).
                        SelectObject(dc,g_fCode);
                        SetTextColor(dc, sel?g_theme.accent:blendColor(g_theme.textDim,g_theme.surface,15));
                        RECT cr={rr.left+S(10),rr.top,rr.left+S(96),rr.bottom};
                        DrawTextW(dc,row.code.c_str(),-1,&cr,
                            DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
                        SelectObject(dc,g_fUI);
                        SetTextColor(dc, sel?g_theme.accent:(online?g_theme.text:g_theme.textDim));
                    }
                    RECT nr={rr.left+S(100),rr.top,rr.right-S(30),rr.bottom};
                    std::wstring nm=s_ms->users[u].fullname.empty()
                        ?s_ms->users[u].username:s_ms->users[u].fullname;
                    DrawTextW(dc,nm.c_str(),-1,&nr,
                        DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                }
            }
            SelectClipRgn(dc,NULL); DeleteObject(rgn);
        }
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps); return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}
static void openMsgCompose(HWND owner){
    if(s_mm&&IsWindow(s_mm)) mmClose();
    static bool reg=false;
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=mmProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=MGMSG_CLASS;
        RegisterClassW(&wc); reg=true; }
    RECT rc; GetClientRect(g_hFrame,&rc);
    POINT org={0,0}; ClientToScreen(g_hFrame,&org);
    s_mm=CreateWindowExW(WS_EX_TOPMOST,MGMSG_CLASS,L"",WS_POPUP|WS_VISIBLE,
        org.x,org.y,rc.right,rc.bottom,g_hFrame,NULL,g_hInst,NULL);
    BringWindowToTop(s_mm); SetFocus(s_mm);
}

// ====================== new-employee form modal =============================
//  v1.8.0 full, polished, production-ready form. Auto-generates the personnel
//  code (empId) and a system-unique identifier (uniqueId) — both editable.
//  Collects: full name, username, password, national-id, father, position,
//  department, mobile, landline, email, shift from/to, weekly work hours,
//  hire-date, address. Creates the login User + EmpProfile in one step.
#define MGNEW_CLASS L"AzNewEmp"
enum {
    NE_E_FULL=3401, NE_E_USER, NE_E_PASS, NE_E_NID, NE_E_FATHER, NE_E_POS,
    NE_E_MOBILE, NE_E_LAND, NE_E_EMAIL, NE_E_SFROM, NE_E_STO, NE_E_HOURS,
    NE_E_HIRE, NE_E_ADDR, NE_E_EMPID, NE_E_UID, NE_CB_DEPT, NE_CB_ROLE,
    NE_CB_SHIFT,    // v2.01 (Part F2): شیفت کاری combo
    NE_B_OK, NE_B_CANCEL, NE_CB_AUTOID, NE_B_PHOTO, NE_B_IDCARD
};
struct NewEmpState {
    HWND eFull,eUser,ePass,eNid,eFather,ePos,eMobile,eLand,eEmail,
         eSfrom,eSto,eHours,eHire,eAddr,eEmpId,eUid,cDept,cRole,bOk,bCancel,bAutoId,
         bPhoto,bIdCard,cShift;   // v2.01: cShift = شیفت کاری combo
    std::vector<DeptCat> depts;
    std::vector<ShiftDef> shiftDefs;  // v2.01: shift definitions for the combo
    bool autoId;
    std::wstring photoPath, idCardPath;   // chosen image files (copied on save)
};
static HWND s_ne=NULL; static NewEmpState* s_nes=NULL;
static void neClose(){ if(s_ne&&IsWindow(s_ne)){ HWND v=s_ne; s_ne=NULL; DestroyWindow(v);} }
static RECT neCard(HWND h){ RECT rc; GetClientRect(h,&rc);
    int w=S(760),hh=S(688);
    if(hh>rc.bottom-S(20)) hh=rc.bottom-S(20);
    RECT c={(rc.right-w)/2,(rc.bottom-hh)/2,(rc.right+w)/2,(rc.bottom+hh)/2}; return c; }
static std::wstring neGenEmpId(){
    auto users=loadUsers(); int n=(int)users.size()+1;
    wchar_t b[24]; swprintf(b,24,L"PRS%04d",n); return b;
}
static std::wstring neGenUid(){
    SYSTEMTIME st=iranNow();
    wchar_t b[40]; swprintf(b,40,L"UID%02d%02d%02d%02d%02d%02d",
        st.wYear%100,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
    return b;
}
//  Two-column form layout. Returns y after the last row.
static void neLayout(HWND h){
    if(!s_nes) return; RECT c=neCard(h);
    int top=c.top+S(64), fh=S(30), lh=S(24), gap=S(10);
    int colGap=S(24);
    int colW=(c.right-c.left-S(48)-colGap)/2;
    int rCol=c.right-S(24);                 // right column right edge
    int rColL=rCol-colW;
    int lColR=rColL-colGap;                  // left column right edge
    int lColL=lColR-colW;
    int yR=top, yL=top;
    auto row=[&](HWND ctl, int& y, int colRight, int colLeft, int fieldH){
        MoveWindow(ctl, colLeft, y+lh, colRight-colLeft, fieldH, TRUE);
        ShowWindow(ctl,SW_SHOW);
        y += lh + fieldH + gap;
    };
    // RIGHT column
    row(s_nes->eFull,  yR, rCol, rColL, fh);
    row(s_nes->eUser,  yR, rCol, rColL, fh);
    row(s_nes->ePass,  yR, rCol, rColL, fh);
    row(s_nes->eNid,   yR, rCol, rColL, fh);
    row(s_nes->eFather,yR, rCol, rColL, fh);
    row(s_nes->cDept,  yR, rCol, rColL, S(200));
    row(s_nes->cRole,  yR, rCol, rColL, S(160));
    // v2.01 (Part F2): «شیفت کاری» combo in the right column
    row(s_nes->cShift, yR, rCol, rColL, S(200));
    row(s_nes->eMobile,yR, rCol, rColL, fh);
    // LEFT column
    row(s_nes->eEmpId, yL, lColR, lColL, fh);
    row(s_nes->eUid,   yL, lColR, lColL, fh);
    // auto-id checkbox sits right under the two id fields
    MoveWindow(s_nes->bAutoId, lColL, yL+S(2), lColR-lColL, S(26), TRUE);
    ShowWindow(s_nes->bAutoId,SW_SHOW); yL+=S(34);
    row(s_nes->ePos,   yL, lColR, lColL, fh);
    row(s_nes->eLand,  yL, lColR, lColL, fh);
    row(s_nes->eEmail, yL, lColR, lColL, fh);
    // shift from/to share a row (two halves)
    { int half=(lColR-lColL-S(8))/2;
      MoveWindow(s_nes->eSfrom, lColL+half+S(8), yL+lh, half, fh, TRUE);
      MoveWindow(s_nes->eSto,   lColL,           yL+lh, half, fh, TRUE);
      ShowWindow(s_nes->eSfrom,SW_SHOW); ShowWindow(s_nes->eSto,SW_SHOW);
      yL += lh+fh+gap; }
    row(s_nes->eHours, yL, lColR, lColL, fh);
    row(s_nes->eHire,  yL, lColR, lColL, fh);
    // image-upload buttons (national-id card + personal photo) share one full
    // width row just above the address field. They never overlap any label.
    int upY=c.bottom-S(156);
    { int half=(rCol-lColL-S(8))/2;
      MoveWindow(s_nes->bIdCard, lColL,            upY, half, S(34), TRUE);
      MoveWindow(s_nes->bPhoto,  lColL+half+S(8),  upY, half, S(34), TRUE);
      ShowWindow(s_nes->bIdCard,SW_SHOW); ShowWindow(s_nes->bPhoto,SW_SHOW); }
    // address spans full width near the bottom
    int addrY=c.bottom-S(108);
    MoveWindow(s_nes->eAddr, lColL, addrY, rCol-lColL, S(40), TRUE);
    ShowWindow(s_nes->eAddr,SW_SHOW);
    // buttons
    int by=c.bottom-S(52), bw=S(150);
    MoveWindow(s_nes->bOk,     rCol-bw,         by, bw, S(38), TRUE);
    MoveWindow(s_nes->bCancel, rCol-bw-S(8)-bw, by, bw, S(38), TRUE);
    // id fields visible only when manual
    ShowWindow(s_nes->eEmpId, s_nes->autoId?SW_HIDE:SW_SHOW);
    ShowWindow(s_nes->eUid,   s_nes->autoId?SW_HIDE:SW_SHOW);
}
static void neDoCreate(HWND h){
    if(!s_nes) return;
    wchar_t full[128]={0},user[64]={0},pass[64]={0},nid[32]={0},father[64]={0},
            pos[64]={0},mobile[32]={0},land[32]={0},email[96]={0},
            sfrom[16]={0},sto[16]={0},hours[16]={0},hire[24]={0},addr[256]={0},
            empId[32]={0},uid[48]={0};
    GetWindowTextW(s_nes->eFull,full,128);
    GetWindowTextW(s_nes->eUser,user,64);
    GetWindowTextW(s_nes->ePass,pass,64);
    GetWindowTextW(s_nes->eNid,nid,32);
    GetWindowTextW(s_nes->eFather,father,64);
    GetWindowTextW(s_nes->ePos,pos,64);
    GetWindowTextW(s_nes->eMobile,mobile,32);
    GetWindowTextW(s_nes->eLand,land,32);
    GetWindowTextW(s_nes->eEmail,email,96);
    GetWindowTextW(s_nes->eSfrom,sfrom,16);
    GetWindowTextW(s_nes->eSto,sto,16);
    GetWindowTextW(s_nes->eHours,hours,16);
    GetWindowTextW(s_nes->eHire,hire,24);
    GetWindowTextW(s_nes->eAddr,addr,256);
    GetWindowTextW(s_nes->eEmpId,empId,32);
    GetWindowTextW(s_nes->eUid,uid,48);
    std::wstring un=trim(user), fn=trim(full), pw=trim(pass);
    if(fn.empty()){ MessageBoxW(h,L"نام و نام خانوادگی الزامی است.",L"افزودن کارمند",MB_OK|MB_ICONWARNING); return; }
    if(un.empty()){ MessageBoxW(h,L"نام کاربری الزامی است.",L"افزودن کارمند",MB_OK|MB_ICONWARNING); return; }
    if(pw.empty()) pw=L"1234";
    User u; u.username=un; u.fullname=fn; u.hash=hashPassword(pw);
    int deptSel=(int)SendMessageW(s_nes->cDept,CB_GETCURSEL,0,0);
    std::wstring deptId, deptName;
    if(deptSel>=0 && deptSel<(int)s_nes->depts.size()){
        deptId=s_nes->depts[deptSel].id; deptName=s_nes->depts[deptSel].name; }
    u.dept=deptName;
    int roleSel=(int)SendMessageW(s_nes->cRole,CB_GETCURSEL,0,0);
    u.role=(roleSel==1)?1:0;
    // v2.01 (Part F2): read the assigned shift from the combo.
    int shiftSel=(int)SendMessageW(s_nes->cShift,CB_GETCURSEL,0,0);
    if(shiftSel>0 && shiftSel-1<(int)s_nes->shiftDefs.size()){
        u.shift=s_nes->shiftDefs[shiftSel-1].id;
    } else {
        u.shift=-1;  // not assigned
    }
    std::wstring err;
    if(!addUser(u,err)){ MessageBoxW(h,err.c_str(),L"افزودن کارمند",MB_OK|MB_ICONERROR); return; }
    EmpProfile p; p.username=un; p.deptId=deptId;
    p.nationalId=trim(nid); p.fatherName=trim(father); p.position=trim(pos);
    p.mobile=trim(mobile); p.landline=trim(land); p.email=trim(email);
    p.shiftFrom=trim(sfrom); p.shiftTo=trim(sto); p.workHours=trim(hours);
    p.hireDate=trim(hire); p.address=trim(addr);
    p.empId   = s_nes->autoId? neGenEmpId() : trim(empId);
    p.uniqueId= s_nes->autoId? neGenUid()   : trim(uid);
    p.photoPath=s_nes->photoPath; p.idCardPath=s_nes->idCardPath;
    saveEmpProfile(p);
    if(s_mg){ s_mg->msg=L"کارمند «"+fn+L"» با کد پرسنلی "+toFaDigits(p.empId)+L" ساخته شد.";
        s_mg->msgCol=g_theme.success; mgLoadEmps(); }
    neClose();
    if(g_hManage) InvalidateRect(g_hManage,NULL,TRUE);
}
static LRESULT CALLBACK neProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE:{
        s_nes=new NewEmpState(); s_nes->autoId=true;
        DWORD es=WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL;
        auto mk=[&](int id,DWORD extra)->HWND{
            HWND e=CreateWindowExW(0,L"EDIT",L"",es|extra,0,0,10,10,h,(HMENU)(UINT_PTR)id,g_hInst,0);
            SendMessageW(e,WM_SETFONT,(WPARAM)g_fUI,TRUE); return e; };
        s_nes->eFull=mk(NE_E_FULL,0); s_nes->eUser=mk(NE_E_USER,0);
        s_nes->ePass=mk(NE_E_PASS,ES_PASSWORD); s_nes->eNid=mk(NE_E_NID,0);
        s_nes->eFather=mk(NE_E_FATHER,0); s_nes->ePos=mk(NE_E_POS,0);
        s_nes->eMobile=mk(NE_E_MOBILE,0); s_nes->eLand=mk(NE_E_LAND,0);
        s_nes->eEmail=mk(NE_E_EMAIL,0); s_nes->eSfrom=mk(NE_E_SFROM,0);
        s_nes->eSto=mk(NE_E_STO,0); s_nes->eHours=mk(NE_E_HOURS,0);
        s_nes->eHire=mk(NE_E_HIRE,0); s_nes->eAddr=mk(NE_E_ADDR,0);
        s_nes->eEmpId=mk(NE_E_EMPID,0); s_nes->eUid=mk(NE_E_UID,0);
        SendMessageW(s_nes->eSfrom,EM_SETCUEBANNER,TRUE,(LPARAM)L"از (۰۸:۰۰)");
        SendMessageW(s_nes->eSto,EM_SETCUEBANNER,TRUE,(LPARAM)L"تا (۱۶:۰۰)");
        SendMessageW(s_nes->eHours,EM_SETCUEBANNER,TRUE,(LPARAM)L"ساعت/هفته");
        SendMessageW(s_nes->eHire,EM_SETCUEBANNER,TRUE,(LPARAM)L"۱۴۰۴/۰۳/۲۸");
        s_nes->cDept=createThemedCombo(h,NE_CB_DEPT);
        s_nes->cRole=createThemedCombo(h,NE_CB_ROLE);
        // v2.01 (Part F2): «شیفت کاری» combo — populated from shift definitions.
        s_nes->cShift=createThemedCombo(h,NE_CB_SHIFT);
        SendMessageW(s_nes->cDept,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        SendMessageW(s_nes->cRole,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        SendMessageW(s_nes->cShift,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        // Populate shift combo: «— انتخاب نشده —» then each shift def.
        s_nes->shiftDefs=loadShiftDefs();
        SendMessageW(s_nes->cShift,CB_ADDSTRING,0,(LPARAM)L"— انتخاب نشده —");
        for(const auto& sd : s_nes->shiftDefs){
            int sm=sd.startMin, em=sd.endMin;
            wchar_t lbl[128]; swprintf(lbl,128,L"%s  (%02d:%02d - %02d:%02d)",
                sd.name.c_str(), sm/60, sm%60, em/60, em%60);
            SendMessageW(s_nes->cShift,CB_ADDSTRING,0,(LPARAM)lbl);
        }
        SendMessageW(s_nes->cShift,CB_SETCURSEL,0,0);
        s_nes->depts=loadDepts();
        for(auto&d:s_nes->depts) SendMessageW(s_nes->cDept,CB_ADDSTRING,0,(LPARAM)d.name.c_str());
        // preselect current section if any
        if(s_mg && !s_mg->curCatName.empty()){
            for(int i=0;i<(int)s_nes->depts.size();i++)
                if(s_nes->depts[i].name==s_mg->curCatName){ SendMessageW(s_nes->cDept,CB_SETCURSEL,i,0); break; }
        }
        else if(!s_nes->depts.empty()) SendMessageW(s_nes->cDept,CB_SETCURSEL,0,0);
        SendMessageW(s_nes->cRole,CB_ADDSTRING,0,(LPARAM)L"پذیرش");
        SendMessageW(s_nes->cRole,CB_ADDSTRING,0,(LPARAM)L"مدیریت");
        SendMessageW(s_nes->cRole,CB_SETCURSEL,0,0);
        s_nes->bAutoId=CreateWindowExW(0,L"BUTTON",L"شناسه‌ها به‌صورت خودکار",
            WS_CHILD|BS_AUTOCHECKBOX|BS_RIGHTBUTTON|BS_RIGHT,0,0,10,10,h,(HMENU)NE_CB_AUTOID,g_hInst,0);
        SendMessageW(s_nes->bAutoId,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        SendMessageW(s_nes->bAutoId,BM_SETCHECK,BST_CHECKED,0);
        s_nes->bIdCard=createFlatButton(h,NE_B_IDCARD,L"بارگذاری تصویر کارت ملی",ICO_ID,BS_OUTLINE,0,0,10,10);
        s_nes->bPhoto =createFlatButton(h,NE_B_PHOTO, L"بارگذاری عکس پرسنلی",ICO_USER,BS_OUTLINE,0,0,10,10);
        s_nes->bOk=createFlatButton(h,NE_B_OK,L"ثبت کارمند",ICO_PLUS,BS_PRIMARY,0,0,10,10);
        s_nes->bCancel=createFlatButton(h,NE_B_CANCEL,L"انصراف",ICO_X,BS_OUTLINE,0,0,10,10);
        HWND cardBtns[]={s_nes->bIdCard,s_nes->bPhoto,s_nes->bOk,s_nes->bCancel};
        for(HWND b:cardBtns) setFlatButtonBg(b,g_theme.surface);
        neLayout(h);
        return 0; }
    case WM_SIZE: neLayout(h); return 0;
    case WM_NCDESTROY: if(s_nes){ delete s_nes; s_nes=NULL; } s_ne=NULL;
        if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX:{ HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_CTLCOLORSTATIC:{ HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.text); SetBkColor(dc,g_theme.surface);
        return (LRESULT)g_brSurface; }
    case WM_DRAWITEM:
        if(drawThemedComboItem((LPDRAWITEMSTRUCT)l)) return TRUE;
        break;
    case WM_COMMAND:{ int id=LOWORD(w);
        if(id==NE_B_OK) neDoCreate(h);
        else if(id==NE_B_CANCEL) neClose();
        else if(id==NE_CB_AUTOID){ s_nes->autoId=!s_nes->autoId; neLayout(h);
            InvalidateRect(h,NULL,FALSE); }
        else if(id==NE_B_PHOTO || id==NE_B_IDCARD){
            wchar_t fn[MAX_PATH]={0};
            OPENFILENAMEW ofn={sizeof(ofn)}; ofn.hwndOwner=h;
            ofn.lpstrFilter=L"تصویر\0*.png;*.jpg;*.jpeg;*.bmp;*.gif\0همه\0*.*\0";
            ofn.lpstrFile=fn; ofn.nMaxFile=MAX_PATH;
            ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
            if(GetOpenFileNameW(&ofn)){
                std::wstring stored=copyAttachmentLocal(fn);
                if(stored.empty()) stored=fn;
                if(id==NE_B_PHOTO){ s_nes->photoPath=stored;
                    SetWindowTextW(s_nes->bPhoto,L"عکس پرسنلی ✓"); }
                else { s_nes->idCardPath=stored;
                    SetWindowTextW(s_nes->bIdCard,L"تصویر کارت ملی ✓"); }
            }
        }
        return 0; }
    case WM_KEYDOWN: if(w==VK_ESCAPE){ neClose(); return 0; } break;
    case WM_PAINT:{ PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        { HBRUSH sb=CreateSolidBrush(g_dark?RGB(6,9,14):RGB(28,36,48));
          FillRect(dc,&rc,sb); DeleteObject(sb); }
        gpFillAlpha(dc,rc,0,g_dark?RGB(0,0,0):RGB(20,28,40),120);
        RECT c=neCard(h);
        gpShadowCached(dc0,dc,rc.right,rc.bottom,c,S(20),S(22),80);
        gpGradRoundRectBg(dc,c,S(18),g_theme.surfaceTop,g_theme.surface,g_theme.border,
            g_dark?RGB(6,9,14):RGB(28,36,48));
        SetBkMode(dc,TRANSPARENT);
        { RECT cb={c.left+S(14),c.top+S(14),c.left+S(40),c.top+S(40)};
          RECT ci={cb.left+S(5),cb.top+S(5),cb.right-S(5),cb.bottom-S(5)};
          drawIcon(dc,ICO_X,ci,g_theme.text,S(2)); }
        SelectObject(dc,g_fTitle); SetTextColor(dc,g_theme.accent);
        RECT tr={c.left+S(28),c.top+S(16),c.right-S(54),c.top+S(46)};
        DrawTextW(dc,L"افزودن کارمند جدید",-1,&tr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        // field labels (drawn just above each control)
        SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
        auto lbl=[&](HWND ctl,const wchar_t* t){
            RECT wr; GetWindowRect(ctl,&wr);
            POINT p1={wr.left,wr.top}; ScreenToClient(h,&p1);
            int wcw=wr.right-wr.left;
            RECT lr={p1.x,p1.y-S(20),p1.x+wcw,p1.y-S(2)};
            DrawTextW(dc,t,-1,&lr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        };
        if(s_nes){
            lbl(s_nes->eFull,L"نام و نام خانوادگی *");
            lbl(s_nes->eUser,L"نام کاربری *");
            lbl(s_nes->ePass,L"رمز عبور (پیش‌فرض ۱۲۳۴)");
            lbl(s_nes->eNid,L"کد ملی");
            lbl(s_nes->eFather,L"نام پدر");
            lbl(s_nes->cDept,L"بخش / دپارتمان");
            lbl(s_nes->cRole,L"نوع دسترسی");
            lbl(s_nes->eMobile,L"موبایل");
            if(!s_nes->autoId){ lbl(s_nes->eEmpId,L"کد پرسنلی"); lbl(s_nes->eUid,L"شناسه یکتا"); }
            lbl(s_nes->ePos,L"سمت / عنوان شغلی");
            lbl(s_nes->eLand,L"تلفن ثابت");
            lbl(s_nes->eEmail,L"ایمیل");
            lbl(s_nes->eSto,L"شیفت — از / تا");
            lbl(s_nes->eHours,L"ساعات کاری هفتگی");
            lbl(s_nes->eHire,L"تاریخ استخدام");
            lbl(s_nes->eAddr,L"آدرس");
        }
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps); return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}
static void openNewEmployee(HWND owner){
    if(s_ne&&IsWindow(s_ne)) neClose();
    static bool reg=false;
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=neProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=MGNEW_CLASS;
        RegisterClassW(&wc); reg=true; }
    RECT rc; GetClientRect(g_hFrame,&rc);
    POINT org={0,0}; ClientToScreen(g_hFrame,&org);
    s_ne=CreateWindowExW(WS_EX_TOPMOST,MGNEW_CLASS,L"",WS_POPUP|WS_VISIBLE,
        org.x,org.y,rc.right,rc.bottom,g_hFrame,NULL,g_hInst,NULL);
    BringWindowToTop(s_ne); SetFocus(s_ne);
}

// ============================================================================
//  §1.19.0 — «افزودن بیمار» (Add Patient) modal. Mirrors the reception form so
//  the manager can register a patient with ALL the same fields (name, family,
//  national code, father, birth, gender, mobile, landline, address, insurance,
//  supplementary insurance). Persisted via rememberPatient() into the SAME
//  local store the reception national-ID auto-fill reads from — so a patient
//  added here is instantly recalled by code at the reception desk.
// ============================================================================
#define MGPAT_CLASS L"AzNewPatient"
enum {
    NP_E_FIRST=3501, NP_E_LAST, NP_E_NID, NP_E_FATHER, NP_E_BIRTH,
    NP_E_MOBILE, NP_E_LAND, NP_E_ADDR, NP_CB_GENDER, NP_CB_INS, NP_CB_SUPP,
    NP_B_OK, NP_B_CANCEL
};
struct NewPatState {
    HWND eFirst,eLast,eNid,eFather,eBirth,eMobile,eLand,eAddr,
         cGender,cIns,cSupp,bOk,bCancel;
};
static HWND s_np=NULL; static NewPatState* s_nps=NULL;
static void npClose(){ if(s_np&&IsWindow(s_np)){ HWND v=s_np; s_np=NULL; DestroyWindow(v);} }
static RECT npCard(HWND h){ RECT rc; GetClientRect(h,&rc);
    int w=S(720),hh=S(560);
    if(hh>rc.bottom-S(20)) hh=rc.bottom-S(20);
    RECT c={(rc.right-w)/2,(rc.bottom-hh)/2,(rc.right+w)/2,(rc.bottom+hh)/2}; return c; }
static void npLayout(HWND h){
    if(!s_nps) return; RECT c=npCard(h);
    int top=c.top+S(64), fh=S(30), lh=S(24), gap=S(10), colGap=S(24);
    int colW=(c.right-c.left-S(48)-colGap)/2;
    int rCol=c.right-S(24); int rColL=rCol-colW;
    int lColR=rColL-colGap; int lColL=lColR-colW;
    int yR=top, yL=top;
    auto row=[&](HWND ctl, int& y, int colRight, int colLeft, int fieldH){
        MoveWindow(ctl, colLeft, y+lh, colRight-colLeft, fieldH, TRUE);
        ShowWindow(ctl,SW_SHOW); y += lh + fieldH + gap; };
    // RIGHT column — identity
    row(s_nps->eFirst, yR, rCol, rColL, fh);
    row(s_nps->eLast,  yR, rCol, rColL, fh);
    row(s_nps->eNid,   yR, rCol, rColL, fh);
    row(s_nps->eFather,yR, rCol, rColL, fh);
    row(s_nps->cGender,yR, rCol, rColL, S(160));
    // LEFT column — contact + insurance
    row(s_nps->eBirth, yL, lColR, lColL, fh);
    row(s_nps->eMobile,yL, lColR, lColL, fh);
    row(s_nps->eLand,  yL, lColR, lColL, fh);
    row(s_nps->cIns,   yL, lColR, lColL, S(200));
    row(s_nps->cSupp,  yL, lColR, lColL, S(200));
    // address spans full width near the bottom
    int addrY=c.bottom-S(108);
    MoveWindow(s_nps->eAddr, lColL, addrY, rCol-lColL, S(40), TRUE);
    ShowWindow(s_nps->eAddr,SW_SHOW);
    int by=c.bottom-S(52), bw=S(150);
    MoveWindow(s_nps->bOk,     rCol-bw,         by, bw, S(38), TRUE);
    MoveWindow(s_nps->bCancel, rCol-bw-S(8)-bw, by, bw, S(38), TRUE);
}
static void npDoSave(HWND h){
    if(!s_nps) return;
    wchar_t first[128]={0},last[128]={0},nid[32]={0},father[96]={0},
            birth[32]={0},mobile[32]={0},land[32]={0},addr[256]={0};
    GetWindowTextW(s_nps->eFirst,first,128);
    GetWindowTextW(s_nps->eLast,last,128);
    GetWindowTextW(s_nps->eNid,nid,32);
    GetWindowTextW(s_nps->eFather,father,96);
    GetWindowTextW(s_nps->eBirth,birth,32);
    GetWindowTextW(s_nps->eMobile,mobile,32);
    GetWindowTextW(s_nps->eLand,land,32);
    GetWindowTextW(s_nps->eAddr,addr,256);
    std::wstring fn=trim(first), ln=trim(last), id=trim(nid);
    if(fn.empty()||ln.empty()){
        MessageBoxW(h,L"نام و نام خانوادگی بیمار الزامی است.",L"افزودن بیمار",MB_OK|MB_ICONWARNING); return; }
    if(id.empty()||!validNationalId(id)){
        MessageBoxW(h,L"کد ملی معتبر ۱۰ رقمی الزامی است.",L"افزودن بیمار",MB_OK|MB_ICONWARNING); return; }
    int gi=(int)SendMessageW(s_nps->cGender,CB_GETCURSEL,0,0);
    std::wstring gender = gi==1?L"زن":L"مرد";
    std::vector<int> ins;
    int ii=(int)SendMessageW(s_nps->cIns,CB_GETCURSEL,0,0);
    if(ii>0 && ii<N_INSURANCES) ins.push_back(ii);
    // v1.40.0: supplementary-insurance index (0 == none/first entry → store -1)
    int si=(int)SendMessageW(s_nps->cSupp,CB_GETCURSEL,0,0);
    if(si<=0 || si>=N_SUPP) si=-1;
    // persist into the SAME store reception reads (national-ID dedup inside).
    // v1.40.0: forward landline «تلفن ثابت», address «آدرس» and supp index.
    rememberPatient(id, fn, ln, trim(father), gender, trim(birth), trim(mobile),
        trim(land), trim(addr), ins, si);
    if(s_mg){ s_mg->msg=L"بیمار «"+fn+L" "+ln+L"» با کد ملی "+toFaDigits(id)+L" ثبت شد.";
        s_mg->msgCol=g_theme.success; }
    // force the patients list to reload so the new row appears
    if(s_mg){ s_mg->ptLoaded=false; s_mg->ptLoading=false;
        if(s_mg->ptAll){ delete s_mg->ptAll; s_mg->ptAll=nullptr; } }
    npClose();
    if(g_hManage){ mgPatientsLoadAsync(g_hManage); InvalidateRect(g_hManage,NULL,TRUE); }
}
static LRESULT CALLBACK npProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE:{
        s_nps=new NewPatState();
        DWORD es=WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL;
        auto mk=[&](int id,DWORD extra)->HWND{
            HWND e=CreateWindowExW(0,L"EDIT",L"",es|extra,0,0,10,10,h,(HMENU)(UINT_PTR)id,g_hInst,0);
            SendMessageW(e,WM_SETFONT,(WPARAM)g_fUI,TRUE);
            enableEnterNavigation(e); return e; };
        s_nps->eFirst=mk(NP_E_FIRST,0); s_nps->eLast=mk(NP_E_LAST,0);
        s_nps->eNid=mk(NP_E_NID,ES_NUMBER); s_nps->eFather=mk(NP_E_FATHER,0);
        s_nps->eBirth=mk(NP_E_BIRTH,0); s_nps->eMobile=mk(NP_E_MOBILE,0);
        s_nps->eLand=mk(NP_E_LAND,0); s_nps->eAddr=mk(NP_E_ADDR,0);
        SendMessageW(s_nps->eBirth,EM_SETCUEBANNER,TRUE,(LPARAM)L"۱۳۷۰/۰۵/۲۰");
        s_nps->cGender=createThemedCombo(h,NP_CB_GENDER);
        s_nps->cIns=createThemedCombo(h,NP_CB_INS);
        s_nps->cSupp=createThemedCombo(h,NP_CB_SUPP);
        for(HWND cb:{s_nps->cGender,s_nps->cIns,s_nps->cSupp})
            SendMessageW(cb,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        SendMessageW(s_nps->cGender,CB_ADDSTRING,0,(LPARAM)L"مرد");
        SendMessageW(s_nps->cGender,CB_ADDSTRING,0,(LPARAM)L"زن");
        SendMessageW(s_nps->cGender,CB_SETCURSEL,0,0);
        for(int i=0;i<N_INSURANCES;i++) SendMessageW(s_nps->cIns,CB_ADDSTRING,0,(LPARAM)INSURANCES[i].name);
        SendMessageW(s_nps->cIns,CB_SETCURSEL,0,0);
        for(int i=0;i<N_SUPP;i++) SendMessageW(s_nps->cSupp,CB_ADDSTRING,0,(LPARAM)SUPP_INSURANCES[i].name);
        SendMessageW(s_nps->cSupp,CB_SETCURSEL,0,0);
        s_nps->bOk=createFlatButton(h,NP_B_OK,L"ثبت بیمار",ICO_PLUS,BS_PRIMARY,0,0,10,10);
        s_nps->bCancel=createFlatButton(h,NP_B_CANCEL,L"انصراف",ICO_X,BS_OUTLINE,0,0,10,10);
        setFlatButtonBg(s_nps->bOk,g_theme.surface);
        setFlatButtonBg(s_nps->bCancel,g_theme.surface);
        npLayout(h); SetFocus(s_nps->eFirst);
        return 0; }
    case WM_SIZE: npLayout(h); return 0;
    case WM_NCDESTROY: if(s_nps){ delete s_nps; s_nps=NULL; } s_np=NULL;
        if(g_hFrame) InvalidateRect(g_hFrame,NULL,TRUE); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX:{ HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_CTLCOLORSTATIC:{ HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.text); SetBkColor(dc,g_theme.surface);
        return (LRESULT)g_brSurface; }
    case WM_DRAWITEM:
        if(drawThemedComboItem((LPDRAWITEMSTRUCT)l)) return TRUE;
        break;
    case WM_COMMAND:{ int id=LOWORD(w);
        if(id==NP_B_OK) npDoSave(h);
        else if(id==NP_B_CANCEL) npClose();
        return 0; }
    case WM_KEYDOWN: if(w==VK_ESCAPE){ npClose(); return 0; } break;
    case WM_PAINT:{ PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        { HBRUSH sb=CreateSolidBrush(g_dark?RGB(6,9,14):RGB(28,36,48));
          FillRect(dc,&rc,sb); DeleteObject(sb); }
        gpFillAlpha(dc,rc,0,g_dark?RGB(0,0,0):RGB(20,28,40),120);
        RECT c=npCard(h);
        gpShadowCached(dc0,dc,rc.right,rc.bottom,c,S(20),S(22),80);
        gpGradRoundRectBg(dc,c,S(18),g_theme.surfaceTop,g_theme.surface,g_theme.border,
            g_dark?RGB(6,9,14):RGB(28,36,48));
        SetBkMode(dc,TRANSPARENT);
        SelectObject(dc,g_fTitle); SetTextColor(dc,g_theme.accent);
        RECT tr={c.left+S(28),c.top+S(16),c.right-S(28),c.top+S(46)};
        DrawTextW(dc,L"افزودن بیمار جدید",-1,&tr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
        auto lbl=[&](HWND ctl,const wchar_t* t){
            RECT wr; GetWindowRect(ctl,&wr);
            POINT p1={wr.left,wr.top}; ScreenToClient(h,&p1);
            int wcw=wr.right-wr.left;
            RECT lr={p1.x,p1.y-S(20),p1.x+wcw,p1.y-S(2)};
            DrawTextW(dc,t,-1,&lr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); };
        if(s_nps){
            lbl(s_nps->eFirst,L"نام *");
            lbl(s_nps->eLast,L"نام خانوادگی *");
            lbl(s_nps->eNid,L"کد ملی *");
            lbl(s_nps->eFather,L"نام پدر");
            lbl(s_nps->cGender,L"جنسیت");
            lbl(s_nps->eBirth,L"تاریخ تولد");
            lbl(s_nps->eMobile,L"تلفن همراه");
            lbl(s_nps->eLand,L"تلفن ثابت");
            lbl(s_nps->cIns,L"بیمه");
            lbl(s_nps->cSupp,L"بیمه تکمیلی");
            lbl(s_nps->eAddr,L"آدرس");
        }
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps); return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}
static void openNewPatient(HWND owner){
    (void)owner;
    if(s_np&&IsWindow(s_np)) npClose();
    static bool reg=false;
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=npProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=MGPAT_CLASS;
        RegisterClassW(&wc); reg=true; }
    RECT rc; GetClientRect(g_hFrame,&rc);
    POINT org={0,0}; ClientToScreen(g_hFrame,&org);
    s_np=CreateWindowExW(WS_EX_TOPMOST,MGPAT_CLASS,L"",WS_POPUP|WS_VISIBLE,
        org.x,org.y,rc.right,rc.bottom,g_hFrame,NULL,g_hInst,NULL);
    BringWindowToTop(s_np); SetFocus(s_np);
}

// ============================================================================
//  §1.19.0 — «تنظیمات چاپ» (Print Settings). The manager picks a defined
//  section (e.g. پذیرش) from a list, sees a WYSIWYG preview of the design that
//  section currently prints with (enlargeable), and can:
//    • دانلود طرح فعلی  — export the current design as a .aztpl JSON file
//    • بارگذاری طرح     — import a .aztpl JSON file from disk
//    • اعمال            — apply the (imported) design to the selected section
//  After «اعمال», that section prints with the chosen design (reception نسخه
//  included), via printPrintDesign() at the reception desk.
// ============================================================================
#define MGPCFG_CLASS L"AzPrintCfg"
enum { PC_LIST=3601, PC_B_DOWNLOAD, PC_B_UPLOAD, PC_B_APPLY, PC_B_ENLARGE,
       PC_B_DESIGN, PC_B_CLOSE, PC_B_ZOOMIN, PC_B_ZOOMOUT };
struct PrintCfgState {
    HWND list, bDownload, bUpload, bApply, bEnlarge, bDesign, bClose;
    HWND bZoomIn, bZoomOut;
    std::vector<PrintDesign> designs; // v1.99 طرح‌های تعریف‌شده (Default first)
    int  sel = -1;                  // selected list index
    PrintDesign preview;            // currently-resolved design for sel
    bool havePreview = false;
    PrintDesign pending;            // imported-but-not-yet-applied design
    bool havePending = false;
    bool enlarged = false;          // preview occupies the whole card
    double zoom = 1.0;              // v1.22.0: real preview zoom (0.4..4.0)
};
static HWND s_pc=NULL; static PrintCfgState* s_pcs=NULL;
static void pcClose(){ if(s_pc&&IsWindow(s_pc)){ HWND v=s_pc; s_pc=NULL; DestroyWindow(v);} }
static RECT pcCard(HWND h){ RECT rc; GetClientRect(h,&rc);
    int w=S(940),hh=S(640);
    if(w>rc.right-S(20)) w=rc.right-S(20);
    if(hh>rc.bottom-S(20)) hh=rc.bottom-S(20);
    RECT c={(rc.right-w)/2,(rc.bottom-hh)/2,(rc.right+w)/2,(rc.bottom+hh)/2}; return c; }
// preview pane rect (left side of the card; full card when enlarged)
static RECT pcPreviewRect(HWND h){
    RECT c=pcCard(h);
    if(s_pcs && s_pcs->enlarged){
        RECT r={c.left+S(20),c.top+S(64),c.right-S(20),c.bottom-S(64)}; return r; }
    int listW=S(260);
    RECT r={c.left+S(20),c.top+S(64),c.right-S(20)-listW-S(16),c.bottom-S(64)};
    return r;
}
// Render a PrintDesign scaled-to-fit inside `area` (WYSIWYG paper preview).
// ---- v1.21.0: self-contained helpers for the Print-Settings preview --------
// (printer.cpp's equivalents are file-static, so we duplicate compact versions.)
static inline COLORREF pcCR(unsigned int rgb){
    return RGB((rgb>>16)&0xFF,(rgb>>8)&0xFF,rgb&0xFF);   // 0x00RRGGBB → COLORREF
}
struct PcTable {
    int cols=0,rows=0; bool header=false;
    std::vector<double> widths;
    std::vector<std::vector<std::wstring>> cells;
};
static std::wstring pcU8toW(const std::string& s){
    int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),NULL,0);
    std::wstring w(n,0); if(n) MultiByteToWideChar(CP_UTF8,0,s.c_str(),(int)s.size(),&w[0],n);
    return w;
}
static bool pcParseTable(const std::wstring& jsonW, PcTable& t){
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
                    while(true){ ws(); if(p<s.size()&&s[p]==']'){++p;break;} std::string cv; if(rdStr(cv)) rowv.push_back(pcU8toW(cv)); else { rdNum(); rowv.push_back(L""); }
                        ws(); if(p<s.size()&&s[p]==','){++p;continue;} if(p<s.size()&&s[p]==']'){++p;break;} break; }
                    t.cells.push_back(rowv); }
                ws(); if(p<s.size()&&s[p]==','){++p;continue;} if(p<s.size()&&s[p]==']'){++p;break;} break; } } }
        else { ws(); if(p<s.size()&&s[p]=='"'){ std::string tmp; rdStr(tmp); }
            else if(p<s.size()&&s[p]=='{'){ int d2=0; do{ if(s[p]=='{')d2++; else if(s[p]=='}')d2--; ++p; }while(p<s.size()&&d2>0); }
            else if(p<s.size()&&s[p]=='['){ int d2=0; do{ if(s[p]=='[')d2++; else if(s[p]==']')d2--; ++p; }while(p<s.size()&&d2>0); }
            else rdNum(); }
        ws(); if(p<s.size()&&s[p]==','){++p;continue;} ws(); if(p<s.size()&&s[p]=='}'){++p;break;}
        if(p>=s.size())break;
    }
    if(t.cols<=0||t.rows<=0||t.cells.empty()) return false;
    if((int)t.widths.size()!=t.cols) t.widths.assign(t.cols,1.0);
    return true;
}
// §1.51.0 — preview-mode services table. The print pipeline renders dynamic
// rows from the live record; the designer preview has no record, so it shows a
// header + one placeholder row so the operator still sees a real-looking table.
struct PcServicesModel {
    int cols=4; bool header=true;
    std::vector<double> widths;
    std::vector<std::wstring> labels;
};
static bool pcParseServicesModel(const std::wstring& jsonW, PcServicesModel& m){
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
                std::string cv; if(rdStr(cv)) m.labels.push_back(pcU8toW(cv));
                ws(); if(p<s.size()&&s[p]==','){++p;continue;} if(p<s.size()&&s[p]==']'){++p;break;} break; } } }
        else { ws(); if(p<s.size()&&s[p]=='"'){ std::string tmp; rdStr(tmp); }
            else if(p<s.size()&&s[p]=='{'){ int d2=0; do{ if(s[p]=='{')d2++; else if(s[p]=='}')d2--; ++p; }while(p<s.size()&&d2>0); }
            else if(p<s.size()&&s[p]=='['){ int d2=0; do{ if(s[p]=='[')d2++; else if(s[p]==']')d2--; ++p; }while(p<s.size()&&d2>0); }
            else rdNum(); }
        ws(); if(p<s.size()&&s[p]==','){++p;continue;} ws(); if(p<s.size()&&s[p]=='}'){++p;break;}
        if(p>=s.size())break;
    }
    if(m.cols<1) m.cols=4;
    if((int)m.widths.size()!=m.cols) m.widths.assign(m.cols,1.0);
    if(m.labels.empty()){
        m.labels.clear();
        const wchar_t* def[4]={L"نام خدمت",L"شرح خدمت",L"تعداد",L"مبلغ کل"};
        for(int i=0;i<m.cols;++i) m.labels.push_back(i<4?std::wstring(def[i]):L"");
    }
    return true;
}
static void pcDrawServices(HDC dc, const PrintItem& it, const RECT& box, double scPxMm){
    PcServicesModel m; if(!pcParseServicesModel(it.text,m)) return;
    int X0=box.left,Y0=box.top,X1=box.right,Y1=box.bottom;
    int W=X1-X0,H=Y1-Y0; if(W<=0||H<=0) return;
    double sumw=0; for(double w:m.widths) sumw+=w; if(sumw<=0) sumw=m.cols;
    std::vector<int> cx; cx.reserve(m.cols+1); cx.push_back(X1); double acc=0;
    for(int c=0;c<m.cols;++c){
        acc+=m.widths[c]; cx.push_back(c==m.cols-1?X0:X1-(int)(W*(acc/sumw)));
    }

    double fontPt=it.fontPt>0?it.fontPt:8.5;
    int lf=-(int)(fontPt*scPxMm*25.4/72.0+0.5); if(lf>-2) lf=-2;
    HFONT fH=CreateFontW(lf,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,
        it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
    int hh=it.headerH>0?(int)(it.headerH*scPxMm+0.5):(int)(H*0.16);
    if(hh<14) hh=14; if(hh>H) hh=H;
    int rh=it.rowH>0?(int)(it.rowH*scPxMm+0.5):hh;
    if(rh<12) rh=12; if(hh+rh>H) rh=H-hh;
    if(rh<0) rh=0;
    int total=m.header?1:0;
    if(rh>0) ++total;
    std::vector<int> ry; ry.reserve(total+1); ry.push_back(Y0);
    if(m.header) ry.push_back(Y0+hh);
    if(rh>0) ry.push_back((m.header?Y0+hh:Y0)+rh);
    if((int)ry.size()!=total+1){ DeleteObject(fH); return; }

    if(m.header){
        DWORD fill=it.fillTransparent?0xEFEFEFu:(DWORD)it.fillColor;
        int lum=(int)((((fill>>16)&0xFF)*30+((fill>>8)&0xFF)*59+(fill&0xFF)*11)/100);
        RECT hr={cx[m.cols],ry[0],cx[0],ry[1]};
        HBRUSH hb=CreateSolidBrush(pcCR((int)fill)); FillRect(dc,&hr,hb); DeleteObject(hb);
        HGDIOBJ of=SelectObject(dc,fH);
        SetTextColor(dc,lum<140?RGB(255,255,255):pcCR(it.textColor));
        int pad=S(3);
        for(int c=0;c<m.cols;++c){
            std::wstring cell=c<(int)m.labels.size()?m.labels[c]:L"";
            RECT cr={cx[c+1]+pad,ry[0],cx[c]-pad,ry[1]};
            if(cr.right<=cr.left||cr.bottom<=cr.top) continue;
            DrawTextW(dc,cell.c_str(),-1,&cr,
                DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_END_ELLIPSIS|DT_NOPREFIX);
        }
        SelectObject(dc,of);
    }
    DeleteObject(fH);

    // Recordless ready-template preview: show structure without fabricated
    // service names, codes, quantities or prices. One compact blank row keeps
    // the table editable and the unused page visibly blank.
    int bw=(int)(it.borderWidth*scPxMm); if(bw<1) bw=1;
    HPEN pen=CreatePen(PS_SOLID,bw,pcCR(it.borderColor)); HGDIOBJ op=SelectObject(dc,pen);
    for(int c=0;c<=m.cols;++c){ MoveToEx(dc,cx[c],ry[0],0); LineTo(dc,cx[c],ry[total]); }
    for(int rr=0;rr<=total;++rr){ MoveToEx(dc,cx[m.cols],ry[rr],0); LineTo(dc,cx[0],ry[rr]); }
    SelectObject(dc,op); DeleteObject(pen);
}
// preview-mode table: shows raw cell text (no live record substitution)
static void pcDrawTable(HDC dc, const PrintItem& it, const RECT& box, double scPxMm){
    PcTable t; if(!pcParseTable(it.text,t)) return;
    int X0=box.left,Y0=box.top,X1=box.right,Y1=box.bottom;
    int W=X1-X0,H=Y1-Y0; if(W<=0||H<=0) return;
    double sumw=0; for(double w:t.widths) sumw+=w; if(sumw<=0) sumw=t.cols;
    std::vector<int> cx; cx.push_back(X1); double acc=0;
    for(int c=0;c<t.cols;++c){ acc+=t.widths[c]; cx.push_back(X1-(int)(W*(acc/sumw))); }
    std::vector<int> ry; for(int rr=0;rr<=t.rows;++rr) ry.push_back(Y0+(int)((double)H*rr/t.rows));
    if(t.header && t.rows>0){ RECT hr={cx[t.cols],ry[0],cx[0],ry[1]};
        HBRUSH hb=CreateSolidBrush(RGB(238,242,251)); FillRect(dc,&hr,hb); DeleteObject(hb); }
    double fontPt=it.fontPt>0?it.fontPt:9.0;
    int lf=-(int)(fontPt*scPxMm*25.4/72.0 + 0.5);   // pt → px (WYSIWYG)
    if(lf>-2) lf=-2;
    HFONT fN=CreateFontW(lf,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,
        it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
    HFONT fH=CreateFontW(lf,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,
        it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
    int pad=S(3); SetTextColor(dc,pcCR(it.textColor));
    for(int rr=0;rr<t.rows;++rr){ bool hd=(t.header&&rr==0);
        HGDIOBJ of=SelectObject(dc,hd?fH:fN);
        for(int c=0;c<t.cols;++c){ std::wstring cell;
            if(rr<(int)t.cells.size()&&c<(int)t.cells[rr].size()) cell=t.cells[rr][c];
            RECT cr={cx[c+1]+pad,ry[rr],cx[c]-pad,ry[rr+1]};
            if(cr.right<=cr.left||cr.bottom<=cr.top) continue;
            DrawTextW(dc,cell.c_str(),-1,&cr,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_END_ELLIPSIS|DT_NOPREFIX);
        } SelectObject(dc,of); }
    DeleteObject(fN); DeleteObject(fH);
    int bw=1; HPEN pen=CreatePen(PS_SOLID,bw,pcCR(it.borderColor)); HGDIOBJ op=SelectObject(dc,pen);
    for(int c=0;c<=t.cols;++c){ MoveToEx(dc,cx[c],ry[0],0); LineTo(dc,cx[c],ry[t.rows]); }
    for(int rr=0;rr<=t.rows;++rr){ MoveToEx(dc,cx[t.cols],ry[rr],0); LineTo(dc,cx[0],ry[rr]); }
    SelectObject(dc,op); DeleteObject(pen);
}

static void pcPaintDesign(HDC dc, const RECT& area, const PrintDesign& d, double zoom){
    double pw=d.paperW, ph=d.paperH;
    if(pw<=0||ph<=0){ if(!Paper_Dims(d.paper,pw,ph)){ pw=148; ph=210; }
        if(d.orientation==1) std::swap(pw,ph); }
    int aw=area.right-area.left, ah=area.bottom-area.top;
    if(aw<10||ah<10) return;
    // §1.52.0 — RESPONSIVE A4→A5 AUTO-SCALE (must mirror printPrintDesign exactly).
    // Built-in templates are authored in A4 mm coordinates; if the active paper is
    // smaller than the items' bounding box, shrink the whole design proportionally
    // so WYSIWYG preview matches the printed output.
    double authW=pw, authH=ph;
    for(const auto& it:d.items){ double ex=it.x+it.w; if(ex>authW)authW=ex;
                                  double ey=it.y+it.h; if(ey>authH)authH=ey; }
    double pscale=1.0;
    if(authW>pw+0.01 || authH>ph+0.01){
        double fx=pw/authW, fy=ph/authH;
        pscale = fx<fy ? fx : fy; if(pscale>1.0) pscale=1.0;
    }
    double sc = (aw/pw < ah/ph) ? aw/pw : ah/ph;     // mm→px fit
    if(zoom<=0) zoom=1.0;
    sc *= zoom;                                       // v1.22.0: apply zoom
    // clip drawing to the preview pane so a zoomed-in page never spills out
    HRGN clip=CreateRectRgn(area.left,area.top,area.right,area.bottom);
    SelectClipRgn(dc,clip);
    int pgW=(int)(pw*sc), pgH=(int)(ph*sc);
    int px=area.left+(aw-pgW)/2, py=area.top+(ah-pgH)/2;
    // paper sheet (white) + shadow
    RECT sh={px+S(4),py+S(4),px+pgW+S(4),py+pgH+S(4)};
    HBRUSH shB=CreateSolidBrush(RGB(0,0,0)); FillRect(dc,&sh,shB); DeleteObject(shB);
    RECT pg={px,py,px+pgW,py+pgH};
    HBRUSH wB=CreateSolidBrush(RGB(255,255,255)); FillRect(dc,&pg,wB); DeleteObject(wB);
    { HPEN bp=CreatePen(PS_SOLID,1,RGB(180,180,180)); HGDIOBJ o=SelectObject(dc,bp);
      HGDIOBJ ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
      Rectangle(dc,px,py,px+pgW,py+pgH);
      SelectObject(dc,ob); SelectObject(dc,o); DeleteObject(bp); }
    // apply pscale so an A4-authored design previews correctly on an A5 sheet
    auto mmX=[&](double mm){ return px+(int)(mm*pscale*sc); };
    auto mmY=[&](double mm){ return py+(int)(mm*pscale*sc); };
    // items in z-order
    std::vector<const PrintItem*> ord;
    for(const auto& it:d.items) ord.push_back(&it);
    for(size_t i=1;i<ord.size();++i){ const PrintItem* k=ord[i]; size_t j=i;
        while(j>0 && ord[j-1]->z > k->z){ ord[j]=ord[j-1]; --j; } ord[j]=k; }
    SetBkMode(dc,TRANSPARENT);
    for(const PrintItem* pit:ord){
        const PrintItem& it=*pit;
        int x0=mmX(it.x), y0=mmY(it.y), x1=mmX(it.x+it.w), y1=mmY(it.y+it.h);
        if(it.type==PIT_TABLE){
            RECT rr={x0,y0,x1,y1}; pcDrawTable(dc,it,rr,sc*pscale);
        } else if(it.type==PIT_SERVICES){
            // §1.51.0: dynamic services list (preview shows header + placeholder row)
            RECT rr={x0,y0,x1,y1}; pcDrawServices(dc,it,rr,sc*pscale);
        } else if(it.type==PIT_HLINE){
            HPEN p=CreatePen(PS_SOLID,1,pcCR(it.borderColor)); HGDIOBJ o=SelectObject(dc,p);
            MoveToEx(dc,x0,y0,0); LineTo(dc,x1,y0); SelectObject(dc,o); DeleteObject(p);
        } else if(it.type==PIT_VLINE){
            HPEN p=CreatePen(PS_SOLID,1,pcCR(it.borderColor)); HGDIOBJ o=SelectObject(dc,p);
            MoveToEx(dc,x0,y0,0); LineTo(dc,x0,y1); SelectObject(dc,o); DeleteObject(p);
        } else if(it.type==PIT_RECT||it.type==PIT_FRAME||it.type==PIT_LOGO||
                  it.type==PIT_PHOTO||it.type==PIT_QR||it.type==PIT_IMAGE){
            bool isImgItem=(it.type==PIT_LOGO||it.type==PIT_PHOTO||it.type==PIT_IMAGE);
            // fill rect/frame background when not transparent (WYSIWYG)
            if((it.type==PIT_RECT||it.type==PIT_FRAME) && !it.fillTransparent){
                RECT fr={x0,y0,x1,y1}; HBRUSH fb=CreateSolidBrush(pcCR(it.fillColor));
                FillRect(dc,&fr,fb); DeleteObject(fb);
            }
            // real image (uploaded logo / photo / image) → draw the picture
            bool drawn=false;
            if(isImgItem && !it.imgPath.empty()){
                RECT rr={x0,y0,x1,y1}; drawn=gpDrawImageRectAny(dc,it.imgPath,rr);
            }
            if(!drawn){
                HPEN p=CreatePen(PS_SOLID,1,pcCR(it.borderColor)); HGDIOBJ o=SelectObject(dc,p);
                HGDIOBJ ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
                Rectangle(dc,x0,y0,x1,y1);
                SelectObject(dc,ob); SelectObject(dc,o); DeleteObject(p);
                if(it.type==PIT_LOGO||it.type==PIT_QR||it.type==PIT_PHOTO||it.type==PIT_IMAGE){
                    const wchar_t* ph2=(it.type==PIT_LOGO)?L"لوگو":(it.type==PIT_QR?L"QR":(it.type==PIT_IMAGE?L"تصویر":L"عکس"));
                    RECT rr={x0,y0,x1,y1}; SetTextColor(dc,RGB(150,150,150));
                    HFONT f=CreateFontW(-S(11),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,
                        CLEARTYPE_QUALITY,0,L"Vazirmatn");
                    HGDIOBJ of=SelectObject(dc,f);
                    DrawTextW(dc,ph2,-1,&rr,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
                    SelectObject(dc,of); DeleteObject(f);
                }
            }
        } else { // text: LABEL / FIELD — show sample/text
            std::wstring s=it.text;
            if(it.type==PIT_FIELD){ std::wstring v=it.field.empty()?L"":(L"["+it.field+L"]");
                s=it.prefix+(it.text.empty()?v:it.text)+it.suffix; }
            else s=it.prefix+it.text+it.suffix;
            if(s.empty()) continue;
            // WYSIWYG font size: pt → px.  sc is px-per-mm, and 1pt = 1/72 in =
            // 25.4/72 mm, so px = pt * sc * (25.4/72).  (The old 3.78/2.83 factor
            // was ~3.8× too large and made text overlap.)  §1.52.0: also apply
            // the responsive A4→A5 pscale so fonts shrink with the boxes.
            int lf=-(int)(it.fontPt*pscale*sc*25.4/72.0 + 0.5);
            if(lf>-2) lf=-2;
            RECT rr={x0,y0,x1,y1};
            HFONT f=CreateFontW(lf,0,0,0,it.bold?FW_BOLD:FW_NORMAL,it.italic?1:0,0,0,
                DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,
                it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
            HGDIOBJ of=SelectObject(dc,f); SetTextColor(dc,pcCR(it.textColor));
            UINT al=(it.align==1)?DT_CENTER:(it.align==2)?DT_LEFT:DT_RIGHT;
            UINT dirf=(it.dir==1)?0:DT_RTLREADING;
            if(it.dir==2) al=DT_CENTER;
            DrawTextW(dc,s.c_str(),-1,&rr,al|DT_TOP|DT_WORDBREAK|dirf|DT_NOPREFIX);
            SelectObject(dc,of); DeleteObject(f);
        }
    }
    SelectClipRgn(dc,NULL); DeleteObject(clip);
}
// (re)load the resolved design for the selected section into preview
static void pcLoadPreview(){
    if(!s_pcs) return;
    s_pcs->havePreview=false; s_pcs->havePending=false; s_pcs->enlarged=false;
    s_pcs->zoom=1.0;
    if(s_pcs->sel<0 || s_pcs->sel>=(int)s_pcs->designs.size()) return;
    s_pcs->preview=s_pcs->designs[s_pcs->sel];
    s_pcs->havePreview=true;
}
static void pcLayout(HWND h){
    if(!s_pcs) return; RECT c=pcCard(h);
    int listW=S(260), listX=c.right-S(20)-listW;
    int listTop=c.top+S(64), listH=c.bottom-S(64)-S(60)-listTop;
    MoveWindow(s_pcs->list, listX, listTop, listW, listH, TRUE);
    int by=c.bottom-S(52), bw=S(108), gap=S(7), x=c.right-S(20);
    int zw=S(40);
    MoveWindow(s_pcs->bClose,   x-bw, by, bw, S(38), TRUE); x-=bw+gap;
    MoveWindow(s_pcs->bApply,   x-bw, by, bw, S(38), TRUE); x-=bw+gap;
    MoveWindow(s_pcs->bUpload,  x-bw, by, bw, S(38), TRUE); x-=bw+gap;
    MoveWindow(s_pcs->bDownload,x-bw, by, bw, S(38), TRUE); x-=bw+gap;
    MoveWindow(s_pcs->bDesign,  x-bw, by, bw, S(38), TRUE); x-=bw+gap;
    MoveWindow(s_pcs->bEnlarge, x-bw, by, bw, S(38), TRUE); x-=bw+gap;
    MoveWindow(s_pcs->bZoomIn,  x-zw, by, zw, S(38), TRUE); x-=zw+gap;
    MoveWindow(s_pcs->bZoomOut, x-zw, by, zw, S(38), TRUE);
    for(HWND w:{s_pcs->list,s_pcs->bClose,s_pcs->bApply,s_pcs->bUpload,
                s_pcs->bDownload,s_pcs->bDesign,s_pcs->bEnlarge,
                s_pcs->bZoomIn,s_pcs->bZoomOut}) ShowWindow(w,SW_SHOW);
}
static std::wstring pcDesignsDir(){
    std::wstring d=dataDir()+L"\\exported_designs";
    CreateDirectoryW(d.c_str(),NULL); return d;
}
static void pcDownload(HWND h){
    if(!s_pcs||!s_pcs->havePreview){
        MessageBoxW(h,L"ابتدا یک بخش را انتخاب کنید.",L"دانلود طرح",MB_OK|MB_ICONINFORMATION); return; }
    const PrintDesign& d = s_pcs->havePending? s_pcs->pending : s_pcs->preview;
    std::wstring suggest = (s_pcs->sel>=0? s_pcs->designs[s_pcs->sel].name : L"design")+L".aztpl";
    wchar_t fn[MAX_PATH]={0}; wcsncpy(fn,suggest.c_str(),MAX_PATH-1);
    OPENFILENAMEW ofn={sizeof(ofn)}; ofn.hwndOwner=h;
    ofn.lpstrFilter=L"طرح چاپ درمان‌پلاس\0*.aztpl\0همه\0*.*\0";
    ofn.lpstrFile=fn; ofn.nMaxFile=MAX_PATH; ofn.lpstrDefExt=L"aztpl";
    ofn.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
    if(!GetSaveFileNameW(&ofn)) return;
    std::string json=Design_ToJson(d);
    // write UTF-8 bytes
    HANDLE hf=CreateFileW(fn,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(hf==INVALID_HANDLE_VALUE){ MessageBoxW(h,L"نوشتن فایل ناموفق بود.",L"دانلود طرح",MB_OK|MB_ICONERROR); return; }
    DWORD wr=0; WriteFile(hf,json.data(),(DWORD)json.size(),&wr,NULL); CloseHandle(hf);
    MessageBoxW(h,L"طرح فعلی با موفقیت ذخیره شد.",L"دانلود طرح",MB_OK|MB_ICONINFORMATION);
}
static void pcUpload(HWND h){
    if(!s_pcs) return;
    wchar_t fn[MAX_PATH]={0};
    OPENFILENAMEW ofn={sizeof(ofn)}; ofn.hwndOwner=h;
    ofn.lpstrFilter=L"طرح چاپ درمان‌پلاس\0*.aztpl;*.json\0همه\0*.*\0";
    ofn.lpstrFile=fn; ofn.nMaxFile=MAX_PATH;
    ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
    if(!GetOpenFileNameW(&ofn)) return;
    // read bytes
    HANDLE hf=CreateFileW(fn,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(hf==INVALID_HANDLE_VALUE){ MessageBoxW(h,L"خواندن فایل ناموفق بود.",L"بارگذاری طرح",MB_OK|MB_ICONERROR); return; }
    DWORD sz=GetFileSize(hf,NULL); std::string buf; buf.resize(sz);
    DWORD rd=0; ReadFile(hf,&buf[0],sz,&rd,NULL); CloseHandle(hf); buf.resize(rd);
    // strip a UTF-8 BOM if present
    if(buf.size()>=3 && (unsigned char)buf[0]==0xEF && (unsigned char)buf[1]==0xBB && (unsigned char)buf[2]==0xBF)
        buf.erase(0,3);
    PrintDesign d; std::wstring err; bool ok=false;
    // v1.21.1: accept BOTH file shapes —
    //   (a) native AZTEMPLATE/1 (written by «دانلود طرح» here), and
    //   (b) the browser designer's JS-shaped JSON (written by «دانلود» in the
    //       web designer). Try native first, then fall back to the web parser.
    if(buf.find("AZTEMPLATE/1")!=std::string::npos){
        ok=Design_FromJson(buf,d,err);
    } else {
        ok=Design_FromWebJson(buf,d);
        if(!ok) err=L"ساختار فایل طرح شناسایی نشد (نه AZTEMPLATE/1 و نه طرح طراح وب).";
    }
    if(ok && d.items.empty()){ ok=false; err=L"فایل طرح هیچ آیتمی ندارد."; }
    if(!ok){
        MessageBoxW(h,(L"فایل طرح نامعتبر است.\n"+err).c_str(),L"بارگذاری طرح",MB_OK|MB_ICONERROR); return; }
    // make sure paper dimensions are resolved for the preview
    if(d.paperW<=0||d.paperH<=0){ double pw,ph; if(Paper_Dims(d.paper,pw,ph)){ d.paperW=pw; d.paperH=ph;
        if(d.orientation==1) std::swap(d.paperW,d.paperH); } else { d.paperW=148; d.paperH=210; } }
    s_pcs->pending=d; s_pcs->havePending=true; s_pcs->preview=d; s_pcs->havePreview=true;
    s_pcs->enlarged=false;
    MessageBoxW(h,L"طرح بارگذاری شد. برای اعمال روی بخش، «اعمال» را بزنید.",L"بارگذاری طرح",MB_OK|MB_ICONINFORMATION);
    InvalidateRect(h,NULL,FALSE);
}
static void pcApply(HWND h){
    if(!s_pcs||s_pcs->sel<0||s_pcs->sel>=(int)s_pcs->designs.size()){
        MessageBoxW(h,L"ابتدا یک طرح را انتخاب کنید.",L"اعمال طرح",MB_OK|MB_ICONINFORMATION); return; }
    PrintDesign d = s_pcs->havePending ? s_pcs->pending : s_pcs->designs[s_pcs->sel];
    if(s_pcs->havePending){
        d.id=0;
        if(d.name.empty()) d.name=L"طرح وارداتی";
        int newId=Designs_Insert(d);
        if(newId<=0){ MessageBoxW(h,L"ثبت طرح ناموفق بود.",L"اعمال طرح",MB_OK|MB_ICONERROR); return; }
        MachineDesign_Set(newId);
        s_pcs->havePending=false;
    } else {
        MachineDesign_Set(d.id);
    }
    pcLoadPreview();
    MessageBoxW(h,L"این طرح روی چاپگر همین دستگاه اعمال شد.",
        L"اعمال طرح",MB_OK|MB_ICONINFORMATION);
    InvalidateRect(h,NULL,FALSE);
}
static LRESULT CALLBACK pcProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE:{
        s_pcs=new PrintCfgState();
        s_pcs->list=CreateWindowExW(WS_EX_CLIENTEDGE,L"LISTBOX",L"",
            WS_CHILD|WS_TABSTOP|WS_VSCROLL|LBS_NOTIFY,0,0,10,10,h,(HMENU)PC_LIST,g_hInst,0);
        SendMessageW(s_pcs->list,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        Designs_Init();
        std::vector<PrintDesign> b; Designs_Builtins(b);
        std::vector<PrintDesign> u; Designs_User(u);
        if(!b.empty()){
            PrintDesign def=b[0]; def.name=L"پیش‌فرض";
            s_pcs->designs.push_back(def);
            for(size_t i=1;i<b.size();++i) s_pcs->designs.push_back(b[i]);
        }
        for(auto& d:u) s_pcs->designs.push_back(d);
        for(auto& d:s_pcs->designs)
            SendMessageW(s_pcs->list,LB_ADDSTRING,0,(LPARAM)d.name.c_str());
        s_pcs->bZoomOut =createFlatButton(h,PC_B_ZOOMOUT, L"−",         ICO_NONE,   BS_OUTLINE,0,0,10,10);
        s_pcs->bZoomIn  =createFlatButton(h,PC_B_ZOOMIN,  L"+",         ICO_NONE,   BS_OUTLINE,0,0,10,10);
        s_pcs->bEnlarge =createFlatButton(h,PC_B_ENLARGE, L"تمام‌صفحه", ICO_REFRESH,BS_OUTLINE,0,0,10,10);
        s_pcs->bDesign  =createFlatButton(h,PC_B_DESIGN,  L"طراحی",     ICO_GEAR,   BS_OUTLINE,0,0,10,10);
        s_pcs->bDownload=createFlatButton(h,PC_B_DOWNLOAD,L"دانلود طرح",ICO_SAVE,   BS_OUTLINE,0,0,10,10);
        s_pcs->bUpload  =createFlatButton(h,PC_B_UPLOAD,  L"بارگذاری",  ICO_ID,     BS_OUTLINE,0,0,10,10);
        s_pcs->bApply   =createFlatButton(h,PC_B_APPLY,   L"اعمال",     ICO_PLUS,   BS_PRIMARY,0,0,10,10);
        s_pcs->bClose   =createFlatButton(h,PC_B_CLOSE,   L"بستن",      ICO_X,      BS_OUTLINE,0,0,10,10);
        HWND cardBtns[]={s_pcs->bZoomOut,s_pcs->bZoomIn,s_pcs->bEnlarge,
            s_pcs->bDesign,s_pcs->bDownload,s_pcs->bUpload,s_pcs->bApply,s_pcs->bClose};
        for(HWND b:cardBtns) setFlatButtonBg(b,g_theme.surface);
        if(!s_pcs->designs.empty()){  SendMessageW(s_pcs->list,LB_SETCURSEL,0,0);
            s_pcs->sel=0; pcLoadPreview(); }
        pcLayout(h);
        return 0; }
    case WM_SIZE: pcLayout(h); return 0;
    case WM_NCDESTROY: if(s_pcs){ delete s_pcs; s_pcs=NULL; } s_pc=NULL;
        if(g_hManage) InvalidateRect(g_hManage,NULL,TRUE); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_CTLCOLORLISTBOX:{ HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_COMMAND:{ int id=LOWORD(w), code=HIWORD(w);
        if(id==PC_LIST && code==LBN_SELCHANGE){
            s_pcs->sel=(int)SendMessageW(s_pcs->list,LB_GETCURSEL,0,0);
            pcLoadPreview(); InvalidateRect(h,NULL,FALSE);
        } else if(id==PC_B_ENLARGE){ s_pcs->enlarged=!s_pcs->enlarged;
            pcLayout(h); InvalidateRect(h,NULL,FALSE);
        } else if(id==PC_B_ZOOMIN){ s_pcs->zoom*=1.25; if(s_pcs->zoom>4.0)s_pcs->zoom=4.0;
            InvalidateRect(h,NULL,FALSE);
        } else if(id==PC_B_ZOOMOUT){ s_pcs->zoom/=1.25; if(s_pcs->zoom<0.4)s_pcs->zoom=0.4;
            InvalidateRect(h,NULL,FALSE);
        } else if(id==PC_B_DOWNLOAD){ pcDownload(h);
        } else if(id==PC_B_UPLOAD){ pcUpload(h);
        } else if(id==PC_B_APPLY){ pcApply(h);
        } else if(id==PC_B_DESIGN){
            // open the full visual designer for this section, then refresh
            if(s_pcs->sel>=0 && s_pcs->sel<(int)s_pcs->designs.size()){
                HWND owner=g_hFrame; pcClose();
                PrintDesigner_Open(owner);
                return 0;
            }
        } else if(id==PC_B_CLOSE){ pcClose(); }
        return 0; }
    case WM_KEYDOWN: if(w==VK_ESCAPE){ pcClose(); return 0; } break;
    case WM_PAINT:{ PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        { HBRUSH sb=CreateSolidBrush(g_dark?RGB(6,9,14):RGB(28,36,48));
          FillRect(dc,&rc,sb); DeleteObject(sb); }
        gpFillAlpha(dc,rc,0,g_dark?RGB(0,0,0):RGB(20,28,40),120);
        RECT c=pcCard(h);
        gpShadowCached(dc0,dc,rc.right,rc.bottom,c,S(20),S(22),80);
        gpGradRoundRectBg(dc,c,S(18),g_theme.surfaceTop,g_theme.surface,g_theme.border,
            g_dark?RGB(6,9,14):RGB(28,36,48));
        SetBkMode(dc,TRANSPARENT);
        SelectObject(dc,g_fTitle); SetTextColor(dc,g_theme.accent);
        RECT tr={c.left+S(28),c.top+S(16),c.right-S(28),c.top+S(46)};
        DrawTextW(dc,L"تنظیمات چاپ — طرح‌های تعریف‌شده",-1,&tr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        // preview pane background
        RECT pr=pcPreviewRect(h);
        gpRoundRectBg(dc,pr,S(12),g_theme.bg,g_theme.border,g_theme.surface);
        if(s_pcs && s_pcs->havePreview){
            RECT inner={pr.left+S(10),pr.top+S(10),pr.right-S(10),pr.bottom-S(10)};
            pcPaintDesign(dc,inner,s_pcs->preview,s_pcs->zoom);
            // zoom % badge
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.accent);
            wchar_t zb[32]; swprintf(zb,32,L"بزرگ‌نمایی: %d%%",(int)(s_pcs->zoom*100));
            RECT zr={pr.left+S(12),pr.top+S(6),pr.left+S(170),pr.top+S(26)};
            DrawTextW(dc,toFaDigits(zb).c_str(),-1,&zr,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        } else {
            SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
            DrawTextW(dc,L"یک طرح را انتخاب کنید تا پیش‌نمایش آن نمایش داده شود.",-1,&pr,
                DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        }
        // list caption
        if(!(s_pcs&&s_pcs->enlarged)){
            SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
            int listW=S(260), listX=c.right-S(20)-listW;
            RECT lc={listX,c.top+S(44),listX+listW,c.top+S(62)};
            DrawTextW(dc,L"طرح‌های تعریف‌شده",-1,&lc,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            if(s_pcs&&s_pcs->havePending){
                SetTextColor(dc,g_theme.warn);
                RECT pdR={pr.left,pr.bottom-S(2),pr.right,pr.bottom+S(18)};
                (void)pdR;
            }
        }
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps); return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}
static void openPrintCfg(HWND owner){
    (void)owner;
    if(s_pc&&IsWindow(s_pc)) pcClose();
    static bool reg=false;
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=pcProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=MGPCFG_CLASS;
        RegisterClassW(&wc); reg=true; }
    RECT rc; GetClientRect(g_hFrame,&rc);
    POINT org={0,0}; ClientToScreen(g_hFrame,&org);
    s_pc=CreateWindowExW(WS_EX_TOPMOST,MGPCFG_CLASS,L"",WS_POPUP|WS_VISIBLE,
        org.x,org.y,rc.right,rc.bottom,g_hFrame,NULL,g_hInst,NULL);
    BringWindowToTop(s_pc); SetFocus(s_pc);
}

// ============================================================================
//  DASHBOARD shell — left nav rail + content pages
// ============================================================================
static int  mgNavW(){ return S(220); }     // sidebar width
static int  mgHeadH(){ return S(64); }      // content header height
static const wchar_t* MG_NAV_LABELS[PG_COUNT] = {
    L"داشبورد", L"بیماران", L"بخش‌ها", L"مدیریت خدمات", L"پزشکان", L"کارمندان",
    L"پیام به کارمندان", L"درخواست‌ها", L"پیام‌های ذخیره‌شده",
    L"پشتیبان‌گیری", L"تنظیمات چاپ", L"تعریف شیفت کاری"
};
static int MG_NAV_ICONS[PG_COUNT] = {
    ICO_TAB, ICO_ID, ICO_SHIELD, ICO_PLUS, ICO_CROSS_MED, ICO_USER, ICO_BELL,
    ICO_GEAR, ICO_SAVE, ICO_REFRESH, ICO_GEAR, ICO_CLOCK
};
//  content area to the LEFT of the RTL nav rail (rail is on the RIGHT)
static RECT mgContentRect(HWND h){
    RECT rc; GetClientRect(h,&rc);
    RECT r={rc.left+S(8), rc.top+S(8), rc.right-mgNavW()-S(8), rc.bottom-S(8)};
    return r;
}

// ----------------------------------------------------------------------------
//  v1.63.0 SHARED LIST-ROW / CARD SHELL.
//  Every management list (sections, employees, patients, services, requests,
//  notes) drew its own `gpRoundRectBg(surface / surface2, border / accent)`
//  rectangle. That is why the whole panel looked flat: a row at rest and a
//  hovered row differed only by two colours, with no depth, no gradient and
//  no leading accent. This single helper gives them all one modern language:
//    * at rest — a faint surface gradient with a hairline border,
//    * on hover — an elevation shadow, an accent-warmed border, and a bright
//      3 px accent indicator on the RTL (right) leading edge,
//  and because it is ONE function the look can never drift between pages.
// ----------------------------------------------------------------------------
static void mgRowShell(HDC dc, RECT r, bool hot, int rad, COLORREF accent){
    if(hot){
        gpShadowColor(dc,r,rad,S(7),84,accent);
        gpGradRoundRectBg(dc,r,rad,
            blendColor(g_theme.surfaceTop,g_theme.surface2,45),
            g_theme.surface2, blendColor(g_theme.border,accent,60), g_theme.bg);
        RECT li={r.right-S(5),r.top+S(8),r.right-S(2),r.bottom-S(8)};
        if(li.bottom>li.top) gpRoundRect(dc,li,S(2),accent,CLR_INVALID);
    } else {
        gpGradRoundRectBg(dc,r,rad,
            blendColor(g_theme.surfaceTop,g_theme.surface,55),
            g_theme.surface, g_theme.border, g_theme.bg);
    }
}
//  v1.63.0: the standard management empty state — a tinted icon medallion, a
//  headline and a hint, centred as one block inside `a`. Replaces the lone
//  grey sentence that made every empty page look broken.
static void mgEmptyState(HDC dc, RECT a, int icon, const wchar_t* head,
                         const wchar_t* hint){
    int cx=(a.left+a.right)/2;
    int top=a.top+mgHeadH();
    int cy=(top+a.bottom)/2;
    int med=S(88), gap=S(20), hl=S(32), sl=S(24);
    int blockH=med+gap+hl+S(6)+sl;
    int by=cy-blockH/2; if(by<top+S(16)) by=top+S(16);
    RECT mr={cx-med/2,by,cx+med/2,by+med};
    COLORREF medFill=blendColor(g_theme.surface,g_infoAccent,g_dark?25:18);
    gpRoundRectBg(dc,mr,med/2,medFill,
                  blendColor(g_theme.border,g_infoAccent,42),g_theme.surface);
    int isz=(med*44)/100;
    RECT ir={cx-isz/2,by+(med-isz)/2,cx+isz/2,by+(med-isz)/2+isz};
    drawIcon(dc,icon,ir,blendColor(g_infoAccent,g_theme.text,12),S(2)+1);
    SelectObject(dc,g_fTitle); SetTextColor(dc,g_theme.text);
    RECT hr={a.left+S(20),by+med+gap,a.right-S(20),by+med+gap+hl};
    DrawTextW(dc,head,-1,&hr,
        DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    if(hint && *hint){
        SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
        RECT sr={a.left+S(20),hr.bottom+S(6),a.right-S(20),hr.bottom+S(6)+sl};
        DrawTextW(dc,hint,-1,&sr,
            DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    }
}
// The compose block at the top of the saved-messages page:
//   caption line  +  editor (multi-line)  +  hint line.
// A single source of truth so the editor, its buttons, the hint and the note
// list below them never overlap. Declared early so mgLayout() can use them.
static int mgSavedComposeTop(HWND h){ RECT a=mgContentRect(h); return a.top+mgHeadH()+S(12); }
static int mgSavedCaptionH(){ return S(24); }
static int mgSavedEditorH(){ return S(74); }
static int mgSavedHintH(){ return S(22); }
static int mgSavedComposeBottom(HWND h){
    return mgSavedComposeTop(h)+mgSavedCaptionH()+mgSavedEditorH()+S(8)+mgSavedHintH();
}
static RECT mgNavItemRect(HWND h, int i){
    RECT rc; GetClientRect(h,&rc);
    int x=rc.right-mgNavW()+S(12), w=mgNavW()-S(24);
    int y=rc.top+S(96)+i*(S(48)+S(6));
    RECT r={x,y,x+w,y+S(48)}; return r;
}

// content geometry within mgContentRect ------------------------------------
static int mgCols(int W){ int c=(W-S(24))/S(248); return c<1?1:c; }
static RECT mgSecCardRect(HWND h, int idx){
    RECT a=mgContentRect(h);
    int innerW=a.right-a.left-S(24);
    int cols=innerW/S(248); if(cols<1) cols=1;
    int cw=S(232), ch=S(124), gap=S(16);
    int row=idx/cols, col=idx%cols;
    int x=a.right-S(12)-(col+1)*cw-col*gap;     // RTL: first on the right
    int y=a.top+mgHeadH()+S(58)+row*(ch+gap);
    RECT r={x,y,x+cw,y+ch}; return r;
}
static int mgEmpRowH(){ return S(58); }
static RECT mgEmpRowRect(HWND h, int idx){
    RECT a=mgContentRect(h);
    int y=a.top+mgHeadH()+S(58)+idx*(mgEmpRowH()+S(8));
    RECT r={a.left+S(12),y,a.right-S(12),y+mgEmpRowH()}; return r;
}
// dashboard quick-action tiles + summary cards
static RECT mgSummaryRect(HWND h, int i){      // i=0..3 across the top
    RECT a=mgContentRect(h);
    int n=4, gap=S(14), x0=a.left+S(12);
    int totW=a.right-a.left-S(24);
    int cw=(totW-gap*(n-1))/n;
    int x=a.right-S(12)-(i+1)*cw-i*gap;        // RTL
    int y=a.top+mgHeadH()+S(16);
    RECT r={x,y,x+cw,y+S(100)}; return r;
}
static RECT mgQuickRect(HWND h, int i){        // i=0..3 quick-action buttons
    RECT a=mgContentRect(h);
    int n=4, gap=S(14);
    int totW=a.right-a.left-S(24);
    int cw=(totW-gap*(n-1))/n;
    int x=a.right-S(12)-(i+1)*cw-i*gap;
    int y=a.top+mgHeadH()+S(162);
    RECT r={x,y,x+cw,y+S(108)}; return r;
}

// ---- layout: show/hide controls per page -----------------------------------
static void mgLayout(HWND h){
    if(!s_mg) return;
    RECT a=mgContentRect(h);
    int top=a.top+mgHeadH()+S(12);
    int rEdge=a.right-S(12), fh=S(34);
    // hide everything first
    HWND all[]={s_mg->eName,s_mg->eMgr,s_mg->eId,s_mg->bAdd,s_mg->bAutoId,
        s_mg->bSecNew,s_mg->bSecBack,s_mg->bAbc,s_mg->bNew,s_mg->bCreated,
        s_mg->bAddEmp,s_mg->cDept,s_mg->bMsg,s_mg->bReqs,s_mg->bProfReqs,
        s_mg->eNote,s_mg->bNoteAttach,s_mg->bNoteSend,
        s_mg->ptSearch,s_mg->ptList,s_mg->ptAdd,
        s_mg->svCode,s_mg->svName,s_mg->svCat,s_mg->svDept,s_mg->svPrice,
        s_mg->svIns,s_mg->svStatus,s_mg->svDesc,s_mg->svShort,s_mg->svLoving,
        s_mg->svEquiv,s_mg->svSrvId,s_mg->svRevGrp,s_mg->svHnId,s_mg->svFree,
        s_mg->svGov,s_mg->svFreeOld,s_mg->svGovOld,s_mg->svSave,s_mg->svEdit,
        s_mg->svDelete,s_mg->svClear,s_mg->svSearch,s_mg->svList,
        s_mg->dcDept,s_mg->dcType,s_mg->dcCode,s_mg->dcActiveChk,s_mg->dcMedId,
        s_mg->dcPrefix,s_mg->dcFirst,s_mg->dcLast,s_mg->dcNid,s_mg->dcMobile,
        s_mg->dcEmail,s_mg->dcAddress,s_mg->dcFranchise,s_mg->dcPrintChk,
        s_mg->dcSpec,s_mg->dcInsSpec,s_mg->dcDegree,s_mg->dcContract,s_mg->dcEmerg,
        s_mg->dcPerfChk,s_mg->dcAcct,s_mg->dcSave,s_mg->dcEdit,s_mg->dcDelete,s_mg->dcClear,
        s_mg->dcSearch,s_mg->dcList,
        // v2.01 (Part F2): shift management controls
        s_mg->shName,s_mg->shStart,s_mg->shEnd,s_mg->shSave,s_mg->shDelete,
        s_mg->shClear,s_mg->shList};
    for(HWND w:all) if(w) ShowWindow(w,SW_HIDE);

    if(s_mg->page==PG_PATIENTS){
        // «افزودن بیمار» button on the LEFT, search box fills the rest (RTL), then
        // the list fills the remaining height below.
        int y=top, addW=S(150);
        MoveWindow(s_mg->ptAdd, a.left+S(12), y, addW, fh, TRUE);
        int searchL=a.left+S(12)+addW+S(10);
        MoveWindow(s_mg->ptSearch, searchL, y, rEdge-searchL, fh, TRUE);
        int ly=y+fh+S(10);
        MoveWindow(s_mg->ptList, a.left+S(12), ly,
                   (a.right-a.left)-S(24), a.bottom-ly-S(8), TRUE);
        ShowWindow(s_mg->ptSearch,SW_SHOW);
        ShowWindow(s_mg->ptList,SW_SHOW);
        ShowWindow(s_mg->ptAdd,SW_SHOW);
    } else if(s_mg->page==PG_SECTS){
        if(s_mg->secAddMode){
            // «ایجاد بخش» add form: name • manager • id • auto-checkbox • add
            int y=top, lh=S(22);
            int w1=S(240);
            MoveWindow(s_mg->eName, rEdge-w1, y+lh, w1, fh, TRUE);
            MoveWindow(s_mg->eMgr,  rEdge-w1, y+lh+fh+S(20)+lh, w1, fh, TRUE);
            int y3=y+lh+fh+S(20)+lh+fh+S(20);
            MoveWindow(s_mg->bAutoId, rEdge-w1, y3, w1, S(28), TRUE);
            MoveWindow(s_mg->eId,    rEdge-w1, y3+S(34)+lh, w1, fh, TRUE);
            MoveWindow(s_mg->bAdd,   rEdge-w1, y3+S(34)+lh+fh+S(20), S(160), S(40), TRUE);
            MoveWindow(s_mg->bSecBack, a.left+S(12), top, S(120), S(36), TRUE);
            ShowWindow(s_mg->eName,SW_SHOW); ShowWindow(s_mg->eMgr,SW_SHOW);
            ShowWindow(s_mg->bAutoId,SW_SHOW); ShowWindow(s_mg->bAdd,SW_SHOW);
            ShowWindow(s_mg->bSecBack,SW_SHOW);
            ShowWindow(s_mg->eId, s_mg->autoId?SW_HIDE:SW_SHOW);
        } else {
            // sections grid + «ایجاد بخش» button (top-left)
            MoveWindow(s_mg->bSecNew, a.left+S(12), top, S(160), S(38), TRUE);
            ShowWindow(s_mg->bSecNew,SW_SHOW);
        }
    } else if(s_mg->page==PG_EMPS){
        // filter row: sort buttons (right) + dept combo (right of them) + add (left)
        int y=top;
        int x=rEdge;
        MoveWindow(s_mg->bAbc,     x-S(110), y, S(110), fh, TRUE); x-=S(110)+S(8);
        MoveWindow(s_mg->bNew,     x-S(120), y, S(120), fh, TRUE); x-=S(120)+S(8);
        MoveWindow(s_mg->bCreated, x-S(150), y, S(150), fh, TRUE); x-=S(150)+S(14);
        MoveWindow(s_mg->cDept,    x-S(190), y, S(190), S(220), TRUE);
        MoveWindow(s_mg->bAddEmp,  a.left+S(12), y, S(170), fh, TRUE);
        ShowWindow(s_mg->bAbc,SW_SHOW); ShowWindow(s_mg->bNew,SW_SHOW);
        ShowWindow(s_mg->bCreated,SW_SHOW); ShowWindow(s_mg->cDept,SW_SHOW);
        ShowWindow(s_mg->bAddEmp,SW_SHOW);
    } else if(s_mg->page==PG_MSG){
        // a big «نوشتن پیام» button centered (the compose modal does the work)
        MoveWindow(s_mg->bMsg, a.left+(a.right-a.left)/2-S(110), top+S(30), S(220), S(44), TRUE);
        ShowWindow(s_mg->bMsg,SW_SHOW);
    } else if(s_mg->page==PG_REQS){
        // notification banner occupies [top .. top+60], separator at top+76 —
        // the two action buttons sit BELOW that, never over the banner/title.
        int y=top+S(60)+S(16)+S(16);   // banner + gap + separator gap
        int bw=S(300);
        MoveWindow(s_mg->bReqs,     rEdge-bw, y,        bw, S(46), TRUE);
        MoveWindow(s_mg->bProfReqs, rEdge-bw, y+S(58),  bw, S(46), TRUE);
        ShowWindow(s_mg->bReqs,SW_SHOW); ShowWindow(s_mg->bProfReqs,SW_SHOW);
    } else if(s_mg->page==PG_SAVED){
        // compose block at the top:  caption (painted)  ▸  multiline editor on
        // the right, the two action buttons stacked at its left, all sharing one
        // height so nothing overlaps and the note list starts cleanly below.
        int edTop = mgSavedComposeTop(h)+mgSavedCaptionH();
        int edH   = mgSavedEditorH();
        int bw=S(110), gap=S(10);
        int editR=rEdge, editL=a.left+S(12)+bw+gap;
        MoveWindow(s_mg->eNote, editL, edTop, editR-editL, edH, TRUE);
        // two buttons stacked to the LEFT of the editor, vertically centred
        int bh=S(34), bgap=S(6);
        int btnsTop = edTop + (edH-(bh*2+bgap))/2;
        MoveWindow(s_mg->bNoteSend,   a.left+S(12), btnsTop,            bw, bh, TRUE);
        MoveWindow(s_mg->bNoteAttach, a.left+S(12), btnsTop+bh+bgap,    bw, bh, TRUE);
        ShowWindow(s_mg->eNote,SW_SHOW);
        ShowWindow(s_mg->bNoteAttach,SW_SHOW);
        ShowWindow(s_mg->bNoteSend,SW_SHOW);
    } else if(s_mg->page==PG_SERVICES){
        // ---- service-management form (top) + services table (fills rest) ----
        //  v2.01 (Part C): the service form is now a full 4-column RTL grid
        //  covering the whole service definition — بخش/زیربخش (combo), codes,
        //  short + full names, tariffs (current نرخ آزاد/دولتی + قدیم), revenue
        //  group and status (combo). The retired controls (svCat / svPrice /
        //  svIns / svDesc) are kept (RULE 6) but stay hidden; svPrice still
        //  mirrors s.price for backward compatibility with legacy billing data.
        int lh=S(20), fgap=S(14);
        int formL=a.left+S(12), formR=rEdge, formW=formR-formL;
        const int cols=4;
        int cgap=S(12);
        int colW=(formW-(cols-1)*cgap)/cols;
        auto cx=[&](int c){ return formR-(c+1)*colW-c*cgap; };  // RTL col left edge
        auto cy=[&](int row){ return top+S(28)+lh+row*(lh+fh+fgap); };
        int comboH=S(200);               // tall so the dropdown shows several rows
        // Row 0: بخش/زیربخش (combo) | کد خدمت | نام اختصار | نام کامل
        MoveWindow(s_mg->svDept,   cx(0), cy(0), colW, comboH, TRUE);
        MoveWindow(s_mg->svCode,   cx(1), cy(0), colW, fh, TRUE);
        MoveWindow(s_mg->svShort,  cx(2), cy(0), colW, fh, TRUE);
        MoveWindow(s_mg->svName,   cx(3), cy(0), colW, fh, TRUE);
        // Row 1: کد لوینگ | کد معادل | شناسه خدمت | کد ملی سلامت
        MoveWindow(s_mg->svLoving, cx(0), cy(1), colW, fh, TRUE);
        MoveWindow(s_mg->svEquiv,  cx(1), cy(1), colW, fh, TRUE);
        MoveWindow(s_mg->svSrvId,  cx(2), cy(1), colW, fh, TRUE);
        MoveWindow(s_mg->svHnId,   cx(3), cy(1), colW, fh, TRUE);
        // Row 2: نرخ آزاد | نرخ دولتی | نرخ آزاد قدیم | نرخ دولتی قدیم
        MoveWindow(s_mg->svFree,   cx(0), cy(2), colW, fh, TRUE);
        MoveWindow(s_mg->svGov,    cx(1), cy(2), colW, fh, TRUE);
        MoveWindow(s_mg->svFreeOld,cx(2), cy(2), colW, fh, TRUE);
        MoveWindow(s_mg->svGovOld, cx(3), cy(2), colW, fh, TRUE);
        // Row 3: گروه درآمد | وضعیت (combo)
        MoveWindow(s_mg->svRevGrp, cx(0), cy(3), colW, fh, TRUE);
        MoveWindow(s_mg->svStatus, cx(1), cy(3), colW, comboH, TRUE);
        // action buttons row (right→left): ذخیره | ویرایش | حذف | فرم جدید
        int by=cy(3)+fh+S(6);
        int bw=S(120), bgap=S(10), bx=formR;
        MoveWindow(s_mg->svSave,   bx-bw, by, bw, S(40), TRUE); bx-=bw+bgap;
        MoveWindow(s_mg->svEdit,   bx-bw, by, bw, S(40), TRUE); bx-=bw+bgap;
        MoveWindow(s_mg->svDelete, bx-bw, by, bw, S(40), TRUE); bx-=bw+bgap;
        MoveWindow(s_mg->svClear,  bx-bw, by, bw, S(40), TRUE);
        // search box (left) below the form card + table fills the rest
        int cardBot=by+S(40)+S(10);
        int srchTop=cardBot+S(8), srchH=fh, srchW=S(320);
        MoveWindow(s_mg->svSearch, formL, srchTop, srchW, srchH, TRUE);
        int ly=srchTop+srchH+S(10);
        MoveWindow(s_mg->svList, formL, ly, formW, a.bottom-ly-S(8), TRUE);
        // Only the new fields + dept/status combos + buttons + search + list are
        // shown; the retired controls (svCat/svPrice/svIns/svDesc) stay hidden.
        HWND show[]={s_mg->svDept,s_mg->svCode,s_mg->svShort,s_mg->svName,
            s_mg->svLoving,s_mg->svEquiv,s_mg->svSrvId,s_mg->svHnId,
            s_mg->svFree,s_mg->svGov,s_mg->svFreeOld,s_mg->svGovOld,
            s_mg->svRevGrp,s_mg->svStatus,
            s_mg->svSave,s_mg->svEdit,s_mg->svDelete,s_mg->svClear,
            s_mg->svSearch,s_mg->svList};
        for(HWND w:show) if(w) ShowWindow(w,SW_SHOW);
    } else if(s_mg->page==PG_DOCS){
        // ---- doctor-management form (top) + doctors table (fills rest) ----
        //  A 4-column × 6-row field grid inside one card (v1.78.0: row 5 hosts
        //  the «تعریف به عنوان انجام دهنده» toggle), then the action buttons,
        //  then a search box + the LVS_OWNERDATA table below.
        int lh=S(20), fgap=S(14);
        int formL=a.left+S(12), formR=rEdge, formW=formR-formL;
        const int cols=4;
        int cgap=S(12);
        int colW=(formW-(cols-1)*cgap)/cols;
        auto cx=[&](int c){ return formR-(c+1)*colW-c*cgap; };  // RTL col left
        auto cy=[&](int row){ return top+S(28)+lh+row*(lh+fh+fgap); };
        int comboH=S(200);               // tall so the dropdown shows several rows
        // Row 0: نوع | بخش | کد پزشک | کد نظام پزشکی
        MoveWindow(s_mg->dcType,     cx(0), cy(0), colW, comboH, TRUE);
        MoveWindow(s_mg->dcDept,     cx(1), cy(0), colW, comboH, TRUE);
        MoveWindow(s_mg->dcCode,     cx(2), cy(0), colW, fh, TRUE);
        MoveWindow(s_mg->dcMedId,    cx(3), cy(0), colW, fh, TRUE);
        // Row 1: پیشوند نام | نام | نام خانوادگی | کد ملی
        MoveWindow(s_mg->dcPrefix,   cx(0), cy(1), colW, fh, TRUE);
        MoveWindow(s_mg->dcFirst,    cx(1), cy(1), colW, fh, TRUE);
        MoveWindow(s_mg->dcLast,     cx(2), cy(1), colW, fh, TRUE);
        MoveWindow(s_mg->dcNid,      cx(3), cy(1), colW, fh, TRUE);
        // Row 2: تلفن همراه | ایمیل | فرانشیز | تخصص
        MoveWindow(s_mg->dcMobile,   cx(0), cy(2), colW, fh, TRUE);
        MoveWindow(s_mg->dcEmail,    cx(1), cy(2), colW, fh, TRUE);
        MoveWindow(s_mg->dcFranchise,cx(2), cy(2), colW, fh, TRUE);
        MoveWindow(s_mg->dcSpec,     cx(3), cy(2), colW, fh, TRUE);
        // Row 3: تخصص بیمه | مدرک | نوع قرار داد | قرار پزشک اورژانس
        MoveWindow(s_mg->dcInsSpec,  cx(0), cy(3), colW, fh, TRUE);
        MoveWindow(s_mg->dcDegree,   cx(1), cy(3), colW, fh, TRUE);
        MoveWindow(s_mg->dcContract, cx(2), cy(3), colW, comboH, TRUE);
        MoveWindow(s_mg->dcEmerg,    cx(3), cy(3), colW, fh, TRUE);
        // Row 4: فعال | چاپ در قبض | آدرس | حسابداری
        MoveWindow(s_mg->dcActiveChk,cx(0), cy(4), colW, fh, TRUE);
        MoveWindow(s_mg->dcPrintChk, cx(1), cy(4), colW, fh, TRUE);
        MoveWindow(s_mg->dcAddress,  cx(2), cy(4), colW, fh, TRUE);
        MoveWindow(s_mg->dcAcct,     cx(3), cy(4), colW, fh, TRUE);
        // v1.78.0 Row 5: four toggles (right→left):
        //   تعریف به عنوان انجام دهنده | نمایش در لیست پزشکان سایت | نوبت‌دهی اینترنتی | پزشک بیهوشی
        MoveWindow(s_mg->dcPerfChk,   cx(0), cy(5), colW, fh, TRUE);
        MoveWindow(s_mg->dcSiteChk,   cx(1), cy(5), colW, fh, TRUE);
        MoveWindow(s_mg->dcOnlineChk, cx(2), cy(5), colW, fh, TRUE);
        MoveWindow(s_mg->dcAnesChk,   cx(3), cy(5), colW, fh, TRUE);
        // action buttons row (right→left): ذخیره | ویرایش | حذف | فرم جدید
        int by=cy(5)+fh+S(6);
        int bw=S(120), bgap=S(10), bx=formR;
        MoveWindow(s_mg->dcSave,   bx-bw, by, bw, S(40), TRUE); bx-=bw+bgap;
        MoveWindow(s_mg->dcEdit,   bx-bw, by, bw, S(40), TRUE); bx-=bw+bgap;
        MoveWindow(s_mg->dcDelete, bx-bw, by, bw, S(40), TRUE); bx-=bw+bgap;
        MoveWindow(s_mg->dcClear,  bx-bw, by, bw, S(40), TRUE);
        int cardBot=by+S(40)+S(10);
        // search box (left) + table below the form card
        int srchTop=cardBot+S(8), srchH=fh, srchW=S(320);
        MoveWindow(s_mg->dcSearch, formL, srchTop, srchW, srchH, TRUE);
        int ly=srchTop+srchH+S(10);
        MoveWindow(s_mg->dcList, formL, ly, formW, a.bottom-ly-S(8), TRUE);
        HWND show[]={s_mg->dcType,s_mg->dcDept,s_mg->dcCode,s_mg->dcMedId,
            s_mg->dcPrefix,s_mg->dcFirst,s_mg->dcLast,s_mg->dcNid,s_mg->dcMobile,
            s_mg->dcEmail,s_mg->dcFranchise,s_mg->dcSpec,s_mg->dcInsSpec,
            s_mg->dcDegree,s_mg->dcContract,s_mg->dcEmerg,s_mg->dcActiveChk,
            s_mg->dcPrintChk,s_mg->dcPerfChk,s_mg->dcSiteChk,s_mg->dcOnlineChk,s_mg->dcAnesChk,
            s_mg->dcAddress,s_mg->dcAcct,s_mg->dcSave,
            s_mg->dcEdit,s_mg->dcDelete,s_mg->dcClear,s_mg->dcSearch,s_mg->dcList};
        for(HWND w:show) if(w) ShowWindow(w,SW_SHOW);
    }
}

// ============================================================================
//  helpers: auto-id preview, add-section, card titles
// ============================================================================
static void mgUpdateAutoIdPreview(){
    if(!s_mg) return;
    wchar_t b[24]; swprintf(b,24,L"DEP%03d",(int)s_mg->cats.size()+1);
    s_mg->autoIdPreview=b;
}
static void mgDoAddCat(HWND h){
    if(!s_mg) return;
    wchar_t nm[128]={0},mg[128]={0},id[64]={0};
    GetWindowTextW(s_mg->eName,nm,128);
    GetWindowTextW(s_mg->eMgr,mg,128);
    GetWindowTextW(s_mg->eId,id,64);
    DeptCat c; c.name=trim(nm); c.manager=trim(mg);
    c.id = s_mg->autoId? L"" : trim(id);
    c.icon=L"shield";
    std::wstring err;
    if(addDept(c,err)){
        s_mg->msg=L"بخش «"+c.name+L"» با موفقیت اضافه شد."; s_mg->msgCol=g_theme.success;
        SetWindowTextW(s_mg->eName,L""); SetWindowTextW(s_mg->eMgr,L"");
        SetWindowTextW(s_mg->eId,L"");
        mgLoadCats(); mgUpdateAutoIdPreview();
        s_mg->secAddMode=false; mgLayout(h);
    } else { s_mg->msg=err; s_mg->msgCol=g_theme.danger; }
    InvalidateRect(h,NULL,FALSE);
}
static const wchar_t* mgCardTitle(int page){
    switch(page){
        case PG_DASH:  return L"داشبورد مدیریت";
        case PG_PATIENTS: return L"مدیریت بیماران";
        case PG_SECTS: return L"مدیریت بخش‌ها";
        case PG_SERVICES: return L"مدیریت خدمات";
        case PG_DOCS:  return L"مدیریت پزشکان";
        case PG_EMPS:  return L"فهرست کارمندان";
        case PG_MSG:   return L"پیام به کارمندان";
        case PG_REQS:  return L"درخواست‌های تغییر";
        case PG_SAVED: return L"پیام‌های ذخیره‌شده";
        case PG_BACKUP:return L"پشتیبان‌گیری و بازیابی";
        case PG_PRINTCFG: return L"تنظیمات چاپ بخش‌ها";
        case PG_SHIFTS:  return L"تعریف شیفت کاری";
    }
    return L"";
}

// ---- small drawing util: pill chip -----------------------------------------
static void mgChip(HDC dc, RECT r, const wchar_t* txt, COLORREF fill, COLORREF tx){
    gpRoundRectBg(dc,r,(r.bottom-r.top)/2,fill,fill,g_theme.surface);
    SetTextColor(dc,tx); SelectObject(dc,g_fSmall);
    DrawTextW(dc,txt,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
}

// ============================================================================
//  PAGE PAINTERS
// ============================================================================
// v1.66.0: compute dashboard summary ONCE and cache it. The dash used to call
// loadAllPatients(), loadUsers(), loadDepts(), countTodayReceptions(), and
// unseen*Count() on every WM_PAINT (2–4 times per frame with GDI+ layers),
// re-parsing the same JSON files each time — that I/O is the root cause of
// the FPS drop/hang when entering the management page. The cache is invalidated
// on every page switch (s_mg->dashValid=false) and refreshed lazily.
static void mgRefreshDash(){
    if(!s_mg) return;
    s_mg->dashSec   = (int)loadDepts().size();
    auto users       = loadUsers();
    s_mg->dashEmp   = (int)users.size();
    int nOn=0; for(auto&u:users) if(isUserOnline(u.username)) nOn++;
    s_mg->dashOn    = nOn;
    s_mg->dashReq   = unseenSetReqCount()+unseenProfReqCount();
    s_mg->dashPat   = (int)loadAllPatients().size();
    s_mg->dashToday = countTodayReceptions();
    s_mg->dashValid = true;
}

// ----------------------------------------------------------------------------
//  v1.69.0 DASHBOARD ENTRANCE ANIMATION.
//  A subtle, performant "materialise" effect: the KPI cards, quick-action tiles
//  and activity panel fade in from the card surface in a short staggered
//  sequence, and the KPI figures count up from 0. It is realised WITHOUT any
//  alpha-compositing / offscreen bitmap (the codebase double-buffers with plain
//  BitBlt) — instead each element's fill, border, shadow and text colour is
//  blended toward the card surface by (1-progress), so a low factor dissolves
//  the element into its background and a factor of 1 draws it at full strength.
//  Skipped on g_lowSpec; zero added cost once settled (progress == 1).
// ----------------------------------------------------------------------------
static float mgEase(float p){           // smoothstep — ease-in/out
    if(p<0)p=0; if(p>1)p=1;
    return p*p*(3.0f-2.0f*p);
}
// Per-group entrance factor in 0..1: `delay`/`span` are fractions of the whole
// timeline, so groups appear in sequence (KPIs → tiles → activity panel).
static float mgDashGroup(float anim, float delay, float span){
    if(span<=0.0f) return mgEase(anim);
    float p=(anim-delay)/span;
    return mgEase(p);
}
// Blend a colour toward the card surface by (1-f); f=1 leaves it untouched.
static COLORREF mgFade(COLORREF c, float f){
    int pct=(int)((1.0f-f)*100.0f); if(pct<0)pct=0; if(pct>100)pct=100;
    return blendColor(c, g_theme.surface, pct);
}
static void mgDashAnimStart(HWND h){
    // v1.69.0: entrance animation REMOVED per user request ("برای وارد شدن
    // نیازی به انیمیشن نیست"). Jump straight to the final state — no timer,
    // no fade, no count-up. The dashboard paints instantly.
    if(!s_mg) return;
    s_mg->dashAnim=1.0f; s_mg->dashAnimOn=false;
    (void)h;
}
static void mgDashAnimStop(HWND h){
    if(!s_mg) return;
    if(s_mg->dashAnimOn){ s_mg->dashAnimOn=false; KillTimer(h,MG_DASH_ANIM_TIMER); }
    s_mg->dashAnim=1.0f;
}

static void mgPaintDash(HDC dc, HWND h){
    RECT a=mgContentRect(h);
    // v1.66.0: refresh dash cache once per visit (lazy, invalidated on page switch)
    if(!s_mg->dashValid) mgRefreshDash();
    int nSec=s_mg->dashSec,  nEmp=s_mg->dashEmp,  nOn=s_mg->dashOn;
    int nReq=s_mg->dashReq,  nPat=s_mg->dashPat,  nToday=s_mg->dashToday;
    float anim = s_mg ? s_mg->dashAnim : 1.0f;   // v1.69.0 entrance factor
    SetBkMode(dc,TRANSPARENT);

    // ====================================================== KPI SUMMARY CARDS
    // v1.69.0: each card materialises from the surface (staggered fade) and its
    // figure counts up from 0; a thin glowing baseline pill in the metric's own
    // colour anchors the card (design-language «پیل درخشان»). At rest (ef==1)
    // every colour is full-strength, so steady state costs nothing extra.
    struct SumCard { const wchar_t* lbl; int raw; COLORREF col; int ico; };
    SumCard cards[4]={
        { L"بخش‌ها",         nSec, g_infoAccent,    ICO_SHIELD },
        { L"کارمندان",        nEmp, g_theme.accent,  ICO_USER  },
        { L"برخط",           nOn,  g_theme.success, ICO_BELL   },
        { L"درخواست‌های نو", nReq, nReq>0?g_infoAccent2:g_theme.textDim, ICO_GEAR },
    };
    for(int i=0;i<4;i++){
        float ef=mgDashGroup(anim, 0.00f+(float)i*0.05f, 0.40f);
        if(ef<=0.001f) continue;            // not yet on stage → leave surface
        RECT r=mgSummaryRect(h,i);
        int shA=(int)((g_dark?66:44)*ef);
        if(shA>2) gpShadow(dc,r,S(14),S(7),shA);
        COLORREF top=blendColor(g_theme.surfaceTop,g_theme.surface,45);
        COLORREF bord=blendColor(g_theme.border,cards[i].col,26);
        gpGradRoundRectBg(dc,r,S(14), mgFade(top,ef), mgFade(g_theme.surface,ef),
                          mgFade(bord,ef), g_theme.bg);
        // glyph medallion (top-right) tinted with the metric's own colour
        RECT ir={r.right-S(56),r.top+S(16),r.right-S(20),r.top+S(52)};
        { RECT md=ir; InflateRect(&md,S(7),S(7));
          int mr2=(md.bottom-md.top)/2;
          COLORREF medFill=blendColor(g_theme.surface,cards[i].col,g_dark?27:18);
          COLORREF medBord=blendColor(g_theme.border,cards[i].col,48);
          gpRoundRectBg(dc,md,mr2, mgFade(medFill,ef), mgFade(medBord,ef), g_theme.surface); }
        drawIcon(dc,cards[i].ico,ir, mgFade(cards[i].col,ef), S(2));
        // count-up figure (ticks 0 → cached value as the card materialises)
        int shown=(int)((float)cards[i].raw * ef + 0.5f);
        SetTextColor(dc, mgFade(g_theme.text,ef)); SelectObject(dc,g_fHuge);
        RECT vr={r.left+S(16),r.top+S(12),r.right-S(60),r.top+S(58)};
        DrawTextW(dc,toFaDigits(std::to_wstring(shown)).c_str(),-1,&vr,
            DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        // label (kept clear of the baseline pill on the left)
        SetTextColor(dc, mgFade(g_theme.textDim,ef)); SelectObject(dc,g_fSmall);
        RECT lr={r.left+S(76),r.bottom-S(30),r.right-S(16),r.bottom-S(8)};
        DrawTextW(dc,cards[i].lbl,-1,&lr,
            DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
        // glowing baseline pill (bottom-left) in the metric's colour
        if(ef>0.02f){
            RECT pill={r.left+S(16),r.bottom-S(13),r.left+S(16)+S(48),r.bottom-S(9)};
            gpShadowColor(dc,pill,S(2),S(4),(int)(96*ef),cards[i].col);
            gpRoundRect(dc,pill,S(2),mgFade(cards[i].col,ef),CLR_INVALID);
        }
    }

    // ======================================================== QUICK-ACTION TILES
    RECT q0=mgQuickRect(h,0);
    float efT=mgDashGroup(anim, 0.14f, 0.28f);     // section title
    if(efT>0.001f){
        SetTextColor(dc, mgFade(g_theme.text,efT)); SelectObject(dc,g_fUIB);
        RECT qr={a.left+S(12), q0.top-S(30), a.right-S(12), q0.top-S(8)};
        DrawTextW(dc,L"دسترسی سریع",-1,&qr,
            DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    }
    struct QA { const wchar_t* lbl; const wchar_t* sub; int ico; int page; };
    QA qa[4]={
        { L"ایجاد بخش",       L"افزودن بخش جدید",        ICO_PLUS,  PG_SECTS },
        { L"افزودن کارمند",   L"ثبت کارمند جدید",        ICO_USER,  PG_EMPS  },
        { L"پیام به کارمندان", L"ارسال پیام و رسانه",     ICO_BELL,  PG_MSG   },
        { L"درخواست‌ها",      L"بررسی درخواست‌ها",       ICO_GEAR,  PG_REQS  },
    };
    for(int i=0;i<4;i++){
        float ef=mgDashGroup(anim, 0.18f+(float)i*0.05f, 0.42f);
        if(ef<=0.001f) continue;
        RECT r=mgQuickRect(h,i);
        bool hot=(s_mg&&s_mg->hot==MG_QA_BASE+i);
        mgRowShell(dc,r,hot,S(14),g_infoAccent);
        RECT ir={r.left+(r.right-r.left)/2-S(18),r.top+S(18),
                 r.left+(r.right-r.left)/2+S(18),r.top+S(54)};
        // centred glyph medallion; hover brightens the wash so the tile responds
        { RECT md=ir; InflateRect(&md,S(8),S(8));
          int mr2=(md.bottom-md.top)/2;
          COLORREF medAcc=hot?g_infoAccent:g_theme.accent;
          COLORREF medFill=blendColor(g_theme.surface,medAcc,hot?29:20);
          COLORREF medBord=blendColor(g_theme.border,medAcc,hot?60:38);
          gpRoundRectBg(dc,md,mr2, mgFade(medFill,ef), mgFade(medBord,ef), g_theme.surface); }
        drawIcon(dc,qa[i].ico,ir, mgFade(hot?g_infoAccent:g_theme.accent,ef), S(2));
        SetTextColor(dc, mgFade(g_theme.text,ef)); SelectObject(dc,g_fUIB);
        RECT lr={r.left+S(8),r.top+S(60),r.right-S(8),r.top+S(84)};
        DrawTextW(dc,qa[i].lbl,-1,&lr,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        SetTextColor(dc, mgFade(g_theme.textDim,ef)); SelectObject(dc,g_fSmall);
        RECT sr={r.left+S(8),r.top+S(84),r.right-S(8),r.bottom-S(8)};
        DrawTextW(dc,qa[i].sub,-1,&sr,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    }

    // =================================================== CLINIC-AT-A-GLANCE PANEL
    // v1.69.0: replaced the flat two-chip + always-full «workload» bar with a
    // real dashboard footer — a Jalali date / shift pill, three mini-stat tiles
    // and a meaningful staff-presence gauge (online / total). Everything is read
    // from the cached dash values, so no file/JSON I/O happens per paint.
    float efP=mgDashGroup(anim, 0.30f, 0.50f);
    if(efP>0.001f){
        int panelTop = q0.bottom + S(18);          // derived from the tiles → never overlaps
        int panelBot = a.bottom-S(12);
        if(panelBot - panelTop >= S(150)){
            RECT pr={a.left+S(12),panelTop,a.right-S(12),panelBot};
            int shA=(int)((g_dark?60:40)*efP);
            if(shA>2) gpShadow(dc,pr,S(14),S(7),shA);
            gpGradRoundRectBg(dc,pr,S(14),
                mgFade(blendColor(g_theme.surfaceTop,g_theme.surface,30),efP),
                mgFade(g_theme.surface,efP), mgFade(g_theme.border,efP), g_theme.bg);
            // ---- header: title (right) + Jalali date/shift pill (left) ----
            int pillW=S(196), pillH=S(26), hy=pr.top+S(12);
            RECT pp={pr.left+S(16),hy,pr.left+S(16)+pillW,hy+pillH};
            if(efP>0.02f){
                COLORREF ppFill=blendColor(g_theme.surface,g_theme.accent,g_dark?14:9);
                gpShadowColor(dc,pp,pillH/2,S(4),(int)(80*efP),g_theme.accent);
                gpRoundRectBg(dc,pp,pillH/2, mgFade(ppFill,efP),
                    mgFade(blendColor(g_theme.border,g_theme.accent,45),efP), g_theme.surface);
            }
            SYSTEMTIME now=iranNow();
            std::wstring ds=toFaDigits(jalaliDateShort(now))+L"  •  "+shiftName(detectShift());
            SetTextColor(dc, mgFade(g_theme.accent,efP)); SelectObject(dc,g_fSmall);
            DrawTextW(dc,ds.c_str(),-1,&pp,
                DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
            SetTextColor(dc, mgFade(g_theme.text,efP)); SelectObject(dc,g_fUIB);
            RECT tr={pp.right+S(12),hy,pr.right-S(16),hy+pillH};
            DrawTextW(dc,L"یک نگاه به فعالیت کلینیک",-1,&tr,
                DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
            // ---- three mini-stat tiles ----
            struct AC{ const wchar_t* lbl; int raw; COLORREF col; int ico; };
            AC ac[3]={
                { L"کل بیماران ثبت‌شده", nPat,   g_theme.accent,  ICO_USER   },
                { L"پذیرش‌های امروز",    nToday, g_infoAccent,    ICO_RECEIPT },
                { L"کارمندان برخط",     nOn,    g_theme.success, ICO_BELL    },
            };
            int cy=pr.top+S(48), gap=S(14);
            // purely gap-derived width (no visual floor): adjacent tiles are
            // always separated by exactly `gap`, so they can NEVER overlap,
            // however narrow the panel gets. The min-1 only guards against a
            // negative-width rect on absurdly small windows.
            int cellW=(pr.right-pr.left-S(32)-2*gap)/3; if(cellW<1) cellW=1;
            for(int i=0;i<3;i++){
                int cx=pr.right-S(16)-(i+1)*cellW-i*gap;     // RTL
                RECT cr={cx,cy,cx+cellW,cy+S(64)};
                gpRoundRectBg(dc,cr,S(12),
                    mgFade(blendColor(g_theme.surface,ac[i].col,g_dark?12:8),efP),
                    mgFade(blendColor(g_theme.border,ac[i].col,38),efP), g_theme.surface);
                RECT md={cr.right-S(12)-S(36),cr.top+S(14),cr.right-S(12),cr.top+S(14)+S(36)};
                int mr2=(md.bottom-md.top)/2;
                COLORREF mf=blendColor(g_theme.surface,ac[i].col,g_dark?26:17);
                COLORREF mb=blendColor(g_theme.border,ac[i].col,46);
                gpRoundRectBg(dc,md,mr2, mgFade(mf,efP), mgFade(mb,efP), g_theme.surface);
                RECT ico={md.left+S(6),md.top+S(6),md.right-S(6),md.bottom-S(6)};
                drawIcon(dc,ac[i].ico,ico, mgFade(ac[i].col,efP), S(2));
                int shown=(int)((float)ac[i].raw * efP + 0.5f);
                SetTextColor(dc, mgFade(g_theme.text,efP)); SelectObject(dc,g_fTitle);
                RECT vr={cr.left+S(12),cr.top+S(10),cr.right-S(12)-S(36)-S(8),cr.top+S(38)};
                DrawTextW(dc,toFaDigits(std::to_wstring(shown)).c_str(),-1,&vr,
                    DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
                SetTextColor(dc, mgFade(g_theme.textDim,efP)); SelectObject(dc,g_fSmall);
                RECT lr={cr.left+S(12),cr.top+S(38),cr.right-S(12)-S(36)-S(8),cr.top+S(58)};
                DrawTextW(dc,ac[i].lbl,-1,&lr,
                    DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
            }
            // ---- staff-presence gauge (online / total) ----
            int barTop=cy+S(64)+S(16);
            if(barTop+S(34) <= pr.bottom-S(10)){
                int den=nEmp>0?nEmp:1;
                double frac=(double)nOn/(double)den; if(frac>1.0)frac=1.0; if(frac<0)frac=0;
                std::wstring gl=L"حضور کارمندان — "+toFaDigits(std::to_wstring(nOn))+
                    L" از "+toFaDigits(std::to_wstring(nEmp))+L" برخط";
                SetTextColor(dc, mgFade(g_theme.textDim,efP)); SelectObject(dc,g_fSmall);
                RECT wl={pr.left+S(16),barTop,pr.right-S(16),barTop+S(18)};
                DrawTextW(dc,gl.c_str(),-1,&wl,
                    DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
                RECT track={pr.left+S(16),barTop+S(22),pr.right-S(16),barTop+S(32)};
                gpRoundRectBg(dc,track,S(5),
                    mgFade(g_theme.surface2,efP), mgFade(g_theme.border,efP), g_theme.surface);
                int fillW=(int)((track.right-track.left)*frac);
                if(fillW>0){
                    RECT fillr={track.right-fillW,track.top,track.right,track.bottom};
                    if(efP>0.02f) gpShadowColor(dc,fillr,S(5),S(4),(int)(90*efP),g_theme.success);
                    gpRoundRectBg(dc,fillr,S(5), mgFade(g_theme.success,efP),
                        mgFade(g_theme.success,efP), g_theme.surface2);
                }
            }
        }
    }
}

static void mgPaintSects(HDC dc, HWND h){
    RECT a=mgContentRect(h);
    if(s_mg->secAddMode){
        int top=a.top+mgHeadH()+S(12), rEdge=a.right-S(12), lh=S(22), fh=S(34), w1=S(240);
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
        RECT t1={rEdge-w1,top,rEdge,top+lh};
        DrawTextW(dc,L"نام بخش",-1,&t1,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        RECT t2={rEdge-w1,top+lh+fh+S(20),rEdge,top+lh+fh+S(20)+lh};
        DrawTextW(dc,L"مدیر بخش (اختیاری)",-1,&t2,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        int y3=top+lh+fh+S(20)+lh+fh+S(20);
        RECT t3={rEdge-w1,y3+S(34),rEdge,y3+S(34)+lh};
        std::wstring idlbl=L"شناسهٔ بخش";
        if(s_mg->autoId) idlbl=L"شناسهٔ بخش (خودکار: "+s_mg->autoIdPreview+L")";
        SetTextColor(dc,g_theme.textDim);
        DrawTextW(dc,idlbl.c_str(),-1,&t3,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        return;
    }
    // existing sections grid
    if(s_mg->cats.empty()){
        mgEmptyState(dc,a,ICO_SHIELD,L"هنوز بخشی ثبت نشده است",
            L"با دکمهٔ «بخش جدید» نخستین بخش را بسازید");
        return;
    }
    for(int i=0;i<(int)s_mg->cats.size();i++){
        RECT r=mgSecCardRect(h,i);
        bool hot=(s_mg->hot==MG_CARD_BASE+i);
        mgRowShell(dc,r,hot,S(14),g_infoAccent);
        RECT ir={r.right-S(50),r.top+S(14),r.right-S(18),r.top+S(46)};
        // v1.63.0: the section glyph sits in its own tinted medallion
        { RECT md=ir; InflateRect(&md,S(6),S(6));
          int mr2=(md.bottom-md.top)/2;
          COLORREF medFill=blendColor(g_theme.surface,g_infoAccent,g_dark?25:17);
          gpRoundRectBg(dc,md,mr2,medFill,
              blendColor(g_theme.border,g_infoAccent,45),g_theme.surface); }
        drawIcon(dc,ICO_SHIELD,ir,g_infoAccent,S(2));
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
        RECT nr={r.left+S(14),r.top+S(16),r.right-S(56),r.top+S(46)};
        DrawTextW(dc,s_mg->cats[i].name.c_str(),-1,&nr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_WORD_ELLIPSIS|DT_NOPREFIX);
        SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
        RECT idr={r.left+S(14),r.top+S(52),r.right-S(14),r.top+S(74)};
        DrawTextW(dc,(L"شناسه: "+toFaDigits(s_mg->cats[i].id)).c_str(),-1,&idr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        std::wstring mgr=s_mg->cats[i].manager.empty()?L"بدون مدیر":(L"مدیر: "+s_mg->cats[i].manager);
        RECT mr={r.left+S(14),r.top+S(76),r.right-S(14),r.top+S(98)};
        DrawTextW(dc,mgr.c_str(),-1,&mr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_WORD_ELLIPSIS|DT_NOPREFIX);
    }
}

static void mgPaintEmps(HDC dc, HWND h){
    RECT a=mgContentRect(h);
    if(s_mg->emps.empty()){
        mgEmptyState(dc,a,ICO_USER,L"کارمندی یافت نشد",
            L"عبارت جست‌وجو یا فیلتر بخش را تغییر دهید");
        return;
    }
    for(int i=0;i<(int)s_mg->emps.size();i++){
        RECT r=mgEmpRowRect(h,i);
        if(r.top>a.bottom) break;
        bool hot=(s_mg->hot==MG_DETAIL_BASE+i);
        mgRowShell(dc,r,hot,S(12),g_infoAccent);
        const EmpRow& e=s_mg->emps[i];
        // v1.63.0 presence dot: an ONLINE user gets a glowing green disc with a
        // soft halo (a real presence light); offline stays a flat grey dot.
        { int cy2=(r.top+r.bottom)/2;
          RECT dot={r.right-S(38),cy2-S(6),r.right-S(26),cy2+S(6)};
          if(e.online){
              RECT halo=dot; InflateRect(&halo,S(4),S(4));
              gpFillAlpha(dc,halo,(halo.bottom-halo.top)/2,g_theme.success,70);
              gpRoundRect(dc,dot,S(6),g_theme.success,CLR_INVALID);
          } else {
              gpRoundRect(dc,dot,S(6),
                  blendColor(g_theme.textDim,g_theme.surface,45),CLR_INVALID);
          } }
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
        RECT nr={r.left+S(14),r.top+S(8),r.right-S(52),r.top+S(32)};
        DrawTextW(dc,e.user.fullname.c_str(),-1,&nr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_WORD_ELLIPSIS|DT_NOPREFIX);
        SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
        std::wstring sub=e.deptName;
        if(!e.prof.empId.empty()) sub+=L"  •  کد: "+toFaDigits(e.prof.empId);
        if(!e.prof.position.empty()) sub+=L"  •  "+e.prof.position;
        RECT sr={r.left+S(14),r.top+S(32),r.right-S(48),r.bottom-S(8)};
        DrawTextW(dc,sub.c_str(),-1,&sr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_WORD_ELLIPSIS|DT_NOPREFIX);
    }
}

// v1.22.0: row rect for the employee list shown on the «پیام به کارمندان» page.
static RECT mgMsgEmpRowRect(HWND h, int idx){
    RECT a=mgContentRect(h);
    // compose button sits at top+30..top+74 (see mgLayout PG_MSG); list starts below.
    int y=a.top+mgHeadH()+S(132)+idx*(mgEmpRowH()+S(8));
    RECT r={a.left+S(12),y,a.right-S(12),y+mgEmpRowH()}; return r;
}
static void mgPaintMsg(HDC dc, HWND h){
    RECT a=mgContentRect(h);
    // hint above the compose button
    SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fUI);
    RECT tr={a.left+S(24),a.top+mgHeadH()+S(86),a.right-S(24),a.top+mgHeadH()+S(112)};
    DrawTextW(dc,L"برای ارسال پیام، رسانه و هدف‌گذاری، دکمهٔ «نوشتن پیام» را بزنید.",-1,&tr,
        DT_CENTER|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);

    // ---- employee list (so management can see who will receive the message) --
    int hy=a.top+mgHeadH()+S(118);
    gpLine(dc,a.left+S(12),hy,a.right-S(12),hy,g_theme.border,1);
    SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.accent);
    RECT hr={a.left+S(12),hy+S(6),a.right-S(12),hy+S(28)};
    std::wstring hdr=L"کارمندان ("+toFaDigits(std::to_wstring((int)s_mg->emps.size()))+L" نفر)";
    DrawTextW(dc,hdr.c_str(),-1,&hr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);

    if(s_mg->emps.empty()){
        mgEmptyState(dc,a,ICO_USER,L"کارمندی برای نمایش وجود ندارد",
            L"پس از ثبت کارمندان، گیرندگان پیام اینجا فهرست می‌شوند");
        return;
    }
    for(int i=0;i<(int)s_mg->emps.size();i++){
        RECT r=mgMsgEmpRowRect(h,i);
        if(r.top>a.bottom) break;
        mgRowShell(dc,r,false,S(12),g_infoAccent);
        const EmpRow& e=s_mg->emps[i];
        // v1.63.0 presence light (same treatment as the کارمندان page)
        { int cy2=(r.top+r.bottom)/2;
          RECT dot={r.right-S(38),cy2-S(6),r.right-S(26),cy2+S(6)};
          if(e.online){
              RECT halo=dot; InflateRect(&halo,S(4),S(4));
              gpFillAlpha(dc,halo,(halo.bottom-halo.top)/2,g_theme.success,70);
              gpRoundRect(dc,dot,S(6),g_theme.success,CLR_INVALID);
          } else {
              gpRoundRect(dc,dot,S(6),
                  blendColor(g_theme.textDim,g_theme.surface,45),CLR_INVALID);
          } }
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
        RECT nr={r.left+S(14),r.top+S(8),r.right-S(48),r.top+S(32)};
        DrawTextW(dc,e.user.fullname.c_str(),-1,&nr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_WORD_ELLIPSIS|DT_NOPREFIX);
        SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
        std::wstring sub=e.deptName;
        if(!e.prof.empId.empty()) sub+=L"  •  کد: "+toFaDigits(e.prof.empId);
        if(!e.prof.position.empty()) sub+=L"  •  "+e.prof.position;
        sub+=e.online? L"  •  آنلاین" : L"  •  آفلاین";
        RECT sr={r.left+S(14),r.top+S(32),r.right-S(48),r.bottom-S(8)};
        DrawTextW(dc,sub.c_str(),-1,&sr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_WORD_ELLIPSIS|DT_NOPREFIX);
    }
}

static void mgPaintReqs(HDC dc, HWND h){
    RECT a=mgContentRect(h);
    int top=a.top+mgHeadH()+S(12);
    // ---- notifications ABOVE the buttons (Task 11) ----
    int nSet=unseenSetReqCount(), nProf=unseenProfReqCount();
    RECT nb={a.left+S(12),top,a.right-S(12),top+S(60)};
    COLORREF nc=(nSet+nProf)>0?g_infoAccent:g_theme.border;
    gpRoundRectBg(dc,nb,S(12), g_theme.surface, nc, g_theme.bg);
    RECT ic={nb.right-S(48),nb.top+S(16),nb.right-S(18),nb.top+S(46)};
    drawIcon(dc,ICO_BELL,ic,(nSet+nProf)>0?g_infoAccent:g_theme.textDim,S(2));
    SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
    std::wstring nt;
    if(nSet+nProf>0) nt=L"شما "+toFaDigits(std::to_wstring(nSet+nProf))+L" درخواست بررسی‌نشده دارید.";
    else nt=L"درخواست بررسی‌نشده‌ای وجود ندارد.";
    RECT ntr={nb.left+S(16),nb.top,nb.right-S(54),nb.bottom};
    DrawTextW(dc,nt.c_str(),-1,&ntr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    // ---- separator line ----
    int sy=nb.bottom+S(16);
    gpLine(dc,a.left+S(12),sy,a.right-S(12),sy,g_theme.border,1);
    // the two big buttons sit just below (placed by mgLayout); their position
    // starts at top so we push them with the layout — here we just label hints.
}

// ---- saved messages (LOCAL notes) page -------------------------------------
//  Stored locally per machine/user (data\local_notes_<user>.dat) — NEVER sent
//  over the network. Both staff and management have their own private area.
static int mgNoteListTop(HWND h){ return mgSavedComposeBottom(h)+S(12); }
static int mgNoteRowH(){ return S(78); }
static RECT mgNoteRowRect(HWND h,int i){
    RECT a=mgContentRect(h);
    int y=mgNoteListTop(h)+i*(mgNoteRowH()+S(8));
    RECT r={a.left+S(12),y,a.right-S(12),y+mgNoteRowH()}; return r;
}
// delete button + thumbnail rects within a note row
static RECT mgNoteDelRect(RECT row){ RECT b={row.left+S(12),row.bottom-S(34),row.left+S(82),row.bottom-S(10)}; return b; }
static RECT mgNoteImgRect(RECT row){ int s=S(54); RECT b={row.right-S(12)-s,row.top+S(12),row.right-S(12),row.top+S(12)+s}; return b; }
static void mgPaintSaved(HDC dc, HWND h){
    RECT a=mgContentRect(h);
    int cTop=mgSavedComposeTop(h);
    // section caption ABOVE the editor (icon flush-right, title to its left)
    { SelectObject(dc,g_fUIB); SetTextColor(dc,g_theme.accent);
      int icoW=S(18), gap=S(8);
      RECT ci={a.right-S(12)-icoW,cTop+S(1),a.right-S(12),cTop+S(19)};
      drawIcon(dc,ICO_SAVE,ci,g_theme.accent,S(2));
      RECT cr={a.left+S(12),cTop,a.right-S(12)-icoW-gap,cTop+mgSavedCaptionH()};
      DrawTextW(dc,L"یادداشت جدید",-1,&cr,
          DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
    // editor input-well behind eNote (consistent flat field look)
    { int edTop=cTop+mgSavedCaptionH(), edH=mgSavedEditorH();
      int bw=S(110), gap=S(10);
      RECT well={a.left+S(12)+bw+gap-S(6),edTop-S(4),a.right-S(12)+S(0),edTop+edH+S(4)};
      bool foc=(GetFocus()==s_mg->eNote);
      // v1.63.0: recessed well (top-down darker→input gradient) + a glowing
      // accent ring while focused, so the active field is unmistakable.
      if(foc) gpShadowColor(dc,well,S(9),S(5),70,g_theme.accent);
      gpGradRoundRect(dc,well,S(9),
          blendColor(g_theme.inputBg,g_theme.border,g_dark?26:34),
          g_theme.inputBg, foc?g_theme.accent:g_theme.border); }
    // hint line BELOW the compose block
    SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
    int hintY=mgSavedComposeBottom(h)-mgSavedHintH();
    RECT hint={a.left+S(12),hintY,a.right-S(12),hintY+mgSavedHintH()};
    std::wstring at = s_mg->noteAttach.empty()? std::wstring() : L"   •   پیوست: ✓";
    DrawTextW(dc,(L"یادداشت‌ها فقط روی همین رایانه ذخیره می‌شوند و در شبکه منتقل نمی‌گردند."+at).c_str(),
        -1,&hint,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    auto notes=loadLocalNotes(g_session.user.username);
    if(notes.empty()){
        SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fUI);
        RECT ea=a; ea.top=mgNoteListTop(h)-mgHeadH();
        mgEmptyState(dc,ea,ICO_SAVE,L"یادداشت ذخیره‌شده‌ای وجود ندارد",
            L"متن را بنویسید و «ذخیره» را بزنید");
        return;
    }
    for(int i=0;i<(int)notes.size();i++){
        RECT r=mgNoteRowRect(h,i); if(r.top>=a.bottom-S(16)) break;
        bool hot=(s_mg->noteHot==i);
        mgRowShell(dc,r,hot,S(12),g_infoAccent);
        // time header
        SetTextColor(dc,g_infoAccent2); SelectObject(dc,g_fSmall);
        RECT hr={r.left+S(14),r.top+S(8),r.right-S(80),r.top+S(28)};
        DrawTextW(dc,notes[i].time.c_str(),-1,&hr,
            DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        // body text
        SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUI);
        bool hasImg=!notes[i].attachPath.empty();
        int bodyR = hasImg ? r.right-S(80) : r.right-S(14);
        RECT br={r.left+S(90),r.top+S(28),bodyR,r.bottom-S(8)};
        std::wstring body=notes[i].text;
        DrawTextW(dc,body.c_str(),-1,&br,
            DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS|DT_EDITCONTROL);
        // image thumbnail (click → open/enlarge)
        if(hasImg){
            RECT ir=mgNoteImgRect(r);
            if(!gpDrawImageFileCircle(dc,notes[i].attachPath,ir)){
                gpRoundRectBg(dc,ir,S(8),g_theme.surface2,g_theme.border,g_theme.surface);
                RECT ic={ir.left+S(12),ir.top+S(12),ir.right-S(12),ir.bottom-S(12)};
                drawIcon(dc,ICO_ID,ic,g_theme.textDim,S(2));
            }
        }
        // delete button
        RECT dl=mgNoteDelRect(r);
        gpRoundRect(dc,dl,S(8),g_theme.surface2,g_theme.border,255);
        SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.danger);
        DrawTextW(dc,L"حذف",-1,&dl,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    }
}

// ---- backup page: an intro + a big «باز کردن مدیریت پشتیبان» action --------
static RECT mgBackupBtnRect(HWND h){
    RECT a=mgContentRect(h);
    int top=a.top+mgHeadH()+S(120);
    int w=S(380);
    RECT r={ (a.left+a.right)/2-w/2, top, (a.left+a.right)/2+w/2, top+S(56) };
    return r;
}
static void mgPaintBackup(HDC dc, HWND h){
    RECT a=mgContentRect(h);
    int top=a.top+mgHeadH()+S(16);
    // intro card
    RECT ib={a.left+S(16),top,a.right-S(16),top+S(84)};
    gpRoundRectBg(dc,ib,S(14),g_theme.surface,g_theme.border,g_theme.bg);
    RECT ic={ib.right-S(60),ib.top+S(24),ib.right-S(20),ib.top+S(64)};
    drawIcon(dc,ICO_REFRESH,ic,g_theme.accent,S(2));
    SetTextColor(dc,g_theme.text); SelectObject(dc,g_fUIB);
    RECT t1={ib.left+S(16),ib.top+S(12),ib.right-S(70),ib.top+S(40)};
    DrawTextW(dc,L"پشتیبان‌گیری و بازیابی اطلاعات بیماران",-1,&t1,
        DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
    RECT t2={ib.left+S(16),ib.top+S(40),ib.right-S(70),ib.bottom-S(8)};
    DrawTextW(dc,L"تهیهٔ نسخهٔ پشتیبان کامل، یا بازیابی کامل/انتخابی از فایل‌های پشتیبان (شامل پشتیبان‌های حجیم متین‌طب). عملیات سنگین در پس‌زمینه اجرا می‌شود.",
        -1,&t2,DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
    // big open button — icon flush-right (RTL leading edge), the label centred
    // in the area to its LEFT so the two never overlap or get clipped.
    RECT r=mgBackupBtnRect(h);
    bool hot=(s_mg&&s_mg->hot==MG_BACKUP);
    gpRoundRect(dc,r,S(10), hot?g_theme.accentHover:g_theme.accent, g_theme.accent,255);
    int icoW=S(28), igap=S(10);
    RECT bi={r.right-S(16)-icoW,r.top+S(14),r.right-S(16),r.bottom-S(14)};
    drawIcon(dc,ICO_SAVE,bi,g_theme.accentText,S(2));
    SetTextColor(dc,g_theme.accentText); SelectObject(dc,g_fBig);
    RECT bl={r.left+S(12),r.top,r.right-S(16)-icoW-igap,r.bottom};
    DrawTextW(dc,L"باز کردن مدیریت پشتیبان",-1,&bl,
        DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
}

// ---- patients page: a small status line under the (real-HWND) list ----------
static void mgPaintPatients(HDC dc, HWND h){
    if(!s_mg) return;
    RECT a=mgContentRect(h);
    SetTextColor(dc,g_theme.textDim); SelectObject(dc,g_fSmall);
    RECT sr={a.left+S(24),a.bottom-S(28),a.right-S(24),a.bottom-S(6)};
    std::wstring s;
    if(s_mg->ptLoading)      s=L"در حال بارگذاری بیماران…";
    else if(!s_mg->ptLoaded) s=L"—";
    else {
        size_t tot = s_mg->ptAll? s_mg->ptAll->size():0;
        size_t shown = s_mg->ptView.size();
        s=L"نمایش "+toFaDigits(std::to_wstring(shown))+L" از "+
          toFaDigits(std::to_wstring(tot))+L" بیمار";
    }
    DrawTextW(dc,s.c_str(),-1,&sr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
}

// ---- «مدیریت خدمات» page body: form card labels + hint + status message ----
static void mgPaintServices(HDC dc, HWND h){
    if(!s_mg) return;
    RECT a=mgContentRect(h);
    int rEdge=a.right-S(12), fh=S(34);
    int top=a.top+mgHeadH()+S(12);
    int lh=S(20), fgap=S(14);
    int formL=a.left+S(12), formR=rEdge, formW=formR-formL;
    const int cols=4;
    int cgap=S(12);
    int colW=(formW-(cols-1)*cgap)/cols;
    auto cx=[&](int c){ return formR-(c+1)*colW-c*cgap; };
    auto cy=[&](int row){ return top+S(28)+lh+row*(lh+fh+fgap); };
    int by=cy(3)+fh+S(6);
    int cardBot=by+S(40)+S(10);
    // ---- form card background ----
    RECT card={formL-S(4),top-S(2),formR+S(4),cardBot};
    gpRoundRectBg(dc,card,S(12),g_theme.surface,g_theme.border,g_theme.bg);
    // card title
    SelectObject(dc,g_fSection); SetTextColor(dc,g_theme.sectionInk);
    { RECT tr={formL,top+S(2),formR,top+S(26)};
      DrawTextW(dc,L"مشخصات خدمت",-1,&tr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
    // field label helper (drawn just above its control)
    auto lbl=[&](int x,int ly,int w,const wchar_t* txt){
        SelectObject(dc,g_fLabel); SetTextColor(dc,g_theme.labelInk);
        RECT lr={x,ly,x+w,ly+S(16)};
        DrawTextW(dc,txt,-1,&lr,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    };
    // 4-column × 4-row grid of labels (RTL), matching the field layout exactly.
    struct Lb{int r,c; const wchar_t* t;};
    const Lb lbs[]={
        {0,0,L"بخش/زیربخش"},{0,1,L"کد خدمت"},{0,2,L"نام اختصار"},{0,3,L"نام کامل"},
        {1,0,L"کد لوینگ"},{1,1,L"کد معادل"},{1,2,L"شناسه خدمت"},{1,3,L"کد ملی سلامت"},
        {2,0,L"نرخ آزاد"},{2,1,L"نرخ دولتی"},{2,2,L"نرخ آزاد قدیم"},{2,3,L"نرخ دولتی قدیم"},
        {3,0,L"گروه درآمد"},{3,1,L"وضعیت"}};
    for(const auto& lb:lbs) lbl(cx(lb.c),cy(lb.r)-lh,colW,lb.t);
    // ---- status message (below the buttons) ----
    if(!s_mg->msg.empty()){
        SelectObject(dc,g_fSmall); SetTextColor(dc,s_mg->msgCol);
        RECT mr={formL,cardBot+S(4),formR,cardBot+S(24)};
        DrawTextW(dc,s_mg->msg.c_str(),-1,&mr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    }
    // ---- empty-state hint for the table ----
    if(s_mg->svView.empty()){
        SelectObject(dc,g_fUI); SetTextColor(dc,g_theme.textDim);
        int srchTop=cardBot+S(8), srchH=fh, lt=srchTop+srchH+S(10);
        RECT er={formL,lt+S(30),formR,lt+S(70)};
        DrawTextW(dc,L"هنوز خدمتی ثبت نشده است — فرم بالا را پر کنید و «ذخیره» را بزنید.",
            -1,&er,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    }
}

// ---- «مدیریت پزشکان» page body ---------------------------------------------
//  Paints the form-card chrome only: a soft shadow + surface-gradient card
//  with an accent icon medallion and a fading rule under the title (the design
//  language's height-shadow / surface-gradient / icon-modal / fade-separator
//  elements), then the 4×5 grid of field labels, and a status / empty hint in
//  the search row. The doctor TABLE itself is the LVS_OWNERDATA child (dcList),
//  themed by applyThemedListView; it paints its own rows. Geometry mirrors the
//  mgLayout PG_DOCS branch exactly so labels line up with their controls.
// ============================================================================
//  v2.01 (Part F2) — «تعریف شیفت کاری» shift management helpers (file scope)
// ============================================================================
static void mgShiftsReload(){
    if(!s_mg) return;
    s_mg->shAll=loadShiftDefs();
    s_mg->shLoaded=true;
    if(s_mg->shList){
        ListView_SetItemCountEx(s_mg->shList,(int)s_mg->shAll.size(),
            LVSICF_NOINVALIDATEALL|LVSICF_NOSCROLL);
        InvalidateRect(s_mg->shList,NULL,FALSE);
    }
}
static void mgShiftsClearForm(){
    if(!s_mg) return;
    SetWindowTextW(s_mg->shName,L"");
    SetWindowTextW(s_mg->shStart,L"");
    SetWindowTextW(s_mg->shEnd,L"");
    s_mg->shEditId=-1;
}
static void mgShiftsSave(HWND h){
    if(!s_mg) return;
    wchar_t name[128]={0}, start[16]={0}, end[16]={0};
    GetWindowTextW(s_mg->shName,name,128);
    GetWindowTextW(s_mg->shStart,start,16);
    GetWindowTextW(s_mg->shEnd,end,16);
    std::wstring nm=trim(name);
    if(nm.empty()){ s_mg->msg=L"نام شیفت الزامی است."; s_mg->msgCol=g_theme.danger;
        InvalidateRect(h,NULL,TRUE); return; }
    auto parseTime=[&](const wchar_t* s)->int{
        std::wstring t=trim(s);
        for(auto& c : t) if(c>=L'\u06F0'&&c<=L'\u06F9') c=L'0'+(c-L'\u06F0');
        int hh=0,mm=0;
        size_t cp=t.find(L':');
        if(cp!=std::wstring::npos){ hh=_wtoi(t.substr(0,cp).c_str()); mm=_wtoi(t.substr(cp+1).c_str()); }
        else { hh=_wtoi(t.c_str()); }
        if(hh<0)hh=0; if(hh>23)hh=23; if(mm<0)mm=0; if(mm>59)mm=59;
        return hh*60+mm;
    };
    int sm=parseTime(start), em=parseTime(end);
    if(s_mg->shEditId>=0){
        auto defs=loadShiftDefs();
        for(auto& d : defs) if(d.id==s_mg->shEditId){
            d.name=nm; d.startMin=sm; d.endMin=em;
            saveShiftDefs(defs);
            s_mg->msg=L"شیفت «"+nm+L"» ویرایش شد."; s_mg->msgCol=g_theme.success;
            break;
        }
    } else {
        int id=addShiftDef(nm,sm,em);
        if(id<0){ s_mg->msg=L"افزودن شیفت ناموفق بود."; s_mg->msgCol=g_theme.danger; }
        else { s_mg->msg=L"شیفت «"+nm+L"» اضافه شد."; s_mg->msgCol=g_theme.success; }
    }
    mgShiftsClearForm();
    mgShiftsReload();
    InvalidateRect(h,NULL,TRUE);
}
static void mgShiftsDelete(HWND h){
    if(!s_mg || !s_mg->shList) return;
    int sel=ListView_GetNextItem(s_mg->shList,-1,LVNI_SELECTED);
    if(sel<0 || sel>=(int)s_mg->shAll.size()){
        s_mg->msg=L"ابتدا یک شیفت را از جدول انتخاب کنید."; s_mg->msgCol=g_theme.danger;
        InvalidateRect(h,NULL,TRUE); return;
    }
    const ShiftDef& sd=s_mg->shAll[sel];
    if(sd.id<3){
        s_mg->msg=L"شیفت‌های پیش‌فرض (صبح، عصر، شب) قابل حذف نیستند."; s_mg->msgCol=g_theme.danger;
        InvalidateRect(h,NULL,TRUE); return;
    }
    std::wstring q=L"حذف شیفت «"+sd.name+L"»؟";
    if(MessageBoxW(h,q.c_str(),L"تأیید حذف",MB_YESNO|MB_ICONWARNING)!=IDYES) return;
    deleteShiftDef(sd.id);
    s_mg->msg=L"شیفت حذف شد."; s_mg->msgCol=g_theme.success;
    mgShiftsClearForm();
    mgShiftsReload();
    InvalidateRect(h,NULL,TRUE);
}
static void mgShiftsEditSelected(HWND h){
    if(!s_mg || !s_mg->shList) return;
    int sel=ListView_GetNextItem(s_mg->shList,-1,LVNI_SELECTED);
    if(sel<0 || sel>=(int)s_mg->shAll.size()){
        s_mg->msg=L"ابتدا یک شیفت را از جدول انتخاب کنید."; s_mg->msgCol=g_theme.danger;
        InvalidateRect(h,NULL,TRUE); return;
    }
    const ShiftDef& sd=s_mg->shAll[sel];
    s_mg->shEditId=sd.id;
    SetWindowTextW(s_mg->shName,sd.name.c_str());
    wchar_t t[16];
    swprintf(t,16,L"%02d:%02d",sd.startMin/60,sd.startMin%60);
    SetWindowTextW(s_mg->shStart,toFaDigits(std::wstring(t)).c_str());
    swprintf(t,16,L"%02d:%02d",sd.endMin/60,sd.endMin%60);
    SetWindowTextW(s_mg->shEnd,toFaDigits(std::wstring(t)).c_str());
    InvalidateRect(h,NULL,TRUE);
    SetFocus(s_mg->shName);
}
static void mgPaintDocs(HDC dc, HWND h){
    if(!s_mg) return;
    RECT a=mgContentRect(h);
    int rEdge=a.right-S(12), fh=S(34);
    int top=a.top+mgHeadH()+S(12);
    int lh=S(20), fgap=S(14);
    int formL=a.left+S(12), formR=rEdge, formW=formR-formL;
    const int cols=4;
    int cgap=S(12);
    int colW=(formW-(cols-1)*cgap)/cols;
    auto cx=[&](int c){ return formR-(c+1)*colW-c*cgap; };
    auto cy=[&](int row){ return top+S(28)+lh+row*(lh+fh+fgap); };
    int by=cy(4)+fh+S(6);
    int cardBot=by+S(40)+S(10);
    // ---- form card: soft shadow + surface gradient ----
    RECT card={formL-S(4),top-S(2),formR+S(4),cardBot};
    gpShadow(dc,card,S(12),S(7),g_dark?58:40);
    gpGradRoundRectBg(dc,card,S(12),g_theme.surfaceTop,g_theme.surface,
                      g_theme.border,g_theme.bg);
    // card title with a small accent medallion on the RTL (right) edge
    int ttop=top+S(2), th=S(24), med=S(22), mcy=ttop+th/2;
    RECT mdr={formR-med-S(4),mcy-med/2,formR-S(4),mcy+med/2};
    gpRoundRectBg(dc,mdr,med/2,blendColor(g_theme.surface,g_infoAccent,g_dark?22:16),
                  blendColor(g_theme.border,g_infoAccent,46),g_theme.surface);
    { RECT ir={mdr.left+S(5),mdr.top+S(5),mdr.right-S(5),mdr.bottom-S(5)};
      drawIcon(dc,ICO_CROSS_MED,ir,blendColor(g_infoAccent,g_theme.text,12),S(2)); }
    SelectObject(dc,g_fSection); SetTextColor(dc,g_theme.sectionInk);
    { RECT tr={formL+S(10),ttop,mdr.left-S(10),ttop+th};
      DrawTextW(dc,L"مشخصات پزشک",-1,&tr,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
    // fade separator under the title (strong on the RTL/right side)
    { int sy=ttop+th, x0=formL+S(10), x1=formR-S(10), span=x1-x0;
      if(span>0){ int seg=S(6); if(seg<2)seg=2;
        for(int x=x0;x<x1;x+=seg){ int xe=x+seg; if(xe>x1)xe=x1;
            int al=150-(140*(x1-x))/span; if(al<8)al=8; if(al>255)al=255;
            gpLine(dc,x,sy,xe,sy,blendColor(g_theme.border,g_infoAccent,32),1.0f,al); } } }
    // field labels (4 columns × 6 rows, RTL — v1.78.0: row 5 = انجام دهنده)
    auto lbl=[&](int x,int ly,int w,const wchar_t* txt){
        SelectObject(dc,g_fLabel); SetTextColor(dc,g_theme.labelInk);
        RECT lr={x,ly,x+w,ly+S(16)};
        DrawTextW(dc,txt,-1,&lr,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    };
    struct Lb{int r,c; const wchar_t* t;};
    const Lb lbs[]={
        {0,0,L"نوع"},{0,1,L"بخش"},{0,2,L"کد پزشک"},{0,3,L"کد نظام پزشکی"},
        {1,0,L"پیشوند نام"},{1,1,L"نام"},{1,2,L"نام خانوادگی"},{1,3,L"کد ملی"},
        {2,0,L"تلفن همراه"},{2,1,L"ایمیل"},{2,2,L"فرانشیز"},{2,3,L"تخصص"},
        {3,0,L"تخصص بیمه"},{3,3,L"قرار پزشک اورژانس"},{3,2,L"نوع قرار داد"},{3,1,L"مدرک"},
        {4,0,L"وضعیت فعالیت"},{4,1,L"چاپ در قبض"},{4,2,L"آدرس"},{4,3,L"حسابداری"},
        {5,0,L"تعریف به عنوان انجام دهنده"},
        {5,1,L"نمایش در لیست پزشکان سایت"},
        {5,2,L"نوبت‌دهی اینترنتی"},
        {5,3,L"پزشک بیهوشی"}};
    for(const Lb& lb:lbs) lbl(cx(lb.c),cy(lb.r)-lh,colW,lb.t);
    // ---- search row: box (left, child) + status / empty hint (right) ----
    int srchTop=cardBot+S(8), srchH=fh, srchW=S(320);
    if(!s_mg->msg.empty()){
        SelectObject(dc,g_fSmall); SetTextColor(dc,s_mg->msgCol);
        RECT mr={formL+srchW+S(14),srchTop,formR,srchTop+srchH};
        DrawTextW(dc,s_mg->msg.c_str(),-1,&mr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    } else if(s_mg->dcView.empty()){
        SelectObject(dc,g_fSmall); SetTextColor(dc,g_theme.textDim);
        RECT er={formL+srchW+S(14),srchTop,formR,srchTop+srchH};
        DrawTextW(dc, s_mg->dcAll.empty()?L"هنوز پزشکی ثبت نشده است":L"موردی یافت نشد",
            -1,&er,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    }
}
//  (Helper functions mgShiftsReload/Save/Delete/etc. are at file scope above.)
// ============================================================================
static void mgPaintShifts(HDC dc, HWND h){
    if(!s_mg) return;
    RECT a=mgContentRect(h);
    if(!s_mg->shLoaded) mgShiftsReload();
    // Form card (top section): title + fields.
    int top=a.top+S(16), formW=a.right-a.left-S(48);
    int fh=S(30), lh=S(24), gap=S(10), cgap=S(12);
    int colW=(formW-3*cgap)/4;
    auto cx=[&](int c){ return a.right-S(24)-(c+1)*colW-c*cgap; };
    auto cy=[&](int row){ return top+S(28)+lh+row*(lh+fh+gap); };
    int by=cy(0)+fh+S(6);
    int cardBot=by+S(40)+S(10);
    RECT card={a.left+S(20),top-S(2),a.right-S(24),cardBot};
    gpShadow(dc,card,S(12),S(7),g_dark?58:40);
    gpGradRoundRectBg(dc,card,S(12),g_theme.surfaceTop,g_theme.surface,g_theme.border,g_theme.bg);
    // card title
    int ttop=top+S(2), th=S(24), med=S(22), mcy=ttop+th/2;
    RECT mdr={a.right-S(24)-med-S(4),mcy-med/2,a.right-S(24)-S(4),mcy+med/2};
    gpRoundRectBg(dc,mdr,med/2,blendColor(g_theme.surface,g_infoAccent,g_dark?22:16),
                  blendColor(g_theme.border,g_infoAccent,46),g_theme.surface);
    { RECT ir={mdr.left+S(5),mdr.top+S(5),mdr.right-S(5),mdr.bottom-S(5)};
      drawIcon(dc,ICO_CLOCK,ir,blendColor(g_infoAccent,g_theme.text,12),S(2)); }
    SelectObject(dc,g_fSection); SetTextColor(dc,g_theme.sectionInk);
    { RECT tr={a.left+S(30),ttop,mdr.left-S(10),ttop+th};
      DrawTextW(dc,L"تعریف شیفت کاری",-1,&tr,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX); }
    // fade separator under title
    { int sy=ttop+th, x0=a.left+S(30), x1=a.right-S(34), span=x1-x0;
      if(span>0){ int seg=S(6); if(seg<2)seg=2;
        for(int x=x0;x<x1;x+=seg){ int xe=x+seg; if(xe>x1)xe=x1;
            int al=150-(140*(x1-x))/span; if(al<8)al=8; if(al>255)al=255;
            gpLine(dc,x,sy,xe,sy,blendColor(g_theme.border,g_infoAccent,32),1.0f,al); } } }
    // field labels
    auto lbl=[&](int x,int ly,int w,const wchar_t* txt){
        SelectObject(dc,g_fLabel); SetTextColor(dc,g_theme.labelInk);
        RECT lr={x,ly,x+w,ly+S(16)};
        DrawTextW(dc,txt,-1,&lr,DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    };
    lbl(cx(0),cy(0)-lh,colW,L"نام شیفت");
    lbl(cx(1),cy(0)-lh,colW,L"ساعت شروع");
    lbl(cx(2),cy(0)-lh,colW,L"ساعت پایان");
    // action buttons row
    MoveWindow(s_mg->shSave,  cx(0), by, colW, S(36), TRUE); ShowWindow(s_mg->shSave,SW_SHOW);
    MoveWindow(s_mg->shDelete,cx(1), by, colW, S(36), TRUE); ShowWindow(s_mg->shDelete,SW_SHOW);
    MoveWindow(s_mg->shClear, cx(2), by, colW, S(36), TRUE); ShowWindow(s_mg->shClear,SW_SHOW);
    // status / empty hint
    if(!s_mg->msg.empty()){
        SelectObject(dc,g_fSmall); SetTextColor(dc,s_mg->msgCol);
        RECT mr={a.left+S(30),by+S(40),a.right-S(34),by+S(40)+S(22)};
        DrawTextW(dc,s_mg->msg.c_str(),-1,&mr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    }
    // table below the form card
    int srchTop=cardBot+S(8), ly=srchTop+S(10);
    MoveWindow(s_mg->shList, a.left+S(20), ly, formW, a.bottom-ly-S(8), TRUE);
    ShowWindow(s_mg->shList,SW_SHOW);
    // Show/hide form fields
    HWND show[]={s_mg->shName,s_mg->shStart,s_mg->shEnd};
    for(HWND e:show) ShowWindow(e,SW_SHOW);
}

// ---- the dashboard shell: nav rail (right) + content card (left) -----------
static void mgPaintShell(HDC dc, HWND h){
    RECT rc; GetClientRect(h,&rc);
    FillRect(dc,&rc,g_brBg); SetBkMode(dc,TRANSPARENT);
    // ------------------------------------------------------------------
    //  v1.63.0 MANAGEMENT SHELL REDESIGN.
    //  The rail and the content card were flat surface fills, and a selected
    //  nav item was a solid blue block with no depth — the panel looked like
    //  a mock-up rather than a product. Both surfaces now get an elevation
    //  shadow + a soft top-down gradient, the rail title sits over its own
    //  faint accent wash with a fading rule beneath it, and the SELECTED nav
    //  item becomes a glowing gradient pill with a bright leading indicator
    //  on the RTL (right) edge — the standard "you are here" affordance.
    // ------------------------------------------------------------------
    RECT nav={rc.right-mgNavW(), rc.top, rc.right, rc.bottom};
    RECT navCard={nav.left+S(4),nav.top+S(8),nav.right-S(8),nav.bottom-S(8)};
    gpShadow(dc,navCard,S(16),S(9),g_dark?78:52);
    gpGradRoundRectBg(dc,navCard,S(16),g_theme.surfaceTop,g_theme.surface,
                      g_theme.border,g_theme.bg);
    // app/section title in the rail header
    SetTextColor(dc,g_theme.text); SelectObject(dc,g_fTitle);
    RECT th={nav.left+S(12),nav.top+S(20),nav.right-S(16),nav.top+S(60)};
    DrawTextW(dc,L"پنل مدیریت",-1,&th,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    // fading rule under the rail title (strong on the RTL/right side)
    { int y2=nav.top+S(64), x0=navCard.left+S(12), x1=navCard.right-S(12);
      int span=x1-x0;
      if(span>0){ int seg=S(6); if(seg<2) seg=2;
        for(int x=x0;x<x1;x+=seg){ int xe=x+seg; if(xe>x1) xe=x1;
            int a=200-(180*(x1-x))/span; if(a<8)a=8; if(a>255)a=255;
            gpLine(dc,x,y2,xe,y2,blendColor(g_theme.border,g_infoAccent,25),1.0f,a); } } }
    for(int i=0;i<PG_COUNT;i++){
        RECT r=mgNavItemRect(h,i);
        bool sel=(s_mg && s_mg->page==i);
        bool hot=(s_mg && s_mg->navHot==i);
        if(sel){
            gpShadowColor(dc,r,S(12),S(7),96,g_infoAccent);
            gpGradRoundRect(dc,r,S(12),
                blendColor(g_infoAccent,RGB(255,255,255),18),
                g_infoAccent,CLR_INVALID);
            // leading indicator on the RTL edge
            RECT li={r.right-S(5),r.top+S(9),r.right-S(2),r.bottom-S(9)};
            gpRoundRect(dc,li,S(2),RGB(255,255,255),CLR_INVALID);
        } else if(hot){
            gpShadow(dc,r,S(12),S(5),44);
            gpGradRoundRect(dc,r,S(12),
                blendColor(g_theme.surfaceTop,g_theme.surface2,40),
                g_theme.surface2,blendColor(g_theme.border,g_infoAccent,34));
        }
        RECT ir={r.right-S(44),r.top+S(12),r.right-S(16),r.top+S(40)};
        drawIcon(dc,MG_NAV_ICONS[i],ir, sel?RGB(255,255,255):g_theme.textDim, S(2));
        SetTextColor(dc, sel?RGB(255,255,255):g_theme.text); SelectObject(dc,g_fUI);
        RECT lr={r.left+S(12),r.top,r.right-S(50),r.bottom};
        DrawTextW(dc,MG_NAV_LABELS[i],-1,&lr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        // request badge on the REQS nav item
        if(i==PG_REQS){
            int n=unseenSetReqCount()+unseenProfReqCount();
            if(n>0){
                RECT bd={r.left+S(12),r.top+S(14),r.left+S(34),r.top+S(36)};
                // v1.63.0: a glowing gradient counter so pending requests
                // genuinely demand attention from across the room.
                COLORREF bhost = sel?g_infoAccent:(hot?g_theme.surface2:g_theme.surface);
                gpRoundRectBg(dc,bd,S(11),bhost,CLR_INVALID,bhost);
                gpShadowColor(dc,bd,S(11),S(5),110,g_theme.danger);
                gpGradRoundRect(dc,bd,S(11),
                    blendColor(g_theme.danger,RGB(255,255,255),24),
                    g_theme.danger,CLR_INVALID);
                SetTextColor(dc,RGB(255,255,255)); SelectObject(dc,g_fSmall);
                DrawTextW(dc,toFaDigits(std::to_wstring(n)).c_str(),-1,&bd,
                    DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            }
        }
    }
    // ---- content card on the LEFT ----
    RECT a=mgContentRect(h);
    gpShadow(dc,a,S(18),S(10),g_dark?80:54);
    gpGradRoundRectBg(dc,a,S(18),g_theme.surfaceTop,g_theme.surface,
                      g_theme.border,g_theme.bg);
    // v1.63.0: the page title now sits on a faint accent wash band that spans
    // the header, so the card has a real titlebar instead of floating text.
    { RECT hb={a.left+S(1),a.top+S(1),a.right-S(1),a.top+mgHeadH()};
      COLORREF headerFill=blendColor(g_theme.surface,g_infoAccent,g_dark?12:7);
      gpRoundRectBg(dc,hb,S(16),headerFill,CLR_INVALID,g_theme.surface); }
    SetTextColor(dc,g_theme.text); SelectObject(dc,g_fBig);
    RECT hr={a.left+S(24),a.top+S(16),a.right-S(24),a.top+mgHeadH()-S(8)};
    DrawTextW(dc,mgCardTitle(s_mg?s_mg->page:0),-1,&hr,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
    // two-tone closing edge for the header band (rule + light lip)
    gpLine(dc,a.left+S(16),a.top+mgHeadH(),a.right-S(16),a.top+mgHeadH(),g_theme.border,1);
    gpLine(dc,a.left+S(16),a.top+mgHeadH()+1,a.right-S(16),a.top+mgHeadH()+1,
        blendColor(g_theme.surface,RGB(255,255,255),g_dark?8:70),1.0f,180);
    // page body
    if(s_mg){
        switch(s_mg->page){
            case PG_DASH:  mgPaintDash(dc,h);  break;
            case PG_PATIENTS: mgPaintPatients(dc,h); break;
            case PG_SECTS: mgPaintSects(dc,h); break;
            case PG_SERVICES: mgPaintServices(dc,h); break;
            case PG_DOCS:  mgPaintDocs(dc,h);  break;
            case PG_EMPS:  mgPaintEmps(dc,h);  break;
            case PG_MSG:   mgPaintMsg(dc,h);   break;
            case PG_REQS:  mgPaintReqs(dc,h);  break;
            case PG_SAVED: mgPaintSaved(dc,h); break;
            case PG_BACKUP:mgPaintBackup(dc,h);break;
            case PG_SHIFTS:mgPaintShifts(dc,h);break;
        }
        if(!s_mg->msg.empty()){
            SetTextColor(dc,s_mg->msgCol); SelectObject(dc,g_fSmall);
            RECT mr={a.left+S(24),a.bottom-S(30),a.right-S(24),a.bottom-S(8)};
            DrawTextW(dc,s_mg->msg.c_str(),-1,&mr,DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        }
    }
}

// ============================================================================
//  hit-testing
// ============================================================================
static int mgHit(HWND h, POINT pt){
    if(!s_mg) return 0;
    // nav rail
    for(int i=0;i<PG_COUNT;i++){
        RECT r=mgNavItemRect(h,i);
        if(PtInRect(&r,pt)) return MG_NAV_BASE+i;
    }
    if(s_mg->page==PG_DASH){
        for(int i=0;i<4;i++){ RECT r=mgQuickRect(h,i); if(PtInRect(&r,pt)) return MG_QA_BASE+i; }
    } else if(s_mg->page==PG_SECTS && !s_mg->secAddMode){
        for(int i=0;i<(int)s_mg->cats.size();i++){ RECT r=mgSecCardRect(h,i); if(PtInRect(&r,pt)) return MG_CARD_BASE+i; }
    } else if(s_mg->page==PG_EMPS){
        for(int i=0;i<(int)s_mg->emps.size();i++){ RECT r=mgEmpRowRect(h,i); if(PtInRect(&r,pt)) return MG_DETAIL_BASE+i; }
    } else if(s_mg->page==PG_BACKUP){
        RECT r=mgBackupBtnRect(h); if(PtInRect(&r,pt)) return MG_BACKUP;
    }
    return 0;
}

static void mgRefreshReqBadge(){
    // badge is drawn on the nav rail; just repaint
    if(g_hManage) InvalidateRect(g_hManage,NULL,FALSE);
}

static void mgFillDeptCombo(){
    if(!s_mg || !s_mg->cDept) return;
    SendMessageW(s_mg->cDept,CB_RESETCONTENT,0,0);
    SendMessageW(s_mg->cDept,CB_ADDSTRING,0,(LPARAM)L"همهٔ بخش‌ها");
    auto cats=loadDepts();
    for(auto& c:cats) SendMessageW(s_mg->cDept,CB_ADDSTRING,0,(LPARAM)c.name.c_str());
    // restore selection
    int sel=0;
    if(!s_mg->empDeptFilter.empty()){
        for(int i=0;i<(int)cats.size();i++) if(cats[i].id==s_mg->empDeptFilter){ sel=i+1; break; }
    }
    SendMessageW(s_mg->cDept,CB_SETCURSEL,sel,0);
}

static void mgGoPage(HWND h, int page){
    if(!s_mg) return;
    // §1.19.0: «تنظیمات چاپ» is a modal, not a flat page — open it and keep the
    // current page underneath (don't blank the content area).
    if(page==PG_PRINTCFG){ openPrintCfg(h); return; }
    s_mg->page=page; s_mg->secAddMode=false; s_mg->msg.clear();
    s_mg->dashValid=false;  // v1.66.0: invalidate dash cache on page switch
    if(page==PG_SECTS){ mgLoadCats(); mgUpdateAutoIdPreview(); }
    else if(page==PG_EMPS){ mgLoadEmps(); mgFillDeptCombo(); }
    else if(page==PG_MSG){ mgLoadEmps(); }   // v1.22.0: show employee list
    else if(page==PG_PATIENTS){ mgPatientsLoadAsync(h); }
    else if(page==PG_SERVICES){ mgServicesClearForm(); mgServicesReload(); }
    else if(page==PG_DOCS){ mgDocsClearForm(); mgDocsReload(); }
    else if(page==PG_SHIFTS){ mgShiftsClearForm(); mgShiftsReload(); }  // v2.01 Part F2
    // v1.69.0: replay the subtle dashboard entrance each time the dash is shown;
    // stop it (and the timer) whenever we navigate away.
    if(page==PG_DASH) mgDashAnimStart(h); else mgDashAnimStop(h);
    mgLayout(h);
    InvalidateRect(h,NULL,TRUE);
}

// ---- sort-button label refresh (active highlighting via text marker) -------
static void mgRefreshSortBtns(){
    if(!s_mg) return;
    SetWindowTextW(s_mg->bAbc,     s_mg->empSort==0?L"● الفبایی":L"الفبایی");
    SetWindowTextW(s_mg->bNew,     s_mg->empSort==1?L"● جدیدترین":L"جدیدترین");
    SetWindowTextW(s_mg->bCreated, s_mg->empSort==2?L"● زمان ساخت":L"زمان ساخت");
}

// ----------------------------------------------------------------------------
//  v1.69.0 DOCTORS PAGE — create all child controls once (kept hidden until
//  the page is shown by mgLayout). Mirrors the services control block but is
//  isolated in its own function so WM_CREATE stays readable.
// ----------------------------------------------------------------------------
static void mgBuildDocsPage(HWND h){
    if(!s_mg) return;
    DWORD es=WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL;
    s_mg->dcType    =createThemedCombo(h,MG_DC_TYPE);
    s_mg->dcDept    =createThemedCombo(h,MG_DC_DEPT);
    s_mg->dcCode    =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_DC_CODE,g_hInst,0);
    s_mg->dcMedId   =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_DC_MEDID,g_hInst,0);
    s_mg->dcPrefix  =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_DC_PREFIX,g_hInst,0);
    s_mg->dcFirst   =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_DC_FIRST,g_hInst,0);
    s_mg->dcLast    =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_DC_LAST,g_hInst,0);
    s_mg->dcNid     =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_DC_NID,g_hInst,0);
    s_mg->dcMobile  =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_DC_MOBILE,g_hInst,0);
    s_mg->dcEmail   =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_DC_EMAIL,g_hInst,0);
    s_mg->dcFranchise=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_DC_FRANCHISE,g_hInst,0);
    s_mg->dcSpec    =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_DC_SPEC,g_hInst,0);
    s_mg->dcInsSpec =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_DC_INSSPEC,g_hInst,0);
    s_mg->dcDegree  =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_DC_DEGREE,g_hInst,0);
    s_mg->dcContract=createThemedCombo(h,MG_DC_CONTRACT);
    s_mg->dcEmerg   =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_DC_EMERG,g_hInst,0);
    s_mg->dcAddress =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_DC_ADDRESS,g_hInst,0);
    s_mg->dcAcct    =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_DC_ACCT,g_hInst,0);
    s_mg->dcActiveChk=createFlatButton(h,MG_DC_ACTIVE,L"\u2611 فعال",0,BS_OUTLINE,0,0,10,10);
    s_mg->dcPrintChk =createFlatButton(h,MG_DC_PRINT, L"\u2611 بله",0,BS_OUTLINE,0,0,10,10);
    // v1.78.0: «تعریف به عنوان انجام دهنده» toggle — same visual language as
    // the فعال / چاپ در قبض toggles, default ON.
    s_mg->dcPerfChk  =createFlatButton(h,MG_DC_PERF,  L"\u2611 بله",0,BS_OUTLINE,0,0,10,10);
    // v2.01 (Part B): complementary option checkboxes
    s_mg->dcSiteChk  =createFlatButton(h,MG_DC_SITE,  L"\u2611 بله",0,BS_OUTLINE,0,0,10,10);
    s_mg->dcOnlineChk=createFlatButton(h,MG_DC_ONLINE,L"\u2610 خیر",0,BS_OUTLINE,0,0,10,10);
    s_mg->dcAnesChk  =createFlatButton(h,MG_DC_ANES,  L"\u2610 خیر",0,BS_OUTLINE,0,0,10,10);
    s_mg->dcSave   =createFlatButton(h,MG_DC_SAVE,  L"ذخیره",  ICO_SAVE,  BS_PRIMARY,0,0,10,10);
    s_mg->dcEdit   =createFlatButton(h,MG_DC_EDIT,  L"ویرایش", ICO_GEAR,  BS_OUTLINE,0,0,10,10);
    s_mg->dcDelete =createFlatButton(h,MG_DC_DELETE,L"حذف",    ICO_TRASH, BS_DANGER, 0,0,10,10);
    s_mg->dcClear  =createFlatButton(h,MG_DC_CLEAR, L"فرم جدید",ICO_PLUS, BS_OUTLINE,0,0,10,10);
    s_mg->dcSearch =CreateWindowExW(0,L"EDIT",L"",
        WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL,0,0,10,10,h,(HMENU)MG_DC_SEARCH,g_hInst,0);
    s_mg->dcList   =CreateWindowExW(0,WC_LISTVIEWW,L"",
        WS_CHILD|WS_TABSTOP|LVS_OWNERDATA|LVS_REPORT|LVS_SHOWSELALWAYS|LVS_SINGLESEL,
        0,0,10,10,h,(HMENU)MG_DC_LIST,g_hInst,0);
    ListView_SetExtendedListViewStyle(s_mg->dcList,
        LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
    SendMessageW(s_mg->dcList,WM_SETFONT,(WPARAM)g_fUI,TRUE);
    {   // RTL columns (rightmost first): # | نام | تخصص | نوع | وضعیت
        struct { const wchar_t* t; int w; } dcols[]={
            {L"#",S(44)},{L"نام",S(230)},{L"تخصص",S(170)},{L"نوع",S(90)},{L"وضعیت",S(90)} };
        LVCOLUMNW lc={0}; lc.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_FMT; lc.fmt=LVCFMT_RIGHT;
        for(int i=0;i<5;i++){ lc.pszText=(LPWSTR)dcols[i].t; lc.cx=dcols[i].w;
            ListView_InsertColumn(s_mg->dcList,i,&lc); }
    }
    applyThemedListView(s_mg->dcList);
    // v2.01 (Part F2): shift management page controls
    {
        DWORD es=WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL;
        s_mg->shName  =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_SH_NAME,g_hInst,0);
        s_mg->shStart =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_SH_START,g_hInst,0);
        s_mg->shEnd   =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_SH_END,g_hInst,0);
        s_mg->shSave  =createFlatButton(h,MG_SH_SAVE,  L"ذخیره",ICO_SAVE,BS_PRIMARY,0,0,10,10);
        s_mg->shDelete=createFlatButton(h,MG_SH_DELETE,L"حذف",ICO_TRASH,BS_DANGER,0,0,10,10);
        s_mg->shClear =createFlatButton(h,MG_SH_CLEAR,L"فرم جدید",ICO_PLUS,BS_OUTLINE,0,0,10,10);
        s_mg->shList  =CreateWindowExW(0,WC_LISTVIEWW,L"",
            WS_CHILD|WS_TABSTOP|LVS_OWNERDATA|LVS_REPORT|LVS_SHOWSELALWAYS|LVS_SINGLESEL,
            0,0,10,10,h,(HMENU)MG_SH_LIST,g_hInst,0);
        ListView_SetExtendedListViewStyle(s_mg->shList,
            LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
        SendMessageW(s_mg->shList,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        {   // RTL columns: # | نام شیفت | ساعت شروع | ساعت پایان
            struct { const wchar_t* t; int w; } shcols[]={
                {L"#",S(44)},{L"نام شیفت",S(200)},{L"ساعت شروع",S(120)},{L"ساعت پایان",S(120)} };
            LVCOLUMNW lc={0}; lc.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_FMT; lc.fmt=LVCFMT_RIGHT;
            for(int i=0;i<4;i++){ lc.pszText=(LPWSTR)shcols[i].t; lc.cx=shcols[i].w;
                ListView_InsertColumn(s_mg->shList,i,&lc); }
        }
        applyThemedListView(s_mg->shList);
        HWND shEds[]={s_mg->shName,s_mg->shStart,s_mg->shEnd};
        for(HWND e:shEds){ SendMessageW(e,WM_SETFONT,(WPARAM)g_fUI,TRUE); enableEnterNavigation(e); }
        SendMessageW(s_mg->shStart,EM_SETCUEBANNER,TRUE,(LPARAM)L"HH:MM");
        SendMessageW(s_mg->shEnd,EM_SETCUEBANNER,TRUE,(LPARAM)L"HH:MM");
    }
    SendMessageW(s_mg->dcSearch,WM_SETFONT,(WPARAM)g_fUI,TRUE);
    // type combo: پزشک / پرستار
    SendMessageW(s_mg->dcType,CB_ADDSTRING,0,(LPARAM)L"پزشک");
    SendMessageW(s_mg->dcType,CB_ADDSTRING,0,(LPARAM)L"پرستار");
    SendMessageW(s_mg->dcType,CB_SETCURSEL,0,0);
    // contract-type combo
    { const wchar_t* ct[]={L"حقوقی",L"پورسانتی",L"قراردادی",L"ساعتی"};
      for(int i=0;i<4;i++) SendMessageW(s_mg->dcContract,CB_ADDSTRING,0,(LPARAM)ct[i]);
      SendMessageW(s_mg->dcContract,CB_SETCURSEL,0,0); }
    // fonts + Enter-key navigation on the edit fields
    HWND eds[]={s_mg->dcCode,s_mg->dcMedId,s_mg->dcPrefix,s_mg->dcFirst,s_mg->dcLast,
        s_mg->dcNid,s_mg->dcMobile,s_mg->dcEmail,s_mg->dcFranchise,s_mg->dcSpec,
        s_mg->dcInsSpec,s_mg->dcDegree,s_mg->dcEmerg,s_mg->dcAddress,s_mg->dcAcct};
    for(HWND e:eds){ SendMessageW(e,WM_SETFONT,(WPARAM)g_fUI,TRUE); enableEnterNavigation(e); }
    HWND cmb[]={s_mg->dcType,s_mg->dcDept,s_mg->dcContract};
    for(HWND c:cmb) SendMessageW(c,WM_SETFONT,(WPARAM)g_fUI,TRUE);
}

// ============================================================================
//  window procedure
// ============================================================================
static LRESULT CALLBACK manageProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE: {
        s_mg=new ManageState();
        s_mg->page=PG_DASH; s_mg->hot=0; s_mg->navHot=-1;
        s_mg->secAddMode=false; s_mg->autoId=true;
        s_mg->empSort=1; s_mg->msgCol=g_theme.textDim;
        s_mg->dashValid=false;  // v1.66.0: dash cache
        s_mg->dashAnim=1.0f; s_mg->dashAnimOn=false;  // v1.69.0 entrance (started by mgGoPage)
        DWORD es=WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL;
        s_mg->eName=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_E_NAME,g_hInst,0);
        s_mg->eMgr =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_E_MGR,g_hInst,0);
        s_mg->eId  =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_E_ID,g_hInst,0);
        s_mg->bAdd =createFlatButton(h,MG_ADD_CAT,L"افزودن بخش",ICO_PLUS,BS_PRIMARY,0,0,10,10);
        s_mg->bAutoId=createFlatButton(h,MG_CB_AUTOID,L"\u2611 شناسه خودکار",0,BS_OUTLINE,0,0,10,10);
        s_mg->bSecNew=createFlatButton(h,MG_DESIGN_SEC,L"ایجاد بخش",ICO_PLUS,BS_PRIMARY,0,0,10,10);
        s_mg->bSecBack=createFlatButton(h,MG_BACK,L"بازگشت",ICO_BACK,BS_OUTLINE,0,0,10,10);
        s_mg->bAbc =createFlatButton(h,MG_FILTER_ABC,L"الفبایی",0,BS_OUTLINE,0,0,10,10);
        s_mg->bNew =createFlatButton(h,MG_FILTER_NEW,L"● جدیدترین",0,BS_OUTLINE,0,0,10,10);
        s_mg->bCreated=createFlatButton(h,MG_FILTER_CREATED,L"زمان ساخت",0,BS_OUTLINE,0,0,10,10);
        s_mg->bAddEmp=createFlatButton(h,MG_ADD_EMP,L"افزودن کارمند",ICO_USER,BS_PRIMARY,0,0,10,10);
        s_mg->cDept=createThemedCombo(h,MG_CB_DEPT);
        s_mg->bMsg =createFlatButton(h,MG_MSG,L"نوشتن پیام",ICO_BELL,BS_PRIMARY,0,0,10,10);
        s_mg->bReqs=createFlatButton(h,MG_REQS,L"درخواست‌های تغییر تنظیمات",ICO_GEAR,BS_INFO,0,0,10,10);
        s_mg->bProfReqs=createFlatButton(h,MG_PROFREQS,L"درخواست‌های تغییر پروفایل",ICO_USER,BS_INFO,0,0,10,10);
        // saved messages (local notes) controls
        s_mg->eNote=CreateWindowExW(0,L"EDIT",L"",
            WS_CHILD|WS_TABSTOP|ES_AUTOVSCROLL|ES_MULTILINE|WS_VSCROLL,
            0,0,10,10,h,(HMENU)MG_E_NOTE,g_hInst,0);
        s_mg->bNoteAttach=createFlatButton(h,MG_NOTE_ATTACH,L"تصویر",ICO_ID,BS_OUTLINE,0,0,10,10);
        s_mg->bNoteSend=createFlatButton(h,MG_NOTE_SEND,L"ذخیره",ICO_SAVE,BS_PRIMARY,0,0,10,10);
        s_mg->noteHot=-1;
        // ---- patients page controls (v1.3.0) ----
        s_mg->ptSearch=CreateWindowExW(0,L"EDIT",L"",
            WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL,0,0,10,10,h,(HMENU)MG_PT_SEARCH,g_hInst,0);
        SendMessageW(s_mg->ptSearch,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        s_mg->ptAdd=createFlatButton(h,MG_PT_ADD,L"افزودن بیمار",ICO_PLUS,BS_PRIMARY,0,0,10,10);
        s_mg->ptList=CreateWindowExW(0,WC_LISTVIEWW,L"",
            WS_CHILD|WS_TABSTOP|LVS_OWNERDATA|LVS_REPORT|LVS_SHOWSELALWAYS|LVS_SINGLESEL,
            0,0,10,10,h,(HMENU)MG_PT_LIST,g_hInst,0);
        // Native GRIDLINES use fixed system-light colors under Wine; omit them
        // so the themed body stays clean in both dark and light modes.
        ListView_SetExtendedListViewStyle(s_mg->ptList,
            LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
        SendMessageW(s_mg->ptList,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        {   // RTL report columns (rightmost first): #, name, nid, mobile,
            // v1.40.0 landline, gender, birth.
            struct { const wchar_t* t; int w; } cols[]={
                {L"#",S(44)},{L"نام و نام خانوادگی",S(210)},{L"کد ملی",S(125)},
                {L"تلفن همراه",S(130)},{L"تلفن ثابت",S(120)},{L"جنسیت",S(70)},{L"تاریخ تولد",S(115)} };
            LVCOLUMNW lc={0}; lc.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_FMT; lc.fmt=LVCFMT_RIGHT;
            for(int i=0;i<7;i++){ lc.pszText=(LPWSTR)cols[i].t; lc.cx=cols[i].w;
                ListView_InsertColumn(s_mg->ptList,i,&lc); }
        }
        applyThemedListView(s_mg->ptList);
        // ---- services page controls (v1.28.0) ----
        s_mg->svCode  =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_SV_CODE,g_hInst,0);
        s_mg->svName  =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_SV_NAME,g_hInst,0);
        s_mg->svCat   =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_SV_CAT,g_hInst,0);
        s_mg->svDept  =createThemedCombo(h,MG_SV_DEPT);
        s_mg->svPrice =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_SV_PRICE,g_hInst,0);
        s_mg->svIns   =createThemedCombo(h,MG_SV_INS);
        s_mg->svStatus=createThemedCombo(h,MG_SV_STATUS);
        s_mg->svDesc  =CreateWindowExW(0,L"EDIT",L"",
            WS_CHILD|WS_TABSTOP|ES_AUTOVSCROLL|ES_MULTILINE|WS_VSCROLL,
            0,0,10,10,h,(HMENU)MG_SV_DESC,g_hInst,0);
        // v2.01 (Part C) service-definition fields
        s_mg->svShort  =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_SV_SHORT, g_hInst,0);
        s_mg->svLoving =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_SV_LOVING,g_hInst,0);
        s_mg->svEquiv  =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_SV_EQUIV, g_hInst,0);
        s_mg->svSrvId  =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_SV_SRVID, g_hInst,0);
        s_mg->svRevGrp =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_SV_REVGRP,g_hInst,0);
        s_mg->svHnId   =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_SV_HNID,  g_hInst,0);
        s_mg->svFree   =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_SV_FREE,  g_hInst,0);
        s_mg->svGov    =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_SV_GOV,   g_hInst,0);
        s_mg->svFreeOld=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_SV_FREEOLD,g_hInst,0);
        s_mg->svGovOld =CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)MG_SV_GOVCOLD,g_hInst,0);
        s_mg->svSave  =createFlatButton(h,MG_SV_SAVE,  L"ذخیره",  ICO_SAVE, BS_PRIMARY,0,0,10,10);
        s_mg->svEdit  =createFlatButton(h,MG_SV_EDIT,  L"ویرایش", ICO_GEAR, BS_OUTLINE,0,0,10,10);
        s_mg->svDelete=createFlatButton(h,MG_SV_DELETE,L"حذف",    ICO_TRASH,BS_DANGER, 0,0,10,10);
        s_mg->svClear =createFlatButton(h,MG_SV_CLEAR, L"فرم جدید",ICO_PLUS, BS_OUTLINE,0,0,10,10);
        // ---- doctors page controls (v1.69.0) ----
        mgBuildDocsPage(h);
        HWND pageBtns[]={s_mg->bAdd,s_mg->bAutoId,s_mg->bSecNew,s_mg->bSecBack,
            s_mg->bAbc,s_mg->bNew,s_mg->bCreated,s_mg->bAddEmp,s_mg->bMsg,
            s_mg->bReqs,s_mg->bProfReqs,s_mg->bNoteAttach,s_mg->bNoteSend,
            s_mg->ptAdd,s_mg->svSave,s_mg->svEdit,s_mg->svDelete,s_mg->svClear,
            s_mg->dcSave,s_mg->dcEdit,s_mg->dcDelete,s_mg->dcClear,
            s_mg->dcActiveChk,s_mg->dcPrintChk,s_mg->dcPerfChk};
        for(HWND b:pageBtns) setFlatButtonBg(b,g_theme.surface);
        s_mg->svSearch=CreateWindowExW(0,L"EDIT",L"",
            WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL,0,0,10,10,h,(HMENU)MG_SV_SEARCH,g_hInst,0);
        s_mg->svList  =CreateWindowExW(0,WC_LISTVIEWW,L"",
            WS_CHILD|WS_TABSTOP|LVS_OWNERDATA|LVS_REPORT|LVS_SHOWSELALWAYS|LVS_SINGLESEL,
            0,0,10,10,h,(HMENU)MG_SV_LIST,g_hInst,0);
        ListView_SetExtendedListViewStyle(s_mg->svList,
            LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
        SendMessageW(s_mg->svList,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        {   // RTL columns (rightmost first): ردیف | کد خدمات | کد معادل | نام اختصار |
            // نام کامل | کد ملی خدمت | نرخ آزاد | نرخ دولتی
            struct { const wchar_t* t; int w; } cols[]={
                {L"ردیف",S(50)},{L"کد خدمات",S(90)},{L"کد معادل",S(90)},
                {L"نام اختصار",S(130)},{L"نام کامل",S(220)},{L"کد ملی خدمت",S(120)},
                {L"نرخ آزاد",S(120)},{L"نرخ دولتی",S(120)} };
            LVCOLUMNW lc={0}; lc.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_FMT; lc.fmt=LVCFMT_RIGHT;
            for(int i=0;i<8;i++){ lc.pszText=(LPWSTR)cols[i].t; lc.cx=cols[i].w;
                ListView_InsertColumn(s_mg->svList,i,&lc); }
        }
        applyThemedListView(s_mg->svList);
        SendMessageW(s_mg->svSearch,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        { HWND se[]={s_mg->svCode,s_mg->svName,s_mg->svCat,s_mg->svPrice,s_mg->svDesc,
                     s_mg->svShort,s_mg->svLoving,s_mg->svEquiv,s_mg->svSrvId,
                     s_mg->svRevGrp,s_mg->svHnId,s_mg->svFree,s_mg->svGov,
                     s_mg->svFreeOld,s_mg->svGovOld};
          for(HWND e:se){ SendMessageW(e,WM_SETFONT,(WPARAM)g_fUI,TRUE); enableEnterNavigation(e); }
          HWND sc[]={s_mg->svDept,s_mg->svIns,s_mg->svStatus};
          for(HWND c:sc) SendMessageW(c,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        }
        // status combo values
        SendMessageW(s_mg->svStatus,CB_ADDSTRING,0,(LPARAM)L"فعال");
        SendMessageW(s_mg->svStatus,CB_ADDSTRING,0,(LPARAM)L"غیرفعال");
        SendMessageW(s_mg->svStatus,CB_SETCURSEL,0,0);
        // insurance-type combo values (labels only; no pricing here)
        { const wchar_t* it[]={L"آزاد",L"بیمه پایه",L"بیمه پایه + مکمل"};
          for(int i=0;i<3;i++) SendMessageW(s_mg->svIns,CB_ADDSTRING,0,(LPARAM)it[i]);
          SendMessageW(s_mg->svIns,CB_SETCURSEL,0,0); }

        SendMessageW(s_mg->eNote,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        HWND eds[3]={s_mg->eName,s_mg->eMgr,s_mg->eId};
        for(HWND e:eds){ SendMessageW(e,WM_SETFONT,(WPARAM)g_fUI,TRUE); enableEnterNavigation(e); }
        g_hManage=h;
        mgLoadCats(); mgUpdateAutoIdPreview();
        mgGoPage(h,PG_DASH);
#ifdef AZ_DEBUG_BUILD
        {   // headless screenshot helper: jump straight to a named sub-page
            wchar_t dbgp[24]={0};
            GetEnvironmentVariableW(L"AZ_DEBUG_PAGE", dbgp, 24);
            if(dbgp[0]){
                int pg=-1;
                if(!wcscmp(dbgp,L"saved"))   pg=PG_SAVED;
                else if(!wcscmp(dbgp,L"reqs"))   pg=PG_REQS;
                else if(!wcscmp(dbgp,L"backup")) pg=PG_BACKUP;
                else if(!wcscmp(dbgp,L"msg"))    pg=PG_MSG;
                else if(!wcscmp(dbgp,L"emps"))   pg=PG_EMPS;
                else if(!wcscmp(dbgp,L"sects"))  pg=PG_SECTS;
                else if(!wcscmp(dbgp,L"services")) pg=PG_SERVICES;
                else if(!wcscmp(dbgp,L"docs"))    pg=PG_DOCS;
                if(pg>=0){ mgGoPage(h,pg); InvalidateRect(h,NULL,TRUE); }
            }
        }
#endif
        return 0; }
    case WM_NCDESTROY: if(g_hManage==h) g_hManage=NULL;
        if(s_mg && s_mg->dashAnimOn) KillTimer(h,MG_DASH_ANIM_TIMER);  // v1.69.0
        mgFreeShadowCache();   // v1.68.0: release cached modal shadow DC
        if(s_mg){ delete s_mg->ptAll; s_mg->ptAll=nullptr;
                  delete s_mg; s_mg=NULL; } break;
    case WM_SIZE: if(s_mg) mgLayout(h); return 0;
    case WM_APP_THEME:
        if(s_mg){
            HWND pageBtns[]={s_mg->bAdd,s_mg->bAutoId,s_mg->bSecNew,s_mg->bSecBack,
                s_mg->bAbc,s_mg->bNew,s_mg->bCreated,s_mg->bAddEmp,s_mg->bMsg,
                s_mg->bReqs,s_mg->bProfReqs,s_mg->bNoteAttach,s_mg->bNoteSend,
                s_mg->ptAdd,s_mg->svSave,s_mg->svEdit,s_mg->svDelete,s_mg->svClear,
                s_mg->dcSave,s_mg->dcEdit,s_mg->dcDelete,s_mg->dcClear,
                s_mg->dcActiveChk,s_mg->dcPrintChk};
            for(HWND b:pageBtns) setFlatButtonBg(b,g_theme.surface);
            applyThemedListView(s_mg->ptList);
            applyThemedListView(s_mg->svList);
            applyThemedListView(s_mg->dcList);
        }
        mgFreeShadowCache();   // v1.68.0: shadow cache is theme-coloured
        InvalidateRect(h,NULL,TRUE); return 0;
    case WM_CTLCOLOREDIT:{ HDC dc=(HDC)w; SetTextColor(dc,g_theme.inputText);
        SetBkColor(dc,g_theme.inputBg); return (LRESULT)g_brInput; }
    case WM_CTLCOLORLISTBOX:{ HDC dc=(HDC)w; SetTextColor(dc,g_theme.inputText);
        SetBkColor(dc,g_theme.inputBg); return (LRESULT)g_brInput; }
    case WM_CTLCOLORSTATIC:{ HDC dc=(HDC)w; SetTextColor(dc,g_theme.inputText);
        SetBkColor(dc,g_theme.inputBg); return (LRESULT)g_brInput; }
    case WM_MG_PATIENTS_READY:{
        auto* vec=(std::vector<PatientRow>*)l;
        if(!s_mg){ delete vec; return 0; }
        delete s_mg->ptAll; s_mg->ptAll=vec;
        s_mg->ptLoaded=true; s_mg->ptLoading=false;
        mgPatientsFilter();
        if(s_mg->page==PG_PATIENTS) InvalidateRect(h,NULL,TRUE);
        return 0; }
    case WM_TIMER:
        if(s_mg && w==0xB1 /*patients search debounce*/){
            KillTimer(h,0xB1);
            wchar_t b[256]={0}; if(s_mg->ptSearch) GetWindowTextW(s_mg->ptSearch,b,256);
            s_mg->ptQuery=b; mgPatientsFilter();
            return 0;
        }
        // v1.69.0: dashboard staggered entrance — advance 0→1 (~350 ms), repaint
        // while running, then stop the timer so steady state has zero overhead.
        if(s_mg && w==MG_DASH_ANIM_TIMER){
            s_mg->dashAnim += 0.045f;
            if(s_mg->dashAnim>=1.0f){ s_mg->dashAnim=1.0f; mgDashAnimStop(h); }
            InvalidateRect(h,NULL,FALSE);
            return 0;
        }
        break;
    case WM_DRAWITEM:
        if(drawThemedComboItem((LPDRAWITEMSTRUCT)l)) return TRUE;
        break;
    case WM_NOTIFY:{
        if(!s_mg) break;
        NMHDR* nh=(NMHDR*)l;
        LRESULT headerResult=0;
        if(nh && drawThemedListViewHeader((LPNMCUSTOMDRAW)l,&headerResult))
            return headerResult;
        if(nh && nh->hwndFrom==s_mg->ptList){
            if(nh->code==LVN_GETDISPINFOW){
                NMLVDISPINFOW* di=(NMLVDISPINFOW*)l;
                int vi=di->item.iItem;
                if(s_mg->ptAll && vi>=0 && vi<(int)s_mg->ptView.size() &&
                   (di->item.mask & LVIF_TEXT)){
                    const PatientRow& p=(*s_mg->ptAll)[s_mg->ptView[vi]];
                    static wchar_t cell[256];
                    switch(di->item.iSubItem){
                        case 0: wcsncpy(cell,toFaDigits(std::to_wstring(vi+1)).c_str(),255); break;
                        case 1: wcsncpy(cell,(p.first+L" "+p.last).c_str(),255); break;
                        case 2: wcsncpy(cell,toFaDigits(p.nid).c_str(),255); break;
                        case 3: wcsncpy(cell,toFaDigits(p.mobile).c_str(),255); break;
                        case 4: wcsncpy(cell,toFaDigits(p.landline).c_str(),255); break; // v1.40.0
                        case 5: wcsncpy(cell,mgGenderText(p.gender),255); break;
                        case 6: wcsncpy(cell,toFaDigits(p.birth).c_str(),255); break;
                        default: cell[0]=0; break;
                    }
                    cell[255]=0; di->item.pszText=cell;
                }
                return 0;
            }
        }
        // ---- services table (v1.28.0) ----
        if(nh && nh->hwndFrom==s_mg->svList){
            if(nh->code==LVN_GETDISPINFOW){
                NMLVDISPINFOW* di=(NMLVDISPINFOW*)l;
                int vi=di->item.iItem;
                if(vi>=0 && vi<(int)s_mg->svView.size() && (di->item.mask & LVIF_TEXT)){
                    const ServiceDef& s=s_mg->svAll[s_mg->svView[vi]];
                    static wchar_t cell[256];
                    switch(di->item.iSubItem){
                        case 0: wcsncpy(cell,toFaDigits(std::to_wstring(vi+1)).c_str(),255); break;            // ردیف
                        case 1: wcsncpy(cell,toFaDigits(s.code).c_str(),255); break;                           // کد خدمات
                        case 2: wcsncpy(cell,toFaDigits(s.equivCode).c_str(),255); break;                      // کد معادل
                        case 3: wcsncpy(cell,s.shortName.c_str(),255); break;                                  // نام اختصار
                        case 4: wcsncpy(cell,s.name.c_str(),255); break;                                       // نام کامل
                        case 5: wcsncpy(cell,toFaDigits(s.healthNationalId).c_str(),255); break;               // کد ملی خدمت
                        case 6: wcsncpy(cell,toFaDigits(formatMoney(s.priceFree)).c_str(),255); break;         // نرخ آزاد
                        case 7: wcsncpy(cell,toFaDigits(formatMoney(s.priceGov)).c_str(),255); break;          // نرخ دولتی
                        default: cell[0]=0; break;
                    }
                    cell[255]=0; di->item.pszText=cell;
                }
                return 0;
            }
            if(nh->code==NM_DBLCLK){ mgServicesEditSelected(h); return 0; }
        }
        // ---- doctors table (v1.69.0) ----
        if(nh && nh->hwndFrom==s_mg->dcList){
            if(nh->code==LVN_GETDISPINFOW){
                NMLVDISPINFOW* di=(NMLVDISPINFOW*)l;
                int vi=di->item.iItem;
                if(vi>=0 && vi<(int)s_mg->dcView.size() && (di->item.mask & LVIF_TEXT)){
                    const DoctorDef& d=s_mg->dcAll[s_mg->dcView[vi]];
                    static wchar_t cell[256];
                    switch(di->item.iSubItem){
                        case 0: wcsncpy(cell,toFaDigits(std::to_wstring(vi+1)).c_str(),255); break;
                        case 1: wcsncpy(cell,d.name.c_str(),255); break;
                        case 2: wcsncpy(cell,d.specialty.c_str(),255); break;
                        case 3: wcsncpy(cell,d.docType==1?L"پرستار":L"پزشک",255); break;
                        case 4: wcsncpy(cell,d.active?L"فعال":L"غیرفعال",255); break;
                        default: cell[0]=0; break;
                    }
                    cell[255]=0; di->item.pszText=cell;
                }
                return 0;
            }
            if(nh->code==NM_DBLCLK){ mgDocsEditSelected(h); return 0; }
        }
        // v2.01 (Part F2): shift management table
        if(nh && nh->hwndFrom==s_mg->shList){
            if(nh->code==LVN_GETDISPINFOW){
                NMLVDISPINFOW* di=(NMLVDISPINFOW*)l;
                int vi=di->item.iItem;
                if(vi>=0 && vi<(int)s_mg->shAll.size() && (di->item.mask & LVIF_TEXT)){
                    const ShiftDef& sd=s_mg->shAll[vi];
                    static wchar_t shcell[128];
                    switch(di->item.iSubItem){
                        case 0: wcsncpy(shcell,toFaDigits(std::to_wstring(vi+1)).c_str(),127); break;
                        case 1: wcsncpy(shcell,sd.name.c_str(),127); break;
                        case 2: { wchar_t t[16]; swprintf(t,16,L"%02d:%02d",sd.startMin/60,sd.startMin%60);
                                 wcsncpy(shcell,toFaDigits(std::wstring(t)).c_str(),127); } break;
                        case 3: { wchar_t t[16]; swprintf(t,16,L"%02d:%02d",sd.endMin/60,sd.endMin%60);
                                 wcsncpy(shcell,toFaDigits(std::wstring(t)).c_str(),127); } break;
                        default: shcell[0]=0; break;
                    }
                    shcell[127]=0; di->item.pszText=shcell;
                }
                return 0;
            }
            if(nh->code==NM_DBLCLK){ mgShiftsEditSelected(h); return 0; }
        }
        break; }
    case WM_COMMAND:{
        if(!s_mg) return 0;
        int id=LOWORD(w);
        if(id==MG_PT_SEARCH && HIWORD(w)==EN_CHANGE){
            SetTimer(h,0xB1,250,NULL);  // debounce 250 ms
            return 0;
        }
        if(id==MG_SV_SEARCH && HIWORD(w)==EN_CHANGE){
            wchar_t b[256]={0}; GetWindowTextW(s_mg->svSearch,b,256);
            s_mg->svQuery=b; mgServicesFilter();
            return 0;
        }
        if(id==MG_SV_PRICE && HIWORD(w)==EN_CHANGE){
            mgServicesFormatPriceLive();
            return 0;
        }
        if(id==MG_SV_FREE && HIWORD(w)==EN_CHANGE){    mgServicesFormatFreeLive();    return 0; }
        if(id==MG_SV_GOV && HIWORD(w)==EN_CHANGE){     mgServicesFormatGovLive();     return 0; }
        if(id==MG_SV_FREEOLD && HIWORD(w)==EN_CHANGE){ mgServicesFormatFreeOldLive(); return 0; }
        if(id==MG_SV_GOVCOLD && HIWORD(w)==EN_CHANGE){ mgServicesFormatGovOldLive();  return 0; }
        if(id==MG_DC_SEARCH && HIWORD(w)==EN_CHANGE){
            wchar_t b[256]={0}; GetWindowTextW(s_mg->dcSearch,b,256);
            s_mg->dcQuery=b; mgDocsFilter();
            return 0;
        }
        switch(id){
            case MG_SV_SAVE:   mgServicesSave(h); return 0;
            case MG_SV_EDIT:   mgServicesEditSelected(h); return 0;
            case MG_SV_DELETE: mgServicesDelete(h); return 0;
            case MG_SV_CLEAR:  mgServicesClearForm(); s_mg->msg.clear();
                               InvalidateRect(h,NULL,TRUE); return 0;
            // ---- doctors page (v1.69.0) ----
            case MG_DC_SAVE:   mgDocsSave(h); return 0;
            case MG_DC_EDIT:   mgDocsEditSelected(h); return 0;
            case MG_DC_DELETE: mgDocsDelete(h); return 0;
            case MG_DC_CLEAR:  mgDocsClearForm(); s_mg->msg.clear();
                               InvalidateRect(h,NULL,TRUE); return 0;
            case MG_DC_ACTIVE: s_mg->dcActiveVal=!s_mg->dcActiveVal;
                               mgDocsRefreshChecks(); InvalidateRect(h,NULL,FALSE); return 0;
            case MG_DC_PRINT:  s_mg->dcPrintVal=!s_mg->dcPrintVal;
                               mgDocsRefreshChecks(); InvalidateRect(h,NULL,FALSE); return 0;
            case MG_DC_PERF:   s_mg->dcPerfVal=!s_mg->dcPerfVal;
                               mgDocsRefreshChecks(); InvalidateRect(h,NULL,FALSE); return 0;
            case MG_DC_SITE:   s_mg->dcSiteVal=!s_mg->dcSiteVal;
                               mgDocsRefreshChecks(); InvalidateRect(h,NULL,FALSE); return 0;
            case MG_DC_ONLINE: s_mg->dcOnlineVal=!s_mg->dcOnlineVal;
                               mgDocsRefreshChecks(); InvalidateRect(h,NULL,FALSE); return 0;
            case MG_DC_ANES:   s_mg->dcAnesVal=!s_mg->dcAnesVal;
                               mgDocsRefreshChecks(); InvalidateRect(h,NULL,FALSE); return 0;
            // v2.01 (Part F2): shift management command handlers
            case MG_SH_SAVE:   mgShiftsSave(h); return 0;
            case MG_SH_DELETE: mgShiftsDelete(h); return 0;
            case MG_SH_CLEAR:  mgShiftsClearForm(); s_mg->msg.clear();
                               InvalidateRect(h,NULL,TRUE); return 0;
        }
        if(id==MG_CB_DEPT && HIWORD(w)==CBN_SELCHANGE){
            int sel=(int)SendMessageW(s_mg->cDept,CB_GETCURSEL,0,0);
            auto cats=loadDepts();
            if(sel<=0) s_mg->empDeptFilter.clear();
            else if(sel-1<(int)cats.size()) s_mg->empDeptFilter=cats[sel-1].id;
            mgLoadEmps(); InvalidateRect(h,NULL,TRUE);
            return 0;
        }
        switch(id){
        case MG_ADD_CAT: mgDoAddCat(h); break;
        case MG_CB_AUTOID: s_mg->autoId=!s_mg->autoId;
            SetWindowTextW(s_mg->bAutoId,s_mg->autoId?L"\u2611 شناسه خودکار":L"\u2610 شناسه دستی");
            mgLayout(h); InvalidateRect(h,NULL,FALSE); break;
        case MG_DESIGN_SEC: s_mg->secAddMode=true; s_mg->msg.clear();
            mgUpdateAutoIdPreview(); mgLayout(h); InvalidateRect(h,NULL,TRUE); break;
        case MG_BACK: s_mg->secAddMode=false; mgLayout(h); InvalidateRect(h,NULL,TRUE); break;
        case MG_FILTER_ABC: s_mg->empSort=0; mgRefreshSortBtns(); mgLoadEmps(); InvalidateRect(h,NULL,FALSE); break;
        case MG_FILTER_NEW: s_mg->empSort=1; mgRefreshSortBtns(); mgLoadEmps(); InvalidateRect(h,NULL,FALSE); break;
        case MG_FILTER_CREATED: s_mg->empSort=2; mgRefreshSortBtns(); mgLoadEmps(); InvalidateRect(h,NULL,FALSE); break;
        case MG_ADD_EMP: openNewEmployee(h); break;
        case MG_PT_ADD: openNewPatient(h); break;   // §1.19.0 add-patient modal
        case MG_MSG:  openMsgCompose(h); break;
        case MG_REQS: openSetReqs(h); break;
        case MG_PROFREQS: openProfReqs(h); break;
        case MG_NOTE_ATTACH:{
            wchar_t file[1024]={0};
            OPENFILENAMEW ofn={0}; ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=h;
            ofn.lpstrFilter=L"تصاویر\0*.jpg;*.jpeg;*.png;*.gif;*.bmp\0همهٔ فایل‌ها\0*.*\0\0";
            ofn.lpstrFile=file; ofn.nMaxFile=1024;
            ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
            if(GetOpenFileNameW(&ofn)){
                std::wstring stored=copyAttachmentLocal(file);
                s_mg->noteAttach = stored.empty()? std::wstring(file) : stored;
                SetWindowTextW(s_mg->bNoteAttach,L"تصویر ✓");
                InvalidateRect(h,NULL,FALSE);
            }
            break; }
        case MG_NOTE_SEND:{
            wchar_t buf[4096]={0}; GetWindowTextW(s_mg->eNote,buf,4096);
            std::wstring txt=trim(buf);
            if(txt.empty() && s_mg->noteAttach.empty()){
                MessageBoxW(h,L"متن یادداشت یا تصویری وارد کنید.",L"پیام‌های ذخیره‌شده",
                    MB_ICONINFORMATION|MB_OK);
                break;
            }
            pushLocalNote(g_session.user.username,txt,s_mg->noteAttach);
            s_mg->noteAttach.clear();
            SetWindowTextW(s_mg->eNote,L"");
            SetWindowTextW(s_mg->bNoteAttach,L"تصویر");
            InvalidateRect(h,NULL,TRUE);
            break; }
        }
        return 0; }
    case WM_MOUSEMOVE:{ POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        int hr=mgHit(h,pt);
        int nh=-1;
        if(hr>=MG_NAV_BASE && hr<MG_NAV_BASE+PG_COUNT) nh=hr-MG_NAV_BASE;
        bool ch=false;
        if(s_mg){ if(s_mg->hot!=hr){ s_mg->hot=hr; ch=true; }
                  if(s_mg->navHot!=nh){ s_mg->navHot=nh; ch=true; } }
        if(s_mg && s_mg->page==PG_SAVED){
            int onh=s_mg->noteHot; s_mg->noteHot=-1;
            int cnt=localNoteCount(g_session.user.username);
            for(int i=0;i<cnt;i++){ RECT r=mgNoteRowRect(h,i);
                if(PtInRect(&r,pt)){ s_mg->noteHot=i; break; } }
            if(onh!=s_mg->noteHot) ch=true;
        }
        if(ch) InvalidateRect(h,NULL,FALSE);
        return 0; }
    case WM_LBUTTONDOWN:{ POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        // saved-messages page: note delete / image-open hit-tests first
        if(s_mg && s_mg->page==PG_SAVED){
            auto notes=loadLocalNotes(g_session.user.username);
            for(int i=0;i<(int)notes.size();i++){
                RECT r=mgNoteRowRect(h,i);
                RECT dl=mgNoteDelRect(r), im=mgNoteImgRect(r);
                if(PtInRect(&dl,pt)){
                    if(MessageBoxW(h,L"این یادداشت حذف شود؟",L"حذف یادداشت",
                        MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2)==IDYES){
                        deleteLocalNote(g_session.user.username,i);
                        InvalidateRect(h,NULL,TRUE);
                    }
                    return 0;
                }
                if(!notes[i].attachPath.empty() && PtInRect(&im,pt)){
                    ShellExecuteW(h,L"open",notes[i].attachPath.c_str(),NULL,NULL,SW_SHOWNORMAL);
                    return 0;
                }
            }
        }
        int id=mgHit(h,pt);
        if(id==MG_BACKUP){ openBackupManager(h); }
        else if(id>=MG_NAV_BASE && id<MG_NAV_BASE+PG_COUNT){ mgGoPage(h,id-MG_NAV_BASE); }
        else if(id>=MG_QA_BASE && id<MG_QA_BASE+4){
            int i=id-MG_QA_BASE;
            if(i==0){ mgGoPage(h,PG_SECTS); s_mg->secAddMode=true; mgUpdateAutoIdPreview(); mgLayout(h); InvalidateRect(h,NULL,TRUE); }
            else if(i==1){ mgGoPage(h,PG_EMPS); openNewEmployee(h); }
            else if(i==2){ mgGoPage(h,PG_MSG); openMsgCompose(h); }
            else if(i==3){ mgGoPage(h,PG_REQS); }
        }
        else if(id>=MG_DETAIL_BASE){ int i=id-MG_DETAIL_BASE;
            if(i<(int)s_mg->emps.size()) openEmpDetails(h,s_mg->emps[i]); }
        else if(id>=MG_CARD_BASE){ int i=id-MG_CARD_BASE;
            if(i<(int)s_mg->cats.size()){
                // clicking a section card jumps to its employees, filtered
                s_mg->empDeptFilter=s_mg->cats[i].id;
                mgGoPage(h,PG_EMPS);
            }
        }
        return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:{ PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        mgPaintShell(dc,h);
        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps); return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}

HWND createManageScreen(HWND frame){
    // v1.70.0: the native GDI manage panel is DISABLED. The management surface
    // is now an embedded HTML CRM (assets/crm, hosted by src/web_crm.*) shown via
    // WebCrm_CreateScreen — the same WebView2/MSHTML pattern the admission page
    // uses. SC_MANAGE switches to that screen. The ORIGINAL native GDI panel
    // body is preserved verbatim under #if 0 below so it can be re-enabled by
    // flipping the guard (and removing the WebCrm_CreateScreen return).
    return WebCrm_CreateScreen(frame);
#if 0
    // ---- original native GDI manage panel (disabled v1.70.0) ----
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc={0};
        wc.lpfnWndProc=manageProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.lpszClassName=MG_CLASS;
        RegisterClassW(&wc); reg=true;
    }
    RECT rc=frameContentRect();
    HWND h=CreateWindowExW(0,MG_CLASS,L"",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
        rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,frame,NULL,g_hInst,NULL);
    if(s_mg) mgLayout(h);
    return h;
#endif
}

// §1.19.1 — public «تنظیمات چاپ» entry, reachable from the management Settings
// menu (user_settings.cpp). Opens the print-settings panel as a MODAL surface
// (its own message pump) so it works whether or not the management screen is
// the active surface. Independent of the dashboard's s_mg state.
void PrintCfg_Open(HWND hMain){
    HWND owner = hMain ? hMain : g_hFrame;
    openPrintCfg(owner);
    if(!s_pc || !IsWindow(s_pc)) return;
    if(owner && IsWindow(owner)) EnableWindow(owner,FALSE);
    MSG msg;
    while(s_pc && IsWindow(s_pc) && GetMessageW(&msg,NULL,0,0)){
        if(msg.message==WM_QUIT){ PostQuitMessage((int)msg.wParam); break; }
        if(!(s_pc && IsWindow(s_pc))) break;
        if(IsDialogMessageW(s_pc,&msg)){ if(!(s_pc && IsWindow(s_pc))) break; continue; }
        TranslateMessage(&msg); DispatchMessageW(&msg);
        if(!(s_pc && IsWindow(s_pc))) break;
    }
    if(owner && IsWindow(owner)){ EnableWindow(owner,TRUE); SetForegroundWindow(owner); }
}
