// main.cpp - 入口: 单实例 / 托盘 / 消息循环
#include "app.h"
#include <shellapi.h>
#include <imm.h>

#define IDI_SUN 101

// 托盘身份 GUID 由 exe 完整路径派生: 同一安装路径跨重启保持同一身份 (便于清理僵尸图标),
// 不同路径则是不同身份 —— shell 会把一个 GUID 绑定到首次注册它的 exe 路径,
// 同一 GUID 换路径再 Add 会被拒绝 (NIM_ADD 返回 E_FAIL, 图标完全看不到)。
static GUID TrayGuid() {
    static GUID gd = {};
    static bool ready = false;
    if (ready) return gd;

    std::wstring path = ExeDir() + L"\\" APP_NAME L".exe";
    unsigned __int64 h1 = 1469598103934665603ULL;          // FNV-1a
    unsigned __int64 h2 = 0x9E3779B97F4A7C15ULL;           // 多项式滚动
    for (size_t i = 0; i < path.size(); i++) {
        unsigned __int64 c = (unsigned __int64)(unsigned short)path[i];
        h1 ^= c;  h1 *= 1099511628211ULL;
        h2 = (h2 + c + i) * 6364136223846793005ULL;
    }
    gd.Data1 = (DWORD)(h1 >> 32);
    gd.Data2 = (WORD)(h1 >> 16);
    gd.Data3 = (WORD)((h1 & 0x0FFF) | 0x4000);             // version 4
    for (int i = 0; i < 8; i++) gd.Data4[i] = (BYTE)(h2 >> (8 * i));
    gd.Data4[0] = (BYTE)((gd.Data4[0] & 0x3F) | 0x80);     // variant
    ready = true;
    return gd;
}

static NOTIFYICONDATAW g_nid = {};
static UINT g_taskbarCreated = 0;

UINT TrayTaskbarCreatedMsg() { return g_taskbarCreated; }

static HICON LoadSunIcon() {
    // 托盘图标物理像素: 16@96dpi, 32@192dpi (ico 内含 16/20/24/32 多尺寸)
    int sz = MulDiv(GetSystemMetrics(SM_CXSMICON), (int)g.dpi, 96);
    if (sz < 16) sz = 16;
    HICON h = (HICON)LoadImageW(g.inst, MAKEINTRESOURCEW(IDI_SUN), IMAGE_ICON,
        sz, sz, LR_DEFAULTCOLOR);
    if (!h) h = (HICON)LoadImageW(g.inst, MAKEINTRESOURCEW(IDI_SUN), IMAGE_ICON,
        0, 0, LR_DEFAULTCOLOR);
    if (!h) h = LoadIconW(g.inst, MAKEINTRESOURCEW(IDI_SUN));
    if (!h) h = LoadIconW(nullptr, IDI_APPLICATION);   // 兜底: 宁可有图标也不要空槽
    return h;
}

static int s_hintStage = 0;   // 0/1: 系统"临时提升"期间, 稍后再复查; 2: 已复查过

void TrayAdd() {
    if (!g.hwnd) return;
    s_hintStage = 0;

    // 1) 先按同一 GUID 删除: 崩溃/强杀后 shell 会留下僵尸图标,
    //    残留条目还会让后续 NIM_ADD 冲突并返回 E_FAIL (这是"完全看不到图标"的根因)
    NOTIFYICONDATAW del = {};
    del.cbSize = sizeof(del);
    del.hWnd = g.hwnd;
    del.uID = 1;
    del.uFlags = NIF_GUID;
    del.guidItem = TrayGuid();
    Shell_NotifyIconW(NIM_DELETE, &del);

    // 2) 用固定的 GUID 作为身份:
    //    - 每次启动都用同一 GUID 先删后加, 被强杀的进程留下的僵尸条目会被清掉, 不会叠加;
    //    - 代价是 shell 会记住该 GUID 上次的"已隐藏"状态, 新图标会直接落进 ^ 溢出区,
    //      由 TrayCheckOverflowHint() 用一条静音通知把它临时提升到任务栏。
    memset(&g_nid, 0, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g.hwnd;
    g_nid.uID = 1;
    g_nid.uCallbackMessage = WM_APP_TRAY;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP | NIF_GUID;
    g_nid.guidItem = TrayGuid();
    g_nid.hIcon = LoadSunIcon();
    lstrcpyW(g_nid.szTip, L"当前亮度 --%");

    BOOL ok = FALSE;
    for (int i = 0; i < 5; i++) {          // shell 尚未就绪时重试
        ok = Shell_NotifyIconW(NIM_ADD, &g_nid);
        if (ok) break;
        Sleep(200);
    }

    if (ok) {
        g_nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &g_nid);
    }
}

