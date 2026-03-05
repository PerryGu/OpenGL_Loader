#ifndef PROPERTYPANEL_H
#define PROPERTYPANEL_H

#include <string>
#include <sstream>
#include <map>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>  // For glm::eulerAngles and glm::quat

//#include "pch.h"
#include "../../imgui/imgui.h"
#include "imgui_miniSliderV3.h"

// Forward declaration to avoid circular dependency
class Model;

class PropertyPanel
  {
  public:

      void propertyPanel(bool* p_open, int selectedModelIndex = -1, Model* model = nullptr);
      void setSelectedNode(std::string selectedNode, Model* model = nullptr);
      glm::vec3 getSliderRot_update();
      glm::vec3 getSliderTrans_update();
      glm::vec3 getSliderScale_update();
      
      // Get all bone rotations (for debugging/inspection)
      const std::map<std::string, glm::vec3>& getAllBoneRotations() const { return mBoneRotations; }
      
      // Get all bone translations (for debugging/inspection)
      const std::map<std::string, glm::vec3>& getAllBoneTranslations() const { return mBoneTranslations; }
      
      // Get all bone scales (for debugging/inspection)
      const std::map<std::string, glm::vec3>& getAllBoneScales() const { return mBoneScales; }
      
      // Set slider values (for gizmo to update PropertyPanel transforms)
      // These methods allow external code (like gizmo) to update the transform values
      void setSliderRotations(const glm::vec3& rotations);
      void setSliderTranslations(const glm::vec3& translations);
      void setSliderScales(const glm::vec3& scales);
      
      // Initialize RootNode transforms from model's current transform
      // This is called when RootNode is first selected to sync PropertyPanel with model's current state
      // Prevents double transformation by ensuring PropertyPanel starts with model's current values
      void initializeRootNodeFromModel(const glm::vec3& translation, const glm::quat& rotation, const glm::vec3& scale);
      
      // Get currently selected node name
      std::string getSelectedNodeName() const { return mSelectedNod_name; }
      
      // Index-based selection (replaces name-based for performance)
      void setSelectedBoneIndex(int boneIndex) { mSelectedBoneIndex = boneIndex; }
      int getSelectedBoneIndex() const { return mSelectedBoneIndex; }
      
      // Direct update methods - update Model's arrays directly when slider changes
      // These methods take Model pointer and update m_BoneExtraRotations/Translations/Scales arrays
      void updateBoneRotation(Model* model, int boneIndex, const glm::vec3& rotation);
      void updateBoneTranslation(Model* model, int boneIndex, const glm::vec3& translation);
      void updateBoneScale(Model* model, int boneIndex, const glm::vec3& scale);
      
      // Clear all bone rotations
      void clearAllBoneRotations() { mBoneRotations.clear(); }
      
      // Clear all bone translations
      void clearAllBoneTranslations() { mBoneTranslations.clear(); }
      
      // Clear all bone scales
      void clearAllBoneScales() { mBoneScales.clear(); }
      
      // Reset current bone rotation to (0, 0, 0)
      void resetCurrentBone(Model* model = nullptr);
      
      // Reset all bone rotations to (0, 0, 0)
      void resetAllBones(Model* model = nullptr);
      
      // Remove specific bone transform (useful for RootNode cleanup)
      void removeBoneTransform(const std::string& nodeName) {
          mBoneRotations.erase(nodeName);
          mBoneTranslations.erase(nodeName);
          mBoneScales.erase(nodeName);
      }
      
      // Check if "Reset All Bones" was requested (for clearing Model rotations)
      bool isResetAllBonesRequested() const { return mResetAllBonesRequested; }
      
      // Clear the reset all bones request flag (called after processing)
      void clearResetAllBonesRequest() { mResetAllBonesRequested = false; }
      
      // Clear all transform values (for New Scene)
      // This resets all bone rotations, translations, scales, current sliders, and selected node
      // Ensures that when a new character is imported, it won't inherit previous character's transform values
      void clearAllTransforms();


  private:

    std::string mCurrentFile;
    std::string mSelectedNod_name = "";  // Kept for backward compatibility (display name)
    std::string mPreviousSelectedNode = "";  // Track previous selection to detect changes
    int mSelectedBoneIndex = -1;  // Index-based selection (replaces name-based matching)
    glm::vec3 mSliderRotations = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 mSliderTranslations = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 mSliderScales = glm::vec3(1.0f, 1.0f, 1.0f);  // Default scale is 1.0 (no scaling)
    
    // Map to store rotations for each bone/node (bone name -> rotation vec3)
    std::map<std::string, glm::vec3> mBoneRotations;
    
    // Map to store translations for each bone/node (bone name -> translation vec3)
    std::map<std::string, glm::vec3> mBoneTranslations;
    
    // Map to store scales for each bone/node (bone name -> scale vec3)
    std::map<std::string, glm::vec3> mBoneScales;
    
    // Flag to signal that "Reset All Bones" was requested (for clearing Model rotations/translations/scales)
    bool mResetAllBonesRequested = false;
  };

#endif