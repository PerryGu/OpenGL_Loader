#include "property_panel.h"
#include "../../graphics/model.h"
#include "../../utils/logger.h"


//== constructors ===============================
void PropertyPanel::propertyPanel(bool* p_open, int selectedModelIndex, Model* model)
{
    //ImGui::PushItemWidth(-ImGui::GetWindowWidth() * 0.35f);

    ImGui::PushItemWidth(ImGui::GetFontSize() * 12);
  
    //-- add colapseble attributs ----------------------
    if (ImGui::CollapsingHeader("Transforms"))
    {
        ImGui::Separator();

        // CRITICAL: Check selection state - hide entire Transforms block if nothing is selected
        // This prevents showing "ghost" values from previously selected nodes
        // Selection is invalid if:
        //   1. mSelectedNod_name is empty (no node selected in outliner)
        //   2. selectedModelIndex is -1 (no model selected in ModelManager)
        bool hasValidSelection = !mSelectedNod_name.empty() && selectedModelIndex >= 0;
        
        if (!hasValidSelection) {
            // No valid selection - show "No Selection" message and hide all transform controls
            ImGui::Text("No Selection");
            // Note: PopItemWidth() is called at the end of the function, not here
        } else {
            // Valid selection exists - render transform controls
            //-- set  SelectedNod name -------------- -
            const char* selectedNod_name = mSelectedNod_name.c_str();
            ImGui::Text(selectedNod_name);

        // Translations section (first)
        ImGui::Text("Translations");
        if (ImGui::BeginPopupContextItem("Translations"))
        {
            char buf[64];
            sprintf(buf, "Reset to %i", 0);
            if (ImGui::MenuItem(buf)) {
                mSliderTranslations = glm::vec3(0.0f, 0.0f, 0.0f);
                if (!mSelectedNod_name.empty()) {
                    mBoneTranslations[mSelectedNod_name] = glm::vec3(0.0f, 0.0f, 0.0f);
                }
            }
            ImGui::EndPopup();
        }
        ImGui::Separator();
        
        // Draw the translation slider - detect changes and save immediately when values change
        ImGui::PushID("Translations");
        const float epsilon = 0.001f;
        glm::vec3 previousTranslation = mSliderTranslations;
        nimgui::draw_vec3_widgetx("Translations", mSliderTranslations, 100.0f);
        ImGui::PopID();
        
        // Check if translation values changed by comparing values (most reliable method)
        bool translationChanged = false;
        if (std::abs(mSliderTranslations.x - previousTranslation.x) > epsilon ||
            std::abs(mSliderTranslations.y - previousTranslation.y) > epsilon ||
            std::abs(mSliderTranslations.z - previousTranslation.z) > epsilon) {
            translationChanged = true;
        }
        
        // Save the current translation for the current bone ONLY if values changed
        // This prevents overwriting saved values when switching bones
        if (!mSelectedNod_name.empty() && translationChanged) {
            mBoneTranslations[mSelectedNod_name] = mSliderTranslations;
        }
        
        // DIRECT UPDATE: Update Model's array directly when slider changes (index-based, no strings)
        if (translationChanged && mSelectedBoneIndex >= 0 && model != nullptr) {
            updateBoneTranslation(model, mSelectedBoneIndex, mSliderTranslations);
        }
        
        // Rotations section (second)
        ImGui::Separator();
        ImGui::Text("Rotations");
        if (ImGui::BeginPopupContextItem("Rotations"))
        {
            char buf[64];
            sprintf(buf, "Reset to %i", 0);
            if (ImGui::MenuItem(buf)) {
                mSliderRotations = glm::vec3(0.0f, 0.0f, 0.0f);
                // Also remove from bone rotations map if it exists
                if (!mSelectedNod_name.empty()) {
                    mBoneRotations[mSelectedNod_name] = glm::vec3(0.0f, 0.0f, 0.0f);
                }
            }

            ImGui::EndPopup();
        }
        ImGui::Separator();
        
        // Draw the rotation slider - detect changes and save immediately when values change
        ImGui::PushID("Rotations");
        glm::vec3 previousRotation = mSliderRotations;
        nimgui::draw_vec3_widgetx("Rotations", mSliderRotations, 100.0f);
        ImGui::PopID();
        
        // Check if rotation values changed by comparing values (most reliable method)
        bool rotationChanged = false;
        if (std::abs(mSliderRotations.x - previousRotation.x) > epsilon ||
            std::abs(mSliderRotations.y - previousRotation.y) > epsilon ||
            std::abs(mSliderRotations.z - previousRotation.z) > epsilon) {
            rotationChanged = true;
        }
        
        // Save the current rotation for the current bone ONLY if values changed
        // This prevents overwriting saved values when switching bones
        if (!mSelectedNod_name.empty() && rotationChanged) {
            mBoneRotations[mSelectedNod_name] = mSliderRotations;
        }
        
        // DIRECT UPDATE: Update Model's array directly when slider changes (index-based, no strings)
        if (rotationChanged && mSelectedBoneIndex >= 0 && model != nullptr) {
            updateBoneRotation(model, mSelectedBoneIndex, mSliderRotations);
        }
        
        // Scales section
        ImGui::Separator();
        ImGui::Text("Scales");
        if (ImGui::BeginPopupContextItem("Scales"))
        {
            char buf[64];
            sprintf(buf, "Reset to %i", 1);
            if (ImGui::MenuItem(buf)) {
                mSliderScales = glm::vec3(1.0f, 1.0f, 1.0f);
                if (!mSelectedNod_name.empty()) {
                    mBoneScales[mSelectedNod_name] = glm::vec3(1.0f, 1.0f, 1.0f);
                }
            }
            ImGui::EndPopup();
        }
        ImGui::Separator();
        
        // Draw the scale slider - detect changes and save immediately when values change
        ImGui::PushID("Scales");
        glm::vec3 previousScale = mSliderScales;
        nimgui::draw_vec3_widgetx("Scales", mSliderScales, 100.0f);
        ImGui::PopID();
        
        // Check if scale values changed by comparing values (most reliable method)
        bool scaleChanged = false;
        if (std::abs(mSliderScales.x - previousScale.x) > epsilon ||
            std::abs(mSliderScales.y - previousScale.y) > epsilon ||
            std::abs(mSliderScales.z - previousScale.z) > epsilon) {
            scaleChanged = true;
        }
        
        // Save the current scale for the current bone ONLY if values changed
        // This prevents overwriting saved values when switching bones
        if (!mSelectedNod_name.empty() && scaleChanged) {
            mBoneScales[mSelectedNod_name] = mSliderScales;
        }
        
        // DIRECT UPDATE: Update Model's array directly when slider changes (index-based, no strings)
        if (scaleChanged && mSelectedBoneIndex >= 0 && model != nullptr) {
            updateBoneScale(model, mSelectedBoneIndex, mSliderScales);
        }
        
        // Reset buttons
        ImGui::Separator();
        ImGui::Spacing();
        
        // Button to reset current bone
        if (ImGui::Button("Reset Current Bone")) {
            LOG_INFO("PropertyPanel: Reset Bone clicked");
            resetCurrentBone(model);
        }
        
        ImGui::SameLine();
        
        // Button to reset all bones
        if (ImGui::Button("Reset All Bones")) {
            LOG_INFO("PropertyPanel: Reset All Bones clicked");
            resetAllBones(model);
        }
        }  // End else (hasValidSelection)
    }  // End if (ImGui::CollapsingHeader("Transforms"))
    
    // Flip Normals checkbox
    if (model != nullptr) {
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Checkbox("Flip Normals", &model->m_flipNormals)) {
            LOG_INFO("PropertyPanel: Flip Normals toggled to " + std::string(model->m_flipNormals ? "ON" : "OFF"));
        }
        
        // Default Material checkbox
        if (ImGui::Checkbox("Default Material (Grey)", &model->m_useDefaultMaterial)) {
            LOG_INFO("PropertyPanel: Default Material toggled to " + std::string(model->m_useDefaultMaterial ? "ON" : "OFF"));
        }
        
        // Wireframe checkbox
        if (ImGui::Checkbox("Wireframe", &model->m_wireframeMode)) {
            LOG_INFO("PropertyPanel: Wireframe toggled to " + std::string(model->m_wireframeMode ? "ON" : "OFF"));
        }
        
        // Bounding Box checkbox
        if (ImGui::Checkbox("Bounding Box", &model->m_showBoundingBox)) {
            LOG_INFO("PropertyPanel: Bounding Box toggled to " + std::string(model->m_showBoundingBox ? "ON" : "OFF"));
        }
        
        // Show Skeleton checkbox
        if (ImGui::Checkbox("Show Skeleton", &model->m_showSkeleton)) {
            LOG_INFO("PropertyPanel: Show Skeleton toggled to " + std::string(model->m_showSkeleton ? "ON" : "OFF"));
        }
    }

    //-- End window content (but NOT the window itself) ----
    // Note: ImGui::End() for the window is called in scene.cpp uiElements()
    // This function only renders content inside an already-begun window
    ImGui::PopItemWidth();
    // DO NOT call ImGui::End() here - it's called by the caller (uiElements)
}


