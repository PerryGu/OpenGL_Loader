# Translation Controls Implementation

## Overview
This document describes the implementation of translation controls (X, Y, Z) in the Transforms window, allowing users to manually translate bones/nodes in addition to rotating them.

## Date: 2026-02-09

## Problem Statement
Users wanted to be able to translate bones/nodes in addition to rotating them. The Transforms window already had rotation controls (X, Y, Z), and users requested similar translation controls to modify the position of selected objects.

## Implementation Details

### 1. PropertyPanel Changes

**File:** `src/io/ui/property_panel.h`

**Added Members:**
- `glm::vec3 mSliderTranslations = glm::vec3(0.0f, 0.0f, 0.0f)` - Current translation slider values
- `std::map<std::string, glm::vec3> mBoneTranslations` - Map to store translations for each bone/node

**Added Methods:**
- `glm::vec3 getSliderTrans_update()` - Get current translation slider values
- `const std::map<std::string, glm::vec3>& getAllBoneTranslations() const` - Get all saved translations
- `void clearAllBoneTranslations()` - Clear all saved translations

**File:** `src/io/ui/property_panel.cpp`

**Changes:**
- Added "Translations" section in the UI (similar to "Rotations")
- Added translation sliders using `nimgui::draw_vec3_widgetx("Translations", mSliderTranslations, 100.0f)`
- Implemented change detection and saving logic (similar to rotations)
- Updated `setSelectedNode()` to save/load translations when switching bones
- Updated `resetCurrentBone()` to reset both rotations and translations
- Updated `resetAllBones()` to clear both rotations and translations

### 2. Model Class Changes

**File:** `src/graphics/model.h`

**Added Members:**
- `std::map<std::string, Matrix4f> m_BoneExtraTranslations` - Map to store manual translation matrices for bones

**Added Methods:**
- `void syncBoneTranslations(const std::map<std::string, glm::vec3>& boneTranslations)` - Sync translations from PropertyPanel
- `void clearAllBoneTranslations()` - Clear all saved translations
- `void changeTranslationManuale(Matrix4f& TranslationM_attr, UI_data ui_data, std::string nodeNameInRig)` - Apply manual translations

**File:** `src/graphics/model.cpp`

**Changes:**
- **`ReadNodeHierarchy` (lines 759-787):**
  - Added `TranslationM_attr` initialization (identity matrix)
  - Added call to `changeTranslationManuale()` for ALL bones (both with and without animation channels)
  - For bones with animation: Combined animation translation with manual translation (additive)
  - For bones without animation: Applied manual translation to default transformation
  
- **`changeTranslationManuale` (lines 949-1066):**
  - Similar logic to `changeAttributeManuale` but for translations
  - Uses the same bone name matching logic (exact match + safe suffix match)
  - Applies real-time translation for selected bone
  - Loads saved translations for non-selected bones
  
- **`syncBoneTranslations` (lines 1188-1250):**
  - Converts `glm::vec3` translations from PropertyPanel to `Matrix4f`
  - Stores translations in `m_BoneExtraTranslations` map
  - Uses the same bone name matching logic as `syncBoneRotations`
  - Removes zero translations from the map

- **`clearAllBoneTranslations` (lines 953-960):**
  - Clears `m_BoneExtraTranslations` map
  - Called when "Reset All Bones" is clicked

### 3. Main Loop Changes

**File:** `src/main.cpp`

**Changes:**
- Added `ui_data.mSliderTraslations = scene.mPropertyPanel.getSliderTrans_update();` to update UI_data
- Updated "Reset All Bones" handler to also call `clearAllBoneTranslations()`
- Added translation syncing loop (similar to rotation syncing):
  ```cpp
  const auto& allBoneTranslations = scene.mPropertyPanel.getAllBoneTranslations();
  for (size_t i = 0; i < modelManager.getModelCount(); i++) {
      ModelInstance* instance = modelManager.getModel(static_cast<int>(i));
      if (instance) {
          instance->model.syncBoneTranslations(allBoneTranslations);
      }
  }
  ```

## How It Works

1. **UI Interaction:**
   - User selects a bone in the Outliner
   - Transforms window shows both "Rotations" and "Translations" sections
   - User adjusts translation sliders (X, Y, Z)
   - Changes are detected and saved to `mBoneTranslations` map

2. **Real-Time Application:**
   - When a bone is selected, `changeTranslationManuale()` applies the slider values immediately
   - Translation is applied as a transformation matrix
   - For bones with animation: Manual translation is added to animation translation (additive)
   - For bones without animation: Manual translation is applied to default transformation

3. **Persistence:**
   - When switching bones, current translation is saved to `mBoneTranslations` map
   - When selecting a bone, its saved translation is loaded (or defaults to 0,0,0)
   - `syncBoneTranslations()` ensures all saved translations persist visually across all bones

4. **Reset Functionality:**
   - "Reset Current Bone" resets both rotations and translations for the selected bone
   - "Reset All Bones" clears all saved rotations and translations from all bones

## Key Features

- **Translation Sliders** - X, Y, Z controls similar to rotation sliders
- **Per-Bone Storage** - Each bone's translation is stored separately
- **Visual Persistence** - Translations persist when switching bones
- **Additive with Animation** - Manual translations are added to animation translations
- **Reset Support** - Both individual and global reset functionality

## Technical Notes

- Translations are stored as `glm::vec3` in PropertyPanel and converted to `Matrix4f` in Model
- Uses the same bone name matching logic as rotations (exact match + safe suffix match)
- Translation matrices are applied additively with animation translations
- Zero translations are removed from the map to allow bones to return to default state
- The `UI_data` structure already had `mSliderTraslations` (note the typo in the original code)

## Testing Checklist

- [ ] Load a model with bones
- [ ] Select a bone in the Outliner
- [ ] Adjust translation sliders (X, Y, Z) - bone should move in viewport
- [ ] Switch to another bone - previous bone's translation should persist visually
- [ ] Switch back to original bone - translation values should be restored in UI
- [ ] Click "Reset Current Bone" - selected bone should reset to default position
- [ ] Click "Reset All Bones" - all bones should reset to default positions
- [ ] Test with bones that have animation channels
- [ ] Test with bones that don't have animation channels

## Known Limitations

- Translation values are not saved to external files (only persist during runtime)
- The `UI_data` structure uses `mSliderTraslations` (typo) instead of `mSliderTranslations`
- Translation is applied additively with animation, which may cause unexpected results if animation already has translation

## Future Improvements

- Could add translation limits/min/max values
- Could add visual feedback (gizmos) for translation in viewport
- Could save translations to external file for persistence across sessions
- Could add scale controls (X, Y, Z) in a similar manner
