# Gizmo Transform Test Function

## Overview
This document describes the test function that captures and prints gizmo transformation values to the debug window when gizmo manipulation ends (on mouse release).

## Date
Implementation Date: 2024

## Purpose
The test function provides a way to inspect the exact transformation values that the gizmo receives after manipulation. This is useful for debugging transform issues, understanding coordinate space conversions, and verifying gizmo behavior.

## Features

1. **Action-Based Printing**: Prints transform values only when gizmo manipulation ends (mouse button release), not continuously
2. **Complete Transform Data**: Displays translation, rotation (Euler angles in degrees), and scale values
3. **Node Context**: Includes the name of the node being manipulated for clarity
4. **Clean Output**: Formatted output with clear labels and separators

## Implementation Details

### File Structure

**Modified Files:**
- `src/io/scene.h` - Added member variable and function declaration
- `src/io/scene.cpp` - Added function implementation and integration

### Member Variable

**File:** `src/io/scene.h`

```cpp
bool mGizmoWasUsing = false;  // Previous frame's gizmo using state (for detecting manipulation end)
```

**Purpose:** Tracks the previous frame's gizmo state to detect when manipulation transitions from active to inactive (mouse release).

### Function Declaration

**File:** `src/io/scene.h`

```cpp
void printGizmoTransformValues(const float* modelMatrixArray, const std::string& nodeName);
```

**Parameters:**
- `modelMatrixArray`: The 16-element float array containing the gizmo's transformation matrix (from ImGuizmo)
- `nodeName`: The name of the node being manipulated (for context in output)

### Function Implementation

**File:** `src/io/scene.cpp`

The function:
1. Converts the matrix array to `glm::mat4` using the `float16ToGlmMat4()` utility
2. Decomposes the matrix to extract translation, rotation (quaternion), and scale
3. Converts rotation quaternion to Euler angles (degrees) for readability
4. Prints formatted output to the console

**Output Format:**
```
========================================
[Gizmo Transform Test - Mouse Release]
Node: RootNode
----------------------------------------
Translation (X, Y, Z): (10.5, 2.3, -5.1)
Rotation (X, Y, Z) [degrees]: (0.0, 45.0, 0.0)
Scale (X, Y, Z): (1.0, 1.0, 1.0)
========================================
```

### Integration Point

**File:** `src/io/scene.cpp` - Inside `renderViewportWindow()` method

**Location:** After `ImGuizmo::Manipulate()` call and state update

**Code:**
```cpp
// Update gizmo state (check after Manipulate as it updates these states)
mGizmoUsing = ImGuizmo::IsUsing();
mGizmoOver = ImGuizmo::IsOver();

// Test function: Print gizmo transform values when manipulation ends (mouse release)
// Detect when gizmo state transitions from using to not using
if (mGizmoWasUsing && !mGizmoUsing) {
    // Gizmo manipulation just ended - print the final transform values
    printGizmoTransformValues(modelMatrixArray, selectedNode);
}

// Update previous state for next frame
mGizmoWasUsing = mGizmoUsing;
```

## How It Works

### Detection Logic

1. **State Tracking**: Each frame, `mGizmoUsing` is updated with the current gizmo state from `ImGuizmo::IsUsing()`
2. **Transition Detection**: When `mGizmoWasUsing` is `true` and `mGizmoUsing` becomes `false`, it means the user just released the mouse button (manipulation ended)
3. **Print Trigger**: The test function is called with the final transform matrix and node name
4. **State Update**: `mGizmoWasUsing` is updated to the current state for the next frame

### Flow Diagram

```
User manipulates gizmo (mouse down + drag)
    ↓
mGizmoUsing = true
mGizmoWasUsing = true (after first frame)
    ↓
User releases mouse button
    ↓
mGizmoUsing = false
mGizmoWasUsing = true (still from previous frame)
    ↓
Transition detected: mGizmoWasUsing && !mGizmoUsing
    ↓
printGizmoTransformValues() called
    ↓
Transform values printed to console (once)
    ↓
mGizmoWasUsing = false (updated for next frame)
```

## Usage

### When It Prints

The function prints transform values when:
1. User manipulates the gizmo (moves/rotates/scales)
2. User releases the mouse button (ends manipulation)
3. The gizmo state transitions from "using" to "not using"

### What It Prints

- **Node Name**: The name of the node being manipulated (e.g., "RootNode", "Rig_GRP", "LeftArm")
- **Translation**: X, Y, Z translation values in world space
- **Rotation**: X, Y, Z Euler angles in degrees
- **Scale**: X, Y, Z scale values

### Example Output

```
========================================
[Gizmo Transform Test - Mouse Release]
Node: RootNode
----------------------------------------
Translation (X, Y, Z): (15.234, 0.0, -3.456)
Rotation (X, Y, Z) [degrees]: (0.0, 90.0, 0.0)
Scale (X, Y, Z): (1.0, 1.0, 1.0)
========================================
```

## Key Design Decisions

### Why Print on Mouse Release?

- **No Infinite Loops**: Avoids continuous printing during manipulation
- **Action-Based**: Only prints when a specific user action occurs (mouse release)
- **Clean Console**: Prevents console spam and makes output meaningful
- **Final Values**: Captures the final transform values after manipulation is complete

### Why Track Previous State?

- **Transition Detection**: Need to detect when state changes from active to inactive
- **Single Print**: Ensures we print only once per manipulation session
- **Reliable Detection**: More reliable than checking mouse button state directly (which might have timing issues)

### Why Decompose Matrix?

