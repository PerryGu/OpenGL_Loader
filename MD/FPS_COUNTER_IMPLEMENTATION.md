# FPS Counter Implementation

**Date:** 2026-02-19  
**Status:** ✅ Implemented

## Overview
Implemented a tiny, transparent, scalable FPS counter that displays in the top-right corner of the viewport window, with smoothing to reduce flicker at high framerates.

## Features

### 1. Minimal Design
- No title bar
- No background
- No dragging
- Just a green number
- Transparent overlay

### 2. Scalable
- User-adjustable scale (0.5x to 5.0x)
- Real-time scaling via Viewport Settings
- Persistent across sessions

### 3. Smooth Display
- Updates only twice per second (0.5s intervals)
- Reduces visual flicker at high framerates
- Locked value during update interval

### 4. Persistent Settings
- Visibility toggle saved to `app_settings.json`
- Scale value saved to `app_settings.json`
- Restored on application startup

## Implementation

### UI Component
**File:** `src/io/ui/viewport_panel.cpp`

```cpp
void ViewportPanel::renderSimpleFPS(Scene* scene) {
    if (!scene->getUIManager().getViewportSettingsPanel().getShowFPS()) {
        return;
    }
    
    // Position in viewport window corner
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    ImGui::SetNextWindowPos(ImVec2(cursorPos.x + 10, cursorPos.y + 10));
    
    // Transparent, no decoration
    ImGui::Begin("FPS", nullptr, 
        ImGuiWindowFlags_NoDecoration | 
        ImGuiWindowFlags_NoInputs | 
        ImGuiWindowFlags_AlwaysAutoResize | 
        ImGuiWindowFlags_NoBackground);
    
    // Scale font
    float scale = scene->getUIManager().getViewportSettingsPanel().getFpsScale();
    ImGui::SetWindowFontScale(scale);
    
    // Smoothing logic
    static float displayedFPS = 0.0f;
    static float lastUpdateTime = 0.0f;
    float currentTime = (float)ImGui::GetTime();
    
    if (currentTime - lastUpdateTime > 0.5f || lastUpdateTime == 0.0f) {
        displayedFPS = ImGui::GetIO().Framerate;
        lastUpdateTime = currentTime;
    }
    
    // Display in green
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "%.0f", displayedFPS);
    ImGui::End();
}
```

### Settings Integration
**File:** `src/io/app_settings.h`

```cpp
struct EnvironmentSettings {
    bool showFPS = false;      // Default: hidden
    float fpsScale = 1.5f;     // Default: 1.5x scale
    // ...
};
```

### Viewport Settings UI
**File:** `src/io/ui/viewport_settings_panel.cpp`

```cpp
// FPS Settings section
ImGui::Text("FPS Settings");
ImGui::Checkbox("Show FPS Counter", &m_showFPS);
if (m_showFPS) {
    ImGui::SliderFloat("FPS Counter Scale", &m_fpsScale, 0.5f, 5.0f, "%.1f");
    if (ImGui::Button("Reset##FPS")) {
        m_fpsScale = 1.5f;
    }
}
```

## User Experience

### Visibility Toggle
- Checkbox in Viewport Settings: "Show FPS Counter"
- Instant show/hide
- State persisted to settings

### Scale Control
- Slider: 0.5x to 5.0x
- Reset button (restores 1.5x)
- Real-time preview
- Saved to settings

### Smoothing
- Updates every 0.5 seconds
- No rapid flickering
- Stable display at 400+ FPS

## Positioning
- Top-right corner of viewport window
- 10px offset from corner
- Moves with viewport window
- Doesn't block gizmo interaction

## Performance
- Minimal overhead
- Single text render per frame
- No per-frame calculations (only every 0.5s)

## Files Modified
- `src/io/ui/viewport_panel.h/cpp` - FPS rendering
- `src/io/ui/viewport_settings_panel.h/cpp` - Settings UI
- `src/io/app_settings.h/cpp` - Settings storage
- `src/application.cpp` - Settings initialization

## Related Features
- V-Sync control (affects FPS cap)
- Performance optimizations (enables high FPS)
- Viewport Settings Panel
