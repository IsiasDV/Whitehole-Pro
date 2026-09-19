param()
$path = "c:\Users\conta\Documents\GitHub\Whitehole-Pro\cpp\src\app\gui_win32.cpp"
$lines = Get-Content $path
# Line 887 (0-indexed: 886) should have 4-space indent, not 12
# Fix any inconsistent indentation in this block (lines 886-892)
$lines[886] = '    AppendMenuW(fileMenu, MF_STRING, kIdExit, L"Exit");'
$lines[887] = '    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"File");'
$lines[888] = '    HMENU viewMenu = CreatePopupMenu();'
$lines[889] = '    AppendMenuW(viewMenu, MF_STRING, kIdToggleDark, L"Toggle Dark Theme");'
$lines[890] = '    AppendMenuW(viewMenu, MF_STRING, kIdShowLabels, L"Show Object Labels");'
$lines[891] = '    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(viewMenu), L"View");'
Set-Content -Path $path -Value $lines -Encoding UTF8
Write-Host "Fixed indentation"
