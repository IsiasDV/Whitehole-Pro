@echo off
rem Whitehole Pro launcher (kept for backwards compatibility).
rem Just forwards to Run-Whitehole-Pro.bat so old shortcuts keep working.
setlocal
cd /d "%~dp0"
call "%~dp0Run-Whitehole-Pro.bat" %*

