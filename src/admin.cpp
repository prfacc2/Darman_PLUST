// ============================================================================
//  admin.cpp — hidden admin panel (create users, list/delete users)
//              + management panel placeholder (extensible)
// ============================================================================
#include "app.h"
#include "ui_kit.h"
#include "print_designer.h"   // §1.19.0: print-settings (section design preview/apply)
#include "sections.h"         // §1.19.0: section list for print-settings page
#include "web_admission.h"    // live catalog/insurance sync to embedded Admission
#include "web_crm.h"          // v1.70.0: embedded HTML CRM management surface
#include <commctrl.h>
#include <stdio.h>
#include <algorithm>
#include <thread>
#include <atomic>

// ================================================================ ADMIN ====
#define AD_CLASS L"AzAdmin"
#define ID_AD_FULL    401
#define ID_AD_USER    402
#define ID_AD_PASS    403
#define ID_AD_DEPT    404
#define ID_AD_ROLE    405
#define ID_AD_CREATE  406
#define ID_AD_LIST    407
#define ID_AD_DELETE  408
// v1.10.0 — patients tab controls
#define ID_AD_PSEARCH 410
#define ID_AD_PLIST   411
#define ID_AD_PDEL    412

// custom messages posted by the background patient loader thread
#define WM_AD_PATIENTS_READY (WM_APP+71)

//  v1.3.0: the «بیماران» (patients) tab was MOVED OUT of this hidden-admin
//  (owner/developer backdoor) surface and into the clinic-management panel,
//  where day-to-day operational data belongs (see manage.inc, PG_PATIENTS).
//  AD_TAB_COUNT is therefore 1 (only «کاربران») so the patients tab is no
//  longer reachable from this surface. The internal AD_TAB_PATIENTS id is kept
//  only so the legacy (now-unreachable) code paths still compile cleanly.
enum { AD_TAB_USERS = 0, AD_TAB_PATIENTS = 1, AD_TAB_COUNT = 1 };

struct AdminData {
    HWND eFull, eUser, ePass, eDept, cRole, bCreate, list, bDelete;
    std::wstring msg; COLORREF msgCol;

    // ---- patients tab ----
    int   tab = AD_TAB_USERS;
    HWND  pSearch = nullptr;       // search box
    HWND  pList   = nullptr;       // LVS_OWNERDATA virtual list
    HWND  bPDel   = nullptr;       // delete-patient button
    RECT  tabRects[AD_TAB_COUNT] = {};

    // full data set (loaded once on a worker thread) + current filtered view
    std::vector<PatientRow>* allPatients = nullptr;   // owned, heap (large)
    std::vector<int>         view;                    // indices into allPatients
    std::wstring             query;                   // last applied search
    bool   loading = false;
    bool   loaded  = false;

    // debounce timer id for the search box
    static const UINT_PTR TIMER_SEARCH = 1;
};

