# Skeleton Sphere Impostor Implementation

## Date
Created: 2026-02-23

## Overview
Replaced standard 2D points for skeleton joints with 3D shaded spheres using a "Sphere Impostor" technique in the fragment shader. This provides better visual feedback for joint positions with realistic 3D lighting, making skeleton visualization more intuitive and professional.

## Problem

Previously, skeleton joints were rendered as simple 2D points using `glPointSize()`, which:
- Appeared flat and unlit
- Provided no depth perception
- Looked less professional
- Made it harder to identify joint positions in 3D space

## Solution

Implemented a **Sphere Impostor** technique that renders joints as 3D shaded spheres using fragment shader calculations, without requiring actual 3D geometry.

### Sphere Impostor Technique

The Sphere Impostor technique uses:
1. **Point Primitives**: Each joint is rendered as a single point (`GL_POINTS`)
2. **Vertex Shader**: Sets `gl_PointSize` to create a screen-space quad
3. **Fragment Shader**: Calculates sphere geometry and lighting per-pixel using `gl_PointCoord`

**How It Works**:
- `gl_PointCoord` provides per-fragment coordinates [0,1] within the point quad
- Fragment shader maps these to a circle and calculates a fake 3D normal
- Simple directional lighting creates the 3D sphere appearance
- Circular discard creates the sphere silhouette

**IMPORTANT - OpenGL Compatibility Profile Requirement**:
- The engine runs in OpenGL Compatibility Profile (required for `glLineWidth` support)
- In Compatibility Profile, `gl_PointCoord` is **not automatically generated** for points
- **`GL_POINT_SPRITE` must be explicitly enabled** to generate `gl_PointCoord`
- Without `GL_POINT_SPRITE`, `gl_PointCoord` defaults to (0,0) and all fragments are discarded
- Both `GL_PROGRAM_POINT_SIZE` and `GL_POINT_SPRITE` are enabled before rendering and disabled after

## Implementation

### 1. Created Skeleton Shader Files

#### `Assets/skeleton.vert.glsl`
```glsl
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 model;
uniform float pointSize; // Point size for sphere impostor joints (linked to skeleton line width setting)

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    gl_PointSize = pointSize; // Dynamic point size based on skeleton line width setting
}
```

**Features**:
- Standard vertex transformation (model/view/projection)
- Uses `pointSize` uniform for dynamic point size (linked to skeleton line width setting)
- Works with both points (joints) and lines (bones)

#### `Assets/skeleton.frag.glsl`
```glsl
#version 330 core
out vec4 FragColor;

uniform vec3 color;
uniform bool isPoint; // True for joints (spheres), false for bones (lines)

void main() {
    if (isPoint) {
        // Map gl_PointCoord from [0,1] to [-1,1]
        vec2 coord = gl_PointCoord * 2.0 - 1.0;
        float r2 = dot(coord, coord);
        if (r2 > 1.0) discard; // Circular cutout
        
        // Fake 3D lighting (Sphere Impostor)
        float z = sqrt(1.0 - r2);
        vec3 normal = vec3(coord.x, -coord.y, z); // Invert Y for OpenGL
        vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0)); // Simple directional light
        float diff = max(dot(normal, lightDir), 0.0);
        
        // Add ambient and diffuse
        vec3 finalColor = color * (diff * 0.7 + 0.3);
        FragColor = vec4(finalColor, 1.0);
    } else {
        // Regular rendering for bone lines
        FragColor = vec4(color, 1.0);
    }
}
```

**Features**:
- **Dual Mode**: Handles both points (spheres) and lines (bones) via `isPoint` uniform
- **Circular Discard**: Creates sphere silhouette by discarding fragments outside radius
- **Fake 3D Normal**: Calculates sphere normal from 2D coordinates
- **Simple Lighting**: Directional light with ambient (0.3) + diffuse (0.7) components
- **Color Uniform**: Allows different colors for bones (green) and joints (red)

### 2. Added Skeleton Shader to Application

#### `src/application.h`
Added shader member:
```cpp
Shader m_skeletonShader;
```

#### `src/application.cpp`
**Initialization** (in `init()`):
```cpp
m_skeletonShader = Shader("Assets/skeleton.vert.glsl", "Assets/skeleton.frag.glsl");
```

