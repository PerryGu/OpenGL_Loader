#ifndef MODEL_H
#define MODEL_H


#include "mesh.h"
#include "utils.h"

#include <map>
#include <vector>
#include <algorithm>

// CRITICAL: GLAD must be included before GLFW to prevent OpenGL header conflicts
#include <glad/glad.h>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>


//-- assimp ------------------
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

//-- ofbx ----------------
#include "ofbx.h"
#include "math3D.h"

// Forward declaration for raycast
struct Ray;


//*******************************************************************
struct UI_data
{
    std::string nodeName_toTrasform = "";
    glm::vec3 mSliderRotations = glm::vec3(0.0);
    glm::vec3 mSliderTraslations = glm::vec3(0.0);
    glm::vec3 mSliderScales = glm::vec3(1.0);  // Default scale is 1.0 (no scaling)
   
};

//struct VertexStruct
//{
//    glm::vec3 position; //!< Vertex position 
//    glm::vec3 normal; //!< Vertex normal
//    glm::vec2 uvs; //!< Vertex uv's
//};


struct VertexBoneData
{
    unsigned int IDs[4]; //!< An array of 4 bone Ids that influence a single vertex.
    float Weights[4]; //!< An array of the weight influence per bone. 

    VertexBoneData()
    {
        // 0's out the arrays. 
        Reset();
    }

    void Reset()
    {
        memset(IDs, 0, 4 * sizeof(IDs[0]));
        memset(Weights, 0, 4 * sizeof(Weights[0]));
    }

    void AddBoneData(unsigned int BoneID, float Weight)
    {
        for (unsigned int i = 0; i < 4; i++) {

            // Check to see if there are any empty weight values. 
            if (Weights[i] == 0.0) {
                // Add ID of bone. 
                IDs[i] = BoneID;

                // Set the vertex weight influence for this bone ID. 
                Weights[i] = Weight;
                return;
            }
        }
        // should never get here - more bones than we have space for
        //assert(0);
    }
};

//-- A mesh entry for each mesh read in from the Assimp scene. A model is usually consisted of a collection of these ----------
struct MeshEntry {

    unsigned int BaseVertex; //!< Total number of mesh indices. 
    unsigned int BaseIndex; //!< The base vertex of this mesh in the vertices array for the entire model.
    unsigned int NumIndices; //!< The base index of this mesh in the indices array for the entire model. 
    unsigned int MaterialIndex;

    MeshEntry()
    {
        NumIndices = 0;
        BaseVertex = 0;
        BaseIndex = 0;
    }

    ~MeshEntry() {}
};

//== Stores bone information =====================================
struct BoneInfo
{
    //std::string BoneName; //--set bone name -------------------------- 
    Matrix4f  FinalTransformation; //-- Final transformation to apply to vertices 
    Matrix4f  BoneOffset; //-- Initial offset from local to bone space. 
    //Matrix4f addRotation;

    BoneInfo()
    {
        BoneOffset.SetZero();
        FinalTransformation.SetZero();
    }
};

//== Stores global transform for each bone/node =====================================
struct BoneGlobalTransform
{
    glm::vec3 translation;  // Global translation
    glm::vec3 rotation;    // Global rotation (Euler angles in degrees)
    glm::vec3 scale;       // Global scale

    BoneGlobalTransform()
        : translation(0.0f), rotation(0.0f), scale(1.0f)
    {
    }
};



//== class to represent model ===========
class Model {
public:
    // Linear bone structure for flattened hierarchy (prepared during load time)
    struct LinearBone {
        const aiNode* pNode;    // Pointer to the node in the scene hierarchy
        int parentIndex;         // Index of parent in m_linearSkeleton (-1 for root)
        int boneIndex;           // Index in m_BoneInfo mapping (-1 if not a bone)
        Matrix4f globalMatrix;   // Cached global transformation matrix (computed during update)
        bool isRootBone;         // Flag indicating if this is a root-like bone (set during load, eliminates string operations)
        std::vector<int> childIndices;  // Pre-calculated list of child indices in m_linearSkeleton (eliminates runtime hierarchy traversal)
    };

    glm::vec3 pos;
    glm::vec3 size;
    glm::quat rotation;  // Model rotation as quaternion (for gizmo rotation support)
    
    // Model selection state (prevents selection leakage between characters)
    bool m_isSelected = false;
    
