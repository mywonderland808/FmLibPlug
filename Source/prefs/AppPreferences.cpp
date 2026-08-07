#include "prefs/AppPreferences.h"

namespace fmlib
{

AppPreferences::AppPreferences()
{
    load();
}

juce::File AppPreferences::getFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("FmLibPlug")
        .getChildFile ("settings.xml");
}

void AppPreferences::load()
{
    loadFromFile (getFile());
}

void AppPreferences::loadFromFile (const juce::File& f)
{
    if (! f.existsAsFile())
        return;
    if (auto xml = juce::XmlDocument::parse (f))
    {
        darkTheme = xml->getBoolAttribute ("dark", true);
        groupByBank = xml->getBoolAttribute ("groupByBank", true);
        bankFileView = xml->getBoolAttribute ("bankFileView", true);
        showFileColumns = xml->getBoolAttribute ("showFileColumns", false);
        hideDuplicates = xml->getBoolAttribute ("hideDuplicates", false);
        showTooltips = xml->getBoolAttribute ("showTooltips", true);
        midiChannel = xml->getIntAttribute ("midiChannel", 1);
        sysexPacingMs = xml->getIntAttribute ("pacingMs", 20);
        auditionNote = xml->getIntAttribute ("auditionNote", 60);
        auditionVelocity = xml->getIntAttribute ("auditionVelocity", 100);
        auditionDurationMs = xml->getIntAttribute ("auditionDurationMs", 250);
        midiInputName = xml->getStringAttribute ("midiIn");
        midiOutputName = xml->getStringAttribute ("midiOut");
        baseFolders.clear();
        if (auto* folders = xml->getChildByName ("folders"))
        {
            for (auto* c = folders->getFirstChildElement(); c != nullptr; c = c->getNextElement())
                if (c->hasTagName ("folder"))
                    baseFolders.emplace_back (c->getStringAttribute ("path").toStdString());
        }
        favoriteIds.clear();
        if (auto* fav = xml->getChildByName ("favorites"))
        {
            for (auto* c = fav->getFirstChildElement(); c != nullptr; c = c->getNextElement())
                if (c->hasTagName ("id"))
                    favoriteIds.add (c->getStringAttribute ("v"));
        }
    }
}

void AppPreferences::save() const
{
    saveToFile (getFile());
}

void AppPreferences::saveToFile (const juce::File& f) const
{
    auto xml = std::make_unique<juce::XmlElement> ("FmLibPlugSettings");
    xml->setAttribute ("dark", darkTheme);
    xml->setAttribute ("groupByBank", groupByBank);
    xml->setAttribute ("bankFileView", bankFileView);
    xml->setAttribute ("showFileColumns", showFileColumns);
    xml->setAttribute ("hideDuplicates", hideDuplicates);
    xml->setAttribute ("showTooltips", showTooltips);
    xml->setAttribute ("midiChannel", midiChannel);
    xml->setAttribute ("pacingMs", sysexPacingMs);
    xml->setAttribute ("auditionNote", auditionNote);
    xml->setAttribute ("auditionVelocity", auditionVelocity);
    xml->setAttribute ("auditionDurationMs", auditionDurationMs);
    xml->setAttribute ("midiIn", midiInputName);
    xml->setAttribute ("midiOut", midiOutputName);
    auto* folders = xml->createNewChildElement ("folders");
    for (const auto& p : baseFolders)
        folders->createNewChildElement ("folder")->setAttribute ("path", juce::String::fromUTF8 (p.string().c_str()));
    auto* fav = xml->createNewChildElement ("favorites");
    for (const auto& id : favoriteIds)
        fav->createNewChildElement ("id")->setAttribute ("v", id);

    f.getParentDirectory().createDirectory();
    xml->writeTo (f);
}

} // namespace fmlib
