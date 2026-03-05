# Transform System: Hierarchy Traversal and Coordinate Space Conversion

## Overview
This document describes the mathematical principles and implementation of the transform hierarchy system for coordinate space conversion. The system provides robust functions for traversing parent chains and converting between world space and local space using matrix mathematics.

## Date
Implementation Date: 2024

## Mathematical Principles

### Local Space vs World Space

**Local Space:**
- Transform relative to the parent node
- Position, rotation, and scale are defined in the parent's coordinate system
- Example: A bone's local transform is relative to its parent bone

**World Space:**
- Transform relative to the scene root (absolute position)
- Position, rotation, and scale are defined in the global coordinate system
- Example: A bone's world transform is its absolute position in the scene

### Transform Hierarchy

In a hierarchical transform system:
```
RootNode (world space origin)
  └── Rig_GRP (local space relative to RootNode)
      └── Bone1 (local space relative to Rig_GRP)
          └── Bone2 (local space relative to Bone1)
```

Each node's world transform is calculated by:
1. Starting from the root
2. Multiplying each parent's world matrix by the child's local matrix
3. Accumulating transformations down the hierarchy

### Matrix Mathematics

#### World Matrix Calculation

The world (absolute) transformation of a node is calculated by:

```
World_node = World_parent × Local_node
```

Where:
- `World_parent` is the accumulated transform from root to parent
- `Local_node` is the node's transform relative to its parent
- `World_node` is the node's absolute transform in the scene

This formula is applied recursively up the parent chain:
```
World_node = World_grandparent × Local_parent × Local_node
```

#### World to Local Conversion

To convert a world-space position or transform to local space relative to a parent:

```
Position_local = Inverse(World_parent) × Position_world
Matrix_local = Inverse(World_parent) × Matrix_world
```

Where:
- `World_parent` is the parent's world (absolute) transformation matrix
- `Inverse(World_parent)` undoes all parent transformations
- The result is in the parent's local coordinate system

**Why Matrix Inverse?**
- The inverse matrix "undoes" all transformations (translation, rotation, scale)
- Multiplying by the inverse converts from world space back to local space
- This correctly handles non-uniform scaling, rotations, and translations

## Implementation

### Functions

#### 1. `GetParentChain(ofbx::Object* targetNode)`

**Purpose:** Traverses up the hierarchy to collect all ancestor nodes.

**Returns:** Vector of parent nodes, ordered from root (index 0) to immediate parent (last index).

**Algorithm:**
1. Start from target node
2. Traverse up using `getParent()` repeatedly
3. Collect all parents in reverse order
4. Reverse the collection to get root-to-parent order

**Example:**
```cpp
// For hierarchy: RootNode -> Rig_GRP -> Bone1
// If targetNode is Bone1:
// Returns: [RootNode, Rig_GRP]
```

#### 2. `ComputeWorldMatrix(ofbx::Object* node, const glm::mat4& localMatrix)`

**Purpose:** Calculates the absolute transformation matrix of a node.

**Formula:** `World_node = World_parent × Local_node`

