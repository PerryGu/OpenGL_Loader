#ifndef RAYCAST_H
#define RAYCAST_H

#include <glm/glm.hpp>
#include <limits>
#include <cmath>

/**
 * Ray structure for raycasting
 * 
 * Optimized for performance with pre-calculated inverse direction.
 * The inverse direction is computed once during construction to avoid
 * repeated divisions in intersection tests (critical for AABB intersection).
 */
struct Ray {
    glm::vec3 origin;    // Ray origin point
    glm::vec3 direction; // Ray direction (normalized)
    glm::vec3 invDir;    // Pre-calculated inverse direction (1.0f / direction) for performance
    
    /**
     * Default constructor (creates invalid ray)
     */
    Ray() : origin(0.0f), direction(0.0f), invDir(0.0f) {}
    
    /**
     * Constructs a ray with normalized direction and pre-calculated inverse direction
     * 
     * @param orig Ray origin point
     * @param dir Ray direction (will be normalized automatically)
     */
    Ray(const glm::vec3& orig, const glm::vec3& dir) 
        : origin(orig) {
        direction = glm::normalize(dir);
        // Pre-calculate inverse direction to avoid divisions in intersection tests
        // Handle zero components to prevent division by zero
        invDir.x = (std::abs(direction.x) > 1e-6f) ? (1.0f / direction.x) : std::numeric_limits<float>::max();
        invDir.y = (std::abs(direction.y) > 1e-6f) ? (1.0f / direction.y) : std::numeric_limits<float>::max();
        invDir.z = (std::abs(direction.z) > 1e-6f) ? (1.0f / direction.z) : std::numeric_limits<float>::max();
    }
};

/**
 * Raycast utility functions for viewport selection
 * 
 * These functions help convert mouse screen coordinates to world-space rays
 * and perform intersection tests with bounding boxes.
 */
namespace Raycast {
    /**
     * Convert mouse screen coordinates to a world-space ray
     * 
     * @param mouseX Mouse X coordinate in screen space (0 to viewport width)
     * @param mouseY Mouse Y coordinate in screen space (0 to viewport height)
     * @param viewportWidth Width of the viewport in pixels
     * @param viewportHeight Height of the viewport in pixels
     * @param viewMatrix Camera view matrix
     * @param projectionMatrix Camera projection matrix
     * @return Ray in world space
     */
    Ray screenToWorldRay(float mouseX, float mouseY, 
                        int viewportWidth, int viewportHeight,
                        const glm::mat4& viewMatrix, 
                        const glm::mat4& projectionMatrix);
    
    /**
     * Test if a ray intersects with an axis-aligned bounding box
     * 
     * Uses the slab method (Kay-Kajiya algorithm) for efficient AABB intersection.
     * 
     * @param ray The ray to test
     * @param boxMin Minimum corner of the bounding box
     * @param boxMax Maximum corner of the bounding box
     * @param tOut Output parameter for the intersection distance (if intersection found)
     * @return true if ray intersects the box, false otherwise
     */
    bool rayIntersectsAABB(const Ray& ray, 
                           const glm::vec3& boxMin, 
                           const glm::vec3& boxMax,
                           float& tOut);
    
    /**
     * Calculate intersection of a ray with a plane
     * 
     * @param ray The ray to test
     * @param planePoint A point on the plane
     * @param planeNormal Normal vector of the plane (normalized)
     * @param intersectionOut Output parameter for the intersection point (if intersection found)
     * @return true if ray intersects the plane, false otherwise
     */
    bool rayIntersectsPlane(const Ray& ray,
                            const glm::vec3& planePoint,
                            const glm::vec3& planeNormal,
                            glm::vec3& intersectionOut);
    
    /**
     * Calculate the shortest distance between a ray and a 3D line segment
     * 
     * Uses the formula for distance between a ray and a line segment.
     * Returns the closest distance between the ray and the line segment.
     * 
     * @param ray The ray (origin and direction)
     * @param segmentStart Start point of the line segment
     * @param segmentEnd End point of the line segment
     * @return The shortest distance between the ray and the line segment
     */
    float rayToLineSegmentDistance(const Ray& ray,
                                   const glm::vec3& segmentStart,
                                   const glm::vec3& segmentEnd);
}

#endif // RAYCAST_H