    // Flip normals flag (for models with inverted normals)
    bool m_flipNormals = false;
    
    // Use default material flag (forces grey rendering, ignoring textures)
    bool m_useDefaultMaterial = false;
    
    // Wireframe mode flag (renders model as wireframe)
    bool m_wireframeMode = false;
    
    // Show skeleton flag (renders bone hierarchy as lines)
    bool m_showSkeleton = false;
    
    // Show bounding box flag (renders bounding box wireframe)
    bool m_showBoundingBox = true;
    
    // Selected bone/node name (set by bone-picking raycast)
    std::string m_selectedNodeName = "";

    Model(glm::vec3 pos = glm::vec3(0.0f), glm::vec3 size = glm::vec3(1.0f), bool noTex = false);
    
    // Delete copy constructor and copy assignment (Assimp::Importer is not copyable)
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    
    // Move constructor
    Model(Model&& other) noexcept;
    
    // Move assignment operator
    Model& operator=(Model&& other) noexcept;

    //-- initialize method (to be overriden) -----------
    void init();
    void loadModel(std::string path);


    //-- render instance(s) --------------
    // If customModelMatrix is provided (not nullptr), it will be used instead of building from pos/rotation/size
    // This allows external control of the model transform (e.g., from PropertyPanel RootNode)
    void render(Shader shader, const glm::mat4* customModelMatrix = nullptr);
    
    //-- draw skeleton (bone hierarchy as lines) --------------
    // Draws the skeleton using the exact same modelMatrix used by the viewport/gizmo
    // shader: Shader program to use for rendering (must support model/view/projection uniforms and gridColor uniform)
    void DrawSkeleton(Shader& shader, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& modelMatrix);
    
    //-- pick bone via raycast (bone-picking for viewport selection) --------------
    // Tests ray against bone segments and selects the closest bone within threshold
    // ray: Mouse ray in world space
    // modelMatrix: Model's RootNode transform matrix (same as used for rendering)
    // Returns: true if a bone was selected, false otherwise
    bool pickBone(const Ray& ray, const glm::mat4& modelMatrix);

    //-- free up memory ---------------
    void cleanup();
    
    //-- clear model (remove all meshes and reset state) ----
    void clear();

    //-- skeleton data --------------------------------------
    void BoneTransform(float TimeInSeconds, std::vector<glm::mat4>& Transforms, UI_data ui_data);


    //-- get File Extension --------------------
    std::string getFileExtension();

    //-- get File FPS --------------------
    int getFPS();
    
    //-- get Animation Duration --------------------
    float getAnimationDuration();
    
    //-- get total polygon count (sum of all mesh indices / 3) ----
    size_t getPolygonCount() const;
    
    //-- get bounding box for all meshes ----
    void getBoundingBox(glm::vec3& min, glm::vec3& max);
    
    //-- get bounding box for all meshes with current bone transformations applied ----
    // This method calculates the bounding box considering current animation/deformation state
    // @param min Output parameter for minimum corner
    // @param max Output parameter for maximum corner
    // @param boneTransforms Current bone transformation matrices (from BoneTransform())
    void getBoundingBoxWithBones(glm::vec3& min, glm::vec3& max, const std::vector<glm::mat4>& boneTransforms);
    
    //-- get bounding box for a specific node by name (DEPRECATED - use index-based version) ----
    void getBoundingBoxForNode(const std::string& nodeName, glm::vec3& min, glm::vec3& max);
    
    //-- get bounding box for a specific bone by index (OPTIMIZED - zero string operations) ----
    void getBoundingBoxForBoneIndex(int boneIndex, glm::vec3& min, glm::vec3& max);
    
    // Direct index-based update methods (replaces name-based sync functions for performance)
    // These methods directly update m_BoneExtraRotations/Translations/Scales arrays by bone index
    // Eliminates string operations and map lookups from the hot path
    void updateBoneRotationByIndex(int boneIndex, const glm::vec3& rotation);
    void updateBoneTranslationByIndex(int boneIndex, const glm::vec3& translation);
    void updateBoneScaleByIndex(int boneIndex, const glm::vec3& scale);
    
    // Get bone index from bone name (for PropertyPanel to set selectedBoneIndex)
    // Returns -1 if bone not found
    int getBoneIndexFromName(const std::string& boneName) const;
    
