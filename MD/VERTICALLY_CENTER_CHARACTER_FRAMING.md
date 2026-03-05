# Vertically Center Character in Viewport (F-Key Aim Update)

## Date
Created: 2024

## Overview

Improved the 'F' framing logic to center the character's body vertically in the viewport, avoiding the "floating" look. The camera now aims at the center of the bounding box instead of the gizmo position (feet).

## Objective

**Improve the 'F' framing logic so the character's body is centered in the viewport, avoiding the "floating" look.**

When pressing 'F', the character should appear perfectly centered vertically in the blue viewport area, with the Gizmo remaining at its pivot location (feet).

## Problem

Previously, the camera aimed at the Gizmo position (feet), which made the character appear "floating" because the feet were at the bottom of the viewport and the head was near the top. This created an unbalanced visual appearance.

## Solution

### 1. Calculate Visual Center (`camera.cpp`)

Instead of aiming at the Gizmo position (feet), calculate the center of the World-Space Bounding Box:

```cpp
// 1. CALCULATE VISUAL CENTER: Instead of aiming at the Gizmo position (feet), calculate the center of the World-Space Bounding Box
// This centers the character vertically in the viewport, avoiding the "floating" look
glm::vec3 lookAtTarget = (boundingBoxMin + boundingBoxMax) * 0.5f;
```

**Key Points:**
- `boundingBoxMin` and `boundingBoxMax` are in world space
- Center is calculated as the midpoint: `(min + max) * 0.5f`
- This gives the visual center of the character's body

### 2. Update Camera Aim (`camera.cpp`)

Point the camera's `forward` vector at this `lookAtTarget`:

```cpp
// 2. UPDATE CAMERA AIM: Point the camera's forward vector at this lookAtTarget
// This updates yaw, pitch, and cameraFront to aim at the center of the bounding box
// We need the camera orientation BEFORE calculating distances
bool aimComplete = aimAtTarget(lookAtTarget);
```

**Key Points:**
- `aimAtTarget(lookAtTarget)` updates the camera's orientation to look at the center
- This updates `yaw`, `pitch`, and `cameraFront` to match the center direction
- The `aimAtTarget` method also synchronizes internal state (yaw, pitch, focusPoint)

### 3. Maintain Correct Distance (`camera.cpp`)

Keep the existing framing distance logic that uses the AABB size and FOV:

```cpp
// 2. CALCULATE DISTANCE FROM SIZE (NOT POSITION): Use the FOV to find the static distance
float fovRadians = glm::radians(zoom);  // zoom is the FOV in degrees
float dist = (size / 2.0f) / tan(fovRadians / 2.0f);

// 3. APPLY DISTANCE MULTIPLIER: Since we aim at the center of the bounding box (not feet),
// we need less padding. Use 1.2f multiplier for a small safety buffer to ensure the entire character fits
float finalDist = dist * 1.2f;
```

**Key Points:**
- Distance calculation uses bounding box size (not position)
- Since we aim at the center (not feet), we need less padding
- Changed multiplier from `2.2f` to `1.2f` (no longer need 2x for pivot fix)
- Still ensures the entire character fits in view

### 4. Position Update (`camera.cpp`)

Use `lookAtTarget` (center of bounding box) for camera position:

```cpp
// 4. POSITION UPDATE: Use lookAtTarget (center of bounding box) for camera position
// This ensures the character is centered vertically in the viewport
// The gizmo remains at its pivot location (feet), but the camera aims at the center
// Ensure camera.forward is properly normalized before calculation
glm::vec3 normalizedForward = glm::normalize(cameraFront);
cameraPos = lookAtTarget - normalizedForward * finalDist;
```

**Key Points:**
- Camera position is calculated relative to `lookAtTarget` (center)
- The gizmo remains at its pivot location (feet) - it's not moved
- Camera aims at center, ensuring character is vertically centered

### 5. Final Synchronization (`camera.cpp`)

The `aimAtTarget` method already handles synchronization:

```cpp
// The aimAtTarget() method already synchronized yaw and pitch, and we've set the position
// The camera vectors (cameraFront, cameraRight, cameraUp) are already updated by aimAtTarget()
// No need to recalculate - they're already consistent
```

**Key Points:**
- `aimAtTarget` extracts yaw and pitch from the aim direction
- Sets `focusPoint` to the target (center in this case)
- Updates camera vectors (cameraFront, cameraRight, cameraUp)
- Prevents jitter when moving the mouse afterwards

## Code Changes

### Files Modified

1. **`src/io/camera.cpp`**:
   - Added calculation of `lookAtTarget` (center of bounding box)
   - Changed `aimAtTarget(targetPos)` to `aimAtTarget(lookAtTarget)`
   - Changed distance multiplier from `2.2f` to `1.2f` (no longer need pivot fix)
   - Changed position calculation to use `lookAtTarget` instead of `targetPos`
   - Added comments explaining the centering logic

## Technical Details

### Why Center Instead of Feet?

**Before (Aiming at Feet):**
- Camera aimed at gizmo position (feet)
- Character appeared "floating" (feet at bottom, head near top)
- Unbalanced visual appearance

**After (Aiming at Center):**
- Camera aims at center of bounding box
- Character is vertically centered in viewport
- Balanced visual appearance
- Gizmo remains at pivot location (feet) - not moved

### Distance Multiplier Change

**Before:**
```cpp
float finalDist = dist * 2.2f;  // 2.0f for pivot fix + 0.2f buffer
```

**After:**
```cpp
float finalDist = dist * 1.2f;  // 1.2f buffer (no pivot fix needed)
```

**Why:**
- When aiming at feet, we needed `2.0f` multiplier to see the head (pivot fix)
- When aiming at center, we only need a small buffer (`1.2f`) to ensure the entire character fits
- The center is already in the middle, so no pivot fix is needed

### Gizmo Position

The gizmo position (`targetPos`) is still passed to the function but is not used for aiming or positioning:
- Gizmo remains at its pivot location (feet)
- Camera aims at center of bounding box
- Gizmo is not moved or affected by this change

## Testing

### Verification Steps

1. **Load a character model** (e.g., with feet at bottom, head at top)
2. **Press 'F'** to frame the character
3. **Verify** the character is vertically centered in the viewport
4. **Check** that the gizmo is still at the feet (pivot location)
5. **Move the mouse** - verify no jitter or jumping
6. **Verify** the entire character is visible (head and feet)

### Expected Results

- ✅ Character is vertically centered in the viewport
- ✅ No "floating" appearance
- ✅ Gizmo remains at pivot location (feet)
- ✅ Entire character is visible (head and feet)
- ✅ No jitter when moving mouse after framing
- ✅ Balanced visual appearance

## Summary

Vertically centered character framing by:

✅ **Calculate Visual Center**: `lookAtTarget = (boundingBoxMin + boundingBoxMax) * 0.5f`
✅ **Update Camera Aim**: `aimAtTarget(lookAtTarget)` instead of `aimAtTarget(targetPos)`
✅ **Maintain Correct Distance**: Keep existing distance calculation with adjusted multiplier (1.2f instead of 2.2f)
✅ **Position Update**: Use `lookAtTarget` for camera position calculation
✅ **Final Synchronization**: `aimAtTarget` already handles yaw/pitch synchronization

**Result**: When pressing 'F', the character appears perfectly centered vertically in the blue viewport area, with the Gizmo remaining at its pivot location (feet). The character no longer appears "floating" and has a balanced visual appearance.
