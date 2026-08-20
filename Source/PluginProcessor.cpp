#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "host/PluginSessionState.h"
#include "library/AutoTagger.h"
#include "sysex/SysexParser.h"
#include "sysex/VoiceMorpher.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace
{
constexpr int kMorphLfoTimerMs = 16;
} // namespace

FmLibPlugAudioProcessor::FmLibPlugAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Params", createParameterLayout())
{
    auditionOff.owner = this;
    auditionOn.owner = this;
    controllerNoteDelay.owner = this;
    favorites.loadFrom (prefs.favoriteIds);
    tags.loadFromFile (tagsFile());
    morphPresets.loadFromFile (morphPresetsFile());
    bindMorphParameters();
    setMorphLfoRateParam (prefs.morphLfoRateHz, false);
    setMorphLfoSyncParam (prefs.morphLfoTempoSync, false);
    setMorphLfoDivisionParam (prefs.morphLfoDivision, false);
    setMorphMotionChoice (fmlib::morphMotionToChoice (prefs.morphLfoEnabled, prefs.morphNoteJumpMode), false);
    applyPreferencesToEngine();

    midi.setSysexReceivedCallback ([this, alive = alive] (const std::vector<uint8_t>& bytes)
    {
        juce::MessageManager::callAsync ([this, alive, bytes]
        {
            if (! alive->load())
                return; // Processor went away while the dump was queued.
            handleIncomingSysex (bytes);
        });
    });

    midi.setControllerNoteOnCallback ([this, alive = alive] (int note, int velocity)
    {
        auto run = [this, alive, note, velocity]
        {
            if (! alive->load())
                return;
            handleControllerNoteOn (note, velocity);
        };
        if (juce::MessageManager::getInstance()->isThisTheMessageThread())
            run();
        else
            juce::MessageManager::callAsync (std::move (run));
    });

    midi.setControllerNoteOffCallback ([this, alive = alive] (int note, int)
    {
        auto run = [this, alive, note]
        {
            if (! alive->load())
                return;
            handleControllerNoteOff (note);
        };
        if (juce::MessageManager::getInstance()->isThisTheMessageThread())
            run();
        else
            juce::MessageManager::callAsync (std::move (run));
    });
}

FmLibPlugAudioProcessor::~FmLibPlugAudioProcessor()
{
    *alive = false;
    apvts.removeParameterListener (fmlib::kParamMorphX, this);
    apvts.removeParameterListener (fmlib::kParamMorphY, this);
    apvts.removeParameterListener (fmlib::kParamLockRefX, this);
    apvts.removeParameterListener (fmlib::kParamLockRefY, this);
    apvts.removeParameterListener (fmlib::kParamMorphLfoRate, this);
    apvts.removeParameterListener (fmlib::kParamMorphLfoSync, this);
    apvts.removeParameterListener (fmlib::kParamMorphLfoDivision, this);
    apvts.removeParameterListener (fmlib::kParamMorphMotionMode, this);
    stopTimer();
    ++autoTagEpoch;
    autoTagCancel = true;
    if (autoTagWorker.joinable())
        autoTagWorker.join();
    auditionOn.stopTimer();
    auditionOff.stopTimer();
    releaseAllControllerNotes();
    if (lastAuditionNote >= 0)
        midi.sendNoteOff (lastAuditionNote);
    midi.setSysexReceivedCallback (nullptr);
    midi.setControllerNoteOnCallback (nullptr);
    midi.setControllerNoteOffCallback (nullptr);
    midi.setStatusCallback (nullptr);
    persistAllUserData();
    midi.close();
}

juce::AudioProcessorValueTreeState::ParameterLayout FmLibPlugAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { fmlib::kParamMorphX, 1 }, "Morph X",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { fmlib::kParamMorphY, 1 }, "Morph Y",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { fmlib::kParamLockRefX, 1 }, "Lock Ref X",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { fmlib::kParamLockRefY, 1 }, "Lock Ref Y",
        juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { fmlib::kParamMorphLfoRate, 1 }, "Edge LFO Rate",
        juce::NormalisableRange<float> { -1.5f, 1.5f }, 0.25f));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { fmlib::kParamMorphLfoSync, 1 }, "Edge LFO Sync", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { fmlib::kParamMorphLfoDivision, 1 }, "Edge LFO Div",
        fmlib::morphLfoDivisionLabels(), fmlib::kMorphLfoDivisionDefault));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { fmlib::kParamMorphMotionMode, 1 }, "Morph Motion",
        juce::StringArray { "Off", "Random", "Edges", "Edge LFO" }, 0));
    return layout;
}

void FmLibPlugAudioProcessor::bindMorphParameters()
{
    apvts.addParameterListener (fmlib::kParamMorphX, this);
    apvts.addParameterListener (fmlib::kParamMorphY, this);
    apvts.addParameterListener (fmlib::kParamLockRefX, this);
    apvts.addParameterListener (fmlib::kParamLockRefY, this);
    apvts.addParameterListener (fmlib::kParamMorphLfoRate, this);
    apvts.addParameterListener (fmlib::kParamMorphLfoSync, this);
    apvts.addParameterListener (fmlib::kParamMorphLfoDivision, this);
    apvts.addParameterListener (fmlib::kParamMorphMotionMode, this);
}

juce::File FmLibPlugAudioProcessor::tagsFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("FmLibPlug")
        .getChildFile ("tags.xml");
}

juce::File FmLibPlugAudioProcessor::morphPresetsFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("FmLibPlug")
        .getChildFile ("morph-presets.xml");
}

