# Why RootNode Needs Two Storage Locations

## The Core Problem

**RootNode has the SAME NAME across ALL models**, while regular bones may have unique names or can be shared.

## The Name Collision Issue

### Scenario: Multiple Models with RootNode

When you load multiple FBX files:
- **Model 0** has a node called `"RootNode"`
- **Model 1** has a node called `"RootNode"`  
- **Model 2** has a node called `"RootNode"`

All models use the **exact same name** for their root node!

### What Happens with Only One Storage (Bone Maps)

If RootNode only used the bone maps (like regular bones):

```cpp
// Model 0: User sets RootNode translation to (10, 0, 0)
mBoneTranslations["RootNode"] = glm::vec3(10.0f, 0.0f, 0.0f);
// ✅ Model 0's RootNode is now at (10, 0, 0)

// User switches to Model 1
// Model 1: User sets RootNode translation to (20, 0, 0)
mBoneTranslations["RootNode"] = glm::vec3(20.0f, 0.0f, 0.0f);
// ❌ PROBLEM: This OVERWRITES the previous value!
// Now mBoneTranslations["RootNode"] = (20, 0, 0) for BOTH models!

// User switches back to Model 0
// Model 0 loads: mBoneTranslations["RootNode"]
// ❌ Gets (20, 0, 0) instead of (10, 0, 0) - WRONG!
```

**Result:** Model 0's RootNode position is lost and replaced with Model 1's position!

### Why Regular Bones Don't Have This Problem

Regular bones have two scenarios:

#### Scenario 1: Unique Names Per Model
Many FBX files include unique prefixes or IDs in bone names:
- Model 0: `"1810483883696 LeftArm"` or `"1810483883696LeftArm"`
- Model 1: `"1810483883697 LeftArm"` or `"1810483883697LeftArm"`

These are **different keys** in the map, so no collision:
```cpp
mBoneRotations["1810483883696 LeftArm"] = (45, 0, 0);  // Model 0
mBoneRotations["1810483883697 LeftArm"] = (90, 0, 0);  // Model 1
// ✅ No collision - different keys!
```

#### Scenario 2: Shared Names (Intentional Sharing)
Some bones might have the same name across models (e.g., `"LeftArm"`), but this is **intentional**:
- If you rotate `"LeftArm"` on Model 0, you might WANT Model 1's `"LeftArm"` to also rotate
- This creates consistent poses across multiple characters
- The sharing is a **feature**, not a bug

Looking at the sync code in `main.cpp`:
```cpp
// Sync all saved bone rotations from PropertyPanel to all models
const auto& allBoneRotations = scene.mPropertyPanel.getAllBoneRotations();
for (size_t i = 0; i < modelManager.getModelCount(); i++) {
    instance->model.syncBoneRotations(allBoneRotations);
}
```

This **intentionally** applies the same bone transforms to ALL models - if Model 0's "LeftArm" is rotated, Model 1's "LeftArm" (if it exists) will also be rotated.

## Why RootNode Can't Be Shared

RootNode represents the **entire model's position in world space**. Each model needs its **own independent position**:

- Model 0 might be at position (10, 0, 0)
- Model 1 might be at position (20, 0, 0)
- Model 2 might be at position (0, 5, -10)

If RootNode transforms were shared:
- Moving Model 0 would also move Model 1 and Model 2
- Models would be stuck at the same position
- Multi-model scenes would be impossible

## The Solution: Two Storage Locations

### Storage 1: Bone Maps (Shared)
```cpp
mBoneTranslations["RootNode"] = currentSliderValue;
```

**Purpose:** 
- Maintains compatibility with existing code
- Used for UI display (sliders show this value)
- Used when RootNode is selected (for gizmo, etc.)

**Problem:** 
- Only one value can be stored (shared across all models)
- Gets overwritten when switching models

### Storage 2: Per-Model Storage (Isolated)
```cpp
mModelRootNodeTranslations[modelIndex] = currentSliderValue;
```

