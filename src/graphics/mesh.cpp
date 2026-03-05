/**
 * @file mesh.cpp
 * @brief Mesh rendering implementation with GPU resource management
 * 
 * This file implements the Mesh class which manages vertex data, index buffers (EBO),
 * and texture bindings for efficient OpenGL rendering. The vertex layout includes:
 * position (vec3), normal (vec3), texture coordinates (vec2), bone IDs (vec4), and bone weights (vec4).
 * 
 * Index Buffer Objects (EBO) are used to avoid vertex duplication, reducing memory usage
 * and improving rendering performance for complex meshes.
 */

#include "mesh.h"
#include "../utils/logger.h"


//-- generate list of vertices ------------------------
std::vector<Vertex> Vertex::genList(float* vertices, int noVertices) {
    std::vector<Vertex> ret(noVertices);

    int stride = sizeof(Vertex) / sizeof(float); //8;

    for (int i = 0; i < noVertices; i++) {
        ret[i].pos = glm::vec3(
            vertices[i * stride + 0],
            vertices[i * stride + 1],
            vertices[i * stride + 2]
        );

        ret[i].normal = glm::vec3(
            vertices[i * stride + 3],
            vertices[i * stride + 4],
            vertices[i * stride + 5]
        );

        ret[i].texCoord = glm::vec2(
            vertices[i * stride + 6],
            vertices[i * stride + 7]
        );
  
    }

    return ret;
}

//== constructors =====================================
Mesh::Mesh() : VAO(0), VBO(0), EBO(0), noTex(false) {}

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<unsigned int>& indices, std::vector<Texture> textures)
    : vertices(vertices), indices(indices), textures(textures), noTex(false){
    setup();
}

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<unsigned int>& indices, aiColor4D diffuse, aiColor4D specular)
    : vertices(vertices), indices(indices), diffuse(diffuse), specular(specular), noTex(true) {
    setup();
}

//-- RAII: Move constructor (transfers GPU resource ownership) -----------
Mesh::Mesh(Mesh&& other) noexcept
    : vertices(std::move(other.vertices)),
      indices(std::move(other.indices)),
      textures(std::move(other.textures)),
      VAO(other.VAO),
      VBO(other.VBO),
      EBO(other.EBO),
      diffuse(other.diffuse),
      specular(other.specular),
      noTex(other.noTex) {
    // Transfer ownership: set source mesh IDs to 0 to prevent accidental deletion
    other.VAO = 0;
    other.VBO = 0;
    other.EBO = 0;
}

//-- RAII: Move assignment operator (transfers GPU resource ownership) -----------
Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        // Clean up existing resources before transfer
        cleanup();
        
        // Transfer ownership
        vertices = std::move(other.vertices);
        indices = std::move(other.indices);
        textures = std::move(other.textures);
        VAO = other.VAO;
        VBO = other.VBO;
        EBO = other.EBO;
        diffuse = other.diffuse;
        specular = other.specular;
        noTex = other.noTex;
        
        // Set source mesh IDs to 0 to prevent accidental deletion
        other.VAO = 0;
        other.VBO = 0;
        other.EBO = 0;
    }
    return *this;
}

//-- RAII: Destructor (releases GPU resources) -----------
Mesh::~Mesh() {
    cleanup();
}

//-- render number of instances using shader --------------
// Marked const because render() only reads mesh data and doesn't modify the mesh state
void Mesh::render(Shader shader) const {
    // Check if mesh has at least one valid diffuse texture (with valid OpenGL ID)
    // This must be done before binding textures or VAO
    bool hasValidDiffuse = false;
    for (size_t i = 0; i < textures.size(); i++) {
        if (textures[i].type == aiTextureType_DIFFUSE && textures[i].id > 0) {
            hasValidDiffuse = true;
            break;
        }
    }
    shader.setInt("hasDiffuseTexture", hasValidDiffuse ? 1 : 0);
    
    if (noTex) {
        //-- materials ---------------
        shader.set4Float("material.diffuse", diffuse);
        shader.set4Float("material.specular", specular);
        shader.setInt("noTex", 1);
    }
    else {
        //-- get from textures -----------------------------
        // Performance optimization: Pre-defined texture names to avoid runtime allocations
        // Maximum 8 textures per mesh (reasonable limit for most models)
        static const char* diffuseNames[] = { "diffuse0", "diffuse1", "diffuse2", "diffuse3", "diffuse4", "diffuse5", "diffuse6", "diffuse7" };
        static const char* specularNames[] = { "specular0", "specular1", "specular2", "specular3", "specular4", "specular5", "specular6", "specular7" };
        
        unsigned int diffuseIdx = 0;
        unsigned int specularIdx = 0;

        for (unsigned int i = 0; i < textures.size(); i++) {
            //-- activate texture -------------------
            glActiveTexture(GL_TEXTURE0 + i);

            //-- retrieve texture info (using pre-defined names to avoid std::to_string allocations) -------------
            const char* name = nullptr;
            switch (textures[i].type) {
            case aiTextureType_DIFFUSE:
                if (diffuseIdx < 8) {
                    name = diffuseNames[diffuseIdx++];
                } else {
                    LOG_ERROR("[Mesh] Too many diffuse textures (max 8), skipping texture " + std::to_string(i));
                    continue;
                }
                break;
            case aiTextureType_SPECULAR:
                if (specularIdx < 8) {
                    name = specularNames[specularIdx++];
                } else {
                    LOG_ERROR("[Mesh] Too many specular textures (max 8), skipping texture " + std::to_string(i));
                    continue;
                }
                break;
            default:
                continue; // Skip unknown texture types
            }

            if (name) {
                //-- set the shader value ------------
                shader.setInt(name, i);
                //-- bind texture --------
                textures[i].bind();
            }
        }
    }

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glActiveTexture(GL_TEXTURE0);
    
}

//-- free up memory (legacy method, destructor handles cleanup automatically) ----------------
void Mesh::cleanup() {
    // Only delete if IDs are valid (prevents double-deletion after move)
    if (VAO != 0) {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
    if (VBO != 0) {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
    if (EBO != 0) {
        glDeleteBuffers(1, &EBO);
        EBO = 0;
    }
}



//-- setup data with buffers ---------------------------------
void Mesh::setup() {
    // Validate input data before creating GPU resources
    if (vertices.empty()) {
        LOG_ERROR("[Mesh] setup() called with empty vertex data");
        return;
    }
    if (indices.empty()) {
        LOG_ERROR("[Mesh] setup() called with empty index data");
        return;
    }

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    // Check for OpenGL errors during resource creation
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        LOG_ERROR("[Mesh] OpenGL error during buffer creation: " + std::to_string(error));
        cleanup();
        return;
    }

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

    //-- set vertex attribute pointers ------------- 
    //-- vertex position ------------------
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));

    //-- vertex normal ------------------
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    //-- vertex texCoord ------------------
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));

    //-- boneIds ------------------------
    // Note: Using glVertexAttribPointer (not glVertexAttribIPointer) because boneIds is stored as vec4(float)
    // If changed to int in the future, use glVertexAttribIPointer for better performance
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, boneIds));

    //-- boneWeights ------------------------
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, boneWeights));

    glBindVertexArray(0);
}


