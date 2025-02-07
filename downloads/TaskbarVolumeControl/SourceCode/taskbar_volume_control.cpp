#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "User32.lib")

IMMDeviceEnumerator* pDeviceEnumerator = nullptr;
IMMDevice* pDevice = nullptr;
IAudioEndpointVolume* pAudioEndpointVolume = nullptr;

bool IsMouseOverTaskbar()
{
    POINT cursorPos;
    GetCursorPos(&cursorPos);
    HWND taskbar = FindWindow(L"Shell_TrayWnd", nullptr);
    if (taskbar)
    {
        RECT taskbarRect;
        GetWindowRect(taskbar, &taskbarRect);
        return PtInRect(&taskbarRect, cursorPos);
    }
    return false;
}

void ShowWindowsVolumeOverlay(bool increase)
{
    int command = increase ? APPCOMMAND_VOLUME_UP : APPCOMMAND_VOLUME_DOWN;
    HWND hwnd = GetForegroundWindow();
    SendMessage(hwnd, WM_APPCOMMAND, (WPARAM)hwnd, MAKELPARAM(0, command));
}

void AdjustVolume(float delta)
{
    if (pAudioEndpointVolume)
    {
        float currentVolume = 0.0f;
        pAudioEndpointVolume->GetMasterVolumeLevelScalar(&currentVolume);

        float newVolume = currentVolume + delta;
        newVolume = max(0.0f, min(1.0f, newVolume));

        pAudioEndpointVolume->SetMasterVolumeLevelScalar(newVolume, nullptr);

        ShowWindowsVolumeOverlay(delta > 0);
    }
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    static DWORD lastScrollTime = 0;
    DWORD currentTime = GetTickCount();

    if (nCode == HC_ACTION && wParam == WM_MOUSEWHEEL)
    {
        MSLLHOOKSTRUCT* pMouseStruct = (MSLLHOOKSTRUCT*)lParam;
        if (pMouseStruct && IsMouseOverTaskbar())
        {
            short zDelta = GET_WHEEL_DELTA_WPARAM(pMouseStruct->mouseData);

            if (currentTime - lastScrollTime > 50) // Daha hızlı tepki süresi
            {
                lastScrollTime = currentTime;

                float delta = (zDelta > 0) ? 0.04f : -0.04f; // Daha hassas, 2'şer 2'şer artırma/azaltma
                AdjustVolume(delta);
            }
            return 1;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    CoInitialize(nullptr);
    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pDeviceEnumerator);
    pDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void**)&pAudioEndpointVolume);

    HHOOK hHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, nullptr, 0);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(hHook);
    pAudioEndpointVolume->Release();
    pDevice->Release();
    pDeviceEnumerator->Release();
    CoUninitialize();

    return 0;
}
