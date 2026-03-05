# Fix Camera Aiming at World Origin (Coordinate Space Fix)

## Date
Created: 2024

## Overview

Fixed the camera aiming issue where pressing 'F' focused on (0,0,0) instead of the character's current position. The problem was that the AABB center calculation was using Local Space coordinates instead of World Space coordinates.

## Problem

**Pressing 'F' focuses on (0,0,0) instead of the character's current position.**

This happened because:
- The bounding box returned by `getNodeBoundingBox` is in **Local Space** (model space)
- The center calculation `(boundingBoxMin + boundingBoxMax) * 0.5f` was using local-space coordinates
- The camera was aiming at the local-space center, which is relative to the model's origin, not its world position
- When the model is moved/rotated/scaled via RootNode transform, the local-space center doesn't match the world-space center

## Solution

### 1. Get World-Space AABB (`scene.cpp`)

Transform the bounding box from Local Space to World Space using the model's RootNode transform:

```cpp
// CRITICAL FIX: Transform bounding box to world space using this model's RootNode transform
// The bounding box is calculated in local space, but the model is rendered in world space
// with RootNode transform applied. We must transform the bounding box to match.
if (!modelRootNodeName.empty()) {
    // Get RootNode transform matrix from PropertyPanel
    const auto& allBoneTranslations = mPropertyPanel.getAllBoneTranslations();
    const auto& allBoneRotations = mPropertyPanel.getAllBoneRotations();
    const auto& allBoneScales = mPropertyPanel.getAllBoneScales();
    
    // Build RootNode transform matrix
    glm::mat4 rootNodeTransform = glm::mat4(1.0f);
    rootNodeTransform = glm::translate(rootNodeTransform, rootTranslation);
    rootNodeTransform = rootNodeTransform * glm::mat4_cast(rootRotation);
    rootNodeTransform = glm::scale(rootNodeTransform, rootScale);
    
    // Transform all 8 corners of the bounding box from local space to world space
    glm::vec3 corners[8] = { /* 8 corners */ };
    
    // Transform all corners to world space using RootNode transform
    for (int j = 0; j < 8; j++) {
        corners[j] = glm::vec3(rootNodeTransform * glm::vec4(corners[j], 1.0f));
    }
    
    // Find new min/max in world space (axis-aligned bounding box in world space)
    boundingBoxMin = boundingBoxMax = corners[0];
    for (int j = 1; j < 8; j++) {
        boundingBoxMin = glm::min(boundingBoxMin, corners[j]);
        boundingBoxMax = glm::max(boundingBoxMax, corners[j]);
    }
}
```

**Key Points:**
- Get RootNode transform from PropertyPanel (translation, rotation, scale)
- Build transform matrix: `T * R * S` (translate, rotate, scale)
- Transform all 8 corners of the AABB from local space to world space
- Recalculate min/max in world space to get axis-aligned bounding box

### 2. Calculate True World Center (`camera.cpp`)

The center calculation now uses world-space coordinates:

```cpp
// 1. CALCULATE VISUAL CENTER: Instead of aiming at the Gizmo position (feet), calculate the center of the World-Space Bounding Box
// This centers the character vertically in the viewport, avoiding the "floating" look
glm::vec3 lookAtTarget = (boundingBoxMin + boundingBoxMax) * 0.5f;
```

**Key Points:**
- `boundingBoxMin` and `boundingBoxMax` are now in world space
- Center calculation gives the true world-space center
- Camera aims at the character's actual position in the world

### 3. Verify Gizmo Offset

The gizmo world position (`mGizmoWorldPosition`) is already in world space:
- Extracted from the model matrix after manipulation
- Already accounts for RootNode transform
- Used as fallback if bounding box is not available

### 4. Force Update

The `aimAtTarget` method ensures the camera forward vector is normalized and points directly at the target:

```cpp
// 2. UPDATE CAMERA AIM: Point the camera's forward vector at this lookAtTarget
bool aimComplete = aimAtTarget(lookAtTarget);
```

**Key Points:**
- `aimAtTarget` calculates direction: `aimDir = normalize(lookAtTarget - camera.position)`
- Updates `cameraFront` to point at the target
- Synchronizes yaw, pitch, and focusPoint

### 5. No Lag

The calculation happens after the model's transform has been updated:
- RootNode transform is read from PropertyPanel (updated every frame)
- Bounding box transformation uses current transform values
- No caching or delayed updates

## Code Changes

### Files Modified

1. **`src/io/scene.cpp`**:
   - Added world-space transformation for node bounding box
   - Added world-space transformation for combined bounding box
   - Transforms all 8 corners of AABB from local space to world space
   - Recalculates min/max in world space
   - Uses RootNode transform from PropertyPanel

## Technical Details

### Local Space vs World Space

**Before (Local Space):**
- Bounding box in local space: `(min, max)` relative to model origin
- Center calculation: `(min + max) * 0.5f` gives local-space center
- Camera aims at local-space center, which is at (0,0,0) if model is at origin
- When model moves, local-space center doesn't change

**After (World Space):**
- Bounding box transformed to world space using RootNode transform
- Center calculation: `(worldMin + worldMax) * 0.5f` gives world-space center
- Camera aims at world-space center, which matches the character's actual position
- When model moves, world-space center moves with it

### RootNode Transform

The RootNode transform includes:
- **Translation**: Model position in world space
- **Rotation**: Model rotation in world space
- **Scale**: Model scale in world space

This transform is applied to all vertices during rendering, so the bounding box must also be transformed to match.

### Corner Transformation

Transforming all 8 corners ensures accuracy:
- AABB in local space may not be axis-aligned in world space after rotation
- Transforming corners and recalculating min/max gives axis-aligned bounding box in world space
- This is the same approach used in `main.cpp` for selection/raycasting

## Testing

### Verification Steps

1. **Load a character model** at world origin (0, 0, 0)
2. **Press 'F'** - verify camera centers on character
3. **Move character** to (10, 5, 10) using PropertyPanel translation
4. **Press 'F'** - verify camera centers on character at new position
5. **Rotate character** using PropertyPanel rotation
6. **Press 'F'** - verify camera centers on character (world-space center)
7. **Scale character** using PropertyPanel scale
8. **Press 'F'** - verify camera centers on character (scaled bounding box)

### Expected Results

- ✅ Camera centers on character at current world position
- ✅ Works regardless of character's position in world
- ✅ Works with rotation (world-space center is correct)
- ✅ Works with scale (bounding box is scaled correctly)
- ✅ No longer focuses on (0,0,0) when character is moved

## Summary

Fixed camera aiming at world origin by:

✅ **Get World-Space AABB**: Transform bounding box from local space to world space using RootNode transform
✅ **Calculate True World Center**: Use world-space min/max for center calculation
✅ **Verify Gizmo Offset**: Gizmo world position already in world space (used as fallback)
✅ **Force Update**: `aimAtTarget` ensures camera forward points at target
✅ **No Lag**: Calculation uses current RootNode transform values

**Result**: Pressing 'F' now centers the camera on the character wherever they are in the world, not at the grid center (0,0,0). The camera correctly aims at the character's world-space position.
