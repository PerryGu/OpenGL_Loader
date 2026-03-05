# Settings Save Behavior

## Overview

This document explains **when** settings are saved to `app_settings.json` in the application.

## Save Triggers

Settings are saved in the following scenarios:

### 1. **On Program Exit** (Primary Save)
**Location:** `src/main.cpp` (line ~507)

When the application closes (user clicks X or program terminates):
- All current settings are collected:
  - Camera position, focus point, yaw, pitch, zoom, orbit distance
  - Window position and size
  - Grid settings (size, spacing, enabled, all colors)
  - Environment background color
- `settingsMgr.saveSettings()` is called explicitly
- This ensures all settings are saved even if the program crashes

**Code:**
```cpp
//-- Save settings on exit (when window is closed by clicking X) ----
appSettings.camera.position = cameras[activeCam].cameraPos;
appSettings.camera.focusPoint = cameras[activeCam].focusPoint;
// ... update all settings ...
settingsMgr.saveSettings(); // Force save on exit
```

### 2. **On Settings Manager Destruction** (Safety Net)
**Location:** `src/io/app_settings.h` (destructor)

When `AppSettingsManager` is destroyed (program termination):
- The destructor automatically calls `saveSettings()`
- This is a safety net in case the explicit save on exit fails
- Only saves if settings are dirty (have been modified)

**Code:**
```cpp
~AppSettingsManager() { saveSettings(); } // Auto-save on destruction
```

### 3. **When Settings Are Marked Dirty** (Deferred Save)
**Location:** Various places in `src/io/scene.cpp`

When you change settings through the UI:
- `markDirty()` is called to mark settings as needing save
- Settings are **NOT saved immediately**
- They are saved later (on exit or destruction)

**Settings that trigger `markDirty()`:**
- Grid scale (size) changed
- Grid density (spacing) changed
- Grid enabled/disabled
- Environment background color changed
- Recent files added (when loading a model)

**Code Example:**
```cpp
// In ShowGridOptionsWindow()
if (ImGui::SliderFloat("Grid Scale", &gridSize, 1.0f, 100.0f)) {
    mGrid->setSize(gridSize);
    AppSettingsManager::getInstance().markDirty(); // Mark for save
}
```

## Save Flow

```
User Changes Setting
    ↓
markDirty() called
    ↓
isDirty = true
    ↓
[Wait...]
    ↓
Program Exit / Destruction
    ↓
saveSettings() called
    ↓
Write to app_settings.json
    ↓
isDirty = false
```

## Important Notes

### ⚠️ Settings Are NOT Saved Immediately

When you change a setting:
- ✅ The change is applied immediately (you see it in the UI)
- ✅ The change is marked as "dirty" (needs saving)
- ❌ The change is **NOT** written to disk immediately
- ✅ The change is saved when the program exits

### Why Deferred Saving?

1. **Performance**: Avoids frequent disk writes during interaction
2. **Atomicity**: All settings saved together ensures consistency
3. **Simplicity**: One save point is easier to manage

### What About Auto-Save?

The `autoSave()` method exists but is **not currently called** in the main loop. It's available for future use if periodic saving is needed.

**Current Implementation:**
```cpp
void AppSettingsManager::autoSave() {
    if (isDirty) {
        saveSettings();
    }
}
```

To enable periodic auto-save, you would call `autoSave()` in the main loop, but this is not currently implemented.

## Summary

| Event | Save Happens? | When |
|-------|--------------|------|
| Change grid scale | ❌ No | Marked dirty, saved on exit |
| Change environment color | ❌ No | Marked dirty, saved on exit |
| Toggle grid | ❌ No | Marked dirty, saved on exit |
| Load a model | ❌ No | Recent file added, saved on exit |
| Close program (X button) | ✅ Yes | Explicit save on exit |
| Program crash | ✅ Maybe | Destructor saves if dirty |

## Recommendations

If you want **immediate saving** when settings change, you can:

1. **Call `saveSettings()` directly** after `markDirty()`:
   ```cpp
   AppSettingsManager::getInstance().markDirty();
   AppSettingsManager::getInstance().saveSettings(); // Immediate save
   ```

2. **Use `autoSave()` in the main loop** (with throttling):
   ```cpp
   // In main loop, every N seconds
   settingsMgr.autoSave();
   ```

Currently, the deferred save approach (save on exit) is used for simplicity and performance.
