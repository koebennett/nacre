// Nacre — effects rack. All units are stereo, allocation-free in process(),
// and safe with any block size or sample rate.
#pragma once
#include "Dsp.h"

namespace nacre {

// ------------------------------------------------------------- distortion
// Runs inside the oversampled domain (before the decimator) so drive does not alias.
struct Distortion {
    float sr = 88200, held[2] = {0, 0}, lp[2] = {0, 0}, dc[2][2] = {{0, 0}, {0, 0}};
    int count = 0;
    void prepare(float sampleRate) { sr = sampleRate; reset(); }
    void reset() { held[0] = held[1] = lp[0] = lp[1] = 0; dc[0][0] = dc[0][1] = dc[1][0] = dc[1][1] = 0; count = 0; }
    void process(float* L, float* R, int n, int type, float drive, float tone, float mix) {
        drive = clampf(drive, 0, 1); mix = clampf(mix, 0, 1);
        const float gain = dbToGain(drive * 36.0f);
        const float comp = type <= 3 ? 1.0f / std::sqrt(gain) * (type == 1 ? 0.8f : 1.0f) : 1.0f;
        const float fc = 800.0f * std::pow(25.0f, clampf(tone, 0, 1));
        const float a = 1.0f - std::exp(-kTwoPi * fc / sr);
        const float bits = 16.0f - drive * 13.0f, q = std::exp2(bits - 1.0f);
        const int hold = 1 + (int) (drive * drive * 48.0f);
        float* io[2] = {L, R};
        for (int i = 0; i < n; ++i) {
            const bool take = (count++ % hold) == 0;
            for (int c = 0; c < 2; ++c) {
                const float x = io[c][i];
                float y;
                switch (type) {
                    case 0: y = fastTanh(x * gain) * comp; break;
                    case 1: y = clampf(x * gain, -1.0f, 1.0f) * comp; break;
                    case 2: { float v = x * gain + 0.2f; y = (v > 0 ? fastTanh(v) : v / (1.0f - v * 0.6f) * 0.9f) - 0.197f;
                              dc[c][0] += 0.0005f * (y - dc[c][0]); y = (y - dc[c][0]) * comp; break; }
                    case 3: y = std::sin(x * gain * 1.5707963f) * comp; break;
                    case 4: y = std::round(x * q) / q; break;
                    default: if (take) held[c] = x; y = held[c]; break;
                }
                lp[c] += a * (y - lp[c]);
                io[c][i] = x + (lp[c] - x) * mix;
            }
        }
    }
};

// ------------------------------------------------------------- phaser
struct Phaser {
    float sr = 44100, phase = 0, z[2][6] = {}, fb[2] = {0, 0};
    void prepare(float s) { sr = s; reset(); }
    void reset() { std::memset(z, 0, sizeof z); fb[0] = fb[1] = 0; }
    void process(float* L, float* R, int n, float rate, float depth, float feedback, float mix) {
        float* io[2] = {L, R};
        const float inc = rate / sr;
        for (int i = 0; i < n; ++i) {
            phase += inc; if (phase >= 1) phase -= 1;
            for (int c = 0; c < 2; ++c) {
                float lfo = 0.5f + 0.5f * std::sin(kTwoPi * (phase + (c ? 0.25f : 0.0f)));
                float f = 180.0f * std::pow(28.0f, lfo * depth + (1 - depth) * 0.3f);
                float t = std::tan(kPi * std::min(f / sr, 0.45f)); float a = (t - 1) / (t + 1);
                float x = io[c][i] + fb[c] * feedback;
                for (int s = 0; s < 6; ++s) { float y = a * x + z[c][s]; z[c][s] = x - a * y; x = y; }
                fb[c] = fastTanh(x);
                io[c][i] = io[c][i] * (1 - mix * 0.5f) + x * mix * 0.5f;
            }
        }
    }
};

// ------------------------------------------------------------- chorus
struct Chorus {
    float sr = 44100, phase = 0;
    DelayLine d[2];
    void prepare(float s) { sr = s; d[0].resize((int) (s * 0.05f)); d[1].resize((int) (s * 0.05f)); phase = 0; }
    void reset() { d[0].clear(); d[1].clear(); }
    void process(float* L, float* R, int n, float rate, float depth, float mix) {
        float* io[2] = {L, R};
        const float inc = rate / sr, base = 0.008f * sr, dep = 0.0055f * sr * depth;
        for (int i = 0; i < n; ++i) {
            phase += inc; if (phase >= 1) phase -= 1;
            float in[2] = {io[0][i], io[1][i]};
            d[0].write(in[0]); d[1].write(in[1]);
            for (int c = 0; c < 2; ++c) {
                float w = 0;
                for (int v = 0; v < 3; ++v) {
                    float ph = phase + v / 3.0f + (c ? 0.5f : 0.0f);
                    w += d[c].readFrac(base + dep * (0.5f + 0.5f * std::sin(kTwoPi * ph)));
                }
                io[c][i] = in[c] * (1 - mix * 0.5f) + w * 0.4f * mix;
            }
        }
    }
};

// ------------------------------------------------------------- delay
struct Delay {
    float sr = 44100, time[2] = {0, 0}, lp[2] = {0, 0}, hp[2] = {0, 0};
    DelayLine d[2];
    bool first = true;
    void prepare(float s) { sr = s; d[0].resize((int) (s * 4.1f)); d[1].resize((int) (s * 4.1f)); first = true; reset(); }
    void reset() { d[0].clear(); d[1].clear(); lp[0] = lp[1] = hp[0] = hp[1] = 0; }
    void process(float* L, float* R, int n, float seconds, float feedback, float tone, bool ping, float mix) {
        float* io[2] = {L, R};
        const float target = clampf(seconds * sr, 1.0f, sr * 4.0f);
        if (first) { time[0] = time[1] = target; first = false; }
        const float sm = 1.0f - std::exp(-1.0f / (0.08f * sr));
        const float a = 1.0f - std::exp(-kTwoPi * (1000.0f * std::pow(18.0f, tone)) / sr);
        const float ah = 1.0f - std::exp(-kTwoPi * 90.0f / sr);
        const float dry = std::min(1.0f, 2.0f * (1.0f - mix)), wet = std::min(1.0f, 2.0f * mix);
        for (int i = 0; i < n; ++i) {
            float o[2];
            for (int c = 0; c < 2; ++c) {
                // ping-pong: the right line runs at the same time but is fed from the left
                time[c] += sm * (target - time[c]);
                float y = d[c].readFrac(time[c]);
                lp[c] += a * (y - lp[c]); hp[c] += ah * (lp[c] - hp[c]);
                o[c] = lp[c] - hp[c];
            }
            float in0 = io[0][i], in1 = io[1][i];
            if (ping) {
                d[0].write(fastTanh(0.5f * (in0 + in1) + o[1] * feedback));
                d[1].write(fastTanh(o[0] * feedback));
            } else {
                d[0].write(fastTanh(in0 + o[0] * feedback));
                d[1].write(fastTanh(in1 + o[1] * feedback));
            }
            io[0][i] = in0 * dry + o[0] * wet;
            io[1][i] = in1 * dry + o[1] * wet;
        }
    }
};

// ------------------------------------------------------------- reverb (8-line FDN)
struct Reverb {
    static constexpr int K = 8;
    float sr = 44100, size = -1, lfo = 0;
    DelayLine line[K], pre[2], ap[2][4];
    float damp[K] = {}, gk[K] = {};
    void prepare(float s) {
        sr = s;
        for (auto& l : line) l.resize((int) (s * 0.16f));
        pre[0].resize((int) (s * 0.21f)); pre[1].resize((int) (s * 0.21f));
        for (auto& c : ap) for (auto& a : c) a.resize((int) (s * 0.02f));
        size = -1; reset();
    }
    void reset() { for (auto& l : line) l.clear(); pre[0].clear(); pre[1].clear(); for (auto& c : ap) for (auto& a : c) a.clear(); std::memset(damp, 0, sizeof damp); }
    void process(float* L, float* R, int n, float sizeP, float decay, float dampP, float preMs, float mix) {
        static const float base[K] = {0.0297f, 0.0371f, 0.0411f, 0.0437f, 0.0533f, 0.0599f, 0.0677f, 0.0731f};
        static const float apT[4] = {0.0047f, 0.0036f, 0.0127f, 0.0093f};
        if (size < 0) size = sizeP;
        const float sm = 1.0f - std::exp(-1.0f / (0.3f * sr));
        const float dA = 1.0f - std::exp(-kTwoPi * (14000.0f * std::pow(0.08f, clampf(dampP, 0, 1))) / sr);
        const float dry = std::min(1.0f, 2.0f * (1.0f - mix)), wet = std::min(1.0f, 2.0f * mix) * 0.6f;
        const int pd = (int) (clampf(preMs, 0, 200) * 0.001f * sr) + 1;
        for (int i = 0; i < n; ++i) {
            size += sm * (sizeP - size);
            const float scale = 0.25f + 1.75f * size;
            lfo += 0.7f / sr; if (lfo >= 1) lfo -= 1;
            float in[2] = {L[i], R[i]};
            for (int c = 0; c < 2; ++c) {
                pre[c].write(in[c]);
                float x = pre[c].read(pd);
                for (int a = 0; a < 4; ++a) {          // input diffusion
                    int dl = (int) (apT[a] * sr * (c ? 1.07f : 1.0f));
                    float dv = ap[c][a].read(dl), v = x + 0.6f * dv;
                    ap[c][a].write(v); x = dv - 0.6f * v;
                }
                in[c] = x;
            }
            if ((i & 15) == 0)
                for (int k = 0; k < K; ++k) gk[k] = std::pow(10.0f, -3.0f * base[k] * scale / std::max(decay, 0.05f));
            float y[K], sum = 0;
            for (int k = 0; k < K; ++k) {
                float len = base[k] * scale * sr + 8.0f * std::sin(kTwoPi * (lfo + k * 0.125f)) * (sr / 44100.0f);
                float v = line[k].readFrac(len) * gk[k];
                damp[k] += dA * (v - damp[k]);
                y[k] = damp[k]; sum += y[k];
            }
            sum *= 2.0f / K;
            float oL = 0, oR = 0;
            for (int k = 0; k < K; ++k) {
                float fbv = y[k] - sum + ((k & 1) ? in[1] : in[0]) * 0.5f;
                line[k].write(fbv);
                if (k < 4) { oL += y[k] * ((k & 1) ? -1.0f : 1.0f); oR += y[k + 4] * ((k & 2) ? -1.0f : 1.0f); }
            }
            L[i] = L[i] * dry + oL * wet;
            R[i] = R[i] * dry + oR * wet;
        }
    }
};

// ------------------------------------------------------------- EQ + compressor + limiter
struct Eq3 {
    float sr = 44100, cache[4] = {1e9f, 1e9f, 1e9f, 1e9f};
    Biquad lo, mid, hi;
    void prepare(float s) { sr = s; cache[0] = 1e9f; lo.reset(); mid.reset(); hi.reset(); }
    void reset() { lo.reset(); mid.reset(); hi.reset(); }
    void process(float* L, float* R, int n, float l, float m, float mf, float h) {
        if (l != cache[0] || m != cache[1] || mf != cache[2] || h != cache[3]) {
            lo.lowShelf(120, l, sr); mid.peak(std::min(mf, sr * 0.45f), m, 0.9f, sr); hi.highShelf(std::min(8000.0f, sr * 0.4f), h, sr);
            cache[0] = l; cache[1] = m; cache[2] = mf; cache[3] = h;
        }
        for (int i = 0; i < n; ++i) {
            L[i] = hi.tick(mid.tick(lo.tick(L[i], 0), 0), 0);
            R[i] = hi.tick(mid.tick(lo.tick(R[i], 1), 1), 1);
        }
    }
};

struct Compressor {
    float sr = 44100, env = 0, gr = 0;   // gr in dB, for the UI
    void prepare(float s) { sr = s; env = 0; }
    void reset() { env = 0; }
    void process(float* L, float* R, int n, float thr, float ratio, float atkMs, float relMs, float makeup) {
        const float at = std::exp(-1.0f / (atkMs * 0.001f * sr)), rt = std::exp(-1.0f / (relMs * 0.001f * sr));
        const float mk = dbToGain(makeup), knee = 6.0f;
        float maxGr = 0;
        for (int i = 0; i < n; ++i) {
            float lvl = gainToDb(std::max(std::fabs(L[i]), std::fabs(R[i])));
            float over = lvl - thr, red;
            if (over <= -knee / 2) red = 0;
            else if (over >= knee / 2) red = over * (1 - 1 / ratio);
            else { float o = over + knee / 2; red = (1 - 1 / ratio) * o * o / (2 * knee); }
            env = red > env ? at * env + (1 - at) * red : rt * env + (1 - rt) * red;
            float g = dbToGain(-env) * mk;
            L[i] *= g; R[i] *= g; maxGr = std::max(maxGr, env);
        }
        gr = maxGr;
    }
};

// Zero-latency safety limiter: instant attack, smooth release, final soft ceiling.
struct Limiter {
    float sr = 44100, g = 1;
    void prepare(float s) { sr = s; g = 1; }
    void process(float* L, float* R, int n) {
        const float ceiling = 0.966f;   // -0.3 dBFS
        const float rel = 1.0f - std::exp(-1.0f / (0.08f * sr));
        for (int i = 0; i < n; ++i) {
            float pk = std::max(std::fabs(L[i]), std::fabs(R[i]));
            float want = pk * g > ceiling ? ceiling / pk : 1.0f;
            g = want < g ? want : g + rel * (1.0f - g);
            L[i] = clampf(L[i] * g, -ceiling, ceiling);
            R[i] = clampf(R[i] * g, -ceiling, ceiling);
        }
    }
};

} // namespace nacre
