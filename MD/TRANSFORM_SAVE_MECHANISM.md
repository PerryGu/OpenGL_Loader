# Transform Save Mechanism Analysis

## Overview
This document explains how transform data (Translations, Rotations, Scales) entered in the PropertyPanel Transforms window is saved, and compares the mechanism for RootNode vs regular bones.

## Date
Analysis Date: 2024

## Summary

**Key Finding:** RootNode uses the **same primary mechanism** as regular bones, but with **additional per-model storage** that regular bones don't have.

### Storage Mechanisms

#### 1. Regular Bones (Non-RootNode)
- **Storage:** In-memory only (not persisted to disk)
- **Location:** `PropertyPanel` class member variables:
  - `mBoneRotations` (map: bone name → rotation vec3)
  - `mBoneTranslations` (map: bone name → translation vec3)
  - `mBoneScales` (map: bone name → scale vec3)
- **Key Format:** Bone/node name (e.g., "LeftArm", "Spine", etc.)

#### 2. RootNode
- **Storage:** In-memory only (not persisted to disk)
- **Location:** TWO storage locations:
  1. **Same as regular bones:**
     - `mBoneRotations["RootNode"]`
     - `mBoneTranslations["RootNode"]`
     - `mBoneScales["RootNode"]`
  2. **Additional per-model storage:**
     - `mModelRootNodeTranslations[modelIndex]`
     - `mModelRootNodeRotations[modelIndex]`
     - `mModelRootNodeScales[modelIndex]`
- **Key Format:** 
  - Primary: "RootNode" (same as regular bones)
  - Per-model: Model index (0, 1, 2, etc.)

## Save Triggers

### When Transforms Are Saved

Transforms are saved automatically when:

1. **User changes slider values** (in UI):
   - Detected via epsilon comparison (0.001f threshold)
   - Saved immediately when change detected
   - Location: `propertyPanel()` method

2. **User switches bones** (in Outliner):
   - Current bone's values saved before switching
   - New bone's values loaded (or defaults if not previously set)
   - Location: `setSelectedNode()` method

3. **Gizmo manipulation**:
   - Gizmo updates PropertyPanel via `setSliderRotations/Translations/Scales()`
   - Values saved immediately
   - Location: `setSliderRotations()`, `setSliderTranslations()`, `setSliderScales()` methods

4. **RootNode-specific additional saves**:
   - When RootNode is selected AND values change, ALSO saves to per-model storage
   - Called via `saveRootNodeTransformsToCurrentModel()`
   - Ensures each model's RootNode transforms are isolated

## Code Flow

### Regular Bone Save Flow

```
User changes slider value
    ↓
propertyPanel() detects change (epsilon comparison)
    ↓
mBoneRotations[boneName] = mSliderRotations
mBoneTranslations[boneName] = mSliderTranslations
mBoneScales[boneName] = mSliderScales
    ↓
Values saved to map (in-memory)
```

### RootNode Save Flow

```
User changes slider value (RootNode selected)
    ↓
propertyPanel() detects change (epsilon comparison)
    ↓
mBoneRotations["RootNode"] = mSliderRotations
mBoneTranslations["RootNode"] = mSliderTranslations
mBoneScales["RootNode"] = mSliderScales
    ↓
Detects RootNode is selected
    ↓
saveRootNodeTransformsToCurrentModel() called
    ↓
mModelRootNodeTranslations[mCurrentModelIndex] = mSliderTranslations
mModelRootNodeRotations[mCurrentModelIndex] = mSliderRotations
mModelRootNodeScales[mCurrentModelIndex] = mSliderScales
    ↓
Values saved to BOTH locations (bone map + per-model storage)
```

## Implementation Details

### 1. Change Detection

**File:** `src/io/ui/property_panel.cpp`

**Method:** Epsilon comparison to detect value changes

```cpp
const float epsilon = 0.001f;
glm::vec3 previousTranslation = mSliderTranslations;
nimgui::draw_vec3_widgetx("Translations", mSliderTranslations, 100.0f);

bool translationChanged = false;
if (std::abs(mSliderTranslations.x - previousTranslation.x) > epsilon ||
    std::abs(mSliderTranslations.y - previousTranslation.y) > epsilon ||
    std::abs(mSliderTranslations.z - previousTranslation.z) > epsilon) {
    translationChanged = true;
}

if (!mSelectedNod_name.empty() && translationChanged) {
    mBoneTranslations[mSelectedNod_name] = mSliderTranslations;
    
    // RootNode-specific: Also save to per-model storage
    if (mSelectedNod_name.find("Root") != std::string::npos || 
        mSelectedNod_name == "RootNode") {
        saveRootNodeTransformsToCurrentModel();
    }
}
```

