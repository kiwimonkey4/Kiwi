/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ChatEntry.h"
#include "BinaryData.h"
#include <string>
#include <functional>

//==============================================================================
KiwiPluginAudioProcessorEditor::KiwiPluginAudioProcessorEditor (KiwiPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Set custom LookAndFeel for global font
    setLookAndFeel(&customLookAndFeel);
    
    // Load existing chat history from processor
    chatHistory.loadFromHistory(audioProcessor.getChatHistory());
    chatHistory.setOnMidiDragged([this](const ChatEntry& entry)
    {
        juce::DynamicObject::Ptr props(new juce::DynamicObject());
        props->setProperty("has_midi_file", entry.midiFile.existsAsFile());
        props->setProperty("midi_file_bytes", (double) entry.midiFile.getSize());
        props->setProperty("prompt_length", (int) entry.prompt.length());
        audioProcessor.trackEvent("midi_dragged", juce::var(props.get()));
    });

    // Check if generator is loading and restore loading state
    if (audioProcessor.isGeneratorLoading())
    {
        isLoading = true;
        chatHistory.setVisible(false);
        startTimer(50); // Start rotation timer
        DBG("Editor opened while loading in progress - restoring loading screen");
    }
    
    // Load kiwi image from embedded binary data
    kiwiImage = juce::ImageCache::getFromMemory(BinaryData::kiwi_png, BinaryData::kiwi_pngSize);
    DBG("Image loaded from BinaryData: " + juce::String(kiwiImage.isValid() ? "YES" : "NO"));
    if (kiwiImage.isValid())
    {
        DBG("Image size: " + juce::String(kiwiImage.getWidth()) + "x" + juce::String(kiwiImage.getHeight()));
    }

      // Setup replay button 
    replayButton.setButtonText("Replay");
    replayButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    replayButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    replayButton.onClick = [this] {
        DBG("Replay button clicked. sequenceInProgress: " + juce::String(audioProcessor.getSequenceStatus() ? "true" : "false"));
        if(!audioProcessor.getSequenceStatus()) {
            audioProcessor.replaySequence();
            DBG("replaySequence() called");
        }
        else {
            DBG("replaySequence() SKIPPED - sequence in progress");
        }
    };
    addAndMakeVisible(replayButton);

    // Setup text entry field 
    textEntry.setMultiLine(true);
    textEntry.setReturnKeyStartsNewLine(false);
    juce::Font textEntryFont (customLookAndFeel.getTypefaceForFont (juce::Font()));
    textEntryFont.setHeight(14.0f);
    textEntry.setFont(textEntryFont);
    textEntry.setColour(juce::TextEditor::textColourId, juce::Colours::black);
    textEntry.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF9CCC65));
    textEntry.onReturnKey = [this] { 
        juce::String userInput = textEntry.getText();
        DBG("ENTER PRESSED: " + userInput);
        
        if (userInput.isNotEmpty())
        {
            const auto requestStartMs = juce::Time::getMillisecondCounterHiRes();

            {
                // Privacy-aware: store prompt length + a non-reversible hash for grouping.
                std::hash<std::string> hasher;
                const auto promptUtf8 = userInput.toStdString();
                const auto promptHash64 = (juce::int64) hasher(promptUtf8);

                juce::DynamicObject::Ptr props(new juce::DynamicObject());
                props->setProperty("prompt_length", (int) userInput.length());
                props->setProperty("prompt_hash64", juce::String(promptHash64));
                audioProcessor.trackEvent("prompt_submitted", juce::var(props.get()));
            }

            juce::String savedPrompt = userInput; // Save prompt before clearing
            textEntry.clear(); // Clear immediately 
            
            // Show loading indicator
            isLoading = true;
            rotationAngle = 0.0f;
            
            // Stop any existing timer first to prevent conflicts
            stopTimer();
            
            chatHistory.setVisible(false); // Hide chat so kiwi is visible
            startTimer(50); // Update every 50ms for smooth rotation
            repaint();
            DBG("Loading started - isLoading: " + juce::String(isLoading ? "true" : "false"));
            DBG("Image valid: " + juce::String(kiwiImage.isValid() ? "true" : "false"));

            // Create a safe pointer to this editor - becomes null if editor is destroyed
            juce::Component::SafePointer<KiwiPluginAudioProcessorEditor> safeThis(this);
            
            // Send to API - this runs async on background thread
            audioProcessor.sendPromptToGenerator(userInput, [safeThis, savedPrompt, requestStartMs, &processor = audioProcessor](juce::String response) {
                // This callback runs on main thread after API response
                // Verify we're on the message thread
                jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
                DBG("OpenAI Response received");

                const auto latencyMs = (int) std::round(juce::Time::getMillisecondCounterHiRes() - requestStartMs);
                const bool isError = response.startsWithIgnoreCase("Error:") || response.startsWithIgnoreCase("API error");
                
                // Check if editor still exists before accessing its members
                if (safeThis == nullptr)
                {
                    DBG("Editor was destroyed - skipping UI updates but continuing audio processing");

                    {
                        juce::DynamicObject::Ptr props(new juce::DynamicObject());
                        props->setProperty("latency_ms", latencyMs);
                        props->setProperty("result", isError ? "error" : "ok");
                        processor.trackEvent(isError ? "generation_failed" : "generation_completed", juce::var(props.get()));
                    }
                    
                    // Editor is gone but we can still trigger audio (processor outlives editor)
                    if(!processor.getSequenceStatus()) { 
                        processor.triggerNote();
                    }
                    
                    // Create MIDI file and add to processor history
                    juce::File midiFile = processor.createMidiFile();
                    ChatEntry entry(savedPrompt, "Sequence generated", midiFile);
                    processor.addChatEntry(entry);
                    return;
                }
                
                // Editor still exists - safe to access UI components
                // IMPORTANT: Stop timer FIRST before any UI modifications
                safeThis->stopTimer();
                safeThis->isLoading = false;
                
                // Now safe to modify UI
                safeThis->chatHistory.setVisible(true);
                safeThis->repaint();

                // Trigger sequence generation after response is received
                DBG("About to call triggerNote. sequenceInProgress: " + juce::String(processor.getSequenceStatus() ? "true" : "false"));
                if(!processor.getSequenceStatus()) { 
                    processor.triggerNote();
                    DBG("triggerNote() called");
                }
                else {
                    DBG("triggerNote() SKIPPED - sequence already in progress");
                }

                {
                    juce::DynamicObject::Ptr props(new juce::DynamicObject());
                    props->setProperty("latency_ms", latencyMs);
                    props->setProperty("result", isError ? "error" : "ok");
                    if (!isError)
                        props->setProperty("note_count", processor.getLastGeneratedNoteCount());
                    processor.trackEvent(isError ? "generation_failed" : "generation_completed", juce::var(props.get()));
                }
                
                // Create MIDI file automatically 
                juce::File midiFile = processor.createMidiFile();
                
                // Add to chat history (both UI and processor storage)
                ChatEntry entry(savedPrompt, "Sequence generated", midiFile);
                safeThis->chatHistory.addChatEntry(entry);
                processor.addChatEntry(entry);
            });
        }
    };
    addAndMakeVisible(textEntry);
    
    // Setup chat history
    addAndMakeVisible(chatHistory);

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be 
    setSize (600, 500);
}

