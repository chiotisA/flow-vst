#include "FlowSamplerVoice.h"
#include "FlowSamplerSound.h"

bool FlowSamplerVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<FlowSamplerSound*> (sound) != nullptr;
}

void FlowSamplerVoice::prepare (double sampleRate, int maxBlockSize)
{
    // Time-stretch ratio is clamped to [0.25, 4.0] in renderStretched() — size the input
    // staging buffer for the most input a single block could ever need at that extreme.
    constexpr double maxStretchRatio = 4.0;
    const int maxInputSamples = (int) ((double) maxBlockSize * maxStretchRatio) + 256;

    stagingInput.setSize (2, maxInputSamples, false, false, true);
    stagingOutput.setSize (2, juce::jmax (maxBlockSize, 256), false, false, true);
    stagingPositions.assign ((size_t) maxInputSamples, 0);

    stretcher.presetDefault (2, (float) sampleRate);
    stretcherPrepared = true;
}

void FlowSamplerVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound* sound, int /*currentPitchWheelPosition*/)
{
    playingSound = dynamic_cast<FlowSamplerSound*> (sound);
    jassert (playingSound != nullptr);

    sourceSamplePosition = (double) playingSound->trimStart.load();
    stretchSourcePosition = sourceSamplePosition;
    level = velocity;
    isReleasing = false;

    const auto semitones = midiNoteNumber - playingSound->rootMidiNote.load();
    pitchRatio = std::pow (2.0, semitones / 12.0) * playingSound->sourceSampleRate / getSampleRate();

    if (stretcherPrepared)
    {
        // reset() clears leftover STFT state from whatever note last used this voice — no
        // allocation, safe to call here even though this runs on the audio thread.
        stretcher.reset();
        stretcher.setTransposeSemitones ((float) semitones);
    }

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

    if (playingSound->data.getNumSamples() == 0)
    {
        clearCurrentNote();
        playingSound = nullptr;
        return;
    }

    // Time-stretch needs the engine pre-allocated (prepare() already ran) and a known
    // native tempo to compute a ratio against — most One-Shots have neither reason nor
    // data for this, so they silently fall back to plain playback regardless of the toggle.
    const bool useStretch = stretcherPrepared
                          && playingSound->timeStretchEnabled.load()
                          && playingSound->nativeBpm > 0;

    if (useStretch)
        renderStretched (outputBuffer, startSample, numSamples);
    else
        renderPlain (outputBuffer, startSample, numSamples);
}

void FlowSamplerVoice::renderPlain (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    const auto& data = playingSound->data;
    const int sourceNumChannels = data.getNumChannels();
    const int sourceNumSamples = data.getNumSamples();

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

void FlowSamplerVoice::renderStretched (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    const auto& data = playingSound->data;
    const int sourceNumChannels = data.getNumChannels();
    const int sourceNumSamples = data.getNumSamples();

    const int trimEnd = juce::jlimit (1, sourceNumSamples, playingSound->trimEnd.load());
    const bool isLooping = playingSound->loopingEnabled.load();
    const int loopStart = juce::jlimit (0, trimEnd - 1, playingSound->loopStart.load());
    const int loopEnd = juce::jlimit (loopStart + 1, trimEnd, playingSound->loopEnd.load());

    const double hostBpmValue = hostBpmSource != nullptr ? hostBpmSource->load() : 120.0;
    const double stretchRatioRaw = hostBpmValue / (double) playingSound->nativeBpm;
    const double stretchRatio = juce::jlimit (0.25, 4.0, stretchRatioRaw);

    const int outputNumChannels = juce::jmin (2, outputBuffer.getNumChannels());
    const int maxInput = stagingInput.getNumSamples();
    const int inputSamplesNeeded = juce::jlimit (1, maxInput, (int) std::round ((double) numSamples * stretchRatio));

    // The source-position sequence is the same for every channel (it's just time) —
    // compute it once into stagingPositions rather than redoing this per channel.
    {
        int pos = (int) stretchSourcePosition;
        for (int i = 0; i < inputSamplesNeeded; ++i)
        {
            stagingPositions[(size_t) i] = juce::jlimit (0, sourceNumSamples - 1, pos);
            ++pos;

            if (isLooping && pos >= loopEnd)
                pos = loopStart;
            else if (! isLooping && pos >= trimEnd)
                pos = juce::jmax (0, trimEnd - 1); // hold at the last sample, envelope fades below
        }
        stretchSourcePosition = pos;
    }

    // A one-shot (or loop with looping disabled) that's run out of new material needs the
    // same "fade instead of cut" treatment renderPlain() gives it.
    if (! isLooping && ! isReleasing && (int) stretchSourcePosition >= trimEnd - 1)
    {
        isReleasing = true;
        releaseSamplesRemaining = releaseSamplesTotal;
    }

    for (int ch = 0; ch < outputNumChannels; ++ch)
    {
        const int sourceChannel = juce::jmin (ch, sourceNumChannels - 1);
        const float* sourceData = data.getReadPointer (sourceChannel);
        float* dst = stagingInput.getWritePointer (ch);

        for (int i = 0; i < inputSamplesNeeded; ++i)
            dst[i] = sourceData[stagingPositions[(size_t) i]];
    }

    float* inPtrs[2]  = { stagingInput.getWritePointer (0),  stagingInput.getWritePointer (outputNumChannels > 1 ? 1 : 0) };
    float* outPtrs[2] = { stagingOutput.getWritePointer (0), stagingOutput.getWritePointer (outputNumChannels > 1 ? 1 : 0) };

    stretcher.process (inPtrs, inputSamplesNeeded, outPtrs, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        float envelope = level;

        if (isReleasing)
        {
            envelope *= (float) releaseSamplesRemaining / (float) juce::jmax (1, releaseSamplesTotal);
            --releaseSamplesRemaining;
        }

        for (int ch = 0; ch < outputNumChannels; ++ch)
            outputBuffer.addSample (ch, startSample + i, stagingOutput.getReadPointer (ch)[i] * envelope);

        if (isReleasing && releaseSamplesRemaining <= 0)
        {
            playingSound->currentPlaybackPosition.store (-1);
            clearCurrentNote();
            playingSound = nullptr;
            return;
        }
    }

    if (playingSound != nullptr)
        playingSound->currentPlaybackPosition.store ((int) stretchSourcePosition);
}
