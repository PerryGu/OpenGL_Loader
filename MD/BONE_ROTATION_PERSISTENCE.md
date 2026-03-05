# Bone Rotation Visual Persistence Implementation

## Overview
This document tracks the implementation of visual persistence for bone rotations - ensuring that when a user changes a bone's rotation and switches to another bone, the deformation/pose remains visible in the viewport.

## Date: 2026-02-09

## Problem Statement
- Bone rotation values were being saved in PropertyPanel when switching bones (UI persistence)
- However, the visual deformation was not persisting - when switching bones, the model would revert to its default pose
- Goal: Make bone rotations persist visually in the viewport, so the character maintains its pose even when switching between bones

## Implementation Approach

### 1. Added `syncBoneRotations()` Method to Model Class

**File:** `src/graphics/model.h` and `src/graphics/model.cpp`

**Purpose:** Convert bone rotations from PropertyPanel (stored as `glm::vec3`) to the Model's internal format (`Matrix4f`) and store them in `m_BoneExtraRotations`.

**Implementation:**
```cpp
void Model::syncBoneRotations(const std::map<std::string, glm::vec3>& boneRotations)
{
    for (const auto& pair : boneRotations) {
        const std::string& boneName = pair.first;
        const glm::vec3& rotation = pair.second;
        
        Matrix4f rotationMatrix;
        rotationMatrix.InitRotateTransform(rotation.x, rotation.y, rotation.z);
        m_BoneExtraRotations[boneName] = rotationMatrix;
    }
}
```

### 2. Modified `changeAttributeManuale()` Method

**File:** `src/graphics/model.cpp`

**Original Behavior:**
- Only applied rotations to the currently selected bone
- Other bones used default (identity) rotation

**New Behavior:**
1. **First, check for saved rotations:** For ALL bones, check if they have a saved rotation in `m_BoneExtraRotations` and apply it
2. **Then, override for selected bone:** If this is the currently selected bone, override with the slider value (for real-time updates)

