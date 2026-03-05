/**
 * @file bounding_box.cpp
 * @brief Implementation of the BoundingBox class for rendering wireframe bounding boxes
 * 
 * This file implements the BoundingBox class which provides functionality to:
 * - Calculate bounding box geometry from min/max corners
 * - Generate OpenGL vertex data for 12 edges of a 3D box
 * - Render bounding boxes efficiently using line primitives
 * - Update bounding box geometry in real-time
 * 
 * The bounding box is rendered as a wireframe box with 8 corners and 12 edges,
 * providing a visual representation of an object's spatial extent.
 */

#include "bounding_box.h"
#include "shader.h"
#include "../utils/logger.h"
#include <algorithm>

BoundingBox::BoundingBox(const glm::vec3& min, const glm::vec3& max, const glm::vec3& color)
    : mMin(min), mMax(max), mColor(color), mEnabled(true),
      mVAO(0), mVBO(0), mVertexCount(0), mInitialized(false)
{
}

BoundingBox::~BoundingBox() {
    cleanup();
}

void BoundingBox::init() {
    if (mInitialized) {
        LOG_WARNING("[BoundingBox] Warning: init() called multiple times");
        return;
    }
    
    // Generate vertices
    generateVertices();
    
    // Create and bind VAO
    glGenVertexArrays(1, &mVAO);
    glBindVertexArray(mVAO);
    
    // Create and bind VBO
    glGenBuffers(1, &mVBO);
    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferData(GL_ARRAY_BUFFER, mVertices.size() * sizeof(float), mVertices.data(), GL_STATIC_DRAW);
    
    // Set vertex attributes (position only)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    mInitialized = true;
    // Initialization complete - no logging for production build (v2.0.0)
}

/**
 * Generate vertices for the bounding box
 * 
 * Creates vertex data for all 12 edges of a 3D bounding box.
 * A bounding box has 8 corners connected by 12 edges:
 * - 4 edges on the bottom face
 * - 4 edges on the top face  
 * - 4 vertical edges connecting bottom to top
 * 
 * Each edge is represented by 2 vertices (start and end point),
 * resulting in 24 vertices total (12 edges * 2 vertices).
 */
void BoundingBox::generateVertices() {
    mVertices.clear();
    
    // Performance optimization: Reserve space for 24 vertices (12 edges * 2 vertices per edge)
    // This prevents multiple reallocations during geometry generation
    mVertices.reserve(24);
    
    // A bounding box has 8 corners and 12 edges
    // We'll define the 8 corners first
    glm::vec3 corners[8];
    
    // Bottom face (y = min.y)
    corners[0] = glm::vec3(mMin.x, mMin.y, mMin.z); // Front-left-bottom
    corners[1] = glm::vec3(mMax.x, mMin.y, mMin.z); // Front-right-bottom
    corners[2] = glm::vec3(mMax.x, mMin.y, mMax.z); // Back-right-bottom
    corners[3] = glm::vec3(mMin.x, mMin.y, mMax.z); // Back-left-bottom
    
    // Top face (y = max.y)
    corners[4] = glm::vec3(mMin.x, mMax.y, mMin.z); // Front-left-top
    corners[5] = glm::vec3(mMax.x, mMax.y, mMin.z); // Front-right-top
    corners[6] = glm::vec3(mMax.x, mMax.y, mMax.z); // Back-right-top
    corners[7] = glm::vec3(mMin.x, mMax.y, mMax.z); // Back-left-top
    
    // Now define the 12 edges as line segments (2 vertices per edge)
    // Bottom face edges (4 edges)
    // Edge 0: corners[0] -> corners[1] (front-bottom)
    mVertices.push_back(corners[0].x); mVertices.push_back(corners[0].y); mVertices.push_back(corners[0].z);
    mVertices.push_back(corners[1].x); mVertices.push_back(corners[1].y); mVertices.push_back(corners[1].z);
    
    // Edge 1: corners[1] -> corners[2] (right-bottom)
    mVertices.push_back(corners[1].x); mVertices.push_back(corners[1].y); mVertices.push_back(corners[1].z);
    mVertices.push_back(corners[2].x); mVertices.push_back(corners[2].y); mVertices.push_back(corners[2].z);
    
    // Edge 2: corners[2] -> corners[3] (back-bottom)
    mVertices.push_back(corners[2].x); mVertices.push_back(corners[2].y); mVertices.push_back(corners[2].z);
    mVertices.push_back(corners[3].x); mVertices.push_back(corners[3].y); mVertices.push_back(corners[3].z);
    
    // Edge 3: corners[3] -> corners[0] (left-bottom)
    mVertices.push_back(corners[3].x); mVertices.push_back(corners[3].y); mVertices.push_back(corners[3].z);
    mVertices.push_back(corners[0].x); mVertices.push_back(corners[0].y); mVertices.push_back(corners[0].z);
    
    // Top face edges (4 edges)
    // Edge 4: corners[4] -> corners[5] (front-top)
    mVertices.push_back(corners[4].x); mVertices.push_back(corners[4].y); mVertices.push_back(corners[4].z);
    mVertices.push_back(corners[5].x); mVertices.push_back(corners[5].y); mVertices.push_back(corners[5].z);
    
    // Edge 5: corners[5] -> corners[6] (right-top)
    mVertices.push_back(corners[5].x); mVertices.push_back(corners[5].y); mVertices.push_back(corners[5].z);
    mVertices.push_back(corners[6].x); mVertices.push_back(corners[6].y); mVertices.push_back(corners[6].z);
    
    // Edge 6: corners[6] -> corners[7] (back-top)
    mVertices.push_back(corners[6].x); mVertices.push_back(corners[6].y); mVertices.push_back(corners[6].z);
    mVertices.push_back(corners[7].x); mVertices.push_back(corners[7].y); mVertices.push_back(corners[7].z);
    
    // Edge 7: corners[7] -> corners[4] (left-top)
    mVertices.push_back(corners[7].x); mVertices.push_back(corners[7].y); mVertices.push_back(corners[7].z);
    mVertices.push_back(corners[4].x); mVertices.push_back(corners[4].y); mVertices.push_back(corners[4].z);
    
    // Vertical edges (4 edges)
    // Edge 8: corners[0] -> corners[4] (front-left)
    mVertices.push_back(corners[0].x); mVertices.push_back(corners[0].y); mVertices.push_back(corners[0].z);
    mVertices.push_back(corners[4].x); mVertices.push_back(corners[4].y); mVertices.push_back(corners[4].z);
    
    // Edge 9: corners[1] -> corners[5] (front-right)
    mVertices.push_back(corners[1].x); mVertices.push_back(corners[1].y); mVertices.push_back(corners[1].z);
    mVertices.push_back(corners[5].x); mVertices.push_back(corners[5].y); mVertices.push_back(corners[5].z);
    
    // Edge 10: corners[2] -> corners[6] (back-right)
    mVertices.push_back(corners[2].x); mVertices.push_back(corners[2].y); mVertices.push_back(corners[2].z);
    mVertices.push_back(corners[6].x); mVertices.push_back(corners[6].y); mVertices.push_back(corners[6].z);
    
    // Edge 11: corners[3] -> corners[7] (back-left)
    mVertices.push_back(corners[3].x); mVertices.push_back(corners[3].y); mVertices.push_back(corners[3].z);
    mVertices.push_back(corners[7].x); mVertices.push_back(corners[7].y); mVertices.push_back(corners[7].z);
    
    mVertexCount = mVertices.size() / 3; // 3 floats per vertex (should be 24 for 12 edges)
}

