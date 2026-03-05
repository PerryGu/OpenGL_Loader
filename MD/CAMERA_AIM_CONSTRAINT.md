# Camera Aim Constraint Implementation

## Date
Created: 2024

## Overview

This document describes the implementation of the camera aim constraint feature, which forces the camera to automatically rotate and look directly at the Gizmo's world position. The constraint only affects camera rotation/orientation, not position.

## Objective

**Force camera to rotate and look directly at the Gizmo.**
- **Constraint**: Rotation ONLY. Do not modify camera.position.
- **Target**: Gizmo World Position.

## Implementation Details

### 1. Camera Aim Method (`src/io/camera.h` / `camera.cpp`)

Added `aimAtTarget()` method to the Camera class:

```cpp
void Camera::aimAtTarget(glm::vec3 targetPos);
```

**Functionality:**
- Calculates forward vector from camera position to target position
- Rebuilds camera basis vectors (forward, right, up) while keeping position static
- Updates yaw and pitch from new forward direction for consistency
- Updates focus point to maintain orbit camera behavior
- Handles edge cases (target too close, camera looking straight up/down)

**Key Features:**
- **Position Preserved**: Camera position (`cameraPos`) is never modified
- **Orientation Updated**: Only `cameraFront`, `cameraRight`, `cameraUp` vectors are updated
- **Gimbal Lock Prevention**: Clamps pitch to prevent gimbal lock
- **Edge Case Handling**: Handles cases where target is too close or forward is parallel to world up

### 2. Gizmo World Position Tracking (`src/io/scene.h` / `scene.cpp`)

Added member variables to Scene class to track gizmo world position:

```cpp
glm::vec3 mGizmoWorldPosition = glm::vec3(0.0f);  // Current gizmo world position
bool mGizmoPositionValid = false;  // True if gizmo position is valid (gizmo is visible)
```

**Position Update:**
- Gizmo world position is extracted from `modelMatrixArray` after ImGuizmo manipulation
- Position is updated every frame when gizmo is visible
- Position validity flag is set to `false` when gizmo is not visible (no node selected, no model loaded, etc.)

**Location:** Updated in `renderViewportWindow()` after `ImGuizmo::Manipulate()` call.

### 3. Camera Aim Constraint Method (`src/io/scene.h` / `scene.cpp`)

Added `applyCameraAimConstraint()` method to Scene class:

```cpp
void Scene::applyCameraAimConstraint(Camera* camera);
```

**Functionality:**
1. **Check Gizmo Visibility**: Only applies constraint if `mGizmoPositionValid` is true
2. **Mouse Override**: If right mouse button is pressed, bypasses constraint to allow manual orbit
3. **Apply Constraint**: Calls `camera->aimAtTarget(mGizmoWorldPosition)` to force camera to look at gizmo

**Mouse Override Logic:**
```cpp
// MOUSE OVERRIDE: If right mouse button is pressed, bypass constraint to allow manual orbit
if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
    return;  // User is manually controlling camera, don't apply constraint
}
```

This allows users to manually orbit the camera even when gizmo is visible by holding the right mouse button.

### 4. Main Loop Integration (`src/main.cpp`)

Added call to apply camera aim constraint in the main loop:

```cpp
//-- render ----------------
scene.update();

//-- Apply camera aim constraint (forces camera to look at gizmo) -------------------
// This is called after scene.update() so gizmo position is up-to-date
// The constraint only applies when gizmo is visible and right mouse is not pressed
scene.applyCameraAimConstraint(&cameras[activeCam]);
```

**Timing:**
- Called after `scene.update()` to ensure gizmo position is up-to-date
- Called before view matrix calculation for rendering
- Applied every frame when conditions are met

## Behavior

### When Constraint is Active

1. **Gizmo is Visible**: A node is selected and gizmo is rendered
2. **Right Mouse Not Pressed**: User is not manually controlling camera
3. **Camera Rotates**: Camera automatically rotates to look at gizmo world position
4. **Position Unchanged**: Camera position remains static, only orientation changes

