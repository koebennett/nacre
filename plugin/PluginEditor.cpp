#include "PluginEditor.h"
#include "Diag.h"

namespace ui {

// ------------------------------------------------------------------ icons
static void chevron(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool left) {
    juce::Path p; const float cx = r.getCentreX(), cy = r.getCentreY(), s = r.getHeight() * 0.42f;
    if (left) { p.startNewSubPath(cx + s * 0.45f, cy - s); p.lineTo(cx - s * 0.45f, cy); p.lineTo(cx + s * 0.45f, cy + s); }
    else { p.startNewSubPath(cx - s * 0.45f, cy - s); p.lineTo(cx + s * 0.45f, cy); p.lineTo(cx - s * 0.45f, cy + s); }
    g.setColour(c); g.strokePath(p, juce::PathStrokeType(1.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
static void heart(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool filled) {
    juce::Path p; const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
    p.startNewSubPath(x + w * 0.5f, y + h * 0.95f);
    p.cubicTo(x - w * 0.15f, y + h * 0.45f, x + w * 0.15f, y - h * 0.15f, x + w * 0.5f, y + h * 0.25f);
    p.cubicTo(x + w * 0.85f, y - h * 0.15f, x + w * 1.15f, y + h * 0.45f, x + w * 0.5f, y + h * 0.95f);
    p.closeSubPath();
    if (filled) { g.setColour(theme().red); g.fillPath(p); }
    else { g.setColour(c); g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
}
static void undoIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool redo) {
    juce::Path p; const float cx = r.getCentreX(), cy = r.getCentreY() + 1, rad = r.getHeight() * 0.38f;
    const float s = redo ? -1.0f : 1.0f;
    p.addCentredArc(cx, cy, rad, rad, 0, s * -1.9f, s * 1.3f, true);
    g.setColour(c); g.strokePath(p, juce::PathStrokeType(1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const auto tip = juce::Point<float>(cx, cy).getPointOnCircumference(rad, s * -1.9f);
    juce::Path a; a.addTriangle(tip.x - 3.5f, tip.y - 3.5f, tip.x + 3.5f * s, tip.y + 0.5f, tip.x - 3.5f, tip.y + 4.0f);
    a.applyTransform(juce::AffineTransform::rotation(s * 0.3f, tip.x, tip.y));
    g.fillPath(a);
}
static void themeIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) {
    const float cx = r.getCentreX(), cy = r.getCentreY(), rad = r.getHeight() * 0.42f;
    g.setColour(c);
    g.drawEllipse(cx - rad, cy - rad, rad * 2, rad * 2, 1.6f);
    juce::Path half; half.addPieSegment(cx - rad, cy - rad, rad * 2, rad * 2, 0, juce::MathConstants<float>::pi, 0);
    g.fillPath(half);
}
static void searchIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) {
    const float rad = r.getHeight() * 0.33f; const float cx = r.getCentreX() - 1.5f, cy = r.getCentreY() - 1.5f;
    g.setColour(c); g.drawEllipse(cx - rad, cy - rad, rad * 2, rad * 2, 1.7f);
    g.drawLine(cx + rad * 0.72f, cy + rad * 0.72f, cx + rad * 1.6f, cy + rad * 1.6f, 1.9f);
}

// ------------------------------------------------------------------ preset browser
PresetBrowser::PresetBrowser(NacreProcessor& p) : proc(p) {
    search.setTextToShowWhenEmpty("Search sounds, categories or tags", theme().text3);
    search.setFont(font(14.0f));
    search.setIndents(12, 8);
    search.addListener(this);
    list.setRowHeight(34);
    list.setOutlineThickness(0);
    close.onClick = [this] { if (onClose) onClose(); };
    close.getProperties().set("primary", true);
    addAndMakeVisible(search); addAndMakeVisible(list); addAndMakeVisible(close);
    setWantsKeyboardFocus(true);
}

void PresetBrowser::refresh() {
    cats = proc.presets.categories();
    const auto q = search.getText().trim().toLowerCase();
    filtered.clear();
    const auto& all = proc.presets.all();
    for (int i = 0; i < (int) all.size(); ++i) {
        const auto& e = all[(size_t) i];
        if (category == "Favourites" && !proc.presets.isFavourite(e)) continue;
        if (category != "All" && category != "Favourites" && e.category != category) continue;
        if (q.isNotEmpty()) {
            const auto hay = (e.name + " " + e.category + " " + e.tags).toLowerCase();
            bool ok = true;
            for (auto& w : juce::StringArray::fromTokens(q, " ", "")) if (w.isNotEmpty() && !hay.contains(w)) ok = false;
            if (!ok) continue;
        }
        filtered.push_back(i);
    }
    suppress = true;
    list.updateContent(); list.repaint();
    selectCurrent();
    suppress = false;
    repaint();
}

void PresetBrowser::selectCurrent() {
    const int idx = proc.presets.indexOf(proc.currentPresetName, proc.currentPresetCategory);
    const bool was = suppress; suppress = true;
    list.deselectAllRows();
    for (int r = 0; r < (int) filtered.size(); ++r)
        if (filtered[(size_t) r] == idx) { list.selectRow(r); list.scrollToEnsureRowIsOnscreen(r); break; }
    suppress = was;
}

void PresetBrowser::load(int row) {
    if (row < 0 || row >= (int) filtered.size()) return;
    const auto& e = proc.presets.all()[(size_t) filtered[(size_t) row]];
    auto p = proc.presets.load(e);
    if (!e.factory) { p.category = "User"; }
    proc.applyPreset(p);
}

void PresetBrowser::step(int delta) {
    if (filtered.empty()) refresh();
    if (filtered.empty()) return;
    const int idx = proc.presets.indexOf(proc.currentPresetName, proc.currentPresetCategory);
    int pos = -1;
    for (int r = 0; r < (int) filtered.size(); ++r) if (filtered[(size_t) r] == idx) pos = r;
    const int n = (int) filtered.size();
    pos = pos < 0 ? (delta > 0 ? 0 : n - 1) : (pos + delta + n) % n;
    load(pos);
    selectCurrent();
}

void PresetBrowser::paint(juce::Graphics& g) {
    const auto& t = theme();
    card(g, getLocalBounds().toFloat().reduced(1.0f), 20.0f, true);
    auto sb = sidebar();
    g.setColour(t.well);
    g.fillRoundedRectangle(sb.toFloat().reduced(6), 14.0f);
    auto r = sb.reduced(14, 18);
    g.setColour(t.text); g.setFont(font(17.0f, W::Bold));
    g.drawText("Sounds", r.removeFromTop(28), juce::Justification::centredLeft);
    r.removeFromTop(8);
    const auto& all = proc.presets.all();
    auto countOf = [&](const juce::String& c) {
        int n = 0;
        for (auto& e : all) if (c == "All" || (c == "Favourites" ? proc.presets.isFavourite(e) : e.category == c)) ++n;
        return n;
    };
    juce::StringArray items { "All", "Favourites", "User" };
    items.addArray(cats);
    for (int i = 0; i < items.size(); ++i) {
        auto row = r.removeFromTop(i == 3 ? 34 : 25);
        if (i == 3) { g.setColour(t.cardEdge); g.fillRect(row.getX(), row.getY() + 4, row.getWidth(), 1); row = row.withTrimmedTop(9); }
        const bool sel = items[i] == category;
        if (sel) { g.setColour(t.accent); g.fillRoundedRectangle(row.toFloat().expanded(4, 0), 7.0f); }
        g.setColour(sel ? juce::Colours::white : t.text); g.setFont(font(13.0f, sel ? W::Semibold : W::Regular));
        g.drawText(items[i] == "All" ? juce::String("All Sounds") : items[i], row.reduced(6, 0), juce::Justification::centredLeft);
        g.setColour(sel ? juce::Colours::white.withAlpha(0.8f) : t.text3); g.setFont(font(11.5f));
        g.drawText(juce::String(countOf(items[i])), row.reduced(6, 0), juce::Justification::centredRight);
    }
    auto head = getLocalBounds().withTrimmedLeft(sb.getRight() + 10).reduced(16, 0).withHeight(52).withTrimmedTop(12);
    searchIcon(g, juce::Rectangle<float>((float) search.getX() - 26, (float) search.getY() + 6, 20, 20), t.text2);
    g.setColour(t.text3); g.setFont(font(12.0f));
    g.drawText(juce::String((int) filtered.size()) + " sounds", list.getBounds().withY(getHeight() - 30).withHeight(24), juce::Justification::centredLeft);
    (void) head;
}

void PresetBrowser::resized() {
    auto r = getLocalBounds().withTrimmedLeft(sidebar().getRight() + 10).reduced(16, 14);
    auto top = r.removeFromTop(32);
    close.setBounds(top.removeFromRight(80)); top.removeFromRight(12);
    top.removeFromLeft(28);
    search.setBounds(top);
    r.removeFromTop(10); r.removeFromBottom(26);
    list.setBounds(r);
}

void PresetBrowser::mouseDown(const juce::MouseEvent& e) {
    auto sb = sidebar();
    if (!sb.contains(e.getPosition())) return;
    juce::StringArray items { "All", "Favourites", "User" };
    items.addArray(cats);
    int y = sb.reduced(14, 18).getY() + 36;
    for (int i = 0; i < items.size(); ++i) {
        const int h = i == 3 ? 34 : 25;
        if (e.y >= y && e.y < y + h) { category = items[i]; refresh(); return; }
        y += h;
    }
}

bool PresetBrowser::keyPressed(const juce::KeyPress& k) {
    if (k == juce::KeyPress::escapeKey) { if (onClose) onClose(); return true; }
    if (k == juce::KeyPress::downKey || k == juce::KeyPress::upKey) {
        const int r = juce::jlimit(0, juce::jmax(0, (int) filtered.size() - 1), list.getSelectedRow() + (k == juce::KeyPress::downKey ? 1 : -1));
        list.selectRow(r); list.scrollToEnsureRowIsOnscreen(r); return true;
    }
    return false;
}

void PresetBrowser::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) {
    if (row < 0 || row >= (int) filtered.size()) return;
    const auto& t = theme();
    const auto& e = proc.presets.all()[(size_t) filtered[(size_t) row]];
    auto r = juce::Rectangle<int>(0, 0, w, h).reduced(2, 1);
    if (selected) { g.setColour(t.accent); g.fillRoundedRectangle(r.toFloat(), 8.0f); }
    else if (row % 2) { g.setColour(t.dark ? juce::Colour(0x08ffffff) : juce::Colour(0x06000000)); g.fillRoundedRectangle(r.toFloat(), 9.0f); }
    r.reduce(12, 0);
    const bool fav = proc.presets.isFavourite(e);
    heart(g, r.removeFromRight(18).toFloat().withSizeKeepingCentre(15, 13), selected ? juce::Colours::white : t.text3, fav);
    g.setColour(selected ? juce::Colours::white : t.text); g.setFont(font(13.5f, W::Medium));
    g.drawText(e.name, r.removeFromLeft(240), juce::Justification::centredLeft);
    g.setColour(selected ? juce::Colours::white.withAlpha(0.85f) : t.text2); g.setFont(font(12.5f));
    g.drawText(e.category, r.removeFromLeft(150), juce::Justification::centredLeft);
    g.setColour(selected ? juce::Colours::white.withAlpha(0.7f) : t.text3); g.setFont(font(12.0f));
    g.drawText(e.tags.replace(",", u8("  \xc2\xb7  ")), r.reduced(8, 0), juce::Justification::centredLeft);
}

void PresetBrowser::listBoxItemClicked(int row, const juce::MouseEvent& e) {
    if (row < 0 || row >= (int) filtered.size()) return;
    if (e.x > list.getVisibleRowWidth() - 44) {
        proc.presets.toggleFavourite(proc.presets.all()[(size_t) filtered[(size_t) row]]);
        if (category == "Favourites") refresh(); else list.repaintRow(row);
        repaint(sidebar());
    }
}

// ------------------------------------------------------------------ main content
Content::Content(NacreProcessor& p)
    : proc(p),
      tabs({ "Synth", "Modulation", "Effects", "Arp & Voice" }, [this](int i) { showPage(i); }),
      synth(p), mod(p), fx(p), arp(p), browser(p),
      prevBtn("Previous", [](auto& g, auto r, auto c) { chevron(g, r, c, true); }),
      nextBtn("Next", [](auto& g, auto r, auto c) { chevron(g, r, c, false); }),
      favBtn("Favourite", [](auto&, auto, auto) {}),
      undoBtn("Undo", [](auto& g, auto r, auto c) { undoIcon(g, r, c, false); }),
      redoBtn("Redo", [](auto& g, auto r, auto c) { undoIcon(g, r, c, true); }),
      themeBtn("Theme", [](auto& g, auto r, auto c) { themeIcon(g, r, c); }),
      browseBtn("Browse", [](auto& g, auto r, auto c) { searchIcon(g, r, c); }),
      volume(p, nacre::masterGain, "Volume", 34.0f), meter(p), keyboard(p.keyboardState) {
    applyTheme(proc.presets.settings().getBoolValue("dark", true));
    setLookAndFeel(&look);
    pages[0] = &synth; pages[1] = &mod; pages[2] = &fx; pages[3] = &arp;
    for (auto* pg : pages) addChildComponent(pg);
    addAndMakeVisible(tabs);
    const char* mnames[4] = { "Tone", "Motion", "Space", "Grit" };
    const char* mtips[4] = { "Tone: brightness", "Motion: movement from LFO 2", "Space: reverb and delay amount", "Grit: drive and saturation" };
    for (int i = 0; i < 4; ++i) {
        macros[i] = std::make_unique<Knob>(p, nacre::macro1 + i, mnames[i], 52.0f);
        macros[i]->slider.setTooltip(juce::String(mtips[i]) + "\nEvery factory sound is mapped. Double-click to reset.");
        addAndMakeVisible(*macros[i]);
    }
    favBtn.draw = [this](juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) {
        const int i = proc.presets.indexOf(proc.currentPresetName, proc.currentPresetCategory);
        heart(g, r.withSizeKeepingCentre(r.getWidth() * 1.05f, r.getHeight() * 0.95f), c, i >= 0 && proc.presets.isFavourite(proc.presets.all()[(size_t) i]));
    };
    for (juce::Component* c : std::initializer_list<juce::Component*>{ &presetBtn, &prevBtn, &nextBtn, &favBtn, &saveBtn, &undoBtn, &redoBtn, &themeBtn, &browseBtn, &volume, &meter, &keyboard })
        addAndMakeVisible(c);
    addChildComponent(browser);
    browser.onClose = [this] { browser.setVisible(false); tabs.setVisible(true); };

    presetBtn.onClick = [this] { const bool show = !browser.isVisible(); browser.setVisible(show); };
    browseBtn.onClick = presetBtn.onClick;
    prevBtn.onClick = [this] { stepPreset(-1); };
    nextBtn.onClick = [this] { stepPreset(1); };
    favBtn.onClick = [this] {
        const int i = proc.presets.indexOf(proc.currentPresetName, proc.currentPresetCategory);
        if (i >= 0) proc.presets.toggleFavourite(proc.presets.all()[(size_t) i]);
        favBtn.repaint(); browser.repaint();
    };
    saveBtn.onClick = [this] { savePreset(); };
    undoBtn.onClick = [this] { proc.undo.undo(); };
    redoBtn.onClick = [this] { proc.undo.redo(); };
    themeBtn.onClick = [this] { applyTheme(!theme().dark); proc.presets.settings().setValue("dark", theme().dark); proc.presets.settings().saveIfNeeded(); };
    presetBtn.setTooltip("Browse sounds");
    browseBtn.setTooltip("Browse sounds");
    prevBtn.setTooltip("Previous sound"); nextBtn.setTooltip("Next sound"); favBtn.setTooltip("Favourite");
    saveBtn.setTooltip("Save the current sound as a user preset");
    undoBtn.setTooltip("Undo"); redoBtn.setTooltip("Redo"); themeBtn.setTooltip("Light / dark appearance");
    proc.onPresetChanged = [this] { updatePresetLabel(); };
    tabs.setSelected(proc.presets.settings().getIntValue("page", 0));
    showPage(tabs.getSelected());
    updatePresetLabel();
    setWantsKeyboardFocus(true);
    startTimerHz(10);
}

Content::~Content() { proc.onPresetChanged = nullptr; setLookAndFeel(nullptr); }

void Content::applyTheme(bool dark) {
    theme() = dark ? Theme::darkTheme() : Theme::light();
    setAccent(accentFor(proc.currentPresetCategory, dark));
    backdropKey = 0;
    look.refresh();
    sendLookAndFeelChange();
    repaint();
}

void Content::showPage(int i) {
    for (int k = 0; k < 4; ++k) pages[k]->setVisible(k == i);
    browser.setVisible(false);
    proc.presets.settings().setValue("page", i);
}

void Content::stepPreset(int d) { browser.step(d); updatePresetLabel(); }

void Content::updatePresetLabel() {
    const bool catChanged = !shownPreset.startsWith(proc.currentPresetCategory + "/");
    shownPreset = proc.currentPresetCategory + "/" + proc.currentPresetName;
    if (catChanged) { setAccent(accentFor(proc.currentPresetCategory, theme().dark)); backdropKey = 0; look.refresh(); sendLookAndFeelChange(); repaint(); }
    presetBtn.name = proc.currentPresetName; presetBtn.category = proc.currentPresetCategory; presetBtn.repaint();
    favBtn.repaint();
    repaint(presetBtn.getBounds().expanded(4));
    if (browser.isVisible()) browser.selectCurrent();
}

void Content::savePreset() {
    saveDialog = std::make_unique<juce::AlertWindow>("Save Sound", "Your preset is saved as a text file in Documents/Nacre/Presets.", juce::MessageBoxIconType::NoIcon, this);
    saveDialog->addTextEditor("name", proc.currentPresetCategory == "User" ? proc.currentPresetName : proc.currentPresetName + " (Edit)", "Name");
    saveDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    saveDialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    saveDialog->enterModalState(true, juce::ModalCallbackFunction::create([this](int result) {
        if (result == 1 && saveDialog) {
            auto name = saveDialog->getTextEditorContents("name").trim();
            if (name.isNotEmpty()) {
                auto pr = proc.captureCurrent();
                pr.name = name.toStdString(); pr.category = "User"; pr.author = "User";
                proc.presets.saveUserPreset(pr);
                proc.currentPresetName = name; proc.currentPresetCategory = "User";
                updatePresetLabel(); browser.refresh();
            }
        }
        saveDialog.reset();
    }), false);
}

void Content::timerCallback() {
    undoBtn.setEnabled(proc.undo.canUndo());
    redoBtn.setEnabled(proc.undo.canRedo());
    if (shownPreset != proc.currentPresetCategory + "/" + proc.currentPresetName) updatePresetLabel();
}

bool Content::keyPressed(const juce::KeyPress& k) {
    const auto m = k.getModifiers();
    if (m.isCommandDown() && k.getKeyCode() == 'Z') { if (m.isShiftDown()) proc.undo.redo(); else proc.undo.undo(); return true; }
    if (m.isCommandDown() && k.getKeyCode() == 'F') { browser.setVisible(true); return true; }
    if (m.isCommandDown() && k.getKeyCode() == 'S') { savePreset(); return true; }
    return false;
}

void Content::paint(juce::Graphics& g) {
    const auto& t = theme();
    const juce::uint32 key = t.accent.getARGB() ^ (t.dark ? 0x5a5a5a5au : 0u);
    if (backdrop.isNull() || key != backdropKey) { backdrop = makeBackdrop(W / 2, H / 2); backdropKey = key; }
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(backdrop, getLocalBounds().toFloat());
    // brand: a pearl and an iridescent wordmark
    { const float d = 22.0f; juce::Rectangle<float> pr(24, 15, d, d);
      g.setColour(t.accent.withAlpha(0.35f)); g.fillEllipse(pr.expanded(4));
      g.setGradientFill(juce::ColourGradient(juce::Colours::white, pr.getX() + 5, pr.getY() + 4, t.accent, pr.getRight(), pr.getBottom(), true)); g.fillEllipse(pr);
      g.setGradientFill(juce::ColourGradient(t.accent2.withAlpha(0.0f), pr.getX(), pr.getY(), t.accent2.withAlpha(0.7f), pr.getRight(), pr.getBottom(), false)); g.drawEllipse(pr, 1.0f); }
    g.setGradientFill(juce::ColourGradient(t.text, 56, 14, t.dark ? t.accent2.brighter(0.4f) : t.accent.darker(0.2f), 130, 40, false));
    g.setFont(font(22.0f, W::Bold));
    g.drawText("Nacre", 56, 12, 120, 26, juce::Justification::centredLeft);
    g.setColour(t.text3); g.setFont(font(9.5f, W::Semibold));
    g.drawText("WAVETABLE SYNTHESIZER", 57, 38, 200, 14, juce::Justification::centredLeft);
    // footer glass
    card(g, juce::Rectangle<float>(16, 660, 400, 108));
    card(g, juce::Rectangle<float>(428, 660, 736, 108));
    g.setColour(t.text3); g.setFont(font(10.0f, W::Semibold));
    g.drawText("MACROS", 30, 666, 100, 14, juce::Justification::centredLeft);
}

void Content::resized() {
    // header
    const int cx = W / 2;
    prevBtn.setBounds(cx - 230, 14, 32, 34);
    presetBtn.setBounds(cx - 192, 12, 384, 38);
    nextBtn.setBounds(cx + 198, 14, 32, 34);
    favBtn.setBounds(cx + 236, 14, 32, 34);
    browseBtn.setBounds(cx - 268, 14, 32, 34);
    meter.setBounds(W - 30, 14, 12, 36);
    volume.setBounds(W - 84, 6, 48, 54);
    themeBtn.setBounds(W - 128, 15, 32, 32);
    saveBtn.setBounds(W - 196, 15, 60, 32);
    redoBtn.setBounds(W - 236, 15, 32, 32);
    undoBtn.setBounds(W - 272, 15, 32, 32);
    tabs.setBounds(cx - 240, 68, 480, 32);
    auto pageArea = juce::Rectangle<int>(16, 112, W - 32, 536);
    for (auto* pg : pages) pg->setBounds(pageArea);
    browser.setBounds(juce::Rectangle<int>(16, 64, W - 32, 584));
    // footer
    for (int i = 0; i < 4; ++i) macros[i]->setBounds(30 + i * 96, 682, 84, 78);
    keyboard.setBounds(440, 672, 712, 86);
}

} // namespace ui

// ------------------------------------------------------------------ editor shell (scales the fixed-size content)
NacreEditor::NacreEditor(NacreProcessor& p) : juce::AudioProcessorEditor(p), proc(p), content((nacre_diag::log("editor: building UI"), p)) {
    addAndMakeVisible(content);
    setResizable(true, true);
    const double ratio = (double) ui::Content::W / ui::Content::H;
    getConstrainer()->setFixedAspectRatio(ratio);
    setResizeLimits(ui::Content::W / 2, ui::Content::H / 2, ui::Content::W * 2, ui::Content::H * 2);
    double scale = p.presets.settings().getDoubleValue("scale", 0.0);
    if (scale <= 0.0) {
        scale = 1.0;
        if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            scale = juce::jlimit(0.6, 1.0, (d->userBounds.getHeight() - 140.0) / ui::Content::H);
    }
    setSize(juce::roundToInt(ui::Content::W * scale), juce::roundToInt(ui::Content::H * scale));
    nacre_diag::log("editor: ready, scale " + juce::String(scale, 2));
}

NacreEditor::~NacreEditor() {
    proc.presets.settings().setValue("scale", (double) getWidth() / ui::Content::W);
    proc.presets.settings().saveIfNeeded();
}

void NacreEditor::resized() {
    const float s = (float) getWidth() / (float) ui::Content::W;
    content.setBounds(0, 0, ui::Content::W, ui::Content::H);
    content.setTransform(juce::AffineTransform::scale(s));
}
