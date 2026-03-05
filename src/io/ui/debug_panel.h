#ifndef DEBUG_PANEL_H
#define DEBUG_PANEL_H

#include <string>
#include <vector>

// Forward declarations
class ModelManager;

/**
 * Debug Panel for displaying scene statistics and event log
 * 
 * Provides a floating window that shows:
 * - Total number of models in the scene
 * - Per-model statistics (name, polygon count, animation duration, FPS)
 * - Event log with auto-scrolling
 */
class DebugPanel {
public:
    DebugPanel();
    
    /**
     * Render the debug panel window
     * @param p_open Pointer to boolean controlling window visibility (can be NULL)
     * @param modelManager Pointer to ModelManager for accessing scene data
     */
    void renderPanel(bool* p_open, class ModelManager* modelManager);
    
private:
    // Display mode: true = Info mode (scene statistics), false = Debug mode (event log)
    bool m_isInfoMode = true;
    
    // Log filter toggles
    bool m_showInfo = true;
    bool m_showWarning = true;
    bool m_showError = true;
};

#endif // DEBUG_PANEL_H
