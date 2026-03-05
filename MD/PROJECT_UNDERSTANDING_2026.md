# OpenGL_loader Project Understanding - 2026

## Date
Created: 2026-02-23

## Purpose
This document confirms understanding of the OpenGL_loader project structure, code organization principles, and development guidelines for continued work.

## Project Overview

**OpenGL_loader** is a 3D model viewer and animation tool built with OpenGL, ImGui, and Assimp. It provides a comprehensive interface for loading, viewing, and animating multiple FBX files simultaneously.

### Core Features
- **Multi-Model Support**: Load and display multiple FBX files simultaneously
- **Animation System**: Independent animation playback for each loaded model
- **Bone Manipulation**: Transform bones using PropertyPanel sliders or ImGuizmo gizmo
- **Viewport Rendering**: 3D viewport with camera controls (orbit, zoom, pan)
- **UI Components**: Dockable windows (Viewport, Outliner, PropertyPanel, TimeSlider)
- **Settings Persistence**: Camera state, window position, and recent files saved to JSON

## Code Organization Structure

### Directory Layout
```
src/
├── graphics/          # Graphics-related code (rendering, models, shaders, etc.)
│   ├── math3D.*       # Mathematical utilities and conversions
│   ├── model.*        # 3D model loading and rendering
│   ├── model_manager.* # Multi-model management
│   ├── shader.*       # Shader management
│   ├── grid.*         # Grid rendering
│   ├── bounding_box.* # Bounding box rendering
│   └── raycast.*      # Raycast for viewport selection
├── io/                # Input/Output and UI code
│   ├── keyboard.*     # Low-level keyboard input utility
│   ├── mouse.*        # Low-level mouse input utility
│   ├── scene.*        # Main scene management and application logic
│   ├── camera.*       # Camera controls
│   ├── app_settings.* # Settings persistence
│   └── ui/            # ImGui UI panels
│       ├── outliner.*      # Hierarchy display
│       ├── property_panel.* # Transform controls
│       ├── timeSlider.*     # Animation timeline
│       ├── gizmo_manager.*  # Gizmo manipulation
│       └── viewport_panel.* # Viewport rendering
└── main.cpp           # Application entry point (now minimal, delegates to Application class)
```

### Code Organization Principles

#### 1. Mathematical Functions → `math3D.h/cpp`
- Matrix conversion functions (e.g., `glmMat4ToFloat16`, `float16ToGlmMat4`)
- Vector operations
- Matrix operations
- Quaternion operations
- Mathematical constants and utilities

**Why**: Reusable mathematical utilities needed across different parts of the application.

#### 2. Low-Level Input → `keyboard.h/cpp`, `mouse.h/cpp`
- Key state tracking
- Mouse button state tracking
- Input event callbacks
- Basic input query functions (`key()`, `keyWentDown()`, etc.)

**What does NOT belong here:**
- Application-specific keyboard shortcuts (e.g., gizmo shortcuts)
- High-level input handling logic
- Application command bindings

**Why**: Generic input utilities that provide raw input state. Application-specific logic should be in the appropriate application classes (e.g., `Application`, `Scene`).

#### 3. Application Logic → `application.cpp`, `scene.cpp`
- Keyboard shortcut handlers for specific features (e.g., gizmo shortcuts)
- Feature-specific input handling
- Application workflow logic
- UI interaction logic

**Why**: Application-specific shortcuts and features are part of the application's behavior, not low-level input utilities.

## Debug Print Guidelines

### ✅ DO: Action-Based Printing
Debug prints should occur on specific user actions:
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

### Examples

