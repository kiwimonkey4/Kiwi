#pragma once 
#include "MidiNoteEvent.h"

class MidiNote { 
    public:
        MidiNote(MidiNoteEvent note, int onSamples, int offSamples);

        ~MidiNote() = default;

        bool isFinished() const { return noteOffCountdownSamples < 0 && noteOnCountdownSamples < 0;}
        
        bool hasBeenCounted() const { return counted; }
        void markAsCounted() { counted = true; }

        void processNote(int blockSize, juce::MidiBuffer& midiMessages);
        
        void reset();

    private:
        MidiNoteEvent note; 
        int noteOnCountdownSamples;
        int noteOffCountdownSamples;
        int originalOnSamples;
        int originalOffSamples;
        bool counted = false;

        void noteOn(int blockSize, juce::MidiBuffer& midiMessages);
        
        void noteOff(int blockSize, juce::MidiBuffer& midiMessages);
};