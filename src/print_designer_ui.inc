// ============================================================================
//  print_designer_ui.inc — UI for the print designer (release 1.4.0, §3/§4)
//  Included into print_designer.cpp. Contains:
//    • the section picker dialog (§3.1)
//    • the designer window: canvas + inspector + template strip (§3.3+)
//    • the restore-design window (§4)
//  Real WYSIWYG GDI+ canvas; drag/resize/rotate; undo/redo; .aztpl I/O.
// ============================================================================

using namespace Gdiplus;

// §8 (1.14.0): structured, persistent designer error/crash log
// (logs\print_designer.log). Forward-declared at file scope so the namespace'd
// UI code (e.g. OpenDesignerWindow's create-failure path) can call it; the
// definition lives with the public crash-containment entries below.
static void PrintDesigner_Log(const wchar_t* where, const wchar_t* detail);

// v1.17.0: the dangerous longjmp-from-VEH "crash containment" was removed (it
// was the crash — STATUS_LONGJUMP). No armed flags / setjmp landing pads remain.

// ---------------------------------------------------------------- ids --------
namespace {

#define IDC_DZ_SEARCH   2001
#define IDC_DZ_LIST     2002
#define IDC_DZ_OK       2003
#define IDC_DZ_CANCEL   2004

#define IDC_DSG_SAVE    3001
#define IDC_DSG_SAVEAS  3002
#define IDC_DSG_DOWNLOAD 3003
#define IDC_DSG_EXIT    3004
#define IDC_DSG_PAPER   3005
#define IDC_DSG_ORIENT  3006
#define IDC_DSG_ZOOM    3007
#define IDC_DSG_TOOLSEL  3008   // E2: «ابزار انتخاب» (select tool)
#define IDC_DSG_TOOLHAND 3009   // E2: «ابزار دست» (hand tool)

#define IDC_RST_DROP    4001
#define IDC_RST_PICK    4002
#define IDC_RST_APPLY   4003
#define IDC_RST_CANCEL  4004
#define IDC_RST_LIST    4005

// ---- selected sections passed from picker -> designer ----------------------
struct DesignerLaunch {
    std::vector<int> sectionIds;
};

// =====================================================================
//  SECTION PICKER (§3.1)
// =====================================================================
struct PickState {
    std::vector<Section> all, shown;
    std::vector<int>     selected;   // section ids
    HWND hSearch, hList, hOk, hCount;
    bool result;
    bool reloading;                  // §1.12.0 re-entrancy guard for LVN_ITEMCHANGED
    PickState():hSearch(NULL),hList(NULL),hOk(NULL),hCount(NULL),
                result(false),reloading(false){}
};

static void picker_reload(PickState* st){
    // §1.12.0 crash-hardening: guard against NULL state and a not-yet-created
    // listview/search edit (notifications can arrive before the controls exist).
    if(!st || !st->hList || !IsWindow(st->hList)) return;
    st->reloading = true;            // suppress LVN_ITEMCHANGED re-entrancy
    std::wstring q;
    if(st->hSearch && IsWindow(st->hSearch)){
        wchar_t buf[128]={0}; GetWindowTextW(st->hSearch,buf,127); q=buf;
    }
    std::vector<Section> found;
    Sections_Find(q, found);
    // §7 (1.12.0): the picker is bound to the LIVE Sections registry, so deleted
    // sections never appear. When the search is empty we additionally hide
    // INACTIVE sections (per app rules: inactive = not bookable) so the default
    // list shows only real, active, defined sections. Typing a query still
    // reveals inactive matches so an admin can re-bind a design if needed.
    st->shown.clear();
    bool emptyQuery=q.empty();
    for(const auto& s : found){
        if(emptyQuery && !s.is_active) continue;   // hide inactive by default
        st->shown.push_back(s);
    }
    ListView_DeleteAllItems(st->hList);
    for(size_t i=0;i<st->shown.size();++i){
        const Section& s=st->shown[i];
        LVITEMW it; ZeroMemory(&it,sizeof(it));
        it.mask=LVIF_TEXT|LVIF_PARAM; it.iItem=(int)i; it.iSubItem=0;
        it.lParam=s.id; it.pszText=(LPWSTR)s.code.c_str();
        ListView_InsertItem(st->hList,&it);
        ListView_SetItemText(st->hList,(int)i,1,(LPWSTR)s.name_fa.c_str());
        ListView_SetItemText(st->hList,(int)i,2,(LPWSTR)Sections_KindLabel(s.kind));
        ListView_SetItemText(st->hList,(int)i,3,
            (LPWSTR)(s.is_active?L"\u0641\u0639\u0627\u0644":L"\u063a\u06cc\u0631\u0641\u0639\u0627\u0644"));
        bool chk=std::find(st->selected.begin(),st->selected.end(),s.id)!=st->selected.end();
        ListView_SetCheckState(st->hList,(int)i,chk);
    }
    st->reloading = false;
}
static void picker_syncSel(PickState* st){
    if(!st || !st->hList || !IsWindow(st->hList)) return;
    if(st->reloading) return;        // §1.12.0: ignore notifications fired mid-reload
    st->selected.clear();
    int n=ListView_GetItemCount(st->hList);
    for(int i=0;i<n;i++){
        if(ListView_GetCheckState(st->hList,i)){
            LVITEMW it; ZeroMemory(&it,sizeof(it)); it.mask=LVIF_PARAM; it.iItem=i;
            // §1.14.2 hardening: only trust a section id we actually read back, and
            // reject the LVITEM sentinel value 0 (a row whose lParam never set) so
            // a garbage/zero id can never be carried into the editor open path.
            if(ListView_GetItem(st->hList,&it) && (int)it.lParam>0)
                st->selected.push_back((int)it.lParam);
        }
    }
    if(st->hCount && IsWindow(st->hCount)){
        wchar_t b[64]; swprintf(b,64,
            L"\u0628\u062e\u0634\u200c\u0647\u0627\u06cc \u0627\u0646\u062a\u062e\u0627\u0628 \u0634\u062f\u0647: %d",
            (int)st->selected.size());
        SetWindowTextW(st->hCount,b);
    }
    if(st->hOk && IsWindow(st->hOk)) EnableWindow(st->hOk, !st->selected.empty());
}

static LRESULT CALLBACK PickerProc(HWND h,UINT m,WPARAM w,LPARAM l){
    PickState* st=(PickState*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_CREATE: return 0;
    case WM_COMMAND:
        // §1.12.0: every branch below dereferences `st`; bail if it is not yet
        // attached (notifications can fire during CreateWindow before the
        // SetWindowLongPtr call completes).
        if(!st) return 0;
        if(LOWORD(w)==IDC_DZ_SEARCH && HIWORD(w)==EN_CHANGE){ picker_reload(st); picker_syncSel(st); }
        else if(LOWORD(w)==IDC_DZ_OK){ st->result=true; DestroyWindow(h); }
        else if(LOWORD(w)==IDC_DZ_CANCEL){ st->result=false; DestroyWindow(h); }
        return 0;
    case WM_NOTIFY:{
        if(!st) return 0;
        LPNMHDR nh=(LPNMHDR)l;
        LRESULT headerResult=0;
        if(nh && drawThemedListViewHeader((LPNMCUSTOMDRAW)l,&headerResult))
            return headerResult;
        if(nh->idFrom==IDC_DZ_LIST && nh->code==LVN_ITEMCHANGED){
            NMLISTVIEW* nv=(NMLISTVIEW*)l;
            if(nv->uChanged & LVIF_STATE) picker_syncSel(st);
        }
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc=(HDC)w; SetBkColor(dc,g_theme.surface); SetTextColor(dc,g_theme.text);
        return (LRESULT)g_brSurface;
    }
    case WM_ERASEBKGND:{ RECT rc; GetClientRect(h,&rc); FillRect((HDC)w,&rc,g_brSurface); return 1; }
    case WM_CLOSE: if(st) st->result=false; DestroyWindow(h); return 0;
    // NOTE (§F fix): do NOT PostQuitMessage here. This is a NESTED modal loop;
    // posting WM_QUIT would terminate the outer designer/restore modal loop too
    // (the designer would then never appear). The modal loop below breaks on
    // !IsWindow(h), which fires as soon as this window is destroyed.
    case WM_DESTROY:
        // §1.14.2 hardening: `st` is a stack object in RunSectionPicker. Detach it
        // now so any listview notification that is dispatched AFTER teardown sees a
        // NULL state (every handler bails on !st) instead of a dangling pointer.
        SetWindowLongPtrW(h,GWLP_USERDATA,0);
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

// Returns true and fills out.sectionIds if user pressed «ادامه».
static bool RunSectionPicker(HWND owner, DesignerLaunch& out){
    static bool reg=false;
    const wchar_t* CLS=L"AzSectionPicker";
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=PickerProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=CLS;
        wc.hbrBackground=g_brSurface; RegisterClassW(&wc); reg=true; }
    int W=S(640),H=S(520);
    RECT pr; GetWindowRect(owner,&pr);
    int x=pr.left+((pr.right-pr.left)-W)/2, y=pr.top+((pr.bottom-pr.top)-H)/2;
    PickState st; st.result=false;
    HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME,CLS,
        L"\u0627\u0646\u062a\u062e\u0627\u0628 \u0628\u062e\u0634\u200c\u0647\u0627 \u0628\u0631\u0627\u06cc \u062f\u06cc\u0632\u0627\u06cc\u0646 \u0686\u0627\u067e",
        WS_POPUP|WS_CAPTION|WS_SYSMENU,x,y,W,H,owner,NULL,g_hInst,NULL);
    SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)&st);

    int mgn=S(12), cw=W-2*mgn-S(16);
    HWND cap=CreateWindowExW(0,L"STATIC",
        L"\u062c\u0633\u062a\u062c\u0648 \u0628\u0627 \u0646\u0627\u0645 \u0628\u062e\u0634 \u06cc\u0627 \u06a9\u062f \u0628\u062e\u0634",
        WS_CHILD|WS_VISIBLE|SS_RIGHT,mgn,mgn,cw,S(18),h,NULL,g_hInst,NULL);
    SendMessageW(cap,WM_SETFONT,(WPARAM)g_fSmall,TRUE);
    st.hSearch=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",
        WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,mgn,mgn+S(20),cw,S(26),h,
        (HMENU)IDC_DZ_SEARCH,g_hInst,NULL);
    SendMessageW(st.hSearch,WM_SETFONT,(WPARAM)g_fUI,TRUE);
    st.hList=CreateWindowExW(0,WC_LISTVIEWW,L"",
        WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL,
        mgn,mgn+S(54),cw,H-S(54)-S(96),h,(HMENU)IDC_DZ_LIST,g_hInst,NULL);
    ListView_SetExtendedListViewStyle(st.hList,
        LVS_EX_CHECKBOXES|LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
    SendMessageW(st.hList,WM_SETFONT,(WPARAM)g_fUI,TRUE);
    LVCOLUMNW col; ZeroMemory(&col,sizeof(col)); col.mask=LVCF_TEXT|LVCF_WIDTH;
    struct C{const wchar_t* t;int w;} cols[]={
        {L"\u06a9\u062f",S(70)},
        {L"\u0646\u0627\u0645 \u0628\u062e\u0634",S(240)},
        {L"\u0646\u0648\u0639",S(110)},
        {L"\u0641\u0639\u0627\u0644",S(80)} };
    for(int i=0;i<4;i++){ col.pszText=(LPWSTR)cols[i].t; col.cx=cols[i].w;
        ListView_InsertColumn(st.hList,i,&col); }
    applyThemedListView(st.hList);
    st.hCount=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_RIGHT,
        mgn,H-S(82),cw,S(20),h,NULL,g_hInst,NULL);
    SendMessageW(st.hCount,WM_SETFONT,(WPARAM)g_fSmall,TRUE);
    st.hOk=createFlatButton(h,IDC_DZ_OK,
        L"\u0627\u062f\u0627\u0645\u0647",ICO_CHECK,BS_PRIMARY,
        W-mgn-S(120),H-S(56),S(120),S(34));
    setFlatButtonBg(st.hOk,g_theme.surface);
    EnableWindow(st.hOk,FALSE);
    HWND cancel=createFlatButton(h,IDC_DZ_CANCEL,
        L"\u0627\u0646\u0635\u0631\u0627\u0641",ICO_X,BS_OUTLINE,
        mgn,H-S(56),S(110),S(34));
    setFlatButtonBg(cancel,g_theme.surface);

    Sections_Init();
    picker_reload(&st); picker_syncSel(&st);
    ShowWindow(h,SW_SHOW);
    EnableWindow(owner,FALSE);
    // §F fix: self-contained modal pump. Breaks as soon as `h` is destroyed
    // (checked AFTER dispatch) and never depends on a posted WM_QUIT, so the
    // surrounding designer modal loop is left intact.
    MSG msg;
    while(IsWindow(h) && GetMessageW(&msg,NULL,0,0)){
        if(msg.message==WM_QUIT){ PostQuitMessage((int)msg.wParam); break; }
        if(!IsWindow(h)) break;
        if(IsDialogMessageW(h,&msg)){ if(!IsWindow(h)) break; continue; }
        TranslateMessage(&msg); DispatchMessageW(&msg);
        if(!IsWindow(h)) break;
    }
    EnableWindow(owner,TRUE); SetForegroundWindow(owner);
    out.sectionIds=st.selected;
    return st.result && !st.selected.empty();
}

// =====================================================================
//  DESIGNER WINDOW
// =====================================================================
struct DesignerState {
    PrintDesign design;
    std::vector<int> targetSections;     // from picker / for save
    int  selId;                          // selected item id (-1 none)
    std::vector<int> multiSel;
    double zoom;                         // 0.25 .. 4.0
    double panX, panY;                   // px offset of paper origin in canvas
    bool dirty;
    bool showGrid;
    bool dragging; POINT dragStart; double dragItemX0, dragItemY0;
    bool panning; POINT panStart; double pan0x, pan0y;
    int  toolMode;                       // E2: 0=SELECT (ابزار انتخاب), 1=HAND (ابزار دست)
    UndoStack<PrintDesign> undo;
    HWND hPaperCombo, hOrientCombo, hZoom;
    HWND hToolSelect, hToolHand;         // E2: toolbar tool toggles
    HWND hMain;
    DesignerState():selId(-1),zoom(1.0),panX(40),panY(70),dirty(false),
        showGrid(true),dragging(false),panning(false),toolMode(1),undo(100),
        hToolSelect(NULL),hToolHand(NULL){}
};

static double pxPerMm(DesignerState* st){ return 96.0/25.4 * st->zoom; }

static PrintItem g_designerClip;
static bool g_designerHasClip=false;

