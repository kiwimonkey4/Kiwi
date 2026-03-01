#include "AnalyticsService.h"
#include <ctime>

// Anonymous namespace: helpers used only within this .cpp file 
namespace
{
    /// Read an environment variable, with optional fallback
    juce::String env(const char* name, const juce::String& fallback = {})
    {
        return juce::SystemStats::getEnvironmentVariable(name, fallback);
    }

    /// Parse env as boolean (1/0, true/false, yes/no)
    bool envBool(const char* name, bool fallback)
    {
        const auto value = env(name, fallback ? "1" : "0").trim();
        if (value.equalsIgnoreCase("1") || value.equalsIgnoreCase("true") || value.equalsIgnoreCase("yes"))
            return true;
        if (value.equalsIgnoreCase("0") || value.equalsIgnoreCase("false") || value.equalsIgnoreCase("no"))
            return false;
        return fallback;
    }

    juce::String makeUuid()
    {
        return juce::Uuid().toString();
    }

    /// Current time as ISO 8601 UTC string, e.g. "2025-02-25T14:30:00.123Z"
    juce::String nowIso8601Utc()
    {
        const auto nowMillis = juce::Time::currentTimeMillis();
        const auto epochSeconds = static_cast<std::time_t>(nowMillis / 1000);
        const auto millisPart = static_cast<int>(nowMillis % 1000);

        std::tm utcTime {};
       #if JUCE_WINDOWS
        gmtime_s(&utcTime, &epochSeconds);
       #else
        gmtime_r(&epochSeconds, &utcTime);
       #endif

        return juce::String::formatted("%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                                       utcTime.tm_year + 1900,
                                       utcTime.tm_mon + 1,
                                       utcTime.tm_mday,
                                       utcTime.tm_hour,
                                       utcTime.tm_min,
                                       utcTime.tm_sec,
                                       millisPart);
    }