**Key Logic:**
- Try exact bone name match first
- If not found, try substring matching (in case bone names don't match exactly)
- If no saved rotation found, leave `RotationM_attr` unchanged (preserves original initialization)
- Selected bone always uses slider value for real-time feedback

### 3. Added Sync Call in Main Loop

**File:** `src/main.cpp`

**Location:** Before rendering, in the file loading section

**Implementation:**
```cpp
// Sync all saved bone rotations from PropertyPanel to all models
const auto& allBoneRotations = scene.mPropertyPanel.getAllBoneRotations();
for (size_t i = 0; i < modelManager.getModelCount(); i++) {
    ModelInstance* instance = modelManager.getModel(static_cast<int>(i));
    if (instance) {
        instance->model.syncBoneRotations(allBoneRotations);
    }
}
```

## Files Modified

1. `src/graphics/model.h` - Added `syncBoneRotations()` method declaration
2. `src/graphics/model.cpp` - Implemented `syncBoneRotations()` and modified `changeAttributeManuale()`
3. `src/main.cpp` - Added sync call before rendering

## Known Issues and Fixes

### Issue 1: Model Not Visible After Implementation
**Symptom:** Model disappears from viewport after loading
**Cause:** Incorrect handling of bones without saved rotations
**Fix:** Changed logic to only apply saved rotations if they exist, otherwise leave `RotationM_attr` unchanged

### Issue 2: Initial Broken Implementation
**Symptom:** Model appeared broken/deformed on load
**Cause:** Overwriting `RotationM_attr` with identity for bones without saved rotations
**Fix:** Reverted to preserving original initialization when no saved rotation exists

## Current Status

✅ Bone rotation values persist in UI when switching bones (from previous implementation)
✅ Bone rotations sync from PropertyPanel to Model before rendering
✅ Saved rotations are applied to all bones during transformation
✅ Selected bone uses slider value for real-time updates
✅ Bones without saved rotations use default behavior

### Issue Fixed: Only Bones with Animation Channels Were Working
**Problem:** Only "Spine1" was affecting the model when rotated. Other bones like "Spine", "Spine2", etc. had no effect.

**Root Cause:** `changeAttributeManuale()` was only called inside `if (pNodeAnim)`, which meant only bones with animation channels were processed. Bones without animation channels were never processed, so their rotations were never applied.

**Solution:**
- Moved `changeAttributeManuale()` call outside the `if (pNodeAnim)` block
- Now ALL bones are processed, regardless of whether they have animation channels
- Use `NodeName` from the hierarchy instead of `pNodeAnim->mNodeName` to ensure we have the bone name even without animation
- Added `else` block for bones without animation channels to apply manual rotations to their default transformation

### Issue Fixed: Pose "Reduction" When Switching Bones
**Problem:** When a bone (e.g., "Spine") has rotation 10 and you switch to another bone, the character's pose "reduces" (not at 100%). When switching back to the original bone, it returns to 100%.

**Root Cause:** The substring matching was too aggressive - when "Spine" was selected, it incorrectly matched "Spine1" and "Spine2", causing them to use "Spine"'s slider value. When switching away, those bones no longer matched and fell back to default.

**Solution:**
- Changed selected bone check to use exact match or safe suffix match only
- Safe suffix match requires the selected name to be at the end of the rig name, preceded by a space, digit, or non-alphanumeric character
- This prevents "Spine" from matching "Spine1" while still allowing "1810483883696 Spine" to match "Spine"
- Improved matching logic in both `changeAttributeManuale()` and `syncBoneRotations()`

## Final Implementation Summary

✅ **All bones now work** - Both bones with and without animation channels can be rotated
✅ **Rotations persist visually** - When you change a bone's rotation and switch to another bone, the pose remains at 100%
✅ **No more "jumps"** - The character's pose stays consistent when switching between bones
✅ **Proper name matching** - Handles cases where PropertyPanel names differ from rig bone names (e.g., numeric prefixes)

### Issue Fixed: Bone Name Mismatch
**Problem:** When switching bones, there was a small "jump" in the character's pose because:
- Selected bone used slider value directly
- Non-selected bones looked up saved rotation by `nodeNameInRig`
- Bone names might not match exactly between selection name and rig name

**Solution:** 
- Save rotation with both the selected bone name AND the rig name in `m_BoneExtraRotations`
- This ensures lookup works regardless of which name is used
- All bones now use `m_BoneExtraRotations` as the source of truth, with selected bone overriding for real-time updates

### Issue Fixed: Substring Matching Causing Incorrect Rotations
**Problem:** When "Spine" had rotation 10, switching to "Spine1" or "Spine2" would incorrectly apply "Spine"'s rotation because substring matching was too aggressive:
- "Spine" would match "Spine1", "Spine2", etc.
- This caused rotations to "leak" to child bones that shouldn't have them
- Character pose would "reduce" when switching to related bones

**Solution:**
- Removed substring matching for non-selected bones
- Use EXACT match only when looking up saved rotations
- This ensures "Spine" rotation only applies to "Spine", not "Spine1" or "Spine2"
- Selected bone still uses substring matching to find itself (for real-time updates)

## Testing Checklist

- [ ] Load model - should appear correctly
- [ ] Change bone rotation (e.g., Spine Y to 5) - should deform model
- [ ] Switch to another bone - original bone's deformation should remain visible
- [ ] Switch back to original bone - saved value should be displayed in UI
- [ ] Change multiple bones - all deformations should persist
- [ ] Reset bone to 0 - deformation should disappear

## Rollback Instructions

If issues occur, revert the following:

1. **In `src/graphics/model.cpp`:**
   - Revert `changeAttributeManuale()` to original simple version:
   ```cpp
   void Model::changeAttributeManuale(Matrix4f& RotationM_attr, UI_data ui_data, std::string nodeNameInRig)
   {
       std::string nodeNameSelected = ui_data.nodeName_toTrasform;
       if (nodeNameSelected != "") {
           if (nodeNameInRig.find(nodeNameSelected) != std::string::npos) {
               RotationM_attr.InitRotateTransform(ui_data.mSliderRotations.x, ui_data.mSliderRotations.y, ui_data.mSliderRotations.z);
               m_BoneExtraRotations[nodeNameSelected] = RotationM_attr;
           }
       }
   }
   ```

2. **In `src/graphics/model.h`:**
   - Remove `syncBoneRotations()` method declaration

3. **In `src/graphics/model.cpp`:**
   - Remove `syncBoneRotations()` implementation

4. **In `src/main.cpp`:**
   - Remove the sync loop before rendering

## Notes

- The PropertyPanel already handles saving/loading bone rotations in the UI (from previous implementation)
- This implementation adds the visual persistence layer
- Bone name matching uses both exact and substring matching to handle naming inconsistencies
- The selected bone always uses the slider value for immediate visual feedback
