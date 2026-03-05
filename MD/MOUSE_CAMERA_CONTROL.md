# Mouse Camera Control Implementation

## Overview
This document describes the implementation of mouse-based camera control for the OpenGL_loader project. The camera uses Maya-style viewport navigation controls, compatible with standard 3D modeling software workflows.

## Date
Implementation Date: 2024
Updated: 2024 (Maya-style controls)

## Problem Statement
Previously, camera control required holding the ALT key, which was not intuitive. Additionally, there were issues with:
- Mouse input being captured by ImGui even when trying to control the camera
- Camera jumping when entering the viewport
- No clear indication of when camera control is active
- Controls were not compatible with standard 3D software workflows (like Maya)

## Solution
Implemented a viewport-aware mouse camera control system with Maya-style navigation that:
1. Only responds to mouse input when the cursor is over the viewport window
2. Uses Alt + Left Mouse Button for camera rotation (orbit)
3. Uses Alt + Middle Mouse Button for camera panning
4. Uses Alt + Right Mouse Button for camera dollying (zoom by moving camera)
5. Uses scroll wheel for FOV zoom (works with or without Alt)
6. Prevents ImGui from capturing mouse input when Alt is pressed
7. Properly resets mouse state when entering/leaving viewport

## Changes Made

### 1. Scene Class (`src/io/scene.h` and `src/io/scene.cpp`)

#### Added Member Variable
- `bool mouseOverViewport` - Tracks whether the mouse cursor is currently over the viewport window

#### Added Public Method
- `bool isMouseOverViewport()` - Returns true if mouse is currently over the viewport window

#### Modified Method: `renderViewportWindow()`
**Before:**
```cpp
void Scene::renderViewportWindow()
{
    if (!show_viewport) return;
    ImGui::Begin("Viewport", &show_viewport);
    // ... rest of implementation
}
```

**After:**
```cpp
void Scene::renderViewportWindow()
{
    if (!show_viewport) {
        mouseOverViewport = false;
        return;
    }
    ImGui::Begin("Viewport", &show_viewport);
    
    // Check if mouse is over the viewport window
    mouseOverViewport = ImGui::IsWindowHovered();
    
    // ... rest of implementation
}
```

**Why:** This tracks whether the mouse is hovering over the viewport, which is essential for determining when camera control should be active.

### 2. Mouse Class (`src/io/mouse.h` and `src/io/mouse.cpp`)

#### Added Public Method
- `static void resetFirstMouse()` - Resets the firstMouse flag, preventing camera jump when entering viewport

**Implementation:**
```cpp
void Mouse::resetFirstMouse() {
    firstMouse = true;
}
```

**Why:** When the mouse enters the viewport, we need to reset the firstMouse flag to prevent a sudden camera jump caused by the large delta between the last known position and the current position.

### 3. Camera Class (`src/io/camera.h` and `src/io/camera.cpp`)

#### Added Methods
- `void updateCameraPan(double dx, double dy)` - Pans the camera left/right and up/down
- `void updateCameraDolly(double dy)` - Moves the camera forward/backward (dollying)

**Implementation:**
```cpp
void Camera::updateCameraPan(double dx, double dy) {
    float panSpeed = 0.01f;
    cameraPos -= cameraRight * (float)dx * panSpeed;
    cameraPos += cameraUp * (float)dy * panSpeed;
}

void Camera::updateCameraDolly(double dy) {
    float dollySpeed = 0.1f;
    cameraPos += cameraFront * (float)dy * dollySpeed;
}
```

**Why:** These methods enable Maya-style camera panning and dollying, which are essential for 3D viewport navigation.

### 4. Main Input Processing (`src/main.cpp`)

#### Modified Function: `processInput()`

**Before:**
```cpp
//== mouse transform ============================================
if (Keyboard::key(GLFW_KEY_LEFT_ALT)) {
    double dx = Mouse::getDX() * 0.1, dy = Mouse::getDY() * 0.1;
    if (dx != 0 || deltaTime != 0) {
        cameras[activeCam].updateCameraDirection(dx, dy);
    }
    
    double scrollDy = Mouse::getScrollDY();
    if (scrollDy != 0) {
       cameras[activeCam].updateCameraZoom(scrollDy);
    }
}
```

