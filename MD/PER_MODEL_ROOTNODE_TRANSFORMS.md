# Per-Model RootNode Transform System

## Overview
This document describes the implementation of per-model RootNode transform storage, which prevents transform values from mixing between different models when multiple FBX files are loaded. Previously, RootNode transforms were stored globally, causing transforms from one model to affect another when switching between RootNodes in the Outliner.

## Date
Implementation Date: 2026-02-09

## Problem Statement
When loading multiple FBX files and selecting RootNodes from different models in the Outliner:
1. **Gizmo doesn't move**: When selecting RootNode of one model, then selecting RootNode of another model, the gizmo stayed on the first model instead of moving to the second
2. **Transforms mix between models**: Transform values changed for one model would appear on another model when switching RootNodes
3. **Models jump/swap positions**: When deselecting in the viewport, models would jump to each other's positions or swap places
4. **No model tracking**: The system didn't track which model's RootNode was currently selected, so transforms were applied to the wrong model

## Root Cause
1. **Global Transform Storage**: PropertyPanel stored RootNode transforms globally (one set for all models), not per-model
2. **No Model Selection Update**: When RootNode was selected in Outliner, the selected model wasn't updated to match
3. **No Transform Isolation**: Transforms weren't saved/loaded per-model when switching between RootNodes
4. **Gizmo Used Wrong Model**: Gizmo always used `getSelectedModel()` which didn't update when selecting from Outliner

## Solution Applied

### 1. Added Per-Model RootNode Transform Storage

**File:** `src/io/ui/property_panel.h`

**Added Storage:**
```cpp
// Per-model RootNode transforms (model index -> RootNode transforms)
// This prevents RootNode transforms from mixing between different models
std::map<int, glm::vec3> mModelRootNodeTranslations;  // Model index -> translation
std::map<int, glm::vec3> mModelRootNodeRotations;     // Model index -> rotation (Euler angles)
std::map<int, glm::vec3> mModelRootNodeScales;       // Model index -> scale

// Current model index for RootNode transforms (tracks which model's RootNode is active)
int mCurrentModelIndex = -1;
```

**Purpose:**
- Stores RootNode transforms separately for each model
- Tracks which model's RootNode is currently active
- Prevents transforms from mixing between models

### 2. Added Methods for Per-Model Transform Management

**File:** `src/io/ui/property_panel.h` and `src/io/ui/property_panel.cpp`

**New Methods:**
```cpp
// Save RootNode transforms for a specific model
void saveRootNodeTransformsForModel(int modelIndex);

// Load RootNode transforms for a specific model
void loadRootNodeTransformsForModel(int modelIndex);

// Check if RootNode transforms exist for a specific model
bool hasRootNodeTransformsForModel(int modelIndex) const;

// Set/get current model index
void setCurrentModelIndex(int modelIndex);
int getCurrentModelIndex() const;

// Save RootNode transforms to current model (called when transforms change)
void saveRootNodeTransformsToCurrentModel();
```

**How They Work:**
- `saveRootNodeTransformsForModel()`: Saves current RootNode slider values to the specified model's storage
- `loadRootNodeTransformsForModel()`: Loads RootNode transforms from the specified model's storage into sliders
- `saveRootNodeTransformsToCurrentModel()`: Automatically saves transforms to current model when they change (via gizmo or sliders)

### 3. Added Method to Find Model for Selected Object

**File:** `src/io/ui/outliner.h` and `src/io/ui/outliner.cpp`

**New Method:**
```cpp
// Find which model index a selected object belongs to
int findModelIndexForSelectedObject() const;
```

**Implementation:**
```cpp
int Outliner::findModelIndexForSelectedObject() const
{
    if (!g_selected_object) {
        return -1;
    }
    
    // Search through all loaded scenes to find which one contains the selected object
    for (size_t i = 0; i < mFBXScenes.size(); ++i) {
        if (mFBXScenes[i].scene) {
            // Check if the selected object is the root of this scene
            const ofbx::Object* root = mFBXScenes[i].scene->getRoot();
            if (root == g_selected_object) {
                return static_cast<int>(i);
            }
        }
    }
    
    return -1;
}
```

**Purpose:**
- Determines which model a selected RootNode belongs to
- Used to update model selection when RootNode is selected in Outliner

### 4. Updated RootNode Selection Handling in Main Loop

**File:** `src/main.cpp`

