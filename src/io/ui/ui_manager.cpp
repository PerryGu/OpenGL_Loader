#include "ui_manager.h"
#include "../scene.h"
#include "../../graphics/model_manager.h"
#include "../../graphics/grid.h"
#include "../../graphics/model.h"
#include "../app_settings.h"
#include "../keyboard.h"
#include "../../utils/logger.h"
#include <GLFW/glfw3.h>
#include <filesystem>

UIManager::UIManager() {
    // Initialize file dialog
    mFileDialog.SetTitle("Open mesh");
    mFileDialog.SetFileFilters({ ".fbx", ".obj" });
    
    // Initialize default visibility states
    show_property_panel = true;
    show_outliner = true;
    mShow_timeline = true;
    show_viewport = true;
}

void UIManager::renderEditorUI(Scene* scene) {
    // Show main menu bar
    ShowAppMainMenuBar(scene);
    
    // Show dockspace
    appDockSpace(nullptr);
    
    // Render viewport panel
    mViewportPanel.renderPanel(scene);
    
    // Render Properties window with file dialog
    if (show_property_panel) {
        // Window flags for Properties window
        static bool no_titlebar = false;
        static bool no_scrollbar = false;
        static bool no_menu = false;
        static bool no_move = false;
        static bool no_resize = false;
        static bool no_collapse = false;
        static bool no_close = false;
        static bool no_nav = false;
        static bool no_background = false;
        static bool no_bring_to_front = false;
        static bool no_docking = false;
        static bool unsaved_document = false;

        ImGuiWindowFlags window_flags = 0;
        if (no_titlebar)        window_flags |= ImGuiWindowFlags_NoTitleBar;
        if (no_scrollbar)       window_flags |= ImGuiWindowFlags_NoScrollbar;
        if (!no_menu)           window_flags |= ImGuiWindowFlags_MenuBar;
        if (no_move)            window_flags |= ImGuiWindowFlags_NoMove;
        if (no_resize)          window_flags |= ImGuiWindowFlags_NoResize;
        if (no_collapse)        window_flags |= ImGuiWindowFlags_NoCollapse;
        if (no_nav)             window_flags |= ImGuiWindowFlags_NoNav;
        if (no_background)      window_flags |= ImGuiWindowFlags_NoBackground;
        if (no_bring_to_front)  window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
        if (no_docking)         window_flags |= ImGuiWindowFlags_NoDocking;
        if (unsaved_document)   window_flags |= ImGuiWindowFlags_UnsavedDocument;
        // Note: To prevent closing, pass NULL as p_open to ImGui::Begin() instead of using a flag
        // ImGui doesn't have ImGuiWindowFlags_NoClose - use NULL p_open parameter instead

        // Only render the Properties window if the visibility flag is true
        if (show_property_panel) {
            // We specify a default position/size in case there's no data in the .ini file
            const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + 650, main_viewport->WorkPos.y + 20), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);

            // Main body of the Properties window starts here
            // If no_close is true, pass NULL instead of &show_property_panel to prevent closing
            // CRITICAL: End() must be called regardless of Begin() return value to prevent ImGui assertion errors
            bool* p_open_ptr = no_close ? NULL : &show_property_panel;
            if (ImGui::Begin("Properties_Prop", p_open_ptr, window_flags))
            {
                // Create a file browser instance
                mFileDialog.Display();
                if (mFileDialog.HasSelected())
                {
                    std::string selectedFile = mFileDialog.GetSelected().string();
                    
                    // Professional path sanitization: Convert to absolute path and resolve canonical form
                    // This ensures paths are stored consistently and removes ".." and redundant separators
                    try {
                        std::filesystem::path filePath(selectedFile);
                        if (std::filesystem::exists(filePath)) {
                            // Convert to absolute path first, then canonical (resolves symlinks and "..")
                            std::filesystem::path absolutePath = std::filesystem::absolute(filePath);
                            std::filesystem::path canonicalPath = std::filesystem::canonical(absolutePath);
                            selectedFile = canonicalPath.string();
                        } else {
                            // If file doesn't exist yet (shouldn't happen from file dialog, but handle gracefully)
                            std::filesystem::path absolutePath = std::filesystem::absolute(filePath);
                            selectedFile = absolutePath.string();
                        }
                    } catch (const std::filesystem::filesystem_error& e) {
                        LOG_ERROR("[UIManager] Path sanitization failed: " + std::string(e.what()));
                        // Fall back to original path if sanitization fails
                    }
                    
                    scene->setFilePath(selectedFile);  // This sets mCurrentFile and mNewFileRequested
                    
                    // Save the parent directory of the selected file for future imports
                    try {
                        std::filesystem::path filePath(selectedFile);
                        std::filesystem::path parentDir = filePath.parent_path();
                        std::string dirString = parentDir.string();
                        // Convert backslashes to forward slashes for consistency
                        std::replace(dirString.begin(), dirString.end(), '\\', '/');
                        
                        AppSettings& settings = AppSettingsManager::getInstance().getSettings();
                        settings.environment.lastImportDirectory = dirString;
                        AppSettingsManager::getInstance().markDirty();
                        LOG_INFO("[UIManager] Saved last import directory: " + dirString);
                    } catch (const std::filesystem::filesystem_error& e) {
                        LOG_ERROR("[UIManager] Failed to extract parent directory: " + std::string(e.what()));
                    }
                    
                    mFileDialog.ClearSelected();
                }
                
                // Render property panel content inside this window
                // CRITICAL: Pass selected model index to PropertyPanel to enable selection state checking
                ModelManager* modelManager = scene->getModelManager();
                int selectedModelIndex = (modelManager != nullptr) ? modelManager->getSelectedModelIndex() : -1;
                Model* selectedModel = nullptr;
                if (selectedModelIndex >= 0 && modelManager != nullptr) {
                    ModelInstance* instance = modelManager->getModel(selectedModelIndex);
                    if (instance) {
                        selectedModel = &instance->model;
                    }
                }
                mPropertyPanel.propertyPanel(&show_property_panel, selectedModelIndex, selectedModel);
            }
            // CRITICAL: End() must ALWAYS be called, even if Begin() returned false
            ImGui::End();
        }
    }
    
    // Show Outliner panel
    if (show_outliner) {
        mOutliner.outlinerPanel(&show_outliner);
        
        // CRITICAL: Immediately update model selection when outliner selection changes
        // This ensures strict isolation between models - no data leaks
        // Check if a node is selected in outliner and update model index accordingly
        // CRITICAL FIX: Only search for the model index when the selection ACTUALLY changes!
        static const void* last_selected_object_ptr = nullptr;
        
        ModelManager* modelManager = scene->getModelManager();
        if (modelManager != nullptr && mOutliner.g_selected_object != nullptr) {
            // Only run the heavy O(N) search if the user clicked a different object
            if (mOutliner.g_selected_object != last_selected_object_ptr) {
                int modelIndexForSelectedObject = mOutliner.findModelIndexForSelectedObject();
                if (modelIndexForSelectedObject >= 0) {
                    int currentModelIndex = modelManager->getSelectedModelIndex();
                    bool selectionChanged = (currentModelIndex != modelIndexForSelectedObject);
                    if (selectionChanged) {
                        modelManager->setSelectedModel(modelIndexForSelectedObject);
                    }
                    scene->checkAndTriggerAutoFocusOnSelectionChange();
                }
                // Cache the pointer so we don't search again next frame
                last_selected_object_ptr = mOutliner.g_selected_object;
            }
        } else if (mOutliner.g_selected_object == nullptr) {
            last_selected_object_ptr = nullptr;
        }
    }
    
    // Show TimeSlider panel
    if (mShow_timeline) {
        mTimeSlider.timeSliderPanel(&mShow_timeline);
    }
    
    // Show Viewport Settings window if requested
    if (m_viewportSettingsPanel.isWindowVisible()) {
        // Create a local variable to hold gizmo position validity (needed for reference parameter)
        bool gizmoPositionValid = scene->getGizmoManager().isGizmoPositionValid();
        m_viewportSettingsPanel.renderPanel(
            scene->getCamera(), 
            scene->getGrid(), 
            scene->getClearColor(), 
            gizmoPositionValid, 
            scene->getCameraFramingRequested()
        );
    }
    
    // --- DELETE MODAL LOGIC ---
    ModelManager* modelManager = scene->getModelManager();
    if (modelManager != nullptr && modelManager->getSelectedModelIndex() >= 0) {
        // Trigger popup on Delete key (ignore if typing in text fields)
        if (Keyboard::keyWentDown(GLFW_KEY_DELETE) && !ImGui::GetIO().WantTextInput) {
            ImGui::OpenPopup("Delete Model?");
        }
    }
    
    // Render the modal
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    // CRITICAL FIX: Removed ImGuiWindowFlags_NoMove so the window can be dragged
    if (ImGui::BeginPopupModal("Delete Model?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        
        // Fetch the name of the currently selected character
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

        ImGui::Text("Are you sure you want to delete:");
        // Print the character name in Cyan color to make it stand out
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "  %s", modelName.c_str());
        ImGui::Dummy(ImVec2(0.0f, 2.0f)); // Spacing
        ImGui::Text("This action cannot be undone.");
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus(); // Safe default
        
        ImGui::SameLine();
        
        // Use a red button for destructive action
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
        if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
            if (modelManager != nullptr) {
                int selectedIdx = modelManager->getSelectedModelIndex();
                if (selectedIdx >= 0) {
                    // Log the deletion action before removing the model
                    LOG_INFO("UI Action: Deleted model");
                    
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
        ImGui::PopStyleColor(3);
        
        // Allow Esc key to cancel
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    
    // Render Debug Panel if visible
    if (m_showDebugPanel) {
        ModelManager* modelManager = scene->getModelManager();
        m_debugPanel.renderPanel(&m_showDebugPanel, modelManager);
    }
}

void UIManager::appDockSpace(bool* p_open)
{
    //-- If you strip some features of, this demo is pretty much equivalent to calling DockSpaceOverViewport()!
    //-- In most cases you should be able to just call DockSpaceOverViewport() and ignore all the code below!
    //-- In this specific demo, we are not using DockSpaceOverViewport() because:
    //-- - we allow the host window to be floating/moveable instead of filling the viewport (when opt_fullscreen == false)
    //-- - we allow the host window to have padding (when opt_padding == true)
    //-- - we have a local menu bar in the host window (vs. you could use BeginMainMenuBar() + DockSpaceOverViewport() in your code!)
    //-- TL;DR; this demo is more complicated than what you would normally use.
    //-- If we removed all the options we are showcasing, this demo would become:
    //--     void ShowExampleAppDockSpace()
    //--     {
    //--         ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
    //--     }

    static bool opt_fullscreen = true;
    static bool opt_padding = false;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

    //-- We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
    //-- because it would be confusing to have two docking targets within each others.
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    if (opt_fullscreen)
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    }
    else
    {
        dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
    }

    //-- When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
    //-- and handle the pass-thru hole, so we ask Begin() to not render a background.
    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        window_flags |= ImGuiWindowFlags_NoBackground;

    //-- Important: note that we proceed even if Begin() returns false (aka window is collapsed).
    //-- This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
    //-- all active windows docked into it will lose their parent and become undocked.
    //-- We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
    //-- any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
    if (!opt_padding)
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    
    // CRITICAL: End() must be called regardless of Begin() return value
    if (ImGui::Begin("DockSpace Demo", p_open, window_flags)) {
        if (!opt_padding)
            ImGui::PopStyleVar();

        if (opt_fullscreen)
            ImGui::PopStyleVar(2);

        //-- Submit the DockSpace ---------------
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            
            //-- Set up default docking layout on first run (BEFORE DockSpace) ---------
            // Only set up if no docking layout exists yet (check if central node exists)
            ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
            static bool layout_initialized = false;
            
            if (!layout_initialized && (node == nullptr || node->IsEmpty()))
            {
                layout_initialized = true;
                
                // Get viewport size for layout
                const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
                
                // Clear any existing layout and create new one
                ImGui::DockBuilderRemoveNode(dockspace_id);
                ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
                ImGui::DockBuilderSetNodeSize(dockspace_id, main_viewport->WorkSize);
                
                // Split the dockspace: left panel (20% width) and right area
                ImGuiID dock_id_left;
                ImGuiID dock_id_right;
                dock_id_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.2f, nullptr, &dock_id_right);
                
                // Split the right area: bottom panel (15% height) and center viewport
                ImGuiID dock_id_bottom;
                ImGuiID dock_id_center;
                dock_id_bottom = ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Down, 0.15f, nullptr, &dock_id_center);
                
                // Dock windows to their positions (this sets the docking target for when windows are created)
                // This will override any saved positions in imgui.ini
                ImGui::DockBuilderDockWindow("Properties_Prop", dock_id_left);
                ImGui::DockBuilderDockWindow("Outliner", dock_id_left);
                ImGui::DockBuilderDockWindow("timeSlider", dock_id_bottom);
                ImGui::DockBuilderDockWindow("Viewport", dock_id_center);  // Viewport in the center
                
                // Finalize the layout
                ImGui::DockBuilderFinish(dockspace_id);
            }
            
            // Now call DockSpace (after DockBuilder setup)
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }
        else
        {
            ShowDockingDisabledMessage();
        }
    } else {
        // Begin() returned false - still need to pop style var if we pushed it
        if (!opt_padding)
            ImGui::PopStyleVar();
    }
    
    // CRITICAL: End() must ALWAYS be called, even if Begin() returned false
    ImGui::End();
}

