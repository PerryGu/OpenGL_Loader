#ifndef DEBUG_PANEL_H
#define DEBUG_PANEL_H

#include <string>
#include <vector>

// Forward declarations
class ModelManager;
class PropertyPanel;
class Outliner;

/**
 * Debug Panel for displaying scene statistics and event log
 * 
 * Provides a floating window that shows:
 * - Total number of models in the scene
 * - Per-model statistics (name, polygon count, bone count, animation duration, FPS)
 * - Event log with auto-scrolling
 * - Benchmark tool for stress testing
 */
class DebugPanel {
public:
    DebugPanel();
    
    /**
     * Render the debug panel window
     * @param p_open Pointer to boolean controlling window visibility (can be NULL)
     * @param modelManager Pointer to ModelManager for accessing scene data
     * @param propertyPanel Pointer to PropertyPanel for setting RootNode transforms (for benchmark)
     * @param outliner Pointer to Outliner for getting RootNode names (for benchmark)
     */
    void renderPanel(bool* p_open, class ModelManager* modelManager, 
                     class PropertyPanel* propertyPanel = nullptr, class Outliner* outliner = nullptr);
    
private:
    // Log filter state is persisted in AppSettings (logger.showInfo, showWarning, showError)
};

#endif // DEBUG_PANEL_H
