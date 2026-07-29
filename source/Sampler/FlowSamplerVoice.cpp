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

    sourceSamplePosition = (double) playingSound->trimStart.load();
    level = velocity;
    isReleasing = false;

    const auto semitones = midiNoteNumber - playingSound->rootMidiNote.load();
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
        if (playingSound != nullptr)
            playingSound->currentPlaybackPosition.store (-1);

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

    // Snapshot the editable region once per block — a marker drag mid-block just takes
    // effect on the next block, which is inaudible and keeps this lock-free.
    const int trimEnd = juce::jlimit (1, sourceNumSamples, playingSound->trimEnd.load());
    const bool isLooping = playingSound->loopingEnabled.load();
    const int loopStart = juce::jlimit (0, trimEnd - 1, playingSound->loopStart.load());
    const int loopEnd = juce::jlimit (loopStart + 1, trimEnd, playingSound->loopEnd.load());

    const int outputNumChannels = outputBuffer.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        const int pos0 = juce::jmin ((int) sourceSamplePosition, sourceNumSamples - 1);
        const int pos1 = juce::jmin (pos0 + 1, sourceNumSamples - 1);
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

        if (isLooping && sourceSamplePosition >= loopEnd)
        {
            sourceSamplePosition = loopStart + (sourceSamplePosition - loopEnd);
        }
        else if (! isReleasing && sourceSamplePosition >= trimEnd)
        {
            // Reached the end without looping (one-shot, or loop disabled) — fade out
            // instead of cutting hard.
            isReleasing = true;
            releaseSamplesRemaining = releaseSamplesTotal;
        }

        if (isReleasing && releaseSamplesRemaining <= 0)
        {
            playingSound->currentPlaybackPosition.store (-1);
            clearCurrentNote();
            playingSound = nullptr;
            break;
        }
    }

    if (playingSound != nullptr)
        playingSound->currentPlaybackPosition.store ((int) sourceSamplePosition);
}
