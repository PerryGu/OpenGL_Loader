# Framing Distance Control Feature

## Date
Created: 2026-02-16

## Overview

The Framing Distance Control feature allows users to adjust how tightly the camera frames characters when using the 'F' key or Auto Focus feature. This provides fine-grained control over the framing tightness, from very tight (1.0) to very loose (10.0).

## Feature Description

The Framing Distance slider:
- Controls the multiplier applied to the calculated framing distance
- Range: 1.0 to 10.0 (default: 1.5)
- Lower values (1.0-2.0) = tighter framing (camera closer to character)
- Higher values (5.0-10.0) = looser framing (camera farther from character)
- Automatically triggers re-framing when changed if Auto Focus is enabled
- Persists across sessions via application settings

## Implementation Details

### 1. Settings Structure

**File:** `src/io/app_settings.h`

Added `framingDistanceMultiplier` to `EnvironmentSettings`:
```cpp
struct EnvironmentSettings {
    glm::vec4 backgroundColor = glm::vec4(0.45f, 0.55f, 0.60f, 1.00f);
    bool boundingBoxesEnabled = true;
    float farClipPlane = 5000.0f;
    float framingDistanceMultiplier = 1.5f;  // Multiplier for camera framing distance (1.0 = tight, 10.0 = loose)
} environment;
```

**Default Value:** 1.5f (replaces the previous hardcoded 1.15f multiplier)

### 2. Settings Persistence

**File:** `src/io/app_settings.cpp`

**Loading:**
```cpp
if (env.contains("framingDistanceMultiplier")) {
    settings.environment.framingDistanceMultiplier = env["framingDistanceMultiplier"].get<float>();
}
```

**Saving:**
```cpp
j["environment"]["framingDistanceMultiplier"] = settings.environment.framingDistanceMultiplier;
```

The setting is automatically saved to `app_settings.json` and persists across sessions.

### 3. UI Control

**File:** `src/io/scene.cpp` - `ShowViewportSettingsWindow()` method

Added a new "Camera Framing" section to the Viewport Settings window:
```cpp
// ========== CAMERA FRAMING SECTION ==========
ImGui::Spacing();
ImGui::Separator();
ImGui::Text("Camera Framing");
ImGui::Separator();

// Framing Distance control
float framingDistance = settings.environment.framingDistanceMultiplier;
bool framingDistanceChanged = false;
if (ImGui::SliderFloat("Framing Distance", &framingDistance, 1.0f, 10.0f, "%.2f")) {
    settings.environment.framingDistanceMultiplier = framingDistance;
    AppSettingsManager::getInstance().markDirty();
    framingDistanceChanged = true;
    // If auto-focus is enabled and a character is selected, trigger re-framing
    if (m_autoFocusEnabled && mGizmoPositionValid) {
        m_cameraFramingRequested = true;
        std::cout << "[Framing Distance] Slider changed - requesting camera re-framing (auto-focus active)" << std::endl;
    }
}
ImGui::SameLine();
if (ImGui::Button("Reset##FramingDistance")) {
    framingDistance = 1.5f;  // Reset to default
    settings.environment.framingDistanceMultiplier = 1.5f;
    AppSettingsManager::getInstance().markDirty();
    framingDistanceChanged = true;
    // If auto-focus is enabled and a character is selected, trigger re-framing
    if (m_autoFocusEnabled && mGizmoPositionValid) {
        m_cameraFramingRequested = true;
        std::cout << "[Framing Distance] Reset - requesting camera re-framing (auto-focus active)" << std::endl;
    }
}
```

**UI Location:** Viewport Settings window → Camera Framing section → "Framing Distance" slider

### 4. Camera Function Modification

**File:** `src/io/camera.h` and `src/io/camera.cpp`

**Function Signature Update:**
```cpp
// Before:
bool aimAtTargetWithFraming(glm::vec3 targetPos, glm::vec3 boundingBoxMin, glm::vec3 boundingBoxMax, 
                            glm::vec3 modelScale = glm::vec3(1.0f), float aspectRatio = 16.0f / 9.0f);

// After:
bool aimAtTargetWithFraming(glm::vec3 targetPos, glm::vec3 boundingBoxMin, glm::vec3 boundingBoxMax, 
                            glm::vec3 modelScale = glm::vec3(1.0f), float aspectRatio = 16.0f / 9.0f, 
                            float framingDistanceMultiplier = 1.5f);
```

**Implementation Change:**
```cpp
// Before:
float finalDist = dist * 1.15f;  // Hardcoded 15% padding

// After:
float finalDist = dist * framingDistanceMultiplier;  // User-configurable multiplier
```

### 5. Integration with Framing System

**File:** `src/io/scene.cpp` - `applyCameraAimConstraint()` method

Updated to pass the multiplier from settings:
```cpp
if (hasBoundingBox) {
    // Get framing distance multiplier from settings
    AppSettings& settings = AppSettingsManager::getInstance().getSettings();
    float framingMultiplier = settings.environment.framingDistanceMultiplier;
    aimComplete = camera->aimAtTargetWithFraming(mGizmoWorldPosition, boundingBoxMin, boundingBoxMax, 
                                                 modelScale, aspectRatio, framingMultiplier);
}
```

### 6. Auto-Reframe on Slider Change

