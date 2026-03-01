@echo off
:: ============================================================
:: OpenEditor.bat
:: Launches Unreal Editor directly with PirateGame loaded
:: Faster than double-clicking the .uproject when UE is cold
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

set EDITOR=%UE_PATH%\Engine\Binaries\Win64\UnrealEditor.exe

echo.
echo Launching Unreal Editor...
echo Project: %PROJECT_FILE%
echo.

start "" "%EDITOR%" "%PROJECT_FILE%"
