#include "ui/DeviceBufferPanel.h"
#include "util/StringUtils.h"

namespace fmlib
{

DeviceBufferPanel::DeviceBufferPanel()
{
    model.owner = this;
    list.setModel (&model);
    list.setWantsKeyboardFocus (true);
    list.setMultipleSelectionEnabled (false);
    list.setRowHeight (20);
    addAndMakeVisible (title);
    addAndMakeVisible (req1);
    addAndMakeVisible (req32);
    addAndMakeVisible (send);
    addAndMakeVisible (save);
    addAndMakeVisible (clear);
    addAndMakeVisible (list);

    title.setFont (juce::FontOptions (16.0f, juce::Font::bold));

    req1.onClick = [this] { if (onRequest1) onRequest1(); };
    req32.onClick = [this] { if (onRequest32) onRequest32(); };
    send.onClick = [this] { if (onSend) onSend(); };
    save.onClick = [this] { if (onSave) onSave(); };
    clear.onClick = [this] { if (onClear) onClear(); };

    send.setTooltip ("Send 1 voice, or a full 32-voice bank if the buffer has 32 slots.");
    req1.setTooltip ("Get a 1-voice dump from the device.");
    req32.setTooltip ("Get a 32-voice bank dump from the device.");
}

void DeviceBufferPanel::setBuffer (DeviceBuffer* buffer)
{
    buf = buffer;
    lastSentRow = -1;
    refreshList();
}

void DeviceBufferPanel::reportStatus (const juce::String& s)
{
    if (onStatus)
        onStatus (s);
}

void DeviceBufferPanel::refreshList()
{
    lastSentRow = -1;
    list.updateContent();
    list.repaint();
}

void DeviceBufferPanel::resized()
{
    auto r = getLocalBounds().reduced (4);
    title.setBounds (r.removeFromTop (22));
    auto buttons = r.removeFromTop (28);
    const int w = buttons.getWidth() / 5;
    req1.setBounds (buttons.removeFromLeft (w).reduced (2));
    req32.setBounds (buttons.removeFromLeft (w).reduced (2));
    send.setBounds (buttons.removeFromLeft (w).reduced (2));
    save.setBounds (buttons.removeFromLeft (w).reduced (2));
    clear.setBounds (buttons.reduced (2));
    r.removeFromTop (4);
    list.setBounds (r);
}

void DeviceBufferPanel::paintOverChildren (juce::Graphics& g)
{
    if (! dragOver || dropHighlightRow < 0)
        return;
    auto rowBounds = list.getRowPosition (dropHighlightRow, true);
    rowBounds = getLocalArea (&list, rowBounds);
    g.setColour (findColour (juce::TextButton::buttonOnColourId).withAlpha (0.35f));
    g.fillRect (rowBounds);
    g.setColour (findColour (juce::Label::textColourId));
    g.drawRect (rowBounds, 1);
}

int DeviceBufferPanel::rowAtDrag (const SourceDetails& details) const
{
    auto pos = list.getLocalPoint (this, details.localPosition);
    return list.getRowContainingPosition (pos.x, pos.y);
}

bool DeviceBufferPanel::isInterestedInDragSource (const SourceDetails& dragSourceDetails)
{
    return dragSourceDetails.description.toString() == "fmlib-voice";
}

void DeviceBufferPanel::itemDragEnter (const SourceDetails& details)
{
    dragOver = true;
    dropHighlightRow = rowAtDrag (details);
    // Do not expand/mutate the buffer until drop — cancel must leave Get-1 intact.
    repaint();
}

void DeviceBufferPanel::itemDragMove (const SourceDetails& details)
{
    dragOver = true;
    dropHighlightRow = rowAtDrag (details);
    repaint();
}

void DeviceBufferPanel::itemDragExit (const SourceDetails&)
{
    dragOver = false;
    dropHighlightRow = -1;
    repaint();
}

void DeviceBufferPanel::itemDropped (const SourceDetails& details)
{
    const int row = rowAtDrag (details);
    dragOver = false;
    dropHighlightRow = -1;
    repaint();

    if (buf == nullptr || ! onQueryDragVoice)
        return;
    auto voice = onQueryDragVoice();
    if (! voice)
        return;

    buf->ensureEditableBank();
    int slot = row;
    if (slot < 0)
        slot = 0;
    if (slot >= kBankVoiceCount)
        slot = kBankVoiceCount - 1;
    buf->setSlot (slot, *voice);
    refreshList();
    list.selectRow (slot, false, false);
    reportStatus ("Dropped into slot " + juce::String (slot + 1));
}

void DeviceBufferPanel::loadRow (int row)
{
    if (buf == nullptr || ! onLoadVoice)
        return;
    const auto& voices = buf->getVoices();
    if (! juce::isPositiveAndBelow (row, static_cast<int> (voices.size())))
        return;
    if (row == lastSentRow)
        return;
    lastSentRow = row;
    onLoadVoice (voices[static_cast<size_t> (row)]);
}

int DeviceBufferPanel::Model::getNumRows()
{
    return owner != nullptr && owner->buf != nullptr ? static_cast<int> (owner->buf->getVoices().size()) : 0;
}

void DeviceBufferPanel::Model::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (owner == nullptr || owner->buf == nullptr)
        return;
    const auto& voices = owner->buf->getVoices();
    if (! juce::isPositiveAndBelow (row, static_cast<int> (voices.size())))
        return;
    g.fillAll (selected ? owner->findColour (juce::TextButton::buttonOnColourId) : juce::Colours::transparentBlack);
    g.setColour (owner->findColour (juce::Label::textColourId));
    const auto& e = voices[static_cast<size_t> (row)];
    auto label = toJuce (e.voiceName);
    if (e.bankSlot > 0)
        label = juce::String (e.bankSlot) + ". " + label;
    g.drawText (label, 4, 0, w - 8, h, juce::Justification::centredLeft, true);
}

void DeviceBufferPanel::Model::selectedRowsChanged (int lastRowSelected)
{
    if (owner == nullptr)
        return;
    owner->loadRow (lastRowSelected);
}

void DeviceBufferPanel::Model::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (owner == nullptr)
        return;
    owner->list.grabKeyboardFocus();
    if (owner->onListFocused)
        owner->onListFocused();
    if (row == owner->list.getSelectedRow())
    {
        owner->lastSentRow = -1;
        owner->loadRow (row);
    }
}

} // namespace fmlib
