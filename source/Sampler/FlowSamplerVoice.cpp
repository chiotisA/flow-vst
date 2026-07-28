#include "FlowSamplerVoice.h"
#include "FlowSamplerSound.h"

bool FlowSamplerVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<FlowSamplerSound*> (sound) != nullptr;
}

void FlowSamplerVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound* sound, int /*currentPitchWheelPosition*/)
{
    playingSound = dynamic_cast<FlowSamplerSound*> (sound);
    jassert (playingSound != nullptr);

    sourceSamplePosition = 0.0;
    level = velocity;
    isReleasing = false;

    const auto semitones = midiNoteNumber - playingSound->rootMidiNote;
    pitchRatio = std::pow (2.0, semitones / 12.0) * playingSound->sourceSampleRate / getSampleRate();

    constexpr double releaseTimeSeconds = 0.05;
    releaseSamplesTotal = (int) (releaseTimeSeconds * getSampleRate());
    releaseSamplesRemaining = 0;
}

void FlowSamplerVoice::stopNote (float /*velocity*/, bool allowTailOff)
{
    if (allowTailOff)
    {
        isReleasing = true;
        releaseSamplesRemaining = releaseSamplesTotal;
    }
    else
    {
        clearCurrentNote();
        playingSound = nullptr;
    }
}

void FlowSamplerVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (playingSound == nullptr)
        return;

    const auto& data = playingSound->data;
    const int sourceNumChannels = data.getNumChannels();
    const int sourceNumSamples = data.getNumSamples();

    if (sourceNumSamples == 0)
    {
        clearCurrentNote();
        playingSound = nullptr;
        return;
    }

    const int outputNumChannels = outputBuffer.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        const int pos0 = (int) sourceSamplePosition;
        const int pos1 = (pos0 + 1) % sourceNumSamples;
        const float frac = (float) (sourceSamplePosition - (double) pos0);

        float envelope = level;

        if (isReleasing)
        {
            envelope *= (float) releaseSamplesRemaining / (float) juce::jmax (1, releaseSamplesTotal);
            --releaseSamplesRemaining;
        }

        for (int ch = 0; ch < outputNumChannels; ++ch)
        {
            const int sourceChannel = juce::jmin (ch, sourceNumChannels - 1);
            const float* sourceData = data.getReadPointer (sourceChannel);
            const float sample = sourceData[pos0] + frac * (sourceData[pos1] - sourceData[pos0]);

            outputBuffer.addSample (ch, startSample + i, sample * envelope);
        }

        sourceSamplePosition += pitchRatio;
        if (sourceSamplePosition >= sourceNumSamples)
            sourceSamplePosition -= sourceNumSamples;

        if (isReleasing && releaseSamplesRemaining <= 0)
        {
            clearCurrentNote();
            playingSound = nullptr;
            break;
        }
    }
}
