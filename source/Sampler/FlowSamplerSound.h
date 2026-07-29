#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// Holds one loaded sample's audio data and note-mapping range. Deliberately not
// juce::SamplerSound: its data buffer is private (only its own SamplerVoice can read it),
// and we need direct buffer access here for loop-point playback.
//
// Trim/loop points are written from the editor (message thread) and read every sample
// by FlowSamplerVoice (audio thread) — plain std::atomic<int>, no lock, since each is an
// independent value and torn reads just mean a marker takes effect one block later.
class FlowSamplerSound : public juce::SynthesiserSound
{
public:
    FlowSamplerSound (juce::AudioBuffer<float> audioData, double sourceSampleRateIn, int rootMidiNoteIn)
        : data (std::move (audioData)), sourceSampleRate (sourceSampleRateIn), rootMidiNote (rootMidiNoteIn)
    {
        const auto numSamples = data.getNumSamples();
        trimStart.store (0);
        trimEnd.store (numSamples);
        loopStart.store (0);
        loopEnd.store (numSamples);
    }

    bool appliesToNote (int /*midiNoteNumber*/) override { return true; }
    bool appliesToChannel (int /*midiChannel*/) override { return true; }

    int getNumSamples() const { return data.getNumSamples(); }

    const juce::AudioBuffer<float> data;
    const double sourceSampleRate;

    // Starts at a fixed default octave derived from the catalog's key tag (no pitch
    // detection — deliberately dropped, unreliable on drums/FX/noisy content). Mutable so
    // the user can relocate it by octave (e.g. down to A1 for a bass one-shot) to wherever
    // it actually belongs by ear.
    std::atomic<int> rootMidiNote;

    std::atomic<int> trimStart, trimEnd;
    std::atomic<int> loopStart, loopEnd;
    std::atomic<bool> loopingEnabled { true };

    // Written by whichever FlowSamplerVoice is currently playing this sound, read by
    // WaveformEditor's timer to draw a live playhead. -1 means "not currently playing".
    // With overlapping notes the last voice to render in a block wins — fine for a
    // debug-visualization feature, not a correctness-critical value.
    std::atomic<int> currentPlaybackPosition { -1 };
};
