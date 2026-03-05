# Removal of Dual Storage System for RootNode

## Overview
With automatic RootNode renaming in place, each RootNode now has a unique name. This eliminates the need for the dual storage system (bone maps + per-model storage). RootNode now uses the same storage mechanism as regular bones.

## Date
Removal Date: 2024

## What Was Removed

### 1. PropertyPanel Header (`src/io/ui/property_panel.h`)

**Removed Methods:**
- `saveRootNodeTransformsForModel(int modelIndex)`
- `loadRootNodeTransformsForModel(int modelIndex)`
- `hasRootNodeTransformsForModel(int modelIndex)`
- `getRootNodeTransformsForModel(int modelIndex, ...)`
- `setCurrentModelIndex(int modelIndex)`
- `getCurrentModelIndex()`
- `saveRootNodeTransformsToCurrentModel()`

**Removed Member Variables:**
- `mModelRootNodeTranslations` (map: model index → translation)
- `mModelRootNodeRotations` (map: model index → rotation)
- `mModelRootNodeScales` (map: model index → scale)
- `mCurrentModelIndex` (tracks which model's RootNode is active)

### 2. PropertyPanel Implementation (`src/io/ui/property_panel.cpp`)

**Removed Code:**
- All per-model save/load methods (entire implementations removed)
- All calls to `saveRootNodeTransformsToCurrentModel()` from:
  - Translation change handler
  - Rotation change handler
  - Scale change handler
  - `setSliderRotations()`
  - `setSliderTranslations()`
  - `setSliderScales()`
- Per-model storage clearing from `clearAllTransforms()`

### 3. Main Application (`src/main.cpp`)

**Removed Code:**
- Per-model RootNode save/load logic in RootNode selection handler
- `setCurrentModelIndex()` calls
- `saveRootNodeTransformsForModel()` calls
- `loadRootNodeTransformsForModel()` calls
- `hasRootNodeTransformsForModel()` checks

**Simplified Logic:**
- RootNode selection now just updates the selected model
- PropertyPanel automatically loads correct transforms from bone maps via `setSelectedNode()`
- No explicit per-model save/load needed

## What Remains (Bone Maps Only)

### Storage Mechanism
RootNode now uses **only** the bone maps, same as regular bones:
- `mBoneRotations[uniqueRootNodeName]`
- `mBoneTranslations[uniqueRootNodeName]`
- `mBoneScales[uniqueRootNodeName]`

### How It Works

1. **First Model Loaded:**
   - RootNode name: "RootNode"
   - Saved to: `mBoneTranslations["RootNode"]`

2. **Second Model Loaded:**
   - RootNode name: "RootNode01" (auto-renamed)
   - Saved to: `mBoneTranslations["RootNode01"]`

3. **Third Model Loaded:**
   - RootNode name: "RootNode02" (auto-renamed)
   - Saved to: `mBoneTranslations["RootNode02"]`

Each RootNode has a **unique key** in the bone maps, so no collisions!

## Benefits

1. **Simpler Code**: One storage mechanism instead of two
2. **Consistent**: RootNode works exactly like regular bones
3. **Less Memory**: No duplicate storage per model
4. **Easier to Maintain**: Less code to maintain and debug
5. **Automatic**: Bone maps handle everything automatically

## How It Works Now

### When User Changes RootNode Transform

```cpp
// User changes "RootNode01" translation to (10, 0, 0)
mBoneTranslations["RootNode01"] = glm::vec3(10.0f, 0.0f, 0.0f);
// That's it! No per-model storage needed.
```

### When User Switches RootNodes

```cpp
// User switches from "RootNode" to "RootNode01"
// PropertyPanel::setSelectedNode() is called
// It automatically:
// 1. Saves current bone's values: mBoneTranslations["RootNode"] = currentValue
// 2. Loads new bone's values: mSliderTranslations = mBoneTranslations["RootNode01"]
// Works exactly like regular bones!
```

### When Model is Selected via Viewport

```cpp
// User clicks on Model 1 in viewport
// Outliner selects "RootNode01" (unique name for Model 1)
// PropertyPanel::setSelectedNode("RootNode01") is called
// Automatically loads: mBoneTranslations["RootNode01"]
// No per-model logic needed!
```

## Code Flow Comparison

### Before (Dual Storage)
```
User changes RootNode transform
    ↓
Save to bone map: mBoneTranslations["RootNode"]
    ↓
Also save to per-model: mModelRootNodeTranslations[modelIndex]
    ↓
Two storage locations maintained
```

### After (Single Storage)
```
User changes RootNode transform
    ↓
Save to bone map: mBoneTranslations["RootNode01"]
    ↓
Done! (unique name = no collision)
```

## Testing

To verify the simplified system works:

1. **Load Multiple Models:**
   - Load model1.fbx → RootNode name: "RootNode"
   - Load model2.fbx → RootNode name: "RootNode01"
   - Load model3.fbx → RootNode name: "RootNode02"

2. **Set Different Transforms:**
   - Select "RootNode" → Set translation to (10, 0, 0)
   - Select "RootNode01" → Set translation to (20, 0, 0)
   - Select "RootNode02" → Set translation to (30, 0, 0)

3. **Switch Between RootNodes:**
   - Switch back to "RootNode" → Should show (10, 0, 0) ✅
   - Switch to "RootNode01" → Should show (20, 0, 0) ✅
   - Switch to "RootNode02" → Should show (30, 0, 0) ✅

4. **Verify No Mixing:**
   - Each RootNode maintains its own transform independently
   - No transforms leak between models

## Files Modified

1. ✅ `src/io/ui/property_panel.h` - Removed per-model methods and member variables
2. ✅ `src/io/ui/property_panel.cpp` - Removed per-model implementations and calls
3. ✅ `src/main.cpp` - Simplified RootNode selection handling

## Summary

The dual storage system has been completely removed. RootNode now uses the same simple storage mechanism as regular bones:
- **One storage location**: Bone maps only
- **Unique keys**: Each RootNode has a unique name (auto-renamed)
- **Automatic handling**: PropertyPanel's `setSelectedNode()` handles save/load automatically
- **Simpler code**: Less code, easier to maintain, consistent behavior

The system is now cleaner, simpler, and more maintainable while providing the same functionality!
