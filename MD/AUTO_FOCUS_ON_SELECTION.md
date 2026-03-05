# Auto Focus on Selection Change

## Date
Created: 2026-02-16

## Problem Statement

Currently, Auto Focus only triggers when the Gizmo is released. Users want the camera to automatically frame the character immediately when the selection changes, either by clicking in the Viewport or selecting a node in the Outliner.

## Solution

Enhanced Auto Focus to trigger immediately on selection change:
1. **Viewport Selection**: When a model is selected via raycast, trigger auto-focus if enabled
2. **Outliner Selection**: When a node is clicked in the Outliner, trigger auto-focus if enabled
3. **Framing Logic**: Modified `applyCameraAimConstraint()` to work even when gizmo is not visible yet, calculating target position from selected node's bounding box

## Changes Made

### 1. Viewport Selection Auto-Focus

**File:** `src/main.cpp`

**Location:** Viewport raycast selection logic (lines ~1471-1480)

**Change:**
```cpp
// Select the closest model (if any)
if (closestModelIndex >= 0) {
    modelManager.setSelectedModel(closestModelIndex);
    ModelInstance* selectedInstance = modelManager.getSelectedModel();
    if (selectedInstance) {
        scene.mOutliner.setSelectionToRoot(closestModelIndex);
    }
    
    // AUTO-FOCUS: If auto-focus is enabled, trigger camera framing immediately on selection change
    AppSettings& settings = AppSettingsManager::getInstance().getSettings();
    if (settings.environment.autoFocusEnabled) {
        scene.setFramingTrigger(true);
    }
}
```

**Why:**
- Triggers auto-focus immediately when a model is selected in the viewport
- Only triggers if `autoFocusEnabled` is true
- Ensures camera frames the selected character right away

### 2. Outliner Selection Auto-Focus

**File:** `src/io/ui/outliner.cpp`

**Location:** Node selection in `showObjectGUI()` (lines ~348-365)

**Change:**
```cpp
if (!ImGui::IsItemToggledOpen()) {
    g_selected_object = &object;
    // ... set selected node name ...
    
    // AUTO-FOCUS: Trigger callback if set (used for auto-focus on selection change)
    // The callback will check if auto-focus is enabled and trigger camera framing
    if (mSelectionChangeCallback) {
        mSelectionChangeCallback();
    }
}
```

**Why:**
- Triggers callback when a node is clicked in the Outliner
- Callback is set up in Scene to check auto-focus setting and trigger framing

### 3. Callback Mechanism

**File:** `src/io/ui/outliner.h`

**Location:** Public methods (lines ~129-132)

**Change:**
```cpp
//-- set callback for selection change (used for auto-focus) ----
// This callback is called when a node is selected in the outliner
// The callback should trigger camera framing if auto-focus is enabled
void setSelectionChangeCallback(std::function<void()> callback) { mSelectionChangeCallback = callback; }
```

**Private member:**
```cpp
// Callback function for selection change (used for auto-focus)
std::function<void()> mSelectionChangeCallback;
```

**Why:**
- Allows Outliner to notify Scene when selection changes
- Decouples Outliner from Scene implementation
- Enables auto-focus trigger on selection change

### 4. Callback Setup in Scene

**File:** `src/io/scene.cpp`

**Location:** `setParameters()` method (lines ~94-107)

**Change:**
```cpp
//-- Set up outliner selection change callback for auto-focus ----
// This callback is triggered when a node is clicked in the outliner
// It will check if auto-focus is enabled and trigger camera framing
mOutliner.setSelectionChangeCallback([this]() {
    AppSettings& settings = AppSettingsManager::getInstance().getSettings();
    if (settings.environment.autoFocusEnabled) {
        setFramingTrigger(true);
    }
});
```

**Why:**
- Sets up callback when Scene is initialized
- Checks auto-focus setting before triggering
- Triggers framing if enabled

### 5. Enhanced applyCameraAimConstraint()

**File:** `src/io/scene.cpp`

**Location:** `applyCameraAimConstraint()` method (lines ~2192-2250)

**Changes:**

#### Before:
```cpp
// Only apply constraint if gizmo position is valid (gizmo is visible)
if (!mGizmoPositionValid) {
    m_isFraming = false;  // Gizmo not visible, cancel framing
    return;
}
```

