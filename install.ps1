# CLI DECOR PowerShell Installer for Windows
$ErrorActionPreference = "Stop"

Write-Host "Installing CLI DECOR (C++ Engine) for Windows..." -ForegroundColor Cyan

$InstallDir = Join-Path $HOME ".config\clidecor"
if (!(Test-Path $InstallDir)) {
    New-Item -ItemType Directory -Path $InstallDir | Out-Null
}

$Gcc = Get-Command g++ -ErrorAction SilentlyContinue

if ($Gcc) {
    Write-Host "Compiling C++ binary with g++..." -ForegroundColor Green
    & g++ -O3 -std=c++17 src\main.cpp -o "$InstallDir\clidecor.exe" -lws2_32
} elseif (Test-Path "clidecor.exe") {
    Write-Host "Copying pre-built clidecor.exe..." -ForegroundColor Green
    Copy-Item "clidecor.exe" "$InstallDir\clidecor.exe" -Force
} else {
    Write-Error "g++ compiler not found! Please install MinGW / GCC."
    exit 1
}

if (!(Test-Path "$InstallDir\config.conf")) {
    Copy-Item "config.conf" "$InstallDir\config.conf" -Force
}

# Add to PowerShell Profile
if (!(Test-Path $PROFILE)) {
    New-Item -Type File -Path $PROFILE -Force | Out-Null
}

$Line = "& `"$InstallDir\clidecor.exe`""
$ProfileContent = Get-Content $PROFILE -ErrorAction SilentlyContinue
if ($ProfileContent -notcontains $Line) {
    Add-Content -Path $PROFILE -Value "`n# CLI DECOR - runs on new terminal`n$Line"
    Write-Host "Added CLI DECOR to PowerShell profile ($PROFILE)" -ForegroundColor Green
} else {
    Write-Host "CLI DECOR already in PowerShell profile." -ForegroundColor Yellow
}

Write-Host "`nDone! Open a new PowerShell window or test immediately by running:" -ForegroundColor Cyan
Write-Host "  & `"$InstallDir\clidecor.exe`"" -ForegroundColor Yellow
