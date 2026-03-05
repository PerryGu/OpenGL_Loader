#include "camera.h"
#include "../utils/logger.h"
#include <cmath>  // For std::abs


//== constructor ====================================

//-- default and initialize with position -----------
Camera::Camera(glm::vec3 position)
    : cameraPos(position),
      worldUp(glm::vec3(0.0f, 1.0f, 0.0f)),
      yaw(-90.0f),
      pitch(0.0f),
      speed(2.5f),
      sensitivity(1.0f),
      zoom(45.0f),
      cameraFront(glm::vec3(1.0f, 0.0f, -1.0f)),
      focusPoint(glm::vec3(0.0f, 0.0f, 0.0f)),  // Default focus at origin
      orbitDistance(glm::length(position - focusPoint))  // Calculate initial distance
{
    updateCameraVectors();
    // Safe Guard: If orbitDistance is 0 or very small, force it to 0.1f to prevent division by zero
    // This prevents issues in projection matrices and camera calculations
    if (orbitDistance < 0.1f) {
        orbitDistance = 30.0f;  // Default distance for initialization
    }
}


//== modifiers ============================
//-- change camera direction (mouse movement) -----------
// For Maya-style orbit rotation, this rotates the camera around the focus point
void Camera::updateCameraDirection(double dx, double dy) {
    // CRITICAL: Disable mouse override during smooth camera animation
    // This prevents external LookAt constraints from overriding interpolated yaw/pitch
    if (m_isAnimating) {
        return;  // Animation is active - don't allow manual override
    }
    
    yaw += dx;
    pitch += dy;

    if (pitch > 89.0f) {
        pitch = 89.0f;
    }
    if (pitch < -89.0f) {
        pitch = -89.0f;
    }

    // Calculate direction vector from focus point to camera position
    // The direction from yaw/pitch gives us cameraFront (from camera to focus)
    // We need the opposite: direction from focus to camera
    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction = glm::normalize(direction);
    
    // Update camera position to orbit around focus point
    // direction is cameraFront (points from camera to focus)
    // So camera position = focusPoint - direction * distance
    cameraPos = focusPoint - direction * orbitDistance;
    
    // Update camera vectors with new position
    updateCameraVectors();
}

//-- change camera position in certain direction (keyboard) --------------
void Camera::updateCameraPos(CameraDirection direction, double dt) {
    float velocity = (float)dt * speed;

    switch (direction) {
    case CameraDirection::FORWARD:
        cameraPos += cameraFront * velocity;
        break;
    case CameraDirection::BACKWARD:
        cameraPos -= cameraFront * velocity;
        break;
    case CameraDirection::RIGHT:
        cameraPos += cameraRight * velocity;
        break;
    case CameraDirection::LEFT:
        cameraPos -= cameraRight * velocity;
        break;
    case CameraDirection::UP:
        cameraPos += cameraUp * velocity;
        break;
    case CameraDirection::DOWN:
        cameraPos -= cameraUp * velocity;
        break;
    }
}

//-- change camera zoom (scroll wheel) ------------
void Camera::updateCameraZoom(double dy) {
    // Clamp zoom (FOV) between 1.0f and 90.0f to prevent perspective inversion or distortion
    // 1.0f = very narrow FOV (zoomed in), 90.0f = wide FOV (zoomed out)
    zoom -= dy;
    
    // Clamp to valid range
    if (zoom < 1.0f) {
        zoom = 1.0f;
    }
    else if (zoom > 90.0f) {
        zoom = 90.0f;
    }
}

//-- pan camera (move left/right/up/down) ------------
// In Maya-style, panning moves the focus point, not just the camera
void Camera::updateCameraPan(double dx, double dy) {
    // Pan speed factor (adjust for desired sensitivity)
    // Scale by orbit distance so panning feels consistent at different zoom levels
    float panSpeed = 0.01f * (orbitDistance / 30.0f); // Scale based on distance
    
    // Update camera vectors to get current right/up directions
    updateCameraVectors();
    
    // Move focus point left/right (along cameraRight vector)
    focusPoint -= cameraRight * (float)dx * panSpeed;
    
    // Move focus point up/down (along cameraUp vector)
    focusPoint += cameraUp * (float)dy * panSpeed;
    
    // Recalculate camera position to maintain orbit distance
    // Direction from focus to camera (opposite of cameraFront)
    glm::vec3 direction = glm::normalize(cameraPos - focusPoint);
    cameraPos = focusPoint + direction * orbitDistance;
    
    // Update vectors again
    updateCameraVectors();
}

