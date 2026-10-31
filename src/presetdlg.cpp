// presetdlg.cpp - 自定义快捷档位对话框 320x168 (10 个输入框, 5 列 2 行, 上限可选 50/100)
#include "app.h"
#include <stdlib.h>

static const int DW = 320, DH = 168;

static HWND   s_edits[10];
static HWND   s_note, s_ok;
static HFONT  s_fText, s_fTitle;
static bool   s_bad[10];
static bool   s_dlgSeenFg = false;       // 弹框是否曾拿到前台 (之后失焦即视为点了外部)
static bool   s_closingOutside = false;  // 本次是由"点弹框外部"导致的关闭

static int D(int v) { return MulDiv(v, (int)g.dpi, 96); }

// 上限对应的默认序列: 50 -> 5,10,...,50 ; 100 -> 10,20,...,100
static int DefVal(int i) { return (i + 1) * (g.presetMax / 10); }

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

static void FillDefaults(HWND hwnd) {
    for (int i = 0; i < 10; i++) {
        wchar_t t[8];
        wsprintfW(t, L"%d", DefVal(i));
        SetWindowTextW(s_edits[i], t);
        s_bad[i] = false;
    }
    SetWindowTextW(s_note, L"");
    InvalidateRect(hwnd, nullptr, FALSE);
}

static void DlgLayout(HWND hwnd) {
    int gx = D(19), top = D(46);
    int gapX = D(8), gapY = D(8), cellH = D(24);
    int labelW = D(16), editW = D(36);     // label 只放序号 1-10, 尽量窄
    int cellW = labelW + editW;
    for (int i = 0; i < 10; i++) {
        int col = i % 5, row = i / 5;
        int x = gx + col * (cellW + gapX);
        int y = top + row * (cellH + gapY);
        SetWindowPos(s_edits[i], nullptr, x + labelW, y, editW, cellH, SWP_NOZORDER);
    }

    int noteY = top + 2 * (cellH + gapY) + D(6);
    SetWindowPos(s_note, nullptr, gx, noteY, D(DW) - gx * 2, D(18), SWP_NOZORDER);

    int fy = D(DH) - D(44);
    int bw = D(76), bh = D(28), by = fy + (D(44) - bh) / 2;
    SetWindowPos(GetDlgItem(hwnd, 201), nullptr, D(14), by, bw, bh, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hwnd, 202), nullptr, D(DW) - D(14) - bw * 2 - D(8), by, bw, bh, SWP_NOZORDER);
    SetWindowPos(s_ok, nullptr, D(DW) - D(14) - bw, by, bw, bh, SWP_NOZORDER);

    // 标题栏右侧: 最大值 50 / 100 单选
    int rbW = D(44), rbH = D(20), rbY = (D(34) - rbH) / 2;
    SetWindowPos(GetDlgItem(hwnd, 211), nullptr, D(216), rbY, rbW, rbH, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hwnd, 212), nullptr, D(216) + rbW, rbY, rbW, rbH, SWP_NOZORDER);
}

