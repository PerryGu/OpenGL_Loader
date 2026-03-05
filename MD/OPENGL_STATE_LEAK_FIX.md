# OpenGL State Machine Leak Fix

## Date
Created: 2026-02-23  
Updated: 2026-02-23 (Fixed bone matrix upload issue)

## Problem
The `DrawSkeleton` method uses a basic/debug shader to draw skeleton lines and red joint points, but it was leaving this shader bound in the OpenGL state machine. When the engine looped to draw the next model, it would use this basic shader instead of the skeletal animation shader. Since the basic shader lacks skinning logic and its last uniform color was red, the next model would render in a solid red bind pose.

## Root Cause
OpenGL is a state machine - when you bind a shader, VAO, or set uniforms, those states persist until explicitly changed. The `DrawSkeleton` method was:
1. Activating a basic shader for skeleton visualization
2. Setting the color to red for joint points
3. Not cleaning up the state before returning

**Critical Issue**: Bone matrices are uploaded to the GPU *before* `model.render()` is called. If `DrawSkeleton` left a different shader bound (or unbound it), Model 2's bone matrices would be uploaded to the wrong shader. When `model.render()` finally bound the main shader, it would still contain Model 1's bone data, causing the mesh to detach from its skeleton or completely explode due to bone count mismatch.

## Solution
The fix requires shader activation to happen **BEFORE** bone matrices are uploaded, not during model rendering:

### 1. Shader Activation in ModelManager Render Loops
**Location**: `src/graphics/model_manager.cpp` - `ModelManager::renderAll()` and `ModelManager::renderAllWithRootNodeTransforms()` methods

Added explicit shader activation at the very beginning of each model iteration loop, **BEFORE** bone transforms are calculated and uploaded:

```cpp
void ModelManager::renderAllWithRootNodeTransforms(...) {
    for (size_t i = 0; i < models.size(); ++i) {
        auto& instance = models[i];
        
        // CRITICAL: Activate shader BEFORE uploading bone matrices
        // This ensures bone matrices are uploaded to the correct shader program
        // If DrawSkeleton left a different shader bound, this fixes it
        shader.activate();
        
        // Get bone transforms for this model
        instance->model.BoneTransform(instance->animationTime, transforms, ui_data);
        
        // Set bone transforms in shader (now guaranteed to be correct shader)
        shader.setMat4("bone_transforms", transforms.size(), transforms[0]);
        
        // Render the model
        instance->model.render(shader, modelMatrixToUse);
    }
}
```

**Why**: This ensures that bone matrices are uploaded to the correct shader program before each model is rendered. The shader is activated at the start of each iteration, so even if `DrawSkeleton` left a different shader bound, it's corrected before bone data is uploaded.

### 2. Removed Shader Activation from `Model::render()`
**Location**: `src/graphics/model.cpp` - `Model::render()` method

Removed the `shader.activate()` call that was previously added, since the shader is now properly bound in ModelManager before bone matrices are uploaded.

### 3. State Cleanup in `Model::DrawSkeleton()`
**Location**: `src/graphics/model.cpp` - `Model::DrawSkeleton()` method (line 354)

Only unbind VAO, but **do NOT** unbind the shader program:

```cpp
    // Re-enable depth test
    glEnable(GL_DEPTH_TEST);
    
    // NOTE: Do NOT unbind shader here (glUseProgram(0))
    // The shader must remain bound so the next model's bone matrices can be uploaded correctly
    // The ModelManager will activate the correct shader before uploading bone matrices
    glBindVertexArray(0);
}
```

**Why**: Unbinding the shader (`glUseProgram(0)`) would cause the next model's uniform uploads to fail. The ModelManager will activate the correct shader before uploading bone matrices, so we don't need to unbind it here.

## Benefits

1. **Prevents State Leaks**: Explicit state management ensures each render operation starts with a clean, predictable state
2. **Correct Shader Usage**: Models always render with their intended skeletal animation shader
3. **Proper Color Rendering**: Models render with correct materials/textures instead of solid red
4. **Correct Bone Data**: Each model's bone matrices are uploaded to the correct shader, preventing mesh detachment or explosion
5. **Robust Rendering**: Multiple models can be rendered in sequence without state interference

## Testing
- Verify that models render correctly after skeleton visualization
- Confirm that models use their proper materials/textures (not solid red)
- Check that skeletal animation works correctly for all models
- Ensure multiple models can be rendered in sequence without issues
- **Critical**: Verify that the second model's mesh stays attached to its skeleton (no bone count mismatch)

## Related Files
- `src/graphics/model_manager.cpp` - Shader activation before bone matrix upload
- `src/graphics/model.cpp` - Model rendering and skeleton drawing
- `src/graphics/model.h` - Function declarations
- `src/graphics/shader.h` - Shader class API

## OpenGL State Management Best Practices

When working with OpenGL state machine:
1. **Activate shaders BEFORE uploading uniforms** - Uniforms are uploaded to the currently bound shader program
2. **Don't unbind shaders unnecessarily** - Keep shaders bound if they'll be used again soon
3. **Clean up VAOs** - Always unbind VAOs after use to prevent accidental state leakage
4. **Don't assume state** - Always set what you need, even if you think it's already set
5. **Order matters** - The sequence of state changes is critical (shader → uniforms → render)

## Key Insight

The critical insight is that **uniforms are uploaded to the currently bound shader program**. If bone matrices are uploaded before the shader is activated, they go to the wrong shader. The fix ensures:
1. Shader is activated FIRST (at the start of each model iteration)
2. Bone matrices are uploaded SECOND (to the correct shader)
3. Model is rendered THIRD (with correct bone data)

## Notes
- The fix uses `shader.activate()` which internally calls `glUseProgram(shader.id)`
- `glUseProgram(0)` unbinds any shader program (sets it to NULL) - **do not use this** before bone matrix uploads
- `glBindVertexArray(0)` unbinds any VAO (sets it to NULL) - safe to use for cleanup
- Bone matrices must be uploaded to the shader that will be used for rendering
