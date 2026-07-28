#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);

    addAndMakeVisible (inspectButton);

    // this chunk of code instantiates and opens the melatonin inspector
    inspectButton.onClick = [&] {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible (true);
    };

    addAndMakeVisible (catalogBrowser);
    catalogBrowser.onSampleSelected = [this] (const SampleEntry& entry)
    {
        processorRef.loadSample (entry.file, keyToRootMidiNote (entry.key));
    };

    processorRef.onSampleLoaded = [this] { rebuildWaveformEditor(); };

    addAndMakeVisible (loopToggle);

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (900, 450);
}

void PluginEditor::rebuildWaveformEditor()
{
    waveformEditor.reset();

    if (auto* sound = processorRef.getActiveSound())
    {
        waveformEditor = std::make_unique<WaveformEditor> (*sound);
        addAndMakeVisible (*waveformEditor);

        loopToggle.setToggleState (sound->loopingEnabled.load(), juce::dontSendNotification);
        loopToggle.onClick = [sound, this] { sound->loopingEnabled.store (loopToggle.getToggleState()); };
    }

    resized();
    repaint();
}

PluginEditor::~PluginEditor()
{
    // Editor can be destroyed and recreated by the host independently of the processor —
    // drop the callback so a stale one is never invoked against a freed PluginEditor.
    processorRef.onSampleLoaded = nullptr;
}

void PluginEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    if (waveformEditor == nullptr)
    {
        g.setColour (juce::Colours::white);
        g.setFont (16.0f);
        g.drawText ("No sample loaded (test_samples/test_sample.wav not found)",
                    getLocalBounds(), juce::Justification::centred, false);
    }
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto footer = area.removeFromBottom (40);
    inspectButton.setBounds (footer.removeFromRight (120));
    loopToggle.setBounds (footer.removeFromLeft (80));

    catalogBrowser.setBounds (area.removeFromLeft (260));
    area.removeFromLeft (10);

    if (waveformEditor != nullptr)
        waveformEditor->setBounds (area);
}
