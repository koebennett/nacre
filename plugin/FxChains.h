// Nacre — effect-rack presets. Loading one replaces only the effect section,
// so any sound can be dropped into a different space or treatment.
#pragma once
#include <string>
#include <vector>
#include <cstring>

struct FxChain { const char* name; const char* group; std::string text; };

inline bool isFxParam(const char* id) {
    for (const char* p : { "dist_", "phaser_", "chorus_", "delay_", "reverb_", "eq_", "comp_" })
        if (std::strncmp(id, p, std::strlen(p)) == 0) return true;
    return false;
}

inline const std::vector<FxChain>& fxChains() {
    static const std::vector<FxChain> c = {
        { "Dry", "Basics", "" },
        { "Small Room", "Spaces", "reverb_on=1\nreverb_size=0.25\nreverb_decay=0.7\nreverb_damp=0.6\nreverb_mix=0.18\n" },
        { "Studio Plate", "Spaces", "reverb_on=1\nreverb_size=0.5\nreverb_decay=1.8\nreverb_damp=0.25\nreverb_predelay=20\nreverb_mix=0.25\n" },
        { "Concert Hall", "Spaces", "reverb_on=1\nreverb_size=0.8\nreverb_decay=3.5\nreverb_damp=0.5\nreverb_predelay=25\nreverb_mix=0.3\n" },
        { "Cathedral", "Spaces", "reverb_on=1\nreverb_size=1\nreverb_decay=8\nreverb_damp=0.45\nreverb_predelay=40\nreverb_mix=0.4\n" },
        { "Infinite Space", "Spaces", "reverb_on=1\nreverb_size=1\nreverb_decay=20\nreverb_damp=0.3\nreverb_predelay=60\nreverb_mix=0.5\n" },
        { "Dark Chamber", "Spaces", "reverb_on=1\nreverb_size=0.6\nreverb_decay=2.5\nreverb_damp=0.9\nreverb_mix=0.3\neq_on=1\neq_high=-4\n" },
        { "Bright Shimmer", "Spaces", "reverb_on=1\nreverb_size=0.95\nreverb_decay=10\nreverb_damp=0.05\nreverb_predelay=50\nreverb_mix=0.45\nchorus_on=1\nchorus_rate=0.2\nchorus_depth=0.7\nchorus_mix=0.4\n" },
        { "Slapback", "Echoes", "delay_on=1\ndelay_sync=0\ndelay_time=110\ndelay_feedback=0.1\ndelay_pingpong=0\ndelay_mix=0.25\n" },
        { "Dotted Eighth", "Echoes", "delay_on=1\ndelay_div=1/8 D\ndelay_feedback=0.4\ndelay_mix=0.25\nreverb_on=1\nreverb_mix=0.15\n" },
        { "Ping Pong Quarter", "Echoes", "delay_on=1\ndelay_div=1/4\ndelay_feedback=0.45\ndelay_pingpong=1\ndelay_mix=0.3\n" },
        { "Triplet Bounce", "Echoes", "delay_on=1\ndelay_div=1/8 T\ndelay_feedback=0.5\ndelay_mix=0.25\n" },
        { "Dub Echo", "Echoes", "delay_on=1\ndelay_div=1/4 D\ndelay_feedback=0.75\ndelay_tone=0.3\ndelay_mix=0.35\nreverb_on=1\nreverb_decay=3\nreverb_mix=0.2\nreverb_damp=0.8\n" },
        { "Tape Echo", "Echoes", "delay_on=1\ndelay_sync=0\ndelay_time=320\ndelay_feedback=0.55\ndelay_tone=0.35\ndelay_pingpong=0\ndelay_mix=0.3\ndist_on=1\ndist_type=Tube\ndist_drive=0.15\ndist_mix=0.5\ndist_tone=0.6\n" },
        { "Ambient Wash", "Echoes", "delay_on=1\ndelay_div=1/2\ndelay_feedback=0.7\ndelay_tone=0.5\ndelay_mix=0.3\nreverb_on=1\nreverb_size=1\nreverb_decay=12\nreverb_mix=0.45\n" },
        { "Juno Chorus", "Modulation", "chorus_on=1\nchorus_rate=0.5\nchorus_depth=0.55\nchorus_mix=0.6\n" },
        { "Fast Chorus", "Modulation", "chorus_on=1\nchorus_rate=3\nchorus_depth=0.25\nchorus_mix=0.5\n" },
        { "Wide Ensemble", "Modulation", "chorus_on=1\nchorus_rate=0.9\nchorus_depth=0.8\nchorus_mix=0.7\nreverb_on=1\nreverb_mix=0.2\n" },
        { "Slow Phaser", "Modulation", "phaser_on=1\nphaser_rate=0.12\nphaser_depth=0.8\nphaser_feedback=0.5\nphaser_mix=0.6\n" },
        { "Jet Phaser", "Modulation", "phaser_on=1\nphaser_rate=0.3\nphaser_depth=1\nphaser_feedback=0.85\nphaser_mix=0.7\n" },
        { "Seventies Phase", "Modulation", "phaser_on=1\nphaser_rate=0.6\nphaser_depth=0.6\nphaser_feedback=0.3\nphaser_mix=0.5\nchorus_on=1\nchorus_mix=0.3\n" },
        { "Warm Saturation", "Colour", "dist_on=1\ndist_type=Tube\ndist_drive=0.2\ndist_mix=0.7\ndist_tone=0.7\neq_on=1\neq_low=1.5\neq_high=-1\n" },
        { "Overdrive", "Colour", "dist_on=1\ndist_type=Soft\ndist_drive=0.5\ndist_mix=1\ndist_tone=0.6\n" },
        { "Fuzz Box", "Colour", "dist_on=1\ndist_type=Hard\ndist_drive=0.75\ndist_mix=1\ndist_tone=0.45\neq_on=1\neq_mid=3\neq_mid_freq=1200\n" },
        { "Wavefolder", "Colour", "dist_on=1\ndist_type=Fold\ndist_drive=0.45\ndist_mix=0.8\ndist_tone=0.8\n" },
        { "Lo-Fi Crush", "Colour", "dist_on=1\ndist_type=Bit Crush\ndist_drive=0.55\ndist_mix=0.6\ndist_tone=0.5\neq_on=1\neq_high=-6\n" },
        { "Retro Sampler", "Colour", "dist_on=1\ndist_type=Downsample\ndist_drive=0.3\ndist_mix=1\ndist_tone=0.55\n" },
        { "Telephone", "Colour", "eq_on=1\neq_low=-12\neq_mid=10\neq_mid_freq=1500\neq_high=-12\ndist_on=1\ndist_type=Soft\ndist_drive=0.3\n" },
        { "Radio", "Colour", "eq_on=1\neq_low=-9\neq_mid=6\neq_mid_freq=2200\neq_high=-8\ndist_on=1\ndist_type=Bit Crush\ndist_drive=0.35\ndist_mix=0.4\nreverb_on=1\nreverb_size=0.2\nreverb_mix=0.12\n" },
        { "Glue Compressor", "Dynamics", "comp_on=1\ncomp_threshold=-16\ncomp_ratio=2.5\ncomp_attack=20\ncomp_release=200\ncomp_makeup=3\n" },
        { "Punch", "Dynamics", "comp_on=1\ncomp_threshold=-20\ncomp_ratio=4\ncomp_attack=25\ncomp_release=80\ncomp_makeup=5\n" },
        { "Squash", "Dynamics", "comp_on=1\ncomp_threshold=-30\ncomp_ratio=12\ncomp_attack=1\ncomp_release=120\ncomp_makeup=12\n" },
        { "Bass Weight", "Dynamics", "eq_on=1\neq_low=4\neq_mid=-2\neq_mid_freq=400\ncomp_on=1\ncomp_threshold=-18\ncomp_ratio=4\ncomp_attack=10\ncomp_release=100\ncomp_makeup=3\n" },
        { "Air & Sparkle", "Tone", "eq_on=1\neq_high=6\neq_mid=-1.5\neq_mid_freq=500\n" },
        { "Warm & Round", "Tone", "eq_on=1\neq_low=3\neq_high=-4\n" },
        { "Mid Scoop", "Tone", "eq_on=1\neq_mid=-6\neq_mid_freq=800\n" },
        { "Synthwave Bus", "Production", "chorus_on=1\nchorus_rate=0.5\nchorus_mix=0.5\ndelay_on=1\ndelay_div=1/8 D\ndelay_feedback=0.45\ndelay_mix=0.25\nreverb_on=1\nreverb_size=0.85\nreverb_decay=4\nreverb_mix=0.3\n" },
        { "Lo-Fi Bedroom", "Production", "dist_on=1\ndist_type=Bit Crush\ndist_drive=0.3\ndist_mix=0.35\ndist_tone=0.4\nchorus_on=1\nchorus_rate=0.3\nchorus_depth=0.6\nchorus_mix=0.4\neq_on=1\neq_high=-6\nreverb_on=1\nreverb_mix=0.2\nreverb_damp=0.8\n" },
        { "Big Room", "Production", "comp_on=1\ncomp_threshold=-18\ncomp_ratio=4\ncomp_makeup=4\nreverb_on=1\nreverb_size=0.9\nreverb_decay=3\nreverb_mix=0.25\ndelay_on=1\ndelay_div=1/8 D\ndelay_mix=0.15\n" },
        { "Cinematic", "Production", "reverb_on=1\nreverb_size=1\nreverb_decay=7\nreverb_predelay=45\nreverb_mix=0.4\ndelay_on=1\ndelay_div=1/4 D\ndelay_feedback=0.5\ndelay_tone=0.45\ndelay_mix=0.2\neq_on=1\neq_low=2\neq_high=2\n" },
        { "Trap Bus", "Production", "dist_on=1\ndist_type=Soft\ndist_drive=0.25\ndist_mix=0.5\ncomp_on=1\ncomp_threshold=-14\ncomp_ratio=3\ncomp_makeup=2\nreverb_on=1\nreverb_size=0.4\nreverb_mix=0.12\n" },
        { "Underwater", "Production", "eq_on=1\neq_high=-12\neq_mid=-4\neq_mid_freq=2500\nchorus_on=1\nchorus_rate=0.25\nchorus_depth=1\nchorus_mix=0.6\nreverb_on=1\nreverb_damp=1\nreverb_decay=4\nreverb_mix=0.35\n" },
    };
    return c;
}
