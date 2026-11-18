// Nacre — the four pages of the editor: Synth, Modulation, Effects, Arp & Voice.
#pragma once
#include "Widgets.h"
#include "FxChains.h"

namespace ui {
using namespace nacre;

// ================================================================== SYNTH PAGE
class OscCard : public Card {
public:
    OscCard(NacreProcessor& p, bool isA)
        : Card(isA ? "Oscillator A" : "Oscillator B"), a(isA),
          on(p, a ? oscAOn : oscBOn), table(p, a ? oscATable : oscBTable), warpMode(p, a ? oscAWarpMode : oscBWarpMode), view(p, a),
          pos(p, a ? oscAPos : oscBPos, "Position"), warp(p, a ? oscAWarp : oscBWarp, "Warp"), level(p, a ? oscALevel : oscBLevel, "Level"),
          pan(p, a ? oscAPan : oscBPan, "Pan"), phase(p, a ? oscAPhase : oscBPhase, "Phase Rnd"),
          oct(p, a ? oscAOct : oscBOct, "Octave"), semi(p, a ? oscASemi : oscBSemi, "Semi"), fine(p, a ? oscAFine : oscBFine, "Fine"),
          uni(p, a ? oscAUnison : oscBUnison, "Unison"), det(p, a ? oscADetune : oscBDetune, "Detune"), spread(p, a ? oscASpread : oscBSpread, "Spread") {
        setPower(&on);
        for (juce::Component* c : std::initializer_list<juce::Component*>{ &table, &view, &warpMode, &pos, &warp, &level, &pan, &phase, &oct, &semi, &fine, &uni, &det, &spread }) addAndMakeVisible(c);
        table.setTooltip("Wavetable. 43 procedurally generated, band-limited tables.");
        warpMode.setTooltip("Warp mode: reshapes the phase before the table is read");
    }
    void layout() override {
        table.setBounds(getWidth() - 14 - 176, 9, 176, 24);
        auto c = content();
        auto v = c.removeFromTop(84); view.setBounds(v);
        warpMode.setBounds(v.getRight() - 96, v.getY() + 6, 90, 22);
        c.removeFromTop(4);
        row(c.removeFromTop(58), { &pos, &warp, &level, &pan, &phase });
        row(c.removeFromTop(58), { &oct, &semi, &fine, &uni, &det, &spread });
    }
private:
    bool a;
    Switch on; Choice table, warpMode; WavetableView view;
    Knob pos, warp, level, pan, phase, oct, semi, fine, uni, det, spread;
};

class MixCard : public Card {
public:
    explicit MixCard(NacreProcessor& p)
        : Card("Sub, Noise & Feel"), subOnS(p, subOn, "Sub"), shape(p, subShape), octv(p, subOct),
          subLvl(p, subLevel, "Sub"), noiseLvl(p, noiseLevel, "Noise"), color(p, noiseColor, "Color"), fm(p, fmAmount, "FM B>A"), drft(p, drift, "Drift"),
          gl(p, glide, "Glide"), vel(p, velSens, "Velocity"), bend(p, bendRange, "Bend"), voices(p, polyphony, "Voices") {
        for (juce::Component* c : std::initializer_list<juce::Component*>{ &subOnS, &shape, &octv, &subLvl, &noiseLvl, &color, &fm, &drft, &gl, &vel, &bend, &voices }) addAndMakeVisible(c);
        fm.slider.setTooltip("Phase-modulates oscillator A with oscillator B. Turn B's level down to hear pure FM.");
        drft.slider.setTooltip("Analog drift: slow, per-voice pitch and filter wander");
    }
    void layout() override {
        auto c = content();
        auto r1 = c.removeFromTop(24);
        subOnS.setBounds(r1.removeFromLeft(78));
        shape.setBounds(r1.removeFromLeft(110)); r1.removeFromLeft(8); octv.setBounds(r1.removeFromLeft(70));
        c.removeFromTop(10);
        row(c.removeFromTop(58), { &subLvl, &noiseLvl, &color, &fm, &drft });
        c.removeFromTop(12);
        perfY = c.getY();
        c.removeFromTop(18);
        row(c.removeFromTop(58), { &voices, &gl, &vel, &bend });
    }
    void paint(juce::Graphics& g) override {
        Card::paint(g);
        auto c = content();
        g.setColour(theme().cardEdge); g.fillRect(c.getX(), perfY - 6, c.getWidth(), 1);
        caption(g, juce::Rectangle<int>(c.getX(), perfY, c.getWidth(), 16), "Performance");
    }
private:
    Switch subOnS; Choice shape, octv; Knob subLvl, noiseLvl, color, fm, drft, gl, vel, bend, voices; int perfY = 0;
};

class FilterCard : public Card {
public:
    explicit FilterCard(NacreProcessor& p)
        : Card("Filter"), on1(p, f1On), type1(p, f1Type), view(p),
          cut(p, f1Cut, "Cutoff"), res(p, f1Res, "Resonance"), drv(p, f1Drive, "Drive"), key(p, f1Key, "Key Track"), env(p, f1Env, "Env Amt"),
          on2(p, f2On, "Filter 2"), type2(p, f2Type), cut2(p, f2Cut, "Cutoff"), res2(p, f2Res, "Resonance"), env2(p, f2Env, "Env Amt"), key2(p, f2Key, "Key Track"),
          routing(p, fRouting) {
        setPower(&on1);
        for (juce::Component* c : std::initializer_list<juce::Component*>{ &type1, &view, &cut, &res, &drv, &key, &env, &on2, &type2, &cut2, &res2, &env2, &key2, &routing }) addAndMakeVisible(c);
        env.slider.setTooltip("How far the Filter Envelope moves the cutoff (up to 8 octaves either way)");
    }
    void layout() override {
        type1.setBounds(110, 9, 120, 24);
        auto c = content();
        auto right = c.removeFromRight(206); c.removeFromRight(14);
        view.setBounds(c.removeFromTop(108)); c.removeFromTop(8);
        row(c.removeFromTop(58), { &cut, &res, &drv, &key, &env });
        auto t = right.removeFromTop(24);
        on2.setBounds(t.removeFromLeft(100)); type2.setBounds(t);
        right.removeFromTop(8);
        row(right.removeFromTop(58), { &cut2, &res2 });
        row(right.removeFromTop(58), { &env2, &key2 });
        right.removeFromTop(8);
        routing.setBounds(right.removeFromTop(26));
    }
private:
    Switch on1; Choice type1; FilterView view; Knob cut, res, drv, key, env;
    Switch on2; Choice type2; Knob cut2, res2, env2, key2; ParamSegmented routing;
};

class EnvCard : public Card {
public:
    explicit EnvCard(NacreProcessor& p) : Card("Envelopes") {
        const char* names[3] = { "Amp", "Filter", "Mod" };
        for (int e = 0; e < 3; ++e) {
            const int b = env1A + e * 4;
            views[e] = std::make_unique<EnvView>(p, b);
            addAndMakeVisible(*views[e]);
            const char* lbl[4] = { "Attack", "Decay", "Sustain", "Release" };
            for (int k = 0; k < 4; ++k) { knobs[e][k] = std::make_unique<Knob>(p, b + k, lbl[k]); addAndMakeVisible(*knobs[e][k]); }
            titles[e] = names[e];
        }
    }
    void layout() override {
        auto c = content();
        const int w = (c.getWidth() - 24) / 3;
        for (int e = 0; e < 3; ++e) {
            auto col = juce::Rectangle<int>(c.getX() + e * (w + 12), c.getY(), w, c.getHeight());
            col.removeFromTop(20);
            views[e]->setBounds(col.removeFromTop(96)); col.removeFromTop(8);
            row(col.removeFromTop(58), { knobs[e][0].get(), knobs[e][1].get(), knobs[e][2].get(), knobs[e][3].get() });
        }
    }
    void paint(juce::Graphics& g) override {
        Card::paint(g);
        auto c = content(); const int w = (c.getWidth() - 24) / 3;
        for (int e = 0; e < 3; ++e) caption(g, juce::Rectangle<int>(c.getX() + e * (w + 12), c.getY(), w, 16), titles[e]);
    }
private:
    std::unique_ptr<EnvView> views[3]; std::unique_ptr<Knob> knobs[3][4]; juce::String titles[3];
};

class SynthPage : public juce::Component {
public:
    explicit SynthPage(NacreProcessor& p) : oa(p, true), ob(p, false), mix(p), filter(p), env(p) {
        for (juce::Component* c : std::initializer_list<juce::Component*>{ &oa, &ob, &mix, &filter, &env }) addAndMakeVisible(c);
    }
    void resized() override {
        auto r = getLocalBounds();
        auto top = r.removeFromTop(262); r.removeFromTop(12);
        oa.setBounds(top.removeFromLeft(380)); top.removeFromLeft(12);
        ob.setBounds(top.removeFromLeft(380)); top.removeFromLeft(12); mix.setBounds(top);
        filter.setBounds(r.removeFromLeft(560)); r.removeFromLeft(12); env.setBounds(r);
    }
private:
    OscCard oa, ob; MixCard mix; FilterCard filter; EnvCard env;
};

// ================================================================== MODULATION PAGE
class LfoCard : public Card {
public:
    LfoCard(NacreProcessor& p, int i)
        : Card("LFO " + juce::String(i + 1)), shape(p, lfo1Shape + i * 6), view(p, i), rate(p, lfo1Rate + i * 6, "Rate"), fade(p, lfo1Fade + i * 6, "Fade In"),
          sync(p, lfo1Sync + i * 6, "Sync"), div(p, lfo1Div + i * 6), retrig(p, lfo1Retrig + i * 6, "Per voice") {
        for (juce::Component* c : std::initializer_list<juce::Component*>{ &shape, &view, &rate, &fade, &sync, &div, &retrig }) addAndMakeVisible(c);
        retrig.setTooltip("On: each note starts its own LFO from zero. Off: one free-running LFO shared by all notes.");
        sync.setTooltip("Lock the rate to the host tempo");
        if (i == 1) title += u8("  \xc2\xb7  Motion macro");
        if (i == 2) title += u8("  \xc2\xb7  Vibrato (mod wheel)");
    }
    void layout() override {
        shape.setBounds(getWidth() - 14 - 130, 9, 130, 24);
        auto c = content();
        view.setBounds(c.removeFromLeft(148)); c.removeFromLeft(10);
        auto k = c.removeFromTop(58);
        row(k.removeFromLeft(124), { &rate, &fade });
        k.removeFromLeft(8);
        sync.setBounds(k.removeFromTop(24)); k.removeFromTop(6); div.setBounds(k.removeFromTop(24));
        c.removeFromTop(10);
        retrig.setBounds(c.removeFromTop(24).withWidth(140));
    }
private:
    Choice shape; LfoView view; Knob rate, fade; Switch sync; Choice div; Switch retrig;
};

class MatrixRow : public juce::Component, private juce::Slider::Listener {
public:
    MatrixRow(NacreProcessor& p, int slot)
        : n(slot), src(p, mod1Src + slot * 4), via(p, mod1Via + slot * 4), dst(p, mod1Dst + slot * 4) {
        const auto& d = kParams[mod1Amt + slot * 4];
        amt.setSliderStyle(juce::Slider::LinearHorizontal);
        amt.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        amt.setVelocityModeParameters(0.35, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
        attach = std::make_unique<APVTS::SliderAttachment>(p.apvts, d.id, amt);
        amt.setDoubleClickReturnValue(true, 0.0);
        amt.setTooltip("Amount. Double-click for zero, Shift for fine control.");
        amt.addListener(this);
        via.setTooltip("Optional: scales this route by a second source (e.g. LFO x Mod Wheel)");
        for (juce::Component* c : std::initializer_list<juce::Component*>{ &src, &via, &dst, &amt }) addAndMakeVisible(c);
    }
    ~MatrixRow() override { amt.removeListener(this); }
    void resized() override {
        auto r = getLocalBounds().reduced(0, 4);
        r.removeFromLeft(26);
        src.setBounds(r.removeFromLeft(136)); r.removeFromLeft(24);
        via.setBounds(r.removeFromLeft(118)); r.removeFromLeft(24);
        dst.setBounds(r.removeFromLeft(156)); r.removeFromLeft(16);
        r.removeFromRight(56);
        amt.setBounds(r);
    }
    void paint(juce::Graphics& g) override {
        const auto& t = theme();
        auto r = getLocalBounds();
        const bool active = src.getSelectedItemIndex() > 0 && dst.getSelectedItemIndex() > 0;
        g.setColour(active ? t.accent : t.text3); g.setFont(font(11.5f, W::Semibold));
        g.drawText(juce::String(n + 1), r.removeFromLeft(22), juce::Justification::centred);
        g.setColour(t.text3); g.setFont(font(13.0f, W::Medium));
        g.drawText(juce::String::fromUTF8("\xc3\x97"), juce::Rectangle<int>(src.getRight(), 0, 24, getHeight()), juce::Justification::centred);
        g.drawText(juce::String::fromUTF8("\xe2\x86\x92"), juce::Rectangle<int>(via.getRight(), 0, 24, getHeight()), juce::Justification::centred);
        g.setColour(active ? t.text : t.text3); g.setFont(font(11.5f, W::Semibold));
        g.drawText(juce::String(juce::roundToInt(amt.getValue() * 100)) + "%", juce::Rectangle<int>(getWidth() - 52, 0, 52, getHeight()), juce::Justification::centredRight);
    }
private:
    void sliderValueChanged(juce::Slider*) override { repaint(); }
    int n; Choice src, via, dst; juce::Slider amt; std::unique_ptr<APVTS::SliderAttachment> attach;
};

class MatrixCard : public Card {
public:
    explicit MatrixCard(NacreProcessor& p) : Card("Modulation Matrix") {
        for (int i = 0; i < kModSlots; ++i) { rows[i] = std::make_unique<MatrixRow>(p, i); addAndMakeVisible(*rows[i]); }
    }
    void layout() override {
        auto c = content(); c.removeFromTop(20);
        const int h = c.getHeight() / kModSlots;
        for (int i = 0; i < kModSlots; ++i) rows[i]->setBounds(c.removeFromTop(h));
    }
    void paint(juce::Graphics& g) override {
        Card::paint(g);
        auto c = content().removeFromTop(18);
        c.removeFromLeft(26);
        caption(g, c.removeFromLeft(160), "Source"); caption(g, c.removeFromLeft(142), "Via (optional)");
        caption(g, c.removeFromLeft(172), "Destination"); caption(g, c, "Amount");
        g.setColour(theme().text3); g.setFont(font(11.0f));
        g.drawText("Macros 1-4 = Tone, Motion, Space, Grit", getLocalBounds().reduced(14, 0).withHeight(40), juce::Justification::centredRight);
    }
private:
    std::unique_ptr<MatrixRow> rows[kModSlots];
};

class ModPage : public juce::Component {
public:
    explicit ModPage(NacreProcessor& p) : matrix(p) {
        for (int i = 0; i < 3; ++i) { lfos[i] = std::make_unique<LfoCard>(p, i); addAndMakeVisible(*lfos[i]); }
        addAndMakeVisible(matrix);
    }
    void resized() override {
        auto r = getLocalBounds();
        auto left = r.removeFromLeft(420); r.removeFromLeft(12);
        const int h = (left.getHeight() - 24) / 3;
        for (int i = 0; i < 3; ++i) { lfos[i]->setBounds(left.removeFromTop(h)); left.removeFromTop(12); }
        matrix.setBounds(r);
    }
private:
    std::unique_ptr<LfoCard> lfos[3]; MatrixCard matrix;
};

// ================================================================== EFFECTS PAGE
class FxCard : public Card {
public:
    FxCard(NacreProcessor& p, const juce::String& t, int powerId, std::vector<std::pair<int, const char*>> knobsSpec, int choiceId = -1)
        : Card(t), on(p, powerId) {
        setPower(&on);
        if (choiceId >= 0) { choice = std::make_unique<Choice>(p, choiceId); addAndMakeVisible(*choice); }
        for (auto& [id, name] : knobsSpec) { knobs.push_back(std::make_unique<Knob>(p, id, name)); addAndMakeVisible(*knobs.back()); }
    }
    void layout() override {
        if (choice) choice->setBounds(getWidth() - 14 - 116, 9, 116, 24);
        auto c = content(); c.removeFromTop(6);
        const int perRow = knobs.size() <= 4 ? (int) knobs.size() : 3;
        size_t i = 0;
        while (i < knobs.size()) {
            auto r = c.removeFromTop(64); c.removeFromTop(8);
            const int w = c.getWidth() / juce::jmax(1, perRow);
            for (int k = 0; k < perRow && i < knobs.size(); ++k, ++i) knobs[i]->setBounds(r.getX() + k * w, r.getY(), w, 62);
        }
        extra(c);
    }
    virtual void extra(juce::Rectangle<int>) {}
protected:
    Switch on; std::unique_ptr<Choice> choice; std::vector<std::unique_ptr<Knob>> knobs;
};

class DelayCard : public FxCard {
public:
    explicit DelayCard(NacreProcessor& p)
        : FxCard(p, "Delay", delayOn, { { delayFb, "Feedback" }, { delayTone, "Tone" }, { delayMix, "Mix" } }, delayDiv),
          sync(p, delaySync, "Sync"), ping(p, delayPing, "Ping-pong"), time(p, delayTime, "Time") {
        for (juce::Component* c : std::initializer_list<juce::Component*>{ &sync, &ping, &time }) addAndMakeVisible(c);
    }
    void extra(juce::Rectangle<int> c) override {
        auto r = c.removeFromTop(62);
        time.setBounds(r.removeFromLeft(r.getWidth() / 3));
        r.removeFromLeft(8);
        sync.setBounds(r.removeFromTop(24)); r.removeFromTop(8); ping.setBounds(r.removeFromTop(24));
    }
private:
    Switch sync, ping; Knob time;
};

class CompCard : public FxCard, private juce::Timer {
public:
    explicit CompCard(NacreProcessor& p)
        : FxCard(p, "Compressor", compOn, { { compThresh, "Threshold" }, { compRatio, "Ratio" }, { compMakeup, "Makeup" }, { compAttack, "Attack" }, { compRelease, "Release" } }), proc(p) { startTimerHz(20); }
    void paint(juce::Graphics& g) override {
        FxCard::paint(g);
        const float gr = proc.getEngine().uiCompGr.load();
        auto r = juce::Rectangle<int>(getWidth() - 14 - 80, 13, 80, 16);
        g.setColour(theme().text2); g.setFont(font(11.0f, W::Medium));
        g.drawText("GR " + juce::String(-gr, 1) + " dB", r, juce::Justification::centredRight);
    }
private:
    void timerCallback() override { repaint(getWidth() - 100, 0, 100, 40); }
    NacreProcessor& proc;
};

class FlowCard : public Card {
public:
    FlowCard() : Card("Signal Flow") {}
    void paint(juce::Graphics& g) override {
        Card::paint(g);
        const auto& t = theme();
        const char* steps[] = { "Oscillators + Sub + Noise", "Filter 1 / Filter 2", "Amp Envelope", "Distortion (oversampled)", "Phaser", "Chorus", "Delay", "Reverb", "EQ", "Compressor", "Volume + Limiter" };
        auto c = content(); c.removeFromTop(4);
        const int h = c.getHeight() / 11;
        for (int i = 0; i < 11; ++i) {
            auto r = c.removeFromTop(h);
            g.setColour(i < 3 ? t.text2 : t.accent); g.fillEllipse((float) r.getX() + 2, (float) r.getCentreY() - 3, 6, 6);
            if (i < 10) { g.setColour(t.text3.withAlpha(0.5f)); g.fillRect(r.getX() + 4, r.getCentreY() + 4, 1, h - 7); }
            g.setColour(t.text); g.setFont(font(11.5f, i < 3 ? W::Regular : W::Medium));
            g.drawText(steps[i], r.withTrimmedLeft(16), juce::Justification::centredLeft);
        }
    }
};

class FxPage : public juce::Component {
public:
    explicit FxPage(NacreProcessor& p)
        : proc(p),
          dist(p, "Distortion", distOn, { { distDrive, "Drive" }, { distTone, "Tone" }, { distMix, "Mix" } }, distType),
          phaser(p, "Phaser", phaserOn, { { phaserRate, "Rate" }, { phaserDepth, "Depth" }, { phaserFb, "Feedback" }, { phaserMix, "Mix" } }),
          chorus(p, "Chorus", chorusOn, { { chorusRate, "Rate" }, { chorusDepth, "Depth" }, { chorusMix, "Mix" } }),
          delay(p),
          reverb(p, "Reverb", reverbOn, { { reverbSize, "Size" }, { reverbDecay, "Decay" }, { reverbDamp, "Damping" }, { reverbPre, "Pre-Delay" }, { reverbMix, "Mix" } }),
          eq(p, "EQ", eqOn, { { eqLow, "Low" }, { eqMid, "Mid" }, { eqMidFreq, "Mid Freq" }, { eqHigh, "High" } }),
          comp(p) {
        for (juce::Component* c : std::initializer_list<juce::Component*>{ &dist, &phaser, &chorus, &delay, &reverb, &eq, &comp, &flow, &chains, &prev, &next }) addAndMakeVisible(c);
        int id = 1; juce::String group;
        for (auto& fc : fxChains()) {
            if (group != fc.group) { if (id > 1) chains.addSeparator(); chains.addSectionHeading(fc.group); group = fc.group; }
            chains.addItem(fc.name, id++);
        }
        chains.setTextWhenNothingSelected("Choose an effect preset...");
        chains.onChange = [this] { if (chains.getSelectedId() > 0) proc.applyFxChain(chains.getSelectedId() - 1); };
        prev.onClick = [this] { step(-1); }; next.onClick = [this] { step(1); };
        prev.setTooltip("Previous effect preset"); next.setTooltip("Next effect preset");
    }
    void step(int d) {
        const int n = (int) fxChains().size();
        int i = chains.getSelectedId() - 1; i = i < 0 ? 0 : (i + d + n) % n;
        chains.setSelectedId(i + 1);
    }
    void resized() override {
        auto r = getLocalBounds();
        auto bar = r.removeFromTop(34); r.removeFromTop(10);
        bar.removeFromLeft(130);
        prev.setBounds(bar.removeFromLeft(30)); bar.removeFromLeft(6);
        chains.setBounds(bar.removeFromLeft(280)); bar.removeFromLeft(6);
        next.setBounds(bar.removeFromLeft(30));
        const int w = (r.getWidth() - 36) / 4, h = (r.getHeight() - 12) / 2;
        juce::Component* order[8] = { &dist, &phaser, &chorus, &delay, &reverb, &eq, &comp, &flow };
        for (int i = 0; i < 8; ++i) order[i]->setBounds(r.getX() + (i % 4) * (w + 12), r.getY() + (i / 4) * (h + 12), w, h);
    }
    void paint(juce::Graphics& g) override {
        g.setColour(theme().text); g.setFont(font(13.5f, W::Semibold));
        g.drawText("Effect presets", juce::Rectangle<int>(0, 0, 130, 34), juce::Justification::centredLeft);
        g.setColour(theme().text2); g.setFont(font(12.0f));
        g.drawText("Swaps only the effects, so you can put any sound in a new space.", juce::Rectangle<int>(next.getRight() + 14, 0, 520, 34), juce::Justification::centredLeft);
    }
private:
    NacreProcessor& proc;
    FxCard dist, phaser, chorus; DelayCard delay; FxCard reverb, eq; CompCard comp; FlowCard flow;
    juce::ComboBox chains; juce::TextButton prev { juce::String::fromUTF8("\xe2\x80\xb9") }, next { juce::String::fromUTF8("\xe2\x80\xba") };
};

// ================================================================== ARP & VOICE PAGE
class ArpCard : public Card {
public:
    explicit ArpCard(NacreProcessor& p)
        : Card("Arpeggiator"), on(p, arpOn), mode(p, arpMode), rate(p, arpDiv), latch(p, arpLatch, "Latch"),
          oct(p, arpOct, "Octaves"), gate(p, arpGate, "Gate"), swing(p, arpSwing, "Swing") {
        setPower(&on);
        for (juce::Component* c : std::initializer_list<juce::Component*>{ &mode, &rate, &latch, &oct, &gate, &swing }) addAndMakeVisible(c);
        latch.setTooltip("Keep playing after you let go; the next chord replaces the pattern");
    }
    void layout() override {
        auto c = content(); c.removeFromTop(8);
        mode.setBounds(c.removeFromTop(30)); c.removeFromTop(16);
        auto r = c.removeFromTop(26);
        caption(rateCap = r.removeFromLeft(40));
        rate.setBounds(r.removeFromLeft(130)); r.removeFromLeft(20); latch.setBounds(r.removeFromLeft(110));
        c.removeFromTop(16);
        row(c.removeFromTop(62).withWidth(300), { &oct, &gate, &swing });
    }
    void paint(juce::Graphics& g) override { Card::paint(g); ui::caption(g, rateCap, "Rate"); }
private:
    void caption(juce::Rectangle<int> r) { rateCap = r; }
    Switch on; ParamSegmented mode; Choice rate; Switch latch; Knob oct, gate, swing; juce::Rectangle<int> rateCap;
};

class VoiceCard : public Card {
public:
    explicit VoiceCard(NacreProcessor& p)
        : Card("Voice & Output"), mode(p, voiceMode), quality(p, PID::quality), lim(p, PID::limiter, "Safety limiter"),
          voices(p, polyphony, "Voices"), gl(p, glide, "Glide"), bend(p, bendRange, "Bend"), vel(p, velSens, "Velocity"), dr(p, drift, "Drift"),
          tune(p, masterTune, "Tune"), vol(p, masterGain, "Volume") {
        for (juce::Component* c : std::initializer_list<juce::Component*>{ &mode, &quality, &lim, &voices, &gl, &bend, &vel, &dr, &tune, &vol }) addAndMakeVisible(c);
        quality.setTooltip("High runs the oscillators, filters and distortion at 2x sample rate (cleaner highs). Eco halves the CPU.");
        lim.setTooltip("Transparent zero-latency limiter that keeps the output below -0.3 dBFS");
    }
    void layout() override {
        auto c = content(); c.removeFromTop(8);
        auto r = c.removeFromTop(30);
        mode.setBounds(r.removeFromLeft(r.getWidth() / 2 - 8)); r.removeFromLeft(16);
        quality.setBounds(r);
        c.removeFromTop(14);
        row(c.removeFromTop(62), { &voices, &gl, &bend, &vel, &dr, &tune, &vol });
        c.removeFromTop(12);
        lim.setBounds(c.removeFromTop(24).withWidth(170));
    }
private:
    ParamSegmented mode, quality; Switch lim; Knob voices, gl, bend, vel, dr, tune, vol;
};

class MacroInfoCard : public Card, private juce::Timer {
public:
    explicit MacroInfoCard(NacreProcessor& p) : Card("Macro Routing"), proc(p) { startTimerHz(4); }
    void paint(juce::Graphics& g) override {
        Card::paint(g);
        const auto& t = theme();
        const char* names[4] = { "Tone", "Motion", "Space", "Grit" };
        auto c = content(); c.removeFromTop(6);
        auto srcNames = splitChoices(NACRE_SRC_NAMES); auto dstNames = splitChoices(NACRE_DST_NAMES);
        for (int m = 0; m < 4; ++m) {
            auto r = c.removeFromTop(c.getHeight() / (4 - m));
            g.setColour(t.text); g.setFont(font(12.5f, W::Semibold));
            g.drawText(juce::String(m + 1) + "  " + names[m], r.removeFromLeft(90), juce::Justification::topLeft);
            juce::StringArray lines;
            for (int s = 0; s < kModSlots; ++s) {
                const int so = (int) proc.getParam(mod1Src + s * 4), via = (int) proc.getParam(mod1Via + s * 4), d = (int) proc.getParam(mod1Dst + s * 4);
                const float a = proc.getParam(mod1Amt + s * 4);
                if (d == 0 || (so != sM1 + m && via != sM1 + m)) continue;
                juce::String line = juce::String(dstNames[(size_t) d]) + "  " + (a >= 0 ? "+" : "") + juce::String(juce::roundToInt(a * 100)) + "%";
                if (so != sM1 + m) line = juce::String(srcNames[(size_t) so]) + " depth " + u8("\xe2\x86\x92 ") + line;
                lines.add(line);
            }
            g.setColour(lines.isEmpty() ? t.text3 : t.text2); g.setFont(font(12.0f));
            g.drawFittedText(lines.isEmpty() ? juce::String("Not assigned. Use the Modulation page to route it.") : lines.joinIntoString(u8("   \xc2\xb7   ")),
                             r, juce::Justification::topLeft, 2);
        }
    }
private:
    void timerCallback() override { repaint(); }
    NacreProcessor& proc;
};

class AboutCard : public Card, private juce::Timer {
public:
    explicit AboutCard(NacreProcessor& p) : Card("Tips"), proc(p) {
        addAndMakeVisible(openFolder);
        openFolder.onClick = [this] { auto d = proc.presets.userFolder(); d.createDirectory(); d.startAsProcess(); };
        startTimerHz(4);
    }
    void layout() override { openFolder.setBounds(content().removeFromBottom(28).removeFromLeft(170)); }
    void paint(juce::Graphics& g) override {
        Card::paint(g);
        const auto& t = theme();
        auto c = content(); c.removeFromBottom(36);
        const char* tips[] = {
            "Double-click any control to reset it. Hold Shift while dragging for fine adjustment.",
            "Every factory sound has Tone, Motion, Space and Grit macros, and is level-matched so you can browse without riding the fader.",
            "Mod wheel adds vibrato or brightness, aftertouch opens the filter.",
            "Ctrl/Cmd+Z undoes, Ctrl/Cmd+Shift+Z redoes. Up/Down in the browser steps through sounds.",
            "Your presets are plain text files in Documents/Nacre/Presets." };
        g.setFont(font(12.0f));
        for (auto* s : tips) {
            auto r = c.removeFromTop(c.getHeight() / 5 + 0);
            g.setColour(t.accent); g.fillEllipse((float) r.getX(), (float) r.getY() + 6, 5, 5);
            g.setColour(t.text2); g.drawFittedText(s, r.withTrimmedLeft(14), juce::Justification::topLeft, 2);
        }
        g.setColour(t.text3); g.setFont(font(11.5f));
        g.drawText(juce::String(proc.getEngine().uiVoices.load()) + u8(" voices playing   \xc2\xb7   ") + juce::String(proc.getEngine().getSampleRate() / 1000.0, 1) + " kHz",
                   content().removeFromBottom(28).withTrimmedLeft(184), juce::Justification::centredLeft);
    }
private:
    void timerCallback() override { repaint(); }
    NacreProcessor& proc; juce::TextButton openFolder { "Open preset folder" };
};

class ArpPage : public juce::Component {
public:
    explicit ArpPage(NacreProcessor& p) : arp(p), voice(p), macros(p), about(p) {
        for (juce::Component* c : std::initializer_list<juce::Component*>{ &arp, &voice, &macros, &about }) addAndMakeVisible(c);
    }
    void resized() override {
        auto r = getLocalBounds();
        auto top = r.removeFromTop(262); r.removeFromTop(12);
        arp.setBounds(top.removeFromLeft(560)); top.removeFromLeft(12); voice.setBounds(top);
        macros.setBounds(r.removeFromLeft(560)); r.removeFromLeft(12); about.setBounds(r);
    }
private:
    ArpCard arp; VoiceCard voice; MacroInfoCard macros; AboutCard about;
};

} // namespace ui