//-- dolly camera (move forward/backward) ------------
// In Maya-style, dollying changes the distance to focus point
void Camera::updateCameraDolly(double dy) {
    // Dolly speed factor (adjust for desired sensitivity)
    float dollySpeed = 0.1f;
    
    // Change orbit distance (negative dy moves closer, positive moves farther)
    orbitDistance -= (float)dy * dollySpeed;
    
    // Clamp distance to prevent going through focus or too far
    // Max distance set to 10,000.0 to match far clipping plane limit and maintain precision
    // Safe Guard: If orbitDistance is 0, force it to 0.1f to prevent division by zero
    if (orbitDistance < 0.1f) {
        orbitDistance = 0.1f;  // Minimum safe distance
    }
    if (orbitDistance > 10000.0f) {
        orbitDistance = 10000.0f;
    }
    
    // Recalculate camera position to maintain orbit distance
    // Direction from focus to camera (opposite of cameraFront)
    glm::vec3 direction = glm::normalize(cameraPos - focusPoint);
    cameraPos = focusPoint + direction * orbitDistance;
    
    // Update vectors
    updateCameraVectors();
}


//== accessors ============================
//-- get view matrix for camera -----------
// CRITICAL: During animation, we MUST use the camera's internal vectors (cameraFront, cameraRight, cameraUp)
// which are built from the interpolated yaw/pitch via updateCameraVectors().
// Using glm::lookAt(cameraPos, focusPoint, worldUp) would ignore the interpolated angles and cause snapping.
// Simplified to always use focusPoint - since focusPoint is now interpolated during animation,
// this ensures smooth transitions without switching logic at the end of animation
glm::mat4 Camera::getViewMatrix() {
    // Always use focusPoint - it's interpolated during animation and snapped at the end
    // This prevents the "snap" that happens when switching between different view matrix calculation methods
    // Use worldUp (0, 1, 0) for clean alignment, preventing any tilt
    return glm::lookAt(cameraPos, focusPoint, worldUp);
}

//-- get zoom value for camera ---------
float Camera::getZoom() {
    return zoom;
}

//-- set focus point for orbit rotation ----
void Camera::setFocusPoint(glm::vec3 focus) {
    focusPoint = focus;
    // Recalculate orbit distance
    orbitDistance = glm::length(cameraPos - focusPoint);
    // Safe Guard: If orbitDistance is 0, force it to 0.1f to prevent division by zero
    if (orbitDistance < 0.1f) {
        orbitDistance = 0.1f;  // Minimum safe distance
    }
    // Update camera position to maintain orbit
    // Direction from focus to camera (opposite of cameraFront)
    glm::vec3 direction = glm::normalize(cameraPos - focusPoint);
    cameraPos = focusPoint + direction * orbitDistance;
    updateCameraVectors();
}

//-- get current focus point ----
glm::vec3 Camera::getFocusPoint() {
    return focusPoint;
}

//-- restore camera state (after loading from settings) ----
void Camera::restoreCameraState() {
    // Recalculate camera position based on yaw/pitch and orbit distance
    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction = glm::normalize(direction);
    
    // Update camera position to orbit around focus point
    cameraPos = focusPoint - direction * orbitDistance;
    
    // Update camera vectors
    updateCameraVectors();
}

