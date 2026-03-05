# Global Skeleton Toggle Implementation

## Date
Created: 2026-02-23

## Overview
Implemented a global "Show Skeletons" toggle in the Tools menu using the exact same one-way Macro/Micro control system as the Bounding Boxes toggle. The global toggle acts as a strict broadcaster that updates all models' skeleton visibility when clicked, while individual model toggles in the Properties panel remain independent.

## Problem
Previously, skeleton visibility was only controllable per-model via the Properties panel. There was no global master switch to quickly show/hide skeletons for all models simultaneously, making it cumbersome when working with multiple models.

## Solution
Added a global "Show Skeletons" toggle in the Tools menu that:
1. Uses stored global state (not calculated from individual models)
2. Acts as a strict one-way broadcaster (only updates when clicked)
3. Updates all models' `m_showSkeleton` flags simultaneously
4. Persists state to settings file
5. Initializes new models with the global state

## Implementation

### 1. ModelManager State Storage
**Location**: `src/graphics/model_manager.h` (lines 192-195, 203)

Added skeleton state storage similar to bounding boxes:
```cpp
// Enable/disable skeletons for all models
void setSkeletonsEnabled(bool enabled);

// Get the current skeleton enabled state (for new models)
bool getSkeletonsEnabled() const { return mSkeletonsEnabled; }
```

**Private member**:
```cpp
// Current skeleton enabled state (for new models)
bool mSkeletonsEnabled = false;
```

**Default**: `false` (skeletons hidden by default, matching `m_showSkeleton = false`)

### 2. ModelManager Implementation
**Location**: `src/graphics/model_manager.cpp` (lines 51, 302-310)

**Model Initialization** (line 51):
```cpp
// Set skeleton enabled state to match current setting (respects saved preference)
instance->model.m_showSkeleton = mSkeletonsEnabled;
```

**setSkeletonsEnabled Method** (lines 302-310):
```cpp
void ModelManager::setSkeletonsEnabled(bool enabled) {
    mSkeletonsEnabled = enabled;  // Store the current state for new models
    for (auto& instance : models) {
        if (instance) {
            // Update the model's per-model skeleton visibility flag
            instance->model.m_showSkeleton = enabled;
        }
    }
}
```

### 3. Tools Menu Toggle
**Location**: `src/io/ui/viewport_panel.cpp` (lines 76-95)

Added "Show Skeletons" menu item right after "Bounding Boxes" toggle:
```cpp
// Show Skeletons visibility toggle (master switch - strict one-way broadcaster)
// Use stored global state (NOT calculated from individual models)
// This ensures strict one-way control: global toggle is broadcaster only
bool globalShowSkeletons = modelManager->getSkeletonsEnabled();

// This block triggers ONLY when the user physically clicks the menu item
if (ImGui::MenuItem("Show Skeletons", "", &globalShowSkeletons)) {
    // Strict broadcaster: override all models to match the new master state
    for (size_t i = 0; i < modelManager->getModelCount(); i++) {
        ModelInstance* instance = modelManager->getModel(static_cast<int>(i));
        if (instance) {
            instance->model.m_showSkeleton = globalShowSkeletons;
        }
    }
    
    // Update stored global state
    modelManager->setSkeletonsEnabled(globalShowSkeletons);
    
    // Save to settings
    AppSettings& settings = AppSettingsManager::getInstance().getSettings();
    settings.environment.skeletonsEnabled = globalShowSkeletons;
    AppSettingsManager::getInstance().markDirty();
}
```

**Key Features**:
- Uses `getSkeletonsEnabled()` to get stored state (not calculated from models)
- Only triggers when menu item is clicked (strict broadcaster)
- Updates all models' `m_showSkeleton` flags
- Saves to settings for persistence

### 4. AppSettings Integration
**Location**: `src/io/app_settings.h` (line 47)

Added skeleton state to settings:
```cpp
bool skeletonsEnabled = false;  // Whether skeletons are visible for all models
```

**Location**: `src/io/app_settings.cpp` (lines 99-101, 183)

**Loading**:
```cpp
if (env.contains("skeletonsEnabled")) {
    settings.environment.skeletonsEnabled = env["skeletonsEnabled"].get<bool>();
}
```

**Saving**:
```cpp
j["environment"]["skeletonsEnabled"] = settings.environment.skeletonsEnabled;
```

### 5. Application Initialization
**Location**: `src/application.cpp` (line 270)

Load skeleton state from settings on startup:
```cpp
// Load skeletons enabled state from settings
m_modelManager.setSkeletonsEnabled(appSettings.environment.skeletonsEnabled);
```

## Control Flow

### Global Toggle (Tools Menu)
1. User clicks "Show Skeletons" menu item
2. Menu item uses stored global state (`getSkeletonsEnabled()`)
3. When clicked, broadcasts new state to ALL models
4. Updates stored global state
5. Saves to settings

### Individual Toggle (Property Panel)
1. User clicks "Show Skeleton" checkbox for selected model
2. Only updates that model's `m_showSkeleton` flag
3. Does NOT update global state
4. Does NOT affect other models
5. Global toggle checkbox remains unchanged

### Rendering
1. For each model, check `model.m_showSkeleton`
2. If true, render skeleton (already implemented)
3. No global condition blocks rendering
4. Each model renders independently based on its own flag

## Benefits

1. **Consistent Pattern**: Uses exact same one-way Macro/Micro control as Bounding Boxes
2. **Quick Global Control**: Show/hide all skeletons with one click
3. **Independent Per-Model Control**: Individual toggles still work independently
4. **Settings Persistence**: Global state saved and restored between sessions
5. **New Model Initialization**: New models respect saved global preference

## Testing

### Test Case 1: Global Toggle
1. Load multiple models
2. Click "Show Skeletons" in Tools menu
3. **Expected**: All models' skeletons become visible
4. Click again to turn off
5. **Expected**: All models' skeletons become hidden

### Test Case 2: Individual Toggle Independence
1. Turn off global "Show Skeletons" toggle
2. Select a model
3. Check its "Show Skeleton" checkbox in Properties panel
4. **Expected**: 
   - Skeleton renders for that model
   - Global toggle remains unchecked
   - Other models remain unchanged

### Test Case 3: Global Override
1. Turn on skeletons for Model A and Model B individually
2. Turn off global "Show Skeletons" toggle
3. **Expected**:
   - All models' skeletons turn off
   - Individual checkboxes update to reflect new state

### Test Case 4: Settings Persistence
1. Turn on global "Show Skeletons" toggle
2. Close and restart application
3. **Expected**: Global toggle state is restored from settings

## Related Files
- `src/graphics/model_manager.h` - State storage and getter/setter methods
- `src/graphics/model_manager.cpp` - Implementation and model initialization
- `src/io/ui/viewport_panel.cpp` - Tools menu toggle
- `src/io/app_settings.h` - Settings structure
- `src/io/app_settings.cpp` - Settings loading/saving
- `src/application.cpp` - Application initialization

## Notes
- The global state (`mSkeletonsEnabled`) is stored in ModelManager and used for:
  - Initializing new models
  - Tools menu checkbox state
  - Settings persistence
- Individual model flags (`m_showSkeleton`) control actual rendering
- The two states are synchronized only when the global toggle is clicked (one-way broadcast)
- Default state is `false` (skeletons hidden), matching the default `m_showSkeleton = false`