const juce::String FmLibPlugAudioProcessor::getName() const
{
    return "FmLibPlug";
}

bool FmLibPlugAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return in == juce::AudioChannelSet::mono()
        || in == juce::AudioChannelSet::stereo();
}

bool FmLibPlugAudioProcessor::showDawMidiPorts() const
{
    return wrapperType != juce::AudioProcessor::wrapperType_Standalone;
}

void FmLibPlugAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    midi.exchangeHostMidi (midiMessages);
    cacheHostTransport();
    chaseHostMorphParameters();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* FmLibPlugAudioProcessor::createEditor()
{
    return new FmLibPlugAudioProcessorEditor (*this);
}

void FmLibPlugAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    fmlib::PluginSessionState state;
    state.stateVersion = fmlib::kPluginSessionStateVersion;
    state.apvtsState = apvts.copyState();
    state.liveMorph = buildLiveMorphSnapshot();

    if (auto xml = fmlib::writePluginSessionXml (state))
    {
        juce::MemoryOutputStream mos (destData, false);
        xml->writeTo (mos);
    }
}

void FmLibPlugAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    if (auto xml = juce::XmlDocument::parse (juce::String::fromUTF8 (static_cast<const char*> (data), sizeInBytes)))
    {
        fmlib::PluginSessionState state;
        if (fmlib::readPluginSessionXml (*xml, state))
            restoreSessionFromState (state, getActiveEditor() != nullptr);
    }
}

void FmLibPlugAudioProcessor::restoreSessionFromState (const fmlib::PluginSessionState& state, bool notifyUi)
{
    if (state.apvtsState.isValid())
    {
        ++morphSelfWriteDepth;
        apvts.replaceState (state.apvtsState);
        morphSelfWriteDepth.fetch_sub (1, std::memory_order_relaxed);
    }

    applyLiveMorphPreset (state.liveMorph);
    applyMorphMotionMode (getMorphMotionChoice());

    if (allLiveCornersReady())
        applyLiveMorph (false, false);

    if (notifyUi)
        notifyMorphUiSync();
}

void FmLibPlugAudioProcessor::applyPreferencesToEngine()
{
    library.setBaseFolders (prefs.enabledLibraryFolders());
    midi.setChannel (prefs.midiChannel);
    midi.setSysexPacingMs (prefs.sysexPacingMs);
    midi.setMorphReleaseGuardMs (prefs.morphReleaseGuardMs);
    midi.setMorphStreamMode (static_cast<fmlib::MorphStreamMode> (
        juce::jlimit (0, 1, prefs.morphStreamMode)));
    // ~one param-change per morphEmitMs slice at default budget; scale gently with emit ms.
    const int budget = juce::jlimit (14, 70, (prefs.morphEmitMs <= 50 ? 56 : (prefs.morphEmitMs >= 200 ? 28 : 42)));
    midi.setMorphByteBudget (budget);
    midi.setNotesSoundingQuery ([this] { return hasSoundingNotes(); });
    if (prefs.midiInputName.isNotEmpty())
        midi.openInputByName (prefs.midiInputName);
    if (prefs.midiControllerInputName.isNotEmpty())
        midi.openControllerInputByName (prefs.midiControllerInputName);
    if (prefs.midiOutputName.isNotEmpty())
        midi.openOutputByName (prefs.midiOutputName);
    midi.setControllerThru (prefs.midiControllerThru);
    library.rescanAsync();
}

void FmLibPlugAudioProcessor::persistPreferences()
{
    prefs.favoriteIds = favorites.toStringArray();
    prefs.midiInputName = midi.getInputName();
    prefs.midiControllerInputName = midi.getControllerInputName();
    prefs.midiOutputName = midi.getOutputName();
    prefs.midiChannel = midi.getChannel();
    prefs.sysexPacingMs = midi.getSysexPacingMs();
    prefs.save();
}

void FmLibPlugAudioProcessor::persistTags()
{
    tags.saveToFile (tagsFile());
}

void FmLibPlugAudioProcessor::persistMorphPresets()
{
    morphPresets.saveToFile (morphPresetsFile());
}

void FmLibPlugAudioProcessor::persistAllUserData()
{
    persistPreferences();
    persistTags();
    persistMorphPresets();
}

bool FmLibPlugAudioProcessor::isLiveCornerSet (int corner0to3) const
{
    return juce::isPositiveAndBelow (corner0to3, 4) && liveCornerSet[corner0to3];
}

bool FmLibPlugAudioProcessor::allLiveCornersReady() const
{
    return liveCornerSet[0] && liveCornerSet[1] && liveCornerSet[2] && liveCornerSet[3];
}

void FmLibPlugAudioProcessor::setLiveCorner (int corner0to3, const fmlib::VoiceData& v, const juce::String& name)
{
    if (! juce::isPositiveAndBelow (corner0to3, 4))
        return;
    auto& voice = corner0to3 == 0 ? liveMorph.a
                : corner0to3 == 1 ? liveMorph.b
                : corner0to3 == 2 ? liveMorph.c
                : liveMorph.d;
    auto* n = corner0to3 == 0 ? &liveMorph.nameA
            : corner0to3 == 1 ? &liveMorph.nameB
            : corner0to3 == 2 ? &liveMorph.nameC
            : &liveMorph.nameD;
    voice = v;
    *n = name.toStdString();
    liveCornerSet[corner0to3] = true;
    midi.invalidateMorphBaseline();
}