//-- aim at target (rotation only, keeps camera position) ----
// Forces camera to look at target position by updating orientation only
// Does not modify camera.position, only updates forward/right/up vectors
// Returns true if aim is complete (camera is looking at target)
bool Camera::aimAtTarget(glm::vec3 targetPos) {
    // CRITICAL: Disable aim override during smooth camera animation
    // This prevents external LookAt constraints from overriding interpolated yaw/pitch
    if (m_isAnimating) {
        return false;  // Animation is active - don't allow aim override
    }
    // 1. GET TARGET: Use the absolute World Position of the Gizmo
    // targetPos is already in world space (passed from Scene)
    
    // 2. CALCULATE FORWARD VECTOR: direction from camera to target
    glm::vec3 aimDir = glm::normalize(targetPos - cameraPos);
    
    // Check if target is too close (avoid division by zero and gimbal lock)
    float distance = glm::length(targetPos - cameraPos);
    if (distance < 0.001f) {
        return true;  // Target too close, consider complete
    }
    
    // Check if camera is already looking at target (dot product close to 1.0)
    // This allows smooth completion detection - check BEFORE updating
    float dotProduct = glm::dot(glm::normalize(cameraFront), aimDir);
    bool isAimed = (dotProduct > 0.999f);  // Very close to looking at target (cosine of ~2.5 degrees)
    
    // 3. REBUILD BASIS (STAY IN PLACE): Update orientation while keeping camera.position static
    // 1. EXTRACT ANGLES FROM AIM: Calculate new Yaw and Pitch from the aimDir vector
    // This ensures the camera's internal Euler angles match the new aim direction
    // Formula: pitch = asin(aimDir.y), yaw = atan2(aimDir.x, aimDir.z)
    pitch = glm::degrees(asin(aimDir.y));
    yaw = glm::degrees(atan2(aimDir.x, aimDir.z));
    
    // Clamp pitch to prevent gimbal lock
    if (pitch > 89.0f) {
        pitch = 89.0f;
    }
    if (pitch < -89.0f) {
        pitch = -89.0f;
    }
    
    // 2. UPDATE FOCUS POINT: Set the camera's internal orbit target to be EXACTLY the Gizmo World Position
    // This ensures the focus point matches the target, preventing jumps when transitioning to manual rotation
    focusPoint = targetPos;
    
    // Update orbit distance to match the actual distance to target
    orbitDistance = glm::length(targetPos - cameraPos);
    // Safe Guard: If orbitDistance is 0, force it to 0.1f to prevent division by zero
    if (orbitDistance < 0.1f) {
        orbitDistance = 0.1f;  // Minimum safe distance
    }
    
    // 3. FORCE STATE PERSISTENCE: Recalculate camera position from updated yaw/pitch and focus point
    // This ensures the camera position is consistent with the new angles and focus point
    // The manual mouse-orbit logic uses yaw/pitch/focusPoint to calculate camera position,
    // so we must ensure they are all synchronized
    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction = glm::normalize(direction);
    
    // Update camera position to orbit around focus point using the new direction
    // direction is cameraFront (points from camera to focus)
    // So camera position = focusPoint - direction * distance
    // NOTE: We keep the camera position mostly the same (rotation only), but adjust slightly
    // to ensure perfect alignment with the new yaw/pitch/focusPoint state
    glm::vec3 newCameraPos = focusPoint - direction * orbitDistance;
    
    // Only update camera position if it's significantly different (to maintain "rotation only" behavior)
    // But ensure consistency for state persistence
    float posDiff = glm::length(newCameraPos - cameraPos);
    if (posDiff > 0.01f) {
        // Position needs adjustment for consistency
        cameraPos = newCameraPos;
    }
    
    // Update camera vectors to match the calculated direction
    cameraFront = direction;
    
    // Right vector: cross product of forward and world up
    // Handle edge case where forward is parallel to world up
    cameraRight = glm::normalize(glm::cross(cameraFront, worldUp));
    if (glm::length(cameraRight) < 0.001f) {
        // Camera is looking straight up or down, use default right vector
        cameraRight = glm::vec3(1.0f, 0.0f, 0.0f);
        // Recalculate forward to be orthogonal
        cameraFront = glm::normalize(glm::cross(worldUp, cameraRight));
    }
    
    // Up vector: cross product of right and forward (ensures orthonormal basis)
    cameraUp = glm::normalize(glm::cross(cameraRight, cameraFront));
    
    // 4. CLEAN TERMINATION: All internal state is now synchronized:
    // - yaw and pitch match the aim direction
    // - focusPoint is set to gizmo world position
    // - camera position is consistent with yaw/pitch/focusPoint
    // - camera vectors (cameraFront, cameraRight, cameraUp) are consistent
    // The manual mouse-orbit logic will use these values as its starting point
    
    // 4. UPDATE: Camera orientation is now updated, position remains unchanged
    // Return true if already aimed (for smooth completion), false if still aiming
    return isAimed;
}

