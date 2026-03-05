# Bone Picking Implementation

## Date
Created: 2026-02-23

## Overview
Implemented granular bone-picking functionality that allows users to select individual bones by clicking on them in the viewport when the skeleton is visible. This extends the existing mouse-picking raycast system to work with 3D bone segments instead of just bounding boxes.

## Problem
Previously, clicking on a model in the viewport would only select the entire model. There was no way to select individual bones directly by clicking on them, requiring users to navigate through the outliner hierarchy to find specific bones.

## Solution
Extended the viewport selection system to:
1. Check for bone intersections when a model is selected and its skeleton is visible
2. Calculate the shortest distance between the mouse ray and each bone segment
3. Select the closest bone if it's within a picking threshold
4. Store the selected bone name in the model for use by other systems

## Implementation

### 1. Model Selection Storage
**Location**: `src/graphics/model.h` (line 171)

Added member variable to store selected bone name:
```cpp
// Selected bone/node name (set by bone-picking raycast)
std::string m_selectedNodeName = "";
```

**Purpose**: Stores the name of the bone that was picked via raycast, accessible by other systems (e.g., PropertyPanel, Outliner).

### 2. Ray-to-Line-Segment Distance Function
**Location**: `src/graphics/raycast.h` (lines 67-80), `src/graphics/raycast.cpp` (lines 122-181)

Added mathematical helper function to calculate shortest distance between a ray and a 3D line segment:
```cpp
float rayToLineSegmentDistance(const Ray& ray,
                               const glm::vec3& segmentStart,
                               const glm::vec3& segmentEnd);
```

**Algorithm**:
- Finds the closest points on both the ray and the line segment
- Calculates the distance between those closest points
- Handles edge cases: zero-length segments, parallel ray/segment
- Returns the shortest distance

**Mathematical Approach**:
1. Normalize segment direction vector
2. Calculate system of equations for closest points
3. Handle parallel case (ray and segment are parallel)
4. Clamp segment parameter to segment bounds
5. Return distance between closest points

### 3. Bone Picking Logic
**Location**: `src/viewport_selection.cpp` (lines 147-220)

Extended the viewport selection handler to check for bone intersections:

```cpp
// BONE PICKING: If skeleton is visible, check for bone intersections
if (selectedInstance->model.m_showSkeleton) {
    // Get the model's RootNode transform matrix (same as used for rendering)
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    auto matrixIt = modelRootNodeMatrices.find(closestModelIndex);
    if (matrixIt != modelRootNodeMatrices.end()) {
        modelMatrix = matrixIt->second;
    }
    
    // Calculate character size dynamically for proportional raycast threshold
    glm::vec3 bboxMin, bboxMax;
    selectedInstance->model.getBoundingBox(bboxMin, bboxMax);
    float modelSize = glm::length(bboxMax - bboxMin);
    float dynamicThreshold = std::max(0.05f, modelSize * 0.008f);
    
    // Traverse the model's animated skeleton hierarchy (same logic as DrawSkeleton)
    float closestBoneDistance = std::numeric_limits<float>::max();
    std::string closestBoneName = "";
    
    // Iterate through all nodes in the linear skeleton
    for (size_t i = 0; i < selectedInstance->model.m_linearSkeleton.size(); i++) {
        // ... traverse skeleton and calculate distances ...
        
        // Calculate distance from ray to this bone segment
        float distance = Raycast::rayToLineSegmentDistance(ray, parentWorldPos, nodeWorldPos);
        
        // Dynamic threshold check - scales proportionally with character size
        if (distance < dynamicThreshold && distance < closestBoneDistance) {
            closestBoneDistance = distance;
            closestBoneName = std::string(parentBone.pNode->mName.C_Str());
        }
    }
    
    // If closest bone is within picking threshold, select it
    if (closestBoneDistance < dynamicThreshold && !closestBoneName.empty()) {
        selectedInstance->model.m_selectedNodeName = closestBoneName;
        LOG_INFO("Selected Bone: " + closestBoneName);
    }
}
```

