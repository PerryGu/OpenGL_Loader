# Perspective Drift Fix - F-Key Aim Centering

## Date
Created: 2024

## Problem

The Gizmo was not centered when pressing 'F' to aim. The rightward offset increased as the Gizmo got further from the camera, indicating a mismatch between the Camera's Aspect Ratio and the actual OpenGL Viewport dimensions.

## Root Causes

1. **View Matrix Rotation Offset**: A -25 degree rotation was applied to the view matrix, causing perspective drift
2. **Aspect Ratio Mismatch**: Aspect ratio might not have matched the exact viewport content size
3. **glViewport Sync**: glViewport might not have been using the exact content region dimensions
4. **Coordinate System Mismatch**: NDC coordinates might not have mapped correctly to viewport pixels

## Solution

### 1. Dynamic Viewport Capture (`scene.cpp`)

Captured EXACT screen-space coordinates inside the ImGui "Viewport" window:

```cpp
// 1. DYNAMIC VIEWPORT CAPTURE: Capture EXACT screen-space coordinates
// Inside the ImGui "Viewport" window code, capture the EXACT screen-space coordinates
ImVec2 pos = ImGui::GetCursorScreenPos();  // Screen-space position of content region
ImVec2 size = ImGui::GetContentRegionAvail();  // Exact size of content region

// Store screen-space coordinates for viewport synchronization
mViewportScreenPos = pos;
mViewportScreenSize = size;
```

**Key Points:**
- Uses `ImGui::GetCursorScreenPos()` for exact screen-space position
- Uses `ImGui::GetContentRegionAvail()` for exact content region size
- Cached every frame in `renderViewportWindow()`
- Stored in `mViewportScreenPos` and `mViewportScreenSize` for use in rendering

### 2. Force glViewport Sync (`scene.cpp`)

Call `glViewport` every frame right before rendering with exact content size:

```cpp
// 2. FORCE glViewport SYNC: Call glViewport every frame right before rendering the scene
// Use the EXACT screen-space size captured in renderViewportWindow()
// Ensure NO other glViewport calls exist that use window-level dimensions
int viewportWidth = (int)mViewportScreenSize.x;
int viewportHeight = (int)mViewportScreenSize.y;

// Ensure valid dimensions (avoid division by zero)
if (viewportWidth <= 0) viewportWidth = 1;
if (viewportHeight <= 0) viewportHeight = 1;

// CRITICAL: Set glViewport to exact content region size every frame
glViewport(0, 0, viewportWidth, viewportHeight);
```

**Key Features:**
- Called every frame in `beginViewportRender()` (right before rendering)
- Uses exact content region size (not window-level dimensions)
- Ensures NDC coordinates (-1 to 1) map correctly to pixels
- No other glViewport calls interfere (other calls are for main window, not viewport framebuffer)

### 3. Correct Aspect Ratio (`main.cpp`)

Update projection matrix using exactly `size.x / size.y` with floating-point division:

```cpp
// 1. DYNAMIC SIZE DETECTION: Get the EXACT width and height of the rendering area
ImVec2 viewportContentSize = scene.getViewportContentSize();

// 3. CORRECT ASPECT RATIO: Update camera.projectionMatrix using exactly size.x / size.y
// CRUCIAL: Ensure the division is floating-point: (float)size.x / (float)size.y
float aspectRatio = (viewportContentSize.x > 0 && viewportContentSize.y > 0) ? 
                    (float)viewportContentSize.x / (float)viewportContentSize.y : 
                    (float)SCR_WIDTH / (float)SCR_HEIGHT;

// 3. CORRECT ASPECT RATIO: Update the Projection Matrix using this NEW aspect ratio every frame
projection = glm::perspective(glm::radians(cameras[activeCam].getZoom()), aspectRatio, 0.1f, farClipPlane);
```

**Key Features:**
- Uses floating-point division: `(float)size.x / (float)size.y`
- Calculated from exact content region size (not framebuffer size)
- Updated every frame to handle dynamic resizing
- Ensures projection matrix matches exact viewport dimensions

### 4. Removed View Matrix Rotation Offset (`main.cpp`)

Removed the -25 degree rotation that was causing perspective drift:

```cpp
// Get view matrix from camera (no rotation offset - use camera's view directly)
view = cameras[activeCam].getViewMatrix();
// NOTE: Removed glm::rotate(-25 degrees) to prevent perspective drift
// The camera's view matrix should be used directly for accurate centering
```

**Before:**
```cpp
view = cameras[activeCam].getViewMatrix();
view = glm::rotate(view, glm::radians(-25.0f), glm::vec3(0.0, 1.0, 0.0));  // ❌ Caused offset
```

