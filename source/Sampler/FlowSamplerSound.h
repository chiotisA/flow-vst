#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// Holds one loaded sample's audio data and note-mapping range. Deliberately not
// juce::SamplerSound: its data buffer is private (only its own SamplerVoice can read it),
// and we need direct buffer access here for loop-point playback (added in a later step).
class FlowSamplerSound : public juce::SynthesiserSound
{
public:
    FlowSamplerSound (juce::AudioBuffer<float> audioData, double sourceSampleRateIn, int rootMidiNoteIn)
        : data (std::move (audioData)), sourceSampleRate (sourceSampleRateIn), rootMidiNote (rootMidiNoteIn)
    {
    }

    bool appliesToNote (int /*midiNoteNumber*/) override { return true; }
    bool appliesToChannel (int /*midiChannel*/) override { return true; }

    const juce::AudioBuffer<float> data;
    const double sourceSampleRate;
    const int rootMidiNote;
};
