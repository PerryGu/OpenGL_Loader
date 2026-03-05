# Skeleton Line Width Feature

## Date
Created: 2026-02-23

## Overview
Added a user-configurable slider in the Viewport Settings panel to control the line width (thickness) of rendered skeleton bones and the size of skeleton joint spheres. This allows users to adjust skeleton visibility to their preference, making bones and joints more or less prominent in the viewport. The setting uniformly controls both bone line thickness and joint sphere size for consistent visual scaling.

## Features Implemented

### 1. Settings Storage
**Location**: `src/io/app_settings.h`

Added new field to `EnvironmentSettings` struct:
```cpp
float skeletonLineWidth = 2.0f;  // Line width (thickness) for rendered skeleton bones and joint sphere size (1.0 to 10.0)
```

**Default Value**: `2.0f` (matches previous hardcoded values: line width 2.0, point size 12.0)

**Range**: `1.0f` to `10.0f` (configurable via UI slider)

**Dual Control**: This single setting controls both:
- Bone line thickness (via `glLineWidth`)
- Joint sphere size (via `gl_PointSize` uniform in vertex shader)

### 2. Settings Persistence
**Location**: `src/io/app_settings.cpp`

**Serialization** (save to JSON):
- Added `j["environment"]["skeletonLineWidth"] = settings.environment.skeletonLineWidth;`
- Saves user preference to `app_settings.json`

**Deserialization** (load from JSON):
- Added check for `"skeletonLineWidth"` key in environment settings
- Loads saved value on application startup
- Falls back to default (2.0f) if not found

### 3. UI Control
**Location**: `src/io/ui/viewport_settings_panel.cpp`

Added new "Skeleton Settings" section with:
- **Slider**: `ImGui::SliderFloat("Skeleton Line Width", &skeletonLineWidth, 1.0f, 10.0f, "%.1f")`
- **Reset Button**: Resets to default value (2.0f)
- **Help Text**: Explains what the setting does
- **Auto-save**: Calls `AppSettingsManager::getInstance().markDirty()` on change

**Note**: This single setting controls both:
- Bone line thickness (`glLineWidth`)
- Joint sphere size (`gl_PointSize` via shader uniform)

**UI Layout**:
```
┌─────────────────────────────────────┐
│ Viewport Settings                   │
├─────────────────────────────────────┤
│ ... (other settings) ...            │
├─────────────────────────────────────┤
│ Skeleton Settings                   │
├─────────────────────────────────────┤
│ Skeleton Line Width: [====●====] 2.0│ [Reset]
│                                     │
│ Skeleton Line Width: Controls the   │
│ thickness of skeleton bone lines    │
│ when skeletons are visible.          │
└─────────────────────────────────────┘
```

### 4. Rendering Implementation
**Location**: `src/graphics/model.cpp` (`Model::DrawSkeleton`)

**Bone Lines**:
- **Before**: Hardcoded `glLineWidth(2.0f);`
- **After**: 
  ```cpp
  // Set line width from settings (user-configurable skeleton line thickness)
  float lineWidth = AppSettingsManager::getInstance().getSettings().environment.skeletonLineWidth;
  glLineWidth(lineWidth);
  
  // Draw lines
  glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lineVertices.size()));
  
  // Reset line width to default to avoid affecting other render passes
  glLineWidth(1.0f);
  ```

**Joint Spheres**:
- **Before**: Hardcoded `gl_PointSize = 12.0;` in vertex shader
- **After**: 
  ```cpp
  // Calculate point size proportional to skeleton line width setting
  // This links the joint sphere size to the bone line width for consistent visual scaling
  float lineWidth = AppSettingsManager::getInstance().getSettings().environment.skeletonLineWidth;
  // Base size of 8.0 plus a proportional increase based on the line width slider
  float calculatedPointSize = 8.0f + (lineWidth * 2.0f);
  shader.setFloat("pointSize", calculatedPointSize);
  ```

**Formula**: `pointSize = 8.0 + (lineWidth * 2.0)`
- When `lineWidth = 1.0`: `pointSize = 10.0`
- When `lineWidth = 2.0`: `pointSize = 12.0` (default)
- When `lineWidth = 10.0`: `pointSize = 28.0`

**Important**: 
- Line width is reset to `1.0f` after drawing to prevent affecting other render passes
- Point size is controlled via shader uniform, so no OpenGL state reset needed

## Technical Details

### OpenGL Line Width
- Uses `glLineWidth(float width)` to set line thickness
- Line width is measured in pixels
- Range: 1.0 to 10.0 (OpenGL implementation-dependent, but typically supports this range)

### OpenGL Profile Requirement
**IMPORTANT**: This feature requires **OpenGL Compatibility Profile** instead of Core Profile.

**Why Compatibility Profile?**
- OpenGL Core Profile (3.3+) deprecates `glLineWidth()` and clamps all values > 1.0f to 1.0 on modern drivers
- Compatibility Profile maintains legacy `glLineWidth()` functionality, allowing variable line widths
- The application is configured to use Compatibility Profile in `application.cpp`:
  ```cpp
  // Use Compatibility Profile to support glLineWidth() for skeleton line width feature
  // Core Profile deprecates glLineWidth() and clamps all values > 1.0f to 1.0 on modern drivers
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
  ```

