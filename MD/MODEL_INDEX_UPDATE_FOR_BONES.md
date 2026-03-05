# Model Index Update for Bone Selection

## Date
Created: 2026-02-16

## Problem Statement

Currently, `mModelManager->mSelectedModelIndex` only updates when a RootNode is selected. Selecting a bone directly in the Outliner does not switch the active model context, causing the Gizmo to fail and reset to (0,0,0). This happens because:

1. The Gizmo calculation in `scene.cpp` uses `getSelectedModelIndex()` to get the RootNode transform
2. When a bone is selected, the model index is not updated
3. The Gizmo uses the wrong model's RootNode transform, causing incorrect positioning
4. For secondary models, this often results in the Gizmo jumping to (0,0,0)

## Root Cause

The selection handling logic in `main.cpp` only called `modelManager.setSelectedModel()` when `isRigRootSelected` was true. This meant:
- RootNode selection → Model index updated ✅
- Rig root selection → Model index updated ✅
- Bone selection → Model index NOT updated ❌

Additionally, `findModelIndexForSelectedObject()` only checked if the selected object was the root of a scene, not searching the hierarchy for bones.

## Solution

Refactored the selection handling logic to:
1. Update model index for ANY node selection (not just RootNode/rig root)
2. Enhanced `findModelIndexForSelectedObject()` to recursively search the hierarchy
3. Ensure the Gizmo uses the correct RootNode transform for the selected node's model

## Changes Made

### 1. Model Index Update for Any Node Selection

**File:** `src/main.cpp`

**Location:** Main loop - selection handling section (lines ~716-739)

**Change:**
```cpp
// CRITICAL FIX: Update model index for ANY node selection (not just RootNode/rig root)
// This ensures the Gizmo uses the correct RootNode transform for the selected node's model
// When selecting a bone in 'Character_B', we must identify its model index even if 'Character_A' was previously selected
// The Gizmo must use the world-space context of the model identified by the Outliner
if (!selectedNode.empty()) {
    // Find which model the selected node belongs to
    int nodeModelIndex = scene.mOutliner.findModelIndexForSelectedObject();
    
    if (nodeModelIndex >= 0 && nodeModelIndex < static_cast<int>(modelManager.getModelCount())) {
        // Get current selected model index
        int currentModelIndex = modelManager.getSelectedModelIndex();
        
        // CRITICAL: Update selected model to the one containing this node (if different)
        // This must happen for ANY node selection (RootNode, rig root, or bone)
        // This ensures the Gizmo calculation in scene.cpp uses the correct RootNode transform
        if (currentModelIndex != nodeModelIndex) {
            modelManager.setSelectedModel(nodeModelIndex);
        }
    }
}
```

**Why:** 
- Moves model index update BEFORE checking if it's a rig root
- Updates model index for ANY node selection (RootNode, rig root, or bone)
- Ensures Gizmo calculation uses the correct model's RootNode transform

### 2. Enhanced findModelIndexForSelectedObject()

**File:** `src/io/ui/outliner.cpp`

**Location:** `Outliner::findModelIndexForSelectedObject()` (lines ~820-870)

**Changes:**

#### Before:
```cpp
int Outliner::findModelIndexForSelectedObject() const
{
    // Only checked if selected object is the root
    for (size_t i = 0; i < mFBXScenes.size(); ++i) {
        const ofbx::Object* root = mFBXScenes[i].scene->getRoot();
        if (root == g_selected_object) {
            return static_cast<int>(i);
        }
        // Comment said: "For other nodes, we'd need to search the hierarchy"
    }
    return -1;
}
```

