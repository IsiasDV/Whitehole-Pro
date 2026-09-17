@echo off
rem Whitehole Pro launcher - runs the native C++ editor, no Java required.
setlocal
set "DIR=%~dp0"

for %%P in ("%DIR%whitehole-pro.exe" "%DIR%build\whitehole-pro.exe" "%DIR%build\Release\whitehole-pro.exe" "%DIR%build-msvc\Release\whitehole-pro.exe") do (
    if exist %%~P (
        start "" %%~P %*
        exit /b 0
    )
)

echo Whitehole Pro is not built yet.
echo.
echo Build it with:
echo     cmake -S . -B build
echo     cmake --build build -j
echo.
echo Or use the helper script:
echo     powershell -ExecutionPolicy Bypass -File scripts\build-release.ps1
echo.
pause
exit /b 1
