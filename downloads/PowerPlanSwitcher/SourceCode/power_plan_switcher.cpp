#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <powrprof.h>
#include <commctrl.h>
#include <vector>
#include <string>
#include <algorithm>

#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Global variables
HWND g_hwnd = NULL;
HMENU g_popup_menu = NULL;
NOTIFYICONDATAW g_nid = {};
std::vector<std::pair<std::wstring, GUID>> g_power_plans;
HICON g_hIcon = NULL;

// Function declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateTrayIcon(HWND hwnd);
void RemoveTrayIcon();
void ShowContextMenu(HWND hwnd);
void GetPowerPlans();
void SwitchToPowerPlan(const GUID* guidPtr);
void RefreshPowerPlans();

void GetPowerPlans() {
    g_power_plans.clear();

    // Not: Bu GUID'ler her Windows sisteminde bulunan varsayılan güç planlarıdır
    const GUID GUID_BALANCED_POWER_PLAN = { 0x381b4222, 0xf694, 0x41f0, {0x96, 0x85, 0xff, 0x5b, 0xb2, 0x60, 0xdf, 0x2e} };
    const GUID GUID_HIGH_PERFORMANCE_POWER_PLAN = { 0x8c5e7fda, 0xe8bf, 0x4a96, {0x9a, 0x85, 0xa6, 0xe2, 0x3a, 0x8c, 0x63, 0x5c} };
    const GUID GUID_POWER_SAVER_POWER_PLAN = { 0xa1841308, 0x3541, 0x4fab, {0xbc, 0x81, 0xf7, 0x15, 0x56, 0xf2, 0x0b, 0x4a} };
    
    WCHAR planName[MAX_PATH] = { 0 };
    DWORD nameSize = sizeof(planName);

    // Önce aktif planı al
    GUID* activeGuid = nullptr;
    if (PowerGetActiveScheme(NULL, &activeGuid) == ERROR_SUCCESS) {
        nameSize = sizeof(planName);
        if (PowerReadFriendlyName(NULL, activeGuid, NULL, NULL, (UCHAR*)planName, &nameSize) == ERROR_SUCCESS) {
            g_power_plans.emplace_back(std::wstring(planName) + L" (Aktif)", *activeGuid);
        }
        LocalFree(activeGuid);
    }

    // Varsayılan güç planlarını ekle (eğer varsa)
    // Dengeli Plan
    nameSize = sizeof(planName);
    if (PowerReadFriendlyName(NULL, &GUID_BALANCED_POWER_PLAN, NULL, NULL, (UCHAR*)planName, &nameSize) == ERROR_SUCCESS) {
        bool isActive = false;
        if (activeGuid && memcmp(&GUID_BALANCED_POWER_PLAN, activeGuid, sizeof(GUID)) == 0) {
            isActive = true;
        }
        if (!isActive) {
            g_power_plans.emplace_back(planName, GUID_BALANCED_POWER_PLAN);
        }
    }

    // Yüksek Performans Planı
    nameSize = sizeof(planName);
    if (PowerReadFriendlyName(NULL, &GUID_HIGH_PERFORMANCE_POWER_PLAN, NULL, NULL, (UCHAR*)planName, &nameSize) == ERROR_SUCCESS) {
        bool isActive = false;
        if (activeGuid && memcmp(&GUID_HIGH_PERFORMANCE_POWER_PLAN, activeGuid, sizeof(GUID)) == 0) {
            isActive = true;
        }
        if (!isActive) {
            g_power_plans.emplace_back(planName, GUID_HIGH_PERFORMANCE_POWER_PLAN);
        }
    }

    // Güç Tasarrufu Planı
    nameSize = sizeof(planName);
    if (PowerReadFriendlyName(NULL, &GUID_POWER_SAVER_POWER_PLAN, NULL, NULL, (UCHAR*)planName, &nameSize) == ERROR_SUCCESS) {
        bool isActive = false;
        if (activeGuid && memcmp(&GUID_POWER_SAVER_POWER_PLAN, activeGuid, sizeof(GUID)) == 0) {
            isActive = true;
        }
        if (!isActive) {
            g_power_plans.emplace_back(planName, GUID_POWER_SAVER_POWER_PLAN);
        }
    }

    // Varsa diğer özel planları ekle
    GUID* schemes = nullptr;
    DWORD schemeIndex = 0;
    DWORD schemeBufSize = 0;

    while (PowerEnumerate(NULL, NULL, NULL, ACCESS_SCHEME, schemeIndex, 
           (UCHAR*)&schemes, &schemeBufSize) == ERROR_SUCCESS) {
        nameSize = sizeof(planName);
        if (PowerReadFriendlyName(NULL, schemes, NULL, NULL, (UCHAR*)planName, &nameSize) == ERROR_SUCCESS) {
            // Varsayılan planlardan biri değilse ve aktif plan değilse ekle
            if (memcmp(schemes, &GUID_BALANCED_POWER_PLAN, sizeof(GUID)) != 0 &&
                memcmp(schemes, &GUID_HIGH_PERFORMANCE_POWER_PLAN, sizeof(GUID)) != 0 &&
                memcmp(schemes, &GUID_POWER_SAVER_POWER_PLAN, sizeof(GUID)) != 0) {
                
                bool isActive = false;
                if (activeGuid && memcmp(schemes, activeGuid, sizeof(GUID)) == 0) {
                    isActive = true;
                }
                if (!isActive) {
                    g_power_plans.emplace_back(planName, *schemes);
                }
            }
        }
        schemeIndex++;
    }
}

