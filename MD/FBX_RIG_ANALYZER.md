# FBX Rig Analyzer Implementation

## Overview
This document describes the implementation of the FBX Rig Analyzer feature, which automatically detects and reports the rig root container (parent group like "Rig_GRP") of imported FBX files. The rig root is the NULL_NODE group that contains the rig skeleton hierarchy, not the first bone itself.

## Date
Implementation Date: 2024

## Purpose
When an FBX file is imported into the OpenGL_loader tool, the system automatically analyzes the file's rig structure to find the rig root container. The rig root is the parent group (typically a NULL_NODE like "Rig_GRP") that contains the first bone/joint in the skeleton hierarchy. This information is printed to the console once per import, providing users with immediate visibility into the rig structure of their models.

## Features

1. **Automatic Detection**: Automatically finds the rig root when an FBX file is imported
2. **Single Print**: Prints the rig root name once per import (no loops or intervals)
3. **Modular Design**: Clean, separated utility functions in dedicated files
4. **Console Output**: Displays results in the debug console window

## Implementation Details

### File Structure

```
src/io/
├── fbx_rig_analyzer.h      # Header file with function declarations
└── fbx_rig_analyzer.cpp    # Implementation of rig analysis functions
```

### Key Functions

#### `FBXRigAnalyzer::findRigRoot(const ofbx::IScene* scene)`
- **Purpose**: Finds the rig root container (parent NULL_NODE group) in an FBX scene
- **Parameters**: 
  - `scene`: Pointer to the FBX scene to analyze
- **Returns**: 
  - `std::string`: Name of the rig root container (NULL_NODE like "Rig_GRP"), or first LIMB_NODE if no container found, or empty string if no rig found
- **Behavior**: 
  1. Traverses the scene starting from the root node
  2. Finds the first LIMB_NODE (bone) in the hierarchy (e.g., "Hips")
  3. Traverses up the parent chain to find the containing NULL_NODE group (e.g., "Rig_GRP")
  4. Returns the name of that parent group
  5. If no parent NULL_NODE is found, returns the first LIMB_NODE name as fallback

#### `FBXRigAnalyzer::findFirstLimbNode(const ofbx::Object* object)`
- **Purpose**: Recursively searches for the first LIMB_NODE in the object hierarchy
- **Parameters**: 
  - `object`: The object to start searching from
- **Returns**: 
  - `const ofbx::Object*`: Pointer to the first LIMB_NODE found, or nullptr if none found
- **Behavior**: 
  - Checks if the current object is a LIMB_NODE
  - If not, recursively searches all children
  - Returns the first LIMB_NODE found in depth-first order

#### `FBXRigAnalyzer::findParentNullNode(const ofbx::Object* object)`
- **Purpose**: Finds the parent NULL_NODE container that contains the given object
- **Parameters**: 
  - `object`: The object to start searching from (typically a LIMB_NODE)
- **Returns**: 
  - `const ofbx::Object*`: Pointer to the parent NULL_NODE found, or nullptr if none found
- **Behavior**: 
  - Traverses up the parent chain from the given object
  - Returns the first NULL_NODE parent encountered (typically a group like "Rig_GRP")
  - Returns nullptr if no NULL_NODE parent is found

### Integration Point

The rig analyzer is integrated into the FBX loading process:

**File**: `src/io/ui/outliner.cpp`
**Method**: `Outliner::loadFBXfile()`

**Code Flow**:
```
User imports FBX file
    ↓
Outliner::loadFBXfile() called
    ↓
FBX scene loaded successfully
    ↓
FBXRigAnalyzer::findRigRoot() called
    ↓
Rig root name printed to console (once)
    ↓
Loading process continues
```

**Implementation**:
```cpp
// After successfully loading FBX scene
std::string rigRootName = FBXRigAnalyzer::findRigRoot(scene);
if (!rigRootName.empty()) {
    std::cout << "[Rig Analyzer] Rig Root: '" << rigRootName << "'" << std::endl;
} else {
    std::cout << "[Rig Analyzer] No rig root (LIMB_NODE) found in this FBX file" << std::endl;
}
```

## Console Output

### When Rig Root Container is Found
```
[Rig Analyzer] Rig Root: 'Rig_GRP'
```

### When No Container Found (Fallback to First Bone)
```
[Rig Analyzer] Rig Root: 'Hips'
```
(If no parent NULL_NODE group is found, it falls back to the first LIMB_NODE name)

### When No Rig Root is Found
```
[Rig Analyzer] No rig root (LIMB_NODE) found in this FBX file
```

## Technical Details

### FBX Object Types
The analyzer specifically looks for objects of type `LIMB_NODE`, which represents bones/joints in the FBX skeleton hierarchy. Other object types include:
- `ROOT`: Scene root node
- `MESH`: Geometry meshes
- `NULL_NODE`: Null/empty nodes
- `GEOMETRY`: Geometry objects
- `MATERIAL`: Material definitions
- etc.

### Search Algorithm
The search uses a **two-phase approach**:

**Phase 1: Find First Bone**
1. Start from the scene root
2. Use depth-first traversal to find the first LIMB_NODE (e.g., "Hips")
3. This identifies where the rig skeleton begins

**Phase 2: Find Parent Container**
1. Starting from the first LIMB_NODE found
2. Traverse up the parent chain using `getParent()`
3. Look for the first NULL_NODE parent (typically "Rig_GRP")
4. Return that parent group's name

