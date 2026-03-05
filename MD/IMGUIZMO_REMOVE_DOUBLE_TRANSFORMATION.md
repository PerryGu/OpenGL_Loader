# ImGuizmo Remove Double Transformation - Final Fix

## Problem
Even after initializing PropertyPanel from model transforms, there was still a double transformation issue. The gizmo and character were not aligned - the character moved twice the distance from the gizmo because it was receiving transform values in two different places:
1. The model's `pos/rotation/size` members (from original gizmo manipulation)
2. PropertyPanel RootNode values being applied to `model.pos/rotation/size`

This caused the model to be transformed twice: once from its original `pos/rotation/size` values, and once from PropertyPanel values being applied to those same members.

## Root Cause
The issue was in the approach:
1. `Model::render()` always uses `model.pos/rotation/size` to build the transformation matrix
2. We were setting `model.pos/rotation/size` from PropertyPanel RootNode values
3. But `model.pos/rotation/size` might have had values from before, and we were applying PropertyPanel on top

The fundamental problem: **When RootNode is selected, we should NOT use `model.pos/rotation/size` at all. Instead, we should use PropertyPanel RootNode values directly for rendering, completely bypassing `model.pos/rotation/size`.**

## Solution Applied

### 1. Modified `Model::render()` to Accept Optional Custom Model Matrix (`src/graphics/model.h`, `src/graphics/model.cpp`)

**Added optional parameter:**
```cpp
// If customModelMatrix is provided (not nullptr), it will be used instead of building from pos/rotation/size
// This allows external control of the model transform (e.g., from PropertyPanel RootNode)
void render(Shader shader, const glm::mat4* customModelMatrix = nullptr);
```

**Implementation:**
```cpp
void Model::render(Shader shader, const glm::mat4* customModelMatrix) {
    // ...
    glm::mat4 model = glm::mat4(1.0f);
    if (customModelMatrix != nullptr) {
        // Use provided matrix (from PropertyPanel RootNode transforms)
        model = *customModelMatrix;
    } else {
        // Build model matrix: Translation * Rotation * Scale
        model = glm::translate(model, pos);
        model = model * glm::mat4_cast(rotation);
        model = glm::scale(model, size);
    }
    // ...
}
```

**Why this approach:**
- When RootNode is selected, we pass PropertyPanel RootNode transform matrix directly
- Model's `pos/rotation/size` are completely ignored in this case
- No double transformation - only PropertyPanel values are used
- When RootNode is NOT selected, model uses its own `pos/rotation/size` as before

### 2. Updated `ModelManager::renderAll()` to Accept Optional Model Matrix (`src/graphics/model_manager.h`, `src/graphics/model_manager.cpp`)

**Added parameters:**
```cpp
// If customModelMatrix is provided and modelIndex matches a model, that matrix will be used instead of model's pos/rotation/size
// This allows PropertyPanel RootNode transforms to control the model transform directly
void renderAll(Shader& shader, UI_data& ui_data, std::vector<glm::mat4>& transforms, 
               int modelIndex = -1, const glm::mat4* customModelMatrix = nullptr);
```

**Implementation:**
- Loops through all models
- For the model at `modelIndex`, uses `customModelMatrix` if provided
- Otherwise, model uses its own `pos/rotation/size` (default behavior)

### 3. Updated `main.cpp` to Build and Pass RootNode Matrix (`src/main.cpp`)

**Removed:**
- Code that was setting `model.pos/rotation/size` from PropertyPanel (this was causing double transformation)

**Added:**
- Build model matrix from PropertyPanel RootNode transforms
- Pass this matrix to `renderAll()` when RootNode is selected
- Model's `pos/rotation/size` are NOT modified when RootNode is selected

