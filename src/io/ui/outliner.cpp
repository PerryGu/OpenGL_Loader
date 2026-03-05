#include "outliner.h"
#include "../../utils/logger.h"
#include "../keyboard.h"
#include "../fbx_rig_analyzer.h"
// CRITICAL: Tell GLFW not to include OpenGL headers (we use GLAD instead)
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <algorithm>
#include <string>



int getPropertyCount(ofbx::IElementProperty* prop)
{
    return prop ? getPropertyCount(prop->getNext()) + 1 : 0;
}


//== constructors ===============================
void Outliner::outlinerPanel(bool* p_open)
{
    // Set window flags to allow keyboard input
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;
    
    ImGui::Begin("Outliner", p_open, window_flags);                          

    //-- add attributes-------------- -
    const char* selectedNod_name = mSelectedNod_name.c_str();

    // Display all loaded FBX scenes (one per model)
    // Each scene is displayed with its model name as a header
    if (!mFBXScenes.empty())
    {
        // Rebuild selectable objects list if needed (from all scenes)
        if (mNeedsRebuildList) {
            rebuildSelectableObjectsList();
            mNeedsRebuildList = false;
        }
        
        // Handle keyboard navigation (arrow keys) - call AFTER Begin() so window is active
        handleKeyboardNavigation();
        
        // Display each scene with its model name
        for (size_t i = 0; i < mFBXScenes.size(); ++i) {
            const FBXSceneData& sceneData = mFBXScenes[i];
            
            if (sceneData.scene) {
                // Display model name as a collapsible header
                std::string headerLabel = sceneData.displayName;
                if (headerLabel.empty()) {
                    headerLabel = "Model " + std::to_string(i + 1);
                }
                
                // Use a tree node for the model name (always open, but visually distinct)
                // This creates a visual separator between different models
                ImGuiTreeNodeFlags modelHeaderFlags = ImGuiTreeNodeFlags_DefaultOpen | 
                                                      ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                                      ImGuiTreeNodeFlags_Bullet |
                                                      ImGuiTreeNodeFlags_Framed;
                
                // Style the model header to make it visually distinct
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.3f, 0.4f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.4f, 0.4f, 0.5f, 0.7f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.5f, 0.5f, 0.6f, 0.9f));
                
                ImGui::TreeNodeEx(headerLabel.c_str(), modelHeaderFlags);
                ImGui::PopStyleColor(3);
                
                // Display the scene hierarchy (indented under the model name)
                ImGui::Indent();
                showObjectsGUI(*sceneData.scene);
                ImGui::Unindent();
                
                // Add spacing between models for visual separation
                if (i < mFBXScenes.size() - 1) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                }
            }
        }
        
        if (g_selected_element != nullptr) {
            std::cout << "element." << g_selected_element << std::endl;
        }
    }
    // Legacy support: also check single scene pointer (for backward compatibility)
    else if (ofbx_scene)
    {
        // Rebuild selectable objects list if needed
        if (mNeedsRebuildList) {
            rebuildSelectableObjectsList(*ofbx_scene);
            mNeedsRebuildList = false;
        }
        
        // Handle keyboard navigation (arrow keys) - call AFTER Begin() so window is active
        handleKeyboardNavigation();
        
        showObjectsGUI(*ofbx_scene);
        if (g_selected_element != nullptr) {
            std::cout << "element." << g_selected_element << std::endl;
        }
    }
  
    ImGui::End();
}

