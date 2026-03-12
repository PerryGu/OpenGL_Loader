/**
 * @file model.cpp
 * @brief Core Model class implementation for 3D mesh loading, skeletal animation, and rendering.
 * 
 * ## Architecture Overview
 * 
 * ### Linear Skeleton Optimization
 * This implementation uses a **Linear Skeleton** architecture instead of traditional recursive
 * hierarchy traversal. The skeleton hierarchy is flattened into a linear array (`m_linearSkeleton`)
 * during model load time, providing significant performance benefits:
 * 
 * **Performance Benefits:**
 * - **O(n) complexity**: Single linear loop replaces recursive traversal
 * - **Zero string operations**: Index-based lookups eliminate expensive string comparisons
 * - **Cache-friendly**: Sequential memory access improves CPU cache utilization
 * - **Zero map lookups**: Direct array indexing replaces map-based node lookups
 * - **Dirty flag optimization**: Skips updates when animation time and UI data are unchanged
 * 
 * **How It Works:**
 * 1. During `loadModel()`, `flattenHierarchy()` recursively traverses the scene once to build
 *    the linear skeleton structure with parent-child relationships stored as indices.
 * 2. During animation updates, `updateLinearSkeleton()` iterates linearly through the array,
 *    computing global transforms using cached parent matrices.
 * 3. Bone lookups use index-based caches (`m_NodeToBoneIndexCache`, `m_NodeToAnimChannelCache`)
 *    to eliminate string operations in the hot path.
 * 
 * ### Sphere Impostor Technique
 * The skeleton rendering uses a **Sphere Impostor** technique in the fragment shader to render
 * 3D-looking spheres for joints using only 2D point primitives. This provides:
 * - **Performance**: No geometry shader or tessellation required
 * - **Visual Quality**: Realistic 3D shading with lighting calculations
 * - **Scalability**: Point size dynamically linked to skeleton line width setting
 * 
 * The technique works by:
 * 1. Rendering GL_POINTS with `gl_PointSize` set in vertex shader
 * 2. In fragment shader, using `gl_PointCoord` to calculate sphere surface normals
 * 3. Applying fake 3D lighting based on calculated normals
 * 4. Discarding pixels outside the circular boundary for clean sphere appearance
 * 
 * @author OpenGL_Loader Development Team
 * @version 2.0.0
 */

#include "model.h"
#include "texture.h"  // For PreDecodedImageData

// Forward declaration for stbi_image_free (implementation is in texture.cpp)
extern "C" void stbi_image_free(void* retval_from_stbi_load);
#include "../utils/logger.h"
#include "raycast.h"  // For Ray and Raycast namespace
#include "../io/app_settings.h"  // For AppSettingsManager (skeleton line width)
#include <set>
#include <cfloat>  // For FLT_MAX
#include <functional>  // For std::function
#include <utility>  // For std::move
#include <algorithm>  // For std::transform, std::clamp, std::replace
#include <limits>  // For std::numeric_limits
#include <string>  // For std::string
#include <filesystem>  // For std::filesystem::path (directory extraction)
#include <glm/glm.hpp>
#include "ofbx.h"

// Helper function to convert OpenFBX matrices to GLM
glm::mat4 ofbxToGlm(const ofbx::Matrix& m) {
    glm::mat4 result;
    for(int i = 0; i < 16; ++i) result[i/4][i%4] = (float)m.m[i];
    return result;
}

// Helper function to convert GLM matrices to Matrix4f
Matrix4f glmToMatrix4f(const glm::mat4& m) {
    Matrix4f result;
    for(int i = 0; i < 4; ++i) {
        for(int j = 0; j < 4; ++j) {
            result.m[i][j] = m[j][i]; // GLM is column-major, Matrix4f is row-major
        }
    }
    return result;
}

// Helper function to recursively traverse OpenFBX scene and process SKIN objects
void processOpenFBXObjectsRecursive(const ofbx::Object* obj, std::vector<const ofbx::Skin*>& skins) {
    if (!obj) return;
    
    if (obj->getType() == ofbx::Object::Type::SKIN) {
        skins.push_back(static_cast<const ofbx::Skin*>(obj));
    }
    
    // Traverse children
    int i = 0;
    while (const ofbx::Object* child = const_cast<ofbx::Object*>(obj)->resolveObjectLink(i)) {
        processOpenFBXObjectsRecursive(child, skins);
        ++i;
    }
}


//-- initialize Model ----------------------
Model::Model(glm::vec3 pos, glm::vec3 size, bool noTex)
    : pos(pos), size(size), rotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)), noTex(noTex) {
    // rotation initialized to identity quaternion (no rotation)

    mScene = NULL;
    //mMeshIndex = 0;
    mNumBones = 0;

}

// Move constructor
// Note: Assimp::Importer is not movable, so we create a new one
// The scene pointer cannot be safely transferred, so we set it to NULL
// Models should not be moved after loading (we use unique_ptr to avoid this)
Model::Model(Model&& other) noexcept
    : pos(other.pos), size(other.size), rotation(other.rotation), noTex(other.noTex),
      mFPS(0), mScene(NULL),  // Reset scene - cannot transfer ownership
      meshes(std::move(other.meshes)),
      directory(std::move(other.directory)),
      file_extension(std::move(other.file_extension)),
      textures_loaded(std::move(other.textures_loaded)),
      m_Entries(std::move(other.m_Entries)),
      mNumBones(0),  // Reset bone data
      m_BoneMapping(std::move(other.m_BoneMapping)),
      m_BoneInfo(std::move(other.m_BoneInfo)),
      m_BoneExtraRotations(std::move(other.m_BoneExtraRotations)),
      GlobalTransformation(other.GlobalTransformation),
      m_GlobalInverseTransform(other.m_GlobalInverseTransform),
      mIporter() {  // Create new Importer (cannot move Assimp::Importer)
    // Reset the moved-from object
    other.mScene = NULL;
    other.mNumBones = 0;
    other.mFPS = 0;
}

// Move assignment operator
// Note: Assimp::Importer is not movable, so we create a new one
Model& Model::operator=(Model&& other) noexcept {
    if (this != &other) {
        // Cleanup current resources
        cleanup();
        
        // Move all members except mIporter and mScene
        pos = other.pos;
        size = other.size;
        rotation = other.rotation;
        noTex = other.noTex;
        meshes = std::move(other.meshes);
        directory = std::move(other.directory);
        file_extension = std::move(other.file_extension);
        textures_loaded = std::move(other.textures_loaded);
        m_Entries = std::move(other.m_Entries);
        m_BoneMapping = std::move(other.m_BoneMapping);
        m_BoneInfo = std::move(other.m_BoneInfo);
        m_BoneExtraRotations = std::move(other.m_BoneExtraRotations);
        GlobalTransformation = other.GlobalTransformation;
        m_GlobalInverseTransform = other.m_GlobalInverseTransform;
        
        // Cannot transfer mScene or mIporter - reset them
        mScene = NULL;
        mNumBones = 0;
        mFPS = 0;
        // mIporter is default-constructed (new empty one)
        
        // Reset the moved-from object
        other.mScene = NULL;
        other.mNumBones = 0;
        other.mFPS = 0;
    }
    return *this;
}

void Model::init() {

}


void Model::render(Shader shader, const glm::mat4* customModelMatrix) {
    // Don't render if model is empty (no meshes)
    if (meshes.empty()) {
        return;
    }
    
    // NOTE: Shader is now activated in ModelManager::renderAll* BEFORE bone matrices are uploaded
    // This ensures bone matrices are uploaded to the correct shader program
    
    // Use custom model matrix if provided (e.g., from PropertyPanel RootNode), 
    // otherwise build from model's pos/rotation/size
    glm::mat4 model = glm::mat4(1.0f);
    if (customModelMatrix != nullptr) {
        // Use provided matrix (from PropertyPanel RootNode transforms)
        model = *customModelMatrix;
    } else {
        // Build model matrix: Translation * Rotation * Scale
        // Order matters: we want to scale first, then rotate, then translate
        model = glm::translate(model, pos);
        model = model * glm::mat4_cast(rotation);  // Apply rotation (quaternion to matrix)
        model = glm::scale(model, size);
    }
    
    shader.setMat4("model", model);
    shader.setFloat("material.shininess", 0.5f);
    shader.setInt("flipNormals", m_flipNormals ? 1 : 0);
    shader.setInt("useDefaultMaterial", m_useDefaultMaterial ? 1 : 0);

    // Set polygon mode for wireframe rendering
    if (m_wireframeMode) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // Use const reference to avoid copying Mesh objects (copy constructor is deleted for RAII safety)
    for (const Mesh& mesh : meshes) {
        mesh.render(shader);
    }
    
    // Always reset polygon mode to prevent affecting other rendered objects (e.g., ImGui UI)
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

// Render model using hardware instancing
void Model::renderInstanced(Shader& shader, int count) {
    // Don't render if model is empty (no meshes)
    if (meshes.empty() || count <= 0) {
        return;
    }
    
    // Set model uniform to identity - instance matrices will be used instead
    shader.setMat4("model", glm::mat4(1.0f));
    
    // Set shader uniforms (same as regular render)
    shader.setFloat("material.shininess", 0.5f);
    shader.setInt("flipNormals", m_flipNormals ? 1 : 0);
    shader.setInt("useDefaultMaterial", m_useDefaultMaterial ? 1 : 0);
    
    // Set polygon mode for wireframe rendering
    if (m_wireframeMode) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    
    // Render all meshes with instancing
    for (const Mesh& mesh : meshes) {
        // Safety check: Don't render if VAO is not initialized
        if (mesh.VAO == 0) {
            continue;
        }
        
        // Check if mesh has at least one valid diffuse texture
        bool hasValidDiffuse = false;
        for (size_t i = 0; i < mesh.textures.size(); i++) {
            if (mesh.textures[i].type == aiTextureType_DIFFUSE && mesh.textures[i].id > 0) {
                hasValidDiffuse = true;
                break;
            }
        }
        shader.setInt("hasDiffuseTexture", hasValidDiffuse ? 1 : 0);
        
        // Handle textures (same logic as Mesh::render)
        if (mesh.getNoTex()) {
            shader.set4Float("material.diffuse", mesh.diffuse);
            shader.set4Float("material.specular", mesh.specular);
            shader.setInt("noTex", 1);
        } else {
            static const char* diffuseNames[] = { "diffuse0", "diffuse1", "diffuse2", "diffuse3", "diffuse4", "diffuse5", "diffuse6", "diffuse7" };
            static const char* specularNames[] = { "specular0", "specular1", "specular2", "specular3", "specular4", "specular5", "specular6", "specular7" };
            
            unsigned int diffuseIdx = 0;
            unsigned int specularIdx = 0;
            unsigned int textureUnit = 0;
            
            for (unsigned int i = 0; i < mesh.textures.size(); i++) {
                if (mesh.textures[i].id == 0 || textureUnit >= 16) {
                    continue;
                }
                
                const char* name = nullptr;
                switch (mesh.textures[i].type) {
                case aiTextureType_DIFFUSE:
                    if (diffuseIdx < 8) {
                        name = diffuseNames[diffuseIdx++];
                    } else {
                        continue;
                    }
                    break;
                case aiTextureType_SPECULAR:
                    if (specularIdx < 8) {
                        name = specularNames[specularIdx++];
                    } else {
                        continue;
                    }
                    break;
                default:
                    continue;
                }
                
                glActiveTexture(GL_TEXTURE0 + textureUnit);
                shader.setInt(name, textureUnit);
                mesh.textures[i].bind();
                textureUnit++;
            }
            shader.setInt("noTex", 0);
        }
        
        // Bind VAO and render instanced
        // Note: Instance VBO should already be bound to this VAO from setupInstancing
        glBindVertexArray(mesh.VAO);
        // Instance attributes (locations 5-8) are already set up in setupInstancing
        glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_INT, 0, count);
        glBindVertexArray(0);
        
        glActiveTexture(GL_TEXTURE0);
    }
    
    // Always reset polygon mode
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

// Helper function to check if a node name should be skipped (dummy nodes, cameras, etc.)
bool Model::shouldSkipNode(const std::string& nodeName) {
    // Convert to lowercase for case-insensitive comparison
    std::string lowerName = nodeName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    
    // Skip Assimp helper nodes
    if (lowerName.find("$assimpfbx$") != std::string::npos) {
        return true;
    }
    
    // Skip Maya End Sites
    if (lowerName.find("end") != std::string::npos && lowerName.find("site") != std::string::npos) {
        return true;
    }
    
    // Skip camera nodes
    if (lowerName.find("camera") != std::string::npos) {
        return true;
    }
    
    // Skip dummy/null nodes
    if (lowerName.find("dummy") != std::string::npos || lowerName.find("null") != std::string::npos) {
        return true;
    }
    
    return false;
}

/**
 * @brief Renders the skeleton hierarchy as lines and 3D shaded sphere joints.
 * 
 * This function uses the **Linear Skeleton** optimization to efficiently iterate through
 * the bone hierarchy without recursive traversal. It renders:
 * - **Bone lines**: Green lines connecting parent-child joints
 * - **Joint spheres**: Red 3D shaded spheres using the **Sphere Impostor** technique
 * 
 * **Sphere Impostor Technique:**
 * Joints are rendered as GL_POINTS with a custom fragment shader that creates the illusion
 * of 3D spheres without geometry shaders or tessellation. The technique:
 * 1. Uses `gl_PointSize` in vertex shader to set sphere radius
 * 2. Uses `gl_PointCoord` in fragment shader to calculate sphere surface normals
 * 3. Applies fake 3D lighting based on calculated normals
 * 4. Discards pixels outside circular boundary for clean sphere appearance
 * 
 * **Performance Benefits:**
 * - No geometry shader overhead (uses simple point primitives)
 * - Point size dynamically linked to skeleton line width setting
 * - Linear iteration through `m_linearSkeleton` (O(n) complexity)
 * 
 * @param shader Skeleton shader (must support `isPoint`, `color`, `pointSize` uniforms)
 * @param view Camera view matrix
 * @param projection Camera projection matrix
 * @param modelMatrix Model's root node transform matrix
 */
void Model::DrawSkeleton(Shader& shader, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& modelMatrix) {
    // Don't draw if skeleton is disabled or scene is not loaded
    if (!m_showSkeleton || mScene == nullptr || mScene->mRootNode == nullptr || m_linearSkeleton.empty()) {
        return;
    }
    
    // Collect line vertices and joint positions by iterating through the linear skeleton
    std::vector<glm::vec3> lineVertices;
    std::vector<glm::vec3> jointPoints;
    
    // Iterate through all nodes in the linear skeleton
    for (size_t i = 0; i < m_linearSkeleton.size(); i++) {
        const LinearBone& bone = m_linearSkeleton[i];
        const aiNode* pNode = bone.pNode;
        
        if (pNode == nullptr) {
            continue;
        }
        
        // Get the animated global transform from the linear skeleton
        // This is the current frame's animated transform (not static bind pose)
        Matrix4f animatedGlobalMatrix = bone.globalMatrix;
        animatedGlobalMatrix = animatedGlobalMatrix.Transpose(); // Convert to GLM format
        glm::mat4 animatedGlobalMat4 = animatedGlobalMatrix.toGlmMatrix();
        
        // Apply modelMatrix to get world space
        glm::mat4 nodeWorldTransform = modelMatrix * animatedGlobalMat4;
        
        // Extract world position from the 4th column (translation)
        glm::vec3 nodeWorldPos = glm::vec3(nodeWorldTransform[3]);
        
        // Add this node's position to joint points (for debug visualization)
        jointPoints.push_back(nodeWorldPos);
        
        // Only draw a line if this node has a valid parent
        if (bone.parentIndex >= 0 && bone.parentIndex < static_cast<int>(m_linearSkeleton.size())) {
            const LinearBone& parentBone = m_linearSkeleton[bone.parentIndex];
            
            if (parentBone.pNode == nullptr) {
                continue;
            }
            
            // Mathematical sanity check: ensure node has a valid parent pointer
            // This prevents drawing lines from absolute root node trying to connect to null parent
            if (pNode->mParent == nullptr) {
                continue;
            }
            
            // Get parent's animated global transform
            Matrix4f parentAnimatedGlobalMatrix = parentBone.globalMatrix;
            parentAnimatedGlobalMatrix = parentAnimatedGlobalMatrix.Transpose();
            glm::mat4 parentAnimatedGlobalMat4 = parentAnimatedGlobalMatrix.toGlmMatrix();
            
            // Apply modelMatrix to get world space
            glm::mat4 parentWorldTransform = modelMatrix * parentAnimatedGlobalMat4;
            glm::vec3 parentWorldPos = glm::vec3(parentWorldTransform[3]);
            
            // Mathematical sanity checks to prevent drawing garbage/infinite lines:
            // 1. Check bone length is realistic (prevents extreme/uninitialized transforms)
            // 2. Check parent isn't at exact scene artifact origin (prevents drawing from dummy end-sites)
            float boneLength = glm::distance(parentWorldPos, nodeWorldPos);
            bool isParentAtOrigin = (parentWorldPos == glm::vec3(0.0f, 0.0f, 0.0f));
            
            // Only add line if ALL conditions are met:
            // - Parent pointer is valid (checked above)
            // - Bone length is realistic (< 100.0f units)
            // - Parent position is not at exact origin (0,0,0)
            if (boneLength < 100.0f && !isParentAtOrigin) {
                // Draw line from parent to child (raw hierarchy, no name-based filtering)
                lineVertices.push_back(parentWorldPos);
                lineVertices.push_back(nodeWorldPos);
            }
        }
    }
    
    // Disable depth test so skeleton renders on top
    glDisable(GL_DEPTH_TEST);
    
    // Enable program point size to allow vertex shader to set point size for sphere impostors
    glEnable(GL_PROGRAM_POINT_SIZE);
    // Enable point sprites to generate gl_PointCoord in Compatibility Profile
    // Without this, gl_PointCoord defaults to (0,0) and all fragments are discarded
    glEnable(GL_POINT_SPRITE);
    
    // Activate shader
    shader.activate();
    
    // Set matrices (use identity model matrix since vertices are already in absolute world space)
    glm::mat4 identityModel = glm::mat4(1.0f);
    shader.setMat4("model", identityModel);
    shader.setMat4("view", view);
    shader.setMat4("projection", projection);
    
    // Draw lines first (green bones)
    if (!lineVertices.empty()) {
        // Create temporary VAO/VBO for line drawing
        unsigned int lineVAO, lineVBO;
        glGenVertexArrays(1, &lineVAO);
        glGenBuffers(1, &lineVBO);
        
        glBindVertexArray(lineVAO);
        glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
        glBufferData(GL_ARRAY_BUFFER, lineVertices.size() * sizeof(glm::vec3), lineVertices.data(), GL_STATIC_DRAW);
        
        // Set vertex attributes (position only)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        // Set uniforms for bone lines (not points, green color)
        shader.setBool("isPoint", false);
        shader.set3Float("color", glm::vec3(0.0f, 1.0f, 0.0f)); // Green bones
        
        // Set line width from settings (user-configurable skeleton line thickness)
        float lineWidth = AppSettingsManager::getInstance().getSettings().environment.skeletonLineWidth;
        glLineWidth(lineWidth);
        
        // Draw lines
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lineVertices.size()));
        
        // Reset line width to default to avoid affecting other render passes
        glLineWidth(1.0f);
        
        // Clean up line VAO/VBO
        glBindVertexArray(0);
        glDeleteVertexArrays(1, &lineVAO);
        glDeleteBuffers(1, &lineVBO);
    }
    
    // Draw joints as 3D shaded spheres (red) using Sphere Impostor technique
    if (!jointPoints.empty()) {
        // Create temporary VAO/VBO for joint points
        unsigned int jointVAO, jointVBO;
        glGenVertexArrays(1, &jointVAO);
        glGenBuffers(1, &jointVBO);
        
        glBindVertexArray(jointVAO);
        glBindBuffer(GL_ARRAY_BUFFER, jointVBO);
        glBufferData(GL_ARRAY_BUFFER, jointPoints.size() * sizeof(glm::vec3), jointPoints.data(), GL_STATIC_DRAW);
        
        // Set vertex attributes (position only)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        // Set uniforms for joint spheres (points, red color with 3D shading)
        shader.setBool("isPoint", true);
        shader.set3Float("color", glm::vec3(1.0f, 0.0f, 0.0f)); // Red joints
        
        // Calculate point size proportional to skeleton line width setting
        // This links the joint sphere size to the bone line width for consistent visual scaling
        float lineWidth = AppSettingsManager::getInstance().getSettings().environment.skeletonLineWidth;
        // Base size of 8.0 plus a proportional increase based on the line width slider
        float calculatedPointSize = 8.0f + (lineWidth * 2.0f);
        shader.setFloat("pointSize", calculatedPointSize);
        
        // Draw points (rendered as 3D shaded spheres by fragment shader)
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(jointPoints.size()));
        
        // Clean up joint VAO/VBO
        glBindVertexArray(0);
        glDeleteVertexArrays(1, &jointVAO);
        glDeleteBuffers(1, &jointVBO);
    }
    
    // Disable program point size and point sprites to avoid affecting other render passes
    glDisable(GL_PROGRAM_POINT_SIZE);
    glDisable(GL_POINT_SPRITE);
    
    // Re-enable depth test
    glEnable(GL_DEPTH_TEST);
    
    // NOTE: Do NOT unbind shader here (glUseProgram(0))
    // The shader must remain bound so the next model's bone matrices can be uploaded correctly
    // The ModelManager will activate the correct shader before uploading bone matrices
    glBindVertexArray(0);
}

