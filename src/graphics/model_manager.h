#ifndef MODEL_MANAGER_H
#define MODEL_MANAGER_H

#include "model.h"
#include "bounding_box.h"
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <map>
#include "ofbx.h"  // For ofbx::IScene
#include <glm/glm.hpp>
#include <future>
#include <mutex>
#include <atomic>

//-- Structure to hold a model instance with its own animation state
struct ModelInstance {
    Model model;                    // The model itself
    std::string filePath;           // Path to the loaded file
    std::string displayName;        // Display name (filename)
    
    // Animation state (independent for each model)
    float animationTime;            // Current animation time in seconds
    float totalTime;                // Accumulated time for auto-play
    unsigned int FPS;               // FPS of this model's animation
    bool isPlaying;                 // Whether this model's animation is playing
    double lastUpdateTime;          // Last time this model was updated
    
    // Bounding box for this model instance
    // Automatically calculated when model is loaded and updated in real-time
    // during animation/deformation to reflect current model state
    BoundingBox boundingBox;
    
    // Cached bone transforms for bounding box calculation (performance optimization)
    // This avoids recalculating bone transforms twice per frame
    std::vector<glm::mat4> cachedBoneTransforms;
    bool boneTransformsCached = false;
    
    // Benchmark instancing: Store offsets for lightweight rendering instancing
    // Instead of loading multiple models from disk, we render the same model multiple times
    // at different positions. This is memory-efficient and prevents Outliner overload.
    std::vector<glm::vec3> benchmarkOffsets;
    
    // Random seeds for each benchmark instance (for real-time randomization)
    // Each seed is a vec3 with values between -1.0 and 1.0, used to generate
    // consistent randomization that can be scaled by jitter sliders
    // seed.x = X position jitter direction
    // seed.y = Z position jitter direction (also used for rotation jitter)
    // seed.z = scale jitter direction (uniform scale)
    // Note: Y position is locked to 0.0f to keep characters on the ground
    std::vector<glm::vec3> instanceRandomSeeds;
    
    // Hardware instancing: GPU-side instancing for benchmark tool
    unsigned int instanceVBO = 0;  // VBO for instance matrices
    std::vector<glm::mat4> instanceMatrices;  // Instance transformation matrices
    
    // Setup hardware instancing with instance matrices
    void setupInstancing(const std::vector<glm::mat4>& matrices);
    
    // Constructor
    ModelInstance() 
        : animationTime(0.0f), totalTime(0.0f), FPS(1), 
          isPlaying(false), lastUpdateTime(0.0) {
    }
    
    // Delete copy constructor and copy assignment (Model is not copyable)
    ModelInstance(const ModelInstance&) = delete;
    ModelInstance& operator=(const ModelInstance&) = delete;
    
    // Move constructor
    ModelInstance(ModelInstance&& other) noexcept
        : model(std::move(other.model)),
          filePath(std::move(other.filePath)),
          displayName(std::move(other.displayName)),
          animationTime(other.animationTime),
          totalTime(other.totalTime),
          FPS(other.FPS),
          isPlaying(other.isPlaying),
          lastUpdateTime(other.lastUpdateTime),
          boundingBox(std::move(other.boundingBox)),
          cachedBoneTransforms(std::move(other.cachedBoneTransforms)),
          boneTransformsCached(other.boneTransformsCached),
          benchmarkOffsets(std::move(other.benchmarkOffsets)),
          instanceRandomSeeds(std::move(other.instanceRandomSeeds)),
          instanceVBO(other.instanceVBO),
          instanceMatrices(std::move(other.instanceMatrices)) {
        other.instanceVBO = 0;
    }
    
    // Move assignment operator
    ModelInstance& operator=(ModelInstance&& other) noexcept {
        if (this != &other) {
            model = std::move(other.model);
            filePath = std::move(other.filePath);
            displayName = std::move(other.displayName);
            animationTime = other.animationTime;
            totalTime = other.totalTime;
            FPS = other.FPS;
            isPlaying = other.isPlaying;
            lastUpdateTime = other.lastUpdateTime;
            boundingBox = std::move(other.boundingBox);
            cachedBoneTransforms = std::move(other.cachedBoneTransforms);
            boneTransformsCached = other.boneTransformsCached;
            benchmarkOffsets = std::move(other.benchmarkOffsets);
            instanceRandomSeeds = std::move(other.instanceRandomSeeds);
            instanceVBO = other.instanceVBO;
            instanceMatrices = std::move(other.instanceMatrices);
            other.instanceVBO = 0;
        }
        return *this;
    }
    
    // Initialize animation state after loading
    void initializeAnimation() {
        FPS = model.getFPS();
        if (FPS == 0) FPS = 1; // Prevent division by zero
        animationTime = 0.0f;
        totalTime = 0.0f;
        isPlaying = false;
    }
    
