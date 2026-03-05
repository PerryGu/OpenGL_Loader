#include "app_settings.h"
#include "../version.h"  // For VER_MAJOR, VER_MINOR macros in migration logic
#include "../utils/logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @brief Initializes default background presets with professional color pairs.
 * 
 * This function sets up the 5 default presets (Dark Gray, Light Gray, Black, White, Default)
 * with their respective solid and gradient color configurations. Called during settings
 * initialization to ensure presets are always available.
 * 
 * @param settings Reference to AppSettings to initialize presets in
 */
static void initializeDefaultPresets(AppSettings& settings) {
    using Preset = AppSettings::EnvironmentSettings::BackgroundPreset;
    
    // Dark Gray preset
    Preset darkGray;
    darkGray.solid = glm::vec3(0.1f, 0.1f, 0.1f);
    darkGray.gradientTop = glm::vec3(0.2f, 0.22f, 0.25f);
    darkGray.gradientBottom = glm::vec3(0.05f, 0.05f, 0.07f);
    settings.environment.backgroundPresets["Dark Gray"] = darkGray;
    
    // Light Gray preset
    Preset lightGray;
    lightGray.solid = glm::vec3(0.45f, 0.55f, 0.60f);
    lightGray.gradientTop = glm::vec3(0.45f, 0.55f, 0.60f);
    lightGray.gradientBottom = glm::vec3(0.25f, 0.30f, 0.35f);
    settings.environment.backgroundPresets["Light Gray"] = lightGray;
    
    // Black preset
    Preset black;
    black.solid = glm::vec3(0.0f, 0.0f, 0.0f);
    black.gradientTop = glm::vec3(0.1f, 0.1f, 0.1f);
    black.gradientBottom = glm::vec3(0.0f, 0.0f, 0.0f);
    settings.environment.backgroundPresets["Black"] = black;
    
    // White preset
    Preset white;
    white.solid = glm::vec3(1.0f, 1.0f, 1.0f);
    white.gradientTop = glm::vec3(1.0f, 1.0f, 1.0f);
    white.gradientBottom = glm::vec3(0.75f, 0.75f, 0.80f);
    settings.environment.backgroundPresets["White"] = white;
    
    // Default preset (professional blue-gray)
    Preset defaultPreset;
    defaultPreset.solid = glm::vec3(0.45f, 0.55f, 0.60f);
    defaultPreset.gradientTop = glm::vec3(0.2f, 0.3f, 0.4f);
    defaultPreset.gradientBottom = glm::vec3(0.1f, 0.15f, 0.2f);
    settings.environment.backgroundPresets["Default"] = defaultPreset;
}

/**
 * @brief Loads settings from JSON file with robust fallback to defaults.
 * 
 * This function implements a resilient loading mechanism:
 * - Missing JSON keys automatically fall back to hardcoded defaults in app_settings.h
 * - Uses .contains() checks before accessing JSON values to prevent exceptions
 * - If a key is missing, the struct member's default initialization value is used
 * - Migration logic handles invalid values (e.g., farClipPlane <= 0)
 * - Recent files are automatically cleaned (non-existent files removed)
 * 
 * This ensures that new settings added in v2.0.0+ (VER_MAJOR.VER_MINOR.VER_PATCH) will use sensible defaults
 * even when loading from older settings files that don't contain those keys.
 * 
 * @param filename Path to the JSON settings file
 * @return true if settings loaded successfully, false otherwise
 */
