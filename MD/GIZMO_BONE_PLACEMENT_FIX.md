# Gizmo Bone Placement and Mesh Explosion Fix

## Date
Created: 2026-02-23

## Overview
Fixed two critical bugs:
1. **Gizmo Placement Bug**: The Gizmo was incorrectly positioned at the parent bone's origin instead of the selected bone's actual location. This was caused by manually reconstructing the bone's transform matrix from UI slider values, which missed the bone's structural FBX bind pose offset/length.
2. **Mesh Explosion/Stretching Bug**: After fixing the Gizmo placement, the world-to-local matrix extraction was still using the old logic that expected only UI deltas. Since the Gizmo now receives the full world matrix (including bind pose), the old extraction injected the bind pose into UI sliders, creating a recursive feedback loop that stretched the mesh infinitely.

## Problem

### Symptom
When selecting a bone in the outliner, the Gizmo would appear at the parent bone's position instead of the selected bone's position. This made it impossible to manipulate bones accurately.

### Root Cause
In `GizmoManager::renderGizmo`, the `modelMatrix` for bone nodes was being manually reconstructed by:
1. Getting the parent's world-space matrix
2. Building a local matrix from PropertyPanel UI sliders (translation, rotation, scale)
3. Multiplying: `modelMatrix = parentWorldSpace * localMatrix`

**The Issue**: UI sliders for bones typically represent animation deltas (where local translation is near zero), not the bone's structural bind pose. This reconstruction completely missed the bone's FBX bind pose offset/length, causing the Gizmo to snap to the parent's origin.

## Solution

### Implementation
Changed from UI-based matrix reconstruction to direct retrieval of the bone's actual calculated model-space matrix from the model's linear skeleton.

**New Approach**:
1. Added `Model::getBoneModelSpaceMatrix(int boneIndex)` method
2. This method retrieves the bone's `globalMatrix` from the `LinearBone` structure
3. The `globalMatrix` contains the complete transform including bind pose offset/length
4. Calculate world matrix: `modelMatrix = rootNodeTransform * boneModelSpaceMatrix`

### Code Changes

#### 1. Added Method to `Model` Class (`src/graphics/model.h`)
```cpp
// Get bone's model-space matrix from linear skeleton (includes bind pose offset)
// Returns the bone's actual calculated global matrix in model space, or identity if not found
glm::mat4 getBoneModelSpaceMatrix(int boneIndex) const;
```

#### 2. Implemented Method (`src/graphics/model.cpp`)
```cpp
glm::mat4 Model::getBoneModelSpaceMatrix(int boneIndex) const {
    if (boneIndex < 0 || m_linearSkeleton.empty()) return glm::mat4(1.0f);
    
    // Find the LinearBone entry with matching boneIndex to get its globalMatrix
    for (size_t i = 0; i < m_linearSkeleton.size(); i++) {
        if (m_linearSkeleton[i].boneIndex == boneIndex) {
            // Convert Matrix4f to GLM format
            // Note: Transpose is required to convert from Assimp/Math3D row-major to GLM column-major
            Matrix4f boneGlobalMatrix = m_linearSkeleton[i].globalMatrix;
            boneGlobalMatrix = boneGlobalMatrix.Transpose();
            return boneGlobalMatrix.toGlmMatrix();
        }
    }
    return glm::mat4(1.0f); // Default to identity if not found
}
```

#### 3. Updated Gizmo Matrix Calculation (`src/io/ui/gizmo_manager.cpp`)
**Before** (lines 265-290):
```cpp
// 1. Get Parent's true World Matrix
glm::mat4 parentModelSpace = selectedModel->model.getParentGlobalMatrix(boneIndex);
glm::mat4 parentWorldSpace = rootNodeTransform * parentModelSpace;

// 2. Build Bone's true Local Matrix from PropertyPanel
glm::vec3 localTranslation = propertyPanel.getSliderTrans_update();
// ... build localMatrix from UI sliders ...
modelMatrix = parentWorldSpace * localMatrix;
```

