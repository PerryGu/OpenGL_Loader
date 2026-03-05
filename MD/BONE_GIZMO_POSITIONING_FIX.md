# Bone Gizmo Positioning Fix: Model Isolation

## Date
Created: 2026-02-16

## Problem Statement

When selecting a bone in the first imported character, the gizmo positions correctly. However, in secondary models, the Gizmo often jumps to the world origin (0,0,0) instead of staying on the bone. This is a data leak issue where bone positioning logic is not properly isolated to the selected model instance.

## Root Cause

The bone gizmo positioning logic was:
1. Using the correct model instance (`selectedModel`) but not ensuring strict isolation
2. Falling back to PropertyPanel values when bone not found, which might be (0,0,0) for unmanipulated bones
3. Not using the correct model's RootNode transform in all cases

## Solution

Refactored the bone gizmo positioning logic to:
1. Strictly use `mModelManager->getSelectedModelIndex()` to get the correct ModelInstance
2. Locate bone transform data specifically within the active model instance's global transforms map
3. Calculate gizmo's world-space position using: `WorldPosition = RootNodeTransform[selectedModelIndex] * BoneLocalPosition`
4. Add fallback to FBX scene data when bone not found in transforms map

## Changes Made

### 1. Enhanced Bone Lookup with Model Isolation

**File:** `src/io/scene.cpp`

**Location:** `Scene::renderViewportWindow()` - Bone gizmo positioning section (lines ~1217-1267)

**Changes:**

#### Before:
```cpp
// Get the bone's world position from global transforms
const std::map<std::string, BoneGlobalTransform>& globalTransforms = selectedModel->model.getBoneGlobalTransforms();
auto boneIt = globalTransforms.find(selectedNode);

if (boneIt != globalTransforms.end()) {
    // Use bone transform...
} else {
    // Fallback to PropertyPanel (might be 0,0,0)
    glm::vec3 translation = mPropertyPanel.getSliderTrans_update();
    // ...
}
```

#### After:
```cpp
// CRITICAL FIX: Get the bone's transform data from the SPECIFIC MODEL INSTANCE
// Use mModelManager->getSelectedModelIndex() to ensure we're using the correct model
// This prevents "leaking" position data between different characters
//
// The bone's position from BoneGlobalTransform is in MODEL SPACE (relative to model origin)
// We must transform it by the RootNode transform of THIS SPECIFIC MODEL to get world position
// Formula: WorldPosition = RootNodeTransform[selectedModelIndex] * BoneLocalPosition

// CRITICAL: Get bone transforms from the SELECTED MODEL INSTANCE ONLY
const std::map<std::string, BoneGlobalTransform>& globalTransforms = selectedModel->model.getBoneGlobalTransforms();
auto boneIt = globalTransforms.find(selectedNode);

if (boneIt != globalTransforms.end()) {
    // Found the bone in THIS MODEL's transforms
    const BoneGlobalTransform& boneTransform = boneIt->second;
    glm::vec3 modelSpacePosition = boneTransform.translation;
    
    // CRITICAL: Transform by THIS MODEL's RootNode transform
    // WorldPos = RootNodeTransform[selectedModelIndex] * BoneLocalPos
    glm::vec4 modelSpacePos4(modelSpacePosition, 1.0f);
    glm::vec4 worldPos4 = rootNodeTransform * modelSpacePos4;
    glm::vec3 worldPosition(worldPos4.x, worldPos4.y, worldPos4.z);
    // ... use worldPosition for gizmo
} else {
    // Enhanced fallback: Try FBX scene data for THIS MODEL
    const ofbx::IScene* scene = mOutliner.getScene(selectedModelIndex);
    if (scene && selectedModelIndex >= 0) {
        const ofbx::Object* boneObject = mOutliner.findObjectByName(sceneRoot, selectedNode);
        if (boneObject) {
            // Get bone position from FBX and transform by THIS MODEL's RootNode
            // ... calculate world position
        }
    }
}
```

**Why:**
- Ensures bone lookup is scoped to the selected model instance
- Uses the correct RootNode transform for the selected model
- Provides better fallback when bone not found in transforms map
- Prevents data leaks between different character models

### 2. RootNode Transform Calculation

**File:** `src/io/scene.cpp`

**Location:** Same section (lines ~1190-1215)

**Key Points:**
- RootNode transform is calculated using `selectedModelIndex` to get the correct model's RootNode name
- RootNode name is retrieved using `mOutliner.getRootNodeName(selectedModelIndex)` (model-specific)
- RootNode transform values are retrieved from PropertyPanel using the model-specific RootNode name
- This ensures each model uses its own RootNode transform, not a global one

**Formula:**
```
WorldPosition = RootNodeTransform[selectedModelIndex] * BoneLocalPosition
```

Where:
- `RootNodeTransform[selectedModelIndex]` = Transform matrix built from PropertyPanel values for THIS model's RootNode
- `BoneLocalPosition` = Bone's position in model space (from `BoneGlobalTransform.translation`)

## Key Principles

### 1. Model-Specific Bone Lookup
- Always use `selectedModel->model.getBoneGlobalTransforms()` (scoped to selected model)
- Never search across all models for bone names
- Bone transforms are per-model, not global

### 2. Model-Specific RootNode Transform
- Each model has its own RootNode (e.g., "RootNode", "RootNode01", "RootNode02")
- RootNode transform is retrieved using model-specific name
- RootNode transform values are stored per-model in PropertyPanel

### 3. World Position Calculation
- Bone position in model space must be transformed by the model's RootNode transform
- Formula: `WorldPos = RootNodeTransform * BoneLocalPos`
- This ensures bones in secondary models are positioned correctly relative to their model's origin

### 4. Enhanced Fallback
- When bone not found in transforms map, try FBX scene data
- Get bone's local transform from FBX and transform by RootNode
- Only use PropertyPanel as last resort (might be 0,0,0 for unmanipulated bones)

## Testing Checklist

When testing the fix, verify:
- [ ] Selecting a bone in the first model positions gizmo correctly
- [ ] Selecting a bone in secondary models positions gizmo correctly (not at 0,0,0)
- [ ] Gizmo stays on bone when switching between models
- [ ] RootNode transform is correctly applied for each model
- [ ] Bone positions are isolated to their respective models
- [ ] No data leaks between different character models
- [ ] Fallback to FBX scene data works when transforms not available

## Files Modified

1. `src/io/scene.cpp`
   - Enhanced bone gizmo positioning logic with model isolation
   - Added fallback to FBX scene data when bone not found
   - Improved comments explaining the world position calculation

## Related Documentation

- `MD/SELECTION_ISOLATION_REFACTOR.md` - Selection logic refactoring for model isolation
- `MD/PROJECT_UNDERSTANDING.md` - Project overview and architecture

## Summary

This fix ensures that bone gizmo positioning is strictly isolated to the selected model instance. The gizmo's world position is calculated using the formula `WorldPosition = RootNodeTransform[selectedModelIndex] * BoneLocalPosition`, where both the RootNode transform and bone local position are specific to the selected model. This prevents the gizmo from jumping to (0,0,0) when selecting bones in secondary models and ensures proper isolation between different character models.
