# Fix: zlib.dll Architecture Mismatch

## Problem
Your `zlib.dll` is **32-bit (x86)** but your application is **64-bit (x64)**. This causes error `0xc000007b`.

## Solution
You need to replace `zlib.dll` with a **64-bit (x64)** version.

## Quick Fix Options

### Option 1: Download Pre-built 64-bit zlib.dll (Recommended)
1. Visit one of these sources:
   - **vcpkg** (if you have it): `vcpkg install zlib:x64-windows`
   - **Conan** (if you have it): `conan install zlib/1.2.13@ -s arch=x86_64`
   - **GitHub releases**: Search for "zlib windows x64 dll" or "zlib vc142 x64"

2. Make sure the DLL is:
   - **64-bit (x64)** architecture
   - Compiled with **Visual Studio 2019** (vc142) or compatible
   - **Multi-threaded** (MT) runtime library (to match assimp-vc142-mt.dll)

3. Replace the file:
   - Copy the new `zlib.dll` to the project root directory
   - It will be automatically copied to the output directory on next build

### Option 2: Build from Source
1. Download zlib source: https://www.zlib.net/ or https://github.com/madler/zlib
2. Open the Visual Studio solution/project
3. Set configuration to **x64** and **Release** or **Debug**
4. Set runtime library to **Multi-threaded** (/MT) to match assimp
5. Build the DLL
6. Copy the resulting `zlib.dll` to your project root

### Option 3: Get from Same Source as Assimp
If you got `assimp-vc142-mt.dll` from a specific source (like a package manager or pre-built binaries), get `zlib.dll` from the same source to ensure compatibility.

## Verification
After replacing `zlib.dll`, run:
```powershell
.\check_runtime_error.ps1
```

The script will verify that `zlib.dll` is now 64-bit.

## Important Notes
- The `zlib.dll` must match the architecture of your executable (x64)
- It should be compiled with the same Visual Studio version (2019 = vc142)
- The runtime library should match (MT = Multi-threaded, as in assimp-vc142-mt.dll)

## After Fixing
1. Replace `zlib.dll` in the root directory
2. Rebuild the project (or manually copy to `out\build\x64-Debug\Debug\`)
3. Run the application - it should work now!
