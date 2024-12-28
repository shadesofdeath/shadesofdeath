@echo off
chcp 1254
setlocal enabledelayedexpansion

:: Visual Studio ortamını ayarla
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"

:: C++ dosyasını derle
cl /nologo /EHsc /W4 /std:c++17 /O2 KeyLockIndicator.cpp /link /SUBSYSTEM:WINDOWS user32.lib dwmapi.lib gdi32.lib msimg32.lib

if errorlevel 1 (
    echo Derleme hatası!
    pause
    exit /b 1
)

echo.
echo Derleme başarılı!
echo.
pause