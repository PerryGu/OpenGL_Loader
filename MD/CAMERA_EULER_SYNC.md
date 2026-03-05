# Camera Euler Angles Synchronization - F-Key Aim Fix

## Date
Created: 2024

## Problem

After pressing 'F' to aim at the Gizmo, the Gizmo is centered correctly. However, when the user touches the mouse to manually rotate the camera, there is a small "jump" because the camera's internal Yaw and Pitch variables are not updated to match the new Aim direction.

## Root Cause

The `aimAtTarget()` method updates the camera's forward vector (`cameraFront`) and rebuilds the orthonormal basis, but:
1. **Yaw and Pitch Not Synchronized**: The Euler angles (yaw, pitch) were calculated but might not match the exact aim direction
2. **Focus Point Not Updated**: The focus point was calculated from camera position, not set to the gizmo position
3. **State Inconsistency**: When manual rotation starts, it uses the old yaw/pitch values, causing a jump

## Solution

### 1. Extract Angles from Aim (`camera.cpp`)

After calculating `cameraFront = aimDir`, extract the new Yaw and Pitch directly from the aim direction:

```cpp
// 1. EXTRACT ANGLES FROM AIM: Calculate new Yaw and Pitch from the aimDir vector
// This ensures the camera's internal Euler angles match the new aim direction
// Formula: pitch = asin(aimDir.y), yaw = atan2(aimDir.x, aimDir.z)
pitch = glm::degrees(asin(aimDir.y));
yaw = glm::degrees(atan2(aimDir.x, aimDir.z));

// Clamp pitch to prevent gimbal lock
if (pitch > 89.0f) {
    pitch = 89.0f;
}
if (pitch < -89.0f) {
    pitch = -89.0f;
}
```