**Why epsilon comparison?**
- Prevents saving on every frame (even when values haven't changed)
- Only saves when user actually modifies values
- Reduces unnecessary map updates

### 2. Bone Switching

**File:** `src/io/ui/property_panel.cpp`

**Method:** `setSelectedNode()`

```cpp
void PropertyPanel::setSelectedNode(std::string selectedNode)
{
    if (selectedNode != mSelectedNod_name) {
        // Save current bone's values BEFORE switching
        if (!mSelectedNod_name.empty()) {
            mBoneRotations[mSelectedNod_name] = mSliderRotations;
            mBoneTranslations[mSelectedNod_name] = mSliderTranslations;
            mBoneScales[mSelectedNod_name] = mSliderScales;
        }
        
        // Update selected node
        mSelectedNod_name = selectedNode;
        
        // Load new bone's values (or defaults)
        auto rotIt = mBoneRotations.find(selectedNode);
        if (rotIt != mBoneRotations.end()) {
            mSliderRotations = rotIt->second;
        } else {
            mSliderRotations = glm::vec3(0.0f, 0.0f, 0.0f);
        }
        // ... same for translations and scales ...
    }
}
```

**Key Points:**
- Saves current bone's values before switching
- Loads new bone's saved values (or defaults if not previously set)
- Works for both regular bones and RootNode

### 3. Gizmo Updates

**File:** `src/io/ui/property_panel.cpp`

**Methods:** `setSliderRotations()`, `setSliderTranslations()`, `setSliderScales()`

```cpp
void PropertyPanel::setSliderRotations(const glm::vec3& rotations)
{
    mSliderRotations = rotations;
    
    // Save to bone rotations map
    if (!mSelectedNod_name.empty()) {
        mBoneRotations[mSelectedNod_name] = rotations;
        
        // RootNode-specific: Also save to per-model storage
        if (mSelectedNod_name.find("Root") != std::string::npos || 
            mSelectedNod_name == "RootNode") {
            saveRootNodeTransformsToCurrentModel();
        }
    }
}
```

**Key Points:**
- Called by gizmo when user manipulates transforms
- Saves to bone map immediately
- RootNode also saves to per-model storage

### 4. Per-Model RootNode Storage

**File:** `src/io/ui/property_panel.cpp`

**Method:** `saveRootNodeTransformsToCurrentModel()`

```cpp
void PropertyPanel::saveRootNodeTransformsToCurrentModel()
{
    if (mCurrentModelIndex < 0) {
        return;  // No model set, can't save
    }
    
    // Only save if RootNode is currently selected
    if (mSelectedNod_name.empty() || 
        (mSelectedNod_name.find("Root") == std::string::npos && 
         mSelectedNod_name != "RootNode")) {
        return;  // Not RootNode, don't save
    }
    
    // Save current slider values to current model's storage
    mModelRootNodeTranslations[mCurrentModelIndex] = mSliderTranslations;
    mModelRootNodeRotations[mCurrentModelIndex] = mSliderRotations;
    mModelRootNodeScales[mCurrentModelIndex] = mSliderScales;
}
```

**Purpose:**
- Isolates RootNode transforms per-model
- Prevents transforms from mixing between different models
- Required for multi-model support

## Comparison: RootNode vs Regular Bones

| Aspect | Regular Bones | RootNode |
|--------|--------------|----------|
| **Primary Storage** | `mBoneRotations[name]` | `mBoneRotations["RootNode"]` |
| | `mBoneTranslations[name]` | `mBoneTranslations["RootNode"]` |
| | `mBoneScales[name]` | `mBoneScales["RootNode"]` |
| **Additional Storage** | None | `mModelRootNodeTranslations[index]` |
| | | `mModelRootNodeRotations[index]` |
| | | `mModelRootNodeScales[index]` |
| **Save Triggers** | Slider change, bone switch, gizmo | Same + per-model save |
| **Persistence** | In-memory only | In-memory only |
| **Per-Model Isolation** | No (shared across models) | Yes (isolated per model) |

## Why RootNode Has Additional Storage

RootNode requires per-model storage because:

1. **Multi-Model Support**: When multiple FBX files are loaded, each model needs its own RootNode transform
2. **Transform Isolation**: Without per-model storage, RootNode transforms would mix between models
3. **Gizmo Positioning**: Gizmo needs to know which model's RootNode is selected to position correctly
4. **Model Switching**: When switching between models' RootNodes, each model's transforms must be preserved

Regular bones don't need per-model storage because:
- Bone names are typically unique per model (or can be disambiguated)
- Bone transforms are applied within each model's bone hierarchy
- No need for model-level isolation

## Data Persistence

### Current State: In-Memory Only

**Important:** Transform data is **NOT persisted to disk**. All transforms are stored in memory only and are lost when:
- Application closes
- "New Scene" is executed (clears all transforms)
- Model is removed

### What IS Persisted to Disk

The following settings ARE saved to `app_settings.json`:
- Camera state (position, rotation, zoom)
- Window position and size
- Grid settings (size, spacing, colors, enabled state)
- Environment settings (background color, far clipping plane)
- Recent files list

### What IS NOT Persisted to Disk

The following transform data is NOT saved:
- Bone rotations/translations/scales
- RootNode transforms
- Model positions/rotations/scales
- Character poses

## Future Enhancement: Disk Persistence

To add disk persistence for transforms, you would need to:

1. **Add to AppSettings structure:**
   ```cpp
   struct AppSettings {
       // ... existing settings ...
       std::map<std::string, std::map<std::string, glm::vec3>> boneTransforms;  // model file -> bone name -> transform
   };
   ```

2. **Save on exit:**
   ```cpp
   // In main.cpp cleanup
   appSettings.boneTransforms = scene.mPropertyPanel.getAllBoneTransforms();
   ```

3. **Load on startup:**
   ```cpp
   // After loading model
   scene.mPropertyPanel.loadBoneTransforms(appSettings.boneTransforms[filePath]);
   ```

4. **Handle per-model RootNode transforms:**
   - Save per-model RootNode transforms with model index or file path
   - Load and restore when models are loaded

## Code Locations

### PropertyPanel Class
- **Header:** `src/io/ui/property_panel.h`
- **Implementation:** `src/io/ui/property_panel.cpp`
- **Key Methods:**
  - `propertyPanel()` - UI rendering and change detection
  - `setSelectedNode()` - Bone switching and save/load
  - `setSliderRotations/Translations/Scales()` - Gizmo updates
  - `saveRootNodeTransformsToCurrentModel()` - Per-model RootNode save

### Storage Variables
- **Regular bones:** `mBoneRotations`, `mBoneTranslations`, `mBoneScales` (maps)
- **RootNode per-model:** `mModelRootNodeTranslations`, `mModelRootNodeRotations`, `mModelRootNodeScales` (maps keyed by model index)

## Testing

To verify the save mechanism:

1. **Test Regular Bone:**
   - Select a bone (e.g., "LeftArm")
   - Change rotation to (45, 0, 0)
   - Switch to another bone
   - Switch back to "LeftArm"
   - Verify rotation is still (45, 0, 0)

2. **Test RootNode:**
   - Select RootNode
   - Change translation to (10, 5, 0)
   - Load another model
   - Select first model's RootNode again
   - Verify translation is still (10, 5, 0)

3. **Test Multi-Model:**
   - Load two models
   - Select Model 0 RootNode, set translation to (10, 0, 0)
   - Select Model 1 RootNode, set translation to (20, 0, 0)
   - Switch back to Model 0 RootNode
   - Verify translation is (10, 0, 0) - not (20, 0, 0)

## Summary

1. **Same Primary Mechanism:** Both RootNode and regular bones use the same primary storage (`mBoneRotations`, `mBoneTranslations`, `mBoneScales` maps)

2. **RootNode Has Additional Storage:** RootNode also uses per-model storage (`mModelRootNodeTranslations/Rotations/Scales`) to isolate transforms between different models

3. **In-Memory Only:** All transform data is stored in memory only - not persisted to disk

4. **Automatic Saving:** Transforms are saved automatically when:
   - User changes slider values
   - User switches bones
   - Gizmo manipulates transforms

5. **Per-Model Isolation:** RootNode transforms are isolated per-model to support multi-model workflows
