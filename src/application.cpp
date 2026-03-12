// CRITICAL: GLFW_INCLUDE_NONE MUST be the very first line to prevent OpenGL header conflicts
// This tells GLFW not to include OpenGL headers (we use GLAD instead)
#define GLFW_INCLUDE_NONE

// CRITICAL: Define NOMINMAX before any Windows headers to prevent min/max macro conflicts
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "application.h"
#include "version.h"
#include <string>
#include <vector>
#include <limits>
#include <cctype>  // For std::tolower

#if defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>
#endif

#include <GLFW/glfw3.h>

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

//-- io ----------------
#include "io/camera.h"
#include "io/keyboard.h"
#include "io/mouse.h"
#include "io/app_settings.h"
#include "utils/logger.h"
#include <imgui/imgui.h>
#include <ImGuizmo/ImGuizmo.h>

//-- graphics ------------
#include "graphics/utils.h"
#include "graphics/shader.h"
#include "graphics/texture.h"
#include "graphics/light.h"
#include "graphics/model.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "graphics/grid.h"
#include "graphics/bounding_box.h"
#include "graphics/raycast.h"
#include <map>

// Forward declarations for helper functions
static void glfw_error_callback(int error, const char* description);
static void glfw_drop_callback(GLFWwindow* window, int count, const char** paths);

Application::Application() 
    : m_window(nullptr)
    , deltaTime(0.0f)
    , lastFrame(0.0f)
    , m_cameras{Camera(glm::vec3(0.0f, 9.0f, 30.0f)), Camera(glm::vec3(0.0f, 0.0f, 1.0f))}
    , m_activeCam(0)
    , m_shader()
    , m_lampShader()
    , m_gridShader()
    , m_boundingBoxShader()
    , m_grid(100.0f, 10.0f)
    , m_dirLight{glm::vec3(-0.2f, -1.0f, -0.3f), glm::vec4(0.3f, 0.3f, 0.3f, 1.0f), glm::vec4(0.4f, 0.4f, 0.4f, 1.0f), glm::vec4(0.75f, 0.75f, 0.75f, 1.0f)}
    , m_f_startTime(0.0)
    , m_FPS(1)
    , m_totalTime(0.0f)
    , m_timeVal(0.0f)
    , m_wasPlaying(false)
    , m_file_is_open(false) {
}

Application::~Application() {
    cleanup();
}

/**
 * @brief Initializes the application, setting up GLFW, OpenGL, ImGui, and all rendering systems.
 * 
 * This function performs the following initialization steps:
 * 1. Loads application settings from JSON file
 * 2. Initializes GLFW and creates the main window
 * 3. Initializes OpenGL context via GLAD
 * 4. Sets up ImGui for UI rendering
 * 5. Compiles and links all shaders
 * 6. Initializes the grid, camera, and model manager
 * 7. Restores window size and camera state from settings
 * 
 * @return true if initialization succeeded, false otherwise
 */
bool Application::init() {
    //-- Load application settings FIRST (before creating window) ----
    AppSettingsManager& settingsMgr = AppSettingsManager::getInstance();
    settingsMgr.loadSettings("app_settings.json");
    AppSettings& appSettings = settingsMgr.getSettings();
    
    //-- Restore window size from settings ----
    if (appSettings.window.width > 0 && appSettings.window.height > 0) {
        Scene::SCR_WIDTH = appSettings.window.width;
        Scene::SCR_HEIGHT = appSettings.window.height;
    }
    
    //-- Setup window ---------------------------------
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        return false;
    }

    //--- GL 3.0 + GLSL 130 --------------------------------
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    // Use Compatibility Profile to support glLineWidth() for skeleton line width feature
    // Core Profile deprecates glLineWidth() and clamps all values > 1.0f to 1.0 on modern drivers
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    //-- Create window ----
    m_window = glfwCreateWindow(Scene::SCR_WIDTH, Scene::SCR_HEIGHT, APP_FULL_NAME, NULL, NULL);
    if (m_window == NULL) {
        LOG_ERROR("Could not create window.");
        glfwTerminate();
        return false;
    }
    
    glfwMakeContextCurrent(m_window);
    
    // Register framebuffer resize callback for dynamic window resizing
    glfwSetFramebufferSizeCallback(m_window, Application::framebuffer_size_callback);
    
    // Set window user pointer for drag and drop callback
    glfwSetWindowUserPointer(m_window, this);
    glfwSetDropCallback(m_window, glfw_drop_callback);
    
    // Apply V-Sync from loaded settings (0 = OFF, 1 = ON)
    glfwSwapInterval(appSettings.environment.vSyncEnabled ? 1 : 0);
    
    //-- Initialize OpenGL loader --------------
    #if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
        bool err = gl3wInit() != 0;
    #elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
        bool err = glewInit() != GLEW_OK;
    #elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
        bool err = gladLoadGL() == 0;
    #elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD2)
        bool err = gladLoadGL(glfwGetProcAddress) == 0;
    #elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
        bool err = false;
        glbinding::Binding::initialize();
    #elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
        bool err = false;
        glbinding::initialize([](const char* name) { return (glbinding::ProcAddress)glfwGetProcAddress(name); });
    #else
        bool err = false;
    #endif
    
    if (err) {
        LOG_ERROR("Failed to initialize OpenGL loader!");
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }
    
    //-- Initialize scene with existing window ----
    if (!m_scene.init(m_window)) {
        LOG_ERROR("Could not initialize scene.");
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }
    
    //-- Restore window position and size from settings ----
    if (appSettings.window.posX >= 0 && appSettings.window.posY >= 0) {
        glfwSetWindowPos(m_window, appSettings.window.posX, appSettings.window.posY);
        // Window position restored from settings (no log needed for normal operation)
    }
    
    //-- Validate and restore window size (after position is set) ----
    if (appSettings.window.width > 0 && appSettings.window.height > 0) {
        GLFWmonitor* monitor = glfwGetWindowMonitor(m_window);
        if (!monitor) {
            int winX, winY;
            glfwGetWindowPos(m_window, &winX, &winY);
            
            int monitorCount;
            GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
            
            for (int i = 0; i < monitorCount; i++) {
                const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
                int monX, monY;
                glfwGetMonitorPos(monitors[i], &monX, &monY);
                
                if (winX >= monX && winX < monX + mode->width &&
                    winY >= monY && winY < monY + mode->height) {
                    monitor = monitors[i];
                    break;
                }
            }
        }
        
        int maxWidth = 1920, maxHeight = 1080;
        if (monitor) {
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            maxWidth = mode->width;
            maxHeight = mode->height;
        } else {
            GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
            if (primaryMonitor) {
                const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
                maxWidth = mode->width;
                maxHeight = mode->height;
            }
        }
        
        int clampedWidth = appSettings.window.width;
        int clampedHeight = appSettings.window.height;
        
        if (clampedWidth > maxWidth) {
            clampedWidth = maxWidth;
            LOG_WARNING("Window width clamped from " + std::to_string(appSettings.window.width) + " to " + std::to_string(clampedWidth));
        }
        if (clampedHeight > maxHeight) {
            clampedHeight = maxHeight;
            LOG_WARNING("Window height clamped from " + std::to_string(appSettings.window.height) + " to " + std::to_string(clampedHeight));
        }
        
        if (clampedWidth < 640) clampedWidth = 640;
        if (clampedHeight < 480) clampedHeight = 480;
        
        glfwSetWindowSize(m_window, clampedWidth, clampedHeight);
        // Window size restored from settings (no log needed for normal operation)
    }

    //-- Set scene parameters ----
    m_scene.setParameters();
    
    //-- Set model manager reference in Scene ----
    m_scene.setModelManager(&m_modelManager);
    
    //-- Restore camera state from settings ----
    m_cameras[m_activeCam].focusPoint = appSettings.camera.focusPoint;
    m_cameras[m_activeCam].yaw = appSettings.camera.yaw;
    m_cameras[m_activeCam].pitch = appSettings.camera.pitch;
    m_cameras[m_activeCam].zoom = appSettings.camera.zoom;
    m_cameras[m_activeCam].orbitDistance = appSettings.camera.orbitDistance;
    
    //-- Set camera focus point to origin if not set ----
    if (glm::length(appSettings.camera.focusPoint) < 0.01f) {
        m_cameras[m_activeCam].setFocusPoint(glm::vec3(0.0f, 0.0f, 0.0f));
    } else {
        m_cameras[m_activeCam].restoreCameraState();
    }

    //== Initialize Shaders ===========================================
    m_shader = Shader("Assets/vertex.glsl", "Assets/fragment.glsl");
    if (m_shader.id == 0) {
        LOG_ERROR("Main shader failed to compile!");
    }
    
    m_lampShader = Shader("Assets/vertex.glsl", "Assets/lamp.fs");
    if (m_lampShader.id == 0) {
        LOG_ERROR("Lamp shader failed to compile!");
    }
    
    m_gridShader = Shader("Assets/grid.vert.glsl", "Assets/grid.frag.glsl");
    if (m_gridShader.id == 0) {
        LOG_ERROR("Grid shader failed to compile!");
    }
    
    m_boundingBoxShader = Shader("Assets/bounding_box.vert.glsl", "Assets/bounding_box.frag.glsl");
    if (m_boundingBoxShader.id == 0) {
        LOG_ERROR("Bounding box shader failed to compile!");
    }
    
    m_skeletonShader = Shader("Assets/skeleton.vert.glsl", "Assets/skeleton.frag.glsl");
    if (m_skeletonShader.id == 0) {
        LOG_ERROR("Skeleton shader failed to compile!");
    }
    
    //== Initialize Grid ===========================================
    // Grid is already initialized in constructor, just update settings
    m_grid = Grid(appSettings.grid.size, appSettings.grid.spacing);
    m_grid.setEnabled(appSettings.grid.enabled);
    m_grid.setMajorLineColor(appSettings.grid.majorLineColor);
    m_grid.setMinorLineColor(appSettings.grid.minorLineColor);
    m_grid.setCenterLineColor(appSettings.grid.centerLineColor);
    m_grid.init();
    
    // Set grid reference in Scene for Tools menu control
    m_scene.setGrid(&m_grid);
    
    // Initialize bounding boxes for any models that might already be loaded
    for (size_t i = 0; i < m_modelManager.getModelCount(); i++) {
        ModelInstance* instance = m_modelManager.getModel(static_cast<int>(i));
        if (instance) {
            instance->boundingBox.init();
            instance->initializeBoundingBox();
        }
    }
    
    // Load environment color from settings
    m_scene.setBackgroundColor(appSettings.environment.backgroundColor);
    
    // Load bounding boxes enabled state from settings
    m_modelManager.setBoundingBoxesEnabled(appSettings.environment.boundingBoxesEnabled);
    
    // Load skeletons enabled state from settings
    m_modelManager.setSkeletonsEnabled(appSettings.environment.skeletonsEnabled);
    
    //-- init time ----------------------
    m_f_startTime = glfwGetTime();
    
    // Initialize lastFrame
    lastFrame = glfwGetTime();
    
    // Sync UI panel with loaded V-Sync setting
    m_scene.getUIManager().getViewportSettingsPanel().setVSyncUIState(appSettings.environment.vSyncEnabled);
    
    return true;
}

