# Clean Camera Framing - Remove World-Position Dependency

## Date
Created: 2024

## Overview

Simplified camera framing calculation to remove world-position dependency. The 'F' key distance is now identical regardless of the character's position in the world.

## Objective

**Ensure the 'F' key distance is identical regardless of the character's position.**

The framing calculation is now based purely on bounding box size (local units), not world position, ensuring consistent framing on the first click, every time, everywhere.

## Implementation Details

### 1. Calculate Fixed Size (`camera.cpp`)

Get the Bounding Box height and width in LOCAL units:

```cpp
// 1. CALCULATE FIXED SIZE: Get the Bounding Box height and width in LOCAL units
// Get the Bounding Box height in LOCAL units: float h = selectedAABB.max.y - selectedAABB.min.y
float h = boundingBoxMax.y - boundingBoxMin.y;

// Get the width: float w = selectedAABB.max.x - selectedAABB.min.x
float w = boundingBoxMax.x - boundingBoxMin.x;

// Use the larger one: float size = max(h, w)
// Also check depth (Z) to ensure we account for all dimensions
float d = boundingBoxMax.z - boundingBoxMin.z;
float size = glm::max(glm::max(h, w), d);  // Use maximum dimension

// Apply model scale to size (PropertyPanel scale)
size = size * glm::max(glm::max(modelScale.x, modelScale.y), modelScale.z);
```

**Key Points:**
- `h`: Height in local units (bounding box Y dimension)
- `w`: Width in local units (bounding box X dimension)
- `d`: Depth in local units (bounding box Z dimension)
- `size`: Maximum dimension (ensures object fits in all directions)
- Apply PropertyPanel scale to get world-space size

### 2. Calculate Distance from Size (`camera.cpp`)

Use the FOV to find the static distance (not position-dependent):

```cpp
// 2. CALCULATE DISTANCE FROM SIZE (NOT POSITION): Use the FOV to find the static distance
// Use the FOV to find the static distance: float dist = (size / 2.0f) / tan(glm::radians(camera.m_fov / 2.0f))
float fovRadians = glm::radians(zoom);  // zoom is the FOV in degrees
float dist = (size / 2.0f) / tan(fovRadians / 2.0f);
```

**Formula Explanation:**
- `size / 2.0f`: Half the maximum dimension (radius)
- `fovRadians / 2.0f`: Half the field of view angle
- `tan(fovRadians / 2.0f)`: Tangent of half FOV
- `dist = (size/2) / tan(fov/2)`: Distance to fit object in view

**Why This Works:**
- Distance is calculated from size only (not world position)
- Same size = same distance, regardless of world position
- No dependency on world coordinates

### 3. Apply 2x Multiplier (Pivot Fix) (`camera.cpp`)

Because we aim at the Gizmo (Feet), we need exactly `dist * 2.0f` to see the head:

```cpp
// 3. APPLY 2X MULTIPLIER (PIVOT FIX): Because we aim at the Gizmo (Feet), we need exactly dist * 2.0f to see the head
// Add a 10% buffer: finalDist = dist * 2.2f
float finalDist = dist * 2.2f;
```

**Why 2.2f:**
- `dist * 2.0f`: Distance to see full height when aiming at feet (gizmo at bottom)
- `* 1.1f`: 10% safety buffer (prevents head touching top of frame)
- `finalDist = dist * 2.2f`: Total multiplier (2.0 for pivot fix + 0.2 for buffer)

### 4. Position Update (`camera.cpp`)

Set camera position using gizmo world position:

```cpp
// 4. POSITION UPDATE: target = gizmo.getWorldPosition(); camera.position = target - (camera.forward * finalDist)
// targetPos is already the gizmo world position (passed as parameter)
// Ensure camera.forward is properly normalized before calculation
glm::vec3 normalizedForward = glm::normalize(cameraFront);
cameraPos = targetPos - normalizedForward * finalDist;

// 5. NO STATIC VARIABLES: Update orbit distance to match the new distance (fresh calculation, no previous frame data)
orbitDistance = finalDist;
```

**Key Points:**
- `targetPos`: Gizmo world position (passed as parameter)
- `normalizedForward`: Normalized camera forward vector
- `cameraPos`: Camera position at calculated distance
- `orbitDistance`: Updated to match new distance (fresh calculation)

### 5. No Static Variables (`camera.cpp`)

Ensure no previous frame data or `orbitDistance` is used:

```cpp
// 5. NO STATIC VARIABLES: Update orbit distance to match the new distance (fresh calculation, no previous frame data)
orbitDistance = finalDist;
```

**Key Points:**
- All calculations are fresh (no previous frame data)
- `orbitDistance` is set from current calculation
- No dependency on previous camera state

## Code Changes

### Files Modified

1. **`src/io/camera.cpp`**:
   - Simplified `aimAtTargetWithFraming()` to use size-based calculation
   - Removed complex corner-based calculation
   - Removed world-position dependency
   - Uses simple formula: `dist = (size/2) / tan(fov/2) * 2.2f`

## Technical Details

### Size-Based vs Corner-Based

**Before (Corner-Based):**
```
- Calculate 8 corners in world space
- Transform to camera space
- Calculate distances for each corner
- Find maximum distance
- Complex, position-dependent
```

**After (Size-Based):**
```
- Get bounding box size (local units)
- Apply scale
- Calculate distance from size: dist = (size/2) / tan(fov/2)
- Apply 2.2x multiplier for pivot fix
- Simple, position-independent
```

### Formula Derivation

**Basic Distance:**
```
dist = (size / 2) / tan(fov / 2)
```
- `size / 2`: Half the maximum dimension (radius)
- `tan(fov / 2)`: Half FOV tangent
- Distance to fit object in view

**Pivot Fix:**
```
finalDist = dist * 2.2f
```
- `* 2.0f`: Account for aiming at feet (need to see full height)
- `* 1.1f`: 10% safety buffer
- `* 2.2f`: Combined multiplier

### Why This Works

**Position Independence:**
- Distance is calculated from size only (not world position)
- Same bounding box size = same distance
- Works at (0,0,0) or (500,0,500) - same result

**Pivot Fix:**
- Gizmo is at feet/bottom of character
- Need to see full height above gizmo
- 2x multiplier ensures head is visible
- 10% buffer prevents edge cases

## Testing

### Verification Steps

1. **Load a character model** at world origin (0, 0, 0)
2. **Press 'F'** to aim at gizmo
3. **Note the camera distance** from character
4. **Move character** to (500, 0, 500) or any distant position
5. **Press 'F'** again
6. **Verify camera distance** is the SAME as at origin

### Expected Results

- ✅ Camera distance is identical regardless of character position
- ✅ Consistent framing on first click, every time, everywhere
- ✅ Full character visible from feet to head
- ✅ No world-position dependency
- ✅ Simple, reliable calculation

## Summary

Cleaned camera framing to remove world-position dependency by:

✅ **Calculate Fixed Size**: Get bounding box size in local units (max of h, w, d)
✅ **Calculate Distance from Size**: Use `dist = (size/2) / tan(fov/2)` (not position)
✅ **Apply 2x Multiplier**: `finalDist = dist * 2.2f` for pivot fix (aim at feet, see head)
✅ **Position Update**: `camera.position = target - (camera.forward * finalDist)`
✅ **No Static Variables**: Fresh calculation every time, no previous frame data

**Result**: Consistent framing on the first click, every time, everywhere. The 'F' key distance is identical regardless of the character's position in the world.
