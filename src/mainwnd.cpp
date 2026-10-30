// mainwnd.cpp - 主面板 380x176 逻辑像素, 全自绘 (按 mockups/wireframe.html)
#include "app.h"
#include <math.h>
#include <shellapi.h>
#include <windowsx.h>

// ---- 逻辑尺寸 (标题 / 滑块 / 刻度 / 档位 / 底部, 上下留白 12) ----
static const int LW = 380, LH = 176;
static const int PAD = 12;
static const int HDR_Y = 12, HDR_H = 26;          // 标题行: 太阳 + 名称 + 当前亮度 (同一水平线)
static const int TRACK_X = 20, TRACK_W = 340, TRACK_Y = 58, TRACK_H = 4;
static const int THUMB_W = 14, THUMB_H = 26;       // 竖条控制柄: 中心对齐轨道
static const int TICK_Y = 76, TICK_H = 4;
static const int LBL_Y = 82, LBL_H = 14;
static const int PRESET_Y = 104, PRESET_H = 24, PRESET_W = 28;
static const int BOT_Y = 138, BOT_H = 26;
static const int SW_W = 36, SW_H = 18;

static HFONT fName, fPct, fLabel, fBtn, fText, fHint, fMenu;
static bool g_unsupportedShown = false;

int S(int v) { return MulDiv(v, (int)g.dpi, 96); }

HFONT MakeFont(int px, int weight) {
    return CreateFontW(-S(px), 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

static void RecreateFonts() {
    for (HFONT* f : { &fName, &fPct, &fLabel, &fBtn, &fText, &fHint, &fMenu })
        if (*f) { DeleteObject(*f); *f = nullptr; }
    fName  = MakeFont(13, FW_NORMAL);
    fPct   = MakeFont(26, FW_SEMIBOLD);
    fLabel = MakeFont(10, FW_NORMAL);
    fBtn   = MakeFont(11, FW_NORMAL);
    fText  = MakeFont(12, FW_NORMAL);
    fHint  = MakeFont(11, FW_NORMAL);
    fMenu  = MakeFont(12, FW_NORMAL);
}

void AppReadTheme() {
    HKEY k; DWORD v = 1, cb = sizeof(v);
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_READ, &k) == ERROR_SUCCESS) {
        RegQueryValueExW(k, L"AppsUseLightTheme", nullptr, nullptr, (LPBYTE)&v, &cb);
        RegCloseKey(k);
    }
    g.lightTheme = v != 0;
    if (g.lightTheme)
        g.th = { true, RGB(0xF9,0xF9,0xF9), RGB(0xE0,0xE0,0xE0), RGB(0x1B,0x1B,0x1B),
                 RGB(0x90,0x90,0x90), RGB(0xDC,0xDC,0xDC), RGB(0xC4,0xC4,0xC4),
                 RGB(0xFF,0xFF,0xFF), RGB(0xC9,0xC9,0xC9),
                 RGB(0xE2,0xE2,0xE2) };
    else
        g.th = { false, RGB(0x20,0x20,0x20), RGB(0x3A,0x3A,0x3A), RGB(0xEC,0xEC,0xEC),
                 RGB(0x7E,0x7E,0x7E), RGB(0x45,0x45,0x45), RGB(0x5A,0x5A,0x5A),
                 RGB(0x2B,0x2B,0x2B), RGB(0x4A,0x4A,0x4A),
                 RGB(0x45,0x45,0x45) };
}

// ---- 绘制辅助 ----
static void RR(HDC dc, const RECT& r, int rad, COLORREF fill, COLORREF border) {
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = border ? CreatePen(PS_SOLID, 1, border) : (HPEN)GetStockObject(NULL_PEN);
    HBRUSH ob = (HBRUSH)SelectObject(dc, b);
    HPEN op = (HPEN)SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, rad, rad);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b); if (border) DeleteObject(p);
}

void Txt(HDC dc, const wchar_t* s, HFONT f, COLORREF c, RECT r, UINT fmt) {
    HFONT of = (HFONT)SelectObject(dc, f);
    SetTextColor(dc, c);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, s, -1, &r, fmt);
    SelectObject(dc, of);
}
#define Text Txt