//-- load the FBX file ---------------------------------- 
// This method now adds the scene to the collection instead of replacing
// Multiple FBX files can be loaded and all hierarchies will be displayed
void Outliner::loadFBXfile(std::string filePath)
{
    std::cout << "[Outliner] Loading FBX file: " << filePath.c_str() << std::endl;
    
    // CRITICAL FIX: Removed the duplicate file path check block here.
    // We WANT to allow loading the same FBX multiple times for Instancing.
    // The Outliner already has logic to uniquely rename colliding RootNodes.
    
    FILE* fp = fopen(filePath.c_str(), "rb");
    if (!fp) {
        std::cout << "[Outliner] ERROR: Failed to open file: " << filePath << std::endl;
        return;
    }
    
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    auto* content = new ofbx::u8[file_size];
    fread(content, 1, file_size, fp);
    
    ofbx::IScene* scene = ofbx::load((ofbx::u8*)content, file_size, (ofbx::u64)ofbx::LoadFlags::TRIANGULATE);
    if (!scene) {
        std::cout << "[Outliner] ERROR: Failed to load FBX scene: " << ofbx::getError() << std::endl;
        delete[] content;
        fclose(fp);
        return;
    }
    
    // Extract display name from file path
    std::string displayName = extractFileName(filePath);
    
    // Count how many times this file is already loaded to create a unique display name for the UI
    int duplicateCount = 0;
    for (const auto& existingScene : mFBXScenes) {
        if (existingScene.filePath == filePath) {
            duplicateCount++;
        }
    }
    if (duplicateCount > 0) {
        displayName += " (" + std::to_string(duplicateCount) + ")";
    }
    
    // Create scene data
    FBXSceneData sceneData(scene, filePath, displayName);
    
    // Get root node and check for name collision
    const ofbx::Object* root = scene->getRoot();
    if (root) {
        std::string originalName = root->name;
        sceneData.rootNodeOriginalName = originalName;
        
        // Check if this root node name already exists in loaded scenes
        if (rootNodeNameExists(originalName)) {
            // Generate unique name (e.g., "RootNode01", "RootNode02", etc.)
            std::string uniqueName = generateUniqueRootNodeName(originalName);
            sceneData.rootNodeRenamedName = uniqueName;
            
            // CRITICAL: Modify the actual FBX object name in the rig hierarchy
            // This ensures the renamed name is used throughout the system, not just for display
            // We cast away const because OpenFBX's name is a public char array (technically mutable)
            // This is safe because RootNode is not used in bone lookups (bones use Assimp mesh names)
            ofbx::Object* mutableRoot = const_cast<ofbx::Object*>(root);
            if (mutableRoot && uniqueName.length() < 128) {  // OpenFBX name array is 128 chars
                strncpy_s(mutableRoot->name, sizeof(mutableRoot->name), uniqueName.c_str(), _TRUNCATE);
                std::cout << "[Outliner] Root node name collision detected: '" << originalName 
                          << "' already exists. Renamed to '" << uniqueName << "' (actual object name changed)" << std::endl;
            } else {
                std::cout << "[Outliner] WARNING: Could not modify root node name (too long or null)" << std::endl;
            }
        } else {
            // No collision, use original name
            std::cout << "[Outliner] Root node name: '" << originalName << "' (no collision)" << std::endl;
        }
    }
    
    // Find and store the rig root (parent container of the rig, e.g., "Rig_GRP")
    // This is found once per FBX file import and stored for later use
    // IMPORTANT: Do this BEFORE pushing to vector so the rig root name is stored correctly
    std::string rigRootName = FBXRigAnalyzer::findRigRoot(scene);
    sceneData.rigRootName = rigRootName;  // Store rig root name in scene data
    
    if (!rigRootName.empty()) {
        std::cout << "[Rig Analyzer] Rig Root: '" << rigRootName << "'" << std::endl;
    } else {
        std::cout << "[Rig Analyzer] No rig root (LIMB_NODE) found in this FBX file" << std::endl;
    }
    
    // Add scene to collection (instead of replacing)
    // NOTE: rigRootName must be set BEFORE this push_back, otherwise it won't be stored
    mFBXScenes.push_back(sceneData);
    
    // Keep legacy pointer pointing to the last loaded scene (for backward compatibility)
    ofbx_scene = scene;
    
    // Mark that we need to rebuild the selectable objects list
    mNeedsRebuildList = true;
    
    std::cout << "[Outliner] Successfully loaded FBX scene: " << displayName 
              << " (Total scenes: " << mFBXScenes.size() << ")" << std::endl;

    delete[] content;
    fclose(fp);
}

//-- clear outliner (remove all loaded scenes) ----
void Outliner::clear()
{
    // Clear all FBX scenes
    // Note: We don't delete the scenes here because they're managed by openFBX
    // The scenes will be cleaned up when models are removed
    mFBXScenes.clear();
    
    // Clear legacy single scene pointer
    ofbx_scene = nullptr;
    
    // Clear selected elements
    g_selected_element = nullptr;
    g_selected_object = nullptr;
    
    // Clear selected node name
    mSelectedNod_name = "";
    
    // Clear navigation data
    mSelectableObjects.clear();
    mCurrentSelectionIndex = -1;
    mNeedsRebuildList = true;
    
    std::cout << "[Outliner] Cleared all scenes" << std::endl;
}

