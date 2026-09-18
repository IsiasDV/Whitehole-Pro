@echo off
rem ============================================================================
rem  Run Whitehole Pro (double-click this file)
rem ============================================================================
rem  Finds the already-built editor and launches it. If nothing is built yet,
rem  it tells you exactly what to do (double-click Build.bat first).
rem ============================================================================
setlocal EnableDelayedExpansion
title Whitehole Pro
cd /d "%~dp0"

set "GUI_EXE="
set "CLI_EXE="

for %%P in ("whitehole-pro.exe" "build\whitehole-pro.exe" "build\Release\whitehole-pro.exe" "build-msvc\Release\whitehole-pro.exe") do (
    if exist %%~P if "!GUI_EXE!"=="" set "GUI_EXE=%%~P"
)
for %%P in ("whitehole-pro-console.exe" "build\whitehole-pro-console.exe" "build\Release\whitehole-pro-console.exe" "build-msvc\Release\whitehole-pro-console.exe") do (
    if exist %%~P if "!CLI_EXE!"=="" set "CLI_EXE=%%~P"
)

if not "!GUI_EXE!"=="" (
    start "" "!GUI_EXE!" %*
    exit /b 0
)

if not "!CLI_EXE!"=="" (
    echo No windowed editor found, launching the console build instead...
    start "" "!CLI_EXE!" %*
    exit /b 0
)

echo Whitehole Pro is not built yet - no .exe was found.
echo.
echo DO THIS:
echo   1. Double-click  Build.bat  (it builds everything for you)
echo   2. Then double-click this file again to run the editor.
echo.
echo Advanced alternative:
echo   powershell -ExecutionPolicy Bypass -File scripts\build-release.ps1
echo.
pause
exit /b 1
