@echo off
cd /d "c:\Users\conta\Documents\GitHub\Whitehole-Pro"
echo === Checking vswhere ===
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    echo FOUND: ProgramFiles(x86) path
) else (
    echo NOT FOUND: ProgramFiles(x86) path
)
if exist "%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe" (
    echo FOUND: ProgramFiles path
) else (
    echo NOT FOUND: ProgramFiles path
)
echo.
echo === Checking source files ===
for %%F in (
    cpp\src\app\gui_win32.cpp
    cpp\src\render\viewport_win32.cpp
    cpp\src\app\gui_stub.cpp
    cpp\src\app\cli.cpp
    cpp\src\app\object_db_update.cpp
    cpp\src\main.cpp
    cpp\src\winmain.cpp
    cpp\res\whitehole.rc.in
    cpp\res\whitehole.ico
    cpp\res\whitehole.manifest
) do (
    if exist "%%F" (echo OK: %%F) else (echo MISSING: %%F)
)
echo.
echo DONE