//-- remove a specific FBX scene by index (when a model is removed) ----
void Outliner::removeFBXScene(int modelIndex)
{
    if (modelIndex >= 0 && modelIndex < static_cast<int>(mFBXScenes.size())) {
        std::string displayName = mFBXScenes[modelIndex].displayName;
        
        // Remove exactly the one scene at this index
        mFBXScenes.erase(mFBXScenes.begin() + modelIndex);
        
        // Update legacy pointer to point to last scene (if any)
        if (!mFBXScenes.empty()) {
            ofbx_scene = mFBXScenes.back().scene;
        } else {
            ofbx_scene = nullptr;
        }
        
        // Mark that we need to rebuild the selectable objects list
        mNeedsRebuildList = true;
        
        // Clear selection if the deleted model was selected
        clearSelection();
        
        std::cout << "[Outliner] Removed FBX scene at index " << modelIndex << ": " << displayName 
                  << " (Remaining scenes: " << mFBXScenes.size() << ")" << std::endl;
    } else {
        std::cout << "[Outliner] Invalid model index for removal: " << modelIndex << std::endl;
    }
}

/*
void Outliner::showGUI(const ofbx::IElement& parent)
{

    for (const ofbx::IElement* element = parent.getFirstChild(); element; element = element->getSibling())
    {
        auto id = element->getID();
        char label[128];
        id.toString(label);
        strcat_s(label, " (");
        ofbx::IElementProperty* prop = element->getFirstProperty();
        bool first = true;
        while (prop)
        {
            if (!first)
                strcat_s(label, ", ");
            first = false;

            catProperty(label, *prop);
            prop = prop->getNext();
        }

        
        strcat_s(label, ")");

        ImGui::PushID((const void*)id.begin);
        ImGuiTreeNodeFlags flags = g_selected_element == element ? ImGuiTreeNodeFlags_Selected : 0;
        if (!element->getFirstChild()) flags |= ImGuiTreeNodeFlags_Leaf;
        if (ImGui::TreeNodeEx(label, flags))
        {
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) g_selected_element = element;
            if (element->getFirstChild()) showGUI(*element);
            ImGui::TreePop();
        }
        else
        {
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) g_selected_element = element;

        }
        ImGui::PopID();
    }
}
*/

