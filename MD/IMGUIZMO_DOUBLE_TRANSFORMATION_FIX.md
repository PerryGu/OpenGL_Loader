# ImGuizmo Double Transformation Fix

## Problem
When using the gizmo with PropertyPanel integration, there was a double transformation issue. The model would appear to be transformed twice - once from the original gizmo manipulation (stored in model.pos/rotation/size) and once from the PropertyPanel values being applied.

## Root Cause
When RootNode was first selected in the outliner (e.g., when a model is selected via viewport), the PropertyPanel would initialize RootNode transforms to default values (0,0,0 for translation/rotation, 1,1,1 for scale) if they hadn't been set before. However, the model might already have non-zero transform values from previous gizmo manipulation. This created a mismatch:

1. Model has `pos = (0, 0, 7)` from previous gizmo manipulation
2. User selects model → RootNode is selected in outliner
3. PropertyPanel initializes RootNode transforms to `(0, 0, 0)` (default, first time)
4. PropertyPanel values `(0, 0, 0)` are applied to model → `model.pos = (0, 0, 0)`
5. But the gizmo reads from PropertyPanel `(0, 0, 0)` and displays at `(0, 0, 0)`
6. However, the model might have been rendered at `(0, 0, 7)` before, causing a visual jump

Additionally, if the model had existing values and PropertyPanel started at (0,0,0), applying PropertyPanel would reset the model, but the visual representation might still show the old position briefly, creating a double transformation effect.

## Solution Applied

### 1. Added `initializeRootNodeFromModel()` Method to PropertyPanel (`src/io/ui/property_panel.h`)

**Added method declaration:**
```cpp
// Initialize RootNode transforms from model's current transform
// This is called when RootNode is first selected to sync PropertyPanel with model's current state
// Prevents double transformation by ensuring PropertyPanel starts with model's current values
void initializeRootNodeFromModel(const glm::vec3& translation, const glm::quat& rotation, const glm::vec3& scale);
```

**Purpose:**
- Initializes PropertyPanel RootNode transforms from the model's current transform
- Only initializes if RootNode doesn't already have values (first time selection)
- Prevents overwriting user's manual changes
- Ensures PropertyPanel and model start in sync

### 2. Implemented `initializeRootNodeFromModel()` (`src/io/ui/property_panel.cpp`)

**Implementation details:**
- Checks if RootNode is currently selected
- Only initializes if RootNode doesn't already have values in the maps (first time)
- Converts quaternion rotation to Euler angles (degrees) for PropertyPanel
- Sets slider values and saves to bone maps
- Includes debug output for verification

**Key logic:**
```cpp
// Only initialize if RootNode doesn't already have values (first time selection)
auto transIt = mBoneTranslations.find(mSelectedNod_name);
auto rotIt = mBoneRotations.find(mSelectedNod_name);
auto scaleIt = mBoneScales.find(mSelectedNod_name);

// If RootNode already has values, don't overwrite (user may have set them manually)
if (transIt != mBoneTranslations.end() || rotIt != mBoneRotations.end() || scaleIt != mBoneScales.end()) {
    return;  // Already initialized, don't overwrite
}
```

### 3. Call Initialization When Model is Selected (`src/main.cpp`)

**Added call after `setSelectionToRoot()`:**
```cpp
// Initialize PropertyPanel RootNode transforms from model's current transform
// This prevents double transformation by syncing PropertyPanel with model's current state
// Only initializes if RootNode doesn't already have values (first time selection)
scene.mPropertyPanel.initializeRootNodeFromModel(
    selectedInstance->model.pos,
    selectedInstance->model.rotation,
    selectedInstance->model.size
);
```

**When it's called:**
- Immediately after a model is selected via viewport raycast
- After `setSelectionToRoot()` sets RootNode as selected
- Before PropertyPanel values are used for rendering

### 4. Added Required GLM Include (`src/io/ui/property_panel.h`)

**Added:**
```cpp
#include <glm/gtx/quaternion.hpp>  // For glm::eulerAngles and glm::quat
```

**Why:**
- Needed for `glm::eulerAngles()` to convert quaternion to Euler angles
- Needed for `glm::quat` type in method signature

## How It Works Now

1. **Model Selection:**
   - User selects model via viewport → RootNode is selected in outliner
   - `initializeRootNodeFromModel()` is called with model's current transform
   - PropertyPanel RootNode transforms are initialized to match model's current state

2. **First Time Selection:**
   - If RootNode doesn't have values yet → PropertyPanel is initialized from model
   - Model and PropertyPanel are now in sync

3. **Subsequent Manipulation:**
   - Gizmo manipulates → updates PropertyPanel RootNode transforms
   - PropertyPanel values are applied to model → model updates
   - Model renders using model.pos/rotation/size (which match PropertyPanel)

4. **Manual PropertyPanel Changes:**
   - If user manually changes PropertyPanel values → they're saved
   - Next time RootNode is selected → PropertyPanel keeps user's values (not overwritten)

## Benefits

1. **No Double Transformation:** PropertyPanel and model start in sync, preventing visual jumps
2. **Preserves User Changes:** Once PropertyPanel has values, they're not overwritten
3. **Smooth Transitions:** Model position doesn't jump when RootNode is first selected
4. **Consistent State:** Gizmo, PropertyPanel, and model all show the same transform values

## Testing

To verify the fix:
1. Load a model
2. Use gizmo to move it to position (e.g., Z = 7)
3. Deselect the model
4. Select the model again
5. **Expected:** PropertyPanel should show Z = 7 (not 0), and model should stay at Z = 7
6. **Before fix:** PropertyPanel would show Z = 0, and model would jump to Z = 0

## Files Modified

- `src/io/ui/property_panel.h` - Added method declaration and GLM quaternion include
- `src/io/ui/property_panel.cpp` - Implemented initialization method
- `src/main.cpp` - Added call to initialize PropertyPanel when model is selected
