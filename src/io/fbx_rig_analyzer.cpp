#include "fbx_rig_analyzer.h"
#include "../utils/logger.h"
#include <algorithm>

namespace FBXRigAnalyzer {

    /**
     * Count the total number of descendants (children and their children) of an object.
     * 
     * This helper function recursively counts all descendants in the object hierarchy.
     * Used to identify the LIMB_NODE with the most descendants (typically the root bone like "Hips").
     * 
     * @param object The object to count descendants for
     * @return Total number of descendants (0 if object is null or has no children)
     */
    int countDescendants(const ofbx::Object* object) {
        if (!object) {
            return 0;
        }

        int count = 0;
        int i = 0;
        // Note: const_cast is necessary due to openFBX API design - resolveObjectLink returns non-const
        // even though it's a const method. This is a limitation of the openFBX library.
        while (const ofbx::Object* child = const_cast<ofbx::Object*>(object)->resolveObjectLink(i)) {
            count += 1 + countDescendants(child); // Count this child and all its descendants
            ++i;
        }

        return count;
    }

    /**
     * Find the first LIMB_NODE in the hierarchy, or all LIMB_NODES at the same depth.
     * 
     * This function finds LIMB_NODES and can collect multiple candidates at the same depth
     * for robust rig root detection in complex FBX files.
     * 
     * @param object The object to start searching from
     * @param collectAllAtDepth If true, collects all LIMB_NODES at the first depth where any are found
     * @param results Output vector to store found LIMB_NODES (used when collectAllAtDepth is true)
     * @return Pointer to the first LIMB_NODE found, or nullptr if none found
     * 
     * Note: Default arguments are specified in the header file declaration, not here.
     */
    const ofbx::Object* findFirstLimbNode(const ofbx::Object* object, bool collectAllAtDepth, std::vector<const ofbx::Object*>* results)
    {
        if (!object) {
            return nullptr;
        }

        // Check if this object is a LIMB_NODE (rig root/bone)
        if (object->getType() == ofbx::Object::Type::LIMB_NODE) {
            if (collectAllAtDepth && results) {
                results->push_back(object);
            }
            return object;
        }

        // Recursively search children
        int i = 0;
        // Note: const_cast is necessary due to openFBX API design - resolveObjectLink returns non-const
        // even though it's a const method. This is a limitation of the openFBX library.
        while (const ofbx::Object* child = const_cast<ofbx::Object*>(object)->resolveObjectLink(i)) {
            const ofbx::Object* result = findFirstLimbNode(child, collectAllAtDepth, results);
            if (result != nullptr && !collectAllAtDepth) {
                return result;
            }
            ++i;
        }

        return nullptr;
    }

    /**
     * Find the highest NULL_NODE that is a direct child of the Scene Root.
     * 
     * This function traverses up the parent chain to find NULL_NODE containers,
     * but specifically looks for the highest one that is a direct child of the Scene Root.
     * This ensures we get the main "Rig_GRP" even if there are nested group structures.
     * 
     * Algorithm:
     * 1. Traverse up the parent chain collecting all NULL_NODES
     * 2. Identify which NULL_NODE is a direct child of the Scene Root
     * 3. Return that NULL_NODE (or the first NULL_NODE found if none is a direct child of root)
     * 
     * @param object The object to start searching from (typically a LIMB_NODE)
     * @param sceneRoot The root of the FBX scene (used to identify direct children)
     * @return Pointer to the highest NULL_NODE that is a direct child of Scene Root, or first NULL_NODE found, or nullptr
     */
    const ofbx::Object* findParentNullNode(const ofbx::Object* object, const ofbx::Object* sceneRoot)
    {
        if (!object || !sceneRoot) {
            return nullptr;
        }

        // Get the parent of this object
        const ofbx::Object* parent = object->getParent();
        const ofbx::Object* highestNullNode = nullptr;
        
        // Traverse up the parent chain to find the highest NULL_NODE that is a direct child of Scene Root
        while (parent) {
            // Check if this parent is a NULL_NODE (typically a group container like "Rig_GRP")
            if (parent->getType() == ofbx::Object::Type::NULL_NODE) {
                // Check if this NULL_NODE is a direct child of the Scene Root
                // Note: const_cast is necessary due to openFBX API design - getParent() returns non-const
                // even though it's a const method. This is a limitation of the openFBX library.
                const ofbx::Object* nullNodeParent = const_cast<ofbx::Object*>(parent)->getParent();
                
                if (nullNodeParent == sceneRoot) {
                    // This is a direct child of Scene Root - this is the one we want
                    return parent;
                }
                
                // Store the first NULL_NODE found as fallback (in case none is a direct child of root)
                if (!highestNullNode) {
                    highestNullNode = parent;
                }
            }
            
            // Continue up the parent chain
            parent = parent->getParent();
        }

        // Return the first NULL_NODE found (fallback), or nullptr if none found
        return highestNullNode;
    }

