# Gizmo Scale Compensation Fix

## Overview
This document describes the fix for gizmo movement not matching visual movement 1:1 when the object (Rig_GRP) has a local scale in the FBX file (e.g., 0.01).

## Date
Implementation Date: 2024

## Problem Statement

When manipulating Rig_GRP with the gizmo:
- **Gizmo moves**: 32.3268 units in world space
- **Visual movement**: 0.323268 units (100x smaller than gizmo)
- **Root cause**: Rig_GRP has a local scale of 0.01 in the FBX file, which scales down the translation

### Why This Happens

1. Gizmo provides world-space transform (32.3268, 0, 0)
2. We convert to parent's local space (32.3268, 0, 0) - same since parent is identity
3. We store this in PropertyPanel (32.3268, 0, 0)
4. When rendering, if Rig_GRP's FBX scale (0.01) is applied, the translation gets scaled: 32.3268 * 0.01 = 0.323268
5. Result: Visual movement is 100x smaller than gizmo movement

## Solution

### Two-Part Fix

#### 1. Compensate in Gizmo Manipulation (`src/io/scene.cpp`)

When gizmo is manipulated and we convert world space to local space:
1. Convert world space to parent's local space using `WorldToLocalMatrix()`
2. Get Rig_GRP's FBX local scale (0.01, 0.01, 0.01)
3. **Compensate by dividing translation by the scale**
4. Store compensated value in PropertyPanel

**Code:**
```cpp
// Convert world space to parent's local space
glm::mat4 localMatrix = WorldToLocalMatrix(newModelMatrix, parentObject);
glm::decompose(localMatrix, localScale, localRotation, localTranslation, ...);

// Get Rig_GRP's FBX local scale
ofbx::Vec3 rigLocalScaling = rigRootObject->getLocalScaling();
glm::vec3 rigLocalScale(rigLocalScaling.x, rigLocalScaling.y, rigLocalScaling.z);

// Compensate for object's local scale
glm::vec3 compensatedTranslation = localTranslation;
compensatedTranslation.x /= rigLocalScale.x;  // 32.3268 / 0.01 = 3232.68
compensatedTranslation.y /= rigLocalScale.y;
compensatedTranslation.z /= rigLocalScale.z;

// Store compensated value
mPropertyPanel.setSliderTranslations(compensatedTranslation);
```

#### 2. Apply FBX Scale in Rendering (`src/main.cpp`)

When rendering, apply the FBX local scale to the model matrix:
1. Get Rig_GRP's FBX local scale
2. Apply it to the model matrix during rendering
3. This ensures the compensated translation works correctly

**Code:**
```cpp
// Get FBX local scale
ofbx::Vec3 rigLocalScaling = rigRootObject->getLocalScaling();
glm::vec3 fbxLocalScale(rigLocalScaling.x, rigLocalScaling.y, rigLocalScaling.z);

// Build model matrix with FBX scale applied
rootNodeModelMatrix = glm::translate(rootNodeModelMatrix, rootTranslation);
rootNodeModelMatrix = rootNodeModelMatrix * glm::mat4_cast(rootRotation);
rootNodeModelMatrix = glm::scale(rootNodeModelMatrix, fbxLocalScale);  // Apply FBX scale
rootNodeModelMatrix = glm::scale(rootNodeModelMatrix, rootScale);  // Apply PropertyPanel scale
```

## How It Works

### Example Flow

**Step 1: Gizmo Manipulation**
- Gizmo moves 32.3268 units in world space
- Convert to parent's local space: 32.3268 (parent is identity)
- Get Rig_GRP's FBX scale: 0.01
- Compensate: 32.3268 / 0.01 = 3232.68
- Store 3232.68 in PropertyPanel

**Step 2: Rendering**
- Get PropertyPanel translation: 3232.68
- Get FBX local scale: 0.01
- Build matrix: Translate(3232.68) * Scale(0.01)
- Result: Visual movement = 3232.68 * 0.01 = 32.3268 ✓

**Result:** Visual movement (32.3268) matches gizmo movement (32.3268) 1:1 ✓

## Mathematical Explanation

### The Problem
```
Visual Movement = PropertyPanel Translation × FBX Local Scale
Visual Movement = 32.3268 × 0.01 = 0.323268  ❌ (100x smaller)
```

### The Solution
```
Compensated Translation = World Translation / FBX Local Scale
Compensated Translation = 32.3268 / 0.01 = 3232.68

Visual Movement = Compensated Translation × FBX Local Scale
Visual Movement = 3232.68 × 0.01 = 32.3268  ✓ (matches gizmo)
```

## Files Modified

1. **`src/io/scene.cpp`**:
   - Updated gizmo manipulation logic to compensate for FBX local scale
   - Uses `WorldToLocalMatrix()` for proper conversion
   - Divides translation by object's local scale before storing in PropertyPanel

2. **`src/main.cpp`**:
   - Updated rendering logic to apply FBX local scale to model matrix
   - Ensures compensated translation works correctly during rendering

3. **`MD/GIZMO_SCALE_COMPENSATION_FIX.md`**:
   - This documentation file

4. **`MD/TRANSFORM_SYSTEM.md`**:
   - Updated with scale compensation section

## Testing

To verify the fix:
1. Load an FBX file with Rig_GRP that has a local scale (e.g., 0.01)
2. Select Rig_GRP in the outliner
3. Move the gizmo 32.3268 units in world space
4. Verify:
   - PropertyPanel shows compensated translation (e.g., 3232.68)
   - Visual movement in viewport matches gizmo movement (32.3268 units)
   - Debug output shows both world space and compensated local space values

## Key Points

1. **Scale Compensation**: Translation is divided by FBX local scale to compensate
2. **Rendering Application**: FBX scale is applied during rendering to make compensation work
3. **1:1 Movement**: Visual movement now matches gizmo movement exactly
4. **Works for Any Scale**: Handles any local scale value (0.01, 0.001, etc.)

## Related Features

- **Transform System**: Uses `WorldToLocalMatrix()` for coordinate space conversion
- **PropertyPanel**: Stores compensated values that account for FBX scale
- **Gizmo Integration**: Gizmo now provides 1:1 visual movement regardless of FBX scale

## Summary

The fix ensures that gizmo movement matches visual movement 1:1 by:
1. Compensating for FBX local scale when storing transforms in PropertyPanel
2. Applying FBX local scale during rendering to make the compensation work correctly

This solves the issue where objects with non-identity local scales (like 0.01) moved 100x slower than the gizmo.