// ====================================================== PATIENTS TAB =======
//  A virtualized (LVS_OWNERDATA) ListView over the local patient store. The
//  data is loaded ONCE on a background thread (never blocks the UI) and the
//  search box filters the in-memory set with debounced 250 ms input. Because
//  the list is owner-data, only the visible rows are ever materialized — it
//  stays responsive with 100k+ rows.
static const wchar_t* genderText(const std::wstring& g){
    if(g==L"1"||g==L"مرد"||g==L"M"||g==L"m") return L"مرد";
    if(g==L"0"||g==L"زن"||g==L"F"||g==L"f")  return L"زن";
    return L"—";
}
//  Recompute d->view from d->query (applied against the full set).
static void adPatientsFilter(AdminData* d){
    d->view.clear();
    if(!d->allPatients) return;
    std::wstring q = uikit::NormalizeFa(d->query);
    const auto& all = *d->allPatients;
    d->view.reserve(all.size());
    if(q.empty()){
        for(int i=0;i<(int)all.size();++i) d->view.push_back(i);
    } else {
        for(int i=0;i<(int)all.size();++i){
            const PatientRow& p = all[i];
            std::wstring name = uikit::NormalizeFa(p.first+L" "+p.last);
            std::wstring nid  = uikit::NormalizeFa(p.nid);
            std::wstring mob  = uikit::NormalizeFa(p.mobile);
            if(name.find(q)!=std::wstring::npos ||
               nid.find(q)!=std::wstring::npos  ||
               mob.find(q)!=std::wstring::npos)
                d->view.push_back(i);
        }
    }
    if(d->pList){
        ListView_SetItemCountEx(d->pList,(int)d->view.size(),
            LVSICF_NOINVALIDATEALL|LVSICF_NOSCROLL);
        InvalidateRect(d->pList,NULL,FALSE);
    }
}
//  Kick off the one-time background load (idempotent).
static void adPatientsLoadAsync(HWND h, AdminData* d){
    if(d->loaded || d->loading) return;
    d->loading = true;
    HWND target = h;
    std::thread([target](){
        auto* vec = new std::vector<PatientRow>(loadAllPatients());
        // marshal back to the UI thread — never touch HWND data off-thread
        PostMessageW(target, WM_AD_PATIENTS_READY, 0, (LPARAM)vec);
    }).detach();
}

