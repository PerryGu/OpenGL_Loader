# Multi-Model Animation System

## Overview

The OpenGL_loader tool now supports loading and animating multiple FBX files simultaneously. Each model maintains its own independent animation state, allowing you to load as many animated models as you want and have them all animate independently in the viewport.

## Key Features

### Independent Animation States
- Each loaded model has its own animation time, FPS, and play state
- Models can be at different points in their animation timeline
- Animations loop independently for each model
- Each model's animation is calculated based on its own FPS and duration

### Multiple Model Support
- Load multiple FBX files without replacing previous ones
- All models are rendered simultaneously in the viewport
- Each model maintains its own bone hierarchy and animation data
- Models can have different animation durations and frame rates

### Animation Control
- Global play/stop button controls all models simultaneously
- Time slider shows the longest animation duration across all models
- Each model's animation time is updated independently
- When scrubbing, all models are set to the corresponding time

## Architecture

### ModelInstance Structure

Each loaded model is wrapped in a `ModelInstance` structure that contains:

```cpp
struct ModelInstance {
    Model model;                    // The model itself
    std::string filePath;           // Path to the loaded file
    std::string displayName;        // Display name (filename)
    
    // Animation state (independent for each model)
    float animationTime;            // Current animation time in seconds
    float totalTime;                // Accumulated time for auto-play
    unsigned int FPS;               // FPS of this model's animation
    bool isPlaying;                 // Whether this model's animation is playing
    double lastUpdateTime;          // Last time this model was updated
};
```

### ModelManager Class

The `ModelManager` class manages the collection of models:

- **loadModel()**: Loads a new model and adds it to the collection
- **removeModel()**: Removes a model by index
- **clearAll()**: Removes all models
- **updateAnimations()**: Updates animation state for all models
- **renderAll()**: Renders all models with their bone transformations
- **getCombinedBoundingBox()**: Gets bounding box for all models combined

## How It Works

### Loading Models

When you load a new FBX file:

1. A new `ModelInstance` is created
2. The model is loaded using Assimp
3. Animation state is initialized (FPS, duration, time = 0)
4. The instance is added to the `ModelManager` collection
5. The model is ready to animate independently

**Important**: Loading a new file does NOT replace previous models. All previously loaded models remain in the scene with their animations intact.

### Animation Update Loop

Each frame, the system:

1. Checks the global play/stop state from the UI
2. For each model:
   - Sets the play state based on global control
   - If playing: Updates animation time based on delta time
   - If stopped: Sets animation time from the time slider
   - Handles animation looping independently
3. Updates the time slider to show the current time (from first model)

### Rendering Loop

Each frame, the system:

1. Sets up lighting and camera matrices
2. For each model:
   - Calculates bone transformations at the model's current animation time
   - Sets bone transforms in the shader
   - Renders the model
3. All models appear in the viewport simultaneously

### Bone Transformations

Each model maintains its own:
- Bone hierarchy
- Bone mapping (name to ID)
- Bone offset matrices
- Animation channels

When rendering, each model's bone transformations are calculated independently based on its own animation time and applied before rendering.

## Usage

### Loading Multiple Models

1. Use **File > Open** to load the first FBX file
2. Use **File > Open** again to load additional files
3. Each new file is added to the scene without removing previous ones
4. All models will appear in the viewport

### Animation Control

- **Play Button**: Starts/continues animation for all loaded models
- **Stop Button**: Pauses all animations
- **Time Slider**: Scrubs all models to the selected time (converted to each model's time scale)

### Clearing Models

- Use **File > New Scene** to clear all loaded models
- This removes all models and resets the scene

## Technical Details

### Animation Time Calculation

For each model, animation time is calculated as:

```cpp
float TimeInTicks = animationTime * FPS;
float AnimationTime = fmod(TimeInTicks, animationDuration);
```

This ensures:
- Each model uses its own FPS
- Animations loop at their own duration
- Time is independent between models

### Bone Transform Updates

Each model's bone transforms are calculated per-frame:

```cpp
model.BoneTransform(instance.animationTime, transforms, ui_data);
shader.setMat4("bone_transforms", transforms.size(), transforms[0]);
model.render(shader);
```

This happens for each model before rendering, ensuring each model's skeleton is animated correctly.

### Memory Management

- Each model instance owns its own `Model` object
- Bone data, meshes, and textures are stored per-model
- When a model is removed, its resources are cleaned up
- The `ModelManager` handles cleanup automatically

## Limitations and Considerations

### Shader Bone Limit

The shader supports up to 50 bone transforms. If a model has more than 50 bones, only the first 50 will be used. This is defined in `Assets/vertex.glsl`:

```glsl
uniform mat4 bone_transforms[50];
```

### Performance

- Each additional model increases rendering cost
- Bone transformations are calculated per-model per-frame
- Complex models with many bones will impact performance more

### UI Considerations

- The time slider shows the longest animation duration
- When scrubbing, all models are set to the corresponding time
- Individual model control is not yet implemented in the UI

## Future Enhancements

Potential improvements:

1. **Individual Model Controls**: UI to play/pause/scrub individual models
2. **Model List**: Display all loaded models with their status
3. **Model Removal**: UI to remove specific models without clearing all
4. **Animation Speed**: Per-model animation speed multiplier
5. **Model Visibility**: Toggle visibility of individual models
6. **Model Positioning**: Offset/transform individual models in the scene

## Files Modified

### New Files
- `src/graphics/model_manager.h` - ModelManager class and ModelInstance structure
- `src/graphics/model_manager.cpp` - ModelManager implementation

### Modified Files
- `src/main.cpp` - Updated to use ModelManager instead of single Model
  - Changed from `Model model` to `ModelManager modelManager`
  - Updated loading logic to add models instead of replacing
  - Updated rendering to render all models
  - Updated animation updates to be per-model

## Troubleshooting

### Animation Not Working

If a model's animation stops after loading another:

- **Check**: Ensure the model was loaded successfully (check console output)
- **Check**: Verify the model has animation data (FPS > 0, duration > 0)
- **Check**: Ensure bone data is present (check console for "HasBones" messages)

### Models Not Appearing

If models don't appear in the viewport:

- **Check**: Verify models loaded successfully (check console)
- **Check**: Camera position (press 'A' to frame all)
- **Check**: Viewport is visible and not minimized

### Performance Issues

If performance degrades with multiple models:

- Reduce the number of loaded models
- Use simpler models with fewer bones
- Check GPU usage and memory

## Example Workflow

1. **Load First Model**: File > Open > `walk.fbx`
   - Model appears in viewport
   - Animation plays if play button is active

2. **Load Second Model**: File > Open > `run.fbx`
   - Both models now visible
   - Both animations play independently
   - First model's animation continues uninterrupted

3. **Control Animation**: Use play/stop button
   - Both models respond to global control
   - Each maintains its own animation state

4. **Clear Scene**: File > New Scene
   - All models removed
   - Scene reset

## Conclusion

The multi-model animation system allows you to load and animate multiple FBX files simultaneously, with each model maintaining its own independent animation state. This enables complex scenes with multiple animated characters or objects, all animating independently in the same viewport.
