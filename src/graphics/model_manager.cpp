#include "model_manager.h"
#include "../utils/logger.h"
#include "../io/fbx_rig_analyzer.h"  // For FBXRigAnalyzer::findRigRoot
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <future>
#include <thread>
#include <cstdio>  // For FILE, fopen, fread, fclose, fseek, ftell
#include <cstdlib> // For std::rand, std::srand
#include <ctime>   // For std::time
#include <chrono>  // For timing (if needed in future)

ModelManager::ModelManager() : m_isLoading(false) {
}

ModelManager::~ModelManager() {
    clearAll();
}

// Start async loading of a model (non-blocking)
int ModelManager::loadModelAsync(const std::string& filePath) {
    if (filePath.empty()) {
        LOG_ERROR("[ModelManager] Error: Empty file path");
        return -1;
    }
    
    // Prevent concurrent loading
    {
        std::lock_guard<std::mutex> lock(m_loadMutex);
        if (m_isLoading.load()) {
            LOG_WARNING("[ModelManager] Another model is currently loading. Please wait.");
            return -1;
        }
        m_isLoading = true;
    }
    
    // Get index before adding (reserve the index)
    int index = static_cast<int>(models.size());
    
    // Create async load task
    auto task = std::make_unique<AsyncLoadTask>();
    task->filePath = filePath;
    task->assignedIndex = index;
    
    AsyncLoadTask* taskPtr = task.get();  // Store raw pointer for lambda capture
    
    // Start async CPU-bound loading phase (file I/O and parsing)
    // This runs in a background thread and does NOT create OpenGL resources
    task->future = std::async(std::launch::async, [this, filePath, index, taskPtr]() -> int {
        try {
            // Create model instance for CPU phase
            auto instance = std::make_unique<ModelInstance>();
            instance->filePath = filePath;
            instance->displayName = extractFileName(filePath);
            
            // CPU-BOUND PHASE (Background Thread): Load model data
            // This performs ALL heavy CPU operations:
            //   - File I/O (Assimp::Importer::ReadFile, fread for FBX)
            //   - Assimp scene parsing and mesh extraction (processNode, processMesh)
            //   - Vertex/index/bone weight extraction from aiMesh structures
            //   - Skeletal hierarchy processing (processRig, buildNodeToBoneCache, flattenHierarchy)
            //   - Image decoding (stbi_load) for textures
            //   - Bounding box calculation (iterating over vertices)
            //   - FBX scene loading and rig root analysis (ofbx::load, findRigRoot)
            // This does NOT create OpenGL resources (deferred until GPU phase)
            instance->model.loadModelAsync(filePath);
            
            // Check if model loaded successfully
            if (instance->model.getFileExtension().empty()) {
                LOG_ERROR("[ModelManager] Error: Failed to load model from " + filePath);
                {
                    std::lock_guard<std::mutex> lock(m_loadMutex);
                    m_isLoading = false;
                }
                return -1;
            }
            
            // Initialize animation state (CPU-bound, no OpenGL calls)
            instance->initializeAnimation();
            
            // Calculate bounding box in background thread (CPU-bound, iterates over vertices)
            instance->calculateBoundingBoxAsync();
            
            // Load FBX scene in background thread (CPU-bound, fread and ofbx::load)
            // This moves the heavy file I/O and parsing off the main thread
            std::string fileExtension = instance->model.getFileExtension();
            if (fileExtension == ".fbx") {
                FILE* fp = fopen(filePath.c_str(), "rb");
                if (fp) {
                    fseek(fp, 0, SEEK_END);
                    long file_size = ftell(fp);
                    fseek(fp, 0, SEEK_SET);
                    auto* content = new ofbx::u8[file_size];
                    fread(content, 1, file_size, fp);
                    fclose(fp);
                    
                    ofbx::IScene* scene = ofbx::load((ofbx::u8*)content, file_size, (ofbx::u64)ofbx::LoadFlags::TRIANGULATE);
                    if (scene) {
                        instance->ofbxScene = scene;
                        
                        // Find rig root in background thread (CPU-bound, heavy recursive traversal)
                        // This moves the expensive hierarchy traversal off the main thread
                        instance->rigRootName = FBXRigAnalyzer::findRigRoot(scene);
                        
                        // Note: content array is owned by ofbx::IScene, don't delete it
                    } else {
                        LOG_ERROR("[ModelManager] Failed to load FBX scene: " + std::string(ofbx::getError()));
                        delete[] content;
                    }
                } else {
                    LOG_ERROR("[ModelManager] Failed to open FBX file: " + filePath);
                }
            }
            
            // Store instance for GPU phase (will be moved to models vector on main thread)
            {
                std::lock_guard<std::mutex> lock(m_loadMutex);
                // Verify task pointer is still valid (should be, but check for safety)
                if (m_pendingLoadTask.get() == taskPtr) {
                    taskPtr->instance = std::move(instance);
                    taskPtr->cpuPhaseComplete = true;
                } else {
                    LOG_ERROR("[ModelManager] Task pointer mismatch during async loading");
                    m_isLoading = false;
                    return -1;
                }
            }
            
            return index;
        } catch (const std::exception& e) {
            LOG_ERROR("[ModelManager] Exception during async loading: " + std::string(e.what()));
            {
                std::lock_guard<std::mutex> lock(m_loadMutex);
                m_isLoading = false;
            }
            return -1;
        }
    });
    
    // Store task for polling (must be done after creating future)
    {
        std::lock_guard<std::mutex> lock(m_loadMutex);
        m_pendingLoadTask = std::move(task);
    }
    
    LOG_INFO("[ModelManager] Started async loading: " + filePath);
    return index;
}

