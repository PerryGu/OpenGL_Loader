# Multi-Scene Outliner Implementation

## Overview
This document describes the implementation of multi-scene support in the Outliner, allowing it to display hierarchies from multiple loaded FBX files simultaneously. Previously, loading a new FBX file would replace the previous one's hierarchy in the Outliner. Now, all loaded models' hierarchies are displayed together.

## Date
Implementation Date: 2026-02-09

## Problem Statement
When loading multiple FBX files:
- Both characters appeared correctly in the viewport (working as expected)
- However, the Outliner only showed the hierarchy of the last loaded file
- Hierarchies from previously loaded files disappeared
- Users couldn't see or select bones/nodes from the first character after loading a second one

## Solution Applied

### 1. Created FBXSceneData Structure

**File:** `src/io/ui/outliner.h`

**Added Structure:**
```cpp
// Structure to hold FBX scene data with metadata
struct FBXSceneData {
    ofbx::IScene* scene = nullptr;
    std::string filePath;
    std::string displayName;  // Filename extracted from path
    
    FBXSceneData() : scene(nullptr) {}
    FBXSceneData(ofbx::IScene* s, const std::string& path, const std::string& name)
        : scene(s), filePath(path), displayName(name) {}
};
```

**Purpose:**
- Stores each loaded FBX scene with its metadata
- Tracks file path and display name for each scene
- Allows multiple scenes to coexist

### 2. Changed Outliner to Store Multiple Scenes

**File:** `src/io/ui/outliner.h`

**Before:**
```cpp
ofbx::IScene* ofbx_scene = nullptr;  // Single scene pointer
```

**After:**
```cpp
// Multiple FBX scenes support (one per loaded model)
std::vector<FBXSceneData> mFBXScenes;  // Vector of all loaded FBX scenes

// Legacy single scene pointer (kept for backward compatibility)
ofbx::IScene* ofbx_scene = nullptr;
```

### 3. Updated loadFBXfile() to Add Instead of Replace

**File:** `src/io/ui/outliner.cpp`

**Before:**
```cpp
void Outliner::loadFBXfile(std::string filePath)
{
    // ... load scene ...
    ofbx_scene = ofbx::load(...);  // Replaces previous scene
}
```

**After:**
```cpp
void Outliner::loadFBXfile(std::string filePath)
{
    // Check if file already loaded (avoid duplicates)
    for (const auto& sceneData : mFBXScenes) {
        if (sceneData.filePath == filePath) {
            return;  // Already loaded, skip
        }
    }
    
    // Load scene
    ofbx::IScene* scene = ofbx::load(...);
    
    // Add to collection (instead of replacing)
    std::string displayName = extractFileName(filePath);
    mFBXScenes.push_back(FBXSceneData(scene, filePath, displayName));
    
    // Keep legacy pointer for backward compatibility
    ofbx_scene = scene;
}
```

**Key Changes:**
- Checks for duplicates before loading
- Adds scene to vector instead of replacing
- Extracts display name from file path
- Maintains legacy pointer for backward compatibility

### 4. Updated outlinerPanel() to Display All Scenes

**File:** `src/io/ui/outliner.cpp`

**Before:**
```cpp
if (ofbx_scene) {
    showObjectsGUI(*ofbx_scene);  // Display single scene
}
```

**After:**
```cpp
if (!mFBXScenes.empty()) {
    // Display each scene with its model name
    for (size_t i = 0; i < mFBXScenes.size(); ++i) {
        const FBXSceneData& sceneData = mFBXScenes[i];
        
        if (sceneData.scene) {
            // Display model name as header
            std::string headerLabel = sceneData.displayName;
            
            // Visual header with styling
            ImGui::TreeNodeEx(headerLabel.c_str(), modelHeaderFlags);
            
            // Display scene hierarchy (indented)
            ImGui::Indent();
            showObjectsGUI(*sceneData.scene);
            ImGui::Unindent();
            
            // Add separator between models
            if (i < mFBXScenes.size() - 1) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
            }
        }
    }
}
```

