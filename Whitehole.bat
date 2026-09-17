@echo off
setlocal

rem Always resolve files relative to this launcher, even when it is opened from
rem a shortcut or a different working directory.
cd /d "%~dp0"

set "JAR_PATH="

rem A JAR can be dragged onto this file or passed as the first argument.
if not "%~1"=="" (
    if not exist "%~f1" (
        echo Whitehole Neo could not find the selected JAR:
        echo   %~f1
        pause
        exit /b 2
    )
    set "JAR_PATH=%~f1"
)

rem Support both the historical build name and common Neo release names.
if not defined JAR_PATH if exist "%~dp0Whitehole.jar" set "JAR_PATH=%~dp0Whitehole.jar"
if not defined JAR_PATH if exist "%~dp0Whitehole-Neo.jar" set "JAR_PATH=%~dp0Whitehole-Neo.jar"
if not defined JAR_PATH if exist "%~dp0WhiteholeNeo.jar" set "JAR_PATH=%~dp0WhiteholeNeo.jar"
if not defined JAR_PATH for %%F in ("%~dp0Whitehole*.jar") do if exist "%%~fF" if not defined JAR_PATH set "JAR_PATH=%%~fF"

if not defined JAR_PATH (
    echo Whitehole Neo could not find its application JAR.
    echo Place Whitehole.jar or Whitehole-Neo.jar beside this batch file,
    echo or drag the JAR onto Whitehole.bat.
    pause
    exit /b 2
)

where java >nul 2>&1
if errorlevel 1 (
    echo Java was not found. Install Java 11 or newer and ensure java.exe is on PATH.
    pause
    exit /b 3
)

echo Running "%JAR_PATH%" using:
java -version
java --add-exports=java.desktop/sun.awt=ALL-UNNAMED --add-opens=java.desktop/sun.awt.windows=ALL-UNNAMED -jar "%JAR_PATH%"
set "EXIT_CODE=%ERRORLEVEL%"

if not "%EXIT_CODE%"=="0" (
    echo.
    echo Whitehole Neo exited with error code %EXIT_CODE%.
    echo The JAR was found and launched; review the Java error shown above for the cause.
    pause
)

endlocal & exit /b %EXIT_CODE%
