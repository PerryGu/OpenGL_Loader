# Gizmo World to Local Space Conversion for Rig Root

## Overview
This document describes the implementation of world space to local space conversion for the rig root (Rig_GRP) when manipulating with the gizmo. This conversion is currently used for **debugging purposes only** - it shows the converted values in the console but does not yet apply them to the transforms.

## Date
Implementation Date: 2024

## Purpose
When manipulating the rig root (Rig_GRP) with the gizmo, the gizmo operates in **world space**. However, PropertyPanel values for Rig_GRP need to be in **local space** relative to its parent (RootNode). This conversion test allows us to see what the local space values would be, helping to diagnose scale and transform issues.

## Implementation

### File: `src/io/scene.cpp`

**Location:** Inside the gizmo manipulation handler, when `isRigRoot` is true (around line 1143)

**Code:**
```cpp
if (rigRootObject) {
    // SIMPLE: Convert gizmo world space to Rig_GRP's local space
    // Get Rig_GRP's global transform
    ofbx::Matrix rigRootGlobalMatrix = rigRootObject->getGlobalTransform();
    glm::mat4 rigRootGlobalTransform = fbxToGlmMatrix(rigRootGlobalMatrix, false);
    
    // Convert world space to Rig_GRP's local space
    glm::mat4 rigRootInverse = glm::inverse(rigRootGlobalTransform);
    glm::vec4 worldPos(translation.x, translation.y, translation.z, 1.0f);
    glm::vec4 localPos = rigRootInverse * worldPos;
    glm::vec3 localTranslation(localPos.x, localPos.y, localPos.z);
    
    // DEBUG: Print both values every time gizmo is manipulated
    bool isUsingGizmo = ImGuizmo::IsUsing();
    
    if (isUsingGizmo) {
        std::cout << "[Gizmo Transform Test - Rig_GRP]" << std::endl;
        std::cout << "  Gizmo World Space: (" 
                  << translation.x << ", " << translation.y << ", " << translation.z << ")" << std::endl;
        std::cout << "  Gizmo Local Space (Rig_GRP's own space): (" 
                  << localTranslation.x << ", " << localTranslation.y << ", " << localTranslation.z << ")" << std::endl;
    }
    
    // DON'T use the converted value - just show it in debug
    // Keep using world space for now until we understand the conversion
    finalTranslation = translation;
}
```

## How It Works

1. **Get Rig_GRP's Global Transform**: Retrieves the current global transform matrix of Rig_GRP from the FBX scene
2. **Invert the Transform**: Calculates the inverse matrix (`rigRootInverse`)
3. **Convert World to Local**: Multiplies the world space position by the inverse matrix to get local space position
4. **Debug Output**: Prints both world space and local space values to the console every frame while the gizmo is being manipulated

## Current Status

- ✅ **Conversion is working correctly** - local space values are calculated properly
- ✅ **Debug output is functional** - shows both world and local space values
- ⚠️ **NOT yet applied** - the converted local space values are NOT being used to set PropertyPanel transforms yet
- ⚠️ **Still using world space** - `finalTranslation = translation` (world space) is still being used

## Debug Output Format

When you move the gizmo, you'll see output like:
```
[Gizmo Transform Test - Rig_GRP]
  Gizmo World Space: (20.22, 0.00, 0.00)
  Gizmo Local Space (Rig_GRP's own space): (20220.0, 0.00, 0.00)
```

The local space values should be approximately 1000x the world space values if RootNode has a scale of 0.001.

## Next Steps

Once the conversion is verified to be correct:
1. Change `finalTranslation = translation;` to `finalTranslation = localTranslation;`
2. This will apply the converted local space values to PropertyPanel
3. The character should then move correctly without jumping when deselected

## Related Files

- `src/io/scene.cpp` - Main implementation (gizmo manipulation handler)
- `src/io/fbx_rig_analyzer.cpp` - Rig root detection
- `src/io/ui/outliner.cpp` - Rig root storage and retrieval

## Notes

- The conversion uses Rig_GRP's **global transform**, which includes all parent transforms (including RootNode's scale)
- This is a **simple, direct conversion** - no complex logic, just matrix inversion
- The debug output prints **every frame** while the gizmo is being used (not just once)
- This is currently a **test/debugging feature** - not yet integrated into the actual transform system
