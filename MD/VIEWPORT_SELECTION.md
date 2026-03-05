# Viewport Selection System

## Overview
This document describes the implementation of the viewport selection system, which allows users to select 3D models by clicking on them directly in the viewport window. When a model is selected, it is highlighted in the outliner and its bounding box changes color to indicate selection.

## Date
Implementation Date: 2024

## Features

1. **Mouse Click Selection**: Click on a model in the viewport to select it
2. **Visual Feedback**: Selected models have cyan bounding boxes (unselected models have yellow bounding boxes)
3. **Outliner Integration**: When a model is selected, the root node in the outliner hierarchy is automatically selected and highlighted
4. **Raycast-Based**: Uses efficient ray-box intersection tests to determine which model was clicked
5. **Camera Control Compatibility**: Selection only works when NOT using Alt+drag for camera control
6. **Works with Bounding Boxes Disabled**: Selection works even when bounding boxes are hidden - bounding boxes are calculated on-demand for selection and then discarded to save resources

## Technical Implementation

### 1. Raycast System (`src/graphics/raycast.h` and `src/graphics/raycast.cpp`)

The raycast system provides utilities for converting mouse screen coordinates to world-space rays and performing intersection tests.

#### Key Functions:

- **`screenToWorldRay()`**: Converts mouse screen coordinates (in pixels) to a world-space ray
  - Takes mouse X/Y, viewport dimensions, view matrix, and projection matrix
  - Returns a `Ray` structure with origin and direction
  - Uses inverse projection and view matrices to transform from screen space to world space

- **`rayIntersectsAABB()`**: Tests if a ray intersects an axis-aligned bounding box
  - Uses the Kay-Kajiya slab method for efficient AABB intersection
  - Returns true if intersection found, and outputs the intersection distance
  - Handles edge cases like rays parallel to axis planes

### 2. Selection State Management (`src/graphics/model_manager.h` and `src/graphics/model_manager.cpp`)

The `ModelManager` class tracks which model is currently selected:

- **`mSelectedModelIndex`**: Index of the currently selected model (-1 = none selected)
- **`setSelectedModel(int index)`**: Sets the selected model by index
- **`getSelectedModelIndex()`**: Returns the index of the selected model
- **`getSelectedModel()`**: Returns a pointer to the selected `ModelInstance`

### 3. Mouse Click Handling (`src/main.cpp`)

Mouse clicks are processed in the main render loop:

1. **Detection**: Checks if mouse is over viewport image (not just window) and Alt is NOT pressed (to avoid conflicts with camera control)
2. **Coordinate Conversion**: Converts mouse screen coordinates to viewport-relative coordinates using ImGui's mouse position
3. **Ray Generation**: Creates a world-space ray from the mouse position using current camera view/projection matrices
4. **Bounding Box Calculation**: Calculates bounding boxes on-demand for selection (even if disabled for rendering)
   - Uses cached bone transforms if available (from rendering pass when bounding boxes are enabled)
   - Calculates bone transforms temporarily if bounding boxes are disabled (more expensive but necessary for accurate selection)
   - Bounding box calculation is temporary and discarded after selection check
5. **Intersection Testing**: Tests the ray against all model bounding boxes
6. **Selection**: Selects the closest intersected model (if any), or deselects if clicking empty space

#### Code Flow:
```cpp
if (isOverViewport && !altPressed && file_is_open) {
    if (Mouse::buttonWentDown(GLFW_MOUSE_BUTTON_LEFT)) {
        // Convert mouse to viewport coordinates
        // Generate ray from mouse position
        // Test ray against all bounding boxes
        // Select closest model or deselect
    }
}
```

### 4. Visual Feedback

#### Bounding Box Color:
- **Selected**: Cyan (0.0, 1.0, 1.0)
- **Unselected**: Yellow (1.0, 1.0, 0.0)

The bounding box color is updated every frame in the rendering loop based on the selection state.

#### Outliner Selection:
When a model is selected via viewport click:
- The outliner's `setSelectionToRoot()` method is called
- This selects the root node of the FBX scene hierarchy
- The root node is highlighted in the outliner tree view

### 5. Outliner Integration (`src/io/ui/outliner.h` and `src/io/ui/outliner.cpp`)

New methods added to the `Outliner` class:

- **`setSelectionToRoot()`**: Selects the root node of the current FBX scene
  - Sets `g_selected_object` to the root node
  - Updates `mSelectedNod_name` with the root node's name
  - Updates selection index for keyboard navigation compatibility

- **`clearSelection()`**: Clears the current selection
  - Sets `g_selected_object` to nullptr
  - Clears `mSelectedNod_name`
  - Resets selection index

## Files Modified

### New Files:
1. **`src/graphics/raycast.h`**: Header file for raycast utilities
2. **`src/graphics/raycast.cpp`**: Implementation of raycast functions
3. **`MD/VIEWPORT_SELECTION.md`**: This documentation file

### Modified Files:
1. **`src/graphics/model_manager.h`**:
   - Added `mSelectedModelIndex` member variable
   - Added `setSelectedModel()`, `getSelectedModelIndex()`, and `getSelectedModel()` methods

2. **`src/graphics/model_manager.cpp`**:
   - Implemented selection management methods

3. **`src/main.cpp`**:
   - Added `#include "graphics/raycast.h"`
   - Added `#include <limits>`
   - Added mouse click handling for selection
   - Updated bounding box rendering to use different colors for selected/unselected models
   - Integrated with outliner to update selection when model is clicked

4. **`src/io/scene.h`**:
   - Added `getViewportWindowPos()` and `getViewportWindowSize()` methods

5. **`src/io/scene.cpp`**:
   - Implemented viewport window position and size getters

6. **`src/io/ui/outliner.h`**:
   - Added `setSelectionToRoot()` and `clearSelection()` method declarations

7. **`src/io/ui/outliner.cpp`**:
   - Implemented `setSelectionToRoot()` and `clearSelection()` methods

## Usage

1. **Select a Model**: Click on any model in the viewport (when not holding Alt)
2. **Visual Confirmation**: The model's bounding box turns cyan, and the root node is highlighted in the outliner
3. **Deselect**: Click on empty space in the viewport
4. **Camera Control**: Hold Alt while clicking/dragging to control the camera (selection is disabled during camera control)

## Performance Considerations

- **Raycast Efficiency**: The ray-box intersection test uses the efficient Kay-Kajiya slab method, which is O(1) per bounding box
- **Bounding Box Updates**: Bounding box colors are updated every frame, but this is a simple uniform color change (no geometry regeneration)
- **Selection Testing**: All models are tested for intersection, but the test is very fast (just a few floating-point comparisons per model)
- **On-Demand Bounding Box Calculation**: When bounding boxes are disabled, they are calculated temporarily only during mouse clicks for selection purposes, then discarded. This ensures selection works without the overhead of continuous bounding box updates.
- **Bone Transform Caching**: When bounding boxes are enabled, bone transforms are cached during rendering and reused for selection (most efficient). When disabled, bone transforms are calculated temporarily during selection (more expensive but only happens on click).

## Future Enhancements

Potential improvements to the selection system:

1. **Multi-Selection**: Allow selecting multiple models with Ctrl+Click
2. **Selection Box**: Drag to create a selection box that selects all models within it
3. **Selection Persistence**: Save/restore selection state
4. **Selection Filters**: Filter which objects can be selected (e.g., only meshes, only bones)
5. **Visual Highlighting**: Add additional visual feedback beyond bounding box color (e.g., outline shader)
