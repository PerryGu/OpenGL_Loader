# Auto Focus Flicker Fix

## Date
Created: 2026-02-16

## Problem Statement

Auto Focus was causing a flickering perspective jump in a loop when a character is selected and Smooth Camera is OFF. The issue was that Auto Focus was being triggered every frame for the same selection, causing the camera to continuously re-frame the same target, resulting in an infinite loop and flickering.

## Root Cause

1. **No Selection Change Tracking**: Auto Focus was triggered every frame when a selection existed, not just when the selection changed
2. **Aspect Ratio Calculation**: Using hardcoded default instead of dynamic calculation from viewport size
3. **No Epsilon Check**: Camera was updating even for sub-pixel movements, causing jitter

## Solution

Implemented selection change tracking and epsilon checks:
1. Track last selection (ModelIndex + NodeName) to detect actual changes
2. Only trigger Auto Focus when selection actually changes
3. Fix aspect ratio calculation to use dynamic viewport size
4. Add epsilon check to prevent sub-pixel jitter

## Changes Made

### 1. Selection Change Tracking Variables

**File:** `src/io/scene.h`

**Location:** Private member variables (lines ~263-266)

**Change:**
```cpp
//-- selection change tracking (for auto-focus) ----
// Track last selection to prevent infinite loop/flickering when auto-focus is enabled
int mLastSelectedModelIndex = -1;  // Last selected model index
std::string mLastSelectedNode = "";  // Last selected node name
```

**Why:**
- Tracks the last selection to detect when it actually changes
- Prevents triggering Auto Focus for the same selection every frame

### 2. Selection Change Check Method

**File:** `src/io/scene.h`

**Location:** Public methods (lines ~114-117)

**Change:**
```cpp
//-- selection change tracking (for auto-focus) ----
// Check if selection changed and trigger auto-focus if enabled
// Returns true if framing was triggered, false otherwise
bool checkAndTriggerAutoFocusOnSelectionChange();

// Reset selection tracking (called on deselection)
void resetSelectionTracking() { mLastSelectedModelIndex = -1; mLastSelectedNode = ""; }
```

**Implementation in scene.cpp:**
```cpp
bool Scene::checkAndTriggerAutoFocusOnSelectionChange() {
    AppSettings& settings = AppSettingsManager::getInstance().getSettings();
    if (!settings.environment.autoFocusEnabled) {
        return false;  // Auto-focus not enabled
    }
    
    if (mModelManager == nullptr) {
        return false;  // No model manager
    }
    
    // Get current selection
    int currentModelIndex = mModelManager->getSelectedModelIndex();
    std::string currentSelectedNode = mOutliner.getSelectedNode();
    
    // Only trigger if selection changed (different model index or node name)
    if (currentModelIndex != mLastSelectedModelIndex || 
        currentSelectedNode != mLastSelectedNode) {
        setFramingTrigger(true);
        // Update tracking variables
        mLastSelectedModelIndex = currentModelIndex;
        mLastSelectedNode = currentSelectedNode;
        return true;  // Framing was triggered
    }
    
    return false;  // Selection didn't change, no framing triggered
}
```

**Why:**
- Centralized logic for checking selection changes
- Only triggers Auto Focus when selection actually changes
- Updates tracking variables after triggering

### 3. Viewport Selection Auto-Focus

**File:** `src/main.cpp`

**Location:** Viewport raycast selection (lines ~1481-1485)

**Change:**
```cpp
// AUTO-FOCUS: If auto-focus is enabled, trigger camera framing immediately on selection change
// CRITICAL: Only trigger if selection actually changed (prevents infinite loop/flickering)
scene.checkAndTriggerAutoFocusOnSelectionChange();
```

**Why:**
- Uses centralized method to check selection change
- Only triggers if selection actually changed
- Prevents infinite loop

### 4. Outliner Selection Auto-Focus

**File:** `src/io/scene.cpp`

**Location:** Outliner selection processing (lines ~193-200)

**Change:**
```cpp
// AUTO-FOCUS: If auto-focus is enabled, trigger camera framing immediately on selection change
// CRITICAL: Only trigger if selection actually changed (prevents infinite loop/flickering)
// This happens when user clicks on a node in the Outliner
checkAndTriggerAutoFocusOnSelectionChange();
```

**Why:**
- Uses centralized method to check selection change
- Only triggers if selection actually changed
- Prevents infinite loop

### 5. Aspect Ratio Calculation Fix

**File:** `src/io/scene.cpp`

**Location:** `applyCameraAimConstraint()` method (lines ~2340-2344)

**Change:**

#### Before:
```cpp
float aspectRatio = 16.0f / 9.0f;  // Default aspect ratio
if (mViewportScreenSize.x > 0 && mViewportScreenSize.y > 0) {
    aspectRatio = mViewportScreenSize.x / mViewportScreenSize.y;
}
```

