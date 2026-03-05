# Framing Distance Based on Bounding Box - F-Key Aim Enhancement

## Date
Created: 2024

## Overview

Enhanced the 'F' key aim constraint to include automatic distance adjustment based on the selected object's bounding box. When 'F' is pressed, the camera now:
1. Aims exactly at the Gizmo World Position (rotation)
2. Moves to an optimal distance where the object fits the screen (position)
3. Maintains perfect synchronization of yaw/pitch to prevent mouse-jump

## Objective

**Move camera to a distance where the selected object fits the screen, while still aiming exactly at the Gizmo World Position.**

## Implementation Details

### 1. Get Bounding Data (`scene.cpp`)

Access the BoundingBox object associated with the selected model:

```cpp
// Try to get bounding box for the selected node
std::string selectedNode = mOutliner.getSelectedNode();
if (!selectedNode.empty()) {
    for (size_t i = 0; i < mModelManager->getModelCount(); i++) {
        mModelManager->getNodeBoundingBox(static_cast<int>(i), selectedNode, boundingBoxMin, boundingBoxMax);
        // Check if bounding box is valid
        if (boundingBoxMin != glm::vec3(-1.0f) || boundingBoxMax != glm::vec3(1.0f)) {
            hasBoundingBox = true;
            break;
        }
    }
}

// Fallback to combined bounding box if node not found
if (!hasBoundingBox && mModelManager->getModelCount() > 0) {
    mModelManager->getCombinedBoundingBox(boundingBoxMin, boundingBoxMax);
}
```

**Key Points:**
- First tries to get bounding box for the selected node
- Falls back to combined bounding box if node not found
- Validates bounding box is not default values

### 2. Calculate Target Distance (`camera.cpp`)

Use the camera's FOV to find the distance needed to fit the bounding box from the gizmo position:

```cpp
// 1. CALCULATE TRUE RADIUS FROM GIZMO: Find the distance from the Gizmo to the furthest corner
// This accounts for the fact that the camera aims at the gizmo (pivot point), not the bounding box center
// The gizmo might be at the feet/pivot, so we need to find the furthest corner from the gizmo position
float maxDist = 0.0f;

// Calculate all 8 corners of the AABB
glm::vec3 corners[8] = {
    glm::vec3(boundingBoxMin.x, boundingBoxMin.y, boundingBoxMin.z),  // 0: min corner
    glm::vec3(boundingBoxMax.x, boundingBoxMin.y, boundingBoxMin.z),  // 1: min X, max YZ
    glm::vec3(boundingBoxMin.x, boundingBoxMax.y, boundingBoxMin.z),  // 2: min Y, max XZ
    glm::vec3(boundingBoxMax.x, boundingBoxMax.y, boundingBoxMin.z),  // 3: min Z, max XY
    glm::vec3(boundingBoxMin.x, boundingBoxMin.y, boundingBoxMax.z),  // 4: min Z, max XY
    glm::vec3(boundingBoxMax.x, boundingBoxMin.y, boundingBoxMax.z),  // 5: min Y, max XZ
    glm::vec3(boundingBoxMin.x, boundingBoxMax.y, boundingBoxMax.z),  // 6: min X, max YZ
    glm::vec3(boundingBoxMax.x, boundingBoxMax.y, boundingBoxMax.z)   // 7: max corner
};

// Find the maximum distance from gizmo position to any corner
for (int i = 0; i < 8; i++) {
    float dist = glm::length(corners[i] - targetPos);
    maxDist = glm::max(maxDist, dist);
}

// 2. ADJUST FOR VERTICAL FOV: Since the character is vertical, we must ensure the height fits
// Use 'tan' instead of 'sin' for a more accurate linear framing
float fovRadians = glm::radians(zoom);  // zoom is the FOV in degrees
float idealDistance = maxDist / tan(fovRadians / 2.0f);

// 3. ADD PADDING: Multiply the final distance by 1.5f instead of 1.2f to account for
// the fact that we are looking at the bottom of the object (gizmo at feet/pivot)
idealDistance *= 1.5f;
```