/**
 * Update the bounding box min/max values and regenerate geometry
 * 
 * This method updates the bounding box to reflect new spatial bounds.
 * It regenerates all vertices and updates GPU buffers if the bounding box
 * has been initialized. This is called every frame to update the bounding
 * box based on current model state (animation, deformation, etc.).
 * 
 * PERFORMANCE OPTIMIZATION: Only regenerates vertices if min/max actually changed,
 * avoiding unnecessary GPU buffer updates when the bounding box is static.
 * 
 * @param min New minimum corner of the bounding box
 * @param max New maximum corner of the bounding box
 */
void BoundingBox::update(const glm::vec3& min, const glm::vec3& max) {
    // Performance optimization: Only update if min/max actually changed
    const float epsilon = 0.0001f;
    bool minChanged = (glm::length(mMin - min) > epsilon);
    bool maxChanged = (glm::length(mMax - max) > epsilon);
    
    if (!minChanged && !maxChanged && mInitialized) {
        // Bounding box hasn't changed, skip regeneration
        return;
    }
    
    mMin = min;
    mMax = max;
    
    if (mInitialized) {
        generateVertices();
        updateBuffers();
    }
}

void BoundingBox::updateBuffers() {
    if (!mInitialized || mVAO == 0 || mVBO == 0) {
        return;
    }
    
    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferData(GL_ARRAY_BUFFER, mVertices.size() * sizeof(float), mVertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/**
 * Render the bounding box
 * 
 * Renders the bounding box as a wireframe using GL_LINES.
 * The bounding box is rendered with proper depth testing so it appears
 * correctly in 3D space relative to other objects.
 * 
 * Professional visual feedback:
 * - Selected models: Thicker lines (2.0f) and cyan color for clear selection indication
 * - Unselected models: Standard lines (1.0f) and yellow color
 * 
 * @param shader Shader program to use for rendering (should support "gridColor" uniform)
 * @param view View matrix (camera transformation)
 * @param projection Projection matrix (perspective/orthographic)
 * @param model Model matrix (object transformation, typically identity for bounding boxes)
 * @param isSelected Whether this bounding box represents a selected model (affects line thickness)
 */
void BoundingBox::render(Shader& shader, const glm::mat4& view, const glm::mat4& projection, const glm::mat4& model, bool isSelected) {
    // Only check initialization state - visibility is controlled by Model::m_showBoundingBox at the application level
    // This allows per-model bounding box visibility to work independently of global state
    if (!mInitialized) {
        return;
    }
    
    // Activate shader
    shader.activate();
    
    // Set matrices
    shader.setMat4("model", model);
    shader.setMat4("view", view);
    shader.setMat4("projection", projection);
    
    // Set color (already set by application.cpp based on selection state)
    shader.set3Float("gridColor", mColor);
    
    // Professional visual feedback: Thicker lines for selected models
    // This provides clear visual indication of selection in the viewport
    if (isSelected) {
        glLineWidth(2.0f);  // Thicker lines for selected models (professional editor style)
    } else {
        glLineWidth(1.0f);  // Standard line width for unselected models
    }
    
    // Bind VAO
    glBindVertexArray(mVAO);
    
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    
    // Render all bounding box edges
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(mVertexCount));
    
    // Unbind
    glBindVertexArray(0);
    
    // Reset line width to default (prevents affecting other rendering)
    glLineWidth(1.0f);
}

void BoundingBox::cleanup() {
    if (mVAO != 0) {
        glDeleteVertexArrays(1, &mVAO);
        mVAO = 0;
    }
    if (mVBO != 0) {
        glDeleteBuffers(1, &mVBO);
        mVBO = 0;
    }
    mInitialized = false;
}
