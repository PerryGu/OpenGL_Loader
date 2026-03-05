# Animation Transport Controls - Pause and Stop Separation

## Date
Created: 2026-02-23

## Overview
Implemented professional animation transport controls that separate **Pause** and **Stop** functionality, matching industry-standard animation editors (Maya/Blender behavior). This provides users with precise control over animation playback state.

## Problem
Previously, the Play/Pause button (`>`) would both pause the animation AND reset it to frame 0. This made it impossible to pause mid-animation and resume from the same frame, which is a common workflow in animation editing.

## Solution
Separated the functionality into two distinct operations:
- **Play/Pause Button (`>`)**: Toggles play state without resetting animation time (pausing preserves current frame)
- **Stop Button (`[]`)**: Stops animation AND resets to frame 0 (starts from beginning when Play is pressed)

## Implementation

### 1. ModelInstance Structure Refactoring
**Location**: `src/graphics/model_manager.h` (lines 126-154)

#### `setPlaying(bool playing)` - Play/Pause Toggle
```cpp
/**
 * Play/pause animation (toggle play state without resetting time).
 * 
 * This method only changes the play state. It does NOT reset animation time.
 * Use this for Play/Pause functionality where pausing keeps the character
 * in its current pose.
 * 
 * @param playing True to play animation, false to pause (keeps current frame)
 */
void setPlaying(bool playing) {
    isPlaying = playing;
}
```

**Behavior**:
- Only toggles `isPlaying` flag
- Does NOT modify `animationTime` or `totalTime`
- Preserves current animation frame when pausing

#### `stop()` - Stop and Reset
```cpp
/**
 * Stop animation and reset to frame 0.
 * 
 * This method stops the animation and resets both animationTime and totalTime
 * to 0.0f, ensuring the animation starts from the beginning when Play is
 * pressed again.
 * 
 * Use this for the Stop button functionality (separate from Pause).
 */
void stop() {
    isPlaying = false;
    animationTime = 0.0f;
    totalTime = 0.0f;
}
```

**Behavior**:
- Sets `isPlaying = false`
- Resets `animationTime = 0.0f`
- Resets `totalTime = 0.0f`
- Ensures animation starts from frame 0 when Play is pressed

### 2. ModelManager Stop All Animations
**Location**: `src/graphics/model_manager.h` (line 220), `src/graphics/model_manager.cpp` (lines 316-330)

Added method to stop all model animations simultaneously:
```cpp
/**
 * @brief Stops all animations and resets them to frame 0.
 * 
 * This method calls stop() on all model instances, which:
 * - Sets isPlaying to false
 * - Resets animationTime to 0.0f
 * - Resets totalTime to 0.0f
 * 
 * This is used by the Stop button functionality (separate from Pause).
 */
void ModelManager::stopAllAnimations() {
    for (auto& instance : models) {
        if (instance) {
            instance->stop();
        }
    }
}
```

### 3. UI Button Logic
**Location**: `src/io/ui/timeSlider.cpp` (lines 55-82)

#### Stop Button (`[]`)
```cpp
//-- stop button (resets animation to frame 0) -------------------
if (ImGui::Button("[]", ImVec2(buttonWidth, buttonHeight)))
{
    // Stop button: Request stop and reset to frame 0
    // This is separate from pause - stop resets animation time
    mStopRequested = true;
    mPlay_stop = false;  // Also pause when stopping
    LOG_INFO("Timeline: Stop requested - animation will reset to frame 0");
}
```

**Behavior**:
- Sets `mStopRequested = true` (one-shot flag)
- Sets `mPlay_stop = false` (pauses animation)
- Triggers stop request that resets all animations to frame 0

#### Play/Pause Button (`>`)
```cpp
//-- play/pause button (toggle - does not reset time) -------------------
if (ImGui::Button(">", ImVec2(playButtonWidth, playButtonHeight)))
{
    // Play/Pause toggle: Only changes play state, does NOT reset animation time
    // Pausing keeps the character in its current pose
    mPlay_stop = !mPlay_stop;
    LOG_INFO("Timeline: Play/Pause toggled to " + std::string(mPlay_stop ? "Play" : "Pause"));
}
```

**Behavior**:
- Toggles `mPlay_stop` flag
- Does NOT reset animation time
- Preserves current frame when pausing

### 4. Stop Request Mechanism
**Location**: `src/io/ui/timeSlider.h` (lines 23-24), `src/io/ui/timeSlider.cpp` (lines 119-127)

Added one-shot flag for stop requests:
```cpp
// Stop requested flag (separate from pause)
bool mStopRequested = false;

// Get stop requested flag (resets after being read)
bool getStopRequested() {
    if (mStopRequested) {
        mStopRequested = false;  // Reset flag after being read (one-shot)
        return true;
    }
    return false;
}
```

**Purpose**: Allows UI to signal a stop request that is processed once and then cleared.

### 5. Animation Update Logic
**Location**: `src/application.cpp` (lines 542-598)

