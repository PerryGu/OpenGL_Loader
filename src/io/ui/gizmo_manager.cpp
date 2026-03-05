#define NOMINMAX  // Prevent Windows.h from defining min/max macros
#include "gizmo_manager.h"
#include "../keyboard.h"
#include "../../graphics/model_manager.h"
#include "../ui/outliner.h"
#include "../ui/property_panel.h"
#include "../../graphics/model.h"
#include "../../graphics/math3D.h"
#include "../../graphics/utils.h"
#include "../../graphics/raycast.h"
#include "../app_settings.h"
#include <imgui/imgui.h>
#include <ImGuizmo/ImGuizmo.h>
#include <iostream>

GizmoManager::GizmoManager() {
    // Initialize default values
    mGizmoUsing = false;
    mGizmoOver = false;
    mGizmoWasUsing = false;
    mGizmoInitialStateStored = false;
    mStartMouseWorldPos = glm::vec3(0.0f);
    mStartNodeLocalTranslation = glm::vec3(0.0f);
    mStartParentInverseMatrix = glm::mat4(1.0f);
    mStartParentWorldQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    mStartLocalQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    mWorldRotationAxis = glm::vec3(1.0f, 0.0f, 0.0f);
    mLocalRotationAxis = glm::vec3(1.0f, 0.0f, 0.0f);
    mInitialGizmoRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    mCurrentLocalRotationQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    mInitialNodeName = "";
    mGizmoOperation = ImGuizmo::TRANSLATE;
    mGizmoSize = 0.1f;
    mGizmoWorldPosition = glm::vec3(0.0f);
    mGizmoPositionValid = false;
}

void GizmoManager::handleKeyboardShortcuts(ModelManager* modelManager, bool mouseOverViewport) {
    // Early exit: Only process shortcuts if a model is selected
    // This prevents unnecessary processing and avoids conflicts when no model is selected
    if (modelManager == nullptr || modelManager->getSelectedModel() == nullptr) {
        return;
    }
    
    // Check if viewport window is focused (to avoid conflicts with text input in other windows)
    // If another ImGui window wants keyboard input (e.g., user typing in a text field),
    // we should not process gizmo shortcuts to prevent interference
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard && !mouseOverViewport) {
        return;  // Don't process if another ImGui window wants keyboard input
    }
    
    //==================================================================================
    // Maya-style Operation Mode Shortcuts
    //==================================================================================
    // These shortcuts match Autodesk Maya's standard transform gizmo controls:
    //   W = Translate (Move) - Move objects along X/Y/Z axes
    //   E = Rotate - Rotate objects around X/Y/Z axes
    //   R = Scale - Scale objects along X/Y/Z axes
    //==================================================================================
    if (Keyboard::keyWentDown(GLFW_KEY_W)) {
        mGizmoOperation = ImGuizmo::TRANSLATE;
        // Debug print removed for clean console output
    }
    else if (Keyboard::keyWentDown(GLFW_KEY_E)) {
        mGizmoOperation = ImGuizmo::ROTATE;
        // Debug print removed for clean console output
    }
    else if (Keyboard::keyWentDown(GLFW_KEY_R)) {
        mGizmoOperation = ImGuizmo::SCALE;
        // Debug print removed for clean console output
    }
    
    //==================================================================================
    // Gizmo Size Adjustment Shortcuts
    //==================================================================================
    // Adjust the visual size of the gizmo in the viewport:
    //   + (Plus key or Numpad +) - Increase gizmo size
    //   - (Minus key or Numpad -) - Decrease gizmo size
    // 
    // Size is stored in clip space (0.01 to 1.0 range)
    // The size is clamped to prevent the gizmo from becoming too small or too large
    //==================================================================================
    if (Keyboard::keyWentDown(GLFW_KEY_EQUAL) || Keyboard::keyWentDown(GLFW_KEY_KP_ADD)) {
        mGizmoSize = std::min(mGizmoSize + 0.01f, 1.0f);  // Clamp to max 1.0
        ImGuizmo::SetGizmoSizeClipSpace(mGizmoSize);
        // Debug print removed for clean console output
    }
    else if (Keyboard::keyWentDown(GLFW_KEY_MINUS) || Keyboard::keyWentDown(GLFW_KEY_KP_SUBTRACT)) {
        mGizmoSize = std::max(mGizmoSize - 0.01f, 0.01f);  // Clamp to min 0.01
        ImGuizmo::SetGizmoSizeClipSpace(mGizmoSize);
        // Debug print removed for clean console output
    }
}

