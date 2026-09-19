@echo off
setlocal
cd /d "c:\Users\conta\Documents\GitHub\Whitehole-Pro"
set CMAKE="C:\Program Files\CMake\bin\cmake.exe"
echo Starting rebuild >> build_final2.log
%CMAKE% --build build --config Release --parallel >> build_final2.log 2>&1
echo BUILD_EXIT_CODE=%ERRORLEVEL% >> build_final2.log
echo. >> build_final2.log
echo Running tests... >> build_final2.log
cd build
ctest -C Release --output-on-failure >> ..\build_final2.log 2>&1
echo CTEST_EXIT_CODE=%ERRORLEVEL% >> ..\build_final2.log