void Outliner::showObjectGUI(const ofbx::Object& object)
{
    //std::string selectedNod_name ;
    const char* label = nullptr;
    switch (object.getType())
    {
    case ofbx::Object::Type::GEOMETRY: label = "geometry"; break;
    case ofbx::Object::Type::MESH: label = "mesh"; break;
    case ofbx::Object::Type::MATERIAL: label = "material"; break;
    case ofbx::Object::Type::ROOT: label = "root"; break;
    case ofbx::Object::Type::TEXTURE: label = "texture"; break;
    case ofbx::Object::Type::NULL_NODE: label = "null"; break;
    case ofbx::Object::Type::LIMB_NODE: label = "limb node"; break;
    case ofbx::Object::Type::NODE_ATTRIBUTE: label = "node attribute"; break;
    case ofbx::Object::Type::CLUSTER: label = "cluster"; break;
    case ofbx::Object::Type::SKIN: label = "skin"; break;
    case ofbx::Object::Type::ANIMATION_STACK: label = "animation stack"; break;
    case ofbx::Object::Type::ANIMATION_LAYER: label = "animation layer"; break;
    case ofbx::Object::Type::ANIMATION_CURVE: label = "animation curve"; break;
    case ofbx::Object::Type::ANIMATION_CURVE_NODE: label = "animation curve node"; break;
    default: assert(false); break;
    }

    ImGuiTreeNodeFlags flags = g_selected_object == &object ? ImGuiTreeNodeFlags_Selected : 0;
    // Add OpenOnArrow flag so only clicking the arrow toggles expand/collapse, not clicking the label
    flags |= ImGuiTreeNodeFlags_OpenOnArrow;
    
    char tmp[128];
    // Use renamed name if this is a root node with collision
    std::string displayName = object.name;
    if (object.getType() == ofbx::Object::Type::ROOT) {
        displayName = getRootNodeName(&object);
    }
    sprintf_s(tmp, "%" PRId64 " %s (%s)", object.id, displayName.c_str(), label);

    bool isOpen = ImGui::TreeNodeEx(tmp, flags);
    
    // Handle selection - clicking on the label (not the arrow) selects the item
    // IsItemClicked() returns true for any click on the item
    // IsItemToggledOpen() returns true only if the click toggled the open state (i.e., clicked arrow)
    if (ImGui::IsItemClicked()) {
        // If the item was toggled open, the click was on the arrow - don't change selection
        // If the item wasn't toggled, the click was on the label - select it
        if (!ImGui::IsItemToggledOpen()) {
            g_selected_object = &object;
            // Use renamed name if this is a root node with collision
            if (object.getType() == ofbx::Object::Type::ROOT) {
                mSelectedNod_name = getRootNodeName(&object);
            } else {
                mSelectedNod_name = g_selected_object->name;
            }
            
            // Log selection change
            LOG_INFO("Outliner: Selection changed");
            
            // Update selection index for keyboard navigation
            for (size_t i = 0; i < mSelectableObjects.size(); ++i) {
                if (mSelectableObjects[i] == &object) {
                    mCurrentSelectionIndex = static_cast<int>(i);
                    break;
                }
            }
            
            // AUTO-FOCUS: Trigger callback if set (used for auto-focus on selection change)
            // The callback will check if auto-focus is enabled and trigger camera framing
            if (mSelectionChangeCallback) {
                mSelectionChangeCallback();
            }
        }
    }
    
    if (isOpen)
    {
        int i = 0;
        while (ofbx::Object* child = object.resolveObjectLink(i))
        {
            //std::cout << "child " << child->name << std::endl;
            //mSelectedNod_name = child->name;
            showObjectGUI(*child);
            ++i;
        }
        if (object.getType() == ofbx::Object::Type::ANIMATION_CURVE) {
            showCurveGUI(object);
        }

        ImGui::TreePop();
    }
}


void Outliner::showObjectsGUI(const ofbx::IScene& scene)
{
    // Note: ImGui::Begin("Outliner") is already called in outlinerPanel()
    // So we don't call it here - this function just renders the tree content
    const ofbx::Object* root = scene.getRoot();

    if (root) showObjectGUI(*root);
    int count = scene.getAnimationStackCount();

    for (int i = 0; i < count; ++i)
    {
        const ofbx::Object* stack = scene.getAnimationStack(i);
        showObjectGUI(*stack);
    }
}


void Outliner::showCurveGUI(const ofbx::Object& object) {
    const ofbx::AnimationCurve& curve = static_cast<const ofbx::AnimationCurve&>(object);

    const int c = curve.getKeyCount();
    for (int i = 0; i < c; ++i) {
        const float t = (float)ofbx::fbxTimeToSeconds(curve.getKeyTime()[i]);
        const float v = curve.getKeyValue()[i];
        ImGui::Text("%fs: %f ", t, v);

    }
}

//-- get Selected Node --------------------
std::string Outliner::getSelectedNode()
{
    return mSelectedNod_name;
}

//-- get the effective root node name for a scene (renamed if collision) ----
std::string Outliner::getRootNodeName(int modelIndex) const
{
    if (modelIndex >= 0 && modelIndex < static_cast<int>(mFBXScenes.size())) {
        return mFBXScenes[modelIndex].getRootNodeName();
    }
    return "";
}

//-- get the effective root node name for an object ----
std::string Outliner::getRootNodeName(const ofbx::Object* rootObject) const
{
    if (!rootObject) return "";
    
    // Find which scene this root object belongs to
    for (const auto& sceneData : mFBXScenes) {
        if (sceneData.scene && sceneData.scene->getRoot() == rootObject) {
            return sceneData.getRootNodeName();
        }
    }
    
    // Not found in any scene, return original name
    return rootObject->name;
}

