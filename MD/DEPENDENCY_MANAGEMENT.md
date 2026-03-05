# Dependency Management Guide

## Overview
This document tracks all external dependencies required by the OpenGL_loader project, their architectures, versions, and management procedures.

## Runtime Dependencies (DLLs)

### Required DLLs

| DLL | Architecture | Version | Source | Location |
|-----|-------------|---------|--------|----------|
| `assimp-vc142-mt.dll` | **x64** | v142 (VS 2019) | Assimp distribution | Project root + Output dir |
| `zlib.dll` | **x64** | 1.3.1 | Built from zlib.net source | Project root + Output dir |

### Architecture Requirements
⚠️ **CRITICAL**: All DLLs MUST be **64-bit (x64)** to match the application architecture.

### Visual C++ Runtime
- **MSVCP140.dll** - Visual C++ 2015-2022 Runtime
- **VCRUNTIME140.dll** - Visual C++ 2015-2022 Runtime
- **VCRUNTIME140_1.dll** - Visual C++ 2015-2022 Runtime
- These are typically installed system-wide via Visual C++ Redistributable

## Build-Time Dependencies

### Static Libraries
- **GLFW** - Built from source via CMake FetchContent (static)
- **OpenGL** - System library (opengl32.lib on Windows)
- **Assimp** - `assimp-vc142-mt.lib` (static library, but requires DLL at runtime)

### Header-Only Libraries
- **GLM** - Header-only math library
- **ImGui** - Header-only GUI library
- **openFBX** - Header + source files

## DLL Deployment

### Automatic Deployment (CMakeLists.txt)
The `CMakeLists.txt` automatically copies required DLLs to the output directory via POST_BUILD commands:

```cmake
# Copies zlib.dll and assimp-vc142-mt.dll to output directory
add_custom_command(TARGET OpenGL_loader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/zlib.dll"
        "$<TARGET_FILE_DIR:OpenGL_loader>"
)
```

### Manual Deployment
If automatic copying fails, manually copy DLLs to:
```
out\build\x64-Debug\Debug\
```

## Dependency Verification

### Quick Check
Run the diagnostic script:
```powershell
.\check_runtime_error.ps1
```

This script verifies:
- ✅ DLL presence
- ✅ Architecture (x64 vs x86)
- ✅ Visual C++ Runtime availability
- ✅ Dependency chain

### Manual Verification
Use `dumpbin` to check DLL architecture:
```powershell
dumpbin /headers zlib.dll | Select-String "machine"
```
Should show: `8664 machine (x64)` for 64-bit DLLs

## Updating Dependencies

### Updating zlib.dll
1. Download latest source from https://zlib.net/
2. Build as x64 with Visual Studio 2019 (v142)
3. Set runtime library to Multi-threaded (/MT)
4. Replace `zlib.dll` in project root
5. Rebuild project (or manually copy to output)

### Updating Assimp
1. Obtain x64 version matching your Visual Studio version (vc142 for VS 2019)
2. Replace both `.lib` and `.dll` files
3. Update include paths if needed
4. Rebuild project

## Common Issues

### Error 0xc000007b
**Cause**: Architecture mismatch (32-bit DLL with 64-bit app, or vice versa)
**Solution**: Verify all DLLs are x64 using `check_runtime_error.ps1`

### Missing DLL at Runtime
**Cause**: DLL not copied to output directory
**Solution**: 
1. Check CMakeLists.txt POST_BUILD commands
2. Manually copy DLL to output directory
3. Ensure DLL is in same directory as executable

### Wrong Runtime Library
**Cause**: DLL built with /MD but app uses /MT (or vice versa)
**Solution**: Rebuild DLL with matching runtime library setting

## Build Scripts Reference

### `download_and_build_zlib.ps1`
- Downloads zlib source from zlib.net
- Builds 64-bit DLL with Visual Studio 2019
- Automatically retargets project files
- Copies DLL to project root

### `check_runtime_error.ps1`
- Comprehensive diagnostic tool
- Checks DLL presence and architecture
- Verifies Visual C++ Runtime
- Provides detailed error reporting

### `fix_zlib_architecture.ps1`
- Verifies zlib.dll architecture
- Provides instructions if wrong architecture detected
- Can auto-fix if correct DLL is available

## Best Practices

1. **Always verify architecture** before deploying DLLs
2. **Keep DLLs in version control** or document exact versions
3. **Use diagnostic scripts** before reporting issues
4. **Match runtime libraries** between DLLs and application
5. **Document DLL sources** for future reference

## Version History

### 2026-02-05
- Fixed zlib.dll architecture mismatch (32-bit → 64-bit)
- Built zlib 1.3.1 from source with VS 2019 (v142)
- Updated CMakeLists.txt for automatic DLL deployment
- Created diagnostic and build scripts