bool Model::pickBone(const Ray& ray, const glm::mat4& modelMatrix) {
    // Verbose debug messages commented out for production (v2.0.0)
    // LOG_INFO("[Raycast] Testing pickBone against skeleton...");
    
    // Don't pick if skeleton is disabled or scene is not loaded
    if (!m_showSkeleton || mScene == nullptr || mScene->mRootNode == nullptr || m_linearSkeleton.empty()) {
        // Verbose debug messages commented out for production (v2.0.0)
        // if (!m_showSkeleton) {
        //     LOG_INFO("[Raycast] Skeleton not visible (m_showSkeleton = false)");
        // } else if (mScene == nullptr) {
        //     LOG_INFO("[Raycast] Scene is null");
        // } else if (mScene->mRootNode == nullptr) {
        //     LOG_INFO("[Raycast] Root node is null");
        // } else if (m_linearSkeleton.empty()) {
        //     LOG_INFO("[Raycast] Linear skeleton is empty");
        // }
        return false;
    }
    
    // Clear previous bone selection
    m_selectedNodeName = "";
    
    // Calculate character size dynamically for proportional raycast threshold
    // This safely handles both transform-scaled and vertex-scaled FBX meshes
    glm::vec3 bboxMin, bboxMax;
    getBoundingBox(bboxMin, bboxMax);
    float modelSize = glm::length(bboxMax - bboxMin);
    
    // Base threshold: percentage of character's total size (0.8% of bounding box diagonal)
    // Fallback to minimum absolute value if bounding box is extremely small or invalid
    // This ensures bone picking works on characters of any scale (tiny to massive)
    float baseThreshold = std::max(0.05f, modelSize * 0.008f);
    
    // Try to get base threshold from AppSettingsManager if available (for UI consistency)
    // If not available, use the calculated baseThreshold
    try {
        AppSettings& settings = AppSettingsManager::getInstance().getSettings();
        // Note: Currently no bone picking threshold in settings, but this structure allows future UI control
        // baseThreshold = settings.environment.bonePickingThreshold; // Future enhancement
    } catch (...) {
        // AppSettingsManager not available, use calculated baseThreshold
    }
    
    // Traverse the model's animated skeleton hierarchy (same logic as DrawSkeleton)
    float closestBoneDistance = std::numeric_limits<float>::max();
    std::string closestBoneName = "";
    float closestBoneAdjustedThreshold = 0.0f; // Store adjusted threshold for final verification
    
    // Iterate through all nodes in the linear skeleton
    for (size_t i = 0; i < m_linearSkeleton.size(); i++) {
        const LinearBone& bone = m_linearSkeleton[i];
        const aiNode* pNode = bone.pNode;
        
        if (pNode == nullptr) {
            continue;
        }
        
        // Get the animated global transform from the linear skeleton
        // This is the current frame's animated transform (not static bind pose)
        Matrix4f animatedGlobalMatrix = bone.globalMatrix;
        animatedGlobalMatrix = animatedGlobalMatrix.Transpose(); // Convert to GLM format
        glm::mat4 animatedGlobalMat4 = animatedGlobalMatrix.toGlmMatrix();
        
        // Apply modelMatrix to get world space
        glm::mat4 nodeWorldTransform = modelMatrix * animatedGlobalMat4;
        
        // Extract world position from the 4th column (translation)
        glm::vec3 nodeWorldPos = glm::vec3(nodeWorldTransform[3]);
        
        // Only check bone segments if this node has a valid parent
        if (bone.parentIndex >= 0 && bone.parentIndex < static_cast<int>(m_linearSkeleton.size())) {
            const LinearBone& parentBone = m_linearSkeleton[bone.parentIndex];
            
            if (parentBone.pNode == nullptr) {
                continue;
            }
            
            // Mathematical sanity check: ensure node has a valid parent pointer
            if (pNode->mParent == nullptr) {
                continue;
            }
            
            // Get parent's animated global transform
            Matrix4f parentAnimatedGlobalMatrix = parentBone.globalMatrix;
            parentAnimatedGlobalMatrix = parentAnimatedGlobalMatrix.Transpose();
            glm::mat4 parentAnimatedGlobalMat4 = parentAnimatedGlobalMatrix.toGlmMatrix();
            
            // Apply modelMatrix to get world space
            glm::mat4 parentWorldTransform = modelMatrix * parentAnimatedGlobalMat4;
            glm::vec3 parentWorldPos = glm::vec3(parentWorldTransform[3]);
            
            // Mathematical sanity checks (same as DrawSkeleton):
            // 1. Check bone length is realistic (prevents extreme/uninitialized transforms)
            // 2. Check parent isn't at exact scene artifact origin (prevents picking dummy end-sites)
            float boneLength = glm::distance(parentWorldPos, nodeWorldPos);
            bool isParentAtOrigin = (parentWorldPos == glm::vec3(0.0f, 0.0f, 0.0f));
            
            // Only check valid bone segments
            if (boneLength < 100.0f && !isParentAtOrigin) {
                // Calculate distance from ray to this bone segment
                float distance = Raycast::rayToLineSegmentDistance(ray, parentWorldPos, nodeWorldPos);
                
                // Calculate distance from camera to bone segment for distance-aware threshold
                // Use the midpoint of the bone segment as reference point
                glm::vec3 boneMidpoint = (parentWorldPos + nodeWorldPos) * 0.5f;
                
                // Project bone midpoint onto ray to find closest point on ray
                glm::vec3 toBoneMidpoint = boneMidpoint - ray.origin;
                float tRay = glm::dot(toBoneMidpoint, ray.direction);
                tRay = std::max(0.0f, tRay); // Clamp to ensure point is in front of camera
                glm::vec3 closestPointOnRay = ray.origin + tRay * ray.direction;
                
                // Calculate distance from camera origin to closest point on ray
                float distanceToCamera = glm::length(closestPointOnRay - ray.origin);
                
                // Distance-aware threshold: Adjust threshold based on camera distance
                // This ensures picking feels equally "sticky" regardless of camera distance
                // Formula: adjustedThreshold = baseThreshold * (distanceToCamera * 0.05f)
                float adjustedThreshold = baseThreshold * (distanceToCamera * 0.05f);
                
                // Clamp threshold to prevent it from becoming too small (near camera) or too large (far camera)
                // Minimum: 0.05f (prevents picking from being too sensitive when close)
                // Maximum: 0.5f (prevents picking from being too loose when far)
                adjustedThreshold = std::clamp(adjustedThreshold, 0.05f, 0.5f);

                // Distance-aware threshold check - scales with both character size and camera distance
                if (distance < adjustedThreshold && distance < closestBoneDistance) {
                    closestBoneDistance = distance;
                    closestBoneAdjustedThreshold = adjustedThreshold; // Store adjusted threshold for final verification
                    // The visual segment always selects its driving parent joint
                    closestBoneName = std::string(parentBone.pNode->mName.C_Str());
                }
            }
        }
    }
    
    // If closest bone is within picking threshold, select it
    // Use the stored adjusted threshold that was used to find this bone (ensures consistency)
    if (!closestBoneName.empty() && closestBoneAdjustedThreshold > 0.0f) {
        // Verify the bone is still within the adjusted threshold (double-check for safety)
        if (closestBoneDistance < closestBoneAdjustedThreshold) {
            m_selectedNodeName = closestBoneName;
            
            // Debug logging: Log adjusted threshold value for verification (only when bone is successfully hit)
            LOG_DEBUG("[BonePicking] Hit Bone '" + closestBoneName + "' - Ray Distance: " + std::to_string(closestBoneDistance) + 
                      ", Adjusted Threshold: " + std::to_string(closestBoneAdjustedThreshold) + 
                      ", Base Threshold: " + std::to_string(baseThreshold));
            return true;
        }
    } else {
        // Verbose debug messages commented out for production (v2.0.0)
        // if (closestBoneName.empty()) {
        //     LOG_INFO("[Raycast] No valid bones found in skeleton");
        // } else {
        //     LOG_INFO("[Raycast] Closest bone '" + closestBoneName + "' at distance " + std::to_string(closestBoneDistance) + " exceeds threshold " + std::to_string(dynamicThreshold));
        // }
    }
    
    return false;
}

void Model::cleanup() {
    // Use reference to avoid copying Mesh objects (copy constructor is deleted for RAII safety)
    // Note: cleanup() is non-const, so we use non-const reference
    for (Mesh& mesh : meshes) {
        mesh.cleanup();
    }
}

//-- clear model (remove all meshes and reset state) ----
void Model::clear() {
    // Clean up existing meshes
    cleanup();
    
    // Clear all meshes
    meshes.clear();
    
    // Clear bone entries and bone-related data
    m_Entries.clear();
    m_BoneInfo.clear();
    m_BoneMapping.clear();
    m_BoneExtraRotations.clear();
    m_BoneExtraTranslations.clear();
    m_BoneExtraScales.clear();
    m_BoneGlobalTransforms.clear();
    m_isBoneSelected.clear();
    m_NodeToBoneIndexCache.clear();
    m_linearSkeleton.clear();
    m_NodeToAnimChannelCache.clear();
    m_BoneNameToLinearIndexCache.clear();
    
    // Reset scene pointer
    mScene = NULL;
    
    // Reset other state
    mNumBones = 0;
    directory = "";
    file_extension = "";
    textures_loaded.clear();
    mFPS = 0;
    
    LOG_INFO("[Model] Cleared - " + std::to_string(meshes.size()) + " meshes remaining (should be 0)");
}


//-- load model from path ----------------
void Model::loadModel(std::string path) {
    //-- use ASSIMP to read file ----------
    //-- triangulate = group indices in triangles, flip = flip textures on load --------------------
    // Import flags - ensure bone weights are preserved
    // NOTE: aiProcess_JoinIdenticalVertices can cause issues with bone weights in FBX files
    // Try without it first, or use aiProcess_OptimizeMeshes instead
    unsigned int flags = aiProcess_Triangulate | aiProcess_FlipUVs |
        aiProcess_LimitBoneWeights |  // This limits to 4 bones per vertex, but preserves weights
        aiProcess_SortByPType | aiProcess_GenSmoothNormals;
        // Removed: aiProcess_JoinIdenticalVertices - can strip bone weights in FBX
        // Removed: aiProcess_ValidateDataStructure - can be too strict
        // Removed: aiProcess_ImproveCacheLocality - can reorder vertices and break bone indices
    
    mScene = mIporter.ReadFile(path, flags);
    
    // Debug: Check if bone weights are present in the scene
    if (mScene) {
        LOG_INFO("[loadModel] Scene loaded. Checking for bone weights...");
        int meshesWithBones = 0;
        int totalBonesWithWeights = 0;
        std::map<std::string, int> boneWeightMap; // Track which bones have weights in which meshes
        
        for (unsigned int i = 0; i < mScene->mNumMeshes; i++) {
            const aiMesh* mesh = mScene->mMeshes[i];
            if (mesh->HasBones()) {
                meshesWithBones++;
                LOG_INFO("[loadModel] Mesh " + std::to_string(i) + " (" + std::string(mesh->mName.C_Str()) + ") has " + std::to_string(mesh->mNumBones) + " bones");
                for (unsigned int j = 0; j < mesh->mNumBones; j++) {
                    const aiBone* bone = mesh->mBones[j];
                    std::string boneName = bone->mName.C_Str();
                    if (bone->mNumWeights > 0) {
                        totalBonesWithWeights++;
                        boneWeightMap[boneName] = i; // Remember which mesh has weights for this bone
                        if (totalBonesWithWeights <= 5) { // Print first 5
                            LOG_INFO("[loadModel] Bone '" + boneName + "' in mesh " + std::to_string(i) + " has " + std::to_string(bone->mNumWeights) + " weights");
                        }
                    }
                }
            }
        }
        LOG_INFO("[loadModel] Found " + std::to_string(meshesWithBones) + " meshes with bones, " 
                 + std::to_string(totalBonesWithWeights) + " bones have weight data");
        
        // Store which meshes have bone weights for later reference
        if (totalBonesWithWeights > 0) {
            std::string boneList = "[loadModel] Bones with weights are in meshes: ";
            int count = 0;
            for (auto& pair : boneWeightMap) {
                if (count++ < 20) { // Limit output
                    boneList += pair.first + "(mesh " + std::to_string(pair.second) + ") ";
                }
            }
            if (boneWeightMap.size() > 20) {
                boneList += "... (and " + std::to_string(boneWeightMap.size() - 20) + " more)";
            }
            LOG_INFO(boneList);
        }
    }

    if (mScene) {


        //-- parse directory from path using std::filesystem for cross-platform compatibility --------
        // Use std::filesystem::path to get absolute parent directory (works with both / and \)
        // This ensures directory always points to where the FBX is located on disk, even if loaded from Recent Files
        std::filesystem::path filePath(path);
        std::filesystem::path absolutePath = std::filesystem::absolute(filePath);
        std::filesystem::path parentDir = absolutePath.parent_path();
        directory = parentDir.string();
        
        // Convert backslashes to forward slashes for consistency (matches Recent Files format)
        std::replace(directory.begin(), directory.end(), '\\', '/');

        //-- parse file extension from path --------
        size_t lastDot  = path.find_last_of(".");
        file_extension = path.substr(lastDot);

         //-- call processRig  to check if rig exsist if exsist get all the rig data -------------------
        processRig(mScene);


        //-- process root node --------------
        processNode(mScene->mRootNode, mScene);

        //-- get root Transformation ----------------------------------------
        m_GlobalInverseTransform = mScene->mRootNode->mTransformation;
        m_GlobalInverseTransform.Inverse();
    }
    //-- if no errors -------
    else if (!mScene || mScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !mScene->mRootNode) {
        LOG_ERROR("Could not load model at " + path + " - " + std::string(mIporter.GetErrorString()));
        return;
    }

}