void Application::updateTime() {
    //-- Calculate deltaTime for frame-based updates ----
    double currentTime = glfwGetTime();
    deltaTime = static_cast<float>(currentTime - lastFrame);
    lastFrame = currentTime;
}

void Application::updateCamera() {
    //-- Update framing distance only if slider is being dragged (distance-only, no re-orientation) ----
    // This prevents camera jumps during slider interaction
    m_scene.updateFramingDistanceOnly(&m_cameras[m_activeCam]);
    
    //-- Sync smooth camera UI state to camera ----
    m_cameras[m_activeCam].setSmoothCameraEnabled(m_scene.getSmoothCameraUIState());
    m_cameras[m_activeCam].setSmoothTransitionSpeed(m_scene.getSmoothCameraSpeed());
    
    //-- Update camera (handles smooth animation if active) ----
    m_cameras[m_activeCam].Update(deltaTime);
    
    //-- Check if auto-focus requested (gizmo was released with auto-focus enabled) ----
    if (m_scene.isCameraFramingRequested()) {
        // Auto-focus was triggered - trigger framing request
        // The constraint will gather bounding box data and call requestFraming()
        std::string selectedNode = m_scene.getUIManager().getOutliner().getSelectedNode();
        if (!selectedNode.empty() && m_modelManager.getModelCount() > 0) {
            // Node is selected - trigger one-shot aim constraint
            // The constraint will check if gizmo position is valid and call requestFraming()
            m_scene.setFramingTrigger(true);
        }
        m_scene.resetCameraFramingRequest();
    }
    
    //-- Apply camera aim constraint (gathers framing data when triggered) -------------------
    // This is called after scene.update() so gizmo position is up-to-date
    // The constraint only applies when gizmo is visible and right mouse is not pressed
    // NOTE: This is skipped if slider is active (distance-only update handles it)
    // CRITICAL: Removed guard for isAnimating() to allow interruptible re-targeting mid-flight
    // When a new selection is made during animation, requestFraming() will smoothly re-target
    // NOTE: This will call requestFraming() once when framing trigger is set, then clear the flag
    if (!m_scene.isFramingDistanceSliderActive()) {
        m_scene.applyCameraAimConstraint(&m_cameras[m_activeCam]);
    }
}

void Application::handleSceneEvents() {
    //-- Check if scene should be cleared (New Scene) ----
    if (m_scene.isClearSceneRequested()) {
            // Clear all models
            m_modelManager.clearAll();
            
            // Reset all state
            m_file_is_open = false;
            m_scene.setFilePath("");  // Clear current file path
            
            // Reset FPS and animation state (legacy, now handled per-model)
            m_FPS = 0;
            m_totalTime = 0.0f;
            m_timeVal = 0.0f;
            
            // Don't reset camera - keep current position or reload from settings
            // Camera position is preserved so user doesn't lose their view
            
            // Reset time slider
            m_scene.getUIManager().getTimeSlider().setMaxTimeSlider(0.0f);
            m_scene.getUIManager().getTimeSlider().setTimeValue(0.0f);
            
            // Clear outliner
            m_scene.getUIManager().getOutliner().clear();
            
            // Clear all PropertyPanel transform values (rotations, translations, scales)
            // This ensures that when a new character is imported, it won't inherit
            // the previous character's transform values from the Transforms window
            m_scene.getUIManager().getPropertyPanel().clearAllTransforms();
            
            // Reload camera settings from INI file to restore saved position
            AppSettingsManager& settingsMgr = AppSettingsManager::getInstance();
            AppSettings& appSettings = settingsMgr.getSettings();
            // Reload settings from file (in case they were changed externally)
            settingsMgr.loadSettings("app_settings.json");
            
            // Restore camera from saved settings
            m_cameras[m_activeCam].focusPoint = appSettings.camera.focusPoint;
            m_cameras[m_activeCam].yaw = appSettings.camera.yaw;
            m_cameras[m_activeCam].pitch = appSettings.camera.pitch;
            m_cameras[m_activeCam].zoom = appSettings.camera.zoom;
            m_cameras[m_activeCam].orbitDistance = appSettings.camera.orbitDistance;
            
            // Set focus point and restore camera state
            if (glm::length(appSettings.camera.focusPoint) < 0.01f) {
                m_cameras[m_activeCam].setFocusPoint(glm::vec3(0.0f, 0.0f, 0.0f));
            } else {
                m_cameras[m_activeCam].restoreCameraState();
            }
            
            LOG_INFO("Scene cleared - New Scene (camera restored from settings)");
            m_scene.resetClearSceneRequest();
        }

        //-- Check for async loading completion (polling mechanism) ----
        // This checks if a background loading task has completed and finalizes the model
        // GPU resource creation happens here on the main thread
        if (m_modelManager.checkAsyncLoadStatus()) {
            // A model was just finalized, get its index and continue with initialization
            // The model index is the last one in the collection
            int modelIndex = static_cast<int>(m_modelManager.getModelCount() - 1);
            
            if (modelIndex >= 0) {
                // Get the file path from the model instance
                ModelInstance* instance = m_modelManager.getModel(modelIndex);
                if (instance) {
                    std::string filePath = instance->filePath;
                    
                    // Continue with post-loading initialization (same as synchronous loading)
                    initializeLoadedModel(modelIndex, filePath);
                }
            }
        }
        
        //-- if new file requested, start async loading ------------ 
        if (m_scene.isNewFileRequested()) {
            std::string filePath = m_scene.getFilePath();
            if (filePath != "") {
                // Start async loading (non-blocking, returns immediately)
                // All file I/O (fread, ofbx::load) happens in background thread
                int modelIndex = m_modelManager.loadModelAsync(filePath);
                
                if (modelIndex >= 0) {
                    // Loading started successfully, will be finalized in checkAsyncLoadStatus()
                    LOG_INFO("[Application] Started async loading: " + filePath);
                    // UI initialization will happen in main loop after async load completes
                } else {
                    LOG_ERROR("[Application] Failed to start async loading: " + filePath);
                }
                
                // Reset the file request flag immediately (don't wait for load to complete)
                m_scene.resetNewFileRequest();
            }
        }
}

