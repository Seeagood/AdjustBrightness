// brightness.cpp - 亮度读写
// 首选 Dxva2 物理监视器亮度接口 (轻量, 无 COM/WMI 运行时); 不被支持时才回退 WMI。
#include "app.h"
#include <physicalmonitorenumerationapi.h>
#include <lowlevelmonitorconfigurationapi.h>
#include <highlevelmonitorconfigurationapi.h>
#include <wbemidl.h>
#include <oleauto.h>
#include <powrprof.h>

#pragma comment(lib, "dxva2.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "powrprof.lib")

static PHYSICAL_MONITOR g_pm = {};
static bool g_usePm = false;
static int  g_min = 0, g_max = 100;

static bool InitDxva2() {
    HMONITOR mon = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    if (!mon) return false;

    DWORD n = 0;
    if (!GetNumberOfPhysicalMonitorsFromHMONITOR(mon, &n) || n == 0) return false;

    std::vector<PHYSICAL_MONITOR> pms(n);
    if (!GetPhysicalMonitorsFromHMONITOR(mon, n, pms.data())) return false;

    int pick = -1;
    for (DWORD i = 0; i < n; i++) {
        DWORD mn = 0, cu = 0, mx = 0;
        // 少数驱动会返回 TRUE 但范围是 0..0 (实际不可控): 此时不能当成功,
        // 否则写入会被夹到 0 把屏幕调黑, 应回退 WMI
        if (GetMonitorBrightness(pms[i].hPhysicalMonitor, &mn, &cu, &mx) &&
            mx > mn && mx > 3) {
            pick = (int)i;
            g_min = (int)mn;
            g_max = (int)mx;
            break;
        }
        DestroyPhysicalMonitor(pms[i].hPhysicalMonitor);
    }
    if (pick < 0) return false;
    g_pm = pms[pick];
    g_usePm = true;
    return true;
}

// ---- WMI 回退 (root\WMI, 仅在不支持 Dxva2 时才加载) ----
static IWbemServices* g_svc = nullptr;

static bool WmiOpen() {
    IWbemLocator* loc = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (void**)&loc))) return false;

    BSTR ns = SysAllocString(L"root\\wmi");
    HRESULT hr = loc->ConnectServer(ns, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &g_svc);
    SysFreeString(ns);
    loc->Release();
    if (FAILED(hr) || !g_svc) { g_svc = nullptr; return false; }

    CoSetProxyBlanket(g_svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    return true;
}

static bool InitWmi() {
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return false;
    HRESULT hs = CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
        RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr, EOAC_NONE, nullptr);
    if (FAILED(hs) && hs != RPC_E_TOO_LATE) return false;
    if (!WmiOpen()) return false;

    IEnumWbemClassObject* en = nullptr;
    BSTR ql = SysAllocString(L"WQL");
    BSTR q  = SysAllocString(L"SELECT CurrentBrightness, Level FROM WmiMonitorBrightness");
    HRESULT hr = g_svc->ExecQuery(ql, q, WBEM_FLAG_FORWARD_ONLY, nullptr, &en);
    SysFreeString(ql); SysFreeString(q);
    if (FAILED(hr) || !en) return false;

    IWbemClassObject* obj = nullptr; ULONG ret = 0;
    hr = en->Next(5000, 1, &obj, &ret);
    en->Release();
    if (FAILED(hr) || ret == 0 || !obj) return false;

    VARIANT v; VariantInit(&v);
    if (SUCCEEDED(obj->Get(L"Level", 0, &v, nullptr, nullptr)) && (v.vt & VT_ARRAY)) {
        SAFEARRAY* sa = v.parray; long lo = 0, hi = 0;
        SafeArrayGetLBound(sa, 1, &lo); SafeArrayGetUBound(sa, 1, &hi);
        int mn = 101;
        for (long i = lo; i <= hi; i++) {
            VARIANT e; VariantInit(&e);
            if (SUCCEEDED(SafeArrayGetElement(sa, &i, &e))) {
                long val = (e.vt == VT_I4) ? e.lVal : (e.vt == VT_UI1 ? e.bVal : 101);
                if (val < mn) mn = (int)val;
            }
            VariantClear(&e);
        }
        if (mn >= 0 && mn <= 100 && mn > 3) {
            g.hwFloor = mn;
            g.hwFloorNoted = true;
        }
    }
    VariantClear(&v);
    obj->Release();
    return true;
}