**Key changes:**
```cpp
if (isRootNodeSelected) {
    // Get RootNode transforms from PropertyPanel
    glm::vec3 rootTranslation = scene.mPropertyPanel.getSliderTrans_update();
    glm::vec3 rootRotationEuler = scene.mPropertyPanel.getSliderRot_update();
    glm::vec3 rootScale = scene.mPropertyPanel.getSliderScale_update();
    
    glm::quat rootRotation = glm::quat(glm::radians(rootRotationEuler));
    
    // Build model matrix from PropertyPanel RootNode transforms
    // This matrix will be used INSTEAD of model.pos/rotation/size to prevent double transformation
    rootNodeModelMatrix = glm::mat4(1.0f);
    rootNodeModelMatrix = glm::translate(rootNodeModelMatrix, rootTranslation);
    rootNodeModelMatrix = rootNodeModelMatrix * glm::mat4_cast(rootRotation);
    rootNodeModelMatrix = glm::scale(rootNodeModelMatrix, rootScale);
    
    // Find selected model index and pass matrix to renderAll
    // ...
}

// Pass RootNode model matrix if RootNode is selected (prevents double transformation)
if (useRootNodeMatrix) {
    modelManager.renderAll(shader, ui_data, Transforms, selectedModelIndex, &rootNodeModelMatrix);
} else {
    modelManager.renderAll(shader, ui_data, Transforms);
}
```

## How It Works Now

1. **When RootNode is NOT selected:**
   - Model uses its own `pos/rotation/size` for rendering
   - Standard behavior - no changes

2. **When RootNode IS selected:**
   - PropertyPanel RootNode transforms are read
   - Model matrix is built from PropertyPanel values
   - This matrix is passed directly to `Model::render()` via `ModelManager::renderAll()`
   - Model's `pos/rotation/size` are **completely ignored** - not used, not modified
   - Only PropertyPanel values control the model transform

3. **Gizmo manipulation:**
   - Gizmo reads from PropertyPanel RootNode values
   - Gizmo updates PropertyPanel RootNode values
   - Model matrix is built from PropertyPanel values
   - Model renders using PropertyPanel matrix
   - **No double transformation** - single source of truth (PropertyPanel)

## Benefits

1. **No Double Transformation:** Model transform comes from a single source (PropertyPanel when RootNode selected)
2. **Gizmo Alignment:** Gizmo and model are always aligned because they use the same transform values
3. **Clean Separation:** Model's `pos/rotation/size` are not modified when RootNode is selected
4. **Backward Compatible:** When RootNode is not selected, model uses its own `pos/rotation/size` as before

## Testing

To verify the fix:
1. Load a model
2. Select it (RootNode is automatically selected)
3. Use gizmo to move it (e.g., Z = 7)
4. **Expected:** Gizmo and model should be aligned at the same position
5. **Before fix:** Model would be at double the distance from gizmo

## Additional Fix: Reset model.pos/rotation/size to Identity

**Problem:** Even after passing custom matrix, model was still being transformed twice because `model.pos/rotation/size` still had old values that could interfere.

**Solution:** When RootNode is selected, reset `model.pos/rotation/size` to identity values:
1. **On first selection:** After initializing PropertyPanel from model's current transform, reset `model.pos/rotation/size` to identity
2. **Every frame:** When RootNode is selected, reset `model.pos/rotation/size` to identity before rendering

This ensures that `model.pos/rotation/size` are always identity when RootNode is selected, so they can't cause double transformation.

**Code added in `main.cpp`:**
```cpp
// After initializing PropertyPanel from model
selectedInstance->model.pos = glm::vec3(0.0f, 0.0f, 0.0f);
selectedInstance->model.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Identity quaternion
selectedInstance->model.size = glm::vec3(1.0f, 1.0f, 1.0f);

// Every frame when RootNode is selected (before rendering)
selectedModel->model.pos = glm::vec3(0.0f, 0.0f, 0.0f);
selectedModel->model.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
selectedModel->model.size = glm::vec3(1.0f, 1.0f, 1.0f);
```

## Files Modified

- `src/graphics/model.h` - Added optional customModelMatrix parameter to render()
- `src/graphics/model.cpp` - Implemented custom model matrix support
- `src/graphics/model_manager.h` - Added optional parameters to renderAll()
- `src/graphics/model_manager.cpp` - Implemented custom model matrix passing
- `src/main.cpp` - Build and pass RootNode matrix, reset model.pos/rotation/size to identity when RootNode selected
- `src/io/ui/property_panel.cpp` - Added comment about resetting model transforms