KiwiPluginAudioProcessorEditor::~KiwiPluginAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr); // Reset to default
}

//==============================================================================
void KiwiPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (juce::Colour(0xFF795C34)); // #795C34 - same as replay button

    g.setColour (juce::Colours::white);
    juce::Font font (getLookAndFeel().getTypefaceForFont (juce::Font()));
    font.setHeight(15.0f);
    g.setFont (font);
    
    // Draw rotating kiwi when loading
    if (isLoading && kiwiImage.isValid())
    {
        int imageSize = 100;
        int centerX = getWidth() / 2;
        int centerY = getHeight() / 2 - 50;
        
        // Save graphics state
        juce::Graphics::ScopedSaveState saveState(g);
        
        // Apply rotation around the center of the image
        g.addTransform(juce::AffineTransform::rotation(rotationAngle, 
                                                        (float)centerX, 
                                                        (float)centerY));
        
        // Draw image centered
        g.drawImage(kiwiImage, 
                   centerX - imageSize / 2, 
                   centerY - imageSize / 2, 
                   imageSize, 
                   imageSize, 
                   0, 0, 
                   kiwiImage.getWidth(), 
                   kiwiImage.getHeight());
    }
    else if (isLoading && !kiwiImage.isValid())
    {
        // Fallback to text if image didn't load
        g.setColour(juce::Colours::orange);
        juce::Font loadingFont (getLookAndFeel().getTypefaceForFont (juce::Font()));
        loadingFont.setHeight(20.0f);
        g.setFont(loadingFont);
        g.drawText("Loading...", getLocalBounds(), juce::Justification::centred);
    }
}

void KiwiPluginAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..

    // Position the text entry field at the bottom 
    textEntry.setBounds(10, getHeight() - 110, getWidth() - 110, 100);
    
    // Position the replay button aligned with the text entry field
    replayButton.setBounds(getWidth() - 90, getHeight() - 110, 80, 100); 
    
    // Chat history takes up the rest of the space above
    chatHistory.setBounds(10, 10, getWidth() - 20, getHeight() - 130);
}

void KiwiPluginAudioProcessorEditor::timerCallback()
{
    // Rotate kiwi image
    rotationAngle += 0.1f;
    if (rotationAngle > juce::MathConstants<float>::twoPi)
        rotationAngle -= juce::MathConstants<float>::twoPi;
    repaint();
}