    /**
     * Calculate bounding box in background thread (CPU-bound)
     * Stores the result to be applied later in GPU phase
     */
    void calculateBoundingBoxAsync() {
        model.getBoundingBox(preCalculatedBoundingBoxMin, preCalculatedBoundingBoxMax);
    }
    
    /**
     * Initialize bounding box after loading (GPU phase)
     * 
     * Uses pre-calculated bounding box from calculateBoundingBoxAsync()
     * and updates the bounding box instance. This is called automatically
     * when a model is loaded.
     */
    void initializeBoundingBox() {
        boundingBox.update(preCalculatedBoundingBoxMin, preCalculatedBoundingBoxMax);
    }
    
    // Pre-calculated bounding box (calculated in background thread)
    glm::vec3 preCalculatedBoundingBoxMin = glm::vec3(-1.0f);
    glm::vec3 preCalculatedBoundingBoxMax = glm::vec3(1.0f);
    
    // Pre-loaded FBX scene (loaded in background thread, used for Outliner)
    ofbx::IScene* ofbxScene = nullptr;
    
    // Pre-calculated rig root name (calculated in background thread, avoids heavy traversal on main thread)
    std::string rigRootName;
    
    // GPU upload progress tracking (for time-sliced incremental uploading)
    int meshesUploadedToGPU = 0;      // Number of meshes that have been uploaded to GPU
    int texturesUploadedToGPU = 0;    // Number of textures that have been uploaded to GPU
    bool gpuUploadComplete = false;   // True when all meshes and textures are uploaded
    
    // Update animation time (for auto-play)
    void updateAnimation(double currentTime, float deltaTime) {
        if (isPlaying) {
            totalTime += deltaTime;
            
            // Loop animation if it exceeds duration
            float animDuration = model.getAnimationDuration();
            if (animDuration > 0.0f && FPS > 0) {
                float animTimeInTicks = totalTime * FPS;
                if (animTimeInTicks >= animDuration) {
                    totalTime = std::fmod(animTimeInTicks, animDuration) / FPS;
                }
            }
            
            animationTime = totalTime;
        }
        lastUpdateTime = currentTime;
    }
    
    // Set animation time manually (for scrubbing)
    void setAnimationTime(float time) {
        animationTime = time;
        totalTime = time;
    }
    
    /**
     * Play/pause animation (toggle play state without resetting time).
     * 
     * This method only changes the play state. It does NOT reset animation time.
     * Use this for Play/Pause functionality where pausing keeps the character
     * in its current pose.
     * 
     * To reset animation to frame 0, use stop() instead.
     * 
     * @param playing True to play animation, false to pause (keeps current frame)
     */
    void setPlaying(bool playing) {
        isPlaying = playing;
    }
    
    /**
     * Stop animation and reset to frame 0.
     * 
     * This method stops the animation and resets both animationTime and totalTime
     * to 0.0f, ensuring the animation starts from the beginning when Play is
     * pressed again.
     * 
     * Use this for the Stop button functionality (separate from Pause).
     */
    void stop() {
        isPlaying = false;
        animationTime = 0.0f;
        totalTime = 0.0f;
    }
};

//-- Manager class to handle multiple models with independent animations
class ModelManager {
public:
    ModelManager();
    ~ModelManager();
    
    // Start async loading of a model (non-blocking)
    // Returns the index that will be assigned to the model, or -1 if loading is already in progress
    // Call checkAsyncLoadStatus() in the main loop to poll for completion
    int loadModelAsync(const std::string& filePath);
    
    // Check if async loading has completed and finalize the model (GPU resource creation)
    // Returns true if a model was finalized, false otherwise
    // Must be called on the main thread (OpenGL context required)
    bool checkAsyncLoadStatus();
    
    // Continue incremental GPU upload (time-sliced, one mesh per frame)
    // Returns true when GPU upload is complete and ready for UI initialization
    // Must be called on the main thread (OpenGL context required)
    // Call this EXACTLY ONCE per frame until it returns true
    bool continueGPUUpload();
    
    // Remove a model by index
    void removeModel(int index);
    
    // Remove all models
    void clearAll();
    
    // Get number of loaded models
    size_t getModelCount() const { return models.size(); }
    
    // Get model instance by index
    ModelInstance* getModel(int index);
    
    // Update animations for all models
    void updateAnimations(double currentTime, float deltaTime);
    
    // Render all models
    // If customModelMatrix is provided and modelIndex matches a model, that matrix will be used instead of model's pos/rotation/size
    // This allows PropertyPanel RootNode transforms to control the model transform directly
    void renderAll(Shader& shader, UI_data& ui_data, std::vector<glm::mat4>& transforms, 
                   int modelIndex = -1, const glm::mat4* customModelMatrix = nullptr);
    
