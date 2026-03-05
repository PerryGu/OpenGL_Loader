# Fix: Windows Not Anchoring/Docking

## Problem
The UI windows (Properties_Prop, Outliner, timeSlider) are not automatically docking to the sides of the main window on startup, even though docking is enabled.

## Root Cause
The windows have saved positions in `imgui.ini` that override the default docking layout. When ImGui loads, it restores window positions from `imgui.ini` before the DockBuilder layout is applied.

## Solution

### Option 1: Reset Layout (Quick Fix)
Delete the `imgui.ini` file to reset all window positions:

```powershell
.\reset_imgui_layout.ps1
```

Or manually delete:
- `imgui.ini` (in project root)
- `out\build\x64-Debug\Debug\imgui.ini` (in output directory)

Then rebuild and run - windows will dock automatically.

### Option 2: Code Fix (Permanent)
The DockBuilder setup is already in place. The issue is that saved window positions take precedence. The code will:
1. Set up default docking layout on first run
2. Windows will dock automatically if `imgui.ini` doesn't exist
3. If `imgui.ini` exists with old positions, delete it to reset

## How It Works

1. **DockBuilder Setup**: Creates the docking layout structure
   - Left panel (20%): Properties_Prop, Outliner
   - Bottom panel (15%): timeSlider
   - Center: Main viewport

2. **Window Docking**: `DockBuilderDockWindow()` assigns windows to dock nodes
   - This happens BEFORE windows are created
   - Windows will automatically dock when created

3. **Persistence**: After first run, layout is saved to `imgui.ini`
   - Future runs restore the layout automatically
   - Users can still rearrange and it will be saved

## Current Status

The code is correct, but you need to:
1. **Delete `imgui.ini`** to reset window positions
2. **Rebuild and run** the application
3. Windows will automatically dock on first run

## Verification

After resetting and running:
- ✅ Properties_Prop should dock to left side (top)
- ✅ Outliner should dock to left side (below Properties_Prop)
- ✅ timeSlider should dock to bottom
- ✅ Main viewport should be in center
- ✅ Windows should be anchored and not float

## Files

- `reset_imgui_layout.ps1` - Script to delete imgui.ini files
- `src/io/scene.cpp` - Contains DockBuilder setup code
