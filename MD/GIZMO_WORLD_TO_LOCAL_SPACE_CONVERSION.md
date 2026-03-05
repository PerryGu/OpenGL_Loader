# Gizmo World-to-Local Space Conversion

**Date:** 2026-02-20  
**Status:** ✅ Implemented

## Overview
Implemented proper world-to-local space conversion for bone transformations using ImGuizmo. This allows precise bone manipulation while maintaining correct hierarchical relationships.

## Problem
The Gizmo was writing WORLD space transformations directly into LOCAL space arrays, causing:
- Character mesh explosion (scale reaching extreme values like -107374)
- Incorrect bone positioning
- Broken hierarchical relationships

## Solution

### 1. Gizmo Mode
- Switched ImGuizmo to `ImGuizmo::LOCAL` mode
- This ensures the Gizmo handles align with the bone's local orientation

### 2. Matrix Decomposition
Replaced complex delta/axis-angle logic with pure matrix decomposition:

```cpp
// Formula: Local = Inverse(ParentWorld) * NewWorld
glm::mat4 parentWorldSpace = rootNodeTransform * parentModelSpace;
glm::mat4 parentInverse = glm::inverse(parentWorldSpace);
glm::mat4 newLocalMatrix = parentInverse * newWorldMatrix;
```

### 3. Handedness Fix
Implemented a "Translator" approach to handle coordinate system mismatches:
- **UI → Gizmo:** Invert Y-axis when reading from PropertyPanel
- **Gizmo → UI:** Invert Y-axis when writing to PropertyPanel
- **Internal State:** Keep raw rotation in Gizmo's memory (no inversion)

This prevents rotation jitter caused by feedback loops.

## Implementation Details

### Files Modified
- `src/io/ui/gizmo_manager.cpp`
- `src/graphics/model.h` (added `getParentGlobalMatrix()`)

### Key Functions
- `GizmoManager::renderGizmo()` - Main gizmo rendering and manipulation
- `Model::getParentGlobalMatrix()` - Gets dynamically calculated parent matrix

### Matrix Calculation Flow
1. **Build World Matrix for Gizmo:**
   ```cpp
   modelMatrix = parentWorldSpace * localMatrix;
   ```

2. **After Manipulation, Extract Local:**
   ```cpp
   glm::mat4 newLocalMatrix = parentInverse * newWorldMatrix;
   glm::decompose(newLocalMatrix, newLocalScale, newLocalRotation, newLocalTranslation, ...);
   ```

3. **Update Model Arrays:**
   ```cpp
   model->updateBoneTranslationByIndex(boneIndex, newLocalTranslation);
   model->updateBoneRotationByIndex(boneIndex, newLocalRotationEuler);
   model->updateBoneScaleByIndex(boneIndex, newLocalScale);
   ```

## Benefits
- ✅ Correct bone transformations in local space
- ✅ No mesh explosion or scale issues
- ✅ Smooth rotation without jitter
- ✅ Proper hierarchical bone relationships maintained

## Testing
- Translate gizmo: ✅ Works smoothly
- Rotate gizmo (X/Z axes): ✅ Works perfectly
- Rotate gizmo (Y axis): ✅ Fixed handedness mismatch
- Scale gizmo: ✅ No explosion issues

## Related Files
- `MD/GIZMO_WORLD_TO_LOCAL_CONVERSION.md` (older version)
- `MD/GIZMO_SCALE_COMPENSATION_FIX.md`
