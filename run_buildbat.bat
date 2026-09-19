@echo off
cd /d "c:\Users\conta\Documents\GitHub\Whitehole-Pro"
call Build.bat --clean > build_bat_output.log 2>&1
echo BUILDBAT_EXIT_CODE=%ERRORLEVEL% >> build_bat_output.log
