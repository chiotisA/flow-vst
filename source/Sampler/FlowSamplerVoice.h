#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// Vendored third-party header (modules/signalsmith-stretch/) — its FFT internals trigger
// double->float truncation and shadowed-variable warnings under MSVC's warning level.
// Scoped suppression rather than editing vendored code or weakening our own warning flags.
#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include "signalsmith-stretch.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <vector>

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

    // Called once from PluginProcessor::prepareToPlay (message thread) — pre-allocates the
    // time-stretch engine's internal STFT buffers so startNote()/renderNextBlock() (audio
    // thread) never need to allocate. Must run before time-stretch is ever enabled.
    void prepare (double sampleRate, int maxBlockSize);

    // Points at PluginProcessor::hostBpm — set once right after construction, read every
    // block. Never null in practice (PluginProcessor always sets it), but guarded anyway.
    void setHostBpmSource (const std::atomic<double>* hostBpmSourceIn) { hostBpmSource = hostBpmSourceIn; }

private:
    void renderPlain (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples);
    void renderStretched (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples);

    class FlowSamplerSound* playingSound = nullptr;
    double sourceSamplePosition = 0.0;
    double pitchRatio = 1.0;
    float level = 0.0f;

    bool isReleasing = false;
    int releaseSamplesRemaining = 0;
    int releaseSamplesTotal = 0;

    const std::atomic<double>* hostBpmSource = nullptr;

    signalsmith::stretch::SignalsmithStretch<float> stretcher;
    bool stretcherPrepared = false;
    double stretchSourcePosition = 0.0; // separate position tracker, only used in stretch mode

    // Fixed-capacity staging buffers, sized once in prepare() (message thread) — reading/
    // writing existing capacity in renderStretched() never allocates.
    juce::AudioBuffer<float> stagingInput, stagingOutput;
    std::vector<int> stagingPositions;
};
