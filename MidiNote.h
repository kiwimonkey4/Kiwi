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
        int noteOnCountdownSamples; // How many samples from now until the next note-on event should be triggered
        int noteOffCountdownSamples; // How many samples from now until the next note-off event should be triggered

        // Original timing parameters for resetting the sequence -> essential for replay functionality
        int originalOnSamples; 
        int originalOffSamples; 

        bool counted = false; // Keeps track of whether note has finished playing and been counted towards sequence completion to avoid

        void noteOn(int blockSize, juce::MidiBuffer& midiMessages);
        
        void noteOff(int blockSize, juce::MidiBuffer& midiMessages);
};