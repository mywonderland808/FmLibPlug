#include "prefs/AppPreferences.h"

namespace fmlib
{

AppPreferences::AppPreferences()
    : AppPreferences (true)
{
}

AppPreferences::AppPreferences (bool loadFromDisk)
{
    if (loadFromDisk)
        load();
}

AppPreferences AppPreferences::compiledDefaults()
{
    return AppPreferences (false);
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
        bankFileView = xml->getBoolAttribute ("bankFileView", true);
        if (xml->hasAttribute ("listViewContents"))
            listViewContents = juce::jlimit (0, 1, xml->getIntAttribute ("listViewContents", 0));
        else
            // Former groupByBank only affected Bank headers; List defaults to all voices.
            listViewContents = 0;
        showFileColumns = xml->getBoolAttribute ("showFileColumns", false);
        hideDuplicates = xml->getBoolAttribute ("hideDuplicates", false);
        showTooltips = xml->getBoolAttribute ("showTooltips", true);
        favoritesOnly = xml->getBoolAttribute ("favoritesOnly", false);
        midiChannel = xml->getIntAttribute ("midiChannel", 1);
        sysexPacingMs = xml->getIntAttribute ("pacingMs", 20);
        morphEmitMs = juce::jlimit (20, 250, xml->getIntAttribute ("morphEmitMs", 150));
        morphReleaseGuardMs = juce::jlimit (0, 2000, xml->getIntAttribute ("morphReleaseGuardMs", 250));
        morphNoteSettleMs = juce::jlimit (0, 100, xml->getIntAttribute ("morphNoteSettleMs", 40));

        const int lockSchema = xml->getIntAttribute ("morphLockSchema", 0);
        if (lockSchema < 2)
        {
            // One-shot product migration: schemas 0 and 1 both adopt the EG+Levels
            // factory set, so whatever they saved is deliberately discarded.
            morphLockGroups = morphLockFactoryDefaults;
        }
        else
        {
            morphLockGroups = migrateMorphLockGroups (
                (uint32_t) xml->getIntAttribute ("morphLockGroups", (int) morphLockFactoryDefaults),
                lockSchema);
        }

        morphLockRefX = (float) juce::jlimit (0.0, 1.0, xml->getDoubleAttribute ("morphLockRefX", 0.0));
        morphLockRefY = (float) juce::jlimit (0.0, 1.0, xml->getDoubleAttribute ("morphLockRefY", 0.0));
        tagFilterExpanded = xml->getBoolAttribute ("tagFilterExpanded", false);
        morphLfoEnabled = xml->getBoolAttribute ("morphLfoEnabled", false);
        morphLfoRateHz = (float) xml->getDoubleAttribute ("morphLfoRateHz", 0.25);
        if (! xml->getBoolAttribute ("morphLfoClockwise", true) && morphLfoRateHz > 0.0f)
            morphLfoRateHz = -morphLfoRateHz;
        morphLfoRateHz = juce::jlimit (-1.5f, 1.5f, morphLfoRateHz);
        if (xml->hasAttribute ("morphNoteJumpMode"))
            morphNoteJumpMode = juce::jlimit (0, 2, xml->getIntAttribute ("morphNoteJumpMode", 0));
        else if (xml->getBoolAttribute ("morphRandomOnNote", false))
            morphNoteJumpMode = 1;
        else
            morphNoteJumpMode = 0;
        if (xml->hasAttribute ("morphStreamMode"))
        {
            morphStreamMode = juce::jlimit (0, 1, xml->getIntAttribute ("morphStreamMode", 1));
        }
        else
        {
            // Legacy morphHardwareProfile: 0 = gated (removed), 1 = continuous, 2 = realtime freq.
            morphStreamMode = xml->getIntAttribute ("morphHardwareProfile", 0) == 1
                                  ? static_cast<int> (MorphStreamMode::allParams)
                                  : static_cast<int> (MorphStreamMode::freqOnly);
        }
        midiControllerThru = xml->getBoolAttribute ("midiControllerThru", false);
        auditionNote = xml->getIntAttribute ("auditionNote", 60);
        auditionVelocity = xml->getIntAttribute ("auditionVelocity", 100);
        auditionDurationMs = xml->getIntAttribute ("auditionDurationMs", 250);
        midiInputName = xml->getStringAttribute ("midiIn");
        midiOutputName = xml->getStringAttribute ("midiOut");
        midiControllerInputName = xml->getStringAttribute ("midiControllerIn");
        libraryFolders.clear();
        if (auto* folders = xml->getChildByName ("folders"))
        {
            for (auto* c = folders->getFirstChildElement(); c != nullptr; c = c->getNextElement())
            {
                if (! c->hasTagName ("folder"))
                    continue;
                LibraryFolder folder;
                folder.path = c->getStringAttribute ("path").toStdString();
                folder.enabled = c->getBoolAttribute ("enabled", true);
                if (! folder.path.empty())
                    libraryFolders.push_back (std::move (folder));
            }
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
    xml->setAttribute ("bankFileView", bankFileView);
    xml->setAttribute ("listViewContents", listViewContents);
    xml->setAttribute ("showFileColumns", showFileColumns);
    xml->setAttribute ("hideDuplicates", hideDuplicates);
    xml->setAttribute ("showTooltips", showTooltips);
    xml->setAttribute ("favoritesOnly", favoritesOnly);
    xml->setAttribute ("midiChannel", midiChannel);
    xml->setAttribute ("pacingMs", sysexPacingMs);
    xml->setAttribute ("morphEmitMs", morphEmitMs);
    xml->setAttribute ("morphReleaseGuardMs", morphReleaseGuardMs);
    xml->setAttribute ("morphNoteSettleMs", morphNoteSettleMs);
    xml->setAttribute ("morphLockSchema", 2);
    xml->setAttribute ("morphLockGroups", (int) morphLockGroups);
    xml->setAttribute ("morphLockRefX", (double) morphLockRefX);
    xml->setAttribute ("morphLockRefY", (double) morphLockRefY);
    xml->setAttribute ("tagFilterExpanded", tagFilterExpanded);
    xml->setAttribute ("morphLfoEnabled", morphLfoEnabled);
    xml->setAttribute ("morphLfoRateHz", (double) morphLfoRateHz);
    xml->setAttribute ("morphNoteJumpMode", morphNoteJumpMode);
    xml->setAttribute ("morphStreamMode", morphStreamMode);
    xml->setAttribute ("midiControllerThru", midiControllerThru);
    xml->setAttribute ("auditionNote", auditionNote);
    xml->setAttribute ("auditionVelocity", auditionVelocity);
    xml->setAttribute ("auditionDurationMs", auditionDurationMs);
    xml->setAttribute ("midiIn", midiInputName);
    xml->setAttribute ("midiOut", midiOutputName);
    xml->setAttribute ("midiControllerIn", midiControllerInputName);
    auto* folders = xml->createNewChildElement ("folders");
    for (const auto& folder : libraryFolders)
    {
        auto* el = folders->createNewChildElement ("folder");
        el->setAttribute ("path", juce::String::fromUTF8 (folder.path.string().c_str()));
        el->setAttribute ("enabled", folder.enabled);
    }
    auto* fav = xml->createNewChildElement ("favorites");
    for (const auto& id : favoriteIds)
        fav->createNewChildElement ("id")->setAttribute ("v", id);

    f.getParentDirectory().createDirectory();
    xml->writeTo (f);
}

std::vector<std::filesystem::path> AppPreferences::enabledLibraryFolders() const
{
    std::vector<std::filesystem::path> out;
    out.reserve (libraryFolders.size());
    for (const auto& folder : libraryFolders)
        if (folder.enabled)
            out.push_back (folder.path);
    return out;
}

} // namespace fmlib
