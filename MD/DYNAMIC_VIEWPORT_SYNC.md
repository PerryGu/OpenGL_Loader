# Dynamic Viewport Synchronization

## Date
Created: 2024

## Overview

Implemented dynamic viewport synchronization to ensure the camera's projection matrix and OpenGL viewport match the exact ImGui viewport window's content region size. This ensures that when 'F' is pressed to aim at the gizmo, it will be correctly centered in the viewport.

## Objective

**Force the camera's center to match the ImGui Viewport window's center.**

This is achieved by:
1. Dynamically detecting the exact content region size every frame
2. Synchronizing the aspect ratio to match the viewport dimensions
3. Updating the projection matrix with the correct aspect ratio
4. Setting `glViewport` to match the exact content region size

## Implementation Details

### 1. Dynamic Size Detection (`scene.cpp`)

The viewport content region size is detected every frame in `renderViewportWindow()`:

```cpp
// In renderViewportWindow(), within the ImGui "Viewport" window block
cachedViewportSize = ImGui::GetContentRegionAvail();
```

**Key Points:**
- Uses `ImGui::GetContentRegionAvail()` to get the EXACT width and height of the rendering area
- This excludes window decorations, menu bars, and padding
- Cached for use in rendering and projection matrix calculation
- Updated every frame to handle window resizing

### 2. Aspect Ratio Sync (`main.cpp`)

The aspect ratio is calculated from the actual content region size:

```cpp
// 1. DYNAMIC SIZE DETECTION: Get the EXACT width and height of the rendering area
ImVec2 viewportContentSize = scene.getViewportContentSize();

// 2. ASPECT RATIO SYNC: Immediately update aspect ratio using the NEW content size
float aspectRatio = (viewportContentSize.x > 0 && viewportContentSize.y > 0) ? 
                    (float)viewportContentSize.x / (float)viewportContentSize.y : 
                    (float)SCR_WIDTH / (float)SCR_HEIGHT;

// 2. ASPECT RATIO SYNC: Update the Projection Matrix using this NEW aspect ratio every frame
projection = glm::perspective(glm::radians(cameras[activeCam].getZoom()), aspectRatio, 0.1f, farClipPlane);
```

**Key Features:**
- Aspect ratio is calculated from actual content region size (not framebuffer size)
- Projection matrix is updated every frame with the correct aspect ratio
- Ensures objects are correctly centered regardless of window size

### 3. GL Viewport Sync (`scene.cpp`)

OpenGL viewport is set to match the exact content region size:

```cpp
// In beginViewportRender()
// 3. GL VIEWPORT SYNC: Use the exact content region size for glViewport
int contentWidth = (int)cachedViewportSize.x;
int contentHeight = (int)cachedViewportSize.y;

// Ensure valid dimensions (avoid division by zero)
if (contentWidth <= 0) contentWidth = 1;
if (contentHeight <= 0) contentHeight = 1;

// Set OpenGL viewport to match the exact content region size
// This ensures NDC coordinates (-1 to 1) map correctly to pixels
glViewport(0, 0, contentWidth, contentHeight);
```

**Key Features:**
- `glViewport` is called with the exact content region dimensions
- Ensures NDC coordinates (-1 to 1) map correctly to pixels
- Framebuffer is automatically resized to match content size (in `renderViewportWindow()`)

### 4. Framebuffer Resize (`scene.cpp`)

The framebuffer is automatically resized when the content region size changes:

```cpp
// In renderViewportWindow()
ImVec2 contentSize = cachedViewportSize;

// Resize framebuffer if window size changed
int newWidth = (int)contentSize.x;
int newHeight = (int)contentSize.y;
if (newWidth != viewportWidth || newHeight != viewportHeight) {
    resizeViewportFBO(newWidth, newHeight);
}
```

**Key Features:**
- Framebuffer texture and renderbuffer are resized to match content region
- Happens automatically when window is resized
- Ensures framebuffer always matches the rendering area

### 5. The Aim Fix

Now, when 'F' is pressed to aim at the gizmo:

```cpp
// 4. THE AIM FIX
// - Use the Gizmo's World Position
// - Point the camera forward vector at it
// - Since the Aspect Ratio is synced to the window size, the Gizmo MUST land in the center
camera->aimAtTarget(mGizmoWorldPosition);
```