static PrintItem* findItem(DesignerState* st,int id){
    for(auto& it:st->design.items) if(it.id==id) return &it;
    return nullptr;
}
static int nextItemId(DesignerState* st){
    int mx=0; for(auto& it:st->design.items) if(it.id>mx) mx=it.id; return mx+1;
}
static void commit(DesignerState* st){ st->dirty=true; st->undo.push(st->design); }

// ---------------------------------------------------------------------------
//  E1 — reflowDesign(): when the user changes «سایز کاغذ» (paper size) or
//  orientation, proportionally re-scale EVERY item from the paper size the
//  layout was authored for (baseW/baseH) to the design's current paperW/paperH,
//  then clamp each item inside the printable area so nothing overflows the new
//  page. This is what keeps a user-saved design (loaded from disk) laid out for
//  one paper size from breaking when it is re-targeted at another.
//
//  The services table («جدول خدمات», PIT_SERVICES) stores its column widths as
//  RELATIVE FRACTIONS in the item's text JSON (e.g. "widths":[0.34,0.10,...]),
//  and both the canvas preview and the print engine derive the column edges from
//  the item's outer width — so the columns re-proportion automatically and never
//  collapse, overlap, or lose their borders. Only the item's outer width is
//  clamped here (to the printable width), and a services table is recognised by
//  either its type or its JSON signature (mirroring drawItemGdip), so old designs
//  whose type id was degraded still reflow correctly.
// ---------------------------------------------------------------------------
static bool pdIsServicesItem(const PrintItem& it){
    if(it.type==PIT_SERVICES) return true;
    if(it.type==PIT_TABLE) return false;
    return it.text.find(L"\"labels\"")!=std::wstring::npos &&
           it.text.find(L"\"widths\"")!=std::wstring::npos;
}
static void reflowDesign(PrintDesign& d){
    if(d.paperW<=0 || d.paperH<=0) return;
    // No baseline recorded yet (e.g. a design that bypassed the open sync) —
    // adopt the current paper as the baseline and leave geometry untouched.
    if(d.baseW<=0 || d.baseH<=0){ d.baseW=d.paperW; d.baseH=d.paperH; return; }
    const double sx = d.paperW / d.baseW;   // horizontal scale (new/old)
    const double sy = d.paperH / d.baseH;   // vertical   scale (new/old)
    // printable inset — the same width-based scale the built-in templates use
    // (paperMargin): 3mm for thermal rolls, 6mm for A5/A6-class, 8mm otherwise.
    const double mg = (d.paperW<=80.0)?3.0:(d.paperW<=148.0)?6.0:8.0;
    const double maxRight  = d.paperW - mg;   // x + w must stay at or below this
    const double maxBottom = d.paperH - mg;   // y + h must stay at or below this
    const double printableW = (d.paperW - 2*mg > 1.0) ? (d.paperW - 2*mg) : 1.0;
    for(auto& it : d.items){
        it.x *= sx; it.w *= sx;
        it.y *= sy; it.h *= sy;
        if(it.w < 0.1) it.w = 0.1;
        if(it.h < 0.1) it.h = 0.1;
        if(it.x < 0) it.x = 0;
        if(it.y < 0) it.y = 0;
        // services table: clamp outer width to the full printable width so its
        // relative column fractions keep their proportions and never collapse.
        if(pdIsServicesItem(it) && it.w > printableW) it.w = printableW;
        // keep the right/bottom edge inside the printable area
        if(it.x + it.w > maxRight){
            if(it.x >= maxRight){ it.x = 0; it.w = maxRight; }
            else it.w = maxRight - it.x;
        }
        if(it.y + it.h > maxBottom){
            if(it.y >= maxBottom){ it.y = 0; it.h = maxBottom; }
            else it.h = maxBottom - it.y;
        }
        if(it.w < 0.1) it.w = 0.1;
        if(it.h < 0.1) it.h = 0.1;
    }
    d.baseW = d.paperW;
    d.baseH = d.paperH;
}

// map paper-mm -> canvas px
static POINT mmToPx(DesignerState* st,double mx,double my){
    double k=pxPerMm(st);
    POINT p; p.x=(LONG)(st->panX + mx*k); p.y=(LONG)(st->panY + my*k); return p;
}
static void pxToMm(DesignerState* st,int px,int py,double& mx,double& my){
    double k=pxPerMm(st);
    mx=(px-st->panX)/k; my=(py-st->panY)/k;
}

// hit-test honoring frame special rule (§3.10)
static int hitTest(DesignerState* st,int px,int py){
    double mx,my; pxToMm(st,px,py,mx,my);
    // descending z, frames last
    std::vector<PrintItem*> order;
    for(auto& it:st->design.items) if(!it.is_frame) order.push_back(&it);
    std::sort(order.begin(),order.end(),
        [](PrintItem*a,PrintItem*b){return a->z>b->z;});
    for(auto* it:order){
        if(mx>=it->x && mx<=it->x+it->w && my>=it->y && my<=it->y+it->h)
            return it->id;
    }
    // frames: hollow stroke only
    double k=pxPerMm(st);
    for(auto& it:st->design.items) if(it.is_frame){
        double sw=(it.borderWidth+2)/k;     // stroke thickness in mm
        bool nearL=fabs(mx-it.x)<=sw, nearR=fabs(mx-(it.x+it.w))<=sw;
        bool nearT=fabs(my-it.y)<=sw, nearB=fabs(my-(it.y+it.h))<=sw;
        bool inside=mx>=it.x-sw && mx<=it.x+it.w+sw && my>=it.y-sw && my<=it.y+it.h+sw;
        if(inside && (nearL||nearR||nearT||nearB)) return it.id;
    }
    return -1;
}

// forward
static void designer_layout(HWND h);
static void designer_buildInspector(HWND h);
extern "C" {}

// §1.14.3 crash-proofing: a GDI+ Font built from an un-installed family name on
// some Windows configurations leaves the FontFamily in an error state, and
// constructing a Font from it (or DrawString-ing with it) can raise a hardware
// fault inside gdiplus.dll — which is exactly what produced the contained
// "open editor" crash.
//
// Wine-specific readability: AddFontMemResourceEx makes the embedded Vazirmatn
// faces available to Win32/GDI, but Wine's GDI+ does not consistently enumerate
// those process fonts. Keep a designer-only PrivateFontCollection backed by the
// same embedded resources. Resource memory remains valid for the module lifetime;
// the deliberately process-lifetime collection is not destroyed after GDI+
// shutdown. This changes neither persisted font names nor printer rendering.
static const Gdiplus::PrivateFontCollection* designerEmbeddedFonts(){
    using namespace Gdiplus;
    static PrivateFontCollection* fonts=[]()->PrivateFontCollection*{
        PrivateFontCollection* c=new PrivateFontCollection();
        if(!c || c->GetLastStatus()!=Ok){ delete c; return nullptr; }
        const int ids[2]={101,102};
        bool added=false;
        for(int id:ids){
            HRSRC hr=FindResourceW(g_hInst,MAKEINTRESOURCEW(id),RT_RCDATA);
            if(!hr) continue;
            HGLOBAL hg=LoadResource(g_hInst,hr);
            DWORD sz=SizeofResource(g_hInst,hr);
            const void* data=LockResource(hg);
            if(data && sz>0 && sz<0x7FFFFFFFu &&
               c->AddMemoryFont(data,(INT)sz)==Ok) added=true;
        }
        if(!added){ delete c; return nullptr; }
        return c;
    }();
    return fonts;
}

// makeSafeFont() guarantees a USABLE family every time by degrading
// deterministically: requested name -> embedded Vazirmatn -> installed
// Vazirmatn -> GenericSansSerif. The returned Font is heap-owned (caller
// deletes). `pxSize` is the final glyph size in PIXELS (UnitPixel).
static Gdiplus::Font* makeSafeFont(const std::wstring& reqName,double pxSize,int gdiStyle){
    using namespace Gdiplus;
    REAL px=(REAL)((pxSize>0?pxSize:10.0));
    if(px<1)   px=1;
    if(px>4000)px=4000;                  // absurd zoom * pt → clamp before GDI+
    auto build=[&](const FontFamily& family)->Font*{
        if(family.GetLastStatus()!=Ok || !family.IsAvailable()) return nullptr;
        Font* f=new Font(&family,px,(FontStyle)gdiStyle,UnitPixel);
        if(f && f->GetLastStatus()==Ok) return f;
        delete f;
        // Some faces do not advertise a synthetic italic/bold combination.
        // Retain bold when possible, otherwise prefer readable regular glyphs.
        int fallbackStyle=(gdiStyle&FontStyleBold)?FontStyleBold:FontStyleRegular;
        f=new Font(&family,px,(FontStyle)fallbackStyle,UnitPixel);
        if(f && f->GetLastStatus()==Ok) return f;
        delete f; return nullptr;
    };
    bool wantsVazir=reqName.empty() || reqName==L"Vazirmatn" || reqName==L"Vazir";
    // 1) honour a genuinely installed custom face.
    if(!wantsVazir){
        FontFamily ff(reqName.c_str());
        if(Font* f=build(ff)) return f;
    }
    // 2) use the exact bundled Persian face, including under Wine.
    const PrivateFontCollection* embedded=designerEmbeddedFonts();
    if(embedded){
        FontFamily ff(L"Vazirmatn",embedded);
        if(Font* f=build(ff)) return f;
    }
    // 3) native Windows normally sees the per-user/process-installed face.
    {
        FontFamily ff(L"Vazirmatn");
        if(Font* f=build(ff)) return f;
    }
    // 4) if the requested Vazir alias resolved independently, still honour it.
    if(wantsVazir && !reqName.empty() && reqName!=L"Vazirmatn"){
        FontFamily ff(reqName.c_str());
        if(Font* f=build(ff)) return f;
    }
    // 5) guarded platform fallbacks.
    const FontFamily* generic=FontFamily::GenericSansSerif();
    if(generic){ if(Font* f=build(*generic)) return f; }
    FontFamily arial(L"Arial");
    return build(arial);
}

// ---- GDI+ rendering of one item into canvas px coords ----------------------
static std::wstring sampleValue(const std::wstring& field){
    // v1.97.0 — designer preview shows tokens, never example patient names.
    std::wstring f=field;
    if(f.size()>=2 && f.front()==L'{' && f.back()==L'}') f=f.substr(1,f.size()-2);
    std::wstring low=f;
    for(wchar_t& c:low) if(c>=L'A'&&c<=L'Z') c=(wchar_t)(c-L'A'+L'a');
    if(low==L"full"||low==L"fullname"||low==L"p-name"||low==L"pname"||low==L"p_name")
        return L"[P-Name]";
    if(f.empty()) return L"[field]";
    return L"["+f+L"]";
}