When the Framing Distance slider is changed:
1. The setting is immediately updated and marked as dirty (will be saved)
2. If Auto Focus is enabled (`m_autoFocusEnabled == true`) and a character is selected (`mGizmoPositionValid == true`):
   - The `m_cameraFramingRequested` flag is set to `true`
   - This triggers automatic re-framing in the next frame
   - The camera immediately adjusts to the new framing distance

**Behavior:**
- **With Auto Focus enabled:** Camera re-frames immediately when slider changes
- **Without Auto Focus:** Setting is saved, but camera doesn't re-frame until next 'F' key press or gizmo release

## Usage

### Adjusting Framing Distance

1. Open the Viewport Settings window (Viewport → Tools → Viewport Settings)
2. Scroll to the "Camera Framing" section
3. Adjust the "Framing Distance" slider:
   - **1.0-2.0**: Tight framing (camera close to character)
   - **2.0-4.0**: Medium framing (balanced)
   - **4.0-10.0**: Loose framing (camera far from character)
4. Click "Reset" to restore default value (1.5)

### With Auto Focus

1. Enable Auto Focus (Viewport → Tools → Auto Focus)
2. Select a character
3. Adjust the Framing Distance slider
4. **Camera automatically re-frames** to the new distance

### Without Auto Focus

1. Adjust the Framing Distance slider
2. Press 'F' key or move gizmo and release to apply new framing distance

## Technical Details

### Framing Distance Calculation

The framing distance is calculated as:
```
finalDist = baseDistance * framingDistanceMultiplier
```

Where:
- `baseDistance` = Calculated distance based on bounding box size and FOV
- `framingDistanceMultiplier` = User-configurable value (1.0 to 10.0, default 1.5)

### Default Value Rationale

The default value of **1.5** was chosen to:
- Provide slightly tighter framing than the previous hardcoded 1.15f
- Give a good balance between showing the character clearly and providing context
- Match common industry standards for character framing

### Range Rationale

- **Minimum (1.0)**: Very tight framing, character fills most of the viewport
- **Maximum (10.0)**: Very loose framing, character is small in the viewport with lots of context
- **Default (1.5)**: Balanced framing, character is clearly visible with some context

## Integration with Existing Systems

### Camera Aim Constraint

The Framing Distance Control integrates seamlessly with:
- **Manual 'F' key framing**: Uses the current slider value
- **Auto Focus feature**: Automatically re-frames when slider changes
- **Camera aim constraint system**: All framing uses the configurable multiplier

### Settings Persistence

The framing distance multiplier:
- Is saved to `app_settings.json`
- Persists across application sessions
- Is loaded on application startup
- Can be reset to default (1.5) via the Reset button

## Code Organization

### Files Modified

1. **`src/io/app_settings.h`**
   - Added `framingDistanceMultiplier` to `EnvironmentSettings`

2. **`src/io/app_settings.cpp`**
   - Added loading of `framingDistanceMultiplier` from JSON
   - Added saving of `framingDistanceMultiplier` to JSON

3. **`src/io/scene.cpp`**
   - Added "Camera Framing" section to Viewport Settings window
   - Added Framing Distance slider with auto-reframe logic
   - Updated `applyCameraAimConstraint()` to pass multiplier from settings

4. **`src/io/camera.h`**
   - Updated `aimAtTargetWithFraming()` signature to accept `framingDistanceMultiplier` parameter

5. **`src/io/camera.cpp`**
   - Replaced hardcoded `1.15f` multiplier with parameter value

### Code Principles Followed

- **Modularity:** Feature is self-contained with clear separation of concerns
- **Reusability:** Reuses existing camera framing logic
- **Action-Based Debug Prints:** Prints only on slider change, not every frame
- **Settings Persistence:** Integrated with existing settings system
- **Documentation:** Comprehensive inline comments and markdown documentation

## Debug Output

The feature includes action-based debug prints (following project guidelines):

**When Slider Changes (Auto Focus Active):**
```
[Framing Distance] Slider changed - requesting camera re-framing (auto-focus active)
```

**When Reset Button Clicked (Auto Focus Active):**
```
[Framing Distance] Reset - requesting camera re-framing (auto-focus active)
```

**Note:** These prints only occur on specific actions (slider change/reset), not every frame.

## Related Features

- **Auto Focus Feature:** `MD/AUTO_FOCUS_FEATURE.md`
- **Camera Aim Constraint:** `MD/CAMERA_AIM_CONSTRAINT_2026-02-16_14-30.md`
- **Viewport Settings Window:** `MD/FAR_CLIPPING_PLANE_CONTROL.md`
- **Application Settings:** `MD/APP_SETTINGS.md`

## Future Enhancements

Potential improvements:
1. **Per-Character Settings:** Allow different framing distances for different characters
2. **Animation During Framing:** Smooth interpolation when framing distance changes
3. **Preset Values:** Quick buttons for common framing distances (tight, medium, loose)
4. **Visual Preview:** Show framing distance preview before applying
5. **Keyboard Shortcuts:** Hotkeys to quickly adjust framing distance

## Summary

The Framing Distance Control feature provides users with fine-grained control over camera framing tightness. It integrates seamlessly with the existing camera framing system, supports automatic re-framing when Auto Focus is enabled, and persists settings across sessions. The implementation follows project guidelines for modularity, documentation, and action-based debug output.