// Helper to initialize a loaded model (outliner, property panel, FPS, TimeSlider, etc.)
// Called after async loading completes and GPU resources are created
// NOTE: Bounding box initialization is already done in checkAsyncLoadStatus()
void Application::initializeLoadedModel(int modelIndex, const std::string& filePath) {
    ModelInstance* instance = m_modelManager.getModel(modelIndex);
    if (!instance) {
        LOG_ERROR("[Application] initializeLoadedModel: Invalid model index " + std::to_string(modelIndex));
        return;
    }
    
    std::string file_extension = instance->model.getFileExtension();
    if (file_extension == ".fbx") {
        // Use pre-loaded FBX scene and pre-calculated rig root from background thread
        // No file I/O, ofbx::load, or findRigRoot on main thread
        if (instance->ofbxScene) {
            m_scene.getUIManager().getOutliner().addPreloadedFBXScene(
                instance->ofbxScene, 
                filePath, 
                instance->rigRootName  // Pre-calculated in background thread
            );
        } else {
            LOG_ERROR("[Application] FBX scene not loaded in background thread for: " + filePath);
        }
        
        // Get RootNode name (renamed if collision detected)
        std::string rootNodeName = m_scene.getUIManager().getOutliner().getRootNodeName(modelIndex);
        
        // Get rig root name (if available)
        std::string rigRootName = m_scene.getUIManager().getOutliner().getRigRootName(modelIndex);
        
        // CRITICAL FIX: Automatically select the newly loaded model and its RootNode
        // This ensures initialization runs immediately for the new model
        // Without this, if Model 0 is still selected, Model 1 won't initialize properly
        m_modelManager.setSelectedModel(modelIndex);
        
        // Get RootNode name - declare outside if block so it's available for later use
        std::string newModelRootNode = m_scene.getUIManager().getOutliner().getRootNodeName(modelIndex);
        
        // Safety check: Ensure scene was loaded successfully before selecting
        const ofbx::IScene* loadedScene = m_scene.getUIManager().getOutliner().getScene(modelIndex);
        if (loadedScene != nullptr && !newModelRootNode.empty()) {
            m_scene.getUIManager().getOutliner().setSelectionToRoot(modelIndex);
        }
        
        // CRITICAL FIX: Reset model's pos/rotation/size to identity immediately after loading
        // This ensures the model starts at (0,0,0) regardless of previous models
        // Without this, new models would inherit transforms from previously loaded models
        // PropertyPanel RootNode transforms control the model position exclusively
        instance->model.pos = glm::vec3(0.0f, 0.0f, 0.0f);
        instance->model.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Identity quaternion
        instance->model.size = glm::vec3(1.0f, 1.0f, 1.0f);
        
        // CRITICAL FIX: Initialize RootNode transforms in PropertyPanel to (0,0,0)
        // This ensures RootNode always starts at center, regardless of previous models
        // Each model has its own RootNode entry in PropertyPanel (e.g., "RootNode", "RootNode01")
        // Only initialize if we have a valid RootNode name
        if (!newModelRootNode.empty()) {
            auto rootTransIt = m_scene.getUIManager().getPropertyPanel().getAllBoneTranslations().find(newModelRootNode);
            if (rootTransIt == m_scene.getUIManager().getPropertyPanel().getAllBoneTranslations().end()) {
                // RootNode not found - initialize it to (0,0,0)
                // This is the normal case for a newly loaded model
                ModelInstance* instance = m_modelManager.getModel(modelIndex);
                Model* modelPtr = instance ? &instance->model : nullptr;
                m_scene.getUIManager().getPropertyPanel().setSelectedNode(newModelRootNode, modelPtr);
                m_scene.getUIManager().getPropertyPanel().initializeRootNodeFromModel(
                    glm::vec3(0.0f, 0.0f, 0.0f),  // Always start at origin
                    glm::quat(1.0f, 0.0f, 0.0f, 0.0f),  // Identity rotation
                    glm::vec3(1.0f, 1.0f, 1.0f)  // Identity scale
                );
            } else {
                // RootNode already exists (shouldn't happen for new model, but reset it anyway)
                // This can occur if a model is reloaded or if there's a name collision
                ModelInstance* instance = m_modelManager.getModel(modelIndex);
                Model* modelPtr = instance ? &instance->model : nullptr;
                m_scene.getUIManager().getPropertyPanel().setSelectedNode(newModelRootNode, modelPtr);
                m_scene.getUIManager().getPropertyPanel().initializeRootNodeFromModel(
                    glm::vec3(0.0f, 0.0f, 0.0f),  // Always start at origin
                    glm::quat(1.0f, 0.0f, 0.0f, 0.0f),  // Identity rotation
                    glm::vec3(1.0f, 1.0f, 1.0f)  // Identity scale
                );
            }
            
            // Restore selection to RootNode (user expects to see RootNode selected)
            m_scene.getUIManager().getOutliner().setSelectionToRoot(modelIndex);
            ModelInstance* instance = m_modelManager.getModel(modelIndex);
            Model* modelPtr = instance ? &instance->model : nullptr;
            m_scene.getUIManager().getPropertyPanel().setSelectedNode(newModelRootNode, modelPtr);
        }
    }

    //-- get FPS of the file (for UI display, but each model has its own) -------------
    m_FPS = instance->FPS;
    
    //-- set time slider max to animation duration (use longest duration) -------------
    float animDuration = instance->model.getAnimationDuration();
    
    // Update slider to show longest animation duration across all models
    float maxDuration = animDuration;
    for (size_t i = 0; i < m_modelManager.getModelCount(); i++) {
        ModelInstance* m = m_modelManager.getModel(static_cast<int>(i));
        if (m) {
            float d = m->model.getAnimationDuration();
            if (d > maxDuration) maxDuration = d;
        }
    }
    
    LOG_INFO("Model " + std::to_string(modelIndex) + " loaded - Animation Duration: " + std::to_string(animDuration) + ", FPS: " + std::to_string(m_FPS));
    if (maxDuration > 0.0f) {
        m_scene.getUIManager().getTimeSlider().setMaxTimeSlider(maxDuration);
        LOG_INFO("Set time slider max to: " + std::to_string(maxDuration) + " (longest duration)");
    } else {
        LOG_WARNING("No animation duration found!");
    }
    
    // Add to recent files after successful load
    // Note: Path sanitization is handled inside addRecentFile() using std::filesystem
    AppSettingsManager& settingsMgr = AppSettingsManager::getInstance();
    settingsMgr.addRecentFile(filePath);
    settingsMgr.markDirty();
    
    // Mark that a file is open
    m_file_is_open = true;
}

