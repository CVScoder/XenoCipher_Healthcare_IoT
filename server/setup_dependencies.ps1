# PowerShell script to set up vcpkg and install dependencies for XenoCipher Server
# Run this script from PowerShell as Administrator

Write-Host "=== XenoCipher Server Dependency Setup ===" -ForegroundColor Cyan

# Step 1: Check if git is installed
Write-Host "`n[1/5] Checking for Git..." -ForegroundColor Yellow
try {
    $gitVersion = git --version
    Write-Host "✓ Git found: $gitVersion" -ForegroundColor Green
} catch {
    Write-Host "✗ Git is not installed. Please install Git from https://git-scm.com/download/win" -ForegroundColor Red
    exit 1
}

# Step 2: Install vcpkg
Write-Host "`n[2/5] Installing vcpkg..." -ForegroundColor Yellow
$vcpkgDir = "$env:USERPROFILE\vcpkg"
$vcpkgExe = "$vcpkgDir\vcpkg.exe"

if (Test-Path $vcpkgExe) {
    Write-Host "✓ vcpkg already installed at $vcpkgDir" -ForegroundColor Green
} else {
    Write-Host "Cloning vcpkg to $vcpkgDir..." -ForegroundColor Yellow
    cd $env:USERPROFILE
    git clone https://github.com/Microsoft/vcpkg.git
    cd vcpkg
    .\bootstrap-vcpkg.bat
    Write-Host "✓ vcpkg installed successfully" -ForegroundColor Green
}

# Step 3: Install libpqxx
Write-Host "`n[3/5] Installing libpqxx via vcpkg..." -ForegroundColor Yellow
& $vcpkgExe install libpqxx:x64-windows
if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ libpqxx installed successfully" -ForegroundColor Green
} else {
    Write-Host "✗ Failed to install libpqxx" -ForegroundColor Red
    exit 1
}

# Step 4: Install Crow (if available via vcpkg, otherwise download manually)
Write-Host "`n[4/5] Installing Crow framework..." -ForegroundColor Yellow
$crowVcpkg = & $vcpkgExe search crow
if ($crowVcpkg -match "crow") {
    Write-Host "Installing Crow via vcpkg..." -ForegroundColor Yellow
    & $vcpkgExe install crow:x64-windows
} else {
    Write-Host "Crow not found in vcpkg. Downloading manually..." -ForegroundColor Yellow
    $serverDir = Split-Path -Parent $PSScriptRoot
    $crowDir = Join-Path $serverDir "server\include\crow"
    
    if (-not (Test-Path $crowDir)) {
        New-Item -ItemType Directory -Path $crowDir -Force | Out-Null
        Write-Host "Downloading Crow from GitHub..." -ForegroundColor Yellow
        $crowUrl = "https://github.com/CrowCpp/crow/archive/refs/heads/main.zip"
        $zipPath = "$env:TEMP\crow.zip"
        Invoke-WebRequest -Uri $crowUrl -OutFile $zipPath
        Expand-Archive -Path $zipPath -DestinationPath "$env:TEMP\crow" -Force
        Copy-Item -Path "$env:TEMP\crow\crow-main\include\crow\*" -Destination $crowDir -Recurse -Force
        Remove-Item -Path $zipPath -Force
        Remove-Item -Path "$env:TEMP\crow" -Recurse -Force
        Write-Host "✓ Crow downloaded to $crowDir" -ForegroundColor Green
    } else {
        Write-Host "✓ Crow already exists at $crowDir" -ForegroundColor Green
    }
}

# Step 5: Integrate vcpkg with CMake
Write-Host "`n[5/5] Integrating vcpkg with CMake..." -ForegroundColor Yellow
& $vcpkgExe integrate install
Write-Host "✓ vcpkg integrated with CMake" -ForegroundColor Green

Write-Host "`n=== Setup Complete ===" -ForegroundColor Cyan
Write-Host "vcpkg location: $vcpkgDir" -ForegroundColor White
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "1. Rebuild your CMake project" -ForegroundColor White
Write-Host "2. Update .vscode/c_cpp_properties.json with vcpkg include paths" -ForegroundColor White