    /**
     * Find the rig root container in an FBX scene with robust hierarchy detection.
     * 
     * This function implements a multi-step algorithm to correctly identify the rig root
     * even in complex FBX files with nested group structures:
     * 
     * 1. Find all LIMB_NODES at the first depth where any are found
     * 2. If multiple LIMB_NODES exist at the same depth, select the one with the most descendants
     *    (this is typically the 'Hips' or 'Pelvis' bone which has the entire skeleton as children)
     * 3. Traverse up the parent chain to find the highest NULL_NODE that is a direct child of Scene Root
     * 4. Return the NULL_NODE name if found, otherwise return the selected LIMB_NODE name
     * 
     * Fallback Logic:
     * - If no NULL_NODE container is found, returns the LIMB_NODE name (handles rigs without groups)
     * - If multiple LIMB_NODES at same depth, selects the one with most descendants (most likely root bone)
     * - If no LIMB_NODES found, returns empty string (no rig detected)
     * 
     * @param scene The FBX scene to analyze
     * @return The name of the rig root container (NULL_NODE), or selected LIMB_NODE if no container found, or empty string if no rig found
     */
    std::string findRigRoot(const ofbx::IScene* scene)
    {
        if (!scene) {
            return "";
        }

        // Start searching from the scene root
        const ofbx::Object* root = scene->getRoot();
        if (!root) {
            return "";
        }

        // Step 1: Find all LIMB_NODES at the first depth where any are found
        // This allows us to handle cases where multiple bones exist at the same level
        std::vector<const ofbx::Object*> limbNodesAtFirstDepth;
        const ofbx::Object* firstLimbNode = findFirstLimbNode(root, true, &limbNodesAtFirstDepth);

        if (!firstLimbNode || limbNodesAtFirstDepth.empty()) {
            // No rig found
            return "";
        }

        // Step 2: If multiple LIMB_NODES found at the same depth, select the one with the most descendants
        // This is typically the 'Hips' or 'Pelvis' bone which has the entire skeleton hierarchy as children
        const ofbx::Object* selectedLimbNode = firstLimbNode;
        
        if (limbNodesAtFirstDepth.size() > 1) {
            // Multiple LIMB_NODES at same depth - find the one with most descendants
            int maxDescendants = -1;
            for (const ofbx::Object* limbNode : limbNodesAtFirstDepth) {
                int descendantCount = countDescendants(limbNode);
                if (descendantCount > maxDescendants) {
                    maxDescendants = descendantCount;
                    selectedLimbNode = limbNode;
                }
            }
            
            LOG_DEBUG("[FBXRigAnalyzer] Multiple LIMB_NODES at same depth - selected '" + 
                      std::string(selectedLimbNode->name) + "' with " + std::to_string(maxDescendants) + " descendants");
        }

        // Step 3: Try to find the parent NULL_NODE container (e.g., "Rig_GRP")
        // Look for the highest NULL_NODE that is a direct child of Scene Root
        const ofbx::Object* parentNullNode = findParentNullNode(selectedLimbNode, root);

        if (parentNullNode) {
            // Return the parent group name (e.g., "Rig_GRP")
            LOG_DEBUG("[FBXRigAnalyzer] Found rig root: '" + std::string(parentNullNode->name) + "' (NULL_NODE container)");
            return std::string(parentNullNode->name);
        }

        // Fallback: If no parent NULL_NODE found, return the selected LIMB_NODE name
        // This handles cases where the rig structure doesn't have a containing group
        LOG_DEBUG("[FBXRigAnalyzer] No NULL_NODE container found - using LIMB_NODE: '" + std::string(selectedLimbNode->name) + "'");
        return std::string(selectedLimbNode->name);
    }

} // namespace FBXRigAnalyzer
