# Reset Buttons Implementation

## Overview
This document describes the implementation of two reset buttons in the Transforms window:
1. **Reset Current Bone** - Resets only the currently selected bone to (0, 0, 0)
2. **Reset All Bones** - Resets all bones of the rig to (0, 0, 0)

## Date: 2026-02-09

## Implementation Details

### 1. PropertyPanel Changes

**File:** `src/io/ui/property_panel.h` and `src/io/ui/property_panel.cpp`

**Added Methods:**
- `void resetCurrentBone()` - Resets the currently selected bone's rotation to (0, 0, 0)
- `void resetAllBones()` - Resets all bones' rotations to (0, 0, 0)
- `bool isResetAllBonesRequested()` - Checks if "Reset All Bones" was requested (for clearing Model rotations)
- `void clearResetAllBonesRequest()` - Clears the reset request flag

**Added Member:**
- `bool mResetAllBonesRequested = false` - Flag to signal that Model rotations should be cleared

**UI Changes:**
- Added two buttons in the Transforms section:
  - "Reset Current Bone" button
  - "Reset All Bones" button (placed on the same line using `ImGui::SameLine()`)

### 2. Model Changes

**File:** `src/graphics/model.h` and `src/graphics/model.cpp`

**Added Method:**
- `void clearAllBoneRotations()` - Clears all bone rotations from `m_BoneExtraRotations`, resetting all bones to their default (identity) rotation

**Implementation:**
```cpp
void Model::clearAllBoneRotations()
{
    // Clear all saved rotations - this will make all bones use their default (identity) rotation
    m_BoneExtraRotations.clear();
    
    std::cout << "[Model] Cleared all bone rotations, reset to default" << std::endl;
}
```

### 3. Main Loop Changes

**File:** `src/main.cpp`

**Added Logic:**
- Before syncing bone rotations, check if "Reset All Bones" was requested
- If requested, call `clearAllBoneRotations()` on all models
- Clear the request flag after processing

**Implementation:**
```cpp
// Check if "Reset All Bones" was requested
if (scene.mPropertyPanel.isResetAllBonesRequested()) {
    // Clear all bone rotations from all models
    for (size_t i = 0; i < modelManager.getModelCount(); i++) {
        ModelInstance* instance = modelManager.getModel(static_cast<int>(i));
        if (instance) {
            instance->model.clearAllBoneRotations();
        }
    }
    // Clear the request flag
    scene.mPropertyPanel.clearResetAllBonesRequest();
}
```

## How It Works

### Reset Current Bone
1. User clicks "Reset Current Bone" button
2. `resetCurrentBone()` is called:
   - Sets `mSliderRotations` to (0, 0, 0)
   - Sets the bone's entry in `mBoneRotations` map to (0, 0, 0)
3. On the next frame, `syncBoneRotations()` is called
4. Since the rotation is zero, `syncBoneRotations()` removes it from `m_BoneExtraRotations` in the Model
5. The bone returns to its default pose

### Reset All Bones
1. User clicks "Reset All Bones" button
2. `resetAllBones()` is called:
   - Clears the entire `mBoneRotations` map
   - Resets `mSliderRotations` to (0, 0, 0) if a bone is selected
   - Sets `mResetAllBonesRequested = true`
3. On the next frame, main loop checks `isResetAllBonesRequested()`
4. If true, calls `clearAllBoneRotations()` on all models
5. Clears the request flag
6. All bones return to their default pose

## Files Modified

1. `src/io/ui/property_panel.h` - Added method declarations and flag
2. `src/io/ui/property_panel.cpp` - Implemented reset methods and added UI buttons
3. `src/graphics/model.h` - Added `clearAllBoneRotations()` declaration
4. `src/graphics/model.cpp` - Implemented `clearAllBoneRotations()`
5. `src/main.cpp` - Added check for reset request and clearing of Model rotations

## Testing Checklist

- [ ] Load a model with bones
- [ ] Change rotation of a bone (e.g., Spine Y to 10)
- [ ] Click "Reset Current Bone" - the selected bone should return to default pose
- [ ] Change rotations of multiple bones
- [ ] Click "Reset All Bones" - all bones should return to default pose
- [ ] Verify that switching between bones after reset shows (0, 0, 0) values
- [ ] Verify that the model's pose returns to its original animation pose

## Notes

- The "Reset Current Bone" button only affects the bone that is currently selected in the Outliner
- The "Reset All Bones" button affects all bones in all loaded models
- Both buttons immediately update the UI sliders to (0, 0, 0)
- The visual reset happens on the next frame after clicking the button
- Zero rotations are automatically removed from the Model's `m_BoneExtraRotations` map during `syncBoneRotations()`, which is why "Reset Current Bone" works without explicitly clearing Model rotations
