#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <shellapi.h>
#include <netfw.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <comdef.h>
#include <shlwapi.h>
#include <commdlg.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <dwmapi.h>
#include <windowsx.h> // GET_X_LPARAM, GET_Y_LPARAM için
#include <commctrl.h>
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")

using namespace Microsoft::WRL;

// Struct tanımlamaları
struct AppRule {
    std::wstring name;
    std::wstring path;
    bool blocked;
};

struct MenuItemInfo {
    std::wstring text;
    bool isChecked;
    bool isSeparator;
    UINT_PTR id;
};

// Global değişkenler
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAYICON 1001

HINSTANCE g_hInst = nullptr;
NOTIFYICONDATA g_nid = {0};
std::vector<AppRule> g_apps;
ComPtr<INetFwPolicy2> g_fwPolicy;

// Modern Menu sınıfı
class ModernMenu {
private:
    HWND m_hwndParent;
    HWND m_hwnd;
    std::vector<MenuItemInfo> m_items;
    bool m_isVisible;
    int m_width;
    int m_itemHeight;
    HFONT m_font;
    HBRUSH m_bgBrush;
    HBRUSH m_hoverBrush;
    int m_hoverIndex;

public:
    ModernMenu(HWND parent) : 
        m_hwndParent(parent),
        m_hwnd(nullptr),
        m_isVisible(false),
        m_width(250),
        m_itemHeight(40),
        m_hoverIndex(-1) {
        
        // Modern font oluştur
        m_font = CreateFont(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        // Fırçalar
        m_bgBrush = CreateSolidBrush(RGB(32, 32, 32));
        m_hoverBrush = CreateSolidBrush(RGB(64, 64, 64));

        // Pencere sınıfı
        RegisterMenuClass();
    }

    ~ModernMenu() {
        DeleteObject(m_font);
        DeleteObject(m_bgBrush);
        DeleteObject(m_hoverBrush);
    }

    void AddItem(const std::wstring& text, UINT_PTR id, bool checked = false) {
        MenuItemInfo item;
        item.text = text;
        item.id = id;
        item.isChecked = checked;
        item.isSeparator = false;
        m_items.push_back(item);
    }

    void AddSeparator() {
        MenuItemInfo item;
        item.isSeparator = true;
        m_items.push_back(item);
    }

    void Show(int x, int y) {
        if (m_isVisible) return;

        RECT rc = {x, y, x + m_width, y + (int)(m_items.size() * m_itemHeight)};
        AdjustWindowRectEx(&rc, WS_POPUP, FALSE, WS_EX_LAYERED | WS_EX_TOPMOST);

        m_hwnd = CreateWindowEx(
            WS_EX_LAYERED | WS_EX_TOPMOST,
            L"ModernMenuClass", nullptr,
            WS_POPUP,
            rc.left, rc.top,
            rc.right - rc.left,
            rc.bottom - rc.top,
            m_hwndParent, nullptr,
            g_hInst, this
        );

        SetLayeredWindowAttributes(m_hwnd, 0, 245, LWA_ALPHA);
        ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
        m_isVisible = true;
    }

private:
    static LRESULT CALLBACK MenuWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        ModernMenu* menu = nullptr;
        if (msg == WM_CREATE) {
            menu = (ModernMenu*)((LPCREATESTRUCT)lParam)->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)menu);
        } else {
            menu = (ModernMenu*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        }

        if (menu) return menu->HandleMessage(hwnd, msg, wParam, lParam);
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    void RegisterMenuClass() {
        WNDCLASSEX wc = {0};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.lpfnWndProc = MenuWndProc;
        wc.hInstance = g_hInst;
        wc.lpszClassName = L"ModernMenuClass";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassEx(&wc);
    }

    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                OnPaint(hdc);
                EndPaint(hwnd, &ps);
                return 0;
            }

            case WM_MOUSEMOVE: {
                int y = GET_Y_LPARAM(lParam);
                int newHover = y / m_itemHeight;
                if (newHover != m_hoverIndex) {
                    m_hoverIndex = newHover;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }

            case WM_LBUTTONUP: {
                int index = GET_Y_LPARAM(lParam) / m_itemHeight;
                if (index >= 0 && index < (int)m_items.size()) {
                    SendMessage(m_hwndParent, WM_COMMAND, m_items[index].id, 0);
                }
                DestroyWindow(hwnd);
                return 0;
            }

            case WM_DESTROY:
                m_isVisible = false;
                return 0;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    void OnPaint(HDC hdc) {
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        
        // Modern Windows 11 tarzı arka plan 
        SetDCBrushColor(hdc, RGB(32, 32, 32));
        FillRect(hdc, &rc, (HBRUSH)GetStockObject(DC_BRUSH));
        
        // Gölge efekti
        MARGINS margins = {1, 1, 1, 1};
        DwmExtendFrameIntoClientArea(m_hwnd, &margins);
        
        // Menü öğelerini çiz
        for (size_t i = 0; i < m_items.size(); i++) {
            DrawMenuItem(hdc, i);
        }
    }
    
    void DrawMenuItem(HDC hdc, size_t index) {
        const auto& item = m_items[index];
        RECT rcItem = {0, (int)(index * m_itemHeight), m_width, (int)((index + 1) * m_itemHeight)};
        
        // Hover efekti
        if ((int)index == m_hoverIndex) {
            SetDCBrushColor(hdc, RGB(64, 64, 64));
            FillRect(hdc, &rcItem, (HBRUSH)GetStockObject(DC_BRUSH));
        }
        
        if (item.isSeparator) {
            DrawSeparator(hdc, rcItem);
        } else {
            DrawMenuText(hdc, item, rcItem);
            if (item.isChecked) {
                DrawCheckMark(hdc, rcItem);
            }
        }
    }

    void DrawSeparator(HDC hdc, const RECT& rc) {
        RECT sepRect = rc;
        sepRect.top += m_itemHeight / 2 - 1;
        sepRect.bottom = sepRect.top + 1;
        sepRect.left += 10;
        sepRect.right -= 10;
        SetDCBrushColor(hdc, RGB(80, 80, 80));
        FillRect(hdc, &sepRect, (HBRUSH)GetStockObject(DC_BRUSH));
    }

    void DrawMenuText(HDC hdc, const MenuItemInfo& item, const RECT& rc) {
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, TRANSPARENT);
        SelectObject(hdc, m_font);
        RECT textRect = rc;
        textRect.left += 15;
        DrawText(hdc, item.text.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    }

    void DrawCheckMark(HDC hdc, const RECT& rc) {
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT checkRect = rc;
        checkRect.left += 5;
        DrawText(hdc, L"✓", -1, &checkRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    }

public:
    HWND GetHandle() const { return m_hwnd; }
    
    void SetTheme(LPCWSTR themeName) {
        if (m_hwnd) {
            SetWindowTheme(m_hwnd, themeName, nullptr);
        }
    }
};

// Firewall başlatma
bool InitializeFirewall() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return false;

    hr = CoCreateInstance(__uuidof(NetFwPolicy2), nullptr,
        CLSCTX_INPROC_SERVER, __uuidof(INetFwPolicy2), 
        reinterpret_cast<void**>(g_fwPolicy.GetAddressOf()));
    
    return SUCCEEDED(hr);
}

