# Debug Print Guidelines

## Overview
This document describes the guidelines for debug output in the OpenGL_loader project. Debug prints should be action-based, not frame-based or time-based.

## User Preference (Strict)

When adding "print to the debug window" or any debug/log output:

- **Do** print on **specific actions only**, e.g.:
  - Every time the user **clicks the mouse** (viewport, outliner, button, etc.)
  - Key press (shortcut, arrow key)
  - State transition (selection change, gizmo release, play/stop)
  - One-time events (load, save, error)

- **Do not**:
  - Print in an **infinite loop** or every frame (e.g. in the main/render loop).
  - Print at **time or frame intervals** (e.g. every N seconds or every N frames).

**In short:** Print every time the user performs a discrete action (e.g. mouse click or other explicit action). Never use continuous or interval-based prints.

## Date
Created: 2024 | Updated: 2026-03-09

## Core Principle

**Debug prints should occur on specific user actions, not continuously or at intervals.**

## Rules

### ✅ DO: Action-Based Printing

Print on specific user actions:
- **Mouse clicks** (button press/release)
- **Key presses** (specific key events)
- **State transitions** (e.g., gizmo manipulation ends, animation play/pause)
- **User interactions** (file load, selection change, button clicks)
- **Error conditions** (one-time errors, not repeated warnings)

### ❌ DON'T: Frame-Based or Time-Based Printing

Avoid printing:
- **Every frame** in the main render loop
- **During continuous manipulation** (e.g., while dragging gizmo)
- **At timed intervals** (e.g., every N seconds or frames)
- **In update loops** without action triggers

## Examples

### Good Examples

#### 1. Print on Mouse Release (Gizmo Manipulation End)
```cpp
// Detect when gizmo state transitions from using to not using
if (mGizmoWasUsing && !mGizmoUsing) {
    // Gizmo manipulation just ended - print the final transform values
    printGizmoTransformValues(modelMatrixArray, selectedNode, isRigRoot);
}
```
**Why Good:** Only prints once when manipulation ends (mouse release), not during dragging.

#### 2. Print on File Load
```cpp
if (modelManager.loadModel(filePath) >= 0) {
    std::cout << "[LOAD] Model loaded successfully: " << filePath << std::endl;
}
```
**Why Good:** Prints once when file is loaded, not continuously.

#### 3. Print on Animation State Change
```cpp
if (play_stopVal && !wasPlaying) {
    wasPlaying = true;
    std::cout << "[ANIM] Started playing all animations" << std::endl;
}
```
**Why Good:** Prints only when state transitions from stopped to playing.

### Bad Examples

#### 1. Print Every Frame (REMOVED)
```cpp
// ❌ BAD - Prints every frame
while (!shouldClose()) {
    std::cout << "** UPDATE **" << std::endl;  // DON'T DO THIS
    // ... render code ...
}
```
**Why Bad:** Prints 60+ times per second, cluttering console and impacting performance.

**Status:** ✅ Fixed - Removed from `main.cpp` line 275

#### 2. Print During Continuous Manipulation
```cpp
// ❌ BAD - Prints every frame while dragging gizmo
if (manipulated) {
    std::cout << "DEBUG [Dynamic Calculation] Node: " << selectedNodeName 
              << " | Expected Local: (" << expectedLocal.x << ", ...)" << std::endl;
}
```
**Why Bad:** Prints continuously while user is dragging, creating console spam.

**Status:** ⚠️ Needs Review - Located in `scene.cpp` lines 1174-1186

**Recommendation:** Move this debug output to print only when manipulation ends (similar to `printGizmoTransformValues`), or remove if not needed.

#### 3. Print at Timed Intervals
```cpp
// ❌ BAD - Prints every N seconds
static float lastPrintTime = 0.0f;
if (currentTime - lastPrintTime > 2.0f) {
    std::cout << "Debug info..." << std::endl;
    lastPrintTime = currentTime;
}
```
**Why Bad:** Prints at regular intervals, not based on user actions.