void GizmoManager::renderGizmo(ModelManager* modelManager, Outliner& outliner, PropertyPanel& propertyPanel,
                                const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix,
                                ImVec2 viewportPos, ImVec2 viewportSize, bool& cameraFramingRequested) {
    // Get selected node from outliner
    std::string selectedNode = outliner.getSelectedNode();
    
    if (!selectedNode.empty() && modelManager != nullptr) {
        // CRITICAL: Strictly use getSelectedModelIndex() - no fallback to first model
        // This ensures strict isolation between models - no data leaks
        int selectedModelIndex = modelManager->getSelectedModelIndex();
        
        // Only proceed if a valid model is selected (no fallback to first model)
        if (selectedModelIndex >= 0 && selectedModelIndex < static_cast<int>(modelManager->getModelCount())) {
            ModelInstance* selectedModel = modelManager->getModel(selectedModelIndex);
            
            if (selectedModel != nullptr) {
                // CACHE OUTLINER NAMES TO PREVENT PER-FRAME TREE SEARCHES
                static std::string cachedRigRootName = "";
                static std::string cachedModelRootNodeName = "";
                static int lastModelIndexForNames = -1;
                
                if (selectedModelIndex != lastModelIndexForNames) {
                    cachedRigRootName = outliner.getRigRootName(selectedModelIndex);
                    cachedModelRootNodeName = outliner.getRootNodeName(selectedModelIndex);
                    lastModelIndexForNames = selectedModelIndex;
                }
                
                // LOG: Track gizmo attachment (only print when node changes)
                static std::string lastLoggedGizmoNode = "";
                if (lastLoggedGizmoNode != selectedNode) {
                    std::cout << "LOG: Gizmo active on: " << selectedNode << std::endl;
                    lastLoggedGizmoNode = selectedNode;
                }
                
                // Set ImGuizmo to draw in this window
                ImGuizmo::SetDrawlist();
                
                // Set the rect for ImGuizmo (use provided viewport position and size)
                ImGuizmo::SetRect(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);
                
                // Convert view and projection matrices to float arrays for ImGuizmo
                // Using math3D utility functions for matrix conversion
                float viewMatrixArray[16];
                float projectionMatrixArray[16];
                glmMat4ToFloat16(viewMatrix, viewMatrixArray);
                glmMat4ToFloat16(projectionMatrix, projectionMatrixArray);
                
                // CHANGED: Check if rig root OR RootNode is selected
                // CRITICAL: selectedModelIndex is already set above - use it directly
                // This ensures we're working with the correct model (strict isolation)
                // OPTIMIZED: Use cached name instead of per-frame tree search
                std::string rigRootName = cachedRigRootName;
                
                // Check if the selected node is RootNode (including renamed ones like "RootNode01")
                // RootNode renaming is cosmetic (UI only), but we need to handle it like a rig root
                bool isRootNode = (selectedNode.find("Root") != std::string::npos || 
                                   selectedNode == "RootNode");
                
                // Check if the selected node is the rig root (e.g., "Rig_GRP")
                // The selected node name might have a numeric prefix (e.g., "1818483895296 Rig_GRP"),
                // so we check if it contains the rig root name or matches exactly
                bool isRigRoot = false;
                if (!rigRootName.empty()) {
                    // Check for exact match or if selected node contains the rig root name
                    // (handles cases where object name has numeric prefix like "1818483895296 Rig_GRP")
                    isRigRoot = (selectedNode == rigRootName || 
                                selectedNode.find(rigRootName) != std::string::npos);
                    
                    // Also verify by checking if the selected object is actually the rig root object
                    // This is more reliable than name matching alone
                    if (isRigRoot && selectedModelIndex >= 0) {
                        const ofbx::Object* selectedObject = outliner.g_selected_object;
                        if (selectedObject) {
                            // Get the actual rig root object from the scene
                            // We need to access the scene from mFBXScenes - let's use findObjectByName
                            // to verify the selected object is the rig root
                            const ofbx::Object* sceneRoot = nullptr;
                            // Find the scene root for this model index
                            // We'll search through the outliner's scenes to find the right one
                            // For now, let's use a simpler approach: check if selected object's name contains rig root name
                            std::string selectedObjectName = std::string(selectedObject->name);
                            // Verify the selected object name contains the rig root name
                            if (selectedObjectName.find(rigRootName) == std::string::npos && 
                                selectedObjectName != rigRootName) {
                                // Selected object name doesn't match rig root name
                                isRigRoot = false;
                            }
                        }
                    }
                }
                
                // CRITICAL: Treat RootNode (including renamed ones) as rig root for gizmo positioning
                // This ensures RootNode01 uses PropertyPanel transforms for gizmo, not bone transforms
                if (isRootNode) {
                    isRigRoot = true;  // RootNode should use PropertyPanel transforms for gizmo
                }
                
                glm::mat4 modelMatrix = glm::mat4(1.0f);
                
                if (isRigRoot) {
                    // For rig root, use PropertyPanel transforms (world space)
                    glm::vec3 translation = propertyPanel.getSliderTrans_update();
                    glm::vec3 rotationEuler = propertyPanel.getSliderRot_update();  // Euler angles in degrees
                    glm::vec3 scale = propertyPanel.getSliderScale_update();
                    
                    // Convert Euler angles (degrees) to quaternion
                    glm::quat rotation = glm::quat(glm::radians(rotationEuler));
                    
                    // Build matrix from PropertyPanel transforms
                    modelMatrix = glm::translate(modelMatrix, translation);
                    modelMatrix = modelMatrix * glm::mat4_cast(rotation);
                    modelMatrix = glm::scale(modelMatrix, scale);
                } else {
                    // CRITICAL FIX: For bone nodes, position gizmo at the bone's ACTUAL world position
                    // The bone's position from BoneGlobalTransform is in MODEL SPACE (relative to model origin)
                    // When RootNode is moved, we need to transform the bone's model-space position by RootNode transform
                    // to get its actual world position
                    
                    // CRITICAL: selectedModelIndex is already validated above - use it directly
                    // No fallback to first model - ensures strict isolation between models
                    
                    // Get this model's RootNode transform matrix from PropertyPanel
                    glm::mat4 rootNodeTransform = glm::mat4(1.0f);  // Identity if no RootNode transform
                    if (selectedModelIndex >= 0) {
                        // OPTIMIZED: Use cached name instead of per-frame tree search
                        std::string modelRootNodeName = cachedModelRootNodeName;
                        if (!modelRootNodeName.empty()) {
                            // Get RootNode transforms from PropertyPanel
                            const auto& allBoneTranslations = propertyPanel.getAllBoneTranslations();
                            const auto& allBoneRotations = propertyPanel.getAllBoneRotations();
                            const auto& allBoneScales = propertyPanel.getAllBoneScales();
                            
                            auto transIt = allBoneTranslations.find(modelRootNodeName);
                            auto rotIt = allBoneRotations.find(modelRootNodeName);
                            auto scaleIt = allBoneScales.find(modelRootNodeName);
                            
                            glm::vec3 rootTranslation = (transIt != allBoneTranslations.end()) ? transIt->second : glm::vec3(0.0f);
                            glm::vec3 rootRotationEuler = (rotIt != allBoneRotations.end()) ? rotIt->second : glm::vec3(0.0f);
                            glm::vec3 rootScale = (scaleIt != allBoneScales.end()) ? scaleIt->second : glm::vec3(1.0f);
                            
                            // Build RootNode transform matrix
                            glm::quat rootRotation = glm::quat(glm::radians(rootRotationEuler));
                            rootNodeTransform = glm::mat4(1.0f);
                            rootNodeTransform = glm::translate(rootNodeTransform, rootTranslation);
                            rootNodeTransform = rootNodeTransform * glm::mat4_cast(rootRotation);
                            rootNodeTransform = glm::scale(rootNodeTransform, rootScale);
                        }
                    }
                    
                    // CRITICAL FIX: Get the bone's transform data from the SPECIFIC MODEL INSTANCE
                    // Use modelManager->getSelectedModelIndex() to ensure we're using the correct model
                    // This prevents "leaking" position data between different characters
                    // 
                    // The bone's position from BoneGlobalTransform is in MODEL SPACE (relative to model origin)
                    // We must transform it by the RootNode transform of THIS SPECIFIC MODEL to get world position
                    // Formula: WorldPosition = RootNodeTransform[selectedModelIndex] * BoneLocalPosition
                    
                    // CRITICAL: Get bone transforms from the SELECTED MODEL INSTANCE ONLY
                    // Use selectedModelIndex (already validated above) to get the correct ModelInstance
                    // This ensures we're looking up bones in the correct model (strict isolation)
                    // OPTIMIZED: Use O(1) vector access instead of map allocation every frame
                    // 1. Get the integer index instead of using strings (O(log N) lookup once, no allocation)
                    int boneIndex = selectedModel->model.getBoneIndexFromName(selectedNode);
                    
                    // 2. Get a direct const reference to the flat vector (Zero allocations)
                    const std::vector<BoneGlobalTransform>& globalTransforms = selectedModel->model.getBoneGlobalTransforms();
                    
                    // 3. Direct O(1) array access!
                    if (boneIndex >= 0) {
                        // FIXED: Retrieve the bone's actual calculated model-space matrix directly from the model
                        // The bone's globalMatrix in LinearBone contains the complete transform including
                        // the structural FBX bind pose offset/length, which UI sliders (animation deltas) don't capture
                        // This prevents the Gizmo from snapping to the parent's origin
                        
                        // Get the bone's model-space matrix (includes bind pose offset)
                        glm::mat4 boneModelSpaceMatrix = selectedModel->model.getBoneModelSpaceMatrix(boneIndex);
                        
                        // Calculate the true world matrix by multiplying root node transform by bone's model-space matrix
                        // This correctly positions the Gizmo at the bone's actual location including bind pose offset
                        modelMatrix = rootNodeTransform * boneModelSpaceMatrix;
                    } else {
                        // CRITICAL: Bone not found in THIS MODEL's transforms
                        // This can happen if:
                        //   1. Bone transforms haven't been calculated yet for this model
                        //   2. Bone name doesn't exist in this model (wrong model selected)
                        //   3. Bone is not a valid bone in this model's hierarchy
                        //
                        // Instead of using PropertyPanel values (which might be (0,0,0) or from wrong model),
                        // we should try to get the bone's position from the FBX scene data for THIS MODEL
                        // If that fails, we can't position the gizmo correctly, so we'll use a fallback
                        
                        // Try to get bone position from FBX scene for THIS MODEL
                        const ofbx::IScene* scene = outliner.getScene(selectedModelIndex);
                        if (scene && selectedModelIndex >= 0) {
                            const ofbx::Object* sceneRoot = scene->getRoot();
                            const ofbx::Object* boneObject = outliner.findObjectByName(sceneRoot, selectedNode);
                            
                            if (boneObject) {
                                // Found bone in FBX scene - get its local transform
                                ofbx::Matrix boneLocalMatrix = boneObject->getLocalTransform();
                                glm::mat4 boneLocalTransform = fbxToGlmMatrix(boneLocalMatrix, false);
                                
                                // Extract translation from local transform
                                glm::vec3 boneLocalTranslation;
                                glm::vec3 scale;
                                glm::quat rotation;
                                glm::vec3 skew;
                                glm::vec4 perspective;
                                glm::decompose(boneLocalTransform, scale, rotation, boneLocalTranslation, skew, perspective);
                                
                                // Transform bone's local position by THIS MODEL's RootNode transform to get world position
                                glm::vec4 boneLocalPos4(boneLocalTranslation, 1.0f);
                                glm::vec4 worldPos4 = rootNodeTransform * boneLocalPos4;
                                glm::vec3 worldPosition(worldPos4.x, worldPos4.y, worldPos4.z);
                                
                                // Get rotation and scale from PropertyPanel
                                glm::quat rot;
                                if (mGizmoInitialStateStored && selectedNode == mInitialNodeName && ((mGizmoOperation & ImGuizmo::ROTATE) != 0)) {
                                    rot = mCurrentLocalRotationQuat;
                                } else {
                                    glm::vec3 rotationEuler = propertyPanel.getSliderRot_update();
                                    rot = glm::quat(glm::radians(rotationEuler));
                                }
                                glm::vec3 boneScale = propertyPanel.getSliderScale_update();
                                
                                // Build matrix using world position from FBX data
                                modelMatrix = glm::translate(modelMatrix, worldPosition);
                                modelMatrix = modelMatrix * glm::mat4_cast(rot);
                                modelMatrix = glm::scale(modelMatrix, boneScale);
                            } else {
                                // Bone not found in FBX scene either - use PropertyPanel as last resort
                                // This should rarely happen, but provides a fallback
                                glm::vec3 translation = propertyPanel.getSliderTrans_update();
                                glm::quat rotation;
                                if (mGizmoInitialStateStored && selectedNode == mInitialNodeName && ((mGizmoOperation & ImGuizmo::ROTATE) != 0)) {
                                    rotation = mCurrentLocalRotationQuat;
                                } else {
                                    glm::vec3 rotationEuler = propertyPanel.getSliderRot_update();
                                    rotation = glm::quat(glm::radians(rotationEuler));
                                }
                                glm::vec3 scale = propertyPanel.getSliderScale_update();
                                
                                modelMatrix = glm::translate(modelMatrix, translation);
                                modelMatrix = modelMatrix * glm::mat4_cast(rotation);
                                modelMatrix = glm::scale(modelMatrix, scale);
                            }
                        } else {
                            // Scene not available - use PropertyPanel as fallback
                            glm::vec3 translation = propertyPanel.getSliderTrans_update();
                            glm::quat rotation;
                            if (mGizmoInitialStateStored && selectedNode == mInitialNodeName && ((mGizmoOperation & ImGuizmo::ROTATE) != 0)) {
                                rotation = mCurrentLocalRotationQuat;
                            } else {
                                glm::vec3 rotationEuler = propertyPanel.getSliderRot_update();
                                rotation = glm::quat(glm::radians(rotationEuler));
                            }
                            glm::vec3 scale = propertyPanel.getSliderScale_update();
                            
                            modelMatrix = glm::translate(modelMatrix, translation);
                            modelMatrix = modelMatrix * glm::mat4_cast(rotation);
                            modelMatrix = glm::scale(modelMatrix, scale);
                        }
                    }
                }
                
                // Convert model matrix to float array for ImGuizmo
                float modelMatrixArray[16];
                glmMat4ToFloat16(modelMatrix, modelMatrixArray);
                
                // Set gizmo size (adjustable with +/- keys)
                // This must be called before Manipulate() to apply the size
                ImGuizmo::SetGizmoSizeClipSpace(mGizmoSize);
                
                // Use current gizmo operation (set with W/E/R keys)
                ImGuizmo::OPERATION operation = mGizmoOperation;
                ImGuizmo::MODE mode = ImGuizmo::LOCAL;
                
                // Check if gizmo is being used or hovered (before calling Manipulate)
                // This needs to be checked after SetRect but Manipulate updates these states
                // So we'll check after Manipulate and store for use in selection code
                
                // Manipulate the matrix
                bool manipulated = ImGuizmo::Manipulate(
                    viewMatrixArray,
                    projectionMatrixArray,
                    operation,
                    mode,
                    modelMatrixArray
                );
                
                // Update gizmo state (check after Manipulate as it updates these states)
                mGizmoUsing = ImGuizmo::IsUsing();
                mGizmoOver = ImGuizmo::IsOver();
                
                // Store gizmo world position for camera aim constraint
                // Extract world position from the current model matrix (after manipulation)
                glm::mat4 currentModelMatrix = float16ToGlmMat4(modelMatrixArray);
                glm::vec3 currentScale;
                glm::quat currentRotation;
                glm::vec3 gizmoWorldPos;
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::decompose(currentModelMatrix, currentScale, currentRotation, gizmoWorldPos, skew, perspective);
                mGizmoWorldPosition = gizmoWorldPos;
                mGizmoPositionValid = true;  // Gizmo is visible and has valid position
                
                // STATIC REFERENCE POINTS: Store initial state when manipulation starts (OnMouseDown)
                // Detect when gizmo manipulation starts (transition from not using to using)
                if (!mGizmoWasUsing && mGizmoUsing) {
                    // Gizmo manipulation just started (mouse down) - capture static references
                    mGizmoInitialStateStored = true;
                    mInitialNodeName = selectedNode;
                    
                    // 1. Capture Start_Mouse_World_Pos: Get mouse ray intersection with gizmo plane
                    ImVec2 mousePos = ImGui::GetMousePos();
                    
                    // Convert mouse position to viewport-relative coordinates
                    float mouseX = mousePos.x - viewportPos.x;
                    float mouseY = mousePos.y - viewportPos.y;
                    
                    // Get mouse ray in world space
                    Ray mouseRay = Raycast::screenToWorldRay(mouseX, mouseY, 
                                                             (int)viewportSize.x, (int)viewportSize.y,
                                                             viewMatrix, projectionMatrix);
                    
                    // Get gizmo's initial world position (for plane point)
                    glm::mat4 currentModelMatrixForRay = float16ToGlmMat4(modelMatrixArray);
                    glm::vec3 currentScaleForRay;
                    glm::quat currentRotationForRay;
                    glm::vec3 gizmoWorldPosForRay;
                    glm::vec3 skewForRay;
                    glm::vec4 perspectiveForRay;
                    glm::decompose(currentModelMatrixForRay, currentScaleForRay, currentRotationForRay, gizmoWorldPosForRay, skewForRay, perspectiveForRay);
                    
                    // Calculate camera forward direction from view matrix (third column, negated)
                    glm::mat4 invView = glm::inverse(viewMatrix);
                    glm::vec3 cameraPos = glm::vec3(invView[3]);
                    glm::vec3 cameraForward = -glm::normalize(glm::vec3(viewMatrix[2])); // Negated Z axis of view matrix
                    
                    // Intersect mouse ray with plane perpendicular to camera forward, passing through gizmo position
                    glm::vec3 planePoint = gizmoWorldPosForRay;
                    glm::vec3 planeNormal = cameraForward;
                    glm::vec3 mouseWorldPos;
                    if (Raycast::rayIntersectsPlane(mouseRay, planePoint, planeNormal, mouseWorldPos)) {
                        mStartMouseWorldPos = mouseWorldPos;
                    } else {
                        // Fallback: use gizmo position if ray doesn't intersect
                        mStartMouseWorldPos = gizmoWorldPosForRay;
                    }
                    
                    // 2. Capture Start_Node_Local_Translation
                    mStartNodeLocalTranslation = propertyPanel.getSliderTrans_update();
                    
                    // 2b. AXIS-ANGLE PROJECTION: Capture reference state (OnMouseDown)
                    // Store initial local rotation
                    glm::vec3 startLocalRotationEuler = propertyPanel.getSliderRot_update();  // Euler angles in degrees
                    mStartLocalQuat = glm::normalize(glm::quat(glm::radians(startLocalRotationEuler)));  // Convert to quaternion and normalize
                    mCurrentLocalRotationQuat = mStartLocalQuat;  // Initialize current rotation quaternion
                    
                    // Store initial gizmo world rotation (for angle delta calculation)
                    mInitialGizmoRotation = glm::normalize(currentRotation);
                    
                    // Identify the Active Axis of the Gizmo in World Space
                    // ImGuizmo uses bitwise flags: ROTATE_X, ROTATE_Y, ROTATE_Z, or ROTATE (all)
                    mWorldRotationAxis = glm::vec3(1.0f, 0.0f, 0.0f);  // Default to X axis
                    if ((mGizmoOperation & ImGuizmo::ROTATE_X) != 0 && (mGizmoOperation & ImGuizmo::ROTATE_Y) == 0 && (mGizmoOperation & ImGuizmo::ROTATE_Z) == 0) {
                        mWorldRotationAxis = glm::vec3(1.0f, 0.0f, 0.0f);  // Red axis (X)
                    } else if ((mGizmoOperation & ImGuizmo::ROTATE_Y) != 0 && (mGizmoOperation & ImGuizmo::ROTATE_X) == 0 && (mGizmoOperation & ImGuizmo::ROTATE_Z) == 0) {
                        mWorldRotationAxis = glm::vec3(0.0f, 1.0f, 0.0f);  // Green axis (Y)
                    } else if ((mGizmoOperation & ImGuizmo::ROTATE_Z) != 0 && (mGizmoOperation & ImGuizmo::ROTATE_X) == 0 && (mGizmoOperation & ImGuizmo::ROTATE_Y) == 0) {
                        mWorldRotationAxis = glm::vec3(0.0f, 0.0f, 1.0f);  // Blue axis (Z)
                    } else {
                        // Multiple axes or ROTATE (all) - use X axis as default
                        mWorldRotationAxis = glm::vec3(1.0f, 0.0f, 0.0f);
                    }
                    
                    // Note: Conversion to local space will be done after parent quat is captured below
                    
                    // 3. Capture Start_Parent_Inverse_Matrix (static reference)
                    // Only for bone nodes (not rig root)
                    if (!isRigRoot) {
                        int selectedModelIndexForParent = modelManager->getSelectedModelIndex();
                        if (selectedModelIndexForParent >= 0) {
                            ModelInstance* pModel = modelManager->getModel(selectedModelIndexForParent);
                            if (pModel) {
                                // CRITICAL FIX: Get the dynamically animated parent matrix from the live Model!
                                // No more static FBX lookups. This includes ALL animations and parent transforms up to the root.
                                int boneIndexForParent = pModel->model.getBoneIndexFromName(selectedNode);
                                glm::mat4 parentWorldModelSpace = pModel->model.getParentGlobalMatrix(boneIndexForParent);
                                
                                // Transform parent's world matrix to world space by applying RootNode transform
                                glm::mat4 parentWorldWorldSpace = parentWorldModelSpace;
                                std::string modelRootNodeName = outliner.getRootNodeName(selectedModelIndexForParent);
                                
                                if (!modelRootNodeName.empty()) {
                                    const auto& allBoneTranslations = propertyPanel.getAllBoneTranslations();
                                    const auto& allBoneRotations = propertyPanel.getAllBoneRotations();
                                    const auto& allBoneScales = propertyPanel.getAllBoneScales();
                                    
                                    auto transIt = allBoneTranslations.find(modelRootNodeName);
                                    auto rotIt = allBoneRotations.find(modelRootNodeName);
                                    auto scaleIt = allBoneScales.find(modelRootNodeName);
                                    
                                    glm::vec3 rootTranslation = (transIt != allBoneTranslations.end()) ? transIt->second : glm::vec3(0.0f);
                                    glm::vec3 rootRotationEuler = (rotIt != allBoneRotations.end()) ? rotIt->second : glm::vec3(0.0f);
                                    glm::vec3 rootScale = (scaleIt != allBoneScales.end()) ? scaleIt->second : glm::vec3(1.0f);
                                    
                                    glm::quat rootRotation = glm::quat(glm::radians(rootRotationEuler));
                                    glm::mat4 rootNodeTransform = glm::mat4(1.0f);
                                    rootNodeTransform = glm::translate(rootNodeTransform, rootTranslation);
                                    rootNodeTransform = rootNodeTransform * glm::mat4_cast(rootRotation);
                                    rootNodeTransform = glm::scale(rootNodeTransform, rootScale);
                                    
                                    parentWorldWorldSpace = rootNodeTransform * parentWorldModelSpace;
                                }
                                
                                // Check determinant before inversion
                                float determinant = glm::determinant(parentWorldWorldSpace);
                                const float determinantEpsilon = 1e-12f;
                                
                                if (std::abs(determinant) > determinantEpsilon) {
                                    // ABSOLUTE REFERENCE LOCK: Store static parent inverse matrix ONCE at OnMouseDown
                                    mStartParentInverseMatrix = glm::inverse(parentWorldWorldSpace);
                                    
                                    // SNAP START STATE: Orthonormalize the 3x3 basis to completely kill the 0.01 scale effect
                                    glm::mat3 rotationBasis = glm::mat3(parentWorldWorldSpace);
                                    
                                    glm::vec3 xAxis = glm::normalize(glm::vec3(rotationBasis[0]));
                                    glm::vec3 yAxis = glm::normalize(glm::vec3(rotationBasis[1]));
                                    glm::vec3 zAxis = glm::normalize(glm::vec3(rotationBasis[2]));
                                    
                                    yAxis = glm::normalize(yAxis - glm::dot(yAxis, xAxis) * xAxis);
                                    zAxis = glm::normalize(glm::cross(xAxis, yAxis));
                                    
                                    glm::mat3 orthonormalBasis;
                                    orthonormalBasis[0] = glm::vec3(xAxis);
                                    orthonormalBasis[1] = glm::vec3(yAxis);
                                    orthonormalBasis[2] = glm::vec3(zAxis);
                                    
                                    mStartParentWorldQuat = glm::normalize(glm::quat_cast(orthonormalBasis));
                                } else {
                                    mStartParentInverseMatrix = glm::mat4(1.0f);
                                    mStartParentWorldQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                                    mLocalRotationAxis = glm::normalize(mWorldRotationAxis);
                                }
                            }
                        } else {
                            mStartParentInverseMatrix = glm::mat4(1.0f);
                            mStartParentWorldQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                            mLocalRotationAxis = glm::normalize(mWorldRotationAxis);
                        }
                    } else {
                        // Rig root - use identity (no parent transform)
                        mStartParentInverseMatrix = glm::mat4(1.0f);
                        mStartParentWorldQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Identity quaternion
                        // For identity parent, local axis = world axis
                        mLocalRotationAxis = glm::normalize(mWorldRotationAxis);
                    }
                }
                
                // Reset initial state if node changed or manipulation ended
                if (selectedNode != mInitialNodeName || (!mGizmoUsing && mGizmoWasUsing)) {
                    mGizmoInitialStateStored = false;
                    mInitialNodeName = "";
                    // Reset rotation references (Axis-Angle Projection)
                    mStartParentWorldQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Reset to identity
                    mStartLocalQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Reset to identity
                    mWorldRotationAxis = glm::vec3(1.0f, 0.0f, 0.0f);  // Reset to default
                    mLocalRotationAxis = glm::vec3(1.0f, 0.0f, 0.0f);  // Reset to default
                    mInitialGizmoRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Reset to identity
                    mCurrentLocalRotationQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Reset to identity
                }
                
                // COMMIT ON RELEASE (OnMouseUp): Explicitly save final value and update matrices
                // Detect when gizmo state transitions from using to not using
                if (mGizmoWasUsing && !mGizmoUsing) {
                    // Gizmo manipulation just ended (mouse release)
                    // 1. Get final local translation and rotation from PropertyPanel (already set during drag)
                    glm::vec3 finalLocalTranslation = propertyPanel.getSliderTrans_update();
                    glm::vec3 finalLocalRotationEuler = propertyPanel.getSliderRot_update();
                    
                    // 2. Ensure final values are committed (PropertyPanel already has them, but ensure no revert/undo)
                    // The PropertyPanel values are already set during drag, so this is just a safety check
                    // NO "Revert" or "Undo" logic should be triggered here
                    
                    // 3. For bone nodes, we need to ensure the node's internal matrices are updated
                    // This is handled by the rendering system, but we can trigger an update here if needed
                    // (The PropertyPanel values are already committed, so the next render will use them)
                    
                    // AUTO-FOCUS: If auto-focus is enabled, request camera framing
                    // This will trigger the same framing logic as pressing 'F' key
                    // The framing will use the existing "Vertical Centering" and "AABB-based distance" logic
                    // Use persistent settings value
                    AppSettings& settings = AppSettingsManager::getInstance().getSettings();
                    if (settings.environment.autoFocusEnabled && mGizmoPositionValid) {
                        cameraFramingRequested = true;
                    }
                    
                    // Debug print function disabled - uncomment to enable gizmo transform debugging
                    // printGizmoTransformValues(modelMatrixArray, selectedNode, isRigRoot);
                    
                    // Reset initial state (including rotation references - Axis-Angle Projection)
                    mGizmoInitialStateStored = false;
                    mInitialNodeName = "";
                    mStartParentWorldQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Reset to identity
                    mStartLocalQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Reset to identity
                    mWorldRotationAxis = glm::vec3(1.0f, 0.0f, 0.0f);  // Reset to default
                    mLocalRotationAxis = glm::vec3(1.0f, 0.0f, 0.0f);  // Reset to default
                    mInitialGizmoRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Reset to identity
                    mCurrentLocalRotationQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Reset to identity
                }
                
                // Update previous state for next frame
                mGizmoWasUsing = mGizmoUsing;
                
                // If the gizmo was manipulated, update PropertyPanel Transforms for the selected node
                if (manipulated) {
                    // Convert back to glm::mat4 using math3D utility function
                    glm::mat4 newModelMatrix = float16ToGlmMat4(modelMatrixArray);
                    
                    // Decompose the matrix to get translation, rotation, and scale
                    glm::vec3 scale;
                    glm::quat rotation;
                    glm::vec3 translation;
                    glm::vec3 skew;
                    glm::vec4 perspective;
                    glm::decompose(newModelMatrix, scale, rotation, translation, skew, perspective);
                    
                    // Convert quaternion to Euler angles (degrees) for PropertyPanel
                    glm::vec3 rotationEuler = glm::degrees(glm::eulerAngles(rotation));
                    
                if (isRigRoot) {
                    // For rig root, use world-space values directly
                    propertyPanel.setSliderTranslations(translation);
                    propertyPanel.setSliderRotations(rotationEuler);
                    propertyPanel.setSliderScales(scale);
                    } else {
                        // FIXED: Delta-extraction logic to prevent mesh explosion/stretching bug
                        // The old extraction expected only UI deltas, but now modelMatrix includes bind pose
                        // This new logic isolates only the Gizmo delta and applies it to existing UI values
                        
                        // 1. Fetch current UI values (U_orig) before applying this frame's manipulation
                        glm::vec3 origTrans = propertyPanel.getSliderTrans_update();
                        glm::vec3 origRotEuler = propertyPanel.getSliderRot_update();
                        origRotEuler.y = -origRotEuler.y; // CRITICAL: Translator (UI -> Gizmo)
                        glm::quat origRot;
                        if (mGizmoInitialStateStored && selectedNode == mInitialNodeName && ((mGizmoOperation & ImGuizmo::ROTATE) != 0)) {
                            origRot = mCurrentLocalRotationQuat;
                        } else {
                            origRot = glm::quat(glm::radians(origRotEuler));
                        }
                        glm::vec3 origScale = propertyPanel.getSliderScale_update();
                        
                        // Build U_orig matrix
                        glm::mat4 U_orig = glm::mat4(1.0f);
                        U_orig = glm::translate(U_orig, origTrans);
                        U_orig = U_orig * glm::mat4_cast(origRot);
                        U_orig = glm::scale(U_orig, origScale);
                        
                        // 2. Extract pure Gizmo Delta and apply to U_orig
                        // `modelMatrix` is W_orig (the true world matrix before manipulation)
                        // `modelMatrixArray` contains W_new (the modified world matrix)
                        glm::mat4 W_orig = modelMatrix; 
                        glm::mat4 W_new = float16ToGlmMat4(modelMatrixArray);
                        
                        // Calculate U_new: U_new = U_orig * Inverse(W_orig) * W_new
                        // This isolates purely the Delta introduced by ImGuizmo and applies it to the existing UI values
                        glm::mat4 W_orig_inv = glm::inverse(W_orig);
                        glm::mat4 U_new = U_orig * W_orig_inv * W_new;
                        
                        // 3. Decompose U_new for the Property Panel
                        float localTrans[3], localRot[3], localScale[3];
                        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(U_new), localTrans, localRot, localScale);
                        
                        glm::vec3 newLocalTranslation(localTrans[0], localTrans[1], localTrans[2]);
                        glm::vec3 newLocalScale(localScale[0], localScale[1], localScale[2]);
                        
                        // Raw Gizmo rotation (used strictly for internal stability)
                        glm::vec3 rawGizmoRotationEuler(localRot[0], localRot[1], localRot[2]);
                        
                        // CRITICAL FIX FOR ROTATION JITTER:
                        // Save the RAW rotation to the drag-state!
                        // If we use the inverted Y here, the gizmo fights itself every frame.
                        if ((mGizmoOperation & ImGuizmo::ROTATE) != 0) {
                            mCurrentLocalRotationQuat = glm::quat(glm::radians(rawGizmoRotationEuler));
                        }
                        
                        // Translated rotation for UI and Model (invert Y back)
                        glm::vec3 newLocalRotationEuler = rawGizmoRotationEuler;
                        newLocalRotationEuler.y = -newLocalRotationEuler.y; // CRITICAL: Translator (Gizmo -> UI)
                        
                        // 4. Validation to prevent NaN propagation
                        bool isValid = true;
                        if (std::isnan(newLocalTranslation.x) || std::isnan(newLocalScale.x) || std::isnan(newLocalRotationEuler.x)) isValid = false;
                        if (std::isinf(newLocalTranslation.x) || std::isinf(newLocalScale.x) || std::isinf(newLocalRotationEuler.x)) isValid = false;
                        
                        if (isValid) {
                            // Update UI
                            propertyPanel.setSliderTranslations(newLocalTranslation);
                            propertyPanel.setSliderRotations(newLocalRotationEuler);
                            propertyPanel.setSliderScales(newLocalScale);
                            
                            // Update Model Fast Arrays
                            int boneIndex = selectedModel->model.getBoneIndexFromName(selectedNode);
                            if (boneIndex >= 0) {
                                selectedModel->model.updateBoneTranslationByIndex(boneIndex, newLocalTranslation);
                                selectedModel->model.updateBoneRotationByIndex(boneIndex, newLocalRotationEuler);
                                selectedModel->model.updateBoneScaleByIndex(boneIndex, newLocalScale);
                            }
                        }
                    }
                }
            }  // End if (selectedModel != nullptr)
        } else {
            // No model loaded, reset gizmo state
            mGizmoUsing = false;
            mGizmoOver = false;
            mGizmoWasUsing = false;
            mGizmoInitialStateStored = false;
            mGizmoPositionValid = false;  // Gizmo is not visible, position is invalid
            mInitialNodeName = "";
        }  // End if (selectedModelIndex >= 0 && selectedModelIndex < getModelCount())
    } else {
        // No node selected in outliner, reset gizmo state
        mGizmoUsing = false;
        mGizmoOver = false;
        mGizmoWasUsing = false;
        mGizmoInitialStateStored = false;
        mGizmoPositionValid = false;  // Gizmo is not visible, position is invalid
        mInitialNodeName = "";
    }
}