**Algorithm:**
1. If node has no parent, return local matrix (it's the root)
2. Get parent's local transform from FBX
3. Recursively compute parent's world matrix
4. Multiply: `parentWorld × localMatrix`

**Recursive Nature:**
- Calls itself for each parent up the chain
- Accumulates all transformations from root to node
- Handles any depth of hierarchy

**Example:**
```cpp
// For hierarchy: RootNode -> Rig_GRP -> Bone1
// Computing world matrix for Bone1:
// 1. Get Rig_GRP's world: World_Rig_GRP = World_RootNode × Local_Rig_GRP
// 2. Get Bone1's world: World_Bone1 = World_Rig_GRP × Local_Bone1
```

#### 3. `WorldToLocal(const glm::vec3& worldPos, ofbx::Object* parentNode)`

**Purpose:** Converts a world-space position to local space relative to a parent.

**Formula:** `Position_local = Inverse(World_parent) × Position_world`

**Algorithm:**
1. Get parent's world matrix using `ComputeWorldMatrix()`
2. Invert the parent's world matrix
3. Convert world position to homogeneous coordinates (add w=1)
4. Multiply: `parentWorldInverse × worldPosHomogeneous`
5. Extract 3D position from result

**Handles:**
- Non-uniform scaling
- Rotations
- Translations
- Combined transformations

#### 4. `WorldToLocalMatrix(const glm::mat4& worldMatrix, ofbx::Object* parentNode)`

**Purpose:** Converts a world-space transformation matrix to local space.

**Formula:** `Matrix_local = Inverse(World_parent) × Matrix_world`

**Algorithm:**
1. Get parent's world matrix using `ComputeWorldMatrix()`
2. Invert the parent's world matrix
3. Multiply: `parentWorldInverse × worldMatrix`

**Use Case:**
- Converting gizmo world-space transforms to local space
- Applying transforms relative to parent coordinate system

## Coordinate Space Conversion Flow

### Example: Converting Gizmo Transform to Rig_GRP Local Space

```
1. Gizmo provides world-space transform matrix
   └── World Matrix: Translation(24.82, 0, 0), Rotation(0, 0, 0), Scale(1, 1, 1)

2. Get Rig_GRP's parent (RootNode)
   └── Parent: RootNode

3. Compute RootNode's world matrix
   └── World_RootNode = Identity (root has no parent)

4. Invert RootNode's world matrix
   └── Inverse(World_RootNode) = Identity

5. Convert gizmo transform to local space
   └── Local = Inverse(World_RootNode) × World_Gizmo
   └── Local = Identity × World_Gizmo = World_Gizmo

Result: If parent is identity, local = world
```

### Example: With Non-Identity Parent

```
1. Gizmo provides world-space transform
   └── World Matrix: Translation(24.82, 0, 0), Scale(1, 1, 1)

2. Get parent's world matrix
   └── World_Parent = Translation(0, 0, 0), Scale(0.01, 0.01, 0.01)

3. Invert parent's world matrix
   └── Inverse(World_Parent) = Translation(0, 0, 0), Scale(100, 100, 100)

4. Convert to local space
   └── Local = Inverse(World_Parent) × World_Gizmo
   └── Local Translation = 24.82 × 100 = 2482.0

Result: Local space accounts for parent's scale
```

## Key Concepts

### Matrix Multiplication Order

**Important:** Matrix multiplication order matters!

- **World = Parent × Local**: Parent's transform is applied first, then local
- **Local = Inverse(Parent) × World**: Undo parent's transform, then apply world

### Inherited Transformations

All transformations are inherited through the hierarchy:
- **Translation**: Child's position is relative to parent's position
- **Rotation**: Child's rotation is relative to parent's rotation
- **Scale**: Child's scale is multiplied by parent's scale

The matrix inverse correctly handles all of these:
- Undoes parent's translation
- Undoes parent's rotation
- Undoes parent's scale (by inverting scale factors)

### Non-Uniform Scaling

The matrix inverse correctly handles non-uniform scaling:
- If parent has scale (0.01, 0.01, 0.01)
- Inverse has scale (100, 100, 100)
- World position (24.82, 0, 0) becomes local (2482.0, 0, 0)

## Usage Examples

### Getting Parent Chain

```cpp
ofbx::Object* rigRoot = /* get Rig_GRP */;
std::vector<ofbx::Object*> parents = GetParentChain(rigRoot);

// parents[0] = RootNode (if exists)
// parents[1] = Next parent (if exists)
// parents.back() = Immediate parent
```

### Computing World Matrix

```cpp
ofbx::Object* node = /* get node */;
ofbx::Matrix localFBX = node->getLocalTransform();
glm::mat4 local = fbxToGlmMatrix(localFBX, false);
glm::mat4 world = ComputeWorldMatrix(node, local);
```

### Converting World to Local

```cpp
// Convert position
glm::vec3 worldPos(24.82f, 0.0f, 0.0f);
ofbx::Object* parent = /* get parent */;
glm::vec3 localPos = WorldToLocal(worldPos, parent);

// Convert matrix
glm::mat4 worldMatrix = /* gizmo transform */;
glm::mat4 localMatrix = WorldToLocalMatrix(worldMatrix, parent);
```

## File Locations

- **Header:** `src/graphics/math3D.h`
- **Implementation:** `src/graphics/math3D.cpp`
- **Usage:** `src/io/scene.cpp` (gizmo transform test function)

## Dependencies

- **GLM:** All matrix and vector operations
- **openFBX:** For FBX object hierarchy (`ofbx::Object`)
- **Utils:** For FBX to GLM matrix conversion (`fbxToGlmMatrix`)

## Testing

The transform system is tested through the gizmo transform test function:
1. Manipulate gizmo in world space
2. Get parent chain for Rig_GRP
3. Convert gizmo transform to local space
4. Verify conversion accounts for parent transforms

## Future Enhancements

Potential improvements:
- **Caching:** Cache computed world matrices to avoid recalculation
- **Batch Operations:** Convert multiple transforms at once
- **Validation:** Verify matrix inverses are valid (non-singular)
- **Debug Visualization:** Visualize parent chain and transforms

## Scale Compensation for 1:1 Gizmo Movement

### Problem
When an object has a local scale (e.g., 0.01) in the FBX file, the gizmo movement doesn't match the visual movement 1:1. If the gizmo moves 32.3268 units in world space, but the object has a scale of 0.01, the visual movement would be 100x smaller (0.323268 units).

### Solution
To make gizmo movement match visual movement 1:1, we need to compensate for the object's local scale:

1. **Convert world space to parent's local space** using `WorldToLocalMatrix()`
2. **Account for object's own local scale** by dividing the translation by the scale
3. **Store compensated values** in PropertyPanel

**Formula:**
```
compensatedTranslation = localTranslation / objectLocalScale
```

**Example:**
- Gizmo world space: 32.3268
- Object local scale: 0.01
- Compensated translation: 32.3268 / 0.01 = 3232.68
- When applied with scale: 3232.68 * 0.01 = 32.3268 ✓ (matches gizmo)

### Implementation

**In Gizmo Manipulation (`src/io/scene.cpp`):**
```cpp
// Convert world space to parent's local space
glm::mat4 localMatrix = WorldToLocalMatrix(worldMatrix, parentObject);

// Decompose to get local translation
glm::vec3 localTranslation = /* from decomposed matrix */;

// Get object's FBX local scale
ofbx::Vec3 rigLocalScaling = rigRootObject->getLocalScaling();
glm::vec3 rigLocalScale(rigLocalScaling.x, rigLocalScaling.y, rigLocalScaling.z);

// Compensate for local scale
glm::vec3 compensatedTranslation = localTranslation;
compensatedTranslation.x /= rigLocalScale.x;
compensatedTranslation.y /= rigLocalScale.y;
compensatedTranslation.z /= rigLocalScale.z;

// Store compensated value in PropertyPanel
mPropertyPanel.setSliderTranslations(compensatedTranslation);
```

**In Rendering (`src/main.cpp`):**
```cpp
// Get FBX local scale
ofbx::Vec3 rigLocalScaling = rigRootObject->getLocalScaling();
glm::vec3 fbxLocalScale(rigLocalScaling.x, rigLocalScaling.y, rigLocalScaling.z);

// Build model matrix with FBX scale applied
rootNodeModelMatrix = glm::translate(rootNodeModelMatrix, rootTranslation);
rootNodeModelMatrix = rootNodeModelMatrix * glm::mat4_cast(rootRotation);
rootNodeModelMatrix = glm::scale(rootNodeModelMatrix, fbxLocalScale);  // Apply FBX scale
rootNodeModelMatrix = glm::scale(rootNodeModelMatrix, rootScale);  // Apply PropertyPanel scale
```

This ensures:
- Gizmo moves 32.3268 units in world space
- PropertyPanel stores 3232.68 (compensated)
- Rendering applies scale 0.01: 3232.68 * 0.01 = 32.3268
- Visual movement matches gizmo movement 1:1 ✓

## Summary

The transform system provides robust, mathematically correct functions for:
1. **Hierarchy Traversal:** Getting complete parent chains
2. **World Matrix Computation:** Calculating absolute transforms recursively
3. **Coordinate Space Conversion:** Converting between world and local space using matrix inverses
4. **Scale Compensation:** Accounting for object's local scale to ensure 1:1 gizmo-to-visual movement

All functions handle inherited transformations (scaling, rotation, translation) correctly through proper matrix mathematics, ensuring accurate coordinate space conversions regardless of hierarchy depth or transform complexity.
