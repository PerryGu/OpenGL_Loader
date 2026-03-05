#include "model_manager.h"
#include "../utils/logger.h"
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <glm/glm.hpp>

ModelManager::ModelManager() {
}

ModelManager::~ModelManager() {
    clearAll();
}

int ModelManager::loadModel(const std::string& filePath) {
    if (filePath.empty()) {
        LOG_ERROR("[ModelManager] Error: Empty file path");
        return -1;
    }
    
    // Get index before adding
    int index = static_cast<int>(models.size());
    
    // Create model instance using unique_ptr (avoids moving Model with loaded scene)
    auto instance = std::make_unique<ModelInstance>();
    instance->filePath = filePath;
    instance->displayName = extractFileName(filePath);
    
    // Load the model
    instance->model.loadModel(filePath);
    
    // Check if model loaded successfully
    if (instance->model.getFileExtension().empty()) {
        LOG_ERROR("[ModelManager] Error: Failed to load model from " + filePath);
        return -1;
    }
    
    // Initialize animation state
    instance->initializeAnimation();
    
    // Initialize bounding box - automatically calculates bounding box from model geometry
    // The bounding box will be updated in real-time during rendering to reflect
    // current animation/deformation state
    instance->initializeBoundingBox();
    
    // Set the model's per-model bounding box visibility flag to match current global setting
    // Note: BoundingBox::render() no longer checks mEnabled - visibility is controlled solely by m_showBoundingBox
    // This allows per-model checkboxes to work independently of the global toggle
    instance->model.m_showBoundingBox = mBoundingBoxesEnabled;
    // Keep boundingBox.setEnabled(true) for initialization state (not used for visibility anymore)
    instance->boundingBox.setEnabled(true);
    
    // Set skeleton enabled state to match current setting (respects saved preference)
    instance->model.m_showSkeleton = mSkeletonsEnabled;
    
    // Debug print removed for clean console output
    
    // Add to collection (move the unique_ptr)
    models.push_back(std::move(instance));
    
    return index;
}

void ModelManager::removeModel(int index) {
    if (index < 0 || index >= static_cast<int>(models.size())) {
        LOG_ERROR("[ModelManager] Error: Invalid model index " + std::to_string(index));
        return;
    }
    
    // Debug print removed for clean console output
    
    // Cleanup the model
    models[index]->model.cleanup();
    
    // Remove from vector (unique_ptr will automatically clean up)
    models.erase(models.begin() + index);
    
    // Debug print removed for clean console output
}

void ModelManager::clearAll() {
    // Debug print removed for clean console output
    
    for (auto& instance : models) {
        instance->model.cleanup();
    }
    
    models.clear();
    // Debug print removed for clean console output
}

ModelInstance* ModelManager::getModel(int index) {
    if (index < 0 || index >= static_cast<int>(models.size())) {
        return nullptr;
    }
    return models[index].get();
}

void ModelManager::updateAnimations(double currentTime, float deltaTime) {
    for (auto& instance : models) {
        instance->updateAnimation(currentTime, deltaTime);
    }
}

void ModelManager::renderAll(Shader& shader, UI_data& ui_data, std::vector<glm::mat4>& transforms, 
                              int modelIndex, const glm::mat4* customModelMatrix) {
    for (size_t i = 0; i < models.size(); ++i) {
        auto& instance = models[i];
        
        // CRITICAL: Activate shader BEFORE uploading bone matrices
        // This ensures bone matrices are uploaded to the correct shader program
        // If DrawSkeleton left a different shader bound, this fixes it
        shader.activate();
        
        // Sync all bone rotations from PropertyPanel to the model before rendering
        // This ensures all saved rotations are applied, not just the selected one
        // Note: This will be called from main.cpp with the PropertyPanel's bone rotations
        
        // Get bone transforms for this model
        instance->model.BoneTransform(instance->animationTime, transforms, ui_data);
        
        // Cache bone transforms for bounding box calculation (performance optimization)
        // This avoids recalculating them in the bounding box render loop
        instance->cachedBoneTransforms = transforms;
        instance->boneTransformsCached = true;
        
        // Set bone transforms in shader (must be set before rendering each model)
        if (!transforms.empty()) {
            shader.setMat4("bone_transforms", transforms.size(), transforms[0]);
        } else {
            // Clear bone transforms if model has no bones
            // Create identity matrices array
            std::vector<glm::mat4> identityTransforms(50, glm::mat4(1.0f));
            shader.setMat4("bone_transforms", identityTransforms.size(), identityTransforms[0]);
        }
        
        // Use custom model matrix if provided for this model (from PropertyPanel RootNode)
        // Otherwise, model will use its own pos/rotation/size
        const glm::mat4* modelMatrixToUse = nullptr;
        if (customModelMatrix != nullptr && modelIndex >= 0 && static_cast<size_t>(modelIndex) == i) {
            modelMatrixToUse = customModelMatrix;
        }
        
        // Render the model
        instance->model.render(shader, modelMatrixToUse);
    }
}

