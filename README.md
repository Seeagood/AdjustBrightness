# AdjustBrightness

一个用 Win32 API 手写、全自绘界面的 Windows 内置显示器亮度调节小工具。常驻托盘，开机自启，单文件、无第三方依赖（`/MT` 静态链接）。

界面尺寸 380×176 逻辑像素，默认贴屏幕右下角，跟随系统深浅色主题，Per-Monitor V2 DPI 适配。

## 功能

- **托盘常驻**：左键图标切换面板显示/隐藏，右键菜单（打开面板、快捷档位、开机自启、退出）
- **亮度滑块 0–100**：竖条控制柄，整根竖条都可以按下拖动；下方 0/25/50/75/100 刻度
- **快捷档位**：主面板固定有 0 档，加上自定义档位共 11 个按钮，点击直接跳到对应亮度
- **自定义档位**：5 列 × 2 行共 10 个输入框；标题栏右侧可选「最大值 50 / 100」（50 → 默认 5,10…50，100 → 默认 10,20…100）；保存后自动升序排序并去重；非法输入会标红输入框并提示；点弹框外部即关闭（只关弹框，主面板保留）
- **关于面板**：点左上角标题（太阳图标 +「内置显示器」）弹出，显示版本号与 GitHub 地址，点地址直接打开浏览器
- **开机自启**：写 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`（应用内可随时开关）
- **与系统亮度百分百同步**：改亮度时同时写入当前电源方案的「显示器亮度」，睡眠唤醒、重启、插拔电源都不会被系统改回旧值
- **单实例运行**、失焦自动收进托盘、外部改亮度自动回读同步

## 系统要求

| 项 | 要求 |
| --- | --- |
| 系统 | Windows 10 1809+ / Windows 11，x64 |
| 显示 | 显示器需支持亮度控制（笔记本内置屏，或支持 DDC/CI 亮度的外接显示器） |
| 运行时依赖 | 无。亮度接口优先用 DXVA2（`dxva2.dll`），不可用时回退到 WMI（`root\wmi:WmiMonitorBrightness`） |

> 不支持的显示器上，面板会整体置灰显示。

## 目录结构

```
src/            源码
  main.cpp        程序入口、托盘图标、菜单、消息循环
  mainwnd.cpp     主面板（窗口、自绘、热区、拖动、主题）
  presetdlg.cpp   自定义快捷档位对话框
  aboutdlg.cpp    关于对话框（版本号、GitHub 地址）
  brightness.cpp  DXVA2 / WMI 亮度读写
  config.cpp      config.ini 读写 + 开机自启
  app.h           共享定义（常量、主题、全局状态、接口声明）
res/            图标与清单（app.rc、sun.ico、app.manifest）
mockups/        设计稿 wireframe.html
installer/      Inno Setup 脚本 setup.iss
tools/          图标生成脚本 gen_icon.js
build.bat       一键编译（Release /MT）
```

## 构建

### 依赖

- MSVC 14.x（VS 2022 Build Tools 或完整 VS 即可），x64
- Windows SDK 10（含 `rc.exe`、`dxva2.lib`、`wbemuuid.lib`）

### 方式一：build.bat（便携工具链目录）

`build.bat` 期望把编译工具链放在 `%AB_MSVC%`，默认 `%TEMP%\abtools\msvc`，目录结构为：

```
<AB_MSVC>\VC\Tools\MSVC\<版本>\
<AB_MSVC>\Windows Kits\10\Include\<版本>\
<AB_MSVC>\Windows Kits\10\Lib\<版本>\
<AB_MSVC>\Windows Kits\10\bin\<版本>\x64\
```

即把 VS Build Tools 与 Windows SDK 中上述目录原样拷贝到该位置（免安装、可随项目携带）。

```bat
set AB_MSVC=D:\tools\msvc
build.bat
```

产物：`build\AdjustBrightness.exe`

### 方式二：已有的 Visual Studio 环境

在 “x64 Native Tools Command Prompt for VS 2022” 中执行等价命令：

```bat
rc /nologo /utf-8 /fo build\app.res res\app.rc

cl /nologo /utf-8 /MT /O2 /GL /DNDEBUG /EHs-c- /GR- /W3 ^
   /Fobuild\ /Febuild\AdjustBrightness.exe ^
   src\main.cpp src\mainwnd.cpp src\presetdlg.cpp src\aboutdlg.cpp src\brightness.cpp src\config.cpp ^
   build\app.res ^
   /link /LTCG /OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS /MANIFEST:NO ^
   user32.lib gdi32.lib ole32.lib oleaut32.lib wbemuuid.lib powrprof.lib shell32.lib advapi32.lib imm32.lib