**After**:
```cpp
// FIXED: Retrieve the bone's actual calculated model-space matrix directly from the model
// The bone's globalMatrix in LinearBone contains the complete transform including
// the structural FBX bind pose offset/length, which UI sliders (animation deltas) don't capture
// This prevents the Gizmo from snapping to the parent's origin

// Get the bone's model-space matrix (includes bind pose offset)
glm::mat4 boneModelSpaceMatrix = selectedModel->model.getBoneModelSpaceMatrix(boneIndex);

// Calculate the true world matrix by multiplying root node transform by bone's model-space matrix
// This correctly positions the Gizmo at the bone's actual location including bind pose offset
modelMatrix = rootNodeTransform * boneModelSpaceMatrix;
```

## Part 2: Mesh Explosion/Stretching Fix

### Problem

After fixing the Gizmo placement, a new bug appeared: manipulating bones with the Gizmo caused the mesh to explode/stretch infinitely.

**Root Cause**: The world-to-local matrix extraction block (`if (manipulated)`) was still using the old logic:
```cpp
glm::mat4 newLocalMatrix = parentInverse * newWorldMatrix;
```

This extraction expected `newWorldMatrix` to contain only UI deltas. However, after the placement fix, `newWorldMatrix` now correctly includes the bone's full bind pose. The old extraction logic injected the bind pose directly into the UI sliders. Since the engine adds these UI values on top of the bind pose every frame, it created a recursive feedback loop:
- Frame 1: UI = bind pose
- Frame 2: UI = bind pose + bind pose = 2× bind pose
- Frame 3: UI = bind pose + 2× bind pose = 3× bind pose
- ... → Infinite stretching

### Solution

Replaced the old world-to-local extraction with **delta-extraction logic** that isolates only the Gizmo's manipulation delta and applies it to the existing UI values.

**New Approach**:
1. Fetch current UI values (`U_orig`) before manipulation
2. Extract pure Gizmo delta: `Delta = Inverse(W_orig) * W_new`
3. Apply delta to UI: `U_new = U_orig * Delta`
4. Decompose `U_new` for PropertyPanel

**Formula**: `U_new = U_orig * Inverse(W_orig) * W_new`

Where:
- `U_orig` = Current UI transform matrix (from PropertyPanel sliders)
- `W_orig` = Original world matrix (before Gizmo manipulation, includes bind pose)
- `W_new` = New world matrix (after Gizmo manipulation, includes bind pose + delta)
- `U_new` = New UI transform matrix (UI values + Gizmo delta only)

### Code Changes (`src/io/ui/gizmo_manager.cpp`)

**Deleted** (old extraction logic):
- RootNode transform reconstruction
- Parent world matrix calculation
- `newLocalMatrix = parentInverse * newWorldMatrix` extraction

**Added** (delta-extraction logic):
```cpp
// 1. Fetch current UI values (U_orig) before applying this frame's manipulation
glm::vec3 origTrans = propertyPanel.getSliderTrans_update();
glm::vec3 origRotEuler = propertyPanel.getSliderRot_update();
origRotEuler.y = -origRotEuler.y; // CRITICAL: Translator (UI -> Gizmo)
glm::quat origRot;
if (mGizmoInitialStateStored && selectedNode == mInitialNodeName && ((mGizmoOperation & ImGuizmo::ROTATE) != 0)) {
    origRot = mCurrentLocalRotationQuat;
} else {
    origRot = glm::quat(glm::radians(origRotEuler));
}
glm::vec3 origScale = propertyPanel.getSliderScale_update();

// Build U_orig matrix
glm::mat4 U_orig = glm::mat4(1.0f);
U_orig = glm::translate(U_orig, origTrans);
U_orig = U_orig * glm::mat4_cast(origRot);
U_orig = glm::scale(U_orig, origScale);

// 2. Extract pure Gizmo Delta and apply to U_orig
// `modelMatrix` is W_orig (the true world matrix before manipulation)
// `modelMatrixArray` contains W_new (the modified world matrix)
glm::mat4 W_orig = modelMatrix; 
glm::mat4 W_new = float16ToGlmMat4(modelMatrixArray);

// Calculate U_new: U_new = U_orig * Inverse(W_orig) * W_new
// This isolates purely the Delta introduced by ImGuizmo and applies it to the existing UI values
glm::mat4 W_orig_inv = glm::inverse(W_orig);
glm::mat4 U_new = U_orig * W_orig_inv * W_new;

// 3. Decompose U_new for the Property Panel
float localTrans[3], localRot[3], localScale[3];
ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(U_new), localTrans, localRot, localScale);
```

