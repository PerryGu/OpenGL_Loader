# Application Settings System

## Overview
This document describes the application settings system that saves and restores camera state and recent files.

## Implementation Details

### File Structure
- **Settings File**: `app_settings.ini` (created in the application root directory)
- **Format**: INI (simple key-value format)
- **Why INI?**: 
  - No external dependencies
  - Human-readable and editable
  - Perfect for simple key-value data
  - Lightweight and fast

### What Gets Saved

#### 1. Camera State
- Position (X, Y, Z)
- Focus Point (X, Y, Z) - the point the camera orbits around
- Yaw (horizontal rotation)
- Pitch (vertical rotation)
- Zoom (FOV)
- Orbit Distance (distance from camera to focus point)

#### 2. Recent Files
- Up to 3 most recently opened files
- Stored as full file paths
- Automatically updated when files are loaded
- Displayed in the File > Import menu

### Code Structure

#### Files Created
- `src/io/app_settings.h` - Settings manager header
- `src/io/app_settings.cpp` - Settings manager implementation

#### Key Classes
- `AppSettings` - Structure holding all settings data
- `AppSettingsManager` - Singleton manager for loading/saving settings

### Integration Points

#### 1. Application Startup (`src/main.cpp`)
```cpp
// Load settings on startup
AppSettingsManager& settingsMgr = AppSettingsManager::getInstance();
settingsMgr.loadSettings("app_settings.ini");

// Restore camera state
AppSettings& settings = settingsMgr.getSettings();
cameras[activeCam].focusPoint = settings.camera.focusPoint;
cameras[activeCam].yaw = settings.camera.yaw;
// ... restore other camera properties
cameras[activeCam].restoreCameraState();
```

#### 2. Camera State Saving (`src/main.cpp`)
- Camera state is saved every 2 seconds automatically
- Also saved on application exit

#### 3. Recent Files Menu (`src/io/scene.cpp`)
- Recent files appear under File > Import menu
- Clicking a recent file loads it immediately
- Files are numbered (1., 2., 3.) for clarity

#### 4. File Loading (`src/main.cpp`)
- When a file is successfully loaded, it's added to recent files
- Recent files list is automatically saved

### Settings File Format

```ini
; OpenGL_loader Application Settings
; This file stores camera state and recent files

[Camera]
PositionX=0.0
PositionY=9.0
PositionZ=30.0
FocusPointX=0.0
FocusPointY=0.0
FocusPointZ=0.0
Yaw=-90.0
Pitch=0.0
Zoom=45.0
OrbitDistance=30.0

[RecentFiles]
File0="C:/path/to/file1.fbx"
File1="C:/path/to/file2.obj"
File2="C:/path/to/file3.fbx"
```

### API Usage

#### Loading Settings
```cpp
AppSettingsManager& mgr = AppSettingsManager::getInstance();
mgr.loadSettings("app_settings.ini");
```

#### Saving Settings
```cpp
AppSettingsManager& mgr = AppSettingsManager::getInstance();
mgr.saveSettings();  // Uses default filename
// or
mgr.saveSettings("custom_settings.ini");
```

#### Adding Recent File
```cpp
AppSettingsManager::getInstance().addRecentFile("path/to/file.fbx");
```

#### Getting Recent Files
```cpp
const std::vector<std::string>& recentFiles = 
    AppSettingsManager::getInstance().getRecentFiles();
```

### Camera State Restoration

The `Camera::restoreCameraState()` method recalculates the camera position based on:
- Focus point
- Yaw and pitch angles
- Orbit distance

This ensures the camera is positioned correctly after loading settings.

### Recent Files Behavior

1. **Adding Files**: When a file is successfully loaded, it's automatically added to recent files
2. **Ordering**: Most recent file appears first
3. **Duplicates**: If a file is already in the list, it's moved to the top
4. **Limit**: Maximum of 6 files are kept (MAX_RECENT_FILES)
5. **Menu Display**: Only the filename (not full path) is shown in the menu

### Path Sanitization and Cross-Platform Compatibility

The Recent Files feature implements professional path canonicalization to prevent path corruption:

1. **Path Sanitization**: All file paths are sanitized using `std::filesystem::absolute()` and `std::filesystem::canonical()`:
   - Converts relative paths to absolute paths
   - Resolves ".." and redundant directory separators
   - Removes build artifacts (e.g., "out\build" directories) from paths
   - Ensures consistent path storage across platforms

2. **File Existence Validation**: 
   - Files are verified to exist before being added to the recent files list
   - When loading from recent files menu, files are verified to still exist before attempting to load
   - Non-existent files are logged as warnings and skipped

