#include "Generator.h"
#include "MidiNoteEvent.h"
#include "MidiNote.h"

#define MAX_TIMEOUT_MS 300000 // 5 minutes before aborting POST request 
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

/**
 * @brief Creates a string representation of the MIDI note sequence from the string JSON response 
 * @param apiResponse The raw string response from the OpenAI API
 */
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
        DBG("API key successfully loaded ");
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

/**
 * @brief Computes total number of notes in the sequence from the sequenceJSON string 
 */
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

/**
 * @brief Converts string representation of the MIDI note sequence into a vector of MidiNote objects that can be processed in the audio thread 
 * @param bpm Beats Per Minute tempo of the host 
 * @param sampleRate Sample rate of the host 
 */
void Generator::extractSequence(double bpm, double sampleRate)
{
    
    if (sequenceJSON.startsWith("Error:"))
        return;

    int runningSampleCount = triggerDelaySamples;  // Start with initial delay to ensure notes are scheduled in the future
    noteSequence.clear(); // Clear previous note sequence if it exists

    // Parse the JSON to get note data with beat timing 
    auto notesJson = juce::JSON::parse(sequenceJSON);
    if (notesJson.isObject())
    {
        auto* notesObj = notesJson.getDynamicObject();
        auto notesArray = notesObj->getProperty("notes");
        
        if (notesArray.isArray())
        {
            DBG("Found " + juce::String(notesArray.size()) + " notes");

            // Loop through each note in the JSON array and create MidiNote objects with timing parameters 
            for (int i = 0; i < notesArray.size(); i++)
            {
                auto note = notesArray[i];
                if (note.isObject())
                {

                    // Extract note information from the string JSON 
                    auto* noteObj = note.getDynamicObject();
                    float startBeats = noteObj->getProperty("start_beats");
                    float durationBeats = noteObj->getProperty("duration_beats");
                    int midiNote = noteObj->getProperty("midi_note");
                    int velocity = noteObj->getProperty("velocity"); 

                    double startInSeconds = (startBeats * 60.0) / bpm;
                    double durationInSeconds = (durationBeats * 60.0) / bpm;
                    int startSamples = triggerDelaySamples + juce::jmax(1, (int) std::round(startInSeconds * sampleRate)); 
                    int noteDurationSamples = juce::jmax(1, (int) std::round(durationInSeconds * sampleRate));

                    // Create midi events and add to the sequence 
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

/**
 * @brief Processes all the notes in the noteSequence and updates their timing parameters accordingly 
 * @param blockSize The size of the current audio processing block in samples
 * @param midiMessages The MIDI buffer to which the note-on and note-off events should be added when triggered 
 */
void Generator::processSequence(int blockSize, juce::MidiBuffer& midiMessages)
{
    // Loop through all notes 
    for(auto it = noteSequence.begin(); it != noteSequence.end(); ++it) {
        it->processNote(blockSize, midiMessages); // Update the note's timing parameters and add events to MIDI buffer if triggered

        // Check if note has finished playing and update sequenceTracker accordingly to track overall sequence completion
        if(it->isFinished() && !it->hasBeenCounted()) {
            it->markAsCounted();
            sequenceTracker++; 
        }
    }
}

/**
 * @brief Checks if a sequence has finished playing 
 * @return true if all notes in the sequence have been processed and played, false otherwise 
 */
bool Generator::isSequenceFinished() {
    if(sequenceTracker >= noteSequence.size()) {
        sequenceTracker = 0; // Reset for the next sequence 
        return true; 
    }
    return false;
}

/**
 * @brief Resets all notes in the sequence to their original timing parameters and resets the sequenceTracker, allowing the entire sequence to be replayed from the beginning
 */
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

/**
 * @brief Sends user's prompt to OpenAI API and handle JSON response 
 * @param prompt User's prompt 
 * @param recentPrompts An array of the recent prompts to provide context to the API
 * @param callback A function to call that takes in a juce::String with the API response once it has been received and processed 
 */
void Generator::sendToGenerator(const juce::String& prompt,
                                const juce::StringArray& recentPrompts,
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
    juce::String requestInput = apiInstructions;
    if (recentPrompts.size() > 0)
    {

        // Add recent prompts as context to the request input 
        requestInput << "\n\nContext from previous user prompts (oldest to newest):";
        for (int i = 0; i < recentPrompts.size(); ++i)
            requestInput << "\n- Previous prompt " + juce::String(i + 1) + ": " + recentPrompts[i];
    }
    requestInput << "\n\nCurrent user prompt:\n" + prompt;

    juce::DynamicObject::Ptr jsonBody = new juce::DynamicObject();
    jsonBody->setProperty("model", "gpt-5.2-2025-12-11");
    jsonBody->setProperty("input", requestInput);

    // Create nested JSON object to force API to return a JSON response that can be easily parsed 
    juce::DynamicObject::Ptr formatConfig = new juce::DynamicObject();
    formatConfig->setProperty("type", "json_object");
    juce::DynamicObject::Ptr textConfig = new juce::DynamicObject();
    textConfig->setProperty("format", juce::var(formatConfig.get()));
    jsonBody->setProperty("text", juce::var(textConfig.get()));

    juce::String jsonString = juce::JSON::toString(juce::var(jsonBody.get()));

    DBG("Request URL: " + apiEndpoint);
    DBG("Request Body: " + jsonString);

    // Build POST request 
    juce::URL url = juce::URL(apiEndpoint).withPOSTData(jsonString);
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
        .withExtraHeaders(
            "Content-Type: application/json\r\n"
            "Authorization: Bearer " + apiKey
        )
        .withConnectionTimeoutMs(MAX_TIMEOUT_MS)
        .withNumRedirectsToFollow(MAX_REDIRECTS);


    // Send POST request on a background thread to prevent freezing the UI 
    auto state = sharedState;
    juce::Thread::launch([state, url, options, callback, this]() mutable
    {
        DBG("Starting HTTP request...");

        int statusCode = 0; // variable holding HTTP status code from response 

        auto stream = url.createInputStream(options.withStatusCode(&statusCode)); // Send the API request and capture pointer to input stream + HTTP status code 

        // Handle connection errors
        if (stream == nullptr)
        {
            DBG("Failed to create stream. Status code: " + juce::String(statusCode));

            // Call back on main thread to update UI and trigger any callbacks, but only if Generator is still valid
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

        // Read the entire response as a string
        juce::String response = stream->readEntireStreamAsString();
        DBG("Status Code: " + juce::String(statusCode));
        DBG("OpenAI Raw Response:\n" + response);
        
        // Handle non-200 HTTP responses as errors
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

        // Queue callback on main thread in the case of a successful response
        juce::MessageManager::callAsync([state, this, callback, response]()
        {
            if (state->isValid)
            {
                // Parse the sequence JSON on the main thread 
                getSequenceJSON(response);
                loading = false;
            }
            
            if (callback)
                callback("Kiwi");
        });
    });
    
}