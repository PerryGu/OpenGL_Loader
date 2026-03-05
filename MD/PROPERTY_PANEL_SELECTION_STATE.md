# PropertyPanel Selection State Refactoring

## Date
Created: 2026-02-16

## Problem Statement

When clicking on empty space in the viewport, the node name disappears from the Properties window, but the transform sliders (Translation, Rotation, Scale) remain visible. This causes "ghost" values from previously selected nodes to be displayed, which is confusing and can lead to accidental modifications.

## Solution

Refactored the `propertyPanel()` function to hide the entire "Transforms" UI block when nothing is selected. The UI now strictly reflects the current selection context and does not show values from previously selected nodes.

## Changes Made

### 1. Added Selection State Parameter

**File:** `src/io/ui/property_panel.h`

**Change:**
```cpp
void propertyPanel(bool* p_open, int selectedModelIndex = -1);
```

**Why:** Allows PropertyPanel to check both the node selection (from Outliner) and model selection (from ModelManager) to determine if there's a valid selection.

### 2. Selection State Checking

**File:** `src/io/ui/property_panel.cpp`

**Location:** `PropertyPanel::propertyPanel()` - Beginning of Transforms section

**Change:**
```cpp
// CRITICAL: Check selection state - hide entire Transforms block if nothing is selected
// This prevents showing "ghost" values from previously selected nodes
// Selection is invalid if:
//   1. mSelectedNod_name is empty (no node selected in outliner)
//   2. selectedModelIndex is -1 (no model selected in ModelManager)
bool hasValidSelection = !mSelectedNod_name.empty() && selectedModelIndex >= 0;

if (!hasValidSelection) {
    // No valid selection - show "No Selection" message and hide all transform controls
    ImGui::Text("No Selection");
} else {
    // Valid selection exists - render transform controls
    // ... (all existing transform slider code) ...
}
```

**Why:** 
- Checks both conditions: node name must not be empty AND model index must be valid
- Hides all transform sliders when there's no valid selection
- Shows "No Selection" message instead of ghost values
- Prevents accidental modifications to non-existent selections

### 3. Updated Function Call

**File:** `src/io/scene.cpp`

**Location:** `Scene::uiElements()` - PropertyPanel call

**Change:**
```cpp
// CRITICAL: Pass selected model index to PropertyPanel to enable selection state checking
// This ensures transform sliders are hidden when nothing is selected (no "ghost" values)
int selectedModelIndex = (mModelManager != nullptr) ? mModelManager->getSelectedModelIndex() : -1;
mPropertyPanel.propertyPanel(p_open, selectedModelIndex);
```

**Why:** Passes the current selected model index from ModelManager to PropertyPanel so it can check the selection state.

## Behavior

### Before
- Node name disappears when clicking empty space
- Transform sliders remain visible with previous values
- "Ghost" values from previously selected nodes are displayed
- Users can accidentally modify non-existent selections

### After
- Node name disappears when clicking empty space
- Transform sliders are completely hidden
- "No Selection" message is displayed instead
- No ghost values are shown
- UI strictly reflects current selection context

## Selection State Logic

A valid selection requires **both** conditions to be true:
1. **Node Selection**: `mSelectedNod_name` must not be empty (node selected in Outliner)
2. **Model Selection**: `selectedModelIndex` must be >= 0 (model selected in ModelManager)

If either condition is false, the entire Transforms section is hidden and "No Selection" is displayed.

## Immediate Updates

The UI updates immediately when deselection occurs because:
- `setSelectedNode("")` is called when clicking empty space (clears `mSelectedNod_name`)
- `setSelectedModel(-1)` is called when clicking empty space (sets model index to -1)
- `propertyPanel()` is called every frame with the current selection state
- The selection check happens at the beginning of the Transforms section

## Testing Checklist

When testing the refactoring, verify:
- [ ] Clicking on empty space hides all transform sliders
- [ ] "No Selection" message appears when nothing is selected
- [ ] No ghost values are displayed from previously selected nodes
- [ ] Selecting a node shows the transform sliders correctly
- [ ] Transform sliders update immediately when selection changes
- [ ] Window updates correctly when deselection occurs
- [ ] No errors or crashes when switching between selections

## Files Modified

1. `src/io/ui/property_panel.h`
   - Added `selectedModelIndex` parameter to `propertyPanel()` function

2. `src/io/ui/property_panel.cpp`
   - Added selection state checking at the beginning of Transforms section
   - Wrapped all transform controls in `else` block (only shown when selection is valid)
   - Added "No Selection" message when nothing is selected

3. `src/io/scene.cpp`
   - Updated `propertyPanel()` call to pass selected model index

## Related Documentation

- `MD/SELECTION_ISOLATION_REFACTOR.md` - Selection logic refactoring for model isolation
- `MD/PROJECT_UNDERSTANDING.md` - Project overview and architecture

## Summary

This refactoring ensures that the PropertyPanel UI strictly reflects the current selection context. When nothing is selected, all transform controls are hidden and a "No Selection" message is displayed. This prevents confusion from ghost values and accidental modifications to non-existent selections. The UI updates immediately when selection changes, providing clear feedback to the user about the current selection state.
