# Application Class Refactoring

**Date:** 2026-02-19  
**Status:** ✅ Implemented

## Overview
Extracted the application lifecycle (window creation, OpenGL initialization, and main game loop) from `main.cpp` and `scene.cpp` into a dedicated `Application` class, making `main.cpp` a clean entry point.

## Architecture

### Before: Scattered Logic
- Window creation in `Scene::init()`
- OpenGL initialization in `Scene::init()`
- Main loop in `main.cpp`
- Mixed responsibilities

### After: Clean Separation
```
main.cpp (Entry Point)
    └── Application
        ├── Window Management
        ├── OpenGL Context
        ├── Scene
        ├── ModelManager
        └── Main Loop
```

## Application Class

### Structure
**File:** `src/application.h/cpp`

```cpp
class Application {
public:
    Application();
    ~Application();
    bool init();
    void run();
    void cleanup();

private:
    GLFWwindow* m_window;
    Scene m_scene;
    ModelManager m_modelManager;
    
    // Main loop helpers
    void updateTime();
    void updateCamera();
    void handleSceneEvents();
    void updateAnimations(double current_time, float interval);
    void syncTransforms();
    void renderFrame();
};
```

### Responsibilities

#### 1. Window Management
- GLFW initialization
- Window creation
- Context management
- Cleanup

#### 2. OpenGL Initialization
- GLAD/GLEW loader initialization
- OpenGL state setup
- Error handling

#### 3. Main Loop Organization
The `run()` method is clean and readable:
```cpp
void Application::run() {
    while (!m_scene.shouldClose()) {
        updateTime();
        processInput(deltaTime);
        
        m_scene.update();
        updateCamera();
        handleSceneEvents();
        
        if (m_file_is_open) {
            updateAnimations(fCurrentTime, fInterval);
            syncTransforms();
        }
        
        renderFrame();
        
        m_scene.endViewportRender();
        glfwSwapBuffers(m_window);
        glfwPollEvents();
    }
}
```

## Helper Methods

### 1. updateTime()
Calculates delta time and frame timing.

### 2. updateCamera()
- Camera framing distance updates
- Smooth camera interpolation
- Camera aim constraints
- Auto-focus triggers

### 3. handleSceneEvents()
- Clear scene requests
- New file requests
- Other scene-level events

### 4. updateAnimations()
- Animation playback control
- Per-model animation updates
- Time synchronization

### 5. syncTransforms()
- Rig root selection handling
- PropertyPanel synchronization
- RootNode transform matrix building
- "Reset All Bones" handling

### 6. renderFrame()
- View/Projection matrix calculations
- Grid rendering
- Model rendering with transforms
- Bounding box rendering

## Scene Class Changes

### Removed from Scene
- `glfwCreateWindow()` - Moved to Application
- OpenGL loader initialization - Moved to Application
- `void newFrame()` - Replaced with direct `glfwSwapBuffers()`

### Updated Scene::init()
```cpp
// OLD: Created window, initialized OpenGL
// NEW: Just accepts existing window
bool Scene::init(GLFWwindow* existingWindow) {
    window = existingWindow;
    return true;
}
```

## Main.cpp Simplification

### Before: ~500+ lines
- Window creation
- OpenGL initialization
- Main loop
- Event handling
- Rendering logic

### After: ~10 lines
```cpp
#include "application.h"

int main() {
    Application app;
    if (app.init()) {
        app.run();
    }
    app.cleanup();
    return 0;
}
```

## Benefits

### 1. Clean Entry Point
- `main.cpp` is now trivial
- Easy to understand application flow
- Clear separation of concerns

### 2. Testability
- Application class can be unit tested
- Mock dependencies easily
- Isolated lifecycle management

### 3. Maintainability
- All application logic in one place
- Clear method organization
- Easy to add new features

### 4. Reusability
- Application class can be reused
- Easy to create multiple windows
- Platform-specific code isolated

## Files Created
- `src/application.h`
- `src/application.cpp`

## Files Modified
- `src/main.cpp` - Simplified to entry point
- `src/io/scene.h/cpp` - Removed window/OpenGL code
- `src/viewport_selection.h/cpp` - Extracted selection logic

## Related Documentation
- `MD/UI_ARCHITECTURE_REFACTORING.md`
- `MD/CODE_ORGANIZATION.md`