// Load model with deferred GPU resource creation (for async loading)
// 
// CPU PHASE (Background Thread): This function runs ENTIRELY in a background thread.
// All CPU-bound operations are performed here:
//   - File I/O (Assimp::Importer::ReadFile)
//   - Scene parsing and mesh extraction (processNode, processMesh)
//   - Vertex/index/bone weight extraction from aiMesh structures
//   - Skeletal hierarchy processing (processRig, buildNodeToBoneCache, flattenHierarchy)
//   - Image decoding (stbi_load) for textures
//   - Bounding box calculation (iterating over vertices)
//   - Population of std::vector<Mesh> and std::vector<Texture>
// 
// CRITICAL: This function MUST NOT call any OpenGL functions (gl*)
// All OpenGL resource creation (VAOs, VBOs, textures) is deferred to createGPUResources()
// which runs on the main thread with a valid OpenGL context.
//
// Execution flow:
// 1. Background thread: loadModelAsync() -> loadModel() -> processRig() -> processNode() -> processMesh()
//    - processRig(): Builds bone mapping, caches, and flattens hierarchy (CPU-bound)
//    - processNode(): Recursively processes aiNode structures (CPU-bound)
//    - processMesh(): Extracts vertices, indices, bone weights from aiMesh (CPU-bound)
//    - processMesh() creates Mesh objects with deferSetup=true (NO glGenVertexArrays, glGenBuffers)
//    - processMesh() calls loadTextures() which creates Texture objects with deferredSetup=true (NO glGenTextures)
//    - loadTextures() calls decodeImageData() which uses stbi_load (CPU-bound, NO OpenGL calls)
// 2. Main thread: createGPUResources() is called after background thread completes
//    - Creates all OpenGL resources (VAOs, VBOs, textures) on main thread
//    - Uses pre-extracted vertex/index data and pre-decoded image data
void Model::loadModelAsync(std::string path) {
    // Set flag to defer GPU resource creation
    m_deferGPUResources = true;
    
    // Perform ALL CPU-bound loading (file I/O, Assimp parsing, mesh extraction, bone processing)
    // This will:
    //   - Parse the Assimp scene (mIporter.ReadFile)
    //   - Process rig and build skeletal caches (processRig, buildNodeToBoneCache, flattenHierarchy)
    //   - Extract all mesh data (processNode, processMesh) - vertices, indices, bone weights
    //   - Decode all texture images (stbi_load)
    //   - Populate meshes and textures vectors
    // BUT will NOT create any OpenGL resources (deferred until GPU phase)
    loadModel(path);
    
    // Note: GPU resources will be created later by calling createGPUResources() on main thread
}

// Create GPU resources for meshes (must be called on main thread with OpenGL context)
// 
// GPU PHASE (Main Thread): This function runs EXCLUSIVELY on the main thread.
// It performs ONLY OpenGL resource creation using pre-processed data from the CPU phase.
// 
// CRITICAL: This function MUST NOT perform any CPU-bound operations:
//   - NO iteration over aiNode or aiMesh structures
//   - NO std::vector population (vertices/indices already populated in background thread)
//   - NO image decoding (stbi_load) - uses pre-decoded image data
//   - NO bounding box calculation (uses pre-calculated values)
//   - NO skeletal hierarchy traversal (already processed in background thread)
// 
// This function ONLY:
//   - Creates OpenGL resources (glGenVertexArrays, glGenBuffers, glGenTextures)
//   - Uploads pre-extracted vertex/index data to GPU (glBufferData)
//   - Uploads pre-decoded image data to GPU (glTexImage2D)
//   - Sets up vertex attribute pointers (glVertexAttribPointer)
//   - Links texture IDs to mesh objects (simple pointer/ID copying)
//
// Call this after loadModelAsync() completes in the background thread.
void Model::createGPUResources() {
    // First, create GPU resources for all textures that were deferred
    // Iterate through textures_loaded and upload any textures that have pre-decoded data
    for (Texture& tex : textures_loaded) {
        if (tex.id == 0) {
            // Texture was created with deferred setup, now create OpenGL resources
            tex.generate();
            
            // Check if texture has pre-decoded image data (from background thread)
            if (tex.preDecodedData.isValid()) {
                // Fast GPU upload using pre-decoded data (no CPU-intensive decoding)
                if (!tex.uploadToGPU(tex.preDecodedData)) {
                    LOG_ERROR("[Model] Failed to upload texture to GPU: " + tex.path);
                    // If upload failed, delete the invalid texture ID
                    if (tex.id > 0) {
                        glDeleteTextures(1, &tex.id);
                        tex.id = 0;
                    }
                    // Note: preDecodedData will be automatically freed by shared_ptr when it goes out of scope
                }
            } else {
                // No pre-decoded data (texture decode failed in background thread)
                // Try loading now as fallback (should rarely happen)
                LOG_WARNING("[Model] Texture has no pre-decoded data, attempting fallback load: " + tex.path);
                if (!tex.load(false)) {
                    LOG_ERROR("[Model] Failed to load texture: " + tex.path);
                    // If loading failed, delete the invalid texture ID
                    if (tex.id > 0) {
                        glDeleteTextures(1, &tex.id);
                        tex.id = 0;
                    }
                }
            }
        }
    }
    
    // Link texture IDs to mesh objects (fast pointer/ID copying, no CPU processing)
    // This is a lightweight operation that just copies OpenGL texture IDs from textures_loaded
    // to the mesh texture references. All texture data was already decoded in the background thread.
    for (Mesh& mesh : meshes) {
        for (Texture& meshTex : mesh.textures) {
            if (meshTex.id == 0) {
                // Find matching texture in textures_loaded (simple string comparison)
                bool found = false;
                for (const Texture& loadedTex : textures_loaded) {
                    if (loadedTex.path == meshTex.path && loadedTex.dir == meshTex.dir && loadedTex.id > 0) {
                        // Copy only the OpenGL texture ID (fast, no data copying)
                        meshTex.id = loadedTex.id;
                        found = true;
                        break;
                    }
                }
                // Warn if texture wasn't found (shouldn't happen in normal flow)
                if (!found) {
                    LOG_WARNING("[Model] Mesh texture not found in textures_loaded: " + meshTex.path + " (dir: " + meshTex.dir + ")");
                }
            }
        }
        // Create GPU resources for the mesh (VAOs, VBOs, etc.)
        // Uses pre-extracted vertex/index data from background thread
        mesh.createGPUResources();
    }
    
    // Reset flag
    m_deferGPUResources = false;
}

// Time-sliced incremental GPU upload (uploads one mesh per call)
// Returns true when all meshes and textures are uploaded, false otherwise
// Must be called on main thread with valid OpenGL context
// Call this EXACTLY ONCE per frame until it returns true
bool Model::uploadNextMeshToGPU(int& meshesUploaded, int& texturesUploaded) {
    // First, upload textures incrementally (one texture per call until all are done)
    if (texturesUploaded < static_cast<int>(textures_loaded.size())) {
        // Upload next texture
        Texture& tex = textures_loaded[texturesUploaded];
        if (tex.id == 0) {
            // Texture was created with deferred setup, now create OpenGL resources
            tex.generate();
            
            // Check if texture has pre-decoded image data (from background thread)
            if (tex.preDecodedData.isValid()) {
                // Fast GPU upload using pre-decoded data (no CPU-intensive decoding)
                if (!tex.uploadToGPU(tex.preDecodedData)) {
                    LOG_ERROR("[Model] Failed to upload texture to GPU: " + tex.path);
                    // If upload failed, delete the invalid texture ID
                    if (tex.id > 0) {
                        glDeleteTextures(1, &tex.id);
                        tex.id = 0;
                    }
                    // Note: preDecodedData will be automatically freed by shared_ptr when it goes out of scope
                }
            } else {
                // No pre-decoded data (texture decode failed in background thread)
                // Try loading now as fallback (should rarely happen)
                LOG_WARNING("[Model] Texture has no pre-decoded data, attempting fallback load: " + tex.path);
                if (!tex.load(false)) {
                    LOG_ERROR("[Model] Failed to load texture: " + tex.path);
                    // If loading failed, delete the invalid texture ID
                    if (tex.id > 0) {
                        glDeleteTextures(1, &tex.id);
                        tex.id = 0;
                    }
                }
            }
        }
        texturesUploaded++;
        return false; // Not complete yet, more textures or meshes to upload
    }
    
    // All textures uploaded, now upload meshes incrementally (one mesh per call)
    if (meshesUploaded < static_cast<int>(meshes.size())) {
        // Link texture IDs to this mesh (fast pointer/ID copying, no CPU processing)
        Mesh& mesh = meshes[meshesUploaded];
        for (Texture& meshTex : mesh.textures) {
            if (meshTex.id == 0) {
                // Find matching texture in textures_loaded (simple string comparison)
                bool found = false;
                for (const Texture& loadedTex : textures_loaded) {
                    if (loadedTex.path == meshTex.path && loadedTex.dir == meshTex.dir && loadedTex.id > 0) {
                        // Copy only the OpenGL texture ID (fast, no data copying)
                        meshTex.id = loadedTex.id;
                        found = true;
                        break;
                    }
                }
                // Warn if texture wasn't found (shouldn't happen in normal flow)
                if (!found) {
                    LOG_WARNING("[Model] Mesh texture not found in textures_loaded: " + meshTex.path + " (dir: " + meshTex.dir + ")");
                }
            }
        }
        
        // Create GPU resources for this mesh (VAOs, VBOs, etc.)
        mesh.createGPUResources();
        meshesUploaded++;
        return false; // Not complete yet, more meshes to upload
    }
    
    // All textures and meshes uploaded
    m_deferGPUResources = false;
    return true; // Complete!
}

//-- process node in object file ----------------------------
// CPU-BOUND: This function runs in the background thread during loadModelAsync().
// It recursively traverses the Assimp scene hierarchy and extracts mesh data.
// NO OpenGL calls are made here - all GPU resource creation is deferred.
void Model::processNode(aiNode* node, const aiScene* mScene) {

    m_Entries.resize(mScene->mNumMeshes);

    //-- process all meshes ------------------
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        unsigned int meshIndex = node->mMeshes[i];
        aiMesh* mesh = mScene->mMeshes[meshIndex];
        LOG_INFO("[processNode] Processing mesh index " + std::to_string(meshIndex) + " from node '" + std::string(node->mName.C_Str()) + "'");
        // Use emplace_back with move to efficiently transfer Mesh ownership (RAII-safe)
        meshes.emplace_back(std::move(processMesh(mesh, mScene)));
    }

    //-- process all child nodes -----------------
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], mScene);
    }
}


//-- process mesh in assimp file --------------------------
// CPU-BOUND: This function runs in the background thread during loadModelAsync().
// It extracts vertex data, indices, bone weights, and textures from aiMesh structures.
// All std::vector population happens here (vertices, indices, textures).
// NO OpenGL calls are made here - Mesh is created with deferSetup=true.
Mesh Model::processMesh(const aiMesh* mesh, const aiScene* mScene) {

    LOG_INFO("[processMesh] Processing mesh: '" + std::string(mesh->mName.C_Str()) + "'");
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;


    //-- vertices ---------------------
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {

        Vertex vertex;
        //-- position ---------------------
        vertex.pos = glm::vec3(
            mesh->mVertices[i].x,
            mesh->mVertices[i].y,
            mesh->mVertices[i].z
        );

        //-- normal vectors --------------
        vertex.normal = glm::vec3(
            mesh->mNormals[i].x,
            mesh->mNormals[i].y,
            mesh->mNormals[i].z
        );

        //-- Textures ---------------------------
        if (mesh->mTextureCoords[0]) {
            vertex.texCoord = glm::vec2(
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y
            );
        }
        else {
            vertex.texCoord = glm::vec2(0.0f);
        }

        //-- Rig data ------------------------------
        // Initialize to no bones (will be set by LoadBones if vertex has bones)
        vertex.boneIds = glm::vec4(0.0f);
        vertex.boneWeights = glm::vec4(0.0f);

        vertices.push_back(vertex);

    }

    //-- process indices ------------------------
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }

    //== process material ==========================================
    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* material = mScene->mMaterials[mesh->mMaterialIndex];
        /*
        if (noTex) {
            //-- use Material -------------------
            //-- diffuse color ---
            aiColor4D  diff(1.0f);
            aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &diff);

            //-- specular color ---------------
            aiColor4D  spec(1.0f);
            aiGetMaterialColor(material, AI_MATKEY_COLOR_SPECULAR, &spec);

            return Mesh(vertices, indices, diff, spec, m_deferGPUResources);
        }
        */
        //-- use Textures -----------
        //-- 1. diffuse maps -------------
        std::vector<Texture> diffuseMaps = loadTextures(material, aiTextureType_DIFFUSE);
        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

        //-- 2. specular maps ------------
        std::vector<Texture> specularMaps = loadTextures(material, aiTextureType_SPECULAR);
        textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());

    }


    std::string mashName = mesh->mName.C_Str();
    
    //== load boneData to vertices ==================================
    if (mesh->HasBones()) {
        LOG_INFO("[processMesh] Mesh has " + std::to_string(mesh->mNumBones) + " bones, " + std::to_string(vertices.size()) + " vertices");
        uint nBoneCount = mesh->mNumBones;
        std::vector<uint> boneCounts;
        boneCounts.resize(vertices.size(), 0);
        
        int bonesSkipped = 0;
        int bonesProcessed = 0;
        int totalWeightsAssigned = 0;

        //-- loop through each bone -------------------------------------
        for (uint i = 0; i < nBoneCount; i++) {
            aiBone* bone = mesh->mBones[i];
            glm::mat4 m = assimpToGlmMatrix(bone->mOffsetMatrix);
            std::string boneName = bone->mName.C_Str();
            
            // Check if bone exists in mapping
            if (m_BoneMapping.find(boneName) == m_BoneMapping.end()) {
                LOG_WARNING("[processMesh] Bone '" + boneName + "' not found in m_BoneMapping!");
                if (bonesSkipped == 0) { // Only print available bones once
                    std::string availableBones = "[processMesh] Available bones in mapping (" + std::to_string(m_BoneMapping.size()) + " total): ";
                    int count = 0;
                    for (auto& pair : m_BoneMapping) {
                        if (count++ < 10) availableBones += "'" + pair.first + "' ";
                    }
                    if (m_BoneMapping.size() > 10) availableBones += "... (and " + std::to_string(m_BoneMapping.size() - 10) + " more)";
                    LOG_INFO(availableBones);
                }
                bonesSkipped++;
                continue; // Skip this bone
            }
            
            unsigned int boneIndex = m_BoneMapping[boneName];
            bonesProcessed++;

            //-- loop through each vertex that have that bone ------------------------
            // Check if mWeights is valid before accessing it
            if (bone->mWeights == nullptr) {
                // This bone is referenced by this mesh but has no weight data for THIS mesh's vertices
                // This is normal in FBX - bones can be referenced by multiple meshes but weights are per-mesh
                // Only print warning for first few to avoid spam
                if (bonesProcessed <= 3) {
                    LOG_INFO("[processMesh] Bone '" + boneName + "' referenced but has no weights for this mesh's vertices (normal in FBX)");
                }
                continue;
            }
            if (bone->mNumWeights == 0) {
                if (bonesProcessed <= 3) {
                    LOG_INFO("[processMesh] Bone '" + boneName + "' has 0 weights for this mesh");
                }
                continue;
            }
            
            // Debug: Print first bone's weight info
            if (bonesProcessed == 1 && i == 0) {
                LOG_INFO("[processMesh] First bone '" + boneName + "' has " + std::to_string(bone->mNumWeights) + " weights");
                if (bone->mNumWeights > 0) {
                    LOG_INFO("[processMesh] First weight: vertex ID=" + std::to_string(bone->mWeights[0].mVertexId) 
                             + ", weight=" + std::to_string(bone->mWeights[0].mWeight) + ", vertices.size()=" + std::to_string(vertices.size()));
                }
            }
            
            for (int j = 0; j < bone->mNumWeights; j++) {
                uint id = bone->mWeights[j].mVertexId;
                
                // Check if vertex ID is valid
                if (id >= vertices.size()) {
                    if (j < 3) { // Only print first few to avoid spam
                        LOG_ERROR("[processMesh] Vertex ID " + std::to_string(id) + " out of range (max: " + std::to_string(vertices.size()) + ") for bone '" + boneName + "'");
                    }
                    continue;
                }
                
                float weight = bone->mWeights[j].mWeight;
                if (weight <= 0.0f) {
                    continue; // Skip zero weights
                }
                
                boneCounts[id]++;
                totalWeightsAssigned++;

                    switch (boneCounts[id]) {
                    case 1:
                        vertices[id].boneIds.x = boneIndex;
                        vertices[id].boneWeights.x = weight;
                        break;
                    case 2:
                        vertices[id].boneIds.y = boneIndex;
                        vertices[id].boneWeights.y = weight;
                    break;
                case 3:
                    vertices[id].boneIds.z = boneIndex;
                    vertices[id].boneWeights.z = weight;
                    break;
                case 4:
                    vertices[id].boneIds.w = boneIndex;
                    vertices[id].boneWeights.w = weight;
                    break;

                default:
                    // Bone allocation failed - vertex already has 4 bones assigned (max limit)
                    break;
                }
                }
            }

        //-- normalize weights to make all weights sum 1 --------------------
        int verticesWithBones = 0;
        int verticesWithoutBones = 0;
        for (int i = 0; i < vertices.size(); i++) {
            glm::vec4& boneWeights = vertices[i].boneWeights;
            float totalWeight = boneWeights.x + boneWeights.y + boneWeights.z + boneWeights.w;
            if (totalWeight > 0.0f) {
                vertices[i].boneWeights = glm::vec4(
                    boneWeights.x / totalWeight,
                    boneWeights.y / totalWeight,
                    boneWeights.z / totalWeight,
                    boneWeights.w / totalWeight
                );
                verticesWithBones++;
            } else {
                verticesWithoutBones++;
            }
        }
        
        // Debug: Check if vertices have bone data
        LOG_INFO("[processMesh] Summary: " + std::to_string(bonesProcessed) + " bones processed, " + std::to_string(bonesSkipped) + " bones skipped, " 
                 + std::to_string(totalWeightsAssigned) + " weights assigned");
        
        if (verticesWithBones > 0) {
            LOG_INFO("[processMesh] " + std::to_string(verticesWithBones) + " vertices have bone weights, " 
                     + std::to_string(verticesWithoutBones) + " vertices have no bones");
            // Check a few sample vertices
            for (int i = 0; i < std::min(5, (int)vertices.size()); i++) {
                if (vertices[i].boneWeights.x > 0.0f || vertices[i].boneWeights.y > 0.0f) {
                    std::string vertexInfo = "[processMesh] Vertex " + std::to_string(i) + " - Bone IDs: [" 
                                            + std::to_string(vertices[i].boneIds.x) + ", " + std::to_string(vertices[i].boneIds.y) + ", " 
                                            + std::to_string(vertices[i].boneIds.z) + ", " + std::to_string(vertices[i].boneIds.w) + "], Weights: [" 
                                            + std::to_string(vertices[i].boneWeights.x) + ", " + std::to_string(vertices[i].boneWeights.y) + ", " 
                                            + std::to_string(vertices[i].boneWeights.z) + ", " + std::to_string(vertices[i].boneWeights.w) + "]";
                    LOG_INFO(vertexInfo);
                    break; // Only print first one
                }
            }
        } else {
            LOG_WARNING("[processMesh] No vertices have bone weights! This mesh won't animate!");
            if (bonesSkipped > 0) {
                LOG_INFO("[processMesh] This is likely because " + std::to_string(bonesSkipped) + " bones were skipped (not found in mapping)");
            }
            if (totalWeightsAssigned == 0 && bonesProcessed > 0) {
                LOG_WARNING("[processMesh] Bones were found but no weights were assigned - check vertex IDs!");
            }
        }
    }

    // Use deferred setup if in async loading mode (GPU resources created later on main thread)
    return Mesh(vertices, indices, textures, m_deferGPUResources);
}