void Application::updateAnimations(double fCurrentTime, float fInterval) {
    //-- get gui attribute first to know if we're playing -----------------------------
    bool play_stopVal = m_scene.getUIManager().getTimeSlider().getPlay_stop();
    
    // Check if stop was requested (separate from pause)
    bool stopRequested = m_scene.getUIManager().getTimeSlider().getStopRequested();
    
    // Handle stop request: Stop all animations and reset to frame 0
    if (stopRequested) {
        m_modelManager.stopAllAnimations();
        m_scene.getUIManager().getTimeSlider().setTimeValue(0.0f);  // Reset slider to 0
        m_timeVal = 0.0f;  // Reset global clock to keep in sync with models
        m_wasPlaying = false;
        LOG_INFO("Stopped all animations - reset to frame 0");
    }

    //-- Update animation state for all models independently -----------------------------
    // Set play state for all models based on global play/stop button
    for (size_t i = 0; i < m_modelManager.getModelCount(); i++) {
        ModelInstance* instance = m_modelManager.getModel(static_cast<int>(i));
        if (instance) {
            // If we just started playing, update timing references
            // Note: If we were stopped, stop() already reset animationTime to 0
            // If we were paused, animationTime is preserved (character stays in current pose)
            if (play_stopVal && !m_wasPlaying) {
                // Starting to play (either from stopped or paused state)
                m_f_startTime = fCurrentTime;
                // Sync totalTime with current animationTime (preserves paused frame or uses 0 from stop)
                instance->totalTime = instance->animationTime;
                instance->lastUpdateTime = fCurrentTime;
            }
            
            instance->setPlaying(play_stopVal);
            
            // Update animation time for this model
            if (play_stopVal) {
                instance->updateAnimation(fCurrentTime, fInterval);
            } else {
                // When paused, allow manual scrubbing via slider
                // Pausing keeps the character in its current pose (animationTime preserved)
                // User can scrub the slider to manually set animation time
                float sliderTime = m_scene.getUIManager().getTimeSlider().getTimeValue();
                if (instance->FPS > 0) {
                    instance->setAnimationTime(sliderTime / instance->FPS);
                }
                instance->lastUpdateTime = fCurrentTime;
            }
        }
    }
    
    if (play_stopVal && !m_wasPlaying) {
        m_wasPlaying = true;
        m_totalTime = 0.0f;
        LOG_INFO("Started playing all animations");
    }
    
    if (!play_stopVal && m_wasPlaying && !stopRequested) {
        // Paused (not stopped) - animation time is preserved
        m_wasPlaying = false;
        LOG_INFO("Paused all animations - current frame preserved");
    }
    
    // Update global time slider based on first model (for UI display)
    if (m_modelManager.getModelCount() > 0) {
        ModelInstance* firstModel = m_modelManager.getModel(0);
        if (firstModel && play_stopVal) {
            // Update slider while playing (shows current animation time)
            m_scene.getUIManager().getTimeSlider().setTimeValue(firstModel->animationTime * firstModel->FPS);
        }
        // When stopped, slider is already reset to 0 in the stop handler above
        // User can still manually scrub the slider, which will update animation time via the else block
    }
}

/**
 * @brief Synchronizes transform data from UI (PropertyPanel) to the animation system.
 * 
 * This function implements per-model RootNode transform isolation to prevent models from
 * interfering with each other's positions. Each model maintains its own RootNode transform
 * matrix in the m_modelRootNodeMatrices map, keyed by model index.
 * 
 * Key Logic:
 * - For RootNode selections: Sets UI data to identity (RootNode transforms are handled separately)
 * - For bone nodes: Applies PropertyPanel slider values to bone transforms
 * - Builds per-model RootNode matrices from PropertyPanel data for ALL models
 * - Resets model.pos/rotation/size to identity to prevent double-transformation
 * 
 * The per-model isolation ensures that:
 * - Each model uses its own RootNode entry (e.g., "RootNode", "RootNode01", "RootNode02")
 * - Models don't jump to each other's positions when deselected
 * - Transform hierarchies remain isolated between models
 */