static void drawItemGdip(Graphics& g,DesignerState* st,const PrintItem& it,bool selected){
    double k=pxPerMm(st);
    REAL x=(REAL)(st->panX+it.x*k), y=(REAL)(st->panY+it.y*k);
    REAL w=(REAL)(it.w*k), hgt=(REAL)(it.h*k);
    GraphicsState gs=g.Save();
    if(it.rot!=0){
        g.TranslateTransform(x+w/2,y+hgt/2);
        g.RotateTransform((REAL)it.rot);
        g.TranslateTransform(-(x+w/2),-(y+hgt/2));
    }
    Color tc(255,(it.textColor>>16)&0xFF,(it.textColor>>8)&0xFF,it.textColor&0xFF);
    Color bc(255,(it.borderColor>>16)&0xFF,(it.borderColor>>8)&0xFF,it.borderColor&0xFF);
    // Some pre-native/web exports persisted a service model while degrading the
    // numeric item type. Recognise the model signature for canvas display only;
    // the item itself, its JSON, and every save/print path remain untouched.
    int canvasType=it.type;
    if(canvasType!=PIT_TABLE && it.text.find(L"\"labels\"")!=std::wstring::npos &&
       it.text.find(L"\"widths\"")!=std::wstring::npos)
        canvasType=PIT_SERVICES;
    // fill
    if(!it.fillTransparent && canvasType!=PIT_HLINE && canvasType!=PIT_VLINE &&
       canvasType!=PIT_SERVICES){
        Color fc(255,(it.fillColor>>16)&0xFF,(it.fillColor>>8)&0xFF,it.fillColor&0xFF);
        SolidBrush fb(fc); g.FillRectangle(&fb,x,y,w,hgt);
    }
    switch(canvasType){
    case PIT_HLINE:{ Pen p(bc,(REAL)std::max(1.0,it.borderWidth*st->zoom));
        g.DrawLine(&p,x,y+hgt/2,x+w,y+hgt/2); break; }
    case PIT_VLINE:{ Pen p(bc,(REAL)std::max(1.0,it.borderWidth*st->zoom));
        g.DrawLine(&p,x+w/2,y,x+w/2,y+hgt); break; }
    case PIT_RECT: case PIT_FRAME:{
        Pen p(bc,(REAL)std::max(1.0,it.borderWidth*st->zoom));
        g.DrawRectangle(&p,x,y,w,hgt); break; }
    case PIT_QR:{
        Pen p(bc,1.0f); g.DrawRectangle(&p,x,y,w,hgt);
        // a crude QR finder-pattern preview
        SolidBrush b(Color(255,0,0,0));
        REAL cell=w/7;
        for(int i=0;i<7;i++)for(int j=0;j<7;j++)
            if(((i+j)%2)==0 || i==0||j==0||i==6||j==6)
                g.FillRectangle(&b,x+i*cell,y+j*cell,cell,cell);
        break; }
    case PIT_SERVICES:{
#ifdef AZ_DEBUG_BUILD
        { static bool logged=false; if(!logged){ logged=true;
          writeFileUtf8(exeDir()+L"\\designer_services_branch.txt",
              L"PIT_SERVICES branch entered\r\n",false); } }
#endif
        // Designer-only structural preview. The JSON remains the persisted model
        // and the printer still fills rows exclusively from ReceptionRecord.
        // Here we render configured labels/widths/row geometry and neutral blank
        // rows, so no receipt data is implied or fabricated on the canvas.
        struct Model {
            int cols=4; bool header=true;
            std::vector<double> widths;
            std::vector<std::wstring> labels;
        } m;
        std::string json=js_esc(L""); // reserve no alternate serialization path
        {
            int n=WideCharToMultiByte(CP_UTF8,0,it.text.c_str(),(int)it.text.size(),NULL,0,NULL,NULL);
            json.assign(n,0);
            if(n) WideCharToMultiByte(CP_UTF8,0,it.text.c_str(),(int)it.text.size(),&json[0],n,NULL,NULL);
        }
        size_t pos=0;
        auto ws=[&]{ while(pos<json.size() && (json[pos]==' '||json[pos]=='\t'||json[pos]=='\r'||json[pos]=='\n')) ++pos; };
        auto str=[&](std::string& out)->bool{
            ws(); if(pos>=json.size()||json[pos]!='"') return false; ++pos;
            while(pos<json.size()&&json[pos]!='"'){
                char c=json[pos++];
                if(c=='\\'&&pos<json.size()){
                    char e=json[pos++];
                    switch(e){
                    case 'n':out+='\n';break; case 'r':out+='\r';break;
                    case 't':out+='\t';break; case '"':out+='"';break;
                    case '\\':out+='\\';break; case '/':out+='/';break;
                    case 'u':{
                        if(pos+4<=json.size()){
                            unsigned v=(unsigned)strtoul(json.substr(pos,4).c_str(),NULL,16); pos+=4;
                            if(v<0x80) out+=(char)v;
                            else if(v<0x800){ out+=(char)(0xC0|(v>>6)); out+=(char)(0x80|(v&0x3F)); }
                            else { out+=(char)(0xE0|(v>>12)); out+=(char)(0x80|((v>>6)&0x3F)); out+=(char)(0x80|(v&0x3F)); }
                        } break; }
                    default:out+=e;break;
                    }
                } else out+=c;
            }
            if(pos<json.size()&&json[pos]=='"') ++pos;
            return true;
        };
        auto number=[&]()->double{
            ws(); size_t begin=pos;
            while(pos<json.size() && (isdigit((unsigned char)json[pos])||json[pos]=='-'||
                  json[pos]=='+'||json[pos]=='.'||json[pos]=='e'||json[pos]=='E')) ++pos;
            return atof(json.substr(begin,pos-begin).c_str());
        };
        ws(); if(pos<json.size()&&json[pos]=='{') ++pos;
        while(pos<json.size()){
            ws(); if(pos<json.size()&&json[pos]=='}'){ ++pos; break; }
            std::string key; if(!str(key)) break;
            ws(); if(pos<json.size()&&json[pos]==':') ++pos;
            if(key=="cols") m.cols=(int)number();
            else if(key=="header"){
                ws();
                if(json.compare(pos,4,"true")==0){m.header=true;pos+=4;}
                else if(json.compare(pos,5,"false")==0){m.header=false;pos+=5;}
                else number();
            } else if(key=="widths"){
                ws(); if(pos<json.size()&&json[pos]=='['){ ++pos;
                    while(pos<json.size()){
                        ws(); if(pos<json.size()&&json[pos]==']'){++pos;break;}
                        m.widths.push_back(number()); ws();
                        if(pos<json.size()&&json[pos]==','){++pos;continue;}
                        if(pos<json.size()&&json[pos]==']'){++pos;break;} break;
                    }
                }
            } else if(key=="labels"){
                ws(); if(pos<json.size()&&json[pos]=='['){ ++pos;
                    while(pos<json.size()){
                        ws(); if(pos<json.size()&&json[pos]==']'){++pos;break;}
                        std::string value;
                        if(str(value)) m.labels.push_back(js_utf8_to_w(value));
                        ws(); if(pos<json.size()&&json[pos]==','){++pos;continue;}
                        if(pos<json.size()&&json[pos]==']'){++pos;break;} break;
                    }
                }
            } else {
                ws();
                if(pos<json.size()&&json[pos]=='"'){ std::string ignored; str(ignored); }
                else if(pos<json.size()&&(json[pos]=='['||json[pos]=='{')){
                    char open=json[pos], close=open=='['?']':'}'; int depth=0;
                    do { if(json[pos]==open) ++depth; else if(json[pos]==close) --depth; ++pos; }
                    while(pos<json.size()&&depth>0);
                } else number();
            }
            ws(); if(pos<json.size()&&json[pos]==',') ++pos;
        }
        if(m.cols<1) m.cols=5;
        if((int)m.widths.size()!=m.cols) m.widths.assign(m.cols,1.0);
        if(m.labels.empty()){
            // v1.97.0: نام خدمت | تعداد | مبلغ کل | شرح خدمت last
            const wchar_t* def[4]={L"نام خدمت",L"تعداد",L"مبلغ کل",L"شرح خدمت"};
            for(int c=0;c<m.cols;++c) m.labels.push_back(c<4?std::wstring(def[c]):L"");
        }
        while((int)m.labels.size()<m.cols) m.labels.push_back(L"");
        double sum=0; for(double width:m.widths) if(width>0) sum+=width;
        if(sum<=0){ m.widths.assign(m.cols,1.0); sum=m.cols; }
        std::vector<REAL> cx(m.cols+1); cx[0]=x+w;
        double acc=0;
        for(int c=0;c<m.cols;++c){
            acc+=m.widths[c]>0?m.widths[c]:0;
            cx[c+1]=(c==m.cols-1)?x:(REAL)(x+w-w*(acc/sum));
        }
        double pxPerMillimetre=pxPerMm(st);
        REAL headH=m.header?(REAL)((it.headerH>0?it.headerH:6.0)*pxPerMillimetre):0;
        REAL rowH=(REAL)((it.rowH>0?it.rowH:6.0)*pxPerMillimetre);
        if(headH<1&&m.header) headH=1;
        if(rowH<1) rowH=1;
        if(headH>hgt) headH=hgt;
        int blankRows=(int)((hgt-headH)/rowH);
        if(blankRows<1 && hgt-headH>0) blankRows=1;
        if(blankRows>4) blankRows=4;       // structural preview, not page-filling data
        REAL tableH=headH+blankRows*rowH;
        if(tableH>hgt) tableH=hgt;
        if(m.header){
            unsigned fill=it.fillTransparent?0xEFEFEF:it.fillColor;
            SolidBrush hb(Color(255,(fill>>16)&0xFF,(fill>>8)&0xFF,fill&0xFF));
            g.FillRectangle(&hb,x,y,w,headH);
            int lum=(int)((((fill>>16)&0xFF)*30+((fill>>8)&0xFF)*59+(fill&0xFF)*11)/100);
            SolidBrush textBrush(lum<140?Color(255,255,255,255):tc);
            int style=FontStyleBold|(it.italic?FontStyleItalic:0);
            double pt=(it.fontPt>0)?it.fontPt:8.5;
            Font* font=makeSafeFont(it.fontName,pt*1.333*(st->zoom>0?st->zoom:1.0),style);
            if(font){
                StringFormat sf; sf.SetAlignment(StringAlignmentCenter);
                sf.SetLineAlignment(StringAlignmentCenter);
                sf.SetFormatFlags(StringFormatFlagsDirectionRightToLeft);
                for(int c=0;c<m.cols;++c){
                    REAL pad=(REAL)std::max(1.0,it.padding*pxPerMillimetre);
                    RectF cell(cx[c+1]+pad,y,cx[c]-cx[c+1]-2*pad,headH);
                    if(cell.Width>0) g.DrawString(m.labels[c].c_str(),-1,font,cell,&sf,&textBrush);
                }
                delete font;
            }
        }
        Pen grid(bc,(REAL)std::max(1.0,it.borderWidth*pxPerMillimetre));
        for(int c=0;c<=m.cols;++c) g.DrawLine(&grid,cx[c],y,cx[c],y+tableH);
        if(m.header) g.DrawLine(&grid,x,y+headH,x+w,y+headH);
        for(int r=0;r<=blankRows;++r){
            REAL yy=y+headH+r*rowH; if(yy>y+tableH) yy=y+tableH;
            g.DrawLine(&grid,x,yy,x+w,yy);
        }
        // v1.97.0 — first data row shows field tokens, never example names.
        if(blankRows>=1){
            int style=it.italic?FontStyleItalic:FontStyleRegular;
            double pt=(it.fontPt>0)?it.fontPt:8.5;
            Font* font=makeSafeFont(it.fontName,pt*1.333*(st->zoom>0?st->zoom:1.0),style);
            if(font){
                SolidBrush textBrush(tc);
                StringFormat sf; sf.SetAlignment(StringAlignmentCenter);
                sf.SetLineAlignment(StringAlignmentCenter);
                sf.SetFormatFlags(StringFormatFlagsDirectionRightToLeft);
                REAL pad=(REAL)std::max(1.0,it.padding*pxPerMillimetre);
                for(int c=0;c<m.cols;++c){
                    std::wstring tok=L"["+m.labels[c]+L"]";
                    RectF cell(cx[c+1]+pad,y+headH,cx[c]-cx[c+1]-2*pad,rowH);
                    if(cell.Width>0) g.DrawString(tok.c_str(),-1,font,cell,&sf,&textBrush);
                }
                delete font;
            }
        }
        break; }
    case PIT_BARCODE:{
        // v1.55.0 — representative 1-D barcode preview. The real symbol is
        // generated at print time from the bound field (live data); here we
        // draw a stable, deterministic bar pattern of the same proportions so
        // the operator can size and position it accurately (WYSIWYG geometry).
        SolidBrush b(tc);
        REAL barsH = (REAL)(hgt*0.78);
        if(barsH<2) barsH=hgt;
        REAL qz = (REAL)(w*0.06);
        REAL avail = w-2*qz; if(avail<4) { qz=0; avail=w; }
        // 67 modules, a fixed pseudo-pattern (NOT random — a constant table)
        static const char PAT[] =
            "1101100101110110010011010011100101101110010110011101001100101101101";
        int n=(int)(sizeof(PAT)/sizeof(PAT[0]))-1;
        int i=0;
        while(i<n){
            if(PAT[i]=='0'){ ++i; continue; }
            int j=i; while(j<n && PAT[j]=='1') ++j;
            REAL bx0 = x+qz + avail*(REAL)i/(REAL)n;
            REAL bx1 = x+qz + avail*(REAL)j/(REAL)n;
            if(bx1-bx0 < 1.0f) bx1 = bx0+1.0f;
            g.FillRectangle(&b,bx0,y,bx1-bx0,barsH);
            i=j;
        }
        // human-readable numeric line placeholder under the bars
        if(hgt-barsH > 3){
            FontFamily ff(it.fontName.empty()?L"Vazirmatn":it.fontName.c_str());
            if(ff.IsAvailable()){
                REAL fpx=(REAL)std::max(6.0,(it.fontPt>0?it.fontPt:8.0)*st->zoom*(96.0/72.0)*0.9);
                Font fnt(&ff,fpx,FontStyleRegular,UnitPixel);
                StringFormat sf; sf.SetAlignment(StringAlignmentCenter);
                sf.SetLineAlignment(StringAlignmentCenter);
                RectF tr(x,y+barsH,w,hgt-barsH);
                SolidBrush tb(tc);
                g.DrawString(L"\u06f1\u06f2\u06f3\u06f4\u06f5\u06f6\u06f7",-1,&fnt,tr,&sf,&tb);
            }
        }
        break; }
    case PIT_LOGO: case PIT_IMAGE: case PIT_PHOTO:{
        // v1.23.0 WYSIWYG: render the ACTUAL uploaded image (file path or
        // data:base64 URI) using the SAME engine + object-fit + padding as the
        // print output, so the preview is pixel-identical to the printed page.
        bool drawn=false;
        if(!it.imgPath.empty()){
            RECT rr={(LONG)x,(LONG)y,(LONG)(x+w),(LONG)(y+hgt)};
            int padPx=(int)(it.padding*pxPerMm(st)); if(padPx<0)padPx=0;
            // Borrow the underlying HDC from the GDI+ Graphics so we reuse the
            // exact print-time renderer. The HDC MUST be released before any
            // further GDI+ drawing on `g`.
            HDC hdc=g.GetHDC();
            if(hdc){
                drawn=gpDrawImageRectFit(hdc,it.imgPath,rr,it.objectFit,padPx);
                g.ReleaseHDC(hdc);
            }
        }
        if(!drawn){
            // no image yet → labelled placeholder box (designer affordance only;
            // never printed because the print engine also falls back per-item).
            Pen p(bc,1.0f); g.DrawRectangle(&p,x,y,w,hgt);
            // §1.14.3: crash-proof font.
            double gpt=(double)(std::min(w,hgt)*0.5); if(gpt<1) gpt=1;
            Font* f=makeSafeFont(L"Vazirmatn",gpt,FontStyleRegular);
            if(f){
                SolidBrush tb(Color(255,150,150,150));
                StringFormat sf; sf.SetAlignment(StringAlignmentCenter);
                sf.SetLineAlignment(StringAlignmentCenter);
                const wchar_t* gl=(it.type==PIT_PHOTO)?L"\u0639":(it.type==PIT_LOGO)?L"\u0644":L"\u062a";
                RectF rf(x,y,w,hgt); g.DrawString(gl,-1,f,rf,&sf,&tb);
                delete f;
            }
        }
        break; }
    case PIT_LABEL: case PIT_FIELD: default:{
        std::wstring text;
        if(it.type==PIT_FIELD) text=it.prefix+sampleValue(it.field)+it.suffix;
        else text=it.text;
        // §1.14.3: crash-proof font construction (deterministic fallback chain).
        int style=FontStyleRegular;
        if(it.bold) style|=FontStyleBold;
        if(it.italic) style|=FontStyleItalic;
        double pt=(it.fontPt>0)?it.fontPt:10;
        double zf=(st->zoom>0?st->zoom:1.0);
        double px2=pt*1.333*zf; if(px2<1) px2=1;
        Font* f=makeSafeFont(it.fontName, px2, style);
        if(f){
            SolidBrush tb(tc);
            StringFormat sf;
            // v1.23.0: per-item direction (0=RTL 1=LTR 2=center-auto) mirrors the
            // print engine, so preview == print for mixed Persian/English.
            bool rtl=(it.dir!=1);
            if(rtl) sf.SetFormatFlags(StringFormatFlagsDirectionRightToLeft);
            // horizontal alignment. In RTL, StringAlignmentNear == visual right.
            int al=it.align; if(it.dir==2) al=1;        // center-auto
            if(al==1) sf.SetAlignment(StringAlignmentCenter);
            else if(al==2) sf.SetAlignment(rtl?StringAlignmentFar:StringAlignmentNear); // visual left
            else           sf.SetAlignment(rtl?StringAlignmentNear:StringAlignmentFar); // visual right
            // vertical alignment (0=top 1=middle 2=bottom).
            sf.SetLineAlignment(it.valign==1?StringAlignmentCenter:
                                it.valign==2?StringAlignmentFar:StringAlignmentNear);
            // inner padding (mm → canvas px) so text never touches the box edge.
            REAL padpx=(REAL)(it.padding*pxPerMm(st)); if(padpx<0)padpx=0;
            REAL rw=w-2*padpx, rh=hgt-2*padpx;
            REAL ox=x+padpx, oy=y+padpx;
            if(rw<=0){ ox=x; rw=w; }
            if(rh<=0){ oy=y; rh=hgt; }
            RectF rf(ox,oy,rw,rh);
            g.DrawString(text.c_str(),-1,f,rf,&sf,&tb);
            delete f;
        }
        break; }
    }
    g.Restore(gs);
    if(selected){
        Pen sp(Color(255,0,120,255),1.5f);
        g.DrawRectangle(&sp,x-1,y-1,w+2,hgt+2);
    }
}

