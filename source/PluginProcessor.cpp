#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Sampler/FlowSamplerSound.h"

//==============================================================================
PluginProcessor::PluginProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
{
    formatManager.registerBasicFormats();

    for (int i = 0; i < 8; ++i)
    {
        auto* voice = new FlowSamplerVoice();
        voice->setHostBpmSource (&hostBpm);
        synth.addVoice (voice);
    }

    // Nothing loaded at startup — the catalog browser picks a sample. On a fresh clone
    // or CI, test_samples/ (gitignored, real Beastsamples content) won't exist either way.
}

void PluginProcessor::loadSample (const juce::File& file, int rootMidiNote, int nativeBpm)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));

    if (reader == nullptr)
        return;

    juce::AudioBuffer<float> buffer ((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read (&buffer, 0, buffer.getNumSamples(), 0, true, true);

    synth.clearSounds();
    synth.addSound (new FlowSamplerSound (std::move (buffer), reader->sampleRate, rootMidiNote, nativeBpm));

    if (onSampleLoaded)
        onSampleLoaded();
}

FlowSamplerSound* PluginProcessor::getActiveSound() const
{
    if (synth.getNumSounds() == 0)
        return nullptr;

    return dynamic_cast<FlowSamplerSound*> (synth.getSound (0).get());
}

PluginProcessor::~PluginProcessor()
{
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);

    // Pre-allocates the time-stretch engine's internal STFT buffers once here (message
    // thread, safe to allocate) rather than on first use inside renderNextBlock (audio
    // thread, must never allocate).
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<FlowSamplerVoice*> (synth.getVoice (i)))
            voice->prepare (sampleRate, samplesPerBlock);
}

void PluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Real host tempo when hosted in a DAW; Standalone has no playhead to report one, so
    // it falls back to manualBpm (set from the UI's Standalone-only BPM field).
    double bpm = manualBpm.load();
    if (auto* currentPlayHead = getPlayHead())
        if (auto position = currentPlayHead->getPosition())
            if (auto reportedBpm = position->getBpm())
                bpm = *reportedBpm;
    hostBpm.store (bpm);

    // Synthesiser voices add into the buffer, so it must start silent.
    buffer.clear();

    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused (destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