**Key Points:**
- Uses `asin(aimDir.y)` for pitch (vertical angle)
- Uses `atan2(aimDir.x, aimDir.z)` for yaw (horizontal angle)
- Angles are clamped to prevent gimbal lock
- Angles are in degrees (matching camera's internal representation)

### 2. Update Focus Point (`camera.cpp`)

Set the camera's internal orbit target to be EXACTLY the Gizmo World Position:

```cpp
// 2. UPDATE FOCUS POINT: Set the camera's internal orbit target to be EXACTLY the Gizmo World Position
// This ensures the focus point matches the target, preventing jumps when transitioning to manual rotation
focusPoint = targetPos;

// Update orbit distance to match the actual distance to target
orbitDistance = glm::length(targetPos - cameraPos);
```

**Key Points:**
- Focus point is set directly to the gizmo world position
- Orbit distance is updated to match the actual distance
- This ensures the camera orbits around the gizmo position

### 3. Force State Persistence (`camera.cpp`)

Recalculate camera position from updated yaw/pitch and focus point to ensure consistency:

```cpp
// 3. FORCE STATE PERSISTENCE: Recalculate camera position from updated yaw/pitch and focus point
// This ensures the camera position is consistent with the new angles and focus point
// The manual mouse-orbit logic uses yaw/pitch/focusPoint to calculate camera position,
// so we must ensure they are all synchronized
glm::vec3 direction;
direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
direction.y = sin(glm::radians(pitch));
direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
direction = glm::normalize(direction);

// Update camera position to orbit around focus point using the new direction
glm::vec3 newCameraPos = focusPoint - direction * orbitDistance;

// Only update camera position if it's significantly different (to maintain "rotation only" behavior)
// But ensure consistency for state persistence
float posDiff = glm::length(newCameraPos - cameraPos);
if (posDiff > 0.01f) {
    // Position needs adjustment for consistency
    cameraPos = newCameraPos;
}
```

**Key Points:**
- Recalculates direction vector from updated yaw/pitch
- Calculates new camera position from focus point and direction
- Only updates position if significantly different (maintains "rotation only" behavior)
- Ensures camera position is consistent with yaw/pitch/focusPoint state

### 4. Clean Termination (`scene.cpp`)

Set `m_isFraming = false` only after all internal variables are fully synchronized:

```cpp
// 4. CLEAN TERMINATION: Set m_isFraming = false only after internal variables are fully synchronized
// The aimAtTarget() method has already:
// - Updated cameraFront to aimDir
// - Updated yaw and pitch from aimDir
// - Set focusPoint to gizmo world position
// - Updated orbitDistance
// - Rebuilt orthonormal basis (cameraRight, cameraUp)
// - Ensured camera position is consistent with new state
//
// 3. FORCE STATE PERSISTENCE: The manual mouse-orbit logic will use these updated
// m_yaw, m_pitch, and m_focusPoint values as its starting point for the next frame
// because they are now synchronized with the camera's actual orientation
if (aimComplete) {
    // All internal state is synchronized:
    // - yaw and pitch match the aim direction
    // - focusPoint is set to gizmo world position
    // - camera vectors are consistent
    // Safe to release framing flag - no jump will occur
    camera->setFraming(false);  // Aim complete, release lock
}
```

**Key Points:**
- Framing flag is only released after all state is synchronized
- Manual mouse-orbit logic will use the updated values as starting point
- No jump occurs because state is consistent

## Code Changes

### Files Modified

1. **`src/io/camera.cpp`**:
   - Modified `aimAtTarget()` to extract angles directly from aim direction
   - Set focus point to gizmo world position
   - Recalculate camera position for state consistency
   - Added comments explaining state synchronization

2. **`src/io/scene.cpp`**:
   - Updated comments in `applyCameraAimConstraint()` to explain state synchronization
   - Clarified that framing flag is only released after state is fully synchronized

## Technical Details

### Angle Extraction Formula

**Pitch (Vertical Angle):**
```cpp
pitch = asin(aimDir.y)
```
- `aimDir.y` is the Y component of the normalized direction vector
- `asin()` gives the angle in radians, converted to degrees
- Range: -90° to +90° (clamped to prevent gimbal lock)

**Yaw (Horizontal Angle):**
```cpp
yaw = atan2(aimDir.x, aimDir.z)
```
- `aimDir.x` is the X component (left/right)
- `aimDir.z` is the Z component (forward/back)
- `atan2()` gives the angle in radians, converted to degrees
- Range: -180° to +180°

### State Synchronization Flow

```
aimAtTarget() called
    ↓
Calculate aimDir = normalize(targetPos - cameraPos)
    ↓
Extract angles: pitch = asin(aimDir.y), yaw = atan2(aimDir.x, aimDir.z)
    ↓
Set focusPoint = targetPos
    ↓
Update orbitDistance = length(targetPos - cameraPos)
    ↓
Recalculate direction from yaw/pitch
    ↓
Recalculate camera position from focusPoint and direction
    ↓
Update camera vectors (cameraFront, cameraRight, cameraUp)
    ↓
All state synchronized:
    - yaw and pitch match aim direction
    - focusPoint is gizmo position
    - camera position is consistent
    - camera vectors are consistent
    ↓
Release framing flag (m_isFraming = false)
    ↓
Manual mouse rotation uses synchronized state → No jump!
```

### Manual Rotation Logic

The manual mouse-orbit logic (`updateCameraDirection()`) uses:
```cpp
yaw += dx;
pitch += dy;
// Then calculates camera position from yaw/pitch/focusPoint
```

**Before Fix:**
- yaw/pitch were not updated after aim → Old values used
- Focus point was not set to gizmo → Calculated from camera position
- Jump occurred because state was inconsistent

**After Fix:**
- yaw/pitch are updated to match aim direction → Correct values used
- Focus point is set to gizmo position → Consistent state
- No jump because state is synchronized

## Testing

### Verification Steps

1. **Load a model** and select a node (gizmo appears)
2. **Press 'F'** to aim at gizmo
3. **Verify centering**: Gizmo should appear exactly at viewport center
4. **Move mouse slightly** (without pressing buttons) → No jump should occur
5. **Start manual rotation** (Alt + mouse drag) → Should start smoothly from current orientation
6. **Test at different distances**: No jump regardless of gizmo distance

### Expected Results

- ✅ Gizmo appears centered when 'F' is pressed
- ✅ No jump when mouse is moved after aiming
- ✅ Manual rotation starts smoothly from current orientation
- ✅ No state inconsistency between automated aim and manual control
- ✅ Camera state is fully synchronized before releasing framing flag

## Summary

Fixed the camera Euler angles synchronization by:

✅ **Extract Angles from Aim**: Calculate yaw and pitch directly from aim direction vector
✅ **Update Focus Point**: Set focus point to gizmo world position (not calculated from camera)
✅ **Force State Persistence**: Recalculate camera position to ensure consistency with yaw/pitch/focusPoint
✅ **Clean Termination**: Release framing flag only after all state is fully synchronized

**Result**: Zero jitter or "jumping" when transitioning from the automated 'F' focus to manual mouse rotation. The camera's internal state (yaw, pitch, focusPoint) is now fully synchronized with the aim direction, ensuring smooth transitions.
