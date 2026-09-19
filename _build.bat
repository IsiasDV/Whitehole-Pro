@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
if errorlevel 1 (
  echo vcvarsall failed > _build_stage.txt
  exit /b 1
)
set "PATH=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
echo configuring > _build_stage.txt
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DWHITEHOLE_BUILD_TESTS=ON -DWHITEHOLE_BUILD_GUI=ON -DWHITEHOLE_STATIC_RUNTIME=ON > build_cfg.txt 2>&1
if errorlevel 1 (
  echo configure failed > _build_stage.txt
  exit /b 1
)
echo building > _build_stage.txt
cmake --build build > build_out.txt 2>&1
if errorlevel 1 (
  echo build failed > _build_stage.txt
  exit /b 1
)
echo testing > _build_stage.txt
build\whitehole_core_tests.exe > tests_out.txt 2>&1
if errorlevel 1 (
  echo tests failed > _build_stage.txt
  exit /b 1
)
echo done > _build_stage.txt
exit /b 0