#include <thread>
#include <atomic>
#include "Wavetables.h"
#include <complex>
#include <functional>

namespace nacre {

using cd = std::complex<double>;
static constexpr int N = 4096;                  // analysis resolution
static constexpr double PI = 3.14159265358979323846;

// A frame is described either in the time domain (one cycle, N samples)
// or directly as harmonic coefficients (sine-phase: H[h] = amplitude of sin(h*x)).
struct Gen {
    std::function<void(double t, double* x)> time;   // fill N samples
    std::function<void(double t, cd* H)> spec;       // fill H[1..kWtMaxHarm]
};

static inline void sineH(cd* H, int h, double a) { H[h] += cd(0.0, -a); }  // FFT convention for a*sin(hx)
static double frac(double x) { return x - std::floor(x); }
static double sinc(double x) { return std::sin(2 * PI * x); }
static double smooth(double x) { x = std::clamp(x, 0.0, 1.0); return x * x * (3 - 2 * x); }

// Formant gain at frequency f for a set of (freq, bandwidth, gain) formants.
static double formants(double f, const double (*F)[3], int n) {
    double g = 0;
    for (int i = 0; i < n; ++i) { double d = (f - F[i][0]) / F[i][1]; g += F[i][2] / (1 + d * d); }
    return g;
}
static void vowelSpec(double t, cd* H, double f0, double tilt, double v0, double vSpan) {
    static const double V[5][4][3] = {
        {{730,90,1},{1090,110,.5},{2440,170,.25},{3400,250,.1}},   // a
        {{530,70,1},{1840,100,.45},{2480,150,.3},{3500,250,.1}},   // e
        {{270,60,1},{2290,100,.3},{3010,150,.25},{3700,250,.1}},   // i
        {{570,80,1},{840,90,.6},{2410,150,.15},{3400,250,.05}},    // o
        {{300,60,1},{870,80,.4},{2240,150,.1},{3300,250,.05}}};    // u
    double p = std::clamp(v0 + t * vSpan, 0.0, 3.999);
    int i = (int) p; double u = smooth(p - i);
    double F[4][3];
    for (int k = 0; k < 4; ++k) for (int j = 0; j < 3; ++j) F[k][j] = V[i][k][j] + (V[i + 1][k][j] - V[i][k][j]) * u;
    for (int h = 1; h <= kWtMaxHarm; ++h) {
        double f = h * f0; if (f > 9000) break;
        sineH(H, h, formants(f, F, 4) * std::pow(h, -tilt));
    }
}

static std::vector<Gen> makeGens() {
    std::vector<Gen> g(kNumTables);
    auto T = [&](int i, std::function<void(double, double*)> f) { g[(size_t) i].time = std::move(f); };
    auto S = [&](int i, std::function<void(double, cd*)> f) { g[(size_t) i].spec = std::move(f); };

    S(0, [](double t, cd* H) { // Basic Shapes: sine -> triangle -> saw -> square
        auto shape = [](int s, int h) -> double {
            switch (s) {
                case 0: return h == 1 ? 1.0 : 0.0;
                case 1: return (h & 1) ? 8 / (PI * PI) * (((h - 1) / 2) & 1 ? -1.0 : 1.0) / (h * h) : 0.0;
                case 2: return 2 / PI * ((h & 1) ? 1.0 : -1.0) / h;
                default: return (h & 1) ? 4 / PI / h : 0.0;
            }
        };
        double p = t * 3; int s = std::min((int) p, 2); double u = p - s;
        for (int h = 1; h <= kWtMaxHarm; ++h) sineH(H, h, shape(s, h) * (1 - u) + shape(s + 1, h) * u);
    });
    S(1, [](double t, cd* H) { double e = 2.6 - 1.9 * t; for (int h = 1; h <= kWtMaxHarm; ++h) sineH(H, h, std::pow(h, -e)); });
    T(2, [](double t, double* x) { double w = 0.5 - 0.47 * t; for (int i = 0; i < N; ++i) x[i] = (double) i / N < w ? 1 : -1; });
    T(3, [](double t, double* x) { for (int i = 0; i < N; ++i) { double p = (double) i / N; double saw = 1 - 2 * p, sq = p < .5 ? 1 : -1, dbl = 1 - 2 * frac(2 * p);
        x[i] = t < .5 ? saw + (dbl * .7 - saw) * smooth(t * 2) : dbl * .7 + (sq - dbl * .7) * smooth(t * 2 - 1); } });
    T(4, [](double t, double* x) { double gn = 1 + t * t * 18; for (int i = 0; i < N; ++i) x[i] = std::tanh(sinc((double) i / N) * gn); });
    T(5, [](double t, double* x) { for (int i = 0; i < N; ++i) { double s = sinc((double) i / N); x[i] = s > 0 ? s : s * (1 - 2 * t); } });
    T(6, [](double t, double* x) { double gn = 1 + t * 6; for (int i = 0; i < N; ++i) { double p = (double) i / N;
        double tri = p < .25 ? 4 * p : (p < .75 ? 2 - 4 * p : 4 * p - 4); double v = tri * gn; v = std::asin(std::sin(v * PI / 2)) * 2 / PI; x[i] = v; } });
    T(7, [](double t, double* x) { double gn = 1 + t * 5; for (int i = 0; i < N; ++i) x[i] = std::sin(sinc((double) i / N) * gn * PI / 2); });
    T(8, [](double t, double* x) { double r = 1 + t * 7; for (int i = 0; i < N; ++i) { double p = (double) i / N; x[i] = std::sin(2 * PI * p * r) * (1 - p * .3); } });
    T(9, [](double t, double* x) { double r = 1 + t * 7; for (int i = 0; i < N; ++i) { double p = (double) i / N; x[i] = 1 - 2 * frac(p * r); } });
    S(10, [](double t, cd* H) { double c = 2 + t * t * 60, Q = 7; for (int h = 1; h <= kWtMaxHarm; ++h) { double r = h / c;
        sineH(H, h, 1.0 / h / std::sqrt((1 - r * r) * (1 - r * r) + r * r / (Q * Q))); } });
    S(11, [](double t, cd* H) { double c = 1 + t * 40; sineH(H, 1, .5); for (int h = 2; h <= 96; ++h) { double d = (h - c) / 1.6; sineH(H, h, std::exp(-d * d)); } });
    S(12, [](double t, cd* H) { double lim = 1 + t * 80; for (int h = 1; h <= kWtMaxHarm; ++h) {
        double a = (h & 1) ? 1.0 / h : 0.08 / h; sineH(H, h, a * smooth((lim - h) / 4 + 1)); } });
    S(13, [](double t, cd* H) { int k = 0; for (int h = 1; h <= 256; h *= 2, ++k) sineH(H, h, h == 1 ? 1 : smooth(t * 6 - k + 1) * .7 / std::sqrt(k + 1.0)); });
    S(14, [](double t, cd* H) { // drawbar registrations (harmonics 1,2,3,4,6,8,10,12,16)
        static const int hs[9] = {1,2,3,4,6,8,10,12,16};
        static const double R[6][9] = {{8,8,8,0,0,0,0,0,0},{8,8,8,8,0,0,0,0,0},{8,6,8,0,0,0,0,0,8},{8,8,8,8,8,8,8,8,8},{8,8,6,8,0,4,0,0,4},{8,0,8,0,6,0,4,0,2}};
        double p = t * 5; int s = std::min((int) p, 4); double u = smooth(p - s);
        for (int j = 0; j < 9; ++j) { double d = R[s][j] + (R[s + 1][j] - R[s][j]) * u; sineH(H, hs[j], d > 0 ? std::pow(10, (d - 8) * 3 / 20) : 0); }
    });
    T(15, [](double t, double* x) { for (int i = 0; i < N; ++i) { double p = 2 * PI * i / N; x[i] = std::sin(p + t * 4.5 * std::sin(p)); } });
    T(16, [](double t, double* x) { for (int i = 0; i < N; ++i) { double p = 2 * PI * i / N; x[i] = std::sin(p + t * 3.5 * std::sin(2 * p)); } });
    T(17, [](double t, double* x) { for (int i = 0; i < N; ++i) { double p = 2 * PI * i / N; x[i] = std::sin(p + t * 3 * std::sin(3 * p)); } });
    T(18, [](double t, double* x) { for (int i = 0; i < N; ++i) { double p = 2 * PI * i / N; x[i] = std::sin(p + t * 1.6 * std::sin(7 * p)) * .8 + .2 * std::sin(2 * p); } });
    T(19, [](double t, double* x) { for (int i = 0; i < N; ++i) { double p = 2 * PI * i / N; x[i] = .55 * std::sin(p) + .6 * std::sin(3 * p + t * 2.5 * std::sin(7 * p)) + .25 * t * std::sin(11 * p); } });
    T(20, [](double t, double* x) { double lv = std::pow(2.0, 7 - 6 * t); for (int i = 0; i < N; ++i) x[i] = std::round(sinc((double) i / N) * lv) / lv; });
    T(21, [](double t, double* x) { double st = std::round(32 - t * 29); for (int i = 0; i < N; ++i) x[i] = 1 - 2 * std::floor((double) i / N * st) / (st - 1); });
    S(22, [](double t, cd* H) { vowelSpec(t, H, 110, 0.4, 0, 4); });
    S(23, [](double t, cd* H) { vowelSpec(t, H, 165, 0.9, 4, -4); });
    S(24, [](double t, cd* H) { vowelSpec(t, H, 55, 0.3, 3, -2.5); });
    S(25, [](double t, cd* H) { // two resonances moving in opposite directions over an odd-heavy source
        double c1 = 3 + t * 30, c2 = 40 - t * 30;
        for (int h = 1; h <= kWtMaxHarm; ++h) { double a = ((h & 1) ? 1.0 : .5) / std::pow(h, .8);
            double d1 = (h - c1) / 2.5, d2 = (h - c2) / 4; sineH(H, h, a * (0.3 + 3 / (1 + d1 * d1) + 2 / (1 + d2 * d2))); } });
    S(26, [](double t, cd* H) { sineH(H, 1, 1); sineH(H, 2, .25 + t * .2); sineH(H, 3, .08 + .2 * t); sineH(H, 4, .05 * t);
        double tine = .03 + t * .35; sineH(H, 7, tine); sineH(H, 14, tine * .4); sineH(H, 20, tine * .2 * t); });
    S(27, [](double t, cd* H) { double k = .4 * (1 - t) * (1 - t) + .004; for (int h = 1; h <= kWtMaxHarm; ++h) sineH(H, h, std::exp(-h * k) / h * (h % 7 == 0 ? .4 : 1)); });
    S(28, [](double t, cd* H) { static const double F[4][3] = {{280,120,1},{900,300,.6},{2300,500,.5},{3600,600,.25}};
        for (int h = 1; h <= kWtMaxHarm; ++h) { double f = h * 220.0; sineH(H, h, (.2 + formants(f, F, 4)) / std::pow(h, 1.1 - .5 * t) * std::exp(-f / (3000 + t * 9000))); } });
    S(29, [](double t, cd* H) { double b = 2 + t * t * 40; for (int h = 1; h <= kWtMaxHarm; ++h) sineH(H, h, std::exp(-h / b) * (h >= 3 && h <= 7 ? 1.4 : 1) / std::sqrt((double) h)); });
    S(30, [](double t, cd* H) { double b = 3 + t * 30; for (int h = 1; h <= kWtMaxHarm; ++h) sineH(H, h, ((h & 1) ? 1.0 : .12) / h * std::exp(-h / b)); });
    S(31, [](double t, cd* H) { sineH(H, 1, 1 - .5 * t); sineH(H, 2, .1 + .7 * t); sineH(H, 3, .06 + .1 * t); sineH(H, 4, .1 * t); sineH(H, 5, .02); });
    S(32, [](double t, cd* H) { static const int pr[] = {1,2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71};
        for (int k = 0; k < 21; ++k) { double c = k / 20.0; double d = (c - t) / .25; sineH(H, pr[k], (k == 0 ? .7 : .5) * std::exp(-d * d) + .05); } });
    S(33, [](double t, cd* H) { sineH(H, 1, .8); for (int k = 2; k <= 10; ++k) { int h = k * (4 + (int) std::round(t * 4)); if (h <= kWtMaxHarm) sineH(H, h, .5 / std::sqrt((double) k)); } });
    S(34, [](double t, cd* H) { // smooth morph between five seeded random spectra
        double p = t * 4; int s = std::min((int) p, 3); double u = smooth(p - s);
        for (int h = 1; h <= 160; ++h) { auto amp = [&](int k) { return (0.5 + 0.5 * hashf((uint32_t) (k * 977 + h * 131))) / std::pow(h, .9); };
            double a = amp(s) * (1 - u) + amp(s + 1) * u; double ph = PI * hashf((uint32_t) (h * 71 + 5)); H[h] += std::polar(a, ph); } });
    S(35, [](double t, cd* H) { for (int h = 1; h <= 256; h *= 2) sineH(H, h, 1 / std::sqrt((double) h));
        for (int h = 2; h <= 200; ++h) { double c = 16 + t * 120; double d = (h - c) / 10; H[h] += std::polar(.15 * std::exp(-d * d), PI * hashf((uint32_t) h)); } });
    S(36, [](double t, cd* H) { double per = 2 + t * 14; for (int h = 1; h <= kWtMaxHarm; ++h) sineH(H, h, (0.5 + 0.5 * std::cos(2 * PI * h / per)) / std::pow(h, .9)); });
    S(37, [](double t, cd* H) { double sp = 4 + t * 24; for (int h = 1; h <= kWtMaxHarm; ++h) sineH(H, h, std::fabs(std::cos(PI * h / sp)) / h); });
    S(38, [](double t, cd* H) { double lim = 2 + t * t * 510; for (int h = 1; h <= kWtMaxHarm; ++h) H[h] += cd(smooth((lim - h) / (1 + lim * .2) + 1) * .1, 0); });
    S(39, [](double t, cd* H) { for (int h = 1; h <= kWtMaxHarm; ++h) H[h] += std::polar(std::pow(h, -1.2 + .7 * t), PI * hashf((uint32_t) (h * 13 + 1))); });
    S(40, [](double t, cd* H) { for (int h = 1; h <= kWtMaxHarm; ++h) { double a = 0; if (h % 2 == 0) a += 2.0 / h; if (h % 3 == 0) a += (.4 + .6 * t) * 3.0 / h; sineH(H, h, a * std::exp(-h / (20 + 200 * t))); } });
    S(41, [](double t, cd* H) { for (int h = 1; h <= kWtMaxHarm; ++h) { double a = 0; for (int r : {4, 5, 6}) if (h % r == 0) a += (double) r / h; sineH(H, h, a * std::exp(-h / (24 + 300 * t))); } });
    S(42, [](double t, cd* H) { for (int h = 1; h <= kWtMaxHarm; ++h) { double a = 0; for (int r : {16, 19, 24}) if (h % r == 0) a += (double) r / h; sineH(H, h, a * std::exp(-h / (60 + 400 * t))); } });
    return g;
}

WavetableBank& WavetableBank::get() {
    static WavetableBank bank;   // thread-safe static init
    return bank;
}

WavetableBank::WavetableBank() {
    auto gens = makeGens();
    tables.resize(kNumTables);
    auto buildTable = [&](int ti) {
        std::vector<double> x(N);
        std::vector<cd> H((size_t) kWtMaxHarm + 1), buf;
        Wavetable& w = tables[(size_t) ti];
        for (int l = 0; l < kWtLevels; ++l) w.data[l].assign((size_t) kWtFrames * (size_t) (levelSize(l) + 1), 0.0f);
        w.preview.assign((size_t) kWtFrames * 128, 0.0f);
        for (int f = 0; f < kWtFrames; ++f) {
            const double t = (double) f / (kWtFrames - 1);
            std::fill(H.begin(), H.end(), cd(0, 0));
            if (gens[(size_t) ti].time) {
                gens[(size_t) ti].time(t, x.data());
                buf.assign(N, cd(0, 0));
                for (int i = 0; i < N; ++i) buf[(size_t) i] = x[(size_t) i];
                fft(buf, false);
                for (int h = 1; h <= kWtMaxHarm; ++h) H[(size_t) h] = buf[(size_t) h] * (2.0 / N);
            } else {
                gens[(size_t) ti].spec(t, H.data());
            }
            // Gentle roll-off of the top octave avoids a hard brick-wall ring (Gibbs) in every level.
            float norm = 1.0f;
            for (int l = 0; l < kWtLevels; ++l) {
                const int size = levelSize(l), hm = levelHarmonics(l);
                buf.assign((size_t) size, cd(0, 0));
                for (int h = 1; h <= hm; ++h) {
                    double r = (double) h / hm, win = r < .7 ? 1.0 : 0.5 + 0.5 * std::cos(PI * (r - .7) / .3);
                    cd c = H[(size_t) h] * win * 0.5;
                    buf[(size_t) h] += c; buf[(size_t) (size - h)] += std::conj(c);
                }
                fft(buf, true);
                float* dst = w.data[l].data() + (size_t) f * (size_t) (size + 1);
                if (l == 0) {   // normalise every frame to the same peak, measured on the richest level
                    double pk = 1e-9;
                    for (int i = 0; i < size; ++i) pk = std::max(pk, std::fabs(buf[(size_t) i].real()));
                    norm = (float) (0.98 / pk);
                }
                for (int i = 0; i < size; ++i) dst[i] = (float) buf[(size_t) i].real() * norm;
                dst[size] = dst[0];
                if (l == 2) for (int i = 0; i < 128; ++i) w.preview[(size_t) f * 128 + (size_t) i] = dst[i * size / 128];
            }
        }
    };
    // Tables are independent: build them on several threads so loading in a host stays quick on any build type.
    std::atomic<int> next { 0 };
    auto worker = [&] { for (int ti; (ti = next.fetch_add(1)) < kNumTables;) buildTable(ti); };
    std::vector<std::thread> pool;
    const int nt = std::max(1, std::min(8, (int) std::thread::hardware_concurrency()) - 1);
    try { for (int i = 0; i < nt; ++i) pool.emplace_back(worker); } catch (...) {}
    worker();
    for (auto& th : pool) th.join();
}

} // namespace nacre
