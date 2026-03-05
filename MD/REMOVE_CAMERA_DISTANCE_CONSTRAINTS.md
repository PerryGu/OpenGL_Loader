# Remove Camera Distance Constraints & Increase Clipping Range

## Date
Created: 2024

## Overview

Removed camera distance constraints and increased the far clipping plane to support large-scale models (e.g., 50x scaled models). The camera can now zoom out freely without being clipped or stopping prematurely.

## Problem

When a model is scaled by 50x, the camera was "stuck" and could not move back far enough to see the entire model. This was caused by:
1. **Far clipping plane too small**: Default was 100.0f, max in UI was 1000.0f
2. **Distance clamps too restrictive**: Multiple clamps limiting camera distance to 1000.0f
3. **Framing logic hardcoded limits**: Framing distance calculation had max distance of 1000.0f

## Solution

### 1. Increase Far Clipping Plane (`app_settings.h`, `scene.cpp`)

**Default Value:**
- Changed from `100.0f` to `100000.0f` in `app_settings.h`

**UI Slider Range:**
- Changed from `10.0f - 1000.0f` to `10.0f - 100000.0f` in `scene.cpp`

**Reset Value:**
- Changed from `100.0f` to `100000.0f` in `scene.cpp`

### 2. Remove/Increase Distance Clamps (`camera.cpp`)

**Orbit Distance Clamp:**
```cpp
// Before:
if (orbitDistance > 1000.0f) {
    orbitDistance = 1000.0f;
}

// After:
if (orbitDistance > 50000.0f) {
    orbitDistance = 50000.0f;
}
```

**Framing Distance Clamp (in `aimAtTargetWithFraming`):**
```cpp
// Before:
if (finalDist > 1000.0f) {
    finalDist = 1000.0f;
}

// After:
if (finalDist > 50000.0f) {
    finalDist = 50000.0f;
}
```

**Frame Distance Clamp (in other framing methods):**
```cpp
// Before:
if (frameDistance > 1000.0f) {
    frameDistance = 1000.0f;
}

// After:
if (frameDistance > 50000.0f) {
    frameDistance = 50000.0f;
}
```

### 3. Projection Matrix Updates

The projection matrix is updated every frame in `main.cpp`:
```cpp
float farClipPlane = settings.environment.farClipPlane;
projection = glm::perspective(glm::radians(cameras[activeCam].getZoom()), aspectRatio, 0.1f, farClipPlane);
```

**Result:** When the far clipping plane is changed in the UI, it takes effect immediately on the next frame (no explicit `updateProjectionMatrix()` call needed).

### 4. Framing Logic Verification

The framing logic (`aimAtTargetWithFraming`) now uses the increased clamp value (50,000.0f), ensuring it doesn't override the bounding box calculation for large models.

## Code Changes

### Files Modified

1. **`src/io/app_settings.h`**:
   - Changed default `farClipPlane` from `100.0f` to `100000.0f`
   - Updated comment to indicate support for large-scale models

2. **`src/io/scene.cpp`**:
   - Increased slider range from `10.0f - 1000.0f` to `10.0f - 100000.0f`
   - Changed reset value from `100.0f` to `100000.0f`
   - Added comment explaining the change

3. **`src/io/camera.cpp`**:
   - Increased orbit distance clamp from `1000.0f` to `50000.0f` (3 locations)
   - Added comments explaining the change for large-scale models

## Technical Details

### Why 100,000.0 for Far Clipping Plane?

- Supports models scaled up to 50x or more
- Provides ample headroom for very large scenes
- Still reasonable for depth buffer precision (though precision decreases with larger ranges)

### Why 50,000.0 for Distance Clamps?

- Allows camera to move far enough to frame large models
- Prevents infinite distances (still has reasonable upper bound)
- Matches the scale of large models (50x scaled model might need 10,000+ units distance)

### Projection Matrix Update

The projection matrix is recalculated every frame in `main.cpp`:
- Uses `settings.environment.farClipPlane` directly
- No caching or delayed updates
- Changes take effect immediately

## Testing

### Verification Steps

1. **Load a model** and scale it by 50x in the Property Panel
2. **Press 'F'** to frame the model
3. **Verify** the camera can zoom out far enough to see the entire model
4. **Check** that objects are not clipped (not disappearing at distance)
5. **Adjust** the Far Clipping Plane slider in Viewport Settings
6. **Verify** the change takes effect immediately

### Expected Results

- ✅ Camera can zoom out to see entire 50x scaled model
- ✅ Objects are not clipped at distance
- ✅ Far clipping plane slider allows values up to 100,000.0
- ✅ Framing ('F' key) works correctly for large models
- ✅ No premature camera stopping

## Summary

Removed camera distance constraints and increased clipping range by:

✅ **Increase Far Clipping Plane**: Default 100,000.0f, UI range 10.0f - 100,000.0f
✅ **Remove Distance Clamps**: Increased from 1,000.0f to 50,000.0f (3 locations)
✅ **Projection Matrix Updates**: Already updates every frame (no changes needed)
✅ **Framing Logic**: Verified no hardcoded max distance overriding bounding box calculation

**Result**: The user can now zoom out freely to see the entire 50x scaled model without it being clipped or the camera stopping prematurely.
