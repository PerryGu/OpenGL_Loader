#ifndef SCENE_H
#define SCENE_H


// CRITICAL: Include glad.h FIRST before any ImGui OpenGL implementation headers
// This prevents OpenGL header conflicts (glad.h must be included before any OpenGL headers)
#if defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>
#endif

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/ImFileBrowser.h>
#include <ImGuizmo/ImGuizmo.h>

#include <vector>
#include <map>
#include <string>
#include <inttypes.h>
#include <iostream>

#include "camera.h"
#include "keyboard.h"
#include "mouse.h"

#include "../graphics/shader.h"
#include "../io/ui/gizmo_manager.h"
#include "../io/ui/ui_manager.h"

// Forward declaration for Grid (to avoid circular includes)
class Grid;

class Scene {
public:

    // Legacy window dimensions (updated by framebuffer resize callback)
    // NOTE: These are legacy static members for backward compatibility.
    // The actual viewport size is tracked dynamically via getViewportContentSize().
    static unsigned int SCR_WIDTH;
    static unsigned int SCR_HEIGHT;


    //== callbacks ================
    //-- window resize -------
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);


    //== constructors ============
    //-- default -----
    Scene() ;
 

    
    //== initialization ============
    //-- to be called after constructor --------
    bool init(GLFWwindow* existingWindow) {
        if (existingWindow == nullptr) {
            std::cout << "Scene::init() called with null window pointer." << std::endl;
            return false;
        }
        window = existingWindow;
        return true;
    }

    //**********************************************
    void setParameters();
    void update();

    //-- called after main loop -----
    void cleanup();
     
    //== accessors ===========================
    //-- determine if window should close ----
    bool shouldClose();

    //-- set if the window should close ----------
    void setShouldClose(bool shouldClose);

    std::string getFilePath();
    
    //-- set file path (for loading from recent files) ----
    void setFilePath(const std::string& filepath);
    
    //-- get recent files for menu ----
    std::vector<std::string> getRecentFiles();
    
    //-- check if a new file was requested (from recent files menu) ----
    bool isNewFileRequested();
    
    //-- reset new file request flag (called after file is loaded) ----
    void resetNewFileRequest();
    
    //-- clear scene (new scene) ----
    void clearScene();
    
    //-- check if scene should be cleared ----
    bool isClearSceneRequested();
    
    //-- reset clear scene request flag ----
    void resetClearSceneRequest();
    
    //-- check if camera framing was requested (auto-focus feature) ----
    bool isCameraFramingRequested();
    
    //-- reset camera framing request flag ----
    void resetCameraFramingRequest();
    
    //-- check if framing distance slider is currently active (being dragged) ----
    bool isFramingDistanceSliderActive() const { return m_uiManager.getViewportSettingsPanel().isFramingDistanceSliderActive(); }
    bool getSmoothCameraUIState() const { return m_uiManager.getViewportSettingsPanel().getSmoothCameraUIState(); }
    float getSmoothCameraSpeed() const { return m_uiManager.getViewportSettingsPanel().getSmoothCameraSpeed(); }
    void setFramingTrigger(bool trigger) { m_isFraming = trigger; }  // Set framing trigger (called by 'F' key or Auto-Focus)
    
    //-- selection change tracking (for auto-focus) ----
    // Check if selection changed and trigger auto-focus if enabled
    // Returns true if framing was triggered, false otherwise
    bool checkAndTriggerAutoFocusOnSelectionChange();
    
    // Reset selection tracking (called on deselection)
    void resetSelectionTracking() { mLastSelectedModelIndex = -1; mLastSelectedNode = ""; }
    
    //-- viewport rendering -------------------
    void beginViewportRender();   // Start rendering to the framebuffer
    void endViewportRender();     // End rendering to the framebuffer
    ImVec2 getViewportSize();     // Get current viewport size (framebuffer size)
    ImVec2 getViewportContentSize() const;  // Get actual content region size (for aspect ratio)
    bool isMouseOverViewport();   // Check if mouse is currently over the viewport window
    
    //-- Check if mouse is over viewport image (not just window) ----
    bool isMouseOverViewportImage() const { return mouseOverViewportImage; }
    
    //-- Check if ImGuizmo is currently using the mouse (for preventing selection conflicts) ----
    bool isGizmoUsing() const { return m_gizmoManager.isGizmoUsing(); }
    
    //-- Check if mouse is over ImGuizmo gizmo (for preventing selection conflicts) ----
    bool isGizmoOver() const { return m_gizmoManager.isGizmoOver(); }
    
    //-- Get viewport window position and size (for mouse coordinate conversion) ----
    ImVec2 getViewportWindowPos() const;
    ImVec2 getViewportWindowSize() const;
    
    //-- get window pointer (for window position/size operations) ----
    GLFWwindow* getWindow() { return window; }
    
    //-- set grid reference (for Tools menu control) ----
    void setGrid(Grid* grid) { mGrid = grid; }
    
    //-- set model manager reference (for Tools menu control) ----
    void setModelManager(class ModelManager* modelManager) { mModelManager = modelManager; }
    
    //-- Getters for UIManager to access Scene state -------------------
    ModelManager* getModelManager() { return mModelManager; }
    Grid* getGrid() { return mGrid; }
    Camera& getCamera() { return mCamera; }
    GizmoManager& getGizmoManager() { return m_gizmoManager; }
    ImVec4& getClearColor() { return clear_color; }
    bool& getCameraFramingRequested() { return m_cameraFramingRequested; }
    UIManager& getUIManager() { return m_uiManager; }
    
    //-- Getters for ViewportPanel to access rendering data -------------------
    unsigned int getViewportTexture() const { return viewportTexture; }
    const glm::mat4& getViewMatrix() const { return mViewMatrix; }
    const glm::mat4& getProjectionMatrix() const { return mProjectionMatrix; }
    int getViewportWidth() const { return viewportWidth; }
    int getViewportHeight() const { return viewportHeight; }
    
    //-- Setter to update viewport state from UI -------------------
    void setViewportState(bool overWindow, bool overImage, ImVec2 pos, ImVec2 size) {
        mouseOverViewport = overWindow;
        mouseOverViewportImage = overImage;
        cachedViewportPos = pos;
        cachedViewportSize = size;
        mViewportScreenPos = pos;
        mViewportScreenSize = size;
    }
    
    //-- Getters for UI state (used by UIManager for settings sync) -------------------
    bool getAutoFocusEnabled() const { return m_autoFocusEnabled; }
    bool getShowFPS() const { return m_showFPS; }
    void setAutoFocusEnabled(bool enabled) { m_autoFocusEnabled = enabled; }
    void setShowFPS(bool show) { m_showFPS = show; }
    
    //-- set view and projection matrices for gizmo rendering -------------------
    void setViewProjectionMatrices(const glm::mat4& view, const glm::mat4& projection) {
        mViewMatrix = view;
        mProjectionMatrix = projection;
    }
    
    
    //-- set background color (for environment settings) ----
    void setBackgroundColor(const glm::vec4& color) { clear_color = ImVec4(color.r, color.g, color.b, color.a); }
    
    //-- get background color (for saving settings) ----
    glm::vec4 getBackgroundColor() const { return glm::vec4(clear_color.x, clear_color.y, clear_color.z, clear_color.w); }
    
    //-- camera aim constraint (forces camera to look at gizmo) -------------------
    void applyCameraAimConstraint(Camera* camera);  // Apply camera aim constraint if gizmo is visible
    
    //-- update framing distance only (distance-only update during slider drag) ----
    void updateFramingDistanceOnly(Camera* camera);  // Update camera distance along current view direction (no re-orientation)
    
    //-- framebuffer management (public for ViewportPanel) -------------------
    void resizeViewportFBO(int width, int height);  // Resize the framebuffer
    
    //-- background rendering -------------------
    void drawBackgroundGradient();  // Draw gradient background using settings

