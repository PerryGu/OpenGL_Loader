# ImGuizmo Rotation Fix

## Problem
When using the rotate gizmo (E key), the gizmo visual would update and show rotation degrees changing, but the model itself would not rotate. Translate and scale modes worked correctly.

## Root Cause
The `Model` class only had `pos` (position) and `size` (scale) members, but no rotation member. When the gizmo was manipulated in rotate mode:
1. The rotation was extracted from the decomposed matrix
2. But it was not stored in the Model class
3. The render function only applied translation and scale, ignoring rotation

## Solution Applied

### 1. Added Rotation Member to Model Class (`src/graphics/model.h`)

**Added:**
```cpp
glm::quat rotation;  // Model rotation as quaternion (for gizmo rotation support)
```

**Why quaternion?**
- Quaternions are more efficient and avoid gimbal lock
- GLM provides good quaternion support
- Standard for 3D graphics

### 2. Updated Model Constructor (`src/graphics/model.cpp`)

**Initialized rotation to identity:**
```cpp
Model::Model(glm::vec3 pos, glm::vec3 size, bool noTex)
    : pos(pos), size(size), rotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)), noTex(noTex) {
    // rotation initialized to identity quaternion (no rotation)
}
```

### 3. Updated Model Render Function (`src/graphics/model.cpp`)

**Before:**
```cpp
glm::mat4 model = glm::mat4(1.0f);
model = glm::translate(model, pos);
model = glm::scale(model, size);
```

**After:**
```cpp
// Build model matrix: Translation * Rotation * Scale
// Order matters: we want to scale first, then rotate, then translate
glm::mat4 model = glm::mat4(1.0f);
model = glm::translate(model, pos);
model = model * glm::mat4_cast(rotation);  // Apply rotation (quaternion to matrix)
model = glm::scale(model, size);
```

**Transform Order:**
- Standard TRS (Translation * Rotation * Scale) order
- Scale is applied first (in local space)
- Then rotation (around local origin)
- Finally translation (to world position)

### 4. Updated Move Constructor and Assignment (`src/graphics/model.cpp`)

**Added rotation to move operations:**
```cpp
// Move constructor
Model::Model(Model&& other) noexcept
    : pos(other.pos), size(other.size), rotation(other.rotation), noTex(other.noTex), ...

// Move assignment
rotation = other.rotation;
```

### 5. Updated Gizmo Code to Store Rotation (`src/io/scene.cpp`)

**Before:**
```cpp
// Update model position and size
selectedModel->model.pos = translation;
selectedModel->model.size = scale;
// Note: Rotation is extracted but not currently stored
```

**After:**
```cpp
// Update model transform: position, rotation, and scale
selectedModel->model.pos = translation;
selectedModel->model.rotation = rotation;  // Store rotation quaternion
selectedModel->model.size = scale;
```

### 6. Updated Gizmo Matrix Building (`src/io/scene.cpp`)

**Before:**
```cpp
// Build model matrix from pos and size
glm::mat4 modelMatrix = glm::mat4(1.0f);
modelMatrix = glm::translate(modelMatrix, selectedModel->model.pos);
modelMatrix = glm::scale(modelMatrix, selectedModel->model.size);
```

**After:**
```cpp
// Build model matrix from pos, rotation, and size
// Order: Translation * Rotation * Scale (standard TRS order)
glm::mat4 modelMatrix = glm::mat4(1.0f);
modelMatrix = glm::translate(modelMatrix, selectedModel->model.pos);
modelMatrix = modelMatrix * glm::mat4_cast(selectedModel->model.rotation);  // Apply rotation
modelMatrix = glm::scale(modelMatrix, selectedModel->model.size);
```

## How It Works Now

1. **Initial State**: Model rotation is initialized to identity (no rotation)
2. **Gizmo Manipulation**: When user rotates the gizmo, the rotation quaternion is extracted from the decomposed matrix
3. **Storage**: Rotation is stored in `Model::rotation` member
4. **Rendering**: Model matrix includes rotation: `T * R * S`
5. **Persistence**: Rotation persists across frames until changed

## Transform Order Explanation

The transform order `Translation * Rotation * Scale` means:
1. **Scale** is applied first (in model's local space)
2. **Rotation** is applied around the model's local origin
3. **Translation** moves the rotated/scaled model to world position

This is the standard order for 3D transforms and ensures:
- Scaling happens in local space (not affected by rotation)
- Rotation happens around the model's center
- Translation positions the model in world space

## Testing

After fix:
1. ✅ Select a model
2. ✅ Press E to switch to rotate mode
3. ✅ Drag rotation rings - model should rotate correctly
4. ✅ Press W to switch to translate - model should move (rotation preserved)
5. ✅ Press R to switch to scale - model should scale (rotation preserved)
6. ✅ Rotation should persist when switching between modes

## Files Modified

1. `src/graphics/model.h` - Added rotation member
2. `src/graphics/model.cpp` - Updated constructor, render, and move operations
3. `src/io/scene.cpp` - Updated gizmo code to store and use rotation

## Date
Fixed: [Current Session]