static LRESULT CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        s_fText  = MakeFont(12, FW_NORMAL);
        s_fTitle = MakeFont(13, FW_SEMIBOLD);
        for (int i = 0; i < 10; i++) {
            s_edits[i] = CreateWindowExW(0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_CENTER,
                0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)(100 + i), g.inst, nullptr);
            SendMessageW(s_edits[i], WM_SETFONT, (WPARAM)s_fText, TRUE);
            SendMessageW(s_edits[i], EM_SETLIMITTEXT, 3, 0);
        }
        // 平时为空, 只有输入非法时才显示错误提示
        s_note = CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)200, g.inst, nullptr);
        SendMessageW(s_note, WM_SETFONT, (WPARAM)s_fText, TRUE);

        CreateWindowExW(0, L"BUTTON", L"恢复默认",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)201, g.inst, nullptr);
        CreateWindowExW(0, L"BUTTON", L"取消",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)202, g.inst, nullptr);
        s_ok = CreateWindowExW(0, L"BUTTON", L"确定",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)203, g.inst, nullptr);
        CreateWindowExW(0, L"BUTTON", L"50",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)211, g.inst, nullptr);
        CreateWindowExW(0, L"BUTTON", L"100",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)212, g.inst, nullptr);

        // 填充当前档位 (不足 10 个用当前上限的默认值补齐)
        for (int i = 0; i < 10; i++) {
            wchar_t t[8];
            int v = i < (int)g.presets.size() ? g.presets[i] : DefVal(i);
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

        // 标题栏 (右侧留给"最大值: 50 / 100")
        Txt(dc, L"自定义快捷档位", s_fTitle, g.th.text,
            { D(14), 0, D(150), D(34) }, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        Txt(dc, L"最大值", s_fText, g.th.dim,
            { D(150), 0, D(212), D(34) }, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        HPEN p = CreatePen(PS_SOLID, 1, g.th.sep);
        HPEN op = (HPEN)SelectObject(dc, p);
        MoveToEx(dc, 0, D(34), nullptr); LineTo(dc, cr.right, D(34));
        int fy = D(DH) - D(44);
        MoveToEx(dc, 0, fy, nullptr); LineTo(dc, cr.right, fy);
        SelectObject(dc, op); DeleteObject(p);

        // 行标签 1..10 (窄, 紧贴输入框)
        HFONT of = (HFONT)SelectObject(dc, s_fText);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, g.th.dim);
        for (int i = 0; i < 10; i++) {
            RECT er; GetWindowRect(s_edits[i], &er);
            MapWindowPoints(nullptr, hwnd, (POINT*)&er, 2);
            RECT lr = { er.left - D(15), er.top, er.left - D(4), er.bottom };
            wchar_t t[8]; wsprintfW(t, L"%d", i + 1);
            DrawTextW(dc, t, -1, &lr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
        SelectObject(dc, of);
        // 非法输入红框
        for (int i = 0; i < 10; i++) {
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
        SetTextColor(dc, (HWND)lp == s_note ? RGB(0xE8, 0x11, 0x23) : g.th.text);
        SetBkColor(dc, g.th.bg);
        if (!s_br || s_brc != g.th.bg) {
            if (s_br) DeleteObject(s_br);
            s_br = CreateSolidBrush(g.th.bg);
            s_brc = g.th.bg;
        }
        return (LRESULT)s_br;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* di = (DRAWITEMSTRUCT*)lp;
        int id = (int)di->CtlID;
        RECT r = di->rcItem;
        bool pressed = (di->itemState & ODS_SELECTED) != 0;
        HBRUSH bg = CreateSolidBrush(g.th.bg);   // owner-draw 不自动擦背景
        FillRect(di->hDC, &r, bg);
        DeleteObject(bg);

        if (id == 211 || id == 212) {   // 最大值单选
            bool on = g.presetMax == (id == 211 ? 50 : 100);
            int cx = r.left + D(7), cy = (r.top + r.bottom) / 2, rr = D(6);
            HPEN p = CreatePen(PS_SOLID, 1, on ? ACCENT : g.th.btnBorder);
            HBRUSH b = CreateSolidBrush(g.th.bg);
            HPEN op = (HPEN)SelectObject(di->hDC, p);
            HBRUSH ob = (HBRUSH)SelectObject(di->hDC, b);
            Ellipse(di->hDC, cx - rr, cy - rr, cx + rr, cy + rr);
            SelectObject(di->hDC, op); SelectObject(di->hDC, ob);
            DeleteObject(p); DeleteObject(b);
            if (on) {
                HBRUSH dot = CreateSolidBrush(ACCENT);
                HBRUSH od = (HBRUSH)SelectObject(di->hDC, dot);
                Ellipse(di->hDC, cx - rr + D(2), cy - rr + D(2), cx + rr - D(2), cy + rr - D(2));
                SelectObject(di->hDC, od);
                DeleteObject(dot);
            }
            r.left += D(18);
            Txt(di->hDC, id == 211 ? L"50" : L"100", s_fText, g.th.text, r,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        if (id != 201 && id != 202 && id != 203) break;

        bool primary = (id == 203);
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
        if (id == 211 || id == 212) {   // 切换上限: 重新填入该上限的默认序列
            g.presetMax = (id == 211 ? 50 : 100);
            FillDefaults(hwnd);
            return 0;
        }
        if (id == 201) {  // 恢复默认
            FillDefaults(hwnd);
            return 0;
        }
        if (id == 202) { DestroyWindow(hwnd); return 0; }   // 取消
        if (id == 203) {                                     // 确定
            std::vector<int> v;
            bool bad = false;
            for (int i = 0; i < 10; i++) {
                wchar_t t[8] = {};
                GetWindowTextW(s_edits[i], t, 8);
                if (!t[0]) { s_bad[i] = true; bad = true; continue; }
                int x = _wtoi(t);
                s_bad[i] = x < 1 || x > g.presetMax;
                if (s_bad[i]) bad = true; else v.push_back(x);
            }
            if (bad) {
                wchar_t msg[64];
                wsprintfW(msg, L"请修正红色输入框：仅接受 1–%d 整数。", g.presetMax);
                SetWindowTextW(s_note, msg);
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
                PresetsBuildButtons();   // 主面板 = 内置 0 + 自定义
                MainWndLayout();
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
