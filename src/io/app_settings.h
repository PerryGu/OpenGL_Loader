#pragma once
#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <glm/glm.hpp>

// Application settings structure
struct AppSettings {
    // Camera settings
    struct CameraSettings {
        glm::vec3 position = glm::vec3(0.0f, 9.0f, 30.0f);
        glm::vec3 focusPoint = glm::vec3(0.0f, 0.0f, 0.0f);
        float yaw = -90.0f;
        float pitch = 0.0f;
        float zoom = 45.0f;
        float orbitDistance = 30.0f;
        float speed = 2.5f;  // Camera movement speed
        float sensitivity = 1.0f;  // Camera rotation sensitivity
        bool smoothCameraEnabled = false;  // Whether smooth camera animation is enabled
        float smoothTransitionSpeed = 15.0f;  // Speed multiplier for smooth camera interpolation (higher = faster transition)
    } camera;
    
    // Window settings
    struct WindowSettings {
        int posX = -1;  // -1 means use default (centered)
        int posY = -1;
        int width = 1920;
        int height = 1080;
    } window;
    
    // Grid settings
    struct GridSettings {
        float size = 20.0f;           // Total size of the grid
        float spacing = 1.0f;         // Spacing between grid lines
        bool enabled = true;          // Whether grid is visible
        glm::vec3 majorLineColor = glm::vec3(0.5f, 0.5f, 0.5f);   // Gray
        glm::vec3 minorLineColor = glm::vec3(0.3f, 0.3f, 0.3f);   // Dark gray
        glm::vec3 centerLineColor = glm::vec3(0.2f, 0.2f, 0.2f);   // Darker gray (muted, matches grid)
    } grid;
    
    // Environment/Viewport settings
    struct EnvironmentSettings {
        /**
         * @brief Background preset structure storing colors for both solid and gradient modes.
         * 
         * Each preset contains:
         * - solid: RGB color for solid background mode
         * - gradientTop: Top color for gradient background mode
         * - gradientBottom: Bottom color for gradient background mode
         */
        struct BackgroundPreset {
            glm::vec3 solid = glm::vec3(0.45f, 0.55f, 0.60f);  // Solid color (RGB)
            glm::vec3 gradientTop = glm::vec3(0.2f, 0.2f, 0.2f);  // Gradient top color (RGB)
            glm::vec3 gradientBottom = glm::vec3(0.1f, 0.1f, 0.1f);  // Gradient bottom color (RGB)
        };
        
        // Map of preset names to their color configurations (5 presets: "Dark Gray", "Light Gray", "Black", "White", "Default")
        std::map<std::string, BackgroundPreset> backgroundPresets;
        
        // Name of the currently active preset (e.g., "Dark Gray", "Default")
        std::string currentPresetName = "Default";
        
        // Current active colors (synced from active preset based on useGradient mode)
        glm::vec4 backgroundColor = glm::vec4(0.45f, 0.55f, 0.60f, 1.00f);  // Viewport background color (solid mode)
        glm::vec3 viewportGradientTop = glm::vec3(0.2f, 0.2f, 0.2f);  // Top color for gradient background
        glm::vec3 viewportGradientBottom = glm::vec3(0.1f, 0.1f, 0.1f);  // Bottom color for gradient background
        bool useGradient = true;  // Whether to use gradient background instead of solid color
        std::string lastImportDirectory = "";  // Last directory used for importing files (persisted across sessions)
        bool boundingBoxesEnabled = true;  // Whether bounding boxes are visible for all models
        bool skeletonsEnabled = false;  // Whether skeletons are visible for all models
        float skeletonLineWidth = 2.0f;  // Line width (thickness) for rendered skeleton bones (1.0 to 10.0)
        float farClipPlane = 5000.0f;  // Maximum clipping distance (far plane) for the camera projection (supports large-scale models)
        float framingDistanceMultiplier = 1.5f;  // Multiplier for camera framing distance (1.0 = tight, 10.0 = loose)
        bool autoFocusEnabled = false;  // Whether auto-focus is enabled (automatically frame camera when gizmo is released)
        bool showFPS = false;  // Whether FPS counter is visible
        float fpsScale = 1.5f;  // Scale factor for FPS counter text size (0.5 to 5.0)
        bool vSyncEnabled = true;
    } environment;
    
    // Outliner panel filter checkboxes (persisted to app_settings.json)
    struct OutlinerSettings {
        bool showGeometry = true;
        bool showAnimations = true;
        bool showRigGroups = true;
    } outliner;
    
    // Logger/Debug panel filter checkboxes (persisted to app_settings.json)
    struct LoggerSettings {
        bool showInfo = true;
        bool showWarning = true;
        bool showError = true;
    } logger;
    
    // Recent files (up to 6)
    std::vector<std::string> recentFiles;
    static const int MAX_RECENT_FILES = 6;
};

// Settings manager - singleton
class AppSettingsManager {
public:
    static AppSettingsManager& getInstance() {
        static AppSettingsManager instance;
        return instance;
    }
    
    // Load settings from JSON file
    bool loadSettings(const std::string& filename = "app_settings.json");
    
    // Save settings to JSON file
    bool saveSettings(const std::string& filename = "app_settings.json");
    
    // Get settings
    AppSettings& getSettings() { return settings; }
    const AppSettings& getSettings() const { return settings; }
    
    // Add file to recent files list
    void addRecentFile(const std::string& filepath);
    
    // Get recent files
    const std::vector<std::string>& getRecentFiles() const { return settings.recentFiles; }
    
    // Mark settings as dirty (need to save)
    void markDirty() { isDirty = true; }
    
    // Auto-save if dirty (call periodically)
    void autoSave();
    
private:
    AppSettingsManager() = default;
    ~AppSettingsManager() { saveSettings(); } // Auto-save on destruction
    
    AppSettings settings;
    bool isDirty = false;
    std::string settingsFile = "app_settings.json";
    std::chrono::steady_clock::time_point lastSaveTime;  // Last time settings were saved (for auto-save throttling)
    static constexpr double AUTO_SAVE_INTERVAL_SECONDS = 60.0;  // Minimum interval between auto-saves (reduces SSD wear)
};

#endif