bool AppSettingsManager::loadSettings(const std::string& filename) {
    settingsFile = filename;
    std::ifstream file(filename);
    
    // Initialize default presets first (ensures presets are always available)
    initializeDefaultPresets(settings);
    
    // Set default current preset if not already set
    if (settings.environment.currentPresetName.empty()) {
        settings.environment.currentPresetName = "Default";
    }
    
    if (!file.is_open()) {
        LOG_INFO("[Settings] No settings file found, using defaults");
        // Sync current colors from default preset
        auto it = settings.environment.backgroundPresets.find(settings.environment.currentPresetName);
        if (it != settings.environment.backgroundPresets.end()) {
            if (settings.environment.useGradient) {
                settings.environment.viewportGradientTop = it->second.gradientTop;
                settings.environment.viewportGradientBottom = it->second.gradientBottom;
            } else {
                glm::vec3 solid = it->second.solid;
                settings.environment.backgroundColor = glm::vec4(solid.r, solid.g, solid.b, 1.0f);
            }
        }
        return false;
    }
    
    try {
        json j;
        file >> j;
        file.close();
        
        // Load camera settings
        if (j.contains("camera")) {
            auto& cam = j["camera"];
            if (cam.contains("position")) {
                settings.camera.position.x = cam["position"][0].get<float>();
                settings.camera.position.y = cam["position"][1].get<float>();
                settings.camera.position.z = cam["position"][2].get<float>();
            }
            if (cam.contains("focusPoint")) {
                settings.camera.focusPoint.x = cam["focusPoint"][0].get<float>();
                settings.camera.focusPoint.y = cam["focusPoint"][1].get<float>();
                settings.camera.focusPoint.z = cam["focusPoint"][2].get<float>();
            }
            if (cam.contains("yaw")) settings.camera.yaw = cam["yaw"].get<float>();
            if (cam.contains("pitch")) settings.camera.pitch = cam["pitch"].get<float>();
            if (cam.contains("zoom")) settings.camera.zoom = cam["zoom"].get<float>();
            if (cam.contains("orbitDistance")) settings.camera.orbitDistance = cam["orbitDistance"].get<float>();
            if (cam.contains("speed")) settings.camera.speed = cam["speed"].get<float>();
            if (cam.contains("sensitivity")) settings.camera.sensitivity = cam["sensitivity"].get<float>();
            if (cam.contains("smoothCameraEnabled")) settings.camera.smoothCameraEnabled = cam["smoothCameraEnabled"].get<bool>();
            if (cam.contains("smoothTransitionSpeed")) settings.camera.smoothTransitionSpeed = cam["smoothTransitionSpeed"].get<float>();
        }
        
        // Load window settings
        if (j.contains("window")) {
            auto& win = j["window"];
            if (win.contains("posX")) settings.window.posX = win["posX"].get<int>();
            if (win.contains("posY")) settings.window.posY = win["posY"].get<int>();
            if (win.contains("width")) settings.window.width = win["width"].get<int>();
            if (win.contains("height")) settings.window.height = win["height"].get<int>();
        }
        
        // Load grid settings
        if (j.contains("grid")) {
            auto& grid = j["grid"];
            if (grid.contains("size")) settings.grid.size = grid["size"].get<float>();
            if (grid.contains("spacing")) settings.grid.spacing = grid["spacing"].get<float>();
            if (grid.contains("enabled")) settings.grid.enabled = grid["enabled"].get<bool>();
            if (grid.contains("majorLineColor")) {
                settings.grid.majorLineColor.r = grid["majorLineColor"][0].get<float>();
                settings.grid.majorLineColor.g = grid["majorLineColor"][1].get<float>();
                settings.grid.majorLineColor.b = grid["majorLineColor"][2].get<float>();
            }
            if (grid.contains("minorLineColor")) {
                settings.grid.minorLineColor.r = grid["minorLineColor"][0].get<float>();
                settings.grid.minorLineColor.g = grid["minorLineColor"][1].get<float>();
                settings.grid.minorLineColor.b = grid["minorLineColor"][2].get<float>();
            }
            if (grid.contains("centerLineColor")) {
                settings.grid.centerLineColor.r = grid["centerLineColor"][0].get<float>();
                settings.grid.centerLineColor.g = grid["centerLineColor"][1].get<float>();
                settings.grid.centerLineColor.b = grid["centerLineColor"][2].get<float>();
                
                // Migration: Update old red center line color to new darker gray
                // Check if it's the old red color (0.8, 0.2, 0.2) and update to new default (0.2, 0.2, 0.2)
                if (settings.grid.centerLineColor.r > 0.7f && 
                    settings.grid.centerLineColor.g < 0.3f && 
                    settings.grid.centerLineColor.b < 0.3f) {
                    settings.grid.centerLineColor = glm::vec3(0.2f, 0.2f, 0.2f);
                }
            }
        }
        
        // Load environment settings
        if (j.contains("environment")) {
            auto& env = j["environment"];
            if (env.contains("backgroundColor")) {
                settings.environment.backgroundColor.r = env["backgroundColor"][0].get<float>();
                settings.environment.backgroundColor.g = env["backgroundColor"][1].get<float>();
                settings.environment.backgroundColor.b = env["backgroundColor"][2].get<float>();
                settings.environment.backgroundColor.a = env["backgroundColor"][3].get<float>();
            }
            if (env.contains("viewportGradientTop")) {
                settings.environment.viewportGradientTop.r = env["viewportGradientTop"][0].get<float>();
                settings.environment.viewportGradientTop.g = env["viewportGradientTop"][1].get<float>();
                settings.environment.viewportGradientTop.b = env["viewportGradientTop"][2].get<float>();
            }
            if (env.contains("viewportGradientBottom")) {
                settings.environment.viewportGradientBottom.r = env["viewportGradientBottom"][0].get<float>();
                settings.environment.viewportGradientBottom.g = env["viewportGradientBottom"][1].get<float>();
                settings.environment.viewportGradientBottom.b = env["viewportGradientBottom"][2].get<float>();
            }
            if (env.contains("useGradient")) {
                settings.environment.useGradient = env["useGradient"].get<bool>();
            }
            if (env.contains("boundingBoxesEnabled")) {
                settings.environment.boundingBoxesEnabled = env["boundingBoxesEnabled"].get<bool>();
            }
            if (env.contains("skeletonsEnabled")) {
                settings.environment.skeletonsEnabled = env["skeletonsEnabled"].get<bool>();
            }
            if (env.contains("skeletonLineWidth")) {
                settings.environment.skeletonLineWidth = env["skeletonLineWidth"].get<float>();
            }
            if (env.contains("farClipPlane")) {
                float loadedFarClipPlane = env["farClipPlane"].get<float>();
                // Migration: Reset invalid farClipPlane values (0 or negative) to default
                // This handles corrupted settings or legacy files that may have invalid values
                if (loadedFarClipPlane <= 0.0f) {
                    settings.environment.farClipPlane = 5000.0f;  // Default value from app_settings.h
                    LOG_INFO("[Settings] Migrated invalid farClipPlane (" + std::to_string(loadedFarClipPlane) + 
                             ") to default (5000.0)");
                } else {
                    settings.environment.farClipPlane = loadedFarClipPlane;
                }
            }
            // Note: If farClipPlane key is missing, it will use the default value from app_settings.h (5000.0f)
            if (env.contains("framingDistanceMultiplier")) {
                settings.environment.framingDistanceMultiplier = env["framingDistanceMultiplier"].get<float>();
            }
            if (env.contains("autoFocusEnabled")) {
                settings.environment.autoFocusEnabled = env["autoFocusEnabled"].get<bool>();
            }
            if (env.contains("showFPS")) {
                settings.environment.showFPS = env["showFPS"].get<bool>();
            }
            if (env.contains("fpsScale")) {
                settings.environment.fpsScale = env["fpsScale"].get<float>();
            }
            if (env.contains("vSyncEnabled")) {
                settings.environment.vSyncEnabled = env["vSyncEnabled"].get<bool>();
            }
            if (env.contains("lastImportDirectory") && env["lastImportDirectory"].is_string()) {
                settings.environment.lastImportDirectory = env["lastImportDirectory"].get<std::string>();
            }
            
            // Load background presets map
            if (env.contains("backgroundPresets") && env["backgroundPresets"].is_object()) {
                settings.environment.backgroundPresets.clear();
                for (auto& [key, value] : env["backgroundPresets"].items()) {
                    AppSettings::EnvironmentSettings::BackgroundPreset preset;
                    if (value.contains("solid") && value["solid"].is_array() && value["solid"].size() >= 3) {
                        preset.solid = glm::vec3(
                            value["solid"][0].get<float>(),
                            value["solid"][1].get<float>(),
                            value["solid"][2].get<float>()
                        );
                    }
                    if (value.contains("gradientTop") && value["gradientTop"].is_array() && value["gradientTop"].size() >= 3) {
                        preset.gradientTop = glm::vec3(
                            value["gradientTop"][0].get<float>(),
                            value["gradientTop"][1].get<float>(),
                            value["gradientTop"][2].get<float>()
                        );
                    }
                    if (value.contains("gradientBottom") && value["gradientBottom"].is_array() && value["gradientBottom"].size() >= 3) {
                        preset.gradientBottom = glm::vec3(
                            value["gradientBottom"][0].get<float>(),
                            value["gradientBottom"][1].get<float>(),
                            value["gradientBottom"][2].get<float>()
                        );
                    }
                    settings.environment.backgroundPresets[key] = preset;
                }
            }
            
            // Load current preset name
            if (env.contains("currentPresetName") && env["currentPresetName"].is_string()) {
                settings.environment.currentPresetName = env["currentPresetName"].get<std::string>();
            }
            
            // Note: smoothCamera, framingPadding, and framingSpeed settings removed
            // These were legacy settings that are no longer part of EnvironmentSettings
        }
        
        // Load recent files with existence check
        // This automatically cleans up the list when files are deleted from disk
        settings.recentFiles.clear();
        if (j.contains("recentFiles") && j["recentFiles"].is_array()) {
            for (const auto& file : j["recentFiles"]) {
                if (file.is_string() && !file.get<std::string>().empty()) {
                    std::string filePath = file.get<std::string>();
                    // Check if file still exists on disk before adding to recent files
                    // This cleans up the list automatically when files are deleted
                    if (std::filesystem::exists(filePath)) {
                        settings.recentFiles.push_back(filePath);
                    } else {
                        LOG_DEBUG("[Settings] Removed non-existent file from recent files: " + filePath);
                    }
                }
            }
        }
        
        // Sync current colors to active preset if preset exists
        if (!settings.environment.currentPresetName.empty()) {
            auto it = settings.environment.backgroundPresets.find(settings.environment.currentPresetName);
            if (it != settings.environment.backgroundPresets.end()) {
                if (settings.environment.useGradient) {
                    // Sync gradient colors from preset
                    settings.environment.viewportGradientTop = it->second.gradientTop;
                    settings.environment.viewportGradientBottom = it->second.gradientBottom;
                } else {
                    // Sync solid color from preset
                    glm::vec3 solid = it->second.solid;
                    settings.environment.backgroundColor = glm::vec4(solid.r, solid.g, solid.b, 1.0f);
                }
            }
        }
        
        LOG_INFO("[Settings] Loaded settings from " + filename);
        LOG_INFO("[Settings] Found " + std::to_string(settings.recentFiles.size()) + " recent files");
        return true;
    }
    catch (const json::exception& e) {
        LOG_ERROR("[Settings] JSON parsing error: " + std::string(e.what()));
        return false;
    }
}

