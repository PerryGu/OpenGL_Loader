# Fixing Build Errors - ModelManager Linker Errors

## Problem
You're seeing linker errors (LNK2019) for all ModelManager methods. This means the linker can't find the implementation of ModelManager.

## Solution

The new `model_manager.cpp` file needs to be included in the build. Since you're using CMake, you need to regenerate the CMake cache.

### Option 1: Regenerate CMake in Visual Studio (Recommended)

1. In Visual Studio, go to **Project** → **CMake Settings for OpenGL_loader**
2. Or right-click on `CMakeLists.txt` in Solution Explorer
3. Select **Delete Cache and Reconfigure**
4. Wait for CMake to regenerate
5. Rebuild the solution (Build → Rebuild Solution)

### Option 2: Manual CMake Regeneration

1. Close Visual Studio
2. Delete the `out\build\x64-Debug` folder (or just the `CMakeFiles` subfolder)
3. Reopen Visual Studio
4. Visual Studio should automatically regenerate CMake
5. Build the project

### Option 3: Command Line CMake

If the above doesn't work, you can regenerate from command line:

```powershell
cd "F:\Work_stuff\VisualStudio_cursor\OpenGL_loader"
Remove-Item -Recurse -Force "out\build\x64-Debug\CMakeFiles" -ErrorAction SilentlyContinue
```

Then in Visual Studio, the CMake cache will regenerate automatically.

## Verification

After regenerating, you should see:
- `model_manager.cpp` in the Solution Explorer under Source Files
- No linker errors
- Successful build

## Why This Happens

CMake uses `GLOB_RECURSE` in `CMakeLists.txt` to automatically find source files, but Visual Studio's CMake integration caches the file list. When new files are added, the cache needs to be regenerated.

## Files That Should Be Included

After regeneration, these files should be in the build:
- `src/graphics/model_manager.cpp` ✅
- `src/graphics/model_manager.h` ✅ (header, included automatically)

If `model_manager.cpp` still doesn't appear, you may need to manually add it to the CMakeLists.txt (though GLOB_RECURSE should catch it).
