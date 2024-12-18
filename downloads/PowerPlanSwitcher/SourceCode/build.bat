@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
cd /d %~dp0

cl /EHsc /W4 /DUNICODE /D_UNICODE /utf-8 ^
   power_plan_switcher.cpp ^
   /link user32.lib shell32.lib shlwapi.lib powrprof.lib comctl32.lib

if %ERRORLEVEL% neq 0 (
    echo Compilation failed!
    pause
    exit /b 1
)

echo Compilation successful!
pause