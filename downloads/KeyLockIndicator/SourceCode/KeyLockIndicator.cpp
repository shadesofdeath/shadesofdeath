#define UNICODE
#define _UNICODE
#include <windows.h>
#include <string>

// Global variables
HWND g_hwnd = NULL;
HHOOK g_hook = NULL;
HINSTANCE g_hInstance = NULL;
DWORD g_lastKeyTime = 0;
bool g_isVisible = false;
UINT_PTR g_currentTimer = 0;
int g_windowSize = 300; // Global pencere boyutu

const UINT_PTR HIDE_TIMER_ID = 1;

enum class IndicatorType
{
    CAPS,
    NUM,
    SCROLL
};

// Store current indicator type
IndicatorType g_currentType = IndicatorType::CAPS;

void DrawStateText(HDC hdc, RECT rect, bool isOn)
{
    SetBkMode(hdc, TRANSPARENT);
    int dpi = GetDpiForWindow(WindowFromDC(hdc));

    // Kare arka plan çizimi
    int boxSize = rect.bottom - rect.top - MulDiv(10, dpi, 400); // değeri AZ büyütüldü (40 yerine 20)
    int boxLeft = (rect.right - rect.left - boxSize) / 2;  // merkeze al
    int boxTop = (rect.bottom - rect.top - boxSize) / 3;

    // BÜYÜK font için yeni yaklaşım
    LOGFONT lf = {0};
    lf.lfHeight = -(boxSize * 1 / 2); // 1/2'den 2/5'e küçültüldü
    lf.lfWidth = 0;
    lf.lfEscapement = 0;
    lf.lfOrientation = 0;
    lf.lfWeight = FW_ULTRALIGHT; // En kalın font
    lf.lfItalic = FALSE;
    lf.lfUnderline = FALSE;
    lf.lfStrikeOut = FALSE;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_TT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = CLEARTYPE_NATURAL_QUALITY; // En iyi font kalitesi
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");

    HFONT hFont = CreateFontIndirect(&lf);
    SelectObject(hdc, hFont);
    SetTextColor(hdc, RGB(255, 255, 255));

    std::wstring text;
    switch (g_currentType)
    {
    case IndicatorType::CAPS:
        text = isOn ? L"AA" : L"aa";
        break;
    case IndicatorType::NUM:
        text = isOn ? L"123" : L"abc";
        break;
    case IndicatorType::SCROLL:
        text = isOn ? L"SCRL" : L"scrl";
        break;
    }

    // Şeffaf arka plan için
    BYTE alpha = 160;
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA};

    HDC hdcMem = CreateCompatibleDC(hdc);
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = boxSize;
    bmi.bmiHeader.biHeight = -boxSize;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void *pvBits;
    HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0);
    SelectObject(hdcMem, hBitmap);

    // Yuvarlatılmış köşeli dikdörtgen
    BeginPath(hdcMem);
    int radius = MulDiv(16, dpi, 300);
    RoundRect(hdcMem, 0, 0, boxSize, boxSize, radius, radius);
    EndPath(hdcMem);

    HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
    SelectObject(hdcMem, hBrush);
    FillPath(hdcMem);
    DeleteObject(hBrush);

    AlphaBlend(hdc, boxLeft, boxTop, boxSize, boxSize,
               hdcMem, 0, 0, boxSize, boxSize, blend);

    // Metni ortala ve çiz
    SetTextAlign(hdc, TA_CENTER | TA_BASELINE);

    // Önce metin boyutunu al
    SIZE textSize;
    GetTextExtentPoint32(hdc, text.c_str(), text.length(), &textSize);

    // Metni kare içinde tam ortala
    int textX = boxLeft + (boxSize / 2);
    int textY = boxTop + (boxSize * 55 / 100); // Biraz daha yukarı

    ExtTextOutW(hdc, textX, textY, 0, NULL, text.c_str(), text.length(), NULL);

    // Temizlik
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    DeleteObject(hFont);
}

