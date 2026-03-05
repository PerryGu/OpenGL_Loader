#ifndef VIEWPORT_SELECTION_H
#define VIEWPORT_SELECTION_H

#include <glm/glm.hpp>
#include <map>

// Forward declarations
class Camera;
class ModelManager;
class Scene;

class ViewportSelectionManager {
public:
    void handleSelectionClick(float viewportMouseX, float viewportMouseY, 
                              float viewportWidth, float viewportHeight,
                              Camera& camera, float farClipPlane,
                              ModelManager& modelManager, Scene& scene,
                              const std::map<int, glm::mat4>& modelRootNodeMatrices);
};

#endif // VIEWPORT_SELECTION_H
