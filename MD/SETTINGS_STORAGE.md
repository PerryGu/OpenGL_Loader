# Settings Storage Options

## Overview
This document explains the options for storing application settings and data beyond ImGui window positions.

## Current State
- **Window positions/sizes**: Stored in `imgui.ini` (managed by ImGui automatically)
- **Application settings**: Currently not persisted

## Option 1: Separate Settings File (Recommended) ✅

### Why Separate File?
1. **Separation of Concerns**: UI layout vs application data
2. **Safety**: Corrupting one doesn't affect the other
3. **Flexibility**: Choose format (JSON, XML, binary), versioning, encryption
4. **Maintainability**: Easier to read, edit, debug
5. **Performance**: Can load only what you need

### Implementation
A basic settings system has been created:
- `src/io/settings.h` - Settings structure and manager
- `src/io/settings.cpp` - Load/save implementation

### Usage Example
```cpp
// In main.cpp or wherever you need settings
#include "io/settings.h"

// Get settings manager
SettingsManager& settingsMgr = SettingsManager::getInstance();

// Load settings on startup
settingsMgr.loadSettings("app_settings.json");

// Access settings
AppSettings& settings = settingsMgr.getSettings();
cameras[activeCam].sensitivity = settings.cameraSensitivity;

// Modify settings
settings.cameraSensitivity = 2.0f;
settingsMgr.markDirty(); // Mark for saving

// Auto-save periodically (in main loop)
settingsMgr.autoSave();
```

### File Format Options

#### JSON (Recommended)
- **File**: `app_settings.json`
- **Pros**: Human-readable, widely supported, easy to parse
- **Cons**: Slightly larger file size
- **Libraries**: nlohmann/json, rapidjson, or simple custom parser

#### XML
- **File**: `app_settings.xml`
- **Pros**: Structured, supports comments
- **Cons**: Verbose, harder to parse manually

#### Binary
- **File**: `app_settings.bin`
- **Pros**: Fast, compact
- **Cons**: Not human-readable, version-dependent

#### Custom INI
- **File**: `app_settings.ini`
- **Pros**: Simple, similar to imgui.ini
- **Cons**: Less structured than JSON

## Option 2: Extend imgui.ini (Alternative)

### Using ImGui Settings Handlers
ImGui supports custom settings handlers to add your own data to `imgui.ini`.

### Implementation Example
```cpp
// In Scene::setParameters() or similar
ImGuiSettingsHandler handler;
handler.TypeName = "AppSettings";
handler.TypeHash = ImHashStr("AppSettings");
handler.ReadOpenFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name) -> void* {
    // Return pointer to your settings struct
    return &appSettings;
};
handler.ReadLineFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line) {
    AppSettings* settings = (AppSettings*)entry;
    // Parse line and update settings
    // Example: "cameraSensitivity=2.0"
};
handler.WriteAllFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf) {
    AppSettings* settings = &appSettings;
    buf->appendf("[%s][Data]\n", handler->TypeName);
    buf->appendf("cameraSensitivity=%f\n", settings->cameraSensitivity);
    buf->appendf("cameraSpeed=%f\n", settings->cameraSpeed);
    // etc...
};
ImGui::AddSettingsHandler(&handler);
```

### Pros of Using imgui.ini
- ✅ Everything in one file
- ✅ Automatic save/load with ImGui
- ✅ No additional file management

### Cons of Using imgui.ini
- ❌ Mixing UI layout with application data
- ❌ Risk: Corrupting settings could break UI layout
- ❌ Less flexible (tied to ImGui's save/load cycle)
- ❌ Harder to version or migrate settings
- ❌ Users might accidentally delete when resetting UI

## Recommendation

**Use Option 1 (Separate File)** for:
- Camera settings
- Application preferences
- Recent files list
- User preferences
- Any data that's not directly UI-related

**Keep in imgui.ini** for:
- Window positions/sizes (already handled by ImGui)
- Docking layouts (already handled by ImGui)
- UI-specific state

## File Structure

```
OpenGL_loader/
├── imgui.ini              # UI layout (managed by ImGui)
├── app_settings.json      # Application settings (your code)
└── ...
```

## Next Steps

1. **Choose format**: JSON recommended for readability
2. **Add JSON library** (optional): Use nlohmann/json for proper JSON support
3. **Integrate settings manager**: Load on startup, save on changes
4. **Add settings UI**: Create ImGui panel to edit settings
5. **Version settings**: Add version field for future migrations

## Example: Using nlohmann/json (Better Implementation)

If you want a proper JSON implementation:

```cpp
#include <nlohmann/json.hpp>
using json = nlohmann::json;

bool SettingsManager::loadSettings(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;
    
    json j;
    file >> j;
    
    settings.cameraSensitivity = j.value("cameraSensitivity", 1.0f);
    settings.cameraSpeed = j.value("cameraSpeed", 2.5f);
    // etc...
    
    return true;
}

bool SettingsManager::saveSettings(const std::string& filename) {
    json j;
    j["cameraSensitivity"] = settings.cameraSensitivity;
    j["cameraSpeed"] = settings.cameraSpeed;
    // etc...
    
    std::ofstream file(filename);
    file << j.dump(2); // Pretty print with 2-space indent
    return true;
}
```

## Summary

**Best Practice**: Use a separate `app_settings.json` file for application data. This keeps concerns separated, makes the code more maintainable, and prevents UI layout issues from affecting application settings.
