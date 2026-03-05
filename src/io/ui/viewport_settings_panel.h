#ifndef VIEWPORT_SETTINGS_PANEL_H
#define VIEWPORT_SETTINGS_PANEL_H

#include <imgui/imgui.h>

// Forward declarations
class Camera;
class Grid;

class ViewportSettingsPanel {
public:
    ViewportSettingsPanel();
    
    // Render the Viewport Settings window
    // Parameters:
    //   camera: Reference to camera for smooth camera settings
    //   grid: Pointer to grid for grid settings (can be nullptr)
    //   clearColor: Reference to background color (will be modified)
    //   gizmoPositionValid: Reference to gizmo position validity flag (for framing trigger)
    //   cameraFramingRequested: Reference to camera framing request flag (will be set if needed)
    void renderPanel(Camera& camera, Grid* grid, ImVec4& clearColor, bool& gizmoPositionValid, bool& cameraFramingRequested);
    
    // Window visibility control
    bool& getWindowVisibility() { return show_viewport_settings; }
    bool isWindowVisible() const { return show_viewport_settings; }
    void setWindowVisible(bool visible) { show_viewport_settings = visible; }
    
    // Getters for UI state (used by Scene for syncing with settings)
    bool getSmoothCameraUIState() const { return m_smoothCameraUIState; }
    float getSmoothCameraSpeed() const { return m_smoothCameraSpeed; }
    float getFpsScale() const { return m_fpsScale; }
    bool isFramingDistanceSliderActive() const { return m_framingDistanceSliderActive; }
    bool getVSyncUIState() const { return m_vSyncEnabled; }
    
    // Setters for UI state (used by Scene for loading from settings)
    void setSmoothCameraUIState(bool state) { m_smoothCameraUIState = state; }
    void setSmoothCameraSpeed(float speed) { m_smoothCameraSpeed = speed; }
    void setFpsScale(float scale) { m_fpsScale = scale; }
    void setVSyncUIState(bool state) { m_vSyncEnabled = state; }
    
private:
    // Window visibility flag
    bool show_viewport_settings = false;
    
    // UI state variables
    bool m_smoothCameraUIState = false;  // UI checkbox state for smooth camera
    float m_smoothCameraSpeed = 15.0f;  // UI slider value for smooth camera transition speed
    float m_fpsScale = 1.5f;  // Scale factor for FPS counter text size (0.5 to 5.0)
    bool m_framingDistanceSliderActive = false;  // True if framing distance slider is currently being dragged
    bool m_vSyncEnabled = true;
};

#endif // VIEWPORT_SETTINGS_PANEL_H
