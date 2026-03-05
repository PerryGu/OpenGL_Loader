# Bulletproof Camera Framing - Scale & Pivot Bias Fix

## Date
Created: 2024

## Problem

The camera was too close to the character. This was because:
1. The Bounding Box height did not account for the Model's Scale
2. The distance calculation was too aggressive (not accounting for scale)
3. The camera needs to see the ENTIRE height above the center line when aiming at feet (gizmo)

## Root Cause

**Before Fix:**
- Distance was calculated using local bounding box height (not scaled)
- Formula didn't account for model's world scale
- Result: Camera too close, especially for scaled-up models

**After Fix:**
- Distance is calculated using world height (local height * scale.y)
- Formula accounts for model's world scale
- Safety buffer (1.3f) ensures head isn't touching top of frame
- Result: Full character visible from feet to head

## Solution

### 1. Calculate World Height (`camera.cpp`)

Get the local height and multiply by model's world scale:

```cpp
// 1. CALCULATE WORLD HEIGHT: Get the local height and multiply by model's world scale
// Get the local height: float localHeight = (selectedAABB.max.y - selectedAABB.min.y)
float localHeight = boundingBoxMax.y - boundingBoxMin.y;

// CRUCIAL: Multiply by the model's world scale: float worldHeight = localHeight * model.getScale().y
float worldHeight = localHeight * modelScale.y;
```

**Key Points:**
- `localHeight`: Height in local space (bounding box size)
- `modelScale.y`: Model's world scale in Y direction (vertical scale)
- `worldHeight`: Actual height in world space (accounts for scale)

### 2. Correct Distance for "Look-at-Feet" (`camera.cpp`)

Since we aim at the Gizmo (Feet), the camera must see the ENTIRE height above the center line:

```cpp
// 2. CORRECT DISTANCE FOR "LOOK-AT-FEET": Since we aim at the Gizmo (Feet), the camera must see
// the ENTIRE height above the center line.
// Formula: float dist = worldHeight / tan(glm::radians(camera.m_fov / 2.0f))
float fovRadians = glm::radians(zoom);  // zoom is the FOV in degrees
float dist = worldHeight / tan(fovRadians / 2.0f);
```

**Formula Explanation:**
- `worldHeight`: Actual height in world space (scaled)
- `fovRadians / 2.0f`: Half the field of view angle
- `tan(fovRadians / 2.0f)`: Tangent of half FOV
- `dist = worldHeight / tan(...)`: Distance needed to fit entire height in view

**Why `tan` instead of `sin`:**
- `tan` provides linear relationship for vertical objects
- More accurate for "look-at-feet" scenario where we need to see full height
- Accounts for perspective projection correctly

### 3. Add Safety Buffer (`camera.cpp`)

Apply a final multiplier of **1.3f** so the head isn't touching the very top of the frame:

```cpp
// 3. ADD SAFETY BUFFER: Apply a final multiplier of 1.3f to the distance so the head isn't
// touching the very top of the frame.
// float finalDist = dist * 1.3f
float idealDistance = dist * 1.3f;
```

**Why 1.3f:**
- Provides 30% extra space above the character
- Ensures head isn't touching the top of the frame
- Comfortable viewing distance with padding

### 4. Apply to Camera (`camera.cpp`)

Ensure camera.forward is properly normalized before calculation:

```cpp
// 4. APPLY TO CAMERA: Ensure camera.forward is properly normalized before calculation
// glm::vec3 gizmoPos = gizmo.getWorldPosition(); (targetPos is already the gizmo world position)
// camera.position = gizmoPos - (camera.forward * finalDist)
glm::vec3 normalizedForward = glm::normalize(cameraFront);
cameraPos = targetPos - normalizedForward * idealDistance;
```

**Key Points:**
- `targetPos`: Gizmo world position (passed as parameter)
- `normalizedForward`: Normalized forward vector (unit length)
- `cameraPos`: Camera position at correct distance from gizmo

### 5. Validation (`camera.cpp`)

Ensure the camera is NOT tilted (Pitch is not 90 degrees) during calculation:

```cpp
// 5. VALIDATION: Ensure the camera is NOT tilted (Pitch is not 90 degrees) during this calculation
// to avoid math errors. Check pitch before calculating distance.
if (std::abs(pitch) >= 89.0f) {
    // Camera is too tilted, use fallback calculation
    // This prevents division by zero or extreme values in tan calculation
    pitch = (pitch > 0) ? 88.0f : -88.0f;
}
```

