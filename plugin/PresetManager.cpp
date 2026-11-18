#include "PresetManager.h"

static const char* kCategoryOrder[] = { "Pads", "Atmospheres", "Keys", "Bass", "Leads", "Plucks", "Bells & Mallets", "Strings",
                                        "Brass & Winds", "Organs", "Choirs & Vox", "Arps & Sequences", "Chords & Stabs", "Drones",
                                        "FX & Risers", "Percussion" };

PresetManager::PresetManager() {
    juce::PropertiesFile::Options o;
    o.applicationName = "Nacre";
    o.filenameSuffix = ".settings";
    o.folderName = "Nacre";
    o.osxLibrarySubFolder = "Application Support";
    o.processLock = nullptr;
    props = std::make_unique<juce::PropertiesFile>(o);
    favourites.addTokens(props->getValue("favourites"), "\n", "");
    favourites.removeEmptyStrings();
    rescan();
}

juce::File PresetManager::userFolder() const {
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Nacre").getChildFile("Presets");
}

void PresetManager::rescan() {
    entries.clear();
    int n = 0;
    const nacre::FactoryPreset* f = nacre::factoryPresets(n);
    for (int i = 0; i < n; ++i) entries.push_back({ f[i].name, f[i].category, f[i].tags, true, {}, i });
    auto dir = userFolder();
    if (dir.isDirectory()) {
        auto files = dir.findChildFiles(juce::File::findFiles, true, "*.nacre");
        files.sort();
        for (auto& file : files) {
            auto p = nacre::Preset::parse(file.loadFileAsString().toStdString());
            Entry e { juce::String(p.name), "User", juce::String(p.tags), false, file, -1 };
            if (e.name.isEmpty()) e.name = file.getFileNameWithoutExtension();
            entries.push_back(e);
        }
    }
}

juce::StringArray PresetManager::categories() const {
    juce::StringArray c;
    for (auto* s : kCategoryOrder) c.add(s);
    for (auto& e : entries) if (!c.contains(e.category) && e.category != "User") c.add(e.category);
    return c;
}

nacre::Preset PresetManager::load(const Entry& e) const {
    if (e.factory) {
        int n = 0; const nacre::FactoryPreset* f = nacre::factoryPresets(n);
        if (e.factoryIndex >= 0 && e.factoryIndex < n) return nacre::Preset::parse(f[e.factoryIndex].data);
        return {};
    }
    auto p = nacre::Preset::parse(e.file.loadFileAsString().toStdString());
    p.category = "User";
    return p;
}

bool PresetManager::isFavourite(const Entry& e) const { return favourites.contains(key(e)); }

void PresetManager::toggleFavourite(const Entry& e) {
    if (isFavourite(e)) favourites.removeString(key(e)); else favourites.add(key(e));
    props->setValue("favourites", favourites.joinIntoString("\n"));
    props->saveIfNeeded();
}

juce::File PresetManager::saveUserPreset(const nacre::Preset& p) {
    auto dir = userFolder();
    dir.createDirectory();
    auto safe = juce::File::createLegalFileName(juce::String(p.name)).trim();
    if (safe.isEmpty()) safe = "Untitled";
    auto file = dir.getChildFile(safe + ".nacre");
    file.replaceWithText(juce::String(p.serialize()));
    rescan();
    return file;
}

int PresetManager::indexOf(const juce::String& name, const juce::String& category) const {
    for (size_t i = 0; i < entries.size(); ++i) if (entries[i].name == name && entries[i].category == category) return (int) i;
    return -1;
}
