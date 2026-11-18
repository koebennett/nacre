#pragma once
#include <JuceHeader.h>
#include "Synth.h"
#include "Preset.h"
#include "PresetManager.h"

class NacreProcessor : public juce::AudioProcessor {
public:
    NacreProcessor();
    ~NacreProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Nacre"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ---- used by the editor
    juce::AudioProcessorValueTreeState apvts;
    juce::UndoManager undo;
    juce::MidiKeyboardState keyboardState;
    PresetManager presets;
    nacre::Engine& getEngine() { return engine; }

    void applyPreset(const nacre::Preset& p, bool undoable = true);   // message thread
    nacre::Preset captureCurrent() const;
    void applyFxChain(int index);
    juce::String currentPresetName { "Init" }, currentPresetCategory { "Init" };
    std::function<void()> onPresetChanged;                           // editor refresh hook (message thread)

    float getParam(int id) const { return raw[(size_t) id]->load(); }
    juce::RangedAudioParameter* param(int id) const { return params[(size_t) id]; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    template <typename Float> void render(juce::AudioBuffer<Float>&, juce::MidiBuffer&);
    void handleMidi(const juce::MidiMessage&);

    nacre::Engine engine;
    std::vector<std::atomic<float>*> raw;
    std::vector<juce::RangedAudioParameter*> params;
    juce::AudioBuffer<float> scratch;
    std::atomic<bool> presetJustLoaded { false };
    bool waitingForReset = false;
    int currentProgram = 0;

    JUCE_DECLARE_WEAK_REFERENCEABLE(NacreProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NacreProcessor)
};