void FmLibPlugAudioProcessor::clearLiveCorners()
{
    liveMorph.a = {};
    liveMorph.b = {};
    liveMorph.c = {};
    liveMorph.d = {};
    liveMorph.nameA.clear();
    liveMorph.nameB.clear();
    liveMorph.nameC.clear();
    liveMorph.nameD.clear();
    liveCornerSet[0] = liveCornerSet[1] = liveCornerSet[2] = liveCornerSet[3] = false;
    midi.invalidateMorphBaseline();
}

void FmLibPlugAudioProcessor::applyLiveMorphPreset (const fmlib::MorphPreset& p)
{
    liveMorph = p;
    liveCornerSet[0] = liveCornerSet[1] = liveCornerSet[2] = liveCornerSet[3] = true;
    pushLiveMorphToApvts();
    morphLastLfoStep = -1;
    morphLfoPhase = 0.0f;
    midi.invalidateMorphBaseline();
}

fmlib::MorphPreset FmLibPlugAudioProcessor::buildLiveMorphSnapshot() const
{
    auto p = liveMorph;
    p.posX = getMorphX();
    p.posY = getMorphY();
    p.lockRefX = getLockRefX();
    p.lockRefY = getLockRefY();
    return p;
}

void FmLibPlugAudioProcessor::setLiveLockGroups (uint32_t groups)
{
    liveMorph.lockGroups = groups & fmlib::morphLockAllGroups;
}

void FmLibPlugAudioProcessor::setMorphEgressPaused (bool paused)
{
    morphEgressPaused = paused;
    if (paused)
        stopTimer();
    else
        updateMorphMotionEngine();
}

float FmLibPlugAudioProcessor::getMorphX() const
{
    if (auto* p = apvts.getRawParameterValue (fmlib::kParamMorphX))
        return p->load();
    return 0.0f;
}

float FmLibPlugAudioProcessor::getMorphY() const
{
    if (auto* p = apvts.getRawParameterValue (fmlib::kParamMorphY))
        return p->load();
    return 0.0f;
}

float FmLibPlugAudioProcessor::getLockRefX() const
{
    if (auto* p = apvts.getRawParameterValue (fmlib::kParamLockRefX))
        return p->load();
    return 0.0f;
}

float FmLibPlugAudioProcessor::getLockRefY() const
{
    if (auto* p = apvts.getRawParameterValue (fmlib::kParamLockRefY))
        return p->load();
    return 0.0f;
}

float FmLibPlugAudioProcessor::getMorphLfoRate() const
{
    if (auto* p = apvts.getRawParameterValue (fmlib::kParamMorphLfoRate))
        return p->load();
    return 0.25f;
}

bool FmLibPlugAudioProcessor::getMorphLfoSync() const
{
    if (auto* p = apvts.getRawParameterValue (fmlib::kParamMorphLfoSync))
        return p->load() >= 0.5f;
    return false;
}

int FmLibPlugAudioProcessor::getMorphLfoDivision() const
{
    if (auto* p = apvts.getRawParameterValue (fmlib::kParamMorphLfoDivision))
        return fmlib::clampMorphLfoDivision ((int) p->load());
    return fmlib::kMorphLfoDivisionDefault;
}

int FmLibPlugAudioProcessor::getMorphMotionChoice() const
{
    if (auto* p = apvts.getRawParameterValue (fmlib::kParamMorphMotionMode))
        return (int) p->load();
    return 0;
}

void FmLibPlugAudioProcessor::setMorphPosition (float x, float y, bool beginGesture, bool endGesture)
{
    const float cx = fmlib::clampMorph01 (x);
    const float cy = fmlib::clampMorph01 (y);
    morphLastWrittenX = cx;
    morphLastWrittenY = cy;
    morphSelfWriteDepth.fetch_add (1, std::memory_order_relaxed);
    if (auto* px = apvts.getParameter (fmlib::kParamMorphX))
    {
        if (beginGesture)
            px->beginChangeGesture();
        px->setValueNotifyingHost (px->convertTo0to1 (cx));
        if (endGesture)
            px->endChangeGesture();
    }
    if (auto* py = apvts.getParameter (fmlib::kParamMorphY))
    {
        if (beginGesture)
            py->beginChangeGesture();
        py->setValueNotifyingHost (py->convertTo0to1 (cy));
        if (endGesture)
            py->endChangeGesture();
    }
    morphSelfWriteDepth.fetch_sub (1, std::memory_order_relaxed);
    liveMorph.posX = cx;
    liveMorph.posY = cy;
    applyLiveMorph (! endGesture, false);
}

void FmLibPlugAudioProcessor::publishMorphMotionPosition (float x, float y, bool dragEmit)
{
    const float cx = fmlib::clampMorph01 (x);
    const float cy = fmlib::clampMorph01 (y);
    morphLastWrittenX = cx;
    morphLastWrittenY = cy;
    morphSelfWriteDepth.fetch_add (1, std::memory_order_relaxed);
    if (auto* px = apvts.getParameter (fmlib::kParamMorphX))
        px->setValueNotifyingHost (px->convertTo0to1 (cx));
    if (auto* py = apvts.getParameter (fmlib::kParamMorphY))
        py->setValueNotifyingHost (py->convertTo0to1 (cy));
    morphSelfWriteDepth.fetch_sub (1, std::memory_order_relaxed);

    liveMorph.posX = cx;
    liveMorph.posY = cy;
    if (fmlib::isEdgeLfoMotion (getMorphMotionChoice()))
        requestApplyLiveMorph (true, false);
    else
        applyLiveMorph (dragEmit, false);
    notifyMorphUiSync();
}

