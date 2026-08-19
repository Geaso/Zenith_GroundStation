@echo off
setlocal enabledelayedexpansion

rem ===========================================================================
rem  Zenith Ground Station - 客户交付打包
rem
rem  产出一份自包含的 dist\ZenithGroundStation\ 目录 + 同名 zip。
rem  目标机器无需安装 Qt、无需 VC++ 运行库，解压双击即用。
rem
rem  刻意采用【白名单】策略：先建空目录，再逐项放入明确需要的东西。
rem  绝不从工程目录整体拷贝 —— 那样迟早会把 .git / build / src 一起发出去。
rem ===========================================================================

set "QT_DIR=C:\Qt\6.6.3\mingw_64"
set "MINGW_DIR=C:\Qt\Tools\mingw1310_64"

pushd "%~dp0\.."
set "PROJ_DIR=%CD%"
set "BUILD_DIR=%PROJ_DIR%\build-release"
set "DIST_ROOT=%PROJ_DIR%\dist"
set "DIST_DIR=%DIST_ROOT%\ZenithGroundStation"

echo.
echo ==========================================================
echo  Zenith Ground Station 交付打包
echo  工程目录: %PROJ_DIR%
echo ==========================================================
echo.

if not exist "%QT_DIR%\bin\windeployqt.exe" (
    echo [错误] 找不到 Qt: %QT_DIR%
    echo        请修改本脚本顶部的 QT_DIR 变量
    goto :fail
)

set "PATH=%QT_DIR%\bin;%MINGW_DIR%\bin;%PATH%"

rem --- 1. 全新 Release 构建 -------------------------------------------------
echo [1/6] 编译 Release ...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
pushd "%BUILD_DIR%"
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_DIR%" .. >nul
if errorlevel 1 (popd & echo [错误] CMake 配置失败 & goto :fail)
mingw32-make -j8 >nul
if errorlevel 1 (popd & echo [错误] 编译失败 & goto :fail)
popd
if not exist "%BUILD_DIR%\ZenithGroundStationQt.exe" (
    echo [错误] 没有生成 exe
    goto :fail
)
echo       完成

rem --- 2. 清空交付目录 ------------------------------------------------------
echo [2/6] 准备交付目录 ...
if exist "%DIST_DIR%" rmdir /s /q "%DIST_DIR%"
mkdir "%DIST_DIR%"
echo       %DIST_DIR%

rem --- 3. 只放入 exe（白名单第一项）---------------------------------------
echo [3/6] 拷入主程序 ...
copy /y "%BUILD_DIR%\ZenithGroundStationQt.exe" "%DIST_DIR%\" >nul
if errorlevel 1 goto :fail

rem --- 4. windeployqt 收集运行时依赖 ---------------------------------------
echo [4/6] 收集 Qt 运行时 ...
windeployqt.exe --release --no-translations --no-system-d3d-compiler ^
    --qmldir "%PROJ_DIR%\qml" --dir "%DIST_DIR%" "%DIST_DIR%\ZenithGroundStationQt.exe" >nul
if errorlevel 1 (echo [错误] windeployqt 失败 & goto :fail)

rem mingw 运行库：windeployqt 不一定会带全，显式补上
for %%F in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
    if exist "%MINGW_DIR%\bin\%%F" copy /y "%MINGW_DIR%\bin\%%F" "%DIST_DIR%\" >nul
)

rem --- 5. 剔除不该交付的东西 ------------------------------------------------
echo [5/6] 剔除调试通道 ...
rem QML 调试器插件：留着等于给对方一个运行时检视 QML 对象树的入口
if exist "%DIST_DIR%\qmltooling" (
    rmdir /s /q "%DIST_DIR%\qmltooling"
    echo       已删除 qmltooling\ ^(QML 调试器插件^)
)
if exist "%DIST_DIR%\translations" rmdir /s /q "%DIST_DIR%\translations"
rem 任何漏网的调试符号
del /s /q "%DIST_DIR%\*.pdb" >nul 2>&1

rem --- 6. 放入外置配置与许可声明 --------------------------------------------
echo [6/6] 放入配置与许可声明 ...
mkdir "%DIST_DIR%\config"
copy /y "%PROJ_DIR%\config\script_actions.json" "%DIST_DIR%\config\" >nul

> "%DIST_DIR%\第三方许可声明.txt" (
    echo Zenith Ground Station
    echo.
    echo 本软件使用 Qt 6.6.3，依据 GNU LGPL v3 授权。
    echo Qt 以独立动态库形式随本软件分发，用户可自行替换为
    echo 相同版本的 Qt 库以完成重新链接。
    echo.
    echo Qt 官网:     https://www.qt.io
    echo Qt 源代码:   https://download.qt.io/archive/qt/6.6/6.6.3/single/
    echo LGPL v3 全文: https://www.gnu.org/licenses/lgpl-3.0.html
    echo.
    echo 本软件自身代码版权归开发方所有，不随本许可开放。
)

rem --- 打包 ------------------------------------------------------------------
echo.
echo 正在压缩 ...
for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd"') do set "STAMP=%%i"
set "ZIP_PATH=%DIST_ROOT%\ZenithGroundStation-%STAMP%.zip"
if exist "%ZIP_PATH%" del /q "%ZIP_PATH%"
powershell -NoProfile -Command "Compress-Archive -Path '%DIST_DIR%\*' -DestinationPath '%ZIP_PATH%' -Force"

rem --- 自检 ------------------------------------------------------------------
echo.
echo ==========================================================
echo  打包完成
echo ==========================================================
echo  目录: %DIST_DIR%
echo  压缩: %ZIP_PATH%
echo.
echo  交付前自检:
if exist "%DIST_DIR%\qmltooling" (echo   [警告] qmltooling 仍存在) else (echo   [OK] 无 QML 调试器插件)
if exist "%DIST_DIR%\*.pdb"     (echo   [警告] 存在 pdb 符号文件) else (echo   [OK] 无调试符号)
if exist "%DIST_DIR%\src"       (echo   [警告] 混入了 src 目录) else (echo   [OK] 无源码目录)
if exist "%DIST_DIR%\.git"      (echo   [警告] 混入了 .git 目录) else (echo   [OK] 无 git 仓库)
echo.
echo  可直接把上面的 zip 发给客户。
echo.
popd
endlocal
exit /b 0

:fail
echo.
echo 打包失败。
popd
endlocal
exit /b 1
