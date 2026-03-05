# Skeleton Rogue Line Fix

## Date
Created: 2026-02-23

## Problem
The skeleton hierarchy was displaying correctly (spine and legs visible thanks to the joints), but there was a rogue "infinite" line shooting up/down. This was happening because:
1. The absolute root node was trying to connect to a null parent
2. A dummy end-site had an uninitialized/extreme transform

## Solution
Added mathematical sanity checks in `Model::DrawSkeleton` to filter out garbage lines without using string names (which would break the hierarchy).

## Implementation

### Location
`src/graphics/model.cpp` - `Model::DrawSkeleton` function (lines 226-255)

### Changes
Replaced the previous root-node-at-origin check with comprehensive mathematical sanity checks:

1. **Parent Pointer Validation**: Check that `pNode->mParent != nullptr`
   - Prevents drawing lines from absolute root node trying to connect to null parent

2. **Bone Length Check**: Verify `glm::distance(parentWorldPos, nodeWorldPos) < 100.0f`
   - Prevents drawing lines with extreme/uninitialized transforms
   - Filters out bones longer than 100 units (realistic bone length limit)

3. **Parent Position Check**: Ensure `parentWorldPos != glm::vec3(0.0f, 0.0f, 0.0f)`
   - Prevents drawing from dummy end-sites at the exact scene artifact origin
   - More robust than checking if it's the root node (catches any node at origin)

### Code Logic
```cpp
// Mathematical sanity check: ensure node has a valid parent pointer
if (pNode->mParent == nullptr) {
    continue;
}

// Calculate bone length and check parent position
float boneLength = glm::distance(parentWorldPos, nodeWorldPos);
bool isParentAtOrigin = (parentWorldPos == glm::vec3(0.0f, 0.0f, 0.0f));

// Only add line if ALL conditions are met:
// - Parent pointer is valid (checked above)
// - Bone length is realistic (< 100.0f units)
// - Parent position is not at exact origin (0,0,0)
if (boneLength < 100.0f && !isParentAtOrigin) {
    lineVertices.push_back(parentWorldPos);
    lineVertices.push_back(nodeWorldPos);
}
```

## Benefits

1. **Mathematical Approach**: No string-based filtering that could break hierarchy
2. **Comprehensive Filtering**: Catches multiple edge cases (null parent, extreme transforms, origin artifacts)
3. **Maintains Hierarchy**: Preserves the correct skeleton structure while filtering garbage
4. **Performance**: Simple mathematical checks with minimal overhead

## Testing
- Verify skeleton displays correctly without rogue lines
- Confirm spine and legs remain visible
- Check that valid bones are still drawn correctly
- Ensure no valid skeleton connections are filtered out

## Related Files
- `src/graphics/model.cpp` - Main implementation
- `src/graphics/model.h` - Function declaration