//-- load list of textures (for each mesh) ------------------------------
std::vector<Texture> Model::loadTextures(aiMaterial* mat, aiTextureType type) {
    std::vector<Texture> textures;

    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
        aiString str;
        mat->GetTexture(type, i, &str);
        LOG_INFO("Texture: " + std::string(str.C_Str()));

        //-- prevent duplicate loading -------------
        bool skip = false;
        for (unsigned int j = 0; j < textures_loaded.size(); j++) {
            if (std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0) {
                if (m_deferGPUResources) {
                    // In deferred mode, reuse texture even if id == 0 (not loaded yet)
                    // This prevents duplicate texture entries during async loading
                    textures.push_back(textures_loaded[j]);
                    skip = true;
                    break;
                } else {
                    // In immediate mode, only reuse texture if it has a valid ID (successfully loaded)
                    if (textures_loaded[j].id > 0) {
                        textures.push_back(textures_loaded[j]);
                        skip = true;
                        break;
                    }
                    // If texture failed to load before, try loading it again
                }
            }
        }

        if (!skip) {
            //-- not loaded yet ------------------
            // CRITICAL: When m_deferGPUResources is true, this constructor will NOT call generate()
            // and we will NOT call load(), ensuring ZERO OpenGL calls in background thread
            Texture tex(directory, str.C_Str(), type, m_deferGPUResources);
            
            if (m_deferGPUResources) {
                // Deferred mode: Decode image data in background thread (CPU-bound, no OpenGL calls)
                // This moves the heavy stbi_load operation off the main thread
                PreDecodedImageData preDecodedData;
                if (tex.decodeImageData(preDecodedData, false)) {
                    // Store pre-decoded data in texture object using move semantics
                    tex.preDecodedData = std::move(preDecodedData);
                    textures.push_back(tex);
                    textures_loaded.push_back(tex);
                } else {
                    // Image decode failed, but still add texture (will fail gracefully in GPU phase)
                    textures.push_back(tex);
                    textures_loaded.push_back(tex);
                }
            } else {
                // Immediate mode: Load texture now (OpenGL calls happen here on main thread)
                // Only add texture if loading succeeded (ID will be > 0)
                if (tex.load(false) && tex.id > 0) {
                    textures.push_back(tex);
                    textures_loaded.push_back(tex);
                }
            }
        }
    }

    return textures;
}

//-- get File Extension -----------------------
std::string Model::getFileExtension()
{
    return file_extension;
}

//====================================================================================================
//-- RIG & ANIMATION STUFF --------------------------------------------------------------------------
// 
//-- processRig --------------------------------------------------------------------------------------
// CPU-BOUND: This function runs in the background thread during loadModelAsync().
// It processes the skeletal hierarchy, builds bone mappings, caches, and flattens the hierarchy.
// All CPU-intensive operations (buildNodeToBoneCache, flattenHierarchy) happen here.
// NO OpenGL calls are made here.
void Model::processRig(const aiScene* mScene)
{
    LOG_INFO("** processRig ***");

    m_Entries.resize(mScene->mNumMeshes);
    //-- Prepare vectors for vertex attributes and indices ---
    std::vector<VertexBoneData> bones;
    unsigned int NumVertices = 0;

    for (unsigned int i = 0; i < m_Entries.size(); i++) {

        //--- Increment total vertices and indices. 
        NumVertices += mScene->mMeshes[i]->mNumVertices;
    }

    //-- Reserve space in the vectors for the vertex attributes and indices -----
    bones.resize(NumVertices);

    // Total vertices processed (debug info removed for professional build)
    for (unsigned int i = 0; i < m_Entries.size(); i++) {

        //--  Load the mesh's bones -------
        const aiMesh* mesh = mScene->mMeshes[i];
        // Mesh name processing (debug info removed for professional build)
        if (mesh->HasBones()) {

            LOG_INFO("** HasBones **");
            LoadBones(i, mesh, bones);
        }
    }

    if (mScene->HasAnimations())
    {
        //-- get keys/time for each bone ----------------------------
        mFPS = mScene->mAnimations[0]->mTicksPerSecond;
        // Animation FPS loaded (debug info removed for professional build)
    }
    
    // Build node-to-bone index cache for fast lookups (eliminates string operations in hot path)
    m_NodeToBoneIndexCache.clear();
    if (mScene->mRootNode != nullptr) {
        buildNodeToBoneCache(mScene->mRootNode);
    }
    
    // Build animation channel cache (eliminates string lookups in hot path)
    m_NodeToAnimChannelCache.clear();
    if (mScene->HasAnimations() && mScene->mNumAnimations > 0) {
        const aiAnimation* pAnimation = mScene->mAnimations[0];
        if (pAnimation != NULL) {
            // Cache animation channels by node name during load time
            for (unsigned int i = 0; i < pAnimation->mNumChannels; i++) {
                const aiNodeAnim* pNodeAnim = pAnimation->mChannels[i];
                std::string animNodeName(pNodeAnim->mNodeName.data);
                
                // Find the node in the scene hierarchy and cache it
                // We'll populate this during flattenHierarchy
            }
        }
    }
    
    // Flatten hierarchy into linear structure (preparation for linear loop optimization)
    // This must be called AFTER m_BoneMapping is fully initialized (after all LoadBones calls)
    m_linearSkeleton.clear();
    if (mScene->mRootNode != nullptr) {
        flattenHierarchy(mScene->mRootNode, -1);
    }
    
    // Populate animation channel cache after skeleton is flattened
    if (mScene->HasAnimations() && mScene->mNumAnimations > 0) {
        const aiAnimation* pAnimation = mScene->mAnimations[0];
        if (pAnimation != NULL) {
            for (const auto& linearBone : m_linearSkeleton) {
                if (linearBone.pNode != nullptr) {
                    std::string nodeName(linearBone.pNode->mName.data);
                    for (unsigned int i = 0; i < pAnimation->mNumChannels; i++) {
                        const aiNodeAnim* pNodeAnim = pAnimation->mChannels[i];
                        if (std::string(pNodeAnim->mNodeName.data) == nodeName) {
                            m_NodeToAnimChannelCache[linearBone.pNode] = pNodeAnim;
                            break;
                        }
                    }
                }
            }
        }
    }
    
    // Populate bone name to linear index cache (for getBoneIndexFromName optimization)
    m_BoneNameToLinearIndexCache.clear();
    for (size_t i = 0; i < m_linearSkeleton.size(); i++) {
        const LinearBone& linearBone = m_linearSkeleton[i];
        if (linearBone.boneIndex >= 0 && linearBone.pNode != nullptr) {
            std::string boneName(linearBone.pNode->mName.data);
            m_BoneNameToLinearIndexCache[boneName] = i;
        }
    }
    
    // CRITICAL: Ensure ALL vectors are resized to exactly mNumBones after LoadBones completes
    // This prevents out-of-bounds access in updateLinearSkeleton
    // LoadBones may have resized them incrementally, but we need to ensure they're exactly mNumBones
    if (mNumBones > 0) {
        // Resize vectors to exactly mNumBones
        size_t currentRotSize = m_BoneExtraRotations.size();
        size_t currentTransSize = m_BoneExtraTranslations.size();
        size_t currentScaleSize = m_BoneExtraScales.size();
        
        m_BoneExtraRotations.resize(mNumBones);
        m_BoneExtraTranslations.resize(mNumBones);
        m_BoneExtraScales.resize(mNumBones);
        m_BoneGlobalTransforms.resize(mNumBones);
        m_isBoneSelected.resize(mNumBones, false);
        
        // Initialize newly added elements to identity matrices
        // LoadBones may have already initialized some, but we ensure all are identity
        Matrix4f identity;
        identity.InitIdentity();
        for (size_t i = currentRotSize; i < mNumBones; i++) {
            m_BoneExtraRotations[i] = identity;
        }
        for (size_t i = currentTransSize; i < mNumBones; i++) {
            m_BoneExtraTranslations[i] = identity;
        }
        for (size_t i = currentScaleSize; i < mNumBones; i++) {
            m_BoneExtraScales[i] = identity;
        }
        
        LOG_INFO("[Model] Resized all bone transform vectors to mNumBones: " + std::to_string(mNumBones));
    } else {
        // No bones - clear all vectors
        m_BoneExtraRotations.clear();
        m_BoneExtraTranslations.clear();
        m_BoneExtraScales.clear();
        m_BoneGlobalTransforms.clear();
        m_isBoneSelected.clear();
        LOG_WARNING("[Model] No bones found, cleared all transform vectors");
    }
    
    // Reset dirty flags
    m_transformsDirty = true;
    m_lastAnimationTime = -1.0f;
}

//-- load all bones in the scene ------------------------------------------- 
void Model::LoadBones(unsigned int meshIndex, const aiMesh* mesh, std::vector<VertexBoneData>& bones)
{
    // Bone loading complete (debug info removed for professional build)
   
    //-- Loop through all bones in the Assimp mesh ---
    for (unsigned int i = 0; i < mesh->mNumBones; i++) {

        unsigned int BoneIndex = 0;

        //-- Obtain the bone name ---
        std::string BoneName(mesh->mBones[i]->mName.data);

        //-- If bone isn't already in the map ---
        if (m_BoneMapping.find(BoneName) == m_BoneMapping.end()) {

            //-- Set the bone ID to be the current total number of bones -----
            BoneIndex = mNumBones;

            //-- Increment total bones ---
            mNumBones++;

            //-- Push new bone info into bones vector ----
            BoneInfo bi;
            m_BoneInfo.push_back(bi);
            
        }
        else {

            //-- Bone ID is already in map ----
            BoneIndex = m_BoneMapping[BoneName];
        }

        m_BoneMapping[BoneName] = BoneIndex;

        //-- Obtains the offset matrix which transforms the bone from mesh space into bone space ----
        // For Assimp: Use the offset matrix directly (works for most files)
        m_BoneInfo[BoneIndex].BoneOffset = mesh->mBones[i]->mOffsetMatrix;
        //m_BoneInfo[BoneIndex].BoneName = mesh->mBones[i]->mName.C_Str();
        // Initialize with identity matrix instead of zero matrix
        // Zero matrices would break transformations when applied
        // Ensure vectors are large enough for this bone index
        if (m_BoneExtraRotations.size() <= BoneIndex) {
            m_BoneExtraRotations.resize(BoneIndex + 1);
            m_BoneExtraTranslations.resize(BoneIndex + 1);
            m_BoneExtraScales.resize(BoneIndex + 1);
            m_BoneGlobalTransforms.resize(BoneIndex + 1);
            m_isBoneSelected.resize(BoneIndex + 1, false);
        }
        
        Matrix4f addRotation;
        addRotation.InitIdentity();
        m_BoneExtraRotations[BoneIndex] = addRotation;
        
        // Initialize translations and scales with identity matrices as well
        Matrix4f addTranslation;
        addTranslation.InitIdentity();
        m_BoneExtraTranslations[BoneIndex] = addTranslation;
        
        Matrix4f addScale;
        addScale.InitIdentity();
        m_BoneExtraScales[BoneIndex] = addScale;
        
       
        //-- Iterate over all the affected vertices by this bone i.e weights ----
        // Check if mWeights is valid before accessing it
        if (mesh->mBones[i]->mWeights != nullptr && mesh->mBones[i]->mNumWeights > 0) {
            for (unsigned int j = 0; j < mesh->mBones[i]->mNumWeights; j++) {

                //-- Obtain an index to the affected vertex within the array of vertices -----
                unsigned int VertexID = m_Entries[meshIndex].BaseVertex + mesh->mBones[i]->mWeights[j].mVertexId;

                //-- The value of how much this bone influences the vertex ------
                float Weight = mesh->mBones[i]->mWeights[j].mWeight;

                //-- Insert bone data for particular vertex ID. A maximum of 4 bones can influence the same vertex ------
                bones[VertexID].AddBoneData(BoneIndex, Weight);

            }
        }
    }
}

