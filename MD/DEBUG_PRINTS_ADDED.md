# Debug Prints Added for Model Loading Issue

## Date
Added: 2024

## Purpose

Added comprehensive debug output to trace why new models appear at the previous model's position instead of at the center (0,0,0).

## Debug Print Locations

### 1. Model Loading (`src/main.cpp` lines 357-400)

**When:** Immediately after a model is loaded

**Output:**
```
========================================
[DEBUG LOAD] Model Index: <index>
[DEBUG LOAD] File: <filepath>
[DEBUG LOAD] Model pos: (x, y, z)
[DEBUG LOAD] Model rotation: (x, y, z, w)
[DEBUG LOAD] Model size: (x, y, z)
[DEBUG LOAD] RootNode name: '<name>' (or 'RootNode01' if renamed)
[DEBUG LOAD] Rig Root name: '<name>' (e.g., 'Rig_GRP')
[DEBUG LOAD] WARNING: Transforms already exist for '<name>': (x, y, z)
  OR
[DEBUG LOAD] No transforms found for '<name>' (will initialize to 0,0,0)
========================================
```

**What to Check:**
- Is the RootNode name correctly renamed? (e.g., "RootNode01" for second model)
- Are transforms already found for the new model's name? (This would indicate the bug)
- What is the model's initial position? (Should be 0,0,0)

### 2. Rig Root Initialization Check (`src/main.cpp` lines 542-640)

**When:** When a rig root is selected (happens automatically when model is selected)

**Output:**
```
========================================
[DEBUG INIT] Rig Root Selection Check
[DEBUG INIT] rigRootModelIndex: <index>
[DEBUG INIT] Currently selected node: '<name>'
[DEBUG INIT] getRigRootName(<index>): '<name>'
[DEBUG INIT] getRootNodeName(<index>): '<name>'
[DEBUG INIT] Final rigRootNameForModel: '<name>'
[DEBUG INIT] Selected model pos: (x, y, z)
[DEBUG INIT] TRANSFORMS FOUND for '<name>': (x, y, z)
[DEBUG INIT] SKIPPING initialization (using existing transforms)
  OR
[DEBUG INIT] NO TRANSFORMS FOUND for '<name>'
[DEBUG INIT] INITIALIZING from model's current transform
[DEBUG INIT] Initializing with:
[DEBUG INIT]   Translation: (x, y, z)
[DEBUG INIT]   Rotation: (x, y, z, w)
[DEBUG INIT]   Scale: (x, y, z)
[DEBUG INIT] After initialization, PropertyPanel has:
[DEBUG INIT]   PropertyPanel Translation: (x, y, z)
[DEBUG INIT] Model pos/rotation/size reset to identity
========================================
```

**What to Check:**
- Is `rigRootNameForModel` the correct (renamed) name for the model being initialized?
- Are transforms found when they shouldn't be? (This indicates the bug)
- What values are being initialized? (Should be 0,0,0 for new models)

### 3. PropertyPanel Node Selection (`src/io/ui/property_panel.cpp` lines 159-230)

**When:** When `setSelectedNode()` is called (switching between nodes)

**Output:**
```
========================================
[DEBUG PROPERTYPANEL] setSelectedNode() called
[DEBUG PROPERTYPANEL] Previous node: '<name>'
[DEBUG PROPERTYPANEL] New node: '<name>'
[DEBUG PROPERTYPANEL] Saved previous node '<name>' transforms:
[DEBUG PROPERTYPANEL]   Translation: (x, y, z)
[DEBUG PROPERTYPANEL] Loaded translation from bone maps: (x, y, z)
  OR
[DEBUG PROPERTYPANEL] No translation found, using default (0,0,0)
[DEBUG PROPERTYPANEL] Final slider values for '<name>':
[DEBUG PROPERTYPANEL]   Translation: (x, y, z)
[DEBUG PROPERTYPANEL]   Rotation: (x, y, z)
[DEBUG PROPERTYPANEL]   Scale: (x, y, z)
========================================
```

**What to Check:**
- What node name is being loaded? (Should be the renamed name for new models)
- Are transforms loaded from bone maps when they shouldn't be?
- What are the final slider values? (Should be 0,0,0 for new models)

### 4. Viewport Selection (`src/main.cpp` lines 1251-1270)

**When:** When user clicks on a model in the viewport

