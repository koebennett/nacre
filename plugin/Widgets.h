// Nacre — controls and visualisers.
#pragma once
#include "LookAndFeel.h"
#include "PluginProcessor.h"

namespace ui {
inline juce::String u8(const char* s) { return juce::String::fromUTF8(s); }
using APVTS = juce::AudioProcessorValueTreeState;

// ---------------------------------------------------------------- knob
class Knob : public juce::Component, private juce::Slider::Listener {
public:
    Knob(NacreProcessor& p, int id, juce::String labelText, float size = 0)
        : proc(p), pid(id), label(std::move(labelText)), forcedSize(size) {
        const auto& d = nacre::kParams[id];
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters(juce::degreesToRadians(225.0f), juce::degreesToRadians(495.0f), true);
        slider.setMouseDragSensitivity(220);
        slider.setVelocityModeParameters(0.35, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
        slider.getProperties().set("bipolar", d.lo < 0 && std::abs(d.lo + d.hi) < 1e-4f);
        slider.setPopupDisplayEnabled(false, false, nullptr);
        slider.setTooltip(juce::String(d.name) + "\nDouble-click to reset. Hold Shift for fine control.");
        attach = std::make_unique<APVTS::SliderAttachment>(p.apvts, d.id, slider);
        slider.setDoubleClickReturnValue(true, d.def);
        slider.addListener(this);
        slider.addMouseListener(this, false);
        addAndMakeVisible(slider);
    }
    ~Knob() override { slider.removeListener(this); }
    void resized() override {
        auto r = getLocalBounds();
        const int s = forcedSize > 0 ? (int) forcedSize : juce::jmin(r.getWidth(), r.getHeight() - 16);
        slider.setBounds(r.removeFromTop(s).withSizeKeepingCentre(s, s));
    }
    void paint(juce::Graphics& g) override {
        const auto& t = theme();
        auto r = getLocalBounds().withTrimmedTop(slider.getBottom());
        const bool showValue = hovering || slider.isMouseButtonDown();
        g.setColour(showValue ? t.accent : t.text2);
        g.setFont(font(11.0f, showValue ? W::Semibold : W::Medium));
        const auto& d = nacre::kParams[pid];
        g.drawText(showValue ? juce::String(nacre::formatValue(d, (float) slider.getValue())) : label, r, juce::Justification::centredTop, false);
    }
    void mouseEnter(const juce::MouseEvent&) override { hovering = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hovering = false; repaint(); }
    juce::Slider slider;
private:
    void sliderValueChanged(juce::Slider*) override { repaint(); }
    NacreProcessor& proc; int pid; juce::String label; float forcedSize; bool hovering = false;
    std::unique_ptr<APVTS::SliderAttachment> attach;
};

// ---------------------------------------------------------------- switch (iOS style)
class Switch : public juce::ToggleButton {
public:
    Switch(NacreProcessor& p, int id, juce::String text = {}) : juce::ToggleButton(text) {
        setTooltip(nacre::kParams[id].name);
        attach = std::make_unique<APVTS::ButtonAttachment>(p.apvts, nacre::kParams[id].id, *this);
    }
private:
    std::unique_ptr<APVTS::ButtonAttachment> attach;
};

// ---------------------------------------------------------------- pop-up choice
class Choice : public juce::ComboBox {
public:
    Choice(NacreProcessor& p, int id) {
        int i = 1;
        for (auto& s : nacre::splitChoices(nacre::kParams[id].choices)) addItem(s, i++);
        setTooltip(nacre::kParams[id].name);
        attach = std::make_unique<APVTS::ComboBoxAttachment>(p.apvts, nacre::kParams[id].id, *this);
    }
private:
    std::unique_ptr<APVTS::ComboBoxAttachment> attach;
};

// ---------------------------------------------------------------- segmented control
class Segmented : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    Segmented(juce::StringArray items, std::function<void(int)> onPick) : labels(std::move(items)), pick(std::move(onPick)) {}
    void setSelected(int i) { if (i != sel) { sel = i; if (shown < 0) shown = (float) i; startTimerHz(60); repaint(); } }
    int getSelected() const { return sel; }
    void paint(juce::Graphics& g) override {
        const auto& t = theme();
        auto r = getLocalBounds().toFloat();
        const float rr = juce::jmin(12.0f, r.getHeight() * 0.5f);
        g.setColour(t.dark ? juce::Colour(0x30000000) : juce::Colour(0x0f000000)); g.fillRoundedRectangle(r, rr);
        g.setColour(t.dark ? juce::Colour(0x18ffffff) : juce::Colour(0x12000000)); g.drawRoundedRectangle(r.reduced(0.5f), rr, 0.8f);
        const float w = r.getWidth() / (float) labels.size();
        if (sel >= 0) {
            const float px = shown < 0 ? (float) sel : shown;
            auto s = juce::Rectangle<float>(r.getX() + w * px, r.getY(), w, r.getHeight()).reduced(3.0f);
            g.setColour(t.accent.withAlpha(0.30f)); g.fillRoundedRectangle(s.expanded(2.0f), rr);   // glow
            g.setGradientFill(juce::ColourGradient(t.accent, s.getX(), s.getY(), t.accent2, s.getRight(), s.getBottom(), false));
            g.fillRoundedRectangle(s, juce::jmax(4.0f, rr - 3.0f));
            g.setColour(juce::Colours::white.withAlpha(0.30f)); g.fillRoundedRectangle(s.withHeight(s.getHeight() * 0.5f).reduced(1.5f, 0.5f), juce::jmax(3.0f, rr - 4.0f));   // gloss
        }
        for (int i = 0; i < labels.size(); ++i) {
            g.setColour(i == sel ? juce::Colours::white : t.text2);
            g.setFont(font(juce::jmin(13.0f, r.getHeight() * 0.45f), i == sel ? W::Semibold : W::Medium));
            g.drawText(labels[i], juce::Rectangle<float>(r.getX() + w * (float) i, r.getY(), w, r.getHeight()), juce::Justification::centred);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override {
        const int i = juce::jlimit(0, labels.size() - 1, (int) ((float) e.x / ((float) getWidth() / (float) labels.size())));
        setSelected(i); if (pick) pick(i);
    }
private:
    void timerCallback() override {
        if (shown < 0) shown = (float) sel;
        shown += ((float) sel - shown) * 0.28f;
        if (std::abs((float) sel - shown) < 0.004f) { shown = (float) sel; stopTimer(); }
        repaint();
    }
    juce::StringArray labels; std::function<void(int)> pick; int sel = 0; float shown = -1.0f;
};

// Segmented control bound to a choice parameter.
class ParamSegmented : public Segmented {
public:
    ParamSegmented(NacreProcessor& p, int id)
        : Segmented([&] { juce::StringArray a; for (auto& s : nacre::splitChoices(nacre::kParams[id].choices)) a.add(s); return a; }(),
                    [this](int i) { attach->setValueAsCompleteGesture((float) i); }) {
        attach = std::make_unique<juce::ParameterAttachment>(*p.param(id), [this](float v) { setSelected((int) std::lround(v)); }, &p.undo);
        attach->sendInitialUpdate();
        setTooltip(nacre::kParams[id].name);
    }
private:
    std::unique_ptr<juce::ParameterAttachment> attach;
};

// ---------------------------------------------------------------- card
class Card : public juce::Component {
public:
    explicit Card(juce::String t) : title(std::move(t)) {}
    void paint(juce::Graphics& g) override {
        card(g, getLocalBounds().toFloat().reduced(1.0f));
        auto ta = titleArea();
        if (!power) {
            const auto& t = theme();
            g.setGradientFill(juce::ColourGradient(t.accent, 0, 0, t.accent2, 7, 7, false)); g.fillEllipse((float) ta.getX(), (float) ta.getCentreY() - 3.5f, 7, 7);
            ta.removeFromLeft(15);
        }
        g.setColour(theme().text); g.setFont(font(13.5f, W::Semibold));
        g.drawText(title, ta, juce::Justification::centredLeft);
    }
    juce::Rectangle<int> titleArea() const { return getLocalBounds().reduced(14, 0).withHeight(40).withTrimmedLeft(power ? 44 : 0); }
    juce::Rectangle<int> content() const { return getLocalBounds().reduced(14, 10).withTrimmedTop(32); }
    void setPower(juce::Component* sw) { power = sw; addAndMakeVisible(sw); }
    void resized() override { if (power) power->setBounds(14, 11, 38, 20); layout(); }
    virtual void layout() {}
    juce::String title;
protected:
    juce::Component* power = nullptr;
};

// Lay knobs out in a row, evenly.
inline void row(juce::Rectangle<int> r, std::initializer_list<juce::Component*> cs) {
    const int n = (int) cs.size(); if (!n) return;
    const int w = r.getWidth() / n; int i = 0;
    for (auto* c : cs) { if (c) c->setBounds(r.getX() + i * w, r.getY(), w, r.getHeight()); ++i; }
}

// Little caption above a control.
inline void caption(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& s) {
    g.setColour(theme().text2); g.setFont(font(11.0f, W::Medium)); g.drawText(s, r, juce::Justification::centredLeft);
}

// ---------------------------------------------------------------- visualisers
class Visual : public juce::Component, private juce::Timer {
public:
    explicit Visual(NacreProcessor& p) : proc(p) { startTimerHz(30); setInterceptsMouseClicks(false, false); }
protected:
    float P(int id) const { return proc.getParam(id); }
    virtual uint64_t signature() const { return 0; }
    NacreProcessor& proc;
private:
    void timerCallback() override { auto s = signature(); if (s != last) { last = s; repaint(); } }
    uint64_t last = 1;
};

inline uint64_t mix64(uint64_t h, float v) { uint32_t b; std::memcpy(&b, &v, 4); return (h ^ b) * 0x100000001b3ULL; }

inline void panel(juce::Graphics& g, juce::Rectangle<float> r) {
    const auto& t = theme();
    g.setColour(t.well);
    g.fillRoundedRectangle(r, 12.0f);
    g.setColour(t.dark ? juce::Colour(0x12ffffff) : juce::Colour(0x0f000000)); g.drawRoundedRectangle(r.reduced(0.5f), 12.0f, 0.8f);
}

class WavetableView : public Visual {
public:
    WavetableView(NacreProcessor& p, bool isA) : Visual(p), a(isA) {}
    uint64_t signature() const override {
        uint64_t h = 1469598103934665603ULL;
        h = mix64(h, P(a ? nacre::oscATable : nacre::oscBTable)); h = mix64(h, livePos()); h = mix64(h, P(a ? nacre::oscAOn : nacre::oscBOn));
        h = mix64(h, P(a ? nacre::oscAWarp : nacre::oscBWarp)); h = mix64(h, P(a ? nacre::oscAWarpMode : nacre::oscBWarpMode)); h = mix64(h, theme().dark);
        return h;
    }
    float livePos() const {
        auto& e = proc.getEngine();
        return e.uiVoices.load() > 0 ? (a ? e.uiPosA.load() : e.uiPosB.load()) : P(a ? nacre::oscAPos : nacre::oscBPos);
    }
    void paint(juce::Graphics& g) override {
        const auto& t = theme();
        auto r = getLocalBounds().toFloat();
        panel(g, r);
        const auto& wt = nacre::WavetableBank::get().table((int) P(a ? nacre::oscATable : nacre::oscBTable));
        const bool on = P(a ? nacre::oscAOn : nacre::oscBOn) > 0.5f;
        const float pos = livePos();
        const int warpMode = (int) P(a ? nacre::oscAWarpMode : nacre::oscBWarpMode);
        const float warp = P(a ? nacre::oscAWarp : nacre::oscBWarp);
        auto area = r.reduced(14.0f, 12.0f);
        const int layers = 14;
        const float dx = area.getWidth() * 0.16f / layers, dy = area.getHeight() * 0.30f / layers;
        const float w = area.getWidth() - dx * layers, h = (area.getHeight() - dy * layers) * 0.42f;
        auto framePath = [&](float fpos, float ox, float oy) {
            juce::Path p;
            const float fp = juce::jlimit(0.0f, 1.0f, fpos) * (nacre::kWtFrames - 1);
            const int f0 = juce::jmin((int) fp, nacre::kWtFrames - 2); const float ft = fp - (float) f0;
            for (int i = 0; i <= 96; ++i) {
                float ph = (float) i / 96.0f;
                ph = warpPhase(ph, warpMode, warp);
                const float xi = ph * 127.0f; const int i0 = juce::jmin((int) xi, 126); const float fr = xi - (float) i0;
                auto at = [&](int f) { const float* d = wt.preview.data() + f * 128; return d[i0] + (d[i0 + 1] - d[i0]) * fr; };
                const float v = at(f0) + (at(f0 + 1) - at(f0)) * ft;
                const float x = area.getX() + ox + w * (float) i / 96.0f, y = area.getBottom() - h - oy - v * h * 0.95f;
                if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
            }
            return p;
        };
        for (int k = layers - 1; k >= 0; --k) {
            const float fpos = (float) k / (layers - 1);
            g.setColour(t.text3.withAlpha(on ? 0.22f + 0.18f * (1.0f - (float) k / layers) : 0.12f));
            g.strokePath(framePath(fpos, dx * k, dy * k), juce::PathStrokeType(1.0f));
        }
        const float kk = pos * (layers - 1);
        auto cur = framePath(pos, dx * kk, dy * kk);
        g.setColour((on ? t.accent : t.text3).withAlpha(0.95f));
        g.strokePath(cur, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(t.text2); g.setFont(font(10.5f, W::Medium));
        g.drawText(juce::String(juce::roundToInt(pos * 100)) + "%", r.reduced(10, 6), juce::Justification::topLeft);
    }
    static float warpPhase(float p, int mode, float w) {
        switch (mode) {
            case 1: { float q = p * (1.0f + 7.0f * w); return q - std::floor(q); }
            case 2: { float k = (w - 0.5f) * 2.0f; k = k >= 0 ? k * 12.0f : k * 0.92f; return p * (1.0f + k) / (1.0f + k * p); }
            case 3: { float wd = 1.0f - 0.92f * w; return p < wd ? p / wd : 0.0f; }
            case 4: { float m = p < 0.5f ? p : 1.0f - p; return p + (m - p) * w; }
            default: return juce::jmin(p, 0.9999f);
        }
    }
private:
    bool a;
};

class FilterView : public Visual {
public:
    using Visual::Visual;
    uint64_t signature() const override {
        uint64_t h = 7; for (int id : { nacre::f1On, nacre::f1Type, nacre::f1Cut, nacre::f1Res, nacre::f2On, nacre::f2Type, nacre::f2Cut, nacre::f2Res, nacre::fRouting }) h = mix64(h, P(id));
        h = mix64(h, proc.getEngine().uiVoices.load() > 0 ? proc.getEngine().uiCutoff.load() : -1.0f); return mix64(h, theme().dark);
    }
    void paint(juce::Graphics& g) override {
        const auto& t = theme();
        auto r = getLocalBounds().toFloat(); panel(g, r);
        auto a = r.reduced(8.0f, 10.0f);
        const float lo = std::log(20.0f), hi = std::log(20000.0f);
        auto xOf = [&](float f) { return a.getX() + (std::log(f) - lo) / (hi - lo) * a.getWidth(); };
        auto yOf = [&](float db) { return a.getY() + (18.0f - juce::jlimit(-42.0f, 18.0f, db)) / 60.0f * a.getHeight(); };
        g.setColour(t.text3.withAlpha(0.25f));
        for (float f : { 100.0f, 1000.0f, 10000.0f }) g.fillRect(xOf(f), a.getY(), 1.0f, a.getHeight());
        g.fillRect(a.getX(), yOf(0), a.getWidth(), 1.0f);
        const bool o1 = P(nacre::f1On) > 0.5f, o2 = P(nacre::f2On) > 0.5f, par = P(nacre::fRouting) > 0.5f;
        float live = proc.getEngine().uiVoices.load() > 0 ? proc.getEngine().uiCutoff.load() : P(nacre::f1Cut);
        juce::Path p;
        for (int i = 0; i <= 160; ++i) {
            const float f = std::exp(lo + (hi - lo) * (float) i / 160.0f);
            float db = 0;
            const float d1 = o1 ? nacre::filterResponseDb((int) P(nacre::f1Type), live, P(nacre::f1Res), f) : 0.0f;
            const float d2 = o2 ? nacre::filterResponseDb((int) P(nacre::f2Type), P(nacre::f2Cut), P(nacre::f2Res), f) : 0.0f;
            if (par && o1 && o2) db = nacre::gainToDb(0.7f * (std::pow(10.0f, d1 / 20) + std::pow(10.0f, d2 / 20)));
            else db = d1 + d2;
            const float x = a.getX() + a.getWidth() * (float) i / 160.0f, y = yOf(db);
            if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
        }
        juce::Path fill(p); fill.lineTo(a.getRight(), a.getBottom()); fill.lineTo(a.getX(), a.getBottom()); fill.closeSubPath();
        g.setGradientFill(juce::ColourGradient(t.accent.withAlpha(0.28f), 0, a.getY(), t.accent.withAlpha(0.02f), 0, a.getBottom(), false));
        g.fillPath(fill);
        g.setColour((o1 || o2) ? t.accent : t.text3);
        g.strokePath(p, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (o1) { const float x = xOf(juce::jlimit(20.0f, 20000.0f, live)); g.setColour(t.accent); g.fillEllipse(x - 4, yOf(nacre::filterResponseDb((int) P(nacre::f1Type), live, P(nacre::f1Res), live)) - 4, 8, 8); }
        g.setColour(t.text3); g.setFont(font(9.5f));
        g.drawText("100", juce::Rectangle<float>(xOf(100) + 3, a.getBottom() - 12, 30, 12), juce::Justification::left);
        g.drawText("1k", juce::Rectangle<float>(xOf(1000) + 3, a.getBottom() - 12, 30, 12), juce::Justification::left);
        g.drawText("10k", juce::Rectangle<float>(xOf(10000) + 3, a.getBottom() - 12, 30, 12), juce::Justification::left);
    }
};

class EnvView : public Visual {
public:
    EnvView(NacreProcessor& p, int firstId) : Visual(p), base(firstId) {}
    uint64_t signature() const override { uint64_t h = 3; for (int i = 0; i < 4; ++i) h = mix64(h, P(base + i)); return mix64(h, theme().dark); }
    void paint(juce::Graphics& g) override {
        const auto& t = theme();
        auto r = getLocalBounds().toFloat(); panel(g, r);
        auto a = r.reduced(10.0f, 12.0f);
        const float A = P(base), D = P(base + 1), S = P(base + 2), R = P(base + 3);
        auto tw = [](float s) { return std::sqrt(s / 10.0f) + 0.04f; };
        const float total = tw(A) + tw(D) + 0.35f + tw(R);
        const float sx = a.getWidth() / total;
        float x = a.getX();
        auto y = [&](float v) { return a.getBottom() - v * a.getHeight(); };
        juce::Path p; p.startNewSubPath(x, y(0));
        const float xa = x + tw(A) * sx;
        p.quadraticTo(x + (xa - x) * 0.35f, y(0.85f), xa, y(1)); x = xa;
        const float xd = x + tw(D) * sx;
        p.quadraticTo(x + (xd - x) * 0.15f, y(S), xd, y(S)); x = xd;
        const float xs = x + 0.35f * sx; p.lineTo(xs, y(S)); x = xs;
        const float xr = x + tw(R) * sx;
        p.quadraticTo(x + (xr - x) * 0.15f, y(0), xr, y(0));
        juce::Path fill(p); fill.lineTo(xr, a.getBottom()); fill.lineTo(a.getX(), a.getBottom()); fill.closeSubPath();
        g.setGradientFill(juce::ColourGradient(t.accent.withAlpha(0.25f), 0, a.getY(), t.accent.withAlpha(0.02f), 0, a.getBottom(), false));
        g.fillPath(fill);
        g.setColour(t.accent); g.strokePath(p, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(t.text3.withAlpha(0.5f));
        for (float xx : { xa, xd, xs }) g.fillRect(xx, a.getY(), 1.0f, a.getHeight());
    }
private:
    int base;
};

class LfoView : public Visual {
public:
    LfoView(NacreProcessor& p, int index) : Visual(p), idx(index) {}
    uint64_t signature() const override {
        uint64_t h = 5; h = mix64(h, P(nacre::lfo1Shape + idx * 6)); h = mix64(h, proc.getEngine().uiLfo[idx].load()); return mix64(h, theme().dark);
    }
    void paint(juce::Graphics& g) override {
        const auto& t = theme();
        auto r = getLocalBounds().toFloat(); panel(g, r);
        auto a = r.reduced(10.0f, 14.0f);
        const int shape = (int) P(nacre::lfo1Shape + idx * 6);
        juce::Path p;
        juce::Random rnd(7);
        float held = rnd.nextFloat() * 2 - 1, r0 = held, r1 = rnd.nextFloat() * 2 - 1;
        for (int i = 0; i <= 200; ++i) {
            const float ph2 = (float) i / 100.0f; const float ph = ph2 - std::floor(ph2);
            if (i > 0 && (int) ph2 != (int) ((float) (i - 1) / 100.0f)) { r0 = r1; r1 = rnd.nextFloat() * 2 - 1; held = r1; }
            float v;
            switch (shape) {
                case 0: v = std::sin(juce::MathConstants<float>::twoPi * ph); break;
                case 1: v = ph < 0.5f ? 4 * ph - 1 : 3 - 4 * ph; break;
                case 2: v = 2 * ph - 1; break;
                case 3: v = 1 - 2 * ph; break;
                case 4: v = ph < 0.5f ? 1.0f : -1.0f; break;
                case 5: v = held; break;
                default: v = r0 + (r1 - r0) * (0.5f - 0.5f * std::cos(juce::MathConstants<float>::pi * ph)); break;
            }
            const float x = a.getX() + a.getWidth() * (float) i / 200.0f, y = a.getCentreY() - v * a.getHeight() * 0.5f;
            if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
        }
        g.setColour(t.text3.withAlpha(0.35f)); g.fillRect(a.getX(), a.getCentreY(), a.getWidth(), 1.0f);
        g.setColour(t.accent); g.strokePath(p, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float v = proc.getEngine().uiLfo[idx].load();
        const float yy = a.getCentreY() - v * a.getHeight() * 0.5f;
        g.setColour(t.accent.withAlpha(0.25f)); g.fillRect(a.getRight() - 4, juce::jmin(yy, a.getCentreY()), 3.0f, std::abs(yy - a.getCentreY()));
        g.setColour(t.accent); g.fillEllipse(a.getRight() - 6.5f, yy - 4, 8, 8);
    }
private:
    int idx;
};

class Meter : public juce::Component, private juce::Timer {
public:
    explicit Meter(NacreProcessor& p) : proc(p) { startTimerHz(30); }
    void paint(juce::Graphics& g) override {
        const auto& t = theme();
        auto r = getLocalBounds().toFloat();
        const float w = (r.getWidth() - 3) / 2;
        for (int c = 0; c < 2; ++c) {
            auto b = juce::Rectangle<float>(r.getX() + c * (w + 3), r.getY(), w, r.getHeight());
            g.setColour(t.track); g.fillRoundedRectangle(b, 2.0f);
            const float db = nacre::gainToDb(lvl[c]);
            const float frac = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
            auto f = b.withTrimmedTop(b.getHeight() * (1 - frac));
            g.setColour(db > -1.0f ? t.red : (db > -9.0f ? juce::Colour(0xffffcc00) : t.green));
            g.fillRoundedRectangle(f, 2.0f);
        }
    }
private:
    void timerCallback() override {
        auto& e = proc.getEngine();
        const float in[2] = { e.uiPeakL.exchange(0.0f), e.uiPeakR.exchange(0.0f) };
        bool changed = false;
        for (int c = 0; c < 2; ++c) { const float nv = juce::jmax(in[c], lvl[c] * 0.82f); changed |= std::abs(nv - lvl[c]) > 1e-4f; lvl[c] = nv; }
        if (changed) repaint();
    }
    NacreProcessor& proc; float lvl[2] = { 0, 0 };
};

} // namespace ui
