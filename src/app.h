#pragma once
// AdjustBrightness - 共享定义
// 设计稿: mockups/wireframe.html (380x240 逻辑像素, 强调色 #0078D4)

#ifndef UNICODE
#define UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>

#define APP_NAME    L"AdjustBrightness"
#define APP_CLASS   L"AdjustBrightnessMainWnd"
#define DLG_CLASS   L"AdjustBrightnessPresetDlg"

#define WM_APP_TRAY   (WM_APP + 1)
#define WM_APP_SHOW   (WM_APP + 2)

#define IDT_POLL   1   // 外部亮度回读 750ms
#define IDT_CHASE  2   // 拖动追赶 15ms
#define IDT_HIDE   3   // 失焦延迟收起 150ms
#define IDT_HINT   4   // 托盘图标是否落进 ^ 溢出区的检查 (一次性)
#define IDT_OUTCLICK 5 // 弹框因点外部而关闭后: 轮询"面板外又一次左键按下" (40ms)

// 托盘菜单项 ID (owner-draw 数据)
#define MI_SEP_DATA  (-1)
#define MI_INFO      900
#define MI_OPEN      901
#define MI_AUTORUN   902
#define MI_EXIT      903
#define MI_PRESET_BASE 100

#define ACCENT RGB(0x00, 0x78, 0xD4)

// ---- 主题配色 (与 wireframe 一致) ----
struct Theme {
    bool light;
    COLORREF bg, border, text, dim, track, tick, btnBg, btnBorder, sep;
};

// ---- 全局状态 ----
struct AppState {
    HINSTANCE inst = nullptr;
    HWND      hwnd = nullptr;      // 主面板
    HWND      dlg  = nullptr;      // 自定义对话框 (模态)
    UINT      dpi  = 96;
    bool      lightTheme = true;
    Theme     th = {};

    // 亮度
    bool  supported  = false;      // WMI 亮度接口是否可用
    int   hwFloor    = 3;          // 实际下发下限: max(3, 硬件最小值)
    bool  hwFloorNoted = false;    // hwFloor > 3 时界面标注
    int   curValue   = 50;         // 界面显示值 0..100
    int   lastWritten  = -1;       // 我们最后一次下发的值 (回声抑制)
    DWORD lastWriteTick = 0;

    // 拖动
    bool   dragging = false;
    int    sliderVal = 50;
    double chaseVal  = 50.0;
    DWORD  lastSendTick = 0;
    int    lastSentValue = -1;

    // 配置
    std::vector<int> presets;      // 快捷档位, 升序去重
    POINT winPos = {0, 0};
    bool  hasWinPos = false;
    bool  autorun = true;

    // 布局热区 (像素, 已按 DPI 缩放)
    RECT rcSlider = {}, rcCustom = {}, rcSwitch = {};
    std::vector<RECT> rcPreset;
};
extern AppState g;

// ---- 工具 ----
int  S(int v);                      // 逻辑像素 -> 物理像素
void AppReadTheme();                // 读系统深浅色并填充 g.th
HFONT MakeFont(int px, int weight); // 按当前 DPI 建字体 (Segoe UI, 中文回落微软雅黑)
void Txt(HDC dc, const wchar_t* s, HFONT f, COLORREF c, RECT r, UINT fmt); // 文本绘制
void MainWndLayout();               // 主面板热区重建

// ---- brightness.cpp ----
bool BrightInit();                  // 初始化 WMI; 返回接口是否可用
int  BrightRead();                  // 回读原始亮度; 失败返回 -1
bool BrightWrite(int logicalValue); // 下发逻辑值 (0 -> hwFloor)
int  BrightRawToLogical(int raw);   // <=3 显示为 0

// ---- config.cpp ----
void ConfigLoad();
void ConfigSavePresets();
void ConfigSaveState();
std::wstring ExeDir();

// ---- autorun (HKCU Run, 唯一允许的注册表写入) ----
bool AutorunGet();
void AutorunSet(bool on);

// ---- mainwnd.cpp ----
bool MainWndCreate();
void MainWndShow(bool show);
void MainWndArmOutsideClickHide();         // 面板可见但未获焦点: 下一次面板外左键按下即收起
void MainWndApplyValue(int v, bool write); // 设置亮度 (UI + 可选下发)
void MainWndRefreshPoll();                 // 回读并同步 UI
void MainWndRecreateLayout();              // DPI/主题变化后重建
LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);

// ---- presetdlg.cpp ----
void PresetDlgOpen();               // 模态打开自定义对话框

// ---- tray / menu (main.cpp) ----
void TrayAdd();
void TrayRemove();
void TrayUpdateTip();
void TrayShowMenu(POINT pt);
UINT TrayTaskbarCreatedMsg();       // RegisterWindowMessage("TaskbarCreated")
void TrayCheckOverflowHint();       // 图标被折叠进 ^ 溢出区时的首次提示
