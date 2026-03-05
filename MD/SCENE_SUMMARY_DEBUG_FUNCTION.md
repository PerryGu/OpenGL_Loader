# Scene Summary Debug Function

## Date
Added: 2024

## Purpose

Added a comprehensive debug function `printSceneSummary()` that displays an organized summary of all loaded models in the scene after each file load. This helps diagnose issues with model positioning and transform inheritance.

## Function Details

### Location
- **Declaration:** `src/main.cpp` line 70
- **Implementation:** `src/main.cpp` lines 80-200
- **Called:** `src/main.cpp` line 528 (after each model load)

### When It Runs
Automatically called after each new FBX file is loaded, providing a complete snapshot of the scene state.

## Output Format

The function prints a formatted summary with the following sections:

### Header
```
╔═══════════════════════════════════════════════════════════════════════════════╗
║                    SCENE SUMMARY - ALL LOADED MODELS                         ║
╚═══════════════════════════════════════════════════════════════════════════════╝

Total Models in Scene: <count>
```

### Per-Model Information

For each model, it displays:

1. **Model Header**
   - Model index
   - Selection status ([SELECTED] if currently selected)

2. **Basic Info**
   - Display name (filename)
   - Full file path
   - RootNode name (original or renamed, e.g., "RootNode" or "RootNode01")
   - Rig Root name (e.g., "Rig_GRP" or [NONE])

3. **Model Transform**
   - Position (model.pos)
   - Rotation (model.rotation quaternion)
   - Scale (model.size)

4. **PropertyPanel Transforms**
   - RootNode translation from bone maps
   - Rig Root translation from bone maps (if different from RootNode)
   - Shows [NOT FOUND] if transforms don't exist

5. **Animation State**
   - FPS
   - Current animation time
   - Is playing status

### Bone Map Summary

At the end, displays all entries in the PropertyPanel bone maps:
```
┌───────────────────────────────────────────────────────────────────────────────┐
│ ALL BONE MAP ENTRIES (PropertyPanel):                                        │
├───────────────────────────────────────────────────────────────────────────────┤
│   'RootNode' -> (20.00, 0.00, 0.00)                                          │
│   'RootNode01' -> (0.00, 0.00, 0.00)                                         │
│   'Rig_GRP' -> (10.00, 5.00, 0.00)                                           │
└───────────────────────────────────────────────────────────────────────────────┘
```

## Example Output

```
╔═══════════════════════════════════════════════════════════════════════════════╗
║                    SCENE SUMMARY - ALL LOADED MODELS                         ║
╚═══════════════════════════════════════════════════════════════════════════════╝

Total Models in Scene: 2

┌───────────────────────────────────────────────────────────────────────────────┐
│ MODEL 0 [SELECTED]                                                            │
├───────────────────────────────────────────────────────────────────────────────┤
│ Display Name: astroBoy_walk_Maya.fbx                                         │
│ File Path: F:\...\astroBoy_walk_Maya.fbx                                     │
│ RootNode Name: 'RootNode'                                                    │
│ Rig Root Name: 'Rig_GRP'                                                     │
├───────────────────────────────────────────────────────────────────────────────┤
│ MODEL TRANSFORM (model.pos/rotation/size):                                   │
│   Position:     (0.00, 0.00, 0.00)                                          │
│   Rotation:     (0.00, 0.00, 0.00, 1.00)                                    │
│   Scale:        (1.00, 1.00, 1.00)                                          │
├───────────────────────────────────────────────────────────────────────────────┤
│ PROPERTYPANEL TRANSFORMS (from bone maps):                                   │
│   RootNode 'RootNode' Translation: (20.00, 0.00, 0.00)                       │
│   Rig Root 'Rig_GRP' Translation: [NOT FOUND]                                │
├───────────────────────────────────────────────────────────────────────────────┤
│ ANIMATION STATE:                                                              │
│   FPS: 30                                                                     │
│   Animation Time: 0.00s                                                      │
│   Is Playing: NO                                                             │
└───────────────────────────────────────────────────────────────────────────────┘

┌───────────────────────────────────────────────────────────────────────────────┐
│ MODEL 1                                                                        │
├───────────────────────────────────────────────────────────────────────────────┤
│ Display Name: Walk.fbx                                                        │
│ File Path: F:\...\Walk.fbx                                                   │
│ RootNode Name: 'RootNode01'                                                  │
│ Rig Root Name: 'Rig_GRP'                                                     │
├───────────────────────────────────────────────────────────────────────────────┤
│ MODEL TRANSFORM (model.pos/rotation/size):                                   │
│   Position:     (0.00, 0.00, 0.00)                                          │
│   Rotation:     (0.00, 0.00, 0.00, 1.00)                                    │
│   Scale:        (1.00, 1.00, 1.00)                                          │
├───────────────────────────────────────────────────────────────────────────────┤
│ PROPERTYPANEL TRANSFORMS (from bone maps):                                   │
│   RootNode 'RootNode01' Translation: [NOT FOUND - will use (0,0,0)]          │
│   Rig Root 'Rig_GRP' Translation: [NOT FOUND]                                │
├───────────────────────────────────────────────────────────────────────────────┤
│ ANIMATION STATE:                                                              │
│   FPS: 30                                                                     │
│   Animation Time: 0.00s                                                      │
│   Is Playing: NO                                                             │
└───────────────────────────────────────────────────────────────────────────────┘

┌───────────────────────────────────────────────────────────────────────────────┐
│ ALL BONE MAP ENTRIES (PropertyPanel):                                        │
├───────────────────────────────────────────────────────────────────────────────┤
│   'RootNode' -> (20.00, 0.00, 0.00)                                          │
└───────────────────────────────────────────────────────────────────────────────┘

╚═══════════════════════════════════════════════════════════════════════════════╝
```

