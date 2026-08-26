// ============================================================================
//  accounting.cpp — native GDI accounting dashboard (v1.97)
//  Home «حسابداری» entrance. Theme-safe (light / dark / neon); no pure white.
//  KPIs and recent tickets come from Accounting_Stats / Accounting_Recent.
// ============================================================================
#include "app.h"
#include "clinic_ops.h"

#define ACC_CLASS L"AzAccounting"

struct AccWin {
    AccountingStats st;
    std::vector<CashTicket> recent;
};

static std::wstring accFaMoney(long long v){
    return std::wstring(L"\x202A")+toFaDigits(formatMoney(v))+L"\x202C";
}
static COLORREF accTeal(){
    if(g_themeMode==TM_NEON) return RGB(0x2A,0xC8,0xB4);
    if(g_dark) return RGB(0x3A,0xB4,0x9C);
    return RGB(0x12,0x7A,0x6A);
}
static COLORREF accGold(){
    if(g_themeMode==TM_NEON) return RGB(0xE0,0xC4,0x6A);
    if(g_dark) return RGB(0xD0,0xB4,0x68);
    return RGB(0xA8,0x84,0x32);
}
static void accReload(AccWin* w){
    if(!w) return;
    w->st = Accounting_Stats();
    w->recent = Accounting_Recent(14);
}
static bool accIsRefund(const CashTicket& t){
    return t.status==L"refund" || t.status==L"cancelled";
}