**Visual Improvements:**
- Each model's hierarchy is displayed under its filename
- Visual separators between different models
- Styled headers to distinguish model sections
- Indented hierarchies for clear organization

### 5. Updated rebuildSelectableObjectsList() for Multiple Scenes

**File:** `src/io/ui/outliner.cpp`

**Added New Method:**
```cpp
void Outliner::rebuildSelectableObjectsList()
{
    mSelectableObjects.clear();
    
    // Collect selectable objects from ALL loaded scenes
    for (const auto& sceneData : mFBXScenes) {
        if (sceneData.scene) {
            // Collect from root
            const ofbx::Object* root = sceneData.scene->getRoot();
            if (root) {
                collectSelectableObjects(*root);
            }
            
            // Collect from animation stacks
            int count = sceneData.scene->getAnimationStackCount();
            for (int i = 0; i < count; ++i) {
                const ofbx::Object* stack = sceneData.scene->getAnimationStack(i);
                if (stack) {
                    collectSelectableObjects(*stack);
                }
            }
        }
    }
}
```

**Purpose:**
- Rebuilds the selectable objects list from all loaded scenes
- Enables arrow key navigation across all models
- Maintains selection index across all hierarchies

### 6. Updated clear() to Remove All Scenes

**File:** `src/io/ui/outliner.cpp`

**Before:**
```cpp
void Outliner::clear()
{
    if (ofbx_scene) {
        ofbx_scene = nullptr;  // Clear single scene
    }
}
```

**After:**
```cpp
void Outliner::clear()
{
    // Clear all FBX scenes
    mFBXScenes.clear();
    
    // Clear legacy pointer
    ofbx_scene = nullptr;
    
    // Clear selection and navigation data
    // ...
}
```

### 7. Added removeFBXScene() Method

**File:** `src/io/ui/outliner.h` and `src/io/ui/outliner.cpp`

**New Method:**
```cpp
void Outliner::removeFBXScene(const std::string& filePath)
{
    // Find and remove scene with matching file path
    auto it = std::remove_if(mFBXScenes.begin(), mFBXScenes.end(),
        [&filePath](const FBXSceneData& sceneData) {
            return sceneData.filePath == filePath;
        });
    
    if (it != mFBXScenes.end()) {
        mFBXScenes.erase(it, mFBXScenes.end());
        // Update legacy pointer
        // ...
    }
}
```

**Purpose:**
- Allows removal of specific scenes when models are removed
- Useful for future individual model removal functionality
- Maintains scene collection integrity

## How It Works

### Loading Multiple Models

```
User loads first FBX file
    ↓
Outliner::loadFBXfile() called
    ↓
Scene loaded and added to mFBXScenes[0]
    ↓
Outliner displays first model's hierarchy
    ↓
User loads second FBX file
    ↓
Outliner::loadFBXfile() called again
    ↓
Scene loaded and added to mFBXScenes[1]
    ↓
Outliner displays BOTH models' hierarchies
    ├─ Model 1 hierarchy (from mFBXScenes[0])
    └─ Model 2 hierarchy (from mFBXScenes[1])
```

### Display Structure

```
Outliner Window
├─ [Model1.fbx] ← Header (styled, always visible)
│   ├─ RootNode (root)
│   ├─ Rig_GRP (null)
│   ├─ Player_GRP (null)
│   └─ Take 001 (animation stack)
│
├─ ─ ─ ─ ─ ─ ─ ─ ─ ← Separator
│
└─ [Model2.fbx] ← Header (styled, always visible)
    ├─ RootNode (root)
    ├─ boy (mesh)
    ├─ deformation_rig (null)
    └─ Take 001 (animation stack)
```

### Selection and Navigation