// Firewall kuralını silme fonksiyonu
bool RemoveFirewallRule(const std::wstring& name) {
    ComPtr<INetFwRules> pFwRules;
    HRESULT hr = g_fwPolicy->get_Rules(reinterpret_cast<INetFwRules**>(pFwRules.GetAddressOf()));
    if (FAILED(hr)) return false;

    hr = pFwRules->Remove(_bstr_t(name.c_str()));
    return SUCCEEDED(hr);
}

// Firewall kuralı ekleme
bool AddFirewallRule(const std::wstring& name, const std::wstring& path, bool block) {
    // Önce varolan kuralı sil
    RemoveFirewallRule(name);

    // Yeni kuralı ekle
    ComPtr<INetFwRules> pFwRules;
    HRESULT hr = g_fwPolicy->get_Rules(reinterpret_cast<INetFwRules**>(pFwRules.GetAddressOf()));
    if (FAILED(hr)) return false;

    ComPtr<INetFwRule> pFwRule;
    hr = CoCreateInstance(__uuidof(NetFwRule), nullptr,
        CLSCTX_INPROC_SERVER, __uuidof(INetFwRule),
        reinterpret_cast<void**>(pFwRule.GetAddressOf()));
    if (FAILED(hr)) return false;

    pFwRule->put_Name(_bstr_t(name.c_str()));
    pFwRule->put_ApplicationName(_bstr_t(path.c_str()));
    pFwRule->put_Action(block ? NET_FW_ACTION_BLOCK : NET_FW_ACTION_ALLOW);
    pFwRule->put_Enabled(VARIANT_TRUE);
    pFwRule->put_Direction(NET_FW_RULE_DIR_OUT);

    hr = pFwRules->Add(pFwRule.Get());
    return SUCCEEDED(hr);
}

