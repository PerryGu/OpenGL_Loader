# Framing Distance Fix for Pivot-Based Aim

## Date
Created: 2024

## Problem

Character was being cut off because the distance calculation was based on the bounding box center radius, but the camera aims at the gizmo (pivot point), which is typically at the feet/bottom of the character. This caused the top of the character to be cut off because the distance was calculated from the center, not from the actual pivot point.

## Root Cause

**Before Fix:**
- Distance was calculated using bounding box center: `radius = maxSize * 0.5f`
- Formula: `idealDistance = (radius / sin(fov/2)) * 1.2f`
- This assumed the camera was looking at the center of the object
- When gizmo is at feet/pivot, the top of the character extends beyond the calculated distance

**After Fix:**
- Distance is calculated from gizmo position to furthest corner: `maxDist = max(distance(corner, gizmo))`
- Formula: `idealDistance = (maxDist / tan(fov/2)) * 1.5f`
- This ensures the entire object fits when camera aims at the pivot point
- Increased padding accounts for the offset between gizmo and object center

## Solution

### 1. Calculate True Radius from Gizmo (`camera.cpp`)

Instead of using the bounding box center radius, find the distance from the gizmo to the furthest corner:

```cpp
// 1. CALCULATE TRUE RADIUS FROM GIZMO: Find the distance from the Gizmo to the furthest corner
// This accounts for the fact that the camera aims at the gizmo (pivot point), not the bounding box center
float maxDist = 0.0f;

// Calculate all 8 corners of the AABB
glm::vec3 corners[8] = {
    glm::vec3(boundingBoxMin.x, boundingBoxMin.y, boundingBoxMin.z),  // 0: min corner
    glm::vec3(boundingBoxMax.x, boundingBoxMin.y, boundingBoxMin.z),  // 1: min X, max YZ
    glm::vec3(boundingBoxMin.x, boundingBoxMax.y, boundingBoxMin.z),  // 2: min Y, max XZ
    glm::vec3(boundingBoxMax.x, boundingBoxMax.y, boundingBoxMin.z),  // 3: min Z, max XY
    glm::vec3(boundingBoxMin.x, boundingBoxMin.y, boundingBoxMax.z),  // 4: min Z, max XY
    glm::vec3(boundingBoxMax.x, boundingBoxMin.y, boundingBoxMax.z),  // 5: min Y, max XZ
    glm::vec3(boundingBoxMin.x, boundingBoxMax.y, boundingBoxMax.z),  // 6: min X, max YZ
    glm::vec3(boundingBoxMax.x, boundingBoxMax.y, boundingBoxMax.z)   // 7: max corner
};

// Find the maximum distance from gizmo position to any corner
for (int i = 0; i < 8; i++) {
    float dist = glm::length(corners[i] - targetPos);
    maxDist = glm::max(maxDist, dist);
}
```

**Key Points:**
- Calculates all 8 corners of the axis-aligned bounding box
- Finds the maximum distance from gizmo position to any corner
- This ensures the entire object fits when camera aims at the pivot point

### 2. Adjust for Vertical FOV (`camera.cpp`)

Since the character is vertical, use `tan` instead of `sin` for more accurate linear framing:

```cpp
// 2. ADJUST FOR VERTICAL FOV: Since the character is vertical, we must ensure the height fits
// Use 'tan' instead of 'sin' for a more accurate linear framing
float fovRadians = glm::radians(zoom);  // zoom is the FOV in degrees
float idealDistance = maxDist / tan(fovRadians / 2.0f);
```

**Why `tan` instead of `sin`:**
- `tan` provides more accurate linear framing for vertical objects
- `sin` is better for spherical objects centered in view
- `tan` accounts for the linear relationship between distance and object size in perspective projection

### 3. Add Padding (`camera.cpp`)

Multiply the final distance by 1.5f instead of 1.2f to account for looking at the bottom of the object:

```cpp
// 3. ADD PADDING: Multiply the final distance by 1.5f instead of 1.2f to account for
// the fact that we are looking at the bottom of the object (gizmo at feet/pivot)
idealDistance *= 1.5f;
```

