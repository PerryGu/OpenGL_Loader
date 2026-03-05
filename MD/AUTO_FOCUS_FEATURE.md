# Auto-Focus Feature Implementation

## Date
Created: 2026-02-16

## Overview

The Auto-Focus feature automatically triggers camera framing/focus logic whenever the Gizmo is released after being moved. This provides a seamless workflow where the camera automatically re-centers and re-frames the character every time you finish moving them with the Gizmo.

## Feature Description

When enabled, the Auto-Focus feature:
- Automatically triggers camera framing when the user releases the Gizmo after manipulation
- Uses the same framing logic as the 'F' key (Vertical Centering and AABB-based distance)
- Only activates when a valid gizmo position exists (gizmo is visible)
- Can be toggled on/off via a checkbox in the Tools menu

## Implementation Details

### 1. UI Control

**Location:** `src/io/scene.cpp` - `renderViewportWindow()` method, Tools menu

**Implementation:**
```cpp
// Auto Focus toggle (automatically frame camera when gizmo is released)
if (ImGui::MenuItem("Auto Focus", NULL, &m_autoFocusEnabled)) {
    // Checkbox state is automatically updated by ImGui::MenuItem
    // No additional action needed - the state is stored in m_autoFocusEnabled
}
```

**UI Location:** Viewport window → Tools menu → "Auto Focus" checkbox

### 2. State Management

**Member Variables** (`src/io/scene.h`):
- `bool m_autoFocusEnabled = false;` - Tracks whether auto-focus is enabled
- `bool m_cameraFramingRequested = false;` - Flag to signal camera should frame

**Accessor Methods** (`src/io/scene.h`):
```cpp
bool isCameraFramingRequested();  // Check if camera framing was requested
void resetCameraFramingRequest(); // Reset camera framing request flag
```

### 3. Gizmo Release Detection

**Location:** `src/io/scene.cpp` - `renderViewportWindow()` method, after `ImGuizmo::Manipulate()`

**Implementation:**
```cpp
// COMMIT ON RELEASE (OnMouseUp): Explicitly save final value and update matrices
// Detect when gizmo state transitions from using to not using
if (mGizmoWasUsing && !mGizmoUsing) {
    // Gizmo manipulation just ended (mouse release)
    
    // AUTO-FOCUS: If auto-focus is enabled, request camera framing
    // This will trigger the same framing logic as pressing 'F' key
    // The framing will use the existing "Vertical Centering" and "AABB-based distance" logic
    if (m_autoFocusEnabled && mGizmoPositionValid) {
        m_cameraFramingRequested = true;
        std::cout << "[Auto Focus] Gizmo released - requesting camera framing" << std::endl;
    }
    
    // ... rest of gizmo release handling ...
}
```

**Detection Logic:**
- Detects transition from `mGizmoWasUsing = true` to `mGizmoUsing = false`
- This occurs when the user releases the mouse button after dragging the gizmo
- Only triggers if `m_autoFocusEnabled` is true and `mGizmoPositionValid` is true

### 4. Camera Framing Trigger

**Location:** `src/main.cpp` - Main loop, after `scene.update()`

**Implementation:**
```cpp
//-- Check if auto-focus requested (gizmo was released with auto-focus enabled) ----
if (scene.isCameraFramingRequested()) {
    // Auto-focus was triggered - use the same logic as 'F' key
    // Check if gizmo is visible by checking if there's a selected node
    std::string selectedNode = scene.mOutliner.getSelectedNode();
    if (!selectedNode.empty() && modelManager.getModelCount() > 0) {
        // Node is selected - trigger one-shot aim constraint
        // The constraint will check if gizmo position is valid
        cameras[activeCam].setFraming(true);
        std::cout << "[Auto Focus] Camera framing triggered (gizmo released)" << std::endl;
    }
    scene.resetCameraFramingRequest();
}
```

**Flow:**
1. Check if `isCameraFramingRequested()` returns true
2. Verify that a node is selected (gizmo is visible)
3. Set camera framing flag: `cameras[activeCam].setFraming(true)`
4. Reset the request flag: `resetCameraFramingRequest()`

### 5. Framing Logic Reuse

The auto-focus feature reuses the existing camera framing system:
- **Vertical Centering:** Ensures the character is centered vertically in the viewport
- **AABB-based Distance:** Calculates optimal camera distance based on bounding box
- **One-Shot Logic:** Camera frames once, then releases control (same as 'F' key)

