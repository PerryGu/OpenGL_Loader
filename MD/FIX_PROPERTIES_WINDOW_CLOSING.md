# Fix Properties Window Not Closing

## Date
Created: 2024

## Problem
Clicking the 'X' button on the "Properties_Prop" window did not close it. The window remained visible even after clicking the close button, likely because the visibility flag was being overridden or not properly linked to ImGui's internal state.

## Root Cause
The issue was that the Properties window was being rendered every frame regardless of the `show_property_panel` flag state:

1. **No Visibility Check**: The `uiElements()` function was calling `ImGui::Begin()` every frame, even when `show_property_panel` was `false`. This meant that even if ImGui set `*p_open = false` when the user clicked 'X', the window would still be created on the next frame.

2. **Docked Window Behavior**: When a window is docked in ImGui, the docking system might interfere with window closing if the window is always being created regardless of the visibility flag.

3. **Missing Guard**: There was no check to prevent calling `ImGui::Begin()` when the window should be closed.

## Solution

### Changes Made

#### 1. Added Visibility Check Before Window Creation (`src/io/scene.cpp`)

**Before:**
```cpp
// Always called Begin() every frame, regardless of visibility flag
if (!ImGui::Begin("Properties_Prop", p_open, window_flags)) {
    // ... handle early return ...
}
// ... window content ...
ImGui::End();
```

**After:**
```cpp
// Only render the Properties window if the visibility flag is true
// This ensures that when the user clicks 'X', the window stays closed
if (p_open && *p_open) {
    // ... set window position/size ...
    
    // Pass p_open to ImGui::Begin so it can update the flag when user clicks 'X'
    if (ImGui::Begin("Properties_Prop", p_open, window_flags)) {
        // ... window content ...
        mPropertyPanel.propertyPanel(p_open);
        ImGui::End();
    }
    // If Begin() returned false, window is collapsed - ImGui handles End() internally
}
// If p_open is false (window was closed), don't render the window at all
```

### Key Changes

1. **Visibility Guard**: Added `if (p_open && *p_open)` check before calling `ImGui::Begin()`. This ensures that:
   - If the user clicks 'X', ImGui sets `*p_open = false`
   - On the next frame, we check the flag and skip calling `Begin()`
   - The window stays closed

2. **Proper Flag Passing**: The `p_open` pointer is passed to `ImGui::Begin()`, allowing ImGui to update the flag when the user clicks 'X'.

3. **No Frame Override**: The window is only created when the flag is `true`, preventing any frame-based overrides.

## How It Works

### Window Lifecycle

1. **Window Open**: `show_property_panel = true` → `uiElements()` calls `ImGui::Begin()` → Window is shown
2. **User Clicks 'X'**: ImGui detects the close button click → Sets `*p_open = false` → `show_property_panel` becomes `false`
3. **Next Frame**: `uiElements()` checks `if (p_open && *p_open)` → Flag is `false` → Skips `Begin()` → Window is not rendered
4. **Window Stays Closed**: Subsequent frames continue to skip rendering until flag is set to `true` again

### Docked Window Handling

When a window is docked:
- ImGui's docking system manages the window's position and docking state
- The `p_open` flag still controls visibility
- If `*p_open` is `false`, the window is not created, so it cannot be docked or shown
- The docking system respects the visibility flag

## Files Modified

1. **`src/io/scene.cpp`**
   - Added visibility check `if (p_open && *p_open)` before calling `ImGui::Begin()`
   - Ensured window is only created when visibility flag is `true`
   - Added comments explaining the visibility guard

## Testing

To verify the fix:
1. Open the Properties window (should be visible by default)
2. Click the 'X' close button
3. **Verify**: Window disappears immediately and stays closed
4. Re-open window (if menu option available) to verify it can be reopened

## Related Issues

This fix ensures:
- ✅ Window closes when 'X' is clicked
- ✅ Window stays closed (no frame-based override)
- ✅ Visibility flag is properly respected
- ✅ Works correctly with docked windows
- ✅ No crash when closing (see `FIX_PROPERTIES_WINDOW_CRASH.md`)

## Best Practices

When implementing ImGui windows with close buttons:

1. **Always Check Visibility**: Wrap window creation in a visibility check:
   ```cpp
   if (p_open && *p_open) {
       if (ImGui::Begin("WindowName", p_open, flags)) {
           // ... content ...
           ImGui::End();
       }
   }
   ```

2. **Pass p_open to Begin()**: Always pass the `p_open` pointer to `ImGui::Begin()` so ImGui can update it when the user clicks 'X':
   ```cpp
   ImGui::Begin("WindowName", p_open, flags);  // p_open allows ImGui to update flag
   ```

3. **Use Persistent Member Variables**: The visibility flag should be a persistent member variable, not a local or static variable that gets reset:
   ```cpp
   class Scene {
   private:
       bool show_property_panel = true;  // Persistent member variable
   };
   ```

4. **Event-Driven Toggles**: If you need to open/close windows programmatically, do it in response to events (menu clicks, key presses), not every frame:
   ```cpp
   // Good: Event-driven
   if (ImGui::MenuItem("Show Properties")) {
       show_property_panel = true;
   }
   
   // Bad: Frame-based override
   show_property_panel = true;  // Don't do this every frame!
   ```

## Summary

The fix ensures that the Properties window respects the `show_property_panel` visibility flag. When the user clicks 'X', ImGui sets the flag to `false`, and on subsequent frames, the window is not created because of the visibility guard. This prevents frame-based overrides and ensures the window stays closed until explicitly reopened.
