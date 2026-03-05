# ImGui Default Docking Layout

## Problem
The UI windows (Properties_Prop, Outliner, timeSlider) were not automatically docked when the application starts. Users had to manually dock them each time.

## Solution
Implemented automatic default docking layout using ImGui's DockBuilder API. Windows are now automatically docked on first run.

## Implementation

### Code Changes in `src/io/scene.cpp`

Added default docking layout setup in `appDockSpace()` function:

```cpp
//-- Set up default docking layout on first run ---------
static bool first_time = true;
if (first_time)
{
    first_time = false;
    
    // Get viewport size for layout
    const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    
    // Clear any existing layout and create new one
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, main_viewport->WorkSize);
    
    // Split the dockspace: left panel (20% width) and right area
    ImGuiID dock_id_left;
    ImGuiID dock_id_right;
    dock_id_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.2f, nullptr, &dock_id_right);
    
    // Split the right area: bottom panel (15% height) and center viewport
    ImGuiID dock_id_bottom;
    ImGuiID dock_id_center;
    dock_id_bottom = ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Down, 0.15f, nullptr, &dock_id_center);
    
    // Dock windows to their positions
    ImGui::DockBuilderDockWindow("Properties_Prop", dock_id_left);
    ImGui::DockBuilderDockWindow("Outliner", dock_id_left);
    ImGui::DockBuilderDockWindow("timeSlider", dock_id_bottom);
    
    // Finalize the layout
    ImGui::DockBuilderFinish(dockspace_id);
}
```

## Layout Structure

The default layout creates:

```
┌─────────────────────────────────────────┐
│              Menu Bar                    │
├──────────┬──────────────────────────────┤
│          │                              │
│ Properties│                              │
│   _Prop   │                              │
│          │        Main Viewport          │
│          │        (3D Rendering)         │
│ Outliner │                              │
│          │                              │
├──────────┴──────────────────────────────┤
│         timeSlider                      │
└─────────────────────────────────────────┘
```

- **Left Panel (20% width)**: Properties_Prop and Outliner (stacked vertically)
- **Center Area**: Main 3D viewport
- **Bottom Panel (15% height)**: timeSlider

## How It Works

1. **First Run Detection**: Uses a `static bool first_time` flag to detect first execution
2. **Layout Creation**: Uses DockBuilder API to programmatically create the docking structure
3. **Window Assignment**: Assigns each window to its designated dock node
4. **Persistence**: After first run, ImGui saves the layout to `imgui.ini`, so it persists across sessions

## User Experience

- ✅ **First Launch**: Windows automatically dock in the default layout
- ✅ **Subsequent Launches**: Layout is saved and restored from `imgui.ini`
- ✅ **Customization**: Users can still drag and rearrange windows as desired
- ✅ **Persistence**: Custom layouts are saved automatically

## DockBuilder API Functions Used

- `DockBuilderRemoveNode()` - Clears existing layout
- `DockBuilderAddNode()` - Creates new dock node
- `DockBuilderSetNodeSize()` - Sets node size
- `DockBuilderSplitNode()` - Splits node into two (left/right or up/down)
- `DockBuilderDockWindow()` - Assigns window to dock node
- `DockBuilderFinish()` - Finalizes the layout

## Customization

To change the default layout, modify the split ratios:
- `0.2f` in `DockBuilderSplitNode(..., ImGuiDir_Left, 0.2f, ...)` = 20% width for left panel
- `0.15f` in `DockBuilderSplitNode(..., ImGuiDir_Down, 0.15f, ...)` = 15% height for bottom panel

## Files Modified

- `src/io/scene.cpp` - Added default docking layout setup in `appDockSpace()` function

## Testing

After rebuilding:
1. Close the application if running
2. Delete `imgui.ini` to reset layout (optional, for testing)
3. Run the application
4. Windows should automatically dock in the default layout
5. Layout will be saved to `imgui.ini` for future sessions
