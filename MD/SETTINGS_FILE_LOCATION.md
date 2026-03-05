# Settings File Location and Build Behavior

## Current Location

The `app_settings.ini` file is currently saved in:
```
out\build\x64-Debug\Debug\app_settings.ini
```

This is the **Debug output directory** where the executable runs from.

## Is the File Deleted During Build?

### Normal Build (F7 / Build Solution)
- ✅ **File is NOT deleted** - Normal builds only compile changed source files
- ✅ **Settings are preserved** - Your camera position and recent files remain intact

### Clean Solution (Build → Clean Solution)
- ⚠️ **File MAY be deleted** - Some clean operations remove all files in the Debug folder
- ⚠️ **Settings will be lost** - Camera position and recent files will reset to defaults

### Rebuild Solution (Build → Rebuild Solution)
- ⚠️ **File MAY be deleted** - Rebuild cleans and rebuilds everything
- ⚠️ **Settings will be lost** - Camera position and recent files will reset to defaults

### Full CMake Reconfiguration
- ⚠️ **File MAY be deleted** - If you delete the build directory or reconfigure CMake
- ⚠️ **Settings will be lost** - Camera position and recent files will reset to defaults

## Does the File Have to Be There?

**No, the file is optional!**

The application handles missing files gracefully:

1. **On Startup**: If `app_settings.ini` doesn't exist:
   - The app uses default camera settings
   - No recent files are loaded
   - The app runs normally

2. **On Save**: If the file doesn't exist:
   - The app creates it automatically
   - Settings are saved to the new file

3. **Error Handling**: The code checks if the file exists:
   ```cpp
   if (!file.is_open()) {
       std::cout << "[Settings] No settings file found, using defaults" << std::endl;
       return false;  // Uses default settings
   }
   ```

## Recommendations

### Option 1: Keep Current Location (Simple)
- **Pros**: Works automatically, no code changes needed
- **Cons**: May be deleted during Clean/Rebuild operations
- **Best for**: Development where losing settings occasionally is acceptable

### Option 2: Move to Project Root (Recommended)
Save the file in the project root directory instead of the Debug folder:
- **Location**: `F:\Work_stuff\VisualStudio_cursor\OpenGL_loader\app_settings.ini`
- **Pros**: Survives Clean/Rebuild operations
- **Cons**: Requires code change to use absolute or relative path
- **Best for**: Preserving settings across builds

### Option 3: Move to User AppData (Most Robust)
Save the file in the user's AppData directory:
- **Location**: `%APPDATA%\OpenGL_loader\app_settings.ini`
- **Pros**: Survives all builds, follows Windows conventions
- **Cons**: Requires code change, harder to find manually
- **Best for**: Production applications

## Current Behavior

The file is saved relative to the **current working directory** when the application runs, which is typically:
- `out\build\x64-Debug\Debug\` (where the executable is located)

This means:
- ✅ Normal builds preserve the file
- ⚠️ Clean/Rebuild may delete it
- ✅ File is optional - app works without it
- ✅ File is created automatically on first save

## Summary

| Operation | File Deleted? | Settings Lost? |
|-----------|---------------|----------------|
| Normal Build | ❌ No | ❌ No |
| Clean Solution | ⚠️ Maybe | ⚠️ Maybe |
| Rebuild Solution | ⚠️ Maybe | ⚠️ Maybe |
| File Missing | N/A | ✅ Uses defaults |

**The file is optional** - the application will work fine without it, using default settings.