**Related Methods:**
- `Camera::aimAtTargetWithFraming()` - Main framing logic with distance calculation
- `Camera::aimAtTarget()` - Rotation-only aim (fallback)
- `Scene::applyCameraAimConstraint()` - Applies the framing constraint

## Usage

### Enabling Auto-Focus

1. Open the Viewport window
2. Click on the "Tools" menu in the viewport menu bar
3. Check the "Auto Focus" checkbox
4. The feature is now enabled

### Using Auto-Focus

1. Select a character/node in the Outliner or by clicking in the viewport
2. The Gizmo appears at the selected node's position
3. Move/rotate/scale the character using the Gizmo
4. Release the mouse button
5. **Camera automatically frames the character** (if Auto Focus is enabled)

### Disabling Auto-Focus

1. Open the Viewport window
2. Click on the "Tools" menu
3. Uncheck the "Auto Focus" checkbox
4. The feature is now disabled

## Safety Checks

### Manual 'F' Key Still Works

The manual 'F' key functionality is **unaffected** by the Auto-Focus setting:
- Pressing 'F' always triggers framing, regardless of Auto Focus checkbox state
- Auto-Focus only triggers on gizmo release, not on 'F' key press
- Both features can be used independently

### Gizmo Position Validation

Auto-Focus only triggers when:
- `m_autoFocusEnabled` is true (checkbox is checked)
- `mGizmoPositionValid` is true (gizmo is visible and has valid position)
- A node is selected (gizmo exists)

This prevents:
- Framing when no character is selected
- Framing when gizmo is not visible
- Unnecessary camera movements

## Debug Output

The feature includes action-based debug prints (following project guidelines):

**When Gizmo is Released:**
```
[Auto Focus] Gizmo released - requesting camera framing
```

**When Camera Framing is Triggered:**
```
[Auto Focus] Camera framing triggered (gizmo released)
```

**Note:** These prints only occur on specific actions (gizmo release), not every frame.

## Integration with Existing Systems

### Camera Aim Constraint System

Auto-Focus integrates seamlessly with the existing camera aim constraint system:
- Uses the same `Camera::setFraming(true)` flag
- Triggers the same `applyCameraAimConstraint()` logic
- Respects the same safety checks (right mouse button cancels framing)

### Gizmo State Tracking

Auto-Focus uses the existing gizmo state tracking:
- `mGizmoUsing` - Current gizmo manipulation state
- `mGizmoWasUsing` - Previous frame's gizmo state
- `mGizmoPositionValid` - Gizmo position validity

## Code Organization

### Files Modified

1. **`src/io/scene.h`**
   - Added `m_autoFocusEnabled` member variable
   - Added `m_cameraFramingRequested` member variable
   - Added `isCameraFramingRequested()` and `resetCameraFramingRequest()` methods

2. **`src/io/scene.cpp`**
   - Added Auto Focus checkbox to Tools menu
   - Added auto-focus logic in gizmo release detection
   - Implemented accessor methods

3. **`src/main.cpp`**
   - Added camera framing request check in main loop
   - Triggers camera framing when auto-focus is requested

### Code Principles Followed

- **Modularity:** Feature is self-contained with clear separation of concerns
- **Reusability:** Reuses existing camera framing logic
- **Action-Based Debug Prints:** Prints only on gizmo release, not every frame
- **Documentation:** Comprehensive inline comments and markdown documentation

## Future Enhancements

Potential improvements:
1. **Smooth Camera Movement:** Add smooth interpolation instead of instant framing
2. **Configurable Delay:** Add a small delay before framing (to avoid rapid re-framing)
3. **Per-Model Settings:** Allow auto-focus to be enabled per-model
4. **Animation During Framing:** Continue animation playback during camera framing
5. **Settings Persistence:** Save auto-focus state to app settings

## Related Documentation

- **Camera Aim Constraint:** `MD/CAMERA_AIM_CONSTRAINT_2026-02-16_14-30.md`
- **Camera Aim One-Shot:** `MD/CAMERA_AIM_ONE_SHOT.md`
- **Gizmo Transform System:** `MD/IMGUIZMO_PROPERTYPANEL_INTEGRATION.md`
- **Debug Print Guidelines:** `MD/DEBUG_PRINT_GUIDELINES.md`

## Summary

The Auto-Focus feature provides a convenient way to automatically frame characters after moving them with the Gizmo. It integrates seamlessly with the existing camera framing system, reuses all the perfected "Vertical Centering" and "AABB-based distance" logic, and maintains compatibility with manual 'F' key framing. The implementation follows project guidelines for modularity, documentation, and action-based debug output.