- **Readability**: Translation, rotation, and scale are easier to understand than raw matrix values
- **Debugging**: Separated values make it easier to identify which component changed
- **Consistency**: Matches the format used in PropertyPanel (Euler angles in degrees)

## State Reset

The `mGizmoWasUsing` flag is reset to `false` in the following scenarios:
- When no model is loaded
- When no node is selected in outliner
- When viewport is not initialized

This ensures the state tracking doesn't get stuck in an incorrect state.

## Technical Details

### Matrix Conversion

The function uses `float16ToGlmMat4()` from `math3D.h` to convert the 16-element float array (column-major format) to a GLM matrix. This is the same conversion used throughout the codebase for ImGuizmo integration.

### Matrix Decomposition

The function uses `glm::decompose()` to extract:
- **Translation**: Direct position offset
- **Rotation**: Stored as quaternion, then converted to Euler angles
- **Scale**: Per-axis scaling factors

### Rotation Conversion

Rotation is converted from quaternion to Euler angles using:
```cpp
glm::vec3 rotationEuler = glm::degrees(glm::eulerAngles(rotation));
```

This provides human-readable rotation values in degrees, matching the PropertyPanel format.

## Future Enhancements

Potential improvements:
1. **File Output**: Option to save transform values to a file
2. **Transform History**: Keep a history of transform changes
3. **Comparison**: Compare transforms before and after manipulation
4. **Coordinate Space**: Show both world space and local space values
5. **Matrix Display**: Option to show full 4x4 matrix values

## Testing

To test the function:
1. Load a model with a rig
2. Select a node in the outliner (e.g., RootNode)
3. Manipulate the gizmo (move/rotate/scale)
4. Release the mouse button
5. Check the console for the transform values output

The output should appear once per manipulation session (when you release the mouse).

## Related Features

- **ImGuizmo Integration**: Uses ImGuizmo's `IsUsing()` state to detect manipulation
- **PropertyPanel**: Transform values are also updated in PropertyPanel during manipulation
- **Transform Save Mechanism**: Transform values are saved to PropertyPanel's bone transform maps

## Files Modified

1. **`src/io/scene.h`**:
   - Added `mGizmoWasUsing` member variable
   - Added `printGizmoTransformValues()` function declaration

2. **`src/io/scene.cpp`**:
   - Added state transition detection logic
   - Implemented `printGizmoTransformValues()` function
   - Added state reset in all gizmo state reset locations

3. **`MD/GIZMO_TRANSFORM_TEST_FUNCTION.md`**:
   - This documentation file

## World to Local Space Conversion (Root Rig)

### Overview
When the gizmo manipulates the root rig (Rig_GRP), it operates in **world space**. However, PropertyPanel values need to be in **local space** relative to the parent (RootNode). The test function now converts world space transforms to local space and displays both.

### Conversion Method

For root rig nodes:
1. **Get Parent Transform**: Retrieves the parent object's (RootNode) global transform
2. **Invert Parent Transform**: Calculates the inverse matrix to convert from world to local space
3. **Apply Conversion**: Multiplies world space transform by parent inverse: `localMatrix = parentInverse * worldMatrix`
4. **Decompose**: Extracts local translation, rotation, and scale from the converted matrix

### Output Format

When manipulating the root rig, the output shows both world and local space values:

```
========================================
[Gizmo Transform Test - Mouse Release]
Node: Rig_GRP
----------------------------------------
WORLD SPACE:
  Translation (X, Y, Z): (15.234, 0.0, -3.456)
  Rotation (X, Y, Z) [degrees]: (0.0, 90.0, 0.0)
  Scale (X, Y, Z): (1.0, 1.0, 1.0)
----------------------------------------
LOCAL SPACE (relative to parent):
  Translation (X, Y, Z): (15234.0, 0.0, -3456.0)
  Rotation (X, Y, Z) [degrees]: (0.0, 90.0, 0.0)
  Scale (X, Y, Z): (1.0, 1.0, 1.0)
========================================
```

### Technical Details

**Conversion Formula:**
```cpp
// Get parent's global transform
glm::mat4 parentGlobalTransform = fbxToGlmMatrix(parentObject->getGlobalTransform(), false);

// Invert to convert from world to local
glm::mat4 parentInverse = glm::inverse(parentGlobalTransform);

// Convert world space transform to local space
glm::mat4 localMatrix = parentInverse * worldMatrix;
```

**Why This Works:**
- The parent's global transform represents how the parent is positioned in world space
- Inverting it gives us the transformation from world space to the parent's local space
- Multiplying the world space transform by this inverse converts it to local space relative to the parent

### Current Status

- ✅ **World space values**: Displayed correctly from gizmo
- ✅ **Local space conversion**: Implemented for root rig (Rig_GRP)
- ✅ **Both values shown**: World and local space displayed side-by-side
- ⚠️ **Only for root rig**: Conversion currently only works for rig root nodes
- ⚠️ **Not yet applied**: Local space values are shown but not yet used to update PropertyPanel

### Future Enhancements

- Extend conversion to all bone nodes in the hierarchy
- Apply local space values to PropertyPanel transforms
- Add option to show/hide world or local space values
- Display conversion matrix for debugging

## Summary

The gizmo transform test function provides a clean, action-based way to inspect gizmo transformation values. It prints values only when gizmo manipulation ends (mouse release), avoiding console spam while providing useful debugging information. For root rig nodes, it now also converts and displays local space values relative to the parent, making it easier to understand the transform hierarchy. The implementation is modular, well-documented, and follows the project's coding standards.
