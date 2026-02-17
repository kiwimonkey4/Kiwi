#include "Generator.h"
#include "MidiNoteEvent.h"
#include "MidiNote.h"

#define MAX_TIMEOUT_MS 300000 // 5 minutes
#define MAX_REDIRECTS 5
#define TICKS_PER_QUARTER_NOTE 480

Generator::Generator()
    : sharedState(std::make_shared<SharedState>())
{
    apiKey = loadApiKey();
}

Generator::~Generator()
{
    // Mark as invalid so background threads won't access this object
    sharedState->isValid = false;
    
    // Clean up all created MIDI files
    for (const auto& file : createdMidiFiles)
    {
        if (file.existsAsFile())
        {
            file.deleteFile();
            DBG("Deleted MIDI file: " + file.getFullPathName());
        }
    }
    createdMidiFiles.clear();
}

void Generator::getSequenceJSON(const juce::String& apiResponse)
{
    juce::String content;
    auto parsed = juce::JSON::parse(apiResponse);
    if (parsed.isObject())
    {
        auto* obj = parsed.getDynamicObject();
        auto output = obj->getProperty("output");

        if (output.isArray())
        {
            for (auto& item : *output.getArray())
            {
                if (!item.isObject()) continue;

                auto* itemObj = item.getDynamicObject();
                if (itemObj->getProperty("type").toString() != "message")
                    continue;

                auto contentArr = itemObj->getProperty("content");
                if (!contentArr.isArray() || contentArr.size() == 0)
                    continue;

                auto first = contentArr[0];
                if (first.isObject())
                {
                    auto* textObj = first.getDynamicObject();
                    content = textObj->getProperty("text").toString();
                    break;
                }
            }
        }
    }
    
    if (content.isEmpty())
    {
        DBG("Failed to extract content from API response");
        return;
    }
    
    DBG("Extracted MIDI JSON: " + content);
    sequenceJSON = content; 
}

juce::String Generator::loadApiKey() const
{
    DBG("=== Attempting to load API key ===");
    
    auto key = juce::SystemStats::getEnvironmentVariable("KIWI_OPENAI_API_KEY", "").trim();
    DBG("Environment variable KIWI_OPENAI_API_KEY: " + (key.isEmpty() ? "NOT FOUND" : "Found (" + juce::String(key.length()) + " chars)"));
    
    if (key.isNotEmpty())
    {
        DBG("✓ Loaded API key from environment variable");
        return key;
    }

    auto keyFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("KiwiPlugin")
                       .getChildFile("openai_api_key.txt");

    DBG("Checking file: " + keyFile.getFullPathName());
    DBG("File exists: " + juce::String(keyFile.existsAsFile() ? "YES" : "NO"));
    
    if (keyFile.existsAsFile())
    {
        auto fileKey = keyFile.loadFileAsString().trim();
        DBG("✓ Loaded API key from file: " + keyFile.getFullPathName() + " (" + juce::String(fileKey.length()) + " chars)");
        return fileKey;
    }

    DBG("✗ No API key found - checked env var and file");
    return {};
}

int Generator::getNoteCountFromSequenceJSON() const
{
    auto notesJson = juce::JSON::parse(sequenceJSON);
    if (! notesJson.isObject())
        return 0;

    if (auto* notesObj = notesJson.getDynamicObject())
    {
        auto notesArray = notesObj->getProperty("notes");
        if (notesArray.isArray())
            return notesArray.size();
    }

    return 0;
}

