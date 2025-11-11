# Quick Start Guide - Fix Missing Dependencies

## Problem
You're getting these errors:
- `cannot open source file "crow.h"`
- `cannot open source file "pqxx/pqxx"`

## Solution - 3 Steps

### Step 1: Install vcpkg (One-time setup)

Open PowerShell and run:

```powershell
cd $env:USERPROFILE
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
```

**Note:** If you don't have Git, download it from https://git-scm.com/download/win

### Step 2: Install libpqxx

```powershell
cd $env:USERPROFILE\vcpkg
.\vcpkg install libpqxx:x64-windows
```

This will take a few minutes. Wait for it to complete.

### Step 3: Install Crow Framework

**Option A: Quick PowerShell script (Recommended)**
```powershell
cd D:\Projects\XenoCipher_Healthcare_IoT\server
.\download_crow.ps1
```

**Option B: Manual download**
1. Go to: https://github.com/CrowCpp/crow
2. Click "Code" → "Download ZIP"
3. Extract the ZIP
4. Copy the contents of `crow-main/include/crow/` to `D:\Projects\XenoCipher_Healthcare_IoT\server\include\crow/`

## Step 4: Update VSCode IntelliSense

1. Open `.vscode/c_cpp_properties.json`
2. Find the `includePath` array (around line 283)
3. Make sure these paths are present (adjust username `chall` to yours):
   ```json
   "${workspaceFolder}/server/include",
   "C:/Users/chall/vcpkg/installed/x64-windows/include",
   "C:/Users/chall/vcpkg/installed/x64-windows/include/pqxx"
   ```
4. Save the file
5. **Restart VSCode** (important!)

## Step 5: Rebuild CMake Project

```powershell
cd D:\Projects\XenoCipher_Healthcare_IoT\server\build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$env:USERPROFILE\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build . --config Release
```

## Verify Installation

Check if files exist:
```powershell
# Check Crow
Test-Path "D:\Projects\XenoCipher_Healthcare_IoT\server\include\crow\crow.h"

# Check vcpkg
$env:USERPROFILE\vcpkg\vcpkg.exe list
```

Both should return `True` or show the packages.

## Troubleshooting

**"vcpkg: command not found"**
- Make sure you completed Step 1
- Add vcpkg to PATH or use full path: `$env:USERPROFILE\vcpkg\vcpkg.exe`

**"Git: command not found"**
- Install Git from: https://git-scm.com/download/win
- Restart PowerShell after installation

**IntelliSense still shows errors**
- Restart VSCode
- Run: `Ctrl+Shift+P` → "C/C++: Reset IntelliSense Database"

**CMake still can't find packages**
- Make sure you're using the vcpkg toolchain:
  ```powershell
  cmake .. -DCMAKE_TOOLCHAIN_FILE=$env:USERPROFILE\vcpkg\scripts\buildsystems\vcpkg.cmake
  ```

## All-in-One Script

If you prefer, run the automated setup script:

```powershell
cd D:\Projects\XenoCipher_Healthcare_IoT\server
.\setup_dependencies.ps1
```

Then rebuild your CMake project as shown in Step 5.

