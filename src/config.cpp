// config.cpp - config.ini (仅写安装目录) + 开机自启 (HKCU Run, 唯一注册表写入)
#include "app.h"

AppState g;

std::wstring ExeDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf);
    size_t i = p.find_last_of(L'\\');
    return i == std::wstring::npos ? L"." : p.substr(0, i);
}

static std::wstring IniPath() { return ExeDir() + L"\\config.ini"; }

// 档位: 升序 + 去重
static std::vector<int> NormalizePresets(std::vector<int> v) {
    std::vector<int> out;
    for (int x : v) {
        if (x < 0) x = 0; if (x > 100) x = 100;
        bool dup = false;
        for (int y : out) if (y == x) { dup = true; break; }
        if (!dup) out.push_back(x);
    }
    for (size_t i = 0; i < out.size(); i++)
        for (size_t j = i + 1; j < out.size(); j++)
            if (out[j] < out[i]) { int t = out[i]; out[i] = out[j]; out[j] = t; }
    if (out.empty()) out.push_back(50);
    return out;
}

static std::vector<int> DefaultPresets() {
    std::vector<int> v;
    for (int i = 0; i <= 50; i += 5) v.push_back(i);
    return v;
}

void ConfigLoad() {
    // 档位
    wchar_t buf[512] = {};
    GetPrivateProfileStringW(L"presets", L"list", L"", buf, 512, IniPath().c_str());
    if (buf[0]) {
        std::vector<int> v; int cur = 0; bool has = false;
        for (wchar_t* p = buf; ; p++) {
            if (*p >= L'0' && *p <= L'9') { cur = cur * 10 + (*p - L'0'); has = true; }
            else {
                if (has) v.push_back(cur);
                cur = 0; has = false;
                if (*p == 0) break;
            }
        }
        g.presets = NormalizePresets(v);
    } else {
        g.presets = DefaultPresets();
    }
    while (g.presets.size() > 11) g.presets.pop_back();

    g.curValue = GetPrivateProfileIntW(L"state", L"lastBrightness", 50, IniPath().c_str());
    if (g.curValue < 0 || g.curValue > 100) g.curValue = 50;
    g.sliderVal = g.curValue;
    g.chaseVal = (double)g.curValue;

    // 窗口位置不做持久化: 每次启动都重新固定到屏幕右下角, 仅本次运行内记住手动拖动的位置

    g.autorun = AutorunGet();
}

void ConfigSavePresets() {
    std::wstring s;
    for (size_t i = 0; i < g.presets.size(); i++) {
        if (i) s += L",";
        s += std::to_wstring(g.presets[i]);
    }
    WritePrivateProfileStringW(L"presets", L"list", s.c_str(), IniPath().c_str());
}

void ConfigSaveState() {
    WritePrivateProfileStringW(L"state", L"lastBrightness",
        std::to_wstring(g.curValue).c_str(), IniPath().c_str());
}

// ---- 开机自启: HKCU\...\Run ----
static const wchar_t* RUN_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

bool AutorunGet() {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_READ, &k) != ERROR_SUCCESS) return false;
    wchar_t buf[MAX_PATH * 2]; DWORD cb = sizeof(buf), type = 0;
    LRESULT r = RegQueryValueExW(k, APP_NAME, nullptr, &type, (LPBYTE)buf, &cb);
    RegCloseKey(k);
    return r == ERROR_SUCCESS && type == REG_SZ;
}

void AutorunSet(bool on) {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) return;
    if (on) {
        std::wstring p = L"\"" + ExeDir() + L"\\" APP_NAME L".exe\"";
        RegSetValueExW(k, APP_NAME, 0, REG_SZ, (const BYTE*)p.c_str(),
            (DWORD)((p.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(k, APP_NAME);
    }
    RegCloseKey(k);
    g.autorun = on;
}
