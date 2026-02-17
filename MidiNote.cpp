#include "MidiNote.h"

MidiNote::MidiNote(MidiNoteEvent note, int onSamples, int offSamples):
    note(note), 
    noteOnCountdownSamples(onSamples), 
    noteOffCountdownSamples(offSamples),
    originalOnSamples(onSamples),
    originalOffSamples(offSamples)
{
    jassert(offSamples > onSamples); // Assertion failure in debug build 
}

void MidiNote::processNote(int blockSize, juce::MidiBuffer& midiMessages) {
    noteOn(blockSize, midiMessages);
    noteOff(blockSize, midiMessages); 
}

void MidiNote::reset() {
    noteOnCountdownSamples = originalOnSamples;
    noteOffCountdownSamples = originalOffSamples;
    counted = false;
}

void MidiNote::noteOn(int blockSize, juce::MidiBuffer& midiMessages) {
    if(noteOnCountdownSamples >= 0 && noteOnCountdownSamples < blockSize) {
        midiMessages.addEvent(
            juce::MidiMessage::noteOn(note.midiChannel, note.note, note.velocity),
            noteOnCountdownSamples
        );
        noteOnCountdownSamples = -1; // Note has started 
    }
    else if (noteOnCountdownSamples >= blockSize) { // Note starts in a future block 
        noteOnCountdownSamples -= blockSize; // Block is consumed
    }
}

void MidiNote::noteOff(int blockSize, juce::MidiBuffer& midiMessages) {
    if(noteOffCountdownSamples >= 0 && noteOffCountdownSamples < blockSize) {
        midiMessages.addEvent(
            juce::MidiMessage::noteOff(note.midiChannel, note.note),
            noteOffCountdownSamples
        );
        noteOffCountdownSamples = -1; // Note has ended 
    }
    else if (noteOffCountdownSamples >= blockSize) { // Note ends in a future block 
        noteOffCountdownSamples -= blockSize; // Block is consumed 
    }
}