//-- Process OpenFBX SKIN/CLUSTER objects to calculate bone offset matrices --
// This function processes FBX Skins and Clusters to calculate proper bone offsets
// using the formula: Offset = Inverse(LinkMatrix) * TransformMatrix
// This is more robust for non-Maya FBX files than using Assimp's mOffsetMatrix directly
void Model::processOpenFBXBones(const ofbx::IScene* fbxScene) {
    if (!fbxScene) return;
    
    // Iterate through all objects in the scene to find SKIN objects
    const ofbx::Object* root = fbxScene->getRoot();
    if (!root) return;
    
    // Collect all SKIN objects by traversing the hierarchy
    std::vector<const ofbx::Skin*> skins;
    processOpenFBXObjectsRecursive(root, skins);
    
    // Process each SKIN object
    for (const ofbx::Skin* skin : skins) {
        if (!skin) continue;
        
        // Get all clusters (bones) from this skin
        int clusterCount = skin->getClusterCount();
        for (int j = 0; j < clusterCount; ++j) {
            const ofbx::Cluster* cluster = skin->getCluster(j);
            if (!cluster) continue;
            
            // Get the bone (link) associated with this cluster
            const ofbx::Object* link = cluster->getLink();
            if (!link) continue;
            
            // Get bone name
            std::string boneName = link->name;
            
            // Find or create bone index
            unsigned int boneIndex = 0;
            if (m_BoneMapping.find(boneName) == m_BoneMapping.end()) {
                boneIndex = mNumBones;
                mNumBones++;
                BoneInfo bi;
                m_BoneInfo.push_back(bi);
                m_BoneMapping[boneName] = boneIndex;
            } else {
                boneIndex = m_BoneMapping[boneName];
            }
            
            // Get the bind pose matrices from the cluster
            // Note: getTransformLinkMatrix() is the link matrix, getTransformMatrix() is the transform matrix
            ofbx::Matrix linkMatrix = cluster->getTransformLinkMatrix();
            ofbx::Matrix transformMatrix = cluster->getTransformMatrix();
            
            // Convert and calculate the proper offset
            glm::mat4 glmLink = ofbxToGlm(linkMatrix);
            glm::mat4 glmTransform = ofbxToGlm(transformMatrix);
            
            // The offset matrix transforms vertices from mesh space to bone local space
            // Formula: Offset = Inverse(LinkMatrix) * TransformMatrix
            glm::mat4 offsetGlm = glm::inverse(glmLink) * glmTransform;
            
            // Convert back to Matrix4f for storage
            m_BoneInfo[boneIndex].BoneOffset = glmToMatrix4f(offsetGlm);
        }
    }
}

//-- get transform  for each bone ------------------------------------------- 
void Model::BoneTransform(float TimeInSeconds, std::vector<glm::mat4>& Transforms, UI_data ui_data)
{
    // Safety check: if scene is NULL or no bones, clear transforms and return
    if (mScene == NULL || mNumBones == 0) {
        Transforms.clear();
        return;
    }

    // DIRTY FLAG OPTIMIZATION: Skip updateLinearSkeleton if nothing changed
    bool needsUpdate = m_transformsDirty;
    
    // Check if animation time changed
    if (!needsUpdate) {
        bool hasAnimations = mScene->HasAnimations();
        if (hasAnimations) {
            float TimeInTicks = TimeInSeconds * mFPS;
            float AnimationTime = fmod(TimeInTicks, mScene->mAnimations[0]->mDuration);
            if (AnimationTime != m_lastAnimationTime) {
                needsUpdate = true;
            }
        }
    }
    
    // Check if UI data changed (selection or slider values)
    if (!needsUpdate) {
        if (ui_data.nodeName_toTrasform != m_lastUI_data.nodeName_toTrasform ||
            glm::length(ui_data.mSliderRotations - m_lastUI_data.mSliderRotations) > 0.001f ||
            glm::length(ui_data.mSliderTraslations - m_lastUI_data.mSliderTraslations) > 0.001f ||
            glm::length(ui_data.mSliderScales - m_lastUI_data.mSliderScales) > 0.001f) {
            needsUpdate = true;
        }
    }
    
    // Calculate animation time
    float AnimationTime = 0.0f;
    bool hasAnimations = mScene->HasAnimations();
    if (hasAnimations) {
        float TimeInTicks = TimeInSeconds * mFPS;
        AnimationTime = fmod(TimeInTicks, mScene->mAnimations[0]->mDuration);
    }
    
    // DIRTY FLAG OPTIMIZATION: Skip update if nothing changed
    if (!needsUpdate && AnimationTime == m_lastAnimationTime && !m_transformsDirty) {
        // Nothing changed - skip update (use cached transforms)
    } else {
        // Update linear skeleton (replaces recursive ReadNodeHierarchy)
        updateLinearSkeleton(AnimationTime, ui_data);
        
        // Cache animation time and UI data
        m_lastAnimationTime = AnimationTime;
        m_lastUI_data = ui_data;
        m_transformsDirty = false;
    }

    //-- Populates transforms vector with new bone transformation matrices -------------------
    // This is always done (even if updateLinearSkeleton was skipped) because m_BoneInfo is already updated
    Transforms.resize(mNumBones);
    
    // Safety check: ensure m_BoneInfo has enough elements
    unsigned int numBonesToProcess = (mNumBones < m_BoneInfo.size()) ? mNumBones : m_BoneInfo.size();
    for (unsigned int i = 0; i < numBonesToProcess; i++) {
        Matrix4f Trans = m_BoneInfo[i].FinalTransformation;
        Trans = Trans.Transpose();
        Transforms[i] = Trans.toGlmMatrix();
    }
    // Fill remaining with identity if needed
    for (unsigned int i = numBonesToProcess; i < mNumBones; i++) {
        Transforms[i] = glm::mat4(1.0f);
    }
    // Resize to match mNumBones (fill remaining with identity if needed)
    if (Transforms.size() < mNumBones) {
        Transforms.resize(mNumBones, glm::mat4(1.0f));
    }
}

//-- Linear skeleton update (replaces recursive ReadNodeHierarchy for performance) ----
// OPTIMIZED: ZERO string operations, ZERO map lookups, ZERO recursive calls
void Model::updateLinearSkeleton(float AnimationTime, UI_data ui_data)
{
    if (m_linearSkeleton.empty() || mScene == nullptr) {
        return;
    }
    
    // Get animation pointer (if available)
    const aiAnimation* pAnimation = nullptr;
    if (mScene->HasAnimations() && mScene->mNumAnimations > 0) {
        pAnimation = mScene->mAnimations[0];
    }
    
    // Identity matrix for root nodes
    Matrix4f Identity;
    Identity.InitIdentity();
    
    // Simple linear loop - iterate over flattened skeleton
    for (size_t i = 0; i < m_linearSkeleton.size(); i++) {
        LinearBone& bone = m_linearSkeleton[i];
        const aiNode* pNode = bone.pNode;
        
        if (pNode == nullptr) {
            continue;
        }
        
        // Get bone index (if this is a bone)
        int boneIndex = bone.boneIndex;
        
        // Check if this is RootNode (scene root)
        bool isRootNode = (pNode == mScene->mRootNode);
        
        // Get animation channel from cache (O(1) lookup, no string operations)
        const aiNodeAnim* pNodeAnim = nullptr;
        if (pAnimation != nullptr) {
            auto animIt = m_NodeToAnimChannelCache.find(pNode);
            if (animIt != m_NodeToAnimChannelCache.end()) {
                pNodeAnim = animIt->second;
            }
        }
        
        // Get parent's global matrix (or identity if root)
        // SAFETY CHECK: Validate parentIndex before accessing m_linearSkeleton
        Matrix4f parentGlobalMatrix = Identity;
        if (bone.parentIndex >= 0 && bone.parentIndex < static_cast<int>(m_linearSkeleton.size())) {
            parentGlobalMatrix = m_linearSkeleton[bone.parentIndex].globalMatrix;
        } else if (bone.parentIndex != -1) {
            // Invalid parent index - log error and use identity
            LOG_WARNING("[Model] Invalid parentIndex " + std::to_string(bone.parentIndex) 
                        + " for bone at index " + std::to_string(i) + " (skeleton size: " + std::to_string(m_linearSkeleton.size()) + ")");
        }
        
        // Start with node's default transformation
        Matrix4f localMatrix(pNode->mTransformation);
        
        // Fast index-based access to manual transforms (O(1) vector access, no strings)
        Matrix4f RotationM_attr;
        RotationM_attr.InitIdentity();
        Matrix4f TranslationM_attr;
        TranslationM_attr.InitIdentity();
        Matrix4f ScaleM_attr;
        ScaleM_attr.InitIdentity();
        
        // Load manual transforms from index-based vectors (no string lookups)
        // SAFETY CHECK: Validate boneIndex and ensure all vectors have the same size
        if (boneIndex >= 0) {
            // Check all three vectors have the same size and boneIndex is within bounds
            size_t maxSize = std::max({m_BoneExtraRotations.size(), m_BoneExtraTranslations.size(), m_BoneExtraScales.size()});
            if (boneIndex < static_cast<int>(maxSize)) {
                // Ensure vectors are large enough (resize if needed)
                if (boneIndex >= static_cast<int>(m_BoneExtraRotations.size())) {
                    m_BoneExtraRotations.resize(boneIndex + 1);
                }
                if (boneIndex >= static_cast<int>(m_BoneExtraTranslations.size())) {
                    m_BoneExtraTranslations.resize(boneIndex + 1);
                }
                if (boneIndex >= static_cast<int>(m_BoneExtraScales.size())) {
                    m_BoneExtraScales.resize(boneIndex + 1);
                }
                
                RotationM_attr = m_BoneExtraRotations[boneIndex];
                TranslationM_attr = m_BoneExtraTranslations[boneIndex];
                ScaleM_attr = m_BoneExtraScales[boneIndex];
            } else {
                // Bone index out of bounds - log error
                LOG_WARNING("[Model] Bone index " + std::to_string(boneIndex) 
                            + " out of bounds (max size: " + std::to_string(maxSize) + ")");
            }
        }
        
        // CRITICAL: Manual transforms are already loaded from index-based arrays above (no string operations)
        // PropertyPanel directly updates m_BoneExtraRotations/Translations/Scales arrays when sliders change
        // No need for changeAttributeManuale/changeTranslationManuale/changeScaleManuale calls here
        // The transforms are applied below when combining with animation matrices
        
        // Compute local transformation matrix
        if (!isRootNode) {
            if (pNodeAnim != nullptr) {
                // Has animation channel - interpolate and combine with manual transforms
                aiQuaternion RotationQ;
                CalcInterpolatedRotation(RotationQ, AnimationTime, pNodeAnim);
                Matrix4f RotationM = Matrix4f(RotationQ.GetMatrix());
                
                aiVector3D Translation;
                CalcInterpolatedTranslation(Translation, AnimationTime, pNodeAnim);
                Matrix4f TranslationM;
                TranslationM.InitTranslationTransform(Translation.x, Translation.y, Translation.z);
                
                aiVector3D Scaling;
                CalcInterpolatedScaling(Scaling, AnimationTime, pNodeAnim);
                Matrix4f ScalingM;
                ScalingM.InitScaleTransform(Scaling.x, Scaling.y, Scaling.z);
                
                // Combine: (Animation Translation + Manual Translation) * Rotation * Manual Rotation * Scale * Manual Scale
                Matrix4f CombinedTranslationM = TranslationM * TranslationM_attr;
                Matrix4f CombinedScalingM = ScalingM * ScaleM_attr;
                localMatrix = CombinedTranslationM * RotationM * RotationM_attr * CombinedScalingM;
            } else {
                // No animation channel - use default transform with manual transforms applied
                localMatrix = localMatrix * TranslationM_attr * RotationM_attr * ScaleM_attr;
            }
        } else {
            // RootNode: Always use identity (transform comes from model matrix only)
            localMatrix.InitIdentity();
        }
        
        // Compute global matrix: GlobalMatrix[i] = ParentGlobalMatrix * LocalMatrix
        Matrix4f globalMatrix = parentGlobalMatrix * localMatrix;
        
        // Store global matrix in LinearBone for children to use
        bone.globalMatrix = globalMatrix;
        
        // Extract translation directly from matrix (NO DECOMPOSE - performance critical)
        // Matrix4f stores data in m[row][column] format, translation is in column 3
        glm::vec3 worldPosition(globalMatrix.m[0][3], globalMatrix.m[1][3], globalMatrix.m[2][3]);
        
        // Update bone global transforms (for bounding box and gizmo positioning)
        // SAFETY CHECK: Validate boneIndex before accessing m_BoneGlobalTransforms
        if (boneIndex >= 0) {
            if (boneIndex < static_cast<int>(m_BoneGlobalTransforms.size())) {
                BoneGlobalTransform& globalTransform = m_BoneGlobalTransforms[boneIndex];
                globalTransform.translation = worldPosition;
                // Note: rotation and scale are not needed for bounding box, so we skip decompose
                // If needed later, they can be extracted from the matrix without full decompose
            } else {
                // Bone index out of bounds - log error
                LOG_WARNING("[Model] Bone index " + std::to_string(boneIndex) 
                            + " out of bounds for m_BoneGlobalTransforms (size: " + std::to_string(m_BoneGlobalTransforms.size()) + ")");
            }
        }
        
        // Update FinalTransformation for bone (used in shader)
        // SAFETY CHECK: Validate boneIndex before accessing m_BoneInfo
        if (boneIndex >= 0) {
            if (boneIndex < static_cast<int>(m_BoneInfo.size())) {
                m_BoneInfo[boneIndex].FinalTransformation = m_GlobalInverseTransform * globalMatrix * m_BoneInfo[boneIndex].BoneOffset;
            } else {
                // Bone index out of bounds - log error
                LOG_WARNING("[Model] Bone index " + std::to_string(boneIndex) 
                            + " out of bounds for m_BoneInfo (size: " + std::to_string(m_BoneInfo.size()) + ")");
            }
        }
    }
}


//-- Build node-to-bone index cache by traversing scene hierarchy ---------------------------------
// CPU-BOUND: Recursively builds node-to-bone index cache (runs in background thread)
// This eliminates string lookups in the hot path during animation updates.
void Model::buildNodeToBoneCache(const aiNode* pNode)
{
    if (pNode == nullptr) {
        return;
    }
    
    // Check if this node is a bone (exists in m_BoneMapping)
    std::string nodeName(pNode->mName.data);
    auto it = m_BoneMapping.find(nodeName);
    if (it != m_BoneMapping.end()) {
        // This node is a bone - cache the mapping
        m_NodeToBoneIndexCache[pNode] = static_cast<int>(it->second);
    } else {
        // Not a bone - mark as -1
        m_NodeToBoneIndexCache[pNode] = -1;
    }
    
    // Recursively process children
    for (unsigned int i = 0; i < pNode->mNumChildren; i++) {
        buildNodeToBoneCache(pNode->mChildren[i]);
    }
}

//-- Flatten hierarchy into linear structure (called during load time) ----------------------------
// CPU-BOUND: Recursively flattens the scene hierarchy into a linear structure (runs in background thread)
// This enables O(1) bone lookups and linear iteration during animation updates.
void Model::flattenHierarchy(const aiNode* pNode, int parentIdx)
{
    if (pNode == nullptr) {
        return;
    }
    
    // Get the current node's index (will be the size before we add it)
    int currentIndex = static_cast<int>(m_linearSkeleton.size());
    
    // Find bone index for this node (if it's a bone)
    int boneIndex = -1;
    std::string nodeName(pNode->mName.data);
    auto it = m_BoneMapping.find(nodeName);
    if (it != m_BoneMapping.end()) {
        boneIndex = static_cast<int>(it->second);
    }
    
    // Count children first to reserve space
    int numChildren = static_cast<int>(pNode->mNumChildren);
    
    // Add this node to the linear skeleton
    LinearBone linearBone;
    linearBone.pNode = pNode;
    linearBone.parentIndex = parentIdx;
    linearBone.boneIndex = boneIndex;
    linearBone.globalMatrix.InitIdentity();  // Initialize to identity (will be computed during update)
    linearBone.childIndices.reserve(numChildren);  // Reserve space for children
    
    // Identify root-like bones during load time (eliminates string operations in hot path)
    // Check node name once during load, not every frame
    linearBone.isRootBone = false;
    if (pNode != nullptr && pNode->mName.length > 0) {
        std::string nodeName(pNode->mName.data);
        // Convert to lowercase for comparison (done once during load)
        std::transform(nodeName.begin(), nodeName.end(), nodeName.begin(), ::tolower);
        // Check if name contains root-like keywords
        linearBone.isRootBone = (nodeName.find("rootnode") != std::string::npos || 
                                 nodeName.find("root") != std::string::npos || 
                                 nodeName.find("world") != std::string::npos);
    }
    
    m_linearSkeleton.push_back(linearBone);
    
    // Recursively process children, passing current index as their parent
    // Store child indices in parent's childIndices vector
    for (unsigned int i = 0; i < pNode->mNumChildren; i++) {
        int childIndex = static_cast<int>(m_linearSkeleton.size());  // Index child will have after being added
        flattenHierarchy(pNode->mChildren[i], currentIndex);
        // After child is added, store its index in parent's childIndices
        m_linearSkeleton[currentIndex].childIndices.push_back(childIndex);
    }
}