**Added Logic:**
```cpp
if (isRootNodeSelected) {
    // Find which model this RootNode belongs to
    int rootNodeModelIndex = scene.mOutliner.findModelIndexForSelectedObject();
    
    if (rootNodeModelIndex >= 0 && rootNodeModelIndex < static_cast<int>(modelManager.getModelCount())) {
        // Get current selected model index
        int currentModelIndex = modelManager.getSelectedModelIndex();
        
        // If switching to a different model's RootNode
        if (currentModelIndex != rootNodeModelIndex && currentModelIndex >= 0) {
            // Save current model's RootNode transforms before switching
            scene.mPropertyPanel.saveRootNodeTransformsForModel(currentModelIndex);
        }
        
        // Update selected model to the one containing this RootNode
        modelManager.setSelectedModel(rootNodeModelIndex);
        scene.mPropertyPanel.setCurrentModelIndex(rootNodeModelIndex);
        
        // Load this model's RootNode transforms (or use defaults if first time)
        if (scene.mPropertyPanel.hasRootNodeTransformsForModel(rootNodeModelIndex)) {
            // Load saved transforms
            scene.mPropertyPanel.loadRootNodeTransformsForModel(rootNodeModelIndex);
        } else {
            // First time - initialize from model's current transform
            // ... initialize and save ...
        }
    }
}
```

**Key Features:**
- Detects when RootNode is selected in Outliner
- Finds which model the RootNode belongs to
- Saves current model's transforms before switching
- Updates selected model to match the RootNode
- Loads the correct model's transforms (or initializes if first time)

### 5. Updated Transform Save Logic

**File:** `src/io/ui/property_panel.cpp`

**Updated Methods:**
- `setSliderRotations()`: Now saves to per-model storage when RootNode is selected
- `setSliderTranslations()`: Now saves to per-model storage when RootNode is selected
- `setSliderScales()`: Now saves to per-model storage when RootNode is selected
- UI slider change handlers: Now save to per-model storage when RootNode is selected

**Implementation:**
```cpp
void PropertyPanel::setSliderRotations(const glm::vec3& rotations)
{
    mSliderRotations = rotations;
    if (!mSelectedNod_name.empty()) {
        mBoneRotations[mSelectedNod_name] = rotations;
        
        // If RootNode is selected, also save to per-model storage
        if (mSelectedNod_name.find("Root") != std::string::npos || mSelectedNod_name == "RootNode") {
            saveRootNodeTransformsToCurrentModel();
        }
    }
}
```

**Purpose:**
- Ensures transforms are immediately saved to the correct model
- Works for both gizmo manipulation and manual slider changes
- Prevents data loss when switching between models

### 6. Updated setSelectionToRoot to Accept Model Index

**File:** `src/io/ui/outliner.h` and `src/io/ui/outliner.cpp`

**Before:**
```cpp
void setSelectionToRoot();  // Always selected first model's root
```

**After:**
```cpp
void setSelectionToRoot(int modelIndex = -1);  // Selects specified model's root
```

**Implementation:**
```cpp
void Outliner::setSelectionToRoot(int modelIndex)
{
    // If valid model index provided, select that model's root
    if (modelIndex >= 0 && modelIndex < static_cast<int>(mFBXScenes.size())) {
        if (mFBXScenes[modelIndex].scene) {
            const ofbx::Object* root = mFBXScenes[modelIndex].scene->getRoot();
            // ... select root ...
        }
    }
    // ... fallback to first scene ...
}
```

**Purpose:**
- Allows selecting the root of a specific model (not just the first one)
- Used when a model is selected via viewport to select that model's root

### 7. Added Transform Save on Deselection

**File:** `src/main.cpp`

**Added Logic:**
```cpp
// When deselecting (clicking empty space)
if (closestModelIndex < 0) {
    // Save current model's RootNode transforms before deselecting
    int currentModelIndex = modelManager.getSelectedModelIndex();
    if (currentModelIndex >= 0 && isRootNodeSelected) {
        scene.mPropertyPanel.saveRootNodeTransformsForModel(currentModelIndex);
    }
    modelManager.setSelectedModel(-1);
}
```

**Purpose:**
- Saves transforms before deselecting to prevent data loss
- Ensures transforms are preserved when clicking empty space

## How It Works

### Flow: Selecting RootNode from Outliner

```
User clicks RootNode in Outliner
    ↓
Outliner::findModelIndexForSelectedObject() finds which model (e.g., model 1)
    ↓
Main loop detects RootNode selected
    ↓
If switching models:
    - Save current model's RootNode transforms (e.g., model 0)
    ↓
Update selected model to model 1
    ↓
Set PropertyPanel current model index to 1
    ↓
If model 1 has saved transforms:
    - Load model 1's RootNode transforms into sliders
Else:
    - Initialize from model 1's current transform
    - Save initialized transforms
    ↓
Gizmo now uses model 1 (correct model)
    ↓
Transform changes save to model 1's storage
```

### Flow: Manipulating RootNode with Gizmo

```
User manipulates gizmo for model 1's RootNode
    ↓
Gizmo updates PropertyPanel sliders
    ↓
setSliderRotations/Translations/Scales() called
    ↓
Detects RootNode is selected
    ↓
Calls saveRootNodeTransformsToCurrentModel()
    ↓
Saves to mModelRootNodeTranslations[1], etc.
    ↓
Transforms are preserved for model 1
```

