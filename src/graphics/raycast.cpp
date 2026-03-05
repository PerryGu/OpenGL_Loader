/**
 * @file raycast.cpp
 * @brief High-performance raycast utilities for viewport selection
 * 
 * This file implements optimized functions for:
 * - Converting mouse screen coordinates to world-space rays
 * - Testing ray-box intersections using the Kay-Kajiya slab method
 * - Calculating ray-to-line-segment distances for bone picking
 * 
 * **Performance Optimizations:**
 * - Pre-calculated inverse direction in Ray struct (avoids divisions in hot path)
 * - Kay-Kajiya slab method for efficient AABB intersection (O(1) complexity)
 * - No logging in mathematical functions (silent picking system)
 * 
 * **Kay-Kajiya Slab Method:**
 * The AABB intersection test uses the Kay-Kajiya algorithm, which treats the bounding
 * box as three pairs of parallel planes (slabs). For each axis, it calculates the
 * intersection interval with the box. The ray intersects the box if and only if all
 * three intervals overlap. This method is highly efficient (O(1) complexity) and
 * avoids the need for multiple plane intersection tests.
 * 
 * The raycast system enables clicking on models in the viewport to select them.
 */

#include "raycast.h"
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>
#include <limits>

namespace Raycast {

Ray screenToWorldRay(float mouseX, float mouseY, 
                    int viewportWidth, int viewportHeight,
                    const glm::mat4& viewMatrix, 
                    const glm::mat4& projectionMatrix) {
    // Convert mouse coordinates to normalized device coordinates (NDC)
    // NDC range: [-1, 1] for both X and Y
    float x = (2.0f * mouseX) / viewportWidth - 1.0f;
    float y = 1.0f - (2.0f * mouseY) / viewportHeight; // Flip Y axis (screen Y is top-down, NDC Y is bottom-up)
    
    // Create ray in clip space (homogeneous coordinates)
    glm::vec4 rayClip = glm::vec4(x, y, -1.0f, 1.0f);
    
    // Transform to eye space (camera space)
    glm::mat4 invProjection = glm::inverse(projectionMatrix);
    glm::vec4 rayEye = invProjection * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f); // Set z to -1 (pointing into screen) and w to 0 (direction vector)
    
    // Transform to world space
    glm::mat4 invView = glm::inverse(viewMatrix);
    glm::vec4 rayWorld = invView * rayEye;
    
    // Extract ray origin and direction, use constructor to pre-calculate inverse direction
    glm::vec3 rayOrigin = glm::vec3(invView[3]); // Camera position (translation part of view matrix)
    glm::vec3 rayDirection = glm::vec3(rayWorld);
    
    // Use Ray constructor which normalizes direction and pre-calculates invDir
    return Ray(rayOrigin, rayDirection);
}

bool rayIntersectsAABB(const Ray& ray, 
                       const glm::vec3& boxMin, 
                       const glm::vec3& boxMax,
                       float& tOut) {
    /**
     * Kay-Kajiya slab method for AABB intersection
     * 
     * This algorithm treats the bounding box as three pairs of parallel planes (slabs).
     * For each axis (X, Y, Z), it calculates the intersection interval with the box.
     * The ray intersects the box if and only if all three intervals overlap.
     * 
     * Performance: O(1) complexity - constant time regardless of box size
     * Optimization: Uses pre-calculated invDir to avoid divisions in hot path
     */
    
    float tMin = 0.0f;
    float tMax = std::numeric_limits<float>::max();
    
    // Test intersection with each axis-aligned plane (slab)
    for (int i = 0; i < 3; i++) {
        // Check if ray is parallel to this axis (using pre-calculated invDir)
        // If invDir is max (from constructor), direction component was near-zero
        if (std::abs(ray.invDir[i]) > 1e6f) {
            // Ray is parallel to this axis (direction component was near-zero, invDir set to max in constructor)
            // Check if origin is outside the box on this axis
            if (ray.origin[i] < boxMin[i] || ray.origin[i] > boxMax[i]) {
                return false; // Ray misses the box
            }
        } else {
            // Calculate intersection distances with min and max planes
            // CRITICAL PERFORMANCE FIX: Use pre-calculated invDir instead of division
            float t1 = (boxMin[i] - ray.origin[i]) * ray.invDir[i];
            float t2 = (boxMax[i] - ray.origin[i]) * ray.invDir[i];
            
            // Ensure t1 < t2 (swap if necessary)
            if (t1 > t2) {
                std::swap(t1, t2);
            }
            
            // Update intersection interval (slab intersection)
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            
            // If intervals don't overlap, ray misses the box
            if (tMin > tMax) {
                return false;
            }
        }
    }
    
    // If we get here, all three slabs overlap - ray intersects the box
    // Return the closest intersection point (tMin)
    tOut = tMin;
    return true;
}