//-- get the rig root name for a model ----
std::string Outliner::getRigRootName(int modelIndex) const
{
    if (modelIndex >= 0 && modelIndex < static_cast<int>(mFBXScenes.size())) {
        return mFBXScenes[modelIndex].getRigRootName();
    }
    return "";
}

//-- get the FBX scene for a model index ----
const ofbx::IScene* Outliner::getScene(int modelIndex) const
{
    if (modelIndex >= 0 && modelIndex < static_cast<int>(mFBXScenes.size())) {
        return mFBXScenes[modelIndex].scene;
    }
    return nullptr;
}

//-- check if a root node name already exists in loaded scenes ----
bool Outliner::rootNodeNameExists(const std::string& name) const
{
    for (const auto& sceneData : mFBXScenes) {
        // Check both original and renamed names
        if (sceneData.rootNodeOriginalName == name || 
            sceneData.rootNodeRenamedName == name ||
            sceneData.getRootNodeName() == name) {
            return true;
        }
    }
    return false;
}

//-- generate a unique root node name (e.g., "RootNode01", "RootNode02", etc.) ----
std::string Outliner::generateUniqueRootNodeName(const std::string& baseName) const
{
    // Try base name with numbers: "RootNode01", "RootNode02", etc.
    for (int i = 1; i < 1000; ++i) {
        char buffer[256];
        sprintf_s(buffer, "%s%02d", baseName.c_str(), i);
        std::string candidateName = buffer;
        
        if (!rootNodeNameExists(candidateName)) {
            return candidateName;
        }
    }
    
    // Fallback: use timestamp or random suffix if we run out of numbers
    char buffer[256];
    sprintf_s(buffer, "%s_%d", baseName.c_str(), static_cast<int>(mFBXScenes.size()));
    return buffer;
}

//-- find an object by name in the scene hierarchy ----
// Recursively searches for an object with the given name
// Returns pointer to the object if found, nullptr otherwise
// Handles cases where object names have numeric prefixes (e.g., "1818483895296 Rig_GRP")
const ofbx::Object* Outliner::findObjectByName(const ofbx::Object* object, const std::string& name) const
{
    if (!object) {
        return nullptr;
    }
    
    std::string objectName = std::string(object->name);
    
    // Check for exact match
    if (objectName == name) {
        return object;
    }
    
    // Check if object name contains the search name (handles numeric prefixes)
    // This handles cases like "1818483895296 Rig_GRP" matching "Rig_GRP"
    if (objectName.find(name) != std::string::npos) {
        // Additional check: make sure the name appears at the end or after a space/number
        // This prevents false matches like "Rig_GRP_Other" matching "Rig_GRP"
        size_t pos = objectName.find(name);
        if (pos != std::string::npos) {
            // Check if the match is at the end or followed by a space/end of string
            size_t nameEnd = pos + name.length();
            if (nameEnd >= objectName.length() || objectName[nameEnd] == ' ' || objectName[nameEnd] == '\0') {
                return object;
            }
        }
    }
    
    // Recursively search children
    int i = 0;
    while (const ofbx::Object* child = const_cast<ofbx::Object*>(object)->resolveObjectLink(i)) {
        const ofbx::Object* result = findObjectByName(child, name);
        if (result != nullptr) {
            return result;
        }
        ++i;
    }
    
    return nullptr;
}

