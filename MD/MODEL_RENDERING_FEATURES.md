# Model Rendering Features

## Date
Created: 2026-02-23

## Overview
Added several per-model rendering features to improve model visualization and debugging capabilities. These features include texture fallback handling, default material override, wireframe mode, and normal flipping. All features are accessible through the Property Panel and provide per-model control.

## Features Implemented

### 1. Texture Fallback System
**Purpose**: Handle missing or failed textures gracefully by providing a grey fallback color instead of rendering black models.

**Implementation**:
- **Texture Loading**: Modified `Texture::load()` to return `bool` indicating success/failure
- **Failed Texture Handling**: When `stbi_load()` fails, the texture ID is set to 0 and the texture is not added to the mesh
- **Shader Fallback**: Fragment shader uses grey color (`vec3(0.65, 0.65, 0.65)`) when no valid diffuse texture is available
- **Validation**: Mesh rendering checks for valid texture IDs (`id > 0`) before considering a texture as available

**Files Modified**:
- `src/graphics/texture.cpp` - Added return value and failure handling
- `src/graphics/texture.h` - Updated method signature
- `src/graphics/model.cpp` - Only adds textures with valid IDs
- `src/graphics/mesh.cpp` - Validates texture IDs before setting shader uniform
- `Assets/fragment.glsl` - Implements grey fallback in lighting calculations

**Behavior**:
- **Valid Texture**: Model renders with loaded texture
- **Missing Texture**: Model renders with grey fallback color (integrated into lighting)
- **Failed Texture Load**: Texture is not added to mesh, fallback is used

### 2. Default Material Override
**Purpose**: Force models to render in grey color regardless of textures, useful for inspecting geometry and skinning without texture distractions.

**Implementation**:
- **Model Flag**: Added `bool m_useDefaultMaterial = false;` to `Model` class
- **UI Control**: Checkbox in Property Panel labeled "Default Material (Grey)"
- **Shader Uniform**: `useDefaultMaterial` uniform passed to fragment shader
- **Shader Logic**: When enabled, forces `baseColor = vec3(0.65, 0.65, 0.65)` regardless of texture availability

**Files Modified**:
- `src/graphics/model.h` - Added `m_useDefaultMaterial` member
- `src/graphics/model.cpp` - Passes uniform to shader
- `src/io/ui/property_panel.cpp` - Added checkbox UI
- `Assets/fragment.glsl` - Implements override logic

**Usage**:
1. Select a model in the Outliner
2. Open Property Panel
3. Check "Default Material (Grey)" checkbox
4. Model renders in grey, ignoring all textures

### 3. Wireframe Mode
**Purpose**: Render models as wireframes to inspect geometry topology, edge flow, and mesh structure.

**Implementation**:
- **Model Flag**: Added `bool m_wireframeMode = false;` to `Model` class
- **UI Control**: Checkbox in Property Panel labeled "Wireframe"
- **OpenGL State**: Uses `glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)` for wireframe rendering
- **State Management**: Always resets to `GL_FILL` after rendering to prevent affecting other objects (UI, grid, etc.)

**Files Modified**:
- `src/graphics/model.h` - Added `m_wireframeMode` member
- `src/graphics/model.cpp` - Sets/resets polygon mode in `render()` method
- `src/io/ui/property_panel.cpp` - Added checkbox UI

**Usage**:
1. Select a model in the Outliner
2. Open Property Panel
3. Check "Wireframe" checkbox
4. Model renders as wireframe (edges only)

**Technical Details**:
- Polygon mode is set before mesh rendering loop
- Always reset to `GL_FILL` at the end of `render()` method
- Prevents wireframe mode from affecting ImGui UI or other rendered objects

### 4. Flip Normals
**Purpose**: Fix models with inverted normals that appear completely black due to incorrect lighting calculations.

**Implementation**:
- **Model Flag**: Added `bool m_flipNormals = false;` to `Model` class
- **UI Control**: Checkbox in Property Panel labeled "Flip Normals"
- **Shader Uniform**: `flipNormals` uniform passed to fragment shader
- **Shader Logic**: When enabled, negates the normal vector before lighting calculations

**Files Modified**:
- `src/graphics/model.h` - Added `m_flipNormals` member
- `src/graphics/model.cpp` - Passes uniform to shader
- `src/io/ui/property_panel.cpp` - Added checkbox UI
- `Assets/fragment.glsl` - Implements normal flipping

**Usage**:
1. Select a model that appears black
2. Open Property Panel
3. Check "Flip Normals" checkbox
4. Model should render correctly with proper lighting

## Files Created

None (all features added to existing files)

## Files Modified

### 1. `src/graphics/model.h`
**Added Members**:
```cpp
bool m_flipNormals = false;              // Flip normals flag
bool m_useDefaultMaterial = false;        // Default material override
bool m_wireframeMode = false;             // Wireframe rendering mode
```

### 2. `src/graphics/model.cpp`
**Changes in `render()` method**:
- Added `shader.setInt("flipNormals", m_flipNormals ? 1 : 0);`
- Added `shader.setInt("useDefaultMaterial", m_useDefaultMaterial ? 1 : 0);`
- Added polygon mode setting before mesh loop
- Added polygon mode reset after mesh loop

### 3. `src/graphics/texture.cpp`
**Changes in `load()` method**:
- Changed return type from `void` to `bool`
- Returns `true` on successful load, `false` on failure
- Deletes texture ID and sets to 0 on failure
- Proper error logging with `LOG_ERROR()`

