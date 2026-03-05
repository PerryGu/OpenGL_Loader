# Camera Aim Constraint - One-Shot Trigger Implementation

## Date
Created: 2024

## Overview

Changed the camera aim constraint from a constant lock to a **one-shot trigger** that activates when the 'F' key is pressed. The camera rotates to look at the Gizmo once, then immediately releases control so the user can manually rotate the camera without fighting automated logic.

## Implementation Details

### 1. Control Flag (`camera.h`)

Added a boolean member variable to track framing state:

```cpp
bool m_isFraming = false;  // True when 'F' key is pressed to aim at gizmo (one-shot)
```

**Accessor Methods:**
```cpp
void setFraming(bool framing) { m_isFraming = framing; }
bool isFraming() const { return m_isFraming; }
```

### 2. Key Press Trigger (`main.cpp`)

Modified the 'F' key handler to trigger the one-shot aim constraint:

```cpp
//-- Focus on Gizmo: Press F to aim camera at gizmo (one-shot) ----
if (Keyboard::keyWentDown(GLFW_KEY_F)) {
    std::string selectedNode = scene.mOutliner.getSelectedNode();
    if (!selectedNode.empty() && modelManager.getModelCount() > 0) {
        // Node is selected - trigger one-shot aim constraint
        cameras[activeCam].setFraming(true);
        std::cout << "[Camera Aim] F key pressed - aiming at gizmo (one-shot)" << std::endl;
    } else {
        // Fallback to original framing behavior if no node selected
        // ... (original framing code)
    }
}
```

**Behavior:**
- If a node is selected (gizmo visible): Triggers one-shot aim constraint
- If no node selected: Falls back to original framing behavior (frame all objects)

### 3. One-Shot Logic (`scene.cpp`)

Modified `applyCameraAimConstraint()` to only run when the framing flag is set:

```cpp
void Scene::applyCameraAimConstraint(Camera* camera) {
    // 5. MOUSE INTERRUPT: Right mouse button cancels framing immediately
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        camera->setFraming(false);  // Force cancel framing
        return;
    }
    
    // Only apply constraint if framing flag is set (triggered by 'F' key)
    if (!camera->isFraming()) {
        return;  // Framing not active, don't apply constraint
    }
    
    // Only apply if gizmo position is valid (gizmo is visible)
    if (!mGizmoPositionValid) {
        camera->setFraming(false);  // Gizmo not visible, cancel framing
        return;
    }
    
    // Apply aim constraint (rotation only)
    bool aimComplete = camera->aimAtTarget(mGizmoWorldPosition);
    
    // Release lock when aim is complete
    if (aimComplete) {
        camera->setFraming(false);
    }
}
```

**Key Features:**
- ✅ Only runs when `m_isFraming` is true (triggered by 'F' key)
- ✅ Right mouse button immediately cancels framing
- ✅ Automatically cancels if gizmo becomes invisible
- ✅ Releases lock when aim is complete

### 4. Completion Detection (`camera.cpp`)

Modified `aimAtTarget()` to return completion status:

```cpp
bool Camera::aimAtTarget(glm::vec3 targetPos) {
    // Calculate direction to target
    glm::vec3 aimDir = glm::normalize(targetPos - cameraPos);
    
    // Check if camera is already looking at target (dot product close to 1.0)
    // This allows smooth completion detection - check BEFORE updating
    float dotProduct = glm::dot(glm::normalize(cameraFront), aimDir);
    bool isAimed = (dotProduct > 0.999f);  // Very close to looking at target
    
    // Rebuild camera basis vectors (rotation only, position unchanged)
    cameraFront = aimDir;
    cameraRight = glm::normalize(glm::cross(cameraFront, worldUp));
    cameraUp = glm::normalize(glm::cross(cameraRight, cameraFront));
    
    // Update yaw/pitch and focus point
    // ...
    
    return isAimed;  // Return true if already aimed, false if still aiming
}
```