//-- set selection to RootNode (when model is selected via viewport) ----
// CHANGED: Always selects RootNode (or renamed RootNode like "RootNode01") instead of rig root
// This ensures viewport selection always selects the topmost root, regardless of rig root detection
// Selects the RootNode of the specified model's scene
// If modelIndex is -1 or invalid, selects the RootNode of the first scene
void Outliner::setSelectionToRoot(int modelIndex)
{
    // Safety check: Ensure mFBXScenes is not empty
    if (mFBXScenes.empty()) {
        std::cout << "[Outliner] WARNING: setSelectionToRoot called but no scenes loaded" << std::endl;
        return;
    }
    
    // If valid model index provided, select that model's RootNode
    if (modelIndex >= 0 && modelIndex < static_cast<int>(mFBXScenes.size())) {
        // Safety check: Ensure scene data exists and scene pointer is valid
        if (mFBXScenes[modelIndex].scene) {
            // Always select RootNode (topmost root), not rig root
            const ofbx::Object* root = mFBXScenes[modelIndex].scene->getRoot();
            if (root) {
                g_selected_object = root;
                // Use the renamed RootNode name (e.g., "RootNode01" if collision detected)
                mSelectedNod_name = mFBXScenes[modelIndex].getRootNodeName();
                
                // Safety check: Ensure we have a valid name
                if (mSelectedNod_name.empty()) {
                    std::cout << "[Outliner] WARNING: RootNode name is empty, using object name" << std::endl;
                    mSelectedNod_name = root->name;
                }
                
                // Update selection index for keyboard navigation
                for (size_t i = 0; i < mSelectableObjects.size(); ++i) {
                    if (mSelectableObjects[i] == root) {
                        mCurrentSelectionIndex = static_cast<int>(i);
                        break;
                    }
                }
                // Debug print removed for clean console output
                return;
            } else {
                std::cout << "[Outliner] WARNING: Scene root is null for model " << modelIndex << std::endl;
            }
        } else {
            std::cout << "[Outliner] WARNING: Scene pointer is null for model " << modelIndex << std::endl;
        }
    } else {
        std::cout << "[Outliner] WARNING: Invalid model index " << modelIndex << " (max: " << mFBXScenes.size() << ")" << std::endl;
    }
    
    // Fallback: Select RootNode from first scene
    if (!mFBXScenes.empty() && mFBXScenes[0].scene) {
        const ofbx::Object* root = mFBXScenes[0].scene->getRoot();
        if (root) {
            g_selected_object = root;
            mSelectedNod_name = mFBXScenes[0].getRootNodeName();
            
            // Update selection index for keyboard navigation
            for (size_t i = 0; i < mSelectableObjects.size(); ++i) {
                if (mSelectableObjects[i] == root) {
                    mCurrentSelectionIndex = static_cast<int>(i);
                    break;
                }
            }
            // Debug print removed for clean console output
            return;
        }
    }
    
    // Fallback to legacy single scene pointer
    if (ofbx_scene) {
        const ofbx::Object* root = ofbx_scene->getRoot();
        if (root) {
            g_selected_object = root;
            // Find the scene data for this root
            for (const auto& sceneData : mFBXScenes) {
                if (sceneData.scene == ofbx_scene) {
                    mSelectedNod_name = sceneData.getRootNodeName();
                    break;
                }
            }
            // Fallback to original name if not found
            if (mSelectedNod_name.empty()) {
                mSelectedNod_name = root->name;
            }
            
            // Update selection index for keyboard navigation
            for (size_t i = 0; i < mSelectableObjects.size(); ++i) {
                if (mSelectableObjects[i] == root) {
                    mCurrentSelectionIndex = static_cast<int>(i);
                    break;
                }
            }
        }
    }
}

//-- clear selection (deselect) ----
void Outliner::clearSelection()
{
    g_selected_object = nullptr;
    mSelectedNod_name = "";
    mCurrentSelectionIndex = -1;
}

void Outliner::setSelectionToNode(int modelIndex, const std::string& nodeName)
{
    if (modelIndex < 0 || modelIndex >= static_cast<int>(mFBXScenes.size())) {
        LOG_WARNING("[Outliner] Invalid model index " + std::to_string(modelIndex) + " for node selection");
        return;
    }
    
    if (!mFBXScenes[modelIndex].scene) {
        LOG_WARNING("[Outliner] Scene is null for model " + std::to_string(modelIndex));
        return;
    }
    
    const ofbx::Object* root = mFBXScenes[modelIndex].scene->getRoot();
    if (!root) {
        LOG_WARNING("[Outliner] Root object is null for model " + std::to_string(modelIndex));
        return;
    }
    
    // Find the object in the FBX hierarchy
    const ofbx::Object* foundObject = findObjectByName(root, nodeName);
    
    if (foundObject) {
        // Only log if selection actually changed (prevents duplicate logs on same click)
        bool selectionChanged = (g_selected_object != foundObject || mSelectedNod_name != foundObject->name);
        
        g_selected_object = foundObject;
        mSelectedNod_name = foundObject->name;
        
        // Update selection index for keyboard navigation
        for (size_t i = 0; i < mSelectableObjects.size(); ++i) {
            if (mSelectableObjects[i] == foundObject) {
                mCurrentSelectionIndex = static_cast<int>(i);
                break;
            }
        }
        
        // Only log when selection actually changes (production v2.0.0 - reduce log noise)
        if (selectionChanged) {
            LOG_INFO("[Outliner] Selection synced from Viewport: '" + nodeName + "'");
        }
        
        // Trigger auto-focus callback if set
        if (mSelectionChangeCallback) {
            mSelectionChangeCallback();
        }
    } else {
        LOG_WARNING("[Outliner] Viewport selected bone '" + nodeName + "' not found in FBX hierarchy.");
    }
}

