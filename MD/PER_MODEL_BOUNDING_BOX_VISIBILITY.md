# Per-Model Bounding Box Visibility

## Date
Created: 2026-02-23

## Overview
Added per-model granularity to bounding box visibility controls. Each model now has its own "Bounding Box" checkbox in the Properties panel, while the global "Bounding Boxes" toggle in the Tools menu acts as a master switch that updates all models simultaneously.

## Problem
Previously, bounding box visibility was controlled globally for all models. Users had no way to show/hide bounding boxes for individual models, which limited flexibility when working with multiple models in the scene.

## Solution
Implemented per-model bounding box visibility with a master switch:

1. **Per-Model Control**: Each model has its own `m_showBoundingBox` flag
2. **Property Panel Integration**: Individual checkbox in Properties panel for selected model
3. **Master Switch**: Tools menu toggle updates all models' visibility simultaneously
4. **Render Logic**: Updated to check individual model's flag instead of global state

## Implementation

### 1. Model Flag Addition
**Location**: `src/graphics/model.h` (line 168)

Added per-model bounding box visibility flag:
```cpp
// Show bounding box flag (renders bounding box wireframe)
bool m_showBoundingBox = true;
```

**Default**: `true` (bounding boxes visible by default)

### 2. Property Panel Checkbox
**Location**: `src/io/ui/property_panel.cpp` (lines 205-208)

Added checkbox below "Show Skeleton" checkbox:
```cpp
// Bounding Box checkbox
if (ImGui::Checkbox("Bounding Box", &model->m_showBoundingBox)) {
    LOG_INFO("PropertyPanel: Bounding Box toggled to " + std::string(model->m_showBoundingBox ? "ON" : "OFF"));
}
```

**Behavior**: 
- Only visible when a model is selected
- Controls the selected model's bounding box visibility
- Logs state changes for debugging

### 3. Render Logic Update
**Location**: `src/application.cpp` (line 898)

Changed from checking `boundingBox.isEnabled()` to checking model's flag:
```cpp
// Only render bounding box if enabled (visibility toggle)
// Calculation always happens, but rendering is conditional
// Check individual model's bounding box visibility flag
if (instance->model.m_showBoundingBox) {
    // ... render bounding box ...
}
```

**Why**: This ensures each model's bounding box visibility is controlled by its own flag, not a global state.

### 4. Tools Menu Master Switch
**Location**: `src/io/ui/viewport_panel.cpp` (lines 51-75)

Updated the Tools menu "Bounding Boxes" toggle to act as a master switch:
```cpp
// Get current state (true if at least one model has bounding box enabled)
bool boundingBoxesEnabled = false;
for (size_t i = 0; i < modelManager->getModelCount(); i++) {
    ModelInstance* instance = modelManager->getModel(static_cast<int>(i));
    if (instance && instance->model.m_showBoundingBox) {
        boundingBoxesEnabled = true;
        break;
    }
}

if (ImGui::MenuItem("Bounding Boxes", NULL, &boundingBoxesEnabled)) {
    // Master switch: set all models' bounding box visibility to match the new state
    for (size_t i = 0; i < modelManager->getModelCount(); i++) {
        ModelInstance* instance = modelManager->getModel(static_cast<int>(i));
        if (instance) {
            instance->model.m_showBoundingBox = boundingBoxesEnabled;
        }
    }
    
    // Also update the legacy boundingBox.setEnabled() for compatibility
    modelManager->setBoundingBoxesEnabled(boundingBoxesEnabled);
    
    // Save to settings
    AppSettings& settings = AppSettingsManager::getInstance().getSettings();
    settings.environment.boundingBoxesEnabled = boundingBoxesEnabled;
    AppSettingsManager::getInstance().markDirty();
}
```

**Behavior**:
- Menu checkbox state reflects whether at least one model has bounding boxes enabled
- When toggled, sets ALL models' `m_showBoundingBox` to the new state
- Visually updates all Property Panel checkboxes (ImGui handles this automatically)
- Saves state to settings for persistence

### 5. Model Initialization
**Location**: `src/graphics/model_manager.cpp` (line 49)

When a model is loaded, its bounding box visibility is initialized to match the global setting:
```cpp
// Set bounding box enabled state to match current setting (respects saved preference)
instance->boundingBox.setEnabled(mBoundingBoxesEnabled);
// Also set the model's per-model bounding box visibility flag
instance->model.m_showBoundingBox = mBoundingBoxesEnabled;
```

**Why**: Ensures new models respect the saved preference from settings.

### 6. Legacy Compatibility
**Location**: `src/graphics/model_manager.cpp` (line 280)

Updated `setBoundingBoxesEnabled()` to also update model flags:
```cpp
void ModelManager::setBoundingBoxesEnabled(bool enabled) {
    mBoundingBoxesEnabled = enabled;
    for (auto& instance : models) {
        if (instance) {
            instance->boundingBox.setEnabled(enabled);
            // Also update the model's per-model bounding box visibility flag
            instance->model.m_showBoundingBox = enabled;
        }
    }
}
```

**Why**: Maintains backward compatibility with existing code that calls this method.

## User Experience

### Individual Control
1. Select a model in the Outliner
2. Open Properties panel
3. Find "Bounding Box" checkbox below "Show Skeleton"
4. Toggle to show/hide that model's bounding box

### Master Control
1. Open Tools menu (in Viewport window menu bar)
2. Click "Bounding Boxes" menu item
3. All models' bounding boxes are toggled simultaneously
4. Property Panel checkboxes update automatically to reflect new state

## Benefits

1. **Granular Control**: Show/hide bounding boxes per model
2. **Master Switch**: Quick toggle for all models when needed
3. **Visual Feedback**: Property Panel checkboxes reflect current state
4. **Settings Persistence**: Global state saved to settings file
5. **Backward Compatible**: Legacy code still works via `setBoundingBoxesEnabled()`

## Technical Notes

- Bounding box **calculation** still happens every frame (needed for selection/framing)
- Only bounding box **rendering** is controlled by the visibility flag
- The flag is stored per-model, not in the BoundingBox class
- Tools menu checkbox state is calculated dynamically (checks if any model has it enabled)

## Related Files
- `src/graphics/model.h` - Model class with `m_showBoundingBox` flag
- `src/io/ui/property_panel.cpp` - Individual model checkbox
- `src/io/ui/viewport_panel.cpp` - Tools menu master switch
- `src/application.cpp` - Render logic using model flag
- `src/graphics/model_manager.cpp` - Model initialization and legacy compatibility
