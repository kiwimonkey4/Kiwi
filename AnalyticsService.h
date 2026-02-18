#pragma once

#include <JuceHeader.h>

class AnalyticsService
{
public:
    AnalyticsService();
    ~AnalyticsService();

    void trackEvent(const juce::String& eventName, const juce::var& properties = juce::var());
    void flushAsync();

    juce::String getUserId() const;
    juce::String getSessionId() const { return sessionId; }

private:
    juce::File getBaseDir() const;
    juce::File getUserIdFile() const;
    juce::File getEventsFile() const;

    void loadOrCreateUserId();
    void appendEventToDisk(const juce::var& eventObject);
    bool isEnabled() const;

    void maybeFlushAsync();
    void flushOnBackgroundThread();

    juce::String endpoint;
    juce::String apiKey;

    juce::String userId;
    juce::String sessionId;

    mutable juce::CriticalSection lock;
    std::atomic<bool> flushInProgress { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalyticsService)
};