void Application::syncTransforms() {
    //-- Obtains newly transformed matrices from the bone hierarchy at the given time. -----
    std::string selectedNode = m_scene.getUIManager().getOutliner().getSelectedNode();
    m_uiData.nodeName_toTrasform = selectedNode;
    
    // CRITICAL FIX: We already efficiently updated the selected model in scene.update()
    // using the cached pointer. We DO NOT need to run the heavy O(N) search 
    // findModelIndexForSelectedObject() here every frame anymore!
    
    int selectedModelIndex = m_modelManager.getSelectedModelIndex();
    
    // CACHE OUTLINER NAMES IN MAIN LOOP TO PREVENT PER-FRAME TREE SEARCHES
    static std::string cachedRigRootName = "";
    static int lastModelIndexForNames = -1;
    
    if (selectedModelIndex != lastModelIndexForNames) {
        cachedRigRootName = m_scene.getUIManager().getOutliner().getRigRootName(selectedModelIndex);
        lastModelIndexForNames = selectedModelIndex;
    }
    
    std::string rigRootName = cachedRigRootName;
    
    // Check if the selected node is the rig root (e.g., "Rig_GRP") OR RootNode (e.g., "RootNode", "RootNode01")
    // The selected node name might have a numeric prefix (e.g., "1810483895296 Rig_GRP"),
    // so we check if it contains the rig root name or matches exactly
    // Also check for RootNode (including renamed ones like "RootNode01") - these should also use PropertyPanel transforms
    bool isRigRootSelected = false;
    bool isRootNodeSelected = (!selectedNode.empty() && 
                              (selectedNode.find("Root") != std::string::npos || 
                               selectedNode == "RootNode"));
    
    if (!rigRootName.empty()) {
        // Check for exact match or if selected node contains the rig root name
        // (handles cases where object name has numeric prefix like "1810483895296 Rig_GRP")
        isRigRootSelected = (selectedNode == rigRootName || 
                            selectedNode.find(rigRootName) != std::string::npos);
    }
    
    // CRITICAL: Treat RootNode (including renamed ones) as rig root for transform purposes
    // This ensures RootNode01 uses PropertyPanel transforms, not model.pos
    // RootNode renaming is cosmetic (UI only), but we need to handle it like a rig root
    if (isRootNodeSelected) {
        isRigRootSelected = true;  // RootNode should use PropertyPanel transforms
    }
    
    //-- Handle rig root selection (for initialization only) ----
    // When rig root is selected, we need to:
    // 1. Initialize PropertyPanel values if this is the first time selecting this rig root
    // Note: Model index is already updated above for ANY node selection
    if (isRigRootSelected) {
        // Use the already-updated selected model index
        int rigRootModelIndex = m_modelManager.getSelectedModelIndex();
        
        if (rigRootModelIndex >= 0 && rigRootModelIndex < static_cast<int>(m_modelManager.getModelCount())) {
            
            // CRITICAL: Check if this rig root needs initialization BEFORE calling setSelectedNode()
            // This ensures that when setSelectedNode() is called, it will load the correct values
            // FIX: Get the rig root name for THIS SPECIFIC MODEL (rigRootModelIndex), not the currently selected node
            // This ensures we check the correct (possibly renamed) name for the model being initialized
            
            // CRITICAL: If RootNode is selected, prioritize it over rig root for initialization
            // RootNode renaming is cosmetic (UI only), but we need to initialize the actual selected RootNode
            std::string rigRootNameForModel;
            if (isRootNodeSelected) {
                // RootNode is selected - use the selected node name (e.g., "RootNode01")
                rigRootNameForModel = m_scene.getUIManager().getOutliner().getSelectedNode();
            } else {
                // Rig root is selected - use rig root name
                rigRootNameForModel = m_scene.getUIManager().getOutliner().getRigRootName(rigRootModelIndex);
                
                // If no rig root found, try RootNode name (renamed if collision)
                if (rigRootNameForModel.empty()) {
                    rigRootNameForModel = m_scene.getUIManager().getOutliner().getRootNodeName(rigRootModelIndex);
                }
                // Fallback to selected node only if both above are empty
                if (rigRootNameForModel.empty()) {
                    rigRootNameForModel = m_scene.getUIManager().getOutliner().getSelectedNode();
                }
            }
            
            ModelInstance* selectedModel = m_modelManager.getSelectedModel();
            
            if (selectedModel != nullptr) {
                // Check if this rig root already has saved transforms in bone maps
                // Use the CORRECT name (renamed if collision) for this specific model
                // This prevents new models from inheriting transforms from previous models with the same name
                auto transIt = m_scene.getUIManager().getPropertyPanel().getAllBoneTranslations().find(rigRootNameForModel);
                
                if (transIt == m_scene.getUIManager().getPropertyPanel().getAllBoneTranslations().end()) {
                    // First time selecting this rig root - initialize from model's current transform
                    // Model's pos should be (0,0,0) for a newly loaded model
                    glm::vec3 currentPos = selectedModel->model.pos;
                    glm::quat currentRotation = selectedModel->model.rotation;
                    glm::vec3 currentSize = selectedModel->model.size;
                    
                    // CRITICAL: Initialize BEFORE calling setSelectedNode() to ensure values are in bone maps
                    // We need to manually set the selected node name temporarily for initialization
                    // This ensures initializeRootNodeFromModel() uses the correct rig root name (renamed if collision)
                    ModelInstance* instance = m_modelManager.getModel(rigRootModelIndex);
                    Model* modelPtr = instance ? &instance->model : nullptr;
                    m_scene.getUIManager().getPropertyPanel().setSelectedNode(rigRootNameForModel, modelPtr);
                    
                    m_scene.getUIManager().getPropertyPanel().initializeRootNodeFromModel(
                        currentPos,
                        currentRotation,
                        currentSize
                    );
                    
                    // CRITICAL: Reset model.pos/rotation/size to identity after initializing PropertyPanel
                    // This prevents double transformation - PropertyPanel will control the transform exclusively
                    selectedModel->model.pos = glm::vec3(0.0f, 0.0f, 0.0f);
                    selectedModel->model.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Identity quaternion
                    selectedModel->model.size = glm::vec3(1.0f, 1.0f, 1.0f);
                }
            }
        }
    }
    
    // Now call setSelectedNode() - this will load the correct values (either from bone maps or defaults)
    // Note: If we already initialized above, this will just ensure PropertyPanel is synced
    int currentSelectedModelIndex = m_modelManager.getSelectedModelIndex();
    ModelInstance* instance = (currentSelectedModelIndex >= 0) ? m_modelManager.getModel(currentSelectedModelIndex) : nullptr;
    Model* modelPtr = instance ? &instance->model : nullptr;
    m_scene.getUIManager().getPropertyPanel().setSelectedNode(selectedNode, modelPtr);
    
    // CRITICAL: Always exclude RootNode from bone transforms for ALL models
    // RootNode transforms are applied via per-model RootNode transform matrices, not through bone hierarchy
    // This prevents double transformation regardless of selection state
    // Check if PropertyPanel's selected node is a RootNode (any model's RootNode)
    // We check PropertyPanel because it might have a different selected node than Outliner
    std::string propertyPanelSelectedNode = m_scene.getUIManager().getPropertyPanel().getSelectedNodeName();
    bool isSelectedNodeRootNode = false;
    
    if (!propertyPanelSelectedNode.empty()) {
        // Check if PropertyPanel's selected node is RootNode (including renamed ones like "RootNode01")
        isSelectedNodeRootNode = (propertyPanelSelectedNode.find("Root") != std::string::npos || 
                                 propertyPanelSelectedNode == "RootNode");
        
        // Also check if it matches any model's RootNode name
        if (!isSelectedNodeRootNode) {
            for (size_t i = 0; i < m_modelManager.getModelCount(); i++) {
                std::string modelRootNodeName = m_scene.getUIManager().getOutliner().getRootNodeName(static_cast<int>(i));
                if (propertyPanelSelectedNode == modelRootNodeName) {
                    isSelectedNodeRootNode = true;
                    break;
                }
            }
        }
    }
    
    // For RootNode: set bone transform values to identity (RootNode transform comes from model matrix only)
    // For other nodes: apply PropertyPanel values to bone transforms
    if (isSelectedNodeRootNode) {
        // RootNode is selected in PropertyPanel - exclude it from bone transforms
        // RootNode transform is applied via per-model RootNode transform matrices, not through bone hierarchy
        m_uiData.mSliderRotations = glm::vec3(0.0f, 0.0f, 0.0f);
        m_uiData.mSliderTraslations = glm::vec3(0.0f, 0.0f, 0.0f);
        m_uiData.mSliderScales = glm::vec3(1.0f, 1.0f, 1.0f);
    } else {
        // Non-RootNode selected - apply PropertyPanel values to bone transforms
        m_uiData.mSliderRotations = m_scene.getUIManager().getPropertyPanel().getSliderRot_update();
        m_uiData.mSliderTraslations = m_scene.getUIManager().getPropertyPanel().getSliderTrans_update();
        m_uiData.mSliderScales = m_scene.getUIManager().getPropertyPanel().getSliderScale_update();
    }
    
    // Check if "Reset All Bones" was requested to clean up RootNode memory
    if (m_scene.getUIManager().getPropertyPanel().isResetAllBonesRequested()) {
        int selectedModelIndex = m_modelManager.getSelectedModelIndex();
        if (selectedModelIndex >= 0) {
            // Erase the RootNode from UI memory so the character jumps back to origin
            std::string rootName = m_scene.getUIManager().getOutliner().getRootNodeName(selectedModelIndex);
            if (!rootName.empty()) {
                m_scene.getUIManager().getPropertyPanel().removeBoneTransform(rootName);
            }
            // Also erase Rig_GRP if it exists
            std::string rigName = m_scene.getUIManager().getOutliner().getRigRootName(selectedModelIndex);
            if (!rigName.empty()) {
                m_scene.getUIManager().getPropertyPanel().removeBoneTransform(rigName);
            }
            
            // CRITICAL FIX: Trigger auto-focus if enabled so the camera follows the character back to origin
            if (m_scene.getAutoFocusEnabled()) {
                m_scene.setFramingTrigger(true);
            }
        }
        m_scene.getUIManager().getPropertyPanel().clearResetAllBonesRequest();
    }
    
    // CRITICAL FIX: Each model must use its own RootNode transforms from PropertyPanel
    // This prevents models from jumping to each other's positions when deselected
    // 
    // PROBLEM: Previously, only the selected model used PropertyPanel RootNode transforms.
    // When nothing was selected, other models fell back to their internal model.pos/rotation/size
    // (which were reset to identity), causing them to jump to (0,0,0) or inherit incorrect transforms.
    //
    // SOLUTION: Build per-model RootNode transform matrices for ALL models, not just the selected one.
    // Each model has its own RootNode entry in PropertyPanel (e.g., "RootNode", "RootNode01", "RootNode02").
    // This ensures each model always uses its own RootNode transform, isolating hierarchies.
    
    // Build per-model RootNode transform matrices
    m_modelRootNodeMatrices.clear();
    
    for (size_t i = 0; i < m_modelManager.getModelCount(); i++) {
        int modelIndex = static_cast<int>(i);
        std::string modelRootNodeName = m_scene.getUIManager().getOutliner().getRootNodeName(modelIndex);
        
        if (!modelRootNodeName.empty()) {
            // Get this model's RootNode transforms from PropertyPanel
            // Each model has its own RootNode entry in bone maps (e.g., "RootNode", "RootNode01")
            const auto& allBoneTranslations = m_scene.getUIManager().getPropertyPanel().getAllBoneTranslations();
            const auto& allBoneRotations = m_scene.getUIManager().getPropertyPanel().getAllBoneRotations();
            const auto& allBoneScales = m_scene.getUIManager().getPropertyPanel().getAllBoneScales();
            
            auto transIt = allBoneTranslations.find(modelRootNodeName);
            auto rotIt = allBoneRotations.find(modelRootNodeName);
            auto scaleIt = allBoneScales.find(modelRootNodeName);
            
            glm::vec3 translation = (transIt != allBoneTranslations.end()) ? transIt->second : glm::vec3(0.0f);
            glm::vec3 rotationEuler = (rotIt != allBoneRotations.end()) ? rotIt->second : glm::vec3(0.0f);
            glm::vec3 scale = (scaleIt != allBoneScales.end()) ? scaleIt->second : glm::vec3(1.0f);
            
            // Convert Euler angles (degrees) to quaternion
            glm::quat rotation = glm::quat(glm::radians(rotationEuler));
            
            // Build model matrix from this model's RootNode transforms
            glm::mat4 modelMatrix = glm::mat4(1.0f);
            modelMatrix = glm::translate(modelMatrix, translation);
            modelMatrix = modelMatrix * glm::mat4_cast(rotation);
            modelMatrix = glm::scale(modelMatrix, scale);
            
            m_modelRootNodeMatrices[modelIndex] = modelMatrix;
            
            // CRITICAL: Reset model.pos/rotation/size to identity
            // PropertyPanel RootNode transforms control the model position exclusively
            ModelInstance* instance = m_modelManager.getModel(modelIndex);
            if (instance) {
                instance->model.pos = glm::vec3(0.0f, 0.0f, 0.0f);
                instance->model.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                instance->model.size = glm::vec3(1.0f, 1.0f, 1.0f);
            }
        }
    }
}