int BrightRawToLogical(int raw) {
    if (raw < 0) return -1;
    // 硬件最低值即代表逻辑 0% (例如硬件最小 5 -> 界面显示 0, 而不是 5)
    return raw <= g.hwFloor ? 0 : raw;
}

// ---- 与 Windows 自身亮度状态同步 ----
// Windows 把"当前屏幕亮度"存在当前电源方案里 (显示子组 SUB_VIDEO, 设置"显示器亮度",
// 交流/直流各一份)。WmiSetBrightness / SetMonitorBrightness 只改面板, 不改这个值,
// 于是睡眠唤醒、重启、插拔电源时系统会拿旧值重新下发, 把我们的设置覆盖掉。
// 这里把同一个值写进当前方案 (用户级电源方案, 不需要管理员), 双方就百分百一致。
static const GUID SUB_VIDEO = { 0x7516b95f, 0xf776, 0x4464,
                                { 0x8c, 0x53, 0x06, 0x16, 0x7f, 0x40, 0xcc, 0x99 } };
static const GUID SET_BRIGHT  = { 0xaded5e82, 0xb909, 0x4619,
                                  { 0x99, 0x49, 0xf5, 0xd7, 0x1d, 0xac, 0x0b, 0xcb } };
static const GUID SET_DIMMED  = { 0xf1fbfde2, 0xa960, 0x4165,
                                  { 0x9f, 0x88, 0x50, 0x66, 0x79, 0x11, 0xce, 0x96 } };

bool BrightSyncSystem(int logicalValue) {
    int v = (logicalValue <= 0) ? g.hwFloor : logicalValue;
    if (v < 0) v = 0; if (v > 100) v = 100;

    GUID* scheme = nullptr;
    if (PowerGetActiveScheme(nullptr, &scheme) != ERROR_SUCCESS || !scheme) return false;

    // 方案里已经是同一个值: 什么都不做 (避免每次回读同步都重设一次电源方案)
    DWORD ac = 0, dc = 0;
    if (PowerReadACValueIndex(nullptr, scheme, &SUB_VIDEO, &SET_BRIGHT, &ac) == ERROR_SUCCESS &&
        PowerReadDCValueIndex(nullptr, scheme, &SUB_VIDEO, &SET_BRIGHT, &dc) == ERROR_SUCCESS &&
        (int)ac == v && (int)dc == v) {
        LocalFree(scheme);
        return true;
    }

    bool ok = true;
    ok &= PowerWriteACValueIndex(nullptr, scheme, &SUB_VIDEO, &SET_BRIGHT, (DWORD)v) == ERROR_SUCCESS;
    ok &= PowerWriteDCValueIndex(nullptr, scheme, &SUB_VIDEO, &SET_BRIGHT, (DWORD)v) == ERROR_SUCCESS;
    ok &= PowerWriteACValueIndex(nullptr, scheme, &SUB_VIDEO, &SET_DIMMED, (DWORD)v) == ERROR_SUCCESS;
    ok &= PowerWriteDCValueIndex(nullptr, scheme, &SUB_VIDEO, &SET_DIMMED, (DWORD)v) == ERROR_SUCCESS;
    // 让系统立刻采纳新值 (等价于 powercfg /setactive 当前方案)
    ok &= PowerSetActiveScheme(nullptr, scheme) == ERROR_SUCCESS;
    LocalFree(scheme);
    return ok;
}

bool BrightInit() {
    if (InitDxva2()) {
        if (g_min > 3) { g.hwFloor = g_min; g.hwFloorNoted = true; }
        return true;
    }
    return InitWmi();
}

