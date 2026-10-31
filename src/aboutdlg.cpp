// aboutdlg.cpp - 关于对话框 300x168 (点主面板左上角标题弹出, 点外部自动关闭)
#include "app.h"
#include <shellapi.h>
#include <windowsx.h>

static const int AW = 300, AH = 152;

static bool InRect(const RECT& r, POINT p) {
    return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
}

static HFONT s_fTitle, s_fName, s_fVer, s_fText, s_fLink;
static HWND  s_ok;
static bool  s_seenFg = false;           // 弹框是否曾拿到前台 (之后失焦即视为点了外部)
static bool  s_closingOutside = false;   // 本次是由"点弹框外部"导致的关闭

static int A(int v) { return MulDiv(v, (int)g.dpi, 96); }

// 带下划线的链接字体: 下划线由字体自身绘制, 保证与文字基线严格对齐
static HFONT MakeLinkFont(int px, int weight) {
    LOGFONTW lf = {};
    lf.lfHeight     = -MulDiv(px, (int)g.dpi, 96);
    lf.lfWeight     = weight;
    lf.lfUnderline  = TRUE;
    lf.lfQuality    = CLEARTYPE_QUALITY;
    lf.lfCharSet    = DEFAULT_CHARSET;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    return CreateFontIndirectW(&lf);
}

// GitHub 链接热区 (逻辑布局: 标题 0-34 / 名称行 44-72 / 链接行 74-94 / 页脚 108-)
static const RECT LINK_L = { 14, 74, AW - 14, 94 };
static RECT LinkRect() { RECT r = { A(LINK_L.left), A(LINK_L.top), A(LINK_L.right), A(LINK_L.bottom) }; return r; }

static void RoundBtn(HDC dc, const RECT& r, int rad, COLORREF fill, COLORREF border) {
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = border ? CreatePen(PS_SOLID, 1, border) : (HPEN)GetStockObject(NULL_PEN);
    HBRUSH ob = (HBRUSH)SelectObject(dc, b);
    HPEN op = (HPEN)SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, rad, rad);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b); if (border) DeleteObject(p);
}