**Completion Detection:**
- Uses dot product between current forward and target direction
- Threshold: `dotProduct > 0.999f` (approximately 2.5 degrees)
- Checked **before** updating camera vectors (for accurate detection)
- Returns `true` when aim is complete, `false` when still aiming

### 5. Mouse Interrupt

Right mouse button immediately cancels framing:

```cpp
// 5. MOUSE INTERRUPT: If right mouse button is pressed, cancel framing immediately
if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
    camera->setFraming(false);  // Force cancel framing
    return;  // User is manually controlling camera
}
```

**Behavior:**
- Right mouse button pressed → Framing flag set to `false`
- Constraint immediately stops applying
- User can manually control camera without interference

## Behavior Flow

### Normal Operation (No Framing)

1. User loads model and selects node → Gizmo appears
2. Camera behaves normally (no automatic aiming)
3. User can manually orbit camera with Alt+mouse

### One-Shot Aim (F Key Pressed)

1. User presses 'F' key
2. `m_isFraming` flag set to `true`
3. Camera starts rotating to look at gizmo (one frame)
4. Completion check: dot product between current forward and target direction
5. If `dotProduct > 0.999f`: Aim complete → `m_isFraming` set to `false`
6. If not complete: Continue aiming next frame
7. Once complete: Lock released, user can immediately control camera manually

### Mouse Interrupt

1. User presses 'F' → Framing starts
2. User presses right mouse button → Framing immediately cancelled
3. `m_isFraming` set to `false`
4. User can manually control camera

## Comparison: Before vs After

### Before (Constant Lock)
- ❌ Camera constantly aimed at gizmo when visible
- ❌ User had to hold right mouse to override
- ❌ Fighting between automated and manual control
- ❌ No way to disable without right mouse

### After (One-Shot Trigger)
- ✅ Camera aims at gizmo only when 'F' is pressed
- ✅ Aim completes in 1-2 frames (immediate)
- ✅ Lock automatically releases when complete
- ✅ User can immediately control camera after aim
- ✅ Right mouse button cancels framing instantly
- ✅ No constant interference with manual control

## Code Changes Summary

### Files Modified

1. **`src/io/camera.h`**:
   - Added `bool m_isFraming = false;` member variable
   - Added `setFraming()` and `isFraming()` accessor methods
   - Changed `aimAtTarget()` return type from `void` to `bool`

2. **`src/io/camera.cpp`**:
   - Modified `aimAtTarget()` to return completion status
   - Added dot product check for smooth completion detection

3. **`src/io/scene.cpp`**:
   - Modified `applyCameraAimConstraint()` to check framing flag
   - Added right mouse button interrupt logic
   - Added automatic lock release on completion

4. **`src/main.cpp`**:
   - Modified 'F' key handler to trigger framing flag
   - Added fallback to original framing behavior when no node selected

## Testing Checklist

When testing the one-shot camera aim:

- [ ] Press 'F' when gizmo is visible → Camera rotates to look at gizmo
- [ ] Camera position does not change (only rotation)
- [ ] Aim completes in 1-2 frames (immediate)
- [ ] After aim completes, user can immediately control camera manually
- [ ] Right mouse button cancels framing immediately
- [ ] Pressing 'F' multiple times re-triggers aim (one-shot each time)
- [ ] If no node selected, 'F' falls back to original framing behavior
- [ ] If gizmo becomes invisible, framing is automatically cancelled
- [ ] No constant interference with manual camera control

## Summary

The camera aim constraint is now a **one-shot trigger** activated by the 'F' key:

✅ **One-Shot**: Activates only when 'F' is pressed, not constantly
✅ **Immediate Completion**: Aim completes in 1-2 frames
✅ **Auto-Release**: Lock automatically releases when aim is complete
✅ **Mouse Interrupt**: Right mouse button cancels framing instantly
✅ **User Control**: User can immediately control camera after aim completes
✅ **No Fighting**: No constant interference between automated and manual control

**Key Improvement**: The camera aims at the gizmo once when 'F' is pressed, then immediately releases control so the user can manually rotate the camera without any automated interference.