//-- set Selecte dNode -----------------------------
void PropertyPanel::setSelectedNode(std::string selectedNode, Model* model)
{
    // Only process if the selected node actually changed
    if (selectedNode != mSelectedNod_name) {
        // Note: Selection change tracking is handled internally
        // Debug prints removed for clean console output
        
        // CRITICAL: Save current bone's rotation, translation, and scale BEFORE switching
        // This must happen before we change mSelectedNod_name
        // BUT: Only save if the current bone name is not empty and is different from the new one
        // This prevents saving when switching from empty selection to a bone
        if (!mSelectedNod_name.empty() && mSelectedNod_name != selectedNode) {
            // Save the current slider values for the bone we're leaving
            mBoneRotations[mSelectedNod_name] = mSliderRotations;
            mBoneTranslations[mSelectedNod_name] = mSliderTranslations;
            mBoneScales[mSelectedNod_name] = mSliderScales;
            
            // Removed debug prints - they were running every frame
        }
        
        // Update previous selection tracking
        mPreviousSelectedNode = mSelectedNod_name;
        
        // Update the selected node name
        mSelectedNod_name = selectedNode;
        
        // INDEX-BASED SELECTION: Set selectedBoneIndex from bone name (eliminates string matching in hot path)
        if (model != nullptr && !selectedNode.empty()) {
            mSelectedBoneIndex = model->getBoneIndexFromName(selectedNode);
        } else {
            mSelectedBoneIndex = -1;
        }
        
        // Load the new bone's rotation, translation, and scale (or default if not previously set)
        if (!selectedNode.empty()) {
            // Load rotation
            auto rotIt = mBoneRotations.find(selectedNode);
            if (rotIt != mBoneRotations.end()) {
                mSliderRotations = rotIt->second;
                // Removed debug prints - they were running every frame
            } else {
                mSliderRotations = glm::vec3(0.0f, 0.0f, 0.0f);
                // Removed debug prints - they were running every frame
            }
            
            // Load translation
            auto transIt = mBoneTranslations.find(selectedNode);
            if (transIt != mBoneTranslations.end()) {
                mSliderTranslations = transIt->second;
                // Removed debug prints - they were running every frame
            } else {
                mSliderTranslations = glm::vec3(0.0f, 0.0f, 0.0f);
                // Removed debug prints - they were running every frame
            }
            
            // Load scale
            auto scaleIt = mBoneScales.find(selectedNode);
            if (scaleIt != mBoneScales.end()) {
                mSliderScales = scaleIt->second;
                // Removed debug prints - they were running every frame
            } else {
                mSliderScales = glm::vec3(1.0f, 1.0f, 1.0f);  // Default scale is 1.0 (no scaling)
                // Removed debug prints - they were running every frame
            }
            
            // Removed debug prints - they were running every frame
        }
    }
}