void FmLibPlugAudioProcessor::handleMorphPositionHostChange (const juce::String& parameterID, float newValue)
{
    if (morphSelfWriteDepth.load (std::memory_order_relaxed) > 0)
        return;

    if (fmlib::isEdgeLfoMotion (getMorphMotionChoice()))
        return;

    if (parameterID == fmlib::kParamMorphX)
        liveMorph.posX = fmlib::clampMorph01 (newValue);
    else
        liveMorph.posY = fmlib::clampMorph01 (newValue);

    requestApplyLiveMorph (true, false);
}

void FmLibPlugAudioProcessor::requestApplyLiveMorph (bool dragEmit, bool liveAllParams)
{
    if (liveAllParams)
        morphApplyLiveAllPending = true;

    if (dragEmit && ! liveAllParams)
    {
        const auto now = (uint32_t) juce::Time::getMillisecondCounter();
        const uint32_t minMs = (uint32_t) juce::jlimit (20, 250, prefs.morphEmitMs);
        const auto last = morphLastApplyMs.load (std::memory_order_relaxed);
        if (last != 0 && (now - last) < minMs)
        {
            notifyMorphUiSync();
            return;
        }
        morphLastApplyMs.store (now, std::memory_order_relaxed);
    }

    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        const bool allParams = morphApplyLiveAllPending.exchange (false) || liveAllParams;
        applyLiveMorph (dragEmit, allParams);
        notifyMorphUiSync();
        return;
    }

    bool expected = false;
    if (! morphApplyPending.compare_exchange_strong (expected, true))
        return;

    juce::MessageManager::callAsync ([this, dragEmit, alive = alive]
    {
        if (! alive->load())
            return;
        morphApplyPending = false;
        const bool allParams = morphApplyLiveAllPending.exchange (false);
        applyLiveMorph (dragEmit, allParams);
        notifyMorphUiSync();
    });
}

void FmLibPlugAudioProcessor::chaseHostMorphParameters()
{
    if (morphSelfWriteDepth.load (std::memory_order_relaxed) > 0)
        return;
    if (fmlib::isEdgeLfoMotion (getMorphMotionChoice()))
        return;

    const float x = getMorphX();
    const float y = getMorphY();
    if (fmlib::morphPositionMatchesLastPluginWrite (x, y, morphLastAppliedX, morphLastAppliedY))
        return;
    if (fmlib::morphPositionMatchesLastPluginWrite (x, y, morphLastWrittenX, morphLastWrittenY))
        return;

    liveMorph.posX = x;
    liveMorph.posY = y;
    requestApplyLiveMorph (true, false);
}

void FmLibPlugAudioProcessor::cacheHostTransport()
{
    if (auto* head = getPlayHead())
    {
        if (auto pos = head->getPosition())
        {
            hostPlaying.store (pos->getIsPlaying(), std::memory_order_relaxed);
            if (auto bpm = pos->getBpm())
                hostBpm.store (*bpm, std::memory_order_relaxed);
            if (auto ppq = pos->getPpqPosition())
                hostPpq.store (*ppq, std::memory_order_relaxed);
            return;
        }
    }
    hostPlaying.store (false, std::memory_order_relaxed);
}

bool FmLibPlugAudioProcessor::isTransportPlaying() const
{
    return hostPlaying.load (std::memory_order_relaxed);
}

float FmLibPlugAudioProcessor::effectiveMorphLfoRateHz() const
{
    const float signedFree = getMorphLfoRate();
    if (! getMorphLfoSync())
        return signedFree;
    return fmlib::morphLfoRateHzFromTempo (hostBpm.load (std::memory_order_relaxed),
                                           getMorphLfoDivision(),
                                           signedFree);
}

void FmLibPlugAudioProcessor::setLockRefPosition (float x, float y, bool beginGesture, bool endGesture)
{
    const float cx = fmlib::clampMorph01 (x);
    const float cy = fmlib::clampMorph01 (y);
    morphSelfWriteDepth.fetch_add (1, std::memory_order_relaxed);
    if (auto* px = apvts.getParameter (fmlib::kParamLockRefX))
    {
        if (beginGesture)
            px->beginChangeGesture();
        px->setValueNotifyingHost (px->convertTo0to1 (cx));
        if (endGesture)
            px->endChangeGesture();
    }
    if (auto* py = apvts.getParameter (fmlib::kParamLockRefY))
    {
        if (beginGesture)
            py->beginChangeGesture();
        py->setValueNotifyingHost (py->convertTo0to1 (cy));
        if (endGesture)
            py->endChangeGesture();
    }
    morphSelfWriteDepth.fetch_sub (1, std::memory_order_relaxed);
    liveMorph.lockRefX = cx;
    liveMorph.lockRefY = cy;
}

void FmLibPlugAudioProcessor::setMorphLfoRateParam (float hz, bool notifyHost)
{
    const auto clamped = fmlib::clampMorphLfoRate (hz);
    if (! notifyHost)
        morphSelfWriteDepth.fetch_add (1, std::memory_order_relaxed);
    if (auto* p = apvts.getParameter (fmlib::kParamMorphLfoRate))
        p->setValueNotifyingHost (p->convertTo0to1 (clamped));
    if (! notifyHost)
        morphSelfWriteDepth.fetch_sub (1, std::memory_order_relaxed);
}

void FmLibPlugAudioProcessor::setMorphLfoSyncParam (bool sync, bool notifyHost)
{
    if (! notifyHost)
        morphSelfWriteDepth.fetch_add (1, std::memory_order_relaxed);
    if (auto* p = apvts.getParameter (fmlib::kParamMorphLfoSync))
        p->setValueNotifyingHost (sync ? 1.0f : 0.0f);
    if (! notifyHost)
        morphSelfWriteDepth.fetch_sub (1, std::memory_order_relaxed);
}

