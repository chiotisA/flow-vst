#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>

// On-demand pitch detection (the "Detect Pitch" button) — a guitar-tuner-style YIN
// implementation. Runs on the message thread only, triggered by a button click, never
// from the audio thread: a few thousand samples analyzed once is negligible CPU, but it's
// still real work (an O(maxLag * analysisLength) loop) that has no business running per
// audio block.
//
// YIN measures how well the waveform matches a delayed copy of itself at each candidate
// lag (like a tuner comparing a note against itself one cycle later); the shortest lag
// with a strong match is the fundamental period, which converts directly to frequency.
// Chordal/percussive/noisy content won't produce a confident match — this deliberately
// returns 0.0 (not a guess) in that case, since Flow always needs manual override anyway.
inline double detectPitchYin (const float* samples, int numSamples, double sampleRate,
                               double minFrequencyHz = 40.0, double maxFrequencyHz = 1000.0)
{
    if (sampleRate <= 0.0 || numSamples <= 0)
        return 0.0;

    const int maxLag = (int) (sampleRate / minFrequencyHz);
    const int minLag = juce::jmax (1, (int) (sampleRate / maxFrequencyHz));
    const int analysisLength = juce::jmin (numSamples - maxLag, 4096);

    if (analysisLength <= 0 || maxLag <= minLag)
        return 0.0;

    // Difference function: for each candidate lag, how much the signal differs from
    // itself shifted by that lag. A true periodic signal has a deep dip at its period.
    std::vector<double> difference ((size_t) maxLag + 1, 0.0);
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double sum = 0.0;
        for (int i = 0; i < analysisLength; ++i)
        {
            const double delta = (double) samples[i] - (double) samples[i + lag];
            sum += delta * delta;
        }
        difference[(size_t) lag] = sum;
    }

    // Cumulative mean normalized difference — this is the step that makes YIN prefer the
    // true fundamental over its harmonics, unlike plain autocorrelation.
    std::vector<double> cmnd ((size_t) maxLag + 1, 1.0);
    double runningSum = 0.0;
    for (int lag = 1; lag <= maxLag; ++lag)
    {
        runningSum += difference[(size_t) lag];
        cmnd[(size_t) lag] = runningSum > 0.0 ? difference[(size_t) lag] * lag / runningSum : 1.0;
    }

    constexpr double threshold = 0.15;
    int bestLag = -1;

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        if (cmnd[(size_t) lag] < threshold)
        {
            // First dip under threshold, walked forward to its local minimum — the
            // standard YIN refinement, avoids locking onto the leading edge of the dip.
            while (lag + 1 <= maxLag && cmnd[(size_t) (lag + 1)] < cmnd[(size_t) lag])
                ++lag;

            bestLag = lag;
            break;
        }
    }

    if (bestLag <= 0)
        return 0.0; // No confident periodicity found — silence, noise, or percussive content.

    return sampleRate / (double) bestLag;
}

inline int frequencyToMidiNote (double frequencyHz)
{
    return (int) std::round (69.0 + 12.0 * std::log2 (frequencyHz / 440.0));
}
