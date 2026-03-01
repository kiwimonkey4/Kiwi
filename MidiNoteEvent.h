#pragma once
#include <JuceHeader.h>

struct MidiNoteEvent {
    int midiChannel; // MIDI channel for the note event (1-16)
    int note; // MIDI note number (0-127)
    juce::uint8 velocity; // How hard the note is played (0-127)
};