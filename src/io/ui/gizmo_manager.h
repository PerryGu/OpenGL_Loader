#ifndef GIZMO_MANAGER_H
#define GIZMO_MANAGER_H

#include <string>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <imgui/imgui.h>  // ImGui must be included before ImGuizmo
#include <ImGuizmo/ImGuizmo.h>

// Forward declarations
class ModelManager;
class Outliner;
class PropertyPanel;

class GizmoManager {
public:
    GizmoManager();
    
    // Handle keyboard shortcuts for gizmo (W/E/R for operation, +/- for size)
    void handleKeyboardShortcuts(ModelManager* modelManager, bool mouseOverViewport);
    
    // Render the gizmo and handle manipulation
    // Parameters:
    //   modelManager: Pointer to model manager for accessing models
    //   outliner: Reference to outliner for selection info
    //   propertyPanel: Reference to property panel for transform updates
    //   viewMatrix: View matrix for gizmo rendering
    //   projectionMatrix: Projection matrix for gizmo rendering
    //   viewportPos: Viewport window position
    //   viewportSize: Viewport window size
    //   cameraFramingRequested: Reference to camera framing request flag (will be set if needed)
    void renderGizmo(ModelManager* modelManager, Outliner& outliner, PropertyPanel& propertyPanel,
                     const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix,
                     ImVec2 viewportPos, ImVec2 viewportSize, bool& cameraFramingRequested);
    
    // Print gizmo transform values to debug window
    void printGizmoTransformValues(const float* modelMatrixArray, const std::string& nodeName, bool isRigRoot = false);
    
    // Getters for Scene to access gizmo state
    bool isGizmoUsing() const { return mGizmoUsing; }
    bool isGizmoOver() const { return mGizmoOver; }
    glm::vec3 getGizmoWorldPosition() const { return mGizmoWorldPosition; }
    bool isGizmoPositionValid() const { return mGizmoPositionValid; }
    
private:
    // Gizmo state tracking (for preventing selection conflicts)
    bool mGizmoUsing = false;  // True if gizmo is currently being manipulated
    bool mGizmoOver = false;   // True if mouse is over the gizmo
    bool mGizmoWasUsing = false;  // Previous frame's gizmo using state (for detecting manipulation end)
    
    // Reference-based delta transformation state (Static Reference Points)
    bool mGizmoInitialStateStored = false;  // True if initial state has been stored for current manipulation
    glm::vec3 mStartMouseWorldPos = glm::vec3(0.0f);  // Mouse ray intersection with gizmo plane at start (static reference)
    glm::vec3 mStartNodeLocalTranslation = glm::vec3(0.0f);  // Initial local translation of selected node (static reference)
    glm::mat4 mStartParentInverseMatrix = glm::mat4(1.0f);  // Parent's inverse world matrix at start (static reference)
    
    // Rotation reference state (Axis-Angle Projection)
    glm::quat mStartParentWorldQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Parent's world rotation quaternion (orthonormalized, captured ONCE at OnMouseDown)
    glm::quat mStartLocalQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Initial node local rotation (captured ONCE at OnMouseDown)
    glm::vec3 mWorldRotationAxis = glm::vec3(1.0f, 0.0f, 0.0f);  // Active rotation axis in world space (captured ONCE at OnMouseDown)
    glm::vec3 mLocalRotationAxis = glm::vec3(1.0f, 0.0f, 0.0f);  // Active rotation axis in local space (captured ONCE at OnMouseDown, normalized)
    glm::quat mInitialGizmoRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Initial gizmo world rotation (for angle delta calculation)
    
    // Current rotation state (Quaternion-only during drag)
    glm::quat mCurrentLocalRotationQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Current local rotation quaternion (updated during drag, no Euler conversion)
    
    std::string mInitialNodeName = "";  // Node name when manipulation started (to detect node changes)
    
    // Gizmo operation and size state
    ImGuizmo::OPERATION mGizmoOperation = ImGuizmo::TRANSLATE;  // Current gizmo operation (W/E/R)
    float mGizmoSize = 0.1f;  // Gizmo size in clip space (adjustable with +/-)
    
    // Gizmo world position (for camera aim constraint)
    glm::vec3 mGizmoWorldPosition = glm::vec3(0.0f);  // Current gizmo world position
    bool mGizmoPositionValid = false;  // True if gizmo position is valid (gizmo is visible)
};

#endif // GIZMO_MANAGER_H
