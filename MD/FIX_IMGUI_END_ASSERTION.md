# Fix: ImGui "Calling End() too many times!" Assertion Error (Viewport Window)

## Problem
The application crashes with the following assertion error:
```
Assertion failed: (g.CurrentWindowStack.Size > 1) && "Calling End() too many times!", 
file F:\Work_stuff\VisualStudio_cursor\OpenGL_loader\Dependencies\imgui\imgui.cpp, line 6917
```

This error occurs in the `renderViewportWindow()` function when the viewport window is displayed.

## Root Cause
The `ImGui::Begin()` function in `renderViewportWindow()` was not checking its return value. When `ImGui::Begin()` returns `false` (which happens when a window is collapsed or closed), we should return early. However, the code was still calling `ImGui::End()` at the end of the function, which caused an imbalance in the window stack.

## Solution Applied

### Changes Made to `src/io/scene.cpp`:

**Before (WRONG):**
```cpp
void Scene::renderViewportWindow()
{
    if (!show_viewport) {
        mouseOverViewport = false;
        return;
    }
    
    // Create viewport window with menu bar flag
    ImGui::Begin("Viewport", &show_viewport, ImGuiWindowFlags_MenuBar);
    // ... rest of code ...
    ImGui::End();
}
```

**After (CORRECT):**
```cpp
void Scene::renderViewportWindow()
{
    if (!show_viewport) {
        mouseOverViewport = false;
        return;
    }
    
    // Create viewport window with menu bar flag
    // IMPORTANT: Check if Begin() returns false (window collapsed/closed) and return early
    if (!ImGui::Begin("Viewport", &show_viewport, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        mouseOverViewport = false;
        return;
    }
    // ... rest of code ...
    ImGui::End();
}
```

## Why This Fixes the Error

The "Calling End() too many times!" error occurs when:
- `ImGui::Begin()` returns `false` (window collapsed/closed)
- Code continues executing and calls `ImGui::End()` anyway
- This creates an imbalance: `Begin()` didn't push a window, but `End()` tries to pop one

By checking the return value of `ImGui::Begin()` and returning early (with a matching `End()` call), we ensure:
- ✅ Proper Begin/End pairing for all window states
- ✅ No window stack imbalance
- ✅ Safe handling of collapsed/closed windows

## Testing

After rebuilding:
1. The assertion error should be gone
2. The viewport window should work correctly when collapsed/expanded
3. The application should run without crashes

## Files Modified

- `src/io/scene.cpp` - Added proper `ImGui::Begin()` return value check in `renderViewportWindow()`

## Date
Fixed: [Current Session]