// Check if async loading has completed and finalize the model (GPU resource creation)
// Returns true if a model was finalized, false otherwise
// Must be called on the main thread (OpenGL context required)
bool ModelManager::checkAsyncLoadStatus() {
    std::lock_guard<std::mutex> lock(m_loadMutex);
    
    if (!m_pendingLoadTask) {
        return false;  // No pending load
    }
    
    // Check if CPU phase is complete
    if (!m_pendingLoadTask->cpuPhaseComplete) {
        // Check future status (non-blocking)
        auto status = m_pendingLoadTask->future.wait_for(std::chrono::milliseconds(0));
        if (status != std::future_status::ready) {
            return false;  // Still loading
        }
        
        // Future is ready, get result
        int result = m_pendingLoadTask->future.get();
        if (result < 0) {
            // Loading failed
            LOG_ERROR("[ModelManager] Async loading failed for " + m_pendingLoadTask->filePath);
            m_pendingLoadTask.reset();
            m_isLoading = false;
            return false;
        }
        
        // CPU phase completed, instance should be stored
        if (!m_pendingLoadTask->instance) {
            LOG_ERROR("[ModelManager] CPU phase completed but instance is null");
            m_pendingLoadTask.reset();
            m_isLoading = false;
            return false;
        }
        
        m_pendingLoadTask->cpuPhaseComplete = true;
    }
    
    // GPU-BOUND PHASE (Main Thread): CPU phase is complete, now prepare for incremental GPU upload
    // This phase runs EXCLUSIVELY on the main thread with valid OpenGL context.
    // CRITICAL: Do NOT add instance to models collection until GPU upload is 100% complete
    // to prevent render race condition (rendering uninitialized VAOs).
    if (m_pendingLoadTask->cpuPhaseComplete && m_pendingLoadTask->instance) {
        auto instance = std::move(m_pendingLoadTask->instance);
        int index = m_pendingLoadTask->assignedIndex;
        std::string filePath = m_pendingLoadTask->filePath;
        
        // Initialize GPU upload progress tracking (for time-sliced incremental uploading)
        instance->meshesUploadedToGPU = 0;
        instance->texturesUploadedToGPU = 0;
        instance->gpuUploadComplete = false;
        
        // Set the model's per-model bounding box visibility flag to match current global setting
        instance->model.m_showBoundingBox = mBoundingBoxesEnabled;
        instance->boundingBox.setEnabled(true);
        
        // Set skeleton enabled state to match current setting
        instance->model.m_showSkeleton = mSkeletonsEnabled;
        
        // Store instance in temporary location (NOT in active models collection)
        // This prevents render race condition - model won't be rendered until GPU upload is complete
        m_pendingGPUUploadInstance = std::move(instance);
        m_pendingGPUUploadIndex = index;
        
        // Clear pending task (CPU phase complete, GPU upload will continue incrementally)
        m_pendingLoadTask.reset();
        m_isLoading = false;
        
        LOG_INFO("[ModelManager] CPU phase completed, starting incremental GPU upload: " + filePath + " (index: " + std::to_string(index) + ")");
        return false;  // Not finalized yet, GPU upload will continue incrementally
    }
    
    return false;
}

