# Drag and Drop FBX File Support

## Date
Created: 2026-02-23

## Overview
Added drag and drop support for FBX files using GLFW. Users can now drag FBX files from the file system directly onto the application window to load them.

## Implementation

### Changes Made

#### 1. Application Header (`application.h`)
Added a new public method declaration:
```cpp
void handleDroppedFile(const std::string& path);
```

**Purpose**: Public interface for handling dropped files. Called by the GLFW drop callback.

#### 2. GLFW Drop Callback (`application.cpp`)
Added a static callback function that GLFW calls when files are dropped:
```cpp
static void glfw_drop_callback(GLFWwindow* window, int count, const char** paths)
```

**Functionality**:
- Retrieves the Application instance from the window user pointer
- Iterates through all dropped file paths
- Checks if the file extension is `.fbx` (case-insensitive)
- Calls `handleDroppedFile()` for the first valid FBX file found
- Only processes the first valid FBX file (breaks after finding one)

**Why Static**: GLFW callbacks must be C-compatible functions, so they cannot be member functions. The static function uses the window user pointer to access the Application instance.

#### 3. Handle Dropped File Method (`application.cpp`)
Implemented the `Application::handleDroppedFile()` method:
```cpp
void Application::handleDroppedFile(const std::string& path) {
    m_scene.setFilePath(path);
}
```

**Purpose**: Delegates to the existing file loading mechanism. The Scene class already handles file path setting and loading through `setFilePath()`.

#### 4. Callback Registration (`application.cpp` in `Application::init()`)
Registered the drop callback right after making the context current:
```cpp
glfwSetWindowUserPointer(m_window, this);
glfwSetDropCallback(m_window, glfw_drop_callback);
```

**Why Here**: 
- Window must be created and context must be current before registering callbacks
- User pointer must be set before the callback can be used
- This location ensures the window is fully initialized

### File Extensions
- Added `#include <cctype>` for `std::tolower()` to perform case-insensitive file extension comparison

## Usage

Users can now:
1. Drag an FBX file from Windows Explorer (or any file manager)
2. Drop it onto the OpenGL_loader application window
3. The file will be automatically loaded using the existing file loading mechanism

## Technical Details

### File Extension Validation
- Checks if the file path is at least 4 characters long (minimum for ".fbx")
- Extracts the last 4 characters as the file extension
- Converts extension to lowercase for case-insensitive comparison
- Only processes files with `.fbx` extension

### Multiple Files
- If multiple files are dropped, only the first valid FBX file is processed
- Non-FBX files are ignored
- The callback breaks after finding the first valid FBX file

### Integration with Existing System
- Uses the existing `Scene::setFilePath()` method
- No changes needed to the file loading logic
- Follows the same loading path as the file dialog selection
- All existing file loading features (outliner, animation, etc.) work the same way

## Code Organization

### Modularity
- **Callback Function**: Static function in `application.cpp` (GLFW requirement)
- **Handler Method**: Public method in `Application` class (clean interface)
- **Registration**: In `Application::init()` (initialization phase)

### Documentation
- Inline comments explain the purpose of each component
- Comments explain why static callback is needed
- Comments explain the file extension validation logic

## Testing

To test:
1. Build and run the application
2. Drag an FBX file from Windows Explorer
3. Drop it onto the application window
4. Verify the file loads correctly (check outliner, viewport, etc.)

## Future Enhancements

Potential improvements:
- Support for multiple file types (OBJ, etc.)
- Support for dropping multiple FBX files at once
- Visual feedback when dragging files over the window
- Error messages for invalid file types

## Related Files

- `src/application.h` - Method declaration
- `src/application.cpp` - Implementation and callback registration
- `src/io/scene.h` - File loading mechanism (`setFilePath()`)