static void accPaintKpi(HDC dc, RECT rc, const wchar_t* label, const std::wstring& value,
                        COLORREF brand, int icon){
    gpShadow(dc, rc, S(14), S(8), g_dark?90:46);
    COLORREF top=blendColor(g_theme.surfaceTop, brand, g_dark?10:6);
    gpGradRoundRect(dc, rc, S(14), top, g_theme.surface,
                    blendColor(g_theme.border, brand, 28));
    RECT bar=rc; bar.left=rc.right-S(5);
    gpFillAlpha(dc, bar, S(3), brand, 210);
    int ico=S(22);
    RECT ir={rc.right-S(18)-ico, rc.top+S(14), rc.right-S(18), rc.top+S(14)+ico};
    drawIcon(dc, icon, ir, brand, S(2));
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, g_fSmall);
    SetTextColor(dc, g_theme.textDim);
    RECT lr={rc.left+S(14), rc.top+S(12), ir.left-S(8), rc.top+S(32)};
    DrawTextW(dc, label, -1, &lr,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
    SelectObject(dc, g_fTitle);
    SetTextColor(dc, g_theme.sectionInk);
    RECT vr={rc.left+S(14), rc.top+S(36), rc.right-S(16), rc.bottom-S(14)};
    DrawTextW(dc, value.c_str(), -1, &vr,
        DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
}

static LRESULT CALLBACK accProc(HWND h, UINT m, WPARAM w, LPARAM l){
    AccWin* d=(AccWin*)GetWindowLongPtrW(h,GWLP_USERDATA);
    switch(m){
    case WM_CREATE: {
        d=new AccWin();
        SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)d);
        accReload(d);
        return 0; }
    case WM_NCDESTROY:
        delete d;
        return 0;
    case WM_APP_THEME:
        if(d) accReload(d);
        InvalidateRect(h,NULL,TRUE);
        return 0;
    case WM_SIZE:
        InvalidateRect(h,NULL,TRUE);
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc0=BeginPaint(h,&ps);
        RECT rc; GetClientRect(h,&rc);
        HDC dc=CreateCompatibleDC(dc0);
        HBITMAP bmp=CreateCompatibleBitmap(dc0, rc.right, rc.bottom);
        HGDIOBJ obm=SelectObject(dc,bmp);
        FillRect(dc,&rc,g_brBg);
        SetBkMode(dc,TRANSPARENT);

        int pad=S(22);
        RECT head={rc.left+pad, rc.top+S(14), rc.right-pad, rc.top+S(52)};
        gpShadow(dc, head, S(12), S(6), g_dark?70:36);
        gpGradRoundRect(dc, head, S(12),
            blendColor(g_theme.surfaceTop, accTeal(), g_dark?8:4),
            g_theme.surface,
            blendColor(g_theme.border, accGold(), 22));
        int ico=S(22);
        RECT ir={head.right-S(16)-ico, (head.top+head.bottom)/2-ico/2,
                 head.right-S(16),     (head.top+head.bottom)/2+ico/2};
        drawIcon(dc, ICO_WALLET, ir, accTeal(), S(2));
        SelectObject(dc, g_fTitle);
        SetTextColor(dc, g_theme.sectionInk);
        RECT tr={head.left+S(16), head.top, ir.left-S(10), head.bottom};
        DrawTextW(dc, L"داشبورد حسابداری", -1, &tr,
            DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
        SelectObject(dc, g_fSmall);
        SetTextColor(dc, g_theme.textDim);
        std::wstring sub=toFaDigits(d && !d->st.date.empty()
            ? d->st.date : jalaliDateShort(iranNow()));
        RECT sr={head.left+S(18), head.top, head.left+S(220), head.bottom};
        DrawTextW(dc, sub.c_str(), -1, &sr,
            DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

        AccountingStats st=d?d->st:AccountingStats();
        const wchar_t* labs[4]={L"درآمد امروز",L"پرداخت‌نشده",L"صندوق‌شده",L"استرداد"};
        long long vals[4]={st.income, st.unpaidAmt, st.cashedAmt, st.refundAmt};
        COLORREF cols[4]={accTeal(), g_theme.warn, accGold(), g_theme.danger};
        int icons[4]={ICO_WALLET, ICO_CLOCK, ICO_CHECK, ICO_X};
        int gap=S(12);
        int kw=(rc.right-rc.left-2*pad-3*gap)/4;
        if(kw<S(120)) kw=S(120);
        int ky=head.bottom+S(16);
        int kh=S(96);
        for(int i=0;i<4;i++){
            int x=rc.right-pad-(i+1)*kw-i*gap;
            RECT kr={x, ky, x+kw, ky+kh};
            accPaintKpi(dc, kr, labs[i], accFaMoney(vals[i])+L" ریال", cols[i], icons[i]);
        }

        RECT table={rc.left+pad, ky+kh+S(18), rc.right-pad, rc.bottom-S(16)};
        if(table.bottom-table.top>S(80)){
            gpShadow(dc, table, S(14), S(8), g_dark?80:40);
            gpGradRoundRect(dc, table, S(14),
                g_theme.surfaceTop, g_theme.surface, g_theme.border);
            SelectObject(dc, g_fUIB);
            SetTextColor(dc, g_theme.sectionInk);
            RECT tt={table.left+S(16), table.top+S(10), table.right-S(16), table.top+S(36)};
            DrawTextW(dc, L"بلیت‌های اخیر", -1, &tt,
                DT_RIGHT|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);

            const wchar_t* hc[6]={L"زمان",L"وضعیت",L"مبلغ",L"بخش",L"بیمار",L"بارکد"};
            int pct[6]={14,14,18,18,22,14};
            int tw=table.right-table.left-S(24);
            int hx=table.right-S(12);
            int hy=table.top+S(42);
            int hh=S(28);
            RECT hdr={table.left+S(10), hy, table.right-S(10), hy+hh};
            gpFillAlpha(dc, hdr, S(8), g_theme.surface2, 220);
            SelectObject(dc, g_fSmall);
            SetTextColor(dc, g_theme.labelInk);
            int cxr=hx;
            for(int i=5;i>=0;i--){
                int wcol=tw*pct[i]/100;
                RECT cr={cxr-wcol, hy, cxr, hy+hh};
                DrawTextW(dc, hc[i], -1, &cr,
                    DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
                cxr-=wcol;
            }

            static const std::vector<CashTicket> kEmpty;
            const std::vector<CashTicket>& rows=d?d->recent:kEmpty;
            int rowH=S(28);
            int y=hy+hh+S(4);
            int maxRows=(table.bottom-S(12)-y)/rowH;
            if(maxRows<0) maxRows=0;
            int n=(int)rows.size();
            if(n>maxRows) n=maxRows;
            if(n==0){
                SelectObject(dc, g_fUI);
                SetTextColor(dc, g_theme.textDim);
                RECT er={table.left+S(16), y+S(12), table.right-S(16), y+S(48)};
                DrawTextW(dc, L"بلیت اخیری ثبت نشده است.", -1, &er,
                    DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX);
            }
            for(int i=0;i<n;i++){
                const CashTicket& t=rows[(size_t)i];
                RECT rr={table.left+S(10), y, table.right-S(10), y+rowH};
                if(i%2) gpFillAlpha(dc, rr, S(6), g_theme.surface2, 140);
                std::wstring name=t.first;
                if(!t.last.empty()){ if(!name.empty()) name+=L" "; name+=t.last; }
                if(name.empty()) name=L"—";
                std::wstring sec=t.subName.empty()?t.sectionName:t.subName;
                if(sec.empty()) sec=t.sectionName;
                if(sec.empty()) sec=L"—";
                std::wstring stt=accIsRefund(t)?L"استرداد":(t.paid>0?L"صندوق‌شده":L"پرداخت‌نشده");
                COLORREF sc=accIsRefund(t)?g_theme.danger:(t.paid>0?accTeal():g_theme.warn);
                std::wstring amt=accFaMoney(t.paid>0?t.paid:t.payable);
                std::wstring when=t.time.empty()?t.jdate:t.time;
                std::wstring cells[6]={
                    toFaDigits(when), stt, amt, sec, name,
                    t.barcode.empty()?L"—":toFaDigits(t.barcode)
                };
                int xx=hx;
                for(int c=5;c>=0;c--){
                    int wcol=tw*pct[c]/100;
                    RECT cr={xx-wcol+S(4), y, xx-S(4), y+rowH};
                    SetTextColor(dc, c==1?sc:g_theme.text);
                    DrawTextW(dc, cells[c].c_str(), -1, &cr,
                        DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_RTLREADING|DT_NOPREFIX|DT_END_ELLIPSIS);
                    xx-=wcol;
                }
                y+=rowH;
            }
        }

        BitBlt(dc0,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,obm); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h,&ps);
        return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}

HWND createAccountingScreen(HWND frame){
    static bool reg=false;
    if(!reg){
        WNDCLASSW wc={0};
        wc.lpfnWndProc=accProc; wc.hInstance=g_hInst;
        wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        wc.lpszClassName=ACC_CLASS;
        RegisterClassW(&wc); reg=true;
    }
    RECT rc=frameContentRect();
    return CreateWindowExW(0,ACC_CLASS,L"",
        WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
        rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,frame,NULL,g_hInst,NULL);
}