```

## 打包安装程序

需要 [Inno Setup 6](https://jrsoftware.org/isinfo.php)：

```bat
ISCC.exe installer\setup.iss
```

产物：`installer\Output\AdjustBrightnessSetup.exe`

- 安装到 `%LOCALAPPDATA%\AdjustBrightness`，`PrivilegesRequired=lowest`，不弹 UAC
- 安装时可勾选「开机自动启动」
- 覆盖安装/卸载前会自动结束正在运行的实例

> 仓库带了 `.github/workflows/release.yml`：推送 `v*` 标签（或手动 Run workflow）时，会在 GitHub 的 Windows runner 上自动编译 + 打包，并把 `AdjustBrightness.exe` 与 `AdjustBrightnessSetup.exe` 上传到对应 Release。

## 配置

所有配置只落在**程序所在目录** 的 `config.ini`，不写 AppData、不写临时目录：

```ini
[state]
lastBrightness=30      ; 当前亮度 (仅作读不到显示器时的兜底)
hwFloor=5              ; 实测到的硬件最低值 (该值在界面上显示为 0%)
[presets]
max=50                                 ; 自定义档位上限: 50 或 100 (默认 50)
list=5,10,15,20,25,30,35,40,45,50      ; 自定义档位, 升序去重, 最多 10 个 (0 由主面板内置)
```

窗口位置**不做持久化**：每次启动都重新贴到屏幕右下角；只有本次运行内手动拖动的位置会被记住。

亮度的唯一可信来源是**显示器当前的真实亮度**：

- 启动时读取面板真实值并沿用它，不会用 `lastBrightness` 把屏幕改回旧值（在移动中心、系统设置或显示器按键改过、或软件没运行时系统自己变过，都以真实值为准）；
- 运行中每 750ms 回读一次，发现外部改动就立刻更新界面、写进电源方案并落盘；
- 只有读不到亮度（例如 DDC 未就绪）时才退回 `lastBrightness` 并下发。

卸载会连同 `config.ini` 一起删掉，重装后首次启动同样沿用屏幕当前亮度。

注册表写入只有两处：

1. 开机自启项 `HKCU\...\Run`
2. 当前**用户级电源方案**里的「显示器亮度」值（`SUB_VIDEO` / `aded5e82-…`，交流、直流各一份）。这是 Windows 自己保存“当前屏幕亮度”的地方，写它才能让系统与面板保持同一个值；不需要管理员权限，也不改动电源方案的其它设置。

## 更新日志

**v1.0.3**

- 重新设计亮度同步：**以显示器当前真实亮度为唯一来源**。启动时读取面板真实值并沿用，不再用 `lastBrightness` 把屏幕改回旧值，彻底修复「软件调 20 → 移动中心改 30 → 退出再启动又变回 20」。
- 运行中每 750ms 回读，发现外部改动（移动中心、系统设置、显示器按键、其它工具）立即更新界面、同步电源方案并落盘；退出时也统一保存。
- 电源方案写入改为幂等：AC/DC 已一致时不再重写，避免外部改动时重复重设电源方案。
- 修复 0% 语义：下发最低档后回读硬件真实下限（本机为 5）并持久化为 `hwFloor`，界面稳定显示 0%，重启后仍是 0%（之前会显示成 5%）。
- `config.ini` 新增 `[state] hwFloor`。
- 安装包版本号同步为 1.0.3。

**v1.0.2**

- 新增「关于」面板：点左上角标题（太阳图标 +「内置显示器」）弹出，显示版本号与 GitHub 地址，点地址直接打开浏览器。
- 自定义档位弹框重新设计：5 列 × 2 行共 10 个输入框；标题右侧可选「最大值 50 / 100」，切换后按上限重填默认值（50 → 5,10…50，100 → 10,20…100）；保存时仍自动升序去重；输入超出范围时标红输入框并保留错误提示。
- 0 档改为主面板内置，不再需要单独自定义 0；主面板固定显示 0 + 最多 10 个自定义档位，共 11 个按钮。
- 修复：卸载重装后首次启动会把亮度改成 50%。现在首次运行沿用屏幕当前亮度并记为「上次亮度」，之后才按记录恢复。
- 安装包版本号同步为 1.0.2。

**v1.0.1**

- 修复：睡眠唤醒后亮度被系统改回旧值（例如 50%）。根因是 `WmiSetBrightness` / `SetMonitorBrightness` 只改面板，不改 Windows 电源方案里的「显示器亮度」，唤醒/重启/插拔电源时系统会按旧值重新下发。
- 现在每次提交亮度（档位、菜单、拖动松手、启动恢复）都会同步写入当前电源方案的亮度值，界面、面板、系统三方一致；唤醒后再补写一次并复查，作为兜底。
- 安装包版本号同步为 1.0.1。

**v1.0.0** — 首个发布版本。

## 说明

- 界面与交互按 `mockups/wireframe.html` 实现，比例与语义保持一致
- 项目未附带开源许可证，默认保留所有权利；如需开源请自行补充 `LICENSE`
