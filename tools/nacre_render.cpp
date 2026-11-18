// Nacre offline tool: unit tests, preset analysis (loudness, peak, tail) and WAV rendering.
//   nacre_render test
//   nacre_render stats <phrase> <preset files...>     -> one JSON line per preset
//   nacre_render wav <phrase> <preset file> <out.wav> [sampleRate]
//   nacre_render bench
#include "../engine/Synth.h"
#include "../engine/Preset.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <cstdio>

using namespace nacre;

static std::string readFile(const std::string& p) { std::ifstream f(p, std::ios::binary); std::stringstream s; s << f.rdbuf(); return s.str(); }

static void applyPreset(Engine& e, const Preset& p) { for (int i = 0; i < kNumParams; ++i) e.params[i] = p.values[i]; }

struct Ev { double t; int note; float vel; bool on; };

// Standard audition phrases, one per kind of patch.
static std::vector<Ev> phrase(const std::string& kind, double& length) {
    std::vector<Ev> ev;
    auto add = [&](double t0, double dur, int n, float v) { ev.push_back({t0, n, v, true}); ev.push_back({t0 + dur, n, 0, false}); };
    if (kind == "chord") { for (int n : {48, 55, 60, 64}) add(0.0, 3.0, n, 0.8f); length = 6.0; }
    else if (kind == "bass") { int ns[] = {36, 36, 43, 46, 36, 36, 41, 43}; for (int i = 0; i < 8; ++i) add(i * 0.25, 0.2, ns[i], i % 2 ? 0.7f : 0.95f); length = 3.0; }
    else if (kind == "lead") { int ns[] = {60, 63, 67, 70, 67, 63}; for (int i = 0; i < 6; ++i) add(i * 0.4, 0.38, ns[i], 0.85f); length = 4.0; }
    else if (kind == "pluck") { int ns[] = {60, 64, 67, 72, 67, 64, 60, 55}; for (int i = 0; i < 8; ++i) add(i * 0.25, 0.12, ns[i], 0.85f); length = 4.0; }
    else if (kind == "keys") { for (int n : {48, 55, 60, 64}) add(0.0, 1.0, n, 0.8f); for (int n : {50, 57, 62, 65}) add(1.0, 1.0, n, 0.7f); length = 4.0; }
    else if (kind == "drone") { add(0.0, 5.0, 48, 0.8f); add(0.0, 5.0, 55, 0.8f); length = 8.0; }
    else if (kind == "arp") { for (int n : {48, 51, 55, 58}) add(0.0, 4.0, n, 0.8f); length = 6.0; }
    else /* note */ { add(0.0, 2.0, 60, 0.9f); length = 4.0; }
    std::sort(ev.begin(), ev.end(), [](const Ev& a, const Ev& b) { return a.t < b.t || (a.t == b.t && !a.on && b.on); });
    return ev;
}

static void render(Engine& e, const std::vector<Ev>& ev, double length, double sr, std::vector<float>& L, std::vector<float>& R, int block = 256) {
    const size_t N = (size_t) (length * sr);
    L.assign(N, 0); R.assign(N, 0);
    size_t pos = 0, ei = 0;
    double ppq = 0;
    while (pos < N) {
        size_t end = std::min(N, pos + (size_t) block);
        e.setTransport(true, 120.0, ppq);
        while (pos < end) {
            size_t next = end;
            if (ei < ev.size()) next = std::min(next, std::max(pos, (size_t) (ev[ei].t * sr)));
            if (next > pos) { e.process(L.data() + pos, R.data() + pos, (int) (next - pos)); pos = next; }
            while (ei < ev.size() && (size_t) (ev[ei].t * sr) <= pos) { if (ev[ei].on) e.noteOn(ev[ei].note, ev[ei].vel); else e.noteOff(ev[ei].note); ++ei; }
        }
        ppq += (double) block / sr * 2.0;
    }
}

