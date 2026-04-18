@echo off
setlocal enabledelayedexpansion
cd /d %~dp0

echo [1/3] Checking repository dependencies...
if exist .git (
    git submodule update --init --recursive
)

echo [2/3] Configuring ReGT By Vinz...
if exist ReGTByVinz\build rmdir /s /q ReGTByVinz\build
cmake -S ReGTByVinz -B ReGTByVinz\build -G "Visual Studio 17 2022"
if errorlevel 1 goto :fail

echo [3/3] Building ReGT By Vinz...
cmake --build ReGTByVinz\build --config Release
if errorlevel 1 goto :fail

echo.
echo Build complete.
goto :eof

:fail
echo.
echo Setup or build failed. Make sure Visual Studio 2022 Build Tools and CMake are installed.
exit /b 1
