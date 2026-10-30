# AdjustBrightness

一个用 Win32 API 手写、全自绘界面的 Windows 内置显示器亮度调节小工具。常驻托盘，开机自启，单文件、无第三方依赖（`/MT` 静态链接）。

界面尺寸 380×176 逻辑像素，默认贴屏幕右下角，跟随系统深浅色主题，Per-Monitor V2 DPI 适配。

## 功能

- **托盘常驻**：左键图标切换面板显示/隐藏，右键菜单（打开面板、快捷档位、开机自启、退出）
- **亮度滑块 0–100**：竖条控制柄，整根竖条都可以按下拖动；下方 0/25/50/75/100 刻度
- **快捷档位**：默认 0、5、10…50，点击直接跳到对应亮度
- **自定义档位**：3 列 × 11 个输入框，保存后自动升序排序并去重；点弹框外部即关闭（只关弹框，主面板保留）
- **开机自启**：写 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`（应用内可随时开关）
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
   src\main.cpp src\mainwnd.cpp src\presetdlg.cpp src\brightness.cpp src\config.cpp ^
   build\app.res ^
   /link /LTCG /OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS /MANIFEST:NO ^
   user32.lib gdi32.lib ole32.lib oleaut32.lib wbemuuid.lib shell32.lib advapi32.lib imm32.lib
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

## 配置

所有配置只落在**程序所在目录** 的 `config.ini`，不写 AppData、不写临时目录：

```ini
[state]
lastBrightness=30      ; 退出时记住的亮度
[presets]
list=0,5,10,15,20,25,30,35,40,45,50   ; 快捷档位, 升序去重
```

窗口位置**不做持久化**：每次启动都重新贴到屏幕右下角；只有本次运行内手动拖动的位置会被记住。唯一的注册表写入是开机自启项（`HKCU\...\Run`）。

## 说明

- 界面与交互按 `mockups/wireframe.html` 实现，比例与语义保持一致
- 项目未附带开源许可证，默认保留所有权利；如需开源请自行补充 `LICENSE`