### When Constraint is Inactive

1. **No Gizmo**: No node selected or no model loaded
2. **Manual Override**: Right mouse button is pressed (allows manual orbit)
3. **Normal Camera Control**: Camera behaves normally (Maya-style orbit controls)

## Technical Details

### Coordinate System

- **World Space**: Gizmo position is in world space (accounts for all parent transformations)
- **Camera Space**: Camera orientation is updated in world space
- **Basis Vectors**: Forward, right, and up vectors are recalculated to form orthonormal basis

### Edge Cases Handled

1. **Target Too Close**: If distance to target < 0.001f, constraint is skipped
2. **Gimbal Lock**: Pitch is clamped to [-89°, 89°] to prevent gimbal lock
3. **Forward Parallel to World Up**: Uses default right vector and recalculates forward
4. **Gizmo Not Visible**: Constraint is automatically disabled when gizmo is not visible

### Integration with Existing Systems

- **Orbit Camera**: Maintains orbit camera behavior by updating focus point
- **Maya-Style Controls**: Works alongside existing Alt+mouse camera controls
- **Gizmo System**: Integrates seamlessly with ImGuizmo manipulation system
- **Viewport Selection**: Does not interfere with viewport selection or gizmo manipulation

## Code Organization

### Files Modified

1. **`src/io/camera.h`**: Added `aimAtTarget()` method declaration
2. **`src/io/camera.cpp`**: Implemented `aimAtTarget()` method
3. **`src/io/scene.h`**: Added gizmo position tracking and constraint method
4. **`src/io/scene.cpp`**: Implemented gizmo position tracking and constraint logic
5. **`src/main.cpp`**: Added constraint application in main loop

### Code Principles Followed

- **Modularity**: Camera aim logic is in Camera class, constraint logic is in Scene class
- **Separation of Concerns**: Position tracking separate from constraint application
- **Documentation**: Inline comments explain why, not just what
- **Action-Based**: No frame-based debug prints (follows project guidelines)

## Usage

### Automatic Behavior

The constraint is automatically applied when:
- A node is selected in the outliner
- Gizmo is visible in the viewport
- Right mouse button is not pressed

### Manual Override

To manually control the camera while gizmo is visible:
- Hold right mouse button (Alt + Right Mouse for Maya-style dolly)
- Constraint is automatically bypassed
- Release right mouse to resume automatic aiming

## Future Enhancements

### Potential Improvements

1. **Smoothing**: Add interpolation/smoothing for camera rotation (prevent jittery movement)
2. **Distance-Based**: Only apply constraint when camera is within certain distance of gizmo
3. **Toggle Option**: Add UI option to enable/disable constraint
4. **Speed Control**: Add parameter to control how quickly camera rotates to target
5. **Offset Angle**: Allow slight offset from direct aim (for better viewing angles)

## Testing Checklist

When testing the camera aim constraint:

- [ ] Camera rotates to look at gizmo when node is selected
- [ ] Camera position does not change (only rotation)
- [ ] Right mouse button bypasses constraint (allows manual orbit)
- [ ] Constraint is disabled when no node is selected
- [ ] Constraint is disabled when no model is loaded
- [ ] Camera smoothly follows gizmo when gizmo is moved
- [ ] No jittery or erratic camera movement
- [ ] Works with all gizmo operations (translate, rotate, scale)
- [ ] Does not interfere with viewport selection
- [ ] Does not interfere with gizmo manipulation

## Summary

The camera aim constraint feature automatically rotates the camera to look at the Gizmo's world position, providing a focused view of the object being manipulated. The constraint only affects camera rotation, preserving camera position, and can be overridden by holding the right mouse button for manual camera control.

**Key Points:**
- ✅ Rotation only (position unchanged)
- ✅ Automatic when gizmo is visible
- ✅ Manual override with right mouse button
- ✅ Seamless integration with existing systems
- ✅ Follows project code organization principles
