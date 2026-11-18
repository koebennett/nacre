// Nacre — visual language "Pearl & Glass": frosted glass cards floating over a soft iridescent backdrop,
// pearl-bead knobs, glowing value arcs, and an accent colour that follows the category of the loaded sound.
#pragma once
#include <JuceHeader.h>
#include "BinaryData.h"
#include "Diag.h"

namespace ui {

struct Theme {
    bool dark = true;
    juce::Colour bg, card, cardEdge, raised, text, text2, text3, accent, accent2, track, green, red, shadow, keyWhite, keyBlack,
                 popup, well, glassTop, glassBot, edgeHi, edgeLo, capHi, capLo;
    static Theme light() {
        Theme t; t.dark = false;
        t.bg = juce::Colour(0xfff3f0f7); t.card = juce::Colours::white; t.cardEdge = juce::Colour(0x14000000);
        t.raised = juce::Colour(0x0f1b1b21); t.text = juce::Colour(0xff1b1b21); t.text2 = juce::Colour(0xff5f6070); t.text3 = juce::Colour(0xffa4a5b4);
        t.accent = juce::Colour(0xff0a84ff); t.accent2 = t.accent; t.track = juce::Colour(0x16000000); t.green = juce::Colour(0xff2fbf5b); t.red = juce::Colour(0xffff3b30);
        t.shadow = juce::Colour(0x1c3a3560); t.keyWhite = juce::Colour(0xfffbfbfd); t.keyBlack = juce::Colour(0xff2a2b36);
        t.popup = juce::Colour(0xfffbfafd); t.well = juce::Colour(0x0c1b1b21);
        t.glassTop = juce::Colour(0xd2ffffff); t.glassBot = juce::Colour(0x8effffff); t.edgeHi = juce::Colour(0xf2ffffff); t.edgeLo = juce::Colour(0x70ffffff);
        t.capHi = juce::Colours::white; t.capLo = juce::Colour(0xffd9d5e6);
        return t;
    }
    static Theme darkTheme() {
        Theme t; t.dark = true;
        t.bg = juce::Colour(0xff07080d); t.card = juce::Colour(0xff161722); t.cardEdge = juce::Colour(0x1affffff);
        t.raised = juce::Colour(0x1affffff); t.text = juce::Colour(0xfff5f5f7); t.text2 = juce::Colour(0xffaeb0c2); t.text3 = juce::Colour(0xff6c6f84);
        t.accent = juce::Colour(0xff0a84ff); t.accent2 = t.accent; t.track = juce::Colour(0x26ffffff); t.green = juce::Colour(0xff30d158); t.red = juce::Colour(0xffff453a);
        t.shadow = juce::Colour(0x55000000); t.keyWhite = juce::Colour(0xffdedbe9); t.keyBlack = juce::Colour(0xff101118);
        t.popup = juce::Colour(0xff1b1c28); t.well = juce::Colour(0x4a000000);
        t.glassTop = juce::Colour(0x1affffff); t.glassBot = juce::Colour(0x0affffff); t.edgeHi = juce::Colour(0x48ffffff); t.edgeLo = juce::Colour(0x0effffff);
        t.capHi = juce::Colour(0xff40445c); t.capLo = juce::Colour(0xff12131c);
        return t;
    }
};

inline Theme& theme() { static Theme t = Theme::darkTheme(); return t; }

// The accent colour follows the category of the loaded sound, so every sound has its own colour.
inline juce::Colour accentFor(const juce::String& cat, bool dark) {
    struct E { const char* k; juce::uint32 c; };
    static const E tab[] = { {"Pads",0xff9b8cff},{"Bass",0xffff7a3d},{"Leads",0xffff4f8b},{"Keys",0xff3ec6e0},{"Plucks",0xff4fd69a},
        {"Bells",0xffffc24d},{"Strings",0xffd98cff},{"Brass",0xffffa23a},{"Organs",0xffe86a5a},{"Choirs",0xff8fb8ff},{"Chords",0xff6c9bff},
        {"Drones",0xff7a8cff},{"Atmos",0xff56d1c0},{"FX",0xffb9e63c},{"Arps",0xffff6bd6},{"Perc",0xfff0a04b} };
    juce::Colour c(0xff0a84ff);
    for (auto& e : tab) if (cat.startsWith(e.k)) { c = juce::Colour(e.c); break; }
    return dark ? c : c.darker(0.2f);
}
inline void setAccent(juce::Colour c) {
    auto& t = theme();
    t.accent = c;
    t.accent2 = c.withRotatedHue(0.11f).withMultipliedSaturation(0.9f).brighter(0.1f);
}

struct Fonts {
    juce::Typeface::Ptr regular, medium, semibold, bold;
    static Fonts& get() { static Fonts f; return f; }
    Fonts() {
        regular = juce::Typeface::createSystemTypefaceFor(BinaryData::InterRegular_otf, BinaryData::InterRegular_otfSize);
        medium = juce::Typeface::createSystemTypefaceFor(BinaryData::InterMedium_otf, BinaryData::InterMedium_otfSize);
        semibold = juce::Typeface::createSystemTypefaceFor(BinaryData::InterSemiBold_otf, BinaryData::InterSemiBold_otfSize);
        bold = juce::Typeface::createSystemTypefaceFor(BinaryData::InterBold_otf, BinaryData::InterBold_otfSize);
        if (!regular || !medium || !semibold || !bold) nacre_diag::log("embedded fonts failed to load - using system font");
    }
};
enum class W { Regular, Medium, Semibold, Bold };
// Falls back to the system font if an embedded font fails to load (DirectWrite can refuse OTF data on some Windows setups).
inline juce::Font font(float size, W w = W::Regular) {
    auto& f = Fonts::get();
    juce::Typeface::Ptr tf = w == W::Bold ? f.bold : w == W::Semibold ? f.semibold : w == W::Medium ? f.medium : f.regular;
    if (tf == nullptr) return juce::Font(juce::FontOptions(size, w >= W::Semibold ? juce::Font::bold : juce::Font::plain));
    return juce::Font(juce::FontOptions().withTypeface(tf).withHeight(size));
}

inline void card(juce::Graphics& g, juce::Rectangle<float> r, float radius = 18.0f, bool solid = false) {
    const auto& t = theme();
    for (int i = 0; i < 4; ++i) {   // cheap layered shadow (a real blur is too slow to repaint often)
        g.setColour(t.shadow.withMultipliedAlpha(0.32f / (float) (i + 1)));
        g.fillRoundedRectangle(r.expanded((float) i * 2.5f).translated(0, 3.0f + (float) i * 1.6f), radius + (float) i * 2.5f);
    }
    juce::Path p; p.addRoundedRectangle(r, radius);
    if (solid) { g.setColour(t.popup); g.fillPath(p); }
    g.setGradientFill(juce::ColourGradient(t.glassTop, 0, r.getY(), t.glassBot, 0, r.getBottom(), false));
    g.fillPath(p);
    juce::Path e; e.addRoundedRectangle(r.reduced(0.5f), radius - 0.5f);
    g.setGradientFill(juce::ColourGradient(t.edgeHi, 0, r.getY(), t.edgeLo, 0, r.getBottom(), false));
    g.strokePath(e, juce::PathStrokeType(1.0f));
    if (!t.dark) { g.setColour(t.cardEdge); g.drawRoundedRectangle(r.expanded(0.5f), radius + 0.5f, 0.8f); }
}

// Pre-rendered backdrop: soft colour orbs tinted by the accent, over a deep (dark) or pearl (light) base.
inline juce::Image makeBackdrop(int w, int h) {
    const auto& t = theme();
    juce::Image img(juce::Image::ARGB, w, h, true);
    juce::Graphics g(img);
    const float fw = (float) w, fh = (float) h;
    g.setGradientFill(juce::ColourGradient(t.dark ? juce::Colour(0xff0d0e17) : juce::Colour(0xfff8f5fb), 0, 0,
                                           t.dark ? juce::Colour(0xff05060a) : juce::Colour(0xffece8f3), 0, fh, false));
    g.fillAll();
    auto orb = [&](float x, float y, float rad, juce::Colour c, float a) {
        g.setGradientFill(juce::ColourGradient(c.withAlpha(a), x, y, c.withAlpha(0.0f), x + rad, y, true));
        g.fillEllipse(x - rad, y - rad, rad * 2, rad * 2);
    };
    const float da = t.dark ? 1.0f : 0.8f;
    orb(fw * 0.10f, fh * 0.02f, fw * 0.62f, t.accent, 0.42f * da);
    orb(fw * 0.98f, fh * 0.42f, fw * 0.52f, t.accent2, 0.30f * da);
    orb(fw * 0.45f, fh * 1.08f, fw * 0.60f, t.accent.withRotatedHue(-0.12f), 0.24f * da);
    // faint iridescent sheen
    juce::ColourGradient sh(juce::Colours::white.withAlpha(0.0f), 0, 0, juce::Colours::white.withAlpha(0.0f), fw, fh, false);
    sh.addColour(0.35, t.accent2.withAlpha(t.dark ? 0.05f : 0.07f));
    sh.addColour(0.6, juce::Colours::white.withAlpha(t.dark ? 0.03f : 0.18f));
    g.setGradientFill(sh); g.fillAll();
    return img;
}

class Look : public juce::LookAndFeel_V4 {
public:
    Look() {
        setDefaultSansSerifTypeface(Fonts::get().regular);
        refresh();
    }
    void refresh() {
        const auto& t = theme();
        setColour(juce::PopupMenu::backgroundColourId, t.popup);
        setColour(juce::PopupMenu::textColourId, t.text);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, t.accent);
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::ComboBox::textColourId, t.text);
        setColour(juce::Label::textColourId, t.text);
        setColour(juce::TextEditor::backgroundColourId, t.raised);
        setColour(juce::TextEditor::textColourId, t.text);
        setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        setColour(juce::TextEditor::focusedOutlineColourId, t.accent.withAlpha(0.6f));
        setColour(juce::TextEditor::highlightColourId, t.accent.withAlpha(0.3f));
        setColour(juce::CaretComponent::caretColourId, t.accent);
        setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::ScrollBar::thumbColourId, t.text3);
        setColour(juce::TooltipWindow::backgroundColourId, t.dark ? juce::Colour(0xf0262838) : juce::Colour(0xf01b1b21));
        setColour(juce::TooltipWindow::textColourId, juce::Colours::white);
        setColour(juce::AlertWindow::backgroundColourId, t.popup);
        setColour(juce::AlertWindow::textColourId, t.text);
        setColour(juce::TextButton::buttonColourId, t.raised);
        setColour(juce::TextButton::textColourOffId, t.text);
        setColour(juce::TextButton::buttonOnColourId, t.accent);
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        setColour(juce::ResizableWindow::backgroundColourId, t.bg);
        setColour(juce::TextButton::buttonColourId, t.raised);
    }

    // ---- knobs: pearl bead with a glowing accent arc and a line indicator
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override {
        const auto& t = theme();
        auto r = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(2.5f);
        const float size = juce::jmin(r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre(size, size);
        const auto c = r.getCentre();
        const float rad = size * 0.5f, stroke = juce::jmax(2.4f, size * 0.075f), arcR = rad - stroke * 0.5f;
        const float ang = a0 + pos * (a1 - a0);
        const bool bipolar = s.getProperties().getWithDefault("bipolar", false);
        const float origin = bipolar ? (a0 + a1) * 0.5f : a0;
        const auto pst = juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        juce::Path track; track.addCentredArc(c.x, c.y, arcR, arcR, 0, a0, a1, true);
        g.setColour(t.track); g.strokePath(track, pst);
        const bool en = s.isEnabled();
        if (std::abs(ang - origin) > 0.01f) {
            juce::Path arc; arc.addCentredArc(c.x, c.y, arcR, arcR, 0, juce::jmin(origin, ang), juce::jmax(origin, ang), true);
            if (en) { g.setColour(t.accent.withAlpha(t.dark ? 0.22f : 0.16f)); g.strokePath(arc, juce::PathStrokeType(stroke * 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
            g.setGradientFill(juce::ColourGradient(en ? t.accent : t.text3, c.x - rad, c.y + rad, en ? t.accent2 : t.text3, c.x + rad, c.y - rad, false));
            g.strokePath(arc, pst);
        }
        const float capR = rad - stroke * 1.8f;
        g.setColour(t.shadow); g.fillEllipse(c.x - capR - 0.5f, c.y - capR + 2.0f, capR * 2 + 1, capR * 2 + 1);
        g.setGradientFill(juce::ColourGradient(t.capHi, c.x - capR * 0.5f, c.y - capR * 0.7f, t.capLo, c.x + capR * 0.5f, c.y + capR, true));
        g.fillEllipse(c.x - capR, c.y - capR, capR * 2, capR * 2);
        g.setGradientFill(juce::ColourGradient(t.edgeHi, c.x, c.y - capR, t.edgeLo, c.x, c.y + capR, false));
        g.drawEllipse(c.x - capR, c.y - capR, capR * 2, capR * 2, 0.9f);
        const auto p1 = c.getPointOnCircumference(capR * 0.30f, ang), p2 = c.getPointOnCircumference(capR * 0.80f, ang);
        juce::Path ind; ind.startNewSubPath(p1); ind.lineTo(p2);
        g.setColour(en ? t.accent : t.text3);
        g.strokePath(ind, juce::PathStrokeType(juce::jmax(1.9f, size * 0.055f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // ---- horizontal bipolar amount bars (mod matrix)
    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s) override {
        const auto& t = theme();
        auto r = juce::Rectangle<float>((float) x, (float) y + h * 0.5f - 3.0f, (float) w, 6.0f);
        g.setColour(t.track); g.fillRoundedRectangle(r, 3.0f);
        const bool bipolar = s.getMinimum() < 0;
        const float zero = bipolar ? r.getCentreX() : r.getX();
        g.setColour(t.accent);
        g.fillRoundedRectangle(juce::Rectangle<float>(juce::jmin(zero, pos), r.getY(), std::abs(pos - zero), r.getHeight()), 3.0f);
        if (bipolar) { g.setColour(t.text3); g.fillRect(zero - 0.5f, r.getY() - 3, 1.0f, r.getHeight() + 6); }
        g.setColour(juce::Colours::white);
        { juce::Path th; th.addEllipse(pos - 7.0f, r.getCentreY() - 7.0f, 14.0f, 14.0f);
          juce::DropShadow(juce::Colour(0x30000000), 4, { 0, 1 }).drawForPath(g, th); }
        g.fillEllipse(pos - 7.0f, r.getCentreY() - 7.0f, 14.0f, 14.0f);
        g.setColour(t.cardEdge); g.drawEllipse(pos - 7.0f, r.getCentreY() - 7.0f, 14.0f, 14.0f, 0.8f);
    }

    // ---- iOS switch
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override {
        const auto& t = theme();
        auto r = b.getLocalBounds().toFloat();
        auto sw = r.removeFromLeft(juce::jmin(r.getWidth(), r.getHeight() * 1.65f)).withSizeKeepingCentre(juce::jmin(r.getHeight() * 1.65f, 34.0f), juce::jmin(r.getHeight(), 20.0f));
        const bool on = b.getToggleState();
        if (on) { g.setColour(t.accent.withAlpha(0.28f)); g.fillRoundedRectangle(sw.expanded(2.5f), sw.getHeight() * 0.5f + 2.5f); }
        if (on) g.setGradientFill(juce::ColourGradient(t.accent, sw.getX(), 0, t.accent2, sw.getRight(), 0, false)); else g.setColour(t.dark ? juce::Colour(0x38ffffff) : juce::Colour(0x26000000));
        g.fillRoundedRectangle(sw, sw.getHeight() * 0.5f);
        const float d = sw.getHeight() - 4.0f;
        const float kx = on ? sw.getRight() - d - 2.0f : sw.getX() + 2.0f;
        juce::DropShadow(juce::Colour(0x38000000), 4, { 0, 1 }).drawForRectangle(g, juce::Rectangle<int>((int) kx, (int) sw.getY() + 2, (int) d, (int) d));
        g.setColour(juce::Colours::white.darker(over ? 0.04f : 0.0f));
        g.fillEllipse(kx, sw.getY() + 2.0f, d, d);
        if (b.getButtonText().isNotEmpty()) {
            g.setColour(t.text); g.setFont(font(12.5f, W::Medium));
            g.drawText(b.getButtonText(), r.withTrimmedLeft(8), juce::Justification::centredLeft);
        }
    }

    // ---- pop-up buttons
    void drawComboBox(juce::Graphics& g, int w, int h, bool down, int, int, int, int, juce::ComboBox& cb) override {
        const auto& t = theme();
        auto r = juce::Rectangle<float>(0, 0, (float) w, (float) h).reduced(0.5f);
        g.setColour(t.raised.withMultipliedAlpha(down ? 1.9f : (cb.isMouseOver(true) ? 1.45f : 1.0f)));
        g.fillRoundedRectangle(r, juce::jmin(10.0f, h * 0.5f));
        g.setColour(t.dark ? juce::Colour(0x1affffff) : juce::Colour(0x14000000)); g.drawRoundedRectangle(r, juce::jmin(10.0f, h * 0.5f), 0.8f);
        // chevrons
        const float cx = (float) w - 13.0f, cy = (float) h * 0.5f;
        juce::Path p;
        p.startNewSubPath(cx - 3.5f, cy - 1.5f); p.lineTo(cx, cy - 5.0f); p.lineTo(cx + 3.5f, cy - 1.5f);
        p.startNewSubPath(cx - 3.5f, cy + 1.5f); p.lineTo(cx, cy + 5.0f); p.lineTo(cx + 3.5f, cy + 1.5f);
        g.setColour(cb.isEnabled() ? t.text2 : t.text3);
        g.strokePath(p, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    juce::Font getComboBoxFont(juce::ComboBox& cb) override { return font(juce::jmin(13.0f, cb.getHeight() * 0.5f), W::Medium); }
    void positionComboBoxText(juce::ComboBox& cb, juce::Label& l) override {
        l.setBounds(4, 0, cb.getWidth() - 24, cb.getHeight());
        l.setFont(getComboBoxFont(cb));
        l.setColour(juce::Label::textColourId, theme().text);
    }

    void drawPopupMenuBackground(juce::Graphics& g, int w, int h) override {
        const auto& t = theme();
        g.fillAll(t.popup);
        g.setColour(t.dark ? juce::Colour(0x30ffffff) : t.cardEdge); g.drawRect(0, 0, w, h);
    }
    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area, bool sep, bool active, bool hi, bool ticked, bool sub,
                           const juce::String& text, const juce::String& shortcut, const juce::Drawable*, const juce::Colour*) override {
        const auto& t = theme();
        if (sep) { g.setColour(t.cardEdge); g.fillRect(area.reduced(10, 0).withHeight(1).withY(area.getCentreY())); return; }
        auto r = area.reduced(5, 1).toFloat();
        if (hi && active) { g.setGradientFill(juce::ColourGradient(t.accent, r.getX(), 0, t.accent2, r.getRight(), 0, false)); g.fillRoundedRectangle(r, 7.0f); }
        g.setColour(hi && active ? juce::Colours::white : (active ? t.text : t.text3));
        g.setFont(font(13.0f, ticked ? W::Semibold : W::Regular));
        auto tr = r.reduced(8, 0);
        if (ticked) {
            juce::Path tick; auto tb = tr.removeFromLeft(14).withSizeKeepingCentre(10, 8);
            tick.startNewSubPath(tb.getX(), tb.getCentreY()); tick.lineTo(tb.getX() + 3.5f, tb.getBottom()); tick.lineTo(tb.getRight(), tb.getY());
            g.strokePath(tick, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        } else tr.removeFromLeft(14);
        g.drawText(text, tr.withTrimmedLeft(4), juce::Justification::centredLeft);
        if (sub) { g.drawText(juce::String::fromUTF8("\xe2\x80\xba"), tr, juce::Justification::centredRight); }
        else if (shortcut.isNotEmpty()) { g.setFont(font(12.0f)); g.drawText(shortcut, tr, juce::Justification::centredRight); }
    }
    juce::Font getPopupMenuFont() override { return font(13.0f); }
    void getIdealPopupMenuItemSize(const juce::String& text, bool sep, int standard, int& w, int& h) override {
        if (sep) { w = 50; h = 9; return; }
        w = juce::GlyphArrangement::getStringWidthInt(font(13.0f, W::Semibold), text) + 52; h = juce::jmax(24, standard);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override {
        const auto& t = theme();
        auto r = b.getLocalBounds().toFloat().reduced(0.5f);
        const bool on = b.getToggleState() || b.getProperties().getWithDefault("primary", false);
        const float rr = juce::jmin(10.0f, r.getHeight() * 0.5f);
        if (on) { g.setGradientFill(juce::ColourGradient(down ? t.accent.darker(0.1f) : t.accent, r.getX(), 0, down ? t.accent2.darker(0.1f) : t.accent2, r.getRight(), 0, false)); }
        else g.setColour(t.raised.withMultipliedAlpha(down ? 2.0f : (over ? 1.5f : 1.0f)));
        g.fillRoundedRectangle(r, rr);
        if (!on) { g.setColour(t.dark ? juce::Colour(0x1affffff) : juce::Colour(0x14000000)); g.drawRoundedRectangle(r, rr, 0.8f); }
    }
    juce::Font getTextButtonFont(juce::TextButton&, int h) override { return font(juce::jmin(13.0f, h * 0.48f), W::Medium); }

    int getDefaultScrollbarWidth() override { return 8; }
    void drawScrollbar(juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int ts, int tsize, bool over, bool) override {
        auto r = vertical ? juce::Rectangle<int>(x, ts, w, tsize) : juce::Rectangle<int>(ts, y, tsize, h);
        g.setColour(theme().text3.withAlpha(over ? 0.8f : 0.5f));
        g.fillRoundedRectangle(r.toFloat().reduced(2), 3.0f);
    }

    void drawTooltip(juce::Graphics& g, const juce::String& text, int w, int h) override {
        g.setColour(findColour(juce::TooltipWindow::backgroundColourId));
        g.fillRoundedRectangle(0, 0, (float) w, (float) h, 6.0f);
        g.setColour(juce::Colours::white); g.setFont(font(12.0f));
        g.drawFittedText(text, juce::Rectangle<int>(w, h).reduced(8, 4), juce::Justification::centredLeft, 4);
    }
    juce::Rectangle<int> getTooltipBounds(const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) override {
        const int w = juce::jmin(320, juce::GlyphArrangement::getStringWidthInt(font(12.0f), text) + 20);
        const int lines = juce::jmax(1, juce::GlyphArrangement::getStringWidthInt(font(12.0f), text) / 300 + 1);
        const int h = 10 + lines * 16;
        return juce::Rectangle<int>(pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 16, pos.y + 18, w, h).constrainedWithin(parent);
    }

    void drawAlertBox(juce::Graphics& g, juce::AlertWindow& a, const juce::Rectangle<int>& textArea, juce::TextLayout& tl) override {
        ui::card(g, a.getLocalBounds().toFloat().reduced(3), 16.0f, true);
        tl.draw(g, textArea.toFloat());
    }
    juce::Font getAlertWindowTitleFont() override { return font(16.0f, W::Semibold); }
    juce::Font getAlertWindowMessageFont() override { return font(13.0f); }
    juce::Font getAlertWindowFont() override { return font(13.0f); }
};

} // namespace ui
