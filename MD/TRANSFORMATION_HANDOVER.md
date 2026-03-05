# Transformation System Handover Summary

## Overview
This document summarizes the current state of the bone transformation system, including working components, known issues, and implementation strategy for fixing rotation axis mapping problems.

---

## 1. Core Constraints

### FBX Scale Factor (0.01)
- **Issue**: FBX rigs use a scale factor of 0.01, which results in:
  - Determinant values of approximately `1e-06` (since `0.01³ = 10⁻⁶`)
  - Near-singular matrices that require careful handling
  - Large compensated local values (e.g., 100+ for translation)

### Parent Rotation Offsets
- **Example**: The "Hips" bone has a 93-degree rotation offset
- **Impact**: This creates complex parent-child rotation relationships that must be accounted for when converting between world and local space

### Matrix Inversion Safety Thresholds
- **Current Threshold**: `1e-12` (in `scene.cpp` and `math3D.cpp`)
- **Rationale**: Must allow inversion of matrices with determinants around `1e-06` (expected for 0.01 FBX scales)
- **Location**: 
  - `scene.cpp` line ~1280: `const float determinantEpsilon = 1e-12f;`
  - `math3D.cpp`: `Matrix4f::Inverse()` function

---

## 2. What is WORKING

### Gram-Schmidt Orthonormalization
**Location**: `src/io/scene.cpp`, lines ~1287-1323

The orthonormalization process successfully eliminates jittering during rotation by removing the 0.01 scale effect from the parent's world rotation matrix before converting to quaternion.

**Implementation Snippet**:
```cpp
// SNAP START STATE: Orthonormalize the 3x3 basis to completely kill the 0.01 scale effect
// Extract the 3x3 rotation matrix (upper-left 3x3)
glm::mat3 rotationBasis = glm::mat3(parentWorldWorldSpace);

// Orthonormalize using Gram-Schmidt process
// Extract and normalize the three basis vectors
glm::vec3 xAxis = glm::normalize(glm::vec3(rotationBasis[0]));
glm::vec3 yAxis = glm::normalize(glm::vec3(rotationBasis[1]));
glm::vec3 zAxis = glm::normalize(glm::vec3(rotationBasis[2]));

// Ensure orthogonality: make yAxis perpendicular to xAxis
yAxis = glm::normalize(yAxis - glm::dot(yAxis, xAxis) * xAxis);

// Make zAxis perpendicular to both xAxis and yAxis
zAxis = glm::cross(xAxis, yAxis);
zAxis = glm::normalize(zAxis);

// Rebuild orthonormalized 3x3 matrix
glm::mat3 orthonormalBasis;
orthonormalBasis[0] = glm::vec3(xAxis);
orthonormalBasis[1] = glm::vec3(yAxis);
orthonormalBasis[2] = glm::vec3(zAxis);

// Convert orthonormalized 3x3 matrix to quaternion
mStartParentWorldQuat = glm::normalize(glm::quat_cast(orthonormalBasis));
```

**Why It Works**:
- Removes scale contamination from rotation calculations
- Ensures numerical stability by creating a true rotation matrix
- Prevents jittering caused by scale noise in quaternion conversions

### Static Reference Capture (OnMouseDown)
**Location**: `src/io/scene.cpp`, lines ~1174-1350

The system correctly captures static references at the moment of mouse click:
- `mStartLocalQuat`: Initial node local rotation
- `mStartParentWorldQuat`: Parent's world rotation (orthonormalized)
- `mWorldRotationAxis`: Active gizmo axis in world space (X, Y, or Z)
- `mLocalRotationAxis`: Active axis converted to local space
- `mInitialGizmoRotation`: Initial gizmo world rotation for delta calculation

**Key Principle**: These values are captured **ONCE** at `OnMouseDown` and never updated during drag, preventing feedback loops.

---

## 3. What is BROKEN

### Axis Mapping/Projection Issues
**Location**: `src/io/scene.cpp`, lines ~1485-1555

**Symptoms**:
1. Rotating the gizmo on one axis (e.g., Red/X axis) results in rotation on the wrong bone axis
2. Diagonal bending occurs when rotating a single axis
3. The bone does not follow the gizmo 1:1 along the intended axis

