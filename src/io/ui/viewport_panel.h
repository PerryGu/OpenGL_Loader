#ifndef VIEWPORT_PANEL_H
#define VIEWPORT_PANEL_H

// Forward declaration
class Scene;

class ViewportPanel {
public:
    ViewportPanel();
    
    // Render the viewport ImGui window
    void renderPanel(Scene* scene);
    
private:
    // Render simple FPS counter (called from renderPanel)
    void renderSimpleFPS(Scene* scene);
};

#endif // VIEWPORT_PANEL_H
