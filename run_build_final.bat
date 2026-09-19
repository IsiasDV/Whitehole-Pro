@echo off
cd /d "c:\Users\conta\Documents\GitHub\Whitehole-Pro"
set CMAKE="C:\Program Files\CMake\bin\cmake.exe"
echo Starting build >> build_detached.log
%CMAKE% --build build --config Release --parallel >> build_detached.log 2>&1
echo BUILD_EXIT_CODE=%ERRORLEVEL% >> build_detached.log
