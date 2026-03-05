# Camera Aim Constraint Implementation

## Timestamp
Created: 2026-02-16 14:30
Last Updated: 2026-02-16 14:30

## Overview

This document describes the implementation of the camera aim constraint system that ensures characters are always centered in the viewport when pressing the 'F' key. The aim constraint projects the character's world position to screen space and adjusts the camera's yaw and pitch to center the target in the viewport.

## Problem Statement

Previously, when pressing 'F' to focus on a character:
- The character would sometimes not be visible at all
- The character would appear on the side of the frame instead of centered
- The camera would focus on the bounding box center, but the character might not be centered in the viewport
- The issue was that the camera's yaw/pitch were not adjusted to ensure viewport centering

## Solution

The aim constraint system:
1. Projects the character's world position to screen space
2. Calculates the offset from the viewport center
3. Adjusts the camera's yaw and pitch to minimize this offset
4. Ensures the character is always centered when F is pressed

## Implementation Details

### Location
- **File**: `src/io/camera.cpp`
- **Method**: `Camera::centerTargetInViewport()`
- **Called from**: `Scene::updateCameraTracking()` when framing is active

### Algorithm

1. **Project to Screen Space**
   ```cpp
   glm::vec4 clipPos = projectionMatrix * viewMatrix * glm::vec4(worldTarget, 1.0f);
   ```
   - Projects the world target position through the view and projection matrices
   - Results in clip space coordinates

2. **Perspective Divide**
   ```cpp
   clipPos.x /= clipPos.w;
   clipPos.y /= clipPos.w;
   ```
   - Converts clip space to NDC (Normalized Device Coordinates) ranging from -1 to 1

3. **Convert to Screen Space**
   ```cpp
   float screenX = (clipPos.x + 1.0f) * 0.5f * viewportWidth;
   float screenY = (1.0f - clipPos.y) * 0.5f * viewportHeight;
   ```
   - Converts NDC to screen pixel coordinates
   - Y is flipped because screen coordinates are top-down

4. **Calculate Offset from Center**
   ```cpp
   float offsetX = screenX - centerX;
   float offsetY = screenY - centerY;
   ```
   - Calculates pixel offset from viewport center

5. **Convert to Angular Offset**
   ```cpp
   float horizontalFOV = 2.0f * atan(tan(fovRadians * 0.5f) * aspectRatio);
   float angularOffsetX = (offsetX / viewportWidth) * horizontalFOV;
   float angularOffsetY = (offsetY / viewportHeight) * fovRadians;
   ```
   - Converts pixel offset to angular offset in radians
   - Accounts for aspect ratio for horizontal FOV

6. **Adjust Camera Yaw/Pitch**
   ```cpp
   yaw -= angularOffsetX * smoothingFactor;
   pitch -= angularOffsetY * smoothingFactor;
   ```
   - Adjusts camera rotation to center the target
   - Uses smoothing factor (0.4f) for smooth convergence

7. **Update Camera Position**
   ```cpp
   cameraPos = focusPoint - direction * orbitDistance;
   ```
   - Maintains orbit distance while adjusting rotation
   - Updates camera vectors

### Integration with Framing System

The aim constraint is integrated into the existing framing system:

```cpp
// In Scene::updateCameraTracking()
if (mIsFraming && hasSelection) {
    ImVec2 viewportSize = getViewportWindowSize();
    if (viewportSize.x > 0.0f && viewportSize.y > 0.0f) {
        camera.centerTargetInViewport(targetPos, mViewMatrix, mProjectionMatrix, 
                                     viewportSize.x, viewportSize.y);
    }
}
```

The aim constraint is applied:
- Only when framing is active (`mIsFraming == true`)
- Only when a character is selected (`hasSelection == true`)
- Only when viewport size is valid

## Mathematical Details

### Screen Space Projection

The projection from world space to screen space follows the standard graphics pipeline:

1. **World Space → View Space**: `viewPos = viewMatrix * worldPos`
2. **View Space → Clip Space**: `clipPos = projectionMatrix * viewPos`
3. **Clip Space → NDC**: `ndcPos = clipPos.xy / clipPos.w`
4. **NDC → Screen Space**: 
   - `screenX = (ndcPos.x + 1.0) * 0.5 * viewportWidth`
   - `screenY = (1.0 - ndcPos.y) * 0.5 * viewportHeight`

### Angular Offset Calculation

The angular offset is calculated based on the camera's field of view:

- **Vertical FOV**: `fovRadians = glm::radians(zoom)`
- **Horizontal FOV**: `horizontalFOV = 2.0 * atan(tan(fovRadians * 0.5) * aspectRatio)`
- **Angular Offset X**: `angularOffsetX = (offsetX / viewportWidth) * horizontalFOV`
- **Angular Offset Y**: `angularOffsetY = (offsetY / viewportHeight) * fovRadians`

### Smoothing Factor

The smoothing factor (0.4f) controls how quickly the camera converges to center the target:
- **Lower values (0.1-0.2)**: Slower convergence, smoother but may take longer
- **Higher values (0.4-0.5)**: Faster convergence, more responsive
- **Current value (0.4)**: Balanced for smooth, responsive centering

## Benefits

1. **Consistent Centering**: Character is always centered in viewport when F is pressed
2. **Smooth Convergence**: Smooth interpolation prevents jarring camera movements
3. **Aspect Ratio Aware**: Correctly handles different viewport aspect ratios
4. **FOV Aware**: Accounts for camera field of view in calculations
5. **Non-Intrusive**: Only active during framing, doesn't interfere with manual camera control

## Usage

1. Select a character (via Outliner or viewport click)
2. Press 'F' key
3. Camera automatically frames and centers the character
4. Character is guaranteed to be centered in the viewport

## Edge Cases

### Target Behind Camera
- If `clipPos.w <= 0.0f`, the target is behind the camera
- The aim constraint returns early without adjusting
- Prevents incorrect camera rotation

### Invalid Viewport Size
- Checks that `viewportWidth > 0` and `viewportHeight > 0`
- Only applies aim constraint when viewport is valid

### Gimbal Lock Prevention
- Pitch is clamped to -89° to 89°
- Prevents camera from flipping upside down

## Performance Considerations

- **Per-Frame Cost**: O(1) - simple matrix multiplication and arithmetic
- **Only Active During Framing**: No performance impact when not framing
- **Smooth Convergence**: Multiple frames to converge, but each frame is fast

## Related Features

- **Character-Level Focus (F Key)**: Uses aim constraint for centering
- **Camera Tracking (T Key)**: Can be combined with aim constraint for continuous centering
- **Viewport Selection**: Works with viewport-based character selection

## Future Enhancements

### Potential Improvements

1. **Continuous Centering**: Apply aim constraint during tracking (T key) for continuous centering
2. **Configurable Smoothing**: Allow users to adjust smoothing factor
3. **Dead Zone**: Add a dead zone where small offsets don't trigger adjustment
4. **Distance-Based Smoothing**: Adjust smoothing based on distance to target

## Technical Notes

### Coordinate Systems

- **World Space**: Character position in 3D scene
- **View Space**: Position relative to camera
- **Clip Space**: Position after projection matrix
- **NDC Space**: Normalized device coordinates (-1 to 1)
- **Screen Space**: Pixel coordinates in viewport

### Matrix Order

The projection uses the standard OpenGL matrix order:
- `projectionMatrix * viewMatrix * worldPos`
- This is equivalent to: `projectionMatrix * (viewMatrix * worldPos)`

### Y-Axis Flipping

Screen Y coordinates are flipped because:
- NDC Y increases upward (bottom to top)
- Screen Y increases downward (top to bottom)
- Formula: `screenY = (1.0 - ndcY) * 0.5 * viewportHeight`

## Summary

The camera aim constraint system ensures that when pressing 'F' to focus on a character, the character is always centered in the viewport. By projecting the character's world position to screen space and adjusting the camera's yaw and pitch to minimize the offset from viewport center, the system provides consistent, reliable character framing. The implementation is efficient, non-intrusive, and integrates seamlessly with the existing framing system.