void ShowIndicator(HWND hwnd, const std::wstring &text, IndicatorType type)
{
    g_currentType = type;
    SetWindowText(hwnd, text.c_str());

    if (g_currentTimer != 0)
    {
        KillTimer(hwnd, g_currentTimer);
    }

    if (!g_isVisible)
    {
        RECT workArea;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);

        // Ekran çözünürlüğü ve pencere boyutu hesaplamaları
        int dpi = GetDpiForWindow(hwnd);
        int windowSize = MulDiv(g_windowSize, dpi, 300);

        // Ekranın ortasına konumlandırma için hesaplamalar
        // Yatayda tam orta
        int x = (workArea.right - windowSize) / 2;

        // Üst kenara yakın ama biraz boşluk bırakarak
        int y = workArea.top + MulDiv(30, dpi, 300);

        // Göstergeyi konumlandır ve göster
        SetWindowPos(hwnd,
                     HWND_TOPMOST,    // En üstte göster
                     x,               // X pozisyonu
                     y,               // Y pozisyonu
                     windowSize,      // Genişlik
                     windowSize,      // Yükseklik (kare olduğu için aynı)
                     SWP_NOACTIVATE); // Pencereyi aktif yapma

        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        g_isVisible = true;
    }

    // Zamanlayıcıyı ayarla (2 saniye sonra kaybolması için)
    g_currentTimer = SetTimer(hwnd, HIDE_TIMER_ID, 1800, NULL);

    // Pencereyi yeniden çiz
    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        SetWindowLong(hwnd, GWL_STYLE, 0);
        SetWindowLong(hwnd, GWL_EXSTYLE,
                      WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_COMPOSITED);
        SetLayeredWindowAttributes(hwnd, 0, 160, LWA_ALPHA);
        break;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rect;
        GetClientRect(hwnd, &rect);

        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
        HANDLE hOld = SelectObject(hdcMem, hBitmap);

        SetBkMode(hdcMem, TRANSPARENT);

        TCHAR text[256];
        GetWindowText(hwnd, text, 256);
        bool isOn = (wcsstr(text, L"On") != NULL);

        DrawStateText(hdcMem, rect, isOn);

        // Alt metin
        int dpi = GetDpiForWindow(hwnd);
        int fontSize = MulDiv(60, dpi, 300);
        HFONT hFont = CreateFontW(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, FF_DONTCARE, L"Segoe UI");

        SelectObject(hdcMem, hFont);
        SetTextAlign(hdcMem, TA_CENTER);
        SetTextColor(hdcMem, RGB(255, 255, 255));

        // Alt metin için ortalama
        RECT boxRect = rect;
        boxRect.top = rect.bottom - MulDiv(120, dpi, 300); // Alt boşluk ayarı

        // Metin boyutunu al
        SIZE textSize;
        GetTextExtentPoint32(hdcMem, text, wcslen(text), &textSize);

        // Tam ortada çiz
        int textX = (rect.right + rect.left) / 2;
        int textY = boxRect.top + ((rect.bottom - boxRect.top) - textSize.cy) / 2;
        ExtTextOutW(hdcMem, textX, textY, ETO_CLIPPED, &boxRect, text, wcslen(text), NULL);

        BitBlt(hdc, 0, 0, rect.right, rect.bottom, hdcMem, 0, 0, SRCCOPY);

        SelectObject(hdcMem, hOld);
        DeleteObject(hFont);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);

        EndPaint(hwnd, &ps);
        break;
    }

    case WM_TIMER:
        if (wParam == HIDE_TIMER_ID)
        {
            g_isVisible = false;
            ShowWindow(hwnd, SW_HIDE);
            KillTimer(hwnd, HIDE_TIMER_ID);
            g_currentTimer = 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        KBDLLHOOKSTRUCT *pKbStruct = (KBDLLHOOKSTRUCT *)lParam;

        if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
        {
            if (pKbStruct->vkCode == VK_CAPITAL ||
                pKbStruct->vkCode == VK_NUMLOCK ||
                pKbStruct->vkCode == VK_SCROLL)
            {
                std::wstring text;
                IndicatorType type;
                bool isOn = false;

                switch (pKbStruct->vkCode)
                {
                case VK_CAPITAL:
                    isOn = (GetKeyState(VK_CAPITAL) & 1) != 0;
                    text = isOn ? L"Caps Lock On" : L"Caps Lock Off";
                    type = IndicatorType::CAPS;
                    break;
                case VK_NUMLOCK:
                    isOn = (GetKeyState(VK_NUMLOCK) & 1) != 0;
                    text = isOn ? L"Num Lock On" : L"Num Lock Off";
                    type = IndicatorType::NUM;
                    break;
                case VK_SCROLL:
                    isOn = (GetKeyState(VK_SCROLL) & 1) != 0;
                    text = isOn ? L"Scroll Lock On" : L"Scroll Lock Off";
                    type = IndicatorType::SCROLL;
                    break;
                }

                ShowIndicator(g_hwnd, text, type);
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPWSTR lpCmdLine, int nCmdShow)
{
    g_hInstance = hInstance;

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"KeyLockIndicator";
    wc.hbrBackground = NULL;

    if (!RegisterClassEx(&wc))
        return 1;

    int dpi = GetDpiForSystem();
    int windowSize = MulDiv(g_windowSize, dpi, 300);

    g_hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
        L"KeyLockIndicator", L"", WS_POPUP,
        0, 0, windowSize, windowSize,
        NULL, NULL, hInstance, NULL);

    if (!g_hwnd)
        return 1;

    g_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
    if (!g_hook)
        return 1;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(g_hook);
    return 0;
}