#pragma once

#include <JuceHeader.h>

/**
 * AnalyticsService - Event tracking for the Kiwi plugin.
 *
 * Collects usage events (prompt_submitted, generation_completed, midi_dragged, etc.),
 * persists them to a local JSONL file, and periodically sends batches to the analytics API.
 *
 * Flow: trackEvent() -> append to disk -> maybeFlushAsync() -> POST to API -> delete file on success
 */
class AnalyticsService
{
public:
    AnalyticsService();
    ~AnalyticsService();

    /// Record an event with optional properties. Immediately appends to disk, then may trigger a flush.
    void trackEvent(const juce::String& eventName, const juce::var& properties = juce::var());

    /// Force-send any queued events to the API. Called on shutdown to avoid losing data.
    void flushAsync();

    juce::String getUserId() const;
    juce::String getSessionId() const { return sessionId; }

private:
    /// Base directory for analytics data: %AppData%/KiwiPlugin (or equivalent on macOS/Linux)
    juce::File getBaseDir() const;
    juce::File getUserIdFile() const;   /// analytics_user_id.txt - persistent UUID per install
    juce::File getEventsFile() const;   /// analytics_events.jsonl - queue of events to send

    void loadOrCreateUserId();          /// Load existing user ID from disk, or create + persist new UUID
    void appendEventToDisk(const juce::var& eventObject);  /// Append one JSON line to events file
    bool isEnabled() const;              /// Check KIWI_ANALYTICS_ENABLED env (default: true)

    void flushOnBackgroundThread();      /// Read file, POST to API, delete file on success
    void spacedFlush(); 

    juce::String endpoint;              /// API URL
    juce::String apiKey;                /// Optional X-API-Key header

    juce::String userId;                /// Persistent UUID, stored in analytics_user_id.txt
    juce::String sessionId;             /// New UUID per plugin session

    mutable juce::CriticalSection lock; /// Protects userId and shared state
    std::atomic<bool> flushInProgress { false };  /// Prevents overlapping flush threads 

    int eventCount = 0; 

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalyticsService)
};
