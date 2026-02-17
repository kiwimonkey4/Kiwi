#include "AnalyticsService.h"

namespace
{
    juce::String env(const char* name, const juce::String& fallback = {})
    {
        return juce::SystemStats::getEnvironmentVariable(name, fallback);
    }

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

    juce::String nowIso8601Utc()
    {
        return juce::Time::getCurrentTime().toString(true, true);
    }

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
    endpoint = env("KIWI_ANALYTICS_ENDPOINT", "http://127.0.0.1:8787/api/trackEvent");
    apiKey = env("KIWI_ANALYTICS_API_KEY");
    sessionId = makeUuid();

    loadOrCreateUserId();

    // Best-effort flush at startup (e.g., offline queue from previous session)
    maybeFlushAsync();
}

AnalyticsService::~AnalyticsService()
{
    flushAsync();
}

bool AnalyticsService::isEnabled() const
{
    // Default: enabled if endpoint is provided. You can force-enable to just write JSONL for debugging.
    if (! envBool("KIWI_ANALYTICS_ENABLED", true))
        return false;

    // If no endpoint, we still allow disk logging (useful during development)
    return true;
}

juce::File AnalyticsService::getBaseDir() const
{
    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile("KiwiPlugin");
    base.createDirectory();
    return base;
}

juce::File AnalyticsService::getUserIdFile() const
{
    return getBaseDir().getChildFile("analytics_user_id.txt");
}

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
        file.replaceWithText(userId + "\n");
    }
}

juce::String AnalyticsService::getUserId() const
{
    const juce::ScopedLock scopedLock(lock);
    return userId;
}

void AnalyticsService::appendEventToDisk(const juce::var& eventObject)
{
    auto eventsFile = getEventsFile();

    const auto line = juce::JSON::toString(eventObject, true) + "\n";

    juce::FileOutputStream out(eventsFile);
    if (! out.openedOk())
        return;

    out.setPosition(out.getFile().getSize());
    out.writeText(line, false, false, "\n");
    out.flush();
}

void AnalyticsService::trackEvent(const juce::String& eventName, const juce::var& properties)
{
    if (! isEnabled())
        return;

    if (eventName.isEmpty())
        return;

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

    if (! properties.isVoid() && ! properties.isUndefined())
        event->setProperty("props", properties);

    appendEventToDisk(juce::var(event.get()));

    eventsSinceLastFlush.fetch_add(1);
    maybeFlushAsync();
}

void AnalyticsService::maybeFlushAsync()
{
    // If no endpoint, we only persist to disk.
    if (endpoint.isEmpty())
        return;

    // Flush every ~5 events to avoid spamming the network.
    if (eventsSinceLastFlush.load() < 5)
        return;

    flushAsync();
}

void AnalyticsService::flushAsync()
{
    if (endpoint.isEmpty())
        return;

    bool expected = false;
    if (! flushInProgress.compare_exchange_strong(expected, true))
        return;

    eventsSinceLastFlush.store(0);

    juce::Thread::launch([this]
    {
        flushOnBackgroundThread();
        flushInProgress.store(false);
    });
}

void AnalyticsService::flushOnBackgroundThread()
{
    auto eventsFile = getEventsFile();
    if (! eventsFile.existsAsFile())
        return;

    const auto raw = eventsFile.loadFileAsString();
    if (raw.trim().isEmpty())
        return;

    juce::Array<juce::var> events;
    {
        juce::StringArray lines;
        lines.addLines(raw);

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

    if (events.isEmpty())
        return;

    juce::DynamicObject::Ptr payload(new juce::DynamicObject());
    payload->setProperty("source", "juce_plugin");
    payload->setProperty("app", JucePlugin_Name);
    payload->setProperty("app_version", JucePlugin_VersionString);
    payload->setProperty("events", juce::var(events));

    juce::String json = juce::JSON::toString(juce::var(payload.get()));

    juce::URL url(endpoint);
    url = url.withPOSTData(json);

    int status = 0;
    juce::String headers = "Content-Type: application/json\r\n";
    if (! apiKey.isEmpty())
        headers += "X-API-Key: " + apiKey + "\r\n";

    auto options = makeDefaultOptions(&status).withExtraHeaders(headers);

    std::unique_ptr<juce::InputStream> stream(url.createInputStream(options));
    if (stream == nullptr)
        return;

    (void) stream->readEntireStreamAsString();

    if (status >= 200 && status < 300)
    {
        eventsFile.deleteFile();
    }
}
