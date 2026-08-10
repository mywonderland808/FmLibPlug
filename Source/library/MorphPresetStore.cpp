#include "library/MorphPresetStore.h"
#include <cstring>

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
void writeVoice (juce::XmlElement& parent, const char* tag, const VoiceData& v, const std::string& name)
{
    auto* el = parent.createNewChildElement (tag);
    el->setAttribute ("name", juce::String::fromUTF8 (name.c_str()));
    juce::MemoryBlock mb (v.data(), v.size());
    el->setAttribute ("data", mb.toBase64Encoding());
}

bool readVoice (const juce::XmlElement& parent, const char* tag, VoiceData& v, std::string& name)
{
    if (auto* el = parent.getChildByName (tag))
    {
        name = el->getStringAttribute ("name").toStdString();
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding (el->getStringAttribute ("data")) && mb.getSize() >= v.size())
        {
            std::memcpy (v.data(), mb.getData(), v.size());
            return true;
        }
    }
    return false;
}
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
                p.name = c->getStringAttribute ("name").toStdString();
                p.posX = (float) c->getDoubleAttribute ("posX", 0.0);
                p.posY = (float) c->getDoubleAttribute ("posY", 0.0);
                p.lockGroups = migrateMorphLockGroups ((uint32_t) c->getIntAttribute ("lockGroups", 0), 1);
                p.lockRefX = (float) c->getDoubleAttribute ("lockRefX", 0.0);
                p.lockRefY = (float) c->getDoubleAttribute ("lockRefY", 0.0);
                readVoice (*c, "a", p.a, p.nameA);
                readVoice (*c, "b", p.b, p.nameB);
                readVoice (*c, "c", p.c, p.nameC);
                readVoice (*c, "d", p.d, p.nameD);
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
        el->setAttribute ("name", juce::String::fromUTF8 (p.name.c_str()));
        el->setAttribute ("posX", (double) p.posX);
        el->setAttribute ("posY", (double) p.posY);
        el->setAttribute ("lockGroups", (int) p.lockGroups);
        el->setAttribute ("lockRefX", (double) p.lockRefX);
        el->setAttribute ("lockRefY", (double) p.lockRefY);
        writeVoice (*el, "a", p.a, p.nameA);
        writeVoice (*el, "b", p.b, p.nameB);
        writeVoice (*el, "c", p.c, p.nameC);
        writeVoice (*el, "d", p.d, p.nameD);
    }
    file.getParentDirectory().createDirectory();
    xml->writeTo (file);
}

} // namespace fmlib
