# Settings File Migration: INI to JSON

## Overview

The application settings file has been migrated from INI format (`app_settings.ini`) to JSON format (`app_settings.json`). This provides a cleaner structure, better extensibility, and a standard format.

## Changes Made

### 1. Added JSON Library

**File:** `Dependencies/include/nlohmann/json.hpp`

- Added **nlohmann/json** (v3.11.3) - a header-only JSON library
- No build system changes required (header-only)
- Modern C++ API with excellent error handling

### 2. Updated AppSettingsManager

**Files Modified:**
- `src/io/app_settings.h`
- `src/io/app_settings.cpp`

**Changes:**
- Changed default filename from `app_settings.ini` to `app_settings.json`
- Replaced INI parsing with JSON parsing
- Replaced INI writing with JSON writing
- Added `convertIniToJson()` method for backward compatibility

### 3. Updated Main Application

**File:** `src/main.cpp`

**Changes:**
- Updated `loadSettings()` call to use `"app_settings.json"` instead of `"app_settings.ini"`

## JSON File Format

### Structure

```json
{
    "camera": {
        "position": [0.0, 9.0, 30.0],
        "focusPoint": [0.0, 0.0, 0.0],
        "yaw": -90.0,
        "pitch": 0.0,
        "zoom": 45.0,
        "orbitDistance": 30.0
    },
    "window": {
        "posX": -1,
        "posY": -1,
        "width": 1920,
        "height": 1080
    },
    "grid": {
        "size": 20.0,
        "spacing": 1.0,
        "enabled": true,
        "majorLineColor": [0.5, 0.5, 0.5],
        "minorLineColor": [0.3, 0.3, 0.3],
        "centerLineColor": [0.8, 0.2, 0.2]
    },
    "environment": {
        "backgroundColor": [0.45, 0.55, 0.60, 1.0]
    },
    "recentFiles": [
        "path/to/file1.fbx",
        "path/to/file2.fbx"
    ]
}
```

### Benefits Over INI

1. **Cleaner Structure**
   - Colors as arrays: `[0.5, 0.5, 0.5]` instead of separate R/G/B keys
   - Vectors as arrays: `[0.0, 9.0, 30.0]` instead of separate X/Y/Z keys
   - Recent files as array instead of `File0`, `File1`, etc.

2. **Type Safety**
   - Numbers are numbers (not strings)
   - Booleans are booleans (not "true"/"false" strings)
   - Arrays are arrays (not multiple keys)

3. **Easier to Extend**
   - Adding new settings is straightforward
   - Nested structures are natural
   - No need to invent key naming conventions

4. **Standard Format**
   - Widely supported
   - Easy to parse programmatically
   - Better tooling support

## Backward Compatibility

### Automatic Conversion

When the application starts:
1. It first tries to load `app_settings.json`
2. If not found, it checks for `app_settings.ini`
3. If INI file exists, it automatically converts it to JSON
4. The converted JSON file is saved
5. The old INI file is **not deleted** (you can delete it manually)

### Conversion Process

The `convertIniToJson()` method:
- Parses the old INI file using the same logic as before
- Converts all settings to JSON format
- Saves the new JSON file
- Returns `true` on success

### Example Output

When conversion happens, you'll see:
```
[Settings] Found old INI file, converting to JSON...
[Settings] Successfully converted app_settings.ini to app_settings.json
[Settings] Loaded settings from app_settings.json
```

## Migration Steps (For Users)

1. **First Run After Update:**
   - Application automatically detects old `app_settings.ini`
   - Converts it to `app_settings.json`
   - Old INI file remains (safe to delete after verification)

2. **Manual Verification:**
   - Check that `app_settings.json` was created
   - Verify settings are correct
   - Delete `app_settings.ini` if desired

3. **No Action Required:**
   - If you don't have an old INI file, a new JSON file will be created with defaults
   - All settings work exactly the same way

## Code Changes Summary

### Before (INI):
```cpp
// Parsing
if (key == "PositionX") settings.camera.position.x = std::stof(value);
if (key == "PositionY") settings.camera.position.y = std::stof(value);
if (key == "PositionZ") settings.camera.position.z = std::stof(value);

// Saving
file << "PositionX=" << settings.camera.position.x << "\n";
file << "PositionY=" << settings.camera.position.y << "\n";
file << "PositionZ=" << settings.camera.position.z << "\n";
```

### After (JSON):
```cpp
// Parsing
if (cam.contains("position")) {
    settings.camera.position.x = cam["position"][0].get<float>();
    settings.camera.position.y = cam["position"][1].get<float>();
    settings.camera.position.z = cam["position"][2].get<float>();
}

// Saving
j["camera"]["position"] = {
    settings.camera.position.x,
    settings.camera.position.y,
    settings.camera.position.z
};
```

## Testing

To verify the migration:

1. **Test Loading:**
   - Start the application
   - Check console for "Loaded settings from app_settings.json"
   - Verify all settings are applied correctly

2. **Test Saving:**
   - Change a setting (e.g., grid scale, environment color)
   - Close the application
   - Check that `app_settings.json` was updated
   - Verify the JSON is properly formatted

3. **Test Conversion:**
   - Rename `app_settings.json` to `app_settings.json.backup`
   - Create a test `app_settings.ini` with old format
   - Start the application
   - Verify conversion happened and settings loaded correctly

## Files Modified

1. ✅ `src/io/app_settings.h` - Updated default filename and added conversion method
2. ✅ `src/io/app_settings.cpp` - Complete rewrite for JSON parsing/saving
3. ✅ `src/main.cpp` - Updated to use JSON filename
4. ✅ `Dependencies/include/nlohmann/json.hpp` - Added JSON library

## Notes

- The JSON file is saved with **pretty formatting** (4-space indent) for readability
- All existing functionality remains the same
- Settings are still auto-saved on change and on exit
- No breaking changes to the API

## Future Enhancements

With JSON format, future additions are easier:
- Settings presets
- Export/import settings
- Settings validation
- Nested configuration groups
- Arrays of complex objects
