#ifndef OUTLINER_H
#define OUTLINER_H

#include <string>
#include <sstream>
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include <inttypes.h>
#include <iostream>

#include "../../imgui/imgui.h"
#include "ofbx.h"


template <int N>
void toString(ofbx::DataView view, char(&out)[N])
{
    int len = int(view.end - view.begin);
    if (len > sizeof(out) - 1) len = sizeof(out) - 1;
    strncpy(out, (const char*)view.begin, len);
    out[len] = 0;
}



template <int N>
void catProperty(char(&out)[N], const ofbx::IElementProperty& prop)
{
    char tmp[128];
    switch (prop.getType())
    {
    case ofbx::IElementProperty::DOUBLE: sprintf_s(tmp, "%f", prop.getValue().toDouble()); break;
    case ofbx::IElementProperty::LONG: sprintf_s(tmp, "%" PRId64, prop.getValue().toU64()); break;
    case ofbx::IElementProperty::INTEGER: sprintf_s(tmp, "%d", prop.getValue().toInt()); break;
    case ofbx::IElementProperty::STRING: prop.getValue().toString(tmp); break;
    default: sprintf_s(tmp, "Type: %c", (char)prop.getType()); break;
    }
    strcat_s(out, tmp);
}


// Structure to hold FBX scene data with metadata
struct FBXSceneData {
    ofbx::IScene* scene = nullptr;
    std::string filePath;
    std::string displayName;  // Filename extracted from path
    std::string rootNodeOriginalName;  // Original root node name from FBX file
    std::string rootNodeRenamedName;   // Renamed root node name (if collision detected)
    std::string rigRootName;           // Rig root container name (e.g., "Rig_GRP") - found during load
    
    FBXSceneData() : scene(nullptr) {}
    FBXSceneData(ofbx::IScene* s, const std::string& path, const std::string& name)
        : scene(s), filePath(path), displayName(name) {}
    
    // Get the effective root node name (renamed if collision, original otherwise)
    std::string getRootNodeName() const {
        return rootNodeRenamedName.empty() ? rootNodeOriginalName : rootNodeRenamedName;
    }
    
    // Get the rig root name (parent container of the rig, e.g., "Rig_GRP")
    // Returns empty string if no rig root was found
    std::string getRigRootName() const {
        return rigRootName;
    }
};

class Outliner {

public:

    void outlinerPanel(bool* p_open);
   
    //== load FBX stuff =======================
    typedef unsigned int u32;
    // Legacy single scene pointer (kept for backward compatibility, but now using mFBXScenes vector)
    // DEPRECATED: Use mFBXScenes vector instead
    ofbx::IScene* ofbx_scene = nullptr;
    const ofbx::IElement* g_selected_element = nullptr;
    const ofbx::Object* g_selected_object = nullptr;

    void loadFBXfile(std::string filePath);
    
    // Remove a specific scene by index (when a model is removed)
    void removeFBXScene(int modelIndex);
    //void showGUI(const ofbx::IElement& parent);
    void showObjectGUI(const ofbx::Object& object);
    void showObjectsGUI(const ofbx::IScene& scene);
    static void showCurveGUI(const ofbx::Object& object);
    std::string getSelectedNode();
    
    //-- set selection from external source (e.g., viewport raycast) ----
    // Selects the root node of the specified model's scene when a model is selected via viewport
    // If modelIndex is -1 or invalid, selects the root of the first scene
    void setSelectionToRoot(int modelIndex = -1);
    
    //-- set selection to specific node by name (e.g., bone picking from viewport) ----
    // Searches for a node with the given name in the specified model's scene and selects it
    // This bridges viewport bone picking (string name) to outliner selection (object pointer)
    void setSelectionToNode(int modelIndex, const std::string& nodeName);
    
    //-- clear selection (deselect) ----
    void clearSelection();
    
    //-- clear outliner (remove loaded scene) ----
    void clear();
    
    //-- find which model index a selected object belongs to ----
    // Returns the index of the model that contains the selected object, or -1 if not found
    // This is used to update model selection when RootNode is selected in outliner
    int findModelIndexForSelectedObject() const;
    
    //-- get the effective root node name for a scene (renamed if collision) ----
    // Returns the renamed name if collision was detected, original name otherwise
    std::string getRootNodeName(int modelIndex) const;
    
    //-- get the effective root node name for an object ----
    // Returns the renamed name if the object is a root node with collision, original name otherwise
    std::string getRootNodeName(const ofbx::Object* rootObject) const;
    
    //-- get the rig root name for a model ----
    // Returns the rig root container name (e.g., "Rig_GRP") for the specified model index
    // Returns empty string if no rig root was found or model index is invalid
    std::string getRigRootName(int modelIndex) const;
    
    //-- find an object by name in the scene hierarchy ----
    // Recursively searches for an object with the given name starting from the given object
    // Returns pointer to the object if found, nullptr otherwise
    const ofbx::Object* findObjectByName(const ofbx::Object* object, const std::string& name) const;
    
    //-- get the FBX scene for a model index ----
    // Returns the scene pointer for the specified model index, or nullptr if invalid
    const ofbx::IScene* getScene(int modelIndex) const;
    
    //-- set callback for selection change (used for auto-focus) ----
    // This callback is called when a node is selected in the outliner
    // The callback should trigger camera framing if auto-focus is enabled
    void setSelectionChangeCallback(std::function<void()> callback) { mSelectionChangeCallback = callback; }
    
    //-- set viewport hover state (enables keyboard navigation from viewport) ----
    // Called by ViewportPanel to indicate when the viewport is hovered
    // This allows arrow key navigation to work when hovering over the viewport
    void setViewportHovered(bool hovered) { m_viewportHovered = hovered; }



private:

    std::string mSelectedNod_name = "";
    glm::vec3 mSliderRotations = glm::vec3(0.0f, 0.0f, 0.0f);
    
    // Multiple FBX scenes support (one per loaded model)
    std::vector<FBXSceneData> mFBXScenes;  // Vector of all loaded FBX scenes
    
    // For arrow key navigation
    std::vector<const ofbx::Object*> mSelectableObjects;  // Flat list of all selectable objects in order
    int mCurrentSelectionIndex = -1;  // Index of currently selected object in the list
    bool mNeedsRebuildList = true;  // Flag to rebuild the list when scene changes
    
    // Helper methods for navigation
    void rebuildSelectableObjectsList();  // Rebuilds list from all scenes
    void rebuildSelectableObjectsList(const ofbx::IScene& scene);  // Rebuilds from single scene (legacy)
    void collectSelectableObjects(const ofbx::Object& object);
    void handleKeyboardNavigation();
    
    // Helper to extract filename from path
    std::string extractFileName(const std::string& path);
    
    // Helper to check if a root node name already exists in loaded scenes
    bool rootNodeNameExists(const std::string& name) const;
    
    // Helper to generate a unique root node name (e.g., "RootNode01", "RootNode02", etc.)
    std::string generateUniqueRootNodeName(const std::string& baseName) const;
    
    // Callback function for selection change (used for auto-focus)
    std::function<void()> mSelectionChangeCallback;
    
    // Viewport hover state (enables keyboard navigation from viewport)
    bool m_viewportHovered = false;
};

#endif