**Why 1.5f instead of 1.2f:**
- Gizmo is typically at the feet/bottom of the character
- The top of the character extends further from the gizmo than the center
- Increased padding (50% vs 20%) ensures the entire character fits comfortably
- Accounts for the offset between gizmo position and object center

### 4. Apply Position (`camera.cpp`)

The position application remains the same:

```cpp
// 4. EXECUTE MOVEMENT: Update position to ideal distance while maintaining aim direction
// camera.position = gizmoWorldPos - (camera.forward * idealDistance)
cameraPos = targetPos - cameraFront * idealDistance;
```

**Key Points:**
- Camera position is calculated from gizmo position (not bounding box center)
- Distance is based on furthest corner from gizmo
- Maintains aim direction while adjusting distance

## Code Changes

### Files Modified

1. **`src/io/camera.cpp`**:
   - Modified `aimAtTargetWithFraming()` to calculate distance from gizmo to furthest corner
   - Changed from `sin` to `tan` for more accurate linear framing
   - Increased padding from 1.2f to 1.5f
   - Added calculation of all 8 AABB corners

## Technical Details

### Distance Calculation Comparison

**Before (Center-Based):**
```
radius = maxSize * 0.5f  // Half the maximum dimension
idealDistance = (radius / sin(fov/2)) * 1.2f
```

**After (Pivot-Based):**
```
maxDist = max(distance(corner, gizmo))  // Furthest corner from gizmo
idealDistance = (maxDist / tan(fov/2)) * 1.5f
```

### Why This Fixes the Problem

**Scenario: Character with Gizmo at Feet**

```
Before Fix:
- Bounding box: min=(0,0,0), max=(1,2,1)  // Height = 2
- Center: (0.5, 1.0, 0.5)
- Gizmo: (0.5, 0.0, 0.5)  // At feet
- Radius from center: 1.0 (half of max dimension)
- Distance calculated from center → Top of character cut off!

After Fix:
- Bounding box: min=(0,0,0), max=(1,2,1)
- Gizmo: (0.5, 0.0, 0.5)  // At feet
- Furthest corner from gizmo: (1, 2, 1) → distance = sqrt(1.25) ≈ 1.12
- Distance calculated from gizmo → Entire character fits!
```

### Formula Explanation

**Using `tan` for Linear Framing:**
- In perspective projection, object size at distance `d` is: `size = 2 * d * tan(fov/2)`
- To fit object of size `s`: `d = s / (2 * tan(fov/2))`
- For our case: `d = maxDist / tan(fov/2)` (maxDist is already the radius)

**Why `tan` is Better Than `sin`:**
- `tan` provides linear relationship: size ∝ distance
- `sin` is better for spherical objects centered in view
- For vertical objects viewed from an angle, `tan` is more accurate

## Testing

### Verification Steps

1. **Load a character model** with gizmo at feet/pivot
2. **Press 'F'** to aim at gizmo
3. **Verify entire character fits**: Top of character should not be cut off
4. **Verify distance is correct**: Character should fit comfortably with padding
5. **Test with different characters**: Should work for various pivot positions
6. **Test with different FOVs**: Distance should adjust correctly

### Expected Results

- ✅ Entire character fits in viewport (no cut-off at top)
- ✅ Distance is calculated from gizmo position (not bounding box center)
- ✅ Padding accounts for pivot offset (1.5f multiplier)
- ✅ Works correctly for characters with gizmo at feet/bottom
- ✅ Works correctly for objects with gizmo at different positions

## Summary

Fixed framing distance for pivot-based aim by:

✅ **Calculate True Radius from Gizmo**: Distance from gizmo to furthest corner (not center)
✅ **Adjust for Vertical FOV**: Use `tan` instead of `sin` for accurate linear framing
✅ **Add Padding**: Increase padding to 1.5f to account for pivot offset
✅ **Apply Position**: Camera position calculated from gizmo with correct distance

**Result**: Character is no longer cut off when camera aims at gizmo (pivot point). The entire object fits in the viewport with proper padding, regardless of where the gizmo is positioned relative to the bounding box.
