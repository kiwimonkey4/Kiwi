/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "MidiNote.h"
#include "Generator.h"
#include "ChatEntry.h"
#include "AnalyticsService.h"

using namespace std; 

//==============================================================================
KiwiPluginAudioProcessor::KiwiPluginAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

KiwiPluginAudioProcessor::~KiwiPluginAudioProcessor()
{
}

//==============================================================================
const juce::String KiwiPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool KiwiPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool KiwiPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool KiwiPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double KiwiPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int KiwiPluginAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int KiwiPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void KiwiPluginAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String KiwiPluginAudioProcessor::getProgramName (int index)
{
    return {};
}

void KiwiPluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

/**
 * @brief Pre-playback initialization which is called when sample rate changes 
 * @param sampleRate The new sample rate in Hz
 * @param samplesPerBlock The new block size in samples for audio processing
 */
void KiwiPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback 
    // initialisation that you need..

    juce::ignoreUnused (samplesPerBlock);
    currentSampleRate = (sampleRate > 0.0 ? sampleRate : 44100.0);

}

void KiwiPluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up anything you no longer need. For example, if you were using the
    // spare memory, etc 
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool KiwiPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

/**
 * @brief Method updates output tempo with host tempo
 */
void KiwiPluginAudioProcessor::configureTempo() {

    // Get current tempo and time signature from host, if available
    if (auto* playHead = getPlayHead())
    {
        juce::AudioPlayHead::CurrentPositionInfo posInfo;
        if (playHead->getCurrentPosition (posInfo))
        {
            if (posInfo.bpm > 0.0)
                bpm = posInfo.bpm;
        }
    }
}

bool KiwiPluginAudioProcessor::isGeneratorLoading()
{
    return sequenceGenerator.getLoadingStatus();
}


void KiwiPluginAudioProcessor::sendPromptToGenerator(const juce::String& prompt, const juce::StringArray& recentPrompts,
                                                     std::function<void(juce::String)> callback)
{
    sequenceGenerator.sendToGenerator(prompt, recentPrompts, callback); 
}

void KiwiPluginAudioProcessor::addChatEntry(const ChatEntry& entry)
{
    const juce::ScopedLock lock(chatHistoryLock);
    if (chatHistory.size() >= 10)
        chatHistory.erase(chatHistory.begin());
    chatHistory.push_back(entry);
}

std::vector<ChatEntry> KiwiPluginAudioProcessor::getChatHistory() const
{
    const juce::ScopedLock lock(chatHistoryLock);
    return chatHistory;
}

juce::StringArray KiwiPluginAudioProcessor::getRecentPromptsForContext(int maxPromptCount) const
{
    juce::StringArray recentPrompts;
    if (maxPromptCount <= 0)
        return recentPrompts;

    const juce::ScopedLock lock(chatHistoryLock);
    const int historySize = (int) chatHistory.size();
    const int startIndex = juce::jmax(0, historySize - maxPromptCount);

    for (int i = startIndex; i < historySize; ++i)
    {
        const juce::String prompt = chatHistory[(size_t) i].prompt.trim();
        if (prompt.isNotEmpty())
            recentPrompts.add(prompt);
    }

    return recentPrompts;
}

/**
 * @brief Main audio processing loop where MIDI events are executed 
 * @param buffer The block of audio samples 
 * @param midiMessages Container for MIDI events 
 */
void KiwiPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals; 
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    midiMessages.clear(); 
    
    int blockSize = buffer.getNumSamples(); // Gets the current audio sample block size from the audio buffer
    this->configureTempo(); // Configures tempo each block so that playback dynamically adapts to host tempo changes 
    
    // If button was pressed, schedule the sequence to start 
    if (!sequenceInProgress && shouldGenerateSequence)
    {
        sequenceInProgress = true; 
        shouldGenerateSequence = false; 
        sequenceGenerator.extractSequence(bpm, currentSampleRate);  // parses API response into usable notes 
    }

    // If a sequence is in progress, process the notes and add MIDI events to the buffer as needed 
    if(sequenceInProgress) { 
        sequenceGenerator.processSequence(blockSize, midiMessages);
        if(sequenceGenerator.isSequenceFinished()) {
            DBG("processBlock: Sequence finished.");
            sequenceInProgress = false;
        }
    }

}

/**
 * @brief Resets the note sequence so that it can be played from the beginning again 
 */
void KiwiPluginAudioProcessor::replaySequence() {
        DBG("replaySequence called. noteSequence size: " + juce::String((int)sequenceGenerator.getNoteSequence().size()) + ", sequenceInProgress: " + juce::String(sequenceInProgress ? "true" : "false"));
        if (!sequenceGenerator.getNoteSequence().empty() && !sequenceInProgress) {
            sequenceGenerator.resetSequence();
            shouldGenerateSequence = true;
            DBG("shouldGenerateSequence set to true");
        }
} 

juce::File KiwiPluginAudioProcessor::createMidiFile() {
        return sequenceGenerator.createMidiFile(bpm);
}


//==============================================================================
bool KiwiPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* KiwiPluginAudioProcessor::createEditor()
{
    return new KiwiPluginAudioProcessorEditor (*this);
}

//==============================================================================
void KiwiPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void KiwiPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KiwiPluginAudioProcessor();
}