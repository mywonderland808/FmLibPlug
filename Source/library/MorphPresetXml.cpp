#include "library/MorphPresetXml.h"
#include "sysex/MorphLocks.h"
#include <cstring>

namespace fmlib
{

void writeMorphVoice (juce::XmlElement& parent, const char* tag, const VoiceData& v, const std::string& name)
{
    auto* el = parent.createNewChildElement (tag);
    el->setAttribute ("name", juce::String::fromUTF8 (name.c_str()));
    juce::MemoryBlock mb (v.data(), v.size());
    el->setAttribute ("data", mb.toBase64Encoding());
}

bool readMorphVoice (const juce::XmlElement& parent, const char* tag, VoiceData& v, std::string& name)
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

void writeMorphPresetElement (juce::XmlElement& el, const MorphPreset& p, bool includeName)
{
    if (includeName)
        el.setAttribute ("name", juce::String::fromUTF8 (p.name.c_str()));
    el.setAttribute ("posX", (double) p.posX);
    el.setAttribute ("posY", (double) p.posY);
    el.setAttribute ("lockGroups", (int) p.lockGroups);
    el.setAttribute ("lockRefX", (double) p.lockRefX);
    el.setAttribute ("lockRefY", (double) p.lockRefY);
    writeMorphVoice (el, "a", p.a, p.nameA);
    writeMorphVoice (el, "b", p.b, p.nameB);
    writeMorphVoice (el, "c", p.c, p.nameC);
    writeMorphVoice (el, "d", p.d, p.nameD);
}

bool readMorphPresetElement (const juce::XmlElement& el, MorphPreset& p)
{
    if (! el.hasTagName ("preset"))
        return false;
    p.name = el.getStringAttribute ("name").toStdString();
    p.posX = (float) el.getDoubleAttribute ("posX", 0.0);
    p.posY = (float) el.getDoubleAttribute ("posY", 0.0);
    p.lockGroups = migrateMorphLockGroups ((uint32_t) el.getIntAttribute ("lockGroups", 0), 1);
    p.lockRefX = (float) el.getDoubleAttribute ("lockRefX", 0.0);
    p.lockRefY = (float) el.getDoubleAttribute ("lockRefY", 0.0);
    readMorphVoice (el, "a", p.a, p.nameA);
    readMorphVoice (el, "b", p.b, p.nameB);
    readMorphVoice (el, "c", p.c, p.nameC);
    readMorphVoice (el, "d", p.d, p.nameD);
    return true;
}

} // namespace fmlib