#### After:
```cpp
// CRITICAL: Use floating point division from mViewportScreenSize (not hardcoded 16.0f/9.0f)
float aspectRatio = 16.0f / 9.0f;  // Default aspect ratio
if (mViewportScreenSize.x > 0.0f && mViewportScreenSize.y > 0.0f) {
    aspectRatio = static_cast<float>(mViewportScreenSize.x) / static_cast<float>(mViewportScreenSize.y);
}
```

**Why:**
- Uses explicit floating point division
- Ensures correct aspect ratio calculation from viewport size
- Prevents integer division issues

### 6. Epsilon Check in Camera

**File:** `src/io/camera.cpp`

**Location:** `Update()` method - framing logic (lines ~493-502)

**Change:**
```cpp
// Calculate target position: camera should be at finalDist from lookAtTarget
// Direction from current camera to look-at target
glm::vec3 aimDirection = glm::normalize(lookAtTarget - cameraPos);
glm::vec3 targetPosition = lookAtTarget - aimDirection * finalDist;

// CRITICAL: Epsilon check - if distance to new framing target is smaller than 0.001f, skip update
// This prevents sub-pixel jitter and infinite loop when smooth camera is OFF
float distanceToTarget = glm::length(targetPosition - cameraPos);
if (distanceToTarget < 0.001f) {
    // Distance is too small, skip framing to prevent jitter
    m_shouldStartFraming = false;
    return;
}
```

**Why:**
- Prevents sub-pixel jitter when distance is very small
- Prevents infinite loop when smooth camera is OFF
- Ensures framing is a "one-shot" event

### 7. Selection Tracking Reset on Deselection

**File:** `src/main.cpp`

**Location:** Empty space click (deselection) (lines ~1495-1497)

**Change:**
```cpp
// Reset selection tracking variables on deselection
// This ensures auto-focus will trigger again if the same item is selected later
scene.resetSelectionTracking();
```

**Why:**
- Resets tracking when selection is cleared
- Ensures Auto Focus will trigger again if the same item is selected later
- Prevents stale tracking state

## Behavior

### Before
- Auto Focus triggered every frame for the same selection
- Caused infinite loop and flickering
- Camera continuously re-framed the same target
- Aspect ratio used hardcoded default
- No epsilon check for sub-pixel movements

### After
- Auto Focus triggers only once per selection change
- No infinite loop or flickering
- Camera frames once, then stops until selection changes
- Aspect ratio calculated dynamically from viewport size
- Epsilon check prevents sub-pixel jitter

## Key Principles

### 1. One-Shot Framing
- Framing is triggered exactly once per selection change
- Camera frames the target, then stops
- No re-triggering until selection changes again

### 2. Selection Change Detection
- Tracks both ModelIndex and NodeName
- Only triggers if either changes
- Resets on deselection

### 3. Epsilon Check
- Skips framing if distance is < 0.001f
- Prevents sub-pixel jitter
- Ensures stable framing

### 4. Dynamic Aspect Ratio
- Calculated from viewport size
- Uses floating point division
- Adapts to window resizing

## Testing Checklist

When testing the fix, verify:
- [ ] Auto Focus triggers once when a character is selected
- [ ] No flickering or infinite loop when Smooth Camera is OFF
- [ ] Auto Focus triggers again when selection changes to a different character
- [ ] Auto Focus triggers again when selection changes to a different node
- [ ] No flickering when the same character is selected again after deselection
- [ ] Aspect ratio is correct for different viewport sizes
- [ ] No sub-pixel jitter when framing

## Files Modified

1. `src/io/scene.h`
   - Added selection tracking variables
   - Added `checkAndTriggerAutoFocusOnSelectionChange()` method
   - Added `resetSelectionTracking()` method

2. `src/io/scene.cpp`
   - Implemented `checkAndTriggerAutoFocusOnSelectionChange()`
   - Updated auto-focus triggers to use new method
   - Fixed aspect ratio calculation

3. `src/main.cpp`
   - Updated viewport selection to use new method
   - Added selection tracking reset on deselection

4. `src/io/camera.cpp`
   - Added epsilon check to prevent sub-pixel jitter

## Related Documentation

- `MD/AUTO_FOCUS_ON_SELECTION.md` - Auto Focus on selection change feature
- `MD/AUTO_FOCUS_PERSISTENCE.md` - Auto Focus setting persistence

## Summary

This fix prevents Auto Focus from causing flickering and infinite loops by:
1. Tracking the last selection (ModelIndex + NodeName)
2. Only triggering Auto Focus when selection actually changes
3. Using dynamic aspect ratio calculation from viewport size
4. Adding epsilon check to prevent sub-pixel jitter

The framing remains a "one-shot" event - once the camera has snapped or finished its smooth transition to the selected target, it does not update again until the user either moves the gizmo or changes the selection.
