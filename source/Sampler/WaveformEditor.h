#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "FlowSamplerSound.h"

// Draws the loaded sample's waveform with four draggable markers (trim start/end,
// loop start/end) and writes drags straight into the sound's atomics. Runs on the
// message thread only; FlowSamplerVoice reads the same atomics from the audio thread.
// Also polls sound.currentPlaybackPosition on a timer to draw a live playhead.
class WaveformEditor : public juce::Component, private juce::Timer
{
public:
    explicit WaveformEditor (FlowSamplerSound& soundToEdit);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    void updateRootNoteLabel();

    // Everything below this height is the actual waveform/markers; the strip itself is
    // real child components (label + buttons), so clicks there never reach mouseDown().

    enum class Marker { none, trimStart, trimEnd, loopStart, loopEnd };

    // Keeps trimStart < trimEnd and loopStart/loopEnd inside [trimStart, trimEnd) after
    // every drag — without this, dragging one marker past another leaves the others'
    // valid range inverted (lower bound > upper bound), which crashes jlimit().
    void normalizeRegions();

    float sampleToX (int sample) const;
    int xToSample (float x) const;
    Marker findMarkerNear (float x) const;

    FlowSamplerSound& sound;
    Marker draggingMarker = Marker::none;

    juce::Label rootNoteLabel;
    juce::TextButton rootDownButton { "-" };
    juce::TextButton rootUpButton { "+" };
    juce::TextButton detectPitchButton { "Detect Pitch" };

    static constexpr int grabRadius = 6;
    static constexpr int controlStripHeight = 24;
};