## Technical Details

### Why LinearBone.globalMatrix is Correct
- The `LinearBone` structure stores a `globalMatrix` field that is computed during `updateLinearSkeleton()`
- This matrix includes:
  - The bone's bind pose transform (structural offset/length from FBX)
  - Current animation transforms
  - Parent hierarchy transforms
- It represents the bone's true position in model space, not just animation deltas

### Matrix Conversion
- `LinearBone.globalMatrix` is stored as `Matrix4f` (row-major, Assimp/Math3D format)
- Must be transposed to convert to GLM's column-major format
- Conversion: `Matrix4f::Transpose()` → `Matrix4f::toGlmMatrix()`

### World Space Calculation
- Bone's model-space matrix: `boneModelSpaceMatrix` (from `LinearBone.globalMatrix`)
- Root node transform: `rootNodeTransform` (from PropertyPanel RootNode sliders)
- Final world matrix: `modelMatrix = rootNodeTransform * boneModelSpaceMatrix`

## Benefits

### Part 1: Gizmo Placement
1. **Accurate Gizmo Placement**: Gizmo now appears at the bone's actual location, not the parent's origin
2. **Preserves Bind Pose**: Correctly includes the bone's structural FBX bind pose offset/length
3. **Simpler Code**: Removed complex UI-based matrix reconstruction
4. **More Reliable**: Uses the model's calculated transforms instead of reconstructing from UI state

### Part 2: Mesh Explosion Fix
1. **No More Mesh Stretching**: Delta extraction prevents recursive feedback loop
2. **Correct UI Updates**: Only Gizmo manipulation delta is applied to UI sliders
3. **Preserves Existing Values**: Existing UI values are maintained, only delta is added
4. **Stable Manipulation**: Mesh remains stable during and after Gizmo manipulation

## Related Files

- `src/graphics/model.h` - Added `getBoneModelSpaceMatrix()` declaration
- `src/graphics/model.cpp` - Implemented `getBoneModelSpaceMatrix()` method
- `src/io/ui/gizmo_manager.cpp` - Updated bone matrix calculation and world-to-local extraction

## Testing

### Test Case 1: Bone Selection
1. Load a model with skeleton
2. Select a bone in the outliner (not the root)
3. **Expected**: Gizmo appears at the bone's actual position (not parent's origin)

### Test Case 2: Long Bones
1. Select a long bone (e.g., thigh bone)
2. **Expected**: Gizmo appears at the bone's joint position, not at the hip

### Test Case 3: Short Connector Bones
1. Select a short connector bone (e.g., Pelvis to Hip)
2. **Expected**: Gizmo appears at the connector bone's position, not at the COG

### Test Case 4: Root Node Transform
1. Apply RootNode translation/rotation/scale
2. Select a bone
3. **Expected**: Gizmo appears at the correct world-space position (root transform applied)

### Test Case 5: Gizmo Manipulation (Mesh Stability)
1. Select a bone and manipulate it with the Gizmo
2. **Expected**: Mesh remains stable, no stretching or explosion
3. **Expected**: Only the manipulation delta is applied to UI sliders
4. **Expected**: Mesh returns to normal when manipulation ends

### Test Case 6: Multiple Manipulations
1. Manipulate a bone multiple times
2. **Expected**: Each manipulation adds only its delta, no cumulative bind pose injection
3. **Expected**: Mesh remains stable across multiple manipulations

## Future Considerations

- Consider caching the bone model-space matrices if performance becomes an issue
- The current implementation searches through `m_linearSkeleton` linearly - could be optimized with a map if needed
- This fix ensures the Gizmo uses the same transform data that the rendering system uses, maintaining consistency