**Key Features**:
- Uses the same skeleton traversal logic as `DrawSkeleton` (ensures consistency)
- Uses animated transforms (not static bind pose)
- Applies model's RootNode transform to get world-space positions
- Uses same mathematical sanity checks as `DrawSkeleton` (filters invalid bones)
- Dynamic picking threshold: Calculated as 0.8% of model's bounding box diagonal (scales with character size)

### 4. Skeleton Traversal Logic
The bone picking uses the exact same traversal logic as `DrawSkeleton`:

1. **Iterate through linear skeleton**: Uses `m_linearSkeleton` vector
2. **Get animated transforms**: Uses `bone.globalMatrix` (current frame's animation)
3. **Apply model matrix**: Transforms to world space using RootNode transform
4. **Validate bone segments**: 
   - Checks for valid parent pointer
   - Validates bone length (< 100.0f units)
   - Filters out bones at origin (0,0,0)
5. **Calculate distances**: For each valid bone segment, calculates ray-to-segment distance

## Picking Threshold

**Implementation**: Dynamic threshold calculated as a percentage of the model's bounding box diagonal, with distance-aware adjustment based on camera distance.

### Base Threshold (Character Size)
**Formula**: `baseThreshold = max(0.05f, modelSize * 0.008f)`
- `modelSize` = length of bounding box diagonal (`glm::length(bboxMax - bboxMin)`)
- `0.008f` = 0.8% of model size (proportional multiplier)
- `0.05f` = minimum absolute fallback for extremely small models

**Rationale**:
- **Proportional Scaling**: Threshold scales with character size, making bone picking work on characters of any scale (tiny to massive)
- **Handles Both Scale Types**: Works correctly for both transform-scaled and vertex-scaled FBX meshes
- **Minimum Fallback**: Ensures small models still have a usable picking radius
- **Consistent Experience**: Provides similar "click radius" feel regardless of character size

**Examples**:
- Small character (1 unit diagonal): `baseThreshold = max(0.05f, 1.0 * 0.008) = 0.05f` (uses minimum)
- Medium character (10 units diagonal): `baseThreshold = max(0.05f, 10.0 * 0.008) = 0.08f`
- Large character (100 units diagonal): `baseThreshold = max(0.05f, 100.0 * 0.008) = 0.8f`
- Massive character (1000 units diagonal): `baseThreshold = max(0.05f, 1000.0 * 0.008) = 8.0f`

### Distance-Aware Threshold (Camera Distance)
**Formula**: `adjustedThreshold = clamp(baseThreshold * (distanceToCamera * 0.05f), 0.05f, 0.5f)`
- `distanceToCamera` = Distance from camera origin to bone segment midpoint
- `0.05f` = Distance multiplier (adjusts threshold based on camera distance)
- `0.05f` = Minimum threshold (prevents picking from being too sensitive when close)
- `0.5f` = Maximum threshold (prevents picking from being too loose when far)

**Rationale**:
- **Distance Scaling**: Threshold adjusts based on camera distance, ensuring picking feels equally "sticky" regardless of camera position
- **Near Camera**: Smaller threshold prevents accidental picks when zoomed in close
- **Far Camera**: Larger threshold allows picking when zoomed out (bones appear smaller on screen)
- **Clamping**: Prevents threshold from becoming too small (near camera) or too large (far camera)

**Implementation Location**: `src/graphics/model.cpp` (`Model::pickBone()` method)

**Code**:
```cpp
// Calculate distance from camera to bone segment for distance-aware threshold
glm::vec3 boneMidpoint = (parentWorldPos + nodeWorldPos) * 0.5f;
glm::vec3 toBoneMidpoint = boneMidpoint - ray.origin;
float tRay = glm::dot(toBoneMidpoint, ray.direction);
tRay = std::max(0.0f, tRay); // Clamp to ensure point is in front of camera
glm::vec3 closestPointOnRay = ray.origin + tRay * ray.direction;
float distanceToCamera = glm::length(closestPointOnRay - ray.origin);

// Distance-aware threshold: Adjust threshold based on camera distance
float adjustedThreshold = baseThreshold * (distanceToCamera * 0.05f);
adjustedThreshold = std::clamp(adjustedThreshold, 0.05f, 0.5f);
```

**Adjustment**: The multipliers can be adjusted if the click radius feels too generous or too tight:
- **Base Multiplier** (`0.008f`): Increase for more generous picking, decrease for tighter picking
- **Distance Multiplier** (`0.05f`): Increase for more distance sensitivity, decrease for less sensitivity
- **Clamp Values** (`0.05f`, `0.5f`): Adjust minimum/maximum threshold bounds

## Workflow

1. **User clicks viewport**: Mouse click triggers `handleSelectionClick()`
2. **Model selection**: Raycast tests against bounding boxes, selects closest model
3. **Bone picking check**: If selected model has `m_showSkeleton == true`:
   - Traverse skeleton hierarchy
   - Calculate distances to all bone segments
   - Find closest bone
   - If distance < threshold, select bone
4. **Logging**: Selected bone name is logged via `LOG_INFO()`
5. **Storage**: Selected bone name stored in `model.m_selectedNodeName`

## Integration Points

The selected bone name (`m_selectedNodeName`) can be used by:
- **PropertyPanel**: To show/highlight the selected bone's transforms
- **Outliner**: To automatically expand and highlight the selected bone
- **Gizmo**: To attach gizmo to the selected bone
- **Debug Systems**: To display bone-specific information

## Benefits

1. **Intuitive Selection**: Click directly on bones to select them
2. **Visual Feedback**: Works with visible skeleton (requires skeleton to be shown)
3. **Animated Accuracy**: Uses current frame's animated transforms
4. **Consistent Logic**: Reuses `DrawSkeleton` traversal (no code duplication)
5. **Mathematical Robustness**: Same sanity checks prevent picking invalid bones
6. **Scale-Aware Picking**: Dynamic threshold ensures bone picking works on characters of any scale (tiny to massive)

## Testing

### Test Case 1: Basic Bone Picking
1. Load a model with skeleton
2. Enable "Show Skeleton" for the model
3. Click on a visible bone segment in the viewport
4. **Expected**: 
   - Model is selected
   - Bone name is logged: "Selected Bone: [bone name]"
   - `model.m_selectedNodeName` contains the bone name

### Test Case 2: Skeleton Not Visible
1. Load a model with skeleton
2. Disable "Show Skeleton" for the model
3. Click on the model
4. **Expected**: 
   - Model is selected
   - No bone picking occurs
   - `model.m_selectedNodeName` remains empty

### Test Case 3: Multiple Bones
1. Load a model with skeleton
2. Enable "Show Skeleton"
3. Click near multiple overlapping bones
4. **Expected**: 
   - Closest bone (by distance) is selected
   - Only one bone is selected

### Test Case 4: No Bone Within Threshold
1. Load a model with skeleton
2. Enable "Show Skeleton"
3. Click on the model but far from any bone
4. **Expected**: 
   - Model is selected
   - No bone is selected (distance > threshold)
   - `model.m_selectedNodeName` remains empty

## Related Files
- `src/graphics/model.h` - `m_selectedNodeName` member variable
- `src/graphics/raycast.h` - `rayToLineSegmentDistance()` declaration
- `src/graphics/raycast.cpp` - `rayToLineSegmentDistance()` implementation
- `src/viewport_selection.cpp` - Bone picking logic
- `src/graphics/model.cpp` - `DrawSkeleton()` (reference for traversal logic)

## Future Enhancements

1. **Configurable Threshold Multiplier**: Add UI slider to adjust the 0.008f multiplier (proportional threshold)
2. **Visual Feedback**: Highlight selected bone in skeleton rendering
3. **Multi-Bone Selection**: Support selecting multiple bones (Ctrl+Click)
4. **Bone Radius**: Consider bone thickness/radius in distance calculation
5. **Joint Picking**: Also allow picking joint points (not just segments)

## Notes

- Bone picking only works when skeleton is visible (`m_showSkeleton == true`)
- Uses animated transforms (respects current animation frame)
- Respects model's RootNode transform (world-space accuracy)
- Same mathematical validation as skeleton rendering (consistency)
- Action-based logging (only logs when bone is actually selected)
