/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
KiwiPluginAudioProcessorEditor::KiwiPluginAudioProcessorEditor (KiwiPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Setup button
    generateButton.setButtonText("Generate Notesss");
    generateButton.onClick = [this] { 
    DBG("BUTTON CLICKED!");
    if(!audioProcessor.getSequenceStatus()) { audioProcessor.triggerNote(); }
};
    addAndMakeVisible(generateButton);

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be 
    setSize (400, 300);
}

KiwiPluginAudioProcessorEditor::~KiwiPluginAudioProcessorEditor()
{
}

//==============================================================================
void KiwiPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (15.0f));
    g.drawFittedText ("WE UP!", getLocalBounds(), juce::Justification::centred, 1);
}

void KiwiPluginAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..

    // Position the button in the center
    generateButton.setBounds(150, 130, 100, 40);
}
