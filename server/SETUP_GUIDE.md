# Server Dependencies Setup Guide

This guide will help you install Crow and libpqxx dependencies for the XenoCipher server.

## Prerequisites

- **Git** (for cloning vcpkg and Crow)
- **CMake** (already installed if you're building)
- **Visual Studio** or **Build Tools** with C++ support
- **PostgreSQL** client libraries (libpq)

## Option 1: Automated Setup (Recommended)

### Step 1: Run the PowerShell Setup Script

1. Open PowerShell as **Administrator**
2. Navigate to your project directory:
   ```powershell
   cd D:\Projects\XenoCipher_Healthcare_IoT\server
   ```
3. Run the setup script:
   ```powershell
   .\setup_dependencies.ps1
   ```

This script will:
- Install vcpkg (if not already installed)
- Install libpqxx via vcpkg
- Download and install Crow framework
- Integrate vcpkg with CMake

### Step 2: Rebuild CMake Project

After the script completes, rebuild your CMake project:

```powershell
cd D:\Projects\XenoCipher_Healthcare_IoT\server
mkdir build -Force
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$env:USERPROFILE\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build . --config Release
```

## Option 2: Manual Setup

### Step 1: Install vcpkg

1. Open PowerShell and run:
   ```powershell
   cd $env:USERPROFILE
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   ```

2. Integrate vcpkg with CMake:
   ```powershell
   .\vcpkg integrate install
   ```

### Step 2: Install libpqxx

```powershell
.\vcpkg install libpqxx:x64-windows
```

### Step 3: Install Crow Framework

**Option A: Via vcpkg (if available)**
```powershell
.\vcpkg install crow:x64-windows
```

**Option B: Manual Installation**
1. Download Crow from GitHub:
   - Go to: https://github.com/CrowCpp/crow
   - Click "Code" → "Download ZIP"
   - Extract the ZIP file

2. Copy Crow headers to your project:
   ```powershell
   # Create the directory
   mkdir D:\Projects\XenoCipher_Healthcare_IoT\server\include\crow -Force
   
   # Copy the header files from the extracted Crow folder
   # From: crow-main\include\crow\*
   # To: D:\Projects\XenoCipher_Healthcare_IoT\server\include\crow\
   ```

   Or use PowerShell:
   ```powershell
   $crowZip = "$env:USERPROFILE\Downloads\crow-main.zip"  # Adjust path
   Expand-Archive -Path $crowZip -DestinationPath "$env:TEMP\crow"
   Copy-Item -Path "$env:TEMP\crow\crow-main\include\crow\*" -Destination "D:\Projects\XenoCipher_Healthcare_IoT\server\include\crow\" -Recurse
   ```

### Step 4: Update VSCode IntelliSense

The `.vscode/c_cpp_properties.json` file should be updated with the correct include paths. The setup script should handle this, but if you need to do it manually:

1. Open `.vscode/c_cpp_properties.json`
2. Add these paths to the `includePath` array:
   ```json
   "${workspaceFolder}/server/include",
   "C:/Users/chall/vcpkg/installed/x64-windows/include",
   "C:/Users/chall/vcpkg/installed/x64-windows/include/pqxx"
   ```
   (Adjust the user path `chall` to your username)

### Step 5: Configure CMake with vcpkg

When configuring CMake, specify the vcpkg toolchain:

```powershell
cmake .. -DCMAKE_TOOLCHAIN_FILE=$env:USERPROFILE\vcpkg\scripts\buildsystems\vcpkg.cmake
```

## Troubleshooting

### Error: "cannot open source file 'crow.h'"

1. Check if Crow is installed in `server/include/crow/crow.h`
2. Verify the include path in `c_cpp_properties.json`
3. Restart VSCode to reload IntelliSense

### Error: "cannot open source file 'pqxx/pqxx'"

1. Verify libpqxx is installed: `vcpkg list libpqxx`
2. Check if the vcpkg path is correct in `c_cpp_properties.json`
3. Make sure you ran `vcpkg integrate install`

### Error: "CMake could not find Crow"

1. If using vcpkg, ensure you specified the toolchain file:
   ```powershell
   cmake .. -DCMAKE_TOOLCHAIN_FILE=$env:USERPROFILE\vcpkg\scripts\buildsystems\vcpkg.cmake
   ```
2. If manually installed, check that `server/include/crow/crow.h` exists

### Error: "PostgreSQL not found"

1. Install PostgreSQL from: https://www.postgresql.org/download/windows/
2. Or install just the client libraries
3. Ensure PostgreSQL bin directory is in your PATH

## Verification

After setup, verify the installation:

1. **Check vcpkg packages:**
   ```powershell
   vcpkg list
   ```
   You should see `libpqxx` listed.

2. **Check Crow installation:**
   ```powershell
   Test-Path "D:\Projects\XenoCipher_Healthcare_IoT\server\include\crow\crow.h"
   ```
   Should return `True`.

3. **Build the server:**
   ```powershell
   cd D:\Projects\XenoCipher_Healthcare_IoT\server\build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=$env:USERPROFILE\vcpkg\scripts\buildsystems\vcpkg.cmake
   cmake --build . --config Release
   ```

## Next Steps

Once dependencies are installed, you can:
1. Build the server project
2. Run the server: `.\build\Release\server.exe`
3. Test the endpoints with your ESP32 client

