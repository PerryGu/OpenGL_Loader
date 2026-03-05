# Code Organization Guidelines

## Overview
This document describes the organization principles and structure of the OpenGL_loader project. Following these guidelines helps maintain clean, maintainable, and well-documented code.

## Date
Created: [Current Session]

## Project Structure

### Directory Organization

```
src/
├── graphics/          # Graphics-related code (rendering, models, shaders, etc.)
│   ├── math3D.*       # Mathematical utilities and conversions
│   ├── model.*        # 3D model loading and rendering
│   ├── shader.*       # Shader management
│   └── ...
├── io/                # Input/Output and UI code
│   ├── keyboard.*     # Low-level keyboard input utility
│   ├── mouse.*        # Low-level mouse input utility
│   ├── scene.*        # Main scene management and application logic
│   └── ui/            # ImGui UI panels
└── main.cpp           # Application entry point
```

## Code Organization Principles

### 1. Mathematical Functions → `math3D.h/cpp`

**Purpose**: Central location for all mathematical utilities and conversions.

**What belongs here:**
- Matrix conversion functions (e.g., `glmMat4ToFloat16`, `float16ToGlmMat4`)
- Vector operations
- Matrix operations
- Quaternion operations
- Mathematical constants and utilities

**Example:**
```cpp
// src/graphics/math3D.h
/**
 * Convert GLM 4x4 matrix to float array (column-major format)
 * Used for interfacing with libraries like ImGuizmo
 */
inline void glmMat4ToFloat16(const glm::mat4& mat, float* out);
```

**Why**: These are reusable mathematical utilities that may be needed across different parts of the application. Centralizing them makes them easier to find and maintain.

### 2. Low-Level Input → `keyboard.h/cpp`, `mouse.h/cpp`

**Purpose**: Low-level input state management utilities.

**What belongs here:**
- Key state tracking
- Mouse button state tracking
- Input event callbacks
- Basic input query functions (`key()`, `keyWentDown()`, etc.)

**What does NOT belong here:**
- Application-specific keyboard shortcuts (e.g., gizmo shortcuts)
- High-level input handling logic
- Application command bindings

**Why**: These are generic input utilities that provide raw input state. Application-specific logic should be in the appropriate application classes (e.g., `Scene`).

### 3. Application Logic → `scene.cpp`, `main.cpp`

**Purpose**: High-level application behavior and feature implementation.

**What belongs here:**
- Keyboard shortcut handlers for specific features (e.g., gizmo shortcuts)
- Feature-specific input handling
- Application workflow logic
- UI interaction logic

**Example:**
```cpp
// src/io/scene.cpp
// Gizmo keyboard shortcut handler - application-specific logic
void Scene::handleGizmoKeyboardShortcuts()
{
    // Maya-style shortcuts: W/E/R for operation, +/- for size
    // This is application logic, not a general keyboard utility
}
```

**Why**: Application-specific shortcuts and features are part of the application's behavior, not low-level input utilities. Keeping them in the Scene class makes the code more maintainable and easier to understand.

## File Responsibilities

### `math3D.h/cpp`
- **Purpose**: Mathematical utilities and conversions
- **Contains**: Matrix conversions, vector operations, mathematical constants
- **Used by**: Any code that needs mathematical operations
- **Example functions**: `glmMat4ToFloat16()`, `float16ToGlmMat4()`, `Matrix4f::toGlmMatrix()`

### `keyboard.h/cpp`
- **Purpose**: Low-level keyboard input state management
- **Contains**: Key state tracking, input callbacks, basic key query functions
- **Used by**: Any code that needs to check keyboard state
- **Example functions**: `Keyboard::key()`, `Keyboard::keyWentDown()`, `Keyboard::keyCallback()`

### `scene.h/cpp`
- **Purpose**: Main scene management and application logic
- **Contains**: Scene setup, UI rendering, feature implementations, application-specific shortcuts
- **Used by**: `main.cpp` and UI components
- **Example functions**: `handleGizmoKeyboardShortcuts()`, `renderViewportWindow()`, `update()`