static LRESULT CALLBACK AboutProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        s_fTitle = MakeFont(13, FW_SEMIBOLD);
        s_fName  = MakeFont(15, FW_SEMIBOLD);
        s_fVer   = MakeFont(12, FW_SEMIBOLD);
        s_fText  = MakeFont(11, FW_NORMAL);
        s_fLink  = MakeLinkFont(11, FW_NORMAL);
        s_ok = CreateWindowExW(0, L"BUTTON", L"关闭",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)301, g.inst, nullptr);
        SetWindowPos(s_ok, nullptr, A(AW) - A(14) - A(76), A(AH) - A(44) + A(8),
            A(76), A(28), SWP_NOZORDER);
        s_seenFg = false;
        s_closingOutside = false;
        SetTimer(hwnd, 1, 250, nullptr);   // 轮询前台窗口: 点弹框外即关闭
        return 0;
    }

    case WM_TIMER:
        if (wp == 1) {
            if (GetForegroundWindow() == hwnd) s_seenFg = true;
            else if (s_seenFg) {
                s_closingOutside = true;
                DestroyWindow(hwnd);
                return 0;
            }
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT cr; GetClientRect(hwnd, &cr);
        HBRUSH bg = CreateSolidBrush(g.th.bg);
        FillRect(dc, &cr, bg);
        DeleteObject(bg);

        Txt(dc, L"关于", s_fTitle, g.th.text, { A(14), 0, A(120), A(34) },
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // 名称 + 版本号 (版本号用正常文字色, 与名称同色更醒目)
        Txt(dc, APP_NAME, s_fName, g.th.text, { A(14), A(44), A(190), A(72) },
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        wchar_t ver[32];
        wsprintfW(ver, L"v%s", APP_VERSION);
        Txt(dc, ver, s_fVer, g.th.text, { A(190), A(44), A(AW) - A(14), A(72) },
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        // 链接: 下划线由字体自己画, 与文字基线严格对齐
        Txt(dc, L"github.com/Seeagood/AdjustBrightness", s_fLink, ACCENT, LinkRect(),
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // 分隔线
        HPEN p = CreatePen(PS_SOLID, 1, g.th.sep);
        HPEN op = (HPEN)SelectObject(dc, p);
        MoveToEx(dc, 0, A(34), nullptr); LineTo(dc, cr.right, A(34));
        MoveToEx(dc, 0, A(AH) - A(44), nullptr); LineTo(dc, cr.right, A(AH) - A(44));
        SelectObject(dc, op); DeleteObject(p);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SETCURSOR: {
        POINT p; GetCursorPos(&p); ScreenToClient(hwnd, &p);
        RECT lr = LinkRect();
        SetCursor(LoadCursor(nullptr, InRect(lr, p) ? IDC_HAND : IDC_ARROW));
        return TRUE;
    }

    case WM_LBUTTONDOWN: {
        POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        RECT lr = LinkRect();
        if (InRect(lr, p)) {
            ShellExecuteW(hwnd, L"open", APP_URL, nullptr, nullptr, SW_SHOWNORMAL);
            DestroyWindow(hwnd);
        }
        return 0;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* di = (DRAWITEMSTRUCT*)lp;
        if ((int)di->CtlID != 301) break;
        bool pressed = (di->itemState & ODS_SELECTED) != 0;
        RECT r = di->rcItem;
        HBRUSH bg = CreateSolidBrush(g.th.bg);
        FillRect(di->hDC, &r, bg);
        DeleteObject(bg);
        RoundBtn(di->hDC, r, A(3), pressed ? g.th.track : g.th.btnBg, g.th.btnBorder);
        Txt(di->hDC, L"关闭", s_fText, g.th.text, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return TRUE;
    }

    case WM_COMMAND:
        if (LOWORD(wp) == 301) { DestroyWindow(hwnd); return 0; }
        return 0;

    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        if (s_fTitle) { DeleteObject(s_fTitle); s_fTitle = nullptr; }
        if (s_fName)  { DeleteObject(s_fName);  s_fName = nullptr; }
        if (s_fVer)   { DeleteObject(s_fVer);   s_fVer = nullptr; }
        if (s_fText)  { DeleteObject(s_fText);  s_fText = nullptr; }
        if (s_fLink)  { DeleteObject(s_fLink);  s_fLink = nullptr; }
        g.dlg = nullptr;
        if (s_closingOutside) {
            // 点的是弹框外的其它程序: 只关弹框, 面板继续留着, 由面板自己盯下一次外部点击
            MainWndArmOutsideClickHide();
        } else {
            SetForegroundWindow(g.hwnd);
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void AboutDlgOpen() {
    if (g.dlg) { SetForegroundWindow(g.dlg); return; }
    KillTimer(g.hwnd, IDT_OUTCLICK);   // 弹框期间由弹框自己判断外部点击

    WNDCLASSW wc = {};
    static bool reg = false;
    if (!reg) {
        wc.lpfnWndProc = AboutProc;
        wc.hInstance = g.inst;
        wc.lpszClassName = ABOUT_CLASS;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
        reg = true;
    }

    RECT mr; GetWindowRect(g.hwnd, &mr);
    int W = A(AW), H = A(AH);
    int x = mr.left + (mr.right - mr.left - W) / 2;
    int y = mr.top + (mr.bottom - mr.top - H) / 2;
    RECT ar; SystemParametersInfoW(SPI_GETWORKAREA, 0, &ar, 0);
    if (x + W > ar.right) x = ar.right - W;
    if (y + H > ar.bottom) y = ar.bottom - H;
    if (x < ar.left) x = ar.left;
    if (y < ar.top) y = ar.top;

    g.dlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        ABOUT_CLASS, L"关于", WS_POPUP,
        x, y, W, H, g.hwnd, nullptr, g.inst, nullptr);
    if (!g.dlg) return;
    ShowWindow(g.dlg, SW_SHOW);
    SetForegroundWindow(g.dlg);
}