### Flow: Switching Between Models

```
Model 0 RootNode selected, transforms: (10, 5, 0)
    ↓
User selects Model 1 RootNode in Outliner
    ↓
Save Model 0 transforms: mModelRootNodeTranslations[0] = (10, 5, 0)
    ↓
Load Model 1 transforms: mModelRootNodeTranslations[1] = (0, 0, 0) (or saved value)
    ↓
Gizmo moves to Model 1 position
    ↓
User manipulates Model 1
    ↓
Transforms save to Model 1 storage
    ↓
User selects Model 0 RootNode again
    ↓
Save Model 1 transforms
    ↓
Load Model 0 transforms: (10, 5, 0) - restored!
```

## Benefits

1. **Isolated Transforms**: Each model's RootNode transforms are stored separately
2. **Correct Gizmo Position**: Gizmo moves to the correct model when switching RootNodes
3. **No Transform Mixing**: Transforms don't leak between models
4. **Persistent State**: Each model remembers its RootNode transform values
5. **Automatic Saving**: Transforms are saved automatically when changed (gizmo or sliders)
6. **Proper Deselection**: Transforms are saved before deselecting to prevent data loss

## Technical Details

### Storage Structure

```
PropertyPanel:
├─ mBoneRotations["RootNode"] = (current slider values)
├─ mBoneTranslations["RootNode"] = (current slider values)
├─ mBoneScales["RootNode"] = (current slider values)
│
└─ Per-Model Storage:
   ├─ mModelRootNodeRotations[0] = Model 0's saved rotation
   ├─ mModelRootNodeTranslations[0] = Model 0's saved translation
   ├─ mModelRootNodeScales[0] = Model 0's saved scale
   │
   ├─ mModelRootNodeRotations[1] = Model 1's saved rotation
   ├─ mModelRootNodeTranslations[1] = Model 1's saved translation
   └─ mModelRootNodeScales[1] = Model 1's saved scale
```

### Model Index Tracking

- `mCurrentModelIndex`: Tracks which model's RootNode is currently active
- Updated when RootNode is selected from Outliner
- Updated when model is selected via viewport
- Used to save transforms to the correct model

### Save Triggers

Transforms are saved to per-model storage when:
1. **Gizmo manipulation**: `setSliderRotations/Translations/Scales()` called by gizmo
2. **Manual slider changes**: UI slider change detected in `propertyPanel()`
3. **Model switch**: Before switching to a different model's RootNode
4. **Deselection**: Before deselecting (clicking empty space)

## Testing Checklist

- [ ] Load two FBX files
- [ ] Select RootNode of first model in Outliner
- [ ] Move gizmo - should move first model
- [ ] Select RootNode of second model in Outliner
- [ ] Gizmo should move to second model (not stay on first)
- [ ] Move gizmo - should move second model
- [ ] Select RootNode of first model again
- [ ] First model should be at its previous position (not second model's position)
- [ ] Click empty space to deselect
- [ ] Select RootNode of first model again
- [ ] First model should still be at its previous position
- [ ] Select RootNode of second model
- [ ] Second model should be at its previous position

## Known Limitations

1. **Bone Transforms**: Currently only RootNode transforms are per-model. Bone transforms are still global (shared across models). This could be enhanced in the future.
2. **Model Removal**: When a model is removed, its per-model transforms are not cleaned up (they remain in the maps but are unused). This is acceptable as the maps are cleared on "New Scene".

## Future Enhancements

Potential improvements:
1. **Per-Model Bone Transforms**: Store bone transforms per-model as well (not just RootNode)
2. **Transform Presets**: Save/load transform presets per-model
3. **Transform History**: Undo/redo per-model transform changes
4. **Model Transform Export**: Export per-model transforms to file

## Files Modified

1. `src/io/ui/property_panel.h` - Added per-model storage and methods
2. `src/io/ui/property_panel.cpp` - Implemented per-model save/load logic
3. `src/io/ui/outliner.h` - Added `findModelIndexForSelectedObject()` method
4. `src/io/ui/outliner.cpp` - Implemented model finding logic, updated `setSelectionToRoot()`
5. `src/main.cpp` - Added RootNode selection handling, model switching logic, deselection save

## Related Features

- **Multi-Scene Outliner**: Displays hierarchies from multiple models
- **Viewport Selection**: Selects models and updates Outliner selection
- **Gizmo Integration**: Manipulates RootNode transforms with visual gizmo
- **PropertyPanel**: Displays and edits transform values

## Summary

The per-model RootNode transform system successfully isolates transform values between different models, preventing the mixing and swapping issues that occurred when multiple FBX files were loaded. Each model now maintains its own RootNode transform state, and the gizmo correctly moves to the appropriate model when switching between RootNodes in the Outliner. The system automatically saves and loads transforms when switching between models, ensuring that each model's transform state is preserved.