void FmLibPlugAudioProcessor::setMorphLfoDivisionParam (int division, bool notifyHost)
{
    const int d = fmlib::clampMorphLfoDivision (division);
    if (! notifyHost)
        morphSelfWriteDepth.fetch_add (1, std::memory_order_relaxed);
    if (auto* p = apvts.getParameter (fmlib::kParamMorphLfoDivision))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) d));
    if (! notifyHost)
        morphSelfWriteDepth.fetch_sub (1, std::memory_order_relaxed);
}

void FmLibPlugAudioProcessor::setMorphMotionChoice (int choice, bool notifyHost)
{
    const int c = juce::jlimit (0, 3, choice);
    if (! notifyHost)
        morphSelfWriteDepth.fetch_add (1, std::memory_order_relaxed);
    if (auto* p = apvts.getParameter (fmlib::kParamMorphMotionMode))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) c));
    if (! notifyHost)
        morphSelfWriteDepth.fetch_sub (1, std::memory_order_relaxed);
    applyMorphMotionMode (c);
}

void FmLibPlugAudioProcessor::applyMorphMotionMode (int choice)
{
    const auto mapping = fmlib::morphMotionFromChoice (choice);
    midi.setControllerNotesToCallbacksOnly (mapping.controllerNotesToCallbacksOnly);
    updateMorphMotionEngine();
}

void FmLibPlugAudioProcessor::updateMorphMotionEngine()
{
    const bool lfoOn = getMorphMotionChoice() == static_cast<int> (fmlib::MorphMotionMode::edgeLfo);
    if (lfoOn && ! morphEgressPaused && allLiveCornersReady())
    {
        if (! isTimerRunning() || getTimerInterval() != kMorphLfoTimerMs)
            startTimer (kMorphLfoTimerMs);
    }
    else if (! isTimerRunning() || getTimerInterval() == kMorphLfoTimerMs)
    {
        if (getTimerInterval() == kMorphLfoTimerMs)
            stopTimer();
    }
}

void FmLibPlugAudioProcessor::pushLiveMorphToApvts()
{
    setMorphPosition (liveMorph.posX, liveMorph.posY, false, false);
    setLockRefPosition (liveMorph.lockRefX, liveMorph.lockRefY, false, false);
}

void FmLibPlugAudioProcessor::applyLiveMorph (bool dragEmit, bool liveAllParams)
{
    if (morphEgressPaused || ! allLiveCornersReady())
        return;

    morphLastAppliedX = liveMorph.posX;
    morphLastAppliedY = liveMorph.posY;
    const auto voice = fmlib::VoiceMorpher::morph4 (liveMorph.a, liveMorph.b, liveMorph.c, liveMorph.d,
                                                    morphLastAppliedX, morphLastAppliedY, liveMorph.lockGroups,
                                                    liveMorph.lockRefX, liveMorph.lockRefY);
    // Drag/LFO: stream under budget. Click / drag-end / jump: commit (full dump when idle).
    // Lock-ref: liveAllParams so EG/level locks update a held note in Frequency-only mode.
    if (liveAllParams)
        midi.cancelMorphReleaseGuard();
    midi.sendMorphVoice (voice, ! dragEmit, liveAllParams);
    rememberEditBufferVoice (voice);
}

void FmLibPlugAudioProcessor::advanceMorphLfo (double deltaSeconds)
{
    if (! fmlib::isEdgeLfoMotion (getMorphMotionChoice())
        || morphEgressPaused || ! allLiveCornersReady())
        return;

    const float signedFree = getMorphLfoRate();
    if (std::abs (signedFree) < fmlib::kMorphLfoPauseHz)
        return;

    if (getMorphLfoSync() && isTransportPlaying())
    {
        morphLfoPhase = fmlib::morphLfoPhaseFromPpq (hostPpq.load (std::memory_order_relaxed),
                                                     getMorphLfoDivision(),
                                                     signedFree);
    }
    else
    {
        const float rate = effectiveMorphLfoRateHz();
        morphLfoPhase += (float) (deltaSeconds * (double) rate);
    }
    const float wrapped = morphLfoPhase - std::floor (morphLfoPhase);
    // Quantize to discrete perimeter steps so MIDI only updates on musical ticks.
    const int step = juce::jlimit (0, fmlib::kMorphLfoStepsPerLoop - 1,
                                   (int) std::floor (wrapped * (float) fmlib::kMorphLfoStepsPerLoop));
    if (step == morphLastLfoStep)
        return;
    morphLastLfoStep = step;

    const float qPhase = ((float) step + 0.5f) / (float) fmlib::kMorphLfoStepsPerLoop;
    float x = 0.0f, y = 0.0f;
    // Direction comes from signed lfoPhase only — do not also mirror in edgePosition.
    fmlib::morphPadEdgePosition (qPhase, true, x, y);
    publishMorphMotionPosition (x, y, true);
}

void FmLibPlugAudioProcessor::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounter();
    const double dt = morphLastLfoTimerMs == 0 ? 0.0 : (double) (now - morphLastLfoTimerMs) * 0.001;
    morphLastLfoTimerMs = now;
    advanceMorphLfo (dt);
}

bool FmLibPlugAudioProcessor::applyNoteJump()
{
    if (! allLiveCornersReady())
        return false;

    const int mode = getMorphMotionChoice();
    if (mode == static_cast<int> (fmlib::MorphMotionMode::random))
    {
        publishMorphMotionPosition (juce::Random::getSystemRandom().nextFloat(),
                                    juce::Random::getSystemRandom().nextFloat(),
                                    false);
    }
    else if (mode == static_cast<int> (fmlib::MorphMotionMode::edges))
    {
        static constexpr float kEdges[4][2] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
        const auto& e = kEdges[morphEdgeJumpIndex & 3];
        morphEdgeJumpIndex = (morphEdgeJumpIndex + 1) & 3;
        publishMorphMotionPosition (e[0], e[1], false);
    }
    else
    {
        return false;
    }

    morphLastLfoStep = -1;
    return true;
}

