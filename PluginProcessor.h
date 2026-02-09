/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class KiwiPluginAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    KiwiPluginAudioProcessor();
    ~KiwiPluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;
    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void triggerNote() {
      shouldGenerateSequence = true;
      sequenceInProgress = false;
    }

    bool getSequenceStatus() {return sequenceInProgress;};

private:
    int activeNoteCount = 0;
    double currentSampleRate = 44100.0;

    // MIDI note scheduling (spans across blocks)
    int scheduledMidiChannel = 1;
    int scheduledMidiNote = 60; // Middle C
    juce::uint8 scheduledVelocity = (juce::uint8) 100;

    int noteOnCountdownSamples = -1;   // samples until note-on, relative to block start
    int noteOffCountdownSamples = -1;  // samples until note-off, relative to block start

    int triggerDelaySamples = 10;
    int noteDurationSamples = 22050; // default; recomputed from sample rate

    // Sequence generation flags 
    bool shouldGenerateSequence = false;
    bool sequenceInProgress = false; 

    // Default timing parameters 
    double defaultBpm = 140.0;
    double defaultBeatsPerBar = 4.0;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KiwiPluginAudioProcessor)
};