// S_OK = 图标在任务栏可见; S_FALSE = 被折叠进 ^ 溢出区; E_FAIL = 图标尚未注册
static HRESULT TrayIconRect(RECT& rc) {
    NOTIFYICONIDENTIFIER id = {};
    id.cbSize = sizeof(id);
    id.hWnd = g.hwnd;
    id.uID = 1;
    id.guidItem = TrayGuid();
    rc = {};
    return Shell_NotifyIconGetRect(&id, &rc);
}

// Windows 只会把新托盘图标"临时"提升到任务栏 (几秒后又折叠进 ^), 且只有用户能永久固定。
// 启动约 3 秒后检查: 若图标已在 ^ 里, 发一条静音通知 —— shell 会把正在显示通知的图标
// 再次临时提升到任务栏, 同时提示用户把图标拖到任务栏即可固定显示。
void TrayCheckOverflowHint() {
    if (!g.hwnd) return;
    bool overflow = false;
    for (int i = 0; i < 6; i++) {          // 刚 NIM_ADD 后 shell 可能还没登记
        RECT rc;
        HRESULT hr = TrayIconRect(rc);
        if (hr == S_OK) {                  // 可能只是系统临时提升, 5 秒后再确认一次
            if (s_hintStage < 2) {
                s_hintStage++;
                SetTimer(g.hwnd, IDT_HINT, 5000, nullptr);
            }
            return;
        }
        if (hr == S_FALSE) { overflow = true; break; }
        Sleep(150);
    }
    if (!overflow) return;
    s_hintStage = 2;
    NOTIFYICONDATAW n = {};
    n.cbSize = sizeof(n);
    n.hWnd = g.hwnd;
    n.uID = 1;
    n.uFlags = NIF_INFO | NIF_GUID;
    n.guidItem = TrayGuid();
    n.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
    lstrcpyW(n.szInfoTitle, APP_NAME);
    wsprintfW(n.szInfo, L"当前亮度 %d%%。图标被折叠时点任务栏右侧 ^，把太阳图标拖到任务栏即可固定显示。",
        g.curValue);
    Shell_NotifyIconW(NIM_MODIFY, &n);
}

void TrayRemove() { Shell_NotifyIconW(NIM_DELETE, &g_nid); }