/**
 * @brief Renders a single frame of the application.
 * 
 * This function performs the complete rendering pipeline:
 * 1. Calculates view and projection matrices from camera
 * 2. Renders the grid
 * 3. Sets up lighting and material properties (shared across all models)
 * 4. Renders all models with their per-model RootNode transforms
 * 5. Renders bounding boxes for all models (if enabled)
 * 
 * Optimization Notes:
 * - View position and light properties are set once before the model loop
 * - These uniforms are shared across all models, so they don't need to be set per-model
 * - Per-model RootNode matrices ensure each model is rendered at its correct world position
 */
void Application::renderFrame() {
    //-- Calculate view and projection matrices (needed for both grid and models) ------------------
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);

    // Get view matrix from camera (no rotation offset - use camera's view directly)
    view = m_cameras[m_activeCam].getViewMatrix();
    // NOTE: Removed glm::rotate(-25 degrees) to prevent perspective drift
    // The camera's view matrix should be used directly for accurate centering

    // 1. DYNAMIC SIZE DETECTION: Get the EXACT width and height of the rendering area
    // Use the actual content region size captured in renderViewportWindow()
    ImVec2 viewportContentSize = m_scene.getViewportContentSize();
    
    // 3. CORRECT ASPECT RATIO: Update camera.projectionMatrix using exactly size.x / size.y
    // CRUCIAL: Ensure the division is floating-point: (float)size.x / (float)size.y
    // This ensures accurate aspect ratio calculation without integer truncation
    float aspectRatio;
    if (viewportContentSize.x > 0 && viewportContentSize.y > 0) {
        // Use exact content region size with floating-point division
        aspectRatio = (float)viewportContentSize.x / (float)viewportContentSize.y;
    } else {
        // Fallback to default aspect ratio if viewport not yet initialized
        aspectRatio = (float)Scene::SCR_WIDTH / (float)Scene::SCR_HEIGHT;
    }
    
    // Get far clipping plane from settings
    AppSettings& settings = AppSettingsManager::getInstance().getSettings();
    float farClipPlane = settings.environment.farClipPlane;
    
    // 3. CORRECT ASPECT RATIO: Update the Projection Matrix using this NEW aspect ratio every frame
    // Since the aspect ratio is synced to the exact viewport size, objects will be correctly centered
    projection = glm::perspective(glm::radians(m_cameras[m_activeCam].getZoom()), aspectRatio, 0.1f, farClipPlane);

    //-- Set view and projection matrices for gizmo rendering -------------------
    m_scene.setViewProjectionMatrices(view, projection);

    //-- Render grid (always visible, regardless of file loading) ----------------------------
    m_grid.render(m_gridShader, view, projection);
    
    if (m_file_is_open) {
        //-- Set shared uniforms (applied once for all models) ------------------
        // These uniforms are the same for all models, so set them before the model loop
        m_shader.activate();
        m_shader.set3Float("viewPos", m_cameras[m_activeCam].cameraPos);
        m_shader.setMat4("view", view);
        m_shader.setMat4("projection", projection);

        //-- Set directional light (shared across all models) -----------------
        m_dirLight.render(m_shader);

        //-- Render all models with their independent RootNode transforms ----------------------------
        // Each model uses its own RootNode transform matrix from PropertyPanel
        m_modelManager.renderAllWithRootNodeTransforms(m_shader, m_uiData, m_Transforms, m_modelRootNodeMatrices, 
                                                       view, projection, &m_skeletonShader);
            
        //-- Render bounding boxes for all models (with real-time updates) ----------------------------
        // CRITICAL: Bounding boxes MUST be calculated every frame regardless of visibility toggle
        // They are needed for Raycast Selection and Camera Framing, which must work even when boxes are hidden
        // 
        // PERFORMANCE NOTE: With bone-based calculation (O(Bones) complexity), this is now ultra-fast
        // The calculation is so fast that it can run every frame without performance impact
        // 
        // Bounding boxes are updated every frame to reflect current animation/deformation state.
        // This ensures bounding boxes accurately represent the model's current pose, including:
        // - Animation frames
        // - Bone deformations
        // - Manual bone rotations/translations/scales
        // - Model position and scale transformations
        // 
        // PERFORMANCE NOTE: Bone transforms are cached from renderAll() to avoid recalculation.
        for (size_t i = 0; i < m_modelManager.getModelCount(); i++) {
            ModelInstance* instance = m_modelManager.getModel(static_cast<int>(i));
            if (instance) {
                // CRITICAL: Always calculate bounding box (needed for selection and framing)
                // Use cached bone transforms from renderAll() (performance optimization)
                // This avoids recalculating bone transforms twice per frame
                const std::vector<glm::mat4>& modelTransforms = instance->boneTransformsCached ? 
                    instance->cachedBoneTransforms : std::vector<glm::mat4>();
                
                // Get bounding box with current bone transformations applied
                // This ensures the bounding box updates in real-time as the model animates/deforms
                glm::vec3 min, max;
                if (instance->boneTransformsCached && !modelTransforms.empty() && instance->model.getFileExtension() != "") {
                    // Use bone-aware bounding box calculation for animated models (O(Bones) - ultra-fast)
                    instance->model.getBoundingBoxWithBones(min, max, modelTransforms);
                } else {
                    // Fallback to static bounding box for non-animated models or if cache unavailable
                    instance->model.getBoundingBox(min, max);
                }
                
                // Update bounding box geometry (regenerates vertices if min/max changed)
                // The update() method only regenerates vertices if min/max actually changed
                instance->boundingBox.update(min, max);
                
                // Only render bounding box if enabled (visibility toggle)
                // Calculation always happens, but rendering is conditional
                // Check individual model's bounding box visibility flag
                if (instance->model.m_showBoundingBox) {
                    // Set bounding box color based on selection state
                    // Selected models have a different color (cyan) to indicate selection
                    bool isSelected = (m_modelManager.getSelectedModelIndex() == static_cast<int>(i));
                    if (isSelected) {
                        instance->boundingBox.setColor(glm::vec3(0.0f, 1.0f, 1.0f)); // Cyan for selected
                    } else {
                        instance->boundingBox.setColor(glm::vec3(1.0f, 1.0f, 0.0f)); // Yellow for unselected
                    }
                    
                    // Use the same model matrix as the model rendering
                    // Each model uses its own RootNode transform matrix from PropertyPanel
                    glm::mat4 boundingBoxMatrix = glm::mat4(1.0f);
                    auto matrixIt = m_modelRootNodeMatrices.find(static_cast<int>(i));
                    if (matrixIt != m_modelRootNodeMatrices.end()) {
                        // Use this model's RootNode transform matrix
                        boundingBoxMatrix = matrixIt->second;
                    }
                    // Otherwise, use identity (model.pos/rotation/size are already reset to identity)
                    
                    // Pass selection state for visual feedback (thicker lines for selected models)
                    instance->boundingBox.render(m_boundingBoxShader, view, projection, boundingBoxMatrix, isSelected);
                }
            }
        }
    }
}