bool AppSettingsManager::saveSettings(const std::string& filename) {
    if (!filename.empty()) {
        settingsFile = filename;
    }
    
    try {
        json j;
        
        // Save camera settings
        j["camera"]["position"] = {settings.camera.position.x, settings.camera.position.y, settings.camera.position.z};
        j["camera"]["focusPoint"] = {settings.camera.focusPoint.x, settings.camera.focusPoint.y, settings.camera.focusPoint.z};
        j["camera"]["yaw"] = settings.camera.yaw;
        j["camera"]["pitch"] = settings.camera.pitch;
        j["camera"]["zoom"] = settings.camera.zoom;
        j["camera"]["orbitDistance"] = settings.camera.orbitDistance;
        j["camera"]["speed"] = settings.camera.speed;
        j["camera"]["sensitivity"] = settings.camera.sensitivity;
        j["camera"]["smoothCameraEnabled"] = settings.camera.smoothCameraEnabled;
        j["camera"]["smoothTransitionSpeed"] = settings.camera.smoothTransitionSpeed;
        
        // Save window settings
        j["window"]["posX"] = settings.window.posX;
        j["window"]["posY"] = settings.window.posY;
        j["window"]["width"] = settings.window.width;
        j["window"]["height"] = settings.window.height;
        
        // Save grid settings
        j["grid"]["size"] = settings.grid.size;
        j["grid"]["spacing"] = settings.grid.spacing;
        j["grid"]["enabled"] = settings.grid.enabled;
        j["grid"]["majorLineColor"] = {settings.grid.majorLineColor.r, settings.grid.majorLineColor.g, settings.grid.majorLineColor.b};
        j["grid"]["minorLineColor"] = {settings.grid.minorLineColor.r, settings.grid.minorLineColor.g, settings.grid.minorLineColor.b};
        j["grid"]["centerLineColor"] = {settings.grid.centerLineColor.r, settings.grid.centerLineColor.g, settings.grid.centerLineColor.b};
        
        // Save environment settings
        j["environment"]["backgroundColor"] = {
            settings.environment.backgroundColor.r,
            settings.environment.backgroundColor.g,
            settings.environment.backgroundColor.b,
            settings.environment.backgroundColor.a
        };
        j["environment"]["viewportGradientTop"] = {
            settings.environment.viewportGradientTop.r,
            settings.environment.viewportGradientTop.g,
            settings.environment.viewportGradientTop.b
        };
        j["environment"]["viewportGradientBottom"] = {
            settings.environment.viewportGradientBottom.r,
            settings.environment.viewportGradientBottom.g,
            settings.environment.viewportGradientBottom.b
        };
        j["environment"]["useGradient"] = settings.environment.useGradient;
        j["environment"]["boundingBoxesEnabled"] = settings.environment.boundingBoxesEnabled;
        j["environment"]["skeletonsEnabled"] = settings.environment.skeletonsEnabled;
        j["environment"]["skeletonLineWidth"] = settings.environment.skeletonLineWidth;
        j["environment"]["farClipPlane"] = settings.environment.farClipPlane;
        j["environment"]["framingDistanceMultiplier"] = settings.environment.framingDistanceMultiplier;
        j["environment"]["autoFocusEnabled"] = settings.environment.autoFocusEnabled;
        j["environment"]["showFPS"] = settings.environment.showFPS;
        j["environment"]["fpsScale"] = settings.environment.fpsScale;
        j["environment"]["vSyncEnabled"] = settings.environment.vSyncEnabled;
        j["environment"]["lastImportDirectory"] = settings.environment.lastImportDirectory;
        
        // Save background presets map
        j["environment"]["backgroundPresets"] = json::object();
        for (const auto& [name, preset] : settings.environment.backgroundPresets) {
            j["environment"]["backgroundPresets"][name]["solid"] = {
                preset.solid.r, preset.solid.g, preset.solid.b
            };
            j["environment"]["backgroundPresets"][name]["gradientTop"] = {
                preset.gradientTop.r, preset.gradientTop.g, preset.gradientTop.b
            };
            j["environment"]["backgroundPresets"][name]["gradientBottom"] = {
                preset.gradientBottom.r, preset.gradientBottom.g, preset.gradientBottom.b
            };
        }
        
        // Save current preset name
        j["environment"]["currentPresetName"] = settings.environment.currentPresetName;
        
        // Note: smoothCamera, framingPadding, and framingSpeed settings removed
        // These were legacy settings that are no longer part of EnvironmentSettings
        
        // Save recent files with forward slashes for better readability
        j["recentFiles"] = json::array();
        for (size_t i = 0; i < settings.recentFiles.size() && i < AppSettings::MAX_RECENT_FILES; i++) {
            std::string pathWithForwardSlashes = settings.recentFiles[i];
            // Convert backslashes to forward slashes for JSON output (better readability and Explorer compatibility)
            std::replace(pathWithForwardSlashes.begin(), pathWithForwardSlashes.end(), '\\', '/');
            j["recentFiles"].push_back(pathWithForwardSlashes);
        }
        
        // Write JSON with pretty formatting (indent 4 spaces)
        std::ofstream file(settingsFile);
        if (!file.is_open()) {
            LOG_ERROR("[Settings] Failed to open settings file for writing: " + settingsFile);
            return false;
        }
        
        file << j.dump(4); // Pretty print with 4-space indent
        file.close();
        
        isDirty = false;
        lastSaveTime = std::chrono::steady_clock::now();  // Update last save time for auto-save throttling
        LOG_INFO("[Settings] Saved settings to " + settingsFile);
        return true;
    }
    catch (const json::exception& e) {
        LOG_ERROR("[Settings] JSON save error: " + std::string(e.what()));
        return false;
    }
}