**Good (Action-Based):**
```cpp
// Print when gizmo manipulation ends (mouse release)
if (mGizmoWasUsing && !mGizmoUsing) {
    std::cout << "[Gizmo] Manipulation ended" << std::endl;
    printGizmoTransformValues(...);
}

// Print when node selection changes (not every frame)
static std::string lastLoggedGizmoNode = "";
if (lastLoggedGizmoNode != selectedNode) {
    std::cout << "LOG: Gizmo active on: " << selectedNode << std::endl;
    lastLoggedGizmoNode = selectedNode;
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

1. **File Loading** (`application.cpp` lines 373-489)
   - Prints when files are loaded or fail to load
   - Action: File dialog selection

2. **Animation State Changes** (`application.cpp` lines 530-539)
   - Prints when animation starts/stops
   - Action: Play/Stop button click

3. **Gizmo Node Change** (`gizmo_manager.cpp` lines 124-128)
   - Prints when gizmo attaches to a different node
   - Action: Node selection change (not every frame)
   - Uses static variable to track last logged node

4. **Model Framing** (`application.cpp` lines 1003-1049)
   - Prints when framing objects
   - Action: 'A' key press or 'F' key press

5. **Viewport Selection** (`application.cpp` lines 1131-1173)
   - Handles selection via ViewportSelectionManager
   - Action: Mouse click in viewport

6. **Settings Operations** (`app_settings.cpp`, `application.cpp`)
   - Prints when settings are loaded/saved
   - Action: Settings file operations

7. **Outliner Operations** (`outliner.cpp`)
   - Prints when FBX files are loaded into outliner
   - Action: File load operations

8. **PropertyPanel Operations** (`property_panel.cpp`)
   - Prints when bones are reset or initialized
   - Action: User button clicks

### ✅ Removed (Fixed)

1. **Frame Update Print** (previously in `main.cpp` line 275)
   - Was printing "** UPDATE **" every frame
   - **Status:** Removed

2. **PropertyPanel Debug Prints** (previously in `property_panel.cpp` lines 183-224)
   - Were printing every frame
   - **Status:** Removed (commented out with explanation)

## Development Guidelines

### When Adding Code

1. **Is this a mathematical operation?** → `math3D.h/cpp`
2. **Is this a low-level input utility?** → `keyboard.h/cpp` or `mouse.h/cpp`
3. **Is this application-specific logic?** → `application.cpp` or `scene.cpp` or appropriate feature file
4. **Is this reusable across the project?** → Consider a utility file
5. **Does this need documentation?** → Create/update `.md` file in `MD/` folder

### When Adding Debug Prints

1. **Use state transitions** - Print when state changes, not during continuous state
2. **Use event callbacks** - Print in event handlers, not in update loops
3. **Use conditional compilation** - For debug-only prints, use `#ifdef DEBUG`
4. **Use debug flags** - Allow toggling debug output at runtime
5. **Use consistent prefixes** - `[LOAD]`, `[ANIM]`, `[Gizmo]`, `[Selection]`, `[ERROR]`, etc.
6. **Use static variables** - To track previous state and only print on changes

### Code Comments

- Use comments to explain **why** something is done, not just **what** is done
- Add comments for non-obvious logic or workarounds
- Document function parameters and return values
- Explain mathematical operations and conversions

### Documentation Files (MD/ folder)

- Create `.md` files for significant features or changes
- Document the purpose, implementation, and usage
- Include code examples when helpful
- Update documentation when making changes
- Use descriptive filenames with dates when appropriate

## Key Systems

### 1. Model Management
- **Model**: Single FBX model with animation support
- **ModelInstance**: Wraps Model with independent animation state
- **ModelManager**: Manages collection of ModelInstance objects

### 2. Transform System
- World-to-local coordinate space conversion
- Parent chain traversal
- Scale compensation for 1:1 gizmo movement
- Per-model root node transforms

### 3. Animation System
- Independent animation state per model
- Bone transformation pipeline
- Per-vertex bone blending (up to 4 bones per vertex)
- Animation time scrubbing and playback

### 4. UI System
- ImGui-based interface
- Dockable window layout
- Viewport framebuffer rendering
- Multi-scene outliner support

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

**Key Takeaway**: All debug prints must be action-based (triggered by user actions like mouse clicks or key presses), never frame-based or time-based. All code changes should maintain modularity, be well-documented, and follow the established organization principles.

## Notes for Future Work

- All changes/additions/updates should be done in an orderly and clean way with division into procedures and files (maintain modularity)
- Document and add explanations/comments where necessary
- Create/update orderly *.md files (inside the MD folder) for significant changes
- When printing to debug window, use action-based printing only (mouse clicks, key presses, state transitions)
- Never use infinite loop prints or prints at time intervals