#### After:
```cpp
// CRITICAL: For selection-based framing (auto-focus on selection change),
// we need to calculate target position even if gizmo is not visible yet
// If gizmo is not visible, we'll use the selected node's bounding box center as target
glm::vec3 targetPosition = mGizmoWorldPosition;  // Default to gizmo position if available
bool useGizmoPosition = mGizmoPositionValid;

// If gizmo is not visible, calculate target from selected node's bounding box
if (!mGizmoPositionValid && mModelManager != nullptr) {
    std::string selectedNode = mOutliner.getSelectedNode();
    int selectedModelIndex = mModelManager->getSelectedModelIndex();
    
    if (!selectedNode.empty() && selectedModelIndex >= 0) {
        // Get bounding box for the selected node
        glm::vec3 boundingBoxMin, boundingBoxMax;
        mModelManager->getNodeBoundingBox(selectedModelIndex, selectedNode, boundingBoxMin, boundingBoxMax);
        
        // Calculate center and transform to world space using selected model's RootNode transform
        // ... (transform logic) ...
        targetPosition = worldCenter;
        useGizmoPosition = false;
    }
}
```

**Why:**
- Allows framing even when gizmo is not visible yet
- Calculates target position from selected node's bounding box
- Transforms to world space using selected model's RootNode transform
- Works for both gizmo-based and selection-based framing

### 6. Updated Framing Calls

**File:** `src/io/scene.cpp`

**Location:** `applyCameraAimConstraint()` method (lines ~2343-2357)

**Change:**
```cpp
if (hasBoundingBox) {
    // Use targetPosition (either gizmo position or calculated from bounding box)
    camera->requestFraming(targetPosition, boundingBoxMin, boundingBoxMax, aspectRatio, framingMultiplier);
} else {
    if (!camera->isAnimating()) {
        // Use targetPosition (either gizmo position or calculated from bounding box)
        camera->aimAtTarget(targetPosition);
    }
}
```

**Why:**
- Uses `targetPosition` instead of `mGizmoWorldPosition`
- Works for both gizmo-based and selection-based framing
- Ensures consistent behavior

## Behavior

### Before
- Auto Focus only triggered when gizmo was released
- Selection changes did not trigger auto-focus
- Camera had to wait for gizmo interaction

### After
- Auto Focus triggers immediately on selection change (viewport or outliner)
- Works even when gizmo is not visible yet
- Camera frames the selected character/node right away
- Only triggers if "Auto Focus" option is checked in Tools menu

## Key Principles

### 1. Selection-Based Framing
- Framing can work without gizmo being visible
- Target position is calculated from selected node's bounding box
- Transformed to world space using selected model's RootNode transform

### 2. Callback Mechanism
- Outliner notifies Scene when selection changes
- Decouples Outliner from Scene implementation
- Enables flexible event handling

### 3. Settings-Based Control
- Auto-focus only triggers if `settings.environment.autoFocusEnabled` is true
- User can enable/disable via Tools menu
- Setting persists across sessions

### 4. Model Isolation
- Uses `getSelectedModelIndex()` to get correct model
- Transforms bounding box using selected model's RootNode transform
- No data leaks between models

## Testing Checklist

When testing the enhancement, verify:
- [ ] Selecting a model in viewport triggers auto-focus (if enabled)
- [ ] Selecting a node in outliner triggers auto-focus (if enabled)
- [ ] Auto-focus only triggers when "Auto Focus" is checked in Tools menu
- [ ] Camera frames the correct character/node
- [ ] Works even when gizmo is not visible yet
- [ ] Works for both RootNode and bone selections
- [ ] Works for multiple models (frames correct model)

## Files Modified

1. `src/main.cpp`
   - Added auto-focus trigger after viewport selection

2. `src/io/ui/outliner.cpp`
   - Added callback trigger when node is clicked

3. `src/io/ui/outliner.h`
   - Added callback mechanism for selection changes
   - Added `#include <functional>` for std::function

4. `src/io/scene.cpp`
   - Set up outliner callback in `setParameters()`
   - Enhanced `applyCameraAimConstraint()` to work without gizmo
   - Added auto-focus trigger after outliner selection processing

## Related Documentation

- `MD/AUTO_FOCUS_PERSISTENCE.md` - Auto Focus setting persistence
- `MD/CAMERA_FRAMING_MODEL_ISOLATION.md` - Camera framing model isolation
- `MD/MODEL_INDEX_UPDATE_FOR_BONES.md` - Model index update for bone selection

## Summary

This enhancement allows Auto Focus to trigger immediately when selection changes, either by clicking in the Viewport or selecting a node in the Outliner. The camera automatically frames the selected character/node if "Auto Focus" is enabled in the Tools menu. The framing logic has been enhanced to work even when the gizmo is not visible yet, calculating the target position from the selected node's bounding box and transforming it to world space using the selected model's RootNode transform.
