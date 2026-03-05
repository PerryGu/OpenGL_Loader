# Gizmo Rig Root Connection Implementation

## Overview
This document describes the change from connecting the gizmo to "RootNode" to connecting it to the rig root container (e.g., "Rig_GRP") that is found during FBX file loading.

## Date
Implementation Date: 2024

## Problem Statement

**Previous Behavior:**
- When selecting a character via viewport click, the gizmo appeared and connected to "RootNode" in the outliner
- Gizmo movements controlled the "RootNode" transforms
- "RootNode" is the scene root, not the rig root

**Desired Behavior:**
- When selecting a character via viewport click, the gizmo should connect to the rig root (e.g., "Rig_GRP")
- Gizmo movements should control the rig root transforms
- The rig root is the parent container of the rig skeleton, which is more appropriate for character positioning

## Solution Applied

### 1. Store Rig Root Name During FBX Loading

**File:** `src/io/ui/outliner.h`

**Added to FBXSceneData structure:**
```cpp
struct FBXSceneData {
    // ... existing fields ...
    std::string rigRootName;  // Rig root container name (e.g., "Rig_GRP") - found during load
    
    // Get the rig root name (parent container of the rig, e.g., "Rig_GRP")
    std::string getRigRootName() const {
        return rigRootName;
    }
};
```

**File:** `src/io/ui/outliner.cpp`

**Updated `loadFBXfile()` to store rig root:**
```cpp
// Find and store the rig root (parent container of the rig, e.g., "Rig_GRP")
std::string rigRootName = FBXRigAnalyzer::findRigRoot(scene);
sceneData.rigRootName = rigRootName;  // Store rig root name in scene data
```

### 2. Added Method to Get Rig Root Name

**File:** `src/io/ui/outliner.h` and `src/io/ui/outliner.cpp`

**New Method:**
```cpp
// Get the rig root name for a model
std::string getRigRootName(int modelIndex) const;
```

**Implementation:**
- Returns the rig root name for the specified model index
- Returns empty string if no rig root was found or model index is invalid

### 3. Added Helper Function to Find Object by Name

**File:** `src/io/ui/outliner.h` and `src/io/ui/outliner.cpp`

**New Method:**
```cpp
// Find an object by name in the scene hierarchy
const ofbx::Object* findObjectByName(const ofbx::Object* object, const std::string& name) const;
```

**Purpose:**
- Recursively searches the FBX scene hierarchy to find an object with a specific name
- Used to locate the rig root object by name when selecting it

### 4. Changed setSelectionToRoot() to Select Rig Root

**File:** `src/io/ui/outliner.cpp`

**Before:**
- Selected the scene root node ("RootNode")

**After:**
- Selects the rig root container (e.g., "Rig_GRP") if found
- Falls back to RootNode if no rig root is found (backward compatibility)

**Implementation:**
```cpp
void Outliner::setSelectionToRoot(int modelIndex)
{
    // Get the rig root name for this model
    std::string rigRootName = mFBXScenes[modelIndex].getRigRootName();
    
    if (!rigRootName.empty()) {
        // Find the rig root object by name in the scene hierarchy
        const ofbx::Object* sceneRoot = mFBXScenes[modelIndex].scene->getRoot();
        const ofbx::Object* rigRootObject = findObjectByName(sceneRoot, rigRootName);
        
        if (rigRootObject) {
            // Found rig root - select it
            g_selected_object = rigRootObject;
            mSelectedNod_name = rigRootName;
            // ... update selection index ...
            return;
        }
    }
    
    // Fallback: If no rig root found, select RootNode (backward compatibility)
    // ...
}
```

### 5. Updated Gizmo Detection Logic

**File:** `src/io/scene.cpp`

**Before:**
```cpp
bool isRootNode = (selectedNode.find("Root") != std::string::npos || 
                   selectedNode == "RootNode");
```

