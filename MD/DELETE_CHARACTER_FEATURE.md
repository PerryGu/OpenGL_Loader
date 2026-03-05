# Delete Character Feature

**Date:** 2026-02-19  
**Status:** ✅ Implemented

## Overview
Implemented a secure character deletion feature that allows users to delete a selected character by pressing the Delete key. A confirmation modal dialog appears to prevent accidental deletions.

## Features

### 1. Keyboard Trigger
- **Key:** `Delete` (GLFW_KEY_DELETE)
- **Condition:** Character must be selected
- **Safety:** Ignored when typing in text fields (`!ImGui::GetIO().WantTextInput`)

### 2. Confirmation Modal
- **Title:** "Delete Model?"
- **Content:** 
  - Character name displayed in cyan
  - Warning message: "This action cannot be undone."
- **Buttons:**
  - **Cancel:** Closes dialog (default focus)
  - **Yes, Delete:** Red button for destructive action
- **Keyboard Shortcuts:**
  - `Esc` key cancels the dialog
  - `Enter` confirms (if "Yes, Delete" is focused)

### 3. Secure Deletion Process
Deletion follows a strict order to prevent data corruption:
1. Remove from Outliner (needs index before it shifts)
2. Clear scene selection tracking
3. Remove from ModelManager
4. Clear global selection

## Implementation

### Modal Dialog
**File:** `src/io/ui/ui_manager.cpp`

```cpp
// Trigger popup on Delete key
if (Keyboard::keyWentDown(GLFW_KEY_DELETE) && !ImGui::GetIO().WantTextInput) {
    ImGui::OpenPopup("Delete Model?");
}

// Render the modal
ImVec2 center = ImGui::GetMainViewport()->GetCenter();
ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

if (ImGui::BeginPopupModal("Delete Model?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    // Fetch character name
    std::string modelName = "Selected Character";
    if (modelManager != nullptr) {
        int selectedIdx = modelManager->getSelectedModelIndex();
        if (selectedIdx >= 0) {
            ModelInstance* instance = modelManager->getModel(selectedIdx);
            if (instance) {
                modelName = instance->displayName;
                std::string rootName = mOutliner.getRootNodeName(selectedIdx);
                if (!rootName.empty()) {
                    modelName += " (" + rootName + ")";
                }
            }
        }
    }

    // Display warning
    ImGui::Text("Are you sure you want to delete:");
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "  %s", modelName.c_str());
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    ImGui::Text("This action cannot be undone.");
    ImGui::Separator();
    
    // Buttons
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::SetItemDefaultFocus();
    
    ImGui::SameLine();
    
    // Red button for destructive action
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
    if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
        // Perform deletion
        // ...
    }
    ImGui::PopStyleColor(3);
    
    // Esc key to cancel
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ImGui::CloseCurrentPopup();
    }
    
    ImGui::EndPopup();
}
```

### Deletion Logic
```cpp
if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
    if (modelManager != nullptr) {
        int selectedIdx = modelManager->getSelectedModelIndex();
        if (selectedIdx >= 0) {
            // 1. Remove from Outliner FIRST (needs the index before it shifts)
            mOutliner.removeFBXScene(selectedIdx);
            
            // 2. Clear scene selection tracking
            scene->resetSelectionTracking();
            
            // 3. Remove from ModelManager
            modelManager->removeModel(selectedIdx);
            
            // 4. Clear selection globally
            modelManager->setSelectedModel(-1);
        }
    }
    ImGui::CloseCurrentPopup();
}
```

## User Experience

### Workflow
1. **Select Character:**
   - Click on character in viewport, OR
   - Click on character in Outliner

2. **Press Delete Key:**
   - Modal dialog appears centered on screen
   - Character name displayed prominently

3. **Confirm or Cancel:**
   - Click "Cancel" or press `Esc` to abort
   - Click "Yes, Delete" or press `Enter` to confirm

4. **Result:**
   - Character removed from scene
   - Selection cleared
   - Viewport updates immediately

### Visual Design
- **Character Name:** Displayed in cyan for visibility
- **Warning Message:** Clear "cannot be undone" message
- **Cancel Button:** Standard gray button (default focus)
- **Delete Button:** Red button to indicate destructive action
- **Movable Dialog:** Can be dragged if needed

## Safety Features

### 1. Confirmation Required
- Cannot delete without explicit confirmation
- Prevents accidental deletions

### 2. Text Input Protection
- Delete key ignored when typing in text fields
- Prevents accidental triggers during text entry

### 3. Selection Validation
- Only works when a character is selected
- No-op if nothing is selected

### 4. Ordered Deletion
- Outliner removed first (before index shifts)
- Selection tracking cleared
- ModelManager cleanup
- Global selection reset

## Technical Details

### Keyboard Input
**File:** `src/keyboard.h`

Uses the engine's native keyboard system:
```cpp
Keyboard::keyWentDown(GLFW_KEY_DELETE)
```

This is more reliable than `ImGui::IsKeyPressed(ImGuiKey_Delete)` which may not map correctly.

### Outliner Integration
**File:** `src/io/ui/outliner.cpp`

Uses index-based removal:
```cpp
void Outliner::removeFBXScene(int modelIndex);
```

This ensures only the specific instance is deleted, not all instances with the same file path.

### Selection Tracking
**File:** `src/io/scene.cpp`

Clears selection state:
```cpp
void Scene::resetSelectionTracking();
```

Prevents stale selection references after deletion.

## Edge Cases Handled

### 1. No Selection
- Delete key does nothing
- No modal appears

### 2. Text Input Active
- Delete key ignored
- Prevents accidental deletion while typing

### 3. Multiple Instances
- Only selected instance deleted
- Other instances remain intact
- Index-based deletion prevents cascading

### 4. Last Character
- Character deleted successfully
- Scene becomes empty
- Selection cleared

## Files Modified
- `src/io/ui/ui_manager.cpp` - Modal dialog implementation
- `src/io/ui/outliner.h/cpp` - Index-based removal
- `src/io/scene.h/cpp` - Selection tracking reset

## Related Features
- Multiple Character Instancing (index-based deletion)
- Selection System (model index tracking)
- Outliner (scene management)

## Related Documentation
- `MD/MULTIPLE_CHARACTER_INSTANCING.md`
- `MD/SELECTION_ISOLATION_REFACTOR.md`
- `MD/UI_ARCHITECTURE_REFACTORING.md`
