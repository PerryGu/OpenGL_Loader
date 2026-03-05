#include "viewport_selection.h"
#include "graphics/raycast.h"
#include "io/scene.h"
#include "graphics/model_manager.h"
#include "io/camera.h"
#include "io/app_settings.h"
#include "graphics/model.h"  // For UI_data
#include "graphics/math3D.h"  // For matrix conversion utilities
#include "utils/logger.h"  // For LOG_INFO
#include <imgui/imgui.h>
#include <limits>
#include <string>

void ViewportSelectionManager::handleSelectionClick(float viewportMouseX, float viewportMouseY,
                                                     float viewportWidth, float viewportHeight,
                                                     Camera& camera, float farClipPlane,
                                                     ModelManager& modelManager, Scene& scene,
                                                     const std::map<int, glm::mat4>& modelRootNodeMatrices) {
    // Get view and projection matrices for raycast (same as used for rendering)
    glm::mat4 view = camera.getViewMatrix();
    // NOTE: Removed glm::rotate(-25 degrees) to match rendering (no rotation offset)
    
    // Calculate aspect ratio from viewport dimensions
    float aspectRatio = (viewportWidth > 0 && viewportHeight > 0) ? 
                       viewportWidth / viewportHeight : 
                       16.0f / 9.0f;
    
    glm::mat4 projection = glm::perspective(glm::radians(camera.getZoom()), aspectRatio, 0.1f, farClipPlane);
    
    // CRITICAL: Since framebuffer now matches content size, no scaling needed
    // The viewport image size should match the content size (framebuffer is resized to match)
    // Use content size directly for raycast coordinates
    float contentMouseX = viewportMouseX;
    float contentMouseY = viewportMouseY;
    
    // Create ray from mouse position using content region coordinates
    // This matches the exact viewport dimensions used for rendering
        Ray ray = Raycast::screenToWorldRay(contentMouseX, contentMouseY,
                                            static_cast<int>(viewportWidth),
                                            static_cast<int>(viewportHeight),
                                            view, projection);
    // Verbose debug message commented out for production (v2.0.0)
    // LOG_INFO("[Raycast] Mouse clicked in Viewport. Ray casted.");
    
    // IMPORTANT OPTIMIZATION: Use the passed modelRootNodeMatrices instead of recalculating
    // The matrices are already calculated in syncTransforms() and passed here
    
    // Test ray against all model bounding boxes
    // CRITICAL: Bounding boxes MUST be calculated for selection (even if visibility toggle is off)
    // With bone-based calculation (O(Bones) complexity), this is now ultra-fast
    // Selection must work regardless of bounding box visibility toggle
    int closestModelIndex = -1;
    float closestDistance = std::numeric_limits<float>::max();
    
    // Calculate bounding boxes for selection (always, regardless of visibility toggle)
    // This is called only on mouse click, not every frame
    for (size_t i = 0; i < modelManager.getModelCount(); i++) {
        ModelInstance* instance = modelManager.getModel(static_cast<int>(i));
        if (instance) {
            // Calculate bounding box on-demand for selection
            // This is called only on mouse click, not every frame
            glm::vec3 bboxMin, bboxMax;
            
            // Check if we have cached bone transforms (from the rendering pass)
            // If bounding boxes are disabled, we might not have cached transforms, so we need to calculate them
            bool useBoneAwareBBox = false;
            std::vector<glm::mat4> boneTransforms;
            
            if (instance->boneTransformsCached && !instance->cachedBoneTransforms.empty() && 
                instance->model.getFileExtension() != "") {
                // Use cached bone transforms if available (most efficient)
                useBoneAwareBBox = true;
                boneTransforms = instance->cachedBoneTransforms;
            } else if (instance->model.getFileExtension() != "") {
                // Bounding boxes are disabled, so bone transforms weren't cached
                // We need to calculate them temporarily for accurate selection
                // This is more expensive but necessary for correct selection on animated models
                // Create a temporary UI_data for bone transform calculation
                // (We don't need bone rotations from PropertyPanel for selection, just animation)
                UI_data tempUI_data;
                boneTransforms.clear();
                instance->model.BoneTransform(instance->animationTime, boneTransforms, tempUI_data);
                if (!boneTransforms.empty()) {
                    useBoneAwareBBox = true;
                }
            }
            
            // Get bounding box (with or without bone transformations)
            if (useBoneAwareBBox) {
                instance->model.getBoundingBoxWithBones(bboxMin, bboxMax, boneTransforms);
            } else {
                instance->model.getBoundingBox(bboxMin, bboxMax);
            }
            
            // CRITICAL FIX: Transform bounding box to world space using this model's RootNode transform
            // ALL models now use per-model RootNode transforms, not just the selected one
            // The bounding box is calculated in local space, but the model is rendered in world space
            // with RootNode transform applied. We must transform the bounding box to match.
            auto matrixIt = modelRootNodeMatrices.find(static_cast<int>(i));
            if (matrixIt != modelRootNodeMatrices.end()) {
                // This model has a RootNode transform - apply it to bounding box corners
                // Transform all 8 corners from local space to world space, then recalculate AABB
                glm::mat4 rootNodeTransform = matrixIt->second;
                
                // Transform bounding box corners from local space to world space
                // Get all 8 corners of the bounding box
                glm::vec3 corners[8] = {
                    glm::vec3(bboxMin.x, bboxMin.y, bboxMin.z),
                    glm::vec3(bboxMax.x, bboxMin.y, bboxMin.z),
                    glm::vec3(bboxMin.x, bboxMax.y, bboxMin.z),
                    glm::vec3(bboxMax.x, bboxMax.y, bboxMin.z),
                    glm::vec3(bboxMin.x, bboxMin.y, bboxMax.z),
                    glm::vec3(bboxMax.x, bboxMin.y, bboxMax.z),
                    glm::vec3(bboxMin.x, bboxMax.y, bboxMax.z),
                    glm::vec3(bboxMax.x, bboxMax.y, bboxMax.z)
                };
                
                // Transform all corners to world space using RootNode transform
                for (int j = 0; j < 8; j++) {
                    corners[j] = glm::vec3(rootNodeTransform * glm::vec4(corners[j], 1.0f));
                }
                
                // Find new min/max in world space (axis-aligned bounding box in world space)
                bboxMin = bboxMax = corners[0];
                for (int j = 1; j < 8; j++) {
                    bboxMin = glm::min(bboxMin, corners[j]);
                    bboxMax = glm::max(bboxMax, corners[j]);
                }
            }
            // If no RootNode transform found, bounding box is already in local space (identity transform)
            
            // Test ray-box intersection (now in correct space - world space)
            float t;
            if (Raycast::rayIntersectsAABB(ray, bboxMin, bboxMax, t)) {
                // Found intersection - keep track of closest one
                if (t < closestDistance && t > 0.0f) {
                    closestDistance = t;
                    closestModelIndex = static_cast<int>(i);
                    // Verbose debug message commented out for production (v2.0.0)
                    // LOG_INFO("[Raycast] Hit Bounding Box of model " + std::to_string(i) + " at distance " + std::to_string(t));
                }
            }
        }
    }
    
    // Select the closest model (if any)
    if (closestModelIndex >= 0) {
        modelManager.setSelectedModel(closestModelIndex);
        ModelInstance* selectedInstance = modelManager.getSelectedModel();
        if (selectedInstance) {
            // BONE PICKING: If skeleton is visible, check for bone intersections
            bool bonePicked = false;
            if (selectedInstance->model.m_showSkeleton) {
                // Get the model's RootNode transform matrix (same as used for rendering)
                glm::mat4 modelMatrix = glm::mat4(1.0f);
                auto matrixIt = modelRootNodeMatrices.find(closestModelIndex);
                if (matrixIt != modelRootNodeMatrices.end()) {
                    modelMatrix = matrixIt->second;
                }
                
                // Use Model's public pickBone method (maintains encapsulation)
                bonePicked = selectedInstance->model.pickBone(ray, modelMatrix);
                if (bonePicked) {
                    // Sync outliner selection to the picked bone
                    scene.getUIManager().getOutliner().setSelectionToNode(closestModelIndex, selectedInstance->model.m_selectedNodeName);
                } else {
                    // Verbose debug message commented out for production (v2.0.0)
                    // LOG_INFO("[Raycast] FAILED: Ray passed through bounding box but missed all bones.");
                }
            } else {
                // Verbose debug message commented out for production (v2.0.0)
                // LOG_INFO("[Raycast] Model selected but skeleton not visible (m_showSkeleton = false)");
            }
            
            // Update outliner to show root node of selected model (only if bone picking didn't succeed)
            // This selects the root node in the outliner hierarchy
            // The initialization will happen in the main loop's RootNode handler
            if (!bonePicked) {
                scene.getUIManager().getOutliner().setSelectionToRoot(closestModelIndex);
            }
        }
        
        // AUTO-FOCUS: If auto-focus is enabled, trigger camera framing immediately on selection change
        // CRITICAL: Only trigger if selection actually changed (prevents infinite loop/flickering)
        scene.checkAndTriggerAutoFocusOnSelectionChange();
    } else {
        // Clicked on empty space - deselect
        // Save current model's RootNode transforms before deselecting
        int currentModelIndex = modelManager.getSelectedModelIndex();
        if (currentModelIndex >= 0) {
            std::string currentSelectedNode = scene.getUIManager().getOutliner().getSelectedNode();
            // RootNode transforms are automatically saved to bone maps when values change
            // No need to explicitly save on deselection - bone maps handle it
        }
        // Reset selection tracking variables on deselection
        // This ensures auto-focus will trigger again if the same item is selected later
        scene.resetSelectionTracking();
        
        modelManager.setSelectedModel(-1);
        // Clear outliner selection
        scene.getUIManager().getOutliner().clearSelection();
    }
}