void ModelManager::renderAllWithRootNodeTransforms(Shader& shader, UI_data& ui_data, std::vector<glm::mat4>& transforms,
                                                   const std::map<int, glm::mat4>& modelRootNodeMatrices,
                                                   const glm::mat4& view, const glm::mat4& projection, Shader* skeletonShader) {
    for (size_t i = 0; i < models.size(); ++i) {
        auto& instance = models[i];
        int modelIndex = static_cast<int>(i);
        
        // CRITICAL: Activate shader BEFORE uploading bone matrices
        // This ensures bone matrices are uploaded to the correct shader program
        // If DrawSkeleton left a different shader bound, this fixes it
        shader.activate();
        
        // Get bone transforms for this model
        instance->model.BoneTransform(instance->animationTime, transforms, ui_data);
        
        // Cache bone transforms for bounding box calculation
        instance->cachedBoneTransforms = transforms;
        instance->boneTransformsCached = true;
        
        // Set bone transforms in shader
        if (!transforms.empty()) {
            shader.setMat4("bone_transforms", transforms.size(), transforms[0]);
        } else {
            std::vector<glm::mat4> identityTransforms(50, glm::mat4(1.0f));
            shader.setMat4("bone_transforms", identityTransforms.size(), identityTransforms[0]);
        }
        
        // Use this model's RootNode transform matrix (if available)
        // This ensures each model uses its own RootNode transforms, maintaining hierarchy isolation
        const glm::mat4* modelMatrixToUse = nullptr;
        auto it = modelRootNodeMatrices.find(modelIndex);
        if (it != modelRootNodeMatrices.end()) {
            // Use this model's RootNode transform
            static glm::mat4 cachedMatrix;  // Static to avoid stack allocation
            cachedMatrix = it->second;
            modelMatrixToUse = &cachedMatrix;
        }
        
        // Render the model
        instance->model.render(shader, modelMatrixToUse);
        
        // Draw skeleton if enabled (after model is rendered)
        if (instance->model.m_showSkeleton && skeletonShader != nullptr) {
            // Get the model matrix that was used for rendering
            glm::mat4 skeletonModelMatrix = glm::mat4(1.0f);
            if (modelMatrixToUse != nullptr) {
                skeletonModelMatrix = *modelMatrixToUse;
            } else {
                // Build from model's pos/rotation/size (same as render method)
                skeletonModelMatrix = glm::translate(skeletonModelMatrix, instance->model.pos);
                skeletonModelMatrix = skeletonModelMatrix * glm::mat4_cast(instance->model.rotation);
                skeletonModelMatrix = glm::scale(skeletonModelMatrix, instance->model.size);
            }
            
            // Draw skeleton with the same modelMatrix used for rendering
            instance->model.DrawSkeleton(*skeletonShader, view, projection, skeletonModelMatrix);
        }
    }
}

void ModelManager::getCombinedBoundingBox(glm::vec3& min, glm::vec3& max) {
    if (models.empty()) {
        min = glm::vec3(-1.0f);
        max = glm::vec3(1.0f);
        return;
    }
    
    bool first = true;
    min = glm::vec3(FLT_MAX);
    max = glm::vec3(-FLT_MAX);
    
    for (auto& instance : models) {
        glm::vec3 modelMin, modelMax;
        instance->model.getBoundingBox(modelMin, modelMax);
        
        if (first) {
            min = modelMin;
            max = modelMax;
            first = false;
        } else {
            min.x = std::min(min.x, modelMin.x);
            min.y = std::min(min.y, modelMin.y);
            min.z = std::min(min.z, modelMin.z);
            max.x = std::max(max.x, modelMax.x);
            max.y = std::max(max.y, modelMax.y);
            max.z = std::max(max.z, modelMax.z);
        }
    }
}

void ModelManager::getModelBoundingBox(int index, glm::vec3& min, glm::vec3& max) {
    if (index < 0 || index >= static_cast<int>(models.size())) {
        min = glm::vec3(-1.0f);
        max = glm::vec3(1.0f);
        return;
    }
    
    models[index]->model.getBoundingBox(min, max);
}

