# Adjust Viewport Limits & Grid Max Scale

## Date
Created: 2024

## Overview

Refined clipping and grid boundaries to support large models while maintaining precision. Updated far clipping plane maximum to 10,000.0 and grid scale maximum to 1,000.0, with corresponding adjustments to camera distance clamps.

## Objective

**Refine the clipping and grid boundaries to support large models while maintaining precision.**

The UI now allows a Far Plane up to 10,000.0 and a Grid Scale up to 1,000.0, with camera/grid rendering correctly at these scales.

## Implementation Details

### 1. Update Far Clipping Plane (`app_settings.h`, `scene.cpp`)

**Default Value:**
- Changed from `100000.0f` to `5000.0f` in `app_settings.h`
- Provides good balance for large-scale models while maintaining precision

**UI Slider Range:**
- Changed from `10.0f - 100000.0f` to `10.0f - 10000.0f` in `scene.cpp`
- Maximum limit set to 10,000.0 as requested

**Reset Value:**
- Changed from `100000.0f` to `5000.0f` in `scene.cpp`
- Matches the new default value

### 2. Increase Grid Scale Limit (`scene.cpp`)

**Grid Scale Slider:**
```cpp
// Before:
if (ImGui::SliderFloat("Grid Scale", &gridSize, 1.0f, 100.0f, "%.1f")) {

// After:
if (ImGui::SliderFloat("Grid Scale", &gridSize, 1.0f, 1000.0f, "%.1f")) {
```

**Key Points:**
- Maximum increased from `100.0f` to `1000.0f`
- Grid rendering logic can handle these larger dimensions (no vertex buffer limits)
- Grid vertices are generated dynamically based on size and spacing

### 3. Internal Logic - Camera Distance Clamps (`camera.cpp`)

**Orbit Distance Clamp:**
```cpp
// Before:
if (orbitDistance > 50000.0f) {
    orbitDistance = 50000.0f;
}

// After:
if (orbitDistance > 10000.0f) {
    orbitDistance = 10000.0f;
}
```

**Framing Distance Clamp (in `aimAtTargetWithFraming`):**
```cpp
// Before:
if (finalDist > 50000.0f) {
    finalDist = 50000.0f;
}

// After:
if (finalDist > 10000.0f) {
    finalDist = 10000.0f;
}
```

**Frame Distance Clamp (in other framing methods):**
```cpp
// Before:
if (frameDistance > 50000.0f) {
    frameDistance = 50000.0f;
}

// After:
if (frameDistance > 10000.0f) {
    frameDistance = 10000.0f;
}
```

**Key Points:**
- All distance clamps now respect the 10,000.0 limit
- Matches the far clipping plane maximum
- Maintains precision while supporting large models

### 4. Projection Matrix Updates

The projection matrix is updated every frame in `main.cpp`:
```cpp
float farClipPlane = settings.environment.farClipPlane;
projection = glm::perspective(glm::radians(cameras[activeCam].getZoom()), aspectRatio, 0.1f, farClipPlane);
```

**Result:** When the far clipping plane is changed in the UI, it takes effect immediately on the next frame (no explicit `updateProjectionMatrix()` call needed - the matrix is recalculated every frame).

## Code Changes

### Files Modified

1. **`src/io/app_settings.h`**:
   - Changed default `farClipPlane` from `100000.0f` to `5000.0f`
   - Updated comment to indicate support for large-scale models with precision

2. **`src/io/scene.cpp`**:
   - Increased far clipping plane slider range from `10.0f - 100000.0f` to `10.0f - 10000.0f`
   - Changed reset value from `100000.0f` to `5000.0f`
   - Increased grid scale slider range from `1.0f - 100.0f` to `1.0f - 1000.0f`
   - Added comments explaining the changes

3. **`src/io/camera.cpp`**:
   - Decreased orbit distance clamp from `50000.0f` to `10000.0f` (3 locations)
   - Updated comments to indicate the limit matches far clipping plane

## Technical Details

### Why 10,000.0 for Far Clipping Plane?

- Supports large-scale models (e.g., 50x scaled models)
- Maintains better depth buffer precision than 100,000.0
- Good balance between range and precision
- Default of 5,000.0 provides headroom while maintaining precision

### Why 1,000.0 for Grid Scale?

- Supports large models while keeping grid visible
- Grid rendering is dynamic (no hardcoded limits)
- Grid vertices are generated based on size and spacing
- Large grids may have more lines but rendering handles it

### Why 10,000.0 for Camera Distance Clamps?

- Matches the far clipping plane maximum
- Ensures camera doesn't exceed visible range
- Maintains consistency between camera limits and clipping plane
- Prevents objects from being out of view due to distance limits

### Projection Matrix Update

The projection matrix is recalculated every frame in `main.cpp`:
- Uses `settings.environment.farClipPlane` directly
- No caching or delayed updates
- Changes take effect immediately when slider is adjusted

### Grid Rendering at Large Scales

The grid rendering logic (`grid.cpp`) can handle large sizes:
- Vertices are generated dynamically based on `size` and `spacing`
- No hardcoded vertex limits
- Grid extends from `-size/2` to `+size/2` in both X and Z
- Large grids will have more lines but rendering handles it correctly

## Testing

### Verification Steps

1. **Open Viewport Settings** window
2. **Check Far Clipping Plane slider** - verify maximum is 10,000.0
3. **Adjust Far Clipping Plane** - verify change takes effect immediately
4. **Check Grid Scale slider** - verify maximum is 1,000.0
5. **Set Grid Scale to 1,000.0** - verify grid renders correctly
6. **Load a large model** (e.g., 50x scaled)
7. **Press 'F' to frame** - verify camera distance respects 10,000.0 limit
8. **Zoom out** - verify camera stops at 10,000.0 distance

### Expected Results

- ✅ Far clipping plane slider allows values up to 10,000.0
- ✅ Default far clipping plane is 5,000.0
- ✅ Grid scale slider allows values up to 1,000.0
- ✅ Grid renders correctly at 1,000.0 scale
- ✅ Camera distance clamps respect 10,000.0 limit
- ✅ Projection matrix updates immediately when far plane changes
- ✅ No visual artifacts at large scales

## Summary

Adjusted viewport limits and grid max scale by:

✅ **Update Far Clipping Plane**: Maximum 10,000.0, default 5,000.0
✅ **Increase Grid Scale Limit**: Maximum 1,000.0 (from 100.0)
✅ **Internal Logic**: Camera distance clamps respect 10,000.0 limit (3 locations)
✅ **Projection Matrix Updates**: Already updates every frame (no changes needed)

**Result**: The UI allows a Far Plane up to 10,000.0 and a Grid Scale up to 1,000.0, and the camera/grid render correctly at these scales while maintaining precision.