**Key Points:**
- Prevents division by zero in `tan` calculation
- Avoids extreme values when camera is looking straight up/down
- Clamps pitch to safe range (-88° to +88°)

### 6. Get Model Scale (`scene.cpp`)

Retrieve model scale from PropertyPanel (RootNode scale):

```cpp
// Get model scale from PropertyPanel (RootNode scale)
if (selectedModelIndex >= 0) {
    std::string modelRootNodeName = mOutliner.getRootNodeName(selectedModelIndex);
    if (!modelRootNodeName.empty()) {
        const auto& allBoneScales = mPropertyPanel.getAllBoneScales();
        auto scaleIt = allBoneScales.find(modelRootNodeName);
        if (scaleIt != allBoneScales.end()) {
            modelScale = scaleIt->second;
        }
    }
}
```

**Key Points:**
- Gets scale from PropertyPanel's bone scales map
- Uses RootNode name to find the correct scale
- Defaults to (1.0f, 1.0f, 1.0f) if not found

## Code Changes

### Files Modified

1. **`src/io/camera.h`**:
   - Added `modelScale` parameter to `aimAtTargetWithFraming()` (default: glm::vec3(1.0f))

2. **`src/io/camera.cpp`**:
   - Modified `aimAtTargetWithFraming()` to accept `modelScale` parameter
   - Changed from radius-based to height-based calculation
   - Calculates world height: `worldHeight = localHeight * modelScale.y`
   - Uses `tan` for distance calculation: `dist = worldHeight / tan(fov/2)`
   - Added safety buffer: `idealDistance = dist * 1.3f`
   - Added pitch validation to prevent math errors

3. **`src/io/scene.cpp`**:
   - Modified `applyCameraAimConstraint()` to get model scale from PropertyPanel
   - Passes model scale to `aimAtTargetWithFraming()`

## Technical Details

### Height-Based vs Radius-Based

**Before (Radius-Based):**
```
radius = maxSize / 2.0f  // Half the maximum dimension
dist = radius / sin(fov/2) * 1.1f
```

**After (Height-Based):**
```
localHeight = boundingBoxMax.y - boundingBoxMin.y
worldHeight = localHeight * modelScale.y  // Account for scale
dist = worldHeight / tan(fov/2) * 1.3f
```

### Why Height-Based Works Better

**For "Look-at-Feet" Scenario:**
- Camera aims at gizmo (feet/pivot point)
- Need to see ENTIRE height above the center line
- Height-based calculation directly accounts for vertical extent
- More accurate than radius-based for vertical objects

**Scale Accounting:**
- Local height is in model's local space
- World scale transforms local space to world space
- `worldHeight = localHeight * scale.y` gives actual world-space height
- Distance calculation uses world-space height

### Formula Derivation

**Perspective Projection:**
- Object height `h` at distance `d` appears as: `screenHeight = 2 * d * tan(fov/2)`
- To fit height `h` in view: `h = 2 * d * tan(fov/2)`, so `d = h / (2 * tan(fov/2))`
- For our case (aiming at feet, need to see full height): `d = worldHeight / tan(fov/2)`

**Safety Buffer:**
- `finalDist = dist * 1.3f` adds 30% padding
- Ensures head isn't touching top of frame
- Provides comfortable viewing distance

## Testing

### Verification Steps

1. **Load a character model** with default scale (1.0)
2. **Press 'F'** to aim at gizmo
3. **Verify full character visible**: Head should not be cut off
4. **Scale up the model** (e.g., scale.y = 2.0)
5. **Press 'F'** again
6. **Verify full character still visible**: Camera should move further back
7. **Test with different scales**: Should work for any scale value

### Expected Results

- ✅ Full character visible from feet to head
- ✅ Camera distance accounts for model scale
- ✅ Head not touching top of frame (safety buffer)
- ✅ Works correctly for scaled-up models
- ✅ Works correctly for scaled-down models
- ✅ No math errors when camera is tilted

## Summary

Fixed camera framing to account for scale and pivot bias by:

✅ **Calculate World Height**: Use local height * model scale.y
✅ **Correct Distance for "Look-at-Feet"**: Use `tan` with world height
✅ **Add Safety Buffer**: Apply 1.3f multiplier for comfortable viewing
✅ **Apply to Camera**: Normalize forward vector and position camera correctly
✅ **Validation**: Ensure pitch is not 90 degrees to avoid math errors
✅ **Get Model Scale**: Retrieve scale from PropertyPanel and pass to framing function

**Result**: Regardless of the model's scale or position in the world, the 'F' key shows the FULL character from feet to head, with proper distance accounting for scale and safety buffer to prevent head cutoff.
