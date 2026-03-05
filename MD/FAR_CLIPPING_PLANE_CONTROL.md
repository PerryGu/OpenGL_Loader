# Far Clipping Plane Control

## Summary
Added a configurable far clipping plane control to the Viewport Settings window, allowing users to adjust the maximum distance at which objects are visible in the 3D viewport.

## Problem
When moving the camera back and away from the center, objects (characters, grid) would start to disappear due to being clipped by the far plane of the projection matrix. The far clipping plane was hardcoded to 100.0f, which limited the viewing distance.

## Solution
Implemented a user-configurable far clipping plane setting that:
- Can be adjusted via a slider in the Viewport Settings window
- Is saved to and loaded from application settings
- Is applied to both the rendering projection matrix and the raycast projection matrix (for consistent selection behavior)
- Has a default value of 100.0f and can be adjusted from 10.0f to 1000.0f

## Implementation Details

### 1. Settings Structure (`src/io/app_settings.h`)
Added `farClipPlane` to `EnvironmentSettings`:
```cpp
struct EnvironmentSettings {
    glm::vec4 backgroundColor = glm::vec4(0.45f, 0.55f, 0.60f, 1.00f);
    bool boundingBoxesEnabled = true;
    float farClipPlane = 100.0f;  // Maximum clipping distance (far plane)
} environment;
```

### 2. Settings Loading/Saving (`src/io/app_settings.cpp`)
- Added loading of `farClipPlane` from JSON settings file
- Added saving of `farClipPlane` to JSON settings file
- Default value is 100.0f if not present in settings file

### 3. UI Control (`src/io/scene.cpp`)
Added a new "Camera Clipping" section to the Viewport Settings window:
- **Slider**: "Far Clipping Plane" with range 10.0f to 1000.0f
- **Reset Button**: Resets to default value of 100.0f
- **Help Text**: Explains what the far clipping plane does and how to use it

The section appears after the "Environment Color" section in the Viewport Settings window.

### 4. Projection Matrix Updates (`src/main.cpp`)
Updated both projection matrix creation locations to use the configurable far clipping plane:

1. **Main Rendering Loop** (line ~429):
   ```cpp
   AppSettings& settings = AppSettingsManager::getInstance().getSettings();
   float farClipPlane = settings.environment.farClipPlane;
   projection = glm::perspective(glm::radians(cameras[activeCam].getZoom()), aspectRatio, 0.1f, farClipPlane);
   ```

2. **Raycast Selection** (line ~1028):
   ```cpp
   AppSettings& settings = AppSettingsManager::getInstance().getSettings();
   float farClipPlane = settings.environment.farClipPlane;
   glm::mat4 projection = glm::perspective(glm::radians(cameras[activeCam].getZoom()), aspectRatio, 0.1f, farClipPlane);
   ```

This ensures that both rendering and selection use the same far clipping plane, maintaining consistency.

## Usage
1. Open the Viewport Settings window via **Tools → Viewport Settings**
2. Scroll down to the **"Camera Clipping"** section
3. Adjust the **"Far Clipping Plane"** slider to increase or decrease the maximum viewing distance
4. Click **"Reset"** to restore the default value of 100.0f
5. The setting is automatically saved to `app_settings.json` and persists between sessions

## Technical Notes
- The near clipping plane remains fixed at 0.1f (not configurable)
- The far clipping plane affects both rendering and raycast calculations for consistency
- Higher values allow viewing objects further away but may impact depth buffer precision
- Lower values improve depth buffer precision but limit viewing distance
- The default value of 100.0f is a good balance for most use cases

## Files Modified
- `src/io/app_settings.h` - Added `farClipPlane` to `EnvironmentSettings`
- `src/io/app_settings.cpp` - Added loading/saving of `farClipPlane`
- `src/io/scene.cpp` - Added UI control in Viewport Settings window
- `src/main.cpp` - Updated projection matrix creation to use configurable far plane

## Related Features
- Viewport Settings window (Grid Options, Environment Color)
- Application Settings persistence (JSON format)
- Camera projection matrix management
