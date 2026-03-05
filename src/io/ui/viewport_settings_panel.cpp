#include "viewport_settings_panel.h"
#include "../camera.h"
#include "../../graphics/grid.h"
#include "../app_settings.h"
#include "pch.h"  // Includes version.h via precompiled header
#include <glm/glm.hpp>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

ViewportSettingsPanel::ViewportSettingsPanel() {
    // Initialize default values
    show_viewport_settings = false;
    m_smoothCameraUIState = false;
    m_smoothCameraSpeed = 15.0f;
    m_fpsScale = 1.5f;
    m_framingDistanceSliderActive = false;
}

void ViewportSettingsPanel::renderPanel(Camera& camera, Grid* grid, ImVec4& clearColor, bool& gizmoPositionValid, bool& cameraFramingRequested) {
    // Create window
    if (!ImGui::Begin("Viewport Settings", &show_viewport_settings))
    {
        ImGui::End();
        return;
    }
    
    // ========== GRID OPTIONS SECTION ==========
    ImGui::Text("Grid Options");
    ImGui::Separator();
    
    if (grid == nullptr) {
        ImGui::TextWrapped("Grid is not available.");
    } else {
        // Grid Scale (Size) control
        // Increased maximum to 1,000.0 to support large-scale models
        float gridSize = grid->getSize();
        if (ImGui::SliderFloat("Grid Scale", &gridSize, 1.0f, 1000.0f, "%.1f")) {
            grid->setSize(gridSize);
            // Save to settings
            AppSettings& settings = AppSettingsManager::getInstance().getSettings();
            settings.grid.size = gridSize;
            AppSettingsManager::getInstance().markDirty();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset##Scale")) {
            grid->setSize(20.0f);  // Reset to default size
            AppSettings& settings = AppSettingsManager::getInstance().getSettings();
            settings.grid.size = 20.0f;
            AppSettingsManager::getInstance().markDirty();
        }
        
        // Grid Density (Spacing) control
        float gridSpacing = grid->getSpacing();
        if (ImGui::SliderFloat("Grid Density", &gridSpacing, 0.1f, 10.0f, "%.2f")) {
            grid->setSpacing(gridSpacing);
            // Save to settings
            AppSettings& settings = AppSettingsManager::getInstance().getSettings();
            settings.grid.spacing = gridSpacing;
            AppSettingsManager::getInstance().markDirty();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset##Density")) {
            grid->setSpacing(1.0f);  // Reset to default spacing
            AppSettings& settings = AppSettingsManager::getInstance().getSettings();
            settings.grid.spacing = 1.0f;
            AppSettingsManager::getInstance().markDirty();
        }
        
        // Help text for grid
        ImGui::Spacing();
        ImGui::TextWrapped("Grid Scale: Controls the total size of the grid (extends from -size/2 to +size/2)");
        ImGui::TextWrapped("Grid Density: Controls the spacing between grid lines (smaller = more lines)");
    }
    
    // ========== ENVIRONMENT COLOR SECTION ==========
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Environment Color");
    ImGui::Separator();
    
    AppSettings& settings = AppSettingsManager::getInstance().getSettings();
    
    // Gradient background toggle
    bool useGradient = settings.environment.useGradient;
    if (ImGui::Checkbox("Use Gradient Background", &useGradient)) {
        settings.environment.useGradient = useGradient;
        AppSettingsManager::getInstance().markDirty();
    }
    ImGui::Spacing();
    
    if (useGradient) {
        // Gradient color controls
        ImGui::Text("Gradient Colors");
        ImGui::Spacing();
        
        // Top color
        float topColor[3] = {settings.environment.viewportGradientTop.r, 
                            settings.environment.viewportGradientTop.g, 
                            settings.environment.viewportGradientTop.b};
        if (ImGui::ColorEdit3("Gradient Top", topColor)) {
            settings.environment.viewportGradientTop = glm::vec3(topColor[0], topColor[1], topColor[2]);
            // Update current preset's gradient top color when manually adjusted
            if (!settings.environment.currentPresetName.empty()) {
                auto it = settings.environment.backgroundPresets.find(settings.environment.currentPresetName);
                if (it != settings.environment.backgroundPresets.end()) {
                    it->second.gradientTop = settings.environment.viewportGradientTop;
                }
            }
            AppSettingsManager::getInstance().markDirty();
        }
        
        // Bottom color
        float bottomColor[3] = {settings.environment.viewportGradientBottom.r, 
                               settings.environment.viewportGradientBottom.g, 
                               settings.environment.viewportGradientBottom.b};
        if (ImGui::ColorEdit3("Gradient Bottom", bottomColor)) {
            settings.environment.viewportGradientBottom = glm::vec3(bottomColor[0], bottomColor[1], bottomColor[2]);
            // Update current preset's gradient bottom color when manually adjusted
            if (!settings.environment.currentPresetName.empty()) {
                auto it = settings.environment.backgroundPresets.find(settings.environment.currentPresetName);
                if (it != settings.environment.backgroundPresets.end()) {
                    it->second.gradientBottom = settings.environment.viewportGradientBottom;
                }
            }
            AppSettingsManager::getInstance().markDirty();
        }
        
        ImGui::Spacing();
        ImGui::TextWrapped("Gradient transitions from top to bottom color vertically across the viewport.");
    } else {
        // Solid color picker (RGB)
        ImGui::Text("Viewport Background Color");
        ImGui::Spacing();
        
        // Color picker (RGB)
        if (ImGui::ColorEdit3("Background Color", (float*)&clearColor)) {
            // Color is automatically updated (clearColor is a reference)
            // Save to settings
            settings.environment.backgroundColor = glm::vec4(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
            // Update current preset's solid color when manually adjusted
            if (!settings.environment.currentPresetName.empty()) {
                auto it = settings.environment.backgroundPresets.find(settings.environment.currentPresetName);
                if (it != settings.environment.backgroundPresets.end()) {
                    it->second.solid = glm::vec3(clearColor.x, clearColor.y, clearColor.z);
                }
            }
            AppSettingsManager::getInstance().markDirty();
        }
        
        // Alpha control (optional, but useful)
        ImGui::Spacing();
        if (ImGui::SliderFloat("Alpha", &clearColor.w, 0.0f, 1.0f, "%.2f")) {
            // Alpha is automatically updated
            // Save to settings
            settings.environment.backgroundColor = glm::vec4(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
            AppSettingsManager::getInstance().markDirty();
        }
    }
    
    // Preset colors (context-aware: supports both solid and gradient modes)
    ImGui::Spacing();
    ImGui::Text("Preset Colors:");
    
    // Add tooltip explaining context-aware behavior
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Presets apply as gradients when 'Use Gradient Background' is enabled, or as solid colors when disabled.");
    }
    
    // Note: 'settings' and 'useGradient' are already declared above in the Environment Color section
    // Preset buttons fetch colors from the backgroundPresets map and update currentPresetName
    
    // Helper function to apply a preset
    auto applyPreset = [&](const std::string& presetName) {
        auto it = settings.environment.backgroundPresets.find(presetName);
        if (it != settings.environment.backgroundPresets.end()) {
            const auto& preset = it->second;
            settings.environment.currentPresetName = presetName;
            
            if (useGradient) {
                // Apply gradient colors from preset
                settings.environment.viewportGradientTop = preset.gradientTop;
                settings.environment.viewportGradientBottom = preset.gradientBottom;
            } else {
                // Apply solid color from preset
                glm::vec3 solid = preset.solid;
                clearColor = ImVec4(solid.r, solid.g, solid.b, 1.0f);
                settings.environment.backgroundColor = glm::vec4(solid.r, solid.g, solid.b, 1.0f);
            }
            AppSettingsManager::getInstance().markDirty();
        }
    };
    
    // Dark Gray preset
    if (ImGui::Button("Dark Gray")) {
        applyPreset("Dark Gray");
    }
    ImGui::SameLine();
    
    // Light Gray preset
    if (ImGui::Button("Light Gray")) {
        applyPreset("Light Gray");
    }
    ImGui::SameLine();
    
    // Black preset
    if (ImGui::Button("Black")) {
        applyPreset("Black");
    }
    ImGui::SameLine();
    
    // White preset
    if (ImGui::Button("White")) {
        applyPreset("White");
    }
    
    ImGui::Spacing();
    
    // Default preset
    if (ImGui::Button("Default")) {
        applyPreset("Default");
    }
    
    // ========== CAMERA CLIPPING SECTION ==========
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Camera Clipping");
    ImGui::Separator();
    
    // Far clipping plane control
    // Maximum range set to 10,000.0 to support large-scale models while maintaining precision
    float farClipPlane = settings.environment.farClipPlane;
    if (ImGui::SliderFloat("Far Clipping Plane", &farClipPlane, 10.0f, 10000.0f, "%.1f")) {
        settings.environment.farClipPlane = farClipPlane;
        AppSettingsManager::getInstance().markDirty();
        // Projection matrix is updated every frame in main.cpp, so change takes effect immediately
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset##FarClip")) {
        farClipPlane = 5000.0f;  // Reset to default (supports large-scale models)
        settings.environment.farClipPlane = 5000.0f;
        AppSettingsManager::getInstance().markDirty();
    }
    
    // Help text for far clipping plane
    ImGui::Spacing();
    ImGui::TextWrapped("Far Clipping Plane: Controls the maximum distance at which objects are visible. "
                       "Objects beyond this distance will be clipped and not rendered. "
                       "Increase this value to see objects further away from the camera.");
    
    // ========== CAMERA FRAMING SECTION ==========
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Camera Framing");
    ImGui::Separator();
    
    // Framing Distance control
    float framingDistance = settings.environment.framingDistanceMultiplier;
    
    // Create slider and check if it's currently being dragged
    if (ImGui::SliderFloat("Framing Distance", &framingDistance, 1.0f, 10.0f, "%.2f")) {
        settings.environment.framingDistanceMultiplier = framingDistance;
        AppSettingsManager::getInstance().markDirty();
    }
    
    // Detect if slider is currently being actively dragged
    bool sliderCurrentlyActive = ImGui::IsItemActive();
    
    // Detect slider release (was active last frame, not active now)
    bool sliderJustReleased = m_framingDistanceSliderActive && !sliderCurrentlyActive;
    m_framingDistanceSliderActive = sliderCurrentlyActive;
    
    // On slider release, trigger full re-framing if auto-focus is enabled
    // Use persistent settings value
    if (sliderJustReleased && settings.environment.autoFocusEnabled && gizmoPositionValid) {
        cameraFramingRequested = true;
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Reset##FramingDistance")) {
        framingDistance = 1.5f;  // Reset to default
        settings.environment.framingDistanceMultiplier = 1.5f;
        AppSettingsManager::getInstance().markDirty();
        // Reset button triggers full re-framing if auto-focus is enabled
        // Use persistent settings value
        if (settings.environment.autoFocusEnabled && gizmoPositionValid) {
            cameraFramingRequested = true;
        }
    }
    
    // Help text for framing distance
    ImGui::Spacing();
    ImGui::TextWrapped("Framing Distance: Controls how tightly the camera frames characters when using 'F' key or Auto Focus. "
                       "Lower values (1.0-2.0) = tighter framing, Higher values (5.0-10.0) = looser framing. "
                       "Default: 1.5 (tight, centered frame).");
    
    // Separator between Framing Distance and Smooth Camera sections
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Smooth Camera checkbox
    if (ImGui::Checkbox("Smooth Camera", &m_smoothCameraUIState)) {
        // State is stored in m_smoothCameraUIState, will be synced to camera in main loop
        // Save to settings
        AppSettings& settings = AppSettingsManager::getInstance().getSettings();
        settings.camera.smoothCameraEnabled = m_smoothCameraUIState;
        AppSettingsManager::getInstance().markDirty();
    }
    
    // Smooth Camera Speed slider (only visible when Smooth Camera is enabled)
    if (m_smoothCameraUIState) {
        ImGui::SameLine();
        
        // Use default width to match other sliders in the panel
        if (ImGui::SliderFloat("Speed##Cam", &m_smoothCameraSpeed, 0.1f, 40.0f, "%.1f")) {
            // Update camera immediately for responsive feedback
            camera.setSmoothTransitionSpeed(m_smoothCameraSpeed);
            // Save to settings
            AppSettings& settings = AppSettingsManager::getInstance().getSettings();
            settings.camera.smoothTransitionSpeed = m_smoothCameraSpeed;
            AppSettingsManager::getInstance().markDirty();
        }
        
        ImGui::SameLine();
        // Compact Reset button
        if (ImGui::Button("Reset##SpeedCam")) {
            m_smoothCameraSpeed = 15.0f;
            camera.setSmoothTransitionSpeed(15.0f);
            // Save to settings
            AppSettings& settings = AppSettingsManager::getInstance().getSettings();
            settings.camera.smoothTransitionSpeed = m_smoothCameraSpeed;
            AppSettingsManager::getInstance().markDirty();
        }
    }
    
    // Help text for Smooth Camera controls
    ImGui::Spacing();
    ImGui::TextWrapped("Smooth Camera: When enabled, the camera smoothly transitions to the target position when using 'F' key or Auto Focus, "
                       "instead of instantly snapping. Speed controls the transition speed - lower values (0.1-5.0) = slower, smoother transitions, "
                       "higher values (20.0-40.0) = faster, snappier transitions. Default: 15.0 (balanced speed).");
    
    // ========== FPS SETTINGS SECTION ==========
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("FPS Settings");
    ImGui::Separator();
    
    // V-Sync control
    if (ImGui::Checkbox("V-Sync (Limit FPS to 60)", &m_vSyncEnabled)) {
        // Apply immediately
        glfwSwapInterval(m_vSyncEnabled ? 1 : 0);
        // Save to settings
        AppSettings& settings = AppSettingsManager::getInstance().getSettings();
        settings.environment.vSyncEnabled = m_vSyncEnabled;
        AppSettingsManager::getInstance().markDirty();
    }
    ImGui::Spacing();
    ImGui::TextWrapped("V-Sync: Locks the framerate to your monitor's refresh rate (typically 60 FPS) to prevent screen tearing and reduce CPU/GPU usage.");
    ImGui::Spacing();
    
    // FPS Counter Scale control
    if (ImGui::SliderFloat("FPS Counter Scale", &m_fpsScale, 0.5f, 5.0f, "%.1f")) {
        // Scale is automatically updated (m_fpsScale is a member variable)
        // Changes take effect immediately in renderSimpleFPS()
        // Save to settings
        AppSettings& settings = AppSettingsManager::getInstance().getSettings();
        settings.environment.fpsScale = m_fpsScale;
        AppSettingsManager::getInstance().markDirty();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset##FPSScale")) {
        m_fpsScale = 1.5f;  // Reset to default
        // Save to settings
        AppSettings& settings = AppSettingsManager::getInstance().getSettings();
        settings.environment.fpsScale = m_fpsScale;
        AppSettingsManager::getInstance().markDirty();
    }
    
    // Help text for FPS counter scale
    ImGui::Spacing();
    ImGui::TextWrapped("FPS Counter Scale: Controls the size of the FPS counter text. "
                       "Adjust this value to make the FPS counter more or less visible.");
    
    // ========== SKELETON SETTINGS SECTION ==========
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Skeleton Settings");
    ImGui::Separator();
    
    // Skeleton Line Width control
    float skeletonLineWidth = settings.environment.skeletonLineWidth;
    if (ImGui::SliderFloat("Skeleton Line Width", &skeletonLineWidth, 1.0f, 10.0f, "%.1f")) {
        settings.environment.skeletonLineWidth = skeletonLineWidth;
        AppSettingsManager::getInstance().markDirty();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset##SkeletonLineWidth")) {
        skeletonLineWidth = 2.0f;  // Reset to default
        settings.environment.skeletonLineWidth = 2.0f;
        AppSettingsManager::getInstance().markDirty();
    }
    
    // Help text for skeleton line width
    ImGui::Spacing();
    ImGui::TextWrapped("Skeleton Line Width: Controls the thickness of skeleton bone lines when skeletons are visible. "
                       "Adjust this value to make skeleton bones more or less visible.");
    
    // Version display
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Version: %s", APP_VERSION);
    
    ImGui::End();
}
