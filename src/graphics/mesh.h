#ifndef MESH_H
#define MESH_H

//#include <glad/glad.h>
//#include <GLFW/glfw3.h>

#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

#include "shader.h"
#include "texture.h"


//== structure storing values for each vertex ============
struct Vertex {
    
    //== vertex values =======================
    //-- position -------
    glm::vec3 pos;
    //-- normal vector --------
    glm::vec3 normal;
    //-- texture coordinate ----
    glm::vec2 texCoord;
    //-- tangent vector ------
    //glm::vec3 tangent;

    //-- skeleton data ------------
    glm::vec4 boneIds = glm::vec4(0);
    glm::vec4 boneWeights = glm::vec4(0.0);


    //-- generate list of vertices -----------
    static std::vector<struct Vertex> genList(float* vertices, int noVertices);

    //-- calculate tangent vectors for each face --------
    //static void calcTanVectors(std::vector<Vertex>& list, std::vector<unsigned int>& indices);
};

typedef struct Vertex;



//== class representing Mesh ===================
class Mesh {
public:
    
    //-- list of vertices ----
    std::vector<Vertex> vertices;
    //-- list of indices -------
    std::vector<unsigned int> indices;
    //-- vertex array object pointing to all data for the mesh --------------
    unsigned int VAO;

    //-- texture list ------------
    std::vector<Texture> textures;
    //-- material diffuse value -------
    aiColor4D diffuse;
    //-- material specular value ------
    aiColor4D specular;

    
    //== constructors ===============
    //--  default ------------------
    Mesh();

    //-- intialize with a bounding region -----------
    Mesh(std::vector<Vertex> vertices, std::vector<unsigned int>& indices, std::vector<Texture> textures = {});
    Mesh(std::vector<Vertex> vertices, std::vector<unsigned int>& indices, aiColor4D diffuse, aiColor4D specular);

    //-- RAII: Move constructor (transfers GPU resource ownership) -----------
    Mesh(Mesh&& other) noexcept;

    //-- RAII: Move assignment operator (transfers GPU resource ownership) -----------
    Mesh& operator=(Mesh&& other) noexcept;

    //-- RAII: Destructor (releases GPU resources) -----------
    ~Mesh();



    //-- render number of instances using shader --------------
    // Marked const because render() only reads mesh data and doesn't modify the mesh state
    void render(Shader shader) const;

    //-- free up memory (legacy method, destructor handles cleanup automatically) ----------------
    void cleanup();

private:
    //-- true if has only materials --------------------------------
    bool noTex;

    unsigned int VBO, EBO;

    //-- setup data with buffers ---------------------------------
    void setup();
};



#endif