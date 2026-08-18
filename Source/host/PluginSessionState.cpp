#include "host/PluginSessionState.h"
#include "library/MorphPresetXml.h"

namespace fmlib
{

std::unique_ptr<juce::XmlElement> writePluginSessionXml (const PluginSessionState& state)
{
    auto xml = std::make_unique<juce::XmlElement> ("FmLibPlugState");
    xml->setAttribute ("stateVersion", state.stateVersion);
    if (state.apvtsState.isValid())
    {
        if (auto apvtsXml = state.apvtsState.createXml())
            xml->addChildElement (apvtsXml.release());
    }
    auto* presetEl = xml->createNewChildElement ("preset");
    writeMorphPresetElement (*presetEl, state.liveMorph, true);
    return xml;
}

bool readPluginSessionXml (const juce::XmlElement& root, PluginSessionState& state)
{
    if (! root.hasTagName ("FmLibPlugState"))
        return false;
    state.stateVersion = root.getIntAttribute ("stateVersion", 0);
    if (state.stateVersion > kPluginSessionStateVersion)
        return false;

    state.apvtsState = juce::ValueTree();
    if (auto* apvtsXml = root.getChildByName ("Params"))
        state.apvtsState = juce::ValueTree::fromXml (*apvtsXml);

    state.liveMorph = MorphPreset {};
    if (auto* presetEl = root.getChildByName ("preset"))
        readMorphPresetElement (*presetEl, state.liveMorph);

    return true;
}

} // namespace fmlib
