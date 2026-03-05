# ImGui Docking Fix

## Problem
The application was showing an error message: "ERROR: Docking is not enabled! See Demo Configuration. Set io.ConfigFlags |= ImGuiConfigFlags_DockingEnable in your code, or click here"

## Root Cause
The docking flag (`ImGuiConfigFlags_DockingEnable`) was already being set in `setParameters()`, but there was an issue with how the `ImGuiIO` object was being accessed. The code was using a member variable `io` of type `ImGuiIO` (value type) instead of always using `ImGui::GetIO()` which returns a reference to the actual IO object managed by ImGui.

## Solution Applied

### Changes Made to `src/io/scene.cpp`:

1. **Line 87**: Changed from assignment to reference:
   ```cpp
   // Before:
   io = ImGui::GetIO(); (void)io;
   
   // After:
   ImGuiIO& io = ImGui::GetIO(); (void)io;
   ```

2. **Added verification check** (after setting flags):
   ```cpp
   // Verify docking is enabled (debug check)
   if (!(io.ConfigFlags & ImGuiConfigFlags_DockingEnable)) {
       std::cout << "WARNING: Docking flag not set correctly!" << std::endl;
   }
   ```

3. **Line 176**: Updated viewport check to use direct reference:
   ```cpp
   // Before:
   if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
   
   // After:
   ImGuiIO& io_ref = ImGui::GetIO();
   if (io_ref.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
   ```

## Configuration Flags Set

The following ImGui configuration flags are enabled in `setParameters()`:

- ✅ `ImGuiConfigFlags_NavEnableKeyboard` - Keyboard navigation
- ✅ `ImGuiConfigFlags_DockingEnable` - **Docking support** (this was the missing piece)
- ✅ `ImGuiConfigFlags_ViewportsEnable` - Multi-viewport support

## How Docking Works

With docking enabled, you can:
- Drag windows to dock them together
- Create tab groups
- Split panes horizontally or vertically
- Undock windows to float them

The docking system is initialized automatically when `ImGuiConfigFlags_DockingEnable` is set and `ImGui::CreateContext()` is called.

## Testing

After rebuilding the application:
1. The error message should no longer appear
2. You should be able to drag and dock windows
3. The docking controls should be visible in the UI

## Notes

- The member variable `io` in `scene.h` is still present but should ideally be removed in favor of always using `ImGui::GetIO()` directly
- All flag checks now use `ImGui::GetIO()` to ensure they're checking the actual current state
- The docking flag must be set **before** any ImGui windows are created

## Related Files

- `src/io/scene.cpp` - Main scene implementation with ImGui setup
- `src/io/scene.h` - Scene class header (contains `ImGuiIO io;` member)