**After (Maya-Style Controls):**
```cpp
//== mouse transform (Maya-style controls) ============================================
// Only process camera movement when mouse is over the viewport
bool isOverViewport = scene.isMouseOverViewport();

// Track previous hover state to reset firstMouse when entering viewport
static bool wasOverViewport = false;
if (isOverViewport && !wasOverViewport) {
    // Mouse just entered viewport - reset firstMouse to prevent jump
    Mouse::resetFirstMouse();
}
wasOverViewport = isOverViewport;

// Check if Alt key is pressed (required for Maya-style camera controls)
bool altPressed = Keyboard::key(GLFW_KEY_LEFT_ALT) || Keyboard::key(GLFW_KEY_RIGHT_ALT);

if (isOverViewport && altPressed) {
    // Prevent ImGui from capturing mouse input when Alt is pressed
    ImGuiIO& io = ImGui::GetIO();
    io.WantCaptureMouse = false;
    
    // Maya-style camera controls:
    // Alt + Left Mouse Button: Rotate (orbit)
    if (Mouse::button(GLFW_MOUSE_BUTTON_LEFT)) {
        double dx = Mouse::getDX() * 0.1;
        double dy = Mouse::getDY() * 0.1;
        if (dx != 0 || dy != 0) {
            cameras[activeCam].updateCameraDirection(dx, dy);
        }
    }
    
    // Alt + Middle Mouse Button: Pan
    if (Mouse::button(GLFW_MOUSE_BUTTON_MIDDLE)) {
        double dx = Mouse::getDX();
        double dy = Mouse::getDY();
        if (dx != 0 || dy != 0) {
            cameras[activeCam].updateCameraPan(dx, dy);
        }
    }
    
    // Alt + Right Mouse Button: Dolly (zoom by moving camera)
    if (Mouse::button(GLFW_MOUSE_BUTTON_RIGHT)) {
        double dy = Mouse::getDY();
        if (dy != 0) {
            cameras[activeCam].updateCameraDolly(dy);
        }
    }
}

// Camera dolly with scroll wheel (only when over viewport) - works with or without Alt
// In Maya, scroll wheel dollies (moves camera), not FOV zoom
if (isOverViewport) {
    double scrollDy = Mouse::getScrollDY();
    if (scrollDy != 0) {
        // Scroll up (positive) moves camera forward, scroll down (negative) moves backward
        cameras[activeCam].updateCameraDolly(scrollDy * 0.5); // Scale scroll for better control
    }
}
```

**Key Changes:**
1. **Viewport Detection:** Checks if mouse is over viewport before processing any camera input
2. **First Mouse Reset:** Resets firstMouse flag when mouse enters viewport to prevent camera jump
3. **Maya-Style Controls:** 
   - Alt + Left Mouse: Rotate (orbit)
   - Alt + Middle Mouse: Pan
   - Alt + Right Mouse: Dolly (zoom by movement)
4. **Alt Key Detection:** Checks for both left and right Alt keys
5. **ImGui Capture Prevention:** Sets `io.WantCaptureMouse = false` when Alt is pressed to prevent ImGui from interfering
6. **Scroll Wheel:** Works independently for FOV zoom (with or without Alt)

## Usage (Maya-Style Controls)

### Camera Rotation (Orbit)
- **Action:** Hold **Alt + Left Mouse Button** and move mouse
- **When:** Only works when mouse cursor is over the viewport window
- **Behavior:** 
  - Horizontal movement rotates camera left/right (yaw)
  - Vertical movement rotates camera up/down (pitch)
  - Pitch is clamped to ±89 degrees to prevent gimbal lock
  - Camera orbits around the current view direction

### Camera Panning
- **Action:** Hold **Alt + Middle Mouse Button** and move mouse
- **When:** Only works when mouse cursor is over the viewport window
- **Behavior:** 
  - Horizontal movement pans camera left/right
  - Vertical movement pans camera up/down
  - Camera moves perpendicular to the view direction