static void designer_paint(HWND h){
    DesignerState* st=(DesignerState*)GetWindowLongPtrW(h,GWLP_USERDATA);
    if(!st) return;
    RECT rc; GetClientRect(h,&rc);
    PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
    uikit::MemDC mem(dc0,rc.right,rc.bottom);
    HBRUSH bg=CreateSolidBrush(g_theme.bg); FillRect(mem.dc,&rc,bg); DeleteObject(bg);

    int topBar=S(44), strip=S(60);
    int canvasTop=topBar+strip;
    int inspW=(int)((rc.right)*0.30);
    RECT canvas={0,canvasTop,rc.right-inspW,rc.bottom};

    // Canvas plus explicit toolbar/inspector surfaces. AzFlatBtn controls use
    // the same live page token for their rounded-corner clearing.
    HBRUSH cb=CreateSolidBrush(blendColor(g_theme.bg,g_theme.surface,40));
    FillRect(mem.dc,&canvas,cb); DeleteObject(cb);
    RECT inspector={rc.right-inspW,canvasTop,rc.right,rc.bottom};
    HBRUSH ib=CreateSolidBrush(g_theme.bg);
    FillRect(mem.dc,&inspector,ib); DeleteObject(ib);

    // GDI+ on a clipped region
    {
        Graphics g(mem.dc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        Region clip(Rect(canvas.left,canvas.top,canvas.right-canvas.left,
                         canvas.bottom-canvas.top));
        g.SetClip(&clip);
        double k=pxPerMm(st);
        REAL px=(REAL)st->panX, py=(REAL)st->panY;
        REAL pw=(REAL)(st->design.paperW*k), ph=(REAL)(st->design.paperH*k);
        // page shadow + white page
        SolidBrush sh(Color(40,0,0,0)); g.FillRectangle(&sh,px+4,py+4,pw,ph);
        SolidBrush wp(Color(255,255,255,255)); g.FillRectangle(&wp,px,py,pw,ph);
        Pen pe(Color(255,180,180,180),1.0f); g.DrawRectangle(&pe,px,py,pw,ph);
        // grid
        if(st->showGrid){
            Pen gp(Color(255,235,235,235),1.0f);
            for(double mx=0;mx<=st->design.paperW;mx+=5){
                REAL X=(REAL)(px+mx*k); g.DrawLine(&gp,X,py,X,py+ph);
            }
            for(double my=0;my<=st->design.paperH;my+=5){
                REAL Y=(REAL)(py+my*k); g.DrawLine(&gp,px,Y,px+pw,Y);
            }
        }
        // items in ascending z
        std::vector<const PrintItem*> order;
        for(auto& it:st->design.items) order.push_back(&it);
        std::sort(order.begin(),order.end(),
            [](const PrintItem*a,const PrintItem*b){return a->z<b->z;});
        for(auto* it:order){
            bool sel=(it->id==st->selId) ||
                std::find(st->multiSel.begin(),st->multiSel.end(),it->id)!=st->multiSel.end();
            drawItemGdip(g,st,*it,sel);
        }
    }
    mem.blitTo(dc0);
    EndPaint(h,&ps);
}

// ============================================================================
//  Inspector — v1.18.3 FULL REWRITE.
//  ROOT-CAUSE FIX of the deterministic "open editor" ACCESS_VIOLATION
//  (DarmanPlus.exe+0x12789, every run at the same address): the inspector used
//  the custom owner-drawn uikit controls (AzNumberSpinner / AzColorPicker /
//  AzSwitch). Their per-instance heap state + the global handle table + the
//  WM_COMMAND routing back into them is what faulted on the Management path.
//  Per the brief we DO NOT keep patching them — the entire inspector now uses
//  ONLY standard Win32 controls (EDIT / BUTTON / STATIC / CHECKBOX). Numbers
//  are plain right-aligned EDIT boxes; colours are a plain BUTTON that opens
//  ChooseColorW; the lock is a standard CHECKBOX. There is no custom control,
//  no shared global handle table to corrupt, and no owner-draw state to free.
//
//  Color state is stored in the SELECTED PrintItem (textColor/borderColor), so
//  the colour BUTTONs don't need any side state at all.
// ============================================================================
// Build the inspector child controls for the current selection.
static HWND g_inspectorHwnds[64]; static int g_inspectorCount=0;
static void clearInspector(){
    for(int i=0;i<g_inspectorCount;i++) if(g_inspectorHwnds[i]&&IsWindow(g_inspectorHwnds[i]))
        DestroyWindow(g_inspectorHwnds[i]);
    g_inspectorCount=0;
}

// helper: read a numeric EDIT box (tolerant of Persian/Arabic digits + blanks)
static double readNumEdit(HWND h,int id){
    HWND e=GetDlgItem(h,id); if(!e||!IsWindow(e)) return 0;
    wchar_t b[64]={0}; GetWindowTextW(e,b,63);
    // normalize Persian/Arabic digits to ASCII so _wtof works
    for(wchar_t* p=b;*p;++p){
        if(*p>=0x06F0 && *p<=0x06F9) *p=L'0'+(*p-0x06F0);     // Persian
        else if(*p>=0x0660 && *p<=0x0669) *p=L'0'+(*p-0x0660);// Arabic
    }
    return _wtof(b);
}

static void designer_buildInspector(HWND h){
    DesignerState* st=(DesignerState*)GetWindowLongPtrW(h,GWLP_USERDATA);
    if(!st) return;
    clearInspector();
    RECT rc; GetClientRect(h,&rc);
    if(rc.right<=0) return;
    int inspW=(int)(rc.right*0.30);
    int x0=rc.right-inspW+S(8), y=S(44)+S(60)+S(10), w=inspW-S(16);
    if(w<S(60)) w=S(60);
    HFONT fUI = g_fUI ? g_fUI : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT fSm = g_fSmall ? g_fSmall : fUI;
    HFONT fB  = g_fUIB ? g_fUIB : fUI;
    auto add=[&](HWND c){ if(c && g_inspectorCount<64) g_inspectorHwnds[g_inspectorCount++]=c; };
    auto label=[&](const wchar_t* t,int ww=0){
        HWND c=CreateWindowExW(0,L"STATIC",t,WS_CHILD|WS_VISIBLE|SS_RIGHT,
            x0,y,ww?ww:w,S(18),h,NULL,g_hInst,NULL);
        if(c) SendMessageW(c,WM_SETFONT,(WPARAM)fSm,TRUE); add(c); };
    // a standard right-aligned numeric EDIT box
    auto numEdit=[&](int id,double val,int ex,int ey,int ew){
        wchar_t b[48];
        if(val==(long long)val) swprintf(b,48,L"%lld",(long long)val);
        else swprintf(b,48,L"%.2f",val);
        HWND e=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",b,
            WS_CHILD|WS_VISIBLE|ES_RIGHT|ES_AUTOHSCROLL,ex,ey,ew,S(24),h,
            (HMENU)(INT_PTR)id,g_hInst,NULL);
        if(e) SendMessageW(e,WM_SETFONT,(WPARAM)fUI,TRUE); add(e);
    };
    // a standard color BUTTON (owner-draw via BS_OWNERDRAW; current color read
    // from the item at paint time — see WM_DRAWITEM in DesignerProc)
    auto colorBtn=[&](int id,int ex,int ey,int ew){
        HWND b=CreateWindowExW(0,L"BUTTON",L"",
            WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,ex,ey,ew,S(24),h,
            (HMENU)(INT_PTR)id,g_hInst,NULL);
        if(b) SendMessageW(b,WM_SETFONT,(WPARAM)fUI,TRUE); add(b);
    };

    PrintItem* it = (st->selId>=0)? findItem(st,st->selId):nullptr;
    if(!it){
        // palette header (when nothing selected)
        HWND c=CreateWindowExW(0,L"STATIC",
            L"\u0622\u06cc\u062a\u0645\u200c\u0647\u0627 \u2014 \u0628\u0631\u0627\u06cc \u0627\u0641\u0632\u0648\u062f\u0646 \u06a9\u0644\u06cc\u06a9 \u06a9\u0646\u06cc\u062f",
            WS_CHILD|WS_VISIBLE|SS_RIGHT,x0,y,w,S(20),h,NULL,g_hInst,NULL);
        if(c) SendMessageW(c,WM_SETFONT,(WPARAM)fB,TRUE); add(c); y+=S(28);
        struct PI{const wchar_t* t;int type;};
        static const PI pal[]={
            {L"\u0646\u0627\u0645 (\u0641\u06cc\u0644\u062f)",PIT_FIELD},
            {L"\u0644\u06cc\u0628\u0644 \u0645\u062a\u0646",PIT_LABEL},
            {L"\u062c\u062f\u0648\u0644 \u062e\u062f\u0645\u0627\u062a",PIT_SERVICES},
            {L"\u062c\u062f\u0648\u0644 \u0639\u0645\u0648\u0645\u06cc",PIT_TABLE},
            {L"\u062e\u0637 \u0627\u0641\u0642\u06cc",PIT_HLINE},
            {L"\u062e\u0637 \u0639\u0645\u0648\u062f\u06cc",PIT_VLINE},
            {L"\u0645\u0633\u062a\u0637\u06cc\u0644",PIT_RECT},
            {L"\u06a9\u0627\u062f\u0631 \u062f\u0648\u0631 \u0635\u0641\u062d\u0647",PIT_FRAME},
            {L"\u062a\u0635\u0648\u06cc\u0631 \u0633\u0641\u0627\u0631\u0634\u06cc",PIT_IMAGE},
            {L"\u0644\u0648\u06af\u0648 \u062f\u0631\u0645\u0627\u0646\u06af\u0627\u0647",PIT_LOGO},
            {L"QR / \u0628\u0627\u0631\u06a9\u062f",PIT_QR},
            // v1.55.0 — بارکد خطی واقعی (Code128 / Code39 / EAN-13)
            {L"\u0628\u0627\u0631\u06a9\u062f \u062e\u0637\u06cc (\u0642\u0627\u0628\u0644 \u0627\u0633\u06a9\u0646)",PIT_BARCODE},
            {L"\u0639\u06a9\u0633 \u067e\u0631\u0633\u0646\u0644\u06cc \u0628\u06cc\u0645\u0627\u0631",PIT_PHOTO},
        };
        for(const auto& p:pal){
            // Theme-aware native action: unlike a system BUTTON this remains
            // legible in dark mode and clears its rounded corners to the live
            // inspector surface.
            HWND b=createFlatButton(h,5000+p.type,p.t,ICO_PLUS,BS_OUTLINE,
                x0,y,w,S(26));
            setFlatButtonBg(b,g_theme.bg);
            add(b);
            y+=S(30);
        }
        return;
    }
    // single-selection properties
    label(L"\u0645\u0648\u0642\u0639\u06cc\u062a (X, Y mm)"); y+=S(20);
    numEdit(6001,it->x,x0+w/2+S(2),y,w/2-S(2));
    numEdit(6002,it->y,x0,y,w/2-S(2)); y+=S(28);
    label(L"\u0627\u0628\u0639\u0627\u062f (\u0639\u0631\u0636\u060c \u0627\u0631\u062a\u0641\u0627\u0639 mm)"); y+=S(20);
    numEdit(6003,it->w,x0+w/2+S(2),y,w/2-S(2));
    numEdit(6004,it->h,x0,y,w/2-S(2)); y+=S(28);
    label(L"\u0686\u0631\u062e\u0634 (\u062f\u0631\u062c\u0647)"); y+=S(20);
    numEdit(6005,it->rot,x0,y,w); y+=S(28);
    if(it->type==PIT_LABEL || it->type==PIT_FIELD){
        label(L"\u0627\u0646\u062f\u0627\u0632\u0647 \u0641\u0648\u0646\u062a (pt)"); y+=S(20);
        numEdit(6006,it->fontPt,x0,y,w); y+=S(28);
        label(L"\u0631\u0646\u06af \u0645\u062a\u0646"); y+=S(20);
        colorBtn(6007,x0,y,w); y+=S(28);
        if(it->type==PIT_LABEL){
            label(L"\u0645\u062a\u0646"); y+=S(20);
            HWND e=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",it->text.c_str(),
                WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,x0,y,w,S(24),h,
                (HMENU)6008,g_hInst,NULL);
            if(e) SendMessageW(e,WM_SETFONT,(WPARAM)fUI,TRUE); add(e); y+=S(28);
        }
        if(it->type==PIT_FIELD){
            label(L"\u067e\u06cc\u0634\u0648\u0646\u062f"); y+=S(20);
            HWND e=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",it->prefix.c_str(),
                WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,x0,y,w,S(24),h,
                (HMENU)6013,g_hInst,NULL);
            if(e) SendMessageW(e,WM_SETFONT,(WPARAM)fUI,TRUE); add(e); y+=S(28);
        }
        // v1.23.0: a 3-segment selector (standard push-buttons; the active one is
        // drawn pressed via BS_PUSHLIKE checkbox so we keep ONLY std controls).
        auto seg3=[&](const wchar_t* cap,int baseId,int cur,
                      const wchar_t* a,const wchar_t* b,const wchar_t* c){
            label(cap); y+=S(20);
            int seg=(w-2*S(2))/3;
            const wchar_t* t[3]={a,b,c};
            for(int i=0;i<3;i++){
                HWND bt=CreateWindowExW(0,L"BUTTON",t[i],
                    WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|BS_PUSHLIKE,
                    x0+i*(seg+S(2)),y,seg,S(26),h,
                    (HMENU)(INT_PTR)(baseId+i),g_hInst,NULL);
                if(bt){ SendMessageW(bt,WM_SETFONT,(WPARAM)fSm,TRUE);
                    SendMessageW(bt,BM_SETCHECK,(i==cur)?BST_CHECKED:BST_UNCHECKED,0); }
                add(bt);
            }
            y+=S(30);
        };
        // horizontal alignment: 0=right 1=center 2=left
        seg3(L"\u0686\u06cc\u062f\u0645\u0627\u0646 \u0627\u0641\u0642\u06cc",6020,it->align,
             L"\u0631\u0627\u0633\u062a",L"\u0648\u0633\u0637",L"\u0686\u067e");
        // vertical alignment: 0=top 1=middle 2=bottom
        seg3(L"\u0686\u06cc\u062f\u0645\u0627\u0646 \u0639\u0645\u0648\u062f\u06cc",6026,it->valign,
             L"\u0628\u0627\u0644\u0627",L"\u0648\u0633\u0637",L"\u067e\u0627\u06cc\u06cc\u0646");
        // direction: 0=RTL 1=LTR 2=center-auto
        seg3(L"\u062c\u0647\u062a \u0645\u062a\u0646",6023,it->dir,
             L"\u0641\u0627\u0631\u0633\u06cc",L"\u0644\u0627\u062a\u06cc\u0646",L"\u062e\u0648\u062f\u06a9\u0627\u0631");
    }
    // v1.23.0: image items expose object-fit + padding so the logo/photo fits its
    // rectangle exactly the way the print engine renders it.
    if(it->type==PIT_LOGO || it->type==PIT_IMAGE || it->type==PIT_PHOTO){
        auto seg3i=[&](const wchar_t* cap,int baseId,int cur,
                       const wchar_t* a,const wchar_t* b,const wchar_t* c){
            label(cap); y+=S(20);
            int seg=(w-2*S(2))/3;
            const wchar_t* t[3]={a,b,c};
            for(int i=0;i<3;i++){
                HWND bt=CreateWindowExW(0,L"BUTTON",t[i],
                    WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|BS_PUSHLIKE,
                    x0+i*(seg+S(2)),y,seg,S(26),h,
                    (HMENU)(INT_PTR)(baseId+i),g_hInst,NULL);
                if(bt){ SendMessageW(bt,WM_SETFONT,(WPARAM)fSm,TRUE);
                    SendMessageW(bt,BM_SETCHECK,(i==cur)?BST_CHECKED:BST_UNCHECKED,0); }
                add(bt);
            }
            y+=S(30);
        };
        // object-fit: 0=contain 1=cover 2=fill
        seg3i(L"\u062a\u0646\u0627\u0633\u0628 \u062a\u0635\u0648\u06cc\u0631",6029,it->objectFit,
              L"\u062f\u0627\u062e\u0644",L"\u067e\u0648\u0634\u0634",L"\u06a9\u0634\u06cc\u062f\u0647");
        label(L"\u062d\u0627\u0634\u06cc\u0647 \u062f\u0627\u062e\u0644\u06cc (mm)"); y+=S(20);
        numEdit(6032,it->padding,x0,y,w); y+=S(28);
    }
    label(L"\u0631\u0646\u06af \u06a9\u0627\u062f\u0631"); y+=S(20);
    colorBtn(6009,x0,y,w); y+=S(28);
    {   // lock = standard checkbox
        HWND cb=CreateWindowExW(0,L"BUTTON",L"\u0642\u0641\u0644 \u0645\u0648\u0642\u0639\u06cc\u062a",
            WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|BS_RIGHT|BS_RIGHTBUTTON,
            x0,y,w,S(24),h,(HMENU)6010,g_hInst,NULL);
        if(cb){ SendMessageW(cb,WM_SETFONT,(WPARAM)fSm,TRUE);
            SendMessageW(cb,BM_SETCHECK,it->locked?BST_CHECKED:BST_UNCHECKED,0); }
        add(cb); y+=S(30);
    }
    {   // destructive and duplicate actions use the shared themed control
        HWND bd=createFlatButton(h,6011,L"\u062d\u0630\u0641 \u0622\u06cc\u062a\u0645",ICO_TRASH,BS_DANGER,
            x0,y,w/2-S(2),S(28));
        setFlatButtonBg(bd,g_theme.bg); add(bd);
        HWND bc=createFlatButton(h,6012,L"\u06a9\u067e\u06cc \u0622\u06cc\u062a\u0645",ICO_PLUS,BS_OUTLINE,
            x0+w/2+S(2),y,w/2-S(2),S(28));
        setFlatButtonBg(bc,g_theme.bg); add(bc);
        y+=S(32);
    }
}