**Formula Explanation:**
- `maxDist`: Maximum distance from gizmo position to any corner of the bounding box
- `fovRadians / 2.0f`: Half the field of view angle
- `tan(fovRadians / 2.0f)`: Tangent of half FOV (more accurate for linear framing)
- `maxDist / tan(...)`: Base distance to fit the object from gizmo position
- `* 1.5f`: Padding factor (50% extra space) to account for looking at bottom of object

**Why This Works:**
- The gizmo (pivot point) is often at the feet/bottom of the character
- Calculating distance from gizmo to furthest corner ensures the entire object fits
- Using `tan` instead of `sin` provides more accurate linear framing for vertical objects
- Increased padding (1.5f) accounts for the offset between gizmo and object center

**Distance Clamping:**
```cpp
if (idealDistance < 0.1f) {
    idealDistance = 0.1f;  // Prevent too close
}
if (idealDistance > 1000.0f) {
    idealDistance = 1000.0f;  // Prevent too far
}
```

### 3. Execute Movement (`camera.cpp`)

Keep the camera's CURRENT orientation and update position:

```cpp
// 3. EXECUTE MOVEMENT (ONE-SHOT ON 'F'): Keep the camera's CURRENT orientation
// First, aim at the target (this updates yaw, pitch, and cameraFront)
bool aimComplete = aimAtTarget(targetPos);

// 4. EXECUTE MOVEMENT: Update position to ideal distance while maintaining aim direction
// camera.position = gizmoWorldPos - (camera.forward * idealDistance)
cameraPos = targetPos - cameraFront * idealDistance;

// Update orbit distance to match the new distance
orbitDistance = idealDistance;
```

**Key Points:**
- First calls `aimAtTarget()` to set orientation (yaw, pitch, cameraFront)
- Then moves camera to ideal distance along the aim direction
- Maintains the aim direction while adjusting distance
- Updates orbit distance to match

### 4. Maintain Synchronization (`camera.cpp`)

After moving the camera, ensure yaw and pitch are still perfectly synced:

```cpp
// 5. MAINTAIN SYNCHRONIZATION: Ensure camera.m_yaw and camera.m_pitch are still
// perfectly synced with the forward vector to prevent mouse-jump
glm::vec3 direction;
direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
direction.y = sin(glm::radians(pitch));
direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
direction = glm::normalize(direction);

// Ensure camera position is exactly at the calculated distance
cameraPos = targetPos - direction * idealDistance;
cameraFront = direction;  // Ensure forward vector matches the direction

// Update camera vectors to ensure consistency
cameraRight = glm::normalize(glm::cross(cameraFront, worldUp));
cameraUp = glm::normalize(glm::cross(cameraRight, cameraFront));
```

**Key Points:**
- Recalculates direction from yaw/pitch to ensure consistency
- Updates camera position to exact distance
- Updates camera vectors (cameraFront, cameraRight, cameraUp)
- Prevents mouse-jump by maintaining perfect synchronization

### 5. Termination (`scene.cpp`)

Set `m_isFraming = false` immediately after position and rotation are updated:

```cpp
// 5. TERMINATION: All state is synchronized:
// - yaw and pitch match the aim direction
// - focusPoint is set to gizmo world position
// - camera position is at ideal distance
// - orbitDistance matches ideal distance
// - camera vectors are consistent
if (aimComplete) {
    camera->setFraming(false);  // Aim complete, release lock
}
```

**Key Points:**
- Framing flag is only released after all state is synchronized
- All camera state (position, rotation, vectors) is consistent
- Manual mouse rotation will use synchronized state → No jump

## Code Changes

### Files Modified

1. **`src/io/camera.h`**:
   - Added `aimAtTargetWithFraming()` method declaration

2. **`src/io/camera.cpp`**:
   - Implemented `aimAtTargetWithFraming()` method
   - Calculates bounding sphere radius from bounding box
   - Calculates ideal distance using FOV
   - Moves camera to ideal distance while maintaining aim direction
   - Ensures state synchronization

