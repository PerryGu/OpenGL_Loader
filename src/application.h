#ifndef APPLICATION_H
#define APPLICATION_H

// Forward declarations
class GLFWwindow;

// Include headers
#include "io/scene.h"
#include "graphics/model_manager.h"
#include "graphics/model.h"  // For UI_data
#include "io/camera.h"
#include "graphics/shader.h"
#include "graphics/grid.h"
#include "graphics/light.h"
#include "viewport_selection.h"
#include <vector>
#include <map>
#include <string>

class Application {
public:
    Application();
    ~Application();
    
    bool init();
    void run();
    void cleanup();
    
    // Drag and drop support
    void handleDroppedFile(const std::string& path);
    
    // Window resize callback (static for GLFW callback compatibility)
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    
private:
    void processInput(double deltaTime);
    
    // Helper methods for run() loop
    void updateTime();
    void updateCamera();
    void handleSceneEvents();
    void updateAnimations(double current_time, float interval);
    void syncTransforms();
    void renderFrame();
    
    // Helper to initialize a loaded model (bounding box, outliner, property panel, etc.)
    void initializeLoadedModel(int modelIndex, const std::string& filePath);
    
    // Systems
    GLFWwindow* m_window;
    Scene m_scene;
    ModelManager m_modelManager;
    ViewportSelectionManager m_selectionManager;
    
    // Rendering
    Camera m_cameras[2];
    unsigned int m_activeCam;
    Shader m_shader;
    Shader m_lampShader;
    Shader m_gridShader;
    Shader m_boundingBoxShader;
    Shader m_skeletonShader;
    Grid m_grid;
    DirLight m_dirLight;
    
    // Animation State
    double m_f_startTime;
    unsigned int m_FPS;
    float m_totalTime;
    float m_timeVal;
    bool m_wasPlaying;
    bool m_file_is_open;
    float deltaTime;
    float lastFrame;
    
    // Transforms
    std::map<int, glm::mat4> m_modelRootNodeMatrices;  // Per-model RootNode transform isolation
    std::vector<glm::mat4> m_Transforms;
    UI_data m_uiData;
};

#endif // APPLICATION_H
