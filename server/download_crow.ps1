# Quick script to download and install Crow framework
# Run this from PowerShell

Write-Host "Downloading Crow framework..." -ForegroundColor Cyan

# Get the script directory and build the crow directory path
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$crowDir = Join-Path $scriptDir "include\crow"
$zipPath = "$env:TEMP\crow.zip"

Write-Host "Target directory: $crowDir" -ForegroundColor Yellow

# Create directory
if (-not (Test-Path $crowDir)) {
    New-Item -ItemType Directory -Path $crowDir -Force | Out-Null
    Write-Host "Created directory: $crowDir" -ForegroundColor Green
}

# Try multiple download URLs
$downloadUrls = @(
    "https://github.com/CrowCpp/crow/archive/refs/heads/master.zip",
    "https://github.com/CrowCpp/crow/archive/master.zip",
    "https://codeload.github.com/CrowCpp/crow/zip/master"
)

$downloadSuccess = $false
foreach ($crowUrl in $downloadUrls) {
    Write-Host "Trying: $crowUrl" -ForegroundColor Yellow
    try {
        Invoke-WebRequest -Uri $crowUrl -OutFile $zipPath -UseBasicParsing -ErrorAction Stop
        Write-Host "[OK] Download complete" -ForegroundColor Green
        $downloadSuccess = $true
        break
    } catch {
        Write-Host "[WARN] Failed: $_" -ForegroundColor Yellow
        continue
    }
}

if (-not $downloadSuccess) {
    Write-Host "[ERROR] All download attempts failed" -ForegroundColor Red
    Write-Host "Please download Crow manually from: https://github.com/CrowCpp/crow" -ForegroundColor Yellow
    Write-Host "Extract and copy include/crow/* to: $crowDir" -ForegroundColor Yellow
    exit 1
}

# Extract and copy
Write-Host "Extracting and installing..." -ForegroundColor Yellow
try {
    Expand-Archive -Path $zipPath -DestinationPath "$env:TEMP\crow" -Force
    
    # Try to find the crow include directory (handle different branch names)
    $possiblePaths = @(
        "$env:TEMP\crow\crow-master\include\crow",
        "$env:TEMP\crow\crow-main\include\crow",
        "$env:TEMP\crow\crow\include\crow"
    )
    
    $crowSource = $null
    foreach ($path in $possiblePaths) {
        if (Test-Path $path) {
            $crowSource = $path
            Write-Host "Found Crow at: $path" -ForegroundColor Green
            break
        }
    }
    
    if ($null -eq $crowSource) {
        Write-Host "[ERROR] Could not find Crow include directory in archive" -ForegroundColor Red
        Write-Host "Archive contents:" -ForegroundColor Yellow
        Get-ChildItem "$env:TEMP\crow" | ForEach-Object { Write-Host "  - $($_.Name)" }
        exit 1
    }
    
    Copy-Item -Path "$crowSource\*" -Destination $crowDir -Recurse -Force
    Write-Host "[OK] Crow installed to: $crowDir" -ForegroundColor Green
    
    # Verify installation
    if (Test-Path "$crowDir\crow.h") {
        Write-Host "[OK] Verification successful - crow.h found" -ForegroundColor Green
    } else {
        Write-Host "[WARN] crow.h not found, but files were copied" -ForegroundColor Yellow
    }
    
    # Cleanup
    Remove-Item -Path $zipPath -Force -ErrorAction SilentlyContinue
    Remove-Item -Path "$env:TEMP\crow" -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "[OK] Cleanup complete" -ForegroundColor Green
} catch {
    Write-Host "[ERROR] Installation failed: $_" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Crow framework is now installed!" -ForegroundColor Cyan
Write-Host "Location: $crowDir" -ForegroundColor White
