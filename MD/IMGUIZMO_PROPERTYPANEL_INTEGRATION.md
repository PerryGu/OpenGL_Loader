# ImGuizmo PropertyPanel Integration

## Overview
This document describes the integration of ImGuizmo gizmo with the PropertyPanel Transforms system. The gizmo now updates PropertyPanel RootNode transforms instead of directly modifying the model, allowing the Transforms window to control the model's position.

## Date
Implementation Date: [Current Session]

## Problem
Previously, the gizmo directly modified the model's `pos`, `rotation`, and `size` members. This bypassed the PropertyPanel system, making it impossible to:
- See gizmo changes in the Transforms window
- Reset transforms using the "Reset Current Bone" button
- Have gizmo and PropertyPanel work together

## Solution Applied

### 1. Added PropertyPanel Setter Methods (`src/io/ui/property_panel.h` and `.cpp`)

**Added methods to allow gizmo to update PropertyPanel values:**
```cpp
// Set slider values (for gizmo to update PropertyPanel transforms)
void setSliderRotations(const glm::vec3& rotations);
void setSliderTranslations(const glm::vec3& translations);
void setSliderScales(const glm::vec3& scales);

// Get currently selected node name
std::string getSelectedNodeName() const { return mSelectedNod_name; }
```

**Implementation:**
- These methods update the slider values and save them to the bone transforms map
- This ensures the values persist and are visible in the PropertyPanel UI

### 2. Updated Gizmo to Read from PropertyPanel (`src/io/scene.cpp`)

**Changed gizmo matrix building:**
- **Before**: Built matrix from `model.pos`, `model.rotation`, `model.size`
- **After**: When RootNode is selected, builds matrix from PropertyPanel RootNode transforms

**Code:**
```cpp
// Get the currently selected node from outliner
std::string selectedNode = mOutliner.getSelectedNode();
bool isRootNode = (!selectedNode.empty() && 
                   (selectedNode.find("Root") != std::string::npos || 
                    selectedNode == "RootNode"));

if (isRootNode) {
    // Use PropertyPanel RootNode transforms (gizmo will update these)
    glm::vec3 translation = mPropertyPanel.getSliderTrans_update();
    glm::vec3 rotationEuler = mPropertyPanel.getSliderRot_update();  // Euler angles in degrees
    glm::vec3 scale = mPropertyPanel.getSliderScale_update();
    
    // Convert Euler angles (degrees) to quaternion
    glm::quat rotation = glm::quat(glm::radians(rotationEuler));
    
    // Build matrix from PropertyPanel transforms
    modelMatrix = glm::translate(modelMatrix, translation);
    modelMatrix = modelMatrix * glm::mat4_cast(rotation);
    modelMatrix = glm::scale(modelMatrix, scale);
}
```

### 3. Updated Gizmo to Write to PropertyPanel (`src/io/scene.cpp`)

**Changed gizmo update logic:**
- **Before**: Directly updated `model.pos`, `model.rotation`, `model.size`
- **After**: Updates PropertyPanel RootNode transforms (which then control the model)

**Code:**
```cpp
if (manipulated) {
    // Decompose matrix to get translation, rotation, and scale
    glm::decompose(newModelMatrix, scale, rotation, translation, skew, perspective);
    
    // Convert quaternion to Euler angles (degrees) for PropertyPanel
    glm::vec3 rotationEuler = glm::degrees(glm::eulerAngles(rotation));
    
    // Update PropertyPanel transforms (this will feed into the model)
    if (isRootNode) {
        mPropertyPanel.setSliderTranslations(translation);
        mPropertyPanel.setSliderRotations(rotationEuler);
        mPropertyPanel.setSliderScales(scale);
    }
}
```

### 4. Applied RootNode Transforms to Model (`src/main.cpp`)

**Added code to apply RootNode transforms to model's root transform:**
```cpp
// Apply RootNode transforms from PropertyPanel to model's root transform (pos/rotation/size)
std::string currentSelectedNode = scene.mOutliner.getSelectedNode();
bool isRootNodeSelected = (!currentSelectedNode.empty() && 
                           (currentSelectedNode.find("Root") != std::string::npos || 
                            currentSelectedNode == "RootNode"));

if (isRootNodeSelected) {
    // Get RootNode transforms from PropertyPanel
    glm::vec3 rootTranslation = scene.mPropertyPanel.getSliderTrans_update();
    glm::vec3 rootRotationEuler = scene.mPropertyPanel.getSliderRot_update();
    glm::vec3 rootScale = scene.mPropertyPanel.getSliderScale_update();
    
    // Convert Euler angles (degrees) to quaternion
    glm::quat rootRotation = glm::quat(glm::radians(rootRotationEuler));
    
    // Apply RootNode transforms to the selected model's root transform
    ModelInstance* selectedModel = modelManager.getSelectedModel();
    if (selectedModel != nullptr) {
        selectedModel->model.pos = rootTranslation;
        selectedModel->model.rotation = rootRotation;
        selectedModel->model.size = rootScale;
    }
}
```

## How It Works Now

### Data Flow:
1. **User manipulates gizmo** → Gizmo decomposes matrix
2. **Gizmo updates PropertyPanel** → `setSliderTranslations/Rotations/Scales()` called
3. **PropertyPanel stores values** → Saved in `mBoneTranslations/Rotations/Scales` map for RootNode
4. **PropertyPanel values read** → `getSliderTrans/Rot/Scale_update()` returns values
5. **Model applies transforms** → RootNode transforms applied to `model.pos/rotation/size` in main.cpp
6. **Model renders** → Uses `model.pos/rotation/size` for root transform

### Reset Behavior:
- When user sets Transforms to 0 (or clicks "Reset Current Bone"):
  - PropertyPanel RootNode transforms become (0, 0, 0) for translation/rotation, (1, 1, 1) for scale
  - Model's `pos` becomes (0, 0, 0), `rotation` becomes identity, `size` becomes (1, 1, 1)
  - Model returns to center of axes

## Benefits

1. **Unified Control**: Gizmo and PropertyPanel work together
2. **Visibility**: Gizmo changes are visible in Transforms window
3. **Reset Support**: "Reset Current Bone" button works with gizmo
4. **Consistency**: Single source of truth (PropertyPanel) for RootNode transforms

## Files Modified

1. `src/io/ui/property_panel.h` - Added setter methods and getSelectedNodeName()
2. `src/io/ui/property_panel.cpp` - Implemented setter methods
3. `src/io/scene.cpp` - Updated gizmo to read from and write to PropertyPanel
4. `src/main.cpp` - Added code to apply RootNode transforms to model

## Testing

After rebuilding:
1. ✅ Select a model in viewport → RootNode should be selected in outliner
2. ✅ Manipulate gizmo → Transforms window should update
3. ✅ Set Transforms to 0 → Model should return to center
4. ✅ Click "Reset Current Bone" → Model should return to center
5. ✅ Gizmo and PropertyPanel should stay in sync

## Notes

- Rotation is stored as Euler angles (degrees) in PropertyPanel, converted to quaternion for model
- Only RootNode transforms control model position (other nodes control bones)
- Gizmo only works when RootNode is selected (when model is selected via viewport)
