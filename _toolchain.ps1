$out = 'c:\Users\conta\Documents\GitHub\Whitehole-Pro\_toolchain.txt'
"=== vswhere ===" | Set-Content $out
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
if (Test-Path $vswhere) { & $vswhere -latest -products * -property installationPath | Add-Content $out } else { 'no vswhere' | Add-Content $out }
"=== VS dirs ===" | Add-Content $out
Get-ChildItem 'C:\Program Files\Microsoft Visual Studio' -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name | Add-Content $out
"=== cl.exe ===" | Add-Content $out
Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe' -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName | Add-Content $out
"=== msys ===" | Add-Content $out
if (Test-Path 'C:\msys64\ucrt64\bin\g++.exe') { 'msys ucrt64 g++ present' | Add-Content $out } else { 'no msys g++' | Add-Content $out }
"=== cmake generators ===" | Add-Content $out
& cmake --help 2>&1 | Select-String -Pattern '=' | Select-Object -First 12 | Add-Content $out
