#pragma once
#include <JuceHeader.h>

class MidiFileDragComponent : public juce::Component
{
public:
    void setMidiFile(const juce::File& file);
    
    void paint(juce::Graphics& g) override;
    
    void mouseDrag(const juce::MouseEvent& e) override;
    
private:
    juce::File midiFile;
};