**Usage** (in `renderFrame()`):
Changed skeleton rendering to use dedicated shader:
```cpp
m_modelManager.renderAllWithRootNodeTransforms(m_shader, m_uiData, m_Transforms, m_modelRootNodeMatrices, 
                                               view, projection, &m_skeletonShader);
```

### 3. Updated Skeleton Rendering

#### `src/graphics/model.cpp` (`Model::DrawSkeleton`)

**Key Changes**:

1. **Enable Program Point Size and Point Sprites** (at beginning):
   ```cpp
   glEnable(GL_PROGRAM_POINT_SIZE);
   glEnable(GL_POINT_SPRITE); // Required in Compatibility Profile to generate gl_PointCoord
   ```
   - `GL_PROGRAM_POINT_SIZE`: Allows vertex shader to control point size via `gl_PointSize`
   - `GL_POINT_SPRITE`: Required in Compatibility Profile to generate `gl_PointCoord` for fragment shader

2. **Bone Lines Rendering**:
   ```cpp
   // Set uniforms for bone lines (not points, green color)
   shader.setBool("isPoint", false);
   shader.set3Float("color", glm::vec3(0.0f, 1.0f, 0.0f)); // Green bones
   glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lineVertices.size()));
   ```

3. **Joint Spheres Rendering**:
   ```cpp
   // Set uniforms for joint spheres (points, red color with 3D shading)
   shader.setBool("isPoint", true);
   shader.set3Float("color", glm::vec3(1.0f, 0.0f, 0.0f)); // Red joints
   
   // Calculate point size proportional to skeleton line width setting
   // This links the joint sphere size to the bone line width for consistent visual scaling
   float lineWidth = AppSettingsManager::getInstance().getSettings().environment.skeletonLineWidth;
   // Base size of 8.0 plus a proportional increase based on the line width slider
   float calculatedPointSize = 8.0f + (lineWidth * 2.0f);
   shader.setFloat("pointSize", calculatedPointSize);
   
   glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(jointPoints.size()));
   ```

4. **Disable Program Point Size and Point Sprites** (at end):
   ```cpp
   glDisable(GL_PROGRAM_POINT_SIZE);
   glDisable(GL_POINT_SPRITE);
   ```
   Prevents state leakage to other render passes

## Technical Details

### Sphere Impostor Algorithm

1. **Coordinate Mapping**:
   - `gl_PointCoord` is [0,1] in point quad
   - Map to [-1,1]: `coord = gl_PointCoord * 2.0 - 1.0`
   - Calculate radius squared: `r2 = dot(coord, coord)`

2. **Circular Discard**:
   - If `r2 > 1.0`, fragment is outside circle → `discard`
   - Creates perfect circular silhouette

3. **Fake 3D Normal**:
   - Calculate Z: `z = sqrt(1.0 - r2)` (sphere equation)
   - Normal: `normal = vec3(coord.x, -coord.y, z)`
   - Y is inverted for OpenGL coordinate system

4. **Lighting Calculation**:
   - Directional light: `lightDir = normalize(vec3(1.0, 1.0, 1.0))`
   - Diffuse: `diff = max(dot(normal, lightDir), 0.0)`
   - Final: `color * (diff * 0.7 + 0.3)` (70% diffuse, 30% ambient)

### Shader Uniforms

- **`isPoint`** (bool): Toggles between sphere rendering (true) and line rendering (false)
- **`color`** (vec3): Base color for bones (green) or joints (red)
- **`pointSize`** (float): Point size for sphere impostor joints (linked to skeleton line width setting)
- **`model`** (mat4): Model matrix (identity for world-space vertices)
- **`view`** (mat4): View matrix (camera)
- **`projection`** (mat4): Projection matrix (perspective)

### Point Size

- **Dynamic Size**: `gl_PointSize = pointSize` (uniform, calculated from skeleton line width setting)
- **Formula**: `pointSize = 8.0 + (lineWidth * 2.0)`
  - When `lineWidth = 1.0`: `pointSize = 10.0`
  - When `lineWidth = 2.0`: `pointSize = 12.0` (default)
  - When `lineWidth = 10.0`: `pointSize = 28.0`