void TrayUpdateTip() {
    if (!g_nid.hWnd) return;
    wsprintfW(g_nid.szTip, L"当前亮度 %d%%", g.curValue);
    g_nid.uFlags = NIF_TIP | NIF_SHOWTIP | NIF_GUID;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

// 右键菜单: 灰行信息 + 分隔线 + 11 档位 + 分隔线 + 打开主界面/开机自启/退出
// owner-draw 绘制在 mainwnd.cpp (WM_MEASUREITEM / WM_DRAWITEM)
void TrayShowMenu(POINT pt) {
    HMENU m = CreatePopupMenu();
    auto add = [&](int id) {
        bool disabled = (id == MI_INFO) ||
            (!g.supported && id >= MI_PRESET_BASE && id < MI_PRESET_BASE + 20) ||
            id == MI_SEP_DATA;
        // MF_OWNERDRAW 时第 4 参数即 dwItemData
        AppendMenuW(m, MF_OWNERDRAW | (disabled ? MF_DISABLED | MF_GRAYED : 0),
            (UINT_PTR)id, (LPCWSTR)(INT_PTR)id);
    };
    add(MI_INFO);
    add(MI_SEP_DATA);
    for (size_t i = 0; i < g.presetBtns.size(); i++) add(MI_PRESET_BASE + (int)i);
    add(MI_SEP_DATA);
    add(MI_OPEN);
    add(MI_AUTORUN);
    add(MI_EXIT);

    SetForegroundWindow(g.hwnd);   // 菜单正确获得/归还焦点
    UINT cmd = TrackPopupMenuEx(m,
        TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_BOTTOMALIGN,
        pt.x, pt.y, g.hwnd, nullptr);
    PostMessageW(g.hwnd, WM_NULL, 0, 0);
    DestroyMenu(m);
    if (cmd) SendMessageW(g.hwnd, WM_COMMAND, cmd, 0);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    g.inst = hInst;
    g.dpi = 96;

    // 单实例
    HANDLE mtx = CreateMutexW(nullptr, TRUE, L"Local\\AdjustBrightness.SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND prev = FindWindowW(APP_CLASS, nullptr);
        if (prev) PostMessageW(prev, WM_APP_SHOW, 0, 0);
        CloseHandle(mtx);
        return 0;
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // 面板无任何文字输入 (自定义框是 ES_NUMBER 纯数字), 关闭本进程的 IME/TSF:
    // 否则窗口获得焦点时输入法会把自己(例如搜狗 ~18MB + 10 余线程)注入本进程
    ImmDisableIME(-1);

    AppReadTheme();
    ConfigLoad();

    // 首次运行: 默认开启开机自启
    if (!AutorunGet() && GetFileAttributesW((ExeDir() + L"\\config.ini").c_str()) == INVALID_FILE_ATTRIBUTES)
        AutorunSet(true);

    g.supported = BrightInit();
    if (g.supported) {
        if (g.stateLoaded) {
            // 恢复上次亮度 (0% -> 实际下发 hwFloor)
            BrightWrite(g.curValue);
        } else {
            // 首次运行 (含卸载重装后 config.ini 已不存在): 不把屏幕改成默认 50%,
            // 而是沿用屏幕当前亮度, 并把它记为"上次亮度"
            int v = BrightRawToLogical(BrightRead());
            if (v >= 0) {
                g.curValue = v; g.sliderVal = v; g.chaseVal = (double)v;
            } else {
                BrightWrite(g.curValue);   // 读不到当前亮度时才退回默认值
            }
            ConfigSaveState();
        }
        BrightSyncSystem(g.curValue);   // 启动时也把电源方案里的亮度对齐 (重启后系统自恢复也正确)
        g.lastWritten = BrightRawToLogical(g.curValue == 0 ? g.hwFloor : g.curValue);
        g.lastWriteTick = GetTickCount();
        int raw = BrightRead();
        if (raw >= 0) {
            int v = BrightRawToLogical(raw);
            if (v >= 0) { g.curValue = v; g.sliderVal = v; g.chaseVal = (double)v; }
        }
    }

    if (!MainWndCreate()) return 1;

    // Explorer 重启后 shell 会清空托盘, 必须监听 TaskbarCreated 重新添加
    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    TrayAdd();
    TrayUpdateTip();
    // 系统对新图标的临时提升只有几秒, 3 秒后再检查并决定是否用通知再次提升
    SetTimer(g.hwnd, IDT_HINT, 3000, nullptr);

    SetTimer(g.hwnd, IDT_POLL, 750, nullptr);

    // 托盘消息 / 唤出消息在主窗口处理之外, 这里挂到主窗口
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        // 自定义对话框打开期间: Esc = 取消
        if (g.dlg && msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            DestroyWindow(g.dlg);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    TrayRemove();
    CloseHandle(mtx);
    return 0;
}
