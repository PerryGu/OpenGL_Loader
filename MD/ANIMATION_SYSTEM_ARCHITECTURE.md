# Animation System Architecture

## Overview

This document explains the technical architecture of the animation system in the OpenGL_loader tool, focusing on how multiple models with animations are managed and rendered.

## System Components

### 1. Model Class (`src/graphics/model.h` / `model.cpp`)

The `Model` class represents a single 3D model loaded from an FBX file.

**Key Members:**
- `const aiScene* mScene` - Assimp scene data (contains animation data)
- `Assimp::Importer mIporter` - Assimp importer instance
- `std::vector<Mesh> meshes` - Collection of meshes in the model
- `unsigned int mNumBones` - Number of bones in the skeleton
- `std::map<std::string, unsigned int> m_BoneMapping` - Maps bone names to IDs
- `std::vector<BoneInfo> m_BoneInfo` - Bone offset matrices and transformations
- `unsigned int mFPS` - Animation frames per second

**Key Methods:**
- `loadModel()` - Loads model from file path
- `BoneTransform()` - Calculates bone transformation matrices at given time
- `render()` - Renders the model
- `getFPS()` - Returns animation FPS
- `getAnimationDuration()` - Returns animation duration in ticks

### 2. ModelInstance Structure (`src/graphics/model_manager.h`)

Wraps a `Model` with animation state management.

**Purpose:**
- Maintains independent animation state per model
- Tracks animation time, play state, and timing information
- Provides methods to update animation state

**Key Members:**
- `Model model` - The actual model
- `float animationTime` - Current animation time in seconds
- `float totalTime` - Accumulated time for auto-play
- `unsigned int FPS` - Model's animation FPS
- `bool isPlaying` - Whether animation is playing
- `double lastUpdateTime` - Last update timestamp

**Key Methods:**
- `initializeAnimation()` - Sets up animation state after loading
- `updateAnimation()` - Updates animation time based on delta time
- `setAnimationTime()` - Sets animation time manually (for scrubbing)
- `setPlaying()` - Sets play/pause state

### 3. ModelManager Class (`src/graphics/model_manager.h` / `model_manager.cpp`)

Manages a collection of `ModelInstance` objects.

**Purpose:**
- Stores multiple models
- Updates all model animations
- Renders all models
- Provides access to individual models

**Key Members:**
- `std::vector<ModelInstance> models` - Collection of model instances

**Key Methods:**
- `loadModel()` - Loads and adds a new model
- `removeModel()` - Removes a model by index
- `clearAll()` - Removes all models
- `updateAnimations()` - Updates animation state for all models
- `renderAll()` - Renders all models with bone transformations
- `getCombinedBoundingBox()` - Gets bounding box for all models

## Animation Flow

### Loading Phase

```
User loads FBX file
    ↓
ModelManager::loadModel() called
    ↓
Create ModelInstance
    ↓
Model::loadModel() called
    ↓
Assimp::Importer reads file
    ↓
Model::processRig() extracts bone data
    ↓
Model::processNode() processes meshes
    ↓
ModelInstance::initializeAnimation() sets up animation state
    ↓
ModelInstance added to ModelManager collection
```

### Update Phase (Per Frame)

```
Main loop calls update
    ↓
Get global play/stop state from UI
    ↓
For each ModelInstance:
    ├─ Set isPlaying based on global state
    ├─ If playing:
    │   ├─ Calculate deltaTime
    │   ├─ Update totalTime += deltaTime
    │   ├─ Handle looping (if time > duration)
    │   └─ Set animationTime = totalTime
    └─ If stopped:
        ├─ Get time from slider
        └─ Set animationTime = sliderTime / FPS
```

### Rendering Phase (Per Frame)

```
Main loop calls render
    ↓
Set up lighting and camera matrices
    ↓
For each ModelInstance:
    ├─ Calculate bone transforms:
    │   └─ Model::BoneTransform(animationTime, transforms, ui_data)
    ├─ Set bone transforms in shader:
    │   └─ shader.setMat4("bone_transforms", ...)
    └─ Render model:
        └─ Model::render(shader)
```

## Bone Transformation Pipeline

### 1. Bone Transform Calculation

`Model::BoneTransform()` is called for each model:

```cpp
void Model::BoneTransform(float TimeInSeconds, 
                         std::vector<glm::mat4>& Transforms, 
                         UI_data ui_data)
{
    // Convert time to ticks
    float TimeInTicks = TimeInSeconds * mFPS;
    float AnimationTime = fmod(TimeInTicks, mScene->mAnimations[0]->mDuration);
    
    // Traverse node hierarchy and calculate transformations
    ReadNodeHierarchy(AnimationTime, mScene->mRootNode, Identity, ui_data);
    
    // Build final transformation matrices
    for (each bone) {
        FinalTransform = GlobalInverseTransform * 
                        NodeTransform * 
                        BoneOffset;
        Transforms[boneIndex] = FinalTransform;
    }
}
```

