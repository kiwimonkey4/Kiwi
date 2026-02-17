#include "MidiFileDragComponent.h"

void MidiFileDragComponent::setMidiFile(const juce::File& file)
{
    midiFile = file;
    repaint();
}

void MidiFileDragComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);
    g.setColour(juce::Colours::white);
    if (midiFile.existsAsFile())
        g.drawText(midiFile.getFileName(), getLocalBounds(), juce::Justification::centred);
    else
        g.drawText("No MIDI file", getLocalBounds(), juce::Justification::centred);
}

void MidiFileDragComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (midiFile.existsAsFile() && e.mouseWasDraggedSinceMouseDown())
    {
        juce::StringArray files;
        files.add(midiFile.getFullPathName());
        juce::DragAndDropContainer::performExternalDragDropOfFiles(files, true);
    }
}
