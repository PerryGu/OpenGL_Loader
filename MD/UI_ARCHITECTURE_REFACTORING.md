# UI Architecture Refactoring

**Date:** 2026-02-18 to 2026-02-19  
**Status:** ✅ Implemented

## Overview
Refactored the UI system following the Single Responsibility Principle, extracting UI components into dedicated classes for better modularity and maintainability.

## Architecture Changes

### Before: Monolithic Scene Class
- All UI code in `Scene` class
- Mixed rendering, UI, and scene management
- Difficult to maintain and test

### After: Modular UI System
```
Scene
├── UIManager
│   ├── PropertyPanel
│   ├── Outliner
│   ├── TimeSlider
│   ├── ViewportSettingsPanel
│   ├── ViewportPanel
│   └── GizmoManager
└── (Scene rendering logic)
```

## New Classes

### 1. UIManager
**File:** `src/io/ui/ui_manager.h/cpp`

**Responsibilities:**
- Global editor UI layout
- Panel ownership and lifecycle
- Main menu bar
- Docking space management
- File dialog integration

**Key Methods:**
```cpp
void renderEditorUI(Scene* scene);
Outliner& getOutliner();
PropertyPanel& getPropertyPanel();
```

### 2. ViewportSettingsPanel
**File:** `src/io/ui/viewport_settings_panel.h/cpp`

**Responsibilities:**
- Camera settings (sensitivity, speed, framing)
- Grid settings (size, spacing, colors)
- Environment settings (background color, bounding boxes)
- Interface settings (FPS counter, scale)

**Extracted From:** `Scene::ShowViewportSettingsWindow()`

### 3. ViewportPanel
**File:** `src/io/ui/viewport_panel.h/cpp`

**Responsibilities:**
- ImGui "Viewport" window rendering
- Menu bar (Tools menu)
- Mouse state capture
- FPS counter rendering
- Viewport image display

**Extracted From:** `Scene::renderViewportWindow()`

### 4. GizmoManager
**File:** `src/io/ui/gizmo_manager.h/cpp`

**Responsibilities:**
- ImGuizmo integration
- Gizmo rendering and manipulation
- World-to-local space conversion
- Keyboard shortcuts (Maya-style)
- Transform value printing

**Extracted From:** `Scene::renderViewportWindow()` (gizmo block)

## Benefits

### 1. Single Responsibility
Each class has one clear purpose:
- `UIManager`: UI layout and coordination
- `ViewportPanel`: Viewport window rendering
- `GizmoManager`: 3D gizmo manipulation
- `ViewportSettingsPanel`: Settings UI

### 2. Maintainability
- Easier to locate and fix UI bugs
- Clear separation of concerns
- Reduced coupling between components

### 3. Testability
- Each component can be tested independently
- Mock dependencies easily
- Isolated functionality

### 4. Code Organization
- Related code grouped together
- Clear file structure
- Easier for new developers to understand

## Migration Path

### Scene Class Changes
**Removed:**
- `void renderViewportWindow()`
- `void renderSimpleFPS()`
- `void ShowViewportSettingsWindow()`
- `void ShowAppMainMenuBar()`
- `void appDockSpace()`
- All UI state variables

**Added:**
- `UIManager m_uiManager;`
- Getters for UI components

### Access Pattern
**Before:**
```cpp
mPropertyPanel.propertyPanel(...);
mOutliner.outlinerPanel(...);
```

**After:**
```cpp
m_uiManager.getPropertyPanel().propertyPanel(...);
m_uiManager.getOutliner().outlinerPanel(...);
```

## Files Created
- `src/io/ui/ui_manager.h/cpp`
- `src/io/ui/viewport_settings_panel.h/cpp`
- `src/io/ui/viewport_panel.h/cpp`
- `src/io/ui/gizmo_manager.h/cpp`

## Files Modified
- `src/io/scene.h/cpp` - Removed UI code, added UIManager
- `src/io/ui/property_panel.h/cpp` - Minor adjustments
- `src/io/ui/outliner.h/cpp` - Minor adjustments

## Related Documentation
- `MD/CODE_ORGANIZATION.md`
- `MD/IMGUIZMO_CODE_REFACTORING.md`
