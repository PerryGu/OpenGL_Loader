# ImGui Keyboard Navigation Fix

## Date
Created: 2026-02-23

## Overview
Fixed ImGui keyboard navigation (Tab key to switch between input fields) by reordering GLFW callback registration. Custom GLFW input callbacks must be registered before ImGui initialization to allow ImGui to chain them instead of overwriting them.

## Problem
The Tab key was not working to switch between input fields in ImGui windows. This was because:
1. `ImGui_ImplGlfw_InitForOpenGL()` was called before custom GLFW callbacks were registered
2. ImGui overwrote the custom callbacks instead of chaining them
3. This broke core UI navigation functionality like Tab field switching

## Solution
Reordered the callback registration in `Scene::setParameters()` so that:
1. Custom GLFW callbacks are registered **BEFORE** `ImGui_ImplGlfw_InitForOpenGL()`
2. ImGui can chain the existing callbacks instead of overwriting them
3. Both custom input handling and ImGui navigation work correctly

## Implementation

### File Modified
**Location**: `src/io/scene.cpp` (`Scene::setParameters()`)

### Changes Made

**Before** (incorrect order):
```cpp
// ImGui initialization first
ImGui_ImplGlfw_InitForOpenGL(window, true);
ImGui_ImplOpenGL3_Init(glsl_version);

// Custom callbacks registered AFTER ImGui (overwrites ImGui's callbacks)
glfwSetKeyCallback(window, Keyboard::keyCallback);
glfwSetCursorPosCallback(window, Mouse::cursorPosCallback);
glfwSetMouseButtonCallback(window, Mouse::mouseButtonCallback);
glfwSetScrollCallback(window, Mouse::mouseWheelCallback);
```

**After** (correct order):
```cpp
// CRITICAL: Custom GLFW callbacks must be registered BEFORE ImGui initialization.
// This allows ImGui to chain them and retains UI keyboard navigation (like Tab key).
glfwSetKeyCallback(window, Keyboard::keyCallback);
glfwSetCursorPosCallback(window, Mouse::cursorPosCallback);
glfwSetMouseButtonCallback(window, Mouse::mouseButtonCallback);
glfwSetScrollCallback(window, Mouse::mouseWheelCallback);

// ImGui initialization now safely chains the custom callbacks
ImGui_ImplGlfw_InitForOpenGL(window, true);
ImGui_ImplOpenGL3_Init(glsl_version);
```

## How It Works

### Callback Chaining
When `ImGui_ImplGlfw_InitForOpenGL()` is called with `install_callbacks = true`:
1. ImGui checks if callbacks already exist
2. If callbacks exist, ImGui stores them and chains them in its own callbacks
3. If callbacks don't exist, ImGui creates new ones (overwriting any previous state)

### Registration Order
- **Correct Order**: Custom callbacks → ImGui initialization
  - Custom callbacks are registered first
  - ImGui detects existing callbacks and chains them
  - Both systems work correctly

- **Incorrect Order**: ImGui initialization → Custom callbacks
  - ImGui creates its own callbacks
  - Custom callbacks overwrite ImGui's callbacks
  - ImGui navigation breaks (Tab key doesn't work)

## Benefits

1. **Tab Key Navigation**: Tab key now works to switch between input fields
2. **Custom Input Handling**: Custom keyboard/mouse callbacks still work correctly
3. **No Conflicts**: Both systems coexist without overwriting each other
4. **Standard ImGui Behavior**: Follows ImGui's recommended callback chaining pattern

## Testing

### Test Case 1: Tab Key Navigation
1. Open any ImGui window with multiple input fields (e.g., Property Panel)
2. Click on the first input field
3. Press Tab key
4. **Expected**: Focus moves to the next input field
5. Press Tab again
6. **Expected**: Focus continues to cycle through fields

### Test Case 2: Custom Keyboard Shortcuts
1. Press W/E/R keys (Maya-style gizmo shortcuts)
2. **Expected**: Gizmo operation mode changes correctly
3. Press +/- keys (gizmo size adjustment)
4. **Expected**: Gizmo size changes correctly

### Test Case 3: Mouse Input
1. Right-click and drag in viewport
2. **Expected**: Camera rotates correctly
3. Scroll mouse wheel
4. **Expected**: Camera zooms correctly

## Technical Notes

- **Callback Chaining**: ImGui's `ImGui_ImplGlfw_InitForOpenGL(window, true)` parameter enables automatic callback chaining
- **Install Callbacks**: The `true` parameter tells ImGui to install its own callbacks, but it will chain existing ones
- **Order Dependency**: The registration order is critical - custom callbacks must exist before ImGui initialization
- **No Performance Impact**: Callback chaining has negligible performance overhead

## Related Files
- `src/io/scene.cpp` - `Scene::setParameters()` (callback registration)
- `src/io/keyboard.h/cpp` - Custom keyboard callback implementation
- `src/io/mouse.h/cpp` - Custom mouse callback implementation

## Future Considerations

If additional GLFW callbacks are added in the future:
1. Register them **BEFORE** `ImGui_ImplGlfw_InitForOpenGL()`
2. Ensure they follow the same chaining pattern
3. Test that both custom functionality and ImGui navigation work

## References

- [ImGui GLFW Backend Documentation](https://github.com/ocornut/imgui/blob/master/backends/imgui_impl_glfw.cpp)
- ImGui callback chaining mechanism (internal to ImGui backend)
