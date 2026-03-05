#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <imgui/imgui.h>
#include <imgui/ImFileBrowser.h>
#include "property_panel.h"
#include "outliner.h"
#include "timeSlider.h"
#include "viewport_settings_panel.h"
#include "viewport_panel.h"
#include "debug_panel.h"

// Forward declarations
class Scene;
class ModelManager;

class UIManager {
public:
    UIManager();
    
    // Render the entire editor UI (main menu, dockspace, panels)
    void renderEditorUI(Scene* scene);
    
    // Getters for UI panels (used by Scene)
    Outliner& getOutliner() { return mOutliner; }
    const Outliner& getOutliner() const { return mOutliner; }
    PropertyPanel& getPropertyPanel() { return mPropertyPanel; }
    const PropertyPanel& getPropertyPanel() const { return mPropertyPanel; }
    ViewportSettingsPanel& getViewportSettingsPanel() { return m_viewportSettingsPanel; }
    const ViewportSettingsPanel& getViewportSettingsPanel() const { return m_viewportSettingsPanel; }
    TimeSlider& getTimeSlider() { return mTimeSlider; }
    const TimeSlider& getTimeSlider() const { return mTimeSlider; }
    DebugPanel& getDebugPanel() { return m_debugPanel; }
    
    // Check if viewport window is visible
    bool isViewportWindowVisible() const { return show_viewport; }
    
    // Set viewport window visibility (used by Scene when user closes window)
    void setViewportWindowVisible(bool visible) { show_viewport = visible; }
    
private:
    // UI panel instances
    ImGui::FileBrowser mFileDialog;
    PropertyPanel mPropertyPanel;
    Outliner mOutliner;
    TimeSlider mTimeSlider;
    ViewportSettingsPanel m_viewportSettingsPanel;
    ViewportPanel mViewportPanel;
    DebugPanel m_debugPanel;
    
    // Window visibility flags
    bool show_property_panel = true;
    bool show_outliner = true;
    bool mShow_timeline = true;
    bool show_viewport = true;
    bool m_showDebugPanel = false;
    
    // Private UI methods (moved from Scene)
    void appDockSpace(bool* p_open);
    void ShowDockingDisabledMessage();
    void ShowAppMainMenuBar(Scene* scene);
};

#endif // UI_MANAGER_H