**Purpose:**
- Stores RootNode transforms **separately for each model**
- Model 0: `mModelRootNodeTranslations[0] = (10, 0, 0)`
- Model 1: `mModelRootNodeTranslations[1] = (20, 0, 0)`
- Model 2: `mModelRootNodeTranslations[2] = (0, 5, -10)`

**Solution:**
- Each model's RootNode transform is preserved independently
- No overwriting when switching models
- Enables proper multi-model support

## How It Works Together

### When User Changes RootNode Transform

```cpp
// User changes Model 0's RootNode translation to (10, 0, 0)
mBoneTranslations["RootNode"] = glm::vec3(10.0f, 0.0f, 0.0f);  // For UI/gizmo
mModelRootNodeTranslations[0] = glm::vec3(10.0f, 0.0f, 0.0f); // For Model 0
```

### When User Switches to Model 1

```cpp
// Before switching: Save Model 0's current value
mModelRootNodeTranslations[0] = mBoneTranslations["RootNode"];  // Save (10, 0, 0)

// Load Model 1's saved value
mBoneTranslations["RootNode"] = mModelRootNodeTranslations[1];  // Load (20, 0, 0)
```

### When User Switches Back to Model 0

```cpp
// Before switching: Save Model 1's current value
mModelRootNodeTranslations[1] = mBoneTranslations["RootNode"];  // Save (20, 0, 0)

// Load Model 0's saved value
mBoneTranslations["RootNode"] = mModelRootNodeTranslations[0];  // Load (10, 0, 0) ✅
```

## Visual Example

```
Bone Maps (Shared):
┌─────────────────────┐
│ "RootNode" → (20,0,0)│  ← Only ONE value (gets overwritten)
└─────────────────────┘

Per-Model Storage (Isolated):
┌─────────────────────┐
│ [0] → (10, 0, 0)    │  ← Model 0's position
│ [1] → (20, 0, 0)    │  ← Model 1's position
│ [2] → (0, 5, -10)   │  ← Model 2's position
└─────────────────────┘
```

## Summary

**RootNode needs two storage locations because:**

1. **Name Collision:** All models use the same name "RootNode", causing conflicts in the shared bone map
2. **Independence Required:** Each model needs its own position (can't be shared like regular bones)
3. **UI Compatibility:** Bone maps are still needed for UI display and gizmo interaction
4. **Per-Model Isolation:** Per-model storage preserves each model's RootNode transform independently

**Regular bones don't need this because:**

1. **Unique Names:** Often have unique prefixes/IDs per model (different keys in map)
2. **Intentional Sharing:** When names match, sharing is often desired (consistent poses)
3. **No Position Conflict:** Bone transforms are relative to the model, not world space

## Code Evidence

### RootNode Detection and Dual Save
```cpp
// In propertyPanel() when translation changes:
if (!mSelectedNod_name.empty() && translationChanged) {
    mBoneTranslations[mSelectedNod_name] = mSliderTranslations;  // Save to bone map
    
    // If RootNode is selected, ALSO save to per-model storage
    if (mSelectedNod_name.find("Root") != std::string::npos || 
        mSelectedNod_name == "RootNode") {
        saveRootNodeTransformsToCurrentModel();  // Additional save!
    }
}
```

### Per-Model Save Implementation
```cpp
void PropertyPanel::saveRootNodeTransformsToCurrentModel()
{
    if (mCurrentModelIndex < 0) return;
    
    // Save to per-model storage (isolated per model)
    mModelRootNodeTranslations[mCurrentModelIndex] = mSliderTranslations;
    mModelRootNodeRotations[mCurrentModelIndex] = mSliderRotations;
    mModelRootNodeScales[mCurrentModelIndex] = mSliderScales;
}
```

This dual-storage approach ensures RootNode transforms are preserved correctly for each model while maintaining compatibility with the existing bone transform system.
