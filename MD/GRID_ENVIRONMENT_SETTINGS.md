# Grid and Environment Settings Implementation

## Overview

Grid options and Environment Color settings have been added to the `app_settings.ini` file. These settings are automatically loaded on startup and saved when changed or on program exit.

## Settings Added

### Grid Settings (`[Grid]` section)

All grid configuration is now saved to settings:

- **Size**: Total size of the grid (default: 20.0)
- **Spacing**: Distance between grid lines (default: 1.0)
- **Enabled**: Whether grid is visible (default: true)
- **MajorLineColorR/G/B**: Color for major grid lines (default: 0.5, 0.5, 0.5)
- **MinorLineColorR/G/B**: Color for minor grid lines (default: 0.3, 0.3, 0.3)
- **CenterLineColorR/G/B**: Color for center lines (default: 0.8, 0.2, 0.2)

### Environment Settings (`[Environment]` section)

Viewport background color is now saved:

- **BackgroundColorR/G/B/A**: Viewport background color RGBA (default: 0.45, 0.55, 0.60, 1.0)

## Files Modified

### 1. `src/io/app_settings.h`
**Changes:**
- Added `GridSettings` structure with all grid properties
- Added `EnvironmentSettings` structure with background color
- Added these to `AppSettings` struct

### 2. `src/io/app_settings.cpp`
**Changes:**
- Added parsing for `[Grid]` section in `loadSettings()`
- Added parsing for `[Environment]` section in `loadSettings()`
- Added saving for `[Grid]` section in `saveSettings()`
- Added saving for `[Environment]` section in `saveSettings()`

### 3. `src/io/scene.h`
**Changes:**
- Added `setBackgroundColor()` method
- Added `getBackgroundColor()` method

### 4. `src/io/scene.cpp`
**Changes:**
- Grid Options window now saves settings when values change
- Environment Color window now saves settings when color changes
- Grid checkbox in Tools menu saves enabled state

### 5. `src/main.cpp`
**Changes:**
- Grid initialized from settings on startup
- Environment color loaded from settings on startup
- Grid and environment settings saved on program exit

## How It Works

### Loading Settings

On program startup:
1. `app_settings.ini` is loaded
2. Grid is created with settings from file
3. Environment color is applied from settings
4. If file doesn't exist, defaults are used

### Saving Settings

Settings are saved:
1. **On change**: When you adjust grid scale/density or environment color
2. **On exit**: All settings (including grid and environment) are saved when program closes

### Settings File Format

Example `app_settings.ini`:
```ini
[Grid]
Size=20.0
Spacing=1.0
Enabled=true
MajorLineColorR=0.5
MajorLineColorG=0.5
MajorLineColorB=0.5
MinorLineColorR=0.3
MinorLineColorG=0.3
MinorLineColorB=0.3
CenterLineColorR=0.8
CenterLineColorG=0.2
CenterLineColorB=0.2

[Environment]
BackgroundColorR=0.45
BackgroundColorG=0.55
BackgroundColorB=0.60
BackgroundColorA=1.0
```

## User Experience

1. **Adjust grid settings** in Tools → Grid Options
   - Settings are saved immediately
   - Persist across program restarts

2. **Change environment color** in Tools → Environment Color
   - Color is saved immediately
   - Persists across program restarts

3. **Toggle grid** in Tools → Grid
   - Enabled state is saved
   - Persists across program restarts

## Benefits

- ✅ **Persistent settings** - Your preferences are remembered
- ✅ **Automatic saving** - No need to manually save
- ✅ **Works immediately** - Settings applied on startup
- ✅ **Non-intrusive** - Doesn't affect existing functionality

## Future: JSON Migration

See `MD/SETTINGS_JSON_VS_INI.md` for discussion about potentially switching to JSON format, which would provide:
- Cleaner structure (colors as arrays)
- Better extensibility
- Standard format

For now, INI works perfectly fine and requires no external dependencies.
