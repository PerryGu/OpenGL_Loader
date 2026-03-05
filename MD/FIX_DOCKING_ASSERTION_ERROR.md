# Fix: ImGui "Calling End() too many times!" Assertion Error

## Problem
After implementing default docking layout, the application crashes with:
```
Assertion failed: (g.CurrentWindowStack.Size > 1) && "Calling End() too many times!"
```

## Root Cause
The DockBuilder functions were being called **after** `DockSpace()`, but they need to be called **before**. Additionally, the layout setup might be interfering with window creation.

## Solution Applied

### Changes Made to `src/io/scene.cpp`:

1. **Moved DockBuilder BEFORE DockSpace**:
   ```cpp
   // BEFORE (WRONG):
   ImGui::DockSpace(dockspace_id, ...);
   DockBuilder...();  // ❌ Too late!
   
   // AFTER (CORRECT):
   DockBuilder...();  // ✅ Setup first
   ImGui::DockSpace(dockspace_id, ...);  // Then create dockspace
   ```

2. **Added check to prevent re-initialization**:
   ```cpp
   ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
   if (node == nullptr || node->IsEmpty())
   {
       // Only set up layout if it doesn't exist
   }
   ```

## Why This Fixes the Error

The "Calling End() too many times!" error occurs when:
- DockBuilder operations interfere with window stack
- Layout is set up after windows are already created
- Multiple layout setups conflict

By calling DockBuilder **before** DockSpace and checking if layout exists, we ensure:
- ✅ Layout is set up before any windows are docked
- ✅ No duplicate layout initialization
- ✅ Proper Begin/End pairing for all windows

## Testing

After rebuilding:
1. The assertion error should be gone
2. Windows should dock automatically on first run
3. Layout should persist across sessions

## Files Modified

- `src/io/scene.cpp` - Fixed DockBuilder call order and added existence check
