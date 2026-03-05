#ifndef FBX_RIG_ANALYZER_H
#define FBX_RIG_ANALYZER_H

#include <string>
#include <vector>
#include "ofbx.h"

/**
 * FBX Rig Analyzer
 * 
 * Utility functions for analyzing FBX rig structures with robust hierarchy detection.
 * Provides functionality to find rig roots and analyze skeleton hierarchies, handling
 * complex FBX files with nested group structures and multiple bones at the same depth.
 * 
 * Key Features:
 * - Finds the highest NULL_NODE that is a direct child of Scene Root (main "Rig_GRP")
 * - Handles multiple LIMB_NODES at the same depth by selecting the one with most descendants
 * - Robust fallback logic for rigs without group containers
 */
namespace FBXRigAnalyzer {

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
    std::string findRigRoot(const ofbx::IScene* scene);

    /**
     * Recursively search for the first LIMB_NODE in the object hierarchy.
     * 
     * This is a helper function used by findRigRoot() to traverse the scene tree.
     * Can optionally collect all LIMB_NODES at the first depth where any are found
     * for robust rig root detection in complex FBX files.
     * 
     * @param object The object to start searching from
     * @param collectAllAtDepth If true, collects all LIMB_NODES at the first depth where any are found
     * @param results Output vector to store found LIMB_NODES (used when collectAllAtDepth is true)
     * @return Pointer to the first LIMB_NODE found, or nullptr if none found
     */
    const ofbx::Object* findFirstLimbNode(const ofbx::Object* object, bool collectAllAtDepth = false, std::vector<const ofbx::Object*>* results = nullptr);

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
     * Fallback Logic:
     * - If no NULL_NODE is a direct child of Scene Root, returns the first NULL_NODE found
     * - If no NULL_NODE found at all, returns nullptr
     * 
     * @param object The object to start searching from (typically a LIMB_NODE)
     * @param sceneRoot The root of the FBX scene (used to identify direct children)
     * @return Pointer to the highest NULL_NODE that is a direct child of Scene Root, or first NULL_NODE found, or nullptr
     */
    const ofbx::Object* findParentNullNode(const ofbx::Object* object, const ofbx::Object* sceneRoot);

} // namespace FBXRigAnalyzer

#endif // FBX_RIG_ANALYZER_H
