# ImGuizmo Maya-Style Keyboard Shortcuts

## Overview
This document describes the implementation of Maya-style keyboard shortcuts for the ImGuizmo gizmo, matching the standard workflow in Autodesk Maya.

## Date
Implementation Date: [Current Session]

## Features Implemented

### 1. Operation Mode Shortcuts (Maya-style)
- **W** - Switch to **Translate** (Move) mode
- **E** - Switch to **Rotate** mode
- **R** - Switch to **Scale** mode

### 2. Gizmo Size Adjustment
- **+** (Plus key or Numpad +) - Increase gizmo size
- **-** (Minus key or Numpad -) - Decrease gizmo size

## Implementation Details

### 1. Scene Class Updates (`src/io/scene.h`)

**Added gizmo state variables:**
```cpp
//-- gizmo operation and size state (Maya-style controls) -------------------
ImGuizmo::OPERATION mGizmoOperation = ImGuizmo::TRANSLATE;  // Current gizmo operation (W/E/R)
float mGizmoSize = 0.1f;  // Gizmo size in clip space (adjustable with +/-)
```

**Added method declaration:**
```cpp
//-- handle gizmo keyboard shortcuts (Maya-style: W/E/R for operation, +/- for size) -------------------
void handleGizmoKeyboardShortcuts();
```

### 2. Keyboard Shortcut Handler (`src/io/scene.cpp`)

**Implementation:**
- Checks if a model is selected before processing shortcuts
- Checks if viewport is focused to avoid conflicts with text input in other windows
- Processes W/E/R keys to switch operation modes
- Processes +/- keys to adjust gizmo size
- Uses `ImGuizmo::SetGizmoSizeClipSpace()` to set gizmo size

**Key Features:**
- Only processes shortcuts when a model is selected
- Avoids conflicts with ImGui text input in other windows
- Provides console feedback when operation changes
- Clamps gizmo size between 0.01 and 1.0

### 3. Gizmo Rendering Updates

**Updated to use current operation:**
```cpp
// Set gizmo size (adjustable with +/- keys)
ImGuizmo::SetGizmoSizeClipSpace(mGizmoSize);

// Use current gizmo operation (set with W/E/R keys)
ImGuizmo::OPERATION operation = mGizmoOperation;
```

## Usage

1. **Select a model** by clicking on it in the viewport
2. **Press W** to switch to Translate mode (default)
3. **Press E** to switch to Rotate mode
4. **Press R** to switch to Scale mode
5. **Press +** to increase gizmo size
6. **Press -** to decrease gizmo size

## Technical Notes

### Gizmo Size
- Size is stored in clip space (0.01 to 1.0 range)
- Default size is 0.1
- Size changes are applied immediately using `ImGuizmo::SetGizmoSizeClipSpace()`

### Operation Modes
- **TRANSLATE**: Move the model along X/Y/Z axes
- **ROTATE**: Rotate the model around X/Y/Z axes
- **SCALE**: Scale the model along X/Y/Z axes

### Rotation Support
- Rotation is extracted from the gizmo matrix but not currently stored
- The Model class only has `pos` and `size` members
- If rotation support is needed, it would need to be added to the Model class

## Files Modified

1. `src/io/scene.h` - Added gizmo operation and size state variables, method declaration
2. `src/io/scene.cpp` - Added keyboard shortcut handler, updated gizmo rendering

## Testing

After rebuilding:
1. ✅ Select a model
2. ✅ Press W - gizmo should show translate arrows
3. ✅ Press E - gizmo should show rotation rings
4. ✅ Press R - gizmo should show scale handles
5. ✅ Press + - gizmo should get larger
6. ✅ Press - - gizmo should get smaller
7. ✅ Console should show operation changes

## Future Enhancements

- Add rotation storage to Model class if rotation support is needed
- Add visual indicator showing current operation mode
- Add UI controls (buttons) for operation switching as alternative to keyboard
- Persist gizmo size in settings
