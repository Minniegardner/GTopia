@echo off
setlocal enabledelayedexpansion

cd /d %~dp0\..

set "SRC_DIR=ReGTByVinz"
set "BUILD_DIR=ReGTByVinz\build"
set "GENERATOR="

echo [1/4] Updating submodules...
if exist .git (
    git submodule update --init --recursive
)

echo [2/4] Selecting CMake generator...
cmake -G "Visual Studio 17 2022" -N . >nul 2>nul
if not errorlevel 1 set "GENERATOR=Visual Studio 17 2022"

if not defined GENERATOR (
    cmake -G "Visual Studio 16 2019" -N . >nul 2>nul
    if not errorlevel 1 set "GENERATOR=Visual Studio 16 2019"
)

if not defined GENERATOR (
    cmake -G "Ninja" -N . >nul 2>nul
    if not errorlevel 1 set "GENERATOR=Ninja"
)

if not defined GENERATOR (
    echo Failed to find a supported CMake generator.
    echo Install Visual Studio Build Tools 2022 or Ninja, then retry.
    exit /b 1
)

echo Using generator: %GENERATOR%

echo [3/4] Configuring ReGT By Vinz...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
cmake -S "%SRC_DIR%" -B "%BUILD_DIR%" -G "%GENERATOR%"
if errorlevel 1 goto :fail

echo [4/4] Building ReGT By Vinz...
if "%GENERATOR%"=="Ninja" (
    cmake --build "%BUILD_DIR%"
) else (
    cmake --build "%BUILD_DIR%" --config Release
)
if errorlevel 1 goto :fail

echo.
echo Build complete.
echo Output executable:
echo   %BUILD_DIR%\Release\ReGTByVinz.exe
exit /b 0

:fail
echo.
echo Setup or build failed.
exit /b 1
