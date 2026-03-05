#include "scene.h"
#include "keyboard.h"
#include "mouse.h"
#include "app_settings.h"
#include "../utils/logger.h"
#include <imgui/imgui_internal.h>  // For DockBuilder functions
#include "../graphics/grid.h"  // For Grid class
#include "../graphics/model_manager.h"  // For ModelManager class
#include "../graphics/model.h"  // For BoneGlobalTransform
#include "../graphics/math3D.h"  // For matrix conversion utilities (glmMat4ToFloat16, float16ToGlmMat4)
#include "../graphics/utils.h"  // For fbxToGlmMatrix
#include "../graphics/raycast.h"  // For mouse ray intersection
#include <glm/glm.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>  // For glm::eulerAngles

// Legacy window dimensions (updated by framebuffer resize callback)
// NOTE: These are legacy static members for backward compatibility.
// The actual viewport size is tracked dynamically via getViewportContentSize().
unsigned int Scene::SCR_WIDTH =  1920;
unsigned int Scene::SCR_HEIGHT = 1080;

//== callbacks =============================
//-- window resize ----------------------
void Scene::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    //-- update variables ------
    SCR_WIDTH = width;
    SCR_HEIGHT = height;
    
}


//== constructors ===============================
//-- default -----------------------------
Scene::Scene()
    : window(nullptr) {

    mCurrentFile = "";
    // File dialog is now initialized in UIManager constructor

}



void Scene::setParameters()
{
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glfwSetFramebufferSizeCallback(window, Scene::framebufferSizeCallback);

    //-- Initialize auto-focus state from persistent settings ----
    // This ensures m_autoFocusEnabled is initialized before UI is rendered
    AppSettings& settings = AppSettingsManager::getInstance().getSettings();
    m_autoFocusEnabled = settings.environment.autoFocusEnabled;
    
    //-- Initialize FPS counter state from persistent settings ----
    m_showFPS = settings.environment.showFPS;
    
    //-- Initialize viewport settings panel state from persistent settings ----
    m_uiManager.getViewportSettingsPanel().setFpsScale(settings.environment.fpsScale);
    m_uiManager.getViewportSettingsPanel().setSmoothCameraUIState(settings.camera.smoothCameraEnabled);
    m_uiManager.getViewportSettingsPanel().setSmoothCameraSpeed(settings.camera.smoothTransitionSpeed);
    
    //-- Set up outliner selection change callback ----
    // NOTE: Selection change tracking for auto-focus is handled in the main loop
    // (Application::checkAndTriggerAutoFocusOnSelectionChange) after the outliner panel renders.
    // This callback is registered but currently serves as a placeholder for potential future use
    // (e.g., immediate UI updates, visual feedback, or other selection-dependent features).
    // The callback does not trigger framing directly to avoid conflicts with the main loop's
    // selection change detection logic.
    m_uiManager.getOutliner().setSelectionChangeCallback([this]() {
        // Placeholder callback - selection change tracking handled in main loop
        // This can be extended in the future for immediate UI updates or other features
    });

    //-- Setup Dear ImGui context -----
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    
    // Verify docking is enabled (debug check)
    if (!(io.ConfigFlags & ImGuiConfigFlags_DockingEnable)) {
        LOG_WARNING("Docking flag not set correctly!");
    }
   // io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    //-- Setup Dear ImGui style ---
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    //-- When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones. ---
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    //--io controls callback ----------------------
    // CRITICAL: Custom GLFW callbacks must be registered BEFORE ImGui initialization.
    // This allows ImGui to chain them and retains UI keyboard navigation (like Tab key).
    glfwSetKeyCallback(window, Keyboard::keyCallback);
    glfwSetCursorPosCallback(window, Mouse::cursorPosCallback);
    glfwSetMouseButtonCallback(window, Mouse::mouseButtonCallback);
    glfwSetScrollCallback(window, Mouse::mouseWheelCallback);

    //-- imgui --------------------------------
    //-- Setup Platform/Renderer backends -------------------
    const char* glsl_version = "#version 440";
    ImGui_ImplGlfw_InitForOpenGL(window, true); // Now safely chains the custom callbacks
    ImGui_ImplOpenGL3_Init(glsl_version);
    //glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    //-- Initialize background shader for gradient rendering -------------------
    initBackgroundShader();

}


