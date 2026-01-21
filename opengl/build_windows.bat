@echo off
REM Windows build script
chcp 65001 >nul
echo ========================================
echo OpenGL Project Build Script
echo ========================================
echo.

REM Check if CMake is available
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake command not found!
    echo.
    echo Please install CMake first:
    echo 1. Visit https://cmake.org/download/
    echo 2. Download and install Windows x64 Installer
    echo 3. Check "Add CMake to system PATH" during installation
    echo 4. Restart PowerShell after installation
    echo.
    echo Or use Visual Studio to open CMakeLists.txt
    echo.
    pause
    exit /b 1
)

echo [CHECK] CMake found
cmake --version
echo.

if not exist build (
    mkdir build
    echo [CREATE] build folder
)
cd build

echo [CONFIG] Configuring CMake...
cmake ..

if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

echo Building project...
cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo Build successful!
echo Executable location: build\Release\HelloGL.exe
echo.
echo Press any key to run...
pause > nul

cd Release
if exist HelloGL.exe (
    HelloGL.exe
) else (
    echo Executable not found!
    pause
)