// Continue incremental GPU upload (time-sliced, one mesh per frame)
// Returns true when GPU upload is complete and ready for UI initialization
bool ModelManager::continueGPUUpload() {
    // Check if there's a model waiting for GPU upload
    if (!m_pendingGPUUploadInstance) {
        return false; // No model uploading
    }
    
    ModelInstance* instance = m_pendingGPUUploadInstance.get();
    if (instance->gpuUploadComplete) {
        return false; // Already complete (shouldn't happen, but safety check)
    }
    
    // Upload next mesh/texture (one per frame)
    bool uploadComplete = instance->model.uploadNextMeshToGPU(
        instance->meshesUploadedToGPU,
        instance->texturesUploadedToGPU
    );
    
    if (uploadComplete) {
        // All meshes and textures uploaded, now initialize bounding box
        instance->boundingBox.init();
        instance->initializeBoundingBox();
        
        // NOW add to active models collection (GPU upload is 100% complete)
        // This prevents render race condition - model is fully initialized before rendering
        int index = m_pendingGPUUploadIndex;
        if (index >= static_cast<int>(models.size())) {
            models.resize(index + 1);
        }
        models[index] = std::move(m_pendingGPUUploadInstance);
        m_pendingGPUUploadInstance.reset();
        m_pendingGPUUploadIndex = -1;
        
        instance->gpuUploadComplete = true;
        
        // Initialize instance attributes with identity matrix for non-instanced rendering
        // This ensures the shader always has valid instance matrix data
        ModelInstance* addedInstance = models[index].get();
        if (addedInstance) {
            std::vector<glm::mat4> identityMatrix = { glm::mat4(1.0f) };
            addedInstance->setupInstancing(identityMatrix);
        }
        
        LOG_INFO("[ModelManager] GPU upload completed for model index " + std::to_string(index));
        return true; // Ready for UI initialization
    }
    
    return false; // Still uploading
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
        
        // Bind instance VBO before rendering (required for instance attributes to work)
        // Even for non-instanced models, we need to bind the VBO so the shader can read the identity matrix
        if (instance->instanceVBO != 0) {
            glBindBuffer(GL_ARRAY_BUFFER, instance->instanceVBO);
        }
        
        // Render the model
        instance->model.render(shader, modelMatrixToUse);
        
        // Render benchmark crowd if active (hardware instancing)
        // Each model renders only its own instances; update its instanceVBO with its subset before drawing
        if (!instance->instanceMatrices.empty() && instance->instanceMatrices.size() > 1) {
            if (instance->instanceVBO != 0) {
                glBindBuffer(GL_ARRAY_BUFFER, instance->instanceVBO);
                glBufferData(GL_ARRAY_BUFFER,
                             static_cast<GLsizeiptr>(instance->instanceMatrices.size() * sizeof(glm::mat4)),
                             instance->instanceMatrices.data(), GL_DYNAMIC_DRAW);
            }
            instance->model.renderInstanced(shader, static_cast<int>(instance->instanceMatrices.size()));
        }
        
        // Unbind instance VBO after rendering
        glBindBuffer(GL_ARRAY_BUFFER, 0);
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
        
        // Bind instance VBO before rendering (required for instance attributes to work)
        // Even for non-instanced models, we need to bind the VBO so the shader can read the identity matrix
        if (instance->instanceVBO != 0) {
            glBindBuffer(GL_ARRAY_BUFFER, instance->instanceVBO);
        }
        
        // Render the model
        instance->model.render(shader, modelMatrixToUse);
        
        // Render benchmark crowd if active (hardware instancing)
        // Each model renders only its own instances; update its instanceVBO with its subset before drawing
        if (!instance->instanceMatrices.empty() && instance->instanceMatrices.size() > 1) {
            if (instance->instanceVBO != 0) {
                glBindBuffer(GL_ARRAY_BUFFER, instance->instanceVBO);
                glBufferData(GL_ARRAY_BUFFER,
                             static_cast<GLsizeiptr>(instance->instanceMatrices.size() * sizeof(glm::mat4)),
                             instance->instanceMatrices.data(), GL_DYNAMIC_DRAW);
            }
            instance->model.renderInstanced(shader, static_cast<int>(instance->instanceMatrices.size()));
        }
        
        // Unbind instance VBO after rendering
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        // Draw skeleton if enabled (after model is rendered)
        // OPTIMIZATION: Skeleton is only drawn once per ModelInstance (base model only)
        // Benchmark instances do NOT get skeleton rendering to prevent FPS drops
        // The skeleton represents the base model's rig structure, not the instanced copies
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
            
            // Draw skeleton ONCE for the base model only (ignores benchmark instances)
            // This prevents FPS drops when rendering hundreds of benchmark instances
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

// Setup hardware instancing for a model instance
void ModelInstance::setupInstancing(const std::vector<glm::mat4>& matrices) {
    instanceMatrices = matrices;
    
    // Initialize instance VBO if not already created
    if (instanceVBO == 0) {
        glGenBuffers(1, &instanceVBO);
    }
    
    // Bind and upload instance matrices
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, matrices.size() * sizeof(glm::mat4), matrices.data(), GL_DYNAMIC_DRAW);
    
    // Set up instance attributes for ALL meshes (each mesh needs its VAO configured)
    // All meshes share the same instance data (instanceVBO)
    GLsizei vec4Size = sizeof(glm::vec4);
    const std::vector<Mesh>& meshes = model.getMeshes();
    for (const Mesh& mesh : meshes) {
        if (mesh.VAO != 0) {
            glBindVertexArray(mesh.VAO);
            
            // Bind instance VBO to this VAO
            glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
            
            // Set up vertex attributes for mat4 (4 vec4 columns)
            // Location 5, 6, 7, 8 for the 4 columns of the instance matrix
            for (unsigned int i = 0; i < 4; i++) {
                glEnableVertexAttribArray(5 + i);
                glVertexAttribPointer(5 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * vec4Size));
                glVertexAttribDivisor(5 + i, 1);  // CRITICAL: This makes it instanced (advance once per instance)
            }
            
            glBindVertexArray(0);
        }
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Spawn a grid of model instances from one or more source models (for stress testing).
// Each cell uses a random source from sourceIndices; each model stores only its own instances.
// Clears all models' instance data before spawning to prevent ghost instances.
void ModelManager::spawnBenchmarkGrid(const std::vector<int>& sourceIndices, int rows, int cols, float spacing) {
    if (sourceIndices.empty()) {
        LOG_ERROR("[Benchmark] No source models selected");
        return;
    }
    const int modelCount = static_cast<int>(models.size());
    for (int idx : sourceIndices) {
        if (idx < 0 || idx >= modelCount) {
            LOG_ERROR("[Benchmark] Invalid source model index: " + std::to_string(idx));
            return;
        }
    }

    // Performance fix: clear all instance data for all models before spawning to prevent ghost instances
    std::vector<glm::mat4> identityMatrix = { glm::mat4(1.0f) };
    for (auto& instance : models) {
        if (instance) {
            instance->benchmarkOffsets.clear();
            instance->instanceRandomSeeds.clear();
            instance->instanceMatrices.clear();
            instance->setupInstancing(identityMatrix);
        }
    }

    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    int instanceCount = 0;
    for (int z = 0; z < rows; z++) {
        for (int x = 0; x < cols; x++) {
            int chosenModelIdx = sourceIndices[static_cast<size_t>(std::rand()) % sourceIndices.size()];
            ModelInstance* inst = models[chosenModelIdx].get();
            if (!inst) continue;

            float posX = (x - (cols / 2.0f)) * spacing;
            float posZ = (z - (rows / 2.0f)) * spacing;
            glm::vec3 offset(posX, 0.0f, posZ);

            inst->benchmarkOffsets.push_back(offset);

            glm::vec3 randomSeed(
                (static_cast<float>(std::rand()) / RAND_MAX) * 2.0f - 1.0f,
                (static_cast<float>(std::rand()) / RAND_MAX) * 2.0f - 1.0f,
                (static_cast<float>(std::rand()) / RAND_MAX) * 2.0f - 1.0f
            );
            inst->instanceRandomSeeds.push_back(randomSeed);

            glm::mat4 baseMat = glm::mat4(1.0f);
            baseMat = glm::translate(baseMat, inst->model.pos);
            baseMat = baseMat * glm::mat4_cast(inst->model.rotation);
            baseMat = glm::scale(baseMat, inst->model.size);
            glm::mat4 instanceMat = glm::translate(glm::mat4(1.0f), offset) * baseMat;
            inst->instanceMatrices.push_back(instanceMat);

            instanceCount++;
        }
    }

    // Upload each model's instance matrices to GPU (only models that received instances need update)
    for (auto& instance : models) {
        if (instance && !instance->instanceMatrices.empty()) {
            instance->setupInstancing(instance->instanceMatrices);
        }
    }

    LOG_INFO("[Benchmark] Spawning " + std::to_string(instanceCount) + " instances from " +
             std::to_string(sourceIndices.size()) + " source model(s) via hardware instancing.");
}

// Clear benchmark models (clears all benchmark offset vectors and instance data)
void ModelManager::clearBenchmarkModels() {
    // Iterate through all models and clear their benchmark offsets and instance data
    for (auto& instance : models) {
        if (instance) {
            // Clear benchmark offsets and random seeds
            instance->benchmarkOffsets.clear();
            instance->instanceRandomSeeds.clear();
            
            // CRITICAL: Reset instancing to identity matrix instead of deleting VBO
            // This ensures the shader always has valid instance matrix data
            // An empty vector would cause the shader to read invalid data
            std::vector<glm::mat4> identityMatrix = { glm::mat4(1.0f) };
            instance->setupInstancing(identityMatrix);
            
            LOG_INFO("[Benchmark] Reset instancing for model to identity matrix");
        }
    }
    
    LOG_INFO("[Benchmark] Cleared all benchmark offsets and reset instance buffers to identity");
}

// Update instance buffers for all models with benchmark instances
void ModelManager::updateInstanceBuffers() {
    for (auto& instance : models) {
        if (instance && !instance->instanceMatrices.empty()) {
            // Re-upload instance matrices if they've changed
            if (instance->instanceVBO != 0) {
                glBindBuffer(GL_ARRAY_BUFFER, instance->instanceVBO);
                glBufferData(GL_ARRAY_BUFFER, instance->instanceMatrices.size() * sizeof(glm::mat4), 
                            instance->instanceMatrices.data(), GL_DYNAMIC_DRAW);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
            }
        }
    }
}

// Update benchmark instance matrices with real-time randomization
void ModelManager::updateBenchmarkMatrices(float posJitter, float rotJitter, float scaleJitter) {
    for (auto& instance : models) {
        if (!instance || instance->benchmarkOffsets.empty() || instance->instanceRandomSeeds.empty()) {
            continue;  // Skip if no benchmark instances or random seeds
        }
        
        // Ensure random seeds match benchmark offsets count
        if (instance->instanceRandomSeeds.size() != instance->benchmarkOffsets.size()) {
            LOG_WARNING("[Benchmark] Random seeds count mismatch, skipping update");
            continue;
        }
        
        // Get base transform components for randomization
        glm::vec3 basePos = instance->model.pos;
        glm::quat baseRotation = instance->model.rotation;
        glm::vec3 baseScale = instance->model.size;
        
        // Recalculate instance matrices with randomization
        instance->instanceMatrices.clear();
        instance->instanceMatrices.reserve(instance->benchmarkOffsets.size());
        
        for (size_t i = 0; i < instance->benchmarkOffsets.size(); ++i) {
            const glm::vec3& baseOffset = instance->benchmarkOffsets[i];
            const glm::vec3& seed = instance->instanceRandomSeeds[i];
            
            // Calculate randomized position offset: baseOffset + seed * posJitter
            // Y-axis is locked to 0.0f to keep characters on the ground
            // Use seed.x for X jitter, seed.y for Z jitter (Y is locked)
            glm::vec3 posJitterVec(seed.x * posJitter, 0.0f, seed.y * posJitter);
            glm::vec3 jitteredOffset = baseOffset + posJitterVec;
            
            // Calculate randomized rotation: baseRotation * additional Y-axis rotation
            float rotationAngle = seed.y * rotJitter;  // Rotation in radians
            glm::quat jitterRotation = glm::angleAxis(rotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::quat jitteredRotation = baseRotation * jitterRotation;
            
            // Calculate randomized scale: baseScale * (1.0 + seed.z * scaleJitter)
            // This applies uniform scale jitter
            float scaleFactor = 1.0f + (seed.z * scaleJitter);
            glm::vec3 jitteredScale = baseScale * scaleFactor;
            
            // Build instance matrix matching spawnBenchmarkGrid structure:
            // translate(jitteredOffset) * translate(basePos) * rotate(jitteredRotation) * scale(jitteredScale)
            glm::mat4 jitteredBaseMat = glm::mat4(1.0f);
            jitteredBaseMat = glm::translate(jitteredBaseMat, basePos);
            jitteredBaseMat = jitteredBaseMat * glm::mat4_cast(jitteredRotation);
            jitteredBaseMat = glm::scale(jitteredBaseMat, jitteredScale);
            
            // Apply jittered offset (same as original: translate(offset) * baseMat)
            glm::mat4 instanceMat = glm::translate(glm::mat4(1.0f), jitteredOffset) * jitteredBaseMat;
            
            instance->instanceMatrices.push_back(instanceMat);
        }
        
        // Update GPU buffer with new matrices
        if (instance->instanceVBO != 0 && !instance->instanceMatrices.empty()) {
            glBindBuffer(GL_ARRAY_BUFFER, instance->instanceVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, 
                           instance->instanceMatrices.size() * sizeof(glm::mat4),
                           instance->instanceMatrices.data());
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
    }
}