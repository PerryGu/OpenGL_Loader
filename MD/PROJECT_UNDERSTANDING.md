# OpenGL_loader Project Understanding

## Date
Created: 2024

## Project Overview

**OpenGL_loader** is a 3D model viewer and animation tool built with OpenGL, ImGui, and Assimp. It provides a comprehensive interface for loading, viewing, and animating multiple FBX files simultaneously.

## Core Functionality

### Multi-Model Support
- Load and display multiple FBX files simultaneously
- Each model maintains independent animation state
- Models can be selected, transformed, and animated independently

### Animation System
- Independent animation playback for each loaded model
- Bone transformation pipeline with per-vertex bone blending (up to 4 bones per vertex)
- Animation time scrubbing and playback controls
- Per-model animation FPS and duration tracking

### Transform System
- **Bone Manipulation**: Transform bones using PropertyPanel sliders or ImGuizmo gizmo
- **World-to-Local Conversion**: Automatic coordinate space conversion for rig roots
- **Scale Compensation**: 1:1 gizmo movement regardless of object scale
- **Per-Model RootNode Transforms**: Each model has its own RootNode transform (e.g., "RootNode", "RootNode01", "RootNode02")
- **Transform Persistence**: Bone transforms saved in PropertyPanel bone maps

### Viewport & Rendering
- 3D viewport with camera controls (orbit, zoom, pan)
- Maya-style camera controls (Alt + mouse buttons)
- Bounding boxes for all models (yellow for unselected, cyan for selected)
- Grid rendering with customizable settings
- Viewport selection (click models to select)

### UI Components
- **Dockable Windows**: All panels are dockable (Viewport, Outliner, PropertyPanel, TimeSlider)
- **Outliner**: Hierarchical view of all loaded models' bone structures
- **PropertyPanel**: Transform controls (translation, rotation, scale) for selected nodes
- **TimeSlider**: Animation timeline with play/pause controls
- **Gizmo Controls**: Maya-style shortcuts (W/E/R for translate/rotate/scale)

### Settings & Persistence
- Camera state saved to JSON
- Recent files tracking
- Window position and size persistence
- Grid and environment settings persistence
- Bounding box enabled/disabled state

## Code Organization

### Directory Structure

```
src/
├── graphics/          # Rendering and model management
│   ├── model.*        # Single model loading and rendering
│   ├── model_manager.* # Multi-model management
│   ├── math3D.*       # Mathematical utilities (matrix conversions)
│   ├── shader.*       # Shader management
│   ├── grid.*         # Grid rendering
│   ├── bounding_box.* # Bounding box rendering
│   └── raycast.*      # Raycast for viewport selection
├── io/                # Input/Output and UI
│   ├── scene.*        # Main scene management
│   ├── camera.*       # Camera controls
│   ├── keyboard.*     # Low-level keyboard input
│   ├── mouse.*        # Low-level mouse input
│   ├── app_settings.* # Settings persistence
│   └── ui/            # ImGui UI panels
│       ├── outliner.*      # Hierarchy display
│       ├── property_panel.* # Transform controls
│       └── timeSlider.*     # Animation timeline
└── main.cpp           # Application entry point
```

### Key Systems

#### 1. Model Management
- **Model**: Single FBX model with animation support
- **ModelInstance**: Wraps Model with independent animation state
- **ModelManager**: Manages collection of ModelInstance objects

#### 2. Transform System
- World-to-local coordinate space conversion
- Parent chain traversal
- Scale compensation for 1:1 gizmo movement
- Per-model root node transforms

#### 3. Animation System
- Independent animation state per model
- Bone transformation pipeline
- Per-vertex bone blending (up to 4 bones per vertex)
- Animation time scrubbing and playback

#### 4. UI System
- ImGui-based interface
- Dockable window layout
- Viewport framebuffer rendering
- Multi-scene outliner support

## Code Organization Principles

### Modularity
- **Mathematical Functions** → `math3D.h/cpp`
- **Low-Level Input** → `keyboard.h/cpp`, `mouse.h/cpp`
- **Application Logic** → `scene.cpp`, feature-specific files
- **Reusable Utilities** → Appropriate utility files

