// brightness.cpp - 亮度读写
// 首选 Dxva2 物理监视器亮度接口 (轻量, 无 COM/WMI 运行时); 不被支持时才回退 WMI。
#include "app.h"
#include <physicalmonitorenumerationapi.h>
#include <lowlevelmonitorconfigurationapi.h>
#include <highlevelmonitorconfigurationapi.h>
#include <wbemidl.h>
#include <oleauto.h>

#pragma comment(lib, "dxva2.lib")
#pragma comment(lib, "wbemuuid.lib")

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
    return raw <= 3 ? 0 : raw;
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