## Current Debug Print Locations

### Action-Based (Good) ✅

1. **File Loading** (`main.cpp` lines 373-389)
   - Prints when files are loaded or fail to load
   - Action: File dialog selection

2. **Animation State Changes** (`main.cpp` lines 467, 472)
   - Prints when animation starts/stops
   - Action: Play/Stop button click

3. **Gizmo Manipulation End** (`scene.cpp` line 1114)
   - Calls `printGizmoTransformValues()` when manipulation ends
   - Action: Mouse release after gizmo manipulation

4. **Model Framing** (`main.cpp` lines 819, 836, 846, 853)
   - Prints when framing objects
   - Action: 'A' key press or 'F' key press

5. **Viewport Selection** (`main.cpp` line 1154)
   - Prints when model is selected in viewport
   - Action: Mouse click in viewport

6. **Outliner Selection Change** (`outliner.cpp`)
   - Logs to Logger (Debug panel) when the selected element changes (outliner click, viewport click, or arrow key)
   - Action: Selection change (not every frame)
   - Uses static variable to track last logged element; logs only on transition

### Needs Review ⚠️

1. **Dynamic Debug Calculation** (`scene.cpp` lines 1174-1186)
   - Prints during gizmo manipulation (every frame while dragging)
   - **Issue:** Prints continuously during manipulation
   - **Recommendation:** Move to print only when manipulation ends, or remove

### Removed ✅

1. **Frame Update Print** (`main.cpp` line 275)
   - Was printing "** UPDATE **" every frame
   - **Status:** Removed

## Best Practices

### 1. Use State Transitions
Print when state changes, not during continuous state:
```cpp
// Track previous state
bool wasPlaying = false;

// Check for state transition
if (play_stopVal && !wasPlaying) {
    std::cout << "[ANIM] Started playing" << std::endl;
    wasPlaying = true;
}
```

### 2. Use Event Callbacks
Print in event handlers, not in update loops:
```cpp
void onMouseButtonPress(int button) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        std::cout << "[Mouse] Left button pressed" << std::endl;
    }
}
```

### 3. Use Conditional Compilation
For debug-only prints, use preprocessor directives:
```cpp
#ifdef DEBUG
    std::cout << "[DEBUG] Detailed debug info" << std::endl;
#endif
```

### 4. Use Debug Flags
Allow toggling debug output at runtime:
```cpp
static bool g_debugGizmo = false;  // Can be toggled via UI

if (g_debugGizmo && manipulated) {
    std::cout << "[Gizmo] Debug info" << std::endl;
}
```

## Debug Print Format

Use consistent prefixes for easy filtering:
- `[LOAD]` - File loading operations
- `[ANIM]` - Animation-related operations
- `[Gizmo]` - Gizmo manipulation
- `[Selection]` - Selection changes
- `[Settings]` - Settings operations
- `[ERROR]` - Error conditions
- `[WARNING]` - Warning conditions
- `[DEBUG]` - General debug information

## Console Output Management

### Filtering
Use consistent prefixes to filter output:
```bash
# Filter only animation messages
grep "\[ANIM\]" output.log

# Filter only errors
grep "\[ERROR\]" output.log
```

### Redirecting
For production builds, consider redirecting debug output:
```cpp
#ifdef RELEASE
    // Redirect std::cout to null in release builds
    std::cout.setstate(std::ios_base::failbit);
#endif
```

## Summary

- ✅ **Action-based**: Print on user actions (clicks, key presses, state changes)
- ❌ **Not frame-based**: Don't print every frame
- ❌ **Not time-based**: Don't print at intervals
- ❌ **Not continuous**: Don't print during continuous operations

Following these guidelines ensures:
- Clean console output
- Better performance (no unnecessary I/O)
- Easier debugging (meaningful output)
- Professional user experience
