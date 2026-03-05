# Fix: New Model Appearing at Previous Model's Position

## Date
Fixed: 2024

## Problem Description

When loading a new character after moving the first character's RootNode, the new character appears at the first character's position instead of at the center (0,0,0).

### Steps to Reproduce
1. Load Model 0 (e.g., "Walk.fbx")
2. Move Model 0's RootNode to position (54.10, 0, 0) using gizmo
3. Load Model 1 (e.g., "astroBoy_walk_Maya.fbx")
4. **Bug**: Model 1 appears at (54.10, 0, 0) instead of (0, 0, 0)

## Root Cause

The initialization check in `main.cpp` was using `getSelectedNode()` to get the rig root name, which returns the **currently selected node in the outliner**. When a new model is loaded:

1. Model 0's RootNode transforms are saved as `mBoneTranslations["RootNode"] = (54.10, 0, 0)`
2. Model 1 loads and gets renamed to "RootNode01" (to avoid collision)
3. However, the outliner selection might still be on Model 0's "RootNode"
4. The initialization code at line 544 used `getSelectedNode()`, which returned "RootNode" (the stale selection)
5. The code checked if `mBoneTranslations["RootNode"]` exists - **it does** (from Model 0)
6. Since transforms exist, initialization was skipped
7. Model 1 inherited Model 0's transforms

### The Buggy Code
```cpp
// Line 544 - WRONG: Uses currently selected node (might be stale)
std::string currentRigRootName = scene.mOutliner.getSelectedNode();

// Line 549 - Checks if "RootNode" exists (from Model 0)
auto transIt = scene.mPropertyPanel.getAllBoneTranslations().find(currentRigRootName);
// transIt != end() because Model 0's "RootNode" exists!
// So initialization is skipped ❌
```

## Solution

Instead of using `getSelectedNode()` (which might return a stale selection), the code now uses `getRigRootName(rigRootModelIndex)` or `getRootNodeName(rigRootModelIndex)` to get the **correct rig root name for the specific model being initialized**. This ensures:

1. Model 1's rig root name is correctly retrieved (e.g., "RootNode01" if renamed)
2. The check looks for `mBoneTranslations["RootNode01"]` (which doesn't exist for a new model)
3. Initialization proceeds correctly, setting Model 1 to (0,0,0)

### The Fixed Code
```cpp
// Line 546 - CORRECT: Get rig root name for THIS SPECIFIC MODEL
std::string rigRootNameForModel = scene.mOutliner.getRigRootName(rigRootModelIndex);
// If no rig root found, try RootNode name (renamed if collision)
if (rigRootNameForModel.empty()) {
    rigRootNameForModel = scene.mOutliner.getRootNodeName(rigRootModelIndex);
}
// Fallback to selected node only if both above are empty
if (rigRootNameForModel.empty()) {
    rigRootNameForModel = scene.mOutliner.getSelectedNode();
}

// Line 561 - Checks if "RootNode01" exists (for Model 1)
auto transIt = scene.mPropertyPanel.getAllBoneTranslations().find(rigRootNameForModel);
// transIt == end() because Model 1's "RootNode01" doesn't exist yet
// So initialization proceeds ✅
```

## Why Renaming Should Have Worked (But Didn't)

The renaming mechanism **does work correctly** - Model 1's RootNode is properly renamed to "RootNode01". However, the bug was that the initialization check wasn't using the renamed name. It was using the stale selection from the outliner instead of querying the correct name for the model being initialized.

## Files Modified

1. **`src/main.cpp`** (lines 544-573)
   - Changed from using `getSelectedNode()` to using `getRigRootName(rigRootModelIndex)` / `getRootNodeName(rigRootModelIndex)`
   - Added fallback logic to handle cases where rig root or RootNode might not be found
   - Updated variable name from `currentRigRootName` to `rigRootNameForModel` for clarity

## Testing

After the fix:
1. ✅ Load Model 0 and move it to (54.10, 0, 0)
2. ✅ Load Model 1
3. ✅ Model 1 should appear at (0, 0, 0) - **correct position**
4. ✅ Model 0 should remain at (54.10, 0, 0) - **unchanged**

## Related Features

- **RootNode Auto-Renaming**: Prevents name collisions by renaming RootNode to "RootNode01", "RootNode02", etc.
- **Per-Model RootNode Transforms**: Stores transforms per-model to prevent mixing
- **Model Initialization**: Initializes new models to (0,0,0) when first selected

## Summary

The bug was caused by using the **currently selected node** (which might be stale) instead of querying the **correct rig root name for the specific model being initialized**. The fix ensures that each model's rig root is checked using its own (possibly renamed) name, preventing new models from inheriting transforms from previous models.