3. **`src/io/scene.cpp`**:
   - Modified `applyCameraAimConstraint()` to get bounding box
   - Calls `aimAtTargetWithFraming()` when bounding box is available
   - Falls back to `aimAtTarget()` if no bounding box found

## Technical Details

### Bounding Sphere Calculation

The bounding box is converted to a bounding sphere for distance calculation:

```cpp
radius = max(maxSize.x, maxSize.y, maxSize.z) * 0.5f
```

This ensures the entire object fits within the view frustum, even if it's not a perfect sphere.

### Distance Formula

The ideal distance is calculated using the perspective projection formula:

```
idealDistance = (radius / sin(fov / 2)) * padding
```

**Why this works:**
- In perspective projection, objects at distance `d` with radius `r` appear at size `2 * d * tan(fov/2)`
- To fit radius `r` in view: `r = d * tan(fov/2)`, so `d = r / tan(fov/2)`
- Using `sin` instead of `tan` provides a more conservative estimate
- Padding factor (1.2) adds 20% extra space for comfortable viewing

### State Synchronization Flow

```
aimAtTargetWithFraming() called
    ↓
Get bounding box min/max
    ↓
Calculate radius = maxSize * 0.5f
    ↓
Calculate idealDistance = (radius / sin(fov/2)) * 1.2f
    ↓
Call aimAtTarget() → Updates yaw, pitch, cameraFront, focusPoint
    ↓
Move camera to ideal distance: cameraPos = targetPos - cameraFront * idealDistance
    ↓
Recalculate direction from yaw/pitch for consistency
    ↓
Update camera position to exact distance
    ↓
Update camera vectors (cameraFront, cameraRight, cameraUp)
    ↓
All state synchronized:
    - yaw and pitch match aim direction
    - focusPoint is gizmo position
    - camera position is at ideal distance
    - orbitDistance matches ideal distance
    - camera vectors are consistent
    ↓
Release framing flag (m_isFraming = false)
    ↓
Manual mouse rotation uses synchronized state → No jump!
```

## Benefits

### 1. Automatic Framing
- Camera automatically moves to optimal distance
- Object fits perfectly in viewport
- No manual zooming required

### 2. Maintains Aim Direction
- Camera still aims exactly at gizmo world position
- Rotation is preserved while adjusting distance
- Perfect centering maintained

### 3. No Mouse Jump
- Yaw and pitch are perfectly synchronized
- Camera state is consistent
- Smooth transition to manual rotation

### 4. Fallback Behavior
- If bounding box not available, falls back to rotation-only aim
- Works with or without bounding box data
- Graceful degradation

## Testing

### Verification Steps

1. **Load a model** and select a node (gizmo appears)
2. **Press 'F'** to aim at gizmo
3. **Verify centering**: Gizmo should appear exactly at viewport center
4. **Verify distance**: Object should fit nicely in viewport (with padding)
5. **Move mouse slightly** (without pressing buttons) → No jump should occur
6. **Start manual rotation** (Alt + mouse drag) → Should start smoothly from current orientation
7. **Test with different objects**: Distance should adjust based on object size

### Expected Results

- ✅ Gizmo appears centered when 'F' is pressed
- ✅ Camera moves to optimal distance based on bounding box
- ✅ Object fits nicely in viewport with padding
- ✅ No jump when mouse is moved after aiming
- ✅ Manual rotation starts smoothly from current orientation
- ✅ Works with node bounding box or combined bounding box
- ✅ Falls back gracefully if bounding box not available

## Summary

Implemented framing distance based on bounding box by:

✅ **Get Bounding Data**: Access bounding box for selected node or combined bounding box
✅ **Calculate Target Distance**: Use FOV and bounding sphere radius to calculate ideal distance
✅ **Execute Movement**: Move camera to ideal distance while maintaining aim direction
✅ **Maintain Synchronization**: Ensure yaw/pitch are perfectly synced to prevent mouse-jump
✅ **Clean Termination**: Release framing flag only after all state is synchronized

**Result**: When 'F' is pressed, the camera aims at the gizmo AND moves to an optimal distance where the object fits the screen, with perfect state synchronization for smooth manual rotation.
