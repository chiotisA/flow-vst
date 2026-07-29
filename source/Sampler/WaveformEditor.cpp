#include "WaveformEditor.h"

WaveformEditor::WaveformEditor (FlowSamplerSound& soundToEdit) : sound (soundToEdit)
{
    setInterceptsMouseClicks (true, false);
    startTimerHz (30);

    rootNoteLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (rootNoteLabel);

    // Clamp to the valid MIDI range rather than wrapping — wrapping octave 9 back down
    // to octave 0 on one more click would be a surprising, easy-to-trigger UI trap.
    octaveDownButton.onClick = [this]
    {
        const auto note = sound.rootMidiNote.load();
        if (note - 12 >= 0)
            sound.rootMidiNote.store (note - 12);
        updateRootNoteLabel();
    };
    octaveUpButton.onClick = [this]
    {
        const auto note = sound.rootMidiNote.load();
        if (note + 12 <= 127)
            sound.rootMidiNote.store (note + 12);
        updateRootNoteLabel();
    };
    addAndMakeVisible (octaveDownButton);
    addAndMakeVisible (octaveUpButton);

    updateRootNoteLabel();
}

void WaveformEditor::updateRootNoteLabel()
{
    const auto note = sound.rootMidiNote.load();
    rootNoteLabel.setText ("Root: " + juce::MidiMessage::getMidiNoteName (note, true, true, 3),
                            juce::dontSendNotification);
}

void WaveformEditor::resized()
{
    auto strip = getLocalBounds().removeFromTop (controlStripHeight);
    octaveUpButton.setBounds (strip.removeFromRight (70));
    octaveDownButton.setBounds (strip.removeFromRight (70));
    rootNoteLabel.setBounds (strip);
}

float WaveformEditor::sampleToX (int sample) const
{
    const auto numSamples = juce::jmax (1, sound.getNumSamples());
    return (float) sample / (float) numSamples * (float) getWidth();
}

int WaveformEditor::xToSample (float x) const
{
    const auto numSamples = sound.getNumSamples();
    return juce::jlimit (0, numSamples - 1, (int) (x / (float) getWidth() * (float) numSamples));
}

WaveformEditor::Marker WaveformEditor::findMarkerNear (float x) const
{
    struct Candidate { Marker marker; int sample; };
    const Candidate candidates[] {
        { Marker::trimStart, sound.trimStart.load() },
        { Marker::trimEnd,   sound.trimEnd.load() },
        { Marker::loopStart, sound.loopStart.load() },
        { Marker::loopEnd,   sound.loopEnd.load() }
    };

    Marker closest = Marker::none;
    float closestDistance = (float) grabRadius;

    for (auto& c : candidates)
    {
        const auto distance = std::abs (sampleToX (c.sample) - x);
        if (distance <= closestDistance)
        {
            closestDistance = distance;
            closest = c.marker;
        }
    }

    return closest;
}

void WaveformEditor::mouseDown (const juce::MouseEvent& e)
{
    // The control strip's own children (label/buttons) normally consume their clicks
    // before this ever runs, but the label doesn't intercept clicks — without this guard,
    // clicking it while it happens to sit above a marker's x-column would start a drag.
    if (e.position.y < (float) controlStripHeight)
    {
        draggingMarker = Marker::none;
        return;
    }

    draggingMarker = findMarkerNear ((float) e.position.x);
}

void WaveformEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingMarker == Marker::none)
        return;

    const auto newSample = xToSample ((float) e.position.x);
    const auto numSamples = sound.getNumSamples();

    // Only clamp against the fixed buffer length here — cross-marker ordering
    // (trim vs loop) is resolved afterwards in normalizeRegions(), never inline,
    // so we never hand jlimit() a range some other marker has already inverted.
    switch (draggingMarker)
    {
        case Marker::trimStart: sound.trimStart.store (juce::jlimit (0, numSamples - 1, newSample)); break;
        case Marker::trimEnd:   sound.trimEnd.store   (juce::jlimit (1, numSamples, newSample)); break;
        case Marker::loopStart: sound.loopStart.store (juce::jlimit (0, numSamples - 1, newSample)); break;
        case Marker::loopEnd:   sound.loopEnd.store   (juce::jlimit (1, numSamples, newSample)); break;
        case Marker::none: break;
    }

    normalizeRegions();
    repaint();
}