private:
    //-- window object ----
    GLFWwindow* window;
    Camera mCamera;

    //-- window vals ------
    const char* title;

    float bg[4]; // background color

    //-- GLFW info -----
    int glfwVersionMajor;
    int glfwVersionMinor;

    //-- imgui stuff ---------------------
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    std::string mCurrentFile;
    bool mNewFileRequested = false;  // Flag to signal new file should be loaded
    bool mClearSceneRequested = false;  // Flag to signal scene should be cleared
    bool m_cameraFramingRequested = false;  // Flag to signal camera should frame (auto-focus feature)
    
    //-- framebuffer for viewport rendering -------------------
    unsigned int viewportFBO = 0;        // Framebuffer object
    unsigned int viewportTexture = 0;    // Color texture
    unsigned int viewportRBO = 0;       // Renderbuffer for depth/stencil
    int viewportWidth = 1920;
    int viewportHeight = 1080;
    bool mouseOverViewport = false;     // Track if mouse is over viewport window
    bool mouseOverViewportImage = false; // Track if mouse is over the viewport image (not just window)
    ImVec2 cachedViewportPos = ImVec2(0, 0);  // Cached viewport window position (updated during render)
    ImVec2 cachedViewportSize = ImVec2(0, 0); // Cached viewport window size (updated during render)
    
    //-- grid reference (for Tools menu) -------------------
    Grid* mGrid = nullptr;
    
    //-- model manager reference (for Tools menu control) -------------------
    class ModelManager* mModelManager = nullptr;
    
    //-- view and projection matrices for gizmo rendering -------------------
    glm::mat4 mViewMatrix = glm::mat4(1.0f);
    glm::mat4 mProjectionMatrix = glm::mat4(1.0f);
    
    //-- Gizmo manager (handles all ImGuizmo logic) -------------------
    GizmoManager m_gizmoManager;
    
    //-- UI manager (handles all editor UI panels and layout) -------------------
    UIManager m_uiManager;
    
    //-- auto-focus feature (automatically frame camera when gizmo is released) -------------------
    bool m_autoFocusEnabled = false;  // True if auto-focus is enabled (checkbox in Tools menu)
    
    //-- Simple FPS counter toggle -------------------
    bool m_showFPS = false;  // True if simple FPS counter should be shown (checkbox in Tools menu)
    
    bool m_isFraming = false;  // Flag to trigger camera framing (set by 'F' key or Auto-Focus)
    
    //-- selection change tracking (for auto-focus) ----
    // Track last selection to prevent infinite loop/flickering when auto-focus is enabled
    int mLastSelectedModelIndex = -1;  // Last selected model index
    std::string mLastSelectedNode = "";  // Last selected node name
    
    //-- viewport screen-space coordinates (for dynamic viewport sync) -------------------
    ImVec2 mViewportScreenPos = ImVec2(0, 0);  // Exact screen-space position of viewport content
    ImVec2 mViewportScreenSize = ImVec2(0, 0);  // Exact screen-space size of viewport content
    
    //-- helper methods for framebuffer -------------------
    void createViewportFBO();            // Create the framebuffer
    
    //-- background shader and geometry -------------------
    Shader m_backgroundShader;           // Shader for gradient background
    unsigned int m_backgroundVAO;        // VAO for full-screen quad
    unsigned int m_backgroundVBO;        // VBO for full-screen quad
    void initBackgroundShader();         // Initialize background shader and quad geometry
    
   

    
    
};

#endif