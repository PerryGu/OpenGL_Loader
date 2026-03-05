# Logger UI Enhancements

## Date
Created: 2026-02-23

## Overview
Enhanced the Logger / Info panel with color-coded log levels and filtering capabilities. Logs are now displayed with color-coding (Red for ERROR, Yellow for WARNING, White for INFO) and users can filter which log types are displayed using checkboxes.

## Features Implemented

### 1. Color-Coded Log Display
**Purpose**: Make it easier to identify log levels at a glance by using color-coding.

**Implementation**:
- **ERROR Logs**: Displayed in red (`ImVec4(1.0f, 0.4f, 0.4f, 1.0f)`)
- **WARNING Logs**: Displayed in yellow (`ImVec4(1.0f, 1.0f, 0.4f, 1.0f)`)
- **INFO Logs**: Displayed in white (`ImVec4(1.0f, 1.0f, 1.0f, 1.0f)`)
- **Parsing**: Log level is determined by searching for `[ERROR]` or `[WARNING]` tags in the log string
- **Rendering**: Uses `ImGui::PushStyleColor()` and `ImGui::PopStyleColor()` for proper color management

### 2. Log Filtering
**Purpose**: Allow users to show/hide specific log types to focus on relevant information.

**Implementation**:
- **Filter Checkboxes**: Three checkboxes at the top of the Debug Log mode
  - "Info (White)" - Toggles INFO log visibility
  - "Warning (Yellow)" - Toggles WARNING log visibility
  - "Error (Red)" - Toggles ERROR log visibility
- **Filter Logic**: Logs are filtered before rendering (skipped if filter is disabled)
- **Default State**: All filters enabled by default (`m_showInfo = true`, etc.)

### 3. Clear Log Button
**Purpose**: Allow users to clear the log for a fresh start.

**Implementation**:
- **Button**: "Clear" button positioned above filter checkboxes
- **Functionality**: Calls `Logger::getInstance().clear()` to empty all log entries
- **Layout**: Button is on the same line as filter checkboxes using `ImGui::SameLine()`

### 4. Copy-to-Clipboard Functionality
**Purpose**: Allow users to copy log entries to the clipboard for sharing or external analysis.

**Implementation**:
- **Selectable Rows**: Each log entry is rendered as an `ImGui::Selectable` instead of `ImGui::TextUnformatted`
- **Click to Copy**: Clicking any log line copies its full text to the clipboard using `ImGui::SetClipboardText()`
- **Visual Feedback**: Tooltip appears on hover: "Click to copy to clipboard"
- **Unique Identification**: Each log entry uses `ImGui::PushID()` with loop index for proper widget identification
- **Color Preservation**: Log colors are maintained using `ImGui::PushStyleColor()` before rendering the selectable

## Files Modified

### 1. `src/utils/logger.h`
**Added Method**:
```cpp
void clear() { clearLogs(); }  // Convenience method
```

### 2. `src/io/ui/debug_panel.h`
**Added Members**:
```cpp
bool m_showInfo = true;        // Show/hide INFO logs
bool m_showWarning = true;     // Show/hide WARNING logs
bool m_showError = true;       // Show/hide ERROR logs
```

### 3. `src/io/ui/debug_panel.cpp`
**Changes in `renderPanel()` method**:

**Added Clear Button**:
```cpp
if (ImGui::Button("Clear")) {
    Logger::getInstance().clear();
}
ImGui::SameLine();
```

**Added Filter Checkboxes**:
```cpp
ImGui::Checkbox("Info (White)", &m_showInfo);
ImGui::SameLine();
ImGui::Checkbox("Warning (Yellow)", &m_showWarning);
ImGui::SameLine();
ImGui::Checkbox("Error (Red)", &m_showError);
ImGui::Separator();
```

**Updated Log Rendering Loop**:
```cpp
for (size_t i = 0; i < logs.size(); ++i) {
    const auto& log = logs[i];
    
    // Parse log level
    bool isError = log.find("[ERROR]") != std::string::npos;
    bool isWarning = log.find("[WARNING]") != std::string::npos;
    bool isInfo = !isError && !isWarning;
    
    // Apply filters
    if (isError && !m_showError) continue;
    if (isWarning && !m_showWarning) continue;
    if (isInfo && !m_showInfo) continue;
    
    // Set color based on log level
    ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White
    if (isError) {
        color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); // Red
    } else if (isWarning) {
        color = ImVec4(1.0f, 1.0f, 0.4f, 1.0f); // Yellow
    }
    
    // Render as selectable for copy functionality
    ImGui::PushID(static_cast<int>(i)); // Use loop index for unique identifier
    ImGui::PushStyleColor(ImGuiCol_Text, color); // Keep existing color logic
    
    // Draw as a selectable row. false means it won't stay highlighted after clicking
    if (ImGui::Selectable(log.c_str(), false)) {
        ImGui::SetClipboardText(log.c_str());
    }
    ImGui::PopStyleColor();
    
    // Add a helpful tooltip for UX
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Click to copy to clipboard");
    }
    ImGui::PopID();
}
```