int FmLibPlugAudioProcessor::morphNoteLeadMs() const
{
    return juce::jmax (prefs.morphNoteSettleMs, midi.getMorphLeadEstimateMs());
}

void FmLibPlugAudioProcessor::handleControllerNoteOn (int note, int velocity)
{
    const int mode = getMorphMotionChoice();
    const bool jumpOn = mode == static_cast<int> (fmlib::MorphMotionMode::random)
                     || mode == static_cast<int> (fmlib::MorphMotionMode::edges);
    const bool firstOfPhrase = controllerHeldNotes.empty();
    controllerHeldNotes.insert (note);
    midi.syncNotesSounding (hasSoundingNotes());

    // Off / Edge LFO: notes reach the synth only via Forward controller MIDI (thru).
    if (! jumpOn)
        return;

    if (firstOfPhrase || hasPendingControllerLeadIn())
    {
        if (firstOfPhrase)
        {
            // The incoming note masks any release tail, so don't hold the dump.
            morphEgressPaused = false;
            midi.cancelMorphReleaseGuard();
            applyNoteJump();
        }
        // Morph before the note so the new voice is on the wire when it sounds.
        // Chord tones during lead-in join the same delayed batch.
        playControllerNoteAfterDelay (note, velocity, morphNoteLeadMs());
    }
    else
    {
        playControllerNote (note, velocity);
    }
}

void FmLibPlugAudioProcessor::handleControllerNoteOff (int note)
{
    controllerHeldNotes.erase (note);
    releaseControllerNote (note);
    midi.syncNotesSounding (hasSoundingNotes());
    // Next jump happens on the next note-on, with its own SysEx lead-in.
}

void FmLibPlugAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == fmlib::kParamMorphX || parameterID == fmlib::kParamMorphY)
    {
        handleMorphPositionHostChange (parameterID, newValue);
        return;
    }

    if (morphSelfWriteDepth.load (std::memory_order_relaxed) > 0)
        return;

    if (parameterID == fmlib::kParamLockRefX || parameterID == fmlib::kParamLockRefY)
    {
        if (parameterID == fmlib::kParamLockRefX)
            liveMorph.lockRefX = fmlib::clampMorph01 (newValue);
        else
            liveMorph.lockRefY = fmlib::clampMorph01 (newValue);
        requestApplyLiveMorph (true, true);
        notifyMorphUiSync();
        return;
    }

    if (parameterID == fmlib::kParamMorphMotionMode)
    {
        applyMorphMotionMode (juce::jlimit (0, 3, (int) newValue));
        notifyMorphUiSync();
        return;
    }

    if (parameterID == fmlib::kParamMorphLfoRate
        || parameterID == fmlib::kParamMorphLfoSync
        || parameterID == fmlib::kParamMorphLfoDivision)
        notifyMorphUiSync();
}

void FmLibPlugAudioProcessor::notifyMorphUiSync()
{
    if (! morphUiSync)
        return;

    const auto now = (uint32_t) juce::Time::getMillisecondCounter();
    const auto last = morphLastUiSyncMs.load (std::memory_order_relaxed);
    if (last != 0 && (now - last) < 33)
    {
        morphUiSyncDeferred.store (true, std::memory_order_relaxed);
        return;
    }

    bool expected = false;
    if (! morphUiSyncPending.compare_exchange_strong (expected, true))
        return;

    morphLastUiSyncMs.store (now, std::memory_order_relaxed);
    auto run = [this, alive = alive]
    {
        if (! alive->load())
            return;
        morphUiSyncPending.store (false, std::memory_order_relaxed);
        if (morphUiSync)
            morphUiSync();
        if (morphUiSyncDeferred.exchange (false, std::memory_order_relaxed))
            notifyMorphUiSync();
    };
    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        run();
    else
        juce::MessageManager::callAsync (std::move (run));
}

void FmLibPlugAudioProcessor::sendVoice (const fmlib::VoiceData& voice)
{
    const auto id = fmlib::contentIdFromVoice (voice);
    midi.sendVoice (voice);
    lastEditBufferVoice = voice;
    recent.push (id);
    sendGlobalsWithVoiceLoadIfEnabled();
}

void FmLibPlugAudioProcessor::sendGlobalsWithVoiceLoadIfEnabled()
{
    // Dirty-only: avoid re-writing an unchanged Get/Send snapshot on every voice load.
    if (! prefs.applyGlobalsWithVoiceLoad || ! functionBuffer.shouldApplyWithVoiceLoad())
        return;
    sendFunctionBuffer();
}

bool FmLibPlugAudioProcessor::sendFunctionBuffer()
{
    if (! functionBuffer.canBulkSend())
        return false;
    const auto ok = midi.sendPerformanceBulk (functionBuffer.getData());
    if (ok)
        functionBuffer.markSaved();
    return ok;
}

bool FmLibPlugAudioProcessor::requestFunctionDump()
{
    return midi.requestFunctionDump();
}

void FmLibPlugAudioProcessor::sendMemoryProtectOff()
{
    midi.sendTxFunctionParam (fmlib::TxFunctionParam::memoryProtect, 0);
}

void FmLibPlugAudioProcessor::rememberEditBufferVoice (const fmlib::VoiceData& voice)
{
    lastEditBufferVoice = voice;
}

