#pragma once

#include "PluginProcessor.h"
#include "BinaryData.h"
#include "melatonin_inspector/melatonin_inspector.h"
#include "Sampler/WaveformEditor.h"
#include "Sampler/CatalogBrowser.h"

//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void rebuildWaveformEditor();

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PluginProcessor& processorRef;
    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect the UI" };

    CatalogBrowser catalogBrowser;

    // Null until a sample is picked from the catalog.
    std::unique_ptr<WaveformEditor> waveformEditor;
    juce::ToggleButton loopToggle { "Loop" };

    // Disabled whenever the loaded sample has no native BPM (most One-Shots) — there's
    // nothing to compute a stretch ratio against. Standalone has no host to report a
    // tempo, so it gets a manual BPM field instead; a real DAW host always uses its own.
    juce::ToggleButton timeStretchToggle { "Time-Stretch" };
    juce::Label manualBpmLabel { {}, "Host BPM:" };
    juce::TextEditor manualBpmBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
