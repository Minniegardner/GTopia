@echo off
setlocal enabledelayedexpansion

cd /d %~dp0\..

set "SRC_DIR=ReGTByVinz"
set "BUILD_DIR=ReGTByVinz\build"
set "GENERATOR="
set "PROBE_DIR=ReGTByVinz\.cmake_probe"

echo [1/4] Updating submodules...
if exist .git (
    git submodule update --init --recursive
)

echo [2/4] Selecting CMake generator...
call :try_generator "Visual Studio 17 2022"
if not defined GENERATOR call :try_generator "Visual Studio 16 2019"
if not defined GENERATOR call :try_generator "Ninja"
if not defined GENERATOR call :try_generator "MinGW Makefiles"

if not defined GENERATOR (
    echo Failed to find a working CMake generator plus C++ toolchain.
    echo Install one of these then retry:
    echo   - Visual Studio 2022 Build Tools ^(Desktop development with C++^)
    echo   - Ninja + MSVC/clang-cl/g++ in PATH
    echo   - MinGW g++ for "MinGW Makefiles"
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
if "%GENERATOR%"=="Ninja" (
    echo   %BUILD_DIR%\ReGTByVinz.exe
) else if "%GENERATOR%"=="MinGW Makefiles" (
    echo   %BUILD_DIR%\ReGTByVinz.exe
) else (
    echo   %BUILD_DIR%\Release\ReGTByVinz.exe
)
exit /b 0

:fail
echo.
echo Setup or build failed.
exit /b 1

:try_generator
set "TRY_GEN=%~1"
if exist "%PROBE_DIR%" rmdir /s /q "%PROBE_DIR%"

cmake -S "%SRC_DIR%" -B "%PROBE_DIR%" -G "%TRY_GEN%" >nul 2>nul
if not errorlevel 1 (
    set "GENERATOR=%TRY_GEN%"
)

if exist "%PROBE_DIR%" rmdir /s /q "%PROBE_DIR%"
exit /b 0
