/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "MidiNote.h"
#include "Generator.h"
#include "ChatEntry.h"
#include "AnalyticsService.h"

using namespace std; 
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

    void triggerNote() {shouldGenerateSequence = true;}

    bool getSequenceStatus() {return sequenceInProgress;};
    bool isGeneratorLoading();
    void configureTempo();
    void setSequence();
    
    // Delegate to Generator 
    void sendPromptToGenerator(const juce::String& prompt,  const juce::StringArray& recentPrompts, std::function<void(juce::String)> callback);

    void replaySequence();

    juce::File createMidiFile();
   
    int getLastGeneratedNoteCount() const { return sequenceGenerator.getNoteCountFromSequenceJSON(); }

    // Chat history (persists across editor close/reopen - processor outlives editor)
    void addChatEntry(const ChatEntry& entry);
    std::vector<ChatEntry> getChatHistory() const;
    juce::StringArray getRecentPromptsForContext(int maxPromptCount) const;

private:

    double currentSampleRate = 44100.0; // Default sampling rate in Hz of the audio processing environment

    // Sequence generation flags 
    bool shouldGenerateSequence = false;
    bool sequenceInProgress = false; 

    Generator sequenceGenerator; // Object responsible for communicating with OpenAI API and managing note sequences

    // Timing parameters
    double defaultBpm = 140.0;
    double bpm = defaultBpm;

    // Chat history (persists across editor open/close)
    std::vector<ChatEntry> chatHistory;
    mutable juce::CriticalSection chatHistoryLock;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KiwiPluginAudioProcessor)
};