void Generator::extractSequence(double bpm, double sampleRate)
{
    // Get the sequence JSON from the API response wrapper
    juce::String content = sequenceJSON;
    
    if (content.startsWith("Error:"))
        return;
    
    // Parse the JSON for the notes 
    int runningSampleCount = triggerDelaySamples;  
    noteSequence.clear(); 
    auto notesJson = juce::JSON::parse(content);
    if (notesJson.isObject())
    {
        auto* notesObj = notesJson.getDynamicObject();
        auto notesArray = notesObj->getProperty("notes");
        
        if (notesArray.isArray())
        {
            DBG("Found " + juce::String(notesArray.size()) + " notes");
            
            for (int i = 0; i < notesArray.size(); i++)
            {
                auto note = notesArray[i];
                if (note.isObject())
                {
                    auto* noteObj = note.getDynamicObject();
                    float startBeats = noteObj->getProperty("start_beats");
                    float durationBeats = noteObj->getProperty("duration_beats");
                    int midiNote = noteObj->getProperty("midi_note");
                    int velocity = noteObj->getProperty("velocity"); 

                    double startInSeconds = (startBeats * 60.0) / bpm;
                    double durationInSeconds = (durationBeats * 60.0) / bpm;
                    int startSamples = triggerDelaySamples + juce::jmax(1, (int) std::round(startInSeconds * sampleRate)); 
                    int noteDurationSamples = juce::jmax(1, (int) std::round(durationInSeconds * sampleRate));
                    
                    MidiNoteEvent noteEvent{scheduledMidiChannel, midiNote, (juce::uint8) velocity};
                    noteSequence.emplace_back(noteEvent, startSamples, startSamples + noteDurationSamples);

                    DBG("Note " + juce::String(i) + ": MIDI=" + juce::String(midiNote) + 
                        " start=" + juce::String(startBeats) + 
                        " dur=" + juce::String(durationBeats) + 
                        " vel=" + juce::String(velocity));
                }
            }
        }
    }
    
}

void Generator::processSequence(int blockSize, juce::MidiBuffer& midiMessages)
{
    for(auto it = noteSequence.begin(); it != noteSequence.end(); ++it) {
        it->processNote(blockSize, midiMessages);
        if(it->isFinished() && !it->hasBeenCounted()) {
            it->markAsCounted();
            sequenceTracker++;
        }
    }
}

bool Generator::isSequenceFinished() {
    if(sequenceTracker >= noteSequence.size()) {
        sequenceTracker = 0; // Reset for the next sequence 
        return true; 
    }
    return false;
}

void Generator::resetSequence() {
    sequenceTracker = 0;
    for (auto& note : noteSequence) {
        note.reset();
    }
}

juce::File Generator::createMidiFile(double bpm) {
    juce::MidiFile midiFile;
    juce::MidiMessageSequence track;
    
    // Parse the JSON to get note data with beat timing
    auto notesJson = juce::JSON::parse(sequenceJSON);
    if (notesJson.isObject())
    {
        auto* notesObj = notesJson.getDynamicObject();
        auto notesArray = notesObj->getProperty("notes");
        
        if (notesArray.isArray())
        {
            for (int i = 0; i < notesArray.size(); i++)
            {
                auto note = notesArray[i];
                if (note.isObject())
                {
                    auto* noteObj = note.getDynamicObject();
                    float startBeats = noteObj->getProperty("start_beats");
                    float durationBeats = noteObj->getProperty("duration_beats");
                    int midiNote = noteObj->getProperty("midi_note");
                    int velocity = noteObj->getProperty("velocity");
                    
                    // Convert beats to MIDI ticks (480 ticks per quarter note is standard)
                    double startTicks = startBeats * TICKS_PER_QUARTER_NOTE;
                    double endTicks = (startBeats + durationBeats) * TICKS_PER_QUARTER_NOTE;
    
                    
                    // Add note on
                    track.addEvent(juce::MidiMessage::noteOn(scheduledMidiChannel, midiNote, (juce::uint8)velocity), startTicks);
                    // Add note off
                    track.addEvent(juce::MidiMessage::noteOff(scheduledMidiChannel, midiNote), endTicks);
                }
            }
        }
    }
    
    // If no notes were added, return invalid file 
    if (track.getNumEvents() == 0)
    {
        DBG("No notes to write to MIDI file");
        return juce::File();
    }
    
    track.updateMatchedPairs();
    midiFile.addTrack(track);
    midiFile.setTicksPerQuarterNote(TICKS_PER_QUARTER_NOTE);
    
    // Create file in temp directory with timestamp for guaranteed uniqueness
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::String timestamp = juce::String(juce::Time::currentTimeMillis());
    juce::String filename = "generated_sequence_" + timestamp + ".mid";
    juce::File midiFileOutput = tempDir.getChildFile(filename);
    
    {
        juce::FileOutputStream outputStream(midiFileOutput);
        if (outputStream.openedOk())
        {
            midiFile.writeTo(outputStream);
            outputStream.flush();
            DBG("MIDI file created: " + midiFileOutput.getFullPathName());
            
            // Track this file for cleanup
            createdMidiFiles.push_back(midiFileOutput);
        }
        else
        {
            DBG("Failed to create MIDI file");
            return juce::File();
        }
    } // outputStream goes out of scope and closes here
    
    return midiFileOutput;
}

