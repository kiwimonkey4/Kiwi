/*
  ==============================================================================

    CustomLookAndFeel.h
    Custom LookAndFeel for global font styling

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "BinaryData.h"

//==============================================================================
// Custom LookAndFeel for custom .ttf font
class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel()
    {
        // Load Breathe Fire font from embedded binary data
        customTypeface = juce::Typeface::createSystemTypefaceFor(
            BinaryData::Sakire_ttf, BinaryData::Sakire_ttfSize);
        
        if (customTypeface != nullptr)
        {
            setDefaultSansSerifTypeface(customTypeface);
        }
    }
    
    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& font) override
    {
        if (customTypeface != nullptr)
            return customTypeface;
        
        return juce::LookAndFeel_V4::getTypefaceForFont(font);
    }
    
    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override
    {
        if (customTypeface != nullptr)
        {
            juce::Font buttonFont(customTypeface);
            buttonFont.setHeight(juce::jmin(15.0f, buttonHeight * 0.6f));
            return buttonFont;
        }
        return juce::LookAndFeel_V4::getTextButtonFont(button, buttonHeight);
    }
    
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f, 0.5f);
        auto baseColour = juce::Colour(0xFF795C34); // #795C34
        
        if (shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted)
            baseColour = baseColour.contrasting(shouldDrawButtonAsDown ? 0.2f : 0.05f);
        
        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, 10.0f);
        
        // Draw black border
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(bounds, 10.0f, 2.0f);
    }
    
    void fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor& textEditor) override
    {
        g.setColour(juce::Colour(0xFF9CCC65)); // #9CCC65
        g.fillRoundedRectangle(textEditor.getLocalBounds().toFloat(), 10.0f);
    }
    
    void drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& textEditor) override
    {
        // Optional: add a subtle outline
        g.setColour(textEditor.findColour(juce::TextEditor::outlineColourId));
        g.drawRoundedRectangle(textEditor.getLocalBounds().toFloat().reduced(0.5f), 10.0f, 1.0f);
    }

private:
    juce::Typeface::Ptr customTypeface;
};
