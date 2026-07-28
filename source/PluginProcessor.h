#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "Sampler/FlowSamplerVoice.h"

#if (MSVC)
#include "ipps.h"
#endif

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Dev-only accessor for the waveform editor — the real browser/catalog data model
    // (task 5+) replaces this "there's exactly one active sound at a time" assumption
    // with proper pack/sample selection and multi-sample state.
    class FlowSamplerSound* getActiveSound() const;

    void loadSample (const juce::File& file, int rootMidiNote);

    // Fires after loadSample() swaps in a new FlowSamplerSound, so the editor can rebuild
    // its WaveformEditor to point at it.
    std::function<void()> onSampleLoaded;

private:
    juce::AudioFormatManager formatManager;
    juce::Synthesiser synth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