/**
 * @brief Adds a file to the recent files list with professional path sanitization and duplicate prevention.
 * 
 * This function follows a strict 5-step process:
 * 1. Sanitization: Convert path to absolute/canonical form and replace backslashes with forward slashes
 * 2. Duplicate Check: Verify if sanitized path already exists in recentFiles vector
 * 3. Reordering: If duplicate exists, remove the old entry from its current position
 * 4. Insertion: Insert the sanitized path at index 0 (beginning of vector)
 * 5. Cap Capacity: If vector size exceeds MAX_RECENT_FILES (6), remove the last element
 * 
 * Path sanitization ensures cross-platform compatibility and prevents path corruption
 * from build artifacts (e.g., "out\build" directories) or relative path inconsistencies.
 * Forward slashes make paths easy to copy-paste (e.g., "F:/Work_stuff/Assets/models/Walk.fbx").
 * 
 * Verification: Opening the same file multiple times results in only ONE entry, always at the top.
 * 
 * @param filepath The file path to add (can be relative or absolute)
 */
void AppSettingsManager::addRecentFile(const std::string& filepath) {
    if (filepath.empty()) {
        LOG_WARNING("[Settings] Attempted to add empty filepath to recent files");
        return;
    }
    
    try {
        std::filesystem::path filePath(filepath);
        
        // Verify file exists before adding to recent files
        if (!std::filesystem::exists(filePath)) {
            LOG_ERROR("[Settings] Cannot add non-existent file to recent files: " + filepath);
            return;
        }
        
        // ========== STEP 1: SANITIZATION ==========
        // Convert to absolute path, then canonical form (resolves ".." and redundant separators)
        std::filesystem::path absolutePath = std::filesystem::absolute(filePath);
        std::filesystem::path canonicalPath = std::filesystem::canonical(absolutePath);
        std::string sanitizedPath = canonicalPath.string();
        
        // Replace all backslashes with forward slashes for better readability and Explorer compatibility
        std::replace(sanitizedPath.begin(), sanitizedPath.end(), '\\', '/');
        
        // ========== STEP 2: FILE EXISTENCE CHECK ==========
        // Check if the file still exists on disk before adding/moving it
        // If file was deleted, remove it from recent files entirely
        if (!std::filesystem::exists(canonicalPath)) {
            LOG_WARNING("[Settings] File no longer exists, removing from recent files: " + sanitizedPath);
            // Remove from recent files if it exists there
            auto it = std::find(settings.recentFiles.begin(), settings.recentFiles.end(), sanitizedPath);
            if (it != settings.recentFiles.end()) {
                settings.recentFiles.erase(it);
                markDirty();
            }
            return;  // Don't add deleted file to recent files
        }
        
        // ========== STEP 3: DUPLICATE CHECK ==========
        // Check if this sanitized path already exists in the recentFiles vector
        auto duplicateIt = std::find(settings.recentFiles.begin(), settings.recentFiles.end(), sanitizedPath);
        bool duplicateExists = (duplicateIt != settings.recentFiles.end());
        
        // ========== STEP 4: REORDERING ==========
        // If duplicate exists, remove the old entry from its current position
        if (duplicateExists) {
            settings.recentFiles.erase(duplicateIt);
        }
        
        // ========== STEP 5: INSERTION ==========
        // Insert the sanitized path at the very beginning (index 0) of the vector
        settings.recentFiles.insert(settings.recentFiles.begin(), sanitizedPath);
        
        // ========== STEP 6: CAP CAPACITY ==========
        // If vector size exceeds MAX_RECENT_FILES (6), remove the last element
        while (settings.recentFiles.size() > AppSettings::MAX_RECENT_FILES) {
            settings.recentFiles.pop_back();
        }
        
        // Mark settings as dirty to trigger auto-save
        markDirty();
        
        // Extract filename for logging (better readability)
        std::filesystem::path pathObj(sanitizedPath);
        std::string filename = pathObj.filename().string();
        LOG_INFO("Recent files updated: " + filename);
    } catch (const std::filesystem::filesystem_error& e) {
        LOG_ERROR("[Settings] Path sanitization failed for recent file: " + std::string(e.what()));
        // Don't add file if path sanitization fails (prevents corrupted paths in settings)
    }
}

/**
 * @brief Auto-saves settings if dirty, with throttling to reduce SSD wear.
 * 
 * This function implements a timer-based throttling mechanism to prevent excessive
 * disk writes during heavy UI interaction. Settings are only saved if:
 * 1. The settings are marked as dirty (isDirty == true)
 * 2. At least AUTO_SAVE_INTERVAL_SECONDS (60 seconds) have passed since the last save
 * 
 * This reduces SSD wear and improves performance during rapid UI changes while
 * still ensuring settings are persisted regularly.
 */
void AppSettingsManager::autoSave() {
    if (!isDirty) {
        return;  // Nothing to save
    }
    
    // Get current time
    auto now = std::chrono::steady_clock::now();
    
    // Check if enough time has passed since last save (throttling to reduce SSD wear)
    // On first call, lastSaveTime is default-constructed (epoch), so first save happens immediately
    auto timeSinceLastSave = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSaveTime).count() / 1000.0;
    
    if (timeSinceLastSave >= AUTO_SAVE_INTERVAL_SECONDS) {
        saveSettings();
        lastSaveTime = now;  // Update last save time
    }
    // If not enough time has passed, skip save (settings will be saved on next interval or on shutdown)
}