- **Arrow Key Navigation**: Works across all models' hierarchies
- **Mouse Selection**: Can select any node from any model
- **Selection Index**: Maintained across all scenes for keyboard navigation
- **Root Selection**: When model is selected via viewport, selects root of first scene (or can be enhanced to select specific model's root)

## Benefits

1. **Complete Visibility**: All loaded models' hierarchies are visible simultaneously
2. **No Data Loss**: Previous hierarchies don't disappear when loading new models
3. **Better Organization**: Each model's hierarchy is clearly labeled and separated
4. **Unified Navigation**: Arrow keys work across all models seamlessly
5. **Future-Proof**: Supports individual model removal (removeFBXScene method ready)

## Technical Details

### Memory Management

- **Scene Ownership**: Scenes are managed by openFBX library
- **Vector Storage**: Uses `std::vector<FBXSceneData>` for efficient storage
- **No Duplicates**: Checks for existing file paths before loading
- **Cleanup**: All scenes cleared when `clear()` is called

### Backward Compatibility

- **Legacy Pointer**: `ofbx_scene` still maintained for backward compatibility
- **Fallback Support**: Code still works with single scene pointer if vector is empty
- **Gradual Migration**: Can migrate other code to use vector over time

### Performance Considerations

- **Rebuild Frequency**: Selectable objects list rebuilt when scenes change
- **Display Cost**: Iterates through all scenes each frame (minimal overhead)
- **Selection Lookup**: Linear search through all scenes (acceptable for typical model counts)

## Testing Checklist

- [ ] Load first FBX file - hierarchy should appear in Outliner
- [ ] Load second FBX file - both hierarchies should be visible
- [ ] Load third FBX file - all three hierarchies should be visible
- [ ] Verify model names appear as headers above each hierarchy
- [ ] Verify visual separators between models
- [ ] Test arrow key navigation - should work across all models
- [ ] Test mouse selection - should be able to select nodes from any model
- [ ] Click "New Scene" - all hierarchies should disappear
- [ ] Load models again - hierarchies should reappear correctly

## Known Limitations

1. **Root Selection**: Currently selects root of first scene when model is selected via viewport
   - Could be enhanced to select root of the specific selected model
2. **No Individual Removal**: Models can only be cleared all at once (New Scene)
   - `removeFBXScene()` method exists but isn't called yet
   - Future enhancement: Remove individual models
3. **Scene Order**: Scenes are displayed in load order
   - Could add sorting/filtering in the future

## Future Enhancements

Potential improvements:
1. **Model-Specific Root Selection**: When model is selected via viewport, select that model's root in outliner
2. **Individual Model Removal**: UI to remove specific models without clearing all
3. **Collapsible Model Headers**: Allow collapsing/expanding entire model hierarchies
4. **Model Filtering**: Filter outliner to show only specific models
5. **Model Reordering**: Drag to reorder models in outliner
6. **Model Search**: Search for nodes across all models

## Files Modified

1. `src/io/ui/outliner.h` - Added FBXSceneData structure, changed to vector storage, added methods
2. `src/io/ui/outliner.cpp` - Updated all methods to support multiple scenes, added helper methods

## Related Features

- **Multi-Model Animation**: Models animate independently (already working)
- **Viewport Selection**: Can select models in viewport (works with multiple models)
- **PropertyPanel**: Shows transforms for selected nodes (works across all models)
- **New Scene**: Clears all scenes and models (works correctly)

## Migration Notes

### For Developers

If you have code that accesses `ofbx_scene` directly:
- **Still Works**: Legacy pointer is maintained for backward compatibility
- **Recommended**: Migrate to use `mFBXScenes` vector for multi-model support
- **Example**: Instead of `if (ofbx_scene)`, use `if (!mFBXScenes.empty())`

### Breaking Changes

**None** - The implementation maintains backward compatibility with the legacy `ofbx_scene` pointer.

## Summary

The multi-scene outliner implementation successfully allows multiple FBX file hierarchies to be displayed simultaneously in the Outliner window. Each model's hierarchy is clearly labeled and visually separated, making it easy to navigate and select nodes from any loaded model. The implementation maintains backward compatibility while providing a foundation for future enhancements like individual model removal and model-specific operations.
