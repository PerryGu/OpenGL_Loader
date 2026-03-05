# Multiple Character Instancing

**Date:** 2026-02-19  
**Status:** ✅ Implemented

## Overview
Enabled importing the same character multiple times without name collisions, allowing users to create multiple instances of the same FBX model in the viewport.

## Problem
Previously, the Outliner blocked loading duplicate file paths, causing a mismatch:
- Viewport correctly raycasted Model Index 1
- Outliner fell back to Model Index 0 (only one scene loaded)
- Selection and manipulation failed for duplicate instances

## Solution

### 1. Removed Duplicate File Path Check
**File:** `src/io/ui/outliner.cpp`

**Before:**
```cpp
// Check if file already loaded
for (const auto& scene : mFBXScenes) {
    if (scene.filePath == filePath) {
        std::cout << "[Outliner] File already loaded: " << filePath << std::endl;
        return;  // BLOCKED duplicate loading
    }
}
```

**After:**
```cpp
// CRITICAL FIX: Removed duplicate file path check
// We WANT to allow loading the same FBX multiple times for Instancing
```

### 2. Unique Display Names
Added numerical suffix to display names for duplicate files:
```cpp
// Count how many times this file is already loaded
int duplicateCount = 0;
for (const auto& existingScene : mFBXScenes) {
    if (existingScene.filePath == filePath) {
        duplicateCount++;
    }
}

if (duplicateCount > 0) {
    displayName += " (" + std::to_string(duplicateCount) + ")";
}
```

**Example:**
- First load: `"Character.fbx"`
- Second load: `"Character.fbx (1)"`
- Third load: `"Character.fbx (2)"`

### 3. Index-Based Removal
Changed `removeFBXScene()` to use model index instead of file path:
```cpp
// OLD: void removeFBXScene(const std::string& filePath);
// NEW: void removeFBXScene(int modelIndex);

void Outliner::removeFBXScene(int modelIndex) {
    if (modelIndex >= 0 && modelIndex < static_cast<int>(mFBXScenes.size())) {
        mFBXScenes.erase(mFBXScenes.begin() + modelIndex);
        // ...
    }
}
```

This ensures only the specific instance is deleted, not all instances.

### 4. RootNode Auto-Renaming
The existing RootNode auto-renaming system handles name collisions:
- First instance: `"RootNode"`
- Second instance: `"RootNode_1"`
- Third instance: `"RootNode_2"`

## Benefits

### 1. Scene Composition
- Load multiple instances of the same character
- Create crowd scenes or multiple NPCs
- Test different poses/animations simultaneously

### 2. Independent Manipulation
- Each instance has its own transform state
- Gizmo manipulation affects only the selected instance
- Property Panel controls the active instance

### 3. Selection Accuracy
- Viewport raycast correctly identifies instance by model index
- Outliner selection matches viewport selection
- No fallback to wrong model index

## Implementation Details

### Files Modified
- `src/io/ui/outliner.cpp` - Removed duplicate check, added suffix logic
- `src/io/ui/outliner.h` - Changed `removeFBXScene()` signature
- `src/io/ui/ui_manager.cpp` - Updated Delete Modal to use index

### Data Structures
```cpp
struct FBXScene {
    std::string filePath;      // Full path (can be duplicate)
    std::string displayName;   // Unique display name (with suffix)
    ofbx::IScene* scene;       // OpenFBX scene pointer
};
```

## Usage Example

1. **Load First Instance:**
   - File: `"Character.fbx"`
   - Display: `"Character.fbx"`
   - RootNode: `"RootNode"`

2. **Load Second Instance:**
   - File: `"Character.fbx"` (same path)
   - Display: `"Character.fbx (1)"`
   - RootNode: `"RootNode_1"`

3. **Select and Manipulate:**
   - Click on second instance in viewport
   - Outliner highlights `"Character.fbx (1)"`
   - Gizmo appears on second instance
   - Property Panel shows second instance's transforms

4. **Delete Specific Instance:**
   - Select `"Character.fbx (1)"` in Outliner
   - Press Delete key
   - Only second instance is removed
   - First instance remains intact

## Related Features
- Model Index-based selection
- Per-model RootNode transforms
- Selection isolation
- Delete Modal (index-based deletion)

## Related Documentation
- `MD/ROOTNODE_AUTO_RENAME.md`
- `MD/SELECTION_ISOLATION_REFACTOR.md`
- `MD/MULTI_MODEL_UPGRADE_SUMMARY.md`
