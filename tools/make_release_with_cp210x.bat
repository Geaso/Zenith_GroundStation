@echo off
setlocal

call "%~dp0make_release_core.bat"
if errorlevel 1 goto :fail

pushd "%~dp0\.."
set "PROJ_DIR=%CD%"
set "DIST_ROOT=%PROJ_DIR%\dist"
set "DIST_DIR=%DIST_ROOT%\ZenithGroundStation"
set "CP210X_SRC=%PROJ_DIR%\third_party\silabs\CP210x_Universal_Windows_Driver_11.5.0"
set "SEVENZIP=C:\Program Files\7-Zip"

if not exist "%CP210X_SRC%\silabser.inf" (
    echo [ERROR] Missing CP210x driver source: %CP210X_SRC%
    goto :fail_pushed
)

echo Adding Silicon Labs CP210x VCP Driver 11.5.0...
if not exist "%DIST_DIR%\drivers\CP210x" mkdir "%DIST_DIR%\drivers\CP210x"
xcopy /e /i /y "%CP210X_SRC%\*" "%DIST_DIR%\drivers\CP210x\" >nul
if errorlevel 1 goto :fail_pushed
copy /y "%PROJ_DIR%\tools\Install_CP2102_Driver.cmd" "%DIST_DIR%\" >nul
if errorlevel 1 goto :fail_pushed
copy /y "%PROJ_DIR%\docs\CP2102_DRIVER_README.txt" "%DIST_DIR%\" >nul
if errorlevel 1 goto :fail_pushed

call "%DIST_DIR%\Install_CP2102_Driver.cmd" --check
if errorlevel 1 goto :fail_pushed

for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd"') do set "STAMP=%%i"
set "ZIP_PATH=%DIST_ROOT%\ZenithGroundStation-%STAMP%.zip"
set "SFX_PATH=%DIST_ROOT%\ZenithGroundStation-%STAMP%.exe"
set "PAYLOAD=%DIST_ROOT%\payload.7z"

if not exist "%SEVENZIP%\7z.exe" (
    echo [ERROR] Missing 7-Zip: %SEVENZIP%
    goto :fail_pushed
)

if exist "%PAYLOAD%" del /q "%PAYLOAD%"
if exist "%SFX_PATH%" del /q "%SFX_PATH%"
pushd "%DIST_ROOT%"
"%SEVENZIP%\7z.exe" a -t7z -mx=7 "%PAYLOAD%" "ZenithGroundStation" >nul
if errorlevel 1 (popd & goto :fail_pushed)
popd
copy /b "%SEVENZIP%\7z.sfx" + "%PAYLOAD%" "%SFX_PATH%" >nul
if errorlevel 1 goto :fail_pushed
del /q "%PAYLOAD%"

if exist "%ZIP_PATH%" del /q "%ZIP_PATH%"
powershell -NoProfile -Command "Compress-Archive -Path '%DIST_DIR%\*' -DestinationPath '%ZIP_PATH%' -Force"
if errorlevel 1 goto :fail_pushed

echo.
echo [OK] Customer package includes CP210x VCP Driver 11.5.0.
echo Single EXE: %SFX_PATH%
echo ZIP:        %ZIP_PATH%
popd
endlocal
exit /b 0

:fail_pushed
popd
:fail
echo [ERROR] CP210x customer package failed.
endlocal
exit /b 1
