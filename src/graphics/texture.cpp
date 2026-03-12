#include "texture.h"
#include "../utils/logger.h"
#include <string>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION   
#include <stb/stb_image.h>

// Helper function to determine color mode from channel count
static GLenum getColorModeFromChannels(int nChannels) {
    switch (nChannels) {
    case 1:
        return GL_RED;
    case 4:
        return GL_RGBA;
    default:
        return GL_RGB;
    }
}

//== constructor =====================
Texture::Texture() : id(0) {}

// Copy constructor - now safely copies preDecodedData (shared_ptr shares ownership)
Texture::Texture(const Texture& other)
    : id(other.id), type(other.type), dir(other.dir), path(other.path), 
      width(other.width), height(other.height), nCannnels(other.nCannnels),
      preDecodedData(other.preDecodedData) {  // Copy shared_ptr (safe - shares ownership)
    // preDecodedData is now a shared_ptr, so copying is safe and efficient
    // Multiple Texture objects can share the same image data
}

// Copy assignment - now safely copies preDecodedData (shared_ptr shares ownership)
Texture& Texture::operator=(const Texture& other) {
    if (this != &other) {
        id = other.id;
        type = other.type;
        dir = other.dir;
        path = other.path;
        width = other.width;
        height = other.height;
        nCannnels = other.nCannnels;
        preDecodedData = other.preDecodedData;  // Copy shared_ptr (safe - shares ownership)
    }
    return *this;
}


//-- initialize with image path and type ----------------------
Texture::Texture(std::string dir, std::string path, aiTextureType type, bool deferredSetup)
    : dir(dir), path(path), type(type), id(0) {
    // CRITICAL: Do NOT call generate() if deferring GPU resource creation
    // generate() contains OpenGL calls (glGenTextures) that MUST run on main thread
    if (!deferredSetup) {
        generate();
    }
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

// Decode image data in background thread (CPU-bound, no OpenGL calls)
// Returns true if image was successfully decoded, false otherwise
bool Texture::decodeImageData(PreDecodedImageData& preDecodedData, bool flip) {
    stbi_set_flip_vertically_on_load(flip);
    
    LOG_INFO("[Texture] Decoding image (background thread) - Directory: " + dir + ", File: " + path);
    
    // Build full absolute path for texture file
    std::string fullTexturePath = dir + "/" + path;
    
    // Decode image (CPU-bound operation)
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
            LOG_INFO("[Texture] Successfully decoded from textures/ subfolder: " + fallbackPath);
        }
    }
    
    if (data && width > 0 && height > 0) {
        // Store decoded data
        // Wrap the raw pointer in shared_ptr with custom deleter
        // This ensures the data survives vector copies and moves
        preDecodedData.data = std::shared_ptr<unsigned char>(data, StbiImageDeleter());
        preDecodedData.width = width;
        preDecodedData.height = height;
        preDecodedData.channels = nChannels;
        preDecodedData.colorMode = getColorModeFromChannels(nChannels);
        
        LOG_INFO("[Texture] Image decoded successfully - " + std::to_string(width) + "x" + std::to_string(height) + 
                 ", channels: " + std::to_string(nChannels) + ", data pointer: " + 
                 (preDecodedData.data ? "valid" : "null"));
        return true;
    } else {
        // Both primary and fallback paths failed
        if (!data) {
            LOG_ERROR("[Texture] Image decode failed or missing - Primary: " + fullTexturePath + ", Fallback: " + dir + "/textures/" + path);
        } else {
            LOG_ERROR("[Texture] Image decode failed - Invalid dimensions (width: " + std::to_string(width) + ", height: " + std::to_string(height) + ") - Path: " + fullTexturePath);
        }
        return false;
    }
}

// Upload pre-decoded image data to GPU (fast, main thread only)
// Uses pre-decoded image data from decodeImageData()
// Returns true if upload succeeded, false otherwise
// After calling this, preDecodedData.data will be freed
bool Texture::uploadToGPU(const PreDecodedImageData& preDecodedData) {
    if (!preDecodedData.isValid() || preDecodedData.width <= 0 || preDecodedData.height <= 0) {
        LOG_ERROR("[Texture] Cannot upload invalid image data to GPU - path: " + path + 
                  ", data valid: " + (preDecodedData.isValid() ? "yes" : "no") +
                  ", size: " + std::to_string(preDecodedData.width) + "x" + std::to_string(preDecodedData.height));
        return false;
    }
    
    if (id == 0) {
        LOG_ERROR("[Texture] Cannot upload to GPU: texture ID is 0 (generate() must be called first)");
        return false;
    }
    
    LOG_INFO("[Texture] Uploading image to GPU (main thread) - " + path + 
             ", data pointer: " + (preDecodedData.data ? "valid" : "null"));
    
    // Get raw pointer from shared_ptr for OpenGL call
    unsigned char* imageData = preDecodedData.get();
    if (!imageData) {
        LOG_ERROR("[Texture] Image data pointer is null in uploadToGPU - " + path);
        return false;
    }
    
    // Fast OpenGL calls only (no CPU-intensive decoding)
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, preDecodedData.colorMode, preDecodedData.width, preDecodedData.height, 
                  0, preDecodedData.colorMode, GL_UNSIGNED_BYTE, imageData);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Note: Data will be automatically freed when shared_ptr is destroyed
    // No need to call stbi_image_free manually - the custom deleter handles it
    
    return true;
}

// Move assignment is now default (shared_ptr handles it automatically)
// No custom implementation needed - shared_ptr manages ownership correctly
