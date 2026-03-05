#include "timeSlider.h"
#include "../../utils/logger.h"



//== constructors ===============================
void TimeSlider::timeSliderPanel(bool* p_open)
{

    //ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.65f);
    ImGui::Begin("timeSlider");
    //ImGui::PushItemWidth(ImGui::GetWindowWidth());


    // Calculate button size first (needed for layout calculations)
    // Use a larger minimum size to ensure text/symbols are visible
    float buttonHeight = ImGui::GetFrameHeight();
    float buttonWidth = buttonHeight * 1.8f; // Wider buttons for better visibility of symbols
    // Ensure minimum size for readability (larger minimums)
    if (buttonWidth < 35.0f) buttonWidth = 35.0f;
    if (buttonHeight < 28.0f) buttonHeight = 28.0f;
    
    // Calculate available width for controls (accounting for buttons and spacing)
    float windowWidth = ImGui::GetWindowWidth();
    // Ensure buttons have minimum guaranteed width
    float guaranteedButtonWidth = (buttonWidth < 40.0f) ? 40.0f : buttonWidth;
    float buttonsWidth = guaranteedButtonWidth * 2.0f + ImGui::GetStyle().ItemSpacing.x * 3.0f; // 2 buttons + spacing
    float minMaxInputWidth = windowWidth * 0.06f;
    float timeInputWidth = windowWidth * 0.04f;
    float spacingWidth = ImGui::GetStyle().ItemSpacing.x * 6.0f; // Approximate spacing between elements
    float reservedWidth = minMaxInputWidth * 2.0f + timeInputWidth + buttonsWidth + spacingWidth + 60.0f; // Extra margin for buttons
    
    // Calculate slider width: use remaining space, but ensure minimum size
    float sliderWidth = windowWidth - reservedWidth;
    float minSliderWidth = 100.0f; // Minimum slider width
    if (sliderWidth < minSliderWidth) {
        sliderWidth = minSliderWidth;
    }
    
    ImGui::PushItemWidth(minMaxInputWidth);
    ImGui::InputInt("MinT", &mMinSliderVal);

    ImGui::SameLine();
    ImGui::PushItemWidth(sliderWidth);
    ImGui::SliderFloat("|", &mTime_value, mMinSliderVal, mMaxSliderVal);

    ImGui::SameLine();
    ImGui::PushItemWidth(minMaxInputWidth);
    ImGui::InputInt("MaxT", &mMaxSliderVal);

    ImGui::SameLine();
    ImGui::PushItemWidth(timeInputWidth);
    ImGui::InputFloat("", &mTime_value);

    //-- add Buttons -------------------------------------------------
    //-- stop button (resets animation to frame 0) -------------------
    ImGui::SameLine();
    if (ImGui::Button("[]", ImVec2(buttonWidth, buttonHeight)))
    {
        // Stop button: Request stop and reset to frame 0
        // This is separate from pause - stop resets animation time
        mStopRequested = true;
        mPlay_stop = false;  // Also pause when stopping
        LOG_INFO("Timeline: Stop requested - animation will reset to frame 0");
    }

    //-- play/pause button (toggle - does not reset time) -------------------
    ImGui::SameLine();
    // Ensure button is always visible - use explicit size
    // Make play button slightly larger to ensure ">" symbol is visible
    float playButtonWidth = buttonWidth;
    float playButtonHeight = buttonHeight;
    if (playButtonWidth < 40.0f) playButtonWidth = 40.0f; // Ensure minimum width
    if (playButtonHeight < 30.0f) playButtonHeight = 30.0f; // Ensure minimum height
    
    if (ImGui::Button(">", ImVec2(playButtonWidth, playButtonHeight)))
    {
        // Play/Pause toggle: Only changes play state, does NOT reset animation time
        // Pausing keeps the character in its current pose
        mPlay_stop = !mPlay_stop;
        LOG_INFO("Timeline: Play/Pause toggled to " + std::string(mPlay_stop ? "Play" : "Pause"));
    }
  
    //f (mTime_value > mMaxSliderVal)
    //
    //   mTime_value = mMinSliderVal;
    //

    //-- End  ----
    ImGui::End();
}

//-- get the main time value --------------
float TimeSlider::getTimeValue()
{
    //std::cout << "timeVal " << mTime_value << std::endl;
    return mTime_value;
}

//-- get the main Min TimeSlider value --------------
float TimeSlider::getMinTimeSlider()
{
    return mMinSliderVal;
}

//-- get the main MAx TimeSlider value --------------
float TimeSlider::getMaxTimeSlider()
{
    return mMaxSliderVal;
}

//-- get play or stop value --------------
bool TimeSlider::getPlay_stop()
{
    return mPlay_stop;
}

//-- get stop requested flag (resets after being read) --------------
bool TimeSlider::getStopRequested()
{
    if (mStopRequested) {
        mStopRequested = false;  // Reset flag after being read (one-shot)
        return true;
    }
    return false;
}

//-- set the main time value --------------
void TimeSlider::setTimeValue(float timeValu)
{
     mTime_value = timeValu;
}

//-- set the max time slider value --------------
void TimeSlider::setMaxTimeSlider(float maxTime)
{
     mMaxSliderVal = (int)maxTime;
}