**Why It Works:**
- Projection matrix uses the exact viewport aspect ratio
- `glViewport` matches the exact content region size
- NDC coordinates (-1 to 1) map correctly to viewport center
- Gizmo world position projects to viewport center when camera aims at it

## Code Changes

### Files Modified

1. **`src/io/scene.h`**:
   - Added `getViewportContentSize()` method declaration

2. **`src/io/scene.cpp`**:
   - Added `getViewportContentSize()` implementation
   - Modified `beginViewportRender()` to use content size for `glViewport`
   - Added comments explaining the viewport synchronization

3. **`src/main.cpp`**:
   - Modified projection matrix calculation to use content region size
   - Modified raycast code to use content size for consistency
   - Added comments explaining dynamic size detection and aspect ratio sync

## Technical Details

### Coordinate System Alignment

**Before (Potential Issues):**
- Framebuffer size might not match content region size
- Aspect ratio calculated from framebuffer size
- `glViewport` might use different dimensions
- Mismatch between projection and viewport could cause off-center rendering

**After (Fixed):**
- Framebuffer size matches content region size (auto-resized)
- Aspect ratio calculated from content region size
- `glViewport` uses exact content region dimensions
- Perfect alignment between projection and viewport

### Size Detection Flow

```
Frame Start
    ↓
scene.update() → renderViewportWindow()
    ↓
ImGui::GetContentRegionAvail() → cachedViewportSize
    ↓
Resize framebuffer if size changed
    ↓
scene.beginViewportRender()
    ↓
glViewport(0, 0, contentWidth, contentHeight)
    ↓
main.cpp: Calculate aspect ratio from contentSize
    ↓
glm::perspective(..., aspectRatio, ...)
    ↓
Render with synchronized viewport and projection
```

### Aspect Ratio Calculation

```cpp
aspectRatio = contentWidth / contentHeight
```

**Example:**
- Content region: 800x600 → aspectRatio = 1.333
- Content region: 1920x1080 → aspectRatio = 1.778
- Content region: 400x400 → aspectRatio = 1.0

The projection matrix uses this aspect ratio, ensuring objects are correctly centered.

## Benefits

### 1. Accurate Centering
- Objects at world origin (0,0,0) appear at viewport center
- Gizmo appears centered when camera aims at it
- No off-center rendering due to aspect ratio mismatch

### 2. Dynamic Resizing
- Viewport automatically adapts when window is resized
- Aspect ratio updates immediately
- No manual resizing required

### 3. Consistent Coordinate Systems
- NDC coordinates map correctly to pixels
- Mouse coordinates match rendering coordinates
- Raycast works accurately with viewport selection

### 4. Gizmo Aim Accuracy
- When 'F' is pressed, gizmo lands exactly in viewport center
- No offset due to aspect ratio mismatch
- Perfect alignment between camera aim and viewport center

## Testing Checklist

When testing dynamic viewport synchronization:

- [ ] Resize viewport window → Aspect ratio updates immediately
- [ ] Objects at origin appear centered in viewport
- [ ] Press 'F' to aim at gizmo → Gizmo appears centered
- [ ] Viewport selection (mouse click) works accurately
- [ ] No distortion when window is resized
- [ ] Framebuffer resizes automatically when content region changes
- [ ] `glViewport` matches content region size
- [ ] Projection matrix uses correct aspect ratio

## Summary

Dynamic viewport synchronization ensures:

✅ **Exact Size Detection**: Content region size detected every frame using `GetContentRegionAvail()`
✅ **Aspect Ratio Sync**: Projection matrix uses exact viewport aspect ratio
✅ **GL Viewport Sync**: `glViewport` matches exact content region dimensions
✅ **Framebuffer Match**: Framebuffer automatically resized to match content size
✅ **Perfect Centering**: Gizmo lands exactly in viewport center when 'F' is pressed
✅ **Dynamic Resizing**: Automatically adapts when window is resized

**Key Improvement**: The camera's projection matrix and OpenGL viewport now perfectly match the ImGui viewport window's content region, ensuring accurate centering and eliminating aspect ratio mismatches.
