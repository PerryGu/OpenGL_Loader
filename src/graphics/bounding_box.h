#ifndef BOUNDING_BOX_H
#define BOUNDING_BOX_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

class Shader;

/**
 * BoundingBox class for rendering wireframe bounding boxes in the viewport
 * 
 * This class creates and renders a wireframe box that represents the
 * bounding volume of a 3D object. The bounding box is rendered using
 * line primitives (GL_LINES) similar to the grid implementation.
 * 
 * Features:
 * - Configurable color (default: yellow)
 * - Automatic calculation from min/max points
 * - Efficient rendering using VAO/VBO
 * - Proper depth testing
 * - Real-time updates when model geometry changes
 * 
 * Usage:
 *   1. Create a BoundingBox instance with min/max corners
 *   2. Call init() after OpenGL context is created
 *   3. Call update() whenever the bounding box needs to change
 *   4. Call render() each frame to display the bounding box
 * 
 * The bounding box automatically updates its geometry when update() is called
 * with new min/max values, regenerating vertices and updating GPU buffers.
 */
class BoundingBox {
public:
    /**
     * Constructor
     * @param min Minimum corner of the bounding box (x, y, z)
     * @param max Maximum corner of the bounding box (x, y, z)
     * @param color Color of the bounding box lines (default: yellow)
     */
    BoundingBox(const glm::vec3& min = glm::vec3(-1.0f), 
               const glm::vec3& max = glm::vec3(1.0f),
               const glm::vec3& color = glm::vec3(1.0f, 1.0f, 0.0f));
    
    /**
     * Destructor - cleans up OpenGL resources
     */
    ~BoundingBox();
    
    /**
     * Initialize the bounding box (creates VAO, VBO, etc.)
     * Must be called after OpenGL context is created
     */
    void init();
    
    /**
     * Update the bounding box min/max values and regenerate vertices
     * @param min Minimum corner
     * @param max Maximum corner
     */
    void update(const glm::vec3& min, const glm::vec3& max);
    
    /**
     * Render the bounding box
     * 
     * Renders the bounding box with professional visual feedback:
     * - Selected models: Thicker lines (2.0f) for clear selection indication
     * - Unselected models: Standard lines (1.0f)
     * 
     * @param shader Shader program to use for rendering
     * @param view View matrix (camera)
     * @param projection Projection matrix
     * @param model Model matrix (for object transformation)
     * @param isSelected Whether this bounding box represents a selected model (affects line thickness)
     */
    void render(Shader& shader, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& model = glm::mat4(1.0f), bool isSelected = false);
    
    /**
     * Clean up OpenGL resources
     */
    void cleanup();
    
    // Configuration getters/setters
    void setColor(const glm::vec3& color) { mColor = color; }
    glm::vec3 getColor() const { return mColor; }
    
    void setMin(const glm::vec3& min) { 
        mMin = min; 
        if (mInitialized) {
            generateVertices();
            updateBuffers();
        }
    }
    glm::vec3 getMin() const { return mMin; }
    
    void setMax(const glm::vec3& max) { 
        mMax = max; 
        if (mInitialized) {
            generateVertices();
            updateBuffers();
        }
    }
    glm::vec3 getMax() const { return mMax; }
    
    void setEnabled(bool enabled) { mEnabled = enabled; }
    bool isEnabled() const { return mEnabled; }

private:
    /**
     * Generate vertices for the bounding box
     * Creates vertices for all 12 edges of the box
     */
    void generateVertices();
    
    /**
     * Update the VBO with new vertex data
     */
    void updateBuffers();
    
    glm::vec3 mMin;              // Minimum corner
    glm::vec3 mMax;              // Maximum corner
    glm::vec3 mColor;            // Color of the bounding box lines
    bool mEnabled;               // Whether the bounding box is enabled/visible
    
    // OpenGL resources
    unsigned int mVAO;           // Vertex Array Object
    unsigned int mVBO;           // Vertex Buffer Object
    size_t mVertexCount;         // Number of vertices to render (should be 24 for 12 edges)
    
    std::vector<float> mVertices;  // Vertex data (x, y, z for each vertex)
    
    bool mInitialized;           // Whether init() has been called
};

#endif // BOUNDING_BOX_H
