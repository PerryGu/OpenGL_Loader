# Fix Properties Window Crash on Close

## Date
Created: 2024

## Problem
The application crashed with "abort() has been called" when the user clicked the 'X' close button on the "Properties_Prop" window.

## Root Cause
The crash was caused by an **ImGui Begin/End mismatch**:

1. **Window Management Split**: The `ImGui::Begin()` call was in `uiElements()` function, but `ImGui::End()` was called in `propertyPanel()` function.

2. **Early Return Issue**: When the window was closed (user clicks 'X'), `ImGui::Begin()` returned `false`. The code would return early from `uiElements()`, but `propertyPanel()` was still called separately in `Scene::update()`, attempting to call `ImGui::End()` even though `ImGui::Begin()` was never successfully called.

3. **Missing End Call**: When the window was open, `uiElements()` would call `Begin()` but never call `End()`, relying on `propertyPanel()` to call it. This created a fragile dependency.

## Solution

### Changes Made

#### 1. Fixed Begin/End Balance in `uiElements()` (`src/io/scene.cpp`)

**Before:**
```cpp
if (!ImGui::Begin("Properties_Prop", p_open, window_flags)) {
    ImGui::End();  // Wrong: Always called End() even when window was closed
    return;
}
// ... file dialog code ...
// Missing: No ImGui::End() call when window is open!
```

**After:**
```cpp
if (!ImGui::Begin("Properties_Prop", p_open, window_flags)) {
    // Only call End() if window is collapsed (not closed)
    if (p_open && *p_open) {
        ImGui::End();  // Window is collapsed, need to balance Begin()
    }
    // If window was closed (p_open is false), don't call End()
    return;
}
// ... file dialog code ...
// Call propertyPanel() here (inside the begun window)
if (p_open && *p_open) {
    mPropertyPanel.propertyPanel(p_open);
}
ImGui::End();  // Always call End() when Begin() succeeded
```

#### 2. Removed ImGui::End() from `propertyPanel()` (`src/io/ui/property_panel.cpp`)

**Before:**
```cpp
void PropertyPanel::propertyPanel(bool* p_open) {
    // ... window content ...
    ImGui::PopItemWidth();
    ImGui::End();  // Wrong: End() should be called by caller
}
```

**After:**
```cpp
void PropertyPanel::propertyPanel(bool* p_open) {
    // ... window content ...
    ImGui::PopItemWidth();
    // DO NOT call ImGui::End() here - it's called by the caller (uiElements)
}
```

#### 3. Removed Duplicate `propertyPanel()` Call (`src/io/scene.cpp`)

**Before:**
```cpp
uiElements(&show_property_panel);
if (show_property_panel) {
    mPropertyPanel.propertyPanel(&show_property_panel);  // Called separately
}
```

**After:**
```cpp
uiElements(&show_property_panel);
// propertyPanel() is now called inside uiElements() when window is open
```

## Key Principles

### ImGui Window Lifecycle
1. **Begin/End Must Be Balanced**: Every `ImGui::Begin()` that returns `true` must have a matching `ImGui::End()`.
2. **Collapsed vs Closed**: 
   - **Collapsed**: Window exists but is minimized. `Begin()` returns `false`, but you still need to call `End()`.
   - **Closed**: Window is destroyed. `Begin()` returns `false`, and you should NOT call `End()`.
3. **Check `p_open` Flag**: When `Begin()` returns `false`, check `*p_open` to determine if window was closed (`false`) or collapsed (`true`).

### Window Management Pattern
- **Single Responsibility**: One function should handle both `Begin()` and `End()` for a window.
- **Content Rendering**: Content functions (like `propertyPanel()`) should only render content, not manage window lifecycle.
- **Safety Checks**: Always check `p_open` before accessing window content.

## Files Modified

1. **`src/io/scene.cpp`**
   - Fixed `uiElements()` to properly handle window closing
   - Moved `propertyPanel()` call inside `uiElements()` before `End()`
   - Added proper collapsed vs closed detection
   - Removed duplicate `propertyPanel()` call from `update()`

2. **`src/io/ui/property_panel.cpp`**
   - Removed `ImGui::End()` call from `propertyPanel()`
   - Added comment explaining that End() is called by caller

## Testing

To verify the fix:
1. Open the Properties window
2. Click the 'X' close button
3. Verify: No crash occurs, window closes cleanly
4. Re-open window from menu (if available) to verify it can be reopened

## Related Issues

This fix ensures:
- ✅ No crash when closing Properties window
- ✅ Proper Begin/End balance for ImGui windows
- ✅ Clean separation between window management and content rendering
- ✅ Safe handling of window state (open/closed/collapsed)

## Future Considerations

When adding new ImGui windows:
1. Keep `Begin()` and `End()` in the same function
2. Check `p_open` flag to distinguish between collapsed and closed states
3. Don't call content rendering functions after window is closed
4. Follow the pattern: `Begin()` → content → `End()` all in one place

## Summary

The crash was caused by calling `ImGui::End()` in `propertyPanel()` when the window was already closed (or never begun). The fix ensures:
- `Begin()` and `End()` are balanced in `uiElements()`
- `propertyPanel()` only renders content, doesn't manage window lifecycle
- Proper detection of collapsed vs closed window states
- No duplicate or missing `End()` calls
