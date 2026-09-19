param()
$root = "c:\Users\conta\Documents\GitHub\Whitehole-Pro"

Write-Host "=== Checking vswhere ==="
$vswherePaths = @(
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe",
    "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe"
)
foreach ($p in $vswherePaths) {
    if (Test-Path $p) { Write-Host "FOUND: $p" } else { Write-Host "NOT FOUND: $p" }
}

Write-Host "`n=== Checking source files ==="
$files = @(
    "cpp\src\app\gui_win32.cpp",
    "cpp\src\render\viewport_win32.cpp",
    "cpp\src\app\gui_stub.cpp",
    "cpp\src\app\cli.cpp",
    "cpp\src\app\object_db_update.cpp",
    "cpp\src\main.cpp",
    "cpp\src\winmain.cpp",
    "cpp\res\whitehole.rc.in",
    "cpp\res\whitehole.ico",
    "cpp\res\whitehole.manifest"
)
foreach ($f in $files) {
    $full = Join-Path $root $f
    if (Test-Path $full) { Write-Host "OK: $f" } else { Write-Host "MISSING: $f" }
}

Write-Host "`n=== Checking cl on PATH ==="
$r = Get-Command cl -ErrorAction SilentlyContinue
if ($r) { Write-Host "cl found: $($r.Source)" } else { Write-Host "cl NOT on PATH" }

Write-Host "`nDONE"