**Alternative Solution** (if Compatibility Profile causes issues):
- If switching to Compatibility Profile breaks other modern shaders, the alternative is to implement a Geometry Shader that expands skeleton lines into screen-facing quads
- This would allow variable line widths in Core Profile, but requires more complex shader code

### Settings Flow
1. **User adjusts slider** → `viewport_settings_panel.cpp`
2. **Value stored** → `AppSettings.environment.skeletonLineWidth`
3. **Marked dirty** → `AppSettingsManager::markDirty()`
4. **Auto-saved** → Written to `app_settings.json` (within 30 seconds)
5. **Applied during render** → `Model::DrawSkeleton()` reads setting and applies via `glLineWidth()`

### State Management
- Line width is set **before** `glDrawArrays(GL_LINES, ...)`
- Line width is reset **after** drawing to prevent state leakage
- This ensures other line-based rendering (grid, bounding boxes) is not affected

## Files Modified

1. **`src/io/app_settings.h`**
   - Added `skeletonLineWidth` field to `EnvironmentSettings` struct

2. **`src/io/app_settings.cpp`**
   - Added JSON serialization for `skeletonLineWidth`
   - Added JSON deserialization for `skeletonLineWidth`

3. **`src/io/ui/viewport_settings_panel.cpp`**
   - Added "Skeleton Settings" section
   - Added slider control for skeleton line width
   - Added reset button
   - Added help text

4. **`src/graphics/model.cpp`**
   - Added include for `app_settings.h`
   - Replaced hardcoded `glLineWidth(2.0f)` with setting-based value
   - Added line width reset after drawing

## Usage

### Adjusting Skeleton Line Width
1. Open **Viewport Settings** panel (`View > Viewport Settings` or similar)
2. Scroll to **"Skeleton Settings"** section
3. Adjust **"Skeleton Line Width"** slider (1.0 to 10.0)
4. Setting is automatically saved to `app_settings.json`

### Resetting to Default
1. Click **"Reset"** button next to the slider
2. Line width resets to `2.0f` (default)
3. Setting is automatically saved

### Persistence
- Setting is saved to `app_settings.json` in the application directory
- Value is loaded on application startup
- Changes are auto-saved within 30 seconds (or on application exit)

## Benefits

1. **User Customization**: Users can adjust skeleton visibility to their preference
2. **Unified Control**: Single slider controls both bone thickness and joint size for consistent scaling
3. **Better Visibility**: Thicker lines and larger joints make skeletons more visible in complex scenes
4. **Reduced Clutter**: Thinner lines and smaller joints reduce visual clutter when needed
5. **Persistent Settings**: User preference is saved between sessions
6. **Non-Intrusive**: Line width reset prevents affecting other rendering
7. **Proportional Scaling**: Joint size scales proportionally with bone thickness (formula: `8.0 + lineWidth * 2.0`)

## Related Features

- **Skeleton Visibility Toggle**: `skeletonsEnabled` setting controls whether skeletons are visible
- **Per-Model Skeleton Control**: Individual models can have skeleton visibility toggled
- **Bounding Box Rendering**: Uses separate line rendering (not affected by skeleton line width)

## Future Enhancements

Potential improvements:
- Per-model skeleton line width (currently global)
- Color customization for skeleton lines (currently hardcoded green)
- Different line widths for different bone types (e.g., root bones vs. leaf bones)
- Line width based on bone length (thicker for longer bones)

## Testing

### Test Case 1: Basic Functionality
1. Open Viewport Settings panel
2. Adjust Skeleton Line Width slider
3. **Expected**: Skeleton bone lines change thickness immediately
4. **Expected**: Setting persists after application restart

### Test Case 2: Range Validation
1. Set slider to minimum (1.0)
2. **Expected**: Skeleton lines are thin but visible
3. Set slider to maximum (10.0)
4. **Expected**: Skeleton lines are thick and prominent

### Test Case 3: Reset Functionality
1. Adjust slider to any value
2. Click Reset button
3. **Expected**: Slider returns to 2.0
4. **Expected**: Skeleton lines return to default thickness

### Test Case 4: Persistence
1. Set skeleton line width to 5.0
2. Close and restart application
3. **Expected**: Line width is still 5.0 after restart

### Test Case 5: State Isolation
1. Set skeleton line width to 10.0
2. Check grid and bounding box rendering
3. **Expected**: Grid and bounding box lines are not affected (still default width)

## Notes

- Line width is a global setting (affects all skeleton rendering)
- **OpenGL Compatibility Profile is required** for this feature to work (Core Profile clamps line width to 1.0)
- Some OpenGL implementations may not support all line widths (may round to nearest supported value)
- Line width reset ensures other rendering passes are not affected
- Setting is saved in `app_settings.json` under `environment.skeletonLineWidth`

## OpenGL Profile Configuration

**Location**: `src/application.cpp` (lines 97-100)

The application uses OpenGL Compatibility Profile to support `glLineWidth()`:

```cpp
glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
// Use Compatibility Profile to support glLineWidth() for skeleton line width feature
// Core Profile deprecates glLineWidth() and clamps all values > 1.0f to 1.0 on modern drivers
glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
```

**If Compatibility Profile causes issues**:
- If modern shaders break with Compatibility Profile, consider implementing a Geometry Shader solution
- Geometry Shader can expand lines into screen-facing quads, allowing variable line widths in Core Profile
- This requires more complex shader code but maintains Core Profile compatibility
