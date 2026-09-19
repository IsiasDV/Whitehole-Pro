@echo off
rem ============================================================================
rem  Whitehole Pro - One-Click Build (double-click this file)
rem ============================================================================
rem  What this does:
rem    1. Checks you have CMake + a C++ compiler
rem    2. Configures the project in .\build\
rem    3. Builds whitehole-pro.exe (double-click editor, no console pop-up)
rem       and whitehole-pro-console.exe (console + scripting tool)
rem    4. Runs the built-in tests to make sure everything works
rem
rem  You can also run:  Build.bat --clean   (deletes .\build\ and starts fresh)
rem ============================================================================
setlocal EnableDelayedExpansion
title Whitehole Pro - One-Click Build
cd /d "%~dp0"

echo.
echo  ============================================================
echo   WHITEHOLE PRO - One-Click Build
echo  ============================================================
echo   This will build the native C++ editor. No Java needed.
echo.

if /I "%~1"=="--clean" (
    echo  Cleaning old build folder...
    if exist build rmdir /s /q build
    echo  Clean done.
    echo.
)

rem ---- Step 1: check CMake ---------------------------------------------------
where cmake >nul 2>nul
if errorlevel 1 goto :no_cmake
for /f "tokens=*" %%v in ('cmake --version ^| findstr /i "cmake version"') do set "CMAKE_VER=%%v"
echo  [1/4] Found: !CMAKE_VER!
goto :check_compiler

:no_cmake
echo  [X] CMake was NOT found.
echo.
echo  FIX - pick ONE of these (takes ~2 minutes):
echo    EASIEST: open PowerShell and paste:
echo      winget install Kitware.CMake
echo.
echo    OR download the installer from:
echo      https://cmake.org/download/
echo    ^(tick "Add CMake to the system PATH" during install^)
echo.
echo  Then close this window, open a fresh one, and double-click Build.bat again.
echo.
pause
exit /b 1

:check_compiler
rem ---- Step 2: check for a C++ compiler --------------------------------------
set "HAVE_COMPILER=0"
where cl >nul 2>nul && set "HAVE_COMPILER=1"
where g++ >nul 2>nul && set "HAVE_COMPILER=1"
where c++ >nul 2>nul && set "HAVE_COMPILER=1"

if "!HAVE_COMPILER!"=="1" (
    echo  [2/4] Found a C++ compiler.
    goto :configure
)

echo  [X] No C++ compiler was found.
echo.
echo  FIX - pick ONE:
echo    A. Visual Studio 2022 Community (recommended on Windows):
echo         winget install Microsoft.VisualStudio.2022.Community --silent --override "--add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended --passive"
echo       Or download from: https://visualstudio.microsoft.com/downloads/
echo       ^(tick "Desktop development with C++" during install^)
echo.
echo    B. MSYS2 UCRT64 (lightweight, no Visual Studio):
echo         winget install MSYS2.MSYS2
echo       Then in the UCRT64 terminal run:
echo         pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-make
echo.
echo  Then double-click Build.bat again.
echo.
pause
exit /b 1

:configure
rem ---- Step 3: configure ------------------------------------------------------
echo  [3/4] Configuring... (creates the "build" folder)
echo        Command: cmake -S . -B build
echo.
cmake -S . -B build
if errorlevel 1 goto :configure_failed
echo.
echo  Configuration OK.
goto :build

:configure_failed
echo.
echo  [X] Configuration failed.
echo.
echo  TRY THESE IN ORDER:
echo    1. Delete the "build" folder and run Build.bat again:
echo         rmdir /s /q build
echo    2. If it says "No CMAKE_CXX_COMPILER could be found",
echo       install Visual Studio with C++ (see above) and reboot.
echo    3. Advanced: try the MSYS2 UCRT64 terminal instead of cmd.exe.
echo.
pause
exit /b 1

:build
rem ---- Step 4: build ----------------------------------------------------------
echo.
echo  [4/4] Building... (this takes ~30 seconds to ~5 minutes)
echo        Command: cmake --build build --config Release
echo.
cmake --build build --config Release --parallel
if errorlevel 1 goto :build_failed

rem ---- Find the executables (VS puts them in build\Release\, MSYS2 in build\) -
set "GUI_EXE="
set "CLI_EXE="
for %%P in ("build\Release\whitehole-pro.exe" "build\whitehole-pro.exe" "build-msvc\Release\whitehole-pro.exe") do (
    if exist %%~P if "!GUI_EXE!"=="" set "GUI_EXE=%%~P"
)
for %%P in ("build\Release\whitehole-pro-console.exe" "build\whitehole-pro-console.exe" "build-msvc\Release\whitehole-pro-console.exe") do (
    if exist %%~P if "!CLI_EXE!"=="" set "CLI_EXE=%%~P"
)

echo.
echo  Running built-in tests...
ctest --test-dir build -C Release --output-on-failure
if errorlevel 1 (
    echo.
    echo  [!] Tests reported a failure - the .exe files above may still work,
    echo      but please share the red text above if you ask for help.
) else (
    echo  Tests passed.
)

echo.
echo  ============================================================
echo   BUILD SUCCEEDED!
echo  ============================================================
if not "!GUI_EXE!"=="" echo   Editor (double-click this^): !GUI_EXE!
if not "!CLI_EXE!"=="" echo   Console tool             : !CLI_EXE!
echo.
echo   NEXT STEP: double-click "Run-Whitehole-Pro.bat"
echo   (or launch the Editor .exe directly^)
echo  ============================================================
echo.

rem Offer to launch right now
if not "!GUI_EXE!"=="" (
    choice /M "Launch the editor now"
    if "!ERRORLEVEL!"=="1" start "" "!GUI_EXE!"
)
pause
exit /b 0

:build_failed
echo.
echo  [X] Build failed. See the red errors above.
echo.
echo  COMMON FIXES:
echo    - Delete "build" and retry:  rmdir /s /q build
echo    - Update Visual Studio / reboot after installing C++ tools
echo    - Run from MSYS2 UCRT64 terminal if you use MSYS2
echo    - Ask for help and paste the last ~30 lines of this window
echo.
pause
exit /b 1
