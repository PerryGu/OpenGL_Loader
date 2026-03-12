#include "viewport_panel.h"
#include "../scene.h"
#include "ui_manager.h"
#include "../../graphics/grid.h"
#include "../../graphics/model_manager.h"
#include "../app_settings.h"
#include <imgui/imgui.h>

ViewportPanel::ViewportPanel() {
    // Constructor - no initialization needed
}

void ViewportPanel::renderPanel(Scene* scene) {
    if (!scene->getUIManager().isViewportWindowVisible()) {
        scene->setViewportState(false, false, ImVec2(0, 0), ImVec2(0, 0));
        return;
    }
    
    // Create viewport window with menu bar flag
    // IMPORTANT: Check if Begin() returns false (window collapsed/closed) and return early
    // Note: show_viewport is now managed by UIManager, but we need a local reference for ImGui::Begin
    // ImGui::Begin needs a pointer to update the visibility flag when the user closes the window
    // Use NoBringToFrontOnFocus to prevent viewport from jumping to front and hiding floating windows
    bool show_viewport = scene->getUIManager().isViewportWindowVisible();
    ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (!ImGui::Begin("Viewport", &show_viewport, flags)) {
        // Update UIManager's visibility flag if window was closed
        scene->getUIManager().setViewportWindowVisible(show_viewport);
        ImGui::End();
        scene->setViewportState(false, false, ImVec2(0, 0), ImVec2(0, 0));
        return;
    }
    
    // Add Tools menu to viewport window's menu bar
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Tools"))
        {
            // Grid visibility toggle
            Grid* grid = scene->getGrid();
            if (grid != nullptr) {
                bool gridEnabled = grid->isEnabled();
                if (ImGui::Checkbox("Show Grid", &gridEnabled)) {
                    grid->setEnabled(gridEnabled);
                    // Save to settings
                    AppSettings& settings = AppSettingsManager::getInstance().getSettings();
                    settings.grid.enabled = gridEnabled;
                    AppSettingsManager::getInstance().markDirty();
                }
                
                // Bounding Box visibility toggle (master switch - strict one-way broadcaster)
                ModelManager* modelManager = scene->getModelManager();
                if (modelManager != nullptr) {
                    // Use stored global state (NOT calculated from individual models)
                    // This ensures strict one-way control: global toggle is broadcaster only
                    bool globalDrawBoundingBoxes = modelManager->getBoundingBoxesEnabled();
                    
                    if (ImGui::Checkbox("Show Bounding Boxes", &globalDrawBoundingBoxes)) {
                        // Strict broadcaster: override all models to match the new master state
                        for (size_t i = 0; i < modelManager->getModelCount(); i++) {
                            ModelInstance* instance = modelManager->getModel(static_cast<int>(i));
                            if (instance) {
                                instance->model.m_showBoundingBox = globalDrawBoundingBoxes;
                            }
                        }
                        
                        // Update stored global state and legacy boundingBox.setEnabled() for compatibility
                        modelManager->setBoundingBoxesEnabled(globalDrawBoundingBoxes);
                        
                        // Save to settings
                        AppSettings& settings = AppSettingsManager::getInstance().getSettings();
                        settings.environment.boundingBoxesEnabled = globalDrawBoundingBoxes;
                        AppSettingsManager::getInstance().markDirty();
                    }
                    
                    // Show Skeletons visibility toggle (master switch - strict one-way broadcaster)
                    // Use stored global state (NOT calculated from individual models)
                    // This ensures strict one-way control: global toggle is broadcaster only
                    bool globalShowSkeletons = modelManager->getSkeletonsEnabled();
                    
                    if (ImGui::Checkbox("Show Skeletons", &globalShowSkeletons)) {
                        // Strict broadcaster: override all models to match the new master state
                        for (size_t i = 0; i < modelManager->getModelCount(); i++) {
                            ModelInstance* instance = modelManager->getModel(static_cast<int>(i));
                            if (instance) {
                                instance->model.m_showSkeleton = globalShowSkeletons;
                            }
                        }
                        
                        // Update stored global state
                        modelManager->setSkeletonsEnabled(globalShowSkeletons);
                        
                        // Save to settings
                        AppSettings& settings = AppSettingsManager::getInstance().getSettings();
                        settings.environment.skeletonsEnabled = globalShowSkeletons;
                        AppSettingsManager::getInstance().markDirty();
                    }
                }
                
                // Simple FPS counter toggle
                bool showFPS = scene->getShowFPS();
                if (ImGui::Checkbox("Show FPS Counter", &showFPS)) {
                    // Checkbox state changed - update settings and save
                    scene->setShowFPS(showFPS);
                    AppSettings& settings = AppSettingsManager::getInstance().getSettings();
                    settings.environment.showFPS = showFPS;
                    AppSettingsManager::getInstance().markDirty();
                }
                
                // Separator above Viewport Settings
                ImGui::Separator();
                
                // Auto Focus toggle (automatically frame camera when gizmo is released)
                // Get current state from settings and sync to local variable for UI
                AppSettings& settings = AppSettingsManager::getInstance().getSettings();
                bool autoFocusEnabled = scene->getAutoFocusEnabled();
                if (ImGui::MenuItem("Auto Focus", NULL, &autoFocusEnabled)) {
                    // Checkbox state changed - update settings and save
                    scene->setAutoFocusEnabled(autoFocusEnabled);
                    settings.environment.autoFocusEnabled = autoFocusEnabled;
                    AppSettingsManager::getInstance().markDirty();
                }
                
                // Viewport Settings menu item (combined Grid Options + Environment Color)
                if (ImGui::MenuItem("Viewport Settings")) {
                    scene->getUIManager().getViewportSettingsPanel().setWindowVisible(true);  // Open Viewport Settings window
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    
    // Check if mouse is over the viewport window
    bool mouseOverViewport = ImGui::IsWindowHovered();
    
    // 1. DYNAMIC VIEWPORT CAPTURE: Capture EXACT screen-space coordinates
    // Inside the ImGui "Viewport" window code, capture the EXACT screen-space coordinates
    ImVec2 pos = ImGui::GetCursorScreenPos();  // Screen-space position of content region
    ImVec2 size = ImGui::GetContentRegionAvail();  // Exact size of content region
    
    // Cache for use in main loop (for mouse coordinate conversion and rendering)
    ImVec2 cachedViewportPos = pos;
    ImVec2 cachedViewportSize = size;
    
    // Handle mouse clicks for object selection (only when not dragging with Alt)
    // This will be processed in the main loop after ImGui updates
    // We just track that the viewport is hovered here
    
    // Get the available content region size
    ImVec2 contentSize = cachedViewportSize;
    
    // Resize framebuffer if window size changed
    int newWidth = (int)contentSize.x;
    int newHeight = (int)contentSize.y;
    int currentWidth = scene->getViewportWidth();
    int currentHeight = scene->getViewportHeight();
    if (newWidth != currentWidth || newHeight != currentHeight) {
        scene->resizeViewportFBO(newWidth, newHeight);
    }
    
    // Display the texture
    unsigned int viewportTexture = scene->getViewportTexture();
    bool mouseOverViewportImage = false;
    if (viewportTexture != 0) {
        ImGui::Image((void*)(intptr_t)viewportTexture, contentSize, ImVec2(0, 1), ImVec2(1, 0));
        
        // CRITICAL: Cache the actual image item position and size (not just window position)
        // This ensures mouse coordinates are correctly converted for raycasting
        // The image might have padding or be positioned differently than the window
        // Must be called immediately after ImGui::Image() to get the correct item rect
        ImVec2 imageMin = ImGui::GetItemRectMin();
        ImVec2 imageMax = ImGui::GetItemRectMax();
        cachedViewportPos = imageMin;  // Use actual image position, not window position
        cachedViewportSize = ImVec2(imageMax.x - imageMin.x, imageMax.y - imageMin.y);
        
        // Debug: Log viewport position and size to help diagnose coordinate issues
        // (Can be removed after fixing)
        // std::cout << "[Viewport] Image pos: (" << imageMin.x << ", " << imageMin.y 
        //           << "), size: (" << cachedViewportSize.x << ", " << cachedViewportSize.y << ")" << std::endl;
        
        // Check if mouse is over the actual image (not just the window)
        // This is important because the window might be hovered but mouse might be over menu bar
        mouseOverViewportImage = ImGui::IsItemHovered();
        
        // Pass hover state to Outliner so arrow keys work in the viewport
        scene->getUIManager().getOutliner().setViewportHovered(mouseOverViewportImage);
        
        // Render ImGuizmo gizmo if a node is selected in the outliner
        // The gizmo now appears for any selected node (rig root or bone nodes)
        // CRITICAL: Strictly use getSelectedModelIndex() to ensure model isolation
        // Delegate all gizmo rendering logic to GizmoManager
        scene->getGizmoManager().renderGizmo(
            scene->getModelManager(), 
            scene->getUIManager().getOutliner(), 
            scene->getUIManager().getPropertyPanel(), 
            scene->getViewMatrix(), 
            scene->getProjectionMatrix(), 
            cachedViewportPos, 
            cachedViewportSize, 
            scene->getCameraFramingRequested()
        );
    } else {
        ImGui::Text("Viewport not initialized");
        mouseOverViewportImage = false;
    }
    
    // Update viewport state in Scene (mouse hover, position, size)
    scene->setViewportState(mouseOverViewport, mouseOverViewportImage, cachedViewportPos, cachedViewportSize);
    
    //-- Show simple FPS counter if enabled (inside viewport window) ---
    if (scene->getShowFPS()) {
        renderSimpleFPS(scene);
    }
    
    ImGui::End();
    
    // Sync visibility flag back to UIManager (in case user closed the window)
    scene->getUIManager().setViewportWindowVisible(show_viewport);
}

void ViewportPanel::renderSimpleFPS(Scene* scene) {
    // Get the viewport window's position and content region
    // This ensures the FPS counter moves with the viewport window
    ImVec2 viewportWindowPos = ImGui::GetWindowPos();
    ImVec2 viewportContentMin = ImGui::GetWindowContentRegionMin();
    ImVec2 viewportContentMax = ImGui::GetWindowContentRegionMax();
    
    // Position at top-right corner of viewport content area, offset by 10 pixels
    // Use content region min for Y to account for menu bar, and content max for X to get right edge
    ImVec2 fpsPos = ImVec2(
        viewportWindowPos.x + viewportContentMax.x - 10,
        viewportWindowPos.y + viewportContentMin.y + 10
    );
    
    ImGui::SetNextWindowPos(fpsPos, ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    
    // Create a tiny, transparent window with no decoration, no inputs, auto-resize, and no background
    // CRITICAL: Check if Begin() returns false (window collapsed/closed) and return early
    if (!ImGui::Begin("FPS", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground)) {
        ImGui::End();
        return;
    }
    
    // Apply scale to FPS counter text (only affects this window)
    ImGui::SetWindowFontScale(scene->getUIManager().getViewportSettingsPanel().getFpsScale());
    
    // --- FPS Smoothing Logic ---
    static float displayedFPS = 0.0f;
    static float lastUpdateTime = 0.0f;
    float currentTime = (float)ImGui::GetTime();
    
    // Update the displayed value only twice a second (every 0.5s)
    if (currentTime - lastUpdateTime > 0.5f || lastUpdateTime == 0.0f) {
        displayedFPS = ImGui::GetIO().Framerate;
        lastUpdateTime = currentTime;
    }
    
    // Format FPS text with "FPS: " prefix
    char fpsText[32];
    sprintf_s(fpsText, sizeof(fpsText), "FPS: %.0f", displayedFPS);
    
    // Calculate text size for background rectangle
    ImVec2 textSize = ImGui::CalcTextSize(fpsText);
    
    // Get current cursor position (where text will be drawn)
    ImVec2 textPos = ImGui::GetCursorScreenPos();
    
    // Calculate background rectangle with padding
    const float padding = 4.0f;  // Padding in pixels
    ImVec2 rectMin = ImVec2(textPos.x - padding, textPos.y - padding);
    ImVec2 rectMax = ImVec2(textPos.x + textSize.x + padding, textPos.y + textSize.y + padding);
    
    // Draw semi-transparent dark background rectangle for better readability
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(rectMin, rectMax, IM_COL32(0, 0, 0, 180), 0.0f);  // Semi-transparent black (alpha = 180/255)
    
    // Display FPS text in green over the background
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", fpsText);
    
    ImGui::End();
}