void FmLibPlugAudioProcessor::auditionNote()
{
    auditionNoteAfterDelay (0);
}

void FmLibPlugAudioProcessor::auditionNoteAfterDelay (int delayMs)
{
    auditionHoldActive = false;
    playHeldNoteAfterDelay (prefs.auditionNote, prefs.auditionVelocity, delayMs);
    pendingAutoOff = true;
    pendingAutoOffMs = juce::jmax (50, prefs.auditionDurationMs);
    if (delayMs <= 0 && lastAuditionNote >= 0)
        auditionOff.startTimer (pendingAutoOffMs);
}

void FmLibPlugAudioProcessor::auditionHoldStart (int delayMs)
{
    auditionOff.stopTimer();
    auditionHoldActive = true;
    playHeldNoteAfterDelay (prefs.auditionNote, prefs.auditionVelocity, delayMs);
    pendingAutoOff = false;
}

void FmLibPlugAudioProcessor::auditionHoldStop()
{
    auditionOn.stopTimer();
    auditionOff.stopTimer();
    pendingNote = -1;
    pendingAutoOff = false;
    auditionHoldActive = false;
    if (lastAuditionNote >= 0)
    {
        midi.sendNoteOff (lastAuditionNote);
        lastAuditionNote = -1;
        midi.syncNotesSounding (hasSoundingNotes());
    }
}

void FmLibPlugAudioProcessor::playHeldNoteAfterDelay (int note, int velocity, int delayMs)
{
    const int n = juce::jlimit (0, 127, note);
    const int v = juce::jlimit (1, 127, velocity);
    auditionOff.stopTimer();
    auditionOn.stopTimer();
    if (lastAuditionNote >= 0)
    {
        midi.sendNoteOff (lastAuditionNote);
        lastAuditionNote = -1;
    }

    pendingNote = n;
    pendingVelocity = v;
    pendingAutoOff = false;

    if (delayMs <= 0)
    {
        midi.sendNoteOn (n, v);
        lastAuditionNote = n;
        pendingNote = -1;
        midi.syncNotesSounding (hasSoundingNotes());
        return;
    }

    // Pending delayed audition is not sounding yet - leave morph idle for the lead-in dump.
    midi.syncNotesSounding (hasSoundingNotes());
    auditionOn.startTimer (juce::jmax (1, delayMs));
}

void FmLibPlugAudioProcessor::playControllerNote (int note, int velocity)
{
    const int n = juce::jlimit (0, 127, note);
    const int v = juce::jlimit (1, 127, velocity);
    // If this note was queued for lead-in, drop it from the pending batch.
    if (pendingControllerNote == n)
    {
        controllerNoteDelay.stopTimer();
        pendingControllerNote = -1;
        pendingControllerChord.clear();
    }
    else
    {
        pendingControllerChord.erase (
            std::remove_if (pendingControllerChord.begin(), pendingControllerChord.end(),
                            [n] (const auto& p) { return p.first == n; }),
            pendingControllerChord.end());
    }
    if (controllerSoundingNotes.count (n) > 0)
        midi.sendNoteOff (n);
    midi.sendNoteOn (n, v);
    controllerSoundingNotes.insert (n);
    midi.syncNotesSounding (hasSoundingNotes());
}

void FmLibPlugAudioProcessor::playControllerNoteAfterDelay (int note, int velocity, int delayMs)
{
    const int n = juce::jlimit (0, 127, note);
    const int v = juce::jlimit (1, 127, velocity);
    if (delayMs <= 0 && ! controllerNoteDelay.isTimerRunning())
    {
        playControllerNote (n, v);
        return;
    }

    // Join an in-flight lead-in so chord tones share the same delayed attack.
    if (controllerNoteDelay.isTimerRunning())
    {
        for (auto& p : pendingControllerChord)
            if (p.first == n)
            {
                p.second = v;
                return;
            }
        if (pendingControllerNote == n)
        {
            pendingControllerVelocity = v;
            return;
        }
        pendingControllerChord.push_back ({ n, v });
        return;
    }

    pendingControllerNote = n;
    pendingControllerVelocity = v;
    pendingControllerChord.clear();
    // Do not mark sounding yet - morph SysEx must land while idle.
    controllerNoteDelay.startTimer (juce::jmax (1, delayMs));
    midi.syncNotesSounding (hasSoundingNotes());
}

void FmLibPlugAudioProcessor::releaseControllerNote (int note)
{
    const int n = juce::jlimit (0, 127, note);
    if (pendingControllerNote == n)
    {
        // Promote next queued chord tone, or cancel the lead-in.
        if (! pendingControllerChord.empty())
        {
            pendingControllerNote = pendingControllerChord.front().first;
            pendingControllerVelocity = pendingControllerChord.front().second;
            pendingControllerChord.erase (pendingControllerChord.begin());
        }
        else
        {
            controllerNoteDelay.stopTimer();
            pendingControllerNote = -1;
        }
        midi.syncNotesSounding (hasSoundingNotes());
        return;
    }

    const auto it = std::find_if (pendingControllerChord.begin(), pendingControllerChord.end(),
                                  [n] (const auto& p) { return p.first == n; });
    if (it != pendingControllerChord.end())
    {
        pendingControllerChord.erase (it);
        return;
    }

    if (controllerSoundingNotes.erase (n) > 0)
        midi.sendNoteOff (n);
    midi.syncNotesSounding (hasSoundingNotes());
}

