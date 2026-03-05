# Gradient Background Feature (v2.0.0)

## Date
Created: 2026-02-23

## Overview
Implemented a professional gradient background system for the viewport, allowing users to customize the background with a smooth vertical gradient transition. This feature enhances the visual presentation of the 3D viewport and provides a more polished, professional appearance.

**Rendering Pipeline**: The gradient background is rendered as a dedicated screen-space pass using Normalized Device Coordinates (NDC) from (-1, -1) to (1, 1), ensuring full viewport coverage. Depth testing and depth writing are disabled during background rendering to maintain scene integrity.

## Changes Made

### 1. Settings Structure (`src/io/app_settings.h`)

**Added to `EnvironmentSettings`**:
- `glm::vec3 viewportGradientTop = glm::vec3(0.2f, 0.2f, 0.2f)` - Top color for gradient background
- `glm::vec3 viewportGradientBottom = glm::vec3(0.1f, 0.1f, 0.1f)` - Bottom color for gradient background
- `bool useGradient = true` - Toggle to enable/disable gradient background

**Default Values**:
- Top color: Dark gray (0.2, 0.2, 0.2)
- Bottom color: Darker gray (0.1, 0.1, 0.1)
- Gradient enabled by default

### 2. Settings Serialization (`src/io/app_settings.cpp`)

**Loading**:
- Added JSON deserialization for `viewportGradientTop`, `viewportGradientBottom`, and `useGradient`
- Settings are loaded from `app_settings.json` on application startup

**Saving**:
- Added JSON serialization for all three gradient settings
- Settings are automatically saved when modified via UI

### 3. Gradient Rendering (`src/io/scene.cpp`)

**New Method: `drawBackgroundGradient()`**:
- Renders a full-screen gradient using OpenGL immediate mode (Compatibility Profile)
- Uses `GL_TRIANGLE_STRIP` for efficient quad rendering
- Transitions vertically from top color to bottom color
- Uses NDC coordinates (-1 to 1) to cover entire viewport
- Disables depth testing during rendering (background always behind)
- Resets color state after rendering to avoid affecting other objects

**Implementation Details**:
```cpp
void Scene::drawBackgroundGradient() {
    // Get colors from settings
    glm::vec3 topColor = settings.environment.viewportGradientTop;
    glm::vec3 bottomColor = settings.environment.viewportGradientBottom;
    
    // Proper OpenGL state management to prevent rendering corruption
    // Disable depth testing and depth writing so background is always behind everything
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);  // Don't write to depth buffer
    
    // Draw quad with gradient
    glBegin(GL_TRIANGLE_STRIP);
    glColor3f(topColor.r, topColor.g, topColor.b);
    glVertex2f(-1.0f, 1.0f);  // Top-left
    glColor3f(bottomColor.r, bottomColor.g, bottomColor.b);
    glVertex2f(-1.0f, -1.0f); // Bottom-left
    glColor3f(topColor.r, topColor.g, topColor.b);
    glVertex2f(1.0f, 1.0f);   // Top-right
    glColor3f(bottomColor.r, bottomColor.g, bottomColor.b);
    glVertex2f(1.0f, -1.0f);  // Bottom-right
    glEnd();
    
    // Re-enable depth writing and depth testing for subsequent rendering
    glDepthMask(GL_TRUE);   // Re-enable depth writing
    glEnable(GL_DEPTH_TEST);  // Re-enable depth testing
    glColor3f(1.0f, 1.0f, 1.0f);  // Reset color
}
```

**Updated `beginViewportRender()`**:
- Clears both color and depth buffers first (ensures clean slate)
- Checks `useGradient` setting
- If enabled: Calls `drawBackgroundGradient()` (rendered as screen-space pass)
- If disabled: Uses traditional `glClearColor()` with solid color (already cleared above)
- Enables depth testing after background rendering for subsequent 3D objects

### 4. UI Controls (`src/io/ui/viewport_settings_panel.cpp`)

**Added to "Environment Color" Section**:
- **Checkbox**: "Use Gradient Background" - Toggles gradient on/off
- **ColorEdit3**: "Gradient Top" - Controls top color of gradient
- **ColorEdit3**: "Gradient Bottom" - Controls bottom color of gradient
- **Help Text**: Explains gradient behavior

**Conditional UI**:
- When gradient is enabled: Shows gradient color controls
- When gradient is disabled: Shows traditional solid color picker with alpha control

**Preset Color Logic (Context-Aware)**:
- Preset color buttons (Dark Gray, Light Gray, Black, White, Default) are **context-aware**
- **When Gradient Mode is DISABLED**: Presets apply as solid colors (traditional behavior)
- **When Gradient Mode is ENABLED**: Presets apply as gradient color pairs:
  - **Dark Gray**: Top (0.2, 0.2, 0.2) → Bottom (0.1, 0.1, 0.1)
  - **Light Gray**: Top (0.45, 0.55, 0.60) → Bottom (0.25, 0.35, 0.40)
  - **Black**: Top (0.05, 0.05, 0.05) → Bottom (0.0, 0.0, 0.0)
  - **White**: Top (1.0, 1.0, 1.0) → Bottom (0.8, 0.8, 0.8)
  - **Default**: Restores original project default gradient (Top: 0.2, 0.2, 0.2 → Bottom: 0.1, 0.1, 0.1)
