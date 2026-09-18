#requires -Version 5.1
<#
.SYNOPSIS
    Configures, builds, tests and optionally packages Whitehole Pro.

.DESCRIPTION
    One command from a clean checkout to a shippable Windows executable:

        powershell -ExecutionPolicy Bypass -File scripts/build-release.ps1 -Package

    Produces build\whitehole-pro.exe (windowed editor, static runtime, icon and
    version metadata) plus build\whitehole-pro-console.exe (console/CLI). With -Package
    a dist\WhiteholePro-<version>-win64.zip is written containing both
    executables, the runtime data folder and the documentation.

.PARAMETER Configuration
    CMake build type. Release by default.

.PARAMETER BuildDirectory
    Build tree to use. Defaults to "build".

.PARAMETER Lto
    Enable link-time optimisation (-flto=auto).

.PARAMETER SkipTests
    Skip the native test suite.

.PARAMETER Package
    Stage dist\ and produce a zip archive.
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release',
    [string]$BuildDirectory = 'build',
    [switch]$Lto,
    [switch]$SkipTests,
    [switch]$Package
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$buildPath = Join-Path $root $BuildDirectory

function Write-Step([string]$Text) {
    Write-Host ''
    Write-Host "==> $Text" -ForegroundColor Cyan
}

function Invoke-Checked([string]$FilePath, [string[]]$Arguments) {
    Write-Host ("    " + $FilePath + ' ' + ($Arguments -join ' ')) -ForegroundColor DarkGray
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath exited with code $LASTEXITCODE"
    }
}

# ---------------------------------------------------------------------------
# Toolchain checks
# ---------------------------------------------------------------------------
Write-Step 'Checking the toolchain'
foreach ($tool in 'cmake', 'c++') {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "Required tool '$tool' was not found on PATH. Install CMake and a C++20 compiler (MSYS2 UCRT64 recommends: pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake make)."
    }
    Write-Host ("    found {0}" -f (Get-Command $tool).Source)
}

# ---------------------------------------------------------------------------
# Configure / build / test
# ---------------------------------------------------------------------------
Write-Step "Configuring ($Configuration)"
$configureArgs = @(
    '-S', $root,
    '-B', $buildPath,
    '-DCMAKE_BUILD_TYPE=' + $Configuration,
    '-DWHITEHOLE_STATIC_RUNTIME=ON',
    '-DWHITEHOLE_BUILD_TESTS=ON',
    '-DWHITEHOLE_BUILD_GUI=ON'
)
if ($Lto) { $configureArgs += '-DWHITEHOLE_ENABLE_LTO=ON' }
Invoke-Checked 'cmake' $configureArgs

Write-Step 'Building'
Invoke-Checked 'cmake' @('--build', $buildPath, '--config', $Configuration, '-j', '8')

if (-not $SkipTests) {
    Write-Step 'Running the native test suite'
    Invoke-Checked 'ctest' @('--test-dir', $buildPath, '-C', $Configuration, '--output-on-failure')
}

$consoleExe = Join-Path $buildPath 'whitehole-pro-console.exe'
$guiExe = Join-Path $buildPath 'whitehole-pro.exe'
if (-not (Test-Path $guiExe) -and -not (Test-Path $consoleExe)) {
    throw "No executable was produced in $buildPath"
}

Write-Step 'Smoke test'
if (Test-Path $consoleExe) {
    & $consoleExe map objects (Join-Path $root 'data\templates\SMG2BigGalaxyMap.arc') | Select-Object -First 1
}

if (-not $Package) {
    Write-Step 'Done'
    if (Test-Path $guiExe) { Write-Host "    editor : $guiExe" -ForegroundColor Green }
    if (Test-Path $consoleExe) { Write-Host "    console: $consoleExe" -ForegroundColor Green }
    return
}

# ---------------------------------------------------------------------------
# Package
# ---------------------------------------------------------------------------
Write-Step 'Staging dist'
$version = '0.2.0'
$stageName = "WhiteholePro-$version-win64"
$stage = Join-Path $root (Join-Path 'dist' $stageName)
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path $stage | Out-Null

foreach ($exe in @($guiExe, $consoleExe)) {
    if (Test-Path $exe) { Copy-Item $exe $stage }
}
foreach ($item in 'data', 'docs', 'README.md', 'LICENSE', 'COPYING') {
    $source = Join-Path $root $item
    if (Test-Path $source) {
        Copy-Item $source (Join-Path $stage $item) -Recurse -Force
    }
}

$zip = Join-Path $root ("dist\" + $stageName + '.zip')
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal

Write-Step 'Done'
Write-Host "    folder : $stage" -ForegroundColor Green
Write-Host ("    archive: {0} ({1:N0} bytes)" -f $zip, (Get-Item $zip).Length) -ForegroundColor Green