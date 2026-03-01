@echo off
:: ============================================================
:: GenerateProject.bat
:: Regenerates Visual Studio / Rider project files (.sln)
:: Run this after pulling, adding C++ files, or changing modules
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

set UBT=%UE_PATH%\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe

echo.
echo ============================================================
echo  Generating project files for PirateGame
echo  Engine: %UE_PATH%
echo ============================================================
echo.

"%UBT%" -projectfiles -project="%PROJECT_FILE%" -game -rocket -progress

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Project file generation failed.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [OK] Project files generated. Open PirateGame.sln in Visual Studio or Rider.
pause
