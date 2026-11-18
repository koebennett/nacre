#pragma once
#include "Pages.h"

namespace ui {

// Preset browser overlay: categories on the left, search, list, favourites.
class PresetBrowser : public juce::Component, private juce::ListBoxModel, private juce::TextEditor::Listener {
public:
    explicit PresetBrowser(NacreProcessor& p);
    std::function<void()> onClose;
    void refresh();
    void selectCurrent();
    void step(int delta);                       // load previous / next in the current list
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void visibilityChanged() override { if (isVisible()) { refresh(); selectCurrent(); search.grabKeyboardFocus(); } }

private:
    int getNumRows() override { return (int) filtered.size(); }
    void paintListBoxItem(int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override { load(row); if (onClose) onClose(); }
    void returnKeyPressed(int row) override { load(row); if (onClose) onClose(); }
    void selectedRowsChanged(int row) override { if (row >= 0 && !suppress) load(row); }
    void textEditorTextChanged(juce::TextEditor&) override { refresh(); }
    void textEditorEscapeKeyPressed(juce::TextEditor&) override { if (onClose) onClose(); }
    void load(int row);
    juce::Rectangle<int> sidebar() const { return getLocalBounds().reduced(1).removeFromLeft(210); }

    NacreProcessor& proc;
    juce::TextEditor search;
    juce::ListBox list { "presets", this };
    juce::TextButton close { "Done" };
    juce::StringArray cats;
    juce::String category { "All" };
    std::vector<int> filtered;
    bool suppress = false;
};

// A button that draws a vector icon.
class IconButton : public juce::Button {
public:
    using Draw = std::function<void(juce::Graphics&, juce::Rectangle<float>, juce::Colour)>;
    IconButton(const juce::String& name, Draw d) : juce::Button(name), draw(std::move(d)) {}
    void paintButton(juce::Graphics& g, bool over, bool down) override {
        const auto& t = theme();
        auto r = getLocalBounds().toFloat().reduced(0.5f);
        if (over || down || getToggleState()) { g.setColour(t.raised.withMultipliedAlpha(down ? 2.2f : 1.5f)); g.fillRoundedRectangle(r, 10.0f); }
        draw(g, r.reduced(r.getHeight() * 0.27f), isEnabled() ? (getToggleState() ? t.accent : t.text) : t.text3);
    }
    Draw draw;
};

// The preset name in the header: category above, name below, opens the browser.
class PresetPill : public juce::Button {
public:
    PresetPill() : juce::Button("Preset") {}
    juce::String name, category;
    void paintButton(juce::Graphics& g, bool over, bool down) override {
        const auto& t = theme();
        auto r = getLocalBounds().toFloat().reduced(1.0f);
        const float rr = r.getHeight() * 0.5f;
        g.setColour(t.dark ? juce::Colour(0x30000000) : juce::Colour(0x0e000000)); g.fillRoundedRectangle(r, rr);
        g.setColour(t.raised.withMultipliedAlpha(down ? 2.0f : (over ? 1.4f : 0.8f))); g.fillRoundedRectangle(r, rr);
        g.setGradientFill(juce::ColourGradient(t.edgeHi, 0, r.getY(), t.edgeLo, 0, r.getBottom(), false)); g.drawRoundedRectangle(r, rr, 1.0f);
        { const float d = 9.0f; auto dot = juce::Rectangle<float>(r.getX() + 16.0f, r.getCentreY() - d * 0.5f, d, d);
          g.setColour(t.accent.withAlpha(0.35f)); g.fillEllipse(dot.expanded(3.5f));
          g.setGradientFill(juce::ColourGradient(t.accent2, dot.getX(), dot.getY(), t.accent, dot.getRight(), dot.getBottom(), false)); g.fillEllipse(dot); }
        auto tr = r.reduced(34.0f, 3.0f);
        g.setColour(t.text2); g.setFont(font(9.5f, W::Semibold));
        g.drawText(category.toUpperCase(), tr.removeFromTop(13.0f), juce::Justification::centred);
        g.setColour(t.text); g.setFont(font(15.0f, W::Semibold));
        g.drawText(name, tr, juce::Justification::centred);
        juce::Path p; const float cx = r.getRight() - 16.0f, cy = r.getCentreY();
        p.startNewSubPath(cx - 4, cy - 2); p.lineTo(cx, cy + 2); p.lineTo(cx + 4, cy - 2);
        g.setColour(t.text2); g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};

// On-screen keyboard drawn to match the rest of the interface.
class Keys : public juce::MidiKeyboardComponent {
public:
    Keys(juce::MidiKeyboardState& s) : juce::MidiKeyboardComponent(s, horizontalKeyboard) {
        setScrollButtonsVisible(true); setKeyWidth(19.0f); setLowestVisibleKey(36); setOctaveForMiddleC(4);
        setWantsKeyboardFocus(true);
        for (int id : std::initializer_list<int>{ keySeparatorLineColourId, shadowColourId, upDownButtonBackgroundColourId, whiteNoteColourId })
            setColour(id, juce::Colours::transparentBlack);
    }
    void drawWhiteNote(int note, juce::Graphics& g, juce::Rectangle<float> a, bool down, bool over, juce::Colour, juce::Colour) override {
        const auto& t = theme();
        auto r = a.reduced(1.0f, 0.0f).withTrimmedBottom(1.0f);
        const auto base = t.keyWhite;
        if (down) g.setGradientFill(juce::ColourGradient(t.accent2, 0, r.getY(), t.accent, 0, r.getBottom(), false));
        else g.setGradientFill(juce::ColourGradient(over ? base.brighter(0.05f) : base.brighter(0.04f), 0, r.getY(), base.darker(t.dark ? 0.28f : 0.08f), 0, r.getBottom(), false));
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(t.dark ? juce::Colour(0x30000000) : juce::Colour(0x22000000)); g.drawRoundedRectangle(r.reduced(0.5f), 5.0f, 0.8f);
        if (note % 12 == 0) { g.setColour(down ? juce::Colours::white : t.text3); g.setFont(font(9.0f, W::Medium));
            g.drawText("C" + juce::String(note / 12 - 1), r.removeFromBottom(16), juce::Justification::centred); }
    }
    void drawBlackNote(int, juce::Graphics& g, juce::Rectangle<float> a, bool down, bool over, juce::Colour) override {
        const auto& t = theme();
        auto r = a.reduced(0.5f, 0.0f);
        auto k = r.withTrimmedTop(-4.0f);
        if (down) g.setGradientFill(juce::ColourGradient(t.accent, 0, k.getY(), t.accent2, 0, k.getBottom(), false));
        else g.setGradientFill(juce::ColourGradient(t.keyBlack.brighter(over ? 0.35f : 0.22f), 0, k.getY(), t.keyBlack.darker(0.4f), 0, k.getBottom(), false));
        g.fillRoundedRectangle(k, 4.0f);
        g.setColour(juce::Colours::white.withAlpha(0.16f)); g.fillRoundedRectangle(k.withHeight(1.5f).reduced(2.0f, 0.0f), 0.75f);
    }
    void drawUpDownButton(juce::Graphics& g, int w, int h, bool over, bool, bool up) override {
        const auto& t = theme();
        g.setColour(over ? t.text : t.text2);
        juce::Path p; const float cx = w * 0.5f, cy = h * 0.5f;
        if (up) { p.startNewSubPath(cx - 2, cy - 5); p.lineTo(cx + 3, cy); p.lineTo(cx - 2, cy + 5); }
        else { p.startNewSubPath(cx + 2, cy - 5); p.lineTo(cx - 3, cy); p.lineTo(cx + 2, cy + 5); }
        g.strokePath(p, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};

class Content : public juce::Component, private juce::Timer {
public:
    explicit Content(NacreProcessor& p);
    ~Content() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    static constexpr int W = 1180, H = 780;
    void applyTheme(bool dark);

private:
    void timerCallback() override;
    void showPage(int i);
    void stepPreset(int d);
    void savePreset();
    void updatePresetLabel();

    NacreProcessor& proc;
    Look look;
    Segmented tabs;
    SynthPage synth; ModPage mod; FxPage fx; ArpPage arp;
    juce::Component* pages[4];
    PresetBrowser browser;

    // header
    PresetPill presetBtn;
    juce::TextButton saveBtn { "Save" };
    IconButton prevBtn, nextBtn, favBtn, undoBtn, redoBtn, themeBtn, browseBtn;
    Knob volume;
    Meter meter;
    // footer
    std::unique_ptr<Knob> macros[4];
    Keys keyboard;
    std::unique_ptr<juce::AlertWindow> saveDialog;
    juce::TooltipWindow tooltips { this, 600 };
    juce::String shownPreset;
    juce::Image backdrop; juce::uint32 backdropKey = 0;
};

} // namespace ui

class NacreEditor : public juce::AudioProcessorEditor {
public:
    explicit NacreEditor(NacreProcessor&);
    ~NacreEditor() override;
    void resized() override;
    void paint(juce::Graphics& g) override { g.fillAll(ui::theme().bg); }
private:
    NacreProcessor& proc;
    ui::Content content;
};
