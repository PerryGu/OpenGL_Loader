#pragma once
#ifndef CAMERA_H
#define CAMERA_H

// CRITICAL: GLAD must be included before GLFW to prevent OpenGL header conflicts
#include <glad/glad.h>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

//== enum to represent directions for movement ============
enum class CameraDirection {
    NONE = 0,
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};


//== camera class to help display from POV of camera ===
class Camera {
public:
  
    //-- position ---
   glm::vec3 cameraPos;
   
   //-- camera directional values ----
   glm::vec3 cameraFront;
   glm::vec3 cameraUp;
   glm::vec3 cameraRight;
   
   glm::vec3 worldUp;
   
   //-- camera rotational values ----
   float yaw;   //-- x-axis ---
   float pitch; //-- y-axis ---
   
   //-- camera movement values ---
   float speed;
   float sensitivity;
   float zoom;
   
   //-- orbit camera values (for Maya-style rotation around focus point) ----
   glm::vec3 focusPoint;  // The point the camera orbits around (center of viewport)
   float orbitDistance;   // Distance from camera to focus point
   
    //-- smooth camera animation ----
    bool m_smoothCameraEnabled = false;  // True if smooth camera animation is enabled
    float smoothTransitionSpeed = 15.0f;  // Speed multiplier for smooth camera interpolation (higher = faster transition)
    
    //-- get zoom value for camera -----
    float getZoom();
    
    //-- smooth camera methods ----
    void setSmoothCameraEnabled(bool enabled) { m_smoothCameraEnabled = enabled; }
    bool isSmoothCameraEnabled() const { return m_smoothCameraEnabled; }
    void setSmoothTransitionSpeed(float speed) { smoothTransitionSpeed = speed; }
    float getSmoothTransitionSpeed() const { return smoothTransitionSpeed; }
    bool isAnimating() const { return m_isAnimating; }  // Check if camera is currently animating
    
    //-- request framing (called when F key is pressed or Auto-Focus is triggered) ----
    // Sets up framing data and trigger flag for Update() to process
    void requestFraming(glm::vec3 targetPos, glm::vec3 boundingBoxMin, glm::vec3 boundingBoxMax, float aspectRatio = 16.0f / 9.0f, float framingDistanceMultiplier = 1.5f);
    
    //-- update camera (called every frame) ----
    // Handles smooth camera animation if active and processes framing requests
    void Update(float deltaTime);
   
    
    //-- constructor ---------------------------
    //-- default and initialize with position ----
    Camera(glm::vec3 position = glm::vec3(0.0f));


    //-- modifiers --------------------------------
    //-- change camera direction (mouse movement) ----
    void updateCameraDirection(double dx, double dy);
    
    //-- set focus point for orbit rotation (center of viewport/object) ----
    void setFocusPoint(glm::vec3 focus);
    
    //-- get current focus point ----
    glm::vec3 getFocusPoint();
    
    //-- frame camera to focus on a bounding box (Maya-style framing) ----
    void frameBoundingBox(glm::vec3 min, glm::vec3 max);
    
    //-- restore camera state (after loading from settings) ----
    void restoreCameraState();

    //-- aim at target (rotation only, keeps camera position) ----
    // Forces camera to look at target position by updating orientation only
    // Does not modify camera.position, only updates forward/right/up vectors
    // Returns true if aim is complete (camera is looking at target)
    bool aimAtTarget(glm::vec3 targetPos);
    
    //-- aim at target with framing distance (rotation + distance adjustment) ----
    // Forces camera to look at target position AND move to optimal distance based on bounding box
    // Updates orientation and position to frame the object in view
    // Returns true if aim is complete (camera is looking at target and at correct distance)
    // framingDistanceMultiplier: Multiplier for framing distance (1.0 = tight, 10.0 = loose, default 1.5)
    bool aimAtTargetWithFraming(glm::vec3 targetPos, glm::vec3 boundingBoxMin, glm::vec3 boundingBoxMax, glm::vec3 modelScale = glm::vec3(1.0f), float aspectRatio = 16.0f / 9.0f, float framingDistanceMultiplier = 1.5f);
    
    //-- update framing distance only (distance-only update, no re-orientation) ----
    // Updates camera position along current view direction without changing yaw/pitch
    // Used during slider drag to smoothly dolly camera in/out without camera jumps
    // targetPos: Center of character's AABB (look-at point)
    // newDistance: New distance from target (calculated with new multiplier)
    void updateFramingDistanceOnly(glm::vec3 targetPos, float newDistance);
    
    //-- change camera position in certain direction (keyboard) ----
    void updateCameraPos(CameraDirection direction, double dt);

    //-- change camera zoom (scroll wheel) ----
    void updateCameraZoom(double dy);
    
    //-- pan camera (move left/right/up/down) ----
    void updateCameraPan(double dx, double dy);
    
    //-- dolly camera (move forward/backward) ----
    void updateCameraDolly(double dy);


     //-- accessors --------------
    //-- get view matrix for camera ----
    glm::mat4 getViewMatrix();



private:
 
    //-- private modifier -----
    //-- change camera directional vectors based on movement ---------
    void updateCameraVectors();
    
        //-- smooth camera animation state (private) ----
        bool m_shouldStartFraming = false;  // True when framing should be triggered (set by F key or Auto-Focus)
        bool m_isAnimating = false;  // True when camera is animating to framing target
        glm::vec3 m_startPos = glm::vec3(0.0f);  // Starting camera position (updated on each requestFraming call)
        // RE-TARGETABLE TARGETS: Can be updated mid-flight via requestFraming() for smooth re-targeting
        glm::vec3 m_frozenTargetPos = glm::vec3(0.0f);  // Target camera position (recalculated on each requestFraming call)
        float m_startYaw = 0.0f;  // Starting yaw (updated on each requestFraming call, uses current interpolated value)
        float m_frozenTargetYaw = 0.0f;  // Target yaw (recalculated on each requestFraming call, normalized for shortest path)
        float m_startPitch = 0.0f;  // Starting pitch (updated on each requestFraming call, uses current interpolated value)
        float m_frozenTargetPitch = 0.0f;  // Target pitch (recalculated on each requestFraming call, normalized for shortest path)
        float m_startDistance = 0.0f;  // Starting orbit distance (updated on each requestFraming call)
        float m_frozenTargetDistance = 0.0f;  // Target orbit distance (recalculated on each requestFraming call)
        glm::vec3 m_startFocusPoint = glm::vec3(0.0f);  // Starting focus point (updated on each requestFraming call)
        glm::vec3 m_frozenTargetFocusPoint = glm::vec3(0.0f);  // Target focus point (recalculated on each requestFraming call)
    
    //-- framing request data (stored when requestFraming is called) ----
    glm::vec3 m_framingTargetPos = glm::vec3(0.0f);
    glm::vec3 m_framingBoundingBoxMin = glm::vec3(0.0f);
    glm::vec3 m_framingBoundingBoxMax = glm::vec3(0.0f);
    float m_framingAspectRatio = 16.0f / 9.0f;
    float m_framingDistanceMultiplier = 1.5f;

};



#endif