### 2. Shader Application

Bone transforms are sent to the GPU shader:

```glsl
// Vertex shader receives bone transforms
uniform mat4 bone_transforms[50];

// For each vertex:
mat4 boneTransform = weighted blend of bone transforms;
vec4 pos = boneTransform * vec4(vertexPosition, 1.0);
```

### 3. Per-Vertex Bone Blending

Each vertex can be influenced by up to 4 bones:

```glsl
// Vertex attributes
in vec4 boneIds;      // IDs of influencing bones
in vec4 boneWeights;  // Weights for each bone

// Blend bones
boneTransform = bone_transforms[boneIds.x] * boneWeights.x +
                bone_transforms[boneIds.y] * boneWeights.y +
                bone_transforms[boneIds.z] * boneWeights.z +
                bone_transforms[boneIds.w] * boneWeights.w;
```

## Data Structures

### BoneInfo

```cpp
struct BoneInfo {
    Matrix4f FinalTransformation;  // Final transform to apply
    Matrix4f BoneOffset;            // Offset from mesh to bone space
};
```

### VertexBoneData

```cpp
struct VertexBoneData {
    unsigned int IDs[4];     // Up to 4 bone IDs per vertex
    float Weights[4];        // Weights for each bone
};
```

### MeshEntry

```cpp
struct MeshEntry {
    unsigned int BaseVertex;   // Starting vertex index
    unsigned int BaseIndex;     // Starting index index
    unsigned int NumIndices;   // Number of indices
    unsigned int MaterialIndex; // Material ID
};
```

## Animation Data Storage

### Per Model
- Bone hierarchy (from Assimp scene)
- Bone offset matrices
- Animation channels (rotation, scale, translation keyframes)
- Vertex bone weights
- Animation FPS and duration

### Per ModelInstance
- Current animation time
- Play state
- Accumulated time
- Last update timestamp

## Thread Safety

**Current Implementation:**
- Single-threaded
- All updates happen on main thread
- No synchronization needed

**Future Considerations:**
- Animation updates could be parallelized
- Rendering could use multiple threads
- Would require thread-safe data structures

## Memory Management

### Model Loading
- Assimp scene data is owned by `Assimp::Importer`
- Meshes and textures are copied into `Model` class
- Bone data is stored in `Model` members

### ModelInstance
- Owns a `Model` object (stack allocation)
- Animation state is lightweight (few floats)

### ModelManager
- Stores `ModelInstance` objects in `std::vector`
- Automatic memory management via RAII
- Cleanup handled in destructor

## Performance Considerations

### Animation Updates
- **Cost**: O(bones) per model per frame
- **Optimization**: Only update if playing or time changed
- **Bottleneck**: Node hierarchy traversal

### Rendering
- **Cost**: O(vertices) per model per frame
- **Optimization**: Frustum culling (not yet implemented)
- **Bottleneck**: Bone transform calculations

### Memory
- **Per Model**: ~(vertices + bones + textures) bytes
- **Scaling**: Linear with number of models
- **Limit**: GPU memory for textures and buffers

## Error Handling

### Model Loading Failures
- Returns -1 from `loadModel()` on failure
- Error message printed to console
- Previous models remain loaded

### Animation Errors
- Null checks for scene and bones
- Default to identity transforms if no animation
- Graceful degradation if bone data missing

### Rendering Errors
- Empty transform vector handled
- Identity matrices used if no bones
- Models without animations still render

## Debugging

### Console Output
- Model loading status
- Bone count information
- Animation time updates (periodic)
- Error messages

### Debug Flags
- Animation time logging (every N frames)
- Bone transform validation
- Performance timing (not yet implemented)

## Future Improvements

### Architecture Enhancements
1. **Animation Blending**: Blend between multiple animations
2. **Animation Events**: Trigger events at specific times
3. **IK System**: Inverse kinematics for bone positioning
4. **Animation Layers**: Multiple animations per model

### Performance Optimizations
1. **Animation Caching**: Cache transforms for static poses
2. **LOD System**: Level of detail for distant models
3. **Instancing**: Render multiple instances efficiently
4. **GPU Animation**: Move calculations to GPU

### UI Enhancements
1. **Per-Model Controls**: Individual play/pause/scrub
2. **Animation Timeline**: Visual timeline for each model
3. **Animation Speed**: Per-model speed multiplier
4. **Model List**: Display and manage all loaded models
