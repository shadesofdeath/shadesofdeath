@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
cd /d %~dp0

cl /EHsc /W4 /DUNICODE /D_UNICODE /utf-8 ^
   taskbar_volume_control.cpp ^
   /link ole32.lib user32.lib shell32.lib gdi32.lib ^
   /SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup


if %ERRORLEVEL% neq 0 (
    echo Derleme başarısız!
    pause
    exit /b 1
)

echo Derleme başarılı!
pause
