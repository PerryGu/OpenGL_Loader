# Version 2.0.0 Release

## Date
Created: 2026-02-23

## Overview
Established a formal versioning system for the OpenGL_Loader application, transitioning to version 2.0.0. The version is now reflected in code constants, window title, and UI elements.

## Version Number Format
The application uses Semantic Versioning (SemVer) format: `MAJOR.MINOR.PATCH`
- **MAJOR**: 2 - Major version increment indicating significant changes
- **MINOR**: 0 - Minor version for feature additions
- **PATCH**: 0 - Patch version for bug fixes

## Implementation

### 1. Version Constant
**Location**: `src/application.h` (Application class)

Added private constant to store the application version:
```cpp
// Application version
const std::string m_version = "2.0.0";
```

**Purpose**: Centralized version storage for use throughout the application.

### 2. Window Title
**Location**: `src/application.cpp` (`Application::init()`)

Updated window title to include version:
```cpp
m_window = glfwCreateWindow(Scene::SCR_WIDTH, Scene::SCR_HEIGHT, 
                           ("OpenGL_Loader v" + m_version).c_str(), NULL, NULL);
```

**Result**: Window title displays as "OpenGL_Loader v2.0.0"

### 3. UI Version Display
**Location**: `src/io/ui/viewport_settings_panel.cpp` (`ViewportSettingsPanel::renderPanel()`)

Added version display at the bottom of the Viewport Settings panel:
```cpp
// Version display
ImGui::Spacing();
ImGui::Separator();
ImGui::TextDisabled("Version: %s", "2.0.0");
```

**Purpose**: Provides users with version information directly in the UI.

## Version Display Locations

1. **Window Title Bar**: "OpenGL_Loader v2.0.0"
2. **Viewport Settings Panel**: Dimmed text at bottom showing "Version: 2.0.0"

## Naming Convention

- **Application Name**: "OpenGL_Loader" (with underscore)
- **Version Format**: "v2.0.0" (lowercase 'v' prefix)
- **Display Format**: "Version: 2.0.0" (in UI panels)

## Future Version Updates

When updating the version number:

1. **Update Constant**: Change `m_version` in `src/application.h`
2. **Update Window Title**: Automatically uses `m_version` constant
3. **Update UI Display**: Update hardcoded string in `viewport_settings_panel.cpp`
   - **Note**: Consider refactoring to use the constant from Application class for consistency

### Version Update Checklist
- [ ] Update `m_version` constant in `Application` class
- [ ] Update UI version display string
- [ ] Update this documentation file
- [ ] Update any release notes or changelog
- [ ] Tag the version in version control (e.g., `git tag v2.0.0`)

## Benefits

1. **Clear Version Identification**: Users can easily identify which version they're running
2. **Centralized Version Management**: Single source of truth for version number
3. **Professional Appearance**: Version display in window title and UI
4. **Debugging Aid**: Version information helps with bug reports and support

## Technical Notes

- Version constant is stored as `std::string` for easy string concatenation
- Window title uses string concatenation: `"OpenGL_Loader v" + m_version`
- UI display uses hardcoded string for now (can be refactored to use constant later)
- Version constant is `const` to prevent accidental modification

## Related Files
- `src/application.h` - Version constant definition
- `src/application.cpp` - Window title with version
- `src/io/ui/viewport_settings_panel.cpp` - UI version display

## Migration Notes

### From Previous Versions
- Previous versions did not have formal versioning
- This is the first official versioned release (v2.0.0)
- No migration required for existing users

## Release Notes (v2.0.0)

### Major Features
- Formal versioning system established
- Version displayed in window title and UI
- Centralized version constant for maintainability

### Improvements
- Professional version identification
- Better user experience with visible version information

### Technical Changes
- Added version constant to `Application` class
- Updated window creation to include version
- Added version display to Viewport Settings panel