//-- rebuild selectable objects list from all loaded scenes (for arrow key navigation) --------------------
void Outliner::rebuildSelectableObjectsList()
{
    mSelectableObjects.clear();
    mCurrentSelectionIndex = -1;
    
    // Collect selectable objects from all loaded scenes
    for (const auto& sceneData : mFBXScenes) {
        if (sceneData.scene) {
            // Collect all selectable objects from root
            const ofbx::Object* root = sceneData.scene->getRoot();
            if (root) {
                collectSelectableObjects(*root);
            }
            
            // Collect all selectable objects from animation stacks
            int count = sceneData.scene->getAnimationStackCount();
            for (int i = 0; i < count; ++i) {
                const ofbx::Object* stack = sceneData.scene->getAnimationStack(i);
                if (stack) {
                    collectSelectableObjects(*stack);
                }
            }
        }
    }
    
    // Find current selection index
    if (g_selected_object) {
        for (size_t i = 0; i < mSelectableObjects.size(); ++i) {
            if (mSelectableObjects[i] == g_selected_object) {
                mCurrentSelectionIndex = static_cast<int>(i);
                break;
            }
        }
    }
    
    // Removed debug print
}

//-- rebuild selectable objects list from single scene (legacy method, for backward compatibility) --------------------
void Outliner::rebuildSelectableObjectsList(const ofbx::IScene& scene)
{
    mSelectableObjects.clear();
    mCurrentSelectionIndex = -1;
    
    // Collect all selectable objects from root
    const ofbx::Object* root = scene.getRoot();
    if (root) {
        collectSelectableObjects(*root);
    }
    
    // Collect all selectable objects from animation stacks
    int count = scene.getAnimationStackCount();
    for (int i = 0; i < count; ++i) {
        const ofbx::Object* stack = scene.getAnimationStack(i);
        if (stack) {
            collectSelectableObjects(*stack);
        }
    }
    
    // Find current selection index
    if (g_selected_object) {
        for (size_t i = 0; i < mSelectableObjects.size(); ++i) {
            if (mSelectableObjects[i] == g_selected_object) {
                mCurrentSelectionIndex = static_cast<int>(i);
                break;
            }
        }
    }
}

//-- collect selectable objects recursively --------------------
void Outliner::collectSelectableObjects(const ofbx::Object& object)
{
    // Add this object to the list (only selectable objects like limb nodes, meshes, etc.)
    // Filter out animation curve nodes and other non-selectable items
    ofbx::Object::Type objType = object.getType();
    if (objType == ofbx::Object::Type::LIMB_NODE || 
        objType == ofbx::Object::Type::MESH ||
        objType == ofbx::Object::Type::GEOMETRY ||
        objType == ofbx::Object::Type::ROOT ||
        objType == ofbx::Object::Type::NULL_NODE ||
        objType == ofbx::Object::Type::ANIMATION_STACK) {
        mSelectableObjects.push_back(&object);
    }
    
    // Recursively collect children
    int i = 0;
    while (ofbx::Object* child = const_cast<ofbx::Object*>(&object)->resolveObjectLink(i)) {
        collectSelectableObjects(*child);
        ++i;
    }
}