    // Render all models with per-model RootNode transforms
    // Each model uses its own RootNode transform matrix from PropertyPanel (ensures hierarchy isolation)
    // skeletonShader: Shader to use for skeleton drawing (if models have m_showSkeleton enabled)
    void renderAllWithRootNodeTransforms(Shader& shader, UI_data& ui_data, std::vector<glm::mat4>& transforms,
                                        const std::map<int, glm::mat4>& modelRootNodeMatrices,
                                        const glm::mat4& view, const glm::mat4& projection, Shader* skeletonShader = nullptr);
    
    // Get bounding box for all models combined
    void getCombinedBoundingBox(glm::vec3& min, glm::vec3& max);
    
    // Get bounding box for a specific model
    void getModelBoundingBox(int index, glm::vec3& min, glm::vec3& max);
    
    // Get bounding box for a specific bone by index in a specific model (OPTIMIZED - zero string operations)
    void getNodeBoundingBoxByIndex(int modelIndex, int boneIndex, glm::vec3& min, glm::vec3& max);
    
    // Enable/disable bounding boxes for all models
    void setBoundingBoxesEnabled(bool enabled);
    
    // Get whether bounding boxes are enabled (returns true if at least one model has bounding box enabled)
    bool areBoundingBoxesEnabled() const;
    
    // Get the current bounding box enabled state (for new models)
    bool getBoundingBoxesEnabled() const { return mBoundingBoxesEnabled; }
    
    // Enable/disable skeletons for all models
    void setSkeletonsEnabled(bool enabled);
    
    // Get the current skeleton enabled state (for new models)
    bool getSkeletonsEnabled() const { return mSkeletonsEnabled; }
    
    // Stop all animations and reset to frame 0
    void stopAllAnimations();
    
    // Selection management
    void setSelectedModel(int index);  // Select a model by index (-1 to deselect)
    int getSelectedModelIndex() const { return mSelectedModelIndex; }
    ModelInstance* getSelectedModel();  // Get the currently selected model instance
    
    // Benchmark tools
    // Spawn a grid of model instances from one or more source models (for stress testing).
    // Each grid cell picks a random source from sourceIndices; each model keeps its own instance matrices.
    // Uses hardware instancing per model. Clears all instance data before spawning to prevent ghost instances.
    void spawnBenchmarkGrid(const std::vector<int>& sourceIndices, int rows, int cols, float spacing);
    
    // Clear benchmark models (removes all models except the original source models)
    // Keeps models at indices 0 to (originalCount - 1), removes all others
    void clearBenchmarkModels();
    
    // Update instance buffers for all models with benchmark instances
    void updateInstanceBuffers();
    
    // Update benchmark instance matrices with real-time randomization
    // Recalculates instance matrices based on jitter values and random seeds
    // posJitter: Position jitter amount (scales random seed.x, y, z for position)
    // rotJitter: Rotation jitter amount (scales random seed.y for Y-axis rotation)
    // scaleJitter: Scale jitter amount (scales random seed.z for uniform scale)
    void updateBenchmarkMatrices(float posJitter, float rotJitter, float scaleJitter);
    
private:
    std::vector<std::unique_ptr<ModelInstance>> models;  // Collection of model instances (using unique_ptr to avoid moving)
    
    // Current bounding box enabled state (for new models)
    bool mBoundingBoxesEnabled = true;
    
    // Current skeleton enabled state (for new models)
    bool mSkeletonsEnabled = false;
    
    // Selection state
    int mSelectedModelIndex = -1;  // Index of selected model (-1 = none selected)
    
    // Helper to extract filename from path
    std::string extractFileName(const std::string& path);
    
    // Async loading state
    struct AsyncLoadTask {
        std::string filePath;
        std::future<int> future;  // Future that returns model index when CPU phase completes
        int assignedIndex;         // Index that will be assigned to the model
        bool cpuPhaseComplete;    // True when CPU phase is done, ready for GPU phase
        std::unique_ptr<ModelInstance> instance;  // Model instance (created after CPU phase)
        
        AsyncLoadTask() : assignedIndex(-1), cpuPhaseComplete(false) {}
    };
    
    std::unique_ptr<AsyncLoadTask> m_pendingLoadTask;  // Currently pending async load (only one at a time)
    std::mutex m_loadMutex;  // Mutex for thread-safe access to loading state
    std::atomic<bool> m_isLoading;  // Atomic flag to prevent concurrent loading
    
    // Temporary storage for model during incremental GPU upload (prevents render race condition)
    // Model is NOT added to active models collection until GPU upload is 100% complete
    std::unique_ptr<ModelInstance> m_pendingGPUUploadInstance;
    int m_pendingGPUUploadIndex = -1;  // Index that will be assigned when upload completes
};

#endif // MODEL_MANAGER_H