// 单色小太阳 (自绘, 颜色随主题)
static void DrawSun(HDC dc, int x, int y, int size, COLORREF c) {
    int pw = S(13) / 10; if (pw < 1) pw = 1;
    HPEN p = CreatePen(PS_SOLID, pw, c);
    HPEN op = (HPEN)SelectObject(dc, p);
    HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
    double cx = x + size / 2.0, cy = y + size / 2.0, u = size / 16.0;
    Ellipse(dc, (int)(cx - 3 * u + 0.5), (int)(cy - 3 * u + 0.5),
                (int)(cx + 3 * u + 0.5), (int)(cy + 3 * u + 0.5));
    for (int i = 0; i < 8; i++) {
        double a = i * 3.14159265358979 / 4.0;
        double ca = cos(a), sa = sin(a);
        MoveToEx(dc, (int)(cx + ca * 5 * u + 0.5), (int)(cy + sa * 5 * u + 0.5), nullptr);
        LineTo(dc, (int)(cx + ca * 7 * u + 0.5), (int)(cy + sa * 7 * u + 0.5));
    }
    SelectObject(dc, op); SelectObject(dc, ob);
    DeleteObject(p);
}

static int TrackValFromX(int mx) {
    int tx = S(TRACK_X), tw = S(TRACK_W);
    int v = (int)lround((mx - tx) * 100.0 / tw);
    return v < 0 ? 0 : v > 100 ? 100 : v;
}

static int ThumbX(int v) { return S(TRACK_X) + (int)lround(v * S(TRACK_W) / 100.0); }

