#include "ui/MorpherPanel.h"
#include "sysex/VoiceMorpher.h"

namespace fmlib
{

MorpherPanel::MorpherPanel()
{
    modeMorph.setClickingTogglesState (true);
    modePresets.setClickingTogglesState (true);
    modeMorph.setToggleState (true, juce::dontSendNotification);
    modeMorph.onClick = [this] { setMode (Mode::morph); };
    modePresets.onClick = [this] { setMode (Mode::presets); };
    addAndMakeVisible (modeMorph);
    addAndMakeVisible (modePresets);

    auto wireAssign = [this] (juce::TextButton& b, int corner)
    {
        b.onClick = [this, corner]
        {
            if (onRequestAssignCorner)
                onRequestAssignCorner (corner);
        };
        addAndMakeVisible (b);
    };
    wireAssign (assignA, 0);
    wireAssign (assignB, 1);
    wireAssign (assignC, 2);
    wireAssign (assignD, 3);
    clearBtn.onClick = [this] { clearCorners(); };
    addAndMakeVisible (clearBtn);

    hint.setJustificationType (juce::Justification::centredLeft);
    hint.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (hint);

    presetName.setTextToShowWhenEmpty ("Preset name", juce::Colours::grey);
    addAndMakeVisible (presetName);
    addAndMakeVisible (savePreset);
    addAndMakeVisible (loadPreset);
    addAndMakeVisible (deletePreset);
    addAndMakeVisible (presetList);
    presetList.setVisible (false);
    presetName.setVisible (false);
    savePreset.setVisible (false);
    loadPreset.setVisible (false);
    deletePreset.setVisible (false);

    savePreset.onClick = [this] { saveCurrentPreset(); };
    loadPreset.onClick = [this] { loadSelectedPreset(); };
    deletePreset.onClick = [this] { deleteSelectedPreset(); };
}

void MorpherPanel::setMode (Mode m)
{
    mode = m;
    const bool morph = mode == Mode::morph;
    modeMorph.setToggleState (morph, juce::dontSendNotification);
    modePresets.setToggleState (! morph, juce::dontSendNotification);

    for (auto* c : { &assignA, &assignB, &assignC, &assignD, &clearBtn })
        c->setVisible (morph);
    hint.setVisible (morph);

    presetList.setVisible (! morph);
    presetName.setVisible (! morph);
    savePreset.setVisible (! morph);
    loadPreset.setVisible (! morph);
    deletePreset.setVisible (! morph);
    if (! morph)
        refreshPresetList();
    resized();
    repaint();
}

juce::String MorpherPanel::cornerDisplayName (int corner) const
{
    if (corner < 0 || corner > 3)
        return {};
    if (! cornerSet[corner] || cornerNames[corner].isEmpty())
        return juce::String::charToString (static_cast<juce::juce_wchar> ('A' + corner)) + " (unset)";
    return cornerNames[corner];
}

void MorpherPanel::setCorner (int corner0to3, const VoiceData& v, const juce::String& name)
{
    if (corner0to3 < 0 || corner0to3 > 3)
        return;
    corners[corner0to3] = v;
    cornerSet[corner0to3] = true;
    cornerNames[corner0to3] = name.isNotEmpty() ? name : "Voice";
    emitMorph();
    repaint();
}

void MorpherPanel::clearCorners()
{
    for (int i = 0; i < 4; ++i)
    {
        corners[i] = {};
        cornerSet[i] = false;
        cornerNames[i].clear();
    }
    posX = 0.0f;
    posY = 0.0f;
    repaint();
}

void MorpherPanel::setMarkerPosition (float x, float y)
{
    posX = juce::jlimit (0.0f, 1.0f, x);
    posY = juce::jlimit (0.0f, 1.0f, y);
    emitMorph();
    repaint();
}

void MorpherPanel::setPresetStore (MorphPresetStore* s)
{
    store = s;
    refreshPresetList();
}

MorphPreset MorpherPanel::currentAsPreset (const juce::String& name) const
{
    MorphPreset p;
    p.name = name.toStdString();
    p.a = corners[0];
    p.b = corners[1];
    p.c = corners[2];
    p.d = corners[3];
    p.nameA = cornerNames[0].toStdString();
    p.nameB = cornerNames[1].toStdString();
    p.nameC = cornerNames[2].toStdString();
    p.nameD = cornerNames[3].toStdString();
    p.posX = posX;
    p.posY = posY;
    return p;
}

void MorpherPanel::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto modeRow = r.removeFromTop (28);
    modeMorph.setBounds (modeRow.removeFromLeft (90).reduced (2));
    modePresets.setBounds (modeRow.removeFromLeft (90).reduced (2));
    r.removeFromTop (6);