    // Get the dynamically calculated global matrix of a bone's parent (in model space)
    glm::mat4 getParentGlobalMatrix(int boneIndex) const;
    
    // DEPRECATED: Sync functions (kept for backward compatibility, will be removed)
    // Sync bone rotations from PropertyPanel (converts glm::vec3 to Matrix4f and stores in m_BoneExtraRotations)
    void syncBoneRotations(const std::map<std::string, glm::vec3>& boneRotations);
    
    // Sync bone translations from PropertyPanel (converts glm::vec3 to Matrix4f and stores in m_BoneExtraTranslations)
    void syncBoneTranslations(const std::map<std::string, glm::vec3>& boneTranslations);
    
    // Sync bone scales from PropertyPanel (converts glm::vec3 to Matrix4f and stores in m_BoneExtraScales)
    void syncBoneScales(const std::map<std::string, glm::vec3>& boneScales);
    
    // Clear all bone rotations (reset to default/identity)
    void clearAllBoneRotations();
    
    // Clear all bone translations (reset to default/identity)
    void clearAllBoneTranslations();
    
    // Clear all bone scales (reset to default/identity)
    void clearAllBoneScales();
    
    // Force the model to recalculate transforms (useful after resetting bones in background)
    void forceTransformsDirty() { m_transformsDirty = true; }
    
    // Get global transforms for all bones (indexed by bone index)
    // Returns a vector of bone global transforms (indexed by BoneIndex)
    const std::vector<BoneGlobalTransform>& getBoneGlobalTransforms() const { return m_BoneGlobalTransforms; }
    
    // Get bone's model-space matrix from linear skeleton (includes bind pose offset)
    // Returns the bone's actual calculated global matrix in model space, or identity if not found
    glm::mat4 getBoneModelSpaceMatrix(int boneIndex) const;
    
    // Update selection state (called when selection changes, not every frame)
    void updateBoneSelection(const std::string& selectedNodeName);
    
    // Get global transforms for bones only (for backward compatibility)
    // Returns a map of bone names to their global transforms (only actual bones, not intermediate nodes)
    std::map<std::string, BoneGlobalTransform> getBoneGlobalTransformsOnly() const;
    
    // Print bone global transforms to console (debug output)
    // Prints all bones with their global translation, rotation, and scale
    void printBoneGlobalTransforms() const;

protected:

    //-- true if doesn't have textures -----------
    bool noTex;

    //-- FPS of loade scene ---------------
    unsigned int mFPS;

    //-- Assimp stuff ------------------------
    const aiScene* mScene;
    Assimp::Importer mIporter;

    //-- list of meshes ----------
    std::vector<Mesh> meshes;

    //-- directory containing object file -------------
    std::string directory;

    //-- parse file extension from path --------
    std::string file_extension;

    //-- load list of textures ------------------
    std::vector<Texture> textures_loaded;


    //-- OpenFBX stuff  --------------------------------------
    //std::vector<std::vector<ofbx::Object*>>mainJointstHierarchy_list;
    //std::vector<ofbx::Object*> jointHierarchy_list;
    //std::vector<std::string> jointName_list;
    //std::map<std::int64_t, glm::mat4> jointBindPos_list;
    //unsigned int mJointIndex = 0;
    //ofbx::IScene* ofbx_scene = nullptr;
    std::vector<MeshEntry> m_Entries; //!< Array of mesh entries 

    //== loading Model data with (ASSIMP) =============
   //-- process node in object file -----------------
    void processNode(aiNode* node, const aiScene* scene);

    //-- process mesh in object file -----------------
    Mesh processMesh(const aiMesh* mesh, const aiScene* scene);

    //-- load list of textures ------------------
    std::vector<Texture> loadTextures(aiMaterial* mat, aiTextureType type);

    //-- processRig stuff -------------------------
    void processRig(const aiScene* mScene);

    void LoadBones(unsigned int meshIndex, const aiMesh* pMesh, std::vector<VertexBoneData>& bones);
    
    // Process OpenFBX SKIN/CLUSTER objects to calculate bone offset matrices
    // Uses the formula: Offset = Inverse(LinkMatrix) * TransformMatrix
    // This is more robust for non-Maya FBX files
    void processOpenFBXBones(const ofbx::IScene* fbxScene);

