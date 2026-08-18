#include "library/MorphPresetStore.h"
#include "library/MorphPresetXml.h"

namespace fmlib
{

void MorphPresetStore::add (MorphPreset p)
{
    if (p.name.empty())
        p.name = "Morph " + std::to_string (presets.size() + 1);
    presets.push_back (std::move (p));
}

void MorphPresetStore::removeAt (int index)
{
    if (index >= 0 && index < static_cast<int> (presets.size()))
        presets.erase (presets.begin() + index);
}

bool MorphPresetStore::replaceAt (int index, MorphPreset p)
{
    if (index < 0 || index >= static_cast<int> (presets.size()))
        return false;
    presets[static_cast<size_t> (index)] = std::move (p);
    return true;
}

namespace
{
} // namespace

void MorphPresetStore::loadFromFile (const juce::File& file)
{
    presets.clear();
    if (! file.existsAsFile())
        return;
    if (auto xml = juce::XmlDocument::parse (file))
    {
        if (auto* root = xml->getChildByName ("morphPresets"))
        {
            for (auto* c = root->getFirstChildElement(); c != nullptr; c = c->getNextElement())
            {
                if (! c->hasTagName ("preset"))
                    continue;
                MorphPreset p;
                if (! readMorphPresetElement (*c, p))
                    continue;
                presets.push_back (std::move (p));
            }
        }
    }
}

void MorphPresetStore::saveToFile (const juce::File& file) const
{
    auto xml = std::make_unique<juce::XmlElement> ("FmLibPlugMorph");
    auto* root = xml->createNewChildElement ("morphPresets");
    for (const auto& p : presets)
    {
        auto* el = root->createNewChildElement ("preset");
        writeMorphPresetElement (*el, p, true);
    }
    file.getParentDirectory().createDirectory();
    xml->writeTo (file);
}

} // namespace fmlib
