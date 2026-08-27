@echo off
setlocal

set "DRIVER_DIR=%~dp0drivers\CP210x"
set "DRIVER_INF=%DRIVER_DIR%\silabser.inf"
set "DRIVER_LICENSE=%DRIVER_DIR%\SLAB_License_Agreement_VCP_Windows.txt"

if not exist "%DRIVER_INF%" (
    echo [ERROR] CP210x driver package is incomplete.
    echo Missing: "%DRIVER_INF%"
    pause
    exit /b 2
)

if /i "%~1"=="--check" (
    echo [OK] CP210x driver package is present.
    exit /b 0
)

if /i not "%~1"=="--elevated" (
    if exist "%DRIVER_LICENSE%" (
        start /wait "" notepad.exe "%DRIVER_LICENSE%"
    )
    echo.
    echo The CP210x driver is licensed by Silicon Laboratories Inc.
    choice /c YN /n /m "Do you accept the displayed license and want to install the driver? [Y/N] "
    if errorlevel 2 exit /b 0
)

fltmc >nul 2>&1
if errorlevel 1 (
    echo Requesting administrator permission...
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -ArgumentList '--elevated' -Verb RunAs"
    if errorlevel 1 (
        echo [ERROR] Administrator permission was not granted.
        pause
        exit /b 3
    )
    exit /b 0
)

echo.
echo Installing Silicon Labs CP210x VCP Driver 11.5.0...
"%SystemRoot%\System32\pnputil.exe" /add-driver "%DRIVER_INF%" /install
set "INSTALL_RESULT=%ERRORLEVEL%"

echo.
if not "%INSTALL_RESULT%"=="0" (
    echo [ERROR] Driver installation failed with code %INSTALL_RESULT%.
    echo You can right-click drivers\CP210x\silabser.inf and choose Install.
    pause
    exit /b %INSTALL_RESULT%
)

echo [OK] The CP210x driver package was installed successfully.
echo Reconnect the USB device, then confirm its COM port in Device Manager.
pause
exit /b 0
