#include "host/MorphHostState.h"
#include <cmath>

namespace fmlib
{

MorphMotionMapping morphMotionFromChoice (int choice)
{
    MorphMotionMapping m;
    switch (juce::jlimit (0, 3, choice))
    {
        case 1:
            m.noteJumpMode = 1;
            m.controllerNotesToCallbacksOnly = true;
            break;
        case 2:
            m.noteJumpMode = 2;
            m.controllerNotesToCallbacksOnly = true;
            break;
        case 3:
            m.lfoEnabled = true;
            break;
        default:
            break;
    }
    return m;
}

int morphMotionToChoice (bool lfoEnabled, int noteJumpMode)
{
    if (lfoEnabled)
        return static_cast<int> (MorphMotionMode::edgeLfo);
    if (noteJumpMode == 1)
        return static_cast<int> (MorphMotionMode::random);
    if (noteJumpMode == 2)
        return static_cast<int> (MorphMotionMode::edges);
    return static_cast<int> (MorphMotionMode::off);
}

juce::StringArray morphLfoDivisionLabels()
{
    return { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
             "1/2T", "1/4T", "1/8T", "1/16T", "1/32T",
             "1/4D", "1/8D", "1/16D" };
}

int clampMorphLfoDivision (int division)
{
    return juce::jlimit (0, static_cast<int> (MorphLfoDivision::count) - 1, division);
}

double morphLfoBeatsPerCycle (int division)
{
    switch (static_cast<MorphLfoDivision> (clampMorphLfoDivision (division)))
    {
        case MorphLfoDivision::whole:          return 16.0;
        case MorphLfoDivision::half:           return 8.0;
        case MorphLfoDivision::quarter:        return 4.0;
        case MorphLfoDivision::eighth:         return 2.0;
        case MorphLfoDivision::sixteenth:      return 1.0;
        case MorphLfoDivision::thirtySecond:   return 0.5;
        case MorphLfoDivision::halfT:          return 16.0 / 3.0;
        case MorphLfoDivision::quarterT:       return 8.0 / 3.0;
        case MorphLfoDivision::eighthT:        return 4.0 / 3.0;
        case MorphLfoDivision::sixteenthT:     return 2.0 / 3.0;
        case MorphLfoDivision::thirtySecondT:  return 1.0 / 3.0;
        case MorphLfoDivision::quarterD:       return 6.0;
        case MorphLfoDivision::eighthD:        return 3.0;
        case MorphLfoDivision::sixteenthD:     return 1.5;
        case MorphLfoDivision::count:          break;
    }
    return 4.0;
}

float morphLfoRateHzFromTempo (double bpm, int division, float signedFreeRateHz)
{
    if (std::abs (signedFreeRateHz) < kMorphLfoPauseHz)
        return 0.0f;
    const double safeBpm = juce::jlimit (20.0, 400.0, bpm > 0.0 ? bpm : 120.0);
    const float hz = (float) ((safeBpm / 60.0) / morphLfoBeatsPerCycle (division));
    return signedFreeRateHz < 0.0f ? -hz : hz;
}

float morphLfoPhaseFromPpq (double ppq, int division, float signedFreeRateHz)
{
    const double beats = morphLfoBeatsPerCycle (division);
    double phase = (beats > 0.0) ? (ppq / beats) : 0.0;
    if (signedFreeRateHz < 0.0f)
        phase = -phase;
    return (float) phase;
}

float clampMorph01 (float v)
{
    return juce::jlimit (0.0f, 1.0f, v);
}

float clampMorphLfoRate (float hz)
{
    return juce::jlimit (-1.5f, 1.5f, hz);
}

bool isMorphMotionActive (int motionChoice)
{
    return motionChoice != static_cast<int> (MorphMotionMode::off);
}

bool isEdgeLfoMotion (int motionChoice)
{
    return motionChoice == static_cast<int> (MorphMotionMode::edgeLfo);
}

bool morphPositionMatchesLastPluginWrite (float x, float y, float lastX, float lastY)
{
    constexpr float kEpsilon = 1.0e-5f;
    return std::abs (x - lastX) <= kEpsilon && std::abs (y - lastY) <= kEpsilon;
}

void morphPadEdgePosition (float phase01, bool clockwise, float& x, float& y)
{
    float t = phase01 - std::floor (phase01);
    if (! clockwise)
        t = 1.0f - t;
    const float s = t * 4.0f;
    if (s < 1.0f) { x = s; y = 0.0f; }
    else if (s < 2.0f) { x = 1.0f; y = s - 1.0f; }
    else if (s < 3.0f) { x = 1.0f - (s - 2.0f); y = 1.0f; }
    else { x = 0.0f; y = 1.0f - (s - 3.0f); }
}

} // namespace fmlib