**After:**
```cpp
// Get the selected model index to find its rig root name
int selectedModelIndex = mModelManager->getSelectedModelIndex();
std::string rigRootName = mOutliner.getRigRootName(selectedModelIndex);

// Check if the selected node is the rig root (e.g., "Rig_GRP")
bool isRigRoot = false;
if (!rigRootName.empty()) {
    isRigRoot = (selectedNode == rigRootName);
} else {
    // Fallback: Check for RootNode if no rig root was found
    isRigRoot = (selectedNode.find("Root") != std::string::npos || 
                selectedNode == "RootNode");
}
```

### 6. Updated Main Loop to Use Rig Root

**File:** `src/main.cpp`

**Changed all references from `isRootNodeSelected` to `isRigRootSelected`:**

1. **Rig Root Selection Detection:**
```cpp
// Get the selected model index to find its rig root name
int selectedModelIndex = modelManager.getSelectedModelIndex();
std::string rigRootName = scene.mOutliner.getRigRootName(selectedModelIndex);

// Check if the selected node is the rig root
bool isRigRootSelected = false;
if (!rigRootName.empty()) {
    isRigRootSelected = (selectedNode == rigRootName);
} else {
    // Fallback: Check for RootNode if no rig root was found
    isRigRootSelected = (!selectedNode.empty() && 
                        (selectedNode.find("Root") != std::string::npos || 
                         selectedNode == "RootNode"));
}
```

2. **Rig Root Initialization:**
- Changed from initializing "RootNode" to initializing rig root
- Uses rig root name instead of RootNode name

3. **Rig Root Transform Application:**
- Changed from applying RootNode transforms to applying rig root transforms
- Variable names updated from `isRootNodeSelected` to `isRigRootSelected`
- Variable names updated from `selectedModelIndex` to `rootNodeModelIndex` (for rig root matrix)

4. **Bounding Box Selection:**
- Updated to check for rig root instead of RootNode
- Uses rig root transform matrix for bounding box transformation

### 7. Updated PropertyPanel

**File:** `src/io/ui/property_panel.cpp`

**Updated `initializeRootNodeFromModel()`:**
- Removed restrictive RootNode name check
- Now works for any selected node (rig root or RootNode)
- Method name kept the same for backward compatibility, but functionality expanded

**Before:**
```cpp
// Only initialize if RootNode is currently selected
if (mSelectedNod_name.empty() || 
    (mSelectedNod_name.find("Root") == std::string::npos && mSelectedNod_name != "RootNode")) {
    return;  // Not RootNode, don't initialize
}
```

**After:**
```cpp
// Allow initialization for any selected node (rig root or RootNode)
// The caller (main.cpp) ensures this is only called for rig root/RootNode
if (mSelectedNod_name.empty()) {
    return;  // No node selected, don't initialize
}
```

## How It Works Now

### Flow: Viewport Selection

```
User clicks on model in viewport
    ↓
Model gets selected (modelManager.setSelectedModel())
    ↓
Outliner::setSelectionToRoot() called
    ↓
Gets rig root name for the model (e.g., "Rig_GRP")
    ↓
Finds rig root object by name in scene hierarchy
    ↓
Selects rig root in outliner (g_selected_object = rigRootObject)
    ↓
Gizmo appears and connects to rig root transforms
    ↓
Moving gizmo updates PropertyPanel rig root transforms
    ↓
Rig root transforms applied to model.pos/rotation/size
```

### Flow: Gizmo Manipulation

```
User manipulates gizmo
    ↓
Gizmo checks if rig root is selected (by name comparison)
    ↓
If rig root selected:
    ├─ Reads rig root transforms from PropertyPanel
    ├─ Builds gizmo matrix from PropertyPanel values
    ├─ User manipulates gizmo
    ├─ Gizmo updates PropertyPanel rig root transforms
    └─ Rig root transforms applied to model
```

## Backward Compatibility

The implementation maintains backward compatibility:

1. **Fallback to RootNode:**
   - If no rig root is found during FBX loading, the system falls back to RootNode
   - All rig root checks have fallback logic to check for RootNode

2. **Existing Code:**
   - Code that checks for RootNode still works (as fallback)
   - PropertyPanel methods still work with RootNode names

3. **FBX Files Without Rig:**
   - Files without a rig structure will use RootNode (existing behavior)
   - No breaking changes for files that don't have rig roots

