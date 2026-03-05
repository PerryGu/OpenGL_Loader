# Tighten Camera Framing for Centered Aim

## Date
Created: 2024

## Overview

Tightened camera framing after switching to center-aim. The character was too small in the frame due to an old 2.0x distance multiplier that was no longer needed. The framing now uses world-space AABB directly without double-applying scale.

## Problem

**After switching to "Center-Aim", the character is now too small in the frame.**

This was caused by:
- Old 2.0x distance multiplier (from pivot fix) was no longer needed
- Model scale was being applied twice (once in world-space AABB, once in framing calculation)
- Distance calculation was too conservative

## Solution

### 1. Use World-Space AABB Directly (`camera.cpp`)

Use the `min` and `max` of the **World-Space** Bounding Box directly (which already includes position and scale):

```cpp
// 1. USE WORLD-SPACE AABB DIRECTLY: The bounding box is already in world space (includes position and scale)
// Get the World-Space Bounding Box dimensions directly
float worldHeight = boundingBoxMax.y - boundingBoxMin.y;
float worldWidth = boundingBoxMax.x - boundingBoxMin.x;
float worldDepth = boundingBoxMax.z - boundingBoxMin.z;

// Use the maximum dimension to ensure the entire object fits
float objectSize = glm::max(glm::max(worldHeight, worldWidth), worldDepth);

// NOTE: Do NOT multiply by modelScale again - the World-Space AABB already accounts for it
```

**Key Points:**
- `boundingBoxMin` and `boundingBoxMax` are already in world space (from previous fix)
- World-space AABB already includes position, rotation, and scale
- No need to apply model scale again

### 2. Remove the 2x Multiplier (`camera.cpp`)

Since the camera now points at the **center** of the AABB, use a standard framing formula:

```cpp
// 2. CALCULATE DISTANCE FROM SIZE: Use the FOV to find the static distance
// Since the camera now points at the center of the AABB, use standard framing formula
float fovRadians = glm::radians(zoom);  // zoom is the FOV in degrees
float dist = (objectSize / 2.0f) / tan(fovRadians / 2.0f);

// 3. APPLY SMALL PADDING: Add only 15% padding for a tight, centered frame
float finalDist = dist * 1.15f;
```

**Key Points:**
- Standard framing formula: `dist = (objectSize / 2.0f) / tan(fov / 2.0f)`
- No 2x multiplier needed (that was for pivot fix when aiming at feet)
- Small padding (15%) for tight, centered frame

### 3. Verify No Extra Scaling (`camera.cpp`)

Removed the line that was double-applying scale:

```cpp
// REMOVED: size = size * glm::max(glm::max(modelScale.x, modelScale.y), modelScale.z);
```

**Key Points:**
- World-space AABB already accounts for model scale
- Applying scale again would make the character appear too small
- No double-scaling

### 4. Apply Position (`camera.cpp`)

Camera position is calculated relative to the world-space center:

```cpp
// 4. POSITION UPDATE: Use lookAtTarget (center of bounding box) for camera position
glm::vec3 normalizedForward = glm::normalize(cameraFront);
cameraPos = lookAtTarget - normalizedForward * finalDist;
```

**Key Points:**
- `lookAtTarget` is the center of the world-space AABB
- Camera position is calculated at the correct distance
- Character is centered vertically in viewport

## Code Changes

### Files Modified

1. **`src/io/camera.cpp`**:
   - Changed to use world-space AABB dimensions directly
   - Removed model scale multiplication (was double-applying scale)
   - Changed distance multiplier from `1.2f` to `1.15f` (tighter framing)
   - Updated comments to reflect world-space usage

## Technical Details

### World-Space AABB

The bounding box is already in world space (from previous fix):
- Transformed from local space using RootNode transform
- Includes position, rotation, and scale
- Dimensions are in world units

### Distance Calculation

**Before:**
```cpp
float size = max(h, w, d);
size = size * modelScale;  // Double-applying scale
float dist = (size / 2.0f) / tan(fov / 2.0f);
float finalDist = dist * 1.2f;  // 20% padding
```

**After:**
```cpp
float objectSize = max(worldHeight, worldWidth, worldDepth);  // World-space size
// No scale multiplication - already in world space
float dist = (objectSize / 2.0f) / tan(fov / 2.0f);
float finalDist = dist * 1.15f;  // 15% padding (tighter)
```

### Why 1.15f Padding?

- **1.0f**: Exact fit (character touches edges)
- **1.15f**: 15% padding (tight, centered frame)
- **1.2f**: 20% padding (previous value, too loose)
- **2.2f**: 220% padding (old pivot fix, way too loose)

15% padding provides:
- Tight framing (character fills viewport)
- Small safety margin (prevents clipping)
- Centered appearance

## Testing

### Verification Steps

1. **Load a character model**
2. **Press 'F'** to frame the character
3. **Verify** character fills the viewport (not too small)
4. **Verify** character is centered vertically
5. **Check** entire character is visible (head and feet)
6. **Move character** to different positions
7. **Press 'F'** again - verify framing is consistent

### Expected Results

- ✅ Character fills viewport (not a tiny dot)
- ✅ Character is centered vertically
- ✅ Entire character is visible
- ✅ Tight, professional framing
- ✅ Consistent framing regardless of position

## Summary

Tightened camera framing for centered aim by:

✅ **Use World-Space AABB Directly**: Use world-space dimensions without extra scaling
✅ **Remove 2x Multiplier**: Use standard framing formula (no pivot fix needed)
✅ **Small Padding**: 15% padding for tight, centered frame
✅ **No Extra Scaling**: World-space AABB already includes scale
✅ **Apply Position**: Camera positioned at correct distance from world-space center

**Result**: The character now fills the viewport perfectly, centered vertically, without being a tiny dot in the distance. The framing is tight and professional.
