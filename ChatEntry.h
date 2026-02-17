#pragma once
#include <JuceHeader.h>

struct ChatEntry
{
    juce::String prompt;
    juce::String response;
    juce::File midiFile;
    juce::Time timestamp;
    
    ChatEntry(const juce::String& p, const juce::String& r, const juce::File& f)
        : prompt(p), response(r), midiFile(f), timestamp(juce::Time::getCurrentTime())
    {}
};
