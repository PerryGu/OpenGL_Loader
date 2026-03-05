# Multi-Model Transform System Fixes

## Date
2024

## Overview
This document describes the fixes implemented to resolve issues with multiple models in a scene, including:
1. Models jumping to each other's positions when deselected
2. Double transformation causing characters to move twice as far as intended
3. Viewport selection becoming difficult after moving characters

## Problems Solved

### Problem 1: Models Jumping to Each Other's Positions

**Symptoms:**
- When loading a second character, it would appear at the first character's position instead of the scene center
- When deselecting a moved character, it would jump to another character's position
- Models would overlap each other incorrectly

**Root Cause:**
- Only the **selected** model used PropertyPanel RootNode transforms
- When nothing was selected, other models fell back to their internal `model.pos/rotation/size` (which were reset to identity)
- This caused models to jump to (0,0,0) or inherit incorrect transforms from other models
- Models were sharing transform state instead of having isolated hierarchies

**Solution:**
- Implemented **per-model RootNode transform matrices** for ALL models, not just the selected one
- Each model has its own RootNode entry in PropertyPanel (e.g., "RootNode", "RootNode01", "RootNode02")
- `modelRootNodeMatrices` map is built for all models every frame, containing each model's RootNode transform from PropertyPanel
- `ModelManager::renderAllWithRootNodeTransforms()` applies each model's specific RootNode transform
- Models are now completely isolated - each uses its own RootNode transform independently

**Files Modified:**
- `src/main.cpp`: Builds `modelRootNodeMatrices` for all models, passes to `renderAllWithRootNodeTransforms()`
- `src/graphics/model_manager.cpp`: Implements `renderAllWithRootNodeTransforms()` to apply per-model transforms
- `src/graphics/model_manager.h`: Added `renderAllWithRootNodeTransforms()` method declaration

### Problem 2: Double Transformation

**Symptoms:**
- Moving a character 20 units would result in it moving 20 units correctly
- But when deselected, the character would jump another 20 units (total 40 units)
- When selected again, it would return to the correct 20-unit position

**Root Cause:**
- RootNode transform was being applied **twice**:
  1. From the FBX file in the bone hierarchy (`ReadNodeHierarchy` processes `mScene->mRootNode`)
  2. From PropertyPanel via the model matrix (per-model RootNode transform matrices)
- When RootNode was selected, PropertyPanel values were also being applied to bone transforms via `ui_data`
- This caused the transform to be applied once through the bone hierarchy and once through the model matrix

**Solution:**
1. **RootNode transform in bone hierarchy set to identity:**
   - Modified `Model::ReadNodeHierarchy()` in `src/graphics/model.cpp`
   - Detects if current node is RootNode (`pNode == mScene->mRootNode`)
   - If RootNode, skips all transform calculations and sets `NodeTransformation` to identity
   - RootNode transform now comes ONLY from the model matrix, not from bone hierarchy

2. **PropertyPanel values excluded from bone transforms when RootNode is selected:**
   - Modified `src/main.cpp` to check if PropertyPanel's selected node is a RootNode
   - If RootNode is selected, sets `ui_data.mSliderRotations/Translations/Scales` to identity
   - This prevents RootNode from being transformed in the bone hierarchy
   - RootNode transform comes ONLY from per-model RootNode transform matrices

**Files Modified:**
- `src/graphics/model.cpp`: `ReadNodeHierarchy()` now sets RootNode transform to identity
- `src/main.cpp`: Excludes RootNode from bone transforms when RootNode is selected in PropertyPanel

### Problem 3: Viewport Selection After Moving Characters

**Symptoms:**
- After moving a character, it became harder to select it in the viewport with the mouse
- Selection would work before moving, but fail after moving
- Even with a single character, selection became difficult after moving it from center

**Root Cause:**
- The bounding box used for raycast intersection was in **local space**
- But the model was rendered in **world space** with RootNode transform applied
- The old code only transformed the bounding box when:
  1. The rig root was selected
  2. AND it was the currently selected model
- Since we now use per-model RootNode transforms for ALL models, the bounding box must be transformed for all models

**Solution:**
- Build per-model RootNode transform matrices for selection (same as rendering)
- Transform bounding box corners from local space to world space using each model's RootNode transform
- Recalculate axis-aligned bounding box (AABB) in world space
- Test raycast intersection against transformed bounding boxes
- This ensures the bounding box matches the model's rendered position

**Files Modified:**
- `src/main.cpp`: Builds `modelRootNodeMatrices` for selection, transforms bounding boxes for all models before raycast testing

## Technical Implementation Details

### Per-Model RootNode Transform System

**Building Transform Matrices:**
```cpp
// Build per-model RootNode transform matrices
std::map<int, glm::mat4> modelRootNodeMatrices;

for (size_t i = 0; i < modelManager.getModelCount(); i++) {
    int modelIndex = static_cast<int>(i);
    std::string modelRootNodeName = scene.mOutliner.getRootNodeName(modelIndex);
    
    if (!modelRootNodeName.empty()) {
        // Get this model's RootNode transforms from PropertyPanel
        const auto& allBoneTranslations = scene.mPropertyPanel.getAllBoneTranslations();
        const auto& allBoneRotations = scene.mPropertyPanel.getAllBoneRotations();
        const auto& allBoneScales = scene.mPropertyPanel.getAllBoneScales();
        
        auto transIt = allBoneTranslations.find(modelRootNodeName);
        auto rotIt = allBoneRotations.find(modelRootNodeName);
        auto scaleIt = allBoneScales.find(modelRootNodeName);
        
        glm::vec3 translation = (transIt != allBoneTranslations.end()) ? transIt->second : glm::vec3(0.0f);
        glm::vec3 rotationEuler = (rotIt != allBoneRotations.end()) ? rotIt->second : glm::vec3(0.0f);
        glm::vec3 scale = (scaleIt != allBoneScales.end()) ? scaleIt->second : glm::vec3(1.0f);
        
        // Convert Euler angles (degrees) to quaternion
        glm::quat rotation = glm::quat(glm::radians(rotationEuler));
        
        // Build model matrix from this model's RootNode transforms
        glm::mat4 modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::translate(modelMatrix, translation);
        modelMatrix = modelMatrix * glm::mat4_cast(rotation);
        modelMatrix = glm::scale(modelMatrix, scale);
        
        modelRootNodeMatrices[modelIndex] = modelMatrix;
    }
}
```