//-- set rotations from slider attributes ---------------------------------
void Model::changeAttributeManuale(Matrix4f& RotationM_attr, UI_data ui_data, std::string nodeNameInRig, int boneIndex)
{
    std::string nodeNameSelected = ui_data.nodeName_toTrasform;
    
    // If this is the currently selected bone, use slider value (real-time update)
    // This ensures the selected bone responds immediately to slider changes
    // IMPORTANT: Use exact match or safe suffix match to avoid "Spine" matching "Spine1"
    bool isSelectedBone = false;
    if (nodeNameSelected != "") {
        // Exact match
        if (nodeNameInRig == nodeNameSelected) {
            isSelectedBone = true;
        } else {
            // Safe suffix match: rig name ends with selected name
            // This handles cases like "1810483883696 Spine" or "1810483883696Spine" matching "Spine"
            // But prevents "Spine1" from matching "Spine"
            if (nodeNameInRig.length() >= nodeNameSelected.length()) {
                // Check if rig name ends with selected name
                if (nodeNameInRig.substr(nodeNameInRig.length() - nodeNameSelected.length()) == nodeNameSelected) {
                    // Selected name is at the end of rig name
                    // Check if there's a boundary before it (space, digit, or non-alphanumeric)
                    size_t pos = nodeNameInRig.length() - nodeNameSelected.length();
                    if (pos == 0) {
                        // At start of string - safe match
                        isSelectedBone = true;
                    } else {
                        // Check character before the match - must be space, digit, or non-alphanumeric
                        // This allows "1810483883696 Spine" and "1810483883696Spine" to match "Spine"
                        // But prevents "Spine1" from matching "Spine" (because '1' is alphanumeric and would be part of "Spine1")
                        char beforeChar = nodeNameInRig[pos - 1];
                        if (beforeChar == ' ' || std::isdigit(beforeChar) || !std::isalnum(beforeChar)) {
                            isSelectedBone = true;
                        }
                    }
                }
            }
            // Also check reverse: if selected name ends with rig name (for cases where PropertyPanel has prefix)
            if (!isSelectedBone && nodeNameSelected.length() >= nodeNameInRig.length()) {
                if (nodeNameSelected.substr(nodeNameSelected.length() - nodeNameInRig.length()) == nodeNameInRig) {
                    size_t pos = nodeNameSelected.length() - nodeNameInRig.length();
                    if (pos == 0) {
                        isSelectedBone = true;
                    } else {
                        char beforeChar = nodeNameSelected[pos - 1];
                        if (beforeChar == ' ' || std::isdigit(beforeChar) || !std::isalnum(beforeChar)) {
                            isSelectedBone = true;
                        }
                    }
                }
            }
        }
    }
    
    if (isSelectedBone) {
        RotationM_attr.InitRotateTransform(ui_data.mSliderRotations.x, ui_data.mSliderRotations.y, ui_data.mSliderRotations.z);
        // Save to index-based vector (fast access)
        if (boneIndex >= 0 && boneIndex < static_cast<int>(m_BoneExtraRotations.size())) {
            m_BoneExtraRotations[boneIndex] = RotationM_attr;
        }
    }
    // If not selected bone, RotationM_attr was already loaded from index-based vector in updateLinearSkeleton
    // No additional lookup needed - this eliminates all string map lookups from the hot path
}

//-- set translations from slider attributes ---------------------------------
void Model::changeTranslationManuale(Matrix4f& TranslationM_attr, UI_data ui_data, std::string nodeNameInRig, int boneIndex)
{
    std::string nodeNameSelected = ui_data.nodeName_toTrasform;
    
    // If this is the currently selected bone, use slider value (real-time update)
    // This ensures the selected bone responds immediately to slider changes
    // IMPORTANT: Use exact match or safe suffix match to avoid "Spine" matching "Spine1"
    bool isSelectedBone = false;
    if (nodeNameSelected != "") {
        // Exact match
        if (nodeNameInRig == nodeNameSelected) {
            isSelectedBone = true;
        } else {
            // Safe suffix match: rig name ends with selected name
            // This handles cases like "1810483883696 Spine" or "1810483883696Spine" matching "Spine"
            // But prevents "Spine1" from matching "Spine"
            if (nodeNameInRig.length() >= nodeNameSelected.length()) {
                // Check if rig name ends with selected name
                if (nodeNameInRig.substr(nodeNameInRig.length() - nodeNameSelected.length()) == nodeNameSelected) {
                    // Selected name is at the end of rig name
                    // Check if there's a boundary before it (space, digit, or non-alphanumeric)
                    size_t pos = nodeNameInRig.length() - nodeNameSelected.length();
                    if (pos == 0) {
                        // At start of string - safe match
                        isSelectedBone = true;
                    } else {
                        // Check character before the match - must be space, digit, or non-alphanumeric
                        char beforeChar = nodeNameInRig[pos - 1];
                        if (beforeChar == ' ' || std::isdigit(beforeChar) || !std::isalnum(beforeChar)) {
                            isSelectedBone = true;
                        }
                    }
                }
            }
            // Also check reverse: if selected name ends with rig name (for cases where PropertyPanel has prefix)
            if (!isSelectedBone && nodeNameSelected.length() >= nodeNameInRig.length()) {
                if (nodeNameSelected.substr(nodeNameSelected.length() - nodeNameInRig.length()) == nodeNameInRig) {
                    size_t pos = nodeNameSelected.length() - nodeNameInRig.length();
                    if (pos == 0) {
                        isSelectedBone = true;
                    } else {
                        char beforeChar = nodeNameSelected[pos - 1];
                        if (beforeChar == ' ' || std::isdigit(beforeChar) || !std::isalnum(beforeChar)) {
                            isSelectedBone = true;
                        }
                    }
                }
            }
        }
    }
    
    if (isSelectedBone) {
        TranslationM_attr.InitTranslationTransform(ui_data.mSliderTraslations.x, ui_data.mSliderTraslations.y, ui_data.mSliderTraslations.z);
        // Save to index-based vector (fast access)
        if (boneIndex >= 0 && boneIndex < static_cast<int>(m_BoneExtraTranslations.size())) {
            m_BoneExtraTranslations[boneIndex] = TranslationM_attr;
        }
    }
    // If not selected bone, TranslationM_attr was already loaded from index-based vector in updateLinearSkeleton
    // No additional lookup needed - this eliminates all string map lookups from the hot path
}

//-- set scales from slider attributes ---------------------------------
void Model::changeScaleManuale(Matrix4f& ScaleM_attr, UI_data ui_data, std::string nodeNameInRig, int boneIndex)
{
    std::string nodeNameSelected = ui_data.nodeName_toTrasform;
    
    // If this is the currently selected bone, use slider value (real-time update)
    // This ensures the selected bone responds immediately to slider changes
    // IMPORTANT: Use exact match or safe suffix match to avoid "Spine" matching "Spine1"
    bool isSelectedBone = false;
    if (nodeNameSelected != "") {
        // Exact match
        if (nodeNameInRig == nodeNameSelected) {
            isSelectedBone = true;
        } else {
            // Safe suffix match: rig name ends with selected name
            // This handles cases like "1810483883696 Spine" or "1810483883696Spine" matching "Spine"
            // But prevents "Spine1" from matching "Spine"
            if (nodeNameInRig.length() >= nodeNameSelected.length()) {
                // Check if rig name ends with selected name
                if (nodeNameInRig.substr(nodeNameInRig.length() - nodeNameSelected.length()) == nodeNameSelected) {
                    // Selected name is at the end of rig name
                    // Check if there's a boundary before it (space, digit, or non-alphanumeric)
                    size_t pos = nodeNameInRig.length() - nodeNameSelected.length();
                    if (pos == 0) {
                        // At start of string - safe match
                        isSelectedBone = true;
                    } else {
                        // Check character before the match - must be space, digit, or non-alphanumeric
                        char beforeChar = nodeNameInRig[pos - 1];
                        if (beforeChar == ' ' || std::isdigit(beforeChar) || !std::isalnum(beforeChar)) {
                            isSelectedBone = true;
                        }
                    }
                }
            }
            // Also check reverse: if selected name ends with rig name (for cases where PropertyPanel has prefix)
            if (!isSelectedBone && nodeNameSelected.length() >= nodeNameInRig.length()) {
                if (nodeNameSelected.substr(nodeNameSelected.length() - nodeNameInRig.length()) == nodeNameInRig) {
                    size_t pos = nodeNameSelected.length() - nodeNameInRig.length();
                    if (pos == 0) {
                        isSelectedBone = true;
                    } else {
                        char beforeChar = nodeNameSelected[pos - 1];
                        if (beforeChar == ' ' || std::isdigit(beforeChar) || !std::isalnum(beforeChar)) {
                            isSelectedBone = true;
                        }
                    }
                }
            }
        }
    }
    
    if (isSelectedBone) {
        ScaleM_attr.InitScaleTransform(ui_data.mSliderScales.x, ui_data.mSliderScales.y, ui_data.mSliderScales.z);
        // Save to index-based vector (fast access)
        if (boneIndex >= 0 && boneIndex < static_cast<int>(m_BoneExtraScales.size())) {
            m_BoneExtraScales[boneIndex] = ScaleM_attr;
        }
    }
    // If not selected bone, ScaleM_attr was already loaded from index-based vector in updateLinearSkeleton
    // No additional lookup needed - this eliminates all string map lookups from the hot path
}

//-- add extra rotations (if exsist) --------------------------------
void Model::addExtraRotatin(Matrix4f& RotationM_attr, std::string nodeNameInRig)
{
    // Look up bone index from name and use index-based vector
    auto it = m_BoneMapping.find(nodeNameInRig);
    if (it != m_BoneMapping.end()) {
        unsigned int boneIndex = it->second;
        if (boneIndex < m_BoneExtraRotations.size()) {
            RotationM_attr = m_BoneExtraRotations[boneIndex];
        } else {
            // Bone index out of range - use identity
            RotationM_attr.InitIdentity();
        }
    } else {
        // Bone not found - use identity
        RotationM_attr.InitIdentity();
    }
}

//-- clear all bone rotations (reset to default) --------------------------------
//-- clear all bone rotations (reset to default) --------------------------------
void Model::clearAllBoneRotations()
{
    Matrix4f identity;
    identity.InitIdentity();
    for (size_t i = 0; i < m_BoneExtraRotations.size(); i++) {
        m_BoneExtraRotations[i] = identity;
    }
    m_transformsDirty = true; // CRITICAL FIX: Tell engine to recalculate
    LOG_INFO("[Model] Cleared all bone rotations, reset to default");
}

//-- clear all bone translations (reset to default) --------------------------------
void Model::clearAllBoneTranslations()
{
    Matrix4f identity;
    identity.InitIdentity();
    for (size_t i = 0; i < m_BoneExtraTranslations.size(); i++) {
        m_BoneExtraTranslations[i] = identity;
    }
    m_transformsDirty = true; // CRITICAL FIX: Tell engine to recalculate
    LOG_INFO("[Model] Cleared all bone translations, reset to default");
}

//-- clear all bone scales (reset to default) --------------------------------
void Model::clearAllBoneScales()
{
    Matrix4f identity;
    identity.InitIdentity();
    for (size_t i = 0; i < m_BoneExtraScales.size(); i++) {
        m_BoneExtraScales[i] = identity;
    }
    m_transformsDirty = true; // CRITICAL FIX: Tell engine to recalculate
    LOG_INFO("[Model] Cleared all bone scales, reset to default");
}

//-- Direct index-based update methods (replaces name-based sync for performance) --
void Model::updateBoneRotationByIndex(int boneIndex, const glm::vec3& rotation)
{
    if (boneIndex < 0) return;
    
    // Ensure vector is large enough
    if (boneIndex >= static_cast<int>(m_BoneExtraRotations.size())) {
        m_BoneExtraRotations.resize(boneIndex + 1);
    }
    
    // Convert glm::vec3 rotation to Matrix4f
    Matrix4f rotationMatrix;
    rotationMatrix.InitRotateTransform(rotation.x, rotation.y, rotation.z);
    
    // Direct array update (O(1), no string operations)
    m_BoneExtraRotations[boneIndex] = rotationMatrix;
}

void Model::updateBoneTranslationByIndex(int boneIndex, const glm::vec3& translation)
{
    if (boneIndex < 0) return;
    
    // Ensure vector is large enough
    if (boneIndex >= static_cast<int>(m_BoneExtraTranslations.size())) {
        m_BoneExtraTranslations.resize(boneIndex + 1);
    }
    
    // Convert glm::vec3 translation to Matrix4f
    Matrix4f translationMatrix;
    translationMatrix.InitTranslationTransform(translation.x, translation.y, translation.z);
    
    // Direct array update (O(1), no string operations)
    m_BoneExtraTranslations[boneIndex] = translationMatrix;
}

void Model::updateBoneScaleByIndex(int boneIndex, const glm::vec3& scale)
{
    if (boneIndex < 0) return;
    
    // Ensure vector is large enough
    if (boneIndex >= static_cast<int>(m_BoneExtraScales.size())) {
        m_BoneExtraScales.resize(boneIndex + 1);
    }
    
    // Convert glm::vec3 scale to Matrix4f
    Matrix4f scaleMatrix;
    scaleMatrix.InitScaleTransform(scale.x, scale.y, scale.z);
    
    // Direct array update (O(1), no string operations)
    m_BoneExtraScales[boneIndex] = scaleMatrix;
}

int Model::getBoneIndexFromName(const std::string& boneName) const
{
    auto it = m_BoneMapping.find(boneName);
    if (it != m_BoneMapping.end()) {
        return static_cast<int>(it->second);
    }
    return -1;
}

//-- get the dynamic global matrix of a bone's parent (in model space) ----
glm::mat4 Model::getParentGlobalMatrix(int boneIndex) const {
    if (boneIndex < 0 || m_linearSkeleton.empty()) return glm::mat4(1.0f);
    
    // Find the linear index for this bone
    for (size_t i = 0; i < m_linearSkeleton.size(); i++) {
        if (m_linearSkeleton[i].boneIndex == boneIndex) {
            int parentIndex = m_linearSkeleton[i].parentIndex;
            if (parentIndex >= 0 && parentIndex < static_cast<int>(m_linearSkeleton.size())) {
                // Return the parent's global matrix
                // Note: Transpose is required to convert from Assimp/Math3D row-major to GLM column-major
                Matrix4f mat = m_linearSkeleton[parentIndex].globalMatrix;
                mat = mat.Transpose(); 
                return mat.toGlmMatrix();
            }
            break;
        }
    }
    return glm::mat4(1.0f); // Fallback to identity (root level)
}

glm::mat4 Model::getBoneModelSpaceMatrix(int boneIndex) const {
    if (boneIndex < 0 || m_linearSkeleton.empty()) return glm::mat4(1.0f);
    
    // Find the LinearBone entry with matching boneIndex to get its globalMatrix
    for (size_t i = 0; i < m_linearSkeleton.size(); i++) {
        if (m_linearSkeleton[i].boneIndex == boneIndex) {
            // Convert Matrix4f to GLM format
            // Note: Transpose is required to convert from Assimp/Math3D row-major to GLM column-major
            Matrix4f boneGlobalMatrix = m_linearSkeleton[i].globalMatrix;
            boneGlobalMatrix = boneGlobalMatrix.Transpose();
            return boneGlobalMatrix.toGlmMatrix();
        }
    }
    return glm::mat4(1.0f); // Default to identity if not found
}

