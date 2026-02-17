/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ChatHistoryComponent.h"
#include "CustomLookAndFeel.h"

//==============================================================================
/**
*/
class KiwiPluginAudioProcessorEditor  : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    KiwiPluginAudioProcessorEditor (KiwiPluginAudioProcessor&);
    ~KiwiPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    KiwiPluginAudioProcessor& audioProcessor;
    juce::TextEditor textEntry;
    juce::TextButton replayButton;
    ChatHistoryComponent chatHistory;
    juce::Image kiwiImage;
    float rotationAngle = 0.0f;
    bool isLoading = false;
    CustomLookAndFeel customLookAndFeel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KiwiPluginAudioProcessorEditor)
};
