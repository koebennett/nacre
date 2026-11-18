// Nacre — band-limited, mip-mapped wavetable bank.
// Every table is generated procedurally at start-up (no sample files to ship),
// stored as 32 frames, each with one mip level per octave so that no level
// ever contains harmonics that could alias at the oscillator's current pitch.
#pragma once
#include <vector>
#include <atomic>
#include <mutex>
#include "Dsp.h"

namespace nacre {

constexpr int kWtFrames = 32;
constexpr int kWtLevels = 9;          // 512, 256, ... 2 harmonics
constexpr int kWtMaxHarm = 512;
constexpr int kNumTables = 43;        // must match NACRE_WT_NAMES

inline int levelHarmonics(int l) { return kWtMaxHarm >> l; }
// Table length per level. Low levels (used for low notes, whose top harmonics are tiny) are
// 4x oversampled; higher levels get more points per harmonic so that the images created by
// linear interpolation stay below about -70 dB.
inline int levelSize(int l) { static const int s[kWtLevels] = {2048, 1024, 1024, 512, 512, 256, 256, 256, 256}; return s[l]; }

struct Wavetable {
    // data[level] = kWtFrames * (size + 1) samples; last sample of each frame duplicates the first.
    std::vector<float> data[kWtLevels];
    // A 128-point preview per frame for drawing in the UI.
    std::vector<float> preview;

    // pos 0..1 across frames, phase 0..1, lvl fractional mip level.
    inline float lookup(float pos, float phase, int lvl) const {
        const int size = levelSize(lvl);
        const float fp = clampf(pos, 0.0f, 1.0f) * (float) (kWtFrames - 1);
        const int f0 = std::min((int) fp, kWtFrames - 2);
        const float ft = fp - (float) f0;
        const float x = phase * (float) size;
        int i = (int) x; const float fr = x - (float) i;
        i &= (size - 1);
        const float* a = data[lvl].data() + (size_t) f0 * (size_t) (size + 1) + i;
        const float* b = a + (size + 1);
        const float va = a[0] + (a[1] - a[0]) * fr;
        const float vb = b[0] + (b[1] - b[0]) * fr;
        return va + (vb - va) * ft;
    }
};

class WavetableBank {
public:
    static WavetableBank& get();          // built once per process, shared by all instances
    const Wavetable& table(int i) const { return tables[(size_t) std::clamp(i, 0, kNumTables - 1)]; }
private:
    WavetableBank();
    std::vector<Wavetable> tables;
};

} // namespace nacre
