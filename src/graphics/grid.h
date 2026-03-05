#ifndef GRID_H
#define GRID_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

class Shader;

/**
 * Grid class for rendering a 3D grid in the viewport
 * 
 * This class creates and renders a grid similar to those found in
 * professional 3D software (Blender, Maya, etc.). The grid is rendered
 * on the XZ plane (horizontal plane) at Y=0.
 * 
 * Features:
 * - Configurable size and spacing
 * - Major and minor grid lines with different colors
 * - Center lines (X and Z axes) with distinct color and thicker width
 *   (creates a visible cross at the grid center for easy orientation)
 * - Proper depth testing so models appear above the grid
 */
class Grid {
public:
    /**
     * Constructor
     * @param size Total size of the grid (extends from -size/2 to +size/2 in both X and Z)
     * @param spacing Distance between grid lines
     */
    Grid(float size = 20.0f, float spacing = 1.0f);
    
    /**
     * Destructor - cleans up OpenGL resources
     */
    ~Grid();
    
    /**
     * Initialize the grid (creates VAO, VBO, etc.)
     * Must be called after OpenGL context is created
     */
    void init();
    
    /**
     * Render the grid
     * @param shader Shader program to use for rendering
     * @param view View matrix (camera)
     * @param projection Projection matrix
     */
    void render(Shader& shader, const glm::mat4& view, const glm::mat4& projection);
    
    /**
     * Clean up OpenGL resources
     */
    void cleanup();
    
    // Configuration getters/setters
    void setSize(float size);
    float getSize() const { return mSize; }
    
    void setSpacing(float spacing);
    float getSpacing() const { return mSpacing; }
    
    void setMajorLineColor(const glm::vec3& color);
    glm::vec3 getMajorLineColor() const { return mMajorLineColor; }
    
    void setMinorLineColor(const glm::vec3& color);
    glm::vec3 getMinorLineColor() const { return mMinorLineColor; }
    
    void setCenterLineColor(const glm::vec3& color);
    glm::vec3 getCenterLineColor() const { return mCenterLineColor; }
    
    void setEnabled(bool enabled) { mEnabled = enabled; }
    bool isEnabled() const { return mEnabled; }

private:
    /**
     * Generate grid vertices
     * Creates vertices for all grid lines (major and minor lines, excluding center lines)
     */
    void generateVertices();
    
    /**
     * Generate center line vertices
     * Creates vertices for center lines (X and Z axes) as thick quads (rectangles)
     */
    void generateCenterLineVertices();
    
    float mSize;              // Total size of the grid
    float mSpacing;           // Spacing between grid lines
    glm::vec3 mMajorLineColor;   // Color for major grid lines (every 10th line)
    glm::vec3 mMinorLineColor;   // Color for minor grid lines
    glm::vec3 mCenterLineColor;  // Color for center lines (X and Z axes)
    bool mEnabled;            // Whether the grid is enabled/visible
    
    // OpenGL resources for regular grid lines
    unsigned int mVAO;        // Vertex Array Object
    unsigned int mVBO;        // Vertex Buffer Object
    size_t mVertexCount;      // Number of vertices to render
    
    // OpenGL resources for center lines (thick quads)
    unsigned int mCenterVAO;  // Vertex Array Object for center lines
    unsigned int mCenterVBO;  // Vertex Buffer Object for center lines
    size_t mCenterVertexCount; // Number of vertices for center lines
    
    std::vector<float> mVertices;  // Vertex data (x, y, z for each vertex)
    std::vector<float> mCenterVertices;  // Vertex data for center lines (as quads)
    
    float mCenterLineWidth;   // Width of center lines (thickness)
    
    bool mInitialized;        // Whether init() has been called
};

#endif // GRID_H
