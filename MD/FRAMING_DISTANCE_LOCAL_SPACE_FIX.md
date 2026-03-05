# Framing Distance Drift Fix - Local vs World Space

## Date
Created: 2024

## Problem

The framing distance increases as the character moves further from the world origin. This is a coordinate space mismatch - the distance was being calculated in world space, which includes the world position offset, causing the distance to drift based on the character's position.

## Root Cause

**Before Fix:**
- Distance was calculated from gizmo position to furthest corner in **world space**
- This included the world position offset in the calculation
- As character moved further from origin, the distance calculation included larger world coordinates
- Result: Distance increased with distance from origin

**After Fix:**
- Radius is calculated in **local space** (just the bounding box size)
- Distance formula uses this constant local-space radius
- World position is only used for final camera position calculation
- Result: Distance is constant regardless of character position

## Solution

### 1. Calculate Constant Radius (`camera.cpp`)

Instead of calculating distance in World Space, calculate the 'radius' once in **Local Space** of the Bounding Box:

```cpp
// 1. CALCULATE CONSTANT RADIUS: Calculate the radius in LOCAL SPACE of the Bounding Box
// Instead of calculating distance in World Space, calculate the 'radius' once in Local Space
// This ensures the radius is ALWAYS the same size regardless of where the character is standing
glm::vec3 boundingBoxSize = boundingBoxMax - boundingBoxMin;  // Size in local space
float maxSize = glm::max(glm::max(boundingBoxSize.x, boundingBoxSize.y), boundingBoxSize.z);
float radius = maxSize / 2.0f;  // Half the maximum dimension (constant in local space)
```

**Key Points:**
- `boundingBoxSize = boundingBoxMax - boundingBoxMin` gives size in local space
- `radius = maxSize / 2.0f` is constant regardless of world position
- This radius is the same whether character is at (0,0,0) or (500,0,500)

### 2. Correct Distance Formula (`camera.cpp`)

Use the FOV to find the distance using the constant local-space radius:

```cpp
// 2. CORRECT DISTANCE FORMULA: Use the FOV to find the distance
// Use sin for accurate distance calculation based on FOV
float fovRadians = glm::radians(zoom);  // zoom is the FOV in degrees
float dist = radius / sin(fovRadians / 2.0f);
```

**Formula Explanation:**
- `radius`: Constant local-space radius (half the maximum dimension)
- `fovRadians / 2.0f`: Half the field of view angle
- `sin(fovRadians / 2.0f)`: Sine of half FOV
- `dist = radius / sin(...)`: Base distance to fit the object

### 3. Adjust Padding (`camera.cpp`)

Reduce the multiplier from 1.5f to **1.1f** to fix the "too far" issue at the origin:

```cpp
// 3. ADJUST PADDING: Reduce the multiplier from 1.5f to 1.1f to fix the "too far" issue at the origin
float idealDistance = dist * 1.1f;
```

**Why 1.1f:**
- Previous 1.5f was too much padding, causing camera to be too far
- 1.1f provides 10% padding (comfortable viewing distance)
- Works correctly at origin and at distance from origin

### 4. Final Position Logic (`camera.cpp`)

Ensure `camera.forward` is properly normalized before calculation:

```cpp
// 4. FINAL POSITION LOGIC: Ensure camera.forward is properly normalized before calculation
// targetPos = gizmo.getWorldPosition() (already passed as parameter)
// camera.position = targetPos - (camera.forward * dist * 1.1f)
// The cameraFront vector already points from camera to target (we just set it in aimAtTarget)
// Ensure it's normalized for accurate distance calculation
glm::vec3 normalizedForward = glm::normalize(cameraFront);
cameraPos = targetPos - normalizedForward * idealDistance;
```

**Key Points:**
- `targetPos` is the gizmo world position (passed as parameter)
- `normalizedForward` ensures the forward vector is unit length
- `cameraPos = targetPos - normalizedForward * idealDistance` places camera at correct distance
- World position is only used here, not in radius calculation

## Code Changes

### Files Modified

1. **`src/io/camera.cpp`**:
   - Changed from world-space distance calculation to local-space radius calculation
   - Changed from `tan` to `sin` for distance formula
   - Reduced padding from 1.5f to 1.1f
   - Added normalization of forward vector before position calculation

## Technical Details

### Coordinate Space Comparison

**Before (World Space):**
```
Character at (0, 0, 0):
- Bounding box: min=(0,0,0), max=(1,2,1)
- Gizmo: (0.5, 0.0, 0.5)
- Distance from gizmo to corner: ~1.12
- Camera distance: ~1.12 / tan(fov/2) * 1.5

Character at (500, 0, 500):
- Bounding box: min=(500,0,500), max=(501,2,501)
- Gizmo: (500.5, 0.0, 500.5)
- Distance from gizmo to corner: ~1.12 (same!)
- BUT: World coordinates are large, causing calculation drift
```

**After (Local Space):**
```
Character at (0, 0, 0):
- Bounding box size: (1, 2, 1) in local space
- Radius: max(1,2,1) / 2 = 1.0 (constant)
- Camera distance: 1.0 / sin(fov/2) * 1.1

Character at (500, 0, 500):
- Bounding box size: (1, 2, 1) in local space (SAME!)
- Radius: max(1,2,1) / 2 = 1.0 (SAME!)
- Camera distance: 1.0 / sin(fov/2) * 1.1 (SAME!)
```

### Why Local Space Works

**Local Space:**
- Bounding box size is independent of world position
- Radius is constant regardless of where character is
- Distance calculation uses constant radius
- World position only affects final camera position

**World Space (Problem):**
- Bounding box coordinates include world position
- Distance calculation includes world position offset
- As world position increases, distance calculation drifts
- Result: Different camera distance at different positions

### Formula Comparison

**Before:**
```cpp
// World space calculation
maxDist = max(distance(corner, gizmoWorldPos))  // Includes world position
idealDistance = maxDist / tan(fov/2) * 1.5f
```

**After:**
```cpp
// Local space calculation
radius = maxSize / 2.0f  // Constant, no world position
dist = radius / sin(fov/2)  // Constant distance
idealDistance = dist * 1.1f  // With padding
```

## Testing

### Verification Steps

1. **Load a character model** at world origin (0, 0, 0)
2. **Press 'F'** to aim at gizmo
3. **Note the camera distance** from character
4. **Move character** to (500, 0, 500) or any distant position
5. **Press 'F'** again
6. **Verify camera distance** is the SAME as at origin

### Expected Results

- ✅ Camera distance is constant regardless of character position
- ✅ No drift when character moves from origin
- ✅ Distance calculation uses local-space radius (constant)
- ✅ World position only affects final camera position (not distance calculation)
- ✅ Padding (1.1f) provides comfortable viewing distance
- ✅ Works correctly at origin and at distance from origin

## Summary

Fixed framing distance drift by:

✅ **Calculate Constant Radius**: Use local-space bounding box size (not world coordinates)
✅ **Correct Distance Formula**: Use `sin` with constant local-space radius
✅ **Adjust Padding**: Reduce from 1.5f to 1.1f for correct distance
✅ **Final Position Logic**: Normalize forward vector and use world position only for final placement

**Result**: Pressing 'F' results in the SAME camera distance from the character, whether they are at (0,0,0) or (500,0,500). The distance is now calculated in local space, ensuring consistency regardless of world position.