    if (mode == Mode::morph)
    {
        auto assigns = r.removeFromTop (28);
        assignA.setBounds (assigns.removeFromLeft (64).reduced (1));
        assignB.setBounds (assigns.removeFromLeft (64).reduced (1));
        assignC.setBounds (assigns.removeFromLeft (64).reduced (1));
        assignD.setBounds (assigns.removeFromLeft (64).reduced (1));
        clearBtn.setBounds (assigns.removeFromLeft (70).reduced (1));
        hint.setBounds (r.removeFromTop (22));
        padBounds = r.reduced (8);
    }
    else
    {
        auto row = r.removeFromTop (28);
        presetName.setBounds (row.removeFromLeft (220).reduced (1));
        savePreset.setBounds (row.removeFromLeft (110).reduced (1));
        loadPreset.setBounds (row.removeFromLeft (70).reduced (1));
        deletePreset.setBounds (row.removeFromLeft (80).reduced (1));
        r.removeFromTop (4);
        presetList.setBounds (r);
        padBounds = {};
    }
}

void MorpherPanel::paint (juce::Graphics& g)
{
    if (mode != Mode::morph || padBounds.isEmpty())
        return;

    g.setColour (findColour (juce::TextEditor::backgroundColourId));
    g.fillRect (padBounds);
    g.setColour (findColour (juce::Label::textColourId));
    g.drawRect (padBounds, 1);

    const float x = padBounds.getX() + posX * (float) padBounds.getWidth();
    const float y = padBounds.getY() + posY * (float) padBounds.getHeight();
    g.setColour (findColour (juce::TextButton::buttonOnColourId));
    g.fillEllipse (x - 7.0f, y - 7.0f, 14.0f, 14.0f);

    g.setColour (findColour (juce::Label::textColourId));
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    const int labelW = juce::jmax (80, padBounds.getWidth() / 2 - 12);
    const int labelH = 36;
    g.drawText (cornerDisplayName (0), padBounds.getX() + 6, padBounds.getY() + 6, labelW, labelH,
                juce::Justification::topLeft, true);
    g.drawText (cornerDisplayName (1), padBounds.getRight() - labelW - 6, padBounds.getY() + 6, labelW, labelH,
                juce::Justification::topRight, true);
    g.drawText (cornerDisplayName (2), padBounds.getX() + 6, padBounds.getBottom() - labelH - 6, labelW, labelH,
                juce::Justification::bottomLeft, true);
    g.drawText (cornerDisplayName (3), padBounds.getRight() - labelW - 6, padBounds.getBottom() - labelH - 6, labelW, labelH,
                juce::Justification::bottomRight, true);
}

void MorpherPanel::mouseDown (const juce::MouseEvent& e)
{
    if (mode == Mode::morph)
        updateFromPoint (e.position, true);
}

void MorpherPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (mode == Mode::morph)
        updateFromPoint (e.position, false);
}

void MorpherPanel::mouseUp (const juce::MouseEvent& e)
{
    if (mode == Mode::morph)
        updateFromPoint (e.position, true);
}

