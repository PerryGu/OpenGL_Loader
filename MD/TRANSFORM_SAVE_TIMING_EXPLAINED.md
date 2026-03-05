# Transform Save Timing - Complete Explanation

## When Transforms Are Saved (To Memory)

### Summary: Three Save Triggers

1. **When slider values change** → Saved immediately (every frame while dragging)
2. **When gizmo manipulates** → Saved immediately (every frame while dragging)  
3. **When switching bones** → Saved once (before loading new bone's values)

---

## Detailed Breakdown

### 1. Slider Value Changes (UI)

**When:** User drags sliders in PropertyPanel Transforms window

**Where:** `src/io/ui/property_panel.cpp` - `propertyPanel()` method

**How:**
```cpp
// Every frame, check if slider value changed
glm::vec3 previousTranslation = mSliderTranslations;
nimgui::draw_vec3_widgetx("Translations", mSliderTranslations, 100.0f);

// Compare with previous value
if (std::abs(mSliderTranslations.x - previousTranslation.x) > epsilon) {
    translationChanged = true;
}

// Save immediately if changed
if (translationChanged) {
    mBoneTranslations[mSelectedNod_name] = mSliderTranslations;  // SAVED
}
```

**Timing:** Saved **continuously** while user is dragging (every frame)

**Example:**
- Frame 1: Slider = 0.00 → No change, not saved
- Frame 2: Slider = 0.50 → Changed! Saved to `mBoneTranslations["RootNode"] = (0.50, 0, 0)`
- Frame 3: Slider = 1.00 → Changed! Saved to `mBoneTranslations["RootNode"] = (1.00, 0, 0)`
- Frame 4: Slider = 1.50 → Changed! Saved to `mBoneTranslations["RootNode"] = (1.50, 0, 0)`
- ... continues while dragging

---

### 2. Gizmo Manipulation

**When:** User drags gizmo (translate/rotate/scale)

**Where:** `src/io/scene.cpp` - gizmo manipulation handler

**How:**
```cpp
if (manipulated) {
    // Decompose gizmo matrix
    glm::decompose(newModelMatrix, scale, rotation, translation, ...);
    
    // Update PropertyPanel
    mPropertyPanel.setSliderTranslations(translation);
    // Inside setSliderTranslations():
    mBoneTranslations[mSelectedNod_name] = translations;  // SAVED IMMEDIATELY
}
```

**Timing:** Saved **continuously** while user is dragging gizmo (every frame)

**Example:**
- Frame 1: Gizmo at (0, 0, 0) → Saved to `mBoneTranslations["RootNode"] = (0, 0, 0)`
- Frame 2: Gizmo at (0.1, 0, 0) → Saved to `mBoneTranslations["RootNode"] = (0.1, 0, 0)`
- Frame 3: Gizmo at (0.2, 0, 0) → Saved to `mBoneTranslations["RootNode"] = (0.2, 0, 0)`
- ... continues while dragging

---

### 3. Switching Bones/Nodes

**When:** User selects a different bone/node in Outliner

**Where:** `src/io/ui/property_panel.cpp` - `setSelectedNode()` method

**How:**
```cpp
void PropertyPanel::setSelectedNode(std::string selectedNode) {
    if (selectedNode != mSelectedNod_name) {
        // STEP 1: SAVE CURRENT BONE'S VALUES (before switching)
        if (!mSelectedNod_name.empty()) {
            mBoneRotations[mSelectedNod_name] = mSliderRotations;      // SAVED
            mBoneTranslations[mSelectedNod_name] = mSliderTranslations; // SAVED
            mBoneScales[mSelectedNod_name] = mSliderScales;          // SAVED
        }
        
        // STEP 2: SWITCH TO NEW BONE
        mSelectedNod_name = selectedNode;
        
        // STEP 3: LOAD NEW BONE'S VALUES (or defaults)
        auto transIt = mBoneTranslations.find(selectedNode);
        if (transIt != mBoneTranslations.end()) {
            mSliderTranslations = transIt->second;  // Load saved value
        } else {
            mSliderTranslations = glm::vec3(0.0f, 0.0f, 0.0f);  // Default (NOT saved)
        }
    }
}
```

**Timing:** Saved **once** when switching (before loading new bone's values)

**Important:** Default values loaded into sliders are **NOT saved** to bone maps until user changes them or `initializeRootNodeFromModel()` is called.

**Example:**
- User has "RootNode" selected with translation (10, 0, 0)
- User clicks "LeftArm" in Outliner
- "RootNode" values saved: `mBoneTranslations["RootNode"] = (10, 0, 0)`
- "LeftArm" values loaded: `mSliderTranslations = (0, 0, 0)` (default, not saved yet)

---

## The Bug: Character 2 Jumping to Character 1's Position

### The Problem Sequence

**Setup:**
- Character 1 loaded → RootNode name: "RootNode"
- Character 1 moved to (10, 0, 0) → Saved to `mBoneTranslations["RootNode"] = (10, 0, 0)`
- Character 2 loaded → RootNode name: "RootNode01" (auto-renamed)
- Character 2 appears at center (0, 0, 0) - correct!

**When Character 2 is selected:**

**Frame 1 (Selection Frame):**

1. **Line 507:** `setSelectedNode("RootNode01")` is called
   - PropertyPanel's `mSelectedNod_name` is still **"RootNode"** (from character 1)
   - Saves "RootNode" values: `mBoneTranslations["RootNode"] = (10, 0, 0)` (already saved, no change)
   - Switches `mSelectedNod_name` to "RootNode01"
   - Loads "RootNode01" from bone maps → **Doesn't exist!**
   - Loads defaults into sliders: `mSliderTranslations = (0, 0, 0)` ✅
   - **BUT:** These defaults are **NOT saved** to bone maps yet!

2. **Lines 588-640:** RootNode selection handler
   - Checks if "RootNode01" has values in bone maps → **No** (we just loaded defaults, didn't save)
   - Should initialize from `model.pos` (which is 0, 0, 0) ✅
   - Calls `initializeRootNodeFromModel(0, 0, 0)`
   - Saves to bone maps: `mBoneTranslations["RootNode01"] = (0, 0, 0)` ✅

3. **Lines 646-678:** Builds `rootNodeModelMatrix` from PropertyPanel sliders
   - Reads `mSliderTranslations` which should be (0, 0, 0) ✅
   - Builds matrix: `translate(0, 0, 0)` ✅

**This should work correctly!** But the user says it doesn't...

### Possible Issues

**Issue 1: Timing Problem**
- `setSelectedNode()` is called **every frame** at line 507
- If character 2 is selected, but PropertyPanel's name hasn't updated yet, it might save wrong values
- But we already fixed this by updating PropertyPanel's name before initialization

**Issue 2: Model.pos Already Modified**
- When character 1 is moved, maybe `model.pos` is also being set?
- But we reset `model.pos` to identity when RootNode is selected...
- Unless character 2's `model.pos` was somehow modified?

**Issue 3: Slider Values Not Updated**
- When `setSelectedNode("RootNode01")` loads defaults, sliders are set to (0,0,0)
- But maybe the rendering code is reading old slider values?
- Or maybe `initializeRootNodeFromModel()` is being called but the sliders aren't being updated?

Let me check if there's a case where `initializeRootNodeFromModel()` doesn't update the sliders if values already exist...

Actually, wait - I see it! In `initializeRootNodeFromModel()`, it checks if values already exist and returns early. But when `setSelectedNode()` loads defaults, those defaults are in the **sliders**, not in the **bone maps**. So `initializeRootNodeFromModel()` should still work.

Unless... oh! What if when character 2 is selected, the RootNode selection handler doesn't run because the model index check fails? Or what if it runs but `initializeRootNodeFromModel()` is called with the wrong model's pos?

Let me check the viewport selection code more carefully...
