@echo off
setlocal enabledelayedexpansion

echo Installing CLI DECOR (C++ Engine) for Windows...

set "INSTALL_DIR=%USERPROFILE%\.config\clidecor"
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo Compiling C++ binary with g++...
    g++ -O3 -std=c++17 src\main.cpp -o "%INSTALL_DIR%\clidecor.exe" -lws2_32
    if %errorlevel% neq 0 (
        echo Compilation failed.
        exit /b %errorlevel%
    )
) else (
    if exist clidecor.exe (
        echo Copying pre-built clidecor.exe...
        copy /y clidecor.exe "%INSTALL_DIR%\clidecor.exe" >nul
    ) else (
        echo Error: g++ compiler not found! Please install MinGW / GCC or MSVC.
        exit /b 1
    )
)

if not exist "%INSTALL_DIR%\config.conf" (
    copy /y config.conf "%INSTALL_DIR%\config.conf" >nul
)

echo.
echo CLI DECOR installed successfully to %INSTALL_DIR%\clidecor.exe
echo.
echo To run automatically in PowerShell on startup, add this to your PowerShell profile ($PROFILE):
echo   ^& "%INSTALL_DIR%\clidecor.exe"
echo.
echo Test it now by running:
echo   "%INSTALL_DIR%\clidecor.exe"
echo.
