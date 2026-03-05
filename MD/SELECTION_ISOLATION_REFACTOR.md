# Selection Logic Refactoring: Model Isolation

## Date
Created: 2026-02-16

## Problem Statement

When multiple FBX models are loaded, selecting a node can leak data between models. The selection logic was not strictly isolating operations to the selected model, causing:
- Node lookups searching across all models instead of the selected one
- PropertyPanel updates affecting the wrong model
- Transform calculations using data from incorrect models

## Solution

Refactored the selection logic to ensure strict isolation between models by:
1. Immediately setting model index when selection occurs
2. Strictly using `getSelectedModelIndex()` for all operations
3. Qualifying all node lookups by the selected model index
4. Removing global searches across all models

## Changes Made

### 1. Immediate Model Index Update on Outliner Selection

**File:** `src/io/scene.cpp`

**Location:** After `mOutliner.outlinerPanel()` call in `Scene::update()`

**Change:**
```cpp
// CRITICAL: Immediately update model selection when outliner selection changes
// This ensures strict isolation between models - no data leaks
if (mModelManager != nullptr && mOutliner.g_selected_object != nullptr) {
    int modelIndexForSelectedObject = mOutliner.findModelIndexForSelectedObject();
    if (modelIndexForSelectedObject >= 0) {
        int currentModelIndex = mModelManager->getSelectedModelIndex();
        if (currentModelIndex != modelIndexForSelectedObject) {
            mModelManager->setSelectedModel(modelIndexForSelectedObject);
        }
    }
}
```

**Why:** Ensures that whenever a node is selected in the outliner, the model index is immediately updated. This prevents operations from using the wrong model's data.

### 2. Strict Model Index Usage in renderViewportWindow()

**File:** `src/io/scene.cpp`

**Location:** `Scene::renderViewportWindow()` - Gizmo rendering section

**Changes:**
- Removed fallback to first model when no model is selected
- Strictly use `getSelectedModelIndex()` - no fallbacks
- All operations are qualified by the selected model index

**Before:**
```cpp
ModelInstance* selectedModel = mModelManager->getSelectedModel();
// If no model is explicitly selected, use the first loaded model
if (selectedModel == nullptr && mModelManager->getModelCount() > 0) {
    selectedModel = mModelManager->getModel(0);  // ❌ BAD: Fallback causes leaks
}
```

**After:**
```cpp
// CRITICAL: Strictly use getSelectedModelIndex() - no fallback to first model
// This ensures strict isolation between models - no data leaks
int selectedModelIndex = mModelManager->getSelectedModelIndex();

// Only proceed if a valid model is selected (no fallback to first model)
if (selectedModelIndex >= 0 && selectedModelIndex < static_cast<int>(mModelManager->getModelCount())) {
    ModelInstance* selectedModel = mModelManager->getModel(selectedModelIndex);
    // ... operations qualified by selectedModelIndex ...
}
```

**Why:** Prevents data leaks by ensuring we only operate on the explicitly selected model. No fallback means no accidental operations on the wrong model.

### 3. Removed Global Node Searches

**File:** `src/io/scene.cpp`

**Location:** `Scene::requestFraming()` - Bounding box calculation

**Before:**
```cpp
if (!selectedNode.empty()) {
    // Try to find the node in any of the loaded models
    for (size_t i = 0; i < mModelManager->getModelCount(); i++) {  // ❌ BAD: Global search
        int modelIndex = static_cast<int>(i);
        mModelManager->getNodeBoundingBox(modelIndex, selectedNode, boundingBoxMin, boundingBoxMax);
        // ...
        break;
    }
}
```

**After:**
```cpp
if (!selectedNode.empty() && selectedModelIndex >= 0) {
    // CRITICAL: Only search in the SELECTED MODEL - no global search across all models
    // This ensures strict isolation between models - no data leaks
    mModelManager->getNodeBoundingBox(selectedModelIndex, selectedNode, boundingBoxMin, boundingBoxMax);
    // ... operations qualified by selectedModelIndex ...
}
```

**Why:** Eliminates global searches that can find nodes in the wrong model. All lookups are now pinned to the selected model index.

### 4. Model-Index Qualified Node Lookups

**File:** `src/io/scene.cpp`

**Location:** Throughout `renderViewportWindow()` - Bone transform lookups

**Changes:**
- All bone lookups use `selectedModel->model.getBoneGlobalTransforms()` (already qualified by selected model)
- RootNode name lookups use `mOutliner.getRootNodeName(selectedModelIndex)`
- Rig root name lookups use `mOutliner.getRigRootName(selectedModelIndex)`

**Example:**
```cpp
// CRITICAL: Get the bone's world position from global transforms of the SELECTED MODEL ONLY
// This ensures we're looking up bones in the correct model (strict isolation)
const std::map<std::string, BoneGlobalTransform>& globalTransforms = selectedModel->model.getBoneGlobalTransforms();
auto boneIt = globalTransforms.find(selectedNode);
```

**Why:** Ensures all node and bone lookups are scoped to the selected model, preventing cross-model data leaks.

## Key Principles

### 1. Immediate Model Index Update
- `setSelectedModel(index)` is called immediately when selection occurs
- Both outliner and viewport selections update the model index right away
- No delayed or deferred updates

### 2. Strict Index Usage
- Always use `getSelectedModelIndex()` to identify the active model
- Never fall back to first model or use default indices
- Validate index is in valid range before use

### 3. No Global Searches
- All node lookups are qualified by the selected model index
- No searching across all models for node names
- All operations are pinned to the selected model

### 4. Model-Index Qualified Operations
- PropertyPanel updates use model-specific node names
- Transform calculations use the selected model's data
- Bone lookups are scoped to the selected model

## Testing Checklist

When testing the refactoring, verify:
- [ ] Selecting a node in outliner immediately updates the model index
- [ ] Viewport selection sets the model index correctly
- [ ] Gizmo operations only affect the selected model
- [ ] PropertyPanel updates only affect the selected model
- [ ] Transform calculations use data from the selected model only
- [ ] No data leaks between models when switching selections
- [ ] Bounding box calculations use the correct model
- [ ] Framing operations work correctly for each model independently

## Files Modified

1. `src/io/scene.cpp`
   - Added immediate model index update after outliner selection
   - Refactored `renderViewportWindow()` to strictly use selected model index
   - Removed global searches in `requestFraming()`
   - All node lookups qualified by selected model index

## Related Documentation

- `MD/PROJECT_UNDERSTANDING.md` - Project overview and architecture
- `MD/CODE_ORGANIZATION.md` - Code organization principles
- `MD/VIEWPORT_SELECTION.md` - Viewport selection system details

## Summary

This refactoring ensures strict isolation between models by:
1. Immediately updating the model index on selection
2. Strictly using the selected model index for all operations
3. Eliminating global searches across all models
4. Qualifying all node lookups by the selected model index

The result is a robust selection system that prevents data leaks between models and ensures all operations are correctly scoped to the selected model.