// ITU-R BS.1770 integrated loudness (K-weighting, 400 ms blocks, absolute + relative gates).
static double lufs(const std::vector<float>& L, const std::vector<float>& R, double sr) {
    Biquad a, b;
    // pre-filter (high shelf +4 dB @ 1.68 kHz) and RLB high-pass at 38 Hz, computed for this rate
    { double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196, K = std::tan(kPiD * f0 / sr);
      double Vh = std::pow(10.0, G / 20), Vb = std::pow(Vh, 0.4996667741545416), a0 = 1 + K / Q + K * K;
      a.b0 = (float) ((Vh + Vb * K / Q + K * K) / a0); a.b1 = (float) (2 * (K * K - Vh) / a0); a.b2 = (float) ((Vh - Vb * K / Q + K * K) / a0);
      a.a1 = (float) (2 * (K * K - 1) / a0); a.a2 = (float) ((1 - K / Q + K * K) / a0); }
    { double f0 = 38.13547087602444, Q = 0.5003270373238773, K = std::tan(kPiD * f0 / sr), a0 = 1 + K / Q + K * K;
      b.b0 = 1; b.b1 = -2; b.b2 = 1; b.a1 = (float) (2 * (K * K - 1) / a0); b.a2 = (float) ((1 - K / Q + K * K) / a0); }
    std::vector<double> sq(L.size());
    for (size_t i = 0; i < L.size(); ++i) { double l = b.tick(a.tick(L[i], 0), 0), r = b.tick(a.tick(R[i], 1), 1); sq[i] = l * l + r * r; }
    const size_t blk = (size_t) (0.4 * sr), hop = (size_t) (0.1 * sr);
    std::vector<double> z;
    for (size_t s = 0; s + blk <= sq.size(); s += hop) { double m = 0; for (size_t i = s; i < s + blk; ++i) m += sq[i]; z.push_back(m / (double) blk); }
    auto gated = [&](double thr) { double m = 0; int c = 0; for (double v : z) if (-0.691 + 10 * std::log10(v + 1e-20) > thr) { m += v; ++c; } return c ? -0.691 + 10 * std::log10(m / c) : -200.0; };
    double first = gated(-70);
    return gated(first - 10);
}

static int failures = 0;
#define CHECK(c, msg) do { if (!(c)) { ++failures; std::cout << "  FAIL: " << msg << "\n"; } else std::cout << "  ok   " << msg << "\n"; } while (0)

static bool finiteAll(const std::vector<float>& v) { for (float x : v) if (!std::isfinite(x)) return false; return true; }
static float peakOf(const std::vector<float>& v) { float p = 0; for (float x : v) p = std::max(p, std::fabs(x)); return p; }

