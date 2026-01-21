@echo off
chcp 65001 >nul
echo ========================================
echo GLFW Windows Library Quick Install Script
echo ========================================
echo.

set LIB_DIR=%~dp0lib
set INCLUDE_DIR=%~dp0include

echo Checking for GLFW Windows library...
echo.

REM Check if Windows GLFW already exists
if exist "%LIB_DIR%\glfw3.lib" (
    echo [OK] Found glfw3.lib
    goto :check_dll
) else (
    echo [X] glfw3.lib not found
)

if exist "%LIB_DIR%\glfw3.dll" (
    echo [OK] Found glfw3.dll
) else (
    echo [X] glfw3.dll not found
)

echo.
echo ========================================
echo Need to download GLFW Windows version
echo ========================================
echo.
echo Please choose installation method:
echo.
echo 1. Auto download (requires PowerShell and internet)
echo 2. Manual download instructions
echo 3. Exit
echo.
set /p choice=Enter option (1/2/3): 

if "%choice%"=="1" goto :auto_download
if "%choice%"=="2" goto :manual_guide
if "%choice%"=="3" exit /b 0

:auto_download
echo.
echo Downloading GLFW...
echo.

REM Create temp directory
set TEMP_DIR=%TEMP%\glfw_download
if not exist "%TEMP_DIR%" mkdir "%TEMP_DIR%"

REM Download using PowerShell
powershell -Command "& {[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri 'https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.bin.WIN64.zip' -OutFile '%TEMP_DIR%\glfw.zip'}"

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Download failed, please check network connection
    echo.
    goto :manual_guide
)

echo [OK] Download complete
echo.

REM Extract
powershell -Command "Expand-Archive -Path '%TEMP_DIR%\glfw.zip' -DestinationPath '%TEMP_DIR%' -Force"

echo Copying files...
echo.

REM Copy library files
if exist "%TEMP_DIR%\glfw-3.4.bin.WIN64\lib-vc2022\glfw3.lib" (
    copy /Y "%TEMP_DIR%\glfw-3.4.bin.WIN64\lib-vc2022\glfw3.lib" "%LIB_DIR%\"
    echo [OK] Copied glfw3.lib
) else if exist "%TEMP_DIR%\glfw-3.4.bin.WIN64\lib-vc2019\glfw3.lib" (
    copy /Y "%TEMP_DIR%\glfw-3.4.bin.WIN64\lib-vc2019\glfw3.lib" "%LIB_DIR%\"
    echo [OK] Copied glfw3.lib
)

if exist "%TEMP_DIR%\glfw-3.4.bin.WIN64\lib-vc2022\glfw3.dll" (
    copy /Y "%TEMP_DIR%\glfw-3.4.bin.WIN64\lib-vc2022\glfw3.dll" "%LIB_DIR%\"
    echo [OK] Copied glfw3.dll
) else if exist "%TEMP_DIR%\glfw-3.4.bin.WIN64\lib-vc2019\glfw3.dll" (
    copy /Y "%TEMP_DIR%\glfw-3.4.bin.WIN64\lib-vc2019\glfw3.dll" "%LIB_DIR%\"
    echo [OK] Copied glfw3.dll
)

REM Copy header files
if not exist "%INCLUDE_DIR%\GLFW" mkdir "%INCLUDE_DIR%\GLFW"
if exist "%TEMP_DIR%\glfw-3.4.bin.WIN64\include\GLFW\glfw3.h" (
    copy /Y "%TEMP_DIR%\glfw-3.4.bin.WIN64\include\GLFW\glfw3.h" "%INCLUDE_DIR%\GLFW\"
    echo [OK] Copied glfw3.h
)

REM Cleanup temp files
rmdir /s /q "%TEMP_DIR%"

echo.
echo [OK] GLFW installation complete!
echo.
goto :check_dll

:check_dll
if exist "%LIB_DIR%\glfw3.lib" (
    echo ========================================
    echo GLFW library ready!
    echo ========================================
    echo.
    echo Next steps:
    echo 1. Reconfigure CMake project in Visual Studio
    echo 2. Or delete build folder and rebuild
    echo.
) else (
    echo ========================================
    echo Installation incomplete
    echo ========================================
)

goto :end

:manual_guide
echo.
echo ========================================
echo Manual GLFW Installation Steps
echo ========================================
echo.
echo 1. Visit: https://www.glfw.org/download.html
echo    Or: https://github.com/glfw/glfw/releases
echo.
echo 2. Download "glfw-3.4.bin.WIN64.zip" (or latest version)
echo.
echo 3. After extraction, copy the following files:
echo.
echo    From: glfw-3.4.bin.WIN64\lib-vc2022\glfw3.lib
echo    To: %LIB_DIR%\glfw3.lib
echo.
echo    From: glfw-3.4.bin.WIN64\lib-vc2022\glfw3.dll
echo    To: %LIB_DIR%\glfw3.dll
echo.
echo    From: glfw-3.4.bin.WIN64\include\GLFW\glfw3.h
echo    To: %INCLUDE_DIR%\GLFW\glfw3.h
echo.
echo 4. After completion, reconfigure CMake project in Visual Studio
echo.
pause

:end
echo.
pause

