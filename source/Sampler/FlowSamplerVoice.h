#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// Plays a FlowSamplerSound, looping the whole buffer for as long as the note is held.
// Loop-point editing (step 2 of the Flow build order) will narrow the loop region this
// voice reads from; for now it loops start-to-end of the full sample.
class FlowSamplerVoice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound (juce::SynthesiserSound*) override;

    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound* sound, int currentPitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

private:
    class FlowSamplerSound* playingSound = nullptr;
    double sourceSamplePosition = 0.0;
    double pitchRatio = 1.0;
    float level = 0.0f;

    bool isReleasing = false;
    int releaseSamplesRemaining = 0;
    int releaseSamplesTotal = 0;
};