//-- aim at target with framing distance (rotation + distance adjustment) ----
// Forces camera to look at target position AND move to optimal distance based on bounding box
// Updates orientation and position to frame the object in view
// Returns true if aim is complete (camera is looking at target and at correct distance)
// framingDistanceMultiplier: Multiplier for framing distance (1.0 = tight, 10.0 = loose, default 1.5)
bool Camera::aimAtTargetWithFraming(glm::vec3 targetPos, glm::vec3 boundingBoxMin, glm::vec3 boundingBoxMax, glm::vec3 modelScale, float aspectRatio, float framingDistanceMultiplier) {
    // 1. CALCULATE VISUAL CENTER: Instead of aiming at the Gizmo position (feet), calculate the center of the World-Space Bounding Box
    // This centers the character vertically in the viewport, avoiding the "floating" look
    glm::vec3 lookAtTarget = (boundingBoxMin + boundingBoxMax) * 0.5f;
    
    // 1. USE WORLD-SPACE AABB DIRECTLY: The bounding box is already in world space (includes position and scale)
    // Get the World-Space Bounding Box dimensions directly
    float worldHeight = boundingBoxMax.y - boundingBoxMin.y;
    float worldWidth = boundingBoxMax.x - boundingBoxMin.x;
    float worldDepth = boundingBoxMax.z - boundingBoxMin.z;
    
    // Use the maximum dimension to ensure the entire object fits
    float objectSize = glm::max(glm::max(worldHeight, worldWidth), worldDepth);
    
    // NOTE: Do NOT multiply by modelScale again - the World-Space AABB already accounts for it
    
    // 2. CALCULATE DISTANCE FROM SIZE: Use the FOV to find the static distance
    // Since the camera now points at the center of the AABB, use standard framing formula
    float fovRadians = glm::radians(zoom);  // zoom is the FOV in degrees
    float baseDist = (objectSize / 2.0f) / tan(fovRadians / 2.0f);
    
    // Aspect Ratio Awareness: Adjust distance based on viewport aspect ratio
    // If viewport is narrow (portrait, aspectRatio < 1.0), camera needs to be further back
    // If viewport is wide (landscape, aspectRatio > 1.0), camera can be closer
    // Formula: distanceMultiplier = max(1.0, 1.0 / aspectRatio) for portrait, 1.0 for landscape
    float aspectRatioMultiplier = 1.0f;
    if (aspectRatio < 1.0f) {
        // Portrait mode: Need more distance to fit the object vertically
        aspectRatioMultiplier = 1.0f / aspectRatio;
    }
    // For landscape (aspectRatio >= 1.0), use horizontal FOV which is naturally wider, so multiplier stays 1.0
    
    // 3. APPLY FRAMING DISTANCE MULTIPLIER: Use configurable multiplier from settings
    // Default was 1.15f (15% padding), now uses user-configurable multiplier (default 1.5f)
    float finalDist = baseDist * aspectRatioMultiplier * framingDistanceMultiplier;
    
    // Clamp distance to reasonable values
    // Max distance set to 10,000.0 to match far clipping plane limit and maintain precision
    if (finalDist < 0.1f) {
        finalDist = 0.1f;
    }
    if (finalDist > 10000.0f) {
        finalDist = 10000.0f;
    }
    
    // 4. CALCULATE TARGET POSITION AND ORIENTATION
    // Calculate direction from current camera position to lookAtTarget
    glm::vec3 aimDirection = glm::normalize(lookAtTarget - cameraPos);
    
    // Calculate target yaw and pitch from this direction
    float targetPitch = glm::degrees(asin(aimDirection.y));
    float targetYaw = glm::degrees(atan2(aimDirection.x, aimDirection.z));
    
    // Clamp target pitch to prevent gimbal lock
    if (targetPitch > 89.0f) targetPitch = 89.0f;
    if (targetPitch < -89.0f) targetPitch = -89.0f;
    
    // Calculate target position: from lookAtTarget, move back by finalDist along the aim direction
    // The direction vector points from camera to target, so we use it directly
    glm::vec3 targetPosition = lookAtTarget - aimDirection * finalDist;
    
    // 5. CHECK IF SMOOTH CAMERA IS ENABLED
    if (m_smoothCameraEnabled) {
        // FROZEN TARGETS: Calculate ONCE and freeze - never recalculated during animation
        // This ensures stable animation path without erratic rotation or final snap
        m_frozenTargetPos = targetPosition;
        m_frozenTargetDistance = finalDist;
        
        // CRITICAL: Shortest path rotation (angle wrapping) - normalize to [-180, 180] range
        // This ensures interpolation takes the shortest route and prevents spinning the "long way around"
        float yawDiff = targetYaw - yaw;
        if (yawDiff > 180.0f) {
            targetYaw -= 360.0f;
        } else if (yawDiff < -180.0f) {
            targetYaw += 360.0f;
        }
        
        // Normalize target pitch to find shortest path (though pitch is clamped to [-89, 89])
        float pitchDiff = targetPitch - pitch;
        if (pitchDiff > 180.0f) {
            targetPitch -= 360.0f;
        } else if (pitchDiff < -180.0f) {
            targetPitch += 360.0f;
        }
        
        // FREEZE targets - these will NOT be recalculated until next 'F' press
        m_frozenTargetYaw = targetYaw;
        m_frozenTargetPitch = targetPitch;
        
        m_isAnimating = true;
        
        // Update focus point immediately (needed for camera state consistency)
        focusPoint = lookAtTarget;
        
        // Return false to indicate animation is in progress
        return false;
    } else {
        // IMMEDIATE SNAP: Update camera values directly
        // 2. UPDATE CAMERA AIM: Point the camera's forward vector at this lookAtTarget
        // This updates yaw, pitch, and cameraFront to aim at the center of the bounding box
        bool aimComplete = aimAtTarget(lookAtTarget);
        
        // 4. POSITION UPDATE: Use lookAtTarget (center of bounding box) for camera position
        // This ensures the character is centered vertically in the viewport
        glm::vec3 normalizedForward = glm::normalize(cameraFront);
        cameraPos = lookAtTarget - normalizedForward * finalDist;
        
        // 5. NO STATIC VARIABLES: Update orbit distance to match the new distance (fresh calculation, no previous frame data)
        orbitDistance = finalDist;
        // Safe Guard: If orbitDistance is 0, force it to 0.1f to prevent division by zero
        if (orbitDistance < 0.1f) {
            orbitDistance = 0.1f;  // Minimum safe distance
        }
        
        // 5. TERMINATION: All state is synchronized:
        // - yaw and pitch match the aim direction
        // - focusPoint is set to gizmo world position
        // - camera position is at ideal distance
        // - orbitDistance matches ideal distance
        // - camera vectors are consistent
        return aimComplete;  // Return true if aim was already complete (distance adjustment is immediate)
    }
}