// ---- 布局热区 ----
void MainWndLayout() {
    // 滑块热区覆盖整根竖条 (上下各留 4px, 左右各放宽 12px): 条上任意位置都能按下拖动
    int tcy = S(TRACK_Y) + S(TRACK_H) / 2;
    SetRect(&g.rcSlider, S(TRACK_X) - S(12), tcy - S(THUMB_H) / 2 - S(4),
            S(TRACK_X) + S(TRACK_W) + S(12), tcy + S(THUMB_H) / 2 + S(4));
    g.rcPreset.clear();
    int n = (int)g.presets.size();
    int avail = S(356), bw = S(PRESET_W);
    int gap = n > 1 ? (avail - n * bw) / (n - 1) : 0;
    for (int i = 0; i < n; i++) {
        int x = S(PAD) + i * (bw + gap);
        RECT r; SetRect(&r, x, S(PRESET_Y), x + bw, S(PRESET_Y) + S(PRESET_H));
        g.rcPreset.push_back(r);
    }
    // 底部按钮按文字宽度 (DrawText DT_CALCRECT 测量)
    HDC dc = GetDC(g.hwnd);
    HFONT of = (HFONT)SelectObject(dc, fText);
    auto measure = [&](const wchar_t* s) -> int {
        RECT mr = {};
        DrawTextW(dc, s, -1, &mr, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
        return mr.right;
    };
    int t2 = measure(L"自定义");
    int t3 = measure(L"开机自启");
    int w2 = t2 + S(20);
    SetRect(&g.rcCustom, S(PAD), S(BOT_Y), S(PAD) + w2, S(BOT_Y) + S(BOT_H));
    int gw = t3 + S(7) + S(SW_W);
    SetRect(&g.rcSwitch, S(LW) - S(PAD) - gw, S(BOT_Y), S(LW) - S(PAD), S(BOT_Y) + S(BOT_H));
    SelectObject(dc, of);
    ReleaseDC(g.hwnd, dc);
}

// ---- 主绘制 ----
static void Paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT cr; GetClientRect(hwnd, &cr);
    int W = cr.right, H = cr.bottom;

    HDC m = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, W, H);
    HBITMAP ob = (HBITMAP)SelectObject(m, bmp);

    HBRUSH bg = CreateSolidBrush(g.th.bg);
    FillRect(m, &cr, bg);
    DeleteObject(bg);

    bool en = g.supported;
    COLORREF dimAll = g.th.dim;   // 禁用态统一灰

    // 标题行: 太阳 + 名称 + 当前亮度 (三者同一水平中线)
    DrawSun(m, S(12), S(HDR_Y) + S(5), S(16), en ? (g.lightTheme ? RGB(0x55,0x55,0x55) : RGB(0xBB,0xBB,0xBB)) : dimAll);
    RECT r = { S(36), S(HDR_Y), S(220), S(HDR_Y) + S(HDR_H) };
    Text(m, L"内置显示器", fName, en ? g.th.text : dimAll, r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    wchar_t pct[16];
    wsprintfW(pct, L"%d%%", g.curValue);
    r = { S(220), S(HDR_Y), S(368), S(HDR_Y) + S(HDR_H) };
    Text(m, pct, fPct, en ? ACCENT : dimAll, r, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    // 滑块: 轨道 / 填充 / 竖条控制柄 (任意位置都可按下拖动)
    int ty = S(TRACK_Y), th = S(TRACK_H);
    RECT tr = { S(TRACK_X), ty, S(TRACK_X) + S(TRACK_W), ty + th };
    RR(m, tr, th, en ? g.th.track : dimAll, 0);
    int tx = ThumbX(g.dragging ? g.sliderVal : g.curValue);
    if (en) {
        RECT fr = { tr.left, tr.top, tx, tr.bottom };
        if (fr.right > fr.left) RR(m, fr, th, ACCENT, 0);
    }
    int hw = S(THUMB_W) / 2, hh = S(THUMB_H) / 2, tcy = ty + th / 2;
    RECT tb = { tx - hw, tcy - hh, tx + hw, tcy + hh };
    RR(m, tb, hw, en ? ACCENT : dimAll, 0);

    // 刻度 (每 5%, 共 21 条) + 数字标注
    HBRUSH tk = CreateSolidBrush(g.th.tick);
    for (int i = 0; i <= 20; i++) {
        int x = S(TRACK_X) + (int)lround(i * S(TRACK_W) / 20.0);
        RECT r2 = { x, S(TICK_Y), x + 1, S(TICK_Y) + S(TICK_H) };
        FillRect(m, &r2, tk);
    }
    DeleteObject(tk);
    const wchar_t* lbt[5] = { L"0", L"25", L"50", L"75", L"100" };
    if (en) {
        for (int i = 0; i < 5; i++) {
            int cx = S(TRACK_X) + i * S(TRACK_W) / 4;
            RECT r3 = { cx - S(20), S(LBL_Y), cx + S(20), S(LBL_Y) + S(LBL_H) };
            Text(m, lbt[i], fLabel, g.th.dim, r3, DT_CENTER | DT_SINGLELINE);  // 0/100 与刻度居中对齐
        }
        if (g.hwFloorNoted) {
            wchar_t nb[64];
            wsprintfW(nb, L"硬件最低亮度 %d%%", g.hwFloor);
            RECT r3 = { S(170), S(LBL_Y), S(346), S(LBL_Y) + S(LBL_H) };
            Text(m, nb, fLabel, g.th.dim, r3, DT_RIGHT | DT_SINGLELINE);
        }
    } else {
        Text(m, L"当前设备不支持亮度调节", fHint, dimAll,
            { S(PAD), S(LBL_Y), S(LW) - S(PAD), S(LBL_Y) + S(LBL_H) },
            DT_CENTER | DT_SINGLELINE);
    }

    // 快捷档位按钮
    for (size_t i = 0; i < g.presets.size(); i++) {
        bool sel = en && g.presets[i] == g.curValue;
        RR(m, g.rcPreset[i], S(3),
            !en ? g.th.btnBg : sel ? ACCENT : g.th.btnBg,
            !en ? g.th.btnBorder : sel ? ACCENT : g.th.btnBorder);
        wchar_t t[8]; wsprintfW(t, L"%d", g.presets[i]);
        Text(m, t, fBtn, !en ? dimAll : sel ? RGB(0xFF,0xFF,0xFF) : g.th.text,
            g.rcPreset[i], DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // 不支持提示 (已画在刻度行)

    // 底部: 自定义 / 开机自启
    RR(m, g.rcCustom, S(3), g.th.btnBg, g.th.btnBorder);
    Text(m, L"自定义", fText, g.th.text, g.rcCustom, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    int sy = S(BOT_Y) + (S(BOT_H) - S(SW_H)) / 2;
    RECT swr = { g.rcSwitch.right - S(SW_W), sy, g.rcSwitch.right, sy + S(SW_H) };
    RR(m, swr, S(SW_H), g.autorun ? ACCENT : (g.lightTheme ? RGB(0xDC,0xDC,0xDC) : RGB(0x5A,0x5A,0x5A)), 0);
    int kr = S(7), kx = g.autorun ? swr.right - S(2) - kr : swr.left + S(2) + kr;
    HBRUSH kb = CreateSolidBrush(RGB(0xFF,0xFF,0xFF));
    HBRUSH okb = (HBRUSH)SelectObject(m, kb);
    HPEN np = (HPEN)SelectObject(m, GetStockObject(NULL_PEN));
    Ellipse(m, kx - kr, sy + S(SW_H) / 2 - kr, kx + kr, sy + S(SW_H) / 2 + kr);
    SelectObject(m, okb); SelectObject(m, np);
    DeleteObject(kb);
    RECT ar = { g.rcSwitch.left, g.rcSwitch.top, swr.left - S(7), g.rcSwitch.bottom };
    Text(m, L"开机自启", fText, g.lightTheme ? RGB(0x44,0x44,0x44) : RGB(0xCC,0xCC,0xCC),
        ar, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    // 外边框 (1px)
    HBRUSH brd = CreateSolidBrush(g.th.border);
    FrameRect(m, &cr, brd);
    DeleteObject(brd);

    BitBlt(dc, 0, 0, W, H, m, 0, 0, SRCCOPY);
    SelectObject(m, ob);
    DeleteObject(bmp); DeleteDC(m);
    EndPaint(hwnd, &ps);
}

// ---- 亮度下发 (记录回声抑制状态; 期望值 = 回读映射值, 0% -> hwFloor) ----
static void DoWrite(int logical) {
    if (!g.supported) return;
    if (BrightWrite(logical)) {
        g.lastWritten = BrightRawToLogical(logical == 0 ? g.hwFloor : logical);
        g.lastWriteTick = GetTickCount();
    }
}

// 外部入口: 立即设置亮度 (按钮/菜单/预设)
void MainWndApplyValue(int v, bool write) {
    if (v < 0) v = 0; if (v > 100) v = 100;
    g.curValue = v; g.sliderVal = v; g.chaseVal = (double)v;
    if (write) {
        DoWrite(v);
        BrightSyncSystem(v);   // 同时更新系统自己的亮度状态, 保证与面板一致
        ConfigSaveState();
    }
    TrayUpdateTip();
    if (g.hwnd) InvalidateRect(g.hwnd, nullptr, FALSE);
}

// ---- 唤醒后写回自建值 ----
// Windows 睡眠唤醒时会按电源计划里的"显示器亮度"(本机固定 50%)重新下发一次,
// 直接覆盖掉我们在 WMI 层设定的值。这段时间内不能回读同步 (那会把被覆盖的值
// 当成"用户改动"采纳), 而是等显示器就绪后把我们的值写回, 并复查一次。
static bool s_resumeGuard = false;   // 唤醒窗口内: 暂停回读同步
static int  s_resumeRetry = 0;       // 剩余写回次数

// 回读同步 (无回声写入)
void MainWndRefreshPoll() {
    if (!g.supported) return;
    if (s_resumeGuard) return;
    int raw = BrightRead();
    if (raw < 0) return;
    int v = BrightRawToLogical(raw);
    if (g.lastWritten >= 0) {
        if (v == g.lastWritten) g.lastWritten = -1;            // 我们自己的写入已生效
        else if (GetTickCount() - g.lastWriteTick < 1200) return; // 抑制回声
    }
    if (v != g.curValue && !g.dragging) {
        g.curValue = v; g.sliderVal = v; g.chaseVal = (double)v;
        TrayUpdateTip();
        InvalidateRect(g.hwnd, nullptr, FALSE);
    }
}

// 最近一次"因失焦自动收起"的时刻: 用于区分"点击托盘导致的失焦"和"用户想再点开"
static DWORD s_autoHideTick = 0;
// 用户是否手动拖动过面板: 只有手动移动过的位置才记住, 否则始终默认贴右下角
static bool s_userMoved = false;
// "面板可见但未获焦点"期间的左键状态: 只在按下那一刻判定一次, 避免长按误判
static bool s_mouseWasDown = false;

static void HideToTray() {
    if (!g.hwnd) return;
    KillTimer(g.hwnd, IDT_HIDE);   // 主动收起后撤销待执行的失焦收起
    KillTimer(g.hwnd, IDT_OUTCLICK);
    ConfigSaveState();
    ShowWindow(g.hwnd, SW_HIDE);
}

// 弹框因"点了弹框外部"而关闭后, 面板处于可见但未获焦点的状态:
// 此时系统不会再给面板发 WM_ACTIVATE(WA_INACTIVE), 只能自己盯住下一次左键按下
void MainWndArmOutsideClickHide() {
    if (!g.hwnd) return;
    s_mouseWasDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    SetTimer(g.hwnd, IDT_OUTCLICK, 40, nullptr);
}

void MainWndShow(bool show) {
    if (!g.hwnd) return;
    if (!show) { HideToTray(); return; }
    if (IsWindowVisible(g.hwnd)) { HideToTray(); return; }  // 再次点击 = 收起
    int W = S(LW), H = S(LH);
    RECT ar; SystemParametersInfoW(SPI_GETWORKAREA, 0, &ar, 0);
    int x, y;
    if (g.hasWinPos) {
        x = g.winPos.x; y = g.winPos.y;   // 用户手动拖过的位置
    } else {
        x = ar.right - W - S(8);          // 默认贴屏幕右下角 (托盘上方)
        y = ar.bottom - H - S(8);
    }
    if (x + W > ar.right) x = ar.right - W;
    if (y + H > ar.bottom) y = ar.bottom - H;
    if (x < ar.left) x = ar.left;
    if (y < ar.top) y = ar.top;
    SetWindowPos(g.hwnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    ShowWindow(g.hwnd, SW_SHOW);
    SetWindowPos(g.hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
    SetForegroundWindow(g.hwnd);
    InvalidateRect(g.hwnd, nullptr, FALSE);
}

void MainWndRecreateLayout() {
    RecreateFonts();
    MainWndLayout();
    int W = S(LW), H = S(LH);
    HRGN rgn = CreateRoundRectRgn(0, 0, W + 1, H + 1, S(8), S(8));
    SetWindowRgn(g.hwnd, rgn, TRUE);   // 小圆角
    SetWindowPos(g.hwnd, nullptr, 0, 0, W, H, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(g.hwnd, nullptr, FALSE);
}

// ---- 菜单 owner-draw (右键菜单自绘, 208 宽, 项高 26) ----
static void MenuMeasure(MEASUREITEMSTRUCT* mi) {
    mi->itemWidth = S(208);
    mi->itemHeight = (int)mi->itemData == MI_SEP_DATA ? S(9) : S(26);
}

static void MenuDraw(DRAWITEMSTRUCT* di) {
    HDC dc = di->hDC;
    RECT r = di->rcItem;
    int id = (int)di->itemData;
    COLORREF bgc = g.lightTheme ? RGB(0xF9,0xF9,0xF9) : RGB(0x20,0x20,0x20);
    COLORREF hov = g.lightTheme ? RGB(0xE5,0xF1,0xFB) : RGB(0x33,0x33,0x33);
    COLORREF txc = g.lightTheme ? RGB(0x1B,0x1B,0x1B) : RGB(0xEC,0xEC,0xEC);
    COLORREF dim = RGB(0x9A,0x9A,0x9A);
    bool hovOk = (di->itemState & ODS_SELECTED) && !(di->itemState & ODS_DISABLED) && id != MI_INFO;
    HBRUSH b = CreateSolidBrush(hovOk ? hov : bgc);
    FillRect(dc, &r, b);
    DeleteObject(b);
    if (id == MI_SEP_DATA) {
        HPEN p = CreatePen(PS_SOLID, 1, g.th.sep);
        HPEN op = (HPEN)SelectObject(dc, p);
        int y = (r.top + r.bottom) / 2;
        MoveToEx(dc, r.left + S(8), y, nullptr);
        LineTo(dc, r.right - S(8), y);
        SelectObject(dc, op); DeleteObject(p);
        return;
    }
    wchar_t text[64] = {};
    bool checked = false, disabled = false;
    if (id == MI_INFO) {
        wsprintfW(text, L"当前亮度：%d%%", g.curValue);
        disabled = true;
    } else if (id >= MI_PRESET_BASE && id < MI_PRESET_BASE + 20) {
        int i = id - MI_PRESET_BASE;
        if (i < (int)g.presets.size()) {
            wsprintfW(text, L"%d", g.presets[i]);
            checked = g.supported && g.presets[i] == g.curValue;
            disabled = !g.supported;
        }
    } else if (id == MI_OPEN) lstrcpyW(text, L"打开主界面");
    else if (id == MI_AUTORUN) { lstrcpyW(text, L"开机自启"); checked = g.autorun; }
    else if (id == MI_EXIT) lstrcpyW(text, L"退出");

    if (checked)
        Text(dc, L"✓", fMenu, ACCENT, { r.left, r.top, r.left + S(26), r.bottom },
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    Text(dc, text, fMenu, disabled ? dim : txc,
        { r.left + S(30), r.top, r.right, r.bottom }, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

// ---- 拖动追赶 (ease-out ~250ms, 每 100ms 最多下发一次) ----
static void ChaseTick() {
    double diff = g.sliderVal - g.chaseVal;
    if (fabs(diff) < 0.5) {
        g.chaseVal = g.sliderVal;
    } else {
        g.chaseVal += diff * 0.22;   // 指数 ease-out
    }
    int v = (int)lround(g.chaseVal);
    DWORD now = GetTickCount();
    if (v != g.lastSentValue && now - g.lastSendTick >= 100) {
        DoWrite(v);
        g.lastSendTick = now;
        g.lastSentValue = v;
    }
}

static bool Pt(const RECT& r, POINT p) { return PtInRect(&r, p) != 0; }

// 结束拖动: 保留当前值并下发 (capture 被抢/窗口被隐藏时也走这里, 避免一直追赶)
static void EndDrag(HWND hwnd) {
    if (!g.dragging) return;
    g.dragging = false;
    KillTimer(hwnd, IDT_CHASE);
    g.curValue = g.sliderVal;
    DoWrite(g.sliderVal);
    BrightSyncSystem(g.sliderVal);   // 拖动结束: 系统亮度状态与面板一并落定
    ConfigSaveState();
    TrayUpdateTip();
    InvalidateRect(hwnd, nullptr, FALSE);
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // Explorer 重启: 托盘被 shell 清空, 重新添加图标并恢复提示
    if (msg && msg == TrayTaskbarCreatedMsg()) {
        TrayAdd();
        TrayUpdateTip();
        SetTimer(hwnd, IDT_HINT, 3000, nullptr);   // 重建后同样按需提升到任务栏
        return 0;
    }

    switch (msg) {
    case WM_PAINT: Paint(hwnd); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_NCHITTEST: {   // 顶部空白处可拖动窗口
        POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };   // 屏幕坐标可为负, 必须符号扩展
        ScreenToClient(hwnd, &p);
        if (p.y < S(HDR_Y) + S(HDR_H)) return HTCAPTION;
        return HTCLIENT;
    }

    case WM_LBUTTONDOWN: {
        POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (g.dlg) {   // 弹框打开时点面板任意处 = 关闭弹框
            DestroyWindow(g.dlg);
            return 0;
        }
        if (g.supported && Pt(g.rcSlider, p)) {
            g.dragging = true;
            SetCapture(hwnd);
            g.sliderVal = TrackValFromX(p.x);
            g.curValue = g.sliderVal;
            SetTimer(hwnd, IDT_CHASE, 15, nullptr);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (g.supported) {
            for (size_t i = 0; i < g.rcPreset.size(); i++)
                if (Pt(g.rcPreset[i], p)) { MainWndApplyValue(g.presets[i], true); return 0; }
        }
        if (Pt(g.rcCustom, p)) { PresetDlgOpen(); return 0; }
        if (Pt(g.rcSwitch, p)) {
            AutorunSet(!g.autorun);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        return 0;
    }

    case WM_MOUSEMOVE:
        if (g.dragging) {
            // 拖动时鼠标移出窗口 (客户端坐标为负/超出宽度): 锁定当前值, 不跳变
            POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            RECT cr; GetClientRect(hwnd, &cr);
            if (p.x < 0 || p.x >= cr.right) return 0;
            int v = TrackValFromX(p.x);
            if (v != g.sliderVal) {
                g.sliderVal = v;
                g.curValue = v;          // 百分比即时显示
                TrayUpdateTip();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        return 0;

    case WM_CAPTURECHANGED:   // capture 被抢 (窗口隐藏/弹窗): 按当前值收尾
        EndDrag(hwnd);
        return 0;

    case WM_LBUTTONUP:
        if (g.dragging) {
            EndDrag(hwnd);           // 松手立即下发最终精确值
            ReleaseCapture();
        }
        return 0;

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) { HideToTray(); return 0; }
        return 0;

    case WM_TIMER:
        if (wp == IDT_POLL) { MainWndRefreshPoll(); return 0; }
        if (wp == IDT_CHASE) { if (g.dragging) ChaseTick(); return 0; }
        if (wp == IDT_RESUME) {
            KillTimer(hwnd, IDT_RESUME);
            DoWrite(g.curValue);                       // 写回用户设定的值
            BrightSyncSystem(g.curValue);              // 系统状态也对齐, 免得下次唤醒再被覆盖
            if (--s_resumeRetry > 0) {
                SetTimer(hwnd, IDT_RESUME, 2500, nullptr);   // 显示器/DDC 可能还没完全就绪, 再复查
            } else {
                s_resumeGuard = false;
                MainWndRefreshPoll();                  // 写回后对一次账
            }
            return 0;
        }
        if (wp == IDT_HIDE) {
            KillTimer(hwnd, IDT_HIDE);
            if (IsWindowVisible(hwnd) && GetForegroundWindow() != hwnd && !g.dlg) {
                HideToTray();
                s_autoHideTick = GetTickCount();
            }
            return 0;
        }
        if (wp == IDT_HINT) {
            KillTimer(hwnd, IDT_HINT);
            TrayCheckOverflowHint();
            return 0;
        }
        if (wp == IDT_OUTCLICK) {
            // 面板重新拿到焦点 / 已收起 / 弹框又打开 / 正在拖动: 交给常规失焦路径
            if (!IsWindowVisible(hwnd) || g.dragging || g.dlg ||
                GetForegroundWindow() == hwnd) {
                KillTimer(hwnd, IDT_OUTCLICK);
                return 0;
            }
            bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            if (down && !s_mouseWasDown) {          // 新的一次左键按下
                POINT p; GetCursorPos(&p);
                RECT wr; GetWindowRect(hwnd, &wr);
                if (!Pt(wr, p)) {                   // 按在面板外: 收起
                    KillTimer(hwnd, IDT_OUTCLICK);
                    s_mouseWasDown = down;
                    HideToTray();
                    s_autoHideTick = GetTickCount();
                    return 0;
                }
            }
            s_mouseWasDown = down;
            return 0;
        }
        return 0;

    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE) {
            if (!g.dlg) SetTimer(hwnd, IDT_HIDE, 150, nullptr);
        } else {
            KillTimer(hwnd, IDT_HIDE);
            KillTimer(hwnd, IDT_OUTCLICK);   // 面板已重新获焦, 不再需要盯外部点击
        }
        return 0;

    case WM_POWERBROADCAST:
        if (wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND) {
            if (g.supported) {
                // 系统刚按电源计划值覆盖了亮度: 先停止回读同步, 稍后把我们的值写回
                s_resumeGuard = true;
                s_resumeRetry = 2;
                SetTimer(hwnd, IDT_RESUME, 1500, nullptr);
            }
        }
        return TRUE;

    case WM_SETTINGCHANGE:
        if (lp && lstrcmpW((LPCWSTR)lp, L"ImmersiveColorSet") == 0) {
            AppReadTheme();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_DPICHANGED: {
        g.dpi = LOWORD(wp);
        RECT* nr = (RECT*)lp;
        SetWindowPos(hwnd, nullptr, nr->left, nr->top,
            nr->right - nr->left, nr->bottom - nr->top, SWP_NOZORDER | SWP_NOACTIVATE);
        MainWndRecreateLayout();
        return 0;
    }

    case WM_ENTERSIZEMOVE:   // 用户拖动标题栏: 之后的位置才值得记住
        s_userMoved = true;
        return 0;

    case WM_MOVE:
        if (s_userMoved) {
            RECT wr; GetWindowRect(hwnd, &wr);
            g.winPos.x = wr.left; g.winPos.y = wr.top; g.hasWinPos = true;
        }
        return 0;

    case WM_MEASUREITEM: MenuMeasure((MEASUREITEMSTRUCT*)lp); return TRUE;
    case WM_DRAWITEM:    MenuDraw((DRAWITEMSTRUCT*)lp); return TRUE;

    case WM_APP_TRAY: {
        // NOTIFYICON_VERSION_4: 事件码在 lParam 低字, 图标 ID 在高字
        UINT code = LOWORD(lp);
        // v4 下 NIN_SELECT 与 WM_LBUTTONUP 会同时到达, 只响应后者避免重复 toggle
        if (code == WM_LBUTTONUP) {   // 左键: 切换显示/收起
            if (IsWindowVisible(hwnd)) HideToTray();
            else if (GetTickCount() - s_autoHideTick >= 400) MainWndShow(true);
            // 否则: 面板已被本次点击引起的失焦收起 (150ms 定时器抢先执行), 保持收起
        } else if (code == WM_RBUTTONUP || code == WM_CONTEXTMENU) {
            POINT p; GetCursorPos(&p);
            TrayShowMenu(p);
        }
        return 0;
    }

    case WM_APP_SHOW:   // 单实例: 重复启动唤出 (可见则只置前, 不收起)
        if (IsWindowVisible(hwnd)) SetForegroundWindow(hwnd);
        else MainWndShow(true);
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id >= MI_PRESET_BASE && id < MI_PRESET_BASE + 20) {
            int i = id - MI_PRESET_BASE;
            if (i < (int)g.presets.size()) MainWndApplyValue(g.presets[i], true);
        } else if (id == MI_OPEN) {
            MainWndShow(true);
        } else if (id == MI_AUTORUN) {
            AutorunSet(!g.autorun);
        } else if (id == MI_EXIT) {
            DestroyWindow(hwnd);
        }
        return 0;
    }

    case WM_CLOSE: HideToTray(); return 0;   // 关闭 = 收进托盘
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool MainWndCreate() {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = g.inst;
    wc.lpszClassName = APP_CLASS;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    g.dpi = GetDpiForSystem();   // PMv2 感知下即当前显示器 DPI

    g.hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        APP_CLASS, APP_NAME, WS_POPUP,
        0, 0, S(LW), S(LH), nullptr, nullptr, g.inst, nullptr);
    if (!g.hwnd) return false;
    MainWndRecreateLayout();
    return true;
}