    unsigned int mNumBones; //-- Total number of bones in the model. 
    std::map<std::string, unsigned int> m_BoneMapping; //--- Map of bone names to ids (used during loading only)
    std::vector<BoneInfo> m_BoneInfo;               //-- Array containing bone information such as offset matrix and final transformation. 
    
    // Index-based storage for manual transforms (replaces string-based maps for performance)
    std::vector<Matrix4f> m_BoneExtraRotations;  // Indexed by bone index
    std::vector<Matrix4f> m_BoneExtraTranslations;  // Indexed by bone index
    std::vector<Matrix4f> m_BoneExtraScales;  // Indexed by bone index
    
    // Cache for fast node-to-bone index lookup (eliminates string operations in hot path)
    std::map<const aiNode*, int> m_NodeToBoneIndexCache;  // Maps aiNode* to bone index (-1 if not a bone)
    
    // Flat array to store global transforms for each bone (indexed by BoneIndex)
    // Replaces string-based map for O(1) access without string operations
    std::vector<BoneGlobalTransform> m_BoneGlobalTransforms;
    
    // Boolean array to track which bones are selected (updated only when selection changes)
    // Eliminates string matching inside updateLinearSkeleton loop
    std::vector<bool> m_isBoneSelected;
    
    // Dirty flags for caching (skip updateLinearSkeleton when nothing changed)
    float m_lastAnimationTime = -1.0f;
    UI_data m_lastUI_data;
    bool m_transformsDirty = true;
    
    // Linear skeleton structure (flattened hierarchy for future optimization)
    // Built during load time to prepare for linear loop optimization
    std::vector<LinearBone> m_linearSkeleton;
    
    // Cache for animation channels (maps node pointer to animation channel, eliminates string lookups)
    std::map<const aiNode*, const aiNodeAnim*> m_NodeToAnimChannelCache;
    
    // Cache for bone name to linear skeleton index (for getBoundingBoxForNode optimization)
    std::map<std::string, size_t> m_BoneNameToLinearIndexCache;

    Matrix4f GlobalTransformation; //!< Root node transformation. 
    Matrix4f m_GlobalInverseTransform;

    void CalcInterpolatedRotation(aiQuaternion& Out, float AnimationTime, const aiNodeAnim* pNodeAnim); //!< Calculates the interpolated quaternion between two keyframes. 
    void CalcInterpolatedScaling(aiVector3D& Out, float AnimationTime, const aiNodeAnim* pNodeAnim); //!< Calculates the interpolated scaling vector between two keyframes. 
    void CalcInterpolatedTranslation(aiVector3D& Out, float AnimationTime, const aiNodeAnim* pNodeAnim); //!< Calculates the interpolated translation vector between two keyframes.
    
    // Helper function to check if a node should be skipped (dummy nodes, cameras, etc.)
    bool shouldSkipNode(const std::string& nodeName); 

    unsigned int FindRotation(float AnimationTime, const aiNodeAnim* pNodeAnim); //!< Finds a rotation key given the current animation time. 
    unsigned int FindScale(float AnimationTime, const aiNodeAnim* pNodeAnim); // Finds a scaling key given the current animation time. 
    unsigned int FindTranslation(float AnimationTime, const aiNodeAnim* pNodeAnim); // Finds a translation key given the current animation time. 
    
    // Linear skeleton update (replaces recursive ReadNodeHierarchy for performance)
    void updateLinearSkeleton(float AnimationTime, UI_data ui_data);
    
    // Helper function to populate node-to-bone cache by traversing scene hierarchy
    void buildNodeToBoneCache(const aiNode* pNode);
    
    // Helper function to flatten hierarchy into linear structure (called during load time)
    void flattenHierarchy(const aiNode* pNode, int parentIdx);

    void changeAttributeManuale(Matrix4f& RotationM_attr, UI_data ui_data, std::string nodeNameInRig, int boneIndex = -1);
    void changeTranslationManuale(Matrix4f& TranslationM_attr, UI_data ui_data, std::string nodeNameInRig, int boneIndex = -1);
    void changeScaleManuale(Matrix4f& ScaleM_attr, UI_data ui_data, std::string nodeNameInRig, int boneIndex = -1);
    void Model::addExtraRotatin(Matrix4f& RotationM_attr, std::string nodeNameInRig);

    


};

#endif