void FmLibPlugAudioProcessor::releaseAllControllerNotes()
{
    controllerNoteDelay.stopTimer();
    pendingControllerNote = -1;
    pendingControllerChord.clear();
    for (int n : controllerSoundingNotes)
        midi.sendNoteOff (n);
    controllerSoundingNotes.clear();
    midi.syncNotesSounding (hasSoundingNotes());
}

bool FmLibPlugAudioProcessor::hasPendingControllerLeadIn() const
{
    return controllerNoteDelay.isTimerRunning() || pendingControllerNote >= 0
        || ! pendingControllerChord.empty();
}

bool FmLibPlugAudioProcessor::hasSoundingNotes() const
{
    // Pending delayed note-ons are reserved but not yet on the wire - keep morph idle.
    // Thru notes are OR'd inside MidiDeviceManager::syncNotesSounding.
    // Held controller keys count as sounding so morph gating works when Forward is off.
    return lastAuditionNote >= 0 || ! controllerSoundingNotes.empty()
        || ! controllerHeldNotes.empty();
}

void FmLibPlugAudioProcessor::ControllerNoteDelayTimer::timerCallback()
{
    stopTimer();
    if (owner == nullptr)
        return;

    auto fire = [owner = owner] (int n, int v)
    {
        if (owner->controllerSoundingNotes.count (n) > 0)
            owner->midi.sendNoteOff (n);
        owner->midi.sendNoteOn (n, v);
        owner->controllerSoundingNotes.insert (n);
    };

    if (owner->pendingControllerNote >= 0)
        fire (owner->pendingControllerNote, owner->pendingControllerVelocity);
    owner->pendingControllerNote = -1;

    for (const auto& p : owner->pendingControllerChord)
        fire (p.first, p.second);
    owner->pendingControllerChord.clear();

    owner->midi.syncNotesSounding (owner->hasSoundingNotes());
}

void FmLibPlugAudioProcessor::AuditionOnTimer::timerCallback()
{
    stopTimer();
    if (owner == nullptr || owner->pendingNote < 0)
        return;
    owner->midi.sendNoteOn (owner->pendingNote, owner->pendingVelocity);
    owner->lastAuditionNote = owner->pendingNote;
    owner->pendingNote = -1;
    owner->midi.syncNotesSounding (owner->hasSoundingNotes());
    if (owner->pendingAutoOff)
        owner->auditionOff.startTimer (owner->pendingAutoOffMs);
}

void FmLibPlugAudioProcessor::AuditionOffTimer::timerCallback()
{
    stopTimer();
    if (owner == nullptr)
        return;
    if (owner->lastAuditionNote >= 0)
    {
        owner->midi.sendNoteOff (owner->lastAuditionNote);
        owner->lastAuditionNote = -1;
        owner->midi.syncNotesSounding (owner->hasSoundingNotes());
    }
}

void FmLibPlugAudioProcessor::autoTagLibrary (std::function<void()> onDone)
{
    if (autoTagRunning.exchange (true))
        return;

    autoTagCancel = false;
    const auto epoch = ++autoTagEpoch;
    if (autoTagWorker.joinable())
        autoTagWorker.join();

    autoTagWorker = std::thread ([this, epoch, alive = alive, onDone = std::move (onDone)]
    {
        const auto entries = library.getEntriesCopy();
        std::vector<std::pair<uint64_t, std::vector<std::string>>> batch;
        batch.reserve (entries.size());
        for (const auto& e : entries)
        {
            if (autoTagCancel.load() || autoTagEpoch.load() != epoch)
                break;
            batch.emplace_back (e.contentId, fmlib::AutoTagger::tagsForVoice (e.voice, e.voiceName));
        }

        // The worker is joined in the destructor, but this batch can still be queued behind it.
        juce::MessageManager::callAsync ([this, epoch, alive, batch = std::move (batch), onDone]
        {
            if (! alive->load())
                return;
            if (autoTagEpoch.load() != epoch)
            {
                autoTagRunning = false;
                return;
            }
            // Merge only: never wipe tags the user set manually via the Tags editor.
            for (const auto& [id, tagList] : batch)
                for (const auto& t : tagList)
                    tags.addTag (id, t);
            tags.saveToFile (tagsFile());
            autoTagRunning = false;
            if (onDone)
                onDone();
        });
    });
}

void FmLibPlugAudioProcessor::handleIncomingSysex (const std::vector<uint8_t>& bytes)
{
    if (fmlib::Tx7Performance::looksLikePerformanceBulk (bytes.data(), bytes.size()))
    {
        if (auto perf = fmlib::Tx7Performance::parsePerformanceBulk (bytes))
        {
            functionBuffer.setFromDevice (std::move (*perf));
            midi.reportStatus ("Received TX7 function / performance");
            if (functionBufferChanged)
                functionBufferChanged();
        }
        else
        {
            midi.reportStatus ("TX7 function dump rejected (bad checksum)");
        }
        return;
    }

    const auto parsed = fmlib::SysexParser::parseBytes (bytes);
    if (parsed.voices.empty())
        return;

    std::vector<fmlib::PatchEntry> entries;
    for (const auto& v : parsed.voices)
    {
        fmlib::PatchEntry e;
        e.voice = v.data;
        e.bankSlot = v.bankSlot;
        e.voiceName = fmlib::voiceNameFromData (v.data);
        e.fileName = "device";
        e.relativePath = "Device";
        e.contentId = fmlib::contentIdFromVoice (v.data);
        entries.push_back (std::move (e));
    }
    deviceBuffer.setVoices (std::move (entries));
    // Device dump replaces the edit buffer / memory view - morph diffs need a fresh baseline.
    midi.invalidateMorphBaseline();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FmLibPlugAudioProcessor();
}