**Fallback Behavior:**
- If no parent NULL_NODE is found, return the first LIMB_NODE name
- This handles cases where the rig structure doesn't have a containing group

### Why Parent NULL_NODE?
In most FBX files, the rig hierarchy is organized as:
- **Scene Root** → Contains various groups
  - **Rig_GRP** (NULL_NODE) → Contains the rig skeleton
    - **Hips** (LIMB_NODE) → First bone of the rig
      - **Spine** (LIMB_NODE) → Child bone
      - etc.

The parent NULL_NODE (like "Rig_GRP") is the logical container for the entire rig, making it the appropriate "rig root" to report to users.

## Usage

### Automatic Operation
The rig analyzer runs automatically when:
1. User imports an FBX file via File > Open
2. The FBX file is successfully loaded
3. The scene is added to the Outliner

### No User Interaction Required
- No manual triggering needed
- No UI controls required
- Results appear automatically in console

## Benefits

1. **Immediate Feedback**: Users can see the rig structure as soon as a file is imported
2. **Debugging Aid**: Helps identify rig issues or understand file structure
3. **Documentation**: Provides visibility into the rig hierarchy
4. **Non-Intrusive**: Single print per import, no console spam

## Limitations

1. **First LIMB_NODE Only**: Returns only the first LIMB_NODE found, not the entire rig hierarchy
2. **No Validation**: Doesn't verify if the found node is actually the rig root (just first one found)
3. **No Rig Structure**: Doesn't analyze the complete rig structure, only finds the root

## Future Enhancements

Potential improvements:
1. **Full Rig Hierarchy**: Analyze and print the complete rig structure
2. **Rig Validation**: Verify rig integrity and report issues
3. **Multiple Roots**: Handle cases with multiple rig roots
4. **Rig Visualization**: Display rig structure in UI
5. **Rig Export**: Export rig information to file

## Code Organization

### Modularity
- **Separate Namespace**: `FBXRigAnalyzer` namespace keeps functions organized
- **Dedicated Files**: Separate header and implementation files
- **Clear Interface**: Simple, focused API with clear function names
- **No Dependencies**: Utility functions don't depend on other application code

### Maintainability
- **Well Documented**: Functions have clear comments explaining purpose
- **Single Responsibility**: Each function has one clear purpose
- **Easy to Extend**: Can easily add more rig analysis functions

## Testing

To test the rig analyzer:
1. Import an FBX file with a rig (e.g., animated character)
2. Check the console output
3. Verify the rig root name is printed once
4. Import another FBX file - should print its rig root
5. Import an FBX without rig - should print "No rig root found"

## Related Features

- **Outliner**: Displays the complete FBX hierarchy including rig nodes
- **PropertyPanel**: Allows editing transforms of rig bones
- **Animation System**: Uses rig structure for skeletal animation

## Files Modified

1. **Created**:
   - `src/io/fbx_rig_analyzer.h` - Header file with function declarations
   - `src/io/fbx_rig_analyzer.cpp` - Implementation of rig analysis functions
   - `MD/FBX_RIG_ANALYZER.md` - This documentation file

2. **Modified**:
   - `src/io/ui/outliner.cpp` - Added rig analyzer call in `loadFBXfile()`

## Enhancements (v2.0.0)

### Improved Hierarchy Detection
**Location**: `src/graphics/fbx_rig_analyzer.cpp`

#### Enhanced `findParentNullNode` Logic
- **Traversal Depth**: Now traverses up to the highest NULL_NODE that is a direct child of the Scene Root
- **Robustness**: Handles complex hierarchy structures with multiple NULL_NODE levels
- **Fallback**: Returns first NULL_NODE parent if no direct child of Scene Root is found

#### Enhanced `findRigRoot` Logic
- **Multiple LIMB_NODES at Same Depth**: If multiple LIMB_NODES are found at the same depth, identifies the one with the most descendants
- **Descendant Count**: Uses recursive counting to determine which LIMB_NODE has the largest subtree
- **Robustness**: Handles complex rig structures with multiple potential root candidates

**Code Improvements**:
```cpp
// Enhanced logic to find LIMB_NODE with most descendants
if (multiple LIMB_NODES at same depth) {
    // Count descendants for each candidate
    int maxDescendants = 0;
    const ofbx::Object* bestCandidate = nullptr;
    for (each candidate) {
        int descendantCount = countDescendants(candidate);
        if (descendantCount > maxDescendants) {
            maxDescendants = descendantCount;
            bestCandidate = candidate;
        }
    }
    return bestCandidate;
}
```

### Code Quality Improvements
- **Removed `const_cast`**: Replaced with proper const-correct usage where ofbx API supports it
- **Professional Logging**: Uses `LOG_DEBUG` from `logger.h` instead of `std::cout`
- **English Comments**: All Doxygen comments are in English and describe fallback logic clearly

## Summary

The FBX Rig Analyzer provides automatic detection and reporting of rig roots in imported FBX files. It's implemented as a clean, modular utility that integrates seamlessly into the existing FBX loading process, providing users with immediate visibility into their model's rig structure without any additional user interaction required.

**Version 2.0.0 Enhancements**:
- Improved hierarchy detection for complex rig structures
- Enhanced robustness for multiple LIMB_NODE candidates
- Professional logging and code quality improvements
