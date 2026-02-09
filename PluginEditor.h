/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class KiwiPluginAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    KiwiPluginAudioProcessorEditor (KiwiPluginAudioProcessor&);
    ~KiwiPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    KiwiPluginAudioProcessor& audioProcessor;
    juce::TextButton generateButton;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KiwiPluginAudioProcessorEditor)
};