## What to Look For

### Red Flags (Indicates Bug):

1. **Wrong Transforms for New Model:**
   ```
   │ MODEL 1
   │ RootNode 'RootNode01' Translation: (20.00, 0.00, 0.00)  ❌ Should be [NOT FOUND]
   ```
   This indicates transforms exist for the new model when they shouldn't.

2. **Model Transform Not Reset:**
   ```
   │ MODEL TRANSFORM:
   │   Position:     (20.00, 0.00, 0.00)  ❌ Should be (0.00, 0.00, 0.00)
   ```
   Model's pos should be (0,0,0) after initialization.

3. **Wrong Name in Bone Maps:**
   ```
   │ ALL BONE MAP ENTRIES:
   │   'RootNode' -> (20.00, 0.00, 0.00)
   │   'RootNode' -> (0.00, 0.00, 0.00)  ❌ Duplicate! Should be 'RootNode01'
   ```
   Both models using same name indicates renaming didn't work.

### Expected Behavior (Correct):

1. **Unique Names:**
   ```
   │ MODEL 0: RootNode Name: 'RootNode'
   │ MODEL 1: RootNode Name: 'RootNode01'  ✅ Correctly renamed
   ```

2. **No Transforms for New Model:**
   ```
   │ MODEL 1:
   │   RootNode 'RootNode01' Translation: [NOT FOUND - will use (0,0,0)]  ✅
   ```

3. **Model at Origin:**
   ```
   │ MODEL TRANSFORM:
   │   Position:     (0.00, 0.00, 0.00)  ✅
   ```

## Benefits

1. **Complete Overview:** See all models and their states at once
2. **Easy Comparison:** Compare model positions, transforms, and names side-by-side
3. **Bone Map Visibility:** See exactly what transforms are stored for which names
4. **Debugging Aid:** Quickly identify which model has incorrect transforms
5. **State Verification:** Verify renaming worked correctly
6. **Transform Tracking:** See if transforms are being inherited incorrectly

## Usage

The function runs automatically after each model load. No manual action required.

To use for debugging:
1. Load Model 0
2. Move it (e.g., to X=20)
3. Load Model 1
4. Check the scene summary output
5. Compare Model 0 and Model 1's transforms
6. Identify if Model 1 inherited Model 0's transforms incorrectly

## Technical Details

- Uses `ModelManager::getModelCount()` to iterate through all models
- Uses `Outliner::getRootNodeName()` to get renamed names
- Uses `Outliner::getRigRootName()` to get rig root names
- Accesses `PropertyPanel::getAllBoneTranslations()` to check stored transforms
- Formatted with box-drawing characters for readability
- Uses `std::setprecision(2)` for consistent number formatting

## Files Modified

1. **`src/main.cpp`**
   - Added `#include <iomanip>` for `std::setprecision`
   - Added function declaration
   - Added function implementation
   - Added function call after model loading

## Summary

This debug function provides a comprehensive, organized view of all models in the scene, making it easy to diagnose transform inheritance issues and verify that renaming and initialization are working correctly.