void GizmoManager::printGizmoTransformValues(const float* modelMatrixArray, const std::string& nodeName, bool isRigRoot) {
    // Convert matrix array to glm::mat4 using math3D utility function
    glm::mat4 worldMatrix = float16ToGlmMat4(modelMatrixArray);
    
    // Decompose the world space matrix to get translation, rotation, and scale
    glm::vec3 worldScale;
    glm::quat worldRotation;
    glm::vec3 worldTranslation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(worldMatrix, worldScale, worldRotation, worldTranslation, skew, perspective);
    
    // Convert quaternion to Euler angles (degrees) for readability
    glm::vec3 worldRotationEuler = glm::degrees(glm::eulerAngles(worldRotation));
    
    // Print world space values (from Gizmo)
    std::cout << "========================================" << std::endl;
    std::cout << "[Gizmo Transform Test - Mouse Release]" << std::endl;
    std::cout << "Controlled Object: " << (nodeName.empty() ? "Unknown" : nodeName) << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "WORLD SPACE (Gizmo):" << std::endl;
    std::cout << "  Translation (X, Y, Z): (" 
              << worldTranslation.x << ", " << worldTranslation.y << ", " << worldTranslation.z << ")" << std::endl;
    std::cout << "  Rotation (X, Y, Z) [degrees]: (" 
              << worldRotationEuler.x << ", " << worldRotationEuler.y << ", " << worldRotationEuler.z << ")" << std::endl;
    std::cout << "  Scale (X, Y, Z): (" 
              << worldScale.x << ", " << worldScale.y << ", " << worldScale.z << ")" << std::endl;
    
    // Note: The local space conversion logic requires ModelManager and Outliner references
    // This is a debug function, so we'll keep it simple for now
    // If full local space conversion is needed, it can be added later with proper parameters
    
    std::cout << "========================================" << std::endl;
}