#### Stop Request Handling
```cpp
// Check if stop was requested (separate from pause)
bool stopRequested = m_scene.getUIManager().getTimeSlider().getStopRequested();

// Handle stop request: Stop all animations and reset to frame 0
if (stopRequested) {
    m_modelManager.stopAllAnimations();
    m_scene.getUIManager().getTimeSlider().setTimeValue(0.0f);  // Reset slider to 0
    m_wasPlaying = false;
    LOG_INFO("Stopped all animations - reset to frame 0");
}
```

#### Resume Logic
```cpp
// If we just started playing, update timing references
// Note: If we were stopped, stop() already reset animationTime to 0
// If we were paused, animationTime is preserved (character stays in current pose)
if (play_stopVal && !m_wasPlaying) {
    // Starting to play (either from stopped or paused state)
    m_f_startTime = fCurrentTime;
    // Sync totalTime with current animationTime (preserves paused frame or uses 0 from stop)
    instance->totalTime = instance->animationTime;
    instance->lastUpdateTime = fCurrentTime;
}
```

**Key Logic**:
- After **Stop**: `animationTime` is already 0, so `totalTime` syncs to 0 (starts from beginning)
- After **Pause**: `animationTime` is preserved, so `totalTime` syncs to current frame (continues from paused position)

## User Workflow

### Pause Workflow
1. User clicks Play (`>`) - Animation starts playing
2. User clicks Pause (`>`) - Animation pauses, character stays in current pose
3. User clicks Play (`>`) - Animation resumes from paused frame

### Stop Workflow
1. User clicks Play (`>`) - Animation starts playing
2. User clicks Stop (`[]`) - Animation stops and resets to frame 0
3. User clicks Play (`>`) - Animation starts from frame 0 (beginning)

## Visual Feedback

### Time Slider Behavior
- **When Pausing**: Time slider stays at current frame value (e.g., 45.234)
- **When Stopping**: Time slider immediately jumps to 0.000
- **When Resuming from Pause**: Time slider continues from paused value
- **When Resuming from Stop**: Time slider starts from 0.000

### Character Pose
- **Pause**: Character remains in current pose (frozen at current frame)
- **Stop**: Character returns to bind pose (frame 0)

## Benefits

1. **Professional UX**: Matches industry-standard animation editors (Maya/Blender)
2. **Precise Control**: Users can pause mid-animation to inspect poses
3. **Workflow Efficiency**: No need to manually scrub back to paused frame
4. **Clear Separation**: Stop and Pause have distinct, predictable behaviors
5. **Visual Feedback**: Time slider immediately reflects stop state (0.000)

## Technical Details

### State Management
- **`isPlaying`**: Boolean flag for play/pause state
- **`animationTime`**: Current animation time in seconds (preserved on pause, reset on stop)
- **`totalTime`**: Accumulated time for auto-play (synced with `animationTime` on resume)
- **`mStopRequested`**: One-shot flag for stop requests (auto-resets after being read)

### Timing Synchronization
When resuming from pause:
```cpp
instance->totalTime = instance->animationTime;  // Sync to preserved frame
```

When resuming from stop:
```cpp
// stop() already set animationTime = 0.0f
instance->totalTime = instance->animationTime;  // Syncs to 0.0f
```

## Testing

### Test Case 1: Pause Mid-Animation
1. Load a model with animation
2. Click Play (`>`) - Animation starts
3. Wait for animation to reach frame 30
4. Click Pause (`>`) - Animation pauses
5. **Expected**: Character stays in frame 30 pose, time slider shows 30.xxx
6. Click Play (`>`) - Animation resumes
7. **Expected**: Animation continues from frame 30, not from frame 0

### Test Case 2: Stop Mid-Animation
1. Load a model with animation
2. Click Play (`>`) - Animation starts
3. Wait for animation to reach frame 30
4. Click Stop (`[]`) - Animation stops
5. **Expected**: Character returns to bind pose (frame 0), time slider shows 0.000
6. Click Play (`>`) - Animation starts
7. **Expected**: Animation starts from frame 0 (beginning), not from frame 30

### Test Case 3: Multiple Models
1. Load multiple models with animations
2. Click Play (`>`) - All animations start
3. Click Stop (`[]`) - All animations stop and reset to frame 0
4. **Expected**: All models return to bind pose, all time sliders show 0.000

## Related Files
- `src/graphics/model_manager.h` - `ModelInstance::setPlaying()` and `stop()` methods
- `src/graphics/model_manager.cpp` - `ModelManager::stopAllAnimations()` implementation
- `src/io/ui/timeSlider.h` - `mStopRequested` flag and `getStopRequested()` method
- `src/io/ui/timeSlider.cpp` - UI button logic
- `src/application.cpp` - Animation update logic with stop request handling

## Future Enhancements

1. **Per-Model Controls**: Individual play/pause/stop for each model
2. **Animation Speed**: Per-model speed multiplier
3. **Frame Stepping**: Step forward/backward one frame at a time
4. **Keyframe Navigation**: Jump to next/previous keyframe
5. **Animation Looping**: Toggle loop mode (currently loops automatically)

## Summary

The Animation Transport Controls feature provides professional-grade animation playback control by separating Pause (preserves frame) and Stop (resets to frame 0) functionality. This matches industry-standard animation editors and provides users with precise control over animation state, improving workflow efficiency and user experience.
