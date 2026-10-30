// presetdlg.cpp - 自定义快捷档位对话框 320x260 (3列11输入框, 点外部自动关闭)
#include "app.h"
#include <stdio.h>

static const int DW = 320, DH = 260;
static const wchar_t* DEF_NOTE = L"保存后自动升序排序，重复值合并去重。输入范围 0–100 整数。";

static HWND   s_edits[11];
static HWND   s_note, s_ok;
static HFONT  s_fText, s_fTitle;
static bool   s_bad[11];
static bool   s_dlgSeenFg = false;   // 弹框是否曾拿到前台 (之后失焦即视为点了外部)
static bool   s_closingOutside = false;  // 本次是由"点弹框外部"导致的关闭

static int D(int v) { return MulDiv(v, (int)g.dpi, 96); }

// 与主面板一致的圆角按钮 (自绘)
static void RoundBtn(HDC dc, const RECT& r, int rad, COLORREF fill, COLORREF border) {
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = border ? CreatePen(PS_SOLID, 1, border) : (HPEN)GetStockObject(NULL_PEN);
    HBRUSH ob = (HBRUSH)SelectObject(dc, b);
    HPEN op = (HPEN)SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, rad, rad);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b); if (border) DeleteObject(p);
}

static void DlgLayout(HWND hwnd) {
    int n = 11;
    int gx = D(16), gy = D(34) + D(12);
    int cellH = D(24), gapX = D(12), gapY = D(8);
    // 内容区 288 宽内均布 3 列
    int totalW = D(288);
    int cellW = (totalW - 2 * gapX) / 3;
    for (int i = 0; i < n; i++) {
        int col = i % 3, row = i / 3;
        int x = gx + col * (cellW + gapX);
        int y = gy + row * (cellH + gapY);
        // label 由 WM_PAINT 静态画, 输入框宽 52 靠右
        SetWindowPos(s_edits[i], nullptr, x + cellW - D(52), y, D(52), cellH, SWP_NOZORDER);
    }
    int noteY = gy + 4 * (cellH + gapY) + D(8);
    SetWindowPos(s_note, nullptr, gx, noteY, D(288), D(32), SWP_NOZORDER);

    int fy = D(DH) - D(44);
    int bw = D(76), bh = D(28), by = fy + (D(44) - bh) / 2;
    HWND bRestore = GetDlgItem(hwnd, 201);
    HWND bCancel  = GetDlgItem(hwnd, 202);
    SetWindowPos(bRestore, nullptr, D(14), by, bw, bh, SWP_NOZORDER);
    SetWindowPos(bCancel,  nullptr, D(DW) - D(14) - bw * 2 - D(8), by, bw, bh, SWP_NOZORDER);
    SetWindowPos(s_ok,     nullptr, D(DW) - D(14) - bw, by, bw, bh, SWP_NOZORDER);
}

