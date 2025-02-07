@echo off
setlocal

:: VS Environment
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
cd /d %~dp0

:: Icon kontrolü
if not exist icon.ico (
    echo icon.ico bulunamadı! Varsayılan Windows ikonunu kopyalıyorum...
    copy "%SystemRoot%\System32\SHELL32.dll" shell32.dll
    powershell -Command "$shell = New-Object -ComObject WScript.Shell; $shortcut = $shell.CreateShortcut('temp.lnk'); $shortcut.IconLocation = 'shell32.dll,0'; $bytes = [System.IO.File]::ReadAllBytes('shell32.dll'); [System.IO.File]::WriteAllBytes('icon.ico', $bytes); Remove-Item 'temp.lnk'; Remove-Item 'shell32.dll'"
)

:: Resource ve C++ derleme
rc /nologo resource.rc
if %ERRORLEVEL% neq 0 (
    echo Resource derleme hatası!
    pause
    exit /b 1
)

cl /EHsc /W4 /DUNICODE /D_UNICODE /utf-8 NetBlocker.cpp resource.res ^
   /link ole32.lib user32.lib shell32.lib gdi32.lib advapi32.lib comdlg32.lib comctl32.lib ^
   /SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup

if %ERRORLEVEL% neq 0 (
    echo C++ derleme hatası!
    pause
    exit /b 1
)

echo Derleme başarılı!
pause