void CreateTrayIcon(HWND hwnd) {
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_USER + 1;
    
    // Energy icon from shell32.dll
    if (ExtractIconExW(L"powercpl.dll", 1, &g_hIcon, NULL, 1) > 0 && g_hIcon) {
        g_nid.hIcon = g_hIcon;
    }
    else {
        g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }
    
    wcscpy_s(g_nid.szTip, L"Power Plan Switcher");
    Shell_NotifyIcon(NIM_ADD, &g_nid);
}

void RemoveTrayIcon() {
    if (g_hIcon) {
        DestroyIcon(g_hIcon);
    }
    Shell_NotifyIcon(NIM_DELETE, &g_nid);
}

void RefreshPowerPlans() {
    GetPowerPlans();
    if (g_popup_menu) {
        DestroyMenu(g_popup_menu);
        g_popup_menu = NULL;
    }
}

void ShowContextMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    // önce eski menüyü temizle
    if (g_popup_menu) {
        DestroyMenu(g_popup_menu);
    }
    g_popup_menu = hMenu;

    if (g_power_plans.empty()) {
        InsertMenuW(g_popup_menu, static_cast<UINT>(-1), MF_BYPOSITION | MF_STRING | MF_DISABLED, 
                   0, L"Güç planları bulunamadı");
    }
    else {
        for (size_t i = 0; i < g_power_plans.size(); i++) {
            UINT flags = MF_BYPOSITION | MF_STRING;
            if (i == 0) flags |= MF_CHECKED;
            
            InsertMenuW(g_popup_menu, static_cast<UINT>(-1), flags,
                       1000U + i, g_power_plans[i].first.c_str());
        }
    }

    InsertMenuW(g_popup_menu, static_cast<UINT>(-1), MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    InsertMenuW(g_popup_menu, static_cast<UINT>(-1), MF_BYPOSITION | MF_STRING, WM_CLOSE, L"Çıkış");
    InsertMenuW(g_popup_menu, static_cast<UINT>(-1), MF_BYPOSITION | MF_STRING, WM_CLOSE, L"Sonraki Söz");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(g_popup_menu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN,
                   pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0);
}

void SwitchToPowerPlan(const GUID* guidPtr) {
    if (guidPtr && PowerSetActiveScheme(NULL, guidPtr) == ERROR_SUCCESS) {
        RefreshPowerPlans();
        if (g_nid.hWnd) {
            Shell_NotifyIcon(NIM_MODIFY, &g_nid);
        }
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    INITCOMMONCONTROLSEX icc = {
        sizeof(INITCOMMONCONTROLSEX),
        ICC_WIN95_CLASSES
    };
    InitCommonControlsEx(&icc);

    const wchar_t CLASS_NAME[] = L"PowerPlanSwitcherClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClass(&wc);

    g_hwnd = CreateWindowEx(
        0, CLASS_NAME, L"Power Plan Switcher",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL
    );

    if (g_hwnd == NULL) {
        return 0;
    }

    CreateTrayIcon(g_hwnd);
    GetPowerPlans();

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            RemoveTrayIcon();
            PostQuitMessage(0);
            return 0;

        case WM_USER + 1:
            if (LOWORD(lParam) == WM_RBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                RefreshPowerPlans();
                ShowContextMenu(hwnd);
            }
            return 0;

        case WM_COMMAND: 
            if (LOWORD(wParam) >= 1000 && LOWORD(wParam) < 1000 + g_power_plans.size()) {
                SwitchToPowerPlan(&g_power_plans[LOWORD(wParam) - 1000].second);
            }
            else if (LOWORD(wParam) == WM_CLOSE) {
                PostMessage(hwnd, WM_CLOSE, 0, 0);
            }
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}