// 回读原始亮度 0..100; 失败 -1
int BrightRead() {
    if (g_usePm) {
        DWORD mn = 0, cu = 0, mx = 0;
        if (!GetMonitorBrightness(g_pm.hPhysicalMonitor, &mn, &cu, &mx)) return -1;
        return (int)cu;
    }
    if (!g_svc) return -1;

    IEnumWbemClassObject* en = nullptr;
    BSTR ql = SysAllocString(L"WQL");
    BSTR q  = SysAllocString(L"SELECT CurrentBrightness FROM WmiMonitorBrightness");
    HRESULT hr = g_svc->ExecQuery(ql, q, WBEM_FLAG_FORWARD_ONLY, nullptr, &en);
    SysFreeString(ql); SysFreeString(q);
    if (FAILED(hr) || !en) return -1;

    IWbemClassObject* obj = nullptr; ULONG ret = 0; int result = -1;
    if (SUCCEEDED(en->Next(5000, 1, &obj, &ret)) && ret && obj) {
        VARIANT v; VariantInit(&v);
        if (SUCCEEDED(obj->Get(L"CurrentBrightness", 0, &v, nullptr, nullptr)) && v.vt == VT_UI1)
            result = v.bVal;
        VariantClear(&v);
        obj->Release();
    }
    en->Release();
    return result;
}

// 下发: 逻辑 0 -> hwFloor; 其余原样下发 (硬件自行贴合最近档位)
bool BrightWrite(int logicalValue) {
    int send = (logicalValue <= 0) ? g.hwFloor : logicalValue;
    if (send > 100) send = 100;

    if (g_usePm) {
        if (send < g_min) send = g_min;
        if (send > g_max) send = g_max;
        return SetMonitorBrightness(g_pm.hPhysicalMonitor, (DWORD)send) != FALSE;
    }
    if (!g_svc) return false;

    // 取 WmiMonitorBrightnessMethods 第一个实例路径
    IEnumWbemClassObject* en = nullptr;
    BSTR ql = SysAllocString(L"WQL");
    BSTR q  = SysAllocString(L"SELECT * FROM WmiMonitorBrightnessMethods");
    HRESULT hr = g_svc->ExecQuery(ql, q, WBEM_FLAG_FORWARD_ONLY, nullptr, &en);
    SysFreeString(ql); SysFreeString(q);
    if (FAILED(hr) || !en) return false;

    IWbemClassObject* inst = nullptr; ULONG ret = 0; BSTR path = nullptr;
    if (SUCCEEDED(en->Next(5000, 1, &inst, &ret)) && ret && inst) {
        VARIANT v; VariantInit(&v);
        if (SUCCEEDED(inst->Get(L"__PATH", 0, &v, nullptr, nullptr)) && v.vt == VT_BSTR)
            path = SysAllocString(v.bstrVal);
        VariantClear(&v);
        inst->Release();
    }
    en->Release();
    if (!path) return false;

    // 方法输入参数类: WmiMonitorBrightnessMethods -> GetMethod("WmiSetBrightness") -> in-params
    IWbemClassObject* cls = nullptr;
    BSTR clsName = SysAllocString(L"WmiMonitorBrightnessMethods");
    hr = g_svc->GetObject(clsName, 0, nullptr, &cls, nullptr);
    SysFreeString(clsName);
    if (FAILED(hr) || !cls) { SysFreeString(path); return false; }

    IWbemClassObject* inCls = nullptr;
    BSTR mname = SysAllocString(L"WmiSetBrightness");
    hr = cls->GetMethod(mname, 0, &inCls, nullptr);
    SysFreeString(mname);
    cls->Release();
    if (FAILED(hr) || !inCls) { SysFreeString(path); return false; }

    IWbemClassObject* in = nullptr;
    inCls->SpawnInstance(0, &in);
    inCls->Release();
    if (!in) { SysFreeString(path); return false; }

    VARIANT v; VariantInit(&v);
    v.vt = VT_I4; v.lVal = 0;                       // Timeout = 0
    in->Put(L"Timeout", 0, &v, 0);
    VariantClear(&v);
    v.vt = VT_UI1; v.bVal = (BYTE)send;             // Brightness (uint8)
    in->Put(L"Brightness", 0, &v, 0);
    VariantClear(&v);

    BSTR method = SysAllocString(L"WmiSetBrightness");
    hr = g_svc->ExecMethod(path, method, 0, nullptr, in, nullptr, nullptr);
    SysFreeString(method);
    SysFreeString(path);
    in->Release();
    return SUCCEEDED(hr);
}
