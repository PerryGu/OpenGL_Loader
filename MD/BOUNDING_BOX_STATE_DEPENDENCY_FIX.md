# Bounding Box State Dependency Bug Fix

## Date
Created: 2026-02-23

## Problem
The bounding box toggle logic had a state-dependency bug:
- When the global "Bounding Boxes" menu item was turned off
- Checking an individual model's "Bounding Box" toggle would visually re-check the global toggle
- But the bounding box itself would fail to render

**Root Cause**: The Tools menu was calculating its state from individual model states (reverse synchronization), causing the global toggle to reflect individual model changes instead of being a strict one-way broadcaster.

## Solution
Implemented strict one-way Macro/Micro control:
- **Global toggle is strictly a broadcaster**: Only updates individual models when clicked
- **Individual toggles are independent**: Do NOT affect the global toggle's state
- **Rendering depends solely on model flag**: No outer global condition blocks rendering

## Implementation

### 1. Fixed Tools Menu - Strict One-Way Broadcaster
**Location**: `src/io/ui/viewport_panel.cpp` (lines 51-80)

**Before** (Reverse Synchronization Bug):
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
```

**After** (Strict Broadcaster):
```cpp
// Use stored global state (NOT calculated from individual models)
// This ensures strict one-way control: global toggle is broadcaster only
bool globalDrawBoundingBoxes = modelManager->getBoundingBoxesEnabled();

// This block triggers ONLY when the user physically clicks the menu item
if (ImGui::MenuItem("Bounding Boxes", "", &globalDrawBoundingBoxes)) {
    // Strict broadcaster: override all models to match the new master state
    for (size_t i = 0; i < modelManager->getModelCount(); i++) {
        ModelInstance* instance = modelManager->getModel(static_cast<int>(i));
        if (instance) {
            instance->model.m_showBoundingBox = globalDrawBoundingBoxes;
        }
    }
    
    // Update stored global state and legacy boundingBox.setEnabled() for compatibility
    modelManager->setBoundingBoxesEnabled(globalDrawBoundingBoxes);
    
    // Save to settings
    AppSettings& settings = AppSettingsManager::getInstance().getSettings();
    settings.environment.boundingBoxesEnabled = globalDrawBoundingBoxes;
    AppSettingsManager::getInstance().markDirty();
}
```

**Key Changes**:
1. Uses `getBoundingBoxesEnabled()` to get stored state instead of calculating from models
2. Removed reverse synchronization logic (no longer checks individual model states)
3. Only updates when menu item is clicked (strict broadcaster pattern)

### 2. Render Logic Verification
**Location**: `src/application.cpp` (line 899)

The render loop already correctly depends solely on the model's flag:
```cpp
// Only render bounding box if enabled (visibility toggle)
// Calculation always happens, but rendering is conditional
// Check individual model's bounding box visibility flag
if (instance->model.m_showBoundingBox) {
    // ... render bounding box ...
}
```

**No outer global condition** - rendering is not blocked by any global boolean.

### 3. Property Panel - Independent Control
**Location**: `src/io/ui/property_panel.cpp` (lines 207-210)

Individual model checkbox remains independent:
```cpp
// Bounding Box checkbox
if (ImGui::Checkbox("Bounding Box", &model->m_showBoundingBox)) {
    LOG_INFO("PropertyPanel: Bounding Box toggled to " + std::string(model->m_showBoundingBox ? "ON" : "OFF"));
}
```

**Behavior**: Only updates the selected model's flag, does NOT affect global state.

## Control Flow

### Global Toggle (Tools Menu)
1. User clicks "Bounding Boxes" menu item
2. Menu item uses stored global state (`getBoundingBoxesEnabled()`)
3. When clicked, broadcasts new state to ALL models
4. Updates stored global state
5. Saves to settings

### Individual Toggle (Property Panel)
1. User clicks "Bounding Box" checkbox for selected model
2. Only updates that model's `m_showBoundingBox` flag
3. Does NOT update global state
4. Does NOT affect other models
5. Global toggle checkbox remains unchanged

### Rendering
1. For each model, check `model.m_showBoundingBox`
2. If true, render bounding box
3. No global condition blocks rendering
4. Each model renders independently based on its own flag

## Benefits

1. **Strict One-Way Control**: Global toggle is broadcaster only, never affected by individual changes
2. **Independent Per-Model Control**: Individual toggles work independently
3. **Correct Rendering**: Bounding boxes render based solely on model flag, not global state
4. **Predictable Behavior**: Global toggle state is stored, not calculated
5. **No State Conflicts**: Individual and global controls don't interfere with each other

## Testing

### Test Case 1: Global Toggle Off, Individual Toggle On
1. Turn off global "Bounding Boxes" toggle
2. Select a model
3. Check its "Bounding Box" checkbox in Properties panel
4. **Expected**: 
   - Bounding box renders for that model
   - Global toggle remains unchecked
   - Other models remain unchanged

### Test Case 2: Individual Toggle On, Global Toggle Off
1. Turn on a model's "Bounding Box" checkbox
2. Turn off global "Bounding Boxes" toggle
3. **Expected**:
   - All models' bounding boxes turn off
   - Individual checkboxes update to reflect new state

### Test Case 3: Multiple Individual Toggles
1. Turn on bounding boxes for Model A and Model B
2. Turn off bounding box for Model C
3. **Expected**:
   - Model A and B show bounding boxes
   - Model C does not show bounding box
   - Global toggle state remains unchanged (unless explicitly clicked)

## Related Files
- `src/io/ui/viewport_panel.cpp` - Tools menu (fixed broadcaster logic)
- `src/application.cpp` - Render loop (already correct)
- `src/io/ui/property_panel.cpp` - Individual model checkbox (already correct)
- `src/graphics/model_manager.h` - Global state storage (`mBoundingBoxesEnabled`)

## Notes
- The global state (`mBoundingBoxesEnabled`) is stored in ModelManager and used for:
  - Initializing new models
  - Tools menu checkbox state
  - Settings persistence
- Individual model flags (`m_showBoundingBox`) control actual rendering
- The two states are synchronized only when the global toggle is clicked (one-way broadcast)
