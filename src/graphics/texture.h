#ifndef TEXTURE_H
#define TEXTURE_H

#include <iostream>
#include <glad/glad.h>
#include <assimp/scene.h>



//== class to represent texture ======================
class Texture {
public:

    //==  constructor ======================
    //-- initialize with name --------------
    Texture();

    //-- initialize with image path and type ----------------------------
    //Texture(const char* path, const char* name, bool defaultParams = true);
    Texture(std::string dir, std::string path, aiTextureType type);

    //-- generate texture id ---------------------
    void generate();

    //-- load texture from path -------------------------
    // Returns true if texture loaded successfully, false otherwise
    bool load(bool flip = true);

    //void setFilters(GLenum all);
    //void setFilters(GLenum mag, GLenum min);
    //void setWrap(GLenum all);
    //void setWrap(GLenum s, GLenum t);

    // Marked const because bind() only calls OpenGL functions and doesn't modify the Texture object
    void bind() const;

    //== texture object values ===============
    //-- texture id -----------------------
    unsigned int id;
    //unsigned int tex;
    //const char* name;

    //-- texture type ---------
    aiTextureType type;

    //-- directory of image -------
    std::string dir;

    //-- name of image -----------
    std::string path;


private:
    static int currentId;
    int width;
    int height;
    int nCannnels;
};

#endif