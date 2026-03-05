# Auto Focus Setting Persistence

## Date
Created: 2026-02-16

## Overview

The Auto Focus setting is now persistent across application sessions. When enabled, Auto Focus automatically triggers camera framing whenever the Gizmo is released after being moved. The setting state is saved to `app_settings.json` and restored on application startup.

## Implementation Details

### 1. Settings Structure

**File:** `src/io/app_settings.h`

Added `autoFocusEnabled` to `EnvironmentSettings` struct:
```cpp
struct EnvironmentSettings {
    glm::vec4 backgroundColor = glm::vec4(0.45f, 0.55f, 0.60f, 1.00f);
    bool boundingBoxesEnabled = true;
    float farClipPlane = 5000.0f;
    float framingDistanceMultiplier = 1.5f;
    bool autoFocusEnabled = false;  // Whether auto-focus is enabled (automatically frame camera when gizmo is released)
} environment;
```

**Default Value:** `false` (disabled by default)

### 2. Settings Persistence

**File:** `src/io/app_settings.cpp`

**Loading:**
```cpp
if (env.contains("autoFocusEnabled")) {
    settings.environment.autoFocusEnabled = env["autoFocusEnabled"].get<bool>();
}
```

**Saving:**
```cpp
j["environment"]["autoFocusEnabled"] = settings.environment.autoFocusEnabled;
```

The setting is automatically saved to `app_settings.json` and persists across sessions.

### 3. UI Integration

**File:** `src/io/scene.cpp` - `renderViewportWindow()` method, Tools menu

**Implementation:**
```cpp
// Auto Focus toggle (automatically frame camera when gizmo is released)
// Get current state from settings and sync to local variable for UI
AppSettings& settings = AppSettingsManager::getInstance().getSettings();
m_autoFocusEnabled = settings.environment.autoFocusEnabled;
if (ImGui::MenuItem("Auto Focus", NULL, &m_autoFocusEnabled)) {
    // Checkbox state changed - update settings and save
    settings.environment.autoFocusEnabled = m_autoFocusEnabled;
    AppSettingsManager::getInstance().markDirty();
}
```

**UI Location:** Viewport window → Tools menu → "Auto Focus" checkbox

**Behavior:**
- Checkbox state is synced from `settings.environment.autoFocusEnabled` every frame
- When checkbox is toggled, the setting is immediately updated and marked for saving
- Settings are auto-saved periodically (every 30 seconds if dirty)

### 4. Initialization

**File:** `src/io/scene.cpp` - `setParameters()` method

**Implementation:**
```cpp
//-- Initialize auto-focus state from persistent settings ----
// This ensures m_autoFocusEnabled is initialized before UI is rendered
AppSettings& settings = AppSettingsManager::getInstance().getSettings();
m_autoFocusEnabled = settings.environment.autoFocusEnabled;
```

The setting is initialized from persistent storage when the Scene is set up, ensuring the UI checkbox reflects the saved state on startup.

### 5. Logic Integration

All Auto Focus logic checks now use the persistent `settings.environment.autoFocusEnabled` value:

**Gizmo Release Detection** (`scene.cpp` line ~1508):
```cpp
// AUTO-FOCUS: If auto-focus is enabled, request camera framing
AppSettings& settings = AppSettingsManager::getInstance().getSettings();
if (settings.environment.autoFocusEnabled && mGizmoPositionValid) {
    m_cameraFramingRequested = true;
}
```

**Framing Distance Slider** (`scene.cpp` line ~577):
```cpp
// On slider release, trigger full re-framing if auto-focus is enabled
// Use persistent settings value
if (sliderJustReleased && settings.environment.autoFocusEnabled && mGizmoPositionValid) {
    m_cameraFramingRequested = true;
}
```

**Framing Distance Reset Button** (`scene.cpp` line ~587):
```cpp
// Reset button triggers full re-framing if auto-focus is enabled
// Use persistent settings value
if (settings.environment.autoFocusEnabled && mGizmoPositionValid) {
    m_cameraFramingRequested = true;
}
```

## Usage

### Enabling Auto Focus

1. Open the Viewport window
2. Click on the "Tools" menu in the viewport menu bar
3. Check the "Auto Focus" checkbox
4. The setting is immediately saved to `app_settings.json`
5. The setting will persist across application restarts

### Disabling Auto Focus

1. Open the Viewport window
2. Click on the "Tools" menu
3. Uncheck the "Auto Focus" checkbox
4. The setting is immediately saved to `app_settings.json`
5. The setting will persist across application restarts

## Settings File Location

The Auto Focus setting is stored in `app_settings.json` in the application's working directory:

```json
{
    "environment": {
        "backgroundColor": [0.45, 0.55, 0.60, 1.00],
        "boundingBoxesEnabled": true,
        "farClipPlane": 5000.0,
        "framingDistanceMultiplier": 1.5,
        "autoFocusEnabled": false
    }
}
```

## Technical Notes

### State Synchronization

- `m_autoFocusEnabled` (Scene member variable) is used for UI display
- `settings.environment.autoFocusEnabled` (persistent setting) is used for all logic checks
- The UI syncs `m_autoFocusEnabled` from settings every frame
- When the checkbox is toggled, both values are updated and settings are marked dirty

### Why Two Variables?

- `m_autoFocusEnabled`: Local UI state variable for ImGui checkbox binding
- `settings.environment.autoFocusEnabled`: Persistent setting that controls actual behavior
- This pattern ensures the UI reflects the saved state while logic uses the authoritative persistent value

### Auto-Save Behavior

Settings are automatically saved:
- When explicitly marked dirty (e.g., when Auto Focus checkbox is toggled)
- Periodically (every 30 seconds) if dirty
- On application shutdown (destructor)

## Related Features

- **Auto Focus Feature**: See `MD/AUTO_FOCUS_FEATURE.md` for details on how Auto Focus works
- **Framing Distance Control**: See `MD/FRAMING_DISTANCE_CONTROL.md` for details on framing distance settings
- **Application Settings**: See `MD/APP_SETTINGS.md` for general settings system documentation

## Summary

The Auto Focus setting is now fully persistent:
- ✅ Saved to `app_settings.json`
- ✅ Loaded on application startup
- ✅ Synced with UI checkbox
- ✅ Used in all logic checks
- ✅ Auto-saved when changed

Users can enable Auto Focus once and it will remain enabled across application sessions until manually disabled.
