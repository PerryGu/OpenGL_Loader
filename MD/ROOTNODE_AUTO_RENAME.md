# Automatic RootNode Renaming System

## Overview
This document describes the implementation of automatic RootNode renaming to prevent name collisions when multiple FBX files are loaded. When a RootNode with the same name already exists, the new RootNode is automatically renamed (e.g., "RootNode01", "RootNode02", etc.).

## Date
Implementation Date: 2024

## Problem Solved

**Before:**
- All models used the same name "RootNode" for their root node
- This caused name collisions in the PropertyPanel bone maps
- Required dual storage system (bone maps + per-model storage) to prevent conflicts

**After:**
- First model: RootNode name stays "RootNode"
- Second model: RootNode automatically renamed to "RootNode01"
- Third model: RootNode automatically renamed to "RootNode02"
- Each RootNode has a unique name, eliminating collisions

## Benefits

1. **Eliminates Name Collisions**: Each RootNode has a unique name
2. **Simplifies Storage**: RootNode can potentially use the same storage mechanism as regular bones (no need for dual storage)
3. **Clear Identification**: Users can see which model's RootNode they're working with
4. **Automatic**: No manual intervention required - happens automatically on load

## Implementation Details

### 1. Enhanced FBXSceneData Structure

**File:** `src/io/ui/outliner.h`

**Added Fields:**
```cpp
struct FBXSceneData {
    // ... existing fields ...
    std::string rootNodeOriginalName;  // Original root node name from FBX file
    std::string rootNodeRenamedName;   // Renamed root node name (if collision detected)
    
    // Get the effective root node name (renamed if collision, original otherwise)
    std::string getRootNodeName() const {
        return rootNodeRenamedName.empty() ? rootNodeOriginalName : rootNodeRenamedName;
    }
};
```

### 2. Automatic Renaming on Load

**File:** `src/io/ui/outliner.cpp`

**Method:** `loadFBXfile()`

**Process:**
1. Load FBX file and get root node
2. Check if root node name already exists in loaded scenes
3. If collision detected:
   - Generate unique name (e.g., "RootNode01", "RootNode02", etc.)
   - Store renamed name in `rootNodeRenamedName`
4. If no collision:
   - Use original name
   - `rootNodeRenamedName` remains empty

**Code:**
```cpp
const ofbx::Object* root = scene->getRoot();
if (root) {
    std::string originalName = root->name;
    sceneData.rootNodeOriginalName = originalName;
    
    // Check if this root node name already exists in loaded scenes
    if (rootNodeNameExists(originalName)) {
        // Generate unique name
        std::string uniqueName = generateUniqueRootNodeName(originalName);
        sceneData.rootNodeRenamedName = uniqueName;
        std::cout << "[Outliner] Root node name collision detected: '" << originalName 
                  << "' already exists. Renamed to '" << uniqueName << "'" << std::endl;
    }
}
```

### 3. Name Collision Detection

**Method:** `rootNodeNameExists()`

**Purpose:** Check if a root node name already exists in any loaded scene

**Implementation:**
```cpp
bool Outliner::rootNodeNameExists(const std::string& name) const
{
    for (const auto& sceneData : mFBXScenes) {
        // Check both original and renamed names
        if (sceneData.rootNodeOriginalName == name || 
            sceneData.rootNodeRenamedName == name ||
            sceneData.getRootNodeName() == name) {
            return true;
        }
    }
    return false;
}
```

### 4. Unique Name Generation

**Method:** `generateUniqueRootNodeName()`

**Purpose:** Generate a unique root node name by appending numbers

**Format:** `"RootNode01"`, `"RootNode02"`, `"RootNode03"`, etc.

**Implementation:**
```cpp
std::string Outliner::generateUniqueRootNodeName(const std::string& baseName) const
{
    // Try base name with numbers: "RootNode01", "RootNode02", etc.
    for (int i = 1; i < 1000; ++i) {
        char buffer[256];
        sprintf_s(buffer, "%s%02d", baseName.c_str(), i);
        std::string candidateName = buffer;
        
        if (!rootNodeNameExists(candidateName)) {
            return candidateName;
        }
    }
    
    // Fallback: use scene count as suffix
    char buffer[256];
    sprintf_s(buffer, "%s_%d", baseName.c_str(), static_cast<int>(mFBXScenes.size()));
    return buffer;
}
```

### 5. Updated Root Node Name Usage

**All places where root node names are used now call `getRootNodeName()`:**

1. **Outliner Display:**
   ```cpp
   std::string displayName = object.name;
   if (object.getType() == ofbx::Object::Type::ROOT) {
       displayName = getRootNodeName(&object);
   }
   ```

