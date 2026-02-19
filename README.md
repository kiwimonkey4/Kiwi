# Kiwi

Kiwi is a JUCE-based MIDI generation plugin with a companion analytics stack:

- A desktop plugin that sends prompts to OpenAI, receives structured note JSON, and outputs drag-and-drop MIDI stems. 
- A TypeScript analytics API that validates plugin data with Zod, feeds it into Firestore and serves filtered metrics as well. 
- A Next.js dashboard that visualizes usage, generation quality, and conversion behavior.

## Architecture Overview

```text
User prompt in plugin UI
        |
        v
OpenAI Responses API (JSON note sequence)
        |
        v
Plugin parses notes -> schedules MIDI -> creates .mid file
        |
        +--> AnalyticsService (local queue) --> analytics-api --> Firestore
                                                     |
                                                     v
                                             analytics-dashboard
```

analytics-api hosted w/ Railway, analytics dashboard hosted w/ Vercel 

## Repository Layout

- `Source/` - JUCE plugin source (`PluginEditor`, `PluginProcessor`, `Generator`, analytics instrumentation)
- `Source/analytics-api/` - Express + Zod + Firebase Admin service
- `Source/analytics-dashboard/` - Next.js dashboard client
- `KiwiPlugin.jucer` - Projucer project definition

## How the Plugin Works

### 1) Prompt -> OpenAI -> Note JSON

`Generator::sendToGenerator` sends a request to `https://api.openai.com/v1/responses` with:

- model: `gpt-5.2-2025-12-11`
- strict instructions to return JSON only
- the current prompt plus up to 2 previous prompts of context for more relevant results 

Expected model output format:

```json
{
  "notes": [
    {
      "start_beats": 0.0,
      "duration_beats": 0.5,
      "midi_note": 60,
      "velocity": 100
    }
  ]
}
```

### 2) JSON -> Playback + MIDI file

- `Generator::extractSequence` parses the returned `notes` array and converts beat timing to sample positions using host BPM and sample rate.
- `PluginProcessor::processBlock` emits MIDI note-on/off events while the sequence is active.
- `Generator::createMidiFile` writes the generated sequence to a temporary `.mid` file for drag-and-drop into a DAW.