## UI Layout

### Debug Log Mode Layout
```
┌─────────────────────────────────────┐
│ Logger / Info                       │
├─────────────────────────────────────┤
│ [Switch to Scene Info]              │
├─────────────────────────────────────┤
│ Event Log:                          │
├─────────────────────────────────────┤
│ [Clear] [Info] [Warning] [Error]   │
├─────────────────────────────────────┤
│ ┌─────────────────────────────────┐ │
│ │ [12:34:56] [INFO] Message...    │ │ ← White
│ │ [12:34:57] [WARNING] Warning... │ │ ← Yellow
│ │ [12:34:58] [ERROR] Error...     │ │ ← Red
│ │ ...                             │ │
│ └─────────────────────────────────┘ │
└─────────────────────────────────────┘
```

## Technical Details

### Log Level Parsing
- Searches for `[ERROR]` tag in log string
- Searches for `[WARNING]` tag in log string
- Defaults to INFO if neither tag is found
- Uses `std::string::find()` for efficient string searching

### Color Management
- Uses ImGui's color stack (`PushStyleColor`/`PopStyleColor`)
- Ensures colors don't leak to other UI elements
- Colors are applied per-log-entry

### Filtering Efficiency
- Filters are applied before rendering (early exit with `continue`)
- No performance impact when filters are active
- Filter state persists across frames (stored in DebugPanel members)

### Clear Functionality
- Calls `Logger::clear()` which uses mutex protection
- Thread-safe log clearing
- Immediately updates UI (logs disappear on next frame)

## Usage

### Viewing Colored Logs
1. Open Logger / Info panel (`File > Logger / Info`)
2. Switch to Debug Log mode (if not already)
3. Logs are automatically color-coded:
   - **Red** for errors
   - **Yellow** for warnings
   - **White** for info messages

### Filtering Logs
1. Use checkboxes at the top to show/hide log types
2. Uncheck "Info (White)" to hide all INFO logs
3. Uncheck "Warning (Yellow)" to hide all WARNING logs
4. Uncheck "Error (Red)" to hide all ERROR logs
5. Check boxes again to show hidden log types

### Clearing Logs
1. Click the "Clear" button
2. All log entries are immediately removed
3. New logs will continue to appear as they are generated

### Copying Log Entries
1. Hover over any log line to see the tooltip: "Click to copy to clipboard"
2. Click on the log line to copy its full text to the clipboard
3. Paste the copied text anywhere (text editor, bug report, etc.)
4. Works with all log types (INFO, WARNING, ERROR)

## Benefits

### 1. Visual Clarity
- Errors stand out in red
- Warnings are easily spotted in yellow
- Info messages remain readable in white

### 2. Focused Debugging
- Filter out noise (hide INFO logs when looking for errors)
- Focus on specific log types
- Reduce visual clutter

### 3. Better Workflow
- Quickly identify problems (red = error)
- Clear log for fresh start
- Customize view based on current task

### 4. Professional Appearance
- Color-coding is standard in development tools
- Makes logs more readable and scannable
- Improves overall user experience

### 5. Easy Log Sharing
- Copy log entries for bug reports or documentation
- Share specific error messages with team members
- Extract log data for external analysis tools

## Related Features

### Logger System
- All logging uses `LOG_INFO()`, `LOG_WARNING()`, `LOG_ERROR()` macros
- Log entries are formatted as `[TIMESTAMP] [LEVEL] message`
- Thread-safe logging with automatic timestamp generation

### Debug Panel
- Two modes: Info (scene statistics) and Debug (event log)
- Auto-scrolling when new logs are added
- Scrollable child window for log display

## Future Enhancements

Potential improvements:
- Add log export functionality (save to file)
- Add search/filter by text content
- Add log level statistics (count of each type)
- Add timestamp filtering (show logs from last N minutes)
- Add log highlighting for specific keywords
- Add multi-line selection (select and copy multiple log entries at once)

## Related Files

- `src/utils/logger.h` - Logger class with `clear()` method
- `src/utils/logger.cpp` - Logger implementation
- `src/io/ui/debug_panel.h` - DebugPanel with filter members
- `src/io/ui/debug_panel.cpp` - Color-coding and filtering implementation
