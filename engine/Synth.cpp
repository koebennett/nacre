#include "Synth.h"

namespace nacre {

// ===================================================================== filter
void FilterUnit::setup(int t, float fc, float res, float drv, float rate) {
    if (t != type) { reset(); type = t; }
    fc = clampf(fc, 20.0f, std::min(20000.0f, rate * 0.45f));
    res = clampf(res, 0, 1);
    drive = 1.0f + drv * drv * 12.0f;
    driveComp = 1.0f / (1.0f + drv * drv * 2.5f);
    const float g = tanG(fc, rate);
    switch (t) {
        case fLP24: case fHP24: c1.set(g, 1.848f); c2.set(g, 0.765f * std::pow(0.012f, res)); break;
        case fLadder: ladG = 1.0f - std::exp(-kTwoPi * fc / rate); ladK = res * 4.1f; break;
        case fComb: combLen = clampf(rate / std::max(fc, 30.0f), 2.0f, (float) comb[0].mask - 2.0f); combFb = 0.35f + 0.62f * res; break;
        case fVowel: {
            static const float F[5][3] = {{800,1150,2900},{400,1600,2700},{270,2140,2950},{450,800,2830},{325,700,2530}};
            float p = clampf(std::log(fc / 20.0f) / std::log(1000.0f), 0.0f, 1.0f) * 4.0f; int i = std::min((int) p, 3); float u = p - (float) i;
            const float gains[3] = {1.0f, 0.6f, 0.35f};
            float k = 1.0f / (4.0f + res * 16.0f);
            for (int j = 0; j < 3; ++j) { vc[j].set(tanG(lerpf(F[i][j], F[i + 1][j], u), rate), k); vg[j] = gains[j] * k * 2.2f; }
            break;
        }
        default: c1.set(g, resToK(res)); break;
    }
}

inline float FilterUnit::tick(float x, int ch) {
    if (drive > 1.001f) x = fastTanh(x * drive) * driveComp * 1.6f;
    float lp, bp;
    switch (type) {
        case fLP12: a[ch].tick(x, c1, lp, bp); return lp;
        case fLP24: a[ch].tick(x, c1, lp, bp); b[ch].tick(lp, c2, lp, bp); return lp;
        case fHP12: a[ch].tick(x, c1, lp, bp); return x - c1.k * bp - lp;
        case fHP24: { a[ch].tick(x, c1, lp, bp); float h = x - c1.k * bp - lp; b[ch].tick(h, c2, lp, bp); return h - c2.k * bp - lp; }
        case fBand: a[ch].tick(x, c1, lp, bp); return bp * c1.k;
        case fNotch: a[ch].tick(x, c1, lp, bp); return x - c1.k * bp;
        case fLadder: {
            float* s = lad[ch];
            float u = fastTanh(x - ladK * (s[3] - 0.15f * x));
            s[0] += ladG * (u - s[0]); s[1] += ladG * (s[0] - s[1]); s[2] += ladG * (s[1] - s[2]); s[3] += ladG * (s[2] - s[3]);
            return s[3] * (1.0f + ladK * 0.45f);
        }
        case fComb: {
            float d = comb[ch].readFrac(combLen);
            combLp[ch] += 0.6f * (d - combLp[ch]);
            float y = x + combFb * combLp[ch];
            comb[ch].write(fastTanh(y));
            return y * (1.0f - combFb * 0.55f);
        }
        case fVowel: {
            float y = 0;
            for (int j = 0; j < 3; ++j) { v[ch][j].tick(x, vc[j], lp, bp); y += bp * vg[j] * 4.0f; }
            return y;
        }
        default: return x;
    }
}

// ===================================================================== engine
Engine::Engine() {
    WavetableBank::get();
    setDefaults();
    std::copy(params, params + kNumParams, sp);
}

void Engine::setDefaults() { for (int i = 0; i < kNumParams; ++i) params[i] = kParams[i].def; }

void Engine::prepare(double sampleRate, int block) {
    sr = sampleRate; maxBlock = std::max(block, kCtrl);
    busL.assign((size_t) kCtrl * kMaxOs, 0.0f); busR.assign((size_t) kCtrl * kMaxOs, 0.0f);
    for (auto& v : voices) { v.f1.prepare((float) sr * kMaxOs); v.f2.prepare((float) sr * kMaxOs); }
    phaser.prepare((float) sr); chorus.prepare((float) sr); delay.prepare((float) sr); reverb.prepare((float) sr);
    eq.prepare((float) sr); comp.prepare((float) sr); lim.prepare((float) sr);
    os = params[quality] > 0.5f ? 2 : 1; vsr = (float) sr * (float) os;
    dist.prepare(vsr);
    dcR = 1.0f - kTwoPi * 5.0f / (float) sr;
    reset();
}

void Engine::reset() {
    killEverything();
    std::copy(params, params + kNumParams, sp);
    masterCur = dbToGain(params[masterGain]);
}

void Engine::killEverything() {
    for (auto& v : voices) { v.active = false; v.amp.kill(); v.fenv.kill(); v.menv.kill(); }
    down[0].reset(); down[1].reset(); dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0;
    dist.reset(); phaser.reset(); chorus.reset(); delay.reset(); reverb.reset(); eq.reset(); comp.reset();
    arpN = 0; arpPlayingN = 0; arpRunning = false; stackN = 0;
    std::fill(std::begin(keyDown), std::end(keyDown), false);
}

void Engine::setTransport(bool playing, double tempo, double position) {
    hostPlaying = playing;
    bpm = tempo > 1 ? tempo : 120;
    if (playing) ppq = position;
}

// ------------------------------------------------------------------ MIDI in
void Engine::noteOn(int note, float velocity) {
    note = std::clamp(note, 0, 127);
    if (velocity <= 0) { noteOff(note); return; }
    if (params[arpOn] > 0.5f) {
        if (params[arpLatch] > 0.5f && arpLatchArmed) { arpN = 0; arpLatchArmed = false; }
        for (int i = 0; i < arpN; ++i) if (arpHeld[i].note == note) return;
        if (arpN < 64) arpHeld[arpN++] = {note, velocity};
        keyDown[note] = true;
        return;
    }
    voiceOn(note, velocity);
}

void Engine::noteOff(int note) {
    note = std::clamp(note, 0, 127);
    if (params[arpOn] > 0.5f) {
        keyDown[note] = false;
        if (params[arpLatch] > 0.5f) {
            bool any = false; for (bool k : keyDown) any |= k;
            if (!any) arpLatchArmed = true;     // next key press starts a new chord
            return;
        }
        for (int i = 0; i < arpN; ++i) if (arpHeld[i].note == note) { std::copy(arpHeld + i + 1, arpHeld + arpN, arpHeld + i); --arpN; break; }
        return;
    }
    voiceOff(note);
}

void Engine::controlChange(int cc, float v) {
    switch (cc) {
        case 1: wheel = v; break;
        case 11: expression = v; break;
        case 64: {
            const bool isDown = v >= 0.5f, wasDown = pedal;
            pedal = isDown;
            if (wasDown && !isDown) for (auto& vo : voices) if (vo.active && vo.sustained) { vo.sustained = false; releaseVoice(vo); }
            break;
        }
        case 120: allSoundOff(); break;
        case 121: wheel = 0; bend = 0; chanAT = 0; expression = 1; break;
        case 123: allNotesOff(); break;
        default: break;
    }
}

void Engine::polyPressure(int note, float v) { for (auto& vo : voices) if (vo.active && vo.note == note) vo.pressure = v; }

void Engine::allNotesOff() {
    arpN = 0; arpStop(); stackN = 0;
    for (auto& v : voices) if (v.active) { v.sustained = false; releaseVoice(v); }
    std::fill(std::begin(keyDown), std::end(keyDown), false);
}

void Engine::allSoundOff() { killEverything(); }

// ------------------------------------------------------------------ voices
Voice* Engine::newestVoice() {
    Voice* best = nullptr;
    for (auto& v : voices) if (v.active && !v.fading && (!best || v.age > best->age)) best = &v;
    return best;
}

void Engine::startVoice(Voice& v, int note, float vel, bool fresh) {
    const float glideT = params[glide];
    const float tgt = (float) note;
    v.note = note; v.vel = vel; v.gate = true; v.sustained = false; v.age = ++ageCounter;
    v.target = tgt;
    if (fresh) {
        v.active = true; v.fading = false; v.fade = 1; v.pressure = 0;
        v.pitch = (glideT > 0.0005f && lastPitch >= 0) ? lastPitch : tgt;
        v.amp.kill(); v.fenv.kill(); v.menv.kill();
        v.f1.reset(); v.f2.reset();
        v.time = 0; v.rnd = rng.bi(); v.drift = v.driftTarget = 0;
        v.cutDrift = rng.bi();
        const float pr = params[oscAPhase], prb = params[oscBPhase];
        for (int u = 0; u < kMaxUnison; ++u) {
            v.ph[0][u] = pr * rng.uni(); v.ph[1][u] = prb * rng.uni();
            v.analog[0][u] = rng.bi(); v.analog[1][u] = rng.bi();
        }
        v.subPh = 0; v.noiseLp = 0;
        for (int i = 0; i < 3; ++i) { v.lfo[i].phase = 0; v.lfo[i].r0 = rng.bi(); v.lfo[i].r1 = rng.bi(); }
        v.ampCur = 0;
        std::fill(std::begin(v.src), std::end(v.src), 0.0f);
        std::fill(std::begin(v.mod), std::end(v.mod), 0.0f);
    } else if (glideT <= 0.0005f) {
        v.pitch = tgt;
    }
    v.amp.on(); v.fenv.on(); v.menv.on();
    lastPitch = tgt;
}

void Engine::releaseVoice(Voice& v) {
    if (pedal) { v.sustained = true; return; }
    v.gate = false; v.amp.off(); v.fenv.off(); v.menv.off();
}

void Engine::voiceOn(int note, float vel) {
    keyDown[note] = true;
    const int mode = (int) params[voiceMode];
    if (mode != 0) {                     // mono / legato: one voice, note stack
        bool anyHeld = stackN > 0;
        for (int i = 0; i < stackN; ++i) if (stack[i] == note) { std::copy(stack + i + 1, stack + stackN, stack + i); --stackN; break; }
        if (stackN < 128) stack[stackN++] = note;
        Voice* mv = nullptr;
        for (auto& v : voices) if (v.active && !v.fading) { if (!mv) mv = &v; else { v.fading = true; v.fadeStep = 1.0f / (0.004f * vsr); } }
        if (!mv) {
            for (auto& v : voices) if (!v.active) { mv = &v; break; }
            if (!mv) return;
            const float lp = lastPitch; lastPitch = -1;      // first note of a phrase does not glide
            startVoice(*mv, note, vel, true); (void) lp;
            return;
        }
        const bool legato = (mode == 2) && anyHeld && mv->gate;
        const float glideT = params[glide];
        mv->note = note; mv->target = (float) note; mv->gate = true; mv->sustained = false; mv->age = ++ageCounter;
        if (glideT <= 0.0005f || (mode == 2 && !legato)) mv->pitch = (float) note;
        if (!legato) { mv->vel = vel; mv->amp.on(); mv->fenv.on(); mv->menv.on(); }
        lastPitch = (float) note;
        return;
    }
    // poly
    int count = 0;
    for (auto& v : voices) {
        if (v.active && !v.fading && v.note == note) { v.fading = true; v.fadeStep = 1.0f / (0.003f * vsr); }
        if (v.active && !v.fading) ++count;
    }
    const int limit = (int) params[polyphony];
    if (count >= limit) {
        Voice* victim = nullptr;
        for (auto& v : voices) if (v.active && !v.fading && !v.gate && (!victim || v.amp.v < victim->amp.v)) victim = &v;
        if (!victim) for (auto& v : voices) if (v.active && !v.fading && (!victim || v.age < victim->age)) victim = &v;
        if (victim) { victim->fading = true; victim->fadeStep = 1.0f / (0.004f * vsr); }
    }
    Voice* slot = nullptr;
    for (auto& v : voices) if (!v.active) { slot = &v; break; }
    if (!slot) for (auto& v : voices) if (v.fading && (!slot || v.fade < slot->fade)) slot = &v;
    if (!slot) return;
    startVoice(*slot, note, vel, true);
}

void Engine::voiceOff(int note) {
    keyDown[note] = false;
    const int mode = (int) params[voiceMode];
    if (mode != 0) {
        for (int i = 0; i < stackN; ++i) if (stack[i] == note) { std::copy(stack + i + 1, stack + stackN, stack + i); --stackN; break; }
        Voice* mv = newestVoice();
        if (!mv || mv->note != note) return;
        if (stackN > 0) {                 // fall back to the previous held key, legato
            int n2 = stack[stackN - 1];
            mv->note = n2; mv->target = (float) n2;
            if (params[glide] <= 0.0005f) mv->pitch = (float) n2;
            lastPitch = (float) n2;
        } else releaseVoice(*mv);
        return;
    }
    for (auto& v : voices) if (v.active && !v.fading && v.gate && v.note == note) releaseVoice(v);
}

// ------------------------------------------------------------------ arpeggiator
void Engine::arpStop() {
    for (int i = 0; i < arpPlayingN; ++i) voiceOff(arpPlaying[i]);
    arpPlayingN = 0; arpRunning = false;
}

void Engine::arpTick() {
    const bool on = params[arpOn] > 0.5f;
    if (!on) {
        if (arpWasOn) { arpStop(); arpN = 0; arpWasOn = false; }
        return;
    }
    if (!arpWasOn) { arpWasOn = true; for (auto& v : voices) if (v.active) releaseVoice(v); }
    const double beat = hostPlaying ? ppq : freeBeat;
    const int div = (int) params[arpDiv];
    const double L = divBeats(div);
    if (arpN == 0) { if (arpRunning || arpPlayingN) arpStop(); return; }
    if (!arpRunning || div != arpDivCur) {
        arpDivCur = div;
        if (!arpRunning) arpStep = 0;
        arpRunning = true;
        arpOrigin = hostPlaying ? 0.0 : beat;
        arpGrid = (long) std::ceil((beat - arpOrigin) / L - 1e-6);
        auto at = [&](long g) { return arpOrigin + (double) g * L + ((g & 1) ? params[arpSwing] * L * 0.5 : 0.0); };
        arpNext = at(arpGrid);
        arpOff = 1e300;
    }
    if (hostPlaying && (beat < arpNext - 2 * L - 1e-9)) {        // transport jumped backwards
        arpGrid = (long) std::ceil(beat / L - 1e-6); arpNext = (double) arpGrid * L;
    }
    if (arpPlayingN && beat >= arpOff) { for (int i = 0; i < arpPlayingN; ++i) voiceOff(arpPlaying[i]); arpPlayingN = 0; arpOff = 1e300; }
    if (beat + 1e-9 >= arpNext) {
        for (int i = 0; i < arpPlayingN; ++i) voiceOff(arpPlaying[i]);
        arpPlayingN = 0;
        // build the ordered pattern
        ArpNote sorted[64]; int n = arpN;
        std::copy(arpHeld, arpHeld + n, sorted);
        const int mode = (int) params[arpMode];
        if (mode != 4) std::sort(sorted, sorted + n, [](const ArpNote& a, const ArpNote& b) { return a.note < b.note; });
        const int octs = (int) params[arpOct];
        const int len = n * octs;
        if (mode == 5) {
            for (int i = 0; i < n && arpPlayingN < 64; ++i) arpPlaying[arpPlayingN++] = sorted[i].note;
            for (int i = 0; i < n; ++i) voiceOn(sorted[i].note, sorted[i].vel);
        } else {
            int idx;
            if (mode == 0 || mode == 4) idx = arpStep % len;
            else if (mode == 1) idx = len - 1 - (arpStep % len);
            else if (mode == 2) { int cyc = std::max(1, 2 * len - 2); int s = arpStep % cyc; idx = s < len ? s : cyc - s; }
            else idx = (int) (rng.uni() * (float) len) % len;
            const ArpNote& an = sorted[idx % n];
            const int nn = std::clamp(an.note + 12 * (idx / n), 0, 127);
            arpPlaying[arpPlayingN++] = nn;
            voiceOn(nn, an.vel);
        }
        ++arpStep;
        const double start = arpNext;
        ++arpGrid;
        arpNext = arpOrigin + (double) arpGrid * L + ((arpGrid & 1) ? params[arpSwing] * L * 0.5 : 0.0);
        arpOff = start + std::max(0.02, (arpNext - start) * params[arpGate]);
    }
}

// ------------------------------------------------------------------ modulation
static inline float evalSlots(const float* p, const float* src, int dst) {
    float m = 0;
    for (int s = 0; s < kModSlots; ++s) {
        const int base = mod1Src + s * 4;
        if ((int) p[base + 2] != dst) continue;
        const int so = (int) p[base], via = (int) p[base + 1];
        if (so == sNone) continue;
        m += src[so] * (via == sNone ? 1.0f : src[via]) * p[base + 3];
    }
    return m;
}

void Engine::computeVoiceMods(Voice& v) {
    float* s = v.src;
    const float* p = sp;
    for (int i = 0; i < 3; ++i) {
        const float fadeT = p[lfo1Fade + i * 6];
        const float fadeG = fadeT > 0.001f ? std::min(1.0f, v.time / fadeT) : 1.0f;
        const float raw = p[lfo1Retrig + i * 6] > 0.5f ? v.lfo[i].value((int) p[lfo1Shape + i * 6]) : glfoVal[i];
        s[sLfo1 + i] = raw * fadeG;
    }
    s[sEnv1] = v.amp.v; s[sEnv2] = v.fenv.v; s[sEnv3] = v.menv.v;
    s[sVel] = v.vel; s[sKey] = ((float) v.note - 60.0f) / 60.0f;
    s[sWheel] = wheel; s[sAT] = std::max(chanAT, v.pressure); s[sBend] = bend;
    s[sM1] = p[macro1]; s[sM2] = p[macro2]; s[sM3] = p[macro3]; s[sM4] = p[macro4];
    s[sRand] = v.rnd; s[sNone] = 0;
    // only evaluate targets that are actually used
    std::fill(std::begin(v.mod), std::end(v.mod), 0.0f);
    for (int k = 0; k < kModSlots; ++k) {
        const int base = mod1Src + k * 4;
        const int d = (int) p[base + 2], so = (int) p[base], via = (int) p[base + 1];
        if (d == dNone || so == sNone || d >= kFirstGlobalDst) continue;
        v.mod[d] += s[so] * (via == sNone ? 1.0f : s[via]) * p[base + 3];
    }
}

void Engine::controlTick(int n) {
    // parameter smoothing (~20 ms), geometric for frequencies
    const float k = 1.0f - std::exp(-(float) n / (0.02f * (float) sr));
    for (int i = 0; i < kNumParams; ++i) {
        const ParamDef& d = kParams[i];
        if (d.type != kFloat) { sp[i] = params[i]; continue; }
        if (d.skew < 0) sp[i] *= std::pow(params[i] / std::max(sp[i], 1e-6f), k);
        else sp[i] += (params[i] - sp[i]) * k;
    }
    const int wantOs = params[quality] > 0.5f ? 2 : 1;
    if (wantOs != os) { killEverything(); os = wantOs; vsr = (float) sr * (float) os; dist.prepare(vsr); }

    const double beatsPerSec = bpm / 60.0;
    const double dt = (double) n / sr;

    // global sources
    Voice* nv = newestVoice();
    gsrc[sEnv1] = nv ? nv->amp.v : 0; gsrc[sEnv2] = nv ? nv->fenv.v : 0; gsrc[sEnv3] = nv ? nv->menv.v : 0;
    gsrc[sVel] = nv ? nv->vel : 0; gsrc[sKey] = nv ? ((float) nv->note - 60) / 60 : 0; gsrc[sRand] = nv ? nv->rnd : 0;
    gsrc[sWheel] = wheel; gsrc[sAT] = chanAT; gsrc[sBend] = bend;
    gsrc[sM1] = sp[macro1]; gsrc[sM2] = sp[macro2]; gsrc[sM3] = sp[macro3]; gsrc[sM4] = sp[macro4];
    for (int i = 0; i < 3; ++i) gsrc[sLfo1 + i] = glfoVal[i];
    for (int d = dLfo1Rate; d < kNumDst; ++d) gmod[d] = (d >= kFirstGlobalDst || d <= dLfo3Rate) ? evalSlots(sp, gsrc, d) : 0;

    // global LFOs
    for (int i = 0; i < 3; ++i) {
        const int o = i * 6;
        float hz = sp[lfo1Rate + o];
        const double beats = divBeats((int) sp[lfo1Div + o]);
        if (sp[lfo1Sync + o] > 0.5f) hz = (float) (beatsPerSec / beats);
        hz *= std::exp2(gmod[dLfo1Rate + i] * 3.0f);
        if (sp[lfo1Sync + o] > 0.5f && hostPlaying && std::fabs(gmod[dLfo1Rate + i]) < 1e-6f) {
            float ph = (float) std::fmod(ppq / beats, 1.0);
            if (ph < glfo[i].phase - 0.5f) { glfo[i].r0 = glfo[i].r1; glfo[i].r1 = rng.bi(); }
            glfo[i].phase = ph < 0 ? ph + 1 : ph;
        } else glfo[i].advance(hz * (float) dt, rng);
        glfoVal[i] = glfo[i].value((int) sp[lfo1Shape + o]);
    }

    arpTick();

    // voices: envelopes / LFO / glide / drift at control rate
    const float crate = (float) sr / (float) n;
    int active = 0;
    for (auto& v : voices) {
        if (!v.active) continue;
        ++active;
        v.amp.set(sp[env1A], sp[env1D], sp[env1S], sp[env1R], vsr);
        v.fenv.set(sp[env2A], sp[env2D], sp[env2S], sp[env2R], crate); v.fenv.tick();
        v.menv.set(sp[env3A], sp[env3D], sp[env3S], sp[env3R], crate); v.menv.tick();
        v.time += (float) dt;
        computeVoiceMods(v);
        for (int i = 0; i < 3; ++i) {
            if (sp[lfo1Retrig + i * 6] <= 0.5f) continue;
            float hz = sp[lfo1Sync + i * 6] > 0.5f ? (float) (beatsPerSec / divBeats((int) sp[lfo1Div + i * 6])) : sp[lfo1Rate + i * 6];
            v.lfo[i].advance(hz * std::exp2(v.mod[dLfo1Rate + i] * 3.0f) * (float) dt, rng);
        }
        const float gt = sp[glide];
        if (gt > 0.0005f) v.pitch += (v.target - v.pitch) * (1.0f - std::exp(-(float) dt / (gt * 0.35f)));
        else v.pitch = v.target;
        if (rng.uni() < (float) dt * 3.0f) v.driftTarget = rng.bi();
        v.drift += (v.driftTarget - v.drift) * (float) dt * 2.0f;
    }
    uiVoices.store(active, std::memory_order_relaxed);
    for (int i = 0; i < 3; ++i) uiLfo[i].store(nv && sp[lfo1Retrig + i * 6] > 0.5f ? nv->src[sLfo1 + i] : glfoVal[i], std::memory_order_relaxed);

    if (!hostPlaying) freeBeat += dt * beatsPerSec;
    else ppq += dt * beatsPerSec;
}

// ------------------------------------------------------------------ voice rendering
static inline float warpPhase(float p, int mode, float w) {
    switch (mode) {
        case 1: { float q = p * (1.0f + 7.0f * w); return q - (float) (int) q; }
        case 2: { float kk = (w - 0.5f) * 2.0f; kk = kk >= 0 ? kk * 12.0f : kk * 0.92f; return p * (1.0f + kk) / (1.0f + kk * p); }
        case 3: { float wd = 1.0f - 0.92f * w; return p < wd ? p / wd : 0.0f; }
        case 4: { float m = p < 0.5f ? p : 1.0f - p; return p + (m - p) * w; }
        default: return p;
    }
}

static inline float polyBlep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

void Engine::renderVoice(Voice& v, int nos) {
    const float* p = sp;
    const float* m = v.mod;
    const WavetableBank& bank = WavetableBank::get();
    const float limitHz = os > 1 ? vsr - 26000.0f : vsr * 0.48f;
    const float driftAmt = p[PID::drift];
    const float basePitch = v.pitch + p[masterTune] / 100.0f + bend * p[bendRange] + m[dPitch] * 24.0f + v.drift * driftAmt * 0.12f;

    struct OscSet {
        bool on = false; const Wavetable* wt = nullptr; int units = 1, lvl = 0, warpMode = 0;
        float pos = 0, warp = 0, gain = 0, inc[kMaxUnison] = {}, pl[kMaxUnison] = {}, pr[kMaxUnison] = {};
    } o[2];
    for (int k = 0; k < 2; ++k) {
        const int b = k == 0 ? oscAOn : oscBOn;
        OscSet& s = o[k];
        s.on = p[b] > 0.5f;
        if (!s.on) continue;
        const int dp = k == 0 ? dAPitch : dBPitch;
        s.wt = &bank.table((int) p[b + 1]);
        s.pos = clampf(p[b + 2] + m[k == 0 ? dAPos : dBPos], 0, 1);
        s.warpMode = (int) p[b + 3];
        s.warp = clampf(p[b + 4] + m[k == 0 ? dAWarp : dBWarp], 0, 1);
        const float pitchSt = basePitch + p[b + 5] * 12.0f + p[b + 6] + p[b + 7] / 100.0f + m[dp] * 24.0f;
        const float level = clampf(p[b + 8] + m[k == 0 ? dALevel : dBLevel], 0, 1);
        const float pan = p[b + 9] + m[k == 0 ? dAPan : dBPan];
        s.units = (int) p[b + 10];
        const float det = clampf(p[b + 11] + m[dDetune], 0, 1);
        const float spread = p[b + 12];
        const float maxCents = 60.0f * std::pow(det, 1.6f);
        const float f0 = noteToHz(pitchSt);
        s.gain = level / std::sqrt((float) s.units);
        float worst = f0 * std::exp2(maxCents / 1200.0f);
        float lvlF = std::log2(std::max(kWtMaxHarm * worst / limitHz, 1e-6f));
        s.lvl = std::clamp((int) std::ceil(lvlF), 0, kWtLevels - 1);
        for (int u = 0; u < s.units; ++u) {
            const float t = s.units == 1 ? 0.0f : -1.0f + 2.0f * (float) u / (float) (s.units - 1);
            const float cents = t * maxCents + v.analog[k][u] * driftAmt * 4.0f;
            s.inc[u] = std::min(f0 * std::exp2(cents / 1200.0f) / vsr, 0.49f);
            const float pp = clampf(pan + t * spread, -1, 1);
            const float th = (pp + 1.0f) * kPi * 0.25f;
            s.pl[u] = std::cos(th) * 1.41421f; s.pr[u] = std::sin(th) * 1.41421f;
        }
    }
    if (k_uiNewest == &v) { uiPosA.store(o[0].pos, std::memory_order_relaxed); uiPosB.store(o[1].pos, std::memory_order_relaxed); }

    const float fm = clampf(p[fmAmount] + m[dFm], 0, 1) * 3.0f;
    const bool subOnB = p[subOn] > 0.5f;
    const int subShp = (int) p[subShape];
    const float subInc = std::min(noteToHz(basePitch + 12.0f * (p[subOct] - 2.0f)) / vsr, 0.45f);
    const float subLvl = subOnB ? clampf(p[subLevel] + m[dSub], 0, 1) : 0.0f;
    const float noiseLvl = clampf(p[noiseLevel] + m[dNoise], 0, 1);
    const float ncol = p[noiseColor];
    const float nA = 1.0f - std::exp(-kTwoPi * (ncol < 0 ? 400.0f + 6000.0f * (1.0f + ncol) : 2000.0f) / vsr);

    // filters
    const float key = ((float) v.note - 60.0f);
    const float cd = 1.0f + v.cutDrift * driftAmt * 0.04f;
    const bool f1o = p[f1On] > 0.5f, f2o = p[f2On] > 0.5f;
    float fc1 = p[f1Cut] * cd * std::exp2((p[f1Key] * key + p[f1Env] * v.fenv.v * 96.0f + m[dF1Cut] * 96.0f) / 12.0f);
    float fc2 = p[f2Cut] * cd * std::exp2((p[f2Key] * key + p[f2Env] * v.fenv.v * 96.0f + m[dF2Cut] * 96.0f) / 12.0f);
    if (f1o) v.f1.setup((int) p[f1Type], fc1, p[f1Res] + m[dF1Res], clampf(p[f1Drive] + m[dF1Drive], 0, 1), vsr);
    if (f2o) v.f2.setup((int) p[f2Type], fc2, p[f2Res] + m[dF2Res], 0.0f, vsr);
    const bool parallel = p[fRouting] > 0.5f;
    if (k_uiNewest == &v) uiCutoff.store(clampf(fc1, 20, 20000), std::memory_order_relaxed);

    // amplitude: velocity curve, amp modulation, voice pan
    const float vs = p[velSens];
    const float velG = (1.0f - vs) + vs * v.vel * v.vel;
    const float ampT = velG * clampf(1.0f + m[dAmp], 0.0f, 2.0f) * 0.6f;
    const float vpan = clampf(m[dPan], -1, 1);
    const float th = (vpan + 1.0f) * kPi * 0.25f;
    const float gL = std::cos(th) * 1.41421f, gR = std::sin(th) * 1.41421f;
    const float ampStep = (ampT - v.ampCur) / (float) nos;

    float* outL = busL.data(); float* outR = busR.data();
    for (int i = 0; i < nos; ++i) {
        float bL = 0, bR = 0, bMono = 0;
        if (o[1].on) {
            OscSet& s = o[1];
            for (int u = 0; u < s.units; ++u) {
                float& ph = v.ph[1][u]; ph += s.inc[u]; if (ph >= 1.0f) ph -= 1.0f;
                float x = s.wt->lookup(s.pos, warpPhase(ph, s.warpMode, s.warp), s.lvl);
                bMono += x; bL += x * s.pl[u]; bR += x * s.pr[u];
            }
            bL *= s.gain; bR *= s.gain; bMono *= s.gain;
        }
        float aL = 0, aR = 0;
        if (o[0].on) {
            OscSet& s = o[0];
            const float pm = fm * bMono;
            for (int u = 0; u < s.units; ++u) {
                float& ph = v.ph[0][u]; ph += s.inc[u]; if (ph >= 1.0f) ph -= 1.0f;
                float q = ph + pm; q -= std::floor(q);
                float x = s.wt->lookup(s.pos, warpPhase(q, s.warpMode, s.warp), s.lvl);
                aL += x * s.pl[u]; aR += x * s.pr[u];
            }
            aL *= s.gain; aR *= s.gain;
        }
        float mono = 0;
        if (subLvl > 0) {
            v.subPh += subInc; if (v.subPh >= 1.0f) v.subPh -= 1.0f;
            const float t = v.subPh;
            switch (subShp) {
                case 0: mono += std::sin(kTwoPi * t); break;
                case 1: mono += 1.0f - 4.0f * std::fabs(t - 0.5f); break;
                case 2: mono += (1.0f - 2.0f * t) + polyBlep(t, subInc); break;
                default: { float sq = t < 0.5f ? 1.0f : -1.0f; float t2 = t + 0.5f; t2 -= (float) (int) t2; mono += (sq + polyBlep(t, subInc) - polyBlep(t2, subInc)) * 0.8f; break; }
            }
            mono *= subLvl;
        }
        if (noiseLvl > 0) {
            const float w = rng.bi();
            v.noiseLp += nA * (w - v.noiseLp);
            const float nz = ncol < 0 ? lerpf(w, v.noiseLp * 2.5f, -ncol) : lerpf(w, (w - v.noiseLp), ncol);
            mono += nz * noiseLvl;
        }
        float xL = aL + bL + mono, xR = aR + bR + mono;
        if (f1o || f2o) {
            if (parallel && f1o && f2o) { xL = (v.f1.tick(xL, 0) + v.f2.tick(xL, 0)) * 0.7f; xR = (v.f1.tick(xR, 1) + v.f2.tick(xR, 1)) * 0.7f; }
            else {
                if (f1o) { xL = v.f1.tick(xL, 0); xR = v.f1.tick(xR, 1); }
                if (f2o) { xL = v.f2.tick(xL, 0); xR = v.f2.tick(xR, 1); }
            }
        }
        v.ampCur += ampStep;
        float g = v.ampCur * v.amp.tick();
        if (v.fading) { v.fade -= v.fadeStep; if (v.fade <= 0) { v.fade = 0; v.active = false; } g *= v.fade; }
        outL[i] += xL * g * gL;
        outR[i] += xR * g * gR;
    }
    if (v.amp.st == Env::Idle) v.active = false;
}

// ------------------------------------------------------------------ main loop
void Engine::process(float* L, float* R, int n) {
    int pos = 0;
    while (pos < n) {
        const int m = std::min(kCtrl, n - pos);
        controlTick(m);
        const int nos = m * os;
        std::fill(busL.begin(), busL.begin() + nos, 0.0f);
        std::fill(busR.begin(), busR.begin() + nos, 0.0f);
        k_uiNewest = newestVoice();
        for (auto& v : voices) if (v.active) renderVoice(v, nos);

        // distortion in the oversampled domain
        if (sp[distOn] > 0.5f)
            dist.process(busL.data(), busR.data(), nos, (int) sp[distType], sp[distDrive] + gmod[dDistDrive], sp[distTone], sp[distMix] + gmod[dDistMix]);

        float* oL = L + pos; float* oR = R + pos;
        if (os == 2) for (int i = 0; i < m; ++i) { oL[i] = down[0].down(busL[(size_t) (2 * i)], busL[(size_t) (2 * i + 1)]); oR[i] = down[1].down(busR[(size_t) (2 * i)], busR[(size_t) (2 * i + 1)]); }
        else for (int i = 0; i < m; ++i) { oL[i] = busL[(size_t) i]; oR[i] = busR[(size_t) i]; }
        // DC blocker (~5 Hz): asymmetric waveshaping and pulse partial-cycles can leave an offset
        for (int i = 0; i < m; ++i) {
            float yl = oL[i] - dcX[0] + dcR * dcY[0]; dcX[0] = oL[i]; dcY[0] = yl; oL[i] = yl;
            float yr = oR[i] - dcX[1] + dcR * dcY[1]; dcX[1] = oR[i]; dcY[1] = yr; oR[i] = yr;
        }

        if (sp[phaserOn] > 0.5f) phaser.process(oL, oR, m, sp[phaserRate], sp[phaserDepth], sp[phaserFb], clampf(sp[phaserMix] + gmod[dPhaserMix], 0, 1));
        if (sp[chorusOn] > 0.5f) chorus.process(oL, oR, m, sp[chorusRate], sp[chorusDepth], clampf(sp[chorusMix] + gmod[dChorusMix], 0, 1));
        if (sp[delayOn] > 0.5f) {
            const float secs = sp[delaySync] > 0.5f ? (float) (divBeats((int) sp[delayDiv]) * 60.0 / bpm) : sp[delayTime] * 0.001f;
            delay.process(oL, oR, m, secs, clampf(sp[delayFb] + gmod[dDelayFb], 0, 0.95f), sp[delayTone], sp[delayPing] > 0.5f, clampf(sp[delayMix] + gmod[dDelayMix], 0, 1));
        }
        if (sp[reverbOn] > 0.5f) reverb.process(oL, oR, m, sp[reverbSize], sp[reverbDecay], sp[reverbDamp], sp[reverbPre], clampf(sp[reverbMix] + gmod[dReverbMix], 0, 1));
        if (sp[eqOn] > 0.5f) eq.process(oL, oR, m, sp[eqLow], sp[eqMid], sp[eqMidFreq], sp[eqHigh]);
        if (sp[compOn] > 0.5f) comp.process(oL, oR, m, sp[compThresh], sp[compRatio], sp[compAttack], sp[compRelease], sp[compMakeup]);
        else comp.gr = 0;

        // master gain (ramped), expression, soft reset fade
        const float target = dbToGain(params[masterGain]) * expression;
        const float step = (target - masterCur) / (float) m;
        if (softResetPending && softResetStage == 0) softResetStage = 1;
        for (int i = 0; i < m; ++i) {
            masterCur += step;
            if (softResetStage == 1) { resetGain -= 1.0f / (0.012f * (float) sr); if (resetGain <= 0) { resetGain = 0; softResetStage = 2; } }
            else if (softResetStage == 3) { resetGain += 1.0f / (0.02f * (float) sr); if (resetGain >= 1) { resetGain = 1; softResetStage = 0; } }
            const float g = masterCur * resetGain;
            oL[i] *= g; oR[i] *= g;
        }
        if (softResetStage == 2) {
            for (auto& v : voices) v.active = false;
            down[0].reset(); down[1].reset();
            dist.reset(); phaser.reset(); chorus.reset(); delay.reset(); reverb.reset(); eq.reset(); comp.reset();
            std::copy(params, params + kNumParams, sp);
            masterCur = dbToGain(params[masterGain]) * expression;
            softResetPending = false; softResetStage = 3;
        }
        if (params[limiter] > 0.5f) lim.process(oL, oR, m);

        // never let a NaN/Inf reach the host: if anything blew up, silence and reset state
        bool bad = false;
        for (int i = 0; i < m; ++i) { if (!std::isfinite(oL[i]) || !std::isfinite(oR[i])) { bad = true; break; } peakL = std::max(peakL, std::fabs(oL[i])); peakR = std::max(peakR, std::fabs(oR[i])); }
        if (bad) { std::fill(oL, oL + m, 0.0f); std::fill(oR, oR + m, 0.0f); killEverything(); }
        pos += m;
    }
    uiPeakL.store(std::max(uiPeakL.load(std::memory_order_relaxed), peakL), std::memory_order_relaxed);
    uiPeakR.store(std::max(uiPeakR.load(std::memory_order_relaxed), peakR), std::memory_order_relaxed);
    uiCompGr.store(comp.gr, std::memory_order_relaxed);
    peakL = peakR = 0;
}

} // namespace nacre
