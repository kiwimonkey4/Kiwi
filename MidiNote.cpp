#include "MidiNote.h"

/**
 * @brief Constructor for the MidiNote class, which initializes the note event and timing parameters
 * @param note The MIDI note event containing channel, note number, and velocity
 * @param onSamples The number of samples until the note-on event should be triggered
 * @param offSamples The number of samples until the note-off event should be triggered
 */
MidiNote::MidiNote(MidiNoteEvent note, int onSamples, int offSamples):
    note(note), 
    noteOnCountdownSamples(onSamples), 
    noteOffCountdownSamples(offSamples),
    originalOnSamples(onSamples),
    originalOffSamples(offSamples)
{
    jassert(offSamples > onSamples); // Assertion failure in debug build 
}

/**
 * @brief Handles the processing of the MIDI note within the audio processing block, including triggering note-on and note-off events at the correct times
 * @param blockSize The size of the current audio processing block in samples
 * @param midiMessages The MIDI buffer to which note events should be added when triggered
 */
void MidiNote::processNote(int blockSize, juce::MidiBuffer& midiMessages) {
    noteOn(blockSize, midiMessages);
    noteOff(blockSize, midiMessages); 
}

/**
 * @brief Resets the MIDI note's timing parameters to their original values, allowing the note to be replayed from the beginning of its sequence
 */
void MidiNote::reset() {
    noteOnCountdownSamples = originalOnSamples;
    noteOffCountdownSamples = originalOffSamples;
    counted = false;
}

/**
 * @brief Handles the logic for triggering a MIDI note-on event at the correct time within the audio processing block
 * @param blockSize The size of the current audio processing block in samples
 * @param midiMessages The MIDI buffer to which the note-on event should be added when triggered
 */
void MidiNote::noteOn(int blockSize, juce::MidiBuffer& midiMessages) {

    // Checks if note should be triggered in the current block 
    if(noteOnCountdownSamples >= 0 && noteOnCountdownSamples < blockSize) {

        // Adds the note-on event to the MIDI buffer
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

/**
 * @brief Handles the logic for triggering a MIDI note-off event at the correct time within the audio processing block
 * @param blockSize The size of the current audio processing block in samples
 * @param midiMessages The MIDI buffer to which the note-off event should be added when triggered
 */

void MidiNote::noteOff(int blockSize, juce::MidiBuffer& midiMessages) {

    // Checks if note should be turned off in the current block 
    if(noteOffCountdownSamples >= 0 && noteOffCountdownSamples < blockSize) {

        // Adds the note-off event to the MIDI buffer
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