### Camera Dollying (Zoom by Movement)
- **Action:** Hold **Alt + Right Mouse Button** and move mouse up/down
- **When:** Only works when mouse cursor is over the viewport window
- **Behavior:** 
  - Moving mouse up moves camera forward (zooms in)
  - Moving mouse down moves camera backward (zooms out)
  - Camera moves along the view direction

### Camera Dolly (Scroll Wheel)
- **Action:** Scroll mouse wheel
- **When:** Only works when mouse cursor is over the viewport window
- **Behavior:** 
  - Scroll up moves camera forward (towards the scene/character)
  - Scroll down moves camera backward (away from the scene/character)
  - Camera moves along the view direction (dollying)
  - Works with or without Alt key pressed
  - Matches Maya's scroll wheel behavior

## Technical Details

### Mouse Delta Calculation
The mouse delta (dx, dy) is calculated in `Mouse::cursorPosCallback()`:
- `dx = x - lastX` (horizontal movement)
- `dy = lastY - y` (vertical movement, inverted because screen coordinates are top-to-bottom)

The delta is then scaled by 0.1 in `processInput()` for camera rotation sensitivity.

### First Mouse Flag
The `firstMouse` flag prevents a large camera jump on the first mouse movement by initializing `lastX` and `lastY` to the current mouse position. This flag is reset when:
1. The mouse enters the viewport (to prevent jump when moving from UI to viewport)
2. The application starts

### ImGui Integration
When the right mouse button is pressed over the viewport, we set `io.WantCaptureMouse = false` to prevent ImGui from processing the mouse input. This ensures smooth camera control without interference from the UI system.

## Potential Issues and Fixes

### Issue: Camera Jumps When Entering Viewport
**Symptom:** Camera suddenly rotates when moving mouse from UI panel to viewport
**Fix:** Reset `firstMouse` flag when mouse enters viewport (implemented)

### Issue: Camera Doesn't Respond
**Possible Causes:**
1. Mouse is not over viewport window
2. Alt key is not being pressed (required for Maya-style controls)
3. Wrong mouse button is being used
4. ImGui is capturing mouse input

**Debug Steps:**
- Check `scene.isMouseOverViewport()` returns true
- Verify Alt key is pressed (`Keyboard::key(GLFW_KEY_LEFT_ALT)` or `GLFW_KEY_RIGHT_ALT`)
- Verify correct mouse button is pressed:
  - Left button for rotation
  - Middle button for panning
  - Right button for dollying
- Check `io.WantCaptureMouse` is set to false when Alt is pressed

### Issue: Camera Rotates When Clicking UI Elements
**Fix:** Camera only responds when mouse is over viewport AND right button is pressed. UI elements outside viewport are unaffected.

## Future Improvements

1. **Sensitivity Settings:** Make mouse sensitivity configurable for pan, dolly, and rotation
2. **Invert Y-Axis:** Add option to invert vertical mouse movement
3. **Camera Speed:** Add configurable camera movement speed
4. **Orbit Mode:** Implement orbit camera mode around a focus point (currently uses free-look)
5. **Customizable Controls:** Allow users to customize key/mouse button combinations
6. **Smooth Interpolation:** Add smooth camera movement interpolation for better feel

## Related Files
- `src/main.cpp` - Main input processing
- `src/io/scene.h` / `src/io/scene.cpp` - Viewport and scene management
- `src/io/mouse.h` / `src/io/mouse.cpp` - Mouse input handling
- `src/io/camera.h` / `src/io/camera.cpp` - Camera implementation

## Testing Checklist
- [x] Camera rotates with Alt + Left mouse button over viewport
- [x] Camera pans with Alt + Middle mouse button over viewport
- [x] Camera dollies with Alt + Right mouse button over viewport
- [x] Camera does not respond when mouse is outside viewport
- [x] Camera does not jump when entering viewport
- [x] Scroll wheel dollies camera when over viewport (works with or without Alt)
- [x] Scroll wheel allows moving away from character (scroll down moves camera backward)
- [x] ImGui UI elements still work correctly when Alt is not pressed
- [x] No interference between camera control and UI interaction
- [x] All controls require Alt key (Maya-style compatibility)