void MorpherPanel::updateFromPoint (juce::Point<float> p, bool forceEmit)
{
    if (padBounds.isEmpty())
        return;
    posX = juce::jlimit (0.0f, 1.0f, (p.x - (float) padBounds.getX()) / (float) padBounds.getWidth());
    posY = juce::jlimit (0.0f, 1.0f, (p.y - (float) padBounds.getY()) / (float) padBounds.getHeight());

    const auto now = juce::Time::getMillisecondCounter();
    if (! forceEmit && (now - lastMorphEmitMs) < morphEmitMinMs)
    {
        repaint();
        return;
    }
    lastMorphEmitMs = now;
    emitMorph();
    repaint();
}

void MorpherPanel::emitMorph()
{
    if (! onMorph)
        return;

    int base = -1;
    for (int i = 0; i < 4; ++i)
    {
        if (cornerSet[static_cast<size_t> (i)])
        {
            base = i;
            break;
        }
    }
    if (base < 0)
        return;

    // Unset corners fall back to the first assigned corner (never zeroed INIT data).
    const auto& baseVoice = corners[static_cast<size_t> (base)];
    const auto& a = cornerSet[0] ? corners[0] : baseVoice;
    const auto& b = cornerSet[1] ? corners[1] : a;
    const auto& c = cornerSet[2] ? corners[2] : a;
    const auto& d = cornerSet[3] ? corners[3] : b;
    onMorph (VoiceMorpher::morph4 (a, b, c, d, posX, posY));
}

void MorpherPanel::refreshPresetList()
{
    presetList.updateContent();
    presetList.repaint();
}

void MorpherPanel::saveCurrentPreset()
{
    if (store == nullptr)
        return;
    auto name = presetName.getText();
    if (name.isEmpty())
        name = "Morph set";
    store->add (currentAsPreset (name));
    if (onPresetsChanged)
        onPresetsChanged();
    refreshPresetList();
}

void MorpherPanel::loadSelectedPreset()
{
    if (store == nullptr)
        return;
    const int row = presetList.getSelectedRow();
    if (! juce::isPositiveAndBelow (row, static_cast<int> (store->items().size())))
        return;
    const auto& p = store->items()[static_cast<size_t> (row)];
    setCorner (0, p.a, juce::String::fromUTF8 (p.nameA.c_str()));
    setCorner (1, p.b, juce::String::fromUTF8 (p.nameB.c_str()));
    setCorner (2, p.c, juce::String::fromUTF8 (p.nameC.c_str()));
    setCorner (3, p.d, juce::String::fromUTF8 (p.nameD.c_str()));
    setMarkerPosition (p.posX, p.posY);
    setMode (Mode::morph);
}

void MorpherPanel::deleteSelectedPreset()
{
    if (store == nullptr)
        return;
    store->removeAt (presetList.getSelectedRow());
    if (onPresetsChanged)
        onPresetsChanged();
    refreshPresetList();
}

int MorpherPanel::getNumRows()
{
    return store != nullptr ? static_cast<int> (store->items().size()) : 0;
}

void MorpherPanel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (store == nullptr || ! juce::isPositiveAndBelow (row, static_cast<int> (store->items().size())))
        return;
    g.fillAll (selected ? findColour (juce::TextEditor::highlightColourId)
                         : findColour (juce::ListBox::backgroundColourId));
    g.setColour (findColour (juce::Label::textColourId));
    const auto& p = store->items()[static_cast<size_t> (row)];
    auto label = juce::String::fromUTF8 (p.name.c_str())
               + "  (" + juce::String (p.posX, 2) + ", " + juce::String (p.posY, 2) + ")";
    g.drawText (label, 4, 0, w - 8, h, juce::Justification::centredLeft, true);
}

void MorpherPanel::listBoxItemDoubleClicked (int, const juce::MouseEvent&)
{
    loadSelectedPreset();
}

} // namespace fmlib