//-- update framing distance only (distance-only update, no re-orientation) ----
// Updates camera position along current view direction without changing yaw/pitch
// Used during slider drag to smoothly dolly camera in/out without camera jumps
void Camera::updateFramingDistanceOnly(glm::vec3 targetPos, float newDistance) {
    // Get current view direction (from target to current camera position)
    glm::vec3 currentDir = glm::normalize(cameraPos - targetPos);
    
    // Update camera position along current view direction
    // This maintains the current yaw/pitch, only changing distance
    cameraPos = targetPos + currentDir * newDistance;
    
    // Update orbit distance to match
    orbitDistance = newDistance;
    // Safe Guard: If orbitDistance is 0, force it to 0.1f to prevent division by zero
    if (orbitDistance < 0.1f) {
        orbitDistance = 0.1f;  // Minimum safe distance
    }
    
    // Update focus point to target (maintains camera state consistency)
    focusPoint = targetPos;
    
    // Note: yaw, pitch, and camera vectors remain unchanged
    // This ensures smooth dolly in/out without camera orientation jumps
}

//-- request framing (called when F key is pressed or Auto-Focus is triggered) ----
// Sets up framing data and trigger flag for Update() to process
// CRITICAL: This function can be called even if the camera is already animating (m_isAnimating == true)
// When called mid-flight, it will smoothly re-target the camera to the new selection without interruption
void Camera::requestFraming(glm::vec3 targetPos, glm::vec3 boundingBoxMin, glm::vec3 boundingBoxMax, float aspectRatio, float framingDistanceMultiplier) {
    // Store framing request data (always updates, even if already animating)
    m_framingTargetPos = targetPos;
    m_framingBoundingBoxMin = boundingBoxMin;
    m_framingBoundingBoxMax = boundingBoxMax;
    m_framingAspectRatio = aspectRatio;
    m_framingDistanceMultiplier = framingDistanceMultiplier;
    
    // Set trigger flag - Update() will process this on next frame
    // NO GUARDS: This always sets the flag, allowing re-targeting even during active animation
    m_shouldStartFraming = true;
}

