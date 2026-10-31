@echo off
rem build.bat - Build AdjustBrightness.exe (Release, /MT) with portable MSVC toolchain.
rem Toolchain location: set AB_MSVC=<path>, default %TEMP%\abtools\msvc (see tools\fetch_toolchain.txt)
setlocal
pushd %~dp0

if "%AB_MSVC%"=="" set "AB_MSVC=%TEMP%\abtools\msvc"
if not exist "%AB_MSVC%\VC\Tools\MSVC" (
    echo [ERROR] MSVC toolchain not found: %AB_MSVC%
    echo         See tools\fetch_toolchain.txt or set AB_MSVC to the toolchain root.
    exit /b 1
)

for /f %%i in ('dir /b /ad "%AB_MSVC%\VC\Tools\MSVC"') do set VCV=%%i
set "VCDIR=%AB_MSVC%\VC\Tools\MSVC\%VCV%"
for /f %%i in ('dir /b /ad "%AB_MSVC%\Windows Kits\10\Include"') do set SDKV=%%i
set "SDKINC=%AB_MSVC%\Windows Kits\10\Include\%SDKV%"
set "SDKLIB=%AB_MSVC%\Windows Kits\10\Lib\%SDKV%"
set "SDKBIN=%AB_MSVC%\Windows Kits\10\bin\%SDKV%\x64"

set "PATH=%VCDIR%\bin\Hostx64\x64;%SDKBIN%;%PATH%"
set "INCLUDE=%VCDIR%\include;%SDKINC%\um;%SDKINC%\shared;%SDKINC%\ucrt;%SDKINC%\winrt"
set "LIB=%VCDIR%\lib\x64;%SDKLIB%\um\x64;%SDKLIB%\ucrt\x64"

if not exist build mkdir build

echo [1/2] Resources...
rc.exe /nologo /utf-8 /fo build\app.res res\app.rc || exit /b 1

echo [2/2] Compile and link (MSVC %VCV%, SDK %SDKV%)...
cl.exe /nologo /utf-8 /MT /O2 /GL /DNDEBUG /D_HAS_EXCEPTIONS=0 /GR- /EHs-c- /W3 /WX- ^
    /Fobuild\ /Febuild\AdjustBrightness.exe ^
    src\main.cpp src\mainwnd.cpp src\presetdlg.cpp src\aboutdlg.cpp src\brightness.cpp src\config.cpp ^
    build\app.res ^
    /link /LTCG /OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS /MANIFEST:NO ^
    user32.lib gdi32.lib ole32.lib oleaut32.lib wbemuuid.lib shell32.lib advapi32.lib imm32.lib || exit /b 1

echo.
echo OK: %CD%\build\AdjustBrightness.exe
popd