static void designer_layout(HWND h){
    DesignerState* st=(DesignerState*)GetWindowLongPtrW(h,GWLP_USERDATA);
    if(!st) return;
    RECT rc; GetClientRect(h,&rc);
    int mgn=S(8), bh=S(30), by=S(7);
    int x=mgn;
    auto place=[&](HWND c,int wdt){ if(c){ MoveWindow(c,x,by,wdt,bh,TRUE); x+=wdt+S(6);} };
    place(GetDlgItem(h,IDC_DSG_SAVE),S(90));
    place(GetDlgItem(h,IDC_DSG_SAVEAS),S(140));
    place(GetDlgItem(h,IDC_DSG_DOWNLOAD),S(120));
    place(GetDlgItem(h,IDC_DSG_EXIT),S(80));
    if(st->hPaperCombo) MoveWindow(st->hPaperCombo,rc.right/2-S(110),by,S(110),S(200),TRUE);
    if(st->hOrientCombo) MoveWindow(st->hOrientCombo,rc.right/2+S(4),by,S(100),S(200),TRUE);
    if(st->hZoom) MoveWindow(st->hZoom,rc.right-S(180),by,S(170),bh,TRUE);
    // E2: «ابزار انتخاب» / «ابزار دست» toggles sit just left of the zoom slider.
    if(st->hToolHand && st->hToolSelect){
        int tw=S(116), gap=S(6);
        int hx=rc.right-S(180)-S(8)-tw;        // hand (right-most of the pair)
        int tx=hx-gap-tw;                      // select (left of hand)
        MoveWindow(st->hToolHand,hx,by,tw,bh,TRUE);
        MoveWindow(st->hToolSelect,tx,by,tw,bh,TRUE);
    }
    designer_buildInspector(h);
}

// add a palette item at canvas center
static void addPaletteItem(DesignerState* st,int type){
    PrintItem it;
    it.id=nextItemId(st); it.type=type;
    it.x=st->design.paperW/2-15; it.y=st->design.paperH/2-5; it.w=30; it.h=8;
    it.z = (int)st->design.items.size();
    switch(type){
    case PIT_FRAME: it.is_frame=true; it.x=8;it.y=8;
        it.w=st->design.paperW-16; it.h=st->design.paperH-16;
        it.borderWidth=1; it.borderColor=0x1F6FEB; break;
    case PIT_HLINE: it.w=40; it.h=0.2; it.borderWidth=0.3; break;
    case PIT_VLINE: it.w=0.2; it.h=40; it.borderWidth=0.3; break;
    case PIT_RECT:  it.w=40; it.h=20; it.borderWidth=0.3; break;
    case PIT_QR:    it.w=20; it.h=20; it.field=L"receiptNo"; break;
    case PIT_BARCODE:
        // v1.55.0 — a real, scannable Code128-B symbol bound to the receipt's
        // deterministic barcode payload, with the numeric code printed under
        // the bars (exactly like the insurance page of the paper receipt).
        it.w=52; it.h=16;
        it.field=L"{receiptbarcode}";
        it.text=L"{\"sym\":\"code128\",\"hri\":true,\"quiet\":2}";
        it.fontPt=8; it.align=1; it.dir=1;
        it.textColor=0x000000; it.fillTransparent=true; it.borderWidth=0;
        break;
    case PIT_LOGO:  it.w=24; it.h=24; break;
    case PIT_IMAGE: it.w=30; it.h=20; break;
    case PIT_PHOTO: it.w=22; it.h=28; it.borderWidth=0.3; break;
    case PIT_LABEL: it.text=L"\u0645\u062a\u0646 \u062f\u0644\u062e\u0648\u0627\u0647"; break;
    case PIT_FIELD: it.field=L"firstName"; it.prefix=L"\u0646\u0627\u0645: "; break;
    case PIT_SERVICES: {
        // §1.53.0: dynamic services list, sensible default size + model. Centered
        // horizontally, ~42mm tall (room for header + ~6 service rows). Carries
        // v1.97.0 the canonical four-column RTL header
        // — نام خدمت | تعداد | مبلغ کل | شرح خدمت last — monochrome line-art.
        it.x=st->design.paperW/2-60; it.y=st->design.paperH/2-20; it.w=120; it.h=42;
        it.text=L"{\"cols\":4,\"header\":true,\"widths\":[0.34,0.10,0.22,0.34],"
                L"\"labels\":[\"\u0646\u0627\u0645 \u062e\u062f\u0645\u062a\","
                L"\"\u062a\u0639\u062f\u0627\u062f\","
                L"\"\u0645\u0628\u0644\u063a \u06a9\u0644\","
                L"\"\u0634\u0631\u062d \u062e\u062f\u0645\u062a\"]}";
        it.fontPt=8.5; it.bold=false; it.align=0; it.dir=0;
        it.borderColor=0x000000; it.borderWidth=0.3;
        it.textColor=0x000000; it.fillTransparent=true; it.padding=0.8;
        it.headerH=7; it.rowH=6;
        break; }
    case PIT_TABLE: {
        // a small generic 3×3 editable grid (header + 2 data rows)
        it.x=st->design.paperW/2-40; it.y=st->design.paperH/2-15; it.w=80; it.h=24;
        it.text=L"{\"cols\":3,\"rows\":3,\"header\":true,\"widths\":[1,1,1],"
                L"\"cells\":[[\"\u0639\u0646\u0648\u0627\u0646 \u06f1\",\"\u0639\u0646\u0648\u0627\u0646 \u06f2\",\"\u0639\u0646\u0648\u0627\u0646 \u06f3\"],"
                L"[\"\u0633\u062a\u0648\u0646 \u06f1\",\"\u0633\u062a\u0648\u0646 \u06f2\",\"\u0633\u062a\u0648\u0646 \u06f3\"],"
                L"[\"\u0633\u0637\u0631 \u06f2\",\"\u0633\u0637\u0631 \u06f2\",\"\u0633\u0637\u0631 \u06f2\"]]}";
        it.fontPt=9; it.align=1;
        it.borderColor=0x000000; it.borderWidth=0.3; it.padding=0.8;
        break; }
    }
    st->design.items.push_back(it);
    st->selId=it.id; st->multiSel.clear();
    commit(st);
}

static void designer_save(HWND h,bool asNew);

