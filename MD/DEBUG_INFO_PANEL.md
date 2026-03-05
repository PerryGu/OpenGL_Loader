# Debug / Info Panel Implementation

## Date
Created: 2026-02-23

## Overview
Added a new "Debug / Info" floating window that displays scene statistics and an event log. The panel can be toggled from the File menu and provides real-time information about loaded models.

## Implementation

### Files Created

#### 1. `src/io/ui/debug_panel.h`
- Defines the `DebugPanel` class
- Public methods:
  - `void renderPanel(bool* p_open, class ModelManager* modelManager)` - Renders the panel UI
  - `void addLog(const std::string& message)` - Adds messages to the event log
- Private members:
  - `std::vector<std::string> m_eventLog` - Stores log entries
  - `MAX_LOG_ENTRIES = 1000` - Maximum log entries to prevent memory growth

#### 2. `src/io/ui/debug_panel.cpp`
- Implements the DebugPanel functionality
- `renderPanel()` displays:
  - Total number of models in the scene
  - Per-model statistics (Display Name, Polygon Count, Animation Duration, FPS)
  - Event log with auto-scrolling
- `addLog()` adds timestamped messages to the log (with size limit)

### Files Modified

#### 3. `src/graphics/model.h` and `src/graphics/model.cpp`
Added new public method:
```cpp
size_t getPolygonCount() const;
```

**Implementation**: Iterates through all meshes and calculates total polygon count by summing `mesh.indices.size() / 3` for each mesh (assuming triangles).

**Why**: Provides polygon count information for debug panel display. Each polygon is represented by 3 indices in triangle-based rendering.

#### 4. `src/io/ui/ui_manager.h`
- Added include: `#include "debug_panel.h"`
- Added private member: `DebugPanel m_debugPanel;`
- Added private member: `bool m_showDebugPanel = false;` (visibility flag)
- Added public getter: `DebugPanel& getDebugPanel() { return m_debugPanel; }`

**Why**: Integrates DebugPanel into the UI system, following the same pattern as other UI panels (PropertyPanel, Outliner, etc.).

#### 5. `src/io/ui/ui_manager.cpp`
- **Menu Item**: Added in `ShowAppMainMenuBar()` under File menu:
  ```cpp
  if (ImGui::MenuItem("Debug / Info", NULL)) { m_showDebugPanel = !m_showDebugPanel; }
  ```
  - Located immediately below "Import" menu item
  - Toggles panel visibility on/off

- **Rendering**: Added at the end of `renderEditorUI()`:
  ```cpp
  if (m_showDebugPanel) {
      ModelManager* modelManager = scene->getModelManager();
      m_debugPanel.renderPanel(&m_showDebugPanel, modelManager);
  }
  ```
  - Only renders when `m_showDebugPanel` is true
  - Passes ModelManager pointer for accessing scene data

## Features

### Scene Statistics
- **Total Models**: Displays the number of loaded models
- **Per-Model Information**:
  - **Display Name**: The filename or "Model N" if no name available
  - **Polygon Count**: Total number of polygons (triangles) in the model
  - **Animation Duration**: Duration of the animation in seconds
  - **FPS**: Frames per second of the animation

### Event Log
- Scrollable child window displaying log messages
- Auto-scrolls to bottom when new items are added
- Maximum of 1000 entries (oldest entries are removed when limit is reached)
- Uses `ImGui::TextUnformatted()` for efficient text rendering

## Usage

1. **Open Panel**: Go to `File > Debug / Info` in the main menu bar
2. **View Statistics**: Panel displays scene statistics and model information
3. **View Log**: Scroll through the event log at the bottom
4. **Close Panel**: Click the X button or toggle the menu item again

## Code Organization

### Modularity
- **DebugPanel Class**: Self-contained panel with its own header and implementation
- **Model Method**: `getPolygonCount()` is a const method that doesn't modify state
- **UI Integration**: Follows the same pattern as other UI panels (PropertyPanel, Outliner)

### Documentation
- Inline comments explain the purpose of each component
- Header file includes class-level documentation
- Implementation includes comments explaining the logic

## Technical Details

### Polygon Count Calculation
- Assumes triangle-based rendering (3 indices per polygon)
- Formula: `totalPolygons = sum(mesh.indices.size() / 3) for all meshes`
- Returns `size_t` for compatibility with large models

### Event Log Management
- Uses `std::vector<std::string>` for storage
- Automatically limits size to prevent memory growth
- Removes oldest entries when limit is reached (FIFO behavior)

### Auto-Scrolling
- Checks if scroll position is at or near the bottom
- Uses `ImGui::SetScrollHereY(1.0f)` to scroll to bottom
- Only scrolls if user hasn't manually scrolled up

## Future Enhancements

Potential improvements:
- Add timestamps to log entries
- Add log filtering (by type, date, etc.)
- Add export log functionality
- Add more detailed statistics (vertex count, bone count, etc.)
- Add performance metrics (FPS, render time, etc.)
- Add model selection highlighting in the panel

## Related Files

- `src/io/ui/debug_panel.h` - Panel header
- `src/io/ui/debug_panel.cpp` - Panel implementation
- `src/graphics/model.h` - Model class with `getPolygonCount()`
- `src/graphics/model.cpp` - `getPolygonCount()` implementation
- `src/io/ui/ui_manager.h` - UI manager with DebugPanel integration
- `src/io/ui/ui_manager.cpp` - Menu item and rendering integration
