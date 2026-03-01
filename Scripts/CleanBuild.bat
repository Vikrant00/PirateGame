@echo off
:: ============================================================
:: CleanBuild.bat
:: Deletes Binaries/ and Intermediate/ to force a full rebuild
:: Use when: build is broken, switching branches, weird errors
:: WARNING: Next build will be slow (full recompile)
:: ============================================================
setlocal

set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..

echo.
echo ============================================================
echo  CLEAN BUILD
echo  This will delete:
echo    - %PROJECT_ROOT%\Binaries\
echo    - %PROJECT_ROOT%\Intermediate\
echo ============================================================
echo.
set /p CONFIRM=Are you sure? (y/n):
if /i not "%CONFIRM%"=="y" (
    echo Cancelled.
    pause
    exit /b 0
)

echo.
echo Deleting Binaries...
if exist "%PROJECT_ROOT%\Binaries" rmdir /s /q "%PROJECT_ROOT%\Binaries"

echo Deleting Intermediate...
if exist "%PROJECT_ROOT%\Intermediate" rmdir /s /q "%PROJECT_ROOT%\Intermediate"

echo.
echo [OK] Clean done. Run BuildEditor.bat or open the project to rebuild.
pause