//-- get Slider Rotate update -------------
glm::vec3 PropertyPanel::getSliderRot_update()
{
    return mSliderRotations;
}

//-- get Slider Translate update -------------
glm::vec3 PropertyPanel::getSliderTrans_update()
{
    return mSliderTranslations;
}

//-- get Slider Scale update -------------
glm::vec3 PropertyPanel::getSliderScale_update()
{
    return mSliderScales;
}

//-- reset current bone rotation to (0, 0, 0) --------------------------------
void PropertyPanel::resetCurrentBone(Model* model)
{
    if (!mSelectedNod_name.empty()) {
        // Reset slider values
        mSliderRotations = glm::vec3(0.0f, 0.0f, 0.0f);
        mSliderTranslations = glm::vec3(0.0f, 0.0f, 0.0f);
        mSliderScales = glm::vec3(1.0f, 1.0f, 1.0f);
        
        // Remove from bone rotations/translations/scales map (or set to default)
        // Setting to default ensures it's synced to Model and cleared
        mBoneRotations[mSelectedNod_name] = glm::vec3(0.0f, 0.0f, 0.0f);
        mBoneTranslations[mSelectedNod_name] = glm::vec3(0.0f, 0.0f, 0.0f);
        mBoneScales[mSelectedNod_name] = glm::vec3(1.0f, 1.0f, 1.0f);
        
        // DIRECT UPDATE: Update Model's array directly (index-based, no strings)
        if (mSelectedBoneIndex >= 0 && model != nullptr) {
            updateBoneRotation(model, mSelectedBoneIndex, glm::vec3(0.0f, 0.0f, 0.0f));
            updateBoneTranslation(model, mSelectedBoneIndex, glm::vec3(0.0f, 0.0f, 0.0f));
            updateBoneScale(model, mSelectedBoneIndex, glm::vec3(1.0f, 1.0f, 1.0f));
        }
        
        std::cout << "[PropertyPanel] Reset bone '" << mSelectedNod_name << "' rotations to (0, 0, 0), translations to (0, 0, 0), scales to (1, 1, 1)" << std::endl;
    }
}

