# ImGuizmo PropertyPanel Only Connection

## Implementation
The gizmo now **only** updates PropertyPanel Transforms (Translate, Rotate, Scale). No other connections or side effects.

## Behavior

**When gizmo is manipulated:**
1. Gizmo reads the current transform state
2. User manipulates gizmo (move/rotate/scale)
3. Gizmo outputs new transform values
4. **ONLY PropertyPanel Transforms are updated:**
   - `setSliderTranslations()` - Updates Translate X, Y, Z sliders
   - `setSliderRotations()` - Updates Rotate X, Y, Z sliders  
   - `setSliderScales()` - Updates Scale X, Y, Z sliders

**What is NOT connected:**
- Model's `pos/rotation/size` are NOT updated directly
- No other side effects
- No automatic model position updates
- Gizmo only feeds into PropertyPanel Transforms

## Code Implementation

**File: `src/io/scene.cpp`**

```cpp
// If the gizmo was manipulated, update ONLY PropertyPanel Transforms (Translate, Rotate, Scale)
// No other connections - gizmo only feeds into PropertyPanel Transforms
if (manipulated) {
    // Decompose the manipulated matrix
    glm::decompose(newModelMatrix, scale, rotation, translation, skew, perspective);
    
    // Convert quaternion to Euler angles (degrees) for PropertyPanel
    glm::vec3 rotationEuler = glm::degrees(glm::eulerAngles(rotation));
    
    // Update ONLY PropertyPanel Transforms - no other connections
    if (isRootNode) {
        mPropertyPanel.setSliderTranslations(translation);
        mPropertyPanel.setSliderRotations(rotationEuler);
        mPropertyPanel.setSliderScales(scale);
    }
    // Note: For non-root nodes, gizmo does not update anything (only works with RootNode)
}
```

## Flow

1. **User manipulates gizmo** → Gizmo outputs new transform matrix
2. **Matrix is decomposed** → Translation, Rotation (quat), Scale extracted
3. **Rotation converted to Euler** → Degrees for PropertyPanel display
4. **PropertyPanel updated** → Translate, Rotate, Scale sliders updated
5. **That's it** → No other connections, no model updates

## Requirements

- Gizmo only works when RootNode is selected
- Gizmo only updates PropertyPanel Transforms
- No direct model updates
- No other side effects

## Files Modified

- `src/io/scene.cpp` - Re-enabled gizmo to PropertyPanel connection, removed all other connections
