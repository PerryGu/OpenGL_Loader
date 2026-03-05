#include "texture.h"
#include "../utils/logger.h"
#include <string>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION   
#include <stb/stb_image.h>

//== constructor =====================
Texture::Texture() {}


//-- initialize with image path and type ----------------------
Texture::Texture(std::string dir, std::string path, aiTextureType type)
    : dir(dir), path(path), type(type) {
    generate();
}

//Texture::Texture(const char* path, const char* name, bool defaultParams)
//    : path(path), name(name), id(currentId){
//
//    if (defaultParams) {
//        setFilters(GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR);
//        setWrap(GL_REPEAT);
//    }
//}

//-- generate texture id (set the memory on GPU) --------------
void Texture::generate() {
    glGenTextures(1, &id);
}

//-- load texture from path ---------------------
// Returns true if texture loaded successfully, false otherwise
// Implements fallback logic: tries directory/textureFileName, then directory/textures/textureFileName
bool Texture::load(bool flip) {
    
    stbi_set_flip_vertically_on_load(flip);
   
    LOG_INFO("[Texture] Loading texture - Directory: " + dir + ", File: " + path);
    
    // Build full absolute path for texture file
    std::string fullTexturePath = dir + "/" + path;
    
    int width, height, nChannels;
    unsigned char* data = stbi_load(fullTexturePath.c_str(), &width, &height, &nChannels, 0);
    
    // Fallback: If file doesn't exist in base directory, try textures/ subfolder
    if (!data || width <= 0 || height <= 0) {
        std::string fallbackPath = dir + "/textures/" + path;
        LOG_INFO("[Texture] Primary path failed, trying fallback: " + fallbackPath);
        
        // Free any partial data from first attempt
        if (data) {
            stbi_image_free(data);
            data = nullptr;
        }
        
        // Try loading from textures/ subfolder
        data = stbi_load(fallbackPath.c_str(), &width, &height, &nChannels, 0);
        if (data && width > 0 && height > 0) {
            fullTexturePath = fallbackPath; // Update path for logging
            LOG_INFO("[Texture] Successfully loaded from textures/ subfolder: " + fallbackPath);
        }
    }
    
    GLenum colorMode = GL_RGB;
    switch (nChannels) {
    case 1:
        colorMode = GL_RED;
        break;
    case 4:
        colorMode = GL_RGBA;
        break;
    };

    if (data && width > 0 && height > 0) {
        // Ensure the texture is bound and allocated
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, colorMode, width, height, 0, colorMode, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        stbi_image_free(data);
        return true; // Success
    }
    else {
        // Both primary and fallback paths failed
        if (!data) {
            LOG_ERROR("[Texture] Texture failed to load or missing - Primary: " + fullTexturePath + ", Fallback: " + dir + "/textures/" + path);
        } else {
            LOG_ERROR("[Texture] Texture failed to load - Invalid dimensions (width: " + std::to_string(width) + ", height: " + std::to_string(height) + ") - Path: " + fullTexturePath);
        }
        
        // Delete the invalid texture ID since loading failed
        if (id > 0) {
            glDeleteTextures(1, &id);
            id = 0; // Mark as invalid
        }
        
        if (data) {
            stbi_image_free(data);
        }
        return false; // Failure
    }

}


/*
void Texture::setFilters(GLenum all) {
    setFilters(all, all);
}

void Texture::setFilters(GLenum mag, GLenum min) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min);
}

void Texture::setWrap(GLenum all) {
    setWrap(all, all);
}

void Texture::setWrap(GLenum s, GLenum t) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, s);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, t);
}
*/

// Marked const because bind() only calls OpenGL functions and doesn't modify the Texture object
void Texture::bind() const
{
    glBindTexture(GL_TEXTURE_2D, id);
}
