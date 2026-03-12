#ifndef TEXTURE_H
#define TEXTURE_H

#include <iostream>
#include <glad/glad.h>
#include <assimp/scene.h>
#include <memory>

// Forward declaration for stbi_image_free
extern "C" void stbi_image_free(void* retval_from_stbi_load);

// Custom deleter for stbi_image_free
struct StbiImageDeleter {
    void operator()(unsigned char* ptr) const {
        if (ptr) {
            stbi_image_free(ptr);
        }
    }
};

// Structure to hold pre-decoded image data (loaded in background thread)
// Uses shared_ptr to ensure data survives vector copies and moves
struct PreDecodedImageData {
    std::shared_ptr<unsigned char> data;  // Shared pointer with custom deleter
    int width = 0;
    int height = 0;
    int channels = 0;
    GLenum colorMode = GL_RGB;
    
    // Default constructor
    PreDecodedImageData() : data(nullptr), width(0), height(0), channels(0), colorMode(GL_RGB) {}
    
    // Move constructor (default is fine with shared_ptr)
    PreDecodedImageData(PreDecodedImageData&& other) noexcept = default;
    
    // Move assignment (default is fine with shared_ptr)
    PreDecodedImageData& operator=(PreDecodedImageData&& other) noexcept = default;
    
    // Copy constructor (now safe with shared_ptr - shares ownership)
    PreDecodedImageData(const PreDecodedImageData& other) = default;
    
    // Copy assignment (now safe with shared_ptr - shares ownership)
    PreDecodedImageData& operator=(const PreDecodedImageData& other) = default;
    
    // Destructor (default is fine - shared_ptr handles cleanup)
    ~PreDecodedImageData() = default;
    
    // Helper to get raw pointer for OpenGL calls
    unsigned char* get() const { return data.get(); }
    
    // Check if data is valid
    bool isValid() const { return data != nullptr && data.get() != nullptr; }
};

//== class to represent texture ======================
class Texture {
public:

    //==  constructor ======================
    //-- initialize with name --------------
    Texture();

    //-- initialize with image path and type ----------------------------
    //Texture(const char* path, const char* name, bool defaultParams = true);
    Texture(std::string dir, std::string path, aiTextureType type, bool deferredSetup = false);

    //-- generate texture id ---------------------
    void generate();

    //-- load texture from path -------------------------
    // Returns true if texture loaded successfully, false otherwise
    bool load(bool flip = true);
    
    //-- decode image data in background thread (CPU-bound, no OpenGL calls) --------
    // Returns true if image was successfully decoded, false otherwise
    // The decoded data is stored in preDecodedData
    bool decodeImageData(PreDecodedImageData& preDecodedData, bool flip = true);
    
    //-- upload pre-decoded image data to GPU (fast, main thread only) --------
    // Uses pre-decoded image data from decodeImageData()
    // Returns true if upload succeeded, false otherwise
    // After calling this, preDecodedData.data will be freed
    bool uploadToGPU(const PreDecodedImageData& preDecodedData);

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
    
    // Pre-decoded image data (set by decodeImageData in background thread)
    // NOTE: Uses shared_ptr internally, so copying Texture is safe - data is shared, not duplicated
    PreDecodedImageData preDecodedData;
    
    // Copy constructor - safely copies preDecodedData (shared_ptr shares ownership)
    Texture(const Texture& other);
    
    // Copy assignment - safely copies preDecodedData (shared_ptr shares ownership)
    Texture& operator=(const Texture& other);


private:
    static int currentId;
    int width;
    int height;
    int nCannnels;
};

#endif