// Registry işlemleri için fonksiyonlar
void SaveAppToRegistry(const AppRule& app) {
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\NetBlocker\\Apps", 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        
        RegSetValueEx(hKey, app.name.c_str(), 0, REG_SZ,
            (BYTE*)app.path.c_str(), (app.path.length() + 1) * sizeof(wchar_t));
        
        DWORD blocked = app.blocked ? 1 : 0;
        RegSetValueEx(hKey, (app.name + L"_blocked").c_str(), 0, REG_DWORD,
            (BYTE*)&blocked, sizeof(DWORD));
        
        RegCloseKey(hKey);
    }
}

void LoadAppsFromRegistry() {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\NetBlocker\\Apps", 0,
        KEY_READ, &hKey) == ERROR_SUCCESS) {
        
        WCHAR valueName[256];
        WCHAR valueData[MAX_PATH];
        DWORD valueNameSize, valueDataSize, valueType;
        DWORD index = 0;

        while (true) {
            valueNameSize = 256;
            valueDataSize = MAX_PATH * sizeof(WCHAR);
            
            if (RegEnumValue(hKey, index, valueName, &valueNameSize, nullptr,
                &valueType, (BYTE*)valueData, &valueDataSize) != ERROR_SUCCESS) {
                break;
            }

            if (valueType == REG_SZ && !wcsstr(valueName, L"_blocked")) {
                AppRule rule;
                rule.name = valueName;
                rule.path = valueData;

                // Blocked durumunu oku
                DWORD blocked;
                DWORD blockedSize = sizeof(DWORD);
                if (RegQueryValueEx(hKey, (rule.name + L"_blocked").c_str(), nullptr,
                    nullptr, (BYTE*)&blocked, &blockedSize) == ERROR_SUCCESS) {
                    rule.blocked = blocked == 1;
                }

                g_apps.push_back(rule);
            }
            index++;
        }
        RegCloseKey(hKey);
    }
}

void DeleteAppFromRegistry(const std::wstring& name) {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\NetBlocker\\Apps", 0,
        KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        
        RegDeleteValue(hKey, name.c_str());
        RegDeleteValue(hKey, (name + L"_blocked").c_str());
        RegCloseKey(hKey);
    }
}

// Registry'den silme fonksiyonu
void RemoveAppFromRegistry(const std::wstring& name) {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\NetBlocker\\Apps", 0,
        KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValue(hKey, name.c_str());
        RegDeleteValue(hKey, (name + L"_blocked").c_str());
        RegCloseKey(hKey);
    }
}

// Uygulama ekleme diyalogu
void AddApplication(HWND hwnd) {
    OPENFILENAME ofn = {0};
    WCHAR szFile[MAX_PATH] = {0};
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"Executable\0*.exe\0All\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        AppRule rule;
        rule.path = szFile;
        rule.name = PathFindFileName(szFile);
        rule.blocked = false;
        
        g_apps.push_back(rule);
        AddFirewallRule(rule.name, rule.path, false);
        SaveAppToRegistry(rule);
    }
}

// Uygulama silme fonksiyonu
void RemoveApplication(size_t index) {
    if (index < g_apps.size()) {
        RemoveFirewallRule(g_apps[index].name);
        DeleteAppFromRegistry(g_apps[index].name);
        g_apps.erase(g_apps.begin() + index);
    }
}

