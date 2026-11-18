#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FxChains.h"
#include "Diag.h"

using namespace nacre;

juce::AudioProcessorValueTreeState::ParameterLayout NacreProcessor::createLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < kNumParams; ++i) {
        const ParamDef d = kParams[i];
        const juce::ParameterID pid { d.id, 1 };
        switch (d.type) {
            case kFloat: {
                juce::NormalisableRange<float> range(d.lo, d.hi,
                    [d](float, float, float n) { return fromNorm(d, n); },
                    [d](float, float, float v) { return toNorm(d, v); },
                    [d](float, float, float v) { return snapValue(d, v); });
                auto attrs = juce::AudioParameterFloatAttributes()
                    .withStringFromValueFunction([d](float v, int) { return juce::String(formatValue(d, v)); })
                    .withValueFromStringFunction([d](const juce::String& s) { return parseValue(d, s.toStdString()); });
                layout.add(std::make_unique<juce::AudioParameterFloat>(pid, d.name, range, d.def, attrs));
                break;
            }
            case kInt:
                layout.add(std::make_unique<juce::AudioParameterInt>(pid, d.name, (int) d.lo, (int) d.hi, (int) d.def,
                    juce::AudioParameterIntAttributes().withStringFromValueFunction([d](int v, int) { return juce::String(formatValue(d, (float) v)); })));
                break;
            case kBool:
                layout.add(std::make_unique<juce::AudioParameterBool>(pid, d.name, d.def > 0.5f));
                break;
            case kChoice: {
                juce::StringArray c;
                for (auto& s : splitChoices(d.choices)) c.add(s);
                layout.add(std::make_unique<juce::AudioParameterChoice>(pid, d.name, c, (int) d.def));
                break;
            }
        }
    }
    return layout;
}

NacreProcessor::NacreProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, &undo, "NACRE", createLayout()) {
    nacre_diag::log("processor: constructing");
    {
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        try { WavetableBank::get(); } catch (const std::exception& e) { nacre_diag::log(juce::String("wavetables FAILED: ") + e.what()); }
        nacre_diag::log("wavetables ready in " + juce::String(juce::Time::getMillisecondCounterHiRes() - t0, 0) + " ms");
    }
    for (int i = 0; i < kNumParams; ++i) {
        raw.push_back(apvts.getRawParameterValue(kParams[i].id));
        params.push_back(apvts.getParameter(kParams[i].id));
        jassert(raw.back() != nullptr && params.back() != nullptr);
    }
    // Start on the first factory sound rather than a bare sine.
    try {
        if (!presets.all().empty()) { applyPreset(presets.load(presets.all().front()), false); undo.clearUndoHistory(); }
    } catch (const std::exception& e) { nacre_diag::log(juce::String("initial preset FAILED: ") + e.what()); }
    nacre_diag::log("processor: ready (" + juce::String((int) presets.all().size()) + " presets)");
}

bool NacreProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void NacreProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    nacre_diag::log("prepareToPlay " + juce::String(sampleRate) + " Hz, block " + juce::String(samplesPerBlock));
    for (int i = 0; i < kNumParams; ++i) engine.params[i] = raw[(size_t) i]->load();
    engine.prepare(sampleRate, samplesPerBlock);
    scratch.setSize(2, juce::jmax(samplesPerBlock, 32));
    keyboardState.reset();
}

void NacreProcessor::handleMidi(const juce::MidiMessage& m) {
    if (m.isNoteOn()) engine.noteOn(m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff()) engine.noteOff(m.getNoteNumber());
    else if (m.isPitchWheel()) engine.pitchBend((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isController()) engine.controlChange(m.getControllerNumber(), (float) m.getControllerValue() / 127.0f);
    else if (m.isChannelPressure()) engine.channelPressure((float) m.getChannelPressureValue() / 127.0f);
    else if (m.isAftertouch()) engine.polyPressure(m.getNoteNumber(), (float) m.getAfterTouchValue() / 127.0f);
    else if (m.isAllNotesOff()) engine.allNotesOff();
    else if (m.isAllSoundOff()) engine.allSoundOff();
}

template <typename Float>
void NacreProcessor::render(juce::AudioBuffer<Float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n <= 0) return;
    keyboardState.processNextMidiBuffer(midi, 0, n, true);

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            engine.setTransport(pos->getIsPlaying(), pos->getBpm().orFallback(120.0), pos->getPpqPosition().orFallback(0.0));

    // Preset changes: let the engine fade out and clear its tails before the new values arrive.
    if (presetJustLoaded.exchange(false)) { engine.requestSoftReset(); waitingForReset = true; }
    if (!waitingForReset || !engine.softResetBusy()) {
        for (int i = 0; i < kNumParams; ++i) engine.setParam(i, raw[(size_t) i]->load());
        if (waitingForReset) { engine.snapSmoothing(); waitingForReset = false; }
    }

    if (scratch.getNumSamples() < n) scratch.setSize(2, n, false, false, true);   // only if the host lied about block size
    float* L = scratch.getWritePointer(0);
    float* R = scratch.getWritePointer(1);
    int pos = 0;
    for (const auto meta : midi) {
        const int t = juce::jlimit(0, n, meta.samplePosition);
        if (t > pos) { engine.process(L + pos, R + pos, t - pos); pos = t; }
        handleMidi(meta.getMessage());
    }
    if (pos < n) engine.process(L + pos, R + pos, n - pos);

    const int chans = buffer.getNumChannels();
    for (int c = 0; c < chans; ++c) {
        Float* out = buffer.getWritePointer(c);
        if (chans == 1) for (int i = 0; i < n; ++i) out[i] = (Float) (0.5f * (L[i] + R[i]));
        else { const float* src = c == 0 ? L : (c == 1 ? R : nullptr); for (int i = 0; i < n; ++i) out[i] = src ? (Float) src[i] : (Float) 0; }
    }
}

void NacreProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer& m) { render(b, m); }
void NacreProcessor::processBlock(juce::AudioBuffer<double>& b, juce::MidiBuffer& m) { render(b, m); }

juce::AudioProcessorEditor* NacreProcessor::createEditor() { return new NacreEditor(*this); }

// ------------------------------------------------------------------ presets
Preset NacreProcessor::captureCurrent() const {
    Preset p;
    for (int i = 0; i < kNumParams; ++i) p.values[i] = raw[(size_t) i]->load();
    p.name = currentPresetName.toStdString();
    p.category = currentPresetCategory.toStdString();
    return p;
}

void NacreProcessor::applyPreset(const Preset& p, bool undoable) {
    // Parameter values are atomics and safe to set from any thread (hosts may restore state off the
    // message thread); only the undo transaction and the editor callback need the message thread.
    const bool onMessageThread = juce::MessageManager::existsAndIsCurrentThread();
    if (undoable && onMessageThread) undo.beginNewTransaction("Load " + juce::String(p.name));
    for (int i = 0; i < kNumParams; ++i) {
        auto* prm = params[(size_t) i];
        const float norm = prm->convertTo0to1(p.values[i]);
        if (std::abs(prm->getValue() - norm) > 1e-7f) prm->setValueNotifyingHost(norm);
    }
    presetJustLoaded = true;
    juce::WeakReference<NacreProcessor> safe(this);
    auto finish = [safe, name = juce::String(p.name), cat = juce::String(p.category)] {
        if (auto* self = safe.get()) {
            self->currentPresetName = name; self->currentPresetCategory = cat;
            if (self->onPresetChanged) self->onPresetChanged();
        }
    };
    if (onMessageThread) finish(); else juce::MessageManager::callAsync(finish);
}

void NacreProcessor::applyFxChain(int index) {
    const auto& chains = fxChains();
    if (index < 0 || index >= (int) chains.size()) return;
    undo.beginNewTransaction("Effects: " + juce::String(chains[(size_t) index].name));
    Preset parsed = Preset::parse(chains[(size_t) index].text);
    for (int i = 0; i < kNumParams; ++i) {
        if (!isFxParam(kParams[i].id)) continue;
        auto* prm = params[(size_t) i];
        prm->setValueNotifyingHost(prm->convertTo0to1(parsed.values[i]));
    }
}

int NacreProcessor::getNumPrograms() { int n = 0; factoryPresets(n); return juce::jmax(1, n); }
const juce::String NacreProcessor::getProgramName(int index) {
    int n = 0; const FactoryPreset* f = factoryPresets(n);
    return index >= 0 && index < n ? juce::String(f[index].category) + " - " + f[index].name : juce::String("Init");
}
void NacreProcessor::setCurrentProgram(int index) {
    int n = 0; const FactoryPreset* f = factoryPresets(n);
    if (index < 0 || index >= n) return;
    currentProgram = index;
    applyPreset(Preset::parse(f[index].data), false);
}

// ------------------------------------------------------------------ state
void NacreProcessor::getStateInformation(juce::MemoryBlock& dest) {
    juce::XmlElement xml("NacreState");
    xml.setAttribute("version", 1);
    xml.setAttribute("presetName", currentPresetName);
    xml.setAttribute("presetCategory", currentPresetCategory);
    xml.setAttribute("program", currentProgram);
    xml.addTextElement(juce::String(captureCurrent().serialize()));   // human-readable, forward compatible
    copyXmlToBinary(xml, dest);
}

void NacreProcessor::setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data, size);
    if (!xml || !xml->hasTagName("NacreState")) return;
    Preset p = Preset::parse(xml->getAllSubText().toStdString());
    p.name = xml->getStringAttribute("presetName", "Init").toStdString();
    p.category = xml->getStringAttribute("presetCategory", "Init").toStdString();
    currentProgram = juce::jlimit(0, getNumPrograms() - 1, xml->getIntAttribute("program", 0));   // restore the index only
    applyPreset(p, false);
    updateHostDisplay(ChangeDetails().withProgramChanged(true));
    if (juce::MessageManager::existsAndIsCurrentThread()) undo.clearUndoHistory();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NacreProcessor(); }