- **Screen-Space**: Size is constant regardless of distance
- **Linked to Line Width**: Joint size scales proportionally with bone line thickness for consistent visual appearance
- **Trade-off**: Larger joints are easier to see, but don't scale with distance

## Benefits

1. **Better Visual Feedback**: 3D shaded spheres provide depth perception
2. **Professional Appearance**: Realistic lighting makes skeleton visualization more polished
3. **Easy to Identify**: Spheres stand out more than flat points
4. **Performance**: No additional geometry required (uses point primitives)
5. **Flexible**: Can easily adjust colors, size, and lighting

## Files Created/Modified

### New Files
1. **`Assets/skeleton.vert.glsl`** - Vertex shader for skeleton rendering
2. **`Assets/skeleton.frag.glsl`** - Fragment shader with sphere impostor logic

### Modified Files
1. **`src/application.h`** - Added `m_skeletonShader` member
2. **`src/application.cpp`** - Initialize skeleton shader, pass to render function
3. **`src/graphics/model.cpp`** - Updated `DrawSkeleton` to use new shader with sphere impostor

## Usage

### Visual Result
- **Bones**: Green lines (unchanged, but now use dedicated shader)
- **Joints**: Red 3D shaded spheres (replaces flat points)
- **Lighting**: Simple directional light from top-right creates realistic shading

### Customization

**Point Size** (controlled via Skeleton Line Width setting):
- Adjust the "Skeleton Line Width" slider in Viewport Settings
- Joint sphere size scales automatically: `pointSize = 8.0 + (lineWidth * 2.0)`
- Formula can be adjusted in `model.cpp` if different scaling is desired

**Joint Color** (in `model.cpp`):
```cpp
shader.set3Float("color", glm::vec3(1.0f, 0.0f, 0.0f)); // Red joints
```

**Lighting** (in `skeleton.frag.glsl`):
```glsl
vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0)); // Adjust light direction
float diff = max(dot(normal, lightDir), 0.0);
vec3 finalColor = color * (diff * 0.7 + 0.3); // Adjust ambient/diffuse ratio
```

## Performance Considerations

- **No Geometry Overhead**: Uses point primitives (1 vertex per joint)
- **Fragment Shader Cost**: Sphere calculation is simple (sqrt, dot product)
- **Discard Efficiency**: Early discard for fragments outside circle
- **Fixed Point Size**: Screen-space size means consistent performance

## Future Enhancements

Potential improvements:
- **Distance-Based Scaling**: Adjust point size based on camera distance
- **Configurable Colors**: Allow user to customize bone/joint colors
- **Multiple Light Sources**: Add more complex lighting (point lights, etc.)
- **Animated Joints**: Pulse or highlight selected joints
- **Joint Labels**: Render joint names as text overlays
- **Size Slider**: User-configurable joint sphere size (like line width)

## Related Features

- **Skeleton Line Width**: Controls bone line thickness (separate from joint rendering)
- **Skeleton Visibility**: Global toggle for showing/hiding skeletons
- **Bone Picking**: Click-to-select bones in viewport (works with sphere joints)

## Testing

### Test Case 1: Basic Rendering
1. Load a model with skeleton
2. Enable "Show Skeletons" for the model
3. **Expected**: Joints appear as red 3D shaded spheres, bones as green lines

### Test Case 2: Lighting
1. Rotate camera around skeleton
2. **Expected**: Sphere shading changes with viewing angle (3D appearance)

### Test Case 3: Multiple Models
1. Load multiple models with skeletons
2. Enable skeletons for all
3. **Expected**: All joints render as spheres, no visual artifacts

### Test Case 4: Performance
1. Load model with many joints (100+)
2. Enable skeleton
3. **Expected**: Smooth rendering with no frame rate drops

## Notes

- Sphere impostor technique is a common optimization for rendering many spheres
- No actual 3D geometry is created (uses fragment shader calculations)
- Point size is fixed at 12 pixels (screen-space, not world-space)
- Lighting is simplified (single directional light) for performance
- Works with OpenGL Compatibility Profile (required for `glLineWidth`)