**Current Implementation** (Problematic):
```cpp
// Calculate relative rotation delta
glm::quat worldDeltaQuat = glm::normalize(currentGizmoWorldRotation * glm::inverse(mInitialGizmoRotation));

// Extract rotation angle around the active axis
// ... (complex projection logic using perpendicular vectors)

// Construct delta quaternion
glm::quat deltaQuat = glm::angleAxis(deltaAngle, mLocalRotationAxis);

// Apply to start position
glm::quat newLocalQuat = glm::normalize(deltaQuat * mStartLocalQuat);
```

**Root Cause Analysis**:
1. **Axis Detection**: The code identifies the active axis based on ImGuizmo flags (`ROTATE_X`, `ROTATE_Y`, `ROTATE_Z`), but the gizmo's world-space axis may not align with the bone's local-space axis after parent rotation transformation.

2. **Angle Extraction**: The current method of extracting the angle around the active axis uses a complex projection onto a perpendicular vector, which may not accurately capture the rotation component along the intended axis.

3. **Local Space Conversion**: While `mLocalRotationAxis` is correctly converted from world space to local space using `inverse(mStartParentWorldQuat) * World_Axis_Vector`, the angle extraction may not be preserving the correct rotation direction.

4. **Quaternion Multiplication Order**: The current order (`deltaQuat * mStartLocalQuat`) may be inverted. The code includes a comment suggesting to try `mStartLocalQuat * deltaQuat` if the direction is wrong.

---

## 4. Implementation Strategy

### Goal
Implement a robust "Axis-Angle Projection" or "Reference-Based Delta" method that:
1. Correctly maps gizmo axis rotations to bone axis rotations
2. Eliminates diagonal bending and axis misalignment
3. Ensures 1:1 rotation following the gizmo along the intended axis

### Recommended Approach

#### Option A: Direct Axis-Angle Extraction
1. **OnMouseDown**: Capture static references (already implemented)
2. **OnDrag**: 
   - Calculate the rotation delta quaternion: `worldDeltaQuat = currentGizmoRotation * inverse(initialGizmoRotation)`
   - Extract the axis-angle representation: `glm::axis(worldDeltaQuat)` and `glm::angle(worldDeltaQuat)`
   - Project the axis onto the world rotation axis to get the angle component: `angle = dot(axis, worldAxis) * angle`
   - Convert the angle to local space using the local rotation axis
   - Construct delta quaternion: `deltaQuat = glm::angleAxis(localAngle, mLocalRotationAxis)`
   - Apply: `newLocalQuat = deltaQuat * mStartLocalQuat` (or reverse order if inverted)

#### Option B: Euler Angle Component Extraction
1. **OnMouseDown**: Capture static references (already implemented)
2. **OnDrag**:
   - Calculate rotation delta: `worldDeltaQuat = currentGizmoRotation * inverse(initialGizmoRotation)`
   - Convert to Euler angles: `eulerDelta = glm::eulerAngles(worldDeltaQuat)`
   - Extract the component corresponding to the active axis (X, Y, or Z)
   - Apply this angle delta to the local rotation using the local axis

#### Option C: Mouse Movement-Based Angle Calculation
1. **OnMouseDown**: Capture mouse position and initial gizmo rotation
2. **OnDrag**:
   - Calculate mouse movement delta (in screen space or arcball space)
   - Convert mouse delta to rotation angle around the active axis
   - Apply angle delta directly to local rotation using axis-angle quaternion

### Key Principles
- **Static References**: All initial values must be captured ONCE at `OnMouseDown` and never updated during drag
- **No Feedback Loop**: Do not read from PropertyPanel or bone transforms during drag
- **Quaternion-Only Workflow**: Perform all calculations using quaternions, convert to Euler only for UI display
- **Normalization**: Normalize all quaternions every frame to prevent numerical drift

---

## 5. Key Files Modified

### Primary Files
1. **`src/io/scene.h`** (lines ~214-221)
   - Rotation reference state variables:
     - `mStartParentWorldQuat`: Parent's world rotation (orthonormalized)
     - `mStartLocalQuat`: Initial node local rotation
     - `mWorldRotationAxis`: Active rotation axis in world space
     - `mLocalRotationAxis`: Active rotation axis in local space
     - `mInitialGizmoRotation`: Initial gizmo world rotation
     - `mCurrentLocalRotationQuat`: Current local rotation during drag