void WaveformEditor::normalizeRegions()
{
    const auto numSamples = sound.getNumSamples();

    int trimStart = sound.trimStart.load();
    int trimEnd = sound.trimEnd.load();

    if (trimStart >= trimEnd)
    {
        // Push whichever marker the user *didn't* just drag, so the one they're
        // actively dragging keeps responding instead of snapping back.
        if (draggingMarker == Marker::trimStart)
            trimEnd = trimStart + 1;
        else
            trimStart = trimEnd - 1;
    }

    trimStart = juce::jlimit (0, numSamples - 1, trimStart);
    trimEnd = juce::jlimit (trimStart + 1, numSamples, trimEnd);

    int loopStart = juce::jlimit (trimStart, trimEnd - 1, sound.loopStart.load());
    int loopEnd = juce::jlimit (loopStart + 1, trimEnd, sound.loopEnd.load());

    sound.trimStart.store (trimStart);
    sound.trimEnd.store (trimEnd);
    sound.loopStart.store (loopStart);
    sound.loopEnd.store (loopEnd);
}

void WaveformEditor::mouseMove (const juce::MouseEvent& e)
{
    const auto hovering = findMarkerNear ((float) e.position.x);
    setMouseCursor (hovering == Marker::none ? juce::MouseCursor::NormalCursor
                                              : juce::MouseCursor::LeftRightResizeCursor);
}

void WaveformEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().withTrimmedTop ((float) controlStripHeight);
    g.setColour (juce::Colours::black);
    g.fillRect (bounds);

    const auto& data = sound.data;
    const int numSamples = data.getNumSamples();
    const int numChannels = data.getNumChannels();

    if (numSamples > 0)
    {
        g.setColour (juce::Colours::orange);
        const int width = getWidth();

        // Stereo files get two stacked lanes (top = left, bottom = right) — the standard
        // DAW convention — instead of only ever drawing channel 0 and silently discarding
        // the second channel, which made stereo content indistinguishable from mono.
        const bool isStereo = numChannels >= 2;
        const auto laneHeight = isStereo ? bounds.getHeight() * 0.5f : bounds.getHeight();

        for (int channel = 0; channel < (isStereo ? 2 : 1); ++channel)
        {
            const auto* channelData = data.getReadPointer (channel);
            const auto laneTop = bounds.getY() + (float) channel * laneHeight;
            const auto laneMidY = laneTop + laneHeight * 0.5f;

            for (int x = 0; x < width; ++x)
            {
                const int startSample = (int) ((float) x / (float) width * (float) numSamples);
                const int endSample = juce::jmin (numSamples, (int) ((float) (x + 1) / (float) width * (float) numSamples) + 1);

                float minVal = 0.0f, maxVal = 0.0f;
                for (int s = startSample; s < endSample; ++s)
                {
                    minVal = juce::jmin (minVal, channelData[s]);
                    maxVal = juce::jmax (maxVal, channelData[s]);
                }

                const auto halfLane = laneHeight * 0.5f;
                g.drawLine ((float) x, laneMidY - maxVal * halfLane, (float) x, laneMidY - minVal * halfLane);
            }
        }

        if (isStereo)
        {
            // Thin divider between the two lanes so they read as "two channels," not one
            // waveform that happens to have a gap in it.
            g.setColour (juce::Colours::darkgrey);
            g.drawHorizontalLine ((int) (bounds.getY() + laneHeight), bounds.getX(), bounds.getRight());
        }
    }

    // Trim region shading — everything outside [trimStart, trimEnd) is dimmed.
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (bounds.withRight (sampleToX (sound.trimStart.load())));
    g.fillRect (bounds.withLeft (sampleToX (sound.trimEnd.load())));

    auto drawMarker = [&] (int sample, juce::Colour colour, const juce::String& label, bool labelAbove)
    {
        const auto x = sampleToX (sample);
        g.setColour (colour);
        g.drawLine (x, bounds.getY(), x, bounds.getBottom(), 2.0f);

        g.setFont (12.0f);
        const auto labelY = labelAbove ? bounds.getY() + 2.0f : bounds.getBottom() - 16.0f;
        const auto labelWidth = 70.0f;
        // Keep the label from running off either edge of the component.
        const auto labelX = juce::jlimit (0.0f, bounds.getWidth() - labelWidth, x - labelWidth * 0.5f);
        g.drawText (label, juce::Rectangle<float> (labelX, labelY, labelWidth, 14.0f), juce::Justification::centred, false);
    };

    drawMarker (sound.loopStart.load(), juce::Colours::limegreen, "Loop In", false);
    drawMarker (sound.loopEnd.load(), juce::Colours::limegreen, "Loop Out", false);
    drawMarker (sound.trimStart.load(), juce::Colours::yellow, "Trim In", true);
    drawMarker (sound.trimEnd.load(), juce::Colours::yellow, "Trim Out", true);

    const auto playbackPosition = sound.currentPlaybackPosition.load();
    if (playbackPosition >= 0)
    {
        g.setColour (juce::Colours::white);
        g.drawLine (sampleToX (playbackPosition), bounds.getY(), sampleToX (playbackPosition), bounds.getBottom(), 2.0f);
    }
}