//-- sync bone rotations from PropertyPanel --------------------------------
void Model::syncBoneRotations(const std::map<std::string, glm::vec3>& boneRotations)
{
    // If PropertyPanel map is empty, it means "Reset All Bones" was called
    // In this case, we should clear all rotations that aren't in the PropertyPanel map
    // But we only do this if the map is completely empty (not just has zero values)
    if (boneRotations.empty() && !m_BoneExtraRotations.empty()) {
        // PropertyPanel has been cleared, so clear all Model rotations except those that might be
        // currently being edited (which will be synced below if they're in the map)
        // Actually, if the map is empty, we should clear everything
        // But wait - if resetAllBones was called, clearAllBoneRotations should have been called first
        // So this shouldn't happen. But let's be safe and clear anyway if map is empty
        m_BoneExtraRotations.clear();
        return;
    }
    
    // Convert glm::vec3 rotations from PropertyPanel to Matrix4f and store in m_BoneExtraRotations
    // Only sync rotations that are non-zero (actually changed by user)
    // IMPORTANT: Don't clear the map - we want to preserve rotations that were set during changeAttributeManuale
    for (const auto& pair : boneRotations) {
        const std::string& boneName = pair.first;
        const glm::vec3& rotation = pair.second;
        
        // Only sync if rotation is non-zero (user has actually changed it)
        const float epsilon = 0.001f;
        if (std::abs(rotation.x) > epsilon || std::abs(rotation.y) > epsilon || std::abs(rotation.z) > epsilon) {
            // Convert glm::vec3 rotation to Matrix4f
            Matrix4f rotationMatrix;
            rotationMatrix.InitRotateTransform(rotation.x, rotation.y, rotation.z);
            
            // Find all rig bones that match this PropertyPanel bone name and update index-based vectors
            // This ensures lookup works when nodeNameInRig is used during transformation
            for (const auto& rigBonePair : m_BoneMapping) {
                const std::string& rigBoneName = rigBonePair.first;
                unsigned int boneIndex = rigBonePair.second;
                
                bool shouldUpdate = false;
                
                // Exact match: if rig bone name exactly equals PropertyPanel name
                if (rigBoneName == boneName) {
                    shouldUpdate = true;
                } else {
                    // Check if rig bone name ends with PropertyPanel name
                    // This handles cases like "1810483883696 Spine" or "1810483883696Spine" vs "Spine"
                    // But be careful: "Spine" should match "1810483883696 Spine" or "1810483883696Spine" but NOT "Spine1"
                    if (rigBoneName.length() >= boneName.length()) {
                        // Check if rig name ends with PropertyPanel name
                        if (rigBoneName.substr(rigBoneName.length() - boneName.length()) == boneName) {
                            // PropertyPanel name is at the end of rig name
                            // Check if there's a boundary before it (space, digit, or non-alphanumeric)
                            size_t pos = rigBoneName.length() - boneName.length();
                            if (pos == 0) {
                                // At start of string - safe match
                                shouldUpdate = true;
                            } else {
                                char beforeChar = rigBoneName[pos - 1];
                                // Allow space, digit, or non-alphanumeric before the match
                                // This allows "1810483883696 Spine" and "1810483883696Spine" to match "Spine"
                                // But prevents "Spine1" from matching "Spine"
                                if (beforeChar == ' ' || std::isdigit(beforeChar) || !std::isalnum(beforeChar)) {
                                    shouldUpdate = true;
                                }
                            }
                        }
                    }
                }
                
                // Update index-based vector
                if (shouldUpdate) {
                    // Ensure vector is large enough
                    if (boneIndex >= m_BoneExtraRotations.size()) {
                        m_BoneExtraRotations.resize(boneIndex + 1);
                    }
                    m_BoneExtraRotations[boneIndex] = rotationMatrix;
                }
            }
        } else {
            // If rotation is explicitly set to zero in PropertyPanel, reset to identity
            // This allows bones to return to their default state
            // Find all matching bones and reset their rotations to identity
            for (const auto& rigBonePair : m_BoneMapping) {
                const std::string& rigBoneName = rigBonePair.first;
                unsigned int boneIndex = rigBonePair.second;
                
                bool shouldReset = false;
                
                // Check if this rig bone name matches the PropertyPanel name
                if (rigBoneName == boneName) {
                    shouldReset = true;
                } else if (rigBoneName.length() >= boneName.length()) {
                    // Check if rig name ends with PropertyPanel name
                    if (rigBoneName.substr(rigBoneName.length() - boneName.length()) == boneName) {
                        size_t pos = rigBoneName.length() - boneName.length();
                        if (pos == 0 || rigBoneName[pos - 1] == ' ' || std::isdigit(rigBoneName[pos - 1]) || !std::isalnum(rigBoneName[pos - 1])) {
                            shouldReset = true;
                        }
                    }
                }
                
                // Reset to identity in index-based vector
                if (shouldReset && boneIndex < m_BoneExtraRotations.size()) {
                    Matrix4f identity;
                    identity.InitIdentity();
                    m_BoneExtraRotations[boneIndex] = identity;
                }
            }
        }
    }
}

//-- sync bone translations from PropertyPanel --------------------------------
void Model::syncBoneTranslations(const std::map<std::string, glm::vec3>& boneTranslations)
{
    // Convert glm::vec3 translations from PropertyPanel to Matrix4f and store in m_BoneExtraTranslations
    // Only sync translations that are non-zero (actually changed by user)
    for (const auto& pair : boneTranslations) {
        const std::string& boneName = pair.first;
        const glm::vec3& translation = pair.second;
        
        // Only sync if translation is non-zero (user has actually changed it)
        const float epsilon = 0.001f;
        if (std::abs(translation.x) > epsilon || std::abs(translation.y) > epsilon || std::abs(translation.z) > epsilon) {
            // Convert glm::vec3 translation to Matrix4f
            Matrix4f translationMatrix;
            translationMatrix.InitTranslationTransform(translation.x, translation.y, translation.z);
            
            // Find all rig bones that match this PropertyPanel bone name and update index-based vectors
            for (const auto& rigBonePair : m_BoneMapping) {
                const std::string& rigBoneName = rigBonePair.first;
                unsigned int boneIndex = rigBonePair.second;
                
                bool shouldUpdate = false;
                
                // Exact match: if rig bone name exactly equals PropertyPanel name
                if (rigBoneName == boneName) {
                    shouldUpdate = true;
                } else {
                    // Check if rig bone name ends with PropertyPanel name
                    if (rigBoneName.length() >= boneName.length()) {
                        // Check if rig name ends with PropertyPanel name
                        if (rigBoneName.substr(rigBoneName.length() - boneName.length()) == boneName) {
                            // PropertyPanel name is at the end of rig name
                            // Check if there's a boundary before it (space, digit, or non-alphanumeric)
                            size_t pos = rigBoneName.length() - boneName.length();
                            if (pos == 0) {
                                // At start of string - safe match
                                shouldUpdate = true;
                            } else {
                                char beforeChar = rigBoneName[pos - 1];
                                // Allow space, digit, or non-alphanumeric before the match
                                if (beforeChar == ' ' || std::isdigit(beforeChar) || !std::isalnum(beforeChar)) {
                                    shouldUpdate = true;
                                }
                            }
                        }
                    }
                }
                
                // Update index-based vector
                if (shouldUpdate) {
                    // Ensure vector is large enough
                    if (boneIndex >= m_BoneExtraTranslations.size()) {
                        m_BoneExtraTranslations.resize(boneIndex + 1);
                    }
                    m_BoneExtraTranslations[boneIndex] = translationMatrix;
                }
            }
        } else {
            // If translation is explicitly set to zero in PropertyPanel, reset to identity
            // Find all matching bones and reset their translations to identity
            for (const auto& rigBonePair : m_BoneMapping) {
                const std::string& rigBoneName = rigBonePair.first;
                unsigned int boneIndex = rigBonePair.second;
                
                bool shouldReset = false;
                
                // Check if this rig bone name matches the PropertyPanel name
                if (rigBoneName == boneName) {
                    shouldReset = true;
                } else if (rigBoneName.length() >= boneName.length()) {
                    // Check if rig name ends with PropertyPanel name
                    if (rigBoneName.substr(rigBoneName.length() - boneName.length()) == boneName) {
                        size_t pos = rigBoneName.length() - boneName.length();
                        if (pos == 0 || rigBoneName[pos - 1] == ' ' || std::isdigit(rigBoneName[pos - 1]) || !std::isalnum(rigBoneName[pos - 1])) {
                            shouldReset = true;
                        }
                    }
                }
                
                // Reset to identity in index-based vector
                if (shouldReset && boneIndex < m_BoneExtraTranslations.size()) {
                    Matrix4f identity;
                    identity.InitIdentity();
                    m_BoneExtraTranslations[boneIndex] = identity;
                }
            }
        }
    }
}

//-- sync bone scales from PropertyPanel --------------------------------
void Model::syncBoneScales(const std::map<std::string, glm::vec3>& boneScales)
{
    // Convert glm::vec3 scales from PropertyPanel to Matrix4f and store in m_BoneExtraScales
    // Only sync scales that are not 1.0 (actually changed by user)
    for (const auto& pair : boneScales) {
        const std::string& boneName = pair.first;
        const glm::vec3& scale = pair.second;
        
        // Only sync if scale is not 1.0 (user has actually changed it)
        const float epsilon = 0.001f;
        if (std::abs(scale.x - 1.0f) > epsilon || std::abs(scale.y - 1.0f) > epsilon || std::abs(scale.z - 1.0f) > epsilon) {
            // Convert glm::vec3 scale to Matrix4f
            Matrix4f scaleMatrix;
            scaleMatrix.InitScaleTransform(scale.x, scale.y, scale.z);
            
            // Find all rig bones that match this PropertyPanel bone name and update index-based vectors
            for (const auto& rigBonePair : m_BoneMapping) {
                const std::string& rigBoneName = rigBonePair.first;
                unsigned int boneIndex = rigBonePair.second;
                
                bool shouldUpdate = false;
                
                // Exact match: if rig bone name exactly equals PropertyPanel name
                if (rigBoneName == boneName) {
                    shouldUpdate = true;
                } else {
                    // Check if rig bone name ends with PropertyPanel name
                    if (rigBoneName.length() >= boneName.length()) {
                        // Check if rig name ends with PropertyPanel name
                        if (rigBoneName.substr(rigBoneName.length() - boneName.length()) == boneName) {
                            // PropertyPanel name is at the end of rig name
                            // Check if there's a boundary before it (space, digit, or non-alphanumeric)
                            size_t pos = rigBoneName.length() - boneName.length();
                            if (pos == 0) {
                                // At start of string - safe match
                                shouldUpdate = true;
                            } else {
                                char beforeChar = rigBoneName[pos - 1];
                                // Allow space, digit, or non-alphanumeric before the match
                                if (beforeChar == ' ' || std::isdigit(beforeChar) || !std::isalnum(beforeChar)) {
                                    shouldUpdate = true;
                                }
                            }
                        }
                    }
                }
                
                // Update index-based vector
                if (shouldUpdate) {
                    // Ensure vector is large enough
                    if (boneIndex >= m_BoneExtraScales.size()) {
                        m_BoneExtraScales.resize(boneIndex + 1);
                    }
                    m_BoneExtraScales[boneIndex] = scaleMatrix;
                }
            }
        } else {
            // If scale is explicitly set to 1.0 in PropertyPanel, reset to identity
            // Find all matching bones and reset their scales to identity
            for (const auto& rigBonePair : m_BoneMapping) {
                const std::string& rigBoneName = rigBonePair.first;
                unsigned int boneIndex = rigBonePair.second;
                
                bool shouldReset = false;
                
                // Check if this rig bone name matches the PropertyPanel name
                if (rigBoneName == boneName) {
                    shouldReset = true;
                } else if (rigBoneName.length() >= boneName.length()) {
                    // Check if rig name ends with PropertyPanel name
                    if (rigBoneName.substr(rigBoneName.length() - boneName.length()) == boneName) {
                        size_t pos = rigBoneName.length() - boneName.length();
                        if (pos == 0 || rigBoneName[pos - 1] == ' ' || std::isdigit(rigBoneName[pos - 1]) || !std::isalnum(rigBoneName[pos - 1])) {
                            shouldReset = true;
                        }
                    }
                }
                
                // Reset to identity in index-based vector
                if (shouldReset && boneIndex < m_BoneExtraScales.size()) {
                    Matrix4f identity;
                    identity.InitIdentity();
                    m_BoneExtraScales[boneIndex] = identity;
                }
            }
        }
    }
}

//-- FindRotation ---------------------------------------------
unsigned int Model::FindRotation(float AnimationTime, const aiNodeAnim* pNodeAnim)
{
    //-- Check if there are rotation keyframes -------------- 
    //assert(pNodeAnim->mNumScalingKeys > 0);
    
    //-- Find the rotation key just before the current animation time and return the index
    for (unsigned int i = 0; i < pNodeAnim->mNumRotationKeys - 1; i++) {
        if (AnimationTime < (float)pNodeAnim->mRotationKeys[i + 1].mTime) {
            return i;
        }
    }
    //assert(0);

    return 0;
}

//-- FindScale ---------------------------------------------
unsigned int Model::FindScale(float AnimationTime, const aiNodeAnim* pNodeAnim)
{
    //assert(pNodeAnim->mNumScalingKeys > 0);

    //-- Find the scaling key just before the current animation time and return the index -------------
    for (unsigned int i = 0; i < pNodeAnim->mNumScalingKeys - 1; i++) {
        if (AnimationTime < (float)pNodeAnim->mScalingKeys[i + 1].mTime) {
            return i;
        }
    }
    //assert(0);

    return 0;
}

//-- FindTranslation ---------------------------------------------
unsigned int Model::FindTranslation(float AnimationTime, const aiNodeAnim* pNodeAnim)
{
    //assert(pNodeAnim->mNumPositionKeys > 0);

    //-- Find the translation key just before the current animation time and return the index ---------
    for (unsigned int i = 0; i < pNodeAnim->mNumPositionKeys - 1; i++) {
        if (AnimationTime < (float)pNodeAnim->mPositionKeys[i + 1].mTime) {
            return i;
        }
    }
    //assert(0);

    return 0;
}

/**
 * @brief Calculates interpolated rotation quaternion between two animation keyframes.
 * 
 * This function performs robust keyframe interpolation with bounds checking to prevent
 * out-of-range access errors. If the animation time exceeds the last keyframe, it falls
 * back to the last available keyframe value (clamping behavior).
 * 
 * @param Out Output quaternion (interpolated result)
 * @param AnimationTime Current animation time in ticks
 * @param pNodeAnim Pointer to animation channel containing rotation keyframes
 */
void Model::CalcInterpolatedRotation(aiQuaternion& Out, float AnimationTime, const aiNodeAnim* pNodeAnim)
{
    // Single keyframe case: no interpolation needed
    if (pNodeAnim->mNumRotationKeys == 1) {
        Out = pNodeAnim->mRotationKeys[0].mValue;
        return;
    }
    
    // Find current and next keyframe indices
    unsigned int RotationIndex = FindRotation(AnimationTime, pNodeAnim);
    unsigned int NextRotationIndex = (RotationIndex + 1);
    
    // Bounds check: if next index is out of range, clamp to last keyframe
    if (NextRotationIndex >= pNodeAnim->mNumRotationKeys) {
        LOG_WARNING("[Model] Animation time exceeds rotation keyframes - clamping to last keyframe");
        // Fallback: Use last available keyframe (prevents out-of-bounds access)
        if (pNodeAnim->mNumRotationKeys > 0) {
            Out = pNodeAnim->mRotationKeys[pNodeAnim->mNumRotationKeys - 1].mValue;
        } else {
            Out = aiQuaternion();  // Identity quaternion if no keys available
        }
        return;
    }

    //-- Calculate delta time, i.e time between the two keyframes -------------------------------
    float DeltaTime = pNodeAnim->mRotationKeys[NextRotationIndex].mTime - pNodeAnim->mRotationKeys[RotationIndex].mTime;

    //--- Calculate the elapsed time within the delta time -------------------
    float Factor = (AnimationTime - (float)pNodeAnim->mRotationKeys[RotationIndex].mTime) / DeltaTime;
    //assert(Factor >= 0.0f && Factor <= 1.0f);

    //-- Obtain the quaternions values for the current and next keyframe --------------------- 
    const aiQuaternion& StartRotationQ = pNodeAnim->mRotationKeys[RotationIndex].mValue;
    const aiQuaternion& EndRotationQ = pNodeAnim->mRotationKeys[NextRotationIndex].mValue;

    //-- Interpolate between them using the Factor ----------------------------
    aiQuaternion::Interpolate(Out, StartRotationQ, EndRotationQ, Factor);

    //-- Normalise and set the reference -------------------------
    Out = Out.Normalize();
}

/**
 * @brief Calculates interpolated scaling vector between two animation keyframes.
 * 
 * This function performs robust keyframe interpolation with bounds checking to prevent
 * out-of-range access errors. If the animation time exceeds the last keyframe, it falls
 * back to the last available keyframe value (clamping behavior).
 * 
 * @param Out Output vector (interpolated scaling result)
 * @param AnimationTime Current animation time in ticks
 * @param pNodeAnim Pointer to animation channel containing scaling keyframes
 */
void Model::CalcInterpolatedScaling(aiVector3D& Out, float AnimationTime, const aiNodeAnim* pNodeAnim)
{
    // Single keyframe case: no interpolation needed
    if (pNodeAnim->mNumScalingKeys == 1) {
        Out = pNodeAnim->mScalingKeys[0].mValue;
        return;
    }

    // Find current and next keyframe indices
    unsigned int ScalingIndex = FindScale(AnimationTime, pNodeAnim);
    unsigned int NextScalingIndex = (ScalingIndex + 1);
    
    // Bounds check: if next index is out of range, clamp to last keyframe
    if (NextScalingIndex >= pNodeAnim->mNumScalingKeys) {
        LOG_WARNING("[Model] Animation time exceeds scaling keyframes - clamping to last keyframe");
        // Fallback: Use last available keyframe (prevents out-of-bounds access)
        if (pNodeAnim->mNumScalingKeys > 0) {
            Out = pNodeAnim->mScalingKeys[pNodeAnim->mNumScalingKeys - 1].mValue;
        } else {
            Out = aiVector3D(1.0f, 1.0f, 1.0f);  // Identity scale if no keys available
        }
        return;
    }
    float DeltaTime = pNodeAnim->mScalingKeys[NextScalingIndex].mTime - pNodeAnim->mScalingKeys[ScalingIndex].mTime;
    float Factor = (AnimationTime - (float)pNodeAnim->mScalingKeys[ScalingIndex].mTime) / DeltaTime;
    //assert(Factor >= 0.0f && Factor <= 1.0f);
    const aiVector3D& Start = pNodeAnim->mScalingKeys[ScalingIndex].mValue;
    const aiVector3D& End = pNodeAnim->mScalingKeys[NextScalingIndex].mValue;

    aiVector3D Delta = End - Start;
    Out = Start + Factor * Delta;
}

