#include "grid.h"
#include "shader.h"
#include <iostream>
#include <cmath>

Grid::Grid(float size, float spacing)
    : mSize(size), mSpacing(spacing),
      mMajorLineColor(0.5f, 0.5f, 0.5f),      // Gray for major lines
      mMinorLineColor(0.3f, 0.3f, 0.3f),    // Darker gray for minor lines
      mCenterLineColor(0.2f, 0.2f, 0.2f),   // Darker gray for center lines (thick cross, more muted than grid)
      mEnabled(true),
      mVAO(0), mVBO(0), mVertexCount(0),
      mCenterVAO(0), mCenterVBO(0), mCenterVertexCount(0),
      mCenterLineWidth(0.15f),  // Thickness of center lines (15% of grid spacing for visible cross)
      mInitialized(false)
{
}

Grid::~Grid() {
    cleanup();
}

void Grid::init() {
    if (mInitialized) {
        std::cout << "[Grid] Warning: init() called multiple times" << std::endl;
        return;
    }
    
    // Generate vertices for regular grid lines
    generateVertices();
    
    // Create and bind VAO for regular grid lines
    glGenVertexArrays(1, &mVAO);
    glBindVertexArray(mVAO);
    
    // Create and bind VBO for regular grid lines
    glGenBuffers(1, &mVBO);
    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferData(GL_ARRAY_BUFFER, mVertices.size() * sizeof(float), mVertices.data(), GL_STATIC_DRAW);
    
    // Set vertex attributes (position only)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    // Generate center line vertices (as quads for thickness)
    generateCenterLineVertices();
    
    // Create and bind VAO for center lines
    glGenVertexArrays(1, &mCenterVAO);
    glBindVertexArray(mCenterVAO);
    
    // Create and bind VBO for center lines
    glGenBuffers(1, &mCenterVBO);
    glBindBuffer(GL_ARRAY_BUFFER, mCenterVBO);
    glBufferData(GL_ARRAY_BUFFER, mCenterVertices.size() * sizeof(float), mCenterVertices.data(), GL_STATIC_DRAW);
    
    // Set vertex attributes (position only)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    mInitialized = true;
    std::cout << "[Grid] Initialized with " << mVertexCount << " regular vertices and " 
              << mCenterVertexCount << " center line vertices" << std::endl;
}

void Grid::generateVertices() {
    mVertices.clear();
    
    float halfSize = mSize / 2.0f;
    int majorLineInterval = 10; // Every 10th line is a major line
    
    // Generate grid lines along X axis (lines parallel to Z axis)
    // Skip center line (Z=0) as it will be rendered separately as a thick quad
    for (float z = -halfSize; z <= halfSize + 0.001f; z += mSpacing) {
        int lineIndex = static_cast<int>(std::round(z / mSpacing));
        bool isCenterLine = (std::abs(z) < 0.001f);
        bool isMajorLine = (lineIndex % majorLineInterval == 0);
        
        // Skip center line - it's rendered separately as a thick quad
        if (isCenterLine) {
            continue;
        }
        
        // Start point
        mVertices.push_back(-halfSize);
        mVertices.push_back(0.0f);
        mVertices.push_back(z);
        
        // End point
        mVertices.push_back(halfSize);
        mVertices.push_back(0.0f);
        mVertices.push_back(z);
    }
    
    // Generate grid lines along Z axis (lines parallel to X axis)
    // Skip center line (X=0) as it will be rendered separately as a thick quad
    for (float x = -halfSize; x <= halfSize + 0.001f; x += mSpacing) {
        int lineIndex = static_cast<int>(std::round(x / mSpacing));
        bool isCenterLine = (std::abs(x) < 0.001f);
        bool isMajorLine = (lineIndex % majorLineInterval == 0);
        
        // Skip center line - it's rendered separately as a thick quad
        if (isCenterLine) {
            continue;
        }
        
        // Start point
        mVertices.push_back(x);
        mVertices.push_back(0.0f);
        mVertices.push_back(-halfSize);
        
        // End point
        mVertices.push_back(x);
        mVertices.push_back(0.0f);
        mVertices.push_back(halfSize);
    }
    
    mVertexCount = mVertices.size() / 3; // 3 floats per vertex
}

