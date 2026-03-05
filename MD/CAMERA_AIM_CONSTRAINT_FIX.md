# Camera Aim Constraint - Build Fix & Implementation

## Date
Created: 2024

## Summary

Fixed build errors and implemented camera aim constraint that automatically rotates the camera to look at the Gizmo's world position. The constraint only affects camera rotation/orientation, not position.

## Build Errors Fixed

### 1. Constructor Member Initializers
**Issue:** Constructor was initializing member variables that don't exist in the Camera class:
- `isInterpolating`
- `interpolationTime`
- `interpolationDuration`
- `startPosition`, `targetPosition`
- `startFocusPoint`, `targetFocusPoint`

**Fix:** Removed these unused initializers from the constructor.

### 2. Undeclared Methods
**Issue:** Methods `frameSmoothly`, `updateSmoothInterpolation`, and `centerTargetInViewport` were using undeclared member variables and causing compilation errors.

**Fix:** Commented out these unused methods (they're not declared in `camera.h` and appear to be dead code).

### 3. Private Method Access
**Issue:** `applyCameraAimConstraint` was declared in the private section of Scene class.

**Fix:** Moved `applyCameraAimConstraint` to the public section in `scene.h`.

## Implementation Details

### Camera Aim Method (`camera.cpp`)

The `aimAtTarget()` method implements rotation-only camera aiming:

```cpp
void Camera::aimAtTarget(glm::vec3 targetPos) {
    // 1. GET TARGET: Use absolute World Position of Gizmo
    // 2. CALCULATE FORWARD VECTOR: direction from camera to target
    glm::vec3 aimDir = glm::normalize(targetPos - cameraPos);
    
    // 3. REBUILD BASIS (STAY IN PLACE): Update orientation only
    cameraFront = aimDir;
    cameraRight = glm::normalize(glm::cross(cameraFront, worldUp));
    cameraUp = glm::normalize(glm::cross(cameraRight, cameraFront));
    
    // Update yaw/pitch for consistency
    // Update focus point for orbit camera behavior
}
```

**Key Features:**
- ✅ Uses correct member variables: `cameraPos`, `cameraFront`, `cameraRight`, `cameraUp`, `worldUp`, `yaw`, `pitch`, `focusPoint`, `orbitDistance`
- ✅ Rotation only: `cameraPos` is never modified
- ✅ Handles edge cases: target too close, forward parallel to world up
- ✅ Prevents gimbal lock: clamps pitch to [-89°, 89°]

### Scene Constraint Application (`scene.cpp`)

The `applyCameraAimConstraint()` method applies the constraint with safety checks:

```cpp
void Scene::applyCameraAimConstraint(Camera* camera) {
    // Only apply if gizmo is visible
    if (!mGizmoPositionValid) {
        return;
    }
    
    // MOUSE OVERRIDE: Right mouse button bypasses constraint
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        return;  // Allow manual camera control
    }
    
    // Apply aim constraint
    camera->aimAtTarget(mGizmoWorldPosition);
}
```

**Safety Features:**
- ✅ Only applies when gizmo is visible (`mGizmoPositionValid`)
- ✅ Right mouse button override for manual camera control
- ✅ Gizmo world position is updated every frame when gizmo is visible

### Gizmo Position Tracking

Gizmo world position is extracted and stored every frame:

```cpp
// In renderViewportWindow(), after ImGuizmo::Manipulate()
glm::mat4 currentModelMatrix = float16ToGlmMat4(modelMatrixArray);
glm::vec3 gizmoWorldPos;
glm::decompose(currentModelMatrix, ..., gizmoWorldPos, ...);
mGizmoWorldPosition = gizmoWorldPos;
mGizmoPositionValid = true;
```

## GLM Includes

All necessary GLM headers are included in `camera.h`:
- `#include <glm/glm.hpp>` - Core GLM types and functions
- `#include <glm/gtc/matrix_transform.hpp>` - Matrix transformations
- `#include <glm/gtx/quaternion.hpp>` - Quaternion support

## Integration

The constraint is applied automatically in the main loop:

```cpp
// In main.cpp, after scene.update()
scene.applyCameraAimConstraint(&cameras[activeCam]);
```

**Timing:**
- Called after `scene.update()` to ensure gizmo position is up-to-date
- Called before view matrix calculation for rendering
- Applied every frame when conditions are met

## Behavior

### When Active
- Gizmo is visible (node selected, model loaded)
- Right mouse button is NOT pressed
- Camera automatically rotates to look at gizmo
- Camera position remains unchanged

### When Inactive
- No gizmo visible (no node selected or no model loaded)
- Right mouse button is pressed (manual camera control)
- Normal camera behavior (Maya-style orbit controls)

## Member Variables Used

All member variables are correctly named and accessible:

| Variable | Type | Usage |
|----------|------|-------|
| `cameraPos` | `glm::vec3` | Camera position (never modified) |
| `cameraFront` | `glm::vec3` | Forward direction vector |
| `cameraRight` | `glm::vec3` | Right direction vector |
| `cameraUp` | `glm::vec3` | Up direction vector |
| `worldUp` | `glm::vec3` | World up reference (0, 1, 0) |
| `yaw` | `float` | Horizontal rotation angle |
| `pitch` | `float` | Vertical rotation angle |
| `focusPoint` | `glm::vec3` | Orbit camera focus point |
| `orbitDistance` | `float` | Distance from camera to focus |

## Testing

To verify the implementation:

1. **Load a model** - Gizmo should appear when node is selected
2. **Camera should rotate** - Camera automatically looks at gizmo
3. **Position unchanged** - Camera position stays the same
4. **Right mouse override** - Hold right mouse to manually control camera
5. **No gizmo** - When no node selected, constraint is inactive

## Files Modified

1. **`src/io/camera.h`**: Added `aimAtTarget()` declaration
2. **`src/io/camera.cpp`**: 
   - Fixed constructor (removed unused initializers)
   - Implemented `aimAtTarget()` method
   - Commented out unused methods
3. **`src/io/scene.h`**: 
   - Added gizmo position tracking members
   - Moved `applyCameraAimConstraint()` to public section
4. **`src/io/scene.cpp`**: 
   - Added gizmo position tracking
   - Implemented `applyCameraAimConstraint()` method
5. **`src/main.cpp`**: Added constraint application in main loop

## Summary

✅ **Build errors fixed** - All compilation errors resolved
✅ **Correct member variables** - Using proper Camera class members
✅ **GLM includes correct** - All necessary headers included
✅ **Right mouse override** - Manual camera control when needed
✅ **Rotation only** - Camera position never modified
✅ **Automatic aiming** - Works when gizmo is visible

The camera now automatically rotates to look at the Gizmo's world position while maintaining its position, with a right mouse button override for manual control.