### Documentation
- Inline comments explain **why**, not just **what**
- Markdown files in `MD/` folder document features and changes
- Code examples included in documentation

### Debug Output Guidelines

**CRITICAL RULE**: Debug prints must be **action-based**, not frame-based or time-based.

#### ✅ DO: Action-Based Printing
- Print on **mouse clicks** (button press/release)
- Print on **key presses** (specific key events)
- Print on **state transitions** (e.g., gizmo manipulation ends, animation play/pause)
- Print on **user interactions** (file load, selection change, button clicks)
- Print on **error conditions** (one-time errors, not repeated warnings)

#### ❌ DON'T: Frame-Based or Time-Based Printing
- **No infinite loops**: Don't print in the main render loop every frame
- **No timed intervals**: Don't print based on elapsed time or frame count
- **No continuous printing**: Don't print during continuous manipulation (e.g., while dragging gizmo)

#### Examples

**Good (Action-Based):**
```cpp
// Print when gizmo manipulation ends (mouse release)
if (mGizmoWasUsing && !mGizmoUsing) {
    std::cout << "[Gizmo] Manipulation ended" << std::endl;
    printGizmoTransformValues(...);
}
```

**Bad (Frame-Based):**
```cpp
// ❌ BAD - Prints every frame
while (!shouldClose()) {
    std::cout << "** UPDATE **" << std::endl;  // DON'T DO THIS
    // ... render code ...
}
```

**Bad (Continuous During Manipulation):**
```cpp
// ❌ BAD - Prints every frame while dragging
if (manipulated) {
    std::cout << "DEBUG: Gizmo being manipulated" << std::endl;  // DON'T DO THIS
}
```

## Current Debug Print Status

### ✅ Action-Based (Good)
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

### ✅ Removed (Fixed)
1. **Frame Update Print** (`main.cpp` line 275)
   - Was printing "** UPDATE **" every frame
   - **Status:** Removed

2. **PropertyPanel Debug Prints** (`property_panel.cpp` lines 183-224)
   - Were printing every frame
   - **Status:** Removed (commented out with explanation)

## Development Guidelines

### When Adding Code

1. **Is this a mathematical operation?** → `math3D.h/cpp`
2. **Is this a low-level input utility?** → `keyboard.h/cpp` or `mouse.h/cpp`
3. **Is this application-specific logic?** → `scene.cpp` or appropriate feature file
4. **Is this reusable across the project?** → Consider a utility file
5. **Does this need documentation?** → Create/update `.md` file in `MD/` folder

### When Adding Debug Prints

1. **Use state transitions** - Print when state changes, not during continuous state
2. **Use event callbacks** - Print in event handlers, not in update loops
3. **Use conditional compilation** - For debug-only prints, use `#ifdef DEBUG`
4. **Use debug flags** - Allow toggling debug output at runtime
5. **Use consistent prefixes** - `[LOAD]`, `[ANIM]`, `[Gizmo]`, `[Selection]`, `[ERROR]`, etc.

### Code Comments

- Use comments to explain **why** something is done, not just **what** is done
- Add comments for non-obvious logic or workarounds
- Document function parameters and return values
- Explain mathematical operations and conversions

## Dependencies

- **OpenGL 3.3+**: Graphics rendering
- **GLFW**: Window management
- **ImGui**: UI framework
- **ImGuizmo**: 3D gizmo manipulation
- **GLM**: Mathematics library
- **Assimp**: Model loading (FBX, OBJ)
- **openFBX**: FBX file parsing
- **GLAD**: OpenGL loader

## Build System

- **CMake**: Build configuration
- **Visual Studio**: Primary development environment
- **Windows**: Primary target platform

## Summary

The OpenGL_loader project is a well-organized, modular 3D animation tool with comprehensive multi-model support. The codebase follows clear organization principles, with mathematical utilities, low-level input, and application logic properly separated. Documentation is maintained in the `MD/` folder, and the system supports independent animation playback for multiple models simultaneously.

**Key Takeaway**: All debug prints must be action-based (triggered by user actions like mouse clicks or key presses), never frame-based or time-based.