void Grid::generateCenterLineVertices() {
    mCenterVertices.clear();
    
    float halfSize = mSize / 2.0f;
    // Center line width is proportional to grid spacing for consistent appearance
    float centerLineWidth = mSpacing * mCenterLineWidth;  // Scale width by spacing
    float halfWidth = centerLineWidth / 2.0f;  // Half the thickness of center lines
    
    // Generate center line along X axis (Z=0) as a quad (thick rectangle)
    // This creates a thick horizontal line at the center
    // Quad vertices: bottom-left, bottom-right, top-right, top-left (two triangles)
    
    // Triangle 1: bottom-left, bottom-right, top-right
    mCenterVertices.push_back(-halfSize);  // x
    mCenterVertices.push_back(0.0f);       // y
    mCenterVertices.push_back(-halfWidth); // z (bottom edge)
    
    mCenterVertices.push_back(halfSize);   // x
    mCenterVertices.push_back(0.0f);       // y
    mCenterVertices.push_back(-halfWidth); // z (bottom edge)
    
    mCenterVertices.push_back(halfSize);   // x
    mCenterVertices.push_back(0.0f);       // y
    mCenterVertices.push_back(halfWidth);  // z (top edge)
    
    // Triangle 2: bottom-left, top-right, top-left
    mCenterVertices.push_back(-halfSize);  // x
    mCenterVertices.push_back(0.0f);       // y
    mCenterVertices.push_back(-halfWidth); // z (bottom edge)
    
    mCenterVertices.push_back(halfSize);   // x
    mCenterVertices.push_back(0.0f);       // y
    mCenterVertices.push_back(halfWidth);  // z (top edge)
    
    mCenterVertices.push_back(-halfSize);  // x
    mCenterVertices.push_back(0.0f);       // y
    mCenterVertices.push_back(halfWidth);  // z (top edge)
    
    // Generate center line along Z axis (X=0) as a quad (thick rectangle)
    // This creates a thick vertical line at the center
    // Quad vertices: bottom-left, bottom-right, top-right, top-left (two triangles)
    
    // Triangle 1: bottom-left, bottom-right, top-right
    mCenterVertices.push_back(-halfWidth); // x (left edge)
    mCenterVertices.push_back(0.0f);       // y
    mCenterVertices.push_back(-halfSize);  // z
    
    mCenterVertices.push_back(halfWidth);  // x (right edge)
    mCenterVertices.push_back(0.0f);       // y
    mCenterVertices.push_back(-halfSize);  // z
    
    mCenterVertices.push_back(halfWidth);  // x (right edge)
    mCenterVertices.push_back(0.0f);       // y
    mCenterVertices.push_back(halfSize);   // z
    
    // Triangle 2: bottom-left, top-right, top-left
    mCenterVertices.push_back(-halfWidth); // x (left edge)
    mCenterVertices.push_back(0.0f);       // y
    mCenterVertices.push_back(-halfSize);  // z
    
    mCenterVertices.push_back(halfWidth);  // x (right edge)
    mCenterVertices.push_back(0.0f);       // y
    mCenterVertices.push_back(halfSize);   // z
    
    mCenterVertices.push_back(-halfWidth); // x (left edge)
    mCenterVertices.push_back(0.0f);       // y
    mCenterVertices.push_back(halfSize);   // z
    
    mCenterVertexCount = mCenterVertices.size() / 3; // 3 floats per vertex
}

