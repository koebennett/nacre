// Nacre — small DSP building blocks. No dependencies beyond the standard library.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <complex>
#include <vector>
#include <algorithm>
#include "Params.h"

namespace nacre {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;
constexpr double kPiD = 3.14159265358979323846;

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float fastTanh(float x) { x = clampf(x, -3.0f, 3.0f); float x2 = x * x; return x * (27.0f + x2) / (27.0f + 9.0f * x2); }
inline float noteToHz(float n) { return 440.0f * std::exp2((n - 69.0f) * (1.0f / 12.0f)); }
inline float dbToGain(float d) { return std::pow(10.0f, d * 0.05f); }
inline float gainToDb(float g) { return 20.0f * std::log10(std::max(g, 1e-9f)); }
inline float hashf(uint32_t x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return (float) x * (2.0f / 4294967296.0f) - 1.0f; }
inline int nextPow2(int n) { int p = 1; while (p < n) p <<= 1; return p; }

struct Rng {
    uint32_t s = 0x9E3779B9u;
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
    float bi() { return uni() * 2.0f - 1.0f; }
};

inline void fft(std::vector<std::complex<double>>& a, bool inverse) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = 2.0 * kPiD / (double) len * (inverse ? 1.0 : -1.0);
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (size_t j = 0; j < len / 2; ++j) {
                auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl;
            }
        }
    }
}

// ---------------------------------------------------------------- filters
struct SvfC {
    float a1 = 0, a2 = 0, a3 = 0, k = 1;
    void set(float g, float k_) { k = k_; a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2; }
};
inline float tanG(float fc, float sr) { return std::tan(kPi * clampf(fc / sr, 0.0002f, 0.49f)); }
inline float resToK(float res) { return 1.4142f * std::pow(0.0125f, clampf(res, 0.0f, 1.0f)); } // Q 0.7 .. 56

// Zero-delay-feedback state variable filter (Simper / Zavalishin form).
struct Svf {
    float ic1 = 0, ic2 = 0;
    inline void tick(float x, const SvfC& c, float& lp, float& bp) {
        float v3 = x - ic2;
        float v1 = c.a1 * ic1 + c.a2 * v3;
        float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1;
    }
    void reset() { ic1 = ic2 = 0; }
};

struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] = {0, 0}, z2[2] = {0, 0};
    inline float tick(float x, int ch) {
        float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    void norm(float B0, float B1, float B2, float A0, float A1, float A2) { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; }
    void lowShelf(float f, float dB, float sr) {
        float A = std::pow(10.0f, dB / 40.0f), w = kTwoPi * f / sr, c = std::cos(w), s = std::sin(w), al = s / 2 * 1.4142f, q = 2 * std::sqrt(A) * al;
        norm(A * ((A + 1) - (A - 1) * c + q), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - q), (A + 1) + (A - 1) * c + q, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - q);
    }
    void highShelf(float f, float dB, float sr) {
        float A = std::pow(10.0f, dB / 40.0f), w = kTwoPi * f / sr, c = std::cos(w), s = std::sin(w), al = s / 2 * 1.4142f, q = 2 * std::sqrt(A) * al;
        norm(A * ((A + 1) + (A - 1) * c + q), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - q), (A + 1) - (A - 1) * c + q, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - q);
    }
    void peak(float f, float dB, float Q, float sr) {
        float A = std::pow(10.0f, dB / 40.0f), w = kTwoPi * f / sr, c = std::cos(w), al = std::sin(w) / (2 * Q);
        norm(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
};

// 2x polyphase IIR half-band (two all-pass chains). Near-zero latency, >90 dB
// stop-band; used to run the voices and the distortion at twice the host rate.
struct Halfband {
    static constexpr int K = 6;
    float ax[K] = {}, ay[K] = {}, bx[K] = {}, by[K] = {};
    float held = 0;
    void reset() { std::memset(ax, 0, sizeof ax); std::memset(ay, 0, sizeof ay); std::memset(bx, 0, sizeof bx); std::memset(by, 0, sizeof by); held = 0; }
    static inline float chain(float x, const float* c, float* xs, float* ys) {
        for (int i = 0; i < K; ++i) { float y = c[i] * (x - ys[i]) + xs[i]; xs[i] = x; ys[i] = y; x = y; }
        return x;
    }
    static const float* ca() { static const float c[K] = {0.036681502163648017f, 0.2746317593794541f, 0.56109896978791948f, 0.769741833862266f, 0.8922608180038789f, 0.962094548378084f}; return c; }
    static const float* cb() { static const float c[K] = {0.13654762463195771f, 0.42313861743656667f, 0.6775400499741616f, 0.839889624849638f, 0.9315419599631839f, 0.9878163707328971f}; return c; }
    inline float down(float x0, float x1) { float y = 0.5f * (chain(x0, ca(), ax, ay) + held); held = chain(x1, cb(), bx, by); return y; }
    inline void up(float x, float& y0, float& y1) { y0 = chain(x, ca(), ax, ay); y1 = chain(x, cb(), bx, by); }
};

struct DelayLine {
    std::vector<float> buf; int mask = 0, w = 0;
    void resize(int minLen) { int n = nextPow2(minLen + 4); buf.assign((size_t) n, 0.0f); mask = n - 1; w = 0; }
    void clear() { std::fill(buf.begin(), buf.end(), 0.0f); }
    inline void write(float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    inline float read(int d) const { return buf[(size_t) ((w - d) & mask)]; }
    inline float readFrac(float d) const {
        int i = (int) d; float f = d - (float) i;
        float a = buf[(size_t) ((w - i) & mask)], b = buf[(size_t) ((w - i - 1) & mask)];
        return a + (b - a) * f;
    }
};

// Magnitude (dB) of the filter models, for drawing the response curve in the UI.
inline float filterResponseDb(int type, float fc, float res, float f) {
    using C = std::complex<float>;
    const C s(0.0f, f / fc);
    const float k = resToK(res);
    auto den = [&](float kk) { return s * s + kk * s + 1.0f; };
    C h(1.0f, 0.0f);
    switch (type) {
        case fLP12: h = 1.0f / den(k); break;
        case fLP24: h = 1.0f / (den(1.4142f) * den(k)); break;
        case fLadder: { C g = 1.0f / (1.0f + s); C g4 = g * g * g * g; float kk = 3.9f * res; h = g4 * (1.0f + 0.5f * kk) / (1.0f + kk * g4); break; }
        case fHP12: h = s * s / den(k); break;
        case fHP24: h = s * s * s * s / (den(1.4142f) * den(k)); break;
        case fBand: h = s * std::sqrt(k) * 1.2f / den(k); break;
        case fNotch: h = (s * s + 1.0f) / den(k); break;
        case fComb: { float fb = 0.3f + 0.68f * res; h = (1.0f - fb * 0.5f) / (1.0f - fb * std::exp(C(0.0f, -kTwoPi * f / fc))); break; }
        case fVowel: {
            static const float F[5][3] = {{800,1150,2900},{400,1600,2700},{270,2140,2950},{450,800,2830},{325,700,2530}};
            float p = clampf(std::log(fc / 20.0f) / std::log(1000.0f), 0.0f, 1.0f) * 4.0f; int i = std::min((int) p, 3); float t = p - (float) i;
            const float g[3] = {1.0f, 0.6f, 0.35f}; float kq = 1.0f / (4.0f + res * 16.0f); h = 0;
            for (int j = 0; j < 3; ++j) { C sj(0.0f, f / lerpf(F[i][j], F[i + 1][j], t)); h += g[j] * 2.0f * sj * kq / (sj * sj + kq * sj + 1.0f); }
            break;
        }
        default: break;
    }
    return gainToDb(std::abs(h));
}

} // namespace nacre
