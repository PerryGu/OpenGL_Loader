#include "debug_panel.h"
#include "../../graphics/model_manager.h"
#include "../../graphics/model.h"
#include "../../utils/logger.h"
#include "../app_settings.h"
#include "property_panel.h"
#include "outliner.h"
#include <imgui/imgui.h>
#include <string>
#include <vector>
#include <algorithm>  // For std::max, std::min
#include <cmath>      // For std::abs
#include <cfloat>     // For FLT_MAX
#include <glm/glm.hpp> // For glm::vec3, glm::mat4, glm::vec4

DebugPanel::DebugPanel() {
    // Initialize with welcome message via Logger
    LOG_INFO("Debug Panel initialized");
}

void DebugPanel::renderPanel(bool* p_open, class ModelManager* modelManager, 
                              class PropertyPanel* propertyPanel, class Outliner* outliner) {
    if (!ImGui::Begin("Logger / Info", p_open)) {
        ImGui::End();
        return;
    }
    
    // Use ImGui tabs instead of toggle button
    if (ImGui::BeginTabBar("DebugPanelTabs")) {
        // Event Log tab
        if (ImGui::BeginTabItem("Event Log")) {
            // Event Log content
            ImGui::Text("Event Log:");
            ImGui::Separator();
            
            // Clear Log button
            if (ImGui::Button("Clear")) {
                Logger::getInstance().clear();
            }
            ImGui::SameLine();
            
            // Filter checkboxes (persisted in app_settings.json)
            AppSettings& st = AppSettingsManager::getInstance().getSettings();
            if (ImGui::Checkbox("Info (White)", &st.logger.showInfo)) {
                AppSettingsManager::getInstance().saveSettings();
            }
            ImGui::SameLine();
            if (ImGui::Checkbox("Warning (Yellow)", &st.logger.showWarning)) {
                AppSettingsManager::getInstance().saveSettings();
            }
            ImGui::SameLine();
            if (ImGui::Checkbox("Error (Red)", &st.logger.showError)) {
                AppSettingsManager::getInstance().saveSettings();
            }
            ImGui::Separator();
            
            // Create a child window for the log with scrolling
            ImGui::BeginChild("EventLog", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            
            // Retrieve a copy of logs so iteration is safe if addLog() runs during load (avoids use-after-free crash)
            std::vector<std::string> logs = Logger::getInstance().getLogsCopy();
            
            // Track if we should auto-scroll (if we were already at the bottom before rendering)
            bool shouldAutoScroll = (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f);
            
            // Display all log entries with color-coding and filtering
            for (size_t i = 0; i < logs.size(); ++i) {
                const std::string& log = logs[i];
                
                // Parse log level from the string
                bool isError = log.find("[ERROR]") != std::string::npos;
                bool isWarning = log.find("[WARNING]") != std::string::npos;
                bool isInfo = !isError && !isWarning; // Default to Info if no specific tag
                
                // Apply filters (from app_settings)
                if (isError && !st.logger.showError) continue;
                if (isWarning && !st.logger.showWarning) continue;
                if (isInfo && !st.logger.showInfo) continue;
                
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
            
            ImGui::EndTabItem();
        }
        
        // Scene Info tab
        if (ImGui::BeginTabItem("Scene Info")) {
            // Scene Info content
            if (modelManager != nullptr) {
                size_t baseModelCount = modelManager->getModelCount();
                
                // Calculate total polygon count including benchmark instances
                size_t totalInstances = 0;
                size_t totalPolygons = 0;
                
                for (int i = 0; i < static_cast<int>(baseModelCount); ++i) {
                    ModelInstance* instance = modelManager->getModel(i);
                    if (instance) {
                        size_t basePolys = instance->model.getPolygonCount();
                        size_t instancesForThisModel = instance->benchmarkOffsets.size();
                        
                        totalInstances += instancesForThisModel;
                        totalPolygons += basePolys * (1 + instancesForThisModel);
                    }
                }
                
                // Display summary prominently
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Total Base Models: %zu", baseModelCount);
                if (totalInstances > 0) {
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Total Benchmark Instances: %zu", totalInstances);
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Total Rendered Models: %zu", baseModelCount + totalInstances);
                }
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Total Polygons (Rendered): %zu", totalPolygons);
                ImGui::Separator();
                ImGui::Spacing();
                
                // Display information for each model
                if (baseModelCount > 0) {
                    ImGui::Text("Model Details:");
                    ImGui::Separator();
                    
                    for (size_t i = 0; i < baseModelCount; i++) {
                        ModelInstance* instance = modelManager->getModel(static_cast<int>(i));
                        if (instance != nullptr) {
                            // Display Name
                            std::string displayName = instance->displayName;
                            if (displayName.empty()) {
                                displayName = "Model " + std::to_string(i);
                            }
                            ImGui::Text("Model %zu: %s", i, displayName.c_str());
                            
                            // Polygon Count
                            size_t basePolys = instance->model.getPolygonCount();
                            ImGui::Text("  Polygons: %zu", basePolys);
                            
                            // Benchmark instances info
                            if (!instance->benchmarkOffsets.empty()) {
                                ImGui::Text("  Instances: %zu", instance->benchmarkOffsets.size());
                                ImGui::Text("  Total Polygons: %zu", basePolys * (1 + instance->benchmarkOffsets.size()));
                            }
                            
                            // Bone Count
                            unsigned int boneCount = instance->model.getBoneCount();
                            ImGui::Text("  Bones: %u", boneCount);
                            
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
            
            ImGui::EndTabItem();
        }
        
        // Benchmark tab
        if (ImGui::BeginTabItem("Benchmark")) {
            // Benchmark content
            if (modelManager != nullptr) {
                size_t modelCount = modelManager->getModelCount();
                
                if (modelCount > 0) {
                    // Source Models: multi-select with checkboxes (one per loaded model)
                    // Use vector<int> 0/1 instead of vector<bool> so we can pass a bool* to ImGui::Checkbox
                    static std::vector<int> sourceModelChecked;
                    if (sourceModelChecked.size() != modelCount) {
                        size_t oldSize = sourceModelChecked.size();
                        sourceModelChecked.resize(modelCount, 0);
                        if (oldSize == 0 && sourceModelChecked.size() > 0) sourceModelChecked[0] = 1;
                    }
                    ImGui::Text("Source Models (select one or more):");
                    for (size_t i = 0; i < modelCount; i++) {
                        ModelInstance* instance = modelManager->getModel(static_cast<int>(i));
                        std::string label = instance && !instance->displayName.empty()
                            ? instance->displayName
                            : ("Model " + std::to_string(i));
                        bool checked = (sourceModelChecked[i] != 0);
                        if (ImGui::Checkbox((label + "##src" + std::to_string(i)).c_str(), &checked)) {
                            sourceModelChecked[i] = checked ? 1 : 0;
                        }
                    }
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    // Grid setup sliders
                    static int gridRows = 10;
                    static int gridCols = 10;
                    static float gridSpacing = 5.0f;
                    static bool autoSpacing = true;
                    static float paddingMultiplier = 1.2f;
                    
                    ImGui::Text("Grid Setup:");
                    ImGui::SliderInt("Grid Rows", &gridRows, 1, 50);
                    ImGui::SliderInt("Grid Columns", &gridCols, 1, 50);
                    
                    // Auto-spacing checkbox
                    ImGui::Checkbox("Auto-Spacing (Use Bounding Box)", &autoSpacing);
                    
                    if (autoSpacing) {
                        // Show padding multiplier slider when auto-spacing is enabled
                        ImGui::SliderFloat("Padding Multiplier", &paddingMultiplier, 1.0f, 3.0f);
                    } else {
                        // Show manual spacing slider when auto-spacing is disabled
                        ImGui::SliderFloat("Manual Spacing", &gridSpacing, 1.0f, 50.0f);
                    }
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    // Spawn Benchmark Grid button
                    if (ImGui::Button("Spawn Benchmark Grid", ImVec2(-1, 0))) {
                        std::vector<int> activeSourceIndices;
                        for (size_t i = 0; i < modelCount; i++) {
                            if (sourceModelChecked[i]) activeSourceIndices.push_back(static_cast<int>(i));
                        }
                        if (activeSourceIndices.empty()) {
                            LOG_ERROR("[Benchmark] Select at least one source model");
                        } else {
                        // Calculate spacing based on auto-spacing setting (use first selected model for bounds)
                        float finalSpacing = 5.0f;
                        if (autoSpacing) {
                            ModelInstance* instance = modelManager->getModel(activeSourceIndices[0]);
                            if (instance) {
                                // 1. Get the local min/max directly from the BoundingBox object used in the viewport
                                glm::vec3 localMin = instance->boundingBox.getMin();
                                glm::vec3 localMax = instance->boundingBox.getMax();

                                // 2. Define the 8 corners of the bounding box in local space
                                glm::vec3 corners[8] = {
                                    glm::vec3(localMin.x, localMin.y, localMin.z),
                                    glm::vec3(localMax.x, localMin.y, localMin.z),
                                    glm::vec3(localMax.x, localMin.y, localMax.z),
                                    glm::vec3(localMin.x, localMin.y, localMax.z),
                                    glm::vec3(localMin.x, localMax.y, localMin.z),
                                    glm::vec3(localMax.x, localMax.y, localMin.z),
                                    glm::vec3(localMax.x, localMax.y, localMax.z),
                                    glm::vec3(localMin.x, localMax.y, localMax.z)
                                };

                                // 3. Construct the base transformation matrix applied to the model in the viewport
                                glm::mat4 modelMat = glm::mat4(1.0f);
                                modelMat = glm::translate(modelMat, instance->model.pos);
                                modelMat = modelMat * glm::mat4_cast(instance->model.rotation);
                                modelMat = glm::scale(modelMat, instance->model.size);

                                // 4. Transform all 8 corners to World Space and find the absolute min/max bounds
                                float worldMinX = FLT_MAX, worldMinZ = FLT_MAX;
                                float worldMaxX = -FLT_MAX, worldMaxZ = -FLT_MAX;

                                for (int i = 0; i < 8; ++i) {
                                    glm::vec3 worldPos = glm::vec3(modelMat * glm::vec4(corners[i], 1.0f));
                                    worldMinX = std::min(worldMinX, worldPos.x);
                                    worldMinZ = std::min(worldMinZ, worldPos.z);
                                    worldMaxX = std::max(worldMaxX, worldPos.x);
                                    worldMaxZ = std::max(worldMaxZ, worldPos.z);
                                }

                                // 5. Calculate final spacing based on true World Space dimensions
                                float sizeX = std::abs(worldMaxX - worldMinX);
                                float sizeZ = std::abs(worldMaxZ - worldMinZ);

                                float maxSize = std::max(sizeX, sizeZ);
                                if (maxSize < 0.1f) maxSize = 1.0f;

                                finalSpacing = maxSize * paddingMultiplier;

                                LOG_INFO("[Benchmark] Cyan Box Dimensions - X: " + std::to_string(sizeX) + ", Z: " + std::to_string(sizeZ) + " -> Final Spacing: " + std::to_string(finalSpacing));
                            }
                        } else {
                            finalSpacing = gridSpacing;
                        }
                        
                        modelManager->spawnBenchmarkGrid(activeSourceIndices, gridRows, gridCols, finalSpacing);
                        LOG_INFO("[Benchmark] Spawned grid: " + std::to_string(gridRows) + "x" + std::to_string(gridCols) + 
                                " with spacing " + std::to_string(finalSpacing) + ", " +
                                std::to_string(activeSourceIndices.size()) + " source model(s)");
                        }
                    }
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    // Randomization Controls
                    static float posJitter = 0.0f;
                    static float rotJitter = 0.0f;
                    static float scaleJitter = 0.0f;
                    
                    ImGui::Text("Randomization Controls:");
                    ImGui::Text("Adjust sliders to add variation to benchmark instances");
                    ImGui::Spacing();
                    
                    // Position Jitter slider (XZ only - Y-axis is locked to keep characters on ground)
                    ImGui::SliderFloat("Pos Jitter (XZ only)", &posJitter, 0.0f, 10.0f, "%.2f");
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Adds random position offset to X and Z axes only.\nY-axis is locked to keep characters on the ground.");
                    }
                    bool posJitterChanged = ImGui::IsItemEdited();
                    
                    // Rotation Jitter slider
                    ImGui::SliderFloat("Rotation Jitter", &rotJitter, 0.0f, 3.14159f, "%.3f");
                    bool rotJitterChanged = ImGui::IsItemEdited();
                    
                    // Scale Jitter slider
                    ImGui::SliderFloat("Scale Jitter", &scaleJitter, 0.0f, 1.0f, "%.3f");
                    bool scaleJitterChanged = ImGui::IsItemEdited();
                    
                    // Update matrices if any slider changed
                    if (posJitterChanged || rotJitterChanged || scaleJitterChanged) {
                        modelManager->updateBenchmarkMatrices(posJitter, rotJitter, scaleJitter);
                    }
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    // Clear Benchmark Models button
                    if (ImGui::Button("Clear Benchmark Models", ImVec2(-1, 0))) {
                        modelManager->clearBenchmarkModels();
                        // Reset jitter sliders when clearing
                        posJitter = 0.0f;
                        rotJitter = 0.0f;
                        scaleJitter = 0.0f;
                        LOG_INFO("[Benchmark] Cleared benchmark models");
                    }
                } else {
                    ImGui::Text("No models loaded. Load a model first to use benchmark tool.");
                }
            } else {
                ImGui::Text("ModelManager is null");
            }
            
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }
    
    ImGui::End();
}