static LRESULT CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        s_fText  = MakeFont(12, FW_NORMAL);
        s_fTitle = MakeFont(13, FW_SEMIBOLD);
        for (int i = 0; i < 11; i++) {
            s_edits[i] = CreateWindowExW(0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_CENTER,
                0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)(100 + i), g.inst, nullptr);
            SendMessageW(s_edits[i], WM_SETFONT, (WPARAM)s_fText, TRUE);
            SendMessageW(s_edits[i], EM_SETLIMITTEXT, 3, 0);
        }
        s_note = CreateWindowExW(0, L"STATIC", DEF_NOTE,
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)200, g.inst, nullptr);
        SendMessageW(s_note, WM_SETFONT, (WPARAM)s_fText, TRUE);

        HWND bRestore = CreateWindowExW(0, L"BUTTON", L"恢复默认",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)201, g.inst, nullptr);
        HWND bCancel = CreateWindowExW(0, L"BUTTON", L"取消",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)202, g.inst, nullptr);
        s_ok = CreateWindowExW(0, L"BUTTON", L"确定",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)203, g.inst, nullptr);

        // 填充当前档位 (不足 11 个用默认值补齐位置)
        for (int i = 0; i < 11; i++) {
            wchar_t t[8];
            int v = i < (int)g.presets.size() ? g.presets[i] : (i * 5 <= 50 ? i * 5 : 50);
            wsprintfW(t, L"%d", v);
            SetWindowTextW(s_edits[i], t);
            s_bad[i] = false;
        }
        DlgLayout(hwnd);
        s_dlgSeenFg = false;
        s_closingOutside = false;
        SetTimer(hwnd, 1, 250, nullptr);   // 轮询前台窗口: 点弹框外即关闭
        return 0;
    }

    case WM_TIMER:
        if (wp == 1) {
            if (GetForegroundWindow() == hwnd) s_dlgSeenFg = true;   // 先确认拿到前台
            else if (s_dlgSeenFg) {                                  // 之后失焦 = 点了外部
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

        // 标题栏
        Txt(dc, L"自定义快捷档位", s_fTitle, g.th.text,
            { D(14), 0, D(200), D(34) }, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        HPEN p = CreatePen(PS_SOLID, 1, g.th.sep);
        HPEN op = (HPEN)SelectObject(dc, p);
        MoveToEx(dc, 0, D(34), nullptr); LineTo(dc, cr.right, D(34));
        int fy = D(DH) - D(44);
        MoveToEx(dc, 0, fy, nullptr); LineTo(dc, cr.right, fy);
        SelectObject(dc, op); DeleteObject(p);

        // 行标签 1..11
        HFONT of = (HFONT)SelectObject(dc, s_fText);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, g.th.dim);
        for (int i = 0; i < 11; i++) {
            RECT er; GetWindowRect(s_edits[i], &er);
            MapWindowPoints(nullptr, hwnd, (POINT*)&er, 2);
            RECT lr = { er.left - D(19), er.top, er.left - D(5), er.bottom };
            wchar_t t[8]; wsprintfW(t, L"%d", i + 1);
            DrawTextW(dc, t, -1, &lr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
        SelectObject(dc, of);
        // 非法输入红框
        for (int i = 0; i < 11; i++) {
            if (!s_bad[i]) continue;
            RECT er; GetWindowRect(s_edits[i], &er);
            MapWindowPoints(nullptr, hwnd, (POINT*)&er, 2);
            InflateRect(&er, 1, 1);
            HBRUSH rb = CreateSolidBrush(RGB(0xE8, 0x11, 0x23));
            FrameRect(dc, &er, rb);
            DeleteObject(rb);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        static HBRUSH s_br = nullptr;
        static COLORREF s_brc = 0;
        HDC dc = (HDC)wp;
        SetTextColor(dc, (HWND)lp == s_note ? g.th.dim : g.th.text);
        SetBkColor(dc, g.th.bg);
        if (!s_br || s_brc != g.th.bg) {
            if (s_br) DeleteObject(s_br);
            s_br = CreateSolidBrush(g.th.bg);
            s_brc = g.th.bg;
        }
        return (LRESULT)s_br;
    }

    case WM_DRAWITEM: {  // 三个按钮统一自绘: 中性按钮 = 主面板同款; 确定 = 主按钮 (#0078D4)
        DRAWITEMSTRUCT* di = (DRAWITEMSTRUCT*)lp;
        int id = (int)di->CtlID;
        if (id != 201 && id != 202 && id != 203) break;
        bool primary = (id == 203);
        bool pressed = (di->itemState & ODS_SELECTED) != 0;
        RECT r = di->rcItem;
        HBRUSH bg = CreateSolidBrush(g.th.bg);   // owner-draw 不自动擦背景
        FillRect(di->hDC, &r, bg);
        DeleteObject(bg);
        COLORREF fill, bd;
        if (primary) {
            fill = pressed ? RGB(0, 0x5F, 0xA8) : ACCENT;
            bd = fill;
        } else {
            fill = pressed ? g.th.track : g.th.btnBg;
            bd = g.th.btnBorder;
        }
        RoundBtn(di->hDC, r, D(3), fill, bd);
        const wchar_t* label = id == 201 ? L"恢复默认" : id == 202 ? L"取消" : L"确定";
        Txt(di->hDC, label, s_fText, primary ? RGB(0xFF, 0xFF, 0xFF) : g.th.text, r,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return TRUE;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == 201) {  // 恢复默认
            for (int i = 0; i < 11; i++) {
                wchar_t t[8]; wsprintfW(t, L"%d", i * 5);
                SetWindowTextW(s_edits[i], t);
                s_bad[i] = false;
            }
            SetWindowTextW(s_note, DEF_NOTE);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (id == 202) { DestroyWindow(hwnd); return 0; }   // 取消
        if (id == 203) {                                     // 确定
            std::vector<int> v;
            bool bad = false;
            for (int i = 0; i < 11; i++) {
                wchar_t t[8] = {};
                GetWindowTextW(s_edits[i], t, 8);
                if (!t[0]) { s_bad[i] = true; bad = true; continue; }
                int x = _wtoi(t);
                s_bad[i] = x < 0 || x > 100;
                if (s_bad[i]) bad = true; else v.push_back(x);
            }
            if (bad) {
                SetWindowTextW(s_note, L"请修正红色输入框：仅接受 0–100 整数。");
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            // 升序 + 去重
            for (size_t i = 0; i < v.size(); i++)
                for (size_t j = i + 1; j < v.size(); j++)
                    if (v[j] < v[i]) { int t2 = v[i]; v[i] = v[j]; v[j] = t2; }
            std::vector<int> out;
            for (int x : v) {
                bool dup = false;
                for (int y : out) if (y == x) { dup = true; break; }
                if (!dup) out.push_back(x);
            }
            if (!out.empty()) {
                g.presets = out;
                ConfigSavePresets();
                MainWndLayout();   // 主面板热区重建
                InvalidateRect(g.hwnd, nullptr, FALSE);
            }
            DestroyWindow(hwnd);
            return 0;
        }
        return 0;
    }

    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        if (s_fText) { DeleteObject(s_fText); s_fText = nullptr; }
        if (s_fTitle) { DeleteObject(s_fTitle); s_fTitle = nullptr; }
        g.dlg = nullptr;
        if (s_closingOutside) {
            // 用户点的是弹框外的其它程序: 只关弹框, 面板继续留在屏幕上;
            // 但面板此刻未获焦点, 收不到系统失焦通知, 需要自己盯住下一次外部点击
            MainWndArmOutsideClickHide();
        } else {
            SetForegroundWindow(g.hwnd);   // 点面板/确定/取消: 把焦点还给面板
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void PresetDlgOpen() {
    if (g.dlg) { SetForegroundWindow(g.dlg); return; }
    KillTimer(g.hwnd, IDT_OUTCLICK);   // 弹框期间由弹框自己判断外部点击

    WNDCLASSW wc = {};
    static bool reg = false;
    if (!reg) {
        wc.lpfnWndProc = DlgProc;
        wc.hInstance = g.inst;
        wc.lpszClassName = DLG_CLASS;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
        reg = true;
    }

    RECT mr; GetWindowRect(g.hwnd, &mr);
    int W = D(DW), H = D(DH);
    int x = mr.left + (mr.right - mr.left - W) / 2;
    int y = mr.top + (mr.bottom - mr.top - H) / 2;
    RECT ar; SystemParametersInfoW(SPI_GETWORKAREA, 0, &ar, 0);
    if (x + W > ar.right) x = ar.right - W;
    if (y + H > ar.bottom) y = ar.bottom - H;
    if (x < ar.left) x = ar.left;
    if (y < ar.top) y = ar.top;

    g.dlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        DLG_CLASS, L"自定义快捷档位", WS_POPUP,
        x, y, W, H, g.hwnd, nullptr, g.inst, nullptr);
    if (!g.dlg) return;
    ShowWindow(g.dlg, SW_SHOW);
    SetForegroundWindow(g.dlg);
}