static int runTests() {
    std::cout << "Parameter table\n";
    {
        bool ok = true;
        for (int i = 0; i < kNumParams; ++i) {
            const auto& d = kParams[i];
            for (float n : {0.0f, 0.25f, 0.5f, 0.9f, 1.0f}) { float v = fromNorm(d, n), back = fromNorm(d, toNorm(d, v)); if (std::fabs(back - v) > 1e-3f * std::max(1.0f, std::fabs(v))) ok = false; }
            if (d.def < d.lo - 1e-6f || d.def > d.hi + 1e-6f) { ok = false; std::cout << "    default out of range: " << d.id << "\n"; }
            for (int j = 0; j < i; ++j) if (!strcmp(kParams[j].id, d.id)) { ok = false; std::cout << "    duplicate id " << d.id << "\n"; }
        }
        CHECK(ok, "ids unique, defaults in range, normalised mapping round-trips");
        CHECK((int) kParams[oscATable].hi + 1 == kNumTables, "wavetable list matches the bank");
        CHECK(std::string(formatValue(kParams[f1Cut], 1234.0f)) == "1.23 kHz", "value formatting");
    }
    std::cout << "Preset format\n";
    {
        Preset p; p.name = "T"; p.values[oscATable] = 22; p.values[f1Cut] = 850; p.values[mod3Dst] = dF1Cut; p.values[env1A] = 0.25f;
        Preset q = Preset::parse(p.serialize());
        bool same = true; for (int i = 0; i < kNumParams; ++i) if (std::fabs(p.values[i] - q.values[i]) > 1e-3f * std::max(1.0f, std::fabs(p.values[i]))) same = false;
        CHECK(same && q.name == "T", "serialize / parse round-trip, choices stored by name");
        Preset u = Preset::parse("@name=X\nnot_a_param=3\nfilter1_cutoff=99999\n");
        CHECK(u.values[f1Cut] == 20000.0f, "unknown ids ignored, out-of-range values clamped");
    }
    const double sr = 48000;
    std::cout << "Engine\n";
    {
        Engine e; e.prepare(sr, 512);
        std::vector<float> L, R; double len;
        { auto ev = phrase("chord", len); render(e, ev, len, sr, L, R); }
        CHECK(finiteAll(L) && peakOf(L) > 0.05f, "init patch makes sound");
        float tail = 0; for (size_t i = L.size() - 4800; i < L.size(); ++i) tail = std::max(tail, std::fabs(L[i]));
        CHECK(tail < 1e-4f, "voices release to silence");
        // pitch accuracy: count zero crossings of a sine at A4
        Engine s; s.prepare(sr, 512); s.params[oscATable] = 0; s.params[oscAPos] = 0; s.params[f1On] = 0; s.params[drift] = 0; s.params[limiter] = 0;
        s.reset();
        std::vector<float> a(48000), b(48000);
        s.noteOn(69, 1); s.process(a.data(), b.data(), 48000);
        int zc = 0; for (size_t i = 24000; i < 48000; ++i) if (a[i - 1] < 0 && a[i] >= 0) ++zc;
        CHECK(std::abs(zc - 220) <= 1, "A4 plays at 440 Hz (" + std::to_string(zc * 2) + " Hz measured)");
    }
    {   // aliasing: a bright saw at a high note should have almost no energy off the harmonic series
        Engine e; e.prepare(44100, 512);
        e.params[oscATable] = 1; e.params[oscAPos] = 1; e.params[f1On] = 0; e.params[drift] = 0; e.params[limiter] = 0; e.params[oscAPhase] = 0;
        e.reset();
        const int N = 32768; std::vector<float> L(N + 8192), R(N + 8192);
        e.noteOn(100, 1); e.process(L.data(), R.data(), N + 8192);
        std::vector<std::complex<double>> X(N);
        for (int i = 0; i < N; ++i) X[(size_t) i] = L[(size_t) (i + 8192)] * (0.5 - 0.5 * std::cos(2 * kPiD * i / N));
        fft(X, false);
        const double f0 = 440.0 * std::pow(2.0, (100 - 69) / 12.0);
        double harm = 0, other = 0;
        for (int k = 1; k < N / 2; ++k) {
            double f = k * 44100.0 / N, m = std::norm(X[(size_t) k]);
            double hn = f / f0; bool near = std::fabs(hn - std::round(hn)) * f0 < 40.0 && std::round(hn) >= 1;
            if (near) harm += m; else if (f < 18000) other += m;
        }
        double db = 10 * std::log10(other / harm);
        CHECK(db < -60, "alias rejection below 18 kHz, 2.6 kHz saw: " + std::to_string((int) db) + " dB");
    }
    {   // stress: random parameters at random rates, odd block sizes, many notes
        Engine e; e.prepare(96000, 1024); Rng r; r.s = 1234;
        std::vector<float> L(1024), R(1024); bool fin = true; float pk = 0;
        for (int it = 0; it < 1500; ++it) {
            if (it % 25 == 0) for (int i = 0; i < kNumParams; ++i) if (i != masterGain) e.params[i] = fromNorm(kParams[i], r.uni());
            e.params[masterGain] = 0; e.params[limiter] = 1;
            if (it % 3 == 0) e.noteOn(24 + (int) (r.uni() * 80), r.uni());
            if (it % 4 == 0) e.noteOff(24 + (int) (r.uni() * 80));
            if (it % 200 == 0) e.controlChange(64, r.uni());
            int n = 1 + (int) (r.uni() * 1023);
            e.setTransport(it % 2, 60 + r.uni() * 120, it * 0.1);
            e.process(L.data(), R.data(), n);
            for (int i = 0; i < n; ++i) { fin &= std::isfinite(L[(size_t) i]) && std::isfinite(R[(size_t) i]); pk = std::max(pk, std::fabs(L[(size_t) i])); }
        }
        CHECK(fin, "1500 blocks of random parameters/notes/block sizes: no NaN or Inf");
        CHECK(pk <= 0.97f, "limiter holds the ceiling under abuse (peak " + std::to_string(pk) + ")");
    }
    {   // voice stealing must be click-free: no sample-to-sample jump larger than the signal allows
        Engine e; e.prepare(48000, 64); e.params[polyphony] = 4; e.params[env1R] = 3; e.params[oscATable] = 0; e.params[f1On] = 0; e.reset();
        std::vector<float> L(48000 * 2), R(48000 * 2);
        int pos = 0; for (int k = 0; k < 40; ++k) { e.noteOn(48 + (k * 7) % 24, 0.9f); e.process(L.data() + pos, R.data() + pos, 2400); pos += 2400; }
        float maxJump = 0; for (size_t i = 1; i < (size_t) pos; ++i) maxJump = std::max(maxJump, std::fabs(L[i] - L[i - 1]));
        CHECK(maxJump < 0.08f, "voice stealing is click-free (max step " + std::to_string(maxJump) + ")");
        int active = e.uiVoices.load();
        CHECK(active <= 6, "polyphony limit respected (" + std::to_string(active) + " incl. fading)");
    }
    {   // block-size independence: rendering with 1-sample and 512-sample blocks gives the same result
        auto run = [&](int blk) { Engine e; e.prepare(48000, 512); e.params[drift] = 0; e.params[oscAPhase] = 0; e.reset();
            std::vector<float> L, R; double len; auto ev = phrase("lead", len); render(e, ev, 1.0, 48000, L, R, blk); return L; };
        auto a = run(1), b = run(512);
        float diff = 0; for (size_t i = 0; i < a.size(); ++i) diff = std::max(diff, std::fabs(a[i] - b[i]));
        CHECK(diff < 0.02f, "block size independence (max diff " + std::to_string(diff) + ")");
    }
    {   // sample rate coverage
        bool ok = true;
        for (double s : {22050.0, 44100.0, 48000.0, 88200.0, 96000.0, 192000.0}) {
            Engine e; e.prepare(s, 256); e.params[reverbOn] = 1; e.params[delayOn] = 1; e.params[chorusOn] = 1;
            std::vector<float> L, R; double len; auto ev = phrase("chord", len); render(e, ev, 1.5, s, L, R);
            ok &= finiteAll(L) && peakOf(L) > 0.02f;
        }
        CHECK(ok, "runs at 22.05 / 44.1 / 48 / 88.2 / 96 / 192 kHz");
    }
    {   // arpeggiator steps in time with the host
        Engine e; e.prepare(48000, 256); e.params[arpOn] = 1; e.params[arpDiv] = 14; e.params[env1R] = 0.01f; e.params[env1A] = 0.001f; e.reset();
        std::vector<float> L, R; render(e, {{0.0, 60, 0.8f, true}, {0.0, 64, 0.8f, true}, {0.0, 67, 0.8f, true}, {2.0, 60, 0, false}, {2.0, 64, 0, false}, {2.0, 67, 0, false}}, 2.0, 48000, L, R);
        int onsets = 0; bool above = false;
        for (size_t w = 0; w + 240 <= L.size(); w += 240) {     // 5 ms RMS windows
            double e2 = 0; for (size_t i = w; i < w + 240; ++i) e2 += L[i] * L[i];
            float rms = (float) std::sqrt(e2 / 240);
            if (!above && rms > 0.03f) { ++onsets; above = true; } else if (above && rms < 0.005f) above = false;
        }
        CHECK(onsets >= 14 && onsets <= 17, "arp 1/16 at 120 BPM: " + std::to_string(onsets) + " steps in 2 s (16 expected)");
    }
    {   // sustain pedal holds notes and releases them when lifted
        Engine e; e.prepare(48000, 256); e.params[env1R] = 0.05f; e.reset();
        std::vector<float> L(48000), R(48000);
        e.controlChange(64, 1.0f); e.noteOn(60, 0.9f); e.process(L.data(), R.data(), 4800); e.noteOff(60);
        e.process(L.data(), R.data(), 24000);
        const bool held = e.uiVoices.load() == 1;
        e.controlChange(64, 0.0f); e.process(L.data(), R.data(), 24000);
        CHECK(held && e.uiVoices.load() == 0, "sustain pedal holds, then releases");
    }
    std::cout << (failures ? "\nFAILED: " : "\nAll tests passed. ") << failures << " failure(s)\n";
    return failures ? 1 : 0;
}