//-- update camera (called every frame) ----
// Handles smooth camera animation if active and processes framing requests
// CRITICAL: Framing requests are processed even if already animating, enabling smooth re-targeting
void Camera::Update(float deltaTime) {
    // 1. Check if framing should be started or re-targeted (trigger gate)
    // NO GUARDS: This processes framing requests even if m_isAnimating is already true
    // When re-targeting mid-flight, it uses current camera state as the new start point
    if (m_shouldStartFraming) {
        // CRITICAL: Use AABB center as the look-at target (not gizmo/feet position)
        // This ensures the camera aims at the visual center of the character, not the feet
        glm::vec3 lookAtTarget = (m_framingBoundingBoxMin + m_framingBoundingBoxMax) * 0.5f;
        
        // Get bounding box dimensions (used for distance calculation only)
        // The World-Space AABB already accounts for position and scale, so use dimensions directly
        float worldHeight = m_framingBoundingBoxMax.y - m_framingBoundingBoxMin.y;
        float worldWidth = m_framingBoundingBoxMax.x - m_framingBoundingBoxMin.x;
        float worldDepth = m_framingBoundingBoxMax.z - m_framingBoundingBoxMin.z;
        float objectSize = glm::max(glm::max(worldHeight, worldWidth), worldDepth);
        
        // CRITICAL: Do NOT multiply by model scale - World-Space AABB already accounts for scale
        // Calculate distance from size using FOV
        float fovRadians = glm::radians(zoom);
        float baseDist = (objectSize / 2.0f) / tan(fovRadians / 2.0f);
        
        // Aspect Ratio Awareness: Adjust distance based on viewport aspect ratio
        // If viewport is narrow (portrait, aspectRatio < 1.0), camera needs to be further back
        // If viewport is wide (landscape, aspectRatio > 1.0), camera can be closer
        // Formula: distanceMultiplier = max(1.0, 1.0 / aspectRatio) for portrait, 1.0 for landscape
        float aspectRatioMultiplier = 1.0f;
        if (m_framingAspectRatio < 1.0f) {
            // Portrait mode: Need more distance to fit the object vertically
            aspectRatioMultiplier = 1.0f / m_framingAspectRatio;
        }
        // For landscape (aspectRatio >= 1.0), use horizontal FOV which is naturally wider, so multiplier stays 1.0
        
        float finalDist = baseDist * aspectRatioMultiplier * m_framingDistanceMultiplier;
        
        // Clamp distance
        if (finalDist < 0.1f) finalDist = 0.1f;
        if (finalDist > 10000.0f) finalDist = 10000.0f;
        
        // Calculate target position: camera should be at finalDist from lookAtTarget
        // Direction from current camera to look-at target
        glm::vec3 aimDirection = glm::normalize(lookAtTarget - cameraPos);
        glm::vec3 targetPosition = lookAtTarget - aimDirection * finalDist;
        
        // CRITICAL: Epsilon check - if distance to new framing target is smaller than 0.001f, skip update
        // This prevents sub-pixel jitter and infinite loop when smooth camera is OFF
        float distanceToTarget = glm::length(targetPosition - cameraPos);
        if (distanceToTarget < 0.001f) {
            // Distance is too small, skip framing to prevent jitter
            m_shouldStartFraming = false;
            return;
        }
        
        // If Smooth is ON: capture start state, set animation targets and start animation
        // If Smooth is OFF: snap values immediately
        if (m_smoothCameraEnabled) {
            // RE-TARGETING SUPPORT: Capture CURRENT state as start state (even if already animating)
            // This allows smooth handover when a new selection is made mid-flight
            // The camera will smoothly curve from its current interpolated position to the new target
            m_startPos = cameraPos;  // Use current position (may be mid-animation)
            m_startYaw = yaw;  // Use current yaw (may be mid-animation)
            m_startPitch = pitch;  // Use current pitch (may be mid-animation)
            m_startDistance = orbitDistance;  // Use current distance (may be mid-animation)
            m_startFocusPoint = focusPoint;  // Use current focus point (may be mid-animation)
            
            // RE-TARGETABLE TARGETS: Recalculate targets based on NEW selection
            // If called while already animating, this updates the target to the new model's bounding box
            // The interpolation will smoothly continue from the current intermediate state to the new target
            m_frozenTargetPos = targetPosition;
            m_frozenTargetDistance = finalDist;
            m_frozenTargetFocusPoint = lookAtTarget;  // Store target focus point (don't update focusPoint immediately)
            
            // CRITICAL: Calculate target yaw/pitch from m_frozenTargetPos to lookAtTarget
            // This ensures the target angles match the target position exactly
            // Use atan2 and asin to get exact angles needed to look at the object
            // CRITICAL: Use atan2(z, x) to match updateCameraVectors coordinate system
            glm::vec3 targetAimDirection = glm::normalize(lookAtTarget - m_frozenTargetPos);
            float targetPitch = glm::degrees(asin(targetAimDirection.y));
            float targetYaw = glm::degrees(atan2(targetAimDirection.z, targetAimDirection.x));
            
            // Clamp target pitch to prevent gimbal lock
            if (targetPitch > 89.0f) targetPitch = 89.0f;
            if (targetPitch < -89.0f) targetPitch = -89.0f;
            
            // CRITICAL: Shortest path rotation (angle wrapping) - normalize to [-180, 180] range
            // This ensures interpolation takes the shortest route and prevents spinning the "long way around"
            // Use CURRENT yaw/pitch (may be mid-animation) for shortest path calculation
            float yawDiff = targetYaw - yaw;
            if (yawDiff > 180.0f) {
                targetYaw -= 360.0f;
            } else if (yawDiff < -180.0f) {
                targetYaw += 360.0f;
            }
            
            // Normalize target pitch to find shortest path (though pitch is clamped to [-89, 89])
            float pitchDiff = targetPitch - pitch;
            if (pitchDiff > 180.0f) {
                targetPitch -= 360.0f;
            } else if (pitchDiff < -180.0f) {
                targetPitch += 360.0f;
            }
            
            // Store updated targets (supports re-targeting mid-flight)
            m_frozenTargetYaw = targetYaw;
            m_frozenTargetPitch = targetPitch;
            
            // Start or continue animation (if already animating, this just updates the target)
            m_isAnimating = true;
        } else {
            // IMMEDIATE SNAP: Update camera values directly
            bool aimComplete = aimAtTarget(lookAtTarget);
            glm::vec3 normalizedForward = glm::normalize(cameraFront);
            cameraPos = lookAtTarget - normalizedForward * finalDist;
            orbitDistance = finalDist;
            // Safe Guard: If orbitDistance is 0, force it to 0.1f to prevent division by zero
            if (orbitDistance < 0.1f) {
                orbitDistance = 0.1f;  // Minimum safe distance
            }
            updateCameraVectors();
        }
        
        // ALWAYS clear trigger flag immediately
        m_shouldStartFraming = false;
    }
    
    // 2. Animation interpolation (only runs if animation is active)
    // CRITICAL: Exponential interpolation is frame-based (no timer/progress variable to reset)
    // When re-targeting mid-flight, the interpolation automatically continues from current state
    // because cameraPos, yaw, pitch are the current interpolated values, and targets are updated
    if (m_isAnimating) {
        // Use exponential interpolation for smooth feel
        // Same alpha factor for BOTH Position and Rotation
        // smoothTransitionSpeed controls the interpolation speed (higher = faster transition)
        // Alpha is recalculated each frame based on deltaTime - no progress variable to reset
        float alpha = 1.0f - expf(-smoothTransitionSpeed * deltaTime);
        
        // Interpolate to targets: Update Position, Rotation (Aim), and Focus Point simultaneously
        // MUST use glm::mix on yaw and pitch in parallel with position
        // RE-TARGETING: When targets are updated mid-flight via requestFraming(), the interpolation
        // smoothly continues from the current interpolated values (cameraPos, yaw, pitch) to the new targets
        cameraPos = glm::mix(cameraPos, m_frozenTargetPos, alpha);
        yaw = glm::mix(yaw, m_frozenTargetYaw, alpha);
        pitch = glm::mix(pitch, m_frozenTargetPitch, alpha);
        
        // CRITICAL: Interpolate focus point to prevent jump at end of animation
        // This ensures the camera's look-at target moves smoothly alongside position and angles
        focusPoint = glm::mix(focusPoint, m_frozenTargetFocusPoint, alpha);
        
        // Clamp pitch to prevent gimbal lock
        if (pitch > 89.0f) {
            pitch = 89.0f;
        }
        if (pitch < -89.0f) {
            pitch = -89.0f;
        }
        
        // Update orbit distance toward frozen target
        orbitDistance = glm::mix(orbitDistance, m_frozenTargetDistance, alpha);
        
        // CRITICAL: Force matrix rebuild IMMEDIATELY after mix calls
        // This is non-negotiable - converts new Yaw/Pitch into Forward/Right/Up vectors
        // MUST be called immediately after interpolation to ensure smooth Aim movement
        updateCameraVectors();
        
        // Stop condition: Only set m_isAnimating = false when both position AND angles have reached FROZEN targets
        // Precision Update: Use slightly larger epsilon (0.01f) to prevent camera "vibrating" when almost at target
        // This prevents the camera from oscillating between frames when it's very close but not exactly at the target
        const float epsilon = 0.01f;
        float posDiff = glm::length(cameraPos - m_frozenTargetPos);
        
        // Calculate angle differences with wrap-around handling (shortest path logic)
        float yawDiff = yaw - m_frozenTargetYaw;
        float pitchDiff = pitch - m_frozenTargetPitch;
        
        // Normalize angle differences to handle wrap-around (e.g., -179 to 179 degrees)
        // This ensures we're checking the shortest path difference
        if (yawDiff > 180.0f) yawDiff -= 360.0f;
        else if (yawDiff < -180.0f) yawDiff += 360.0f;
        
        if (pitchDiff > 180.0f) pitchDiff -= 360.0f;
        else if (pitchDiff < -180.0f) pitchDiff += 360.0f;
        
        float absYawDiff = std::abs(yawDiff);
        float absPitchDiff = std::abs(pitchDiff);
        float distDiff = std::abs(orbitDistance - m_frozenTargetDistance);
        
        // Calculate focus point difference
        float focusDiff = glm::length(focusPoint - m_frozenTargetFocusPoint);
        
        // Eliminate final snap: Ensure values are exactly at FROZEN targets before stopping
        if (posDiff < epsilon && absYawDiff < epsilon && absPitchDiff < epsilon && distDiff < epsilon && focusDiff < epsilon) {
            // CRITICAL: Snap to EXACT frozen target values to eliminate visible jump
            // This ensures no floating-point drift causes a visible snap
            cameraPos = m_frozenTargetPos;
            yaw = m_frozenTargetYaw;
            pitch = m_frozenTargetPitch;
            orbitDistance = m_frozenTargetDistance;
            focusPoint = m_frozenTargetFocusPoint;  // Snap focus point to exact target
            
            // Final synchronization: rebuild camera vectors from exact final angles
            updateCameraVectors();
            
            // Stop animation
            m_isAnimating = false;
        }
    }
}