2. **Selection:**
   ```cpp
   if (object.getType() == ofbx::Object::Type::ROOT) {
       mSelectedNod_name = getRootNodeName(&object);
   }
   ```

3. **setSelectionToRoot():**
   ```cpp
   mSelectedNod_name = mFBXScenes[modelIndex].getRootNodeName();
   ```

## RootNode Detection Compatibility

The existing RootNode detection logic uses:
```cpp
selectedNode.find("Root") != std::string::npos
```

This works perfectly with renamed nodes:
- "RootNode" ✅ (contains "Root")
- "RootNode01" ✅ (contains "Root")
- "RootNode02" ✅ (contains "Root")

The exact match check `== "RootNode"` is a fallback and won't match renamed nodes, but the `find("Root")` check will catch them.

## Example Scenarios

### Scenario 1: First Model Loaded
```
Load: model1.fbx
Root node name: "RootNode"
Collision check: No collision (first model)
Result: "RootNode" (no rename)
```

### Scenario 2: Second Model Loaded
```
Load: model2.fbx
Root node name: "RootNode"
Collision check: "RootNode" already exists (from model1)
Result: "RootNode01" (renamed)
```

### Scenario 3: Third Model Loaded
```
Load: model3.fbx
Root node name: "RootNode"
Collision check: "RootNode" and "RootNode01" already exist
Result: "RootNode02" (renamed)
```

### Scenario 4: Model with Different Root Name
```
Load: model4.fbx
Root node name: "SceneRoot"
Collision check: No collision (different name)
Result: "SceneRoot" (no rename)
```

### Scenario 5: Model with Different Root Name that Collides
```
Load: model5.fbx
Root node name: "RootNode01" (coincidentally)
Collision check: "RootNode01" already exists (from model2)
Result: "RootNode0101" (renamed to avoid collision)
```

## Console Output

When a collision is detected, the console will show:
```
[Outliner] Root node name collision detected: 'RootNode' already exists. Renamed to 'RootNode01'
[Outliner] Successfully loaded FBX scene: model2.fbx (Total scenes: 2)
```

When no collision:
```
[Outliner] Root node name: 'RootNode' (no collision)
[Outliner] Successfully loaded FBX scene: model1.fbx (Total scenes: 1)
```

## Future Considerations

### Potential Simplification

With unique RootNode names, the dual storage system (bone maps + per-model storage) might no longer be necessary. However, keeping it provides:

1. **Backward Compatibility**: Works with existing code
2. **Safety**: Extra layer of protection against edge cases
3. **Flexibility**: Can handle models with non-standard root node names

### Possible Enhancement

If we want to remove the dual storage system:
1. RootNode transforms would only use bone maps (like regular bones)
2. Each RootNode would have a unique key: `mBoneTranslations["RootNode"]`, `mBoneTranslations["RootNode01"]`, etc.
3. Per-model storage could be removed

**Trade-off:**
- ✅ Simpler code (one storage mechanism)
- ✅ Consistent with regular bones
- ❌ Requires refactoring existing code
- ❌ Need to ensure all RootNode detection uses renamed names

## Testing

To test the renaming system:

1. **Load First Model:**
   - Load any FBX file
   - Check console: Should show "Root node name: 'RootNode' (no collision)"
   - Check outliner: RootNode should display as "RootNode"

2. **Load Second Model:**
   - Load another FBX file
   - Check console: Should show "Root node name collision detected: 'RootNode' already exists. Renamed to 'RootNode01'"
   - Check outliner: Second model's root should display as "RootNode01"

3. **Load Third Model:**
   - Load another FBX file
   - Check console: Should show renamed to "RootNode02"
   - Check outliner: Third model's root should display as "RootNode02"

4. **Verify PropertyPanel:**
   - Select "RootNode" from first model
   - Set translation to (10, 0, 0)
   - Select "RootNode01" from second model
   - Set translation to (20, 0, 0)
   - Switch back to "RootNode"
   - Verify translation is still (10, 0, 0) - not (20, 0, 0)

## Files Modified

1. ✅ `src/io/ui/outliner.h` - Added root node name tracking to FBXSceneData, added helper methods
2. ✅ `src/io/ui/outliner.cpp` - Implemented collision detection, renaming, and updated all root node name usage

## Related Features

- **Multi-Model Support**: Enables proper multi-model workflows
- **PropertyPanel**: RootNode transforms now use unique keys
- **Per-Model RootNode Transforms**: Still works with renamed nodes (uses model index, not name)

## Summary

The automatic RootNode renaming system:
- ✅ Prevents name collisions automatically
- ✅ Works transparently (no user intervention needed)
- ✅ Maintains compatibility with existing RootNode detection logic
- ✅ Provides clear identification of which model's RootNode is selected
- ✅ Can potentially simplify the storage system in the future
