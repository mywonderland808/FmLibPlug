#pragma once

#include "library/DeviceBuffer.h"
#include "library/FavoritesStore.h"
#include "library/MorphPresetStore.h"
#include "library/PatchLibrary.h"
#include "library/RecentStore.h"
#include "library/TagStore.h"
#include "midi/MidiDeviceManager.h"
#include "prefs/AppPreferences.h"
#include <juce_audio_processors/juce_audio_processors.h>

class FmLibPlugAudioProcessor : public juce::AudioProcessor
{
public:
    FmLibPlugAudioProcessor();
    ~FmLibPlugAudioProcessor() override;

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    fmlib::AppPreferences prefs;
    fmlib::PatchLibrary library;
    fmlib::DeviceBuffer deviceBuffer;
    fmlib::FavoritesStore favorites;
    fmlib::TagStore tags;
    fmlib::RecentStore recent;
    fmlib::MorphPresetStore morphPresets;
    fmlib::MidiDeviceManager midi;

    void applyPreferencesToEngine();
    void persistPreferences();
    void handleIncomingSysex (const std::vector<uint8_t>& bytes);
    void sendVoice (const fmlib::VoiceData& voice);
    void auditionNote();
    void autoTagLibrary();
    juce::File tagsFile() const;
    juce::File morphPresetsFile() const;

private:
    class AuditionOffTimer : public juce::Timer
    {
    public:
        FmLibPlugAudioProcessor* owner = nullptr;
        void timerCallback() override;
    } auditionOff;

    int lastAuditionNote = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FmLibPlugAudioProcessor)
};