## Code Comments and Documentation

### Inline Comments
- Use comments to explain **why** something is done, not just **what** is done
- Add comments for non-obvious logic or workarounds
- Document function parameters and return values
- Explain mathematical operations and conversions

### Example Good Comments:
```cpp
// Convert GLM matrix to float array for ImGuizmo
// GLM matrices are column-major, ImGuizmo expects column-major
// So we can directly copy the memory
glmMat4ToFloat16(mViewMatrix, viewMatrix);
```

### Example Bad Comments:
```cpp
// Copy matrix  // Too obvious, doesn't add value
```

### Documentation Files (MD/)
- Create `.md` files for significant features or changes
- Document the purpose, implementation, and usage
- Include code examples when helpful
- Update documentation when making changes

## Debug Output Guidelines

### Action-Based Printing
Debug prints should be **action-based**, not frame-based or time-based:
- ✅ **Print on mouse clicks** (button press/release)
- ✅ **Print on key presses** (specific key events)
- ✅ **Print on state transitions** (e.g., gizmo manipulation ends)
- ✅ **Print on user interactions** (file load, selection change)

### What to Avoid
- ❌ **No infinite loops**: Don't print in the main render loop every frame
- ❌ **No timed intervals**: Don't print based on elapsed time or frame count
- ❌ **No continuous printing**: Don't print during continuous manipulation (e.g., while dragging gizmo)

### Examples

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
// Prints every frame - DON'T DO THIS
while (!shouldClose()) {
    std::cout << "** UPDATE **" << std::endl;  // ❌ BAD
    // ... render code ...
}
```

**Bad (Continuous During Manipulation):**
```cpp
// Prints every frame while dragging - DON'T DO THIS
if (manipulated) {
    std::cout << "DEBUG: Gizmo being manipulated" << std::endl;  // ❌ BAD
}
```

### When Debug Prints Are Needed
- Use debug prints for **one-time events** or **state transitions**
- Use debug prints for **user-initiated actions** (clicks, key presses)
- Use debug prints for **error conditions** or **warnings**
- Use debug prints for **initialization** or **cleanup** messages

### Removing Debug Prints
If you need to debug during continuous operations:
1. Use conditional compilation (`#ifdef DEBUG`)
2. Use a debug flag that can be toggled
3. Print only on state changes, not during continuous operations
4. Consider using a debug overlay (ImGui window) instead of console output

## Best Practices

1. **Separation of Concerns**: Keep utilities separate from application logic
2. **Reusability**: Place reusable functions in appropriate utility files
3. **Documentation**: Comment complex logic and document features
4. **Consistency**: Follow existing code patterns and organization
5. **Maintainability**: Organize code so it's easy to find and modify

## Recent Refactoring Example

### Before (Poor Organization):
- Matrix conversion functions were in `scene.cpp` as Scene class methods
- These are mathematical utilities, not scene-specific logic

### After (Better Organization):
- Matrix conversion functions moved to `math3D.h` as inline utility functions
- Scene class uses these utilities but doesn't own them
- Better separation of concerns and reusability

## Questions to Ask When Adding Code

1. **Is this a mathematical operation?** → `math3D.h/cpp`
2. **Is this a low-level input utility?** → `keyboard.h/cpp` or `mouse.h/cpp`
3. **Is this application-specific logic?** → `scene.cpp` or appropriate feature file
4. **Is this reusable across the project?** → Consider a utility file
5. **Does this need documentation?** → Create/update `.md` file

## Summary

- **Mathematical functions** → `math3D.h/cpp`
- **Low-level input** → `keyboard.h/cpp`, `mouse.h/cpp`
- **Application logic** → `scene.cpp`, feature-specific files
- **Documentation** → `.md` files in `MD/` directory
- **Comments** → Explain why, not just what

Following these guidelines helps maintain a clean, organized, and maintainable codebase.