void Grid::render(Shader& shader, const glm::mat4& view, const glm::mat4& projection) {
    if (!mEnabled || !mInitialized) {
        return;
    }
    
    // Activate shader
    shader.activate();
    
    // Set matrices
    glm::mat4 model = glm::mat4(1.0f); // Identity matrix (grid is at origin)
    shader.setMat4("model", model);
    shader.setMat4("view", view);
    shader.setMat4("projection", projection);
    
    // Bind VAO
    glBindVertexArray(mVAO);
    
    // Enable depth testing (grid should be behind models)
    glEnable(GL_DEPTH_TEST);
    
    // Render all grid lines (excluding center lines which are rendered separately)
    // Since center lines are excluded from mVertices, we can render all vertices directly
    // Pass 1: Render all lines with minor color
    shader.set3Float("gridColor", mMinorLineColor);
    glDrawArrays(GL_LINES, 0, mVertexCount);
    
    // Pass 2: Render major lines (overwrite minor lines)
    // We need to iterate through the actual grid positions to find major lines
    shader.set3Float("gridColor", mMajorLineColor);
    float halfSize = mSize / 2.0f;
    int majorLineInterval = 10;
    int offset = 0;
    
    // Major lines along X axis (lines parallel to X, varying Z)
    for (float z = -halfSize; z <= halfSize + 0.001f; z += mSpacing) {
        int lineIndex = static_cast<int>(std::round(z / mSpacing));
        bool isCenterLine = (std::abs(z) < 0.001f);
        bool isMajorLine = (lineIndex % majorLineInterval == 0);
        
        // Skip center line - it's rendered separately as a thick quad
        if (!isCenterLine && isMajorLine) {
            glDrawArrays(GL_LINES, offset, 2);
        }
        
        // Only increment offset if this line was included in vertices (not center line)
        if (!isCenterLine) {
            offset += 2;
        }
    }
    
    // Major lines along Z axis (lines parallel to Z, varying X)
    for (float x = -halfSize; x <= halfSize + 0.001f; x += mSpacing) {
        int lineIndex = static_cast<int>(std::round(x / mSpacing));
        bool isCenterLine = (std::abs(x) < 0.001f);
        bool isMajorLine = (lineIndex % majorLineInterval == 0);
        
        // Skip center line - it's rendered separately as a thick quad
        if (!isCenterLine && isMajorLine) {
            glDrawArrays(GL_LINES, offset, 2);
        }
        
        // Only increment offset if this line was included in vertices (not center line)
        if (!isCenterLine) {
            offset += 2;
        }
    }
    
    // Pass 3: Render center lines as thick quads (overwrite everything)
    // Center lines are rendered as quads (rectangles) instead of lines for guaranteed thickness
    shader.set3Float("gridColor", mCenterLineColor);
    
    // Bind center lines VAO
    glBindVertexArray(mCenterVAO);
    
    // Render center lines as triangles (quads made of 2 triangles each)
    // First quad: center line along X axis (Z=0) - 6 vertices (2 triangles)
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    // Second quad: center line along Z axis (X=0) - 6 vertices (2 triangles)
    glDrawArrays(GL_TRIANGLES, 6, 6);
    
    // Unbind
    glBindVertexArray(0);
}

void Grid::cleanup() {
    if (mVAO != 0) {
        glDeleteVertexArrays(1, &mVAO);
        mVAO = 0;
    }
    if (mVBO != 0) {
        glDeleteBuffers(1, &mVBO);
        mVBO = 0;
    }
    if (mCenterVAO != 0) {
        glDeleteVertexArrays(1, &mCenterVAO);
        mCenterVAO = 0;
    }
    if (mCenterVBO != 0) {
        glDeleteBuffers(1, &mCenterVBO);
        mCenterVBO = 0;
    }
    mInitialized = false;
}

void Grid::setSize(float size) {
    if (size > 0.0f && size != mSize) {
        mSize = size;
        if (mInitialized) {
            // Regenerate vertices if already initialized
            generateVertices();
            glBindBuffer(GL_ARRAY_BUFFER, mVBO);
            glBufferData(GL_ARRAY_BUFFER, mVertices.size() * sizeof(float), mVertices.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            mVertexCount = mVertices.size() / 3;
            
            // Regenerate center line vertices
            generateCenterLineVertices();
            glBindBuffer(GL_ARRAY_BUFFER, mCenterVBO);
            glBufferData(GL_ARRAY_BUFFER, mCenterVertices.size() * sizeof(float), mCenterVertices.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            mCenterVertexCount = mCenterVertices.size() / 3;
        }
    }
}

void Grid::setSpacing(float spacing) {
    if (spacing > 0.0f && spacing != mSpacing) {
        mSpacing = spacing;
        if (mInitialized) {
            // Regenerate vertices if already initialized
            generateVertices();
            glBindBuffer(GL_ARRAY_BUFFER, mVBO);
            glBufferData(GL_ARRAY_BUFFER, mVertices.size() * sizeof(float), mVertices.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            mVertexCount = mVertices.size() / 3;
            
            // Regenerate center line vertices (width is based on spacing)
            generateCenterLineVertices();
            glBindBuffer(GL_ARRAY_BUFFER, mCenterVBO);
            glBufferData(GL_ARRAY_BUFFER, mCenterVertices.size() * sizeof(float), mCenterVertices.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            mCenterVertexCount = mCenterVertices.size() / 3;
        }
    }
}

void Grid::setMajorLineColor(const glm::vec3& color) {
    mMajorLineColor = color;
}

void Grid::setMinorLineColor(const glm::vec3& color) {
    mMinorLineColor = color;
}

void Grid::setCenterLineColor(const glm::vec3& color) {
    mCenterLineColor = color;
}

// Note: We need to store vertices as a member variable
// Let me add that to the header file
