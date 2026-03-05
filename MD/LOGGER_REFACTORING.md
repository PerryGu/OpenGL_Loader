# Logger System Refactoring

## Date
Created: 2026-02-23

## Overview
Refactored the logging system from a tightly-coupled DebugPanel-based approach to a professional, decoupled Singleton Logger class with macros. This improves code organization, reusability, and maintainability.

## Implementation

### Files Created

#### 1. `src/utils/logger.h`
- Thread-safe Singleton `Logger` class
- Stores logs in `std::vector<std::string> m_logs`
- Provides `addLog(const std::string& level, const std::string& message)` method with automatic timestamp prepending
- Provides `getLogs()` method to retrieve all logs
- Defines convenience macros:
  - `LOG_INFO(msg)` - Logs with "INFO" level
  - `LOG_WARNING(msg)` - Logs with "WARNING" level
  - `LOG_ERROR(msg)` - Logs with "ERROR" level

**Thread Safety**: Uses `std::mutex` to ensure thread-safe access to log storage.

**Timestamp Format**: `[HH:MM:SS] [LEVEL] message`

#### 2. `src/utils/logger.cpp`
- Implements `Logger::addLog()` with timestamp generation
- Implements `Logger::getTimestamp()` using `std::chrono` and platform-specific time functions
- Implements `Logger::clearLogs()` for log management
- Automatically limits log size to 1000 entries (removes oldest entries when limit is reached)

### Files Modified

#### 3. `src/io/ui/debug_panel.h`
- **Removed**: `m_eventLog` vector member
- **Removed**: `addLog()` public method
- **Removed**: `MAX_LOG_ENTRIES` constant (now handled by Logger)
- **Kept**: `m_isInfoMode` for display mode switching

#### 4. `src/io/ui/debug_panel.cpp`
- **Removed**: `addLog()` implementation
- **Updated**: `renderPanel()` now retrieves logs from `Logger::getInstance().getLogs()`
- **Updated**: Constructor now uses `LOG_INFO()` macro for initialization message
- **Added**: Include for `"../../utils/logger.h"`

#### 5. `src/io/ui/ui_manager.cpp`
- **Added**: Include for `"../../utils/logger.h"`
- **Replaced**: All `m_debugPanel.addLog(...)` calls with `LOG_INFO(...)` macros:
  - "New Scene" menu item → `LOG_INFO("UI Action: File -> New Scene")`
  - "Import" menu item → `LOG_INFO("UI Action: File -> Import")`
  - "Save Settings" menu item → `LOG_INFO("UI Action: File -> Save Settings")`
  - "Yes, Delete" button → `LOG_INFO("UI Action: Deleted model - " + modelName)`

#### 6. `CMakeLists.txt`
- **Added**: Explicit entry for `logger.cpp` to ensure it's included in the build

## Architecture Benefits

### Decoupling
- **Before**: DebugPanel was tightly coupled to UI components that needed to log
- **After**: Any component can log using macros without knowing about DebugPanel

### Reusability
- **Before**: Logging required passing DebugPanel pointer around
- **After**: Logging is available globally via Singleton pattern

### Maintainability
- **Before**: Log storage and display logic were mixed in DebugPanel
- **After**: Log storage (Logger) and display (DebugPanel) are separated

### Thread Safety
- **Before**: No thread safety guarantees
- **After**: Thread-safe logging with mutex protection

### Professional Features
- **Timestamps**: Automatic timestamp generation for all log entries
- **Log Levels**: Support for INFO, WARNING, and ERROR levels
- **Memory Management**: Automatic log size limiting (1000 entries)

## Usage Examples

### Basic Logging
```cpp
#include "../../utils/logger.h"

// Log an info message
LOG_INFO("UI Action: File -> New Scene");

// Log a warning
LOG_WARNING("Model loading took longer than expected");

// Log an error
LOG_ERROR("Failed to load texture: " + filePath);
```

### Retrieving Logs (for display)
```cpp
#include "../../utils/logger.h"

// Get all logs for display
const std::vector<std::string>& logs = Logger::getInstance().getLogs();
for (const auto& log : logs) {
    // Display log entry
}
```

## Code Organization

### Modularity
- **Logger**: Self-contained utility class in `src/utils/`
- **DebugPanel**: UI component that reads from Logger (no direct coupling)
- **UI Components**: Use macros to log (no DebugPanel dependency)

### Separation of Concerns
- **Logger**: Handles log storage, timestamps, and thread safety
- **DebugPanel**: Handles log display in UI
- **UI Components**: Handle their own logic and log events

## Migration Notes

### Removed Dependencies
- No need to pass `DebugPanel*` to UI component functions
- No need to include `debug_panel.h` in files that only need to log

### New Dependencies
- Files that need to log must include `"../../utils/logger.h"`
- Use macros (`LOG_INFO`, `LOG_WARNING`, `LOG_ERROR`) instead of method calls

## Future Enhancements

Potential improvements:
- Add log filtering by level (show only errors, etc.)
- Add log export functionality (save to file)
- Add log rotation (multiple log files)
- Add colored output based on log level
- Add log categories/tags for better organization
- Add performance metrics logging

## Related Files

- `src/utils/logger.h` - Logger class definition
- `src/utils/logger.cpp` - Logger implementation
- `src/io/ui/debug_panel.h` - DebugPanel class (now reads from Logger)
- `src/io/ui/debug_panel.cpp` - DebugPanel implementation
- `src/io/ui/ui_manager.cpp` - UI manager with logging calls
- `CMakeLists.txt` - Build configuration
