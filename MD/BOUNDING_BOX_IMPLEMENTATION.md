# Bounding Box Implementation

## Overview

The bounding box feature provides visual wireframe boxes around each loaded 3D model in the viewport. Bounding boxes automatically update in real-time to reflect changes in model geometry due to animation, deformation, or manual transformations.

## Features

- **Automatic Calculation**: Each loaded model/character automatically receives its own bounding box
- **Real-Time Updates**: Bounding boxes update every frame to reflect current animation/deformation state
- **Efficient Rendering**: Uses GL_LINES for wireframe rendering (same technique as the grid)
- **Visual Feedback**: Yellow wireframe boxes provide clear spatial reference
- **Proper Depth Testing**: Bounding boxes render correctly in 3D space

## Architecture

### File Structure

```
src/graphics/
├── bounding_box.h          # BoundingBox class header
├── bounding_box.cpp        # BoundingBox class implementation
└── model_manager.h         # ModelInstance structure (contains BoundingBox)

Assets/
├── bounding_box.vert.glsl  # Vertex shader for bounding boxes
└── bounding_box.frag.glsl  # Fragment shader for bounding boxes
```

### Class Hierarchy

```
ModelInstance
└── BoundingBox (member variable)
    ├── Stores min/max corners
    ├── Manages OpenGL resources (VAO/VBO)
    └── Handles rendering
```

## Implementation Details

### 1. BoundingBox Class

The `BoundingBox` class encapsulates all bounding box functionality:

**Key Methods:**
- `init()` - Initialize OpenGL resources (VAO/VBO)
- `update(min, max)` - Update bounding box geometry from new min/max values
- `render()` - Render the bounding box as wireframe
- `generateVertices()` - Generate 12 edges (24 vertices) for the box

**Data Structure:**
- 8 corners defining the box
- 12 edges connecting the corners:
  - 4 edges on bottom face
  - 4 edges on top face
  - 4 vertical edges

### 2. Real-Time Updates

Bounding boxes update every frame in the main render loop:

```cpp
// Calculate bone transforms for current animation state
std::vector<glm::mat4> modelTransforms;
instance->model.BoneTransform(instance->animationTime, modelTransforms, ui_data);

// Get bounding box with bone transformations applied
glm::vec3 min, max;
instance->model.getBoundingBoxWithBones(min, max, modelTransforms);

// Update bounding box geometry
instance->boundingBox.update(min, max);
```

### 3. Bone-Aware Bounding Box Calculation

The `Model::getBoundingBoxWithBones()` method calculates bounding boxes considering current bone transformations:

1. **Iterate through all meshes** in the model
2. **For each vertex:**
   - Get original vertex position
   - Apply bone transformations (weighted blend of up to 4 bones)
   - Transform vertex using bone transform matrix
   - Apply model transform (pos and size)
3. **Calculate min/max** from all transformed vertices

This ensures the bounding box accurately reflects:
- Animation poses
- Bone deformations
- Manual bone rotations/translations/scales
- Model position and scale

### 4. Rendering Pipeline

```
Main Loop (main.cpp)
    ↓
For each ModelInstance:
    ├─ Calculate bone transforms (BoneTransform)
    ├─ Calculate bounding box with bones (getBoundingBoxWithBones)
    ├─ Update bounding box geometry (boundingBox.update)
    └─ Render bounding box (boundingBox.render)
```

## Usage

### Automatic Initialization

When a model is loaded:

1. `ModelManager::loadModel()` creates a new `ModelInstance`
2. `ModelInstance::initializeBoundingBox()` is called automatically
3. Bounding box is calculated from model geometry
4. OpenGL resources are initialized (VAO/VBO)

### Manual Control

Bounding boxes can be enabled/disabled:

```cpp
instance->boundingBox.setEnabled(true);   // Show bounding box
instance->boundingBox.setEnabled(false);  // Hide bounding box
```

Color can be customized:

```cpp
instance->boundingBox.setColor(glm::vec3(1.0f, 0.0f, 0.0f)); // Red
```

## Technical Details

### Vertex Generation

The bounding box is represented by 24 vertices (12 edges × 2 vertices per edge):