void Generator::sendToGenerator(const juce::String& prompt,
                               std::function<void(juce::String)> callback)
{
    if (apiKey.isEmpty())
    {
        DBG("Error: API key not set");
        if (callback)
            callback("Error: API key not set");
        return;
    }

    // Set loading flag at the start
    loading = true;

    // Build JSON request 
    juce::DynamicObject::Ptr jsonBody = new juce::DynamicObject();
    jsonBody->setProperty("model", "gpt-5-nano-2025-08-07");
    jsonBody->setProperty("input", apiInstructions + "\n\nUser prompt:\n" + prompt);

    // Force JSON-only output 
    juce::DynamicObject::Ptr formatConfig = new juce::DynamicObject();
    formatConfig->setProperty("type", "json_object");
    juce::DynamicObject::Ptr textConfig = new juce::DynamicObject();
    textConfig->setProperty("format", juce::var(formatConfig.get()));
    jsonBody->setProperty("text", juce::var(textConfig.get()));

    juce::String jsonString = juce::JSON::toString(juce::var(jsonBody.get()));

    DBG("Request URL: " + apiEndpoint);
    DBG("Request Body: " + jsonString);

    juce::URL url = juce::URL(apiEndpoint).withPOSTData(jsonString);

    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
        .withExtraHeaders(
            "Content-Type: application/json\r\n"
            "Authorization: Bearer " + apiKey
        )
        .withConnectionTimeoutMs(MAX_TIMEOUT_MS)
        .withNumRedirectsToFollow(MAX_REDIRECTS);


    // Send request on a background thread
    // Capture shared state to safely check if Generator still exists
    auto state = sharedState;
    juce::Thread::launch([state, url, options, callback, this]() mutable
    {
        DBG("Starting HTTP request...");

        int statusCode = 0; 

        auto stream = url.createInputStream(options.withStatusCode(&statusCode));

        if (stream == nullptr)
        {
            DBG("Failed to create stream. Status code: " + juce::String(statusCode));
            juce::MessageManager::callAsync([state, callback, statusCode, this]()
            {
                if (state->isValid)
                {
                    loading = false;
                }
                if (callback)
                    callback("Error: Failed to connect (status " + juce::String(statusCode) + ")");
            });
            return; 
        }

        juce::String response = stream->readEntireStreamAsString();
        DBG("Status Code: " + juce::String(statusCode));
        DBG("OpenAI Raw Response:\n" + response);

        if (statusCode != 200)
        {
            juce::MessageManager::callAsync([state, callback, response, statusCode, this]()
            {
                if (state->isValid)
                {
                    loading = false;
                }
                if (callback)
                    callback("API error " + juce::String(statusCode) + ":\n" + response);
            });
            return;
        }
        
        // Call back on main thread - the callback will handle getSequenceJSON
        juce::MessageManager::callAsync([state, this, callback, response]()
        {
            // Check if Generator is still valid before accessing member variables
            if (state->isValid)
            {
                // Parse the sequence JSON on the main thread
                getSequenceJSON(response);
                // Clear loading flag after successful response
                loading = false;
            }
            
            if (callback)
                callback(response);
        });
    });
    
}