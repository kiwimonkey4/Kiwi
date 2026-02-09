/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    activeNoteCount = 0;

    noteOnCountdownSamples = -1;
    noteOffCountdownSamples = -1;

    double durationInSeconds = (3.0 * defaultBeatsPerBar * 60.0) / defaultBpm; // for the current notes 
    noteDurationSamples = juce::jmax (1, (int) std::round (durationInSeconds * currentSampleRate));
}

void KiwiPluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
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
    double bpm = defaultBpm;
    double beatsPerBar = defaultBeatsPerBar;        

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

void KiwiPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    midiMessages.clear();
    
    int blockSize = buffer.getNumSamples();

    // Recalculate duration for 3 bars in a sample
    this->configureTempo();  
    double durationInSeconds = (3.0 * beatsPerBar * 60.0) / bpm;
    noteDurationSamples = juce::jmax (1, (int) std::round (durationInSeconds * currentSampleRate));
    
    // If button was pressed, schedule the sequence to start  
    if (shouldGenerateSequence && !sequenceInProgress)
    {
        noteOnCountdownSamples = triggerDelaySamples; // how many samples in the future does the sequence start? 
        noteOffCountdownSamples = triggerDelaySamples + noteDurationSamples; // how many samples in the future does the sequence end?
        sequenceInProgress = true;
        shouldGenerateSequence = false;  
        DBG("Note scheduled: duration = " + juce::String(noteDurationSamples) + " samples (3 bars at " + juce::String(bpm) + " BPM)");
    }
    
    // Handle note ON (does the note start in this block)
    if (noteOnCountdownSamples >= 0 && noteOnCountdownSamples < blockSize)
    {
        midiMessages.addEvent(
            juce::MidiMessage::noteOn(scheduledMidiChannel, scheduledMidiNote, scheduledVelocity),
            noteOnCountdownSamples
        );
        DBG("Note ON at sample " + juce::String(noteOnCountdownSamples));
        noteOnCountdownSamples = -1; // Note has started 
    }
    else if (noteOnCountdownSamples >= blockSize) // Note starts in a future block 
    {
        noteOnCountdownSamples -= blockSize; // Block is consumed 
    }
    
    // Handle note off (does the note turn off in this block) 
    if (noteOffCountdownSamples >= 0 && noteOffCountdownSamples < blockSize)
    {
        midiMessages.addEvent(
            juce::MidiMessage::noteOff(scheduledMidiChannel, scheduledMidiNote),
            noteOffCountdownSamples
        );
        DBG("Note OFF at sample " + juce::String(noteOffCountdownSamples));
        noteOffCountdownSamples = -1; // Note has ended
    }
    else if (noteOffCountdownSamples >= blockSize)
    {
        noteOffCountdownSamples -= blockSize;
    }

    // Sequence has finished being processed
    if(noteOnCountdownSamples < 0 && noteOffCountdownSamples < 0 && sequenceInProgress) {
        DBG("Sequence finished.");
        sequenceInProgress = false; // Reset for next trigger
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