static LRESULT CALLBACK DesignerProc(HWND h,UINT m,WPARAM w,LPARAM l){
    DesignerState* st=(DesignerState*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_CREATE:
        // E2: the default tool is the hand tool — show the hand cursor from the
        // very first frame (state is attached right after CreateWindowExW, so
        // set it here too; WM_SETCURSOR keeps it in sync afterwards).
        SetCursor(LoadCursor(NULL,IDC_HAND));
        return 0;
    case WM_SETCURSOR:{
        // E2: cursor tracks the active tool. Only override over the canvas
        // (HTCLIENT of this window); child controls keep their own cursors.
        if(st && LOWORD(l)==HTCLIENT){
            SetCursor(LoadCursor(NULL, st->toolMode==1 ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }
        return DefWindowProcW(h,m,w,l);
    }
    case WM_SIZE: if(st) designer_layout(h); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: designer_paint(h); return 0;
    case WM_LBUTTONDOWN:{
        if(!st) break;
        int mx=GET_X_LPARAM(l),my=GET_Y_LPARAM(l);
        RECT rc; GetClientRect(h,&rc);
        int inspW=(int)(rc.right*0.30);
        if(mx>rc.right-inspW) break;          // inspector area
        if(my<S(44)+S(60)) break;             // top bar/strip
        // E2: «ابزار دست» (hand tool) always pans the canvas, regardless of
        // whether an item is under the cursor. «ابزار انتخاب» (select tool)
        // keeps the existing hit-test + select + drag behaviour.
        if(st->toolMode==1){
            st->panning=true; st->panStart.x=mx; st->panStart.y=my;
            st->pan0x=st->panX; st->pan0y=st->panY;
            SetCapture(h);
            InvalidateRect(h,NULL,FALSE);
            return 0;
        }
        int id=hitTest(st,mx,my);
        bool ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0;
        if(id<0){
            st->selId=-1; st->multiSel.clear();
            st->panning=true; st->panStart.x=mx; st->panStart.y=my;
            st->pan0x=st->panX; st->pan0y=st->panY;
            SetCapture(h);
        } else {
            if(ctrl){
                st->multiSel.push_back(id);
            } else {
                st->selId=id; st->multiSel.clear();
                PrintItem* it=findItem(st,id);
                if(it && !it->locked){
                    st->dragging=true; st->dragStart.x=mx; st->dragStart.y=my;
                    st->dragItemX0=it->x; st->dragItemY0=it->y;
                    SetCapture(h);
                }
            }
            designer_buildInspector(h);
        }
        InvalidateRect(h,NULL,FALSE);
        return 0;
    }
    case WM_MOUSEMOVE:{
        if(!st) break;
        int mx=GET_X_LPARAM(l),my=GET_Y_LPARAM(l);
        if(st->dragging){
            PrintItem* it=findItem(st,st->selId);
            if(it){ double k=pxPerMm(st);
                it->x=st->dragItemX0+(mx-st->dragStart.x)/k;
                it->y=st->dragItemY0+(my-st->dragStart.y)/k;
                st->dirty=true; InvalidateRect(h,NULL,FALSE); }
        } else if(st->panning){
            st->panX=st->pan0x+(mx-st->panStart.x);
            st->panY=st->pan0y+(my-st->panStart.y);
            InvalidateRect(h,NULL,FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP:{
        if(!st) break;
        if(st->dragging){ st->dragging=false; commit(st); designer_buildInspector(h); }
        if(st->panning) st->panning=false;
        ReleaseCapture();
        return 0;
    }
    case WM_MBUTTONDOWN:{
        if(!st) break;
        st->panning=true; st->panStart.x=GET_X_LPARAM(l); st->panStart.y=GET_Y_LPARAM(l);
        st->pan0x=st->panX; st->pan0y=st->panY; SetCapture(h); return 0;
    }
    case WM_MBUTTONUP: if(st){ st->panning=false; ReleaseCapture(); } return 0;
    case WM_MOUSEWHEEL:{
        if(!st) break;
        int delta=GET_WHEEL_DELTA_WPARAM(w);
        bool ctrl=(LOWORD(w)&MK_CONTROL)!=0;
        bool shift=(LOWORD(w)&MK_SHIFT)!=0;
        if(ctrl){
            POINT pt={GET_X_LPARAM(l),GET_Y_LPARAM(l)}; ScreenToClient(h,&pt);
            double mxBefore,myBefore; pxToMm(st,pt.x,pt.y,mxBefore,myBefore);
            double f=(delta>0)?1.1:1/1.1;
            st->zoom*=f; if(st->zoom<0.25)st->zoom=0.25; if(st->zoom>4.0)st->zoom=4.0;
            // keep cursor anchored
            double k=pxPerMm(st);
            st->panX=pt.x-mxBefore*k; st->panY=pt.y-myBefore*k;
            if(st->hZoom) SendMessageW(st->hZoom,TBM_SETPOS,TRUE,(LPARAM)(int)(st->zoom*100));
        } else if(shift){
            st->panX += (delta>0)? S(40):-S(40);
        } else {
            st->panY += (delta>0)? S(40):-S(40);
        }
        InvalidateRect(h,NULL,FALSE);
        return 0;
    }
    case WM_HSCROLL:{
        if(st && (HWND)l==st->hZoom){
            int pos=(int)SendMessageW(st->hZoom,TBM_GETPOS,0,0);
            st->zoom=pos/100.0; if(st->zoom<0.25)st->zoom=0.25;
            InvalidateRect(h,NULL,FALSE);
        }
        return 0;
    }
    case WM_KEYDOWN:{
        if(!st) break;
        bool ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0;
        PrintItem* it=findItem(st,st->selId);
        switch(w){
        case 'S': if(ctrl){ designer_save(h,(GetKeyState(VK_SHIFT)&0x8000)!=0); } return 0;
        case 'Z': if(ctrl){ PrintDesign p; if(st->undo.undo(p)){ st->design=p; designer_buildInspector(h); InvalidateRect(h,NULL,FALSE);} } return 0;
        case 'Y': if(ctrl){ PrintDesign p; if(st->undo.redo(p)){ st->design=p; designer_buildInspector(h); InvalidateRect(h,NULL,FALSE);} } return 0;
        case 'C': if(ctrl && it){ g_designerClip=*it; g_designerHasClip=true; } return 0;
        case 'V':
        case 'P':
            if(ctrl && g_designerHasClip){
                PrintItem n=g_designerClip; n.id=nextItemId(st); n.x+=6; n.y+=6;
                st->design.items.push_back(n); st->selId=n.id; commit(st);
                designer_buildInspector(h); InvalidateRect(h,NULL,FALSE);
            } return 0;
        case 'D': if(ctrl && it){ PrintItem n=*it; n.id=nextItemId(st); n.x+=10; n.y+=10;
                    st->design.items.push_back(n); st->selId=n.id; commit(st);
                    designer_buildInspector(h); InvalidateRect(h,NULL,FALSE);} return 0;
        case VK_DELETE: case VK_BACK:
            if(it){ st->design.items.erase(std::remove_if(st->design.items.begin(),
                st->design.items.end(),[&](const PrintItem&x){return x.id==st->selId;}),
                st->design.items.end()); st->selId=-1; commit(st);
                designer_buildInspector(h); InvalidateRect(h,NULL,FALSE);} return 0;
        case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
            if(it && !it->locked){ double d=(GetKeyState(VK_SHIFT)&0x8000)?5:1;
                if(w==VK_LEFT)it->x-=d; if(w==VK_RIGHT)it->x+=d;
                if(w==VK_UP)it->y-=d; if(w==VK_DOWN)it->y+=d;
                st->dirty=true; InvalidateRect(h,NULL,FALSE);} return 0;
        case VK_ESCAPE: st->selId=-1; st->multiSel.clear();
            designer_buildInspector(h); InvalidateRect(h,NULL,FALSE); return 0;
        }
        return 0;
    }
    case WM_COMMAND:{
        if(!st) break;
        int id=LOWORD(w);
        int notify=HIWORD(w);
        if(id>=5000 && id<5100){ addPaletteItem(st,id-5000);
            designer_buildInspector(h); InvalidateRect(h,NULL,FALSE); return 0; }
        if(id>=6001 && id<6100){
            // v1.18.3: inspector now uses ONLY standard controls.
            //   6001-6006 = numeric EDIT boxes (commit on EN_KILLFOCUS so we
            //               don't fight the user mid-typing)
            //   6007/6009 = owner-draw color BUTTON (BN_CLICKED -> ChooseColorW)
            //   6008/6013 = text/prefix EDIT (EN_CHANGE)
            //   6010      = lock CHECKBOX (BN_CLICKED)
            //   6011/6012 = delete / duplicate BUTTON (BN_CLICKED)
            PrintItem* it=findItem(st,st->selId);
            if(!it) return 0;
            // numeric edits: commit on focus-loss OR Enter
            if(id>=6001 && id<=6006){
                if(notify==EN_KILLFOCUS){
                    double v=readNumEdit(h,id);
                    if(id==6001) it->x=v;
                    else if(id==6002) it->y=v;
                    else if(id==6003){ it->w=(v<0.1?0.1:v); }
                    else if(id==6004){ it->h=(v<0.1?0.1:v); }
                    else if(id==6005){ if(v<-180)v=-180; if(v>180)v=180; it->rot=v; }
                    else if(id==6006){ if(v<5)v=5; if(v>72)v=72; it->fontPt=v; }
                    st->dirty=true; commit(st); InvalidateRect(h,NULL,FALSE);
                }
                return 0;
            }
            if(id==6007 && notify==BN_CLICKED){
                static COLORREF custom[16]; CHOOSECOLORW cc; ZeroMemory(&cc,sizeof(cc));
                cc.lStructSize=sizeof(cc); cc.hwndOwner=h;
                cc.rgbResult=RGB((it->textColor>>16)&0xFF,(it->textColor>>8)&0xFF,it->textColor&0xFF);
                cc.lpCustColors=custom; cc.Flags=CC_FULLOPEN|CC_RGBINIT|CC_ANYCOLOR;
                if(ChooseColorW(&cc)){ COLORREF c=cc.rgbResult;
                    it->textColor=(GetRValue(c)<<16)|(GetGValue(c)<<8)|GetBValue(c);
                    st->dirty=true; commit(st);
                    HWND b=GetDlgItem(h,6007); if(b) InvalidateRect(b,NULL,TRUE);
                    InvalidateRect(h,NULL,FALSE); }
                return 0;
            }
            if(id==6008 && notify==EN_CHANGE){ wchar_t b[256]={0};
                GetWindowTextW((HWND)l,b,255); it->text=b; st->dirty=true;
                InvalidateRect(h,NULL,FALSE); return 0; }
            if(id==6013 && notify==EN_CHANGE){ wchar_t b[256]={0};
                GetWindowTextW((HWND)l,b,255); it->prefix=b; st->dirty=true;
                InvalidateRect(h,NULL,FALSE); return 0; }
            if(id==6009 && notify==BN_CLICKED){
                static COLORREF custom2[16]; CHOOSECOLORW cc; ZeroMemory(&cc,sizeof(cc));
                cc.lStructSize=sizeof(cc); cc.hwndOwner=h;
                cc.rgbResult=RGB((it->borderColor>>16)&0xFF,(it->borderColor>>8)&0xFF,it->borderColor&0xFF);
                cc.lpCustColors=custom2; cc.Flags=CC_FULLOPEN|CC_RGBINIT|CC_ANYCOLOR;
                if(ChooseColorW(&cc)){ COLORREF c=cc.rgbResult;
                    it->borderColor=(GetRValue(c)<<16)|(GetGValue(c)<<8)|GetBValue(c);
                    if(it->borderWidth<0.2) it->borderWidth=0.3;
                    st->dirty=true; commit(st);
                    HWND b=GetDlgItem(h,6009); if(b) InvalidateRect(b,NULL,TRUE);
                    InvalidateRect(h,NULL,FALSE); }
                return 0;
            }
            if(id==6010 && notify==BN_CLICKED){
                it->locked=(SendMessageW((HWND)l,BM_GETCHECK,0,0)==BST_CHECKED);
                st->dirty=true; commit(st); return 0; }
            if(id==6011 && notify==BN_CLICKED){
                st->design.items.erase(std::remove_if(
                    st->design.items.begin(),st->design.items.end(),
                    [&](const PrintItem&x){return x.id==st->selId;}),st->design.items.end());
                st->selId=-1; commit(st); designer_buildInspector(h);
                InvalidateRect(h,NULL,FALSE); return 0; }
            if(id==6012 && notify==BN_CLICKED){
                PrintItem n=*it; n.id=nextItemId(st); n.x+=10;n.y+=10;
                st->design.items.push_back(n); st->selId=n.id; commit(st);
                designer_buildInspector(h); InvalidateRect(h,NULL,FALSE); return 0; }
            // v1.23.0: 3-segment selectors (BS_PUSHLIKE checkboxes). Set the
            // chosen value, then rebuild the inspector so the sibling buttons
            // un-check (radio behaviour without owner-draw state).
            if(id>=6020 && id<=6031 && notify==BN_CLICKED){
                if(id>=6020 && id<=6022) it->align=id-6020;        // R/C/L
                else if(id>=6023 && id<=6025) it->dir=id-6023;     // RTL/LTR/auto
                else if(id>=6026 && id<=6028) it->valign=id-6026;  // top/mid/bot
                else if(id>=6029 && id<=6031) it->objectFit=id-6029; // contain/cover/fill
                st->dirty=true; commit(st);
                designer_buildInspector(h); InvalidateRect(h,NULL,FALSE); return 0;
            }
            if(id==6032 && notify==EN_KILLFOCUS){
                double v=readNumEdit(h,6032); if(v<0)v=0; if(v>50)v=50;
                it->padding=v; st->dirty=true; commit(st);
                InvalidateRect(h,NULL,FALSE); return 0;
            }
            return 0;
        }
        switch(id){
        case IDC_DSG_SAVE:    designer_save(h,false); return 0;
        case IDC_DSG_SAVEAS:  designer_save(h,true);  return 0;
        case IDC_DSG_DOWNLOAD:{
            wchar_t file[MAX_PATH]=L"design.aztpl";
            OPENFILENAMEW ofn; ZeroMemory(&ofn,sizeof(ofn)); ofn.lStructSize=sizeof(ofn);
            ofn.hwndOwner=h; ofn.lpstrFilter=L"DarmanPlus Template\0*.aztpl\0";
            ofn.lpstrDefExt=L"aztpl"; ofn.lpstrFile=file; ofn.nMaxFile=MAX_PATH;
            ofn.Flags=OFN_OVERWRITEPROMPT;
            if(GetSaveFileNameW(&ofn)){
                std::string j=Design_ToJson(st->design);
                HANDLE fh=CreateFileW(file,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL,NULL);
                if(fh!=INVALID_HANDLE_VALUE){ DWORD wr; WriteFile(fh,j.data(),
                    (DWORD)j.size(),&wr,NULL); CloseHandle(fh); }
            }
            return 0;
        }
        case IDC_DSG_EXIT: SendMessageW(h,WM_CLOSE,0,0); return 0;
        // E2: tool toggles. Each fresh open starts on «ابزار دست» (toolMode=1,
        // set in the DesignerState ctor); the last-used tool is NOT remembered.
        case IDC_DSG_TOOLHAND:
            st->toolMode=1;
            setFlatButtonStyle(st->hToolHand,BS_PRIMARY);
            setFlatButtonStyle(st->hToolSelect,BS_OUTLINE);
            SetCursor(LoadCursor(NULL,IDC_HAND));
            return 0;
        case IDC_DSG_TOOLSEL:
            st->toolMode=0;
            setFlatButtonStyle(st->hToolSelect,BS_PRIMARY);
            setFlatButtonStyle(st->hToolHand,BS_OUTLINE);
            SetCursor(LoadCursor(NULL,IDC_ARROW));
            return 0;
        case IDC_DSG_PAPER:
            if(HIWORD(w)==CBN_SELCHANGE){
                int sel=(int)SendMessageW(st->hPaperCombo,CB_GETCURSEL,0,0);
                wchar_t b[32]={0}; SendMessageW(st->hPaperCombo,CB_GETLBTEXT,sel,(LPARAM)b);
                double pw,ph; if(Paper_Dims(b,pw,ph)){
                    st->design.paper=b;
                    st->design.paperW=(st->design.orientation==1)?ph:pw;
                    st->design.paperH=(st->design.orientation==1)?pw:ph;
                    reflowDesign(st->design);   // E1: scale layout to new paper
                    commit(st); InvalidateRect(h,NULL,FALSE);
                }
            }
            return 0;
        case IDC_DSG_ORIENT:
            if(HIWORD(w)==CBN_SELCHANGE){
                int sel=(int)SendMessageW(st->hOrientCombo,CB_GETCURSEL,0,0);
                if(sel!=st->design.orientation){
                    st->design.orientation=sel;
                    std::swap(st->design.paperW,st->design.paperH);
                    reflowDesign(st->design);   // E1: scale layout to swapped dims
                    commit(st); InvalidateRect(h,NULL,FALSE);
                }
            }
            return 0;
        }
        return 0;
    }
    case WM_CTLCOLORSTATIC:{ HDC dc=(HDC)w; SetBkColor(dc,g_theme.bg);
        SetTextColor(dc,g_theme.text); return (LRESULT)g_brBg; }
    case WM_CTLCOLOREDIT:{ HDC dc=(HDC)w; SetBkColor(dc,g_theme.inputBg);
        SetTextColor(dc,g_theme.text); return (LRESULT)g_brSurface; }
    case WM_DRAWITEM:{
        LPDRAWITEMSTRUCT di=(LPDRAWITEMSTRUCT)l;
        if(drawThemedComboItem(di)) return TRUE;
        // owner-draw colour buttons (6007 text, 6009 border)
        if(di && st && (di->CtlID==6007 || di->CtlID==6009)){
            PrintItem* it=findItem(st,st->selId);
            COLORREF c=RGB(0,0,0);
            if(it){ unsigned v=(di->CtlID==6007)?it->textColor:it->borderColor;
                c=RGB((v>>16)&0xFF,(v>>8)&0xFF,v&0xFF); }
            RECT r=di->rcItem;
            HBRUSH bg=CreateSolidBrush(g_theme.surface); FillRect(di->hDC,&r,bg); DeleteObject(bg);
            RECT sw=r; InflateRect(&sw,-S(3),-S(3));
            HBRUSH cb=CreateSolidBrush(c); FillRect(di->hDC,&sw,cb); DeleteObject(cb);
            HBRUSH fr=CreateSolidBrush(g_theme.border); FrameRect(di->hDC,&sw,fr); DeleteObject(fr);
            if(di->itemState & ODS_FOCUS){ HBRUSH ff=CreateSolidBrush(g_theme.accent);
                FrameRect(di->hDC,&r,ff); DeleteObject(ff); }
            return TRUE;
        }
        break;
    }
    case WM_CLOSE:
        if(st && st->dirty){
            int r=MessageBoxW(h,
                L"\u062a\u063a\u06cc\u06cc\u0631\u0627\u062a \u0630\u062e\u06cc\u0631\u0647 \u0646\u0634\u062f\u0647. \u0622\u06cc\u0627 \u0645\u06cc\u200c\u062e\u0648\u0627\u0647\u06cc\u062f \u0630\u062e\u06cc\u0631\u0647 \u06a9\u0646\u06cc\u062f\u061f",
                L"\u062f\u06cc\u0632\u0627\u06cc\u0646 \u0686\u0627\u067e",MB_YESNOCANCEL|MB_ICONQUESTION);
            if(r==IDCANCEL) return 0;
            if(r==IDYES) designer_save(h,false);
        }
        DestroyWindow(h); return 0;
    case WM_DESTROY:
        clearInspector();
        // §1.14.2: detach the state pointer so any message dispatched between
        // DestroyWindow and the heap `delete st` in OpenDesignerWindow sees NULL
        // (every handler guards on !st) rather than a freed DesignerState.
        SetWindowLongPtrW(h,GWLP_USERDATA,0);
        // §F fix: no PostQuitMessage — the modal loop in OpenDesignerWindow()
        // breaks on !IsWindow(h). Injecting WM_QUIT here would tear down the
        // whole application message pump on the way back to the frame.
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

static void designer_save(HWND h,bool asNew){
    DesignerState* st=(DesignerState*)GetWindowLongPtrW(h,GWLP_USERDATA);
    if(!st) return;
    if(st->design.kind==L"builtin") asNew=true;     // never overwrite a builtin
    if(asNew){
        // simple name prompt via input box (reuse a tiny dialog)
        std::wstring name=st->design.name+L" (\u06a9\u067e\u06cc)";
        st->design.kind=L"user"; st->design.name=name; st->design.id=0;
        int newId=Designs_Insert(st->design);
        st->design.id=newId;
    } else {
        Designs_Update(st->design);
    }
    // apply to selected sections
    for(int sid:st->targetSections) SectionDesign_Set(sid,st->design.id);
    // §D.3: ALSO persist the design per-section under
    // data/print_designs/{depCode}.az_design (forward-compatible JSON per §H —
    // unknown keys are preserved by Design_FromJson on load). The directory is
    // created on demand. This gives each department its own restorable design
    // file alongside the central store binding.
    {
        std::wstring dir = dataDir() + L"\\print_designs";
        CreateDirectoryW(dir.c_str(), NULL);
        std::vector<Section> all; Sections_All(all);
        std::string json = Design_ToJson(st->design);
        for(int sid : st->targetSections){
            std::wstring code;
            for(const auto& s : all) if(s.id==sid){ code=s.code; break; }
            if(code.empty()){ wchar_t b[24]; swprintf(b,24,L"DEP%d",sid); code=b; }
            std::wstring path = dir + L"\\" + code + L".az_design";
            // write raw UTF-8 JSON
            HANDLE fh=CreateFileW(path.c_str(),GENERIC_WRITE,0,NULL,
                                  CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
            if(fh!=INVALID_HANDLE_VALUE){
                DWORD wr; WriteFile(fh,json.data(),(DWORD)json.size(),&wr,NULL);
                CloseHandle(fh);
            }
        }
    }
    // broadcast design push
    if(g_hFrame) PostMessageW(g_hFrame,WM_APP_DESIGN_PUSHED,(WPARAM)st->design.id,0);
    st->dirty=false;
    MessageBoxW(h,L"\u062f\u06cc\u0632\u0627\u06cc\u0646 \u0630\u062e\u06cc\u0631\u0647 \u0648 \u0628\u0631 \u0628\u062e\u0634\u200c\u0647\u0627 \u0627\u0639\u0645\u0627\u0644 \u0634\u062f.",
        L"\u062f\u06cc\u0632\u0627\u06cc\u0646 \u0686\u0627\u067e",MB_OK|MB_ICONINFORMATION);
}

static void OpenDesignerWindow(HWND owner,const DesignerLaunch& launch,
                               const PrintDesign* initial){
    static bool reg=false; const wchar_t* CLS=L"AzPrintDesigner";
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=DesignerProc; wc.hInstance=g_hInst;
        // E2: class cursor defaults to the hand tool (toolMode=1 on open), so the
        // hand cursor shows from the first frame; WM_SETCURSOR flips to the arrow
        // when the select tool is active.
        wc.hCursor=LoadCursor(NULL,IDC_HAND); wc.lpszClassName=CLS;
        wc.hbrBackground=g_brBg; wc.style=CS_HREDRAW|CS_VREDRAW;
        RegisterClassW(&wc); reg=true; }
    // §1.14.2 hardening: the inspector child-handle table is process-global and
    // shared by every designer instance. If a previous editor closed abnormally
    // (e.g. a contained fault) the table could still hold stale HWNDs that the
    // next build would try to destroy. Purge it up-front so each open starts
    // from a known-clean state (idempotent — clearInspector zeroes the count).
    clearInspector();

    DesignerState* st=new DesignerState();
    st->hMain=owner; st->targetSections=launch.sectionIds;
    bool resolved=false;
    if(initial){ st->design=*initial; resolved=true; }
    else {
        // load active design of first section, else T01
        if(!launch.sectionIds.empty())
            resolved=SectionDesign_Resolve(launch.sectionIds[0],st->design);
        if(!resolved) resolved=MachineDesign_Resolve(st->design);
        if(!resolved){ std::vector<PrintDesign> b; Designs_Builtins(b);
            if(!b.empty()){ st->design=b[0]; resolved=true; } }
    }
    // §1.12.0 crash-hardening: NEVER open the designer on an empty/garbage
    // design. If nothing resolved (missing/corrupt store), synthesize a minimal
    // valid A5 blank so the canvas + paint loop have safe geometry.
    if(!resolved || st->design.paperW<=0 || st->design.paperH<=0){
        PrintDesign blank;            // ctor → A5 148×210, no items
        blank.name=L"\u062e\u0627\u0644\u06cc";       // «خالی»
        st->design=blank;
        Breadcrumb(L"print designer: fallback blank design");
    }
    // §1.14.2: a missing paper name leaves the paper combo unselected and lets a
    // later orientation swap operate on an inconsistent design; default it.
    if(st->design.paper.empty()) st->design.paper=L"A5";
    if(st->design.orientation!=0 && st->design.orientation!=1) st->design.orientation=0;
    // Sanitize every item's font so GDI+ FontFamily never receives an empty name.
    for(auto& it : st->design.items){
        if(it.fontName.empty()) it.fontName=L"Vazirmatn";
        if(it.fontPt<=0)        it.fontPt=10;
        if(it.w<=0)             it.w=1;       // zero-size items confuse hit-test/paint
        if(it.h<=0)             it.h=1;
    }
    // E1: the loaded layout already fits its current paper, so adopt that paper
    // as the proportional-reflow baseline. (Templates are regenerated from PW/PH
    // by Design_BuiltinTemplate(), so their coordinates are already correct and
    // reflow only ever applies on a later USER paper/orientation change.)
    st->design.baseW = st->design.paperW;
    st->design.baseH = st->design.paperH;
    st->undo.seed(st->design);

    // §D.2: designer header shows «طراح چاپ — {نام بخش}» for the chosen section.
    std::wstring titleTxt = L"\u0637\u0631\u0627\u062d \u0686\u0627\u067e";   // «طراح چاپ»
    if(!launch.sectionIds.empty()){
        std::vector<Section> all; Sections_All(all);
        for(const auto& s : all) if(s.id==launch.sectionIds[0]){
            titleTxt += L" \u2014 " + s.name_fa; break;
        }
    }
    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA,0,&wa,0);
    HWND h=CreateWindowExW(0,CLS,titleTxt.c_str(),
        WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN,
        wa.left,wa.top,wa.right-wa.left,wa.bottom-wa.top,owner,NULL,g_hInst,NULL);
    // §8 (1.14.0) hardening: if the host window could not be created, never
    // proceed to wire controls against a NULL HWND (that was a guaranteed
    // fault). Free state, re-enable the owner, and bail gracefully.
    if(!h || !IsWindow(h)){
        Breadcrumb(L"print designer: editor window create failed");
        PrintDesigner_Log(L"editor",L"CreateWindow returned NULL");
        delete st;
        if(owner && IsWindow(owner)) EnableWindow(owner,TRUE);
        return;
    }
    SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)st);

    HWND bSave=createFlatButton(h,IDC_DSG_SAVE,L"\u0630\u062e\u06cc\u0631\u0647",ICO_SAVE,BS_PRIMARY,0,0,10,10);
    HWND bSaveAs=createFlatButton(h,IDC_DSG_SAVEAS,L"\u0630\u062e\u06cc\u0631\u0647 \u0628\u0647 \u0639\u0646\u0648\u0627\u0646 \u062c\u062f\u06cc\u062f",ICO_SAVE,BS_OUTLINE,0,0,10,10);
    HWND bDownload=createFlatButton(h,IDC_DSG_DOWNLOAD,L"\u062f\u0627\u0646\u0644\u0648\u062f \u062f\u06cc\u0632\u0627\u06cc\u0646",ICO_SAVE,BS_OUTLINE,0,0,10,10);
    HWND bExit=createFlatButton(h,IDC_DSG_EXIT,L"\u062e\u0631\u0648\u062c",ICO_X,BS_DANGER,0,0,10,10);
    HWND topBtns[]={bSave,bSaveAs,bDownload,bExit};
    for(HWND b:topBtns) setFlatButtonBg(b,g_theme.bg);
    st->hPaperCombo=createThemedCombo(h,IDC_DSG_PAPER);
    SendMessageW(st->hPaperCombo,WM_SETFONT,(WPARAM)g_fUI,TRUE);
    const wchar_t* papers[]={L"A3",L"A4",L"A5",L"A6",L"B5",L"Letter",L"R80",L"R58",
                             L"L90",L"L100",L"L102",L"L130"};
    for(auto p:papers) SendMessageW(st->hPaperCombo,CB_ADDSTRING,0,(LPARAM)p);
    SendMessageW(st->hPaperCombo,CB_SELECTSTRING,(WPARAM)-1,(LPARAM)st->design.paper.c_str());
    st->hOrientCombo=createThemedCombo(h,IDC_DSG_ORIENT);
    SendMessageW(st->hOrientCombo,WM_SETFONT,(WPARAM)g_fUI,TRUE);
    SendMessageW(st->hOrientCombo,CB_ADDSTRING,0,(LPARAM)L"\u0639\u0645\u0648\u062f\u06cc");
    SendMessageW(st->hOrientCombo,CB_ADDSTRING,0,(LPARAM)L"\u0627\u0641\u0642\u06cc");
    SendMessageW(st->hOrientCombo,CB_SETCURSEL,st->design.orientation,0);
    st->hZoom=CreateWindowExW(0,TRACKBAR_CLASSW,L"",
        WS_CHILD|WS_VISIBLE|TBS_HORZ,0,0,10,10,h,(HMENU)IDC_DSG_ZOOM,g_hInst,NULL);
    SendMessageW(st->hZoom,TBM_SETRANGE,TRUE,MAKELONG(25,400));
    SendMessageW(st->hZoom,TBM_SETPOS,TRUE,100);
    // E2: tool toggles. «ابزار دست» is active by default (BS_PRIMARY); the
    // select tool starts as BS_OUTLINE. Positions are finalised in
    // designer_layout(); sizes here are placeholders (0,0,10,10) like the others.
    st->hToolSelect=createFlatButton(h,IDC_DSG_TOOLSEL,
        L"\u0627\u0628\u0632\u0627\u0631 \u0627\u0646\u062a\u062e\u0627\u0628",
        ICO_NONE,BS_OUTLINE,0,0,10,10);
    st->hToolHand=createFlatButton(h,IDC_DSG_TOOLHAND,
        L"\u0627\u0628\u0632\u0627\u0631 \u062f\u0633\u062a",
        ICO_NONE,BS_PRIMARY,0,0,10,10);
    setFlatButtonBg(st->hToolSelect,g_theme.bg);
    setFlatButtonBg(st->hToolHand,g_theme.bg);

    designer_layout(h);
    ShowWindow(h,SW_SHOW);
    EnableWindow(owner,FALSE);
    // ------------------------------------------------------------------------
    //  v1.17.0 — MODAL PUMP, crash root-cause FIXED (no more longjmp-from-VEH).
    //
    //  ROOT CAUSE of the field crash (code 0x80000026 = STATUS_LONGJUMP, last
    //  breadcrumb "print designer: VEH contained pump fault"): the previous
    //  release tried to "contain" a fault by calling longjmp() from INSIDE a
    //  vectored exception handler. On this toolchain (MinGW-w64, Win32 threads,
    //  DWARF/SEH unwind) longjmp() itself issues an RtlUnwind that raises a
    //  SECOND exception with code STATUS_LONGJUMP while the first fault is still
    //  being dispatched — which is unrecoverable and crashed the whole app. In
    //  other words, the "containment" mechanism WAS the crash.
    //
    //  FIX: remove the longjmp/VEH containment entirely. The real defects it was
    //  papering over are already fixed up-front in this open flow — the state is
    //  never NULL/garbage, the design is sanitized, every font goes through
    //  makeSafeFont(), bindings are reconciled and re-validated, and the section
    //  picker only ever yields live section ids. The pump below is a plain,
    //  well-behaved Win32 modal loop that breaks as soon as `h` is destroyed.
    // ------------------------------------------------------------------------
    MSG msg;
    while(IsWindow(h) && GetMessageW(&msg,NULL,0,0)){
        if(msg.message==WM_QUIT){ PostQuitMessage((int)msg.wParam); break; }
        if(!IsWindow(h)) break;
        if(IsDialogMessageW(h,&msg)){ if(!IsWindow(h)) break; continue; }
        TranslateMessage(&msg); DispatchMessageW(&msg);
        if(!IsWindow(h)) break;
    }
    if(owner && IsWindow(owner)){ EnableWindow(owner,TRUE); SetForegroundWindow(owner); }
    delete st;
}

} // anonymous namespace

// ---------------------------------------------------------- public entries ---
//  §8 (1.12.0): the real open flow, wrapped below by a SEH containment layer
//  (PrintDesigner_Open) so a fault inside the picker/editor never kills the
//  whole process. Preconditions are validated up-front and the section<->design
//  store is reconciled with the live Sections registry on every open.
static void PrintDesigner_OpenCore(HWND hMain){
    Breadcrumb(L"print designer: open (enter)");
    if(!hMain || !IsWindow(hMain)) hMain = g_hFrame;
    if(!hMain || !IsWindow(hMain)){
        Breadcrumb(L"print designer: abort — no valid owner");
        return;
    }
    // §1.18.1 CRASH ROOT-CAUSE FIX: the print designer's inspector builds custom
    // controls (AzNumberSpinner / AzColorPicker / AzSwitch) whose window classes
    // are registered by uikit::Az_RegisterControls(). That registration used to
    // only run when the *user-settings* screen was visited. When the designer is
    // opened from the MANAGEMENT panel («دیزاین چاپ») WITHOUT that screen ever
    // having been created, the classes were unregistered, every spinner/color/
    // switch CreateWindowExW returned NULL, and the first access of the (NULL)
    // child handle inside the editor faulted with ACCESS_VIOLATION (the reported
    // crash at "print designer: open editor"). Registering here — idempotent via
    // an internal `done` guard — guarantees the classes always exist before the
    // editor (or section picker) creates any control. Cheap, allocates nothing on
    // weak hardware, and never touches the printer.
    uikit::Az_RegisterControls();
    Breadcrumb(L"print designer: init stores");
    Sections_Init(); Designs_Init();
    DesignerLaunch launch;
    Breadcrumb(L"print designer: machine design (no section picker)");
    // §1.19.0 (restored v1.68.0): PREFER the HTML/CSS/JS designer opened in the
    // operator's DEFAULT BROWSER (loopback host + ShellExecute). The real design
    // editor runs in the browser process — crash-proof, universal (every Windows
    // has a default browser, no WebView2 runtime needed) and the requested UX
    // («دیزاین چاپ توی وب باز بشه، پنل C++ باز نشه»). A small native session
    // tracker stays on screen to reflect save state and end the session. Only if
    // the browser host genuinely cannot start do we fall back to the in-process
    // native GDI editor so the feature is never lost.
    Breadcrumb(L"print designer: try browser designer");
    bool webOk = WebDesigner_Open(hMain, launch.sectionIds);
    if(!webOk){
        Breadcrumb(L"print designer: browser unavailable — native fallback");
        OpenDesignerWindow(hMain,launch,nullptr);
    }
    Breadcrumb(L"print designer: closed");
}

// §8 (1.14.0): crash containment — strengthened.
//  MinGW's GCC win32 build does not expose MS-style __try/__except for
//  arbitrary code, so — mirroring the backup analyzer's proven approach — we
//  arm a thread-local vectored exception handler that longjmp()s back to a
//  landing pad established here on a genuine hardware fault originating inside
//  the designer open flow. Faults anywhere else in the app fall through to the
//  normal crash handler untouched.
//
//  The REAL bugs are fixed up-front (every init step is guarded, the picker and
//  editor never dereference a NULL/garbage state, bindings are reconciled and
//  re-validated, designs are sanitized before the canvas sees them). This VEH
//  is belt-and-braces fault isolation so a regression can never again be
//  process-fatal on «print designer: open» — AND, critically, the landing pad
//  now performs a full cleanup so the app is left in a CONSISTENT state and the
//  designer can be opened again afterwards:
//    • release any mouse capture grabbed mid-drag
//    • tear down orphaned inspector child windows (global handle table)
//    • destroy any half-built designer/picker popup still on screen
//    • re-enable the owner so the UI is not frozen
//    • write a structured, persistent crash report (logs\print_designer.log)
//  A re-entrancy guard makes a second concurrent open a no-op (idempotent).
// v1.17.0: the longjmp landing pads / VEH armed-flags / fault-code storage were
// removed (the longjmp-from-VEH was the crash). Only the simple re-entrancy
// guard remains.
static bool                 s_pdOpening = false;   // re-entrancy / idempotency

// Persistent, structured designer error log (logs\print_designer.log).
static void PrintDesigner_Log(const wchar_t* where, const wchar_t* detail){
    SYSTEMTIME st = iranNow();
    wchar_t hdr[96];
    swprintf(hdr,96,L"[%04d-%02d-%02d %02d:%02d:%02d] print_designer/",
             st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
    std::wstring line = hdr; line += (where?where:L"?");
    line += L": "; line += (detail?detail:L""); line += L"\r\n";
    writeFileUtf8(logsDir()+L"\\print_designer.log", line, true);
}

// Best-effort, exception-safe teardown of any windows the designer may have
// left on screen after a contained fault. Class names are matched so we only
// ever touch our own popups.
static void PrintDesigner_PurgeWindows(){
    ReleaseCapture();
    clearInspector();                       // destroys + zeroes the global table
    const wchar_t* CLS[]={L"AzPrintDesigner",L"AzSectionPicker",L"AzRestoreDesign"};
    for(const wchar_t* cls : CLS){
        HWND w=NULL;
        while((w=FindWindowExW(NULL,NULL,cls,NULL))!=NULL && IsWindow(w)){
            DestroyWindow(w);
            // FindWindowEx restarts from the top each pass; bail if it cannot
            // be destroyed to avoid an infinite loop.
            if(IsWindow(w)) break;
        }
    }
}

void PrintDesigner_Open(HWND hMain){
    Breadcrumb(L"print designer: safe-open");
    // Idempotency: if an open is already in flight (e.g. a double-click on the
    // toolbar entry), do nothing instead of stacking two nested modal loops.
    if(s_pdOpening){
        Breadcrumb(L"print designer: open suppressed (already open)");
        return;
    }
    s_pdOpening = true;

    //  v1.17.0: NO vectored-exception-handler + longjmp "containment" here.
    //  That mechanism was itself the crash source (STATUS_LONGJUMP raised by
    //  longjmp's RtlUnwind from inside a VEH on this DWARF/SEH MinGW toolchain).
    //  The open flow is now plain, with every real precondition guarded inside
    //  PrintDesigner_OpenCore (valid owner, seeded/reconciled stores, live
    //  section ids, sanitized design + safe fonts). Reference the helpers that
    //  used to be VEH-only so -Wunused doesn't fail the build.
    (void)&PrintDesigner_PurgeWindows;
    PrintDesigner_OpenCore(hMain);

    s_pdOpening = false;
}

// =====================================================================
//  RESTORE DESIGN (§4)
// =====================================================================
namespace {
struct RestoreState {
    PrintDesign loaded; bool valid;
    std::vector<int> sections;
    HWND hStatus, hList;
    RestoreState():valid(false){}
};

static void restore_loadFile(HWND h,RestoreState* st,const std::wstring& path){
    HANDLE fh=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,
        OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(fh==INVALID_HANDLE_VALUE){ st->valid=false;
        SetWindowTextW(st->hStatus,L"\u0641\u0627\u06cc\u0644 \u0628\u0627\u0632 \u0646\u0634\u062f"); return; }
    DWORD sz=GetFileSize(fh,NULL); std::string j;
    if(sz!=INVALID_FILE_SIZE&&sz>0){ j.resize(sz); DWORD rd; ReadFile(fh,&j[0],sz,&rd,NULL); j.resize(rd);}
    CloseHandle(fh);
    std::wstring err;
    if(Design_FromJson(j,st->loaded,err)){
        st->valid=true;
        std::wstring msg=L"\u062f\u06cc\u0632\u0627\u06cc\u0646 \u0645\u0639\u062a\u0628\u0631: "+st->loaded.name;
        SetWindowTextW(st->hStatus,msg.c_str());
    } else {
        st->valid=false;
        SetWindowTextW(st->hStatus,err.c_str());
    }
    InvalidateRect(h,NULL,TRUE);
}

static LRESULT CALLBACK RestoreProc(HWND h,UINT m,WPARAM w,LPARAM l){
    RestoreState* st=(RestoreState*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case AZ_DROPZONE_DROPPED:{
        std::wstring* p=(std::wstring*)l;
        if(st && p){ restore_loadFile(h,st,*p); }
        delete p;                 // §1.14.2: free even if state is gone (no leak)
        return 0;
    }
    case WM_COMMAND:{
        if(!st) return 0;         // §1.14.2: bail on a detached/post-destroy state
        int id=LOWORD(w);
        if(id==IDC_RST_PICK){
            wchar_t file[MAX_PATH]={0};
            OPENFILENAMEW ofn; ZeroMemory(&ofn,sizeof(ofn)); ofn.lStructSize=sizeof(ofn);
            ofn.hwndOwner=h; ofn.lpstrFilter=L"DarmanPlus Template\0*.aztpl;*.json\0";
            ofn.lpstrFile=file; ofn.nMaxFile=MAX_PATH; ofn.Flags=OFN_FILEMUSTEXIST;
            if(GetOpenFileNameW(&ofn)) restore_loadFile(h,st,file);
        } else if(id==IDC_RST_APPLY){
            if(!st->valid){ MessageBoxW(h,
                L"\u0627\u0628\u062a\u062f\u0627 \u06cc\u06a9 \u0641\u0627\u06cc\u0644 \u0645\u0639\u062a\u0628\u0631 \u0628\u0627\u0631\u06af\u0630\u0627\u0631\u06cc \u06a9\u0646\u06cc\u062f.",
                L"\u0628\u0627\u0632\u06af\u0631\u062f\u0627\u0646\u06cc",MB_OK|MB_ICONWARNING); return 0; }
            // section picker
            DesignerLaunch launch;
            if(!RunSectionPicker(h,launch)) return 0;
            st->loaded.kind=L"user"; st->loaded.id=0;
            int newId=Designs_Insert(st->loaded);
            for(int sid:launch.sectionIds) SectionDesign_Set(sid,newId);
            if(g_hFrame) PostMessageW(g_hFrame,WM_APP_DESIGN_PUSHED,(WPARAM)newId,0);
            MessageBoxW(h,L"\u062f\u06cc\u0632\u0627\u06cc\u0646 \u0630\u062e\u06cc\u0631\u0647 \u0648 \u0627\u0639\u0645\u0627\u0644 \u0634\u062f.",
                L"\u0628\u0627\u0632\u06af\u0631\u062f\u0627\u0646\u06cc",MB_OK|MB_ICONINFORMATION);
            DestroyWindow(h);
        } else if(id==IDC_RST_CANCEL){ DestroyWindow(h); }
        return 0;
    }
    case WM_CTLCOLORSTATIC:{ HDC dc=(HDC)w; SetBkColor(dc,g_theme.surface);
        SetTextColor(dc,g_theme.text); return (LRESULT)g_brSurface; }
    case WM_ERASEBKGND:{ RECT rc; GetClientRect(h,&rc); FillRect((HDC)w,&rc,g_brSurface); return 1; }
    case WM_CLOSE: DestroyWindow(h); return 0;
    // §F fix: no PostQuitMessage — nested modal loop breaks on !IsWindow(h).
    case WM_DESTROY:
        // §1.14.2: RestoreState is a stack object in RestoreDesign_Open; detach so
        // a post-destroy message can never dereference a dangling pointer.
        SetWindowLongPtrW(h,GWLP_USERDATA,0);
        return 0;
    }
    return DefWindowProcW(h,m,w,l);
}
} // anon

void RestoreDesign_Open(HWND hMain){
    Breadcrumb(L"restore design: open");
    Sections_Init(); Designs_Init();
    SectionDesign_Cleanup();
    static bool reg=false; const wchar_t* CLS=L"AzRestoreDesign";
    if(!reg){ WNDCLASSW wc={0}; wc.lpfnWndProc=RestoreProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName=CLS;
        wc.hbrBackground=g_brSurface; RegisterClassW(&wc); reg=true; }
    int W=S(640),H=S(480);
    RECT pr; GetWindowRect(hMain,&pr);
    int x=pr.left+((pr.right-pr.left)-W)/2, y=pr.top+((pr.bottom-pr.top)-H)/2;
    RestoreState st;
    HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME,CLS,
        L"\u0628\u0627\u0632\u06af\u0631\u062f\u0627\u0646\u06cc \u062f\u06cc\u0632\u0627\u06cc\u0646 \u0686\u0627\u067e",
        WS_POPUP|WS_CAPTION|WS_SYSMENU,x,y,W,H,hMain,NULL,g_hInst,NULL);
    SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)&st);
    int mgn=S(14);
    uikit::AzDropZone_Create(h,IDC_RST_DROP,
        L"\u0641\u0627\u06cc\u0644 .aztpl \u0631\u0627 \u0627\u06cc\u0646\u062c\u0627 \u0631\u0647\u0627 \u06a9\u0646\u06cc\u062f \u06cc\u0627 \u06a9\u0644\u06cc\u06a9 \u06a9\u0646\u06cc\u062f",
        L"DarmanPlus Template\0*.aztpl;*.json\0",mgn,mgn,W-2*mgn,S(120));
    HWND bPick=createFlatButton(h,IDC_RST_PICK,L"\u0627\u0646\u062a\u062e\u0627\u0628 \u0641\u0627\u06cc\u0644",ICO_SAVE,BS_OUTLINE,
        mgn,mgn+S(130),S(140),S(30));
    setFlatButtonBg(bPick,g_theme.surface);
    st.hStatus=CreateWindowExW(0,L"STATIC",
        L"\u0641\u0627\u06cc\u0644\u06cc \u0628\u0627\u0631\u06af\u0630\u0627\u0631\u06cc \u0646\u0634\u062f\u0647",
        WS_CHILD|WS_VISIBLE|SS_RIGHT,mgn,mgn+S(170),W-2*mgn,S(24),h,NULL,g_hInst,NULL);
    SendMessageW(st.hStatus,WM_SETFONT,(WPARAM)g_fUI,TRUE);
    HWND bApply=createFlatButton(h,IDC_RST_APPLY,
        L"\u0630\u062e\u06cc\u0631\u0647 \u0628\u0647 \u0639\u0646\u0648\u0627\u0646 \u062f\u06cc\u0632\u0627\u06cc\u0646 \u062c\u062f\u06cc\u062f \u0648 \u0627\u0639\u0645\u0627\u0644",
        ICO_CHECK,BS_PRIMARY,W-mgn-S(280),H-S(54),S(280),S(34));
    HWND bCancel=createFlatButton(h,IDC_RST_CANCEL,L"\u0627\u0646\u0635\u0631\u0627\u0641",ICO_X,BS_OUTLINE,
        mgn,H-S(54),S(110),S(34));
    setFlatButtonBg(bApply,g_theme.surface);
    setFlatButtonBg(bCancel,g_theme.surface);
    ShowWindow(h,SW_SHOW);
    EnableWindow(hMain,FALSE);
    // §F fix: self-contained modal pump (see RunSectionPicker note).
    MSG msg;
    while(IsWindow(h) && GetMessageW(&msg,NULL,0,0)){
        if(msg.message==WM_QUIT){ PostQuitMessage((int)msg.wParam); break; }
        if(!IsWindow(h)) break;
        if(IsDialogMessageW(h,&msg)){ if(!IsWindow(h)) break; continue; }
        TranslateMessage(&msg); DispatchMessageW(&msg);
        if(!IsWindow(h)) break;
    }
    EnableWindow(hMain,TRUE); SetForegroundWindow(hMain);
}