static void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr) {
    std::ofstream f(path, std::ios::binary);
    auto u32 = [&](uint32_t v) { f.write((const char*) &v, 4); }; auto u16 = [&](uint16_t v) { f.write((const char*) &v, 2); };
    const uint32_t n = (uint32_t) L.size();
    f.write("RIFF", 4); u32(36 + n * 4); f.write("WAVEfmt ", 8); u32(16); u16(1); u16(2); u32((uint32_t) sr); u32((uint32_t) sr * 4); u16(4); u16(16);
    f.write("data", 4); u32(n * 4);
    for (uint32_t i = 0; i < n; ++i) { int16_t a = (int16_t) std::lround(clampf(L[i], -1, 1) * 32767), b = (int16_t) std::lround(clampf(R[i], -1, 1) * 32767); f.write((const char*) &a, 2); f.write((const char*) &b, 2); }
}

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: nacre_render test|stats|wav|bench\n"; return 2; }
    std::string cmd = argv[1];
    if (cmd == "test") return runTests();
    if (cmd == "stats" && argc >= 4) {
        const double sr = 48000;
        Engine e; e.prepare(sr, 256);
        for (int a = 3; a < argc; ++a) {
            Preset p = Preset::parse(readFile(argv[a]));
            applyPreset(e, p); e.params[masterGain] = 0; e.params[limiter] = 0; e.reset();
            std::vector<float> L, R; double len;
            auto t0 = std::chrono::steady_clock::now();
            auto ev = phrase(argv[2], len);
            render(e, ev, len, sr, L, R);
            double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            double dcL = 0; for (float x : L) dcL += x; dcL /= (double) L.size();
            float tail = 0; for (size_t i = L.size() - (size_t) (0.25 * sr); i < L.size(); ++i) tail = std::max(tail, std::fabs(L[i]));
            double corr = 0, eL = 0, eR = 0; for (size_t i = 0; i < L.size(); ++i) { corr += L[i] * R[i]; eL += L[i] * L[i]; eR += R[i] * R[i]; }
            printf("{\"file\":\"%s\",\"lufs\":%.2f,\"peak\":%.4f,\"finite\":%d,\"dc\":%.5f,\"tail\":%.5f,\"corr\":%.3f,\"rt\":%.3f}\n",
                   argv[a], lufs(L, R, sr), std::max(peakOf(L), peakOf(R)), (int) (finiteAll(L) && finiteAll(R)), dcL, tail,
                   corr / std::sqrt(eL * eR + 1e-20), secs / len);
            fflush(stdout);
        }
        return 0;
    }
    if (cmd == "wav" && argc >= 5) {
        const int sr = argc >= 6 ? atoi(argv[5]) : 48000;
        Engine e; e.prepare(sr, 256);
        applyPreset(e, Preset::parse(readFile(argv[3]))); e.reset();
        std::vector<float> L, R; double len;
        auto ev = phrase(argv[2], len);
        render(e, ev, len, sr, L, R);
        writeWav(argv[4], L, R, sr);
        return 0;
    }
    if (cmd == "bench") {
        Engine e; e.prepare(48000, 512);
        e.params[oscAUnison] = 7; e.params[oscBOn] = 1; e.params[oscBUnison] = 7; e.params[f1Type] = fLadder; e.params[f2On] = 1;
        e.params[reverbOn] = 1; e.params[delayOn] = 1; e.params[chorusOn] = 1; e.params[distOn] = 1; e.reset();
        for (int n = 0; n < 8; ++n) e.noteOn(48 + n * 3, 0.8f);
        std::vector<float> L(512), R(512);
        auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < 940; ++b) e.process(L.data(), R.data(), 512);     // ~10 s of audio
        double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        printf("8 voices x 2 osc x 7 unison (112 oscillators), 2 filters, full FX: %.1f%% of one core at 48 kHz\n", secs / 10.03 * 100);
        return 0;
    }
    std::cerr << "bad arguments\n"; return 2;
}