### 4. `src/graphics/texture.h`
**Changes**:
- Updated `load()` signature to return `bool`

### 5. `src/graphics/model.cpp` (loadTextures method)
**Changes**:
- Only adds textures to vector if `load()` returns `true` and `id > 0`
- Updated duplicate check to only reuse textures with valid IDs

### 6. `src/graphics/mesh.cpp`
**Changes in `render()` method**:
- Validates texture IDs before setting `hasDiffuseTexture` uniform
- Checks for `tex.id > 0` in addition to texture type

### 7. `src/io/ui/property_panel.cpp`
**Added UI Controls**:
- "Flip Normals" checkbox (below Transforms section)
- "Default Material (Grey)" checkbox (below Flip Normals)
- "Wireframe" checkbox (below Default Material)
- All checkboxes log toggle actions to Logger

### 8. `Assets/fragment.glsl`
**Added Uniforms**:
```glsl
uniform int flipNormals;
uniform int useDefaultMaterial;
uniform int hasDiffuseTexture;
```

**Updated baseColor Logic**:
```glsl
vec3 baseColor;
if (useDefaultMaterial == 1) {
    baseColor = vec3(0.65, 0.65, 0.65); // Forced Maya grey
} else if (hasDiffuseTexture == 1) {
    baseColor = texture(diffuse0, TextCoord).rgb; // Normal texture
} else {
    baseColor = vec3(0.65, 0.65, 0.65); // Fallback: Grey for missing texture
}
```

**Normal Flipping**:
```glsl
vec3 norm = normalize(Normal);
if (flipNormals == 1) {
    norm = -norm;
}
```

## Architecture

### Per-Model Control
All features are implemented as per-model flags, allowing independent control for each loaded model:
- Each `Model` instance has its own flags
- UI checkboxes update the selected model only
- Shader uniforms are set per-model before rendering

### State Management
- **Wireframe Mode**: OpenGL state is properly managed (set before rendering, reset after)
- **Texture Validation**: Failed textures are not added to meshes, preventing invalid rendering
- **Shader Uniforms**: All flags are passed as integers (0/1) for shader compatibility

### Error Handling
- **Texture Loading**: Failed loads are logged with `LOG_ERROR()` and don't crash the application
- **Fallback Rendering**: Missing textures gracefully fall back to grey instead of black
- **State Isolation**: Wireframe mode doesn't affect other rendered objects

## Usage Examples

### Fixing a Black Model
```cpp
// User selects model in Outliner
// Opens Property Panel
// Checks "Flip Normals" checkbox
// Model now renders correctly
```

### Inspecting Geometry
```cpp
// User selects model in Outliner
// Opens Property Panel
// Checks "Default Material (Grey)" checkbox
// Checks "Wireframe" checkbox
// Model renders as grey wireframe for clean geometry inspection
```

### Handling Missing Textures
```cpp
// Model loads with missing texture files
// Texture loading fails, texture not added to mesh
// Shader detects no valid texture (hasDiffuseTexture == 0)
// Model renders with grey fallback color
// User can still see model structure and lighting
```

## Benefits

### 1. Better Error Handling
- Missing textures don't result in completely black models
- Failed texture loads are logged and handled gracefully
- Models remain visible even with texture issues

### 2. Improved Debugging
- Wireframe mode helps inspect mesh topology
- Default material mode removes texture distractions
- Flip normals fixes lighting issues quickly

### 3. User Experience
- All features accessible through intuitive checkboxes
- Per-model control allows independent settings
- Immediate visual feedback when toggling features

### 4. Professional Workflow
- Mimics Maya's Shaded vs Textured modes
- Grey fallback provides consistent appearance
- Wireframe mode standard in 3D tools

## Technical Details

### Texture Validation Flow
1. `Texture::load()` attempts to load image file
2. If `stbi_load()` fails, returns `false` and sets `id = 0`
3. `Model::loadTextures()` only adds textures with `id > 0`
4. `Mesh::render()` checks `id > 0` before setting shader uniform
5. Shader uses fallback if `hasDiffuseTexture == 0`

### Shader Color Logic Priority
1. **Highest**: `useDefaultMaterial == 1` → Forces grey
2. **Medium**: `hasDiffuseTexture == 1` → Uses texture
3. **Lowest**: Default → Grey fallback

### Polygon Mode Management
- Set before mesh rendering: `glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)`
- Reset after mesh rendering: `glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)`
- Prevents wireframe from affecting:
  - ImGui UI elements
  - Grid rendering
  - Bounding boxes
  - Other models

## Related Features

### Logger System
- All UI actions are logged using `LOG_INFO()` macro
- Texture loading failures are logged using `LOG_ERROR()` macro
- Logs appear in Logger / Info panel with color-coding

### Property Panel
- All checkboxes are grouped together below Transforms section
- Checkboxes only appear when a model is selected
- Toggle actions are logged for debugging

## Future Enhancements

Potential improvements:
- Add texture preview in Property Panel
- Add normal visualization mode
- Add material editor for custom colors
- Add texture reload functionality
- Add batch operations (apply to all models)
- Add preset configurations (save/load model rendering settings)

## Related Files

- `src/graphics/model.h` - Model class with rendering flags
- `src/graphics/model.cpp` - Model rendering implementation
- `src/graphics/texture.h` - Texture class definition
- `src/graphics/texture.cpp` - Texture loading implementation
- `src/graphics/mesh.cpp` - Mesh rendering with texture validation
- `src/io/ui/property_panel.cpp` - UI controls for rendering features
- `Assets/fragment.glsl` - Fragment shader with fallback logic