3. **Implementation Locations**:
   - **File Selection** (`src/io/ui/ui_manager.cpp`): Paths are sanitized when selected from the file dialog
   - **Recent Files Addition** (`src/io/app_settings.cpp::addRecentFile()`): Paths are sanitized and validated before being added to the list
   - **Recent Files Loading** (`src/io/ui/ui_manager.cpp`): Files are verified to exist before loading from the menu

4. **Error Handling**: All filesystem operations are wrapped in try-catch blocks to handle cross-platform path differences gracefully

## Robustness Enhancements (v2.0.0)

### Settings Loading Robustness
**Location**: `src/io/app_settings.cpp` (`AppSettingsManager::loadSettings()`)

#### Missing Key Handling
- **Fallback to Defaults**: All JSON keys are checked with `.contains()` before reading
- **Graceful Degradation**: Missing keys fall back to hardcoded defaults in `app_settings.h`
- **No Crashes**: Invalid or missing JSON data doesn't cause application crashes

**Implementation**:
```cpp
// Robust key checking with fallback to defaults
if (env.contains("farClipPlane")) {
    float loadedFarClipPlane = env["farClipPlane"].get<float>();
    // Additional validation...
} else {
    // Use default from app_settings.h (5000.0f)
}
```

#### Migration Logic
- **Invalid Value Detection**: Checks for invalid values (e.g., `farClipPlane <= 0.0f`)
- **Automatic Correction**: Resets invalid values to defaults with logging
- **Backward Compatibility**: Handles legacy settings files gracefully

**Example**:
```cpp
if (env.contains("farClipPlane")) {
    float loadedFarClipPlane = env["farClipPlane"].get<float>();
    // Migration: Reset invalid farClipPlane values (0 or negative) to default
    if (loadedFarClipPlane <= 0.0f) {
        settings.environment.farClipPlane = 5000.0f;  // Default value
        LOG_INFO("[Settings] Migrated invalid farClipPlane to default (5000.0)");
    } else {
        settings.environment.farClipPlane = loadedFarClipPlane;
    }
}
```

### Recent Files Robustness
**Location**: `src/io/app_settings.cpp` (`AppSettingsManager::addRecentFile()`)

#### File Existence Validation
- **Pre-Add Check**: Verifies file exists before adding to recent files list
- **Post-Load Check**: When loading from recent files menu, verifies file still exists
- **Automatic Cleanup**: Non-existent files are logged as warnings and skipped

**Implementation**:
```cpp
// Verify file exists before adding to recent files
if (!std::filesystem::exists(filePath)) {
    LOG_ERROR("[Settings] Cannot add non-existent file to recent files: " + filepath);
    return;
}
```

### Auto-Save Throttling
**Location**: `src/io/app_settings.cpp` (`AppSettingsManager::autoSave()`)

#### Time-Based Throttling
- **60-Second Interval**: Auto-save only occurs once every 60 seconds if settings are dirty
- **Performance Optimization**: Prevents excessive disk writes during rapid settings changes
- **Timer-Based**: Uses `std::chrono::steady_clock` for accurate time tracking

**Implementation**:
```cpp
void AppSettingsManager::autoSave() {
    if (!isDirty) {
        return; // No changes to save
    }
    
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastSave = std::chrono::duration_cast<std::chrono::duration<double>>(
        now - lastSaveTime).count();
    
    // Only save if enough time has passed since the last save
    if (timeSinceLastSave >= AUTO_SAVE_INTERVAL_SECONDS) {
        saveSettings();
        lastSaveTime = now;
    }
}
```

**Constants**:
- `AUTO_SAVE_INTERVAL_SECONDS = 60.0` (defined in `app_settings.h`)

### Last Import Directory Persistence
**Location**: `src/io/app_settings.h` (`EnvironmentSettings::lastImportDirectory`)

#### Directory Memory
- **Persistent Storage**: Last import directory is saved to `app_settings.json`
- **Auto-Restore**: File dialog opens in last used directory on next import
- **Cross-Session**: Persists across application restarts

**Implementation**:
```cpp
// Save directory when file is selected
std::string dir = std::filesystem::path(selectedFile).parent_path().string();
settings.environment.lastImportDirectory = dir;

// Restore directory when opening file dialog
std::string lastDir = settings.environment.lastImportDirectory;
if (!lastDir.empty() && std::filesystem::exists(lastDir)) {
    mFileDialog.SetPwd(lastDir);
}
```

## Future Enhancements

Potential additions to the settings system:
- Window size and position (if not using ImGui docking)
- Render settings (wireframe, lighting, etc.)
- Animation playback preferences
- Export/import settings profiles

## Troubleshooting

### Settings Not Saving
- Check file permissions in the application directory
- Verify the application has write access

### Camera State Not Restoring
- Ensure `restoreCameraState()` is called after setting camera properties
- Check that focus point is set before restoring state

### Recent Files Not Appearing
- Verify files are being added via `addRecentFile()` after successful load
- Check that settings are being saved (auto-save every 2 seconds)
