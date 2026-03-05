# ImGuizmo Bounding Box Update Fix

## Problem
When RootNode is selected and the gizmo manipulates PropertyPanel Transforms, the bounding box was not updating to match the model's position. The model would move correctly, but the bounding box would stay at the original position.

## Root Cause
The bounding box was being rendered with an identity matrix, assuming the bounding box vertices were already in world space (calculated using `model.pos` and `model.size`). However, when RootNode is selected:

1. `model.pos/rotation/size` are reset to identity to prevent double transformation
2. Bounding box calculation uses `model.pos` (now identity), so vertices are in **local space**
3. Bounding box is rendered with identity matrix, so it stays in local space
4. Model is rendered with `rootNodeModelMatrix`, so it's in **world space**
5. Result: Bounding box and model are in different spaces, so they don't align

## Solution

**Apply the same transform matrix to the bounding box as the model.**

When RootNode is selected and `rootNodeModelMatrix` is used for the model, we should also use `rootNodeModelMatrix` for the bounding box rendering.

## Implementation

**File: `src/main.cpp`**

**Before:**
```cpp
// Render bounding box with identity model matrix (bounding box vertices are already in world space)
glm::mat4 modelMatrix = glm::mat4(1.0f);
instance->boundingBox.render(boundingBoxShader, view, projection, modelMatrix);
```

**After:**
```cpp
// Use the same model matrix as the model rendering
// When RootNode is selected, use rootNodeModelMatrix; otherwise use identity
glm::mat4 boundingBoxMatrix = glm::mat4(1.0f);
if (isRootNodeSelected && useRootNodeMatrix && selectedModelIndex == static_cast<int>(i)) {
    // Use the same root transform matrix as the model
    boundingBoxMatrix = rootNodeModelMatrix;
}
// Otherwise, bounding box vertices are already in world space (calculated with model.pos/size)

instance->boundingBox.render(boundingBoxShader, view, projection, boundingBoxMatrix);
```

## How It Works Now

1. **When RootNode is NOT selected:**
   - Bounding box calculation uses `model.pos/size` → vertices in world space
   - Bounding box rendered with identity matrix → stays in world space ✓
   - Model rendered with `model.pos/rotation/size` → in world space ✓
   - Bounding box and model align ✓

2. **When RootNode IS selected:**
   - `model.pos/rotation/size` reset to identity
   - Bounding box calculation uses `model.pos` (identity) → vertices in local space
   - Bounding box rendered with `rootNodeModelMatrix` → transformed to world space ✓
   - Model rendered with `rootNodeModelMatrix` → in world space ✓
   - Bounding box and model align ✓

## Flow Diagram

```
RootNode Selected:
    ↓
model.pos/rotation/size = identity
    ↓
Bounding Box Calculation:
    - Uses model.pos (identity)
    - Vertices in local space
    ↓
Bounding Box Rendering:
    - Uses rootNodeModelMatrix
    - Transforms to world space ✓
    ↓
Model Rendering:
    - Uses rootNodeModelMatrix
    - Transforms to world space ✓
    ↓
Bounding Box and Model Aligned ✓
```

## Benefits

1. **Bounding Box Updates Correctly** - Moves with the model when gizmo manipulates
2. **Consistent Transform** - Bounding box and model use the same transform matrix
3. **Visual Feedback** - Bounding box accurately represents model position
4. **Gizmo Alignment** - Bounding box, model, and gizmo all align correctly

## Files Modified

- `src/main.cpp` - Apply rootNodeModelMatrix to bounding box rendering when RootNode is selected
