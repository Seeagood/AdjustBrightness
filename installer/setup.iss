; AdjustBrightness Inno Setup 脚本
; 编译: ISCC.exe installer\setup.iss
; 产物: installer\Output\AdjustBrightnessSetup.exe

#define AppName      "AdjustBrightness"
#define AppVersion   "1.0.1"
#define AppPublisher "AdjustBrightness"
#define AppExe       "AdjustBrightness.exe"

[Setup]
AppId={{8B7E3A1C-5F2D-4E9A-B6C1-2A3B4C5D6E7F}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
; 按当前用户安装到 %LOCALAPPDATA%\AdjustBrightness, 不需要管理员权限
DefaultDirName={localappdata}\AdjustBrightness
DefaultGroupName={#AppName}
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
DisableProgramGroupPage=yes
OutputDir=Output
OutputBaseFilename=AdjustBrightnessSetup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; Windows 10 x64 及以上
MinVersion=10.0
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#AppExe}
SetupIconFile=..\res\sun.ico

[Languages]
; 中文语言包是非官方翻译, 官方安装包里不自带 (GitHub runner 上就缺这个文件),
; 因此随仓库带一份 installer\ChineseSimplified.isl; 万一缺失则回退英文, 不中断编译
#if FileExists(SourcePath + "ChineseSimplified.isl")
Name: "chinesesimp"; MessagesFile: "ChineseSimplified.isl"
#else
Name: "english"; MessagesFile: "compiler:Default.isl"
#endif

[Tasks]
Name: "autorun"; Description: "开机自动启动"; GroupDescription: "附加任务:"

[Files]
Source: "..\build\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\卸载 {#AppName}"; Filename: "{uninstallexe}"

[Registry]
; 任务勾选时写入 HKCU Run (应用内也可自行开关, 此处仅为安装时的初始状态)
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; \
    ValueType: string; ValueName: "{#AppName}"; ValueData: """{app}\{#AppExe}"""; \
    Tasks: autorun; Flags: uninsdeletevalue

[Run]
Filename: "{app}\{#AppExe}"; Description: "立即运行 {#AppName}"; \
    Flags: nowait postinstall skipifsilent

[UninstallDelete]
; 删除安装目录下的配置与全部残留
Type: filesandordirs; Name: "{app}"

[Code]
procedure KillApp();
var
  ResultCode: Integer;
begin
  Exec('taskkill.exe', '/F /IM {#AppExe}', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
end;

// 覆盖安装/卸载前结束运行中的实例
function InitializeSetup(): Boolean;
begin
  KillApp();
  Result := True;
end;

function InitializeUninstall(): Boolean;
begin
  KillApp();
  Result := True;
end;
