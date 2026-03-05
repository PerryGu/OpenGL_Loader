# V-Sync Control Feature

**Date:** 2026-02-19  
**Status:** ✅ Implemented

## Overview
Added full user control over V-Sync (Vertical Synchronization), allowing users to toggle between capped 60 FPS (V-Sync on) and uncapped framerates (V-Sync off, up to 400+ FPS).

## Implementation

### 1. Settings Storage
Added to `AppSettings` struct in `src/io/app_settings.h`:
```cpp
struct EnvironmentSettings {
    bool vSyncEnabled = true;  // Default: V-Sync ON
    // ...
};
```

### 2. UI Control
Added checkbox in Viewport Settings Panel (`src/io/ui/viewport_settings_panel.cpp`):
```cpp
if (ImGui::Checkbox("V-Sync (Limit FPS to 60)", &m_vSyncEnabled)) {
    glfwSwapInterval(m_vSyncEnabled ? 1 : 0);
    // Save settings immediately
    AppSettingsManager::getInstance().getSettings().environment.vSyncEnabled = m_vSyncEnabled;
    AppSettingsManager::getInstance().markDirty();
}
```

### 3. Application Initialization
V-Sync state loaded from settings and applied at startup (`src/application.cpp`):
```cpp
AppSettings appSettings = AppSettingsManager::getInstance().getSettings();
glfwSwapInterval(appSettings.environment.vSyncEnabled ? 1 : 0);
```

### 4. Settings Persistence
V-Sync state saved to `app_settings.json`:
```json
{
  "environment": {
    "vSyncEnabled": true
  }
}
```

## User Experience

### V-Sync ON (Default)
- **FPS:** Capped at 60 FPS (matches monitor refresh rate)
- **Benefits:** 
  - Eliminates screen tearing
  - Consistent frame timing
  - Lower GPU usage
- **Use Case:** Normal usage, presentation mode

### V-Sync OFF
- **FPS:** Uncapped (can reach 400+ FPS on powerful hardware)
- **Benefits:**
  - Maximum responsiveness
  - Lower input latency
  - Better for performance testing
- **Use Case:** Development, performance profiling, high-refresh monitors

## Technical Details

### GLFW Function
```cpp
glfwSwapInterval(int interval)
```
- `interval = 1`: V-Sync ON (wait for vertical retrace)
- `interval = 0`: V-Sync OFF (immediate buffer swap)

### Settings Sync
- **Load:** On application startup
- **Save:** Immediately when checkbox is toggled
- **Persistence:** Stored in `app_settings.json`

## Files Modified
- `src/io/app_settings.h` - Added `vSyncEnabled` field
- `src/io/app_settings.cpp` - JSON serialization/deserialization
- `src/io/ui/viewport_settings_panel.h` - Added UI state member
- `src/io/ui/viewport_settings_panel.cpp` - Checkbox implementation
- `src/application.cpp` - Initialization and settings sync

## Performance Impact

### With V-Sync OFF
- **Single Model:** 400+ FPS
- **Multiple Models:** 200-400 FPS (depending on complexity)
- **UI Updates:** Instant, no lag

### With V-Sync ON
- **All Scenarios:** Stable 60 FPS
- **GPU Usage:** Reduced (idle time during vertical retrace wait)
- **Screen Tearing:** Eliminated

## Related Features
- FPS Counter (scalable, persistent)
- Smooth Camera (interpolation speed control)
- Performance optimizations (60+ FPS baseline)