**Output:**
```
========================================
[DEBUG VIEWPORT] VIEWPORT CLICK: Selected model <index>
[DEBUG VIEWPORT] Model pos: (x, y, z)
[DEBUG VIEWPORT] RootNode name for model <index>: '<name>'
[DEBUG VIEWPORT] After setSelectionToRoot, selected node: '<name>'
========================================
```

**What to Check:**
- Is the correct RootNode name being used when selecting a model?
- What is the model's position when selected?

## How to Use These Debug Prints

### Step 1: Load First Model
1. Load Model 0 (e.g., "Walk.fbx")
2. Check `[DEBUG LOAD]` output:
   - RootNode name should be "RootNode"
   - No transforms should be found (new model)
   - Model pos should be (0, 0, 0)

### Step 2: Move First Model
1. Move Model 0's RootNode using gizmo (e.g., to 20, 0, 0)
2. Check console - transforms should be saved

### Step 3: Load Second Model
1. Load Model 1 (e.g., "astroBoy_walk_Maya.fbx")
2. **CRITICAL:** Check `[DEBUG LOAD]` output:
   - RootNode name should be "RootNode01" (renamed!)
   - Check if transforms are found for "RootNode01"
   - If transforms ARE found, that's the bug! (Shouldn't exist for new model)
   - Model pos should be (0, 0, 0)

### Step 4: Check Initialization
1. When Model 1 is selected (automatically or manually), check `[DEBUG INIT]` output:
   - `rigRootNameForModel` should be "RootNode01" (not "RootNode")
   - Check if transforms are found for "RootNode01"
   - If transforms ARE found, that's the bug!
   - If no transforms found, initialization should proceed with (0,0,0)

### Step 5: Check PropertyPanel
1. When `setSelectedNode()` is called, check `[DEBUG PROPERTYPANEL]` output:
   - What node name is being loaded?
   - Are transforms loaded from bone maps?
   - What are the final slider values?

## What to Look For

### Red Flags (Indicates Bug):

1. **Wrong Name Being Checked:**
   ```
   [DEBUG INIT] Final rigRootNameForModel: 'RootNode'
   ```
   Should be "RootNode01" for second model!

2. **Transforms Found for New Model:**
   ```
   [DEBUG LOAD] WARNING: Transforms already exist for 'RootNode01': (20, 0, 0)
   ```
   Should NOT exist for a new model!

3. **Initialization Skipped:**
   ```
   [DEBUG INIT] TRANSFORMS FOUND for 'RootNode01': (20, 0, 0)
   [DEBUG INIT] SKIPPING initialization
   ```
   Should initialize to (0,0,0) for new models!

4. **Wrong Values Loaded:**
   ```
   [DEBUG PROPERTYPANEL] Loaded translation from bone maps: (20, 0, 0)
   ```
   Should be (0,0,0) for new models!

### Expected Behavior (Correct):

1. **Correct Renaming:**
   ```
   [DEBUG LOAD] RootNode name: 'RootNode01'
   ```

2. **No Transforms Found:**
   ```
   [DEBUG LOAD] No transforms found for 'RootNode01' (will initialize to 0,0,0)
   ```

3. **Initialization Proceeds:**
   ```
   [DEBUG INIT] NO TRANSFORMS FOUND for 'RootNode01'
   [DEBUG INIT] INITIALIZING from model's current transform
   [DEBUG INIT] Initializing with:
   [DEBUG INIT]   Translation: (0, 0, 0)
   ```

4. **Default Values Loaded:**
   ```
   [DEBUG PROPERTYPANEL] No translation found, using default (0,0,0)
   ```

## Next Steps

After running the application and checking the debug output:

1. **Identify the Problem:**
   - Which step shows unexpected behavior?
   - What name is being used incorrectly?
   - Where are transforms being found when they shouldn't be?

2. **Report Findings:**
   - Copy the relevant debug output
   - Note which model index is affected
   - Identify the exact point where the bug occurs

3. **Fix the Issue:**
   - Based on the debug output, we can identify:
     - If renaming isn't working correctly
     - If the wrong name is being used for initialization
     - If transforms are persisting incorrectly
     - If there's a timing issue (initialization happening before renaming)

## Summary

These debug prints will help us trace:
- ✅ Model loading and renaming
- ✅ Name resolution (which name is used for which model)
- ✅ Transform lookup (what transforms are found)
- ✅ Initialization flow (when and how models are initialized)
- ✅ PropertyPanel state (what values are loaded/saved)

With this information, we can pinpoint exactly where the bug occurs and fix it.
