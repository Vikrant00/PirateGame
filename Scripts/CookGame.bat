@echo off
:: ============================================================
:: CookGame.bat
:: Cooks and packages PirateGame for Windows (Shipping)
:: Output goes to: Build\WindowsNoEditor\
:: ============================================================
setlocal

set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..
set PROJECT_FILE=%PROJECT_ROOT%\PirateGame.uproject
set OUTPUT_DIR=%PROJECT_ROOT%\Build\WindowsNoEditor

:: Load local engine path
if exist "%SCRIPT_DIR%_LocalConfig.bat" (
    call "%SCRIPT_DIR%_LocalConfig.bat"
) else (
    echo [ERROR] _LocalConfig.bat not found.
    echo Copy Scripts\_LocalConfig.bat.example to Scripts\_LocalConfig.bat and set your UE path.
    pause
    exit /b 1
)

set UAT=%UE_PATH%\Engine\Build\BatchFiles\RunUAT.bat

echo.
echo ============================================================
echo  Cooking PirateGame for Windows (Shipping)
echo  Output: %OUTPUT_DIR%
echo  Engine: %UE_PATH%
echo ============================================================
echo.

call "%UAT%" BuildCookRun ^
    -project="%PROJECT_FILE%" ^
    -noP4 ^
    -platform=Win64 ^
    -clientconfig=Shipping ^
    -cook ^
    -allmaps ^
    -build ^
    -stage ^
    -pak ^
    -archive ^
    -archivedirectory="%OUTPUT_DIR%"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Cook/Package FAILED. Check errors above.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [OK] Game cooked and packaged to: %OUTPUT_DIR%
pause