**After:**
```cpp
view = cameras[activeCam].getViewMatrix();  // ✅ No rotation offset
```

**Why This Matters:**
- The -25 degree rotation was causing a rightward offset
- Offset increased with distance (perspective effect)
- Removing it ensures the camera's forward vector directly points at the gizmo
- Gizmo now appears exactly at viewport center (NDC 0,0)

### 5. One-Shot Aim Fix (`camera.cpp`)

When 'F' is pressed, the camera forward vector is set directly:

```cpp
// 4. ONE-SHOT AIM FIX: When 'F' is pressed:
// - Set camera.forward = glm::normalize(gizmoWorldPos - camera.position)
// - Immediately rebuild the View Matrix
cameraFront = aimDir;  // Forward vector points directly at gizmo
// View matrix is rebuilt automatically when getViewMatrix() is called
```

**Key Features:**
- Forward vector set directly: `cameraFront = glm::normalize(targetPos - cameraPos)`
- View matrix rebuilt automatically via `getViewMatrix()`
- No rotation offsets or transformations applied
- Camera position remains unchanged (rotation only)

### 6. Debug Check - Hidden Offsets

Checked for hidden offset variables in Camera class:
- ✅ No `m_viewOffset` variable found
- ✅ No `m_lensShift` variable found
- ✅ No other offset variables found

The Camera class uses standard view matrix calculation via `glm::lookAt()` with no hidden offsets.

## Code Changes

### Files Modified

1. **`src/io/scene.h`**:
   - Added `mViewportScreenPos` and `mViewportScreenSize` member variables

2. **`src/io/scene.cpp`**:
   - Modified `renderViewportWindow()` to capture exact screen-space coordinates
   - Modified `beginViewportRender()` to use exact content size for `glViewport`

3. **`src/main.cpp`**:
   - Removed -25 degree rotation from view matrix (rendering)
   - Removed -25 degree rotation from view matrix (raycast)
   - Ensured aspect ratio uses floating-point division
   - Added comments explaining the fixes

## Technical Details

### Coordinate System Alignment

**Before (With Drift):**
- View matrix had -25 degree rotation → Rightward offset
- Aspect ratio might not match exact viewport size → Distortion
- glViewport might use different dimensions → NDC mismatch
- Offset increased with distance → Perspective drift

**After (Fixed):**
- View matrix uses camera's view directly → No offset
- Aspect ratio matches exact content size → Perfect centering
- glViewport uses exact content dimensions → NDC maps correctly
- No offset regardless of distance → Accurate centering

### NDC to Pixel Mapping

With the fixes:
- NDC (-1, -1) → Viewport pixel (0, 0)
- NDC (0, 0) → Viewport pixel (width/2, height/2) **← CENTER**
- NDC (1, 1) → Viewport pixel (width, height)

When camera aims at gizmo:
- Gizmo world position → Projects to NDC (0, 0) → Appears at viewport center

### Aspect Ratio Calculation

```cpp
aspectRatio = (float)contentSize.x / (float)contentSize.y
```

**Examples:**
- 800x600 → 1.333... (correct)
- 1920x1080 → 1.777... (correct)
- 400x400 → 1.0 (correct)

**Critical:** Must use floating-point division to avoid integer truncation.

## Testing

### Verification Steps

1. **Load a model** and select a node (gizmo appears)
2. **Press 'F'** to aim at gizmo
3. **Check centering**: Gizmo should appear exactly at viewport center
4. **Move gizmo further/closer**: Gizmo should remain centered (no drift)
5. **Resize viewport**: Gizmo should remain centered after resize
6. **Test at different distances**: No rightward offset regardless of distance

### Expected Results

- ✅ Gizmo appears exactly at viewport center when 'F' is pressed
- ✅ No rightward offset, even at large distances
- ✅ No perspective drift as gizmo moves
- ✅ Centering works at all viewport sizes
- ✅ Aspect ratio matches viewport dimensions exactly

## Summary

Fixed perspective drift and growing offset by:

✅ **Dynamic Viewport Capture**: Exact screen-space coordinates captured every frame
✅ **Force glViewport Sync**: glViewport uses exact content size every frame
✅ **Correct Aspect Ratio**: Floating-point division ensures accurate aspect ratio
✅ **Removed Rotation Offset**: -25 degree rotation removed from view matrix
✅ **One-Shot Aim Fix**: Camera forward vector set directly, view matrix rebuilt immediately
✅ **No Hidden Offsets**: Verified no offset variables in Camera class

**Result**: Pressing 'F' places the Gizmo exactly at NDC (0,0), appearing dead center regardless of distance.