//-- frame camera to focus on a bounding box (Maya-style framing) ----
void Camera::frameBoundingBox(glm::vec3 min, glm::vec3 max) {
    // Calculate center of bounding box
    glm::vec3 center = (min + max) * 0.5f;
    
    // Calculate size of bounding box
    glm::vec3 size = max - min;
    float maxSize = glm::max(glm::max(size.x, size.y), size.z);
    
    // Set focus point to center
    focusPoint = center;
    
    // Calculate appropriate distance to frame the object
    // Use a multiplier to ensure the object fits nicely in view
    float frameDistance = maxSize * 1.5f; // 1.5x multiplier for padding
    
    // Clamp distance to reasonable values
    // Max distance set to 10,000.0 to match far clipping plane limit and maintain precision
    if (frameDistance < 1.0f) {
        frameDistance = 1.0f;
    }
    if (frameDistance > 10000.0f) {
        frameDistance = 10000.0f;
    }
    
    orbitDistance = frameDistance;
    
    // Position camera to look at center from a good angle
    // Default view: slightly above and to the side
    yaw = -45.0f;  // 45 degrees to the right
    pitch = 30.0f; // 30 degrees up
    
    // Calculate camera position based on yaw/pitch and distance
    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction = glm::normalize(direction);
    
    cameraPos = focusPoint - direction * orbitDistance;
    
    // Update camera vectors
    updateCameraVectors();
}

//== private modifier =============================================
//-- change camera directional vectors based on movement ---------
void Camera::updateCameraVectors() {
    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(direction);

    //cameraRight = glm::normalize(glm::cross(cameraFront, Environment::worldUp));
    cameraRight = glm::normalize(glm::cross(cameraFront, worldUp));
    cameraUp = glm::normalize(glm::cross(cameraRight, cameraFront));
}