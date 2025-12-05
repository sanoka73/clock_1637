@echo off
setlocal enabledelayedexpansion

REM ESP8266 Clock - Interactive CLI Tool for Windows

REM Check if platformio is installed
where pio >nul 2>nul
if %errorlevel% neq 0 (
    echo Error: PlatformIO CLI (pio) is not installed or not in PATH
    echo Please install it: https://platformio.org/install/cli
    pause
    exit /b 1
)

:main
set selected=1

:menu
cls
echo ========================================
echo    ESP8266 Clock - PlatformIO CLI
echo ========================================
echo.
echo Use arrow keys (Up/Down) to navigate, Enter to select
echo Or press the number key directly:
echo.

if %selected%==1 (
    echo [94m^> [1] Build[0m
) else (
    echo   [1] Build
)

if %selected%==2 (
    echo [94m^> [2] Upload[0m
) else (
    echo   [2] Upload
)

if %selected%==3 (
    echo [94m^> [3] Monitor[0m
) else (
    echo   [3] Monitor
)

if %selected%==4 (
    echo [94m^> [4] Build + Upload + Monitor[0m
) else (
    echo   [4] Build + Upload + Monitor
)

if %selected%==5 (
    echo [94m^> [5] Clean[0m
) else (
    echo   [5] Clean
)

if %selected%==6 (
    echo [94m^> [6] Device Info[0m
) else (
    echo   [6] Device Info
)

if %selected%==7 (
    echo [94m^> [7] Exit[0m
) else (
    echo   [7] Exit
)

echo.

REM Get user input
choice /c 1234567wu /n /m ""

if errorlevel 9 goto menu_down
if errorlevel 8 goto menu_up
if errorlevel 7 goto do_exit
if errorlevel 6 goto do_device_info
if errorlevel 5 goto do_clean
if errorlevel 4 goto do_run
if errorlevel 3 goto do_monitor
if errorlevel 2 goto do_upload
if errorlevel 1 goto do_build

goto menu

:menu_up
set /a selected-=1
if %selected% lss 1 set selected=7
goto menu

:menu_down
set /a selected+=1
if %selected% gtr 7 set selected=1
goto menu

:do_build
cls
echo ========================================
echo    ESP8266 Clock - Build
echo ========================================
echo.
echo Building project...
echo.
pio run

if %errorlevel% equ 0 (
    echo.
    echo [92mBuild completed successfully![0m
) else (
    echo.
    echo [91mBuild failed![0m
)

echo.
pause
goto menu

:do_upload
cls
echo ========================================
echo    ESP8266 Clock - Upload
echo ========================================
echo.
echo Uploading firmware...
echo.
pio run --target upload

if %errorlevel% equ 0 (
    echo.
    echo [92mUpload completed successfully![0m
) else (
    echo.
    echo [91mUpload failed![0m
)

echo.
pause
goto menu

:do_monitor
cls
echo ========================================
echo    ESP8266 Clock - Monitor
echo ========================================
echo.
echo Starting serial monitor (Ctrl+C to exit)...
echo Baud rate: 115200
echo.
timeout /t 1 >nul
pio device monitor --baud 115200

echo.
pause
goto menu

:do_run
cls
echo ========================================
echo    ESP8266 Clock - Run
echo ========================================
echo.
echo [1/3] Building project...
echo.
pio run

if %errorlevel% neq 0 (
    echo.
    echo [91mBuild failed! Aborting.[0m
    echo.
    pause
    goto menu
)

echo.
echo [92mBuild successful[0m
echo.
echo [2/3] Uploading firmware...
echo.
pio run --target upload

if %errorlevel% neq 0 (
    echo.
    echo [91mUpload failed! Aborting.[0m
    echo.
    pause
    goto menu
)

echo.
echo [92mUpload successful[0m
echo.
echo [3/3] Starting serial monitor (Ctrl+C to exit)...
echo Baud rate: 115200
echo.
timeout /t 2 >nul
pio device monitor --baud 115200

echo.
pause
goto menu

:do_clean
cls
echo ========================================
echo    ESP8266 Clock - Clean
echo ========================================
echo.
echo Cleaning build files...
echo.
pio run --target clean

if %errorlevel% equ 0 (
    echo.
    echo [92mClean completed successfully![0m
) else (
    echo.
    echo [91mClean failed![0m
)

echo.
pause
goto menu

:do_device_info
cls
echo ========================================
echo    ESP8266 Clock - Device Info
echo ========================================
echo.
echo Connected devices:
echo.
pio device list

echo.
pause
goto menu

:do_exit
cls
echo ========================================
echo    ESP8266 Clock - Goodbye!
echo ========================================
echo.
exit /b 0
