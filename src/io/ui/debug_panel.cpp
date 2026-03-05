#include "debug_panel.h"
#include "../../graphics/model_manager.h"
#include "../../graphics/model.h"
#include "../../utils/logger.h"
#include <imgui/imgui.h>
#include <string>

DebugPanel::DebugPanel() {
    // Initialize with welcome message via Logger
    LOG_INFO("Debug Panel initialized");
}

void DebugPanel::renderPanel(bool* p_open, class ModelManager* modelManager) {
    if (!ImGui::Begin("Logger / Info", p_open)) {
        ImGui::End();
        return;
    }
    
    // Toggle button to switch between Info and Debug modes
    if (ImGui::Button(m_isInfoMode ? "Switch to Debug Log" : "Switch to Scene Info", ImVec2(-1, 0))) {
        m_isInfoMode = !m_isInfoMode;
    }
    
    ImGui::Separator();
    
    if (m_isInfoMode) {
        // Info Mode: Display scene statistics
        if (modelManager != nullptr) {
            size_t modelCount = modelManager->getModelCount();
            
            // Calculate total polygon count
            size_t totalPolygons = 0;
            for (size_t i = 0; i < modelCount; i++) {
                ModelInstance* instance = modelManager->getModel(static_cast<int>(i));
                if (instance != nullptr) {
                    totalPolygons += instance->model.getPolygonCount();
                }
            }
            
            // Display summary prominently
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Total Models: %zu", modelCount);
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Total Polygons: %zu", totalPolygons);
            ImGui::Spacing();
            
            // Display information for each model
            if (modelCount > 0) {
                ImGui::Text("Model Details:");
                ImGui::Separator();
                
                for (size_t i = 0; i < modelCount; i++) {
                    ModelInstance* instance = modelManager->getModel(static_cast<int>(i));
                    if (instance != nullptr) {
                        // Display Name
                        std::string displayName = instance->displayName;
                        if (displayName.empty()) {
                            displayName = "Model " + std::to_string(i);
                        }
                        ImGui::Text("Model %zu: %s", i, displayName.c_str());
                        
                        // Polygon Count
                        size_t polygonCount = instance->model.getPolygonCount();
                        ImGui::Text("  Polygons: %zu", polygonCount);
                        
                        // Animation Duration
                        float animDuration = instance->model.getAnimationDuration();
                        ImGui::Text("  Animation Duration: %.2f seconds", animDuration);
                        
                        // FPS
                        unsigned int fps = instance->FPS;
                        ImGui::Text("  FPS: %u", fps);
                        
                        ImGui::Spacing();
                    }
                }
            } else {
                ImGui::Text("No models loaded");
            }
        } else {
            ImGui::Text("ModelManager is null");
        }
    } else {
        // Debug Mode: Display event log from Logger singleton
        ImGui::Text("Event Log:");
        ImGui::Separator();
        
        // Clear Log button
        if (ImGui::Button("Clear")) {
            Logger::getInstance().clear();
        }
        ImGui::SameLine();
        
        // Filter checkboxes
        ImGui::Checkbox("Info (White)", &m_showInfo);
        ImGui::SameLine();
        ImGui::Checkbox("Warning (Yellow)", &m_showWarning);
        ImGui::SameLine();
        ImGui::Checkbox("Error (Red)", &m_showError);
        ImGui::Separator();
        
        // Create a child window for the log with scrolling
        ImGui::BeginChild("EventLog", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        
        // Retrieve logs from Logger singleton
        const std::vector<std::string>& logs = Logger::getInstance().getLogs();
        
        // Track if we should auto-scroll (if we were already at the bottom before rendering)
        bool shouldAutoScroll = (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f);
        
        // Display all log entries with color-coding and filtering
        for (size_t i = 0; i < logs.size(); ++i) {
            const auto& log = logs[i];
            
            // Parse log level from the string
            bool isError = log.find("[ERROR]") != std::string::npos;
            bool isWarning = log.find("[WARNING]") != std::string::npos;
            bool isInfo = !isError && !isWarning; // Default to Info if no specific tag
            
            // Apply filters
            if (isError && !m_showError) continue;
            if (isWarning && !m_showWarning) continue;
            if (isInfo && !m_showInfo) continue;
            
            // Set color based on log level
            ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // Default White
            if (isError) {
                color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); // Red
            } else if (isWarning) {
                color = ImVec4(1.0f, 1.0f, 0.4f, 1.0f); // Yellow
            }
            
            // Render as selectable for copy functionality
            ImGui::PushID(static_cast<int>(i)); // Use loop index for unique identifier
            ImGui::PushStyleColor(ImGuiCol_Text, color); // Keep existing color logic
            
            // Draw as a selectable row. false means it won't stay highlighted after clicking
            if (ImGui::Selectable(log.c_str(), false)) {
                ImGui::SetClipboardText(log.c_str());
            }
            ImGui::PopStyleColor();
            
            // Add a helpful tooltip for UX
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Click to copy to clipboard");
            }
            ImGui::PopID();
        }
        
        // Auto-scroll to bottom when new items are added
        // Only scroll if we were already at the bottom (user hasn't scrolled up)
        if (shouldAutoScroll) {
            ImGui::SetScrollHereY(1.0f);
        }
        
        ImGui::EndChild();
    }
    
    ImGui::End();
}