// Handle dropped file (drag and drop support)
// Called by GLFW drop callback when an FBX file is dropped onto the window
void Application::handleDroppedFile(const std::string& path) {
    // Robustness: Stop all animations before loading new model to prevent animation timer glitches
    // This ensures the new model starts with a clean animation state
    if (m_modelManager.getModelCount() > 0) {
        // Check if any model is currently playing
        bool anyModelPlaying = false;
        for (size_t i = 0; i < m_modelManager.getModelCount(); i++) {
            ModelInstance* instance = m_modelManager.getModel(static_cast<int>(i));
            if (instance && instance->isPlaying) {
                anyModelPlaying = true;
                break;
            }
        }
        
        // Stop all animations if any model is playing
        if (anyModelPlaying) {
            m_modelManager.stopAllAnimations();
            m_scene.getUIManager().getTimeSlider().setTimeValue(0.0f);
            m_timeVal = 0.0f;  // Reset global clock
            m_wasPlaying = false;
            LOG_INFO("Stopped animations before loading new file");
        }
    }
    
    m_scene.setFilePath(path);
}

/**
 * @brief Main application loop - runs until the window is closed.
 * 
 * This function implements the main game loop pattern:
 * 1. Updates time (deltaTime calculation)
 * 2. Processes input (keyboard, mouse)
 * 3. Updates camera (smooth transitions, auto-focus)
 * 4. Handles scene events (file loading, clearing)
 * 5. Updates animations (if playing)
 * 6. Synchronizes transforms (UI to animation system)
 * 7. Renders the frame (grid, models, bounding boxes, UI)
 * 
 * The loop continues until glfwWindowShouldClose() returns true.
 */
void Application::run() {
    //== run the main loop =========================
    while (!m_scene.shouldClose()) {
        updateTime();
        processInput(deltaTime);
        
        m_scene.update();
        updateCamera();
        handleSceneEvents();
        
        // Check if async loading has completed and initialize UI
        // Check if CPU phase is complete (async loading finished)
        // This adds the model to the collection and starts incremental GPU upload
        m_modelManager.checkAsyncLoadStatus();
        
        // Continue incremental GPU upload (time-sliced, one mesh per frame)
        // This distributes the 371ms GPU upload across multiple frames to prevent stuttering
        if (m_modelManager.continueGPUUpload()) {
            // GPU upload complete, now initialize UI components
            int lastModelIndex = static_cast<int>(m_modelManager.getModelCount()) - 1;
            if (lastModelIndex >= 0) {
                ModelInstance* instance = m_modelManager.getModel(lastModelIndex);
                if (instance && instance->gpuUploadComplete) {
                    // Initialize all UI components for the newly loaded model
                    // This includes: Outliner, PropertyPanel RootNode transforms, FPS, TimeSlider
                    initializeLoadedModel(lastModelIndex, instance->filePath);
                }
            }
        }
        
        double fCurrentTime = glfwGetTime();
        float fInterval = static_cast<float>(fCurrentTime - m_f_startTime);
        
        m_scene.beginViewportRender();
        m_file_is_open = (m_modelManager.getModelCount() > 0);
        
        if (m_file_is_open) {
            updateAnimations(fCurrentTime, fInterval);
            syncTransforms();
        }
        
        renderFrame();
        
        m_scene.endViewportRender();
        glfwSwapBuffers(m_window);
        glfwPollEvents();
        
        // DEFERRED DELETION: Check for pending deletion and perform actual deletion at end of frame
        // This happens after rendering and buffer swap, ensuring one frame has passed with zeroed transforms
        // so ImGui state and rendering matrices have synchronized to the zeroed values
        int pendingDeleteIndex = m_scene.getPendingDeleteModelIndex();
        if (pendingDeleteIndex >= 0) {
            // Perform the actual model deletion
            m_modelManager.removeModel(pendingDeleteIndex);
            
            // Explicitly clear selection state
            m_modelManager.setSelectedModel(-1);
            m_scene.getUIManager().getPropertyPanel().setSelectedNode("", nullptr);
            
            // Clear the pending deletion flag
            m_scene.clearPendingDeleteModelIndex();
            
            LOG_INFO("Deferred deletion: Model removed after one frame with zeroed transforms");
        }
        
        m_f_startTime = fCurrentTime;
    }
}

/**
 * @brief Static callback function for GLFW framebuffer resize events.
 * 
 * This callback is automatically called by GLFW when the window is resized.
 * It updates the global scene dimensions and OpenGL viewport to match the new window size.
 * 
 * @param window The GLFW window that was resized
 * @param width New framebuffer width in pixels
 * @param height New framebuffer height in pixels
 */
void Application::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    // Update global scene dimensions
    Scene::SCR_WIDTH = width;
    Scene::SCR_HEIGHT = height;
    
    // Update OpenGL viewport to match new framebuffer size
    glViewport(0, 0, width, height);
}

void Application::cleanup() {
    if (m_window == nullptr) {
        return; // Already cleaned up or never initialized
    }
    
    // Save settings on exit
    AppSettingsManager& settingsMgr = AppSettingsManager::getInstance();
    AppSettings& appSettings = settingsMgr.getSettings();
    
    // Update camera state before saving
    appSettings.camera.position = m_cameras[m_activeCam].cameraPos;
    appSettings.camera.focusPoint = m_cameras[m_activeCam].focusPoint;
    appSettings.camera.yaw = m_cameras[m_activeCam].yaw;
    appSettings.camera.pitch = m_cameras[m_activeCam].pitch;
    appSettings.camera.zoom = m_cameras[m_activeCam].zoom;
    appSettings.camera.orbitDistance = m_cameras[m_activeCam].orbitDistance;
    
    // Save window position and size on exit
    int winX, winY, winW, winH;
    glfwGetWindowPos(m_window, &winX, &winY);
    glfwGetWindowSize(m_window, &winW, &winH);
    appSettings.window.posX = winX;
    appSettings.window.posY = winY;
    appSettings.window.width = winW;
    appSettings.window.height = winH;
    
    // Save grid and environment settings before exit
    appSettings.grid.size = m_grid.getSize();
    appSettings.grid.spacing = m_grid.getSpacing();
    appSettings.grid.enabled = m_grid.isEnabled();
    appSettings.grid.majorLineColor = m_grid.getMajorLineColor();
    appSettings.grid.minorLineColor = m_grid.getMinorLineColor();
    appSettings.grid.centerLineColor = m_grid.getCenterLineColor();
    appSettings.environment.backgroundColor = m_scene.getBackgroundColor();
    
    settingsMgr.saveSettings();
    LOG_INFO("Saved settings on exit (window pos: " + std::to_string(winX) + ", " + std::to_string(winY) + ")");
    
    m_scene.cleanup();
    m_modelManager.clearAll();
    m_grid.cleanup();
    
    glfwDestroyWindow(m_window);
    m_window = nullptr;
    glfwTerminate();
}