void UIManager::ShowDockingDisabledMessage()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("ERROR: Docking is not enabled! See Demo > Configuration.");
    ImGui::Text("Set io.ConfigFlags |= ImGuiConfigFlags_DockingEnable in your code, or ");
    ImGui::SameLine(0.0f, 0.0f);
    if (ImGui::SmallButton("click here"))
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
}

void UIManager::ShowAppMainMenuBar(Scene* scene)
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Scene", NULL)) { 
                scene->clearScene();
                LOG_INFO("UI Action: File -> New Scene");
            }
            if (ImGui::MenuItem("Import", NULL)) { 
                // Restore last import directory if it exists and is valid
                AppSettings& settings = AppSettingsManager::getInstance().getSettings();
                std::string lastDir = settings.environment.lastImportDirectory;
                if (!lastDir.empty()) {
                    try {
                        std::filesystem::path dirPath(lastDir);
                        if (std::filesystem::exists(dirPath) && std::filesystem::is_directory(dirPath)) {
                            mFileDialog.SetPwd(dirPath);
                            LOG_INFO("[UIManager] Restored last import directory: " + lastDir);
                        } else {
                            LOG_WARNING("[UIManager] Last import directory no longer exists: " + lastDir);
                        }
                    } catch (const std::filesystem::filesystem_error& e) {
                        LOG_ERROR("[UIManager] Failed to set import directory: " + std::string(e.what()));
                    }
                }
                
                mFileDialog.Open();
                LOG_INFO("UI Action: File -> Import");
            }
            if (ImGui::MenuItem("Logger / Info", NULL)) { m_showDebugPanel = !m_showDebugPanel; }
            
            // Separator before Save Settings
            ImGui::Separator();
            
            // Save Settings menu item (moved from Tools menu)
            if (ImGui::MenuItem("Save Settings")) {
                // Update all settings from current state before saving
                AppSettings& settings = AppSettingsManager::getInstance().getSettings();
                
                // Update grid settings
                Grid* grid = scene->getGrid();
                if (grid != nullptr) {
                    settings.grid.size = grid->getSize();
                    settings.grid.spacing = grid->getSpacing();
                    settings.grid.enabled = grid->isEnabled();
                    settings.grid.majorLineColor = grid->getMajorLineColor();
                    settings.grid.minorLineColor = grid->getMinorLineColor();
                    settings.grid.centerLineColor = grid->getCenterLineColor();
                }
                
                // Update environment settings
                ImVec4 clearColor = scene->getClearColor();
                settings.environment.backgroundColor = glm::vec4(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
                
                // Update bounding boxes enabled state
                ModelManager* modelManager = scene->getModelManager();
                if (modelManager != nullptr) {
                    settings.environment.boundingBoxesEnabled = modelManager->areBoundingBoxesEnabled();
                }
                
                // Get settings from ViewportSettingsPanel
                settings.environment.fpsScale = m_viewportSettingsPanel.getFpsScale();
                settings.camera.smoothCameraEnabled = m_viewportSettingsPanel.getSmoothCameraUIState();
                settings.camera.smoothTransitionSpeed = m_viewportSettingsPanel.getSmoothCameraSpeed();
                settings.environment.vSyncEnabled = m_viewportSettingsPanel.getVSyncUIState();
                
                // Get settings from Scene (autoFocusEnabled and showFPS are managed by Scene)
                settings.environment.autoFocusEnabled = scene->getAutoFocusEnabled();
                settings.environment.showFPS = scene->getShowFPS();
                
                // Update window position and size
                GLFWwindow* window = scene->getWindow();
                int winX, winY, winW, winH;
                glfwGetWindowPos(window, &winX, &winY);
                glfwGetWindowSize(window, &winW, &winH);
                settings.window.posX = winX;
                settings.window.posY = winY;
                settings.window.width = winW;
                settings.window.height = winH;
                
                // Save immediately
                if (AppSettingsManager::getInstance().saveSettings()) {
                    LOG_INFO("UI Action: File -> Save Settings - Settings saved successfully");
                } else {
                    LOG_ERROR("[Settings] Failed to save settings");
                }
            }
            
            // Show recent files
            std::vector<std::string> recentFiles = scene->getRecentFiles();
            if (!recentFiles.empty()) {
                ImGui::Separator();
                for (size_t i = 0; i < recentFiles.size(); i++) {
                    // Extract just the filename for display
                    std::string displayName = recentFiles[i];
                    size_t lastSlash = displayName.find_last_of("/\\");
                    if (lastSlash != std::string::npos) {
                        displayName = displayName.substr(lastSlash + 1);
                    }
                    
                    // Add index for clarity: "1. filename.fbx"
                    std::string menuLabel = std::to_string(i + 1) + ". " + displayName;
                    
                    if (ImGui::MenuItem(menuLabel.c_str())) {
                        // Recent files paths are already sanitized (absolute and canonical) from addRecentFile()
                        // No additional sanitization needed here, but verify file still exists
                        std::filesystem::path recentPath(recentFiles[i]);
                        if (std::filesystem::exists(recentPath)) {
                            scene->setFilePath(recentFiles[i]);  // This sets mCurrentFile and mNewFileRequested
                        } else {
                            LOG_WARNING("[UIManager] Recent file no longer exists: " + recentFiles[i]);
                        }
                    }
                }
            }
            
            ImGui::EndMenu();
        }
        // Tools menu moved to viewport window - removed from main menu bar
        ImGui::EndMainMenuBar();
    }
}