2. **`src/io/scene.cpp`**
   - **OnMouseDown Logic** (lines ~1174-1350):
     - Static reference capture
     - Gram-Schmidt orthonormalization
     - World-to-local axis conversion
   - **OnDrag Rotation Logic** (lines ~1485-1555):
     - Axis-angle projection calculation (currently broken)
     - Delta quaternion construction
     - Local rotation update
   - **OnMouseUp Logic** (lines ~1375-1401):
     - Final value commit
     - State reset

### Supporting Files
3. **`src/graphics/math3D.cpp`**
   - `Matrix4f::Inverse()`: Determinant safety threshold set to `1e-12`
   - Matrix inversion utilities used for parent world matrix calculations

4. **`src/io/ui/property_panel.cpp`**
   - PropertyPanel integration for displaying Euler angles
   - Bone transform storage (`mBoneTranslations`, `mBoneRotations`, `mBoneScales`)

---

## 6. Mathematical Context

### Coordinate Space Hierarchy
```
World Space (Gizmo)
    ↓ (RootNode Transform)
Model Space (FBX Hierarchy)
    ↓ (Parent Chain)
Local Space (Bone)
```

### Transformation Pipeline
1. **Gizmo Manipulation**: User drags gizmo in world space
2. **World-to-Model Conversion**: Remove RootNode transform to get model-space values
3. **Model-to-Local Conversion**: Use parent's world matrix (in model space) to convert to local space
4. **Local Update**: Apply new local transform to bone
5. **Matrix Update**: Call `node->updateMatrices()` to propagate changes

### Key Formulas

**Parent World Matrix (with RootNode)**:
```
Parent_World_WorldSpace = RootNode_Transform × Parent_World_ModelSpace
```

**World-to-Local Conversion (Translation)**:
```
Local_Translation = Inverse(Parent_World_WorldSpace) × World_Position
```

**World-to-Local Conversion (Rotation)**:
```
Local_Rotation = Inverse(Parent_World_Rotation) × World_Rotation
```

**Axis-Angle Quaternion**:
```
q = angleAxis(θ, axis)
```

**Quaternion Multiplication Order**:
- `q1 * q2`: Apply `q2` first, then `q1`
- For local rotation: `deltaQuat * startQuat` applies delta to start rotation

---

## 7. Testing Scenarios

### Test Case 1: Simple Rotation (No Parent Offset)
- **Setup**: Select a bone with no parent rotation offset
- **Action**: Rotate gizmo's Red (X) axis
- **Expected**: Bone rotates around its local X axis only
- **Current Result**: ❌ May rotate on wrong axis or diagonally

### Test Case 2: Parent Rotation Offset (Hips → Spine)
- **Setup**: Select "Spine" bone (parent "Hips" has 93° rotation)
- **Action**: Rotate gizmo's Red (X) axis
- **Expected**: Spine rotates around its local X axis (which is rotated 93° from world X)
- **Current Result**: ❌ Diagonal bending or wrong axis rotation

### Test Case 3: Multiple Axes
- **Setup**: Select any bone
- **Action**: Rotate gizmo's Green (Y) axis, then Blue (Z) axis
- **Expected**: Each rotation affects only the corresponding local axis
- **Current Result**: ❌ Axis confusion and diagonal movement

---

## 8. Next Steps

1. **Debug Axis Detection**: Verify that `mWorldRotationAxis` correctly identifies the active gizmo axis
2. **Fix Angle Extraction**: Implement a more robust method to extract the rotation angle around the active axis
3. **Test Multiplication Order**: Try both `deltaQuat * startQuat` and `startQuat * deltaQuat` to determine correct order
4. **Add Debug Output**: Print axis vectors, angles, and quaternions during rotation to diagnose the issue
5. **Validate Local Axis Conversion**: Ensure `mLocalRotationAxis` is correctly transformed from world space

---

## 9. Notes for New Session

- The Gram-Schmidt orthonormalization is **working correctly** and should be preserved
- The static reference capture system is **correctly implemented** and prevents feedback loops
- The **axis-angle projection logic** needs to be rewritten or significantly refined
- Consider using ImGuizmo's internal rotation delta calculation if available, rather than extracting from quaternions
- The 0.01 FBX scale and parent rotation offsets are the primary sources of complexity

---

**Last Updated**: Current session  
**Status**: Axis mapping/projection needs fixing, core infrastructure is solid
