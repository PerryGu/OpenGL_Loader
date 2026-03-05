# Viewport Window Implementation

## Summary
The 3D viewport (where FBX models are rendered) is now displayed in a dockable ImGui window called "Viewport". This allows you to move and dock the viewport just like the other UI panels.

## Implementation Details

### 1. Framebuffer Setup
- Created an off-screen framebuffer (FBO) to render the 3D scene
- The framebuffer includes a color texture and depth/stencil renderbuffer
- Automatically resizes when the viewport window is resized

### 2. Viewport Window
- Added `renderViewportWindow()` method that creates an ImGui window
- Displays the framebuffer texture in the window
- Window is dockable and can be moved/resized like other panels

### 3. Docking Layout
- Updated the default docking layout to include the Viewport window in the center
- Viewport is positioned between the left panel (Properties/Outliner) and bottom panel (timeSlider)

### 4. Rendering Changes
- Modified `main.cpp` to render to the framebuffer instead of directly to the window
- Uses `beginViewportRender()` before rendering and `endViewportRender()` after
- Projection matrix now uses the viewport size for correct aspect ratio

## Files Modified
- `src/io/scene.h` - Added framebuffer members and method declarations
- `src/io/scene.cpp` - Added framebuffer management and viewport window rendering
- `src/main.cpp` - Updated to use viewport rendering instead of direct window rendering

## Usage
When you load an FBX file, the 3D model will appear in the "Viewport" window, which you can:
- Move by dragging the title bar
- Resize by dragging the edges
- Dock to different positions in the UI
- Undock to make it a floating window

The viewport automatically adjusts its rendering size when you resize the window.
