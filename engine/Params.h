// Nacre — parameter table. Single source of truth shared by the engine,
// the plugin wrapper, the preset format and the tests.
#pragma once
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

namespace nacre {

enum PType { kFloat, kInt, kBool, kChoice };

struct ParamDef {
    const char* id;      // stable identifier used in presets and host automation
    const char* name;    // human readable
    PType type;
    float lo, hi, def;
    float skew;          // 1 = linear, -1 = logarithmic, otherwise power curve
    const char* unit;    // "Hz" "s" "ms" "dB" "%" "st" "ct" or ""
    const char* choices; // '|' separated, for kChoice
};

// Choice lists. Only ever append to these: presets store the index.
#define NACRE_WT_NAMES "Basic Shapes|Saw Tilt|PWM|Saw-Square|Saturated Sine|Rectify|Tri Fold|Sine Fold|Sine Sync|Saw Sync|Resonant Sweep|Overtone Sweep|Hollow Build|Octave Stack|Organ|FM Saw|FM Hollow|FM Reed|FM Glass|FM Bell|Bit Crush|Stairs|Vowels|Choir|Talk Bass|Growl|E-Piano|Plucked|Bowed|Brass|Reed|Flute|Metallic|Crystal|Spectral Flux|Shimmer|Spectral Comb|Spectral Phaser|Impulse|Air|Power Fifth|Harmonic Major|Harmonic Minor"
#define NACRE_WARP_NAMES "Off|Sync|Bend|Squeeze|Mirror"
#define NACRE_FILTER_NAMES "Low 12|Low 24|Ladder|High 12|High 24|Band|Notch|Comb|Vowel"
#define NACRE_LFO_NAMES "Sine|Triangle|Ramp Up|Ramp Down|Square|Sample & Hold|Smooth Random"
#define NACRE_DIV_NAMES "8 Bars|4 Bars|2 Bars|1 Bar|1/2 D|1/2|1/2 T|1/4 D|1/4|1/4 T|1/8 D|1/8|1/8 T|1/16 D|1/16|1/16 T|1/32|1/64"
#define NACRE_SRC_NAMES "None|LFO 1|LFO 2|LFO 3|Amp Env|Filter Env|Mod Env|Velocity|Key Track|Mod Wheel|Aftertouch|Pitch Bend|Macro 1|Macro 2|Macro 3|Macro 4|Random"
#define NACRE_DST_NAMES "None|Pitch|Osc A Pitch|Osc A Position|Osc A Warp|Osc A Level|Osc A Pan|Osc B Pitch|Osc B Position|Osc B Warp|Osc B Level|Osc B Pan|FM Amount|Sub Level|Noise Level|Filter 1 Cutoff|Filter 1 Reso|Filter 1 Drive|Filter 2 Cutoff|Filter 2 Reso|Amp|Pan|Unison Detune|LFO 1 Rate|LFO 2 Rate|LFO 3 Rate|Dist Drive|Dist Mix|Phaser Mix|Chorus Mix|Delay Mix|Delay Feedback|Reverb Mix"
#define NACRE_DIST_NAMES "Soft|Hard|Tube|Fold|Bit Crush|Downsample"
#define NACRE_ARP_NAMES "Up|Down|Up Down|Random|As Played|Chord"

enum Src { sNone, sLfo1, sLfo2, sLfo3, sEnv1, sEnv2, sEnv3, sVel, sKey, sWheel, sAT, sBend, sM1, sM2, sM3, sM4, sRand, kNumSrc };
enum Dst { dNone, dPitch, dAPitch, dAPos, dAWarp, dALevel, dAPan, dBPitch, dBPos, dBWarp, dBLevel, dBPan, dFm, dSub, dNoise,
           dF1Cut, dF1Res, dF1Drive, dF2Cut, dF2Res, dAmp, dPan, dDetune, dLfo1Rate, dLfo2Rate, dLfo3Rate,
           dDistDrive, dDistMix, dPhaserMix, dChorusMix, dDelayMix, dDelayFb, dReverbMix, kNumDst };
constexpr int kFirstGlobalDst = dDistDrive;
enum FilterType { fLP12, fLP24, fLadder, fHP12, fHP24, fBand, fNotch, fComb, fVowel };
constexpr int kModSlots = 12;

// Beats (quarter notes) for each entry of NACRE_DIV_NAMES.
inline double divBeats(int i) {
    static const double b[] = {32,16,8,4,3,2,4.0/3,1.5,1,2.0/3,0.75,0.5,1.0/3,0.375,0.25,1.0/6,0.125,0.0625};
    return b[std::clamp(i, 0, 17)];
}

#define P_OSC(X,p,N,on,lvl) \
 X(p##On,      #p "_on",      N " On",         kBool,  0,1,on,   1,"","") \
 X(p##Table,   #p "_table",   N " Wavetable",  kChoice,0,0,0,    1,"",NACRE_WT_NAMES) \
 X(p##Pos,     #p "_pos",     N " Position",   kFloat, 0,1,0,    1,"%","") \
 X(p##WarpMode,#p "_warpmode",N " Warp Mode",  kChoice,0,0,0,    1,"",NACRE_WARP_NAMES) \
 X(p##Warp,    #p "_warp",    N " Warp",       kFloat, 0,1,0,    1,"%","") \
 X(p##Oct,     #p "_oct",     N " Octave",     kInt,  -3,3,0,    1,"","") \
 X(p##Semi,    #p "_semi",    N " Semitone",   kInt, -12,12,0,   1,"st","") \
 X(p##Fine,    #p "_fine",    N " Fine",       kFloat,-100,100,0,1,"ct","") \
 X(p##Level,   #p "_level",   N " Level",      kFloat, 0,1,lvl,  1,"%","") \
 X(p##Pan,     #p "_pan",     N " Pan",        kFloat,-1,1,0,    1,"%","") \
 X(p##Unison,  #p "_unison",  N " Unison",     kInt,   1,7,1,    1,"","") \
 X(p##Detune,  #p "_detune",  N " Detune",     kFloat, 0,1,0.25f,1,"%","") \
 X(p##Spread,  #p "_spread",  N " Spread",     kFloat, 0,1,0.7f, 1,"%","") \
 X(p##Phase,   #p "_phase",   N " Phase Random",kFloat,0,1,1,    1,"%","")

#define P_ENV(X,p,N,a,d,s,r) \
 X(p##A, #p "_attack",  N " Attack",  kFloat, 0.0005f,10,a, 3,"s","") \
 X(p##D, #p "_decay",   N " Decay",   kFloat, 0.001f,10,d,  3,"s","") \
 X(p##S, #p "_sustain", N " Sustain", kFloat, 0,1,s,        1,"%","") \
 X(p##R, #p "_release", N " Release", kFloat, 0.001f,15,r,  3,"s","")

#define P_LFO(X,p,N,rate) \
 X(p##Shape, #p "_shape", N " Shape",   kChoice,0,0,0,      1,"",NACRE_LFO_NAMES) \
 X(p##Rate,  #p "_rate",  N " Rate",    kFloat, 0.02f,30,rate,-1,"Hz","") \
 X(p##Sync,  #p "_sync",  N " Sync",    kBool,  0,1,0,      1,"","") \
 X(p##Div,   #p "_div",   N " Division",kChoice,0,0,8,      1,"",NACRE_DIV_NAMES) \
 X(p##Fade,  #p "_fade",  N " Fade In", kFloat, 0,5,0,      2,"s","") \
 X(p##Retrig,#p "_retrig",N " Per Voice",kBool, 0,1,0,      1,"","")

#define P_MOD(X,n) \
 X(mod##n##Src, "mod" #n "_src", "Mod " #n " Source", kChoice,0,0,0, 1,"",NACRE_SRC_NAMES) \
 X(mod##n##Via, "mod" #n "_via", "Mod " #n " Via",    kChoice,0,0,0, 1,"",NACRE_SRC_NAMES) \
 X(mod##n##Dst, "mod" #n "_dst", "Mod " #n " Target", kChoice,0,0,0, 1,"",NACRE_DST_NAMES) \
 X(mod##n##Amt, "mod" #n "_amt", "Mod " #n " Amount", kFloat,-1,1,0, 1,"%","")

#define NACRE_PARAMS(X) \
 X(masterGain, "master_gain", "Volume",        kFloat,-48,12,0,     1,"dB","") \
 X(masterTune, "master_tune", "Tune",          kFloat,-100,100,0,  1,"ct","") \
 X(bendRange,  "bend_range",  "Bend Range",    kInt,   0,24,2,     1,"st","") \
 X(voiceMode,  "voice_mode",  "Voice Mode",    kChoice,0,0,0,      1,"","Poly|Mono|Legato") \
 X(polyphony,  "polyphony",   "Voices",        kInt,   1,32,16,    1,"","") \
 X(glide,      "glide",       "Glide",         kFloat, 0,2,0,      3,"s","") \
 X(drift,      "drift",       "Analog Drift",  kFloat, 0,1,0.15f,  1,"%","") \
 X(velSens,    "vel_sens",    "Velocity",      kFloat, 0,1,0.6f,   1,"%","") \
 X(quality,    "quality",     "Quality",       kChoice,0,0,1,      1,"","Eco|High") \
 X(limiter,    "limiter",     "Limiter",       kBool,  0,1,1,      1,"","") \
 P_OSC(X,oscA,"Osc A",1,0.8f) \
 P_OSC(X,oscB,"Osc B",0,0.8f) \
 X(fmAmount,   "fm_amount",   "FM B to A",     kFloat, 0,1,0,      2,"%","") \
 X(subOn,      "sub_on",      "Sub On",        kBool,  0,1,0,      1,"","") \
 X(subShape,   "sub_shape",   "Sub Shape",     kChoice,0,0,0,      1,"","Sine|Triangle|Saw|Square") \
 X(subOct,     "sub_oct",     "Sub Octave",    kChoice,0,0,1,      1,"","-2|-1|0") \
 X(subLevel,   "sub_level",   "Sub Level",     kFloat, 0,1,0.5f,   1,"%","") \
 X(noiseLevel, "noise_level", "Noise Level",   kFloat, 0,1,0,      1,"%","") \
 X(noiseColor, "noise_color", "Noise Color",   kFloat,-1,1,0,      1,"%","") \
 X(f1On,       "filter1_on",  "Filter 1 On",   kBool,  0,1,1,      1,"","") \
 X(f1Type,     "filter1_type","Filter 1 Type", kChoice,0,0,1,      1,"",NACRE_FILTER_NAMES) \
 X(f1Cut,      "filter1_cutoff","Filter 1 Cutoff",kFloat,20,20000,20000,-1,"Hz","") \
 X(f1Res,      "filter1_reso","Filter 1 Resonance",kFloat,0,1,0.1f,1,"%","") \
 X(f1Drive,    "filter1_drive","Filter 1 Drive",kFloat,0,1,0,      1,"%","") \
 X(f1Key,      "filter1_key", "Filter 1 Key Track",kFloat,0,1,0,   1,"%","") \
 X(f1Env,      "filter1_env", "Filter 1 Env Amount",kFloat,-1,1,0, 1,"%","") \
 X(f2On,       "filter2_on",  "Filter 2 On",   kBool,  0,1,0,      1,"","") \
 X(f2Type,     "filter2_type","Filter 2 Type", kChoice,0,0,3,      1,"",NACRE_FILTER_NAMES) \
 X(f2Cut,      "filter2_cutoff","Filter 2 Cutoff",kFloat,20,20000,200,-1,"Hz","") \
 X(f2Res,      "filter2_reso","Filter 2 Resonance",kFloat,0,1,0.1f,1,"%","") \
 X(f2Key,      "filter2_key", "Filter 2 Key Track",kFloat,0,1,0,   1,"%","") \
 X(f2Env,      "filter2_env", "Filter 2 Env Amount",kFloat,-1,1,0, 1,"%","") \
 X(fRouting,   "filter_routing","Filter Routing",kChoice,0,0,0,    1,"","Serial|Parallel") \
 P_ENV(X,env1,"Amp Env",   0.002f,0.3f,1.0f,0.2f) \
 P_ENV(X,env2,"Filter Env",0.002f,0.4f,0.3f,0.3f) \
 P_ENV(X,env3,"Mod Env",   0.002f,0.5f,0.0f,0.3f) \
 P_LFO(X,lfo1,"LFO 1",2.0f) P_LFO(X,lfo2,"LFO 2",0.5f) P_LFO(X,lfo3,"LFO 3",5.5f) \
 X(macro1, "macro1", "Macro 1 Tone",   kFloat,0,1,0,1,"%","") \
 X(macro2, "macro2", "Macro 2 Motion", kFloat,0,1,0,1,"%","") \
 X(macro3, "macro3", "Macro 3 Space",  kFloat,0,1,0,1,"%","") \
 X(macro4, "macro4", "Macro 4 Grit",   kFloat,0,1,0,1,"%","") \
 P_MOD(X,1) P_MOD(X,2) P_MOD(X,3) P_MOD(X,4) P_MOD(X,5) P_MOD(X,6) \
 P_MOD(X,7) P_MOD(X,8) P_MOD(X,9) P_MOD(X,10) P_MOD(X,11) P_MOD(X,12) \
 X(arpOn,    "arp_on",    "Arp On",       kBool,  0,1,0,    1,"","") \
 X(arpMode,  "arp_mode",  "Arp Mode",     kChoice,0,0,0,    1,"",NACRE_ARP_NAMES) \
 X(arpOct,   "arp_octaves","Arp Octaves", kInt,   1,4,1,    1,"","") \
 X(arpDiv,   "arp_div",   "Arp Rate",     kChoice,0,0,14,   1,"",NACRE_DIV_NAMES) \
 X(arpGate,  "arp_gate",  "Arp Gate",     kFloat, 0.05f,1,0.5f,1,"%","") \
 X(arpSwing, "arp_swing", "Arp Swing",    kFloat, 0,0.75f,0,1,"%","") \
 X(arpLatch, "arp_latch", "Arp Latch",    kBool,  0,1,0,    1,"","") \
 X(distOn,   "dist_on",   "Distortion On",kBool,  0,1,0,    1,"","") \
 X(distType, "dist_type", "Distortion Type",kChoice,0,0,0,  1,"",NACRE_DIST_NAMES) \
 X(distDrive,"dist_drive","Distortion Drive",kFloat,0,1,0.3f,1,"%","") \
 X(distTone, "dist_tone", "Distortion Tone",kFloat,0,1,0.8f,1,"%","") \
 X(distMix,  "dist_mix",  "Distortion Mix",kFloat,0,1,1,    1,"%","") \
 X(phaserOn, "phaser_on", "Phaser On",    kBool,  0,1,0,    1,"","") \
 X(phaserRate,"phaser_rate","Phaser Rate",kFloat, 0.02f,10,0.4f,-1,"Hz","") \
 X(phaserDepth,"phaser_depth","Phaser Depth",kFloat,0,1,0.7f,1,"%","") \
 X(phaserFb, "phaser_feedback","Phaser Feedback",kFloat,0,0.95f,0.4f,1,"%","") \
 X(phaserMix,"phaser_mix","Phaser Mix",   kFloat, 0,1,0.5f, 1,"%","") \
 X(chorusOn, "chorus_on", "Chorus On",    kBool,  0,1,0,    1,"","") \
 X(chorusRate,"chorus_rate","Chorus Rate",kFloat, 0.05f,5,0.6f,-1,"Hz","") \
 X(chorusDepth,"chorus_depth","Chorus Depth",kFloat,0,1,0.5f,1,"%","") \
 X(chorusMix,"chorus_mix","Chorus Mix",   kFloat, 0,1,0.5f, 1,"%","") \
 X(delayOn,  "delay_on",  "Delay On",     kBool,  0,1,0,    1,"","") \
 X(delaySync,"delay_sync","Delay Sync",   kBool,  0,1,1,    1,"","") \
 X(delayTime,"delay_time","Delay Time",   kFloat, 1,2000,350,-1,"ms","") \
 X(delayDiv, "delay_div", "Delay Division",kChoice,0,0,10,  1,"",NACRE_DIV_NAMES) \
 X(delayFb,  "delay_feedback","Delay Feedback",kFloat,0,0.95f,0.4f,1,"%","") \
 X(delayTone,"delay_tone","Delay Tone",   kFloat, 0,1,0.6f, 1,"%","") \
 X(delayPing,"delay_pingpong","Delay Ping Pong",kBool,0,1,1,1,"","") \
 X(delayMix, "delay_mix", "Delay Mix",    kFloat, 0,1,0.25f,1,"%","") \
 X(reverbOn, "reverb_on", "Reverb On",    kBool,  0,1,0,    1,"","") \
 X(reverbSize,"reverb_size","Reverb Size",kFloat, 0,1,0.6f, 1,"%","") \
 X(reverbDecay,"reverb_decay","Reverb Decay",kFloat,0.2f,20,2.5f,-1,"s","") \
 X(reverbDamp,"reverb_damp","Reverb Damping",kFloat,0,1,0.5f,1,"%","") \
 X(reverbPre,"reverb_predelay","Reverb Pre-Delay",kFloat,0,200,10,1,"ms","") \
 X(reverbMix,"reverb_mix","Reverb Mix",   kFloat, 0,1,0.25f,1,"%","") \
 X(eqOn,     "eq_on",     "EQ On",        kBool,  0,1,0,    1,"","") \
 X(eqLow,    "eq_low",    "EQ Low",       kFloat,-12,12,0,  1,"dB","") \
 X(eqMid,    "eq_mid",    "EQ Mid",       kFloat,-12,12,0,  1,"dB","") \
 X(eqMidFreq,"eq_mid_freq","EQ Mid Freq", kFloat, 200,8000,1000,-1,"Hz","") \
 X(eqHigh,   "eq_high",   "EQ High",      kFloat,-12,12,0,  1,"dB","") \
 X(compOn,   "comp_on",   "Compressor On",kBool,  0,1,0,    1,"","") \
 X(compThresh,"comp_threshold","Compressor Threshold",kFloat,-40,0,-12,1,"dB","") \
 X(compRatio,"comp_ratio","Compressor Ratio",kFloat,1,20,4, 2,"","") \
 X(compAttack,"comp_attack","Compressor Attack",kFloat,0.1f,100,10,-1,"ms","") \
 X(compRelease,"comp_release","Compressor Release",kFloat,10,1000,150,-1,"ms","") \
 X(compMakeup,"comp_makeup","Compressor Makeup",kFloat,0,24,0,1,"dB","")

enum PID : int {
#define X(e,id,name,type,lo,hi,def,skew,unit,ch) e,
    NACRE_PARAMS(X)
#undef X
    kNumParams
};

constexpr float countChoices(const char* s) {
    int n = 0;
    for (const char* c = s; *c; ++c) if (*c == '|') ++n;
    return (float) n;
}

inline const ParamDef kParams[kNumParams] = {
#define X(e,id,name,type,lo,hi,def,skew,unit,ch) { id, name, type, (float)(lo), (type == kChoice ? countChoices(ch) : (float)(hi)), (float)(def), (float)(skew), unit, ch },
    NACRE_PARAMS(X)
#undef X
};

inline int findParam(const std::string& id) {
    for (int i = 0; i < kNumParams; ++i) if (id == kParams[i].id) return i;
    return -1;
}

inline float snapValue(const ParamDef& d, float v) {
    v = std::clamp(v, d.lo, d.hi);
    return d.type == kFloat ? v : std::round(v);
}

inline float fromNorm(const ParamDef& d, float n) {
    n = std::clamp(n, 0.0f, 1.0f);
    if (d.type != kFloat) return std::round(d.lo + (d.hi - d.lo) * n);
    if (d.skew < 0) return d.lo * std::pow(d.hi / d.lo, n);
    if (d.skew != 1.0f) n = std::pow(n, d.skew);
    return d.lo + (d.hi - d.lo) * n;
}

inline float toNorm(const ParamDef& d, float v) {
    v = std::clamp(v, d.lo, d.hi);
    if (d.type != kFloat) return (v - d.lo) / (d.hi - d.lo);
    if (d.skew < 0) return std::log(v / d.lo) / std::log(d.hi / d.lo);
    float n = (v - d.lo) / (d.hi - d.lo);
    return d.skew != 1.0f ? std::pow(n, 1.0f / d.skew) : n;
}

inline std::vector<std::string> splitChoices(const char* s) {
    std::vector<std::string> out;
    std::string cur;
    for (const char* c = s; *c; ++c) { if (*c == '|') { out.push_back(cur); cur.clear(); } else cur += *c; }
    out.push_back(cur);
    return out;
}

inline std::string formatValue(const ParamDef& d, float v) {
    char b[64];
    const std::string u = d.unit;
    if (d.type == kBool) return v > 0.5f ? "On" : "Off";
    if (d.type == kChoice) { auto c = splitChoices(d.choices); return c[(size_t) std::clamp((int) std::lround(v), 0, (int) c.size() - 1)]; }
    if (d.type == kInt) { if (u == "st") snprintf(b, sizeof b, "%+d st", (int) std::lround(v)); else snprintf(b, sizeof b, "%d", (int) std::lround(v)); return b; }
    if (u == "Hz") { if (v >= 1000) snprintf(b, sizeof b, "%.2f kHz", v / 1000); else if (v >= 100) snprintf(b, sizeof b, "%.0f Hz", v); else if (v >= 10) snprintf(b, sizeof b, "%.1f Hz", v); else snprintf(b, sizeof b, "%.2f Hz", v); }
    else if (u == "s") { if (v >= 1) snprintf(b, sizeof b, "%.2f s", v); else if (v >= 0.01f) snprintf(b, sizeof b, "%.0f ms", v * 1000); else snprintf(b, sizeof b, "%.1f ms", v * 1000); }
    else if (u == "ms") { if (v >= 10) snprintf(b, sizeof b, "%.0f ms", v); else snprintf(b, sizeof b, "%.1f ms", v); }
    else if (u == "dB") snprintf(b, sizeof b, "%+.1f dB", v);
    else if (u == "%") snprintf(b, sizeof b, "%.0f%%", v * 100);
    else if (u == "ct") snprintf(b, sizeof b, "%+.0f ct", v);
    else snprintf(b, sizeof b, "%.2f", v);
    return b;
}

inline float parseValue(const ParamDef& d, const std::string& s) {
    if (d.type == kChoice) {
        auto c = splitChoices(d.choices);
        for (size_t i = 0; i < c.size(); ++i) if (c[i] == s) return (float) i;
    }
    if (d.type == kBool && (s == "On" || s == "on")) return 1;
    float v = (float) std::atof(s.c_str());
    const std::string u = d.unit;
    if (u == "Hz" && s.find('k') != std::string::npos) v *= 1000;
    if (u == "s" && s.find("ms") != std::string::npos) v /= 1000;
    if (u == "%") v /= 100;
    return snapValue(d, v);
}

} // namespace nacre