/**
 * @brief Calculates interpolated translation vector between two animation keyframes.
 * 
 * This function performs robust keyframe interpolation with bounds checking to prevent
 * out-of-range access errors. If the animation time exceeds the last keyframe, it falls
 * back to the last available keyframe value (clamping behavior).
 * 
 * @param Out Output vector (interpolated translation result)
 * @param AnimationTime Current animation time in ticks
 * @param pNodeAnim Pointer to animation channel containing position keyframes
 */
void Model::CalcInterpolatedTranslation(aiVector3D& Out, float AnimationTime, const aiNodeAnim* pNodeAnim)
{
    // Single keyframe case: no interpolation needed
    if (pNodeAnim->mNumPositionKeys == 1) {
        Out = pNodeAnim->mPositionKeys[0].mValue;
        return;
    }

    // Find current and next keyframe indices
    unsigned int PositionIndex = FindTranslation(AnimationTime, pNodeAnim);
    unsigned int NextPositionIndex = (PositionIndex + 1);
    
    // Bounds check: if next index is out of range, clamp to last keyframe
    if (NextPositionIndex >= pNodeAnim->mNumPositionKeys) {
        LOG_WARNING("[Model] Animation time exceeds translation keyframes - clamping to last keyframe");
        // Fallback: Use last available keyframe (prevents out-of-bounds access)
        if (pNodeAnim->mNumPositionKeys > 0) {
            Out = pNodeAnim->mPositionKeys[pNodeAnim->mNumPositionKeys - 1].mValue;
        } else {
            Out = aiVector3D(0.0f, 0.0f, 0.0f);  // Origin if no keys available
        }
        return;
    }
    float DeltaTime = pNodeAnim->mPositionKeys[NextPositionIndex].mTime - pNodeAnim->mPositionKeys[PositionIndex].mTime;
    float Factor = (AnimationTime - (float)pNodeAnim->mPositionKeys[PositionIndex].mTime) / DeltaTime;
    //assert(Factor >= 0.0f && Factor <= 1.0f);
    const aiVector3D& Start = pNodeAnim->mPositionKeys[PositionIndex].mValue;
    const aiVector3D& End = pNodeAnim->mPositionKeys[NextPositionIndex].mValue;

    aiVector3D Delta = End - Start;
    Out = Start + Factor * Delta;
}



int Model::getFPS()
{
    return mFPS;
}

float Model::getAnimationDuration()
{
    if (mScene && mScene->HasAnimations() && mScene->mNumAnimations > 0) {
        return mScene->mAnimations[0]->mDuration;
    }
    return 0.0f;
}

//-- get total polygon count (sum of all mesh indices / 3) ----
size_t Model::getPolygonCount() const {
    size_t totalPolygons = 0;
    
    // Iterate through all meshes and sum up polygon counts
    // Each polygon is represented by 3 indices (triangles)
    for (const Mesh& mesh : meshes) {
        totalPolygons += mesh.indices.size() / 3;
    }
    
    return totalPolygons;
}

//-- get total bone/joint count ----
unsigned int Model::getBoneCount() const {
    return mNumBones;
}

//-- get bounding box for all meshes ----
void Model::getBoundingBox(glm::vec3& min, glm::vec3& max) {
    if (meshes.empty() || !mScene) {
        min = glm::vec3(-1.0f);
        max = glm::vec3(1.0f);
        return;
    }
    
    bool first = true;
    min = glm::vec3(FLT_MAX);
    max = glm::vec3(-FLT_MAX);
    
    // Iterate through all meshes and find the overall bounding box
    for (unsigned int i = 0; i < mScene->mNumMeshes; i++) {
        const aiMesh* mesh = mScene->mMeshes[i];
        
        for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
            glm::vec3 vertex(
                mesh->mVertices[j].x,
                mesh->mVertices[j].y,
                mesh->mVertices[j].z
            );
            
            // Apply model transform (pos and size)
            vertex = pos + vertex * size;
            
            if (first) {
                min = max = vertex;
                first = false;
            } else {
                min = glm::min(min, vertex);
                max = glm::max(max, vertex);
            }
        }
    }
    
    // If no vertices found, use default
    if (first) {
        min = glm::vec3(-1.0f);
        max = glm::vec3(1.0f);
    }
}

/**
 * Get bounding box for all meshes with current bone transformations applied
 * 
 * CRITICAL PERFORMANCE FIX: This method now uses bone-based calculation instead of vertex iteration
 * This changes complexity from O(Vertices) to O(Bones), dramatically improving performance.
 * Bones are typically 10-100x fewer than vertices, making this much faster.
 * 
 * The method:
 * 1. Gets all bone global transforms (already calculated in updateAnimation)
 * 2. Iterates through bones (O(Bones) complexity)
 * 3. Uses bone translation positions to build min/max box
 * 4. Applies model transform (pos and size)
 * 5. Adds padding to ensure box covers mesh volume
 * 
 * This ensures the bounding box accurately reflects:
 * - Current animation pose
 * - Bone deformations
 * - Manual bone transformations
 * - Model position and scale
 * 
 * @param min Output parameter for minimum corner of bounding box
 * @param max Output parameter for maximum corner of bounding box
 * @param boneTransforms Current bone transformation matrices (from BoneTransform()) - UNUSED, kept for API compatibility
 */
void Model::getBoundingBoxWithBones(glm::vec3& min, glm::vec3& max, const std::vector<glm::mat4>& boneTransforms) {
    // OPTIMIZED: Iterate over m_linearSkeleton using isRootBone flag (ZERO STRING OPERATIONS)
    // This eliminates all std::transform and std::string::find operations from the hot path
    
    if (meshes.empty() || !mScene || m_linearSkeleton.empty() || m_BoneGlobalTransforms.empty()) {
        min = glm::vec3(-1.0f);
        max = glm::vec3(1.0f);
        return;
    }
    
    // Initialize min/max
    bool first = true;
    min = glm::vec3(FLT_MAX);
    max = glm::vec3(-FLT_MAX);
    
    // First pass: Collect all bone positions (before world-space transform) to identify outliers
    std::vector<glm::vec3> bonePositions;
    for (const auto& bone : m_linearSkeleton) {
        // Only process actual bones (boneIndex >= 0)
        if (bone.boneIndex >= 0 && bone.boneIndex < static_cast<int>(m_BoneGlobalTransforms.size())) {
            const BoneGlobalTransform& boneTransform = m_BoneGlobalTransforms[bone.boneIndex];
            bonePositions.push_back(boneTransform.translation);
        }
    }
    
    if (bonePositions.empty()) {
        getBoundingBox(min, max);
        return;
    }
    
    // Calculate center of all bones (in model space) to identify outliers
    glm::vec3 boneCenter = glm::vec3(0.0f);
    for (const auto& pos : bonePositions) {
        boneCenter += pos;
    }
    boneCenter /= static_cast<float>(bonePositions.size());
    
    // Calculate average distance from center to identify outliers
    float avgDistance = 0.0f;
    if (bonePositions.size() > 1) {
        for (const auto& pos : bonePositions) {
            avgDistance += glm::length(pos - boneCenter);
        }
        avgDistance /= static_cast<float>(bonePositions.size());
    }
    
    // Filter threshold: bones more than 3x average distance from center are considered outliers
    const float OUTLIER_THRESHOLD = (avgDistance > 0.01f) ? (3.0f * avgDistance) : 1000.0f;
    
    // Second pass: Iterate over linear skeleton (ZERO STRING OPERATIONS - uses isRootBone flag)
    for (const auto& bone : m_linearSkeleton) {
        // Only process actual bones (boneIndex >= 0)
        if (bone.boneIndex < 0 || bone.boneIndex >= static_cast<int>(m_BoneGlobalTransforms.size())) {
            continue;
        }
        
        const BoneGlobalTransform& boneTransform = m_BoneGlobalTransforms[bone.boneIndex];
        const glm::vec3& bonePosition = boneTransform.translation;
        
        // Filter out root bones at origin using pre-computed flag (NO STRING OPERATIONS)
        bool isAtOrigin = glm::length(bonePosition) < 0.01f;
        if (bone.isRootBone && isAtOrigin) {
            continue;  // Skip root bones at origin
        }
        
        // Also filter bones that are outliers (far from character center) if they're at origin
        if (isAtOrigin && bonePositions.size() > 1) {
            bool isOutlier = glm::length(bonePosition - boneCenter) > OUTLIER_THRESHOLD;
            if (isOutlier) {
                continue;  // Skip outlier bones at origin
            }
        }
        
        // Apply model transform (pos and size) - transform to world space
        glm::vec3 worldPosition = pos + bonePosition * size;
        
        // Update min/max
        if (first) {
            min = max = worldPosition;
            first = false;
        } else {
            min = glm::min(min, worldPosition);
            max = glm::max(max, worldPosition);
        }
    }
    
    // If no bones found after filtering, fallback to static bounding box
    if (first) {
        getBoundingBox(min, max);
        return;
    }
    
    // Add padding/margin AFTER world-space transformation
    const float BONE_BOUNDING_BOX_PADDING = 0.5f;
    min -= glm::vec3(BONE_BOUNDING_BOX_PADDING);
    max += glm::vec3(BONE_BOUNDING_BOX_PADDING);
}

//-- Update bone selection state (called when selection changes, not every frame) ----
void Model::updateBoneSelection(const std::string& selectedNodeName)
{
    // LOG: Track bone selection map rebuilds (only print when node name changes)
    static std::string lastLoggedNodeName = "";
    if (lastLoggedNodeName != selectedNodeName) {
        LOG_INFO("LOG: Rebuilding BoneSelection Map for " + (selectedNodeName.empty() ? "(empty)" : selectedNodeName) + "...");
        lastLoggedNodeName = selectedNodeName;
    }
    
    // Reset all selection flags
    for (size_t i = 0; i < m_isBoneSelected.size(); i++) {
        m_isBoneSelected[i] = false;
    }
    
    // Find the bone index for the selected node name
    if (!selectedNodeName.empty()) {
        auto it = m_BoneMapping.find(selectedNodeName);
        if (it != m_BoneMapping.end()) {
            unsigned int boneIndex = it->second;
            if (boneIndex < m_isBoneSelected.size()) {
                m_isBoneSelected[boneIndex] = true;
            }
        }
    }
    
    // Mark transforms as dirty so updateLinearSkeleton runs next frame
    m_transformsDirty = true;
}

//-- get global transforms for bones only (for backward compatibility) ----
std::map<std::string, BoneGlobalTransform> Model::getBoneGlobalTransformsOnly() const
{
    std::map<std::string, BoneGlobalTransform> boneTransforms;
    
    // Build map from flat vector using bone mapping (only for backward compatibility)
    for (const auto& pair : m_BoneMapping) {
        const std::string& boneName = pair.first;
        unsigned int boneIndex = pair.second;
        
        if (boneIndex < m_BoneGlobalTransforms.size()) {
            boneTransforms[boneName] = m_BoneGlobalTransforms[boneIndex];
        }
    }
    
    return boneTransforms;
}

//-- print bone global transforms to console (debug output) ----
void Model::printBoneGlobalTransforms() const
{
    std::map<std::string, BoneGlobalTransform> boneTransforms = getBoneGlobalTransformsOnly();
    
    if (boneTransforms.empty()) {
        LOG_INFO("[BoneTransforms] No bones found or transforms not calculated yet");
        return;
    }
    
    LOG_INFO("[BoneTransforms] ========================================");
    LOG_INFO("[BoneTransforms] Bone Global Transforms (" + std::to_string(boneTransforms.size()) + " bones):");
    LOG_INFO("[BoneTransforms] ========================================");
    LOG_INFO("[BoneTransforms] Bone Name | Translation (X, Y, Z) | Rotation (X, Y, Z) - Degrees | Scale (X, Y, Z)");
    LOG_INFO("[BoneTransforms] ---------------------------------------------------------------------------------");
    
    for (const auto& pair : boneTransforms) {
        const std::string& boneName = pair.first;
        const BoneGlobalTransform& transform = pair.second;
        
        std::string transformLine = "[BoneTransforms] " + boneName + " | "
                                    + "(" + std::to_string(transform.translation.x) + ", " + std::to_string(transform.translation.y) + ", " + std::to_string(transform.translation.z) + ") | "
                                    + "(" + std::to_string(transform.rotation.x) + ", " + std::to_string(transform.rotation.y) + ", " + std::to_string(transform.rotation.z) + ") | "
                                    + "(" + std::to_string(transform.scale.x) + ", " + std::to_string(transform.scale.y) + ", " + std::to_string(transform.scale.z) + ")";
        LOG_INFO(transformLine);
    }
    
    LOG_INFO("[BoneTransforms] ========================================");
}

//-- get bounding box for a specific node by name (DEPRECATED - use index-based version) ----
//-- get bounding box for a specific bone by index (OPTIMIZED - zero string operations) ----
void Model::getBoundingBoxForBoneIndex(int boneIndex, glm::vec3& min, glm::vec3& max) {
    // CRITICAL PERFORMANCE FIX: Index-based calculation with ZERO string operations
    // Uses pre-calculated child indices from flattenHierarchy to traverse hierarchy
    // Directly accesses m_BoneGlobalTransforms vector (no map lookups)
    
    if (!mScene || boneIndex < 0 || m_linearSkeleton.empty() || m_BoneGlobalTransforms.empty()) {
        // Invalid input or no bones, fallback to all bones
        getBoundingBox(min, max);
        return;
    }
    
    // Find the linear skeleton index for this bone
    int linearIndex = -1;
    for (size_t i = 0; i < m_linearSkeleton.size(); i++) {
        if (m_linearSkeleton[i].boneIndex == boneIndex) {
            linearIndex = static_cast<int>(i);
            break;
        }
    }
    
    if (linearIndex < 0) {
        // Bone not found in linear skeleton, fallback
        getBoundingBox(min, max);
        return;
    }
    
    // Initialize min/max
    bool first = true;
    min = glm::vec3(FLT_MAX);
    max = glm::vec3(-FLT_MAX);
    
    // Collect all descendant bone indices using pre-calculated child indices (NO STRING OPERATIONS)
    std::vector<int> boneIndicesToProcess;
    std::vector<int> stack;
    stack.push_back(linearIndex);
    
    // Traverse hierarchy using pre-calculated child indices (O(Nodes) where Nodes << Vertices)
    while (!stack.empty()) {
        int currentLinearIndex = stack.back();
        stack.pop_back();
        
        if (currentLinearIndex < 0 || currentLinearIndex >= static_cast<int>(m_linearSkeleton.size())) {
            continue;
        }
        
        const LinearBone& currentBone = m_linearSkeleton[currentLinearIndex];
        
        // If this is a bone (not just an intermediate node), add it to the list
        if (currentBone.boneIndex >= 0) {
            boneIndicesToProcess.push_back(currentBone.boneIndex);
        }
        
        // Add all children to the stack (using pre-calculated child indices)
        for (int childIndex : currentBone.childIndices) {
            if (childIndex >= 0 && childIndex < static_cast<int>(m_linearSkeleton.size())) {
                stack.push_back(childIndex);
            }
        }
    }
    
    // Iterate through collected bone indices (O(Bones) complexity, ZERO STRING OPERATIONS)
    for (int processedBoneIndex : boneIndicesToProcess) {
        // Direct vector access (O(1)) - NO MAP LOOKUPS
        if (processedBoneIndex < 0 || processedBoneIndex >= static_cast<int>(m_BoneGlobalTransforms.size())) {
            continue;
        }
        
        const BoneGlobalTransform& boneTransform = m_BoneGlobalTransforms[processedBoneIndex];
        glm::vec3 bonePosition = boneTransform.translation;
        
        // Filter out root bones at origin using pre-computed isRootBone flag
        // Find linear index for this bone to check isRootBone flag (only for bones at origin)
        bool isRootBone = false;
        bool isAtOrigin = glm::length(bonePosition) < 0.01f;
        
        if (isAtOrigin) {
            // Find linear index for this bone (O(N) but only for bones at origin)
            for (size_t i = 0; i < m_linearSkeleton.size(); i++) {
                if (m_linearSkeleton[i].boneIndex == processedBoneIndex) {
                    isRootBone = m_linearSkeleton[i].isRootBone;
                    break;
                }
            }
        }
        
        // Filter out root bones at origin
        if (isRootBone && isAtOrigin) {
            continue;  // Skip this bone
        }
        
        // Apply model transform (pos and size) - transform to world space
        glm::vec3 worldPosition = pos + bonePosition * size;
        
        // Update min/max
        if (first) {
            min = max = worldPosition;
            first = false;
        } else {
            min = glm::min(min, worldPosition);
            max = glm::max(max, worldPosition);
        }
    }
    
    // If no bones found after filtering, fallback to static bounding box
    if (first) {
        getBoundingBox(min, max);
        return;
    }
    
    // Add padding/margin AFTER world-space transformation
    const float BONE_BOUNDING_BOX_PADDING = 0.5f;
    min -= glm::vec3(BONE_BOUNDING_BOX_PADDING);
    max += glm::vec3(BONE_BOUNDING_BOX_PADDING);
}

//**********************************************************************************
//**********************************************************************************





