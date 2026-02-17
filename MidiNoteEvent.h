#pragma once
#include <JuceHeader.h>

struct MidiNoteEvent {
    int midiChannel;
    int note;
    juce::uint8 velocity;
};