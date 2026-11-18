// Tiny load diagnostics: appends to %TEMP%/Nacre-log.txt (Windows) or $TMPDIR/Nacre-log.txt so a host-side
// load problem can be pinpointed. Capped in size; never throws.
#pragma once
#include <JuceHeader.h>
namespace nacre_diag {
inline void log(const juce::String& m) {
    try {
        static juce::CriticalSection cs; static int lines = 0;
        const juce::ScopedLock sl(cs);
        auto f = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Nacre-log.txt");
        if (lines == 0 && f.getSize() > 200000) f.deleteFile();
        if (lines++ > 300) return;
        if (lines == 1) f.appendText("\n==== Nacre " + juce::String(JucePlugin_VersionString) + " | " + juce::SystemStats::getOperatingSystemName()
            + " | " + juce::SystemStats::getCpuModel() + " | JUCE " + juce::String(JUCE_MAJOR_VERSION) + "." + juce::String(JUCE_MINOR_VERSION) + "\n");
        f.appendText(juce::Time::getCurrentTime().formatted("%H:%M:%S  ") + m + "\n");
    } catch (...) {}
}
}
