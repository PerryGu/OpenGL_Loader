# Camera Framing Model Isolation Fix

## Date
Created: 2026-02-16

## Problem Statement

When pressing 'F' to focus the camera, it often focuses on the first character imported instead of the currently selected character. This happens because the code had fallback logic that could search through all models or use combined bounding boxes, causing the camera to frame the wrong character.

## Root Cause

The `applyCameraAimConstraint()` function in `scene.cpp` had:
1. Fallback logic that used `getCombinedBoundingBox()` when node bounding box wasn't found
2. Potential to search through multiple models (though the main loop was in `main.cpp`)
3. No strict enforcement to use only the selected model index

This meant that if a bounding box wasn't found for the selected node in the selected model, it would fall back to combined bounding boxes or other models, causing the camera to frame the first character instead of the selected one.

## Solution

Refactored `applyCameraAimConstraint()` to:
1. Completely remove fallback logic (no `getCombinedBoundingBox()`)
2. Use `mModelManager->getSelectedModelIndex()` directly to get the active model
3. Retrieve bounding box ONLY from the selected model instance
4. Transform bounding box using ONLY the selected model's RootNode transform
5. No fallback to other models - direct logic only

## Changes Made

### 1. Removed Fallback Logic

**File:** `src/io/scene.cpp`

**Location:** `Scene::applyCameraAimConstraint()` (lines ~2183-2332)

**Change:**

#### Before:
```cpp
// Get bounding box from selected model
if (!selectedNode.empty() && selectedModelIndex >= 0) {
    mModelManager->getNodeBoundingBox(selectedModelIndex, selectedNode, boundingBoxMin, boundingBoxMax);
    // ... transform bounding box ...
}

// If no node bounding box found, try to get combined bounding box
if (!hasBoundingBox && mModelManager->getModelCount() > 0) {
    mModelManager->getCombinedBoundingBox(boundingBoxMin, boundingBoxMax);
    // ... transform combined bounding box ...
}
```

#### After:
```cpp
// CRITICAL: Get the active model index directly - no loops through all models
int selectedModelIndex = mModelManager->getSelectedModelIndex();

// Only proceed if we have a valid selection (node and model index)
// No fallback to other models - direct logic only
if (!selectedNode.empty() && selectedModelIndex >= 0) {
    // Get bounding box for the selectedNode from THIS SPECIFIC model instance only
    mModelManager->getNodeBoundingBox(selectedModelIndex, selectedNode, boundingBoxMin, boundingBoxMax);
    // ... transform bounding box using selected model's RootNode transform ...
}
// CRITICAL: No fallback to combined bounding box or other models
// If no valid selection exists, hasBoundingBox remains false and we use rotation-only aim
```

**Why:**
- Removes fallback logic that could cause framing of wrong character
- Ensures camera always frames the selected model, not the first one
- Direct logic - no ambiguity about which model to use

### 2. Direct Model Index Usage

**Change:**
```cpp
// CRITICAL: Get the active model index directly - no loops through all models
// This ensures we use the correct model context for the selected node
int selectedModelIndex = mModelManager->getSelectedModelIndex();
```

**Why:**
- Uses `getSelectedModelIndex()` directly
- No loops through `getModelCount()`
- Ensures we use the model that matches the current selection context

### 3. Model-Specific RootNode Transform

**Change:**
```cpp
// Get RootNode transform matrix from PropertyPanel for THIS SPECIFIC MODEL
std::string modelRootNodeName = mOutliner.getRootNodeName(selectedModelIndex);
// ... build transform using modelRootNodeName ...
// Transform all corners to world space using the selected model's RootNode transform
```

**Why:**
- Uses `getRootNodeName(selectedModelIndex)` to get the correct RootNode name
- Builds transform matrix using PropertyPanel values for that specific model
- Transforms bounding box corners using only the selected model's RootNode transform

### 4. Removed Combined Bounding Box Fallback

**Removed:**
- Entire block that called `getCombinedBoundingBox()`
- Fallback logic that could use wrong model's transform
- Any code that could search through multiple models

**Why:**
- Prevents camera from framing wrong character
- Ensures strict isolation - only selected model is used
- Direct logic - no ambiguity

## Behavior

### Before
- Press 'F' → May frame first character if bounding box not found
- Fallback to combined bounding box → Could use wrong model's transform
- No strict enforcement of selected model index

### After
- Press 'F' → Always frames the selected character
- Uses only selected model's bounding box and RootNode transform
- No fallback - direct logic only
- If no valid selection, uses rotation-only aim (no distance adjustment)

## Key Principles

### 1. Direct Model Index Usage
- Always use `getSelectedModelIndex()` directly
- No loops through all models
- No fallback to other models

### 2. Model-Specific Bounding Box
- Get bounding box only from selected model instance
- Use `getNodeBoundingBox(selectedModelIndex, selectedNode, ...)`
- No global search across models

### 3. Model-Specific RootNode Transform
- Get RootNode name using `getRootNodeName(selectedModelIndex)`
- Build transform matrix using PropertyPanel values for that specific model
- Transform bounding box using only selected model's RootNode transform

### 4. No Fallback Logic
- If bounding box not found, `hasBoundingBox` remains false
- Camera uses rotation-only aim (no distance adjustment)
- No fallback to combined bounding box or other models

## Testing Checklist

When testing the fix, verify:
- [ ] Pressing 'F' frames the selected character, not the first one
- [ ] Selecting a bone in 'Character_B' and pressing 'F' frames 'Character_B'
- [ ] Selecting a bone in 'Character_A' and pressing 'F' frames 'Character_A'
- [ ] Camera uses correct RootNode transform for each character
- [ ] No fallback to wrong character when bounding box not found
- [ ] Rotation-only aim works when no bounding box available

## Files Modified

1. `src/io/scene.cpp`
   - Removed fallback logic to `getCombinedBoundingBox()`
   - Removed loops through all models
   - Direct usage of `getSelectedModelIndex()`
   - Model-specific bounding box retrieval and transformation

## Related Documentation

- `MD/MODEL_INDEX_UPDATE_FOR_BONES.md` - Model index update for bone selection
- `MD/BONE_GIZMO_POSITIONING_FIX.md` - Bone gizmo positioning fix
- `MD/SELECTION_ISOLATION_REFACTOR.md` - Selection logic refactoring

## Summary

This fix ensures that the camera framing logic (`applyCameraAimConstraint()`) uses only the selected model index, with no fallback to other models or combined bounding boxes. The function now:
1. Gets the active model index directly using `getSelectedModelIndex()`
2. Retrieves bounding box only from the selected model instance
3. Transforms bounding box using only the selected model's RootNode transform
4. Has no fallback logic - direct and unambiguous

This prevents the camera from focusing on the first character imported and ensures it always frames the currently selected character.