//-- set slider rotations (for gizmo to update PropertyPanel) --------------------------------
void PropertyPanel::setSliderRotations(const glm::vec3& rotations)
{
    mSliderRotations = rotations;
    // Save to bone rotations map for the currently selected node
    if (!mSelectedNod_name.empty()) {
        mBoneRotations[mSelectedNod_name] = rotations;
    }
}

//-- set slider translations (for gizmo to update PropertyPanel) --------------------------------
void PropertyPanel::setSliderTranslations(const glm::vec3& translations)
{
    mSliderTranslations = translations;
    // Save to bone translations map for the currently selected node
    if (!mSelectedNod_name.empty()) {
        mBoneTranslations[mSelectedNod_name] = translations;
    }
}

//-- set slider scales (for gizmo to update PropertyPanel) --------------------------------
void PropertyPanel::setSliderScales(const glm::vec3& scales)
{
    mSliderScales = scales;
    // Save to bone scales map for the currently selected node
    if (!mSelectedNod_name.empty()) {
        mBoneScales[mSelectedNod_name] = scales;
    }
}

//-- initialize rig root (or RootNode) transforms from model's current transform --------------------------------
// CHANGED: Now works for rig root (e.g., "Rig_GRP") as well as RootNode (backward compatibility)
// This prevents double transformation by syncing PropertyPanel with model's current state
// when rig root is first selected. Only initializes if rig root doesn't already have values.
void PropertyPanel::initializeRootNodeFromModel(const glm::vec3& translation, const glm::quat& rotation, const glm::vec3& scale)
{
    // CHANGED: Allow initialization for any selected node (rig root or RootNode)
    // The caller (main.cpp) ensures this is only called for rig root/RootNode
    // This check is kept for safety but is less restrictive
    if (mSelectedNod_name.empty()) {
        return;  // No node selected, don't initialize
    }
    
    // Only initialize if RootNode doesn't already have values (first time selection)
    // This prevents overwriting user's manual changes
    auto transIt = mBoneTranslations.find(mSelectedNod_name);
    auto rotIt = mBoneRotations.find(mSelectedNod_name);
    auto scaleIt = mBoneScales.find(mSelectedNod_name);
    
    // If rig root/RootNode already has values, don't overwrite (user may have set them manually)
    if (transIt != mBoneTranslations.end() || rotIt != mBoneRotations.end() || scaleIt != mBoneScales.end()) {
        return;  // Already initialized, don't overwrite
    }
    
    // Convert quaternion rotation to Euler angles (degrees) for PropertyPanel
    glm::vec3 rotationEuler = glm::degrees(glm::eulerAngles(rotation));
    
    // CHANGED: Initialize PropertyPanel rig root/RootNode transforms from model's current transform
    mSliderTranslations = translation;
    mSliderRotations = rotationEuler;
    mSliderScales = scale;
    
    // Save to bone maps
    mBoneTranslations[mSelectedNod_name] = translation;
    mBoneRotations[mSelectedNod_name] = rotationEuler;
    mBoneScales[mSelectedNod_name] = scale;
    
    std::cout << "[PropertyPanel] Initialized '" << mSelectedNod_name << "' transforms from model: "
              << "Translation(" << translation.x << ", " << translation.y << ", " << translation.z << "), "
              << "Rotation(" << rotationEuler.x << ", " << rotationEuler.y << ", " << rotationEuler.z << "), "
              << "Scale(" << scale.x << ", " << scale.y << ", " << scale.z << ")" << std::endl;
    
    // NOTE: After initializing PropertyPanel from model, the model's pos/rotation/size should be reset to identity
    // This is done in main.cpp when rig root is selected, to prevent double transformation
}

