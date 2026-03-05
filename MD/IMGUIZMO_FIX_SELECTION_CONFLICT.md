# ImGuizmo Fix: Selection and Gizmo Interaction Conflicts

## Problem
After adding ImGuizmo, two issues occurred:
1. **Intermittent selection**: Sometimes clicking on a model would select it, sometimes it wouldn't
2. **Gizmo deselection**: When trying to grab the gizmo to move a model, the selection would be canceled and the gizmo would disappear

## Root Cause
ImGuizmo was capturing mouse input, but the selection code in `main.cpp` wasn't checking if ImGuizmo was using the mouse. This caused:
- Selection clicks to be processed even when the mouse was over the gizmo
- Clicking on the gizmo would trigger deselection (clicking on empty space)
- Race condition between gizmo input handling and selection input handling

## Solution Applied

### 1. Added Gizmo State Tracking (`src/io/scene.h`)

**Added state variables:**
```cpp
//-- gizmo state tracking (for preventing selection conflicts) -------------------
bool mGizmoUsing = false;  // True if gizmo is currently being manipulated
bool mGizmoOver = false;   // True if mouse is over the gizmo
```

**Added accessor methods:**
```cpp
//-- Check if ImGuizmo is currently using the mouse (for preventing selection conflicts) ----
bool isGizmoUsing() const { return mGizmoUsing; }

//-- Check if mouse is over ImGuizmo gizmo (for preventing selection conflicts) ----
bool isGizmoOver() const { return mGizmoOver; }
```

### 2. Updated Gizmo Rendering (`src/io/scene.cpp`)

**Added state updates after Manipulate():**
```cpp
// Update gizmo state (check after Manipulate as it updates these states)
mGizmoUsing = ImGuizmo::IsUsing();
mGizmoOver = ImGuizmo::IsOver();
```

**Reset state when no model is selected:**
```cpp
} else {
    // No model selected, reset gizmo state
    mGizmoUsing = false;
    mGizmoOver = false;
}
```

### 3. Updated Selection Code (`src/main.cpp`)

**Added ImGuizmo include:**
```cpp
#include <ImGuizmo/ImGuizmo.h>  // For ImGuizmo state checks
```

**Added gizmo state checks before processing selection:**
```cpp
// Check if ImGuizmo is using the mouse - if so, don't process selection
// This prevents deselection when clicking on the gizmo
// Check directly from ImGuizmo (more reliable than cached state)
bool gizmoUsing = ImGuizmo::IsUsing();
bool gizmoOver = ImGuizmo::IsOver();

// IMPORTANT: Skip selection if gizmo is being used or mouse is over gizmo
if (isOverViewportImage && !altPressed && file_is_open && !gizmoUsing && !gizmoOver) {
    // ... selection code ...
}
```

## How It Works

1. **Gizmo State Tracking**: After calling `ImGuizmo::Manipulate()`, we check `IsUsing()` and `IsOver()` to track the gizmo's state
2. **Selection Prevention**: Before processing selection clicks, we check if the gizmo is active
3. **Direct State Check**: We check ImGuizmo state directly in main.cpp (not cached) for accuracy
4. **State Reset**: When no model is selected, gizmo state is reset to prevent stale state

## Benefits

- ✅ Selection works reliably - clicks on models are processed correctly
- ✅ Gizmo can be grabbed without deselecting - clicking on gizmo doesn't trigger deselection
- ✅ No input conflicts - gizmo and selection don't interfere with each other
- ✅ Better user experience - smooth interaction with both selection and gizmo

## Files Modified

1. `src/io/scene.h` - Added gizmo state tracking and accessor methods
2. `src/io/scene.cpp` - Added state updates in gizmo rendering
3. `src/main.cpp` - Added ImGuizmo include and state checks in selection code

## Testing

After rebuilding:
1. ✅ Clicking on a model should reliably select it
2. ✅ Clicking on the gizmo should NOT deselect the model
3. ✅ Dragging the gizmo should move the model without issues
4. ✅ Clicking on empty space (not on gizmo) should deselect

## Date
Fixed: [Current Session]
