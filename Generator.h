#pragma once
#include <JuceHeader.h>
#include "MidiNote.h"

class Generator 
{
public:
    Generator();
    ~Generator();
    
    // Internal state that can be safely shared with background threads
    struct SharedState
    {
        std::atomic<bool> isValid{true};
        juce::CriticalSection lock;
    };
    
    // Send text to OpenAI API and get response via callback
    void sendToGenerator(const juce::String& prompt, std::function<void(juce::String)> callback);
    void extractSequence(double bpm, double sampleRate);
    void processSequence(int blockSize, juce::MidiBuffer& midiMessages);
    bool isSequenceFinished();
    bool getLoadingStatus() const { return loading; }
    void resetSequence();
    juce::File createMidiFile(double bpm);
    int getNoteCountFromSequenceJSON() const;

    std::vector<MidiNote> getNoteSequence() const { return noteSequence; }

private:
    void getSequenceJSON(const juce::String& apiResponse);
  juce::String loadApiKey() const;
    
  juce::String apiKey;

    const juce::String apiInstructions = R"(
    You are a music theory-aware assistant that generates MIDI note sequences for a DAW plugin.

    You MUST return ONLY valid JSON in the exact format below.
    Do NOT include explanations, comments, or extra text.

    Output format:
    {
      "notes": [
        {
          "start_beats": 0.0,
          "duration_beats": 0.5,
          "midi_note": 60,
          "velocity": 100
        }
      ]
    }

    Timing rules:
    - 1 beat = 1 quarter note
    - start_beats and duration_beats are in beats
    - Notes should align to a regular rhythmic grid unless syncopation is intentional
    - Do NOT skip beats unless musically justified

    Pitch rules:
    - midi_note must be an integer between 0 and 127
    - velocity must be an integer between 1 and 127
    - If a key or scale is specified, ALL midi_note values MUST belong to that scale
    - Use correct music theory for scales and chords

    Harmony rules:
    - Chords are represented as multiple notes with the same start_beats
    - Chord tones must belong to the specified key
    - Avoid dissonant or out-of-key notes unless explicitly requested

    Structure rules:
    - Generate a coherent musical phrase (e.g., 1-6 bars)
    - Melody and chords should feel intentional and related
    - Avoid random or erratic note placement

    Validation requirement:
    - Before outputting JSON, internally verify that:
      - All notes obey the requested scale/key
      - Timing values are logical and consistent
      - The sequence matches the user's musical request

    Creativity rule:
    - Be musically creative ONLY within the constraints above.
    )";

    juce::String apiEndpoint = "https://api.openai.com/v1/responses";
    juce::String sequenceJSON; 
    std::vector<MidiNote> noteSequence; 
    std::vector<juce::File> createdMidiFiles; // Track files for cleanup
    int sequenceTracker = 0; 
    int triggerDelaySamples = 10;
    int scheduledMidiChannel = 1;
    bool loading = false; 
    std::shared_ptr<SharedState> sharedState;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Generator)
};