//-- reset all bone rotations to (0, 0, 0) --------------------------------
void PropertyPanel::resetAllBones(Model* model)
{
    if (!model) return;
    
    // Selectively erase ONLY the bones that belong to the current model from the UI maps
    for (auto it = mBoneRotations.begin(); it != mBoneRotations.end(); ) {
        if (model->getBoneIndexFromName(it->first) >= 0) it = mBoneRotations.erase(it);
        else ++it;
    }
    for (auto it = mBoneTranslations.begin(); it != mBoneTranslations.end(); ) {
        if (model->getBoneIndexFromName(it->first) >= 0) it = mBoneTranslations.erase(it);
        else ++it;
    }
    for (auto it = mBoneScales.begin(); it != mBoneScales.end(); ) {
        if (model->getBoneIndexFromName(it->first) >= 0) it = mBoneScales.erase(it);
        else ++it;
    }
    
    // Reset current slider values if a bone is selected
    if (!mSelectedNod_name.empty()) {
        mSliderRotations = glm::vec3(0.0f, 0.0f, 0.0f);
        mSliderTranslations = glm::vec3(0.0f, 0.0f, 0.0f);
        mSliderScales = glm::vec3(1.0f, 1.0f, 1.0f);
        
        // CRITICAL FIX: Explicitly update the map for the currently selected node!
        // This fixes the desync where the RootNode is selected, the Gizmo resets, 
        // but the rendering map doesn't update until deselection.
        mBoneRotations[mSelectedNod_name] = mSliderRotations;
        mBoneTranslations[mSelectedNod_name] = mSliderTranslations;
        mBoneScales[mSelectedNod_name] = mSliderScales;
    }
    
    // DIRECT UPDATE: Clear the actual model's transformations directly here
    if (model) {
        // CRITICAL FIX: Clear the Model's fast internal arrays explicitly
        model->clearAllBoneRotations();
        model->clearAllBoneTranslations();
        model->clearAllBoneScales();
        
        // Force the engine to update the 3D mesh immediately
        model->forceTransformsDirty();
    }
    
    mResetAllBonesRequested = true; // Signal application.cpp to clean up the RootNode
    
    std::cout << "[PropertyPanel] Reset all bones for the selected model." << std::endl;
}