## Benefits

1. **More Intuitive:** Gizmo connects to the actual rig container, not the scene root
2. **Better Organization:** Rig root (e.g., "Rig_GRP") is the logical container for character positioning
3. **Consistent with Industry:** Matches how professional 3D software handles rig roots
4. **Backward Compatible:** Still works with RootNode if no rig root is found

## Technical Details

### Rig Root Storage

- **Location:** Stored in `FBXSceneData::rigRootName`
- **Type:** `std::string` (empty if not found)
- **Persistence:** Stored in memory only (not persisted to disk)
- **Per-Model:** Each model has its own rig root name

### Object Finding

- **Method:** `findObjectByName()` - recursive depth-first search
- **Performance:** O(n) where n is the number of objects in the hierarchy
- **Caching:** No caching (searches on-demand when needed)

### Selection Logic

- **Primary:** Try to select rig root by name
- **Fallback:** Select RootNode if rig root not found
- **Error Handling:** Gracefully handles missing rig root or invalid model index

## Files Modified

1. **`src/io/ui/outliner.h`**:
   - Added `rigRootName` to `FBXSceneData` structure
   - Added `getRigRootName()` method to `FBXSceneData`
   - Added `getRigRootName(int modelIndex)` method to `Outliner`
   - Added `findObjectByName()` method declaration

2. **`src/io/ui/outliner.cpp`**:
   - Updated `loadFBXfile()` to store rig root name
   - Implemented `getRigRootName()` method
   - Implemented `findObjectByName()` helper function
   - Updated `setSelectionToRoot()` to select rig root instead of RootNode

3. **`src/io/scene.cpp`**:
   - Updated gizmo detection to check for rig root instead of RootNode
   - Updated comments to reflect rig root usage

4. **`src/main.cpp`**:
   - Updated all `isRootNodeSelected` checks to `isRigRootSelected`
   - Updated rig root initialization logic
   - Updated rig root transform application
   - Updated bounding box selection to use rig root

5. **`src/io/ui/property_panel.cpp`**:
   - Updated `initializeRootNodeFromModel()` to work with rig root
   - Removed restrictive RootNode name check

## Testing

To test the changes:

1. **Load FBX with Rig:**
   - Import an FBX file with a rig structure (e.g., "Rig_GRP")
   - Check console: Should show `[Rig Analyzer] Rig Root: 'Rig_GRP'`
   - Click on model in viewport
   - Check outliner: Should select "Rig_GRP" (not "RootNode")
   - Check gizmo: Should appear and connect to "Rig_GRP" transforms

2. **Manipulate Gizmo:**
   - Move gizmo - model should move
   - Check PropertyPanel: Transforms should update for "Rig_GRP"
   - Rotate gizmo - model should rotate
   - Scale gizmo - model should scale

3. **FBX Without Rig:**
   - Import an FBX file without rig structure
   - Check console: Should show `[Rig Analyzer] No rig root found`
   - Click on model in viewport
   - Check outliner: Should select "RootNode" (fallback)
   - Gizmo should still work with RootNode

## Known Limitations

1. **Rig Root Must Be Found:** If rig root is not found during loading, falls back to RootNode
2. **Name-Based Matching:** Rig root selection relies on exact name matching
3. **No Rig Root Validation:** Doesn't verify that the found object is actually a rig container

## Future Enhancements

Potential improvements:
1. **Rig Root Validation:** Verify that the found object is actually a NULL_NODE container
2. **Multiple Rig Roots:** Handle cases with multiple rig roots per model
3. **Rig Root Persistence:** Save rig root name to settings file
4. **Rig Root UI:** Display rig root name in UI for user visibility

## Summary

The gizmo connection has been successfully changed from "RootNode" to the rig root container (e.g., "Rig_GRP"). The rig root is automatically detected during FBX loading and stored per model. When a model is selected via viewport, the outliner now selects the rig root instead of RootNode, and the gizmo connects to the rig root transforms. The implementation maintains backward compatibility by falling back to RootNode if no rig root is found.