void ModelManager::getNodeBoundingBox(int modelIndex, const std::string& nodeName, glm::vec3& min, glm::vec3& max) {
    if (modelIndex < 0 || modelIndex >= static_cast<int>(models.size())) {
        min = glm::vec3(-1.0f);
        max = glm::vec3(1.0f);
        return;
    }
    
    models[modelIndex]->model.getBoundingBoxForNode(nodeName, min, max);
}

void ModelManager::getNodeBoundingBoxByIndex(int modelIndex, int boneIndex, glm::vec3& min, glm::vec3& max) {
    if (modelIndex < 0 || modelIndex >= static_cast<int>(models.size())) {
        min = glm::vec3(-1.0f);
        max = glm::vec3(1.0f);
        return;
    }
    
    models[modelIndex]->model.getBoundingBoxForBoneIndex(boneIndex, min, max);
}

std::string ModelManager::extractFileName(const std::string& path) {
    if (path.empty()) {
        return "Unknown";
    }
    
    // Find last slash or backslash
    size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        return path.substr(lastSlash + 1);
    }
    
    return path;
}

void ModelManager::setBoundingBoxesEnabled(bool enabled) {
    mBoundingBoxesEnabled = enabled;  // Store the current state for new models
    for (auto& instance : models) {
        if (instance) {
            // Update the model's per-model bounding box visibility flag
            // Note: boundingBox.setEnabled() is no longer used for visibility control
            // Visibility is controlled solely by Model::m_showBoundingBox
            instance->model.m_showBoundingBox = enabled;
            // Keep boundingBox.setEnabled(true) for initialization state (not used for visibility)
            instance->boundingBox.setEnabled(true);
        }
    }
}

bool ModelManager::areBoundingBoxesEnabled() const {
    // Return true if at least one model has bounding box enabled
    for (const auto& instance : models) {
        if (instance && instance->boundingBox.isEnabled()) {
            return true;
        }
    }
    return false;
}

void ModelManager::setSkeletonsEnabled(bool enabled) {
    mSkeletonsEnabled = enabled;  // Store the current state for new models
    for (auto& instance : models) {
        if (instance) {
            // Update the model's per-model skeleton visibility flag
            instance->model.m_showSkeleton = enabled;
        }
    }
}

/**
 * @brief Stops all animations and resets them to frame 0.
 * 
 * This method calls stop() on all model instances, which:
 * - Sets isPlaying to false
 * - Resets animationTime to 0.0f
 * - Resets totalTime to 0.0f
 * 
 * This is used by the Stop button functionality (separate from Pause).
 */
void ModelManager::stopAllAnimations() {
    for (auto& instance : models) {
        if (instance) {
            instance->stop();
        }
    }
}

void ModelManager::setSelectedModel(int index) {
    // Validate index
    if (index < -1 || index >= static_cast<int>(models.size())) {
        // LOG: Selection change detection (only print when index actually changes)
        static int lastLoggedIndex = -999;
        if (lastLoggedIndex != -1) {
            LOG_INFO("LOG: Selection Changed to Model Index: -1 (deselected)");
            lastLoggedIndex = -1;
        }
        
        mSelectedModelIndex = -1;
        // Deselect all models
        for (auto& modelInstance : models) {
            if (modelInstance) {
                modelInstance->model.m_isSelected = false;
            }
        }
        return;
    }
    
    // LOG: Selection change detection (only print when index actually changes)
    static int lastLoggedIndex = -999;
    if (lastLoggedIndex != index) {
        LOG_INFO("LOG: Selection Changed to Model Index: " + std::to_string(index));
        lastLoggedIndex = index;
    }
    
    mSelectedModelIndex = index;
    
    // CRITICAL: Update model selection flags to prevent selection leakage
    // Set the selected model's flag to true, and all others to false
    for (size_t i = 0; i < models.size(); i++) {
        if (models[i]) {
            models[i]->model.m_isSelected = (static_cast<int>(i) == index);
        }
    }
    
    // Visual feedback: Bounding box highlighting is handled in Application::renderFrame()
    // Selected models automatically get cyan color and thicker lines (2.0f) for clear selection indication
    // Unselected models use yellow color and standard line width (1.0f)
}

ModelInstance* ModelManager::getSelectedModel() {
    if (mSelectedModelIndex >= 0 && mSelectedModelIndex < static_cast<int>(models.size())) {
        return models[mSelectedModelIndex].get();
    }
    return nullptr;
}