//-- clear all transform values (for New Scene) --------------------------------
// This method completely resets the PropertyPanel to a clean state
// Clears all bone rotations, translations, scales, current sliders, and selected node
// Ensures that when a new character is imported after New Scene, it won't inherit 
// the previous character's transform values
void PropertyPanel::clearAllTransforms()
{
    // Clear all saved bone transforms (rotations, translations, scales)
    mBoneRotations.clear();
    mBoneTranslations.clear();
    mBoneScales.clear();
    
    // Reset current slider values to defaults
    mSliderRotations = glm::vec3(0.0f, 0.0f, 0.0f);
    mSliderTranslations = glm::vec3(0.0f, 0.0f, 0.0f);
    mSliderScales = glm::vec3(1.0f, 1.0f, 1.0f);
    
    // Clear selected node name (no bone/node selected)
    mSelectedNod_name = "";
    mPreviousSelectedNode = "";
    
    // Clear reset request flag (if it was set)
    mResetAllBonesRequested = false;
    
    std::cout << "[PropertyPanel] Cleared all transforms for New Scene - all values reset to defaults" << std::endl;
}

// Direct update methods - update Model's arrays directly when slider changes
// These methods eliminate string operations and map lookups from the hot path
void PropertyPanel::updateBoneRotation(Model* model, int boneIndex, const glm::vec3& rotation)
{
    if (model == nullptr || boneIndex < 0) return;
    model->updateBoneRotationByIndex(boneIndex, rotation);
}

void PropertyPanel::updateBoneTranslation(Model* model, int boneIndex, const glm::vec3& translation)
{
    if (model == nullptr || boneIndex < 0) return;
    model->updateBoneTranslationByIndex(boneIndex, translation);
}

void PropertyPanel::updateBoneScale(Model* model, int boneIndex, const glm::vec3& scale)
{
    if (model == nullptr || boneIndex < 0) return;
    model->updateBoneScaleByIndex(boneIndex, scale);
}

//-- zero out transforms for a specific node by name (for deferred deletion) --------------------------------
void PropertyPanel::zeroNodeTransforms(const std::string& nodeName)
{
    if (nodeName.empty()) return;
    
    // Zero out the transforms in the bone maps
    mBoneRotations[nodeName] = glm::vec3(0.0f, 0.0f, 0.0f);
    mBoneTranslations[nodeName] = glm::vec3(0.0f, 0.0f, 0.0f);
    mBoneScales[nodeName] = glm::vec3(1.0f, 1.0f, 1.0f);
    
    // If this node is currently selected, also update the slider values
    if (mSelectedNod_name == nodeName) {
        mSliderRotations = glm::vec3(0.0f, 0.0f, 0.0f);
        mSliderTranslations = glm::vec3(0.0f, 0.0f, 0.0f);
        mSliderScales = glm::vec3(1.0f, 1.0f, 1.0f);
    }
}

//-- set RootNode translation directly by node name (for benchmark tool) --------------------------------
void PropertyPanel::setRootNodeTranslation(const std::string& rootNodeName, const glm::vec3& translation)
{
    if (rootNodeName.empty()) return;
    
    // Set translation in bone maps (rotation and scale remain at defaults)
    mBoneTranslations[rootNodeName] = translation;
    
    // Initialize rotation and scale to defaults if not already set
    if (mBoneRotations.find(rootNodeName) == mBoneRotations.end()) {
        mBoneRotations[rootNodeName] = glm::vec3(0.0f, 0.0f, 0.0f);
    }
    if (mBoneScales.find(rootNodeName) == mBoneScales.end()) {
        mBoneScales[rootNodeName] = glm::vec3(1.0f, 1.0f, 1.0f);
    }
}
