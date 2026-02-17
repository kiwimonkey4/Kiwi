/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
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

//==============================================================================
void KiwiPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback 
    // initialisation that you need..

    juce::ignoreUnused (samplesPerBlock);
    currentSampleRate = (sampleRate > 0.0 ? sampleRate : 44100.0);

    noteOnCountdownSamples = -1;
    noteOffCountdownSamples = -1; 

}

void KiwiPluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up anything you no longer need. For example, if you were using the
    // spare memory, etc 
    noteOnCountdownSamples = -1;
    noteOffCountdownSamples = -1;
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

void KiwiPluginAudioProcessor::configureTempo() {

    // Get current tempo and time signature from host, if available 
    if (auto* playHead = getPlayHead())
    {
        juce::AudioPlayHead::CurrentPositionInfo posInfo;
        if (playHead->getCurrentPosition (posInfo))
        {
            if (posInfo.bpm > 0.0)
                bpm = posInfo.bpm;

            if (posInfo.timeSigNumerator > 0)
                beatsPerBar = (double) posInfo.timeSigNumerator;
        }
    }
}

bool KiwiPluginAudioProcessor::isGeneratorLoading()
{
    return sequenceGenerator.getLoadingStatus();
}

void KiwiPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    midiMessages.clear();
    
    int blockSize = buffer.getNumSamples();
    this->configureTempo();  
    
    // If button was pressed, schedule the sequence to start 
    if (!sequenceInProgress && shouldGenerateSequence)
    {
       // DBG("processBlock: Starting sequence! bpm=" + juce::String(bpm) + " sampleRate=" + juce::String(currentSampleRate));
        sequenceInProgress = true;
        shouldGenerateSequence = false; 
        sequenceGenerator.extractSequence(bpm, currentSampleRate);
       // DBG("processBlock: Extracted " + juce::String((int)sequenceGenerator.getNoteSequence().size()) + " notes");
    }

    if(sequenceInProgress) { 
        sequenceGenerator.processSequence(blockSize, midiMessages);
        if(sequenceGenerator.isSequenceFinished()) {
            DBG("processBlock: Sequence finished.");
            sequenceInProgress = false;
        }
    }

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