static void adRefreshList(AdminData* d){
    ListView_DeleteAllItems(d->list);
    auto us = loadUsers();
    int i=0;
    for(auto& u : us){
        LVITEMW it={0}; it.mask=LVIF_TEXT; it.iItem=i;
        it.pszText=(LPWSTR)u.username.c_str();
        ListView_InsertItem(d->list,&it);
        ListView_SetItemText(d->list,i,1,(LPWSTR)u.fullname.c_str());
        ListView_SetItemText(d->list,i,2,(LPWSTR)u.dept.c_str());
        ListView_SetItemText(d->list,i,3,
            (LPWSTR)(u.role==0?L"پذیرش":L"مدیریت"));
        i++;
    }
}
//  The tab strip sits just under the title. Returns the y of its bottom edge.
static int adTabStripBottom(){ return S(56)+S(38); }
static void adComputeTabRects(HWND h, AdminData* d){
    RECT rc; GetClientRect(h,&rc);
    int pad=S(24), y=S(56), th=S(34);
    const wchar_t* labels[AD_TAB_COUNT]={L"کاربران"};
    // RTL: first tab starts at the right edge and grows leftwards.
    int x = rc.right - pad;
    uikit::WindowDC wdc(h);
    uikit::SelectScope sf(wdc.dc,(HGDIOBJ)g_fUIB);
    for(int i=0;i<AD_TAB_COUNT;i++){
        SIZE sz{0,0};
        GetTextExtentPoint32W(wdc.dc,labels[i],(int)wcslen(labels[i]),&sz);
        int tw = sz.cx + S(36);
        d->tabRects[i].right  = x;
        d->tabRects[i].left   = x - tw;
        d->tabRects[i].top    = y;
        d->tabRects[i].bottom = y + th;
        x -= tw + S(8);
    }
}
static void adApplyTabVisibility(AdminData* d){
    bool users   = (d->tab==AD_TAB_USERS);
    bool pts     = (d->tab==AD_TAB_PATIENTS);
    int su = users?SW_SHOW:SW_HIDE, sp = pts?SW_SHOW:SW_HIDE;
    HWND ucl[]={d->eFull,d->eUser,d->ePass,d->eDept,d->cRole,d->bCreate,
                d->list,d->bDelete};
    for(HWND c:ucl) if(c) ShowWindow(c,su);
    HWND pcl[]={d->pSearch,d->pList,d->bPDel};
    for(HWND c:pcl) if(c) ShowWindow(c,sp);
}
static void adLayout(HWND h, AdminData* d){
    RECT rc; GetClientRect(h,&rc);
    int W=rc.right;
    adComputeTabRects(h,d);
    int top = adTabStripBottom() + S(12);

    // ---------- users tab ----------
    int fw=S(360), pad=S(24);
    int fx=W-pad-fw, fy=top+S(30);
    int ew=fw-S(40), ex=fx+S(20);
    int rh=S(40), gap=S(64);
    MoveWindow(d->eFull, ex, fy+S(28),        ew, rh, TRUE);
    MoveWindow(d->eUser, ex, fy+S(28)+gap,    ew, rh, TRUE);
    MoveWindow(d->ePass, ex, fy+S(28)+2*gap,  ew, rh, TRUE);
    MoveWindow(d->eDept, ex, fy+S(28)+3*gap,  ew, rh, TRUE);
    MoveWindow(d->cRole, ex, fy+S(28)+4*gap,  ew, S(200), TRUE);
    MoveWindow(d->bCreate, ex, fy+S(28)+5*gap+S(8), ew, S(46), TRUE);

    int lx=pad, lw=W-3*pad-fw;
    MoveWindow(d->list, lx, fy, lw, rc.bottom-fy-S(62), TRUE);
    MoveWindow(d->bDelete, lx, rc.bottom-S(54), S(180), S(40), TRUE);

    // ---------- patients tab ----------
    int py = top + S(2);
    if(d->pSearch) MoveWindow(d->pSearch, pad, py, W-2*pad, S(34), TRUE);
    if(d->pList)   MoveWindow(d->pList, pad, py+S(44), W-2*pad,
                              rc.bottom-(py+S(44))-S(62), TRUE);
    if(d->bPDel)   MoveWindow(d->bPDel, pad, rc.bottom-S(54), S(200), S(40), TRUE);
}
static LRESULT CALLBACK adminProc(HWND h, UINT m, WPARAM w, LPARAM l){
    AdminData* d=(AdminData*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_CREATE: {
        d = new AdminData(); d->msgCol = g_theme.textDim;
        SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)d);
        DWORD es=WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL;
        d->eFull=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_AD_FULL,g_hInst,0);
        d->eUser=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_AD_USER,g_hInst,0);
        d->ePass=CreateWindowExW(0,L"EDIT",L"",es|ES_PASSWORD,0,0,10,10,h,(HMENU)ID_AD_PASS,g_hInst,0);
        d->eDept=CreateWindowExW(0,L"EDIT",L"",es,0,0,10,10,h,(HMENU)ID_AD_DEPT,g_hInst,0);
        d->cRole=createThemedCombo(h,ID_AD_ROLE);
        SendMessageW(d->cRole,CB_ADDSTRING,0,(LPARAM)L"پذیرش");
        SendMessageW(d->cRole,CB_ADDSTRING,0,(LPARAM)L"مدیریت");
        SendMessageW(d->cRole,CB_SETCURSEL,0,0);
        d->bCreate=createFlatButton(h,ID_AD_CREATE,L"ساخت کاربر",ICO_PLUS,BS_PRIMARY,0,0,10,10);
        setFlatButtonBg(d->bCreate,g_theme.surface);
        // user table (LAYOUTRTL only on the ListView itself is safe:
        // the control paints itself, no custom BitBlt involved)
        d->list=CreateWindowExW(WS_EX_LAYOUTRTL|WS_EX_RTLREADING,WC_LISTVIEWW,L"",
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,
            0,0,10,10,h,(HMENU)ID_AD_LIST,g_hInst,0);
        // v1.8.0: drop GRIDLINES — borderless, clean, theme-integrated list.
        ListView_SetExtendedListViewStyle(d->list,
            LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
        const wchar_t* cols[4]={L"نام کاربری",L"نام شخص",L"بخش",L"نوع دسترسی"};
        int widths[4]={S(140),S(190),S(170),S(120)};
        for(int i=0;i<4;i++){
            LVCOLUMNW c={0}; c.mask=LVCF_TEXT|LVCF_WIDTH;
            c.pszText=(LPWSTR)cols[i]; c.cx=widths[i];
            ListView_InsertColumn(d->list,i,&c);
        }
        SendMessageW(d->list,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        applyThemedListView(d->list);
        d->bDelete=createFlatButton(h,ID_AD_DELETE,L"حذف کاربر انتخابی",ICO_TRASH,BS_DANGER,0,0,10,10);
        setFlatButtonBg(d->bDelete,g_theme.bg);
        HWND eds[4]={d->eFull,d->eUser,d->ePass,d->eDept};
        for(int i=0;i<4;i++){
            SendMessageW(eds[i],WM_SETFONT,(WPARAM)g_fUI,TRUE);
            enableEnterNavigation(eds[i]);
        }
        SendMessageW(d->cRole,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        adRefreshList(d);

        // ---------- patients tab controls ----------
        d->pSearch=CreateWindowExW(WS_EX_CLIENTEDGE|WS_EX_RTLREADING,L"EDIT",L"",
            WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL,0,0,10,10,h,(HMENU)ID_AD_PSEARCH,g_hInst,0);
        SendMessageW(d->pSearch,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        SendMessageW(d->pSearch,EM_SETCUEBANNER,TRUE,
            (LPARAM)L"جستجوی نام، کد ملی یا موبایل…");
        d->pList=CreateWindowExW(WS_EX_LAYOUTRTL|WS_EX_RTLREADING,WC_LISTVIEWW,L"",
            WS_CHILD|WS_TABSTOP|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS|LVS_OWNERDATA,
            0,0,10,10,h,(HMENU)ID_AD_PLIST,g_hInst,0);
        ListView_SetExtendedListViewStyle(d->pList,
            LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
        {
            const wchar_t* pc[6]={L"کد ملی",L"نام",L"نام خانوادگی",
                                  L"جنسیت",L"تولد",L"موبایل"};
            int pw[6]={S(120),S(140),S(160),S(80),S(110),S(130)};
            for(int i=0;i<6;i++){
                LVCOLUMNW c={0}; c.mask=LVCF_TEXT|LVCF_WIDTH;
                c.pszText=(LPWSTR)pc[i]; c.cx=pw[i];
                ListView_InsertColumn(d->pList,i,&c);
            }
        }
        SendMessageW(d->pList,WM_SETFONT,(WPARAM)g_fUI,TRUE);
        applyThemedListView(d->pList);
        d->bPDel=createFlatButton(h,ID_AD_PDEL,L"حذف بیمار انتخابی",ICO_TRASH,BS_DANGER,0,0,10,10);
        setFlatButtonBg(d->bPDel,g_theme.bg);

        adApplyTabVisibility(d);
        return 0; }
    case WM_NCDESTROY:
        if(d){ delete d->allPatients; delete d; }
        return 0;
    case WM_SIZE: if(d){ adLayout(h,d); adApplyTabVisibility(d); } return 0;

    case WM_AD_PATIENTS_READY: {
        if(!d){ delete (std::vector<PatientRow>*)l; return 0; }
        delete d->allPatients;
        d->allPatients=(std::vector<PatientRow>*)l;
        d->loading=false; d->loaded=true;
        adPatientsFilter(d);
        return 0; }

    case WM_TIMER:
        if(d && w==AdminData::TIMER_SEARCH){
            KillTimer(h,AdminData::TIMER_SEARCH);
            if(d->pSearch){
                wchar_t buf[256]={0};
                GetWindowTextW(d->pSearch,buf,256);
                d->query=buf;
                adPatientsFilter(d);
            }
        }
        return 0;

    case WM_DRAWITEM:
        if(drawThemedComboItem((LPDRAWITEMSTRUCT)l)) return TRUE;
        break;
    case WM_NOTIFY: {
        if(!d) break;
        LPNMHDR nh=(LPNMHDR)l;
        LRESULT headerResult=0;
        if(nh && drawThemedListViewHeader((LPNMCUSTOMDRAW)l,&headerResult))
            return headerResult;
        if(nh->hwndFrom==d->pList){
            if(nh->code==LVN_GETDISPINFOW){
                NMLVDISPINFOW* di=(NMLVDISPINFOW*)l;
                int vi=di->item.iItem;
                if(d->allPatients && vi>=0 && vi<(int)d->view.size()
                   && (di->item.mask & LVIF_TEXT)){
                    const PatientRow& p=(*d->allPatients)[d->view[vi]];
                    const wchar_t* val=L"";
                    switch(di->item.iSubItem){
                        case 0: val=p.nid.c_str();    break;
                        case 1: val=p.first.c_str();  break;
                        case 2: val=p.last.c_str();   break;
                        case 3: val=genderText(p.gender); break;
                        case 4: val=p.birth.c_str();  break;
                        case 5: val=p.mobile.c_str(); break;
                    }
                    // ListView keeps the pointer only until the next call; a
                    // static thread-local buffer keeps the value valid safely.
                    static thread_local wchar_t cell[128];
                    lstrcpynW(cell,val,128);
                    di->item.pszText=cell;
                }
                return 0;
            }
        }
        break; }
    case WM_APP_THEME:        // re-color ListViews + headers in place
        if(d){
            applyThemedListView(d->list);
            applyThemedListView(d->pList);
            if(d->bCreate) setFlatButtonBg(d->bCreate,g_theme.surface);
            if(d->bDelete) setFlatButtonBg(d->bDelete,g_theme.bg);
            if(d->bPDel)   setFlatButtonBg(d->bPDel,g_theme.bg);
        }
        InvalidateRect(h,NULL,TRUE);
        return 0;
    case WM_CTLCOLOREDIT: {
        HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_CTLCOLORLISTBOX: {
        HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_CTLCOLORSTATIC: {
        HDC dc=(HDC)w;
        SetTextColor(dc,g_theme.inputText); SetBkColor(dc,g_theme.inputBg);
        return (LRESULT)g_brInput; }
    case WM_COMMAND: {
        if(!d) return 0;
        int id=LOWORD(w);
        if(id==ID_AD_PSEARCH && HIWORD(w)==EN_CHANGE){
            // debounce 250 ms — restart the timer on every keystroke
            SetTimer(h,AdminData::TIMER_SEARCH,250,NULL);
            return 0;
        }
        if(id==ID_AD_PDEL){
            int sel=ListView_GetNextItem(d->pList,-1,LVNI_SELECTED);
            if(sel>=0 && d->allPatients && sel<(int)d->view.size()){
                const PatientRow& p=(*d->allPatients)[d->view[sel]];
                std::wstring q=L"بیمار «"+p.first+L" "+p.last+L"» («"+p.nid+
                               L"») حذف شود؟";
                std::wstring nid=p.nid;
                if(MessageBoxW(h,q.c_str(),L"حذف بیمار",
                    MB_YESNO|MB_ICONWARNING)==IDYES){
                    if(deletePatient(nid) && d->allPatients){
                        for(auto it=d->allPatients->begin();
                            it!=d->allPatients->end();++it){
                            if(it->nid==nid){ d->allPatients->erase(it); break; }
                        }
                        adPatientsFilter(d);
                    }
                }
            }
            return 0;
        }
        if(id==ID_AD_CREATE){
            wchar_t fb[128],ub[128],pb[128],db[128];
            GetWindowTextW(d->eFull,fb,128); GetWindowTextW(d->eUser,ub,128);
            GetWindowTextW(d->ePass,pb,128); GetWindowTextW(d->eDept,db,128);
            int role=(int)SendMessageW(d->cRole,CB_GETCURSEL,0,0);
            if(!wcslen(pb)){
                d->msg=L"رمز عبور نمی‌تواند خالی باشد."; d->msgCol=g_theme.danger;
            } else {
                User u; u.fullname=trim(fb); u.username=trim(ub);
                u.dept=trim(db); u.role=role; u.hash=hashPassword(pb);
                std::wstring err;
                if(addUser(u,err)){
                    d->msg=L"کاربر «"+u.username+L"» با موفقیت ساخته شد.";
                    d->msgCol=g_theme.success;
                    SetWindowTextW(d->eFull,L""); SetWindowTextW(d->eUser,L"");
                    SetWindowTextW(d->ePass,L""); SetWindowTextW(d->eDept,L"");
                    adRefreshList(d);
                } else { d->msg=err; d->msgCol=g_theme.danger; }
            }
            InvalidateRect(h,NULL,TRUE);
        }
        else if(id==ID_AD_DELETE){
            int sel=ListView_GetNextItem(d->list,-1,LVNI_SELECTED);
            if(sel>=0){
                wchar_t un[128];
                ListView_GetItemText(d->list,sel,0,un,128);
                std::wstring q=L"کاربر «"+std::wstring(un)+L"» حذف شود؟";
                if(MessageBoxW(h,q.c_str(),L"حذف کاربر",
                    MB_YESNO|MB_ICONWARNING)==IDYES){
                    removeUser(un); adRefreshList(d);
                    d->msg=L"کاربر حذف شد."; d->msgCol=g_theme.textDim;
                    InvalidateRect(h,NULL,TRUE);
                }
            }
        }
        return 0; }
    case WM_LBUTTONDOWN: {
        if(!d) return 0;
        POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        for(int i=0;i<AD_TAB_COUNT;i++){
            if(PtInRect(&d->tabRects[i],pt)){
                if(d->tab!=i){
                    d->tab=i;
                    adApplyTabVisibility(d);
                    if(i==AD_TAB_PATIENTS) adPatientsLoadAsync(h,d);
                    adLayout(h,d);
                    InvalidateRect(h,NULL,TRUE);
                }
                break;
            }
        }
        return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0,rc.right,rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        FillRect(dc,&rc,g_brBg);
        SetBkMode(dc,TRANSPARENT);

        SetTextColor(dc,g_theme.text);
        SelectObject(dc,g_fTitle);
        RECT tr={S(24),S(14),rc.right-S(24),S(50)};
        DrawTextW(dc,L"پنل مخفی ادمین",-1,&tr,
            DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);

        // ---- tab strip ----
        adComputeTabRects(h,d);
        const wchar_t* tabLabels[AD_TAB_COUNT]={L"کاربران"};
        SelectObject(dc,g_fUIB);
        for(int i=0;i<AD_TAB_COUNT;i++){
            bool active=(d->tab==i);
            RECT t=d->tabRects[i];
            // v1.63.0: an active tab is a glowing accent pill (elevated,
            // gradient) instead of a flat blue rectangle; inactive tabs get the
            // same soft surface gradient used everywhere else in the app.
            if(active){
                gpShadowColor(dc,t,S(9),S(6),90,g_theme.accent);
                gpGradRoundRect(dc,t,S(9),g_theme.accentHover,g_theme.accent,
                                CLR_INVALID);
            } else {
                gpGradRoundRectBg(dc,t,S(9),
                    blendColor(g_theme.surfaceTop,g_theme.surface,55),
                    g_theme.surface,g_theme.border,g_theme.bg);
            }
            SetTextColor(dc,active?g_theme.accentText:g_theme.textDim);
            DrawTextW(dc,tabLabels[i],-1,&t,
                DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        }

        if(d->tab==AD_TAB_PATIENTS){
            // patients tab is mostly native controls; just a hint line + count
            SelectObject(dc,g_fSmall);
            SetTextColor(dc,g_theme.textDim);
            std::wstring info;
            if(d->loading) info=L"در حال بارگذاری بیماران…";
            else {
                wchar_t b[96];
                swprintf(b,96,L"%d بیمار نمایش داده می‌شود",(int)d->view.size());
                info=toFaDigits(b);
            }
            int py=adTabStripBottom()+S(12)+S(44)+
                   (rc.bottom-(adTabStripBottom()+S(12)+S(44))-S(62))+S(8);
            RECT ir={S(24),py,rc.right-S(24),py+S(22)};
            DrawTextW(dc,info.c_str(),-1,&ir,
                DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
            BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
            SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
            EndPaint(h,&ps);
            return 0;
        }

        // ============ USERS TAB ============
        int topU = adTabStripBottom()+S(12);
        // form card frame
        int fw=S(360), pad=S(24);
        int fx=rc.right-pad-fw, fy=topU+S(30);
        RECT card={fx-S(0),fy-S(20),fx+fw,fy+S(28)+5*S(64)+S(76)};
        // v1.63.0: elevated form card (shadow + surface gradient) to match the
        // reception / management card language.
        gpShadow(dc,card,S(14),S(9),g_dark?76:50);
        gpGradRoundRectBg(dc,card,S(14),g_theme.surfaceTop,g_theme.surface,
                          g_theme.border,g_theme.bg);

        // input wells behind each editable field (consistent flat look + clear
        // separation between a caption and the control beneath it)
        {
            HWND foc=GetFocus();
            HWND fields[5]={d?d->eFull:0,d?d->eUser:0,d?d->ePass:0,
                            d?d->eDept:0,d?d->cRole:0};
            for(int i=0;i<5;i++){
                if(!fields[i]) continue;
                RECT wr; GetWindowRect(fields[i],&wr);
                POINT a={wr.left,wr.top}, b={wr.right,wr.bottom};
                ScreenToClient(h,&a); ScreenToClient(h,&b);
                if(b.y<=a.y) continue;
                int minH=a.y+S(40);
                RECT well={a.x-S(6),a.y-S(4),b.x+S(6),(b.y<minH)?minH:b.y+S(4)};
                bool focused=(fields[i]==foc);
                // v1.63.0: recessed well + accent glow on focus.
                if(focused) gpShadowColor(dc,well,S(9),S(5),70,g_theme.accent);
                gpGradRoundRect(dc,well,S(9),
                    blendColor(g_theme.inputBg,g_theme.border,g_dark?26:34),
                    g_theme.inputBg, focused?g_theme.accent:g_theme.border);
            }
        }

        SelectObject(dc,g_fSmall);
        SetTextColor(dc,g_theme.textDim);
        const wchar_t* labels[5]={L"نام شخص (استفاده‌کننده)",L"نام کاربری (برای ورود)",
            L"رمز عبور",L"بخش (مثلاً دندانپزشکی)",L"نوع دسترسی"};
        for(int i=0;i<5;i++){
            RECT lr={fx+S(20),fy+i*S(64)+S(2),fx+fw-S(20),fy+i*S(64)+S(24)};
            DrawTextW(dc,labels[i],-1,&lr,
                DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);
        }
        // message
        if(d && !d->msg.empty()){
            SetTextColor(dc,d->msgCol);
            RECT mrr={fx+S(10),card.bottom+S(8),fx+fw-S(10),card.bottom+S(60)};
            DrawTextW(dc,d->msg.c_str(),-1,&mrr,
                DT_RIGHT|DT_WORDBREAK|DT_RTLREADING|DT_NOPREFIX);
        }
        // list title
        SetTextColor(dc,g_theme.text);
        SelectObject(dc,g_fUIB);
        RECT lt={pad,topU,rc.right-2*pad-fw,topU+S(26)};
        DrawTextW(dc,L"لیست کاربران",-1,&lt,
            DT_RIGHT|DT_SINGLELINE|DT_RTLREADING|DT_NOPREFIX);

        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}
HWND createAdminScreen(HWND frame){
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc={0};
        wc.lpfnWndProc=adminProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.lpszClassName=AD_CLASS;
        RegisterClassW(&wc); reg=true;
    }
    RECT rc=frameContentRect();
    return CreateWindowExW(0,AD_CLASS,L"",
        WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
        rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,frame,NULL,g_hInst,NULL);
}

// ============================================================ MANAGEMENT ===
//  Full management panel (v1.4.0):
//   • Department CATEGORIES (add name/manager/auto-or-manual id/icon).
//   • Filter the category grid by alphabet / newest.
//   • Click a category → employee TABLE (photo, name, username, shift,
//     online status «برخط»/offline, details button).
//   • Details → formal full info + print preview (A4/A5) of the personnel card.
//   • Create-employee request collects full identity + doc paths.
//   • Printer-design access toggle (per the brief) opens the print designer.
#include "manage.inc"
