#pragma once

#include <juce_core/juce_core.h>

namespace fmlib
{

inline constexpr const char* kParamMorphX = "morphX";
inline constexpr const char* kParamMorphY = "morphY";
inline constexpr const char* kParamLockRefX = "lockRefX";
inline constexpr const char* kParamLockRefY = "lockRefY";
inline constexpr const char* kParamMorphLfoRate = "morphLfoRate";
inline constexpr const char* kParamMorphLfoSync = "morphLfoSync";
inline constexpr const char* kParamMorphLfoDivision = "morphLfoDivision";
inline constexpr const char* kParamMorphMotionMode = "morphMotionMode";

enum class MorphMotionMode : int
{
    off = 0,
    random = 1,
    edges = 2,
    edgeLfo = 3
};

/** Musical length of one Edge LFO orbit (A-B-D-C). */
enum class MorphLfoDivision : int
{
    whole = 0,
    half,
    quarter,
    eighth,
    sixteenth,
    thirtySecond,
    halfT,
    quarterT,
    eighthT,
    sixteenthT,
    thirtySecondT,
    quarterD,
    eighthD,
    sixteenthD,
    count
};

inline constexpr int kMorphLfoDivisionDefault = static_cast<int> (MorphLfoDivision::quarter);
/** Fine perimeter steps — traffic limited by MorphTransport byte budget, not step count. */
inline constexpr int kMorphLfoStepsPerLoop = 96;
inline constexpr float kMorphLfoPauseHz = 0.02f;

juce::StringArray morphLfoDivisionLabels();
int clampMorphLfoDivision (int division);
/** Quarter-notes per LFO cycle. Division is one edge (A→B→D→C); an orbit is 4× that. */
double morphLfoBeatsPerCycle (int division);
/** Signed Hz from BPM + division. Sign/pause come from the free-rate param. */
float morphLfoRateHzFromTempo (double bpm, int division, float signedFreeRateHz);
/** Phase (cycles) locked to host PPQ. */
float morphLfoPhaseFromPpq (double ppq, int division, float signedFreeRateHz);

struct MorphMotionMapping
{
    bool lfoEnabled = false;
    int noteJumpMode = 0;
    bool controllerNotesToCallbacksOnly = false;
};

MorphMotionMapping morphMotionFromChoice (int choice);
int morphMotionToChoice (bool lfoEnabled, int noteJumpMode);

float clampMorph01 (float v);
float clampMorphLfoRate (float hz);

bool isMorphMotionActive (int motionChoice);
bool isEdgeLfoMotion (int motionChoice);
bool morphPositionMatchesLastPluginWrite (float x, float y, float lastX, float lastY);

/** Pad point on the A-B-D-C perimeter for a unit-interval phase. */
void morphPadEdgePosition (float phase01, bool clockwise, float& x, float& y);

} // namespace fmlib
