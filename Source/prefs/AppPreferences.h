#pragma once

#include <juce_core/juce_core.h>
#include <filesystem>
#include <vector>

namespace fmlib
{

class AppPreferences
{
public:
    AppPreferences();

    void load();
    void save() const;

    void loadFromFile (const juce::File& file);
    void saveToFile (const juce::File& file) const;

    std::vector<std::filesystem::path> baseFolders;
    juce::String midiInputName;
    juce::String midiOutputName;
    int midiChannel = 1;
    int sysexPacingMs = 20;
    bool darkTheme = true;
    /** Toolbar Bank vs Single: true = bank files (slot 1..32), false = single-voice files. */
    bool bankFileView = true;
    /** When bankFileView: show section headers grouped by bank file. */
    bool groupByBank = true;
    bool showFileColumns = false;
    bool hideDuplicates = false;
    bool showTooltips = true;
    int auditionNote = 60;
    int auditionVelocity = 100;
    int auditionDurationMs = 250;
    juce::StringArray favoriteIds;

private:
    juce::File getFile() const;
};

} // namespace fmlib