//-- handle keyboard navigation (arrow keys) --------------------
void Outliner::handleKeyboardNavigation()
{
    // Only handle if Outliner is hovered OR Viewport is hovered
    if (!ImGui::IsWindowHovered() && !m_viewportHovered) {
        return;
    }
    
    // Use the existing Keyboard class which is already set up with GLFW
    // GLFW_KEY_UP = 265, GLFW_KEY_DOWN = 264 (defined in GLFW/glfw3.h)
    bool moveUp = Keyboard::keyWentDown(GLFW_KEY_UP);
    bool moveDown = Keyboard::keyWentDown(GLFW_KEY_DOWN);
    
    // Debug: Print when keys are pressed
    if (moveUp || moveDown) {
        std::cout << "[Outliner] Arrow key pressed - Up: " << moveUp << ", Down: " << moveDown << std::endl;
    }
    
    if (!moveUp && !moveDown) {
        return;
    }
    
    if (mSelectableObjects.empty()) {
        std::cout << "[Outliner] ERROR: Selectable objects list is empty!" << std::endl;
        return;
    }
    
    // Initialize selection index if not set
    if (mCurrentSelectionIndex < 0 && !mSelectableObjects.empty()) {
        // If no selection, start at first item
        mCurrentSelectionIndex = 0;
    }
    
    // Update selection index
    if (moveUp) {
        if (mCurrentSelectionIndex > 0) {
            mCurrentSelectionIndex--;
        } else {
            // Wrap to end
            mCurrentSelectionIndex = static_cast<int>(mSelectableObjects.size() - 1);
        }
    } else if (moveDown) {
        if (mCurrentSelectionIndex < static_cast<int>(mSelectableObjects.size() - 1)) {
            mCurrentSelectionIndex++;
        } else {
            // Wrap to beginning
            mCurrentSelectionIndex = 0;
        }
    }
    
    // Update selection
    if (mCurrentSelectionIndex >= 0 && mCurrentSelectionIndex < static_cast<int>(mSelectableObjects.size())) {
        g_selected_object = mSelectableObjects[mCurrentSelectionIndex];
        if (g_selected_object) {
            mSelectedNod_name = g_selected_object->name;
            // Trigger selection callback so Auto-Focus and Gizmo update immediately
            if (mSelectionChangeCallback) {
                mSelectionChangeCallback();
            }
        }
    }
}

//-- extract filename from path (helper method) ----
std::string Outliner::extractFileName(const std::string& path)
{
    if (path.empty()) {
        return "";
    }
    
    // Find the last path separator (works for both Windows and Unix)
    size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash != std::string::npos && lastSlash < path.length() - 1) {
        return path.substr(lastSlash + 1);
    }
    
    // No separator found, return the whole path
    return path;
}

//-- find which model index a selected object belongs to ----
// Helper function to recursively search for an object in a scene's hierarchy
// Returns true if the target object is found in the subtree starting from the given object
static bool findObjectInHierarchy(const ofbx::Object* startObject, const ofbx::Object* targetObject)
{
    if (!startObject || !targetObject) {
        return false;
    }
    
    // Check if this is the target object
    if (startObject == targetObject) {
        return true;
    }
    
    // Recursively search children
    int i = 0;
    while (ofbx::Object* child = const_cast<ofbx::Object*>(startObject)->resolveObjectLink(i)) {
        if (findObjectInHierarchy(child, targetObject)) {
            return true;
        }
        ++i;
    }
    
    return false;
}

// Searches through all loaded scenes to find which one contains the selected object
// Returns the index of the model (matching ModelManager index), or -1 if not found
// CRITICAL: Now searches the entire hierarchy, not just root nodes
// This ensures bones and other nodes can be correctly identified to their owning model
int Outliner::findModelIndexForSelectedObject() const
{
    if (!g_selected_object) {
        return -1;
    }
    
    // Search through all loaded scenes to find which one contains the selected object
    for (size_t i = 0; i < mFBXScenes.size(); ++i) {
        if (mFBXScenes[i].scene) {
            const ofbx::Object* root = mFBXScenes[i].scene->getRoot();
            if (root) {
                // Check if the selected object is the root (fast path)
                if (root == g_selected_object) {
                    return static_cast<int>(i);
                }
                
                // CRITICAL: Recursively search the entire hierarchy to find bones and other nodes
                // This ensures that selecting a bone in 'Character_B' correctly identifies its model
                // even if 'Character_A' was previously selected
                if (findObjectInHierarchy(root, g_selected_object)) {
                    return static_cast<int>(i);
                }
            }
        }
    }
    
    return -1;
}