// Context menu güncelleme
void ShowContextMenu(HWND hwnd, POINT pt) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, 1, L"Uygulama Ekle");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    
    // Her uygulama için menü öğeleri
    for (size_t i = 0; i < g_apps.size(); i++) {
        // Ana öğe - Uygulama adı ve engelleme durumu
        AppendMenu(hMenu, MF_STRING | (g_apps[i].blocked ? MF_CHECKED : MF_UNCHECKED),
            100 + i, g_apps[i].name.c_str());
        
        // Silme seçeneği - girinti ile
        std::wstring removeText = L"   └─ Kaldır: " + g_apps[i].name;
        AppendMenu(hMenu, MF_STRING, 1000 + i, removeText.c_str());
    }
    
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, 2, L"Çıkış");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    PostMessage(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

// Window Procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // Tray icon ekleme
            g_nid.cbSize = sizeof(NOTIFYICONDATA);
            g_nid.hWnd = hwnd;
            g_nid.uID = ID_TRAYICON;
            g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            g_nid.uCallbackMessage = WM_TRAYICON;
            g_nid.hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(1));
            wcscpy_s(g_nid.szTip, L"NetBlocker");
            
            if (!Shell_NotifyIcon(NIM_ADD, &g_nid)) {
                MessageBox(hwnd, L"Tray icon eklenemedi!", L"Hata", MB_ICONERROR);
            }
            break;
        }

        case WM_TRAYICON: {
        if (wParam == ID_TRAYICON) {
            switch (LOWORD(lParam)) {
                case WM_RBUTTONDOWN: {
                    POINT pt;
                    GetCursorPos(&pt);
                    
                    HMENU hMenu = CreatePopupMenu();
                    AppendMenu(hMenu, MF_STRING, 1, L"Uygulama Ekle");
                    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
                    
                    // Her uygulama için alt menü oluştur
                    for (size_t i = 0; i < g_apps.size(); i++) {
                        HMENU hSubMenu = CreatePopupMenu();
                        
                        // Engelle seçeneği
                        AppendMenu(hSubMenu, MF_STRING | (g_apps[i].blocked ? MF_CHECKED : MF_UNCHECKED),
                            100 + i, L"Engelle");
                        
                        // Ayırıcı çizgi
                        AppendMenu(hSubMenu, MF_SEPARATOR, 0, NULL);
                        
                        // Sil seçeneği
                        AppendMenu(hSubMenu, MF_STRING, 1000 + i, L"Sil");
                        
                        // Alt menüyü ana menüye ekle
                        AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hSubMenu, g_apps[i].name.c_str());
                    }
                    
                    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
                    AppendMenu(hMenu, MF_STRING, 2, L"Çıkış");

                    SetForegroundWindow(hwnd);
                    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN,
                        pt.x, pt.y, 0, hwnd, NULL);
                    PostMessage(hwnd, WM_NULL, 0, 0);
                    DestroyMenu(hMenu);
                    return 0;
                }
            }
        }
        break;
    }

        case WM_COMMAND: {
            UINT id = LOWORD(wParam);
            if (id == 1) {
                AddApplication(hwnd);
            }
            else if (id == 2) {
                DestroyWindow(hwnd);
            }
            else if (id >= 100 && id < 100 + g_apps.size()) {
                size_t index = id - 100;
                g_apps[index].blocked = !g_apps[index].blocked;
                AddFirewallRule(g_apps[index].name, g_apps[index].path, g_apps[index].blocked);
                SaveAppToRegistry(g_apps[index]);
            }
            else if (id >= 1000 && id < 1000 + g_apps.size()) {
                // Silme işlemi
                size_t index = id - 1000;
                RemoveFirewallRule(g_apps[index].name);
                RemoveAppFromRegistry(g_apps[index].name);
                g_apps.erase(g_apps.begin() + index);
            }
            break;
        }

        case WM_DESTROY:
            Shell_NotifyIcon(NIM_DELETE, &g_nid);
            PostQuitMessage(0);
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    g_hInst = hInstance;
    SetProcessDPIAware(); // DPI farkındalığını etkinleştir
    
    g_hInst = hInstance;
    
    if (!InitializeFirewall()) {
        MessageBox(nullptr, L"Firewall başlatılamadı!", L"Hata", MB_ICONERROR);
        return 1;
    }

    // Registry'den uygulamaları yükle
    LoadAppsFromRegistry();

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"NetBlockerClass";
    RegisterClassEx(&wc);

// Pencere oluşturma kodunda değişiklik
HWND hwnd = CreateWindowEx(0, L"NetBlockerClass", L"NetBlocker",
    WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
    400, 300, nullptr, nullptr, hInstance, nullptr);

if (!hwnd) {
    MessageBox(nullptr, L"Pencere oluşturulamadı!", L"Hata", MB_ICONERROR);
    return 1;
}

ShowWindow(hwnd, SW_HIDE);
UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();
    return 0;
}