    /// HTTP options for POST request: 10s timeout, 2 redirects, capture status code 
    juce::URL::InputStreamOptions makeDefaultOptions(int* statusCode)
    {
        return juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(10'000)
            .withNumRedirectsToFollow(2)
            .withStatusCode(statusCode);
    }
}

AnalyticsService::AnalyticsService()
{
    // Set endpoint from environment 
    endpoint = env("KIWI_ANALYTICS_ENDPOINT");
    sessionId = makeUuid();  // New session ID each time the plugin loads

    loadOrCreateUserId();  // Load or create persistent user ID from disk
}

AnalyticsService::~AnalyticsService()
{
    // Wait for any ongoing background flush to complete
    while (flushInProgress.load())
        juce::Thread::sleep(10);
    
    // Now flush synchronously on this thread (safe since no other flush is running)
    if (!endpoint.isEmpty())
        flushOnBackgroundThread();
}

bool AnalyticsService::isEnabled() const
{
    // KIWI_ANALYTICS_ENABLED defaults to true; set to 0/false to disable
    if (! envBool("KIWI_ANALYTICS_ENABLED", true))
        return false;

    // Even with no endpoint, we still write to disk (useful for debugging)
    return true;
}

/**
 * @brief Get the base directory for analytics data, ensuring it exists
 * @return juce::File representing the base directory
 */
juce::File AnalyticsService::getBaseDir() const
{

    // Use operating system's standard app data directory and create a subdirectory for our plugin
    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile("KiwiPlugin");
    base.createDirectory();
    return base;
}

juce::File AnalyticsService::getUserIdFile() const
{
    return getBaseDir().getChildFile("analytics_user_id.txt");
}

/**
 * @brief Get the file where analytics events are queued locally
 * @return juce::File representing the events (.jsonl) file 
 */
juce::File AnalyticsService::getEventsFile() const
{
    return getBaseDir().getChildFile("analytics_events.jsonl");
}

void AnalyticsService::loadOrCreateUserId()
{
    const juce::ScopedLock scopedLock(lock);

    auto file = getUserIdFile();
    if (file.existsAsFile())
        userId = file.loadFileAsString().trim();

    if (userId.isEmpty())
    {
        userId = makeUuid();
        file.replaceWithText(userId + "\n");  // Persist so we reuse same ID next session
    }
}

juce::String AnalyticsService::getUserId() const
{
    const juce::ScopedLock scopedLock(lock);
    return userId;
}

/**
 * @brief Append a single event to the local events file in JSONL format
 * @param eventObject A juce::var containing the event data to be appended as a JSON object 
 */
void AnalyticsService::appendEventToDisk(const juce::var& eventObject)
{
    auto eventsFile = getEventsFile();

    // JSONL format: one JSON object per line (easy to append, easy to parse)
    const auto line = juce::JSON::toString(eventObject, true) + "\n";

    juce::FileOutputStream out(eventsFile);
    if (! out.openedOk())
        return;
    out.setPosition(out.getFile().getSize());
    out.writeText(line, false, false, "\n");
    out.flush();
}

/**
 * @brief Records a plugin usage event
 * @param eventName Identifier of the event being tracked 
 * @param properties Additional data for the event in JSON form 
 */
void AnalyticsService::trackEvent(const juce::String& eventName, const juce::var& properties)
{
    if (! isEnabled())
        return;

    if (eventName.isEmpty())
        return;

    // Build event object with required fields 
    juce::DynamicObject::Ptr event(new juce::DynamicObject());
    event->setProperty("event", eventName);
    event->setProperty("ts_iso", nowIso8601Utc());

    {
        const juce::ScopedLock scopedLock(lock);
        event->setProperty("user_id", userId);
        event->setProperty("session_id", sessionId);
    }

    event->setProperty("app", JucePlugin_Name);
    event->setProperty("app_version", JucePlugin_VersionString);

    // Add event data if it is provided 
    if (! properties.isVoid() && ! properties.isUndefined())
        event->setProperty("props", properties);

    appendEventToDisk(juce::var(event.get()));
    eventCount++; 
    spacedFlush(); 
}

/**
 * @brief Trigger a flush of events after every 10 events tracked to prevent spamming the API endpoint 
 */
void AnalyticsService::spacedFlush() {
    if (eventCount >= 10)
    {
        flushAsync();
        eventCount = 0; 
    }
}


/**
 * @brief Asynchronously send tracked events to the analytics API on a background thread 
 */
void AnalyticsService::flushAsync()
{
    if (endpoint.isEmpty())
        return;

    // Only one flush at a time: if one is already running, skip
    bool expected = false;
    if (! flushInProgress.compare_exchange_strong(expected, true))
        return;

    // New background thread for the flush operation 
    juce::Thread::launch([this]
    {
        flushOnBackgroundThread();
        flushInProgress.store(false);  // Allow next flush
    });
}

/**
 * @brief implementation details of how events are flushed to the analytics API 
 */
void AnalyticsService::flushOnBackgroundThread()
{
    auto eventsFile = getEventsFile();

    // Abort if there are no events in the file 
    if (! eventsFile.existsAsFile())
        return;

    const auto raw = eventsFile.loadFileAsString();

    // Abort if the file is empty 
    if (raw.trim().isEmpty())
        return;

    juce::Array<juce::var> events;
    {
        // Add JSON objects from each line to the array
        juce::StringArray lines;
        lines.addLines(raw);

        // Loop through each line and parse as JSON, adding valid objects to the events array
        for (const auto& line : lines)
        {
            const auto trimmed = line.trim();
            if (trimmed.isEmpty())
                continue;

            auto parsed = juce::JSON::parse(trimmed);
            if (! parsed.isVoid() && ! parsed.isUndefined())
                events.add(parsed);
        }
    }

    // Abort if there are no valid events to send 
    if (events.isEmpty())
        return;

    // Build batch payload expected by analytics API 
    juce::DynamicObject::Ptr payload(new juce::DynamicObject());
    payload->setProperty("source", "juce_plugin");
    payload->setProperty("app", JucePlugin_Name);
    payload->setProperty("app_version", JucePlugin_VersionString);
    payload->setProperty("events", juce::var(events));

    // Create JSON-formatted string and send to API via HTTP POST request 
    juce::String json = juce::JSON::toString(juce::var(payload.get()));
    juce::URL url(endpoint);
    url = url.withPOSTData(json);

    int status = 0;
    juce::String headers = "Content-Type: application/json\r\n";

    auto options = makeDefaultOptions(&status).withExtraHeaders(headers);

    std::unique_ptr<juce::InputStream> stream(url.createInputStream(options));
    if (stream == nullptr)
        return;  // Network error; events stay in file, will retry next flush

    (void) stream->readEntireStreamAsString();  // Consume response (we only care about status)

    if (status >= 200 && status < 300)
    {
        eventsFile.deleteFile();  // Successfully sent events, clear the file 
    }
}