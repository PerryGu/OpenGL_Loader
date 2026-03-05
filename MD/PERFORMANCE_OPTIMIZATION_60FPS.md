# Performance Optimization - 60+ FPS Achievement

**Date:** 2026-02-18 to 2026-02-20  
**Status:** ✅ Implemented

## Overview
Implemented comprehensive performance optimizations to achieve and maintain 60+ FPS with multiple characters loaded, ensuring smooth UI responsiveness.

## Key Optimizations

### 1. Bounding Box Calculation (O(Vertices) → O(Bones))

**Problem:** Bounding box calculation was iterating through all vertices every frame, causing severe performance drops.

**Solution:** Switched to bone-based calculation:
```cpp
// OLD: O(Vertices) - iterated through mesh->mNumVertices
// NEW: O(Bones) - iterates through m_BoneGlobalTransforms
for (const auto& boneTransform : m_BoneGlobalTransforms) {
    glm::vec3 bonePos = boneTransform.translation;
    // Update min/max based on bone positions
}
// Add padding (0.5-1.0 units) to cover mesh volume
```

**Impact:** Reduced bounding box calculation time by ~95% for typical models.

### 2. String Operation Elimination

**Problem:** String comparisons and map lookups in hot paths (animation loop).

**Solutions:**
- **Index-based bone access:** Replaced `std::map<std::string, ...>` with `std::vector<...>`
- **Linear skeleton structure:** Flattened hierarchy to `std::vector<LinearBone>`
- **Pre-computed mappings:** `std::map<const aiNode*, int>` for node-to-bone index

**Impact:** Eliminated all string operations from frame-by-frame animation loop.

### 3. Selection Caching

**Problem:** `findModelIndexForSelectedObject()` called 60 times per second.

**Solution:** Cache selection pointer:
```cpp
static const void* last_selected_object_ptr = nullptr;
if (mOutliner.g_selected_object != last_selected_object_ptr) {
    // Only search when selection actually changes
    int modelIndex = mOutliner.findModelIndexForSelectedObject();
    last_selected_object_ptr = mOutliner.g_selected_object;
}
```

**Impact:** Reduced selection overhead from O(N) per frame to O(1) per frame.

### 4. Outliner Name Caching

**Problem:** `getRigRootName()` and `getRootNodeName()` called every frame.

**Solution:** Cache names per model index:
```cpp
static std::string cachedRigRootName = "";
static std::string cachedModelRootNodeName = "";
static int lastModelIndexForNames = -1;

if (selectedModelIndex != lastModelIndexForNames) {
    cachedRigRootName = mOutliner.getRigRootName(selectedModelIndex);
    cachedModelRootNodeName = mOutliner.getRootNodeName(selectedModelIndex);
    lastModelIndexForNames = selectedModelIndex;
}
```

**Impact:** Eliminated per-frame tree traversals.

### 5. Dirty Flag Optimization

**Problem:** Animation transforms recalculated every frame even when nothing changed.

**Solution:** Multi-level dirty flags:
```cpp
if (AnimationTime == m_lastAnimationTime && !m_transformsDirty) {
    // Skip update - use cached transforms
    return;
}
```

**Impact:** Skips expensive calculations when animation time and UI data are unchanged.

### 6. Root Bone Filtering

**Problem:** Bounding box stretched to world origin due to root bones at (0,0,0).

**Solution:** Pre-computed `isRootBone` flag during load:
```cpp
struct LinearBone {
    bool isRootBone;  // Set during flattenHierarchy
    // ...
};

// In bounding box calculation:
if (bone.isRootBone && isAtOrigin) continue;
```

**Impact:** Tighter bounding boxes, faster calculations.

## Performance Metrics

### Before Optimization
- **1 Model:** ~30 FPS
- **2 Models:** ~15 FPS
- **3+ Models:** <10 FPS
- **UI Responsiveness:** Laggy, delayed updates

### After Optimization
- **1 Model:** 60+ FPS
- **2 Models:** 60+ FPS
- **3+ Models:** 60+ FPS (with V-Sync off: 400+ FPS)
- **UI Responsiveness:** Smooth, instant updates

## Files Modified
- `src/graphics/model.cpp` - Bounding box optimization, linear skeleton
- `src/graphics/model.h` - LinearBone structure, index-based access
- `src/io/scene.cpp` - Selection caching, outliner name caching
- `src/application.cpp` - Removed redundant selection searches

## Related Documentation
- `MD/BOUNDING_BOX_BONE_OPTIMIZATION.md`
- `MD/BOUNDING_BOX_TOGGLE_OPTIMIZATION.md`
- `MD/BOUNDING_BOX_PERFORMANCE.md`
