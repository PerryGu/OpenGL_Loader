#ifndef MODEL_MANAGER_H
#define MODEL_MANAGER_H

#include "model.h"
#include "bounding_box.h"
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <map>
#include <glm/glm.hpp>

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
          boneTransformsCached(other.boneTransformsCached) {
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
     * Initialize bounding box after loading
     * 
     * Calculates the initial bounding box from the model's geometry
     * and updates the bounding box instance. This is called automatically
     * when a model is loaded.
     */
    void initializeBoundingBox() {
        glm::vec3 min, max;
        model.getBoundingBox(min, max);
        boundingBox.update(min, max);
    }
    
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
    
    // Load a new model and add it to the collection
    // Returns the index of the newly loaded model, or -1 on failure
    int loadModel(const std::string& filePath);
    
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
    
    // Get bounding box for a specific node in a specific model (DEPRECATED - use index-based version)
    void getNodeBoundingBox(int modelIndex, const std::string& nodeName, glm::vec3& min, glm::vec3& max);
    
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
};

#endif // MODEL_MANAGER_H
