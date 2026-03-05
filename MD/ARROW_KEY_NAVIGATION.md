# Arrow Key Navigation Implementation

## Overview
This document describes the implementation of arrow key navigation in the Outliner window and Viewport, allowing users to navigate through the scene hierarchy using Up/Down arrow keys from either location.

## Date: 2026-02-09

## Problem Statement
Users wanted to be able to navigate through the Outliner's hierarchical list of bones/nodes using keyboard arrow keys (Up/Down), similar to standard file explorers and tree views. Additionally, users requested the ability to navigate directly from the Viewport when hovering over it, without needing to switch focus to the Outliner window.

## Implementation Details

### 1. Outliner Class Changes

**File:** `src/io/ui/outliner.h`

**Added Members:**
- `std::vector<const ofbx::Object*> mSelectableObjects` - Flat list of all selectable objects in tree order
- `int mCurrentSelectionIndex = -1` - Current selection index in the list
- `bool mNeedsRebuildList = true` - Flag to rebuild the list when scene changes
- `bool m_viewportHovered = false` - Tracks whether the Viewport is currently hovered (enables keyboard navigation from viewport)

**Added Methods:**
- `void rebuildSelectableObjectsList(const ofbx::IScene& scene)` - Builds a flat list of all selectable objects
- `void collectSelectableObjects(const ofbx::Object& object)` - Recursively collects objects from the tree
- `void handleKeyboardNavigation()` - Handles arrow key input and updates selection
- `void setViewportHovered(bool hovered)` - Sets the viewport hover state (called by ViewportPanel)

### 2. Implementation Logic

**Rebuilding Selectable Objects List:**
- Called when a new FBX file is loaded (`mNeedsRebuildList = true`)
- Traverses the entire scene hierarchy (root node + animation stacks)
- Collects only selectable objects (LIMB_NODE, MESH, GEOMETRY, ROOT, NULL_NODE, ANIMATION_STACK)
- Filters out non-selectable items like animation curve nodes
- Maintains the visual order as they appear in the tree

**Keyboard Navigation:**
- Uses the existing `Keyboard` class (already set up with GLFW callbacks)
- Checks `Keyboard::keyWentDown(GLFW_KEY_UP)` and `Keyboard::keyWentDown(GLFW_KEY_DOWN)`
- Processes input when either:
  - The Outliner window is hovered (`ImGui::IsWindowHovered()`), OR
  - The Viewport is hovered (`m_viewportHovered`)
- Updates `mCurrentSelectionIndex` based on key presses
- Wraps around at the beginning/end of the list
- Triggers selection change callback immediately to update Gizmo and Auto-Focus

**Selection Update:**
- When arrow keys are pressed, updates `g_selected_object` and `mSelectedNod_name`
- Also updates the selection index when clicking on items manually
- Ensures the keyboard navigation index stays in sync with mouse clicks

### 3. Files Modified

1. **`src/io/ui/outliner.h`**
   - Added `#include <vector>` for `std::vector`
   - Added navigation data members
   - Added helper method declarations

2. **`src/io/ui/outliner.cpp`**
   - Added `#include "../keyboard.h"` and `#include <GLFW/glfw3.h>`
   - Modified `outlinerPanel()` to rebuild list and handle keyboard navigation
   - Modified `showObjectGUI()` to update selection index on click
   - Modified `loadFBXfile()` to set `mNeedsRebuildList = true`
   - Modified `clear()` to reset navigation data
   - Modified `handleKeyboardNavigation()` to check for viewport hover state
   - Modified `handleKeyboardNavigation()` to trigger selection change callback for immediate Gizmo/Auto-Focus updates
   - Implemented `rebuildSelectableObjectsList()`, `collectSelectableObjects()`, and `handleKeyboardNavigation()`

3. **`src/io/ui/viewport_panel.cpp`**
   - Added call to `Outliner::setViewportHovered()` after checking `ImGui::IsItemHovered()` for the viewport image
   - Passes viewport hover state to Outliner so arrow keys work when hovering over the viewport

## How It Works

1. **On File Load:**
   - `loadFBXfile()` sets `mNeedsRebuildList = true`
   - Next frame, `outlinerPanel()` calls `rebuildSelectableObjectsList()`
   - Creates a flat list of all selectable objects in tree order
   - Finds current selection index if an object is already selected

2. **On Arrow Key Press:**
   - User hovers mouse over Outliner window OR Viewport
   - Presses Up or Down arrow key
   - `handleKeyboardNavigation()` detects the key press using `Keyboard::keyWentDown()`
   - Checks if either Outliner window is hovered OR Viewport is hovered (`m_viewportHovered`)
   - Updates `mCurrentSelectionIndex` (wraps at boundaries)
   - Updates `g_selected_object` and `mSelectedNod_name` to match the new index
   - Triggers selection change callback to immediately update Gizmo and Auto-Focus
   - PropertyPanel automatically updates to show the new selection

3. **On Manual Click:**
   - User clicks on an item in the Outliner
   - `showObjectGUI()` updates the selection
   - Also updates `mCurrentSelectionIndex` to keep it in sync

## Key Features

- **Up Arrow** - Move selection up in the list
- **Down Arrow** - Move selection down in the list
- **Wraps Around** - Pressing Up at the top goes to the bottom, Down at the bottom goes to the top
- **Works with Mouse** - Clicking items still works and updates keyboard navigation index
- **Works from Outliner** - Navigation works when mouse is over the Outliner window
- **Works from Viewport** - Navigation also works when mouse is over the Viewport (NEW)
- **Instant Gizmo Update** - Gizmo and Auto-Focus update immediately when selection changes via keyboard

## Technical Notes

- Uses the existing `Keyboard` class which is already integrated with GLFW
- The selectable objects list is rebuilt only when needed (on file load or clear)
- Selection index is automatically initialized to 0 if no selection exists
- The list includes only objects that are meaningful to select (bones, meshes, etc.)
- Animation curve nodes and other non-selectable items are filtered out

## Testing Checklist

- [ ] Load a model with bones
- [ ] Hover mouse over Outliner window
- [ ] Press Down Arrow - selection should move down
- [ ] Press Up Arrow - selection should move up
- [ ] Press Up at the top - should wrap to bottom
- [ ] Press Down at the bottom - should wrap to top
- [ ] Click on an item - keyboard navigation index should update
- [ ] Load a new file - list should rebuild automatically
- [ ] Hover mouse over Viewport (not Outliner)
- [ ] Press Down Arrow - selection should move down (works from viewport)
- [ ] Press Up Arrow - selection should move up (works from viewport)
- [ ] Verify Gizmo updates immediately when selection changes via keyboard
- [ ] Verify Auto-Focus updates immediately when selection changes via keyboard

## Known Limitations

- Navigation works when either the Outliner window OR the Viewport is hovered
- The list is rebuilt on file load, which may cause a brief delay for large scenes
- Only selectable objects (bones, meshes, etc.) are included in navigation

## Future Improvements

- Could add Left/Right arrow keys to expand/collapse tree nodes
- Could add Home/End keys to jump to first/last item
- Could add Page Up/Page Down for faster navigation
- Could add visual highlight/scroll to keep selected item visible
