#pragma once
#include <JuceHeader.h>
#include "Preset.h"

// Factory presets (compiled in) + user presets (Documents/Nacre/Presets/*.nacre)
// + favourites (stored in the user settings file, shared by every instance).
class PresetManager {
public:
    struct Entry {
        juce::String name, category, tags;
        bool factory = true;
        juce::File file;          // user presets only
        int factoryIndex = -1;
    };

    PresetManager();
    void rescan();
    const std::vector<Entry>& all() const { return entries; }
    juce::StringArray categories() const;
    nacre::Preset load(const Entry& e) const;

    bool isFavourite(const Entry& e) const;
    void toggleFavourite(const Entry& e);

    juce::File userFolder() const;
    juce::File saveUserPreset(const nacre::Preset& p);   // returns the file written

    juce::PropertiesFile& settings() { return *props; }
    int indexOf(const juce::String& name, const juce::String& category) const;

private:
    juce::String key(const Entry& e) const { return (e.factory ? "F:" : "U:") + e.category + "/" + e.name; }
    std::vector<Entry> entries;
    std::unique_ptr<juce::PropertiesFile> props;
    juce::StringArray favourites;
};
