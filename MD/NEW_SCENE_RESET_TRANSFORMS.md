# New Scene Reset Transforms Implementation

## Overview
This document describes the implementation of automatic PropertyPanel transform reset when "New Scene" is triggered. This ensures that when a new character is imported after clearing the scene, it won't inherit transform values (rotations, translations, scales) from the previous character.

## Date
Implementation Date: 2026-02-09

## Problem Statement
When using "New Scene" to clear the current scene and import a new character:
- The PropertyPanel Transforms window retained values from the previous character
- New characters would inherit rotations, translations, and scales from the previous character
- This caused unexpected behavior where new characters appeared with transforms they shouldn't have
- Users had to manually reset transforms or risk incorrect character poses

## Solution Applied

### 1. Added `clearAllTransforms()` Method to PropertyPanel

**File:** `src/io/ui/property_panel.h`

**Added Method Declaration:**
```cpp
// Clear all transform values (for New Scene)
// This resets all bone rotations, translations, scales, current sliders, and selected node
// Ensures that when a new character is imported, it won't inherit previous character's transform values
void clearAllTransforms();
```

**File:** `src/io/ui/property_panel.cpp`

**Implementation:**
```cpp
void PropertyPanel::clearAllTransforms()
{
    // Clear all saved bone transforms (rotations, translations, scales)
    mBoneRotations.clear();
    mBoneTranslations.clear();
    mBoneScales.clear();
    
    // Reset current slider values to defaults
    mSliderRotations = glm::vec3(0.0f, 0.0f, 0.0f);
    mSliderTranslations = glm::vec3(0.0f, 0.0f, 0.0f);
    mSliderScales = glm::vec3(1.0f, 1.0f, 1.0f);
    
    // Clear selected node name (no bone/node selected)
    mSelectedNod_name = "";
    mPreviousSelectedNode = "";
    
    // Clear reset request flag (if it was set)
    mResetAllBonesRequested = false;
    
    std::cout << "[PropertyPanel] Cleared all transforms for New Scene - all values reset to defaults" << std::endl;
}
```

### 2. Integrated with New Scene Handler

**File:** `src/main.cpp`

**Added Call in New Scene Handler:**
```cpp
//-- Check if scene should be cleared (New Scene) ----
if (scene.isClearSceneRequested()) {
    // Clear all models
    modelManager.clearAll();
    
    // ... other reset operations ...
    
    // Clear all PropertyPanel transform values (rotations, translations, scales)
    // This ensures that when a new character is imported, it won't inherit
    // the previous character's transform values from the Transforms window
    scene.mPropertyPanel.clearAllTransforms();
    
    // ... rest of New Scene handler ...
}
```

## How It Works

### Flow Diagram

```
User clicks "New Scene"
    ↓
Scene::clearScene() called
    ↓
mClearSceneRequested = true
    ↓
Main loop detects isClearSceneRequested()
    ↓
    ├─ Clear all models (modelManager.clearAll())
    ├─ Reset animation state
    ├─ Clear outliner
    ├─ Clear PropertyPanel transforms ← NEW
    │   ├─ Clear all bone rotations map
    │   ├─ Clear all bone translations map
    │   ├─ Clear all bone scales map
    │   ├─ Reset slider values to defaults
    │   └─ Clear selected node name
    └─ Reset other scene state
    ↓
New Scene ready - clean state
    ↓
User imports new character
    ↓
New character starts with default transforms (0,0,0 for rotation/translation, 1,1,1 for scale)
```

### What Gets Reset

When "New Scene" is triggered, the following PropertyPanel values are reset:

1. **All Bone Rotations Map** - Cleared (no saved rotations)
2. **All Bone Translations Map** - Cleared (no saved translations)
3. **All Bone Scales Map** - Cleared (no saved scales)
4. **Current Slider Values**:
   - Rotations: `(0.0, 0.0, 0.0)`
   - Translations: `(0.0, 0.0, 0.0)`
   - Scales: `(1.0, 1.0, 1.0)`
5. **Selected Node Name** - Cleared (no bone/node selected)
6. **Reset Request Flag** - Cleared (if it was set)

## Benefits

1. **Clean State**: New characters start with default transforms, not inherited values
2. **Predictable Behavior**: Users can expect new characters to appear in their default pose
3. **No Manual Reset Required**: Transforms are automatically cleared when starting a new scene
4. **Prevents Confusion**: Eliminates cases where new characters appear with unexpected poses

## Testing Checklist

- [ ] Load a character model
- [ ] Modify some bone transforms (rotations, translations, scales)
- [ ] Click "New Scene" (File → New Scene)
- [ ] Verify PropertyPanel Transforms window shows:
  - Rotations: (0.00, 0.00, 0.00)
  - Translations: (0.00, 0.00, 0.00)
  - Scales: (1.00, 1.00, 1.00)
- [ ] Import a new character
- [ ] Verify new character appears in default pose (no inherited transforms)
- [ ] Verify console shows: "[PropertyPanel] Cleared all transforms for New Scene - all values reset to defaults"

## Related Features

- **Reset Current Bone**: Resets only the currently selected bone (doesn't clear all)
- **Reset All Bones**: Resets all bones but keeps selected node (used during active session)
- **New Scene**: Clears everything including PropertyPanel transforms (used when starting fresh)

## Technical Notes

- The `clearAllTransforms()` method is more comprehensive than `resetAllBones()`:
  - `resetAllBones()`: Clears maps and resets sliders, but keeps selected node and sets reset flag
  - `clearAllTransforms()`: Clears everything including selected node and reset flag (complete reset)
- This ensures a completely clean state for new scenes
- The method is called synchronously during the New Scene handler, so transforms are cleared before any new models are loaded

## Files Modified

1. `src/io/ui/property_panel.h` - Added `clearAllTransforms()` method declaration
2. `src/io/ui/property_panel.cpp` - Implemented `clearAllTransforms()` method
3. `src/main.cpp` - Added call to `clearAllTransforms()` in New Scene handler

## Future Enhancements

Potential improvements:
- Add confirmation dialog before clearing scene (optional)
- Add option to preserve transforms if desired (advanced users)
- Add undo/redo support for scene operations