```
Bottom Face (4 edges):
- Front-bottom: corners[0] → corners[1]
- Right-bottom: corners[1] → corners[2]
- Back-bottom: corners[2] → corners[3]
- Left-bottom: corners[3] → corners[0]

Top Face (4 edges):
- Front-top: corners[4] → corners[5]
- Right-top: corners[5] → corners[6]
- Back-top: corners[6] → corners[7]
- Left-top: corners[7] → corners[4]

Vertical Edges (4 edges):
- Front-left: corners[0] → corners[4]
- Front-right: corners[1] → corners[5]
- Back-right: corners[2] → corners[6]
- Back-left: corners[3] → corners[7]
```

### Shader Compatibility

Bounding boxes use the same shader structure as the grid:
- Vertex shader: Transforms vertices using model/view/projection matrices
- Fragment shader: Uses `gridColor` uniform for line color

### Performance Considerations

- Bounding box calculation runs every frame for each model
- Bone transformations are calculated once per model per frame
- Vertex generation is optimized (only regenerates when min/max changes)
- GPU buffer updates only occur when geometry changes

## Integration Points

### ModelManager

- `ModelInstance` structure includes `BoundingBox boundingBox` member
- `initializeBoundingBox()` called automatically on model load

### Model Class

- `getBoundingBox()` - Static bounding box (no animation)
- `getBoundingBoxWithBones()` - Dynamic bounding box (with animation)

### Main Render Loop

- Bounding boxes rendered after models
- Updated every frame to reflect current state
- Uses same view/projection matrices as models

## Visual Feedback (v2.0.0)

### Selection-Based Color Coding
**Location**: `src/graphics/bounding_box.cpp` (`BoundingBox::render()`)

Bounding boxes now provide visual feedback based on selection state:
- **Selected Models**: Cyan color (`glm::vec3(0.0f, 1.0f, 1.0f)`) with thicker lines (`glLineWidth(2.0f)`)
- **Unselected Models**: Yellow color (`glm::vec3(1.0f, 1.0f, 0.0f)`) with standard line width (`glLineWidth(1.0f)`)

**Implementation**:
```cpp
// Check if this model is selected
bool isSelected = (model->m_isSelected);

// Set color based on selection state
glm::vec3 boxColor = isSelected ? 
    glm::vec3(0.0f, 1.0f, 1.0f) :  // Cyan for selected
    glm::vec3(1.0f, 1.0f, 0.0f);   // Yellow for unselected

// Set line width based on selection state
glLineWidth(isSelected ? 2.0f : 1.0f);
```

**Benefits**:
- **Clear Visual Feedback**: Users can immediately see which model is selected
- **Professional Appearance**: Matches industry-standard editors (Maya/Blender)
- **Improved UX**: Makes model selection more intuitive and visible

### Per-Model Visibility Control
**Location**: `src/graphics/model.h` (`Model::m_showBoundingBox`)

Each model has independent bounding box visibility control:
- **Per-Model Checkbox**: Property Panel checkbox controls individual model's bounding box
- **Global Toggle**: Tools menu "Bounding Boxes" toggle broadcasts to all models
- **Independent Control**: Per-model checkboxes work regardless of global toggle state

**Implementation**:
```cpp
// Per-model bounding box visibility flag
bool m_showBoundingBox = true;  // Default: visible

// Rendering logic checks this flag
if (model->m_showBoundingBox) {
    boundingBox.render();
}
```

## Future Enhancements

Potential improvements:
- Per-node bounding boxes (for individual bones/parts)
- ~~Bounding box visibility toggle in UI~~ ✅ **Implemented (v2.0.0)**
- ~~Color customization per model~~ ✅ **Implemented (v2.0.0)**
- Bounding box caching for static models
- Frustum culling using bounding boxes

## Related Files

- `src/graphics/grid.h` / `grid.cpp` - Similar line rendering implementation
- `src/graphics/model.cpp` - Bone transformation and bounding box calculation
- `src/main.cpp` - Main render loop with bounding box updates
- `Assets/grid.vert.glsl` / `grid.frag.glsl` - Similar shader structure

## Change Log

### Initial Implementation
- Created `BoundingBox` class with basic wireframe rendering
- Integrated into `ModelInstance` structure
- Automatic initialization on model load

### Real-Time Updates
- Added `getBoundingBoxWithBones()` method for animation-aware calculation
- Updated render loop to recalculate bounding boxes every frame
- Bounding boxes now reflect current animation/deformation state
