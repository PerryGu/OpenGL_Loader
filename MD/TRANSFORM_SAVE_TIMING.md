# Transform Save Timing - Detailed Explanation

## When Transforms Are Saved to Memory

### 1. **When User Changes Slider Values (UI)**
**Location:** `src/io/ui/property_panel.cpp` - `propertyPanel()` method

**Trigger:** User drags sliders in the Transforms window

**Process:**
```cpp
// Every frame, check if slider values changed
if (translationChanged) {
    mBoneTranslations[mSelectedNod_name] = mSliderTranslations;  // SAVED IMMEDIATELY
}
```

**Timing:** Saved **immediately** when values change (every frame while dragging)

**Example:**
- User drags Translation X slider from 0.00 to 10.00
- As soon as value changes (detected via epsilon comparison), saved to `mBoneTranslations["RootNode"]`
- Happens **continuously** while dragging

### 2. **When Gizmo Manipulates**
**Location:** `src/io/scene.cpp` - gizmo manipulation handler

**Trigger:** User drags gizmo (translate/rotate/scale)

**Process:**
```cpp
if (manipulated) {
    // Gizmo updates PropertyPanel
    mPropertyPanel.setSliderTranslations(translation);
    // Inside setSliderTranslations():
    mBoneTranslations[mSelectedNod_name] = translations;  // SAVED IMMEDIATELY
}
```

**Timing:** Saved **immediately** when gizmo is manipulated (every frame while dragging)

**Example:**
- User drags gizmo to move character
- Every frame, gizmo calls `setSliderTranslations()` → saves to bone maps immediately
- Happens **continuously** while dragging gizmo

### 3. **When Switching Bones/Nodes**
**Location:** `src/io/ui/property_panel.cpp` - `setSelectedNode()` method

**Trigger:** User selects a different bone/node in Outliner

**Process:**
```cpp
void PropertyPanel::setSelectedNode(std::string selectedNode) {
    if (selectedNode != mSelectedNod_name) {
        // SAVE CURRENT BONE'S VALUES BEFORE SWITCHING
        if (!mSelectedNod_name.empty()) {
            mBoneRotations[mSelectedNod_name] = mSliderRotations;      // SAVED
            mBoneTranslations[mSelectedNod_name] = mSliderTranslations; // SAVED
            mBoneScales[mSelectedNod_name] = mSliderScales;          // SAVED
        }
        
        // Switch to new bone
        mSelectedNod_name = selectedNode;
        
        // LOAD NEW BONE'S VALUES (or defaults if not previously set)
        auto transIt = mBoneTranslations.find(selectedNode);
        if (transIt != mBoneTranslations.end()) {
            mSliderTranslations = transIt->second;  // Load saved value
        } else {
            mSliderTranslations = glm::vec3(0.0f, 0.0f, 0.0f);  // Default
        }
    }
}
```

**Timing:** Saved **once** when switching (before loading new bone's values)

**Example:**
- User has "RootNode" selected with translation (10, 0, 0)
- User clicks "LeftArm" in Outliner
- "RootNode" values saved: `mBoneTranslations["RootNode"] = (10, 0, 0)`
- "LeftArm" values loaded (or defaults if first time)

## The Problem: Character 2 Jumping to Character 1's Position

### Scenario
1. **Character 1 loaded** → RootNode name: "RootNode"
2. **Character 1 moved** → Gizmo updates → Saved to `mBoneTranslations["RootNode"] = (10, 0, 0)`
3. **Character 2 loaded** → RootNode name: "RootNode01" (auto-renamed)
4. **Character 2 selected** → Should initialize from model.pos (0, 0, 0)

### What's Happening

**When Character 2 is selected:**

1. `setSelectionToRoot(1)` called → Outliner's `mSelectedNod_name` = "RootNode01"
2. `setSelectedNode("RootNode01")` called → This is the problem!

**Inside `setSelectedNode()`:**
```cpp
// Saves CURRENT bone's values (which is still "RootNode" from character 1!)
if (!mSelectedNod_name.empty()) {
    mBoneTranslations[mSelectedNod_name] = mSliderTranslations;  // Saves "RootNode" values
}
// Then switches
mSelectedNod_name = "RootNode01";
// Then loads "RootNode01" - doesn't exist, so loads defaults (0,0,0)
```

**The Issue:**
- When `setSelectedNode("RootNode01")` is called, PropertyPanel's `mSelectedNod_name` is still "RootNode" (from character 1)
- It saves "RootNode" values (which are character 1's position)
- Then switches to "RootNode01" and loads defaults
- But the sliders now have (0,0,0) which is correct

**Then `initializeRootNodeFromModel()` is called:**
- Checks if "RootNode01" has values in bone maps → it doesn't (we just loaded defaults, didn't save them)
- Should initialize from model.pos (0,0,0) → saves to `mBoneTranslations["RootNode01"] = (0,0,0)`

**But wait - the model is already being rendered with PropertyPanel values!**

The problem is that when RootNode is selected, the model's transform comes from PropertyPanel values (via `rootNodeModelMatrix`). So even though we initialize correctly, the model might already be positioned incorrectly because:

1. When character 2 is first selected, `setSelectedNode()` loads defaults (0,0,0) into sliders
2. These slider values are immediately used to build `rootNodeModelMatrix`
3. Model renders at (0,0,0) - correct!
4. But then `initializeRootNodeFromModel()` is called, which should also set (0,0,0)

Actually, I think the real issue might be that when character 2 is selected, the PropertyPanel sliders might still have character 1's values from before the switch!

Let me check the order of operations more carefully...
