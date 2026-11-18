// Nacre — synthesis engine. Pure C++17, no framework dependency, so it can be
// unit-tested and rendered offline, and wrapped by any plug-in format.
#pragma once
#include <atomic>
#include <array>
#include "Params.h"
#include "Dsp.h"
#include "Wavetables.h"
#include "Effects.h"

namespace nacre {

constexpr int kMaxVoices = 40;      // logical polyphony <= 32, the rest lets stolen voices fade out
constexpr int kMaxUnison = 7;
constexpr int kCtrl = 8;            // host-rate samples per control tick
constexpr int kMaxOs = 2;

struct Env {
    enum St { Idle, Atk, Dec, Sus, Rel } st = Idle;
    float v = 0, aC = 1, dC = 0, rC = 0, s = 1;
    void set(float a, float d, float sus, float r, float rate) {
        aC = 1.0f - std::exp(-1.4663f / std::max(a * rate, 1.0f));   // RC curve aimed at 1.3, reaches 1.0 after `a`
        dC = std::exp(-6.9f / std::max(d * rate, 1.0f));
        rC = std::exp(-6.9f / std::max(r * rate, 1.0f));
        s = sus;
    }
    void on() { st = Atk; }
    void off() { if (st != Idle) st = Rel; }
    void kill() { st = Idle; v = 0; }
    inline float tick() {
        switch (st) {
            case Atk: v += (1.3f - v) * aC; if (v >= 1.0f) { v = 1.0f; st = Dec; } break;
            case Dec: v = s + (v - s) * dC; if (std::fabs(v - s) < 1e-4f) st = Sus; break;
            case Sus: v += (s - v) * 0.02f; break;
            case Rel: v *= rC; if (v < 2e-5f) { v = 0; st = Idle; } break;
            default: break;
        }
        return v;
    }
};

struct Lfo {
    float phase = 0, r0 = 0, r1 = 0;
    inline float value(int shape) const {
        switch (shape) {
            case 0: return std::sin(kTwoPi * phase);
            case 1: return phase < 0.5f ? 4 * phase - 1 : 3 - 4 * phase;
            case 2: return 2 * phase - 1;
            case 3: return 1 - 2 * phase;
            case 4: return phase < 0.5f ? 1.0f : -1.0f;
            case 5: return r1;
            default: return r0 + (r1 - r0) * (0.5f - 0.5f * std::cos(kPi * phase));
        }
    }
    inline void advance(float inc, Rng& rng) {
        phase += inc;
        if (phase >= 1.0f) { phase -= std::floor(phase); r0 = r1; r1 = rng.bi(); }
    }
};

struct FilterUnit {
    int type = -1;
    Svf a[2], b[2], v[2][3];
    float lad[2][4] = {}, combLp[2] = {};
    DelayLine comb[2];
    SvfC c1, c2, vc[3];
    float vg[3] = {}, ladG = 0, ladK = 0, combLen = 100, combFb = 0, drive = 1, driveComp = 1;
    void prepare(float maxRate) { comb[0].resize((int) (maxRate / 30.0f)); comb[1].resize((int) (maxRate / 30.0f)); }
    void reset() { for (int c = 0; c < 2; ++c) { a[c].reset(); b[c].reset(); for (auto& s : v[c]) s.reset(); comb[c].clear(); combLp[c] = 0; for (auto& s : lad[c]) s = 0; } }
    void setup(int t, float fc, float res, float drv, float rate);
    inline float tick(float x, int ch);
};

struct Voice {
    bool active = false, gate = false, sustained = false, fading = false;
    int note = 60; float vel = 1, pressure = 0;
    uint64_t age = 0;
    float fade = 1, fadeStep = 0;
    float pitch = 60, target = 60;
    Env amp, fenv, menv;
    Lfo lfo[3]; float time = 0;
    float rnd = 0, drift = 0, driftTarget = 0, analog[2][kMaxUnison] = {}, cutDrift = 0;
    float ph[2][kMaxUnison] = {}, subPh = 0, noiseLp = 0;
    FilterUnit f1, f2;
    float ampCur = 0, panL = 1, panR = 1;
    float src[kNumSrc] = {}, mod[kNumDst] = {};
};

class Engine {
public:
    Engine();
    void prepare(double sampleRate, int maxBlock);
    void reset();