void Application::processInput(double deltaTime) {
    ////-- get keyboard values ------------------
    if (Keyboard::key(GLFW_KEY_ESCAPE)) {
        m_scene.setShouldClose(true);
    }
    
    //-- Frame All: Press A to frame all objects in the scene ----
    if (Keyboard::keyWentDown(GLFW_KEY_A)) {
        if (m_modelManager.getModelCount() > 0) {
            glm::vec3 min, max;
            m_modelManager.getCombinedBoundingBox(min, max);
            m_cameras[m_activeCam].frameBoundingBox(min, max);
        }
    }
    
    //-- Focus on Gizmo: Press F to aim camera at gizmo (one-shot) ----
    if (Keyboard::keyWentDown(GLFW_KEY_F)) {
        // Check if gizmo is visible by checking if there's a selected node
        // The gizmo position will be validated in applyCameraAimConstraint
        std::string selectedNode = m_scene.getUIManager().getOutliner().getSelectedNode();
        if (!selectedNode.empty() && m_modelManager.getModelCount() > 0) {
            // Node is selected - trigger one-shot aim constraint
            // The constraint will check if gizmo position is valid
            m_scene.setFramingTrigger(true);
        } else if (m_modelManager.getModelCount() > 0) {
            // No gizmo visible - fallback to framing selected object (original behavior)
            std::string selectedNode = m_scene.getUIManager().getOutliner().getSelectedNode();
            if (!selectedNode.empty()) {
                // Try to find the node in any of the loaded models
                bool found = false;
                for (size_t i = 0; i < m_modelManager.getModelCount(); i++) {
                    ModelInstance* instance = m_modelManager.getModel(static_cast<int>(i));
                    if (instance) {
                        // Get bone index from name, then use index-based bounding box
                        int boneIndex = instance->model.getBoneIndexFromName(selectedNode);
                        if (boneIndex >= 0) {
                            glm::vec3 min, max;
                            m_modelManager.getNodeBoundingBoxByIndex(static_cast<int>(i), boneIndex, min, max);
                            // Check if bounding box is valid (not default)
                            if (min != glm::vec3(-1.0f) || max != glm::vec3(1.0f)) {
                                m_cameras[m_activeCam].frameBoundingBox(min, max);
                                found = true;
                                break;
                            }
                        }
                    }
                }
                if (!found) {
                    // If node not found, frame all
                    glm::vec3 min, max;
                    m_modelManager.getCombinedBoundingBox(min, max);
                    m_cameras[m_activeCam].frameBoundingBox(min, max);
                }
            } else {
                // If nothing selected, frame all
                glm::vec3 min, max;
                m_modelManager.getCombinedBoundingBox(min, max);
                m_cameras[m_activeCam].frameBoundingBox(min, max);
            }
        }
    }

    //== mouse transform (Maya-style controls) ============================================
    // Maya viewport navigation controls:
    // - Alt + Left Mouse Button: Rotate/Tumble (rotates camera around view center)
    // - Alt + Middle Mouse Button: Pan/Track (shifts camera horizontally/vertically)
    // - Alt + Right Mouse Button: Zoom/Dolly (moves camera closer/farther)
    // - Scroll Wheel: Zoom/Dolly (faster way to zoom in/out, works with or without Alt)
    
    // Only process camera movement when mouse is over the viewport
    bool isOverViewport = m_scene.isMouseOverViewport();
    
    // Track previous hover state to reset firstMouse when entering viewport
    static bool wasOverViewport = false;
    if (isOverViewport && !wasOverViewport) {
        // Mouse just entered viewport - reset firstMouse to prevent jump
        Mouse::resetFirstMouse();
    }
    wasOverViewport = isOverViewport;
    
    // Check if Alt key is pressed (required for Maya-style camera controls)
    bool altPressed = Keyboard::key(GLFW_KEY_LEFT_ALT) || Keyboard::key(GLFW_KEY_RIGHT_ALT);
    
    if (isOverViewport && altPressed) {
        // Prevent ImGui from capturing mouse input when Alt is pressed
        ImGuiIO& io = ImGui::GetIO();
        io.WantCaptureMouse = false;
        
        // Alt + Left Mouse Button: Rotate/Tumble
        // NOTE: Disabled during smooth camera animation (prevents overriding interpolated yaw/pitch)
        if (Mouse::button(GLFW_MOUSE_BUTTON_LEFT) && !m_cameras[m_activeCam].isAnimating()) {
            double dx = Mouse::getDX() * 0.1;
            double dy = Mouse::getDY() * 0.1;
            if (dx != 0 || dy != 0) {
                m_cameras[m_activeCam].updateCameraDirection(dx, dy);
            }
        }
        
        // Alt + Middle Mouse Button: Pan/Track
        if (Mouse::button(GLFW_MOUSE_BUTTON_MIDDLE)) {
            double dx = Mouse::getDX();
            double dy = Mouse::getDY();
            if (dx != 0 || dy != 0) {
                m_cameras[m_activeCam].updateCameraPan(dx, dy);
            }
        }
        
        // Alt + Right Mouse Button: Zoom/Dolly
        if (Mouse::button(GLFW_MOUSE_BUTTON_RIGHT)) {
            double dy = Mouse::getDY();
            if (dy != 0) {
                m_cameras[m_activeCam].updateCameraDolly(dy);
            }
        }
    }
    
    // Scroll Wheel: Zoom/Dolly (faster way to zoom, works with or without Alt)
    // In Maya, scroll wheel dollies (moves camera), not FOV zoom
    if (isOverViewport) {
        double scrollDy = Mouse::getScrollDY();
        if (scrollDy != 0) {
            // Scroll up (positive) moves camera forward, scroll down (negative) moves backward
            m_cameras[m_activeCam].updateCameraDolly(scrollDy * 0.5); // Scale scroll for better control
        }
    }
    
    // Handle mouse clicks for object selection (only when NOT using Alt for camera control)
    // Check if mouse is over the actual viewport image (not just the window, which includes menu bars)
    bool isOverViewportImage = m_scene.isMouseOverViewportImage();
    ImGuiIO& io = ImGui::GetIO();
    bool imguiWantsMouse = io.WantCaptureMouse;
    
    // Check if ImGuizmo is using the mouse - if so, don't process selection
    // This prevents deselection when clicking on the gizmo
    // Check directly from ImGuizmo (more reliable than cached state)
    bool gizmoUsing = ImGuizmo::IsUsing();
    bool gizmoOver = ImGuizmo::IsOver();
    
    // Handle mouse clicks for object selection (only when NOT using Alt for camera control)
    // Selection works even when bounding boxes are disabled - bounding boxes are calculated on-demand
    // for selection purposes without being rendered, then discarded to save resources
    // IMPORTANT: Skip selection if gizmo is being used or mouse is over gizmo
    if (isOverViewportImage && !altPressed && m_file_is_open && !gizmoUsing && !gizmoOver) {
        // Check for left mouse button click
        // Use both GLFW and ImGui methods to ensure we catch the click reliably
        bool glfwClick = Mouse::buttonWentDown(GLFW_MOUSE_BUTTON_LEFT);
        bool imguiClick = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        
        if (glfwClick || (imguiClick && !imguiWantsMouse)) {
            // Get mouse position relative to viewport image (actual rendered image position)
            // This uses the cached position from renderViewportWindow() which is the actual image item position
            ImVec2 viewportPos = m_scene.getViewportWindowPos();
            ImVec2 viewportSize = m_scene.getViewportWindowSize();
            
            // Get mouse position from ImGui (screen space coordinates)
            ImVec2 mousePos = io.MousePos;
            
            // Convert to viewport-relative coordinates (relative to the actual image, not window)
            // viewportPos is the actual image position (from GetItemRectMin())
            float viewportMouseX = mousePos.x - viewportPos.x;
            float viewportMouseY = mousePos.y - viewportPos.y;
            
            // Only process if mouse is within viewport bounds
            // viewportSize is the actual image size (from GetItemRectMax() - GetItemRectMin())
            if (viewportMouseX >= 0 && viewportMouseX < viewportSize.x &&
                viewportMouseY >= 0 && viewportMouseY < viewportSize.y) {
                
                // CRITICAL: Use the CONTENT REGION size for projection and raycast
                // This matches the aspect ratio used for rendering, ensuring accurate raycast
                // The framebuffer is resized to match content size in beginViewportRender()
                ImVec2 contentSize = m_scene.getViewportContentSize();  // Actual content region size
                
                // Get far clipping plane from settings (must match rendering)
                AppSettings& settings = AppSettingsManager::getInstance().getSettings();
                float farClipPlane = settings.environment.farClipPlane;
                
                // Delegate selection logic to ViewportSelectionManager
                m_selectionManager.handleSelectionClick(viewportMouseX, viewportMouseY,
                                                        contentSize.x, contentSize.y,
                                                        m_cameras[m_activeCam], farClipPlane,
                                                        m_modelManager, m_scene,
                                                        m_modelRootNodeMatrices);
            }
        }
    }
}

static void glfw_error_callback(int error, const char* description) {
    LOG_ERROR("GLFW Error " + std::to_string(error) + ": " + std::string(description));
}

// GLFW drop callback for drag and drop file support
// Called when files are dropped onto the window
static void glfw_drop_callback(GLFWwindow* window, int count, const char** paths) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app && count > 0) {
        for (int i = 0; i < count; i++) {
            std::string path = paths[i];
            if (path.length() >= 4) {
                std::string ext = path.substr(path.length() - 4);
                // Convert to lowercase for safe comparison
                for (char& c : ext) c = std::tolower(c);
                if (ext == ".fbx") {
                    app->handleDroppedFile(path);
                    break; // Load the first valid FBX file dropped
                }
            }
        }
    }
}
