$out = 'c:\Users\conta\Documents\GitHub\Whitehole-Pro\_toolchain.txt'
"=== ninja ===" | Set-Content $out
Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\*\*\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe' -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName | Add-Content $out
"=== vcvars ===" | Add-Content $out
Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\*\*\VC\Auxiliary\Build\vcvarsall.bat' -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName | Add-Content $out
"=== mingw make ===" | Add-Content $out
Get-ChildItem 'C:\msys64\ucrt64\bin\*make*.exe' -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name | Add-Content $out
"=== cmake generators ===" | Add-Content $out
& cmake --help 2>&1 | Select-String -Pattern 'Visual Studio|Ninja|Unix Makefiles|MinGW' | Add-Content $out