    // All calls below must come from the audio thread.
    float params[kNumParams];
    void setParam(int id, float v) { if (id >= 0 && id < kNumParams) params[id] = snapValue(kParams[id], v); }
    void setDefaults();
    void setTransport(bool playing, double bpm, double ppq);
    void noteOn(int note, float velocity);
    void noteOff(int note);
    void controlChange(int cc, float value01);
    void pitchBend(float v) { bend = clampf(v, -1, 1); }
    void channelPressure(float v) { chanAT = clampf(v, 0, 1); }
    void polyPressure(int note, float v);
    void allNotesOff();
    void allSoundOff();
    void requestSoftReset() { softResetPending = true; }   // fade out, clear tails, fade back in (used on preset change)
    bool softResetBusy() const { return softResetPending; }
    void snapSmoothing() { std::copy(params, params + kNumParams, sp); masterCur = dbToGain(params[masterGain]) * expression; }
    void process(float* L, float* R, int n);

    // Feedback for the user interface (written by the audio thread, read by the UI).
    std::atomic<float> uiPeakL{0}, uiPeakR{0}, uiCompGr{0}, uiCutoff{1000}, uiPosA{0}, uiPosB{0};
    std::atomic<float> uiLfo[3] = {{0}, {0}, {0}};
    std::atomic<int> uiVoices{0};

    double getSampleRate() const { return sr; }

private:
    void controlTick(int n);
    void renderVoice(Voice& v, int nos);
    void computeVoiceMods(Voice& v);
    void voiceOn(int note, float vel);
    void voiceOff(int note);
    void releaseVoice(Voice& v);
    void startVoice(Voice& v, int note, float vel, bool fresh);
    Voice* newestVoice();
    void arpTick();
    void arpStop();
    void killEverything();

    double sr = 44100, bpm = 120, ppq = 0;
    bool hostPlaying = false;
    int os = 2, maxBlock = 512;
    float vsr = 88200;                       // voice (oversampled) rate
    float sp[kNumParams];                    // smoothed parameters
    std::vector<float> busL, busR;
    Halfband down[2];
    Distortion dist; Phaser phaser; Chorus chorus; Delay delay; Reverb reverb; Eq3 eq; Compressor comp; Limiter lim;
    Voice* k_uiNewest = nullptr;
    Voice voices[kMaxVoices];
    uint64_t ageCounter = 0;
    Rng rng;
    Lfo glfo[3]; float glfoVal[3] = {};
    float gsrc[kNumSrc] = {}, gmod[kNumDst] = {};
    float bend = 0, wheel = 0, chanAT = 0, expression = 1;
    bool pedal = false, keyDown[128] = {};
    int stack[128]; int stackN = 0;           // held keys, oldest first (mono / legato)
    float lastPitch = -1;
    float masterCur = 1, resetGain = 1;
    bool softResetPending = false; int softResetStage = 0;
    float peakL = 0, peakR = 0, dcX[2] = {0, 0}, dcY[2] = {0, 0}, dcR = 0.9993f;

    // arpeggiator
    struct ArpNote { int note; float vel; };
    ArpNote arpHeld[64]; int arpN = 0;
    bool arpLatchArmed = false, arpRunning = false, arpWasOn = false;
    int arpPlaying[64]; int arpPlayingN = 0;
    long arpGrid = 0; int arpStep = 0, arpDivCur = -1;
    double arpOrigin = 0, arpNext = 0, arpOff = 0, freeBeat = 0;
};

} // namespace nacre