- Presets **do not** change the gradient mode toggle - they respect the current mode
- Tooltip explains context-aware behavior when hovering over "Preset Colors:" label

**User Experience**:
- Real-time preview of gradient changes
- Settings automatically saved on modification
- Smooth transition between gradient and solid color modes
- Preset colors work correctly with gradient toggle

### 5. Robustness Improvements

**Framebuffer Resize (`resizeViewportFBO`)**:
- Already had robustness check: `if (width < 1 || height < 1)`
- Ensures valid dimensions before texture allocation
- Prevents OpenGL errors from invalid texture sizes

**Documentation**:
- Added Doxygen comments to `drawBackgroundGradient()`
- Explained implementation details and OpenGL state management
- Documented NDC coordinate usage

## Technical Details

### OpenGL Compatibility Profile

The gradient rendering uses OpenGL immediate mode (`glBegin`/`glEnd`), which is:
- Available in Compatibility Profile (required for `glLineWidth` support)
- Efficient for simple full-screen quads
- No shader compilation required
- Automatic color interpolation between vertices

### Rendering Order

1. **Buffer Clear**: Both color and depth buffers cleared first (clean slate)
2. **Background Rendering** (screen-space pass):
   - If gradient enabled: `drawBackgroundGradient()` 
     - Depth testing disabled (`glDisable(GL_DEPTH_TEST)`)
     - Depth writing disabled (`glDepthMask(GL_FALSE)`)
     - Ensures background is always behind 3D objects
   - If gradient disabled: Solid color already set via `glClearColor()`
3. **Depth Testing Enabled**: Re-enabled for 3D scene rendering
4. **3D Scene Rendering**: Grid, models, bounding boxes, skeletons (depth test enabled)

### Color Interpolation

OpenGL automatically interpolates colors between vertices:
- Top-left and top-right: Top color
- Bottom-left and bottom-right: Bottom color
- Vertical interpolation creates smooth gradient transition

### Performance

- **Minimal Overhead**: Single quad draw call per frame
- **No Shader Compilation**: Uses fixed-function pipeline
- **Efficient State Changes**: Only disables/enables depth test
- **No Texture Lookups**: Direct color values

## Benefits

### 1. Visual Enhancement
- **Professional Appearance**: Gradient backgrounds are common in professional 3D software
- **Better Contrast**: Gradient can improve visibility of 3D models
- **Customizable**: Users can match their preferred color scheme

### 2. User Experience
- **Easy Toggle**: Simple checkbox to enable/disable
- **Real-time Preview**: Changes visible immediately
- **Persistent Settings**: Preferences saved between sessions

### 3. Code Quality
- **Clean Implementation**: Single method for gradient rendering
- **Well-Documented**: Doxygen comments explain implementation
- **Maintainable**: Clear separation between gradient and solid color modes

## Usage

### Enabling Gradient Background

1. Open **Viewport Settings** panel
2. Navigate to **"Environment Color"** section
3. Check **"Use Gradient Background"**
4. Adjust **"Gradient Top"** and **"Gradient Bottom"** colors
5. Settings are automatically saved

### Disabling Gradient Background

1. Uncheck **"Use Gradient Background"**
2. Traditional solid color picker appears
3. Adjust **"Background Color"** and **"Alpha"** as needed

### Recommended Color Schemes

**Dark Professional** (Default):
- Top: (0.2, 0.2, 0.2) - Dark gray
- Bottom: (0.1, 0.1, 0.1) - Darker gray

**Light Studio**:
- Top: (0.7, 0.7, 0.7) - Light gray
- Bottom: (0.5, 0.5, 0.5) - Medium gray

**Sky Blue**:
- Top: (0.5, 0.7, 0.9) - Light blue
- Bottom: (0.2, 0.4, 0.6) - Dark blue

## Preset Colors Enhancement (v2.0.0 Update)

### Context-Aware Preset System

The preset color buttons now intelligently adapt to the current background mode:

**Solid Color Mode** (Gradient disabled):
- Presets apply as single solid colors (unchanged behavior)
- Compatible with existing workflow

**Gradient Mode** (Gradient enabled):
- Presets apply as carefully designed gradient pairs
- Each preset provides a visually pleasing top-to-bottom transition
- Maintains the same preset names for consistency

**User Experience**:
- Tooltip on "Preset Colors:" label explains context-aware behavior
- No mode switching required - presets respect current setting
- Seamless transition between solid and gradient modes

## Related Files
- `src/io/app_settings.h` - Settings structure definition
- `src/io/app_settings.cpp` - Settings serialization/deserialization
- `src/io/scene.h` - Scene class declaration
- `src/io/scene.cpp` - Gradient rendering implementation (shader-based in v2.0.0)
- `src/io/ui/viewport_settings_panel.cpp` - UI controls with context-aware presets

## Future Improvements

1. **Horizontal Gradient**: Option to switch between vertical and horizontal gradients
2. **Multi-Color Gradient**: Support for 3+ color stops
3. **Animated Gradient**: Optional subtle animation for dynamic backgrounds
