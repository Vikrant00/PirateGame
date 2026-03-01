@echo off
:: ============================================================
:: BuildEditor.bat
:: Compiles PirateGame C++ code (DevelopmentEditor)
:: Run this after pulling new C++ changes from teammates
:: ============================================================
setlocal

set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..
set PROJECT_FILE=%PROJECT_ROOT%\PirateGame.uproject

:: Load local engine path
if exist "%SCRIPT_DIR%_LocalConfig.bat" (
    call "%SCRIPT_DIR%_LocalConfig.bat"
) else (
    echo [ERROR] _LocalConfig.bat not found.
    echo Copy Scripts\_LocalConfig.bat.example to Scripts\_LocalConfig.bat and set your UE path.
    pause
    exit /b 1
)

:: Default build config
if not defined BUILD_CONFIG set BUILD_CONFIG=Development

set BUILD_BAT=%UE_PATH%\Engine\Build\BatchFiles\Build.bat

echo.
echo ============================================================
echo  Building PirateGameEditor [%BUILD_CONFIG%Editor / Win64]
echo  Engine: %UE_PATH%
echo ============================================================
echo.

call "%BUILD_BAT%" PirateGameEditor Win64 %BUILD_CONFIG%Editor -Project="%PROJECT_FILE%" -WaitMutex -FromMsBuild

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Build FAILED. Check errors above.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [OK] Build succeeded.
pause