**Rendering with Per-Model Transforms:**
```cpp
// Render all models with their independent RootNode transforms
modelManager.renderAllWithRootNodeTransforms(shader, ui_data, Transforms, modelRootNodeMatrices);
```

**RootNode Identity in Bone Hierarchy:**
```cpp
// In Model::ReadNodeHierarchy()
bool isRootNode = (pNode == mScene->mRootNode);

if (!isRootNode) {
    // Normal bone processing - apply animation and manual transforms
    // ... transform calculations ...
} else {
    // RootNode: Always use identity transform in bone hierarchy
    // RootNode transform comes from model matrix only (PropertyPanel), not from bone hierarchy
    NodeTransformation.InitIdentity();
}
```

**Excluding RootNode from Bone Transforms:**
```cpp
// Check if PropertyPanel's selected node is a RootNode
std::string propertyPanelSelectedNode = scene.mPropertyPanel.getSelectedNodeName();
bool isSelectedNodeRootNode = false;

if (!propertyPanelSelectedNode.empty()) {
    isSelectedNodeRootNode = (propertyPanelSelectedNode.find("Root") != std::string::npos || 
                             propertyPanelSelectedNode == "RootNode");
    // Also check if it matches any model's RootNode name
    // ...
}

if (isSelectedNodeRootNode) {
    // RootNode is selected - exclude it from bone transforms
    ui_data.mSliderRotations = glm::vec3(0.0f, 0.0f, 0.0f);
    ui_data.mSliderTraslations = glm::vec3(0.0f, 0.0f, 0.0f);
    ui_data.mSliderScales = glm::vec3(1.0f, 1.0f, 1.0f);
} else {
    // Non-RootNode selected - apply PropertyPanel values to bone transforms
    ui_data.mSliderRotations = scene.mPropertyPanel.getSliderRot_update();
    ui_data.mSliderTraslations = scene.mPropertyPanel.getSliderTrans_update();
    ui_data.mSliderScales = scene.mPropertyPanel.getSliderScale_update();
}
```

**Viewport Selection with Transformed Bounding Boxes:**
```cpp
// Build per-model RootNode transform matrices for selection
std::map<int, glm::mat4> modelRootNodeMatrices;
// ... build matrices ...

for (size_t i = 0; i < modelManager.getModelCount(); i++) {
    // ... calculate bounding box in local space ...
    
    // Transform bounding box to world space using this model's RootNode transform
    auto matrixIt = modelRootNodeMatrices.find(static_cast<int>(i));
    if (matrixIt != modelRootNodeMatrices.end()) {
        glm::mat4 rootNodeTransform = matrixIt->second;
        
        // Transform all 8 corners from local space to world space
        glm::vec3 corners[8] = { /* ... */ };
        for (int j = 0; j < 8; j++) {
            corners[j] = glm::vec3(rootNodeTransform * glm::vec4(corners[j], 1.0f));
        }
        
        // Recalculate AABB in world space
        min = max = corners[0];
        for (int j = 1; j < 8; j++) {
            min = glm::min(min, corners[j]);
            max = glm::max(max, corners[j]);
        }
    }
    
    // Test raycast intersection against transformed bounding box
    if (Raycast::rayIntersectsAABB(ray, min, max, t)) {
        // ... select model ...
    }
}
```

## Benefits

1. **Isolated Model Hierarchies:** Each model has its own RootNode transform, preventing cross-contamination
2. **No Double Transformation:** RootNode transform is applied only once (via model matrix), not through bone hierarchy
3. **Correct Viewport Selection:** Bounding boxes are transformed to world space, matching rendered model positions
4. **Consistent Behavior:** Models maintain their positions regardless of selection state
5. **Scalable:** Works correctly with any number of models in the scene

## Related Documentation

- `MD/PER_MODEL_ROOTNODE_TRANSFORMS.md`: Initial implementation of per-model RootNode transforms
- `MD/ROOTNODE_AUTO_RENAME.md`: Automatic RootNode renaming to prevent name collisions
- `MD/IMGUIZMO_FIX_DOUBLE_TRANSFORMATION_FINAL.md`: Previous double transformation fix (superseded by this fix)
- `MD/VIEWPORT_SELECTION.md`: Viewport selection system documentation
- `MD/TRANSFORM_SYSTEM.md`: Transform hierarchy system documentation

## Testing

To verify the fixes work correctly:

1. **Test Model Isolation:**
   - Load multiple models
   - Move each model to different positions
   - Deselect all models
   - Verify each model stays at its correct position (no jumping)

2. **Test Double Transformation:**
   - Load a single model
   - Move it 20 units on X axis
   - Deselect it
   - Verify it stays at 20 units (doesn't jump to 40 units)
   - Select it again
   - Verify it's still at 20 units

3. **Test Viewport Selection:**
   - Load a model
   - Move it away from center
   - Try to select it in the viewport
   - Verify selection works correctly (bounding box matches model position)
