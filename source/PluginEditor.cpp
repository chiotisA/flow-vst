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
        processorRef.loadSample (entry.file, keyToRootMidiNote (entry.key), entry.bpm);
    };

    processorRef.onSampleLoaded = [this] { rebuildWaveformEditor(); };

    addAndMakeVisible (loopToggle);
    addAndMakeVisible (timeStretchToggle);

    // Standalone has no host DAW to report a tempo, so it gets a manual BPM field instead
    // — the real VST3 always uses the actual host tempo automatically (see
    // PluginProcessor::processBlock). juce::PluginHostType lets us tell them apart.
    if (juce::PluginHostType::getPluginLoadedAs() == juce::AudioProcessor::wrapperType_Standalone)
    {
        manualBpmBox.setText ("120", juce::dontSendNotification);
        manualBpmBox.setInputRestrictions (5, "0123456789.");
        manualBpmBox.onTextChange = [this]
        {
            const auto bpm = manualBpmBox.getText().getDoubleValue();
            if (bpm > 0.0)
                processorRef.setManualBpm (bpm);
        };
        addAndMakeVisible (manualBpmLabel);
        addAndMakeVisible (manualBpmBox);
        processorRef.setManualBpm (120.0);
    }

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

        // No native BPM (most One-Shots) means there's nothing to compute a stretch ratio
        // against — disable rather than let it silently do nothing when clicked.
        const bool hasNativeBpm = sound->nativeBpm > 0;
        timeStretchToggle.setEnabled (hasNativeBpm);
        timeStretchToggle.setToggleState (hasNativeBpm && sound->timeStretchEnabled.load(), juce::dontSendNotification);
        timeStretchToggle.onClick = [sound, this] { sound->timeStretchEnabled.store (timeStretchToggle.getToggleState()); };
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
    timeStretchToggle.setBounds (footer.removeFromLeft (110));

    if (manualBpmBox.isVisible())
    {
        manualBpmBox.setBounds (footer.removeFromRight (60));
        manualBpmLabel.setBounds (footer.removeFromRight (70));
    }

    catalogBrowser.setBounds (area.removeFromLeft (260));
    area.removeFromLeft (10);

    if (waveformEditor != nullptr)
        waveformEditor->setBounds (area);
}
