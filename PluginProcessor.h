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

    void triggerNote() {
      shouldGenerateSequence = true; 
    }

    bool getSequenceStatus() {return sequenceInProgress;};
    bool isGeneratorLoading();
    void configureTempo();
    void setSequence();
    
    // Delegate to Generator
    void sendPromptToGenerator(const juce::String& prompt, std::function<void(juce::String)> callback);
    
    void replaySequence() {
        DBG("replaySequence called. noteSequence size: " + juce::String((int)sequenceGenerator.getNoteSequence().size()) + ", sequenceInProgress: " + juce::String(sequenceInProgress ? "true" : "false"));
        if (!sequenceGenerator.getNoteSequence().empty() && !sequenceInProgress) {
            sequenceGenerator.resetSequence();
            shouldGenerateSequence = true;
            DBG("shouldGenerateSequence set to true");
        }
    } 
    
    juce::File createMidiFile() {
        return sequenceGenerator.createMidiFile(bpm);
    }

    int getLastGeneratedNoteCount() const {
        return sequenceGenerator.getNoteCountFromSequenceJSON();
    }
    
    // Chat history management
    void addChatEntry(const ChatEntry& entry) {
        // Protect against concurrent access from multiple message thread callbacks
        const juce::ScopedLock lock(chatHistoryLock);
        if (chatHistory.size() >= 10)
            chatHistory.erase(chatHistory.begin());
        chatHistory.push_back(entry);
    }
    
    std::vector<ChatEntry> getChatHistory() const {
        // Return a copy to avoid concurrent access issues
        const juce::ScopedLock lock(chatHistoryLock);
        return chatHistory;
    }

    void trackEvent(const juce::String& eventName, const juce::var& properties = juce::var())
    {
        analytics.trackEvent(eventName, properties);
    }

    AnalyticsService& getAnalytics() { return analytics; }

private:
    juce::StringArray getRecentPromptsForContext(int maxPromptCount) const;

    double currentSampleRate = 44100.0;

    int noteOnCountdownSamples = -1;   // samples until note-on, relative to block start
    int noteOffCountdownSamples = -1;  // samples until note-off, relative to block start

   // int triggerDelaySamples = 10;
    int noteDurationSamples = 22050; // default; recomputed from sample rate

    // Sequence generation flags 
    bool shouldGenerateSequence = false;
    bool sequenceInProgress = false; 

    // Sequence vector 
    // vector<MidiNote> noteSequence;
    Generator sequenceGenerator; 

    // Timing parameters
    double defaultBpm = 140.0;
    double bpm = defaultBpm;
    double defaultBeatsPerBar = 4.0;
    double beatsPerBar = defaultBeatsPerBar;
    
    // Chat history (persists across editor open/close)
    std::vector<ChatEntry> chatHistory;
    mutable juce::CriticalSection chatHistoryLock; // mutable to allow locking in const methods

    AnalyticsService analytics;


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KiwiPluginAudioProcessor)
};