#### After:
```cpp
// Helper function to recursively search for an object in a scene's hierarchy
static bool findObjectInHierarchy(const ofbx::Object* startObject, const ofbx::Object* targetObject)
{
    if (!startObject || !targetObject) {
        return false;
    }
    
    // Check if this is the target object
    if (startObject == targetObject) {
        return true;
    }
    
    // Recursively search children
    int i = 0;
    while (ofbx::Object* child = const_cast<ofbx::Object*>(startObject)->resolveObjectLink(i)) {
        if (findObjectInHierarchy(child, targetObject)) {
            return true;
        }
        ++i;
    }
    
    return false;
}

int Outliner::findModelIndexForSelectedObject() const
{
    // Now recursively searches the entire hierarchy, not just root nodes
    for (size_t i = 0; i < mFBXScenes.size(); ++i) {
        const ofbx::Object* root = mFBXScenes[i].scene->getRoot();
        if (root) {
            // Fast path: check if root
            if (root == g_selected_object) {
                return static_cast<int>(i);
            }
            
            // CRITICAL: Recursively search the entire hierarchy to find bones and other nodes
            if (findObjectInHierarchy(root, g_selected_object)) {
                return static_cast<int>(i);
            }
        }
    }
    return -1;
}
```

**Why:**
- Recursively searches the entire scene hierarchy
- Works for bones, not just root nodes
- Ensures bones can be correctly identified to their owning model

### 3. Updated Rig Root Initialization Logic

**File:** `src/main.cpp`

**Location:** Rig root initialization section (lines ~773-775)

**Change:**
```cpp
//-- Handle rig root selection (for initialization only) ----
// When rig root is selected, we need to:
// 1. Initialize PropertyPanel values if this is the first time selecting this rig root
// Note: Model index is already updated above for ANY node selection
if (isRigRootSelected) {
    // Use the already-updated selected model index
    int rigRootModelIndex = modelManager.getSelectedModelIndex();
    // ... initialization logic ...
}
```

**Why:**
- Model index is already updated above, so we can use it directly
- Removes duplicate model index update logic
- Simplifies the code flow

## Behavior

### Before
- Selecting RootNode → Model index updated ✅
- Selecting rig root → Model index updated ✅
- Selecting bone → Model index NOT updated ❌
- Gizmo uses wrong model's RootNode transform
- Gizmo jumps to (0,0,0) for secondary models

### After
- Selecting RootNode → Model index updated ✅
- Selecting rig root → Model index updated ✅
- Selecting bone → Model index updated ✅
- Gizmo uses correct model's RootNode transform
- Gizmo positions correctly for all models

## Key Principles

### 1. Model Index Update for Any Selection
- Model index is updated immediately when ANY node is selected
- Happens before checking if it's a rig root
- Ensures Gizmo always has the correct model context

### 2. Recursive Hierarchy Search
- `findModelIndexForSelectedObject()` now searches the entire hierarchy
- Works for root nodes, rig roots, bones, and any other nodes
- Ensures correct model identification regardless of node type

### 3. Gizmo Context
- Gizmo uses `getSelectedModelIndex()` to get the correct model
- RootNode transform is retrieved using the model-specific index
- World position calculation uses: `RootNodeTransform[selectedModelIndex] * BoneLocalPosition`

## Testing Checklist

When testing the fix, verify:
- [ ] Selecting a bone in the first model updates model index correctly
- [ ] Selecting a bone in secondary models updates model index correctly
- [ ] Gizmo positions correctly on bones in all models (not at 0,0,0)
- [ ] Gizmo uses the correct RootNode transform for each model
- [ ] Switching between models works correctly
- [ ] RootNode selection still works correctly
- [ ] Rig root selection still works correctly

## Files Modified

1. `src/main.cpp`
   - Moved model index update to happen for ANY node selection
   - Updated rig root initialization to use already-updated model index

2. `src/io/ui/outliner.cpp`
   - Enhanced `findModelIndexForSelectedObject()` to recursively search hierarchy
   - Added `findObjectInHierarchy()` helper function

## Related Documentation

- `MD/SELECTION_ISOLATION_REFACTOR.md` - Selection logic refactoring for model isolation
- `MD/BONE_GIZMO_POSITIONING_FIX.md` - Bone gizmo positioning fix

## Summary

This fix ensures that the model index is updated for ANY node selection, not just RootNode/rig root. The enhanced `findModelIndexForSelectedObject()` function now recursively searches the entire scene hierarchy, allowing it to correctly identify which model a bone belongs to. This ensures the Gizmo always uses the correct RootNode transform for the selected node's model, preventing it from jumping to (0,0,0) when selecting bones in secondary models.