bool rayIntersectsPlane(const Ray& ray,
                       const glm::vec3& planePoint,
                       const glm::vec3& planeNormal,
                       glm::vec3& intersectionOut) {
    // Ray-plane intersection formula:
    // t = dot(planePoint - ray.origin, planeNormal) / dot(ray.direction, planeNormal)
    // intersection = ray.origin + t * ray.direction
    
    float denominator = glm::dot(ray.direction, planeNormal);
    
    // Check if ray is parallel to plane
    if (std::abs(denominator) < 1e-6f) {
        return false;  // Ray is parallel to plane, no intersection
    }
    
    glm::vec3 toPlane = planePoint - ray.origin;
    float t = glm::dot(toPlane, planeNormal) / denominator;
    
    // Only return intersection if it's in front of the ray (t > 0)
    if (t < 0.0f) {
        return false;  // Intersection is behind the ray origin
    }
    
    intersectionOut = ray.origin + t * ray.direction;
    return true;
}

float rayToLineSegmentDistance(const Ray& ray,
                               const glm::vec3& segmentStart,
                               const glm::vec3& segmentEnd) {
    // Calculate the shortest distance between a ray and a line segment
    // Algorithm: Find the closest points on both the ray and the line segment,
    // then calculate the distance between those points
    
    glm::vec3 segmentDir = segmentEnd - segmentStart;
    float segmentLength = glm::length(segmentDir);
    
    // Handle degenerate case (zero-length segment)
    if (segmentLength < 1e-6f) {
        // Segment is a point - calculate distance from ray to point
        glm::vec3 toPoint = segmentStart - ray.origin;
        glm::vec3 projection = glm::dot(toPoint, ray.direction) * ray.direction;
        glm::vec3 perpendicular = toPoint - projection;
        return glm::length(perpendicular);
    }
    
    segmentDir = segmentDir / segmentLength; // Normalize segment direction
    
    // Calculate vectors for the distance calculation
    glm::vec3 w = ray.origin - segmentStart;
    float a = glm::dot(ray.direction, ray.direction); // Should be 1.0 if normalized
    float b = glm::dot(ray.direction, segmentDir);
    float c = glm::dot(segmentDir, segmentDir); // Should be 1.0 if normalized
    float d = glm::dot(ray.direction, w);
    float e = glm::dot(segmentDir, w);
    
    float denom = a * c - b * b; // Denominator for the system of equations
    
    // Handle parallel case (ray and segment are parallel)
    if (std::abs(denom) < 1e-6f) {
        // Ray and segment are parallel - find distance from ray to segment
        // Project segment start onto ray
        float tRay = glm::dot(segmentStart - ray.origin, ray.direction);
        glm::vec3 closestOnRay = ray.origin + tRay * ray.direction;
        
        // Find closest point on segment to the ray point
        float tSeg = glm::dot(closestOnRay - segmentStart, segmentDir);
        tSeg = glm::clamp(tSeg, 0.0f, segmentLength);
        glm::vec3 closestOnSegment = segmentStart + tSeg * segmentDir;
        
        return glm::length(closestOnRay - closestOnSegment);
    }
    
    // Calculate parameters for closest points
    float tRay = (b * e - c * d) / denom;
    float tSeg = (a * e - b * d) / denom;
    
    // Clamp tSeg to segment bounds [0, segmentLength]
    tSeg = glm::clamp(tSeg, 0.0f, segmentLength);
    
    // Calculate closest points
    glm::vec3 closestOnRay = ray.origin + tRay * ray.direction;
    glm::vec3 closestOnSegment = segmentStart + tSeg * segmentDir;
    
    // Return distance between closest points
    return glm::length(closestOnRay - closestOnSegment);
}

} // namespace Raycast