//-- update screen before each frame -----------
void Scene::update() {
   
    //-- Start the Dear ImGui frame ----------
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    //-- Initialize ImGuizmo frame (must be called after ImGui::NewFrame()) ----------
    ImGuizmo::BeginFrame();
    
    //-- Handle gizmo keyboard shortcuts (Maya-style: W/E/R for operation, +/- for size) ----------
    m_gizmoManager.handleKeyboardShortcuts(mModelManager, mouseOverViewport);
    
    //-- Render all editor UI (main menu, dockspace, panels) -------
    // All UI panels and layout are now managed by UIManager
    // Viewport panel is now rendered by UIManager::renderEditorUI
    m_uiManager.renderEditorUI(this);

    //-- Rendering -----------------
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);

    //-- set background color ----------
     //--enable gl depth -----
    glEnable(GL_DEPTH_TEST);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    

    //-- Update and Render additional Platform Windows ---
    //-- (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere ---
    //--  For this specific demo app we could also call glfwMakeContextCurrent(window) directly) --------------------
    ImGuiIO& io_ref = ImGui::GetIO();
    if (io_ref.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }
}
//-- cleanup method -----------
void Scene::cleanup()
{
    // Clean up framebuffer resources
    if (viewportFBO != 0) {
        glDeleteFramebuffers(1, &viewportFBO);
        viewportFBO = 0;
    }
    if (viewportTexture != 0) {
        glDeleteTextures(1, &viewportTexture);
        viewportTexture = 0;
    }
    if (viewportRBO != 0) {
        glDeleteRenderbuffers(1, &viewportRBO);
        viewportRBO = 0;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

//== accessors =============================
//-- determine if window should close ------
bool Scene::shouldClose() {

    return glfwWindowShouldClose(window);

}

//== modifiers ==================
//-- set if the window should close -------------
void Scene::setShouldClose(bool shouldClose) {
    glfwSetWindowShouldClose(window, shouldClose);
}

std::string Scene::getFilePath()
{
    return mCurrentFile;
}

//-- set file path (for loading from recent files) ----
void Scene::setFilePath(const std::string& filepath)
{
    mCurrentFile = filepath;
    mNewFileRequested = true;  // Signal that a new file should be loaded
}

//-- check if a new file was requested ----
bool Scene::isNewFileRequested()
{
    return mNewFileRequested;
}

//-- reset new file request flag ----
void Scene::resetNewFileRequest()
{
    mNewFileRequested = false;
}

//-- clear scene (new scene) ----
void Scene::clearScene()
{
    mCurrentFile = "";
    mClearSceneRequested = true;
}

//-- check if scene should be cleared ----
bool Scene::isClearSceneRequested()
{
    return mClearSceneRequested;
}

//-- reset clear scene request flag ----
void Scene::resetClearSceneRequest()
{
    mClearSceneRequested = false;
}

//-- check if camera framing was requested (auto-focus feature) ----
bool Scene::isCameraFramingRequested()
{
    return m_cameraFramingRequested;
}

//-- reset camera framing request flag ----
void Scene::resetCameraFramingRequest()
{
    m_cameraFramingRequested = false;
}

//-- get recent files for menu ----
std::vector<std::string> Scene::getRecentFiles()
{
    // Forward to settings manager
    return AppSettingsManager::getInstance().getRecentFiles();
}

//-- Initialize background shader and quad geometry -------------------
void Scene::initBackgroundShader() {
        // Initialize shader
        m_backgroundShader = Shader("Assets/background.vert.glsl", "Assets/background.frag.glsl");
        
        // Full-screen quad vertices in NDC coordinates (-1 to 1)
        // Format: x, y (2 floats per vertex)
        float quadVertices[] = {
            -1.0f,  1.0f,  // Top-left
            -1.0f, -1.0f,  // Bottom-left
             1.0f,  1.0f,  // Top-right
             1.0f, -1.0f   // Bottom-right
        };
        
        // Create VAO and VBO
        glGenVertexArrays(1, &m_backgroundVAO);
        glGenBuffers(1, &m_backgroundVBO);
        
        glBindVertexArray(m_backgroundVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_backgroundVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        
        // Set vertex attribute (position: 2 floats)
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        glBindVertexArray(0);
    }

//-- Initialize framebuffer for viewport rendering -------------------
void Scene::createViewportFBO()
{
    // Create framebuffer
    glGenFramebuffers(1, &viewportFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, viewportFBO);
    
    // Create texture for color attachment
    glGenTextures(1, &viewportTexture);
    glBindTexture(GL_TEXTURE_2D, viewportTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, viewportWidth, viewportHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, viewportTexture, 0);
    
    // Create renderbuffer for depth and stencil attachment
    glGenRenderbuffers(1, &viewportRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, viewportRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, viewportWidth, viewportHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, viewportRBO);
    
    // Check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        LOG_ERROR("Framebuffer is not complete!");
    }
    
    // Unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//-- Resize framebuffer when viewport size changes -------------------
void Scene::resizeViewportFBO(int width, int height)
{
    // Robustness check: Ensure dimensions are at least 1px to prevent OpenGL errors
    if (width < 1 || height < 1) {
        return;  // Invalid dimensions, skip resize
    }
    
    viewportWidth = width;
    viewportHeight = height;
    
    if (viewportFBO == 0) {
        createViewportFBO();
        return;
    }
    
    // Resize texture (width and height are guaranteed >= 1 at this point)
    glBindTexture(GL_TEXTURE_2D, viewportTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    
    // Resize renderbuffer (width and height are guaranteed >= 1 at this point)
    glBindRenderbuffer(GL_RENDERBUFFER, viewportRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
}

//-- Begin rendering to viewport framebuffer -------------------
void Scene::beginViewportRender()
{
    if (viewportFBO == 0) {
        createViewportFBO();
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, viewportFBO);
    
    // 2. FORCE glViewport SYNC: Call glViewport every frame right before rendering the scene
    // Use the EXACT screen-space size captured in ViewportPanel::renderPanel()
    // Ensure NO other glViewport calls exist that use window-level dimensions
    int viewportWidth = (int)mViewportScreenSize.x;
    int viewportHeight = (int)mViewportScreenSize.y;
    
    // Fallback to framebuffer size if viewport size not yet captured (first frame)
    if (viewportWidth <= 0 || viewportHeight <= 0) {
        viewportWidth = this->viewportWidth;
        viewportHeight = this->viewportHeight;
    }
    
    // Ensure valid dimensions (avoid division by zero)
    if (viewportWidth <= 0) viewportWidth = 1;
    if (viewportHeight <= 0) viewportHeight = 1;
    
    // CRITICAL: Set glViewport to exact content region size every frame
    // This ensures NDC coordinates (-1 to 1) map correctly to pixels
    // Must be called every frame to handle dynamic resizing
    glViewport(0, 0, viewportWidth, viewportHeight);
    
    // Clear both color and depth buffers first (ensures clean slate)
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Draw background (gradient or solid color) - MUST be done immediately after clear
    // This is a screen-space pass that covers the entire viewport in NDC coordinates
    AppSettings& settings = AppSettingsManager::getInstance().getSettings();
    if (settings.environment.useGradient) {
        // Gradient is drawn as a screen-space pass with depth testing disabled
        // Uses NDC coordinates (-1, -1) to (1, 1) to cover entire viewport
        drawBackgroundGradient();
    }
    // If not using gradient, the solid color clear above is sufficient
    
    // Enable depth testing for subsequent rendering (grid, models, etc.)
    glEnable(GL_DEPTH_TEST);
}

//-- End rendering to viewport framebuffer -------------------
void Scene::endViewportRender()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//-- Get current viewport size -------------------
ImVec2 Scene::getViewportSize()
{
    return ImVec2((float)viewportWidth, (float)viewportHeight);
}

//-- Get viewport content size (actual rendering area) -------------------
ImVec2 Scene::getViewportContentSize() const
{
    // Return actual content region size (for aspect ratio calculation)
    // This is the exact size of the rendering area in the ImGui window
    // Updated every frame in ViewportPanel::renderPanel() via GetContentRegionAvail()
    return cachedViewportSize;
}

//-- Check if mouse is over viewport -------------------
bool Scene::isMouseOverViewport()
{
    return mouseOverViewport;
}

//-- Get viewport window position and size (for mouse coordinate conversion) ----
ImVec2 Scene::getViewportWindowPos() const
{
    // Return cached position (updated during ViewportPanel::renderPanel)
    return cachedViewportPos;
}

ImVec2 Scene::getViewportWindowSize() const
{
    // Return cached size (updated during ViewportPanel::renderPanel)
    return cachedViewportSize;
}

// NOTE: handleGizmoKeyboardShortcuts() and printGizmoTransformValues() have been moved to GizmoManager
// They are no longer part of the Scene class

/**
 * @brief Applies camera aim constraint with one-shot trigger logic and mouse interrupt support.
 * 
 * This function implements a "One-Shot Trigger" pattern for camera framing:
 * - Framing is triggered once when 'F' key is pressed or Auto-Focus detects selection change
 * - The framing flag (m_isFraming) is cleared immediately after the request
 * - This prevents continuous re-triggering and allows manual camera control after framing completes
 * 
 * Mouse Interrupt Feature:
 * - If the right mouse button is pressed, framing is immediately skipped
 * - This ensures manual camera control always takes priority over auto-framing
 * - Provides responsive user experience where manual input overrides automatic behavior
 * 
 * Target Position Calculation:
 * - Primary: Uses gizmo world position if available (most accurate)
 * - Fallback: Calculates from selected node's bounding box center if gizmo not visible
 * - Bounding box is transformed to world space using the model's RootNode transform
 * 
 * Framing Logic:
 * - If bounding box is available: Uses corner-based framing with distance calculation
 * - If no bounding box: Falls back to rotation-only aim (no distance adjustment)
 * - Aspect ratio is calculated from actual viewport content size for accurate framing
 * 
 * @param camera Pointer to the camera to apply the constraint to
 */
void Scene::applyCameraAimConstraint(Camera* camera) {
    // MOUSE INTERRUPT: If right mouse button is pressed, skip framing
    // This ensures manual control always wins
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        return;  // User is manually controlling camera, don't apply constraint
    }
    
    // Only apply constraint if framing flag is set (triggered by 'F' key or Auto-Focus)
    if (!m_isFraming) {
        return;  // Framing not active, don't apply constraint
    }
    
    // CRITICAL: For selection-based framing (auto-focus on selection change),
    // we need to calculate target position even if gizmo is not visible yet
    // If gizmo is not visible, we'll use the selected node's bounding box center as target
    glm::vec3 targetPosition = m_gizmoManager.getGizmoWorldPosition();  // Default to gizmo position if available
    bool useGizmoPosition = m_gizmoManager.isGizmoPositionValid();
    
    // If gizmo is not visible, calculate target from selected node's bounding box
    // CRITICAL: Bounding boxes MUST be calculated for gizmo positioning (even if visibility toggle is off)
    // With bone-based calculation (O(Bones) complexity), this is now ultra-fast
    if (!m_gizmoManager.isGizmoPositionValid() && mModelManager != nullptr) {
        int selectedModelIndex = mModelManager->getSelectedModelIndex();
        
        if (selectedModelIndex >= 0) {
            // OPTIMIZED: Get bone index from PropertyPanel (already calculated, no string lookup needed)
            int boneIndex = m_uiManager.getPropertyPanel().getSelectedBoneIndex();
            
            // Get bounding box for the selected bone using index (ZERO STRING OPERATIONS)
            glm::vec3 boundingBoxMin, boundingBoxMax;
            if (boneIndex >= 0) {
                mModelManager->getNodeBoundingBoxByIndex(selectedModelIndex, boneIndex, boundingBoxMin, boundingBoxMax);
            } else {
                // No bone selected, fallback to all bones
                mModelManager->getModelBoundingBox(selectedModelIndex, boundingBoxMin, boundingBoxMax);
            }
            
            // Check if bounding box is valid
            if (boundingBoxMin != glm::vec3(-1.0f) || boundingBoxMax != glm::vec3(1.0f)) {
                // Calculate center of bounding box in local space
                glm::vec3 localCenter = (boundingBoxMin + boundingBoxMax) * 0.5f;
                
                // Transform to world space using selected model's RootNode transform
                std::string modelRootNodeName = m_uiManager.getOutliner().getRootNodeName(selectedModelIndex);
                if (!modelRootNodeName.empty()) {
                    const auto& allBoneTranslations = m_uiManager.getPropertyPanel().getAllBoneTranslations();
                    const auto& allBoneRotations = m_uiManager.getPropertyPanel().getAllBoneRotations();
                    const auto& allBoneScales = m_uiManager.getPropertyPanel().getAllBoneScales();
                    
                    auto transIt = allBoneTranslations.find(modelRootNodeName);
                    auto rotIt = allBoneRotations.find(modelRootNodeName);
                    auto scaleIt = allBoneScales.find(modelRootNodeName);
                    
                    glm::vec3 rootTranslation = (transIt != allBoneTranslations.end()) ? transIt->second : glm::vec3(0.0f);
                    glm::vec3 rootRotationEuler = (rotIt != allBoneRotations.end()) ? rotIt->second : glm::vec3(0.0f);
                    glm::vec3 rootScale = (scaleIt != allBoneScales.end()) ? scaleIt->second : glm::vec3(1.0f);
                    
                    // Build RootNode transform matrix
                    glm::quat rootRotation = glm::quat(glm::radians(rootRotationEuler));
                    glm::mat4 rootNodeTransform = glm::mat4(1.0f);
                    rootNodeTransform = glm::translate(rootNodeTransform, rootTranslation);
                    rootNodeTransform = rootNodeTransform * glm::mat4_cast(rootRotation);
                    rootNodeTransform = glm::scale(rootNodeTransform, rootScale);
                    
                    // Transform center to world space
                    glm::vec4 localCenter4(localCenter, 1.0f);
                    glm::vec4 worldCenter4 = rootNodeTransform * localCenter4;
                    targetPosition = glm::vec3(worldCenter4.x, worldCenter4.y, worldCenter4.z);
                    useGizmoPosition = false;  // We're using calculated position, not gizmo
                } else {
                    // No RootNode transform, use local center as-is
                    targetPosition = localCenter;
                    useGizmoPosition = false;
                }
            }
        }
    }
    
    // If we still don't have a valid target position, cancel framing
    if (!useGizmoPosition && targetPosition == glm::vec3(0.0f) && !m_gizmoManager.isGizmoPositionValid()) {
        m_isFraming = false;  // No valid target, cancel framing
        return;
    }
    
    // 3. ONE-SHOT LOGIC: Apply aim constraint with framing distance
    // CRITICAL FIX: Use ONLY the selected model index - no loops, no fallbacks
    // This ensures the camera frames the correct character, not the first one imported
    glm::vec3 boundingBoxMin, boundingBoxMax;
    bool hasBoundingBox = false;
    
    // CRITICAL: Bounding boxes MUST be calculated for camera framing (even if visibility toggle is off)
    // With bone-based calculation (O(Bones) complexity), this is now ultra-fast
    // Camera framing must work regardless of bounding box visibility toggle
    if (mModelManager != nullptr) {
        std::string selectedNode = m_uiManager.getOutliner().getSelectedNode();
        
        // CRITICAL: Get the active model index directly - no loops through all models
        // This ensures we use the correct model context for the selected node
        int selectedModelIndex = mModelManager->getSelectedModelIndex();
        
        // Only proceed if we have a valid selection (model index)
        // No fallback to other models - direct logic only
        if (selectedModelIndex >= 0) {
            // OPTIMIZED: Get bone index from PropertyPanel (already calculated, no string lookup needed)
            int boneIndex = m_uiManager.getPropertyPanel().getSelectedBoneIndex();
            
            // Get bounding box for the selectedNode from THIS SPECIFIC model instance only
            // No global search across all models - strict isolation
            // Use index-based version for zero string operations
            if (boneIndex >= 0) {
                mModelManager->getNodeBoundingBoxByIndex(selectedModelIndex, boneIndex, boundingBoxMin, boundingBoxMax);
            } else {
                // No bone selected, fallback to all bones
                mModelManager->getModelBoundingBox(selectedModelIndex, boundingBoxMin, boundingBoxMax);
            }
            
            // Check if bounding box is valid (not default)
            if (boundingBoxMin != glm::vec3(-1.0f) || boundingBoxMax != glm::vec3(1.0f)) {
                hasBoundingBox = true;
                
                // CRITICAL: Transform bounding box to world space using ONLY the selected model's RootNode transform
                // The bounding box is calculated in local space, but the model is rendered in world space
                // with RootNode transform applied. We must transform the bounding box to match.
                std::string modelRootNodeName = m_uiManager.getOutliner().getRootNodeName(selectedModelIndex);
                if (!modelRootNodeName.empty()) {
                    // Get RootNode transform matrix from PropertyPanel for THIS SPECIFIC MODEL
                    const auto& allBoneTranslations = m_uiManager.getPropertyPanel().getAllBoneTranslations();
                    const auto& allBoneRotations = m_uiManager.getPropertyPanel().getAllBoneRotations();
                    const auto& allBoneScales = m_uiManager.getPropertyPanel().getAllBoneScales();
                    
                    auto transIt = allBoneTranslations.find(modelRootNodeName);
                    auto rotIt = allBoneRotations.find(modelRootNodeName);
                    auto scaleIt = allBoneScales.find(modelRootNodeName);
                    
                    glm::vec3 rootTranslation = (transIt != allBoneTranslations.end()) ? transIt->second : glm::vec3(0.0f);
                    glm::vec3 rootRotationEuler = (rotIt != allBoneRotations.end()) ? rotIt->second : glm::vec3(0.0f);
                    glm::vec3 rootScale = (scaleIt != allBoneScales.end()) ? scaleIt->second : glm::vec3(1.0f);
                    
                    // Build RootNode transform matrix for THIS SPECIFIC MODEL
                    glm::quat rootRotation = glm::quat(glm::radians(rootRotationEuler));
                    glm::mat4 rootNodeTransform = glm::mat4(1.0f);
                    rootNodeTransform = glm::translate(rootNodeTransform, rootTranslation);
                    rootNodeTransform = rootNodeTransform * glm::mat4_cast(rootRotation);
                    rootNodeTransform = glm::scale(rootNodeTransform, rootScale);
                    
                    // Transform all 8 corners of the bounding box from local space to world space
                    // Using ONLY the selected model's RootNode transform
                    glm::vec3 corners[8] = {
                        glm::vec3(boundingBoxMin.x, boundingBoxMin.y, boundingBoxMin.z),
                        glm::vec3(boundingBoxMax.x, boundingBoxMin.y, boundingBoxMin.z),
                        glm::vec3(boundingBoxMin.x, boundingBoxMax.y, boundingBoxMin.z),
                        glm::vec3(boundingBoxMax.x, boundingBoxMax.y, boundingBoxMin.z),
                        glm::vec3(boundingBoxMin.x, boundingBoxMin.y, boundingBoxMax.z),
                        glm::vec3(boundingBoxMax.x, boundingBoxMin.y, boundingBoxMax.z),
                        glm::vec3(boundingBoxMin.x, boundingBoxMax.y, boundingBoxMax.z),
                        glm::vec3(boundingBoxMax.x, boundingBoxMax.y, boundingBoxMax.z)
                    };
                    
                    // Transform all corners to world space using the selected model's RootNode transform
                    for (int j = 0; j < 8; j++) {
                        corners[j] = glm::vec3(rootNodeTransform * glm::vec4(corners[j], 1.0f));
                    }
                    
                    // Find new min/max in world space (axis-aligned bounding box in world space)
                    boundingBoxMin = boundingBoxMax = corners[0];
                    for (int j = 1; j < 8; j++) {
                        boundingBoxMin = glm::min(boundingBoxMin, corners[j]);
                        boundingBoxMax = glm::max(boundingBoxMax, corners[j]);
                    }
                }
            }
        }
        // CRITICAL: No fallback to combined bounding box or other models
        // If no valid selection exists, hasBoundingBox remains false and we use rotation-only aim
    }
    
    // Get aspect ratio for corner-based framing calculation
    // CRITICAL: Use viewport content size (actual rendering area) instead of global window size
    // This ensures framing is perfect even when UI panels are open (viewport may be smaller than window)
    ImVec2 viewportContentSize = getViewportContentSize();
    float aspectRatio = 16.0f / 9.0f;  // Default aspect ratio
    if (viewportContentSize.x > 0.0f && viewportContentSize.y > 0.0f) {
        aspectRatio = static_cast<float>(viewportContentSize.x) / static_cast<float>(viewportContentSize.y);
    }
    
    // Request framing ONCE when trigger is set (camera will handle smooth/snap internally)
    // This is only called when m_isFraming is true, and we clear it immediately to prevent re-triggering
    if (hasBoundingBox) {
        // Use corner-based framing distance (smart distance method)
        // Get framing distance multiplier from settings
        AppSettings& settings = AppSettingsManager::getInstance().getSettings();
        float framingMultiplier = settings.environment.framingDistanceMultiplier;
        // Use targetPosition (either gizmo position or calculated from bounding box)
        camera->requestFraming(targetPosition, boundingBoxMin, boundingBoxMax, aspectRatio, framingMultiplier);
    } else {
        // Fallback to rotation-only aim (no distance adjustment)
        // NOTE: This is disabled during animation to prevent overriding interpolated yaw/pitch
        if (!camera->isAnimating()) {
            // Calculate direction to target position and rebuild camera's orthonormal basis
            // This updates cameraFront, yaw, pitch, and focusPoint to match the target position
            // Use targetPosition (either gizmo position or calculated from bounding box)
            camera->aimAtTarget(targetPosition);
        }
    }
    
    // Clear framing flag immediately - camera now handles its own state via m_shouldStartFraming
    // This prevents requestFraming() from being called every frame
    m_isFraming = false;
}

//-- check and trigger auto-focus on selection change ----
// Returns true if framing was triggered, false otherwise
bool Scene::checkAndTriggerAutoFocusOnSelectionChange() {
    AppSettings& settings = AppSettingsManager::getInstance().getSettings();
    if (!settings.environment.autoFocusEnabled) {
        return false;  // Auto-focus not enabled
    }
    
    if (mModelManager == nullptr) {
        return false;  // No model manager
    }
    
    // Get current selection
    int currentModelIndex = mModelManager->getSelectedModelIndex();
    std::string currentSelectedNode = m_uiManager.getOutliner().getSelectedNode();
    
    // Only trigger if selection changed (different model index or node name)
    if (currentModelIndex != mLastSelectedModelIndex || 
        currentSelectedNode != mLastSelectedNode) {
        setFramingTrigger(true);
        // Update tracking variables
        mLastSelectedModelIndex = currentModelIndex;
        mLastSelectedNode = currentSelectedNode;
        return true;  // Framing was triggered
    }
    
    return false;  // Selection didn't change, no framing triggered
}

/**
 * @brief Updates camera framing distance only (distance-only update during slider drag).
 * 
 * This function provides smooth camera dolly (distance adjustment) during slider interaction
 * without re-orienting the camera. This prevents camera jumps and provides a smooth UX.
 * 
 * Key Features:
 * - Only active when framing distance slider is being dragged
 * - Only updates if gizmo position is valid (ensures valid target)
 * - Calculates distance from bounding box size in world space
 * - Updates camera distance along current view direction (no yaw/pitch changes)
 * 
 * Bounding Box Calculation:
 * - Uses bone-aware bounding box calculation (O(Bones) complexity - ultra-fast)
 * - Transforms bounding box from local space to world space using RootNode transform
 * - Works regardless of bounding box visibility toggle (needed for distance calculation)
 * 
 * Distance Calculation:
 * - Base distance calculated from bounding box size and camera FOV
 * - Applies framing distance multiplier from settings
 * - Clamps to reasonable values (0.1 to 10000.0 units)
 * 
 * @param camera Pointer to the camera to update
 */
void Scene::updateFramingDistanceOnly(Camera* camera) {
    // Only update if slider is active and gizmo position is valid
    if (!m_uiManager.getViewportSettingsPanel().isFramingDistanceSliderActive() || !m_gizmoManager.isGizmoPositionValid()) {
        return;
    }
    
    // CRITICAL: Bounding boxes MUST be calculated for framing distance (even if visibility toggle is off)
    // With bone-based calculation (O(Bones) complexity), this is now ultra-fast
    // Framing distance slider must work regardless of bounding box visibility toggle
    if (mModelManager == nullptr) {
        return;
    }
    
    // Get bounding box for distance calculation
    glm::vec3 boundingBoxMin, boundingBoxMax;
    bool hasBoundingBox = false;
    
    int selectedModelIndex = mModelManager->getSelectedModelIndex();
    
    if (selectedModelIndex >= 0) {
        // OPTIMIZED: Get bone index from PropertyPanel (already calculated, no string lookup needed)
        int boneIndex = m_uiManager.getPropertyPanel().getSelectedBoneIndex();
        
        // Use index-based version for zero string operations
        if (boneIndex >= 0) {
            mModelManager->getNodeBoundingBoxByIndex(selectedModelIndex, boneIndex, boundingBoxMin, boundingBoxMax);
        } else {
            // No bone selected, fallback to all bones
            mModelManager->getModelBoundingBox(selectedModelIndex, boundingBoxMin, boundingBoxMax);
        }
        if (boundingBoxMin != glm::vec3(-1.0f) || boundingBoxMax != glm::vec3(1.0f)) {
            hasBoundingBox = true;
            
            // CRITICAL FIX: Transform bounding box to world space using this model's RootNode transform
            // The bounding box is calculated in local space, but the model is rendered in world space
            // with RootNode transform applied. We must transform the bounding box to match.
            std::string modelRootNodeName = m_uiManager.getOutliner().getRootNodeName(selectedModelIndex);
            if (!modelRootNodeName.empty()) {
                // Get RootNode transform matrix from PropertyPanel
                const auto& allBoneTranslations = m_uiManager.getPropertyPanel().getAllBoneTranslations();
                const auto& allBoneRotations = m_uiManager.getPropertyPanel().getAllBoneRotations();
                const auto& allBoneScales = m_uiManager.getPropertyPanel().getAllBoneScales();
                
                auto transIt = allBoneTranslations.find(modelRootNodeName);
                auto rotIt = allBoneRotations.find(modelRootNodeName);
                auto scaleIt = allBoneScales.find(modelRootNodeName);
                
                glm::vec3 rootTranslation = (transIt != allBoneTranslations.end()) ? transIt->second : glm::vec3(0.0f);
                glm::vec3 rootRotationEuler = (rotIt != allBoneRotations.end()) ? rotIt->second : glm::vec3(0.0f);
                glm::vec3 rootScale = (scaleIt != allBoneScales.end()) ? scaleIt->second : glm::vec3(1.0f);
                
                // Build RootNode transform matrix
                glm::quat rootRotation = glm::quat(glm::radians(rootRotationEuler));
                glm::mat4 rootNodeTransform = glm::mat4(1.0f);
                rootNodeTransform = glm::translate(rootNodeTransform, rootTranslation);
                rootNodeTransform = rootNodeTransform * glm::mat4_cast(rootRotation);
                rootNodeTransform = glm::scale(rootNodeTransform, rootScale);
                
                // Transform all 8 corners of the bounding box from local space to world space
                glm::vec3 corners[8] = {
                    glm::vec3(boundingBoxMin.x, boundingBoxMin.y, boundingBoxMin.z),
                    glm::vec3(boundingBoxMax.x, boundingBoxMin.y, boundingBoxMin.z),
                    glm::vec3(boundingBoxMin.x, boundingBoxMax.y, boundingBoxMin.z),
                    glm::vec3(boundingBoxMax.x, boundingBoxMax.y, boundingBoxMin.z),
                    glm::vec3(boundingBoxMin.x, boundingBoxMin.y, boundingBoxMax.z),
                    glm::vec3(boundingBoxMax.x, boundingBoxMin.y, boundingBoxMax.z),
                    glm::vec3(boundingBoxMin.x, boundingBoxMax.y, boundingBoxMax.z),
                    glm::vec3(boundingBoxMax.x, boundingBoxMax.y, boundingBoxMax.z)
                };
                
                // Transform all corners to world space using RootNode transform
                for (int j = 0; j < 8; j++) {
                    corners[j] = glm::vec3(rootNodeTransform * glm::vec4(corners[j], 1.0f));
                }
                
                // Find new min/max in world space (axis-aligned bounding box in world space)
                boundingBoxMin = boundingBoxMax = corners[0];
                for (int j = 1; j < 8; j++) {
                    boundingBoxMin = glm::min(boundingBoxMin, corners[j]);
                    boundingBoxMax = glm::max(boundingBoxMax, corners[j]);
                }
            }
        }
    }
    
    if (!hasBoundingBox) {
        return;  // No bounding box available, can't calculate distance
    }
    
    // CRITICAL FIX: Calculate center of bounding box in WORLD SPACE (not local space)
    // This ensures the camera stays focused on the character's actual world position, not world origin
    glm::vec3 characterCenter = (boundingBoxMin + boundingBoxMax) * 0.5f;
    
    // Calculate base distance from bounding box size (now in world space)
    float worldHeight = boundingBoxMax.y - boundingBoxMin.y;
    float worldWidth = boundingBoxMax.x - boundingBoxMin.x;
    float worldDepth = boundingBoxMax.z - boundingBoxMin.z;
    float objectSize = glm::max(glm::max(worldHeight, worldWidth), worldDepth);
    
    // Calculate base distance using FOV
    float fovRadians = glm::radians(camera->zoom);
    float baseDist = (objectSize / 2.0f) / tan(fovRadians / 2.0f);
    
    // Apply current framing distance multiplier from settings
    AppSettings& settings = AppSettingsManager::getInstance().getSettings();
    float newDistance = baseDist * settings.environment.framingDistanceMultiplier;
    
    // Clamp distance to reasonable values
    if (newDistance < 0.1f) {
        newDistance = 0.1f;
    }
    if (newDistance > 10000.0f) {
        newDistance = 10000.0f;
    }
    
    // Update camera distance only (no re-orientation)
    // Uses characterCenter (world-space position) instead of world origin
    camera->updateFramingDistanceOnly(characterCenter, newDistance);
}

/**
 * @brief Draws a gradient background covering the entire viewport.
 * 
 * This function renders a full-screen gradient using a dedicated shader.
 * The gradient transitions from the top color to the bottom color vertically
 * across the viewport using a screen-space pass.
 * 
 * Implementation:
 * - Uses a dedicated background shader with vertex and fragment stages
 * - Rendered as a screen-space pass with depth testing and depth writing disabled
 * - Uses NDC coordinates (-1 to 1) to cover entire viewport
 * - Full-screen quad rendered using GL_TRIANGLE_STRIP
 * - Proper OpenGL state management prevents corruption of grid and models
 * 
 * The gradient colors are retrieved from AppSettings and can be customized
 * via the Viewport Settings panel.
 */
void Scene::drawBackgroundGradient() {
    AppSettings& settings = AppSettingsManager::getInstance().getSettings();
    glm::vec3 topColor = settings.environment.viewportGradientTop;
    glm::vec3 bottomColor = settings.environment.viewportGradientBottom;
    
    // Proper OpenGL state management to prevent rendering corruption
    // Disable depth testing and depth writing so background is always behind everything
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);  // Don't write to depth buffer
    
    // Use background shader
    m_backgroundShader.activate();
    
    // Set gradient colors
    m_backgroundShader.set3Float("topColor", topColor);
    m_backgroundShader.set3Float("bottomColor", bottomColor);
    
    // Draw full-screen quad
    glBindVertexArray(m_backgroundVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    
    // Re-enable depth writing and depth testing for subsequent rendering (grid, models, etc.)
    glDepthMask(GL_TRUE);   // Re-enable depth writing
    glEnable(GL_DEPTH_TEST);  // Re-enable depth testing
}
