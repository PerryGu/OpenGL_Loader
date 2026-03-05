#ifndef TIMESLIDER_H
#define TIMESLIDER_H

#include <string>
#include <iostream>
#include <glm/glm.hpp>
#include "../../imgui/imgui.h"


class TimeSlider
{

public:

	void timeSliderPanel(bool* p_open);
	float getTimeValue();
	float getMinTimeSlider();
	float getMaxTimeSlider();
	bool getPlay_stop();
	bool getStopRequested();  // Check if stop was requested (resets after being read)
	void setTimeValue(float timeValu);
	void setMaxTimeSlider(float maxTime);

private:
	float mTime_value = 0.0;
	int mMinSliderVal = 0;
	int mMaxSliderVal = 100;
	bool mPlay_stop = false;  // Play/Pause state (true = playing, false = paused)
	bool mStopRequested = false;  // Stop requested flag (separate from pause)
};




#endif