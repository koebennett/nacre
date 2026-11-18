#!/usr/bin/env python3
"""Nacre factory library generator.

Every factory sound is designed here as a small recipe (a base patch plus the
musically meaningful ways it varies), then written to presets/<Category>/<Name>.nacre.
`level_presets.py` renders each one, measures integrated loudness and sets its
volume so the whole library plays back at the same perceived level.

Conventions that make the library feel consistent in use:
  * Macro 1 = Tone, Macro 2 = Motion, Macro 3 = Space, Macro 4 = Grit on every patch
  * Mod wheel = vibrato on melodic patches (brightness/motion on pads, beds and FX)
  * Aftertouch = brightness
  * LFO 1 belongs to the sound design, LFO 2 to the Motion macro, LFO 3 to vibrato
"""
import os, re, sys, random, shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'presets')

class P(dict):
    def __init__(s, **kw):
        super().__init__(**kw); s.mods = []; s.lfo2_used = False
    def mod(s, src, dst, amt, via='None'):
        s.mods.append((src, via, dst, round(amt, 4))); return s

# ---------------------------------------------------------------- helpers
def osc(p, w, table, pos=0.0, level=None, uni=None, det=None, spread=None, oct=None, semi=None, fine=None,
        warp=None, wm=None, phase=None, pan=None):
    k = 'osc' + w + '_'
    p[k + 'on'] = 1; p[k + 'table'] = table; p[k + 'pos'] = pos
    for name, val in (('level', level), ('unison', uni), ('detune', det), ('spread', spread), ('oct', oct), ('semi', semi),
                      ('fine', fine), ('warp', warp), ('warpmode', wm), ('phase', phase), ('pan', pan)):
        if val is not None: p[k + name] = val
    return p

def sub(p, level=0.5, shape='Sine', octv='-1'):
    p.update(sub_on=1, sub_level=level, sub_shape=shape, sub_oct=octv); return p

def noise(p, level, color=0.0):
    p.update(noise_level=level, noise_color=color); return p

def filt(p, ftype, cut, res=0.1, env=0.0, key=0.0, drive=0.0):
    p.update(filter1_on=1, filter1_type=ftype, filter1_cutoff=cut, filter1_reso=res, filter1_env=env, filter1_key=key, filter1_drive=drive); return p

def filt2(p, ftype, cut, res=0.1, env=0.0, key=0.0, parallel=False):
    p.update(filter2_on=1, filter2_type=ftype, filter2_cutoff=cut, filter2_reso=res, filter2_env=env, filter2_key=key)
    if parallel: p['filter_routing'] = 'Parallel'
    return p

def nofilt(p): p['filter1_on'] = 0; return p

def amp(p, a, d, s, r): p.update(env1_attack=a, env1_decay=d, env1_sustain=s, env1_release=r); return p
def fenv(p, a, d, s, r): p.update(env2_attack=a, env2_decay=d, env2_sustain=s, env2_release=r); return p
def menv(p, a, d, s, r): p.update(env3_attack=a, env3_decay=d, env3_sustain=s, env3_release=r); return p

def lfo(p, n, shape='Sine', rate=None, div=None, fade=None, retrig=None):
    k = 'lfo%d_' % n
    p[k + 'shape'] = shape
    if rate is not None: p[k + 'rate'] = rate
    if div is not None: p[k + 'sync'] = 1; p[k + 'div'] = div
    if fade is not None: p[k + 'fade'] = fade
    if retrig is not None: p[k + 'retrig'] = retrig
    if n == 2: p.lfo2_used = True
    return p

def mono(p, glide=0.0, legato=True):
    p.update(voice_mode='Legato' if legato else 'Mono', glide=glide); return p

def rev(p, mix, size=0.6, decay=2.5, damp=0.5, pre=10):
    p.update(reverb_on=1, reverb_mix=mix, reverb_size=size, reverb_decay=decay, reverb_damp=damp, reverb_predelay=pre); return p

def dly(p, mix, div='1/8 D', fb=0.35, tone=0.55, ping=1):
    p.update(delay_on=1, delay_mix=mix, delay_div=div, delay_feedback=fb, delay_tone=tone, delay_pingpong=ping); return p

def cho(p, mix=0.5, rate=0.6, depth=0.5): p.update(chorus_on=1, chorus_mix=mix, chorus_rate=rate, chorus_depth=depth); return p
def pha(p, mix=0.5, rate=0.3, depth=0.7, fb=0.4): p.update(phaser_on=1, phaser_mix=mix, phaser_rate=rate, phaser_depth=depth, phaser_feedback=fb); return p
def dist(p, dtype='Soft', drive=0.3, mix=1.0, tone=0.8): p.update(dist_on=1, dist_type=dtype, dist_drive=drive, dist_mix=mix, dist_tone=tone); return p
def eq(p, low=0, mid=0, freq=1000, high=0): p.update(eq_on=1, eq_low=low, eq_mid=mid, eq_mid_freq=freq, eq_high=high); return p
def comp(p, thr=-14, ratio=3, atk=10, rel=150, makeup=2): p.update(comp_on=1, comp_threshold=thr, comp_ratio=ratio, comp_attack=atk, comp_release=rel, comp_makeup=makeup); return p
def arp(p, mode='Up', div='1/16', octs=1, gate=0.5, swing=0.0):
    p.update(arp_on=1, arp_mode=mode, arp_div=div, arp_octaves=octs, arp_gate=gate, arp_swing=swing); return p

def pick(v, seq): return seq[v % len(seq)]

ST = ['Sine', 'Triangle', 'Ramp Up', 'Ramp Down', 'Square', 'Sample & Hold', 'Smooth Random']

# Per-category conventions: (phrase used for level matching, kind of wheel behaviour, motion LFO)
CATS = {
    'Pads':                ('chord', 'bright', ('Sine', 0.18, None)),
    'Atmospheres':         ('drone', 'bright', ('Smooth Random', 0.25, None)),
    'Keys':                ('keys',  'vibrato', ('Sine', 4.5, None)),
    'Bass':                ('bass',  'vibrato', ('Sine', None, '1/8')),
    'Leads':               ('lead',  'vibrato', ('Triangle', None, '1/4')),
    'Plucks':              ('pluck', 'vibrato', ('Sine', None, '1/2')),
    'Bells & Mallets':     ('pluck', 'vibrato', ('Sine', 0.3, None)),
    'Strings':             ('chord', 'vibrato', ('Sine', 0.2, None)),
    'Brass & Winds':       ('lead',  'vibrato', ('Sine', 0.25, None)),
    'Organs':              ('keys',  'vibrato', ('Sine', 6.0, None)),
    'Choirs & Vox':        ('chord', 'vibrato', ('Smooth Random', 0.3, None)),
    'Arps & Sequences':    ('arp',   'bright', ('Triangle', None, '1 Bar')),
    'Chords & Stabs':      ('keys',  'bright', ('Sine', None, '1/4')),
    'Drones':              ('drone', 'bright', ('Smooth Random', 0.08, None)),
    'FX & Risers':         ('note',  'bright', ('Sample & Hold', None, '1/16')),
    'Percussion':          ('pluck', 'bright', ('Sine', None, '1/8')),
}

RECIPES = []
def recipe(cat, names, tags=''):
    def deco(fn):
        RECIPES.append((cat, names, tags, fn)); return fn
    return deco

# ===================================================================== PADS
@recipe('Pads', ['Velvet Horizon', 'Warm Blanket', 'Amber Fields', 'Soft Analog', 'Lush Eighties'], 'warm,analog,wide')
def pad_analog(v):
    p = P()
    osc(p, 'A', pick(v, ['Saw Tilt', 'Saw Tilt', 'Saw-Square', 'PWM', 'Saw Tilt']), pos=pick(v, [.75, .55, .3, .4, .85]), uni=5, det=pick(v, [.32, .28, .3, .25, .38]), spread=.9)
    osc(p, 'B', pick(v, ['PWM', 'Basic Shapes', 'Saw Tilt', 'PWM', 'Saw-Square']), pos=pick(v, [.5, .67, .6, .2, .5]), uni=3, det=.2, oct=pick(v, [-1, 0, -1, 0, 1]), level=.55)
    filt(p, pick(v, ['Low 24', 'Low 12', 'Ladder', 'Low 24', 'Low 24']), pick(v, [1400, 2200, 1100, 1800, 2600]), .18, env=.12, key=.3)
    amp(p, pick(v, [.8, 1.2, .6, 1.0, .5]), 1.5, .85, pick(v, [2.2, 3.0, 2.0, 2.5, 1.8]))
    fenv(p, 1.5, 2.5, .3, 2.0)
    lfo(p, 1, 'Triangle', pick(v, [.15, .1, .2, .12, .25]))
    p.mod('LFO 1', 'Osc A Position', .12).mod('LFO 1', 'Filter 1 Cutoff', .04)
    if v == 4: cho(p, .6, .5, .6)
    else: cho(p, .45, .4, .5)
    return rev(p, .35, .75, pick(v, [4.5, 6, 3.5, 5, 3.5]))

@recipe('Pads', ['Sunrise Stack', 'Trance Cathedral', 'Arena Glow', 'Wide Open', 'Neon Wash'], 'supersaw,bright,wide')
def pad_super(v):
    p = P()
    osc(p, 'A', 'Saw Tilt', pos=.95, uni=7, det=pick(v, [.45, .55, .5, .4, .6]), spread=1)
    osc(p, 'B', 'Saw Tilt', pos=.9, uni=7, det=pick(v, [.35, .45, .4, .3, .5]), oct=pick(v, [1, -1, 0, 1, -1]), level=.45)
    filt(p, 'Low 12', pick(v, [6000, 4500, 8000, 5000, 7000]), .12, key=.2)
    filt2(p, 'High 12', 180, .1)
    amp(p, pick(v, [.25, .6, .15, .4, .3]), 1, .9, pick(v, [1.5, 2.5, 1.2, 2, 1.8]))
    if v in (3, 4): pha(p, .35, .12, .6, .3)
    eq(p, -2, -1.5, 400, 2)
    return rev(p, .4, .8, pick(v, [4, 7, 3, 5, 4]), .4)

@recipe('Pads', ['Breath of Glass', 'Frosted Window', 'Ice Cathedral', 'Clear Water'], 'glass,bright,evolving')
def pad_glass(v):
    p = P()
    osc(p, 'A', pick(v, ['FM Glass', 'Crystal', 'FM Glass', 'Shimmer']), pos=pick(v, [.4, .3, .7, .5]), uni=3, det=.12)
    osc(p, 'B', 'Basic Shapes', pos=0, oct=1, level=.35)
    filt(p, 'Low 12', pick(v, [5000, 7000, 4000, 9000]), .2, key=.4)
    amp(p, pick(v, [1.2, .8, 2.0, 1.0]), 2, .8, 3.5)
    lfo(p, 1, 'Sine', pick(v, [.08, .12, .06, .1]))
    p.mod('LFO 1', 'Osc A Position', .3)
    cho(p, .35, .3, .4)
    dly(p, .18, '1/4 D', .45, .5)
    return rev(p, .45, .9, pick(v, [7, 6, 9, 8]), .35, 40)

@recipe('Pads', ['Morning Vowel', 'Whisper Choir Pad', 'Open Mouth', 'Distant Voices'], 'vocal,evolving')
def pad_vowel(v):
    p = P()
    osc(p, 'A', pick(v, ['Vowels', 'Choir', 'Vowels', 'Choir']), pos=pick(v, [.2, .5, .6, .3]), uni=5, det=.22, spread=.8)
    osc(p, 'B', 'Saw Tilt', pos=.5, level=.25, uni=3, det=.3)
    filt(p, 'Low 24', pick(v, [3500, 2600, 4200, 2000]), .1, key=.3)
    amp(p, pick(v, [.9, 1.4, .7, 1.8]), 2, .85, 3)
    lfo(p, 1, pick(v, ['Triangle', 'Smooth Random', 'Sine', 'Smooth Random']), pick(v, [.1, .3, .07, .2]))
    p.mod('LFO 1', 'Osc A Position', pick(v, [.35, .25, .4, .3]))
    noise(p, .04, -.4)
    cho(p, .4, .35, .5)
    return rev(p, .4, .85, 6, .45, 25)

@recipe('Pads', ['Slow Motion PWM', 'Juno Dream', 'Tape Memory', 'Lazy Sunday'], 'pwm,analog,vintage')
def pad_pwm(v):
    p = P()
    osc(p, 'A', 'PWM', pos=.3, uni=pick(v, [3, 1, 3, 2]), det=.15)
    osc(p, 'B', 'PWM', pos=.6, oct=-1, level=.5)
    lfo(p, 1, 'Triangle', pick(v, [.4, .7, .25, .35]))
    p.mod('LFO 1', 'Osc A Position', .3).mod('LFO 1', 'Osc B Position', -.25)
    filt(p, 'Low 24', pick(v, [1800, 2400, 1300, 2000]), .15, env=.08, key=.3)
    amp(p, pick(v, [.5, .3, .8, .6]), 1, .9, 1.8); fenv(p, .8, 1.5, .4, 1.5)
    cho(p, pick(v, [.6, .8, .5, .6]), pick(v, [.5, .8, .3, .4]), .6)
    if v == 2: p.update(drift=.6); dist(p, 'Tube', .15, .5, .5)
    return rev(p, .3, .6, 3)

@recipe('Pads', ['Shifting Sands', 'Tidal Drift', 'Slow Bloom', 'Weather System', 'Continental'], 'evolving,wavetable,cinematic')
def pad_evolve(v):
    p = P()
    tables = ['Resonant Sweep', 'Spectral Flux', 'Hollow Build', 'Spectral Phaser', 'Overtone Sweep']
    osc(p, 'A', pick(v, tables), pos=.2, uni=4, det=.25)
    osc(p, 'B', pick(v, ['Spectral Comb', 'Shimmer', 'Saw Tilt', 'Vowels', 'Air']), pos=.5, uni=3, det=.2, oct=pick(v, [0, 1, -1, 0, 1]), level=.5)
    menv(p, pick(v, [4, 6, 3, 8, 5]), 3, .6, 3)
    lfo(p, 1, 'Smooth Random', pick(v, [.15, .1, .2, .08, .12]))
    p.mod('Mod Env', 'Osc A Position', .55).mod('LFO 1', 'Osc B Position', .35).mod('LFO 1', 'Pan', .25)
    filt(p, 'Low 24', pick(v, [2800, 3500, 2000, 4000, 3000]), .15, key=.3)
    amp(p, pick(v, [1.5, 2.5, 1.0, 3.0, 2.0]), 2, .85, 4)
    return rev(p, .45, .9, 8, .5, 30)

@recipe('Pads', ['Midnight Cellar', 'Low Clouds', 'Underground', 'Ink Pad'], 'dark,warm')
def pad_dark(v):
    p = P()
    osc(p, 'A', pick(v, ['Saw Tilt', 'Bowed', 'Reed', 'Saw-Square']), pos=pick(v, [.4, .6, .5, .2]), uni=5, det=.28, oct=-1)
    osc(p, 'B', 'Basic Shapes', pos=.33, level=.5)
    filt(p, pick(v, ['Low 24', 'Ladder', 'Low 24', 'Ladder']), pick(v, [600, 800, 500, 900]), .25, key=.4)
    amp(p, 1.2, 2, .9, 3)
    lfo(p, 1, 'Sine', .07); p.mod('LFO 1', 'Filter 1 Cutoff', .08)
    eq(p, 1.5, -2, 300, -1)
    return rev(p, .35, .7, 5, .75)

@recipe('Pads', ['Heaven Shimmer', 'Silver Lining', 'Starlight', 'Weightless'], 'shimmer,airy,bright')
def pad_shimmer(v):
    p = P()
    osc(p, 'A', pick(v, ['Shimmer', 'Octave Stack', 'Shimmer', 'Crystal']), pos=pick(v, [.5, .7, .9, .4]), uni=5, det=.2, spread=1)
    osc(p, 'B', 'Basic Shapes', oct=1, level=.3)
    noise(p, .05, .4)
    filt(p, 'Low 12', 9000, .1)
    filt2(p, 'High 12', 300)
    amp(p, pick(v, [1.5, 2, 1, 2.5]), 2, .85, 4.5)
    lfo(p, 1, 'Sine', .05); p.mod('LFO 1', 'Osc A Position', .25)
    cho(p, .4, .2, .6)
    dly(p, .3, '1/4', .55, .45)
    return rev(p, .55, 1.0, 12, .3, 60)

@recipe('Pads', ['FM Halo', 'Digital Dawn', 'Eighties Glass Pad', 'Chrome Mist'], 'fm,digital')
def pad_fm(v):
    p = P()
    osc(p, 'A', 'Basic Shapes', pos=0, uni=3, det=.1)
    osc(p, 'B', 'Basic Shapes', pos=0, semi=pick(v, [0, 7, 12, 0]), oct=pick(v, [1, 0, 0, 2]), level=0)
    p['fm_amount'] = pick(v, [.25, .2, .3, .15])
    menv(p, .8, 3, .3, 2)
    p.mod('Mod Env', 'FM Amount', pick(v, [.2, .25, .3, .2]))
    filt(p, 'Low 12', 8000, .1)
    amp(p, pick(v, [.6, .4, .9, 1.2]), 2, .8, 2.5)
    cho(p, .5, .5, .5)
    return rev(p, .35, .75, 4.5)

@recipe('Pads', ['Hollow Halls', 'Paper Lanterns', 'Bamboo Room'], 'hollow,soft')
def pad_hollow(v):
    p = P()
    osc(p, 'A', pick(v, ['Hollow Build', 'FM Hollow', 'Reed']), pos=pick(v, [.45, .35, .4]), uni=3, det=.18)
    osc(p, 'B', 'Flute', pos=.3, oct=1, level=.4)
    filt(p, 'Low 12', 3000, .15, key=.4)
    amp(p, .9, 1.5, .8, 2.5)
    lfo(p, 1, 'Sine', .12); p.mod('LFO 1', 'Osc A Position', .15)
    pha(p, .3, .08, .5, .2)
    return rev(p, .35, .7, 4)

# ===================================================================== ATMOSPHERES
@recipe('Atmospheres', ['Deep Space Radio', 'Orbital Debris', 'Solar Wind', 'Dark Matter', 'Satellite Echo'], 'evolving,texture,sci-fi')
def atm_space(v):
    p = P()
    osc(p, 'A', pick(v, ['Spectral Flux', 'Metallic', 'Air', 'Growl', 'Spectral Comb']), pos=.3, uni=5, det=.4, spread=1)
    osc(p, 'B', pick(v, ['Shimmer', 'Crystal', 'Spectral Phaser', 'Impulse', 'FM Glass']), pos=.5, oct=pick(v, [1, 2, 0, 1, 1]), level=.4)
    noise(p, pick(v, [.1, .05, .2, .08, .06]), pick(v, [-.5, .3, 0, -.8, .5]))
    lfo(p, 1, 'Smooth Random', pick(v, [.3, .5, .2, .15, .4]))
    p.mod('LFO 1', 'Osc A Position', .5).mod('LFO 1', 'Filter 1 Cutoff', .15).mod('LFO 1', 'Osc B Pan', .8)
    filt(p, pick(v, ['Band', 'Comb', 'Low 24', 'Notch', 'Band']), pick(v, [1500, 400, 2500, 1200, 3000]), pick(v, [.3, .6, .2, .4, .35]), key=.5)
    amp(p, 2.5, 3, .9, 5)
    dly(p, .3, '1/4 D', .6, .4)
    return rev(p, .55, 1, 14, .4, 80)

@recipe('Atmospheres', ['Frozen Lake', 'Glacier Breath', 'Arctic Night', 'Snowfield'], 'cold,airy,evolving')
def atm_cold(v):
    p = P()
    osc(p, 'A', pick(v, ['Crystal', 'Air', 'FM Glass', 'Shimmer']), pos=.6, uni=4, det=.15)
    noise(p, .18, .6)
    filt(p, 'Band', pick(v, [3000, 4500, 2500, 6000]), .45, key=.3)
    filt2(p, 'High 12', 400)
    lfo(p, 1, 'Smooth Random', .2); p.mod('LFO 1', 'Filter 1 Cutoff', .25).mod('LFO 1', 'Osc A Position', .3)
    amp(p, 3, 2, .9, 6)
    return rev(p, .6, 1, 16, .2, 50)

@recipe('Atmospheres', ['Machine Room', 'Factory Hum', 'Power Grid', 'Server Farm'], 'industrial,dark,texture')
def atm_machine(v):
    p = P()
    osc(p, 'A', pick(v, ['Bit Crush', 'Stairs', 'Metallic', 'Growl']), pos=.5, uni=3, det=.3, oct=-1)
    osc(p, 'B', 'Spectral Comb', pos=.3, oct=-2, level=.5)
    noise(p, .15, -.3)
    filt(p, 'Comb', pick(v, [110, 220, 80, 165]), .7)
    lfo(p, 1, 'Sample & Hold', div=pick(v, ['1/8', '1/16', '1/4', '1/16 T']))
    p.mod('LFO 1', 'Filter 1 Cutoff', .06).mod('LFO 1', 'Osc A Position', .3)
    amp(p, 1.5, 2, .9, 3)
    dist(p, 'Downsample', .25, .4, .6)
    return rev(p, .4, .8, 6, .7)

@recipe('Atmospheres', ['Rain on Glass', 'Forest Floor', 'Ocean Floor', 'Desert Wind'], 'nature,noise,texture')
def atm_nature(v):
    p = P()
    p['oscA_on'] = 1; p['oscA_level'] = pick(v, [.2, .3, .15, .1]); p['oscA_table'] = pick(v, ['Crystal', 'Bowed', 'Air', 'Flute']); p['oscA_pos'] = .5
    noise(p, pick(v, [.6, .5, .7, .8]), pick(v, [.3, -.4, -.7, .1]))
    filt(p, 'Band', pick(v, [2500, 800, 400, 1500]), .35)
    lfo(p, 1, 'Smooth Random', pick(v, [1.5, .5, .12, .3]))
    p.mod('LFO 1', 'Filter 1 Cutoff', pick(v, [.2, .3, .35, .4])).mod('LFO 1', 'Pan', .4)
    amp(p, 2, 2, 1, 4)
    return rev(p, .45, .9, 7, .5)

@recipe('Atmospheres', ['Haunted Choir', 'Ghost Signal', 'Old Photographs', 'Fading Memory'], 'eerie,cinematic,vocal')
def atm_eerie(v):
    p = P()
    osc(p, 'A', pick(v, ['Choir', 'Vowels', 'Choir', 'Air']), pos=.5, uni=5, det=.35)
    osc(p, 'B', 'Basic Shapes', oct=pick(v, [1, 0, 1, 2]), semi=pick(v, [1, 6, 0, 0]), level=.25)
    lfo(p, 1, 'Smooth Random', .25)
    p.mod('LFO 1', 'Osc A Position', .4).mod('LFO 1', 'Pitch', .006)
    filt(p, 'Low 24', 2500, .2)
    amp(p, 2.5, 2, .9, 5)
    p['drift'] = .6
    pha(p, .3, .05, .8, .5)
    return rev(p, .55, 1, 12, .5, 40)

@recipe('Atmospheres', ['Pulse of the City', 'Neon Rain', 'Night Drive'], 'rhythmic,texture,synced')
def atm_rhythm(v):
    p = P()
    osc(p, 'A', pick(v, ['Saw Tilt', 'PWM', 'FM Saw']), pos=.6, uni=5, det=.3)
    filt(p, 'Low 24', 1500, .35, key=.3)
    lfo(p, 1, pick(v, ['Sample & Hold', 'Square', 'Ramp Down']), div=pick(v, ['1/16', '1/8', '1/8 D']))
    p.mod('LFO 1', 'Filter 1 Cutoff', .18).mod('LFO 1', 'Amp', -.3)
    amp(p, .8, 1, .9, 3)
    dly(p, .35, '1/8 D', .55, .5)
    return rev(p, .4, .8, 5)

# ===================================================================== KEYS
@recipe('Keys', ['Stage Tine', 'Soft Suitcase', 'Bright Tines', 'Mellow Seventy', 'Lo-Fi Keys'], 'epiano,classic')
def keys_ep(v):
    p = P()
    osc(p, 'A', 'E-Piano', pos=pick(v, [.45, .2, .8, .3, .35]), phase=0)
    osc(p, 'B', 'Basic Shapes', pos=0, oct=0, level=.3, phase=0)
    menv(p, .001, pick(v, [.6, .4, .9, .5, .5]), 0, .3)
    p.mod('Mod Env', 'Osc A Position', .4, 'Velocity').mod('Velocity', 'Osc A Position', .3)
    filt(p, 'Low 12', pick(v, [7000, 3500, 12000, 4500, 3000]), .1, key=.3)
    amp(p, .001, pick(v, [3.5, 4, 3, 4, 3]), pick(v, [.15, .25, .1, .2, .2]), pick(v, [.45, .6, .35, .5, .5]))
    p['vel_sens'] = .85
    lfo(p, 1, 'Sine', pick(v, [4.5, 3.5, 5, 4, 3]))
    if v in (1, 3): p.mod('LFO 1', 'Pan', .45)
    if v == 4: p.update(drift=.5); dist(p, 'Bit Crush', .35, .35, .4); eq(p, 2, 0, 1000, -6)
    cho(p, .35, .7, .4)
    return rev(p, .2, .5, 1.8)

@recipe('Keys', ['DX Bright Piano', 'Ballad FM', 'Crystal EP', 'Digital Grand'], 'fm,digital,eighties')
def keys_fm(v):
    p = P()
    osc(p, 'A', 'Basic Shapes', pos=0, phase=0)
    osc(p, 'B', 'Basic Shapes', pos=0, level=0, oct=pick(v, [0, 0, 2, 1]), semi=pick(v, [0, 0, 0, 0]), fine=pick(v, [4, 0, 0, 3]), phase=0)
    p['fm_amount'] = pick(v, [.15, .08, .12, .1])
    menv(p, .001, pick(v, [.8, .5, 1.2, .6]), .05, .3)
    p.mod('Mod Env', 'FM Amount', pick(v, [.45, .3, .5, .35]), 'Velocity')
    nofilt(p)
    amp(p, .001, pick(v, [2.5, 3, 2.5, 4]), .0, .5)
    p['vel_sens'] = .8
    cho(p, .4, .5, .4)
    return rev(p, .22, .55, 2)

@recipe('Keys', ['Funky Clav', 'Rubber Clav', 'Wah Clav'], 'clav,funky')
def keys_clav(v):
    p = P()
    osc(p, 'A', 'PWM', pos=pick(v, [.75, .6, .7]), phase=0)
    osc(p, 'B', 'Saw Tilt', pos=.8, level=.4, phase=0)
    filt(p, pick(v, ['Band', 'Low 12', 'Band']), pick(v, [1800, 900, 600]), pick(v, [.3, .45, .55]), env=pick(v, [.15, .35, .3]), key=.6)
    fenv(p, .001, .25, .1, .1)
    amp(p, .001, .6, .15, .08)
    p['vel_sens'] = .9
    if v == 2: lfo(p, 1, 'Sine', div='1/8'); p.mod('LFO 1', 'Filter 1 Cutoff', .12); p.update(filter1_type='Low 12', filter1_reso=.6)
    if v == 1: pha(p, .25, .5)
    return rev(p, .12, .3, 1)

@recipe('Keys', ['Wurly Bark', 'Reed Piano', 'School Room'], 'reed,vintage')
def keys_wurli(v):
    p = P()
    osc(p, 'A', 'Reed', pos=pick(v, [.5, .3, .4]), phase=0)
    osc(p, 'B', 'Basic Shapes', pos=.33, level=.35, phase=0)
    menv(p, .001, .3, 0, .2)
    p.mod('Mod Env', 'Osc A Position', .4, 'Velocity')
    filt(p, 'Low 12', 4500, .15, key=.4)
    amp(p, .001, 2.5, .3, .35)
    dist(p, 'Tube', pick(v, [.3, .15, .2]), .6, .6)
    lfo(p, 1, 'Sine', 5.5); p.mod('LFO 1', 'Amp', -.12)
    return rev(p, .15, .4, 1.2)

@recipe('Keys', ['Toy Piano', 'Music Class', 'Bedroom Keys', 'Celeste Box'], 'toy,bright,small')
def keys_toy(v):
    p = P()
    osc(p, 'A', pick(v, ['Metallic', 'Crystal', 'Plucked', 'FM Bell']), pos=pick(v, [.3, .2, .7, .4]), phase=0)
    osc(p, 'B', 'Basic Shapes', oct=1, level=.3, phase=0)
    filt(p, 'Low 12', 9000, .1)
    amp(p, .001, pick(v, [.8, 1.2, 1.5, 2]), 0, .4)
    eq(p, -4, 2, 2500, 0)
    return rev(p, .25, .4, 1.5)

@recipe('Keys', ['Velvet Keys', 'Analog Piano Pad', 'Soft Felt'], 'soft,warm')
def keys_soft(v):
    p = P()
    osc(p, 'A', 'Saw Tilt', pos=.3, phase=0, uni=2, det=.1)
    osc(p, 'B', 'Basic Shapes', pos=.33, level=.5, phase=0)
    filt(p, 'Low 24', 900, .1, env=.25, key=.5)
    fenv(p, .001, 1.2, .1, .4)
    amp(p, .002, 3, .3, .6)
    p.mod('Velocity', 'Filter 1 Cutoff', .2)
    cho(p, .4, .4, .4)
    return rev(p, .25, .55, 2.5, .7)

@recipe('Keys', ['Harpsichord Court', 'Baroque Pluck'], 'baroque,bright')
def keys_harpsi(v):
    p = P()
    osc(p, 'A', 'Saw Sync', pos=pick(v, [.25, .15]), phase=0)
    osc(p, 'B', 'Saw Tilt', pos=.9, oct=1, level=.35, phase=0)
    filt(p, 'High 12', 250, .1)
    amp(p, .001, 1.6, 0, .3)
    return rev(p, .2, .5, 1.6)

# ===================================================================== BASS
@recipe('Bass', ['Pure Sub', 'Round Sub', 'Sub Floor', 'Weighty Sine'], 'sub,clean,mono')
def bass_sub(v):
    p = P(); mono(p, pick(v, [0, .04, 0, .06]))
    osc(p, 'A', 'Basic Shapes', pos=pick(v, [0, .1, .05, .2]), phase=0)
    sub(p, pick(v, [0, .3, .5, 0]), 'Sine', '-1') if v in (1, 2) else None
    p['oscA_oct'] = -1
    nofilt(p) if v == 0 else filt(p, 'Low 24', 400, .05)
    amp(p, .002, .3, 1, .08)
    p['vel_sens'] = .2; p['drift'] = 0
    if v == 3: dist(p, 'Soft', .25, 1, .4)
    return p

@recipe('Bass', ['Reese Drift', 'Liquid Reese', 'Hollow Reese', 'Neuro Reese', 'Dark Reese'], 'reese,dnb,wide')
def bass_reese(v):
    p = P(); mono(p, .03)
    osc(p, 'A', 'Saw Tilt', pos=.85, uni=pick(v, [2, 3, 4, 5, 3]), det=pick(v, [.22, .28, .3, .35, .25]), spread=.4, phase=0, oct=-1)
    osc(p, 'B', pick(v, ['Saw Tilt', 'Saw-Square', 'Hollow Build', 'Growl', 'Saw Tilt']), pos=.6, oct=-1, fine=pick(v, [12, 8, 15, 20, -10]), level=.7, phase=0)
    sub(p, .45, 'Sine', '-1')
    filt(p, pick(v, ['Low 24', 'Low 24', 'Ladder', 'Low 24', 'Ladder']), pick(v, [900, 1500, 700, 1200, 500]), .25, key=.3)
    lfo(p, 1, 'Sine', pick(v, [.15, .25, .2, .5, .1]))
    p.mod('LFO 1', 'Filter 1 Cutoff', .05).mod('LFO 1', 'Unison Detune', .15)
    amp(p, .003, .3, 1, .12)
    if v == 3: dist(p, 'Tube', .35, .7, .6)
    if v == 2: pha(p, .4, .2, .7, .5)
    p['vel_sens'] = .3
    return p

@recipe('Bass', ['Eight-Oh-Eight Long', 'Trap Boom', 'Gliding 808', 'Distorted 808', 'Punchy 808'], '808,trap,hip-hop')
def bass_808(v):
    p = P(); mono(p, pick(v, [0, 0, .12, 0, 0]), legato=(v == 2))
    osc(p, 'A', pick(v, ['Basic Shapes', 'Saturated Sine', 'Basic Shapes', 'Saturated Sine', 'Basic Shapes']), pos=pick(v, [0, .2, 0, .5, .1]), phase=0, oct=-1)
    menv(p, .001, pick(v, [.05, .04, .06, .05, .03]), 0, .05)
    p.mod('Mod Env', 'Pitch', pick(v, [.5, .5, .4, .6, .7]))
    nofilt(p)
    amp(p, .001, pick(v, [3, 1.8, 3, 2.5, .8]), pick(v, [0, 0, .3, 0, 0]), pick(v, [.3, .25, .3, .3, .15]))
    if v in (3, 1): dist(p, pick(v, ['Soft', 'Tube', 'Soft', 'Tube', 'Soft']), pick(v, [0, .45, 0, .6, 0]), 1, .5)
    p['vel_sens'] = .25; p['drift'] = 0
    return p

@recipe('Bass', ['Acid Line', 'Squelch', 'Rubber Acid', 'Night Acid', 'Silver Box'], 'acid,303,squelchy')
def bass_acid(v):
    p = P(); mono(p, pick(v, [.06, .05, .08, .06, .04]), legato=True)
    osc(p, 'A', pick(v, ['Saw Tilt', 'Saw Tilt', 'PWM', 'Saw Tilt', 'PWM']), pos=pick(v, [.95, .9, .5, .85, .45]), phase=0)
    filt(p, 'Ladder', pick(v, [350, 500, 300, 250, 400]), pick(v, [.78, .85, .72, .8, .75]), env=pick(v, [.4, .45, .35, .5, .38]), key=.4, drive=pick(v, [.25, .4, .2, .3, .3]))
    fenv(p, .001, pick(v, [.25, .18, .3, .35, .2]), .05, .1)
    amp(p, .001, .4, .8, .06)
    p.mod('Velocity', 'Filter 1 Cutoff', .12)
    if v in (1, 3): dist(p, 'Tube', .3, .6, .7)
    if v == 3: dly(p, .2, '1/8 D', .35)
    p['vel_sens'] = .5
    return p

@recipe('Bass', ['Wobble Monster', 'Dubstep Growl', 'Talking Wub', 'Half-Time Wobble', 'Riddim Saw'], 'wobble,dubstep,synced')
def bass_wobble(v):
    p = P(); mono(p, .02)
    osc(p, 'A', pick(v, ['Growl', 'Talk Bass', 'Talk Bass', 'Saw Tilt', 'Saw Sync']), pos=.3, uni=3, det=.15, phase=0, oct=-1)
    osc(p, 'B', 'Saw Tilt', pos=.8, oct=-1, level=.5, phase=0)
    sub(p, .5, 'Sine', '-1')
    lfo(p, 1, pick(v, ['Sine', 'Triangle', 'Sine', 'Sine', 'Ramp Down']), div=pick(v, ['1/8', '1/16', '1/8 T', '1/4', '1/8']), retrig=1)
    p.mod('LFO 1', 'Osc A Position', .5).mod('LFO 1', 'Filter 1 Cutoff', .25)
    filt(p, pick(v, ['Low 24', 'Vowel', 'Vowel', 'Low 24', 'Ladder']), pick(v, [800, 1000, 600, 700, 900]), .35, key=.2)
    amp(p, .002, .3, 1, .1)
    dist(p, pick(v, ['Tube', 'Soft', 'Fold', 'Tube', 'Hard']), pick(v, [.4, .35, .25, .45, .3]), .8, .7)
    comp(p, -16, 4, 5, 80, 3)
    p['vel_sens'] = .2
    return p

@recipe('Bass', ['Moog Thump', 'Analog Mono', 'Fat Square', 'Classic Funk Bass', 'Warm Mini'], 'analog,mono,classic')
def bass_analog(v):
    p = P(); mono(p, pick(v, [0, .03, 0, .02, 0]), legato=False)
    osc(p, 'A', pick(v, ['Saw Tilt', 'Saw Tilt', 'PWM', 'Saw-Square', 'Saw Tilt']), pos=pick(v, [.8, .7, .05, .5, .6]), phase=0, oct=-1)
    osc(p, 'B', pick(v, ['PWM', 'Saw Tilt', 'PWM', 'Basic Shapes', 'Saw Tilt']), pos=pick(v, [.05, .7, .1, .67, .6]), oct=pick(v, [-1, -1, -2, 0, -1]), fine=pick(v, [5, -7, 0, 0, 4]), level=.6, phase=0)
    filt(p, 'Ladder', pick(v, [350, 500, 600, 800, 300]), pick(v, [.35, .3, .2, .4, .25]), env=pick(v, [.3, .25, .2, .35, .3]), key=.5)
    fenv(p, .001, pick(v, [.3, .5, .4, .2, .6]), .25, .15)
    amp(p, .002, .5, .85, .1)
    p.mod('Velocity', 'Filter 1 Cutoff', .1)
    p['vel_sens'] = .4
    return p

@recipe('Bass', ['FM Slap', 'Digital Pluck Bass', 'Metal Bass', 'Elastic FM'], 'fm,digital,punchy')
def bass_fm(v):
    p = P(); mono(p, 0, legato=False)
    osc(p, 'A', 'Basic Shapes', phase=0, oct=-1)
    osc(p, 'B', 'Basic Shapes', phase=0, oct=pick(v, [-1, 0, 0, -1]), semi=pick(v, [0, 0, 7, 0]), level=0)
    p['fm_amount'] = pick(v, [.1, .05, .15, .2])
    menv(p, .001, pick(v, [.15, .25, .3, .5]), pick(v, [.05, 0, .1, .2]), .1)
    p.mod('Mod Env', 'FM Amount', pick(v, [.45, .5, .4, .3]), 'Velocity')
    nofilt(p)
    amp(p, .001, .6, .6, .1)
    p['vel_sens'] = .5
    return p

@recipe('Bass', ['Plucked Upright', 'Deep Pluck', 'Muted Pick', 'House Organ Bass'], 'pluck,round')
def bass_pluck(v):
    p = P(); mono(p, 0, legato=False)
    osc(p, 'A', pick(v, ['Plucked', 'Saw Tilt', 'PWM', 'Organ']), pos=pick(v, [.5, .6, .4, .1]), phase=0, oct=-1)
    sub(p, .35, 'Sine', '-1')
    filt(p, 'Low 24', pick(v, [500, 300, 700, 1200]), .15, env=pick(v, [.3, .35, .25, .1]), key=.5)
    fenv(p, .001, .2, 0, .1)
    amp(p, .001, pick(v, [.9, .6, .3, .4]), pick(v, [0, .2, 0, .4]), .12)
    p['vel_sens'] = .5
    return p

@recipe('Bass', ['Distorted Saw Bass', 'Industrial Grind', 'Bitcrush Bass', 'Fuzz Bass'], 'distorted,aggressive')
def bass_dist(v):
    p = P(); mono(p, .02)
    osc(p, 'A', pick(v, ['Saw Tilt', 'Saw Sync', 'Bit Crush', 'Saw-Square']), pos=pick(v, [.9, .4, .5, .5]), uni=2, det=.15, phase=0, oct=-1)
    sub(p, .5, 'Sine', '-1')
    filt(p, 'Low 24', pick(v, [1200, 900, 2000, 700]), .2, env=.15, key=.3)
    fenv(p, .001, .4, .4, .2)
    dist(p, pick(v, ['Hard', 'Fold', 'Bit Crush', 'Tube']), pick(v, [.5, .45, .6, .6]), .8, .6)
    eq(p, 2, -3, 500, 1)
    amp(p, .002, .3, 1, .1)
    return p

@recipe('Bass', ['Garage Organ Bass', 'Two-Step Wobble', 'Speed Garage'], 'garage,uk')
def bass_garage(v):
    p = P(); mono(p, .02)
    osc(p, 'A', pick(v, ['Organ', 'Saw-Square', 'Organ']), pos=pick(v, [.2, .5, .6]), phase=0, oct=-1)
    filt(p, 'Low 24', 600, .4, env=.2, key=.3)
    fenv(p, .001, .3, .2, .1)
    lfo(p, 1, 'Sine', div=pick(v, ['1/8', '1/16', '1/8'])); p.mod('LFO 1', 'Filter 1 Cutoff', pick(v, [.0, .1, .06]))
    amp(p, .002, .3, .9, .08)
    sub(p, .4)
    return p

# ===================================================================== LEADS
@recipe('Leads', ['Anthem Lead', 'Euphoria', 'Big Room Lead', 'Festival Saw', 'Hands Up'], 'supersaw,trance,bright')
def lead_super(v):
    p = P()
    osc(p, 'A', 'Saw Tilt', pos=.95, uni=7, det=pick(v, [.4, .5, .45, .55, .35]), spread=.9)
    osc(p, 'B', 'Saw Tilt', pos=.9, uni=5, det=.3, oct=pick(v, [1, 0, 1, -1, 1]), level=.4)
    filt(p, 'Low 12', pick(v, [9000, 7000, 12000, 8000, 6000]), .1)
    amp(p, .005, .5, .9, .3)
    dly(p, .2, '1/8 D', .35, .5)
    p.update(polyphony=8)
    return rev(p, .25, .7, 3)

@recipe('Leads', ['Sync Scream', 'Hard Sync Sweep', 'Tearing Metal', 'Sync Rider'], 'sync,aggressive')
def lead_sync(v):
    p = P(); mono(p, pick(v, [.04, .02, .05, .08]))
    osc(p, 'A', 'Saw Tilt', pos=.9, wm='Sync', warp=pick(v, [.2, .1, .4, .3]), phase=0, uni=pick(v, [1, 2, 3, 1]), det=.1)
    menv(p, .001, pick(v, [.6, 1.2, .4, .8]), .3, .3)
    p.mod('Mod Env', 'Osc A Warp', pick(v, [.5, .7, .4, .5]))
    filt(p, 'Low 12', 8000, .15)
    amp(p, .003, .3, .9, .15)
    if v == 2: dist(p, 'Hard', .35, .5)
    return rev(p, .15, .5, 2)

@recipe('Leads', ['Square Whistle', 'Chip Lead', 'Retro Square', 'Pocket Lead'], 'square,retro')
def lead_square(v):
    p = P(); mono(p, pick(v, [.05, 0, .03, .02]))
    osc(p, 'A', 'PWM', pos=pick(v, [.0, .3, .1, .55]), phase=0)
    nofilt(p) if v == 1 else filt(p, 'Low 24', 4000, .1, key=.5)
    amp(p, .002, .2, .9, .1)
    if v == 1: lfo(p, 1, 'Square', 12); p.update(oscA_table='Bit Crush', oscA_pos=.6)
    dly(p, .18, '1/8 D', .3, .45)
    return rev(p, .15, .4, 1.5)

@recipe('Leads', ['Portamento Silk', 'Glide Pilot', 'Slow Bend Lead', 'Liquid Lead'], 'glide,smooth,mono')
def lead_glide(v):
    p = P(); mono(p, pick(v, [.12, .08, .25, .15]))
    osc(p, 'A', pick(v, ['Saw Tilt', 'Saw-Square', 'Basic Shapes', 'FM Saw']), pos=pick(v, [.7, .3, .55, .3]), uni=2, det=.08)
    osc(p, 'B', 'Basic Shapes', oct=-1, level=.4, pos=.33)
    filt(p, 'Low 24', pick(v, [2500, 3500, 2000, 3000]), .25, env=.1, key=.6)
    amp(p, .01, .4, .9, .25)
    dly(p, .2, '1/4', .4)
    return rev(p, .25, .6, 2.5)

@recipe('Leads', ['FM Bite', 'Digital Horn', 'Laser Lead'], 'fm,digital,bright')
def lead_fm(v):
    p = P(); mono(p, .02)
    osc(p, 'A', 'Basic Shapes', phase=0)
    osc(p, 'B', 'Basic Shapes', phase=0, oct=pick(v, [1, 0, 2]), level=0)
    p['fm_amount'] = pick(v, [.35, .25, .3])
    menv(p, .001, .5, .4, .2); p.mod('Mod Env', 'FM Amount', .2)
    nofilt(p)
    amp(p, .002, .3, .9, .15)
    return rev(p, .2, .5, 2)

@recipe('Leads', ['Screaming Fuzz', 'Rock Solo', 'Overdrive Lead', 'Amp Stack'], 'distorted,guitar-like')
def lead_fuzz(v):
    p = P(); mono(p, .03)
    osc(p, 'A', pick(v, ['Saw Tilt', 'Saw-Square', 'PWM', 'Saw Tilt']), pos=.7, uni=2, det=.1)
    osc(p, 'B', 'Saw Tilt', oct=1, level=.25, pos=.5)
    filt(p, 'Low 24', 3000, .3, key=.5)
    dist(p, pick(v, ['Tube', 'Hard', 'Soft', 'Tube']), pick(v, [.6, .55, .7, .65]), 1, .45)
    eq(p, -2, 3, 1500, -4)
    amp(p, .005, .3, .95, .2)
    dly(p, .15, '1/4', .3)
    return rev(p, .2, .6, 2)

@recipe('Leads', ['Soft Sine Lead', 'Flute Lead', 'Whistler', 'Ocarina'], 'soft,pure')
def lead_soft(v):
    p = P(); mono(p, pick(v, [.05, .03, .08, .04]))
    osc(p, 'A', pick(v, ['Basic Shapes', 'Flute', 'Basic Shapes', 'Flute']), pos=pick(v, [.1, .4, 0, .2]))
    noise(p, pick(v, [0, .06, .03, .05]), .3)
    filt(p, 'Low 12', 6000, .1)
    amp(p, pick(v, [.03, .06, .02, .05]), .3, .9, .25)
    lfo(p, 1, 'Sine', 5.2, fade=.6, retrig=1); p.mod('LFO 1', 'Pitch', .006)
    dly(p, .2, '1/4 D', .35)
    return rev(p, .3, .7, 3)

@recipe('Leads', ['Vocal Lead', 'Talkbox', 'Robot Voice'], 'vocal,formant')
def lead_vocal(v):
    p = P(); mono(p, .04)
    osc(p, 'A', pick(v, ['Vowels', 'Talk Bass', 'Vowels']), pos=.3, uni=2, det=.08)
    menv(p, .05, .6, .3, .3)
    p.mod('Mod Env', 'Osc A Position', .4)
    filt(p, 'Vowel', pick(v, [800, 400, 2000]), .4)
    p.mod('Mod Wheel', 'Filter 1 Cutoff', .3)
    amp(p, .01, .3, .9, .2)
    if v == 2: dist(p, 'Bit Crush', .4, .4)
    return rev(p, .2, .5, 2)

@recipe('Leads', ['Mono Pulse Lead', 'Seventies Prog', 'Brassy Lead'], 'analog,vintage')
def lead_prog(v):
    p = P(); mono(p, pick(v, [.03, .06, .02]))
    osc(p, 'A', pick(v, ['PWM', 'Saw Tilt', 'Brass']), pos=pick(v, [.2, .8, .6]))
    osc(p, 'B', 'Saw Tilt', pos=.8, fine=7, level=.5)
    filt(p, 'Ladder', pick(v, [1200, 900, 1500]), .4, env=.25, key=.6)
    fenv(p, .01, .6, .4, .2)
    amp(p, .005, .3, .9, .2)
    return rev(p, .15, .5, 2)

# ===================================================================== PLUCKS
@recipe('Plucks', ['House Pluck', 'Deep House Stab', 'Future Pluck', 'Tropical Pluck', 'Chill Pluck'], 'pluck,house,short')
def pluck_house(v):
    p = P()
    osc(p, 'A', 'Saw Tilt', pos=.9, uni=pick(v, [3, 5, 7, 2, 3]), det=pick(v, [.2, .25, .35, .1, .2]), phase=0)
    osc(p, 'B', pick(v, ['PWM', 'Saw Tilt', 'Saw Tilt', 'Basic Shapes', 'Basic Shapes']), pos=.3, oct=pick(v, [0, -1, 1, 1, 0]), level=.4, phase=0)
    filt(p, 'Low 24', pick(v, [400, 300, 600, 800, 350]), .25, env=pick(v, [.45, .4, .5, .35, .4]), key=.4)
    fenv(p, .001, pick(v, [.18, .25, .15, .12, .3]), 0, .15)
    amp(p, .001, pick(v, [.35, .5, .3, .25, .6]), 0, .25)
    p.mod('Velocity', 'Filter 1 Cutoff', .1)
    dly(p, pick(v, [.2, .25, .2, .15, .3]), '1/8 D', .35, .5)
    return rev(p, .2, .6, 2)

@recipe('Plucks', ['Kalimba', 'Thumb Piano', 'Wooden Pluck', 'Marimba Soft'], 'mallet,organic')
def pluck_kalimba(v):
    p = P()
    osc(p, 'A', pick(v, ['FM Glass', 'Metallic', 'Hollow Build', 'Basic Shapes']), pos=pick(v, [.5, .15, .3, 0]), phase=0)
    osc(p, 'B', 'Basic Shapes', pos=0, oct=pick(v, [2, 1, 2, 2]), level=.25, phase=0)
    menv(p, .001, .05, 0, .05); p.mod('Mod Env', 'Osc B Level', .5)
    filt(p, 'Low 12', pick(v, [6000, 4000, 3000, 2500]), .1, key=.5)
    amp(p, .001, pick(v, [.9, .7, .4, .6]), 0, .3)
    p['vel_sens'] = .8
    return rev(p, .2, .45, 1.5)

@recipe('Plucks', ['Harp Strings', 'Glass Harp', 'Celtic Harp'], 'harp,gentle')
def pluck_harp(v):
    p = P()
    osc(p, 'A', pick(v, ['Plucked', 'Crystal', 'Plucked']), pos=pick(v, [.6, .4, .4]), phase=0)
    osc(p, 'B', 'Basic Shapes', pos=.33, level=.4, phase=0)
    filt(p, 'Low 12', 3000, .1, env=.25, key=.5)
    fenv(p, .001, .5, 0, .3)
    amp(p, .001, pick(v, [2, 2.5, 1.5]), 0, .6)
    return rev(p, .3, .7, 3)

@recipe('Plucks', ['Bell Pluck', 'Digital Pluck', 'Bright Ping', 'Sparkle'], 'bright,digital')
def pluck_bell(v):
    p = P()
    osc(p, 'A', pick(v, ['FM Bell', 'FM Saw', 'Crystal', 'Shimmer']), pos=pick(v, [.3, .7, .5, .6]), phase=0, uni=2, det=.08)
    filt(p, 'Low 12', 2500, .2, env=.35, key=.5)
    fenv(p, .001, .25, 0, .2)
    amp(p, .001, .5, 0, .35)
    dly(p, .3, '1/8 D', .4, .5)
    return rev(p, .25, .6, 2.5)

@recipe('Plucks', ['Pizzicato Synth', 'Staccato Strings', 'Short Bow'], 'staccato,orchestral')
def pluck_pizz(v):
    p = P()
    osc(p, 'A', 'Bowed', pos=.4, uni=3, det=.15, phase=0)
    filt(p, 'Low 24', 1500, .1, env=.2, key=.5)
    fenv(p, .001, .15, 0, .1)
    amp(p, .001, pick(v, [.25, .35, .18]), 0, .2)
    return rev(p, .3, .7, 2.5)

@recipe('Plucks', ['Synthwave Pluck', 'Arcade Pluck', 'Mallet Saw'], 'retro,synthwave')
def pluck_retro(v):
    p = P()
    osc(p, 'A', pick(v, ['Saw Tilt', 'PWM', 'Saw-Square']), pos=.7, uni=3, det=.18, phase=0)
    filt(p, 'Ladder', 500, .45, env=.4, key=.5)
    fenv(p, .001, .3, 0, .2)
    amp(p, .001, .6, 0, .3)
    cho(p, .5, .6, .5)
    dly(p, .3, '1/8 D', .45, .45)
    return rev(p, .25, .6, 2.5)

@recipe('Plucks', ['Water Drop', 'Bubble Pop', 'Rain Pluck'], 'playful,resonant')
def pluck_drop(v):
    p = P()
    osc(p, 'A', 'Basic Shapes', pos=pick(v, [0, .2, 0]), phase=0, oct=1)
    menv(p, .001, .08, 0, .05); p.mod('Mod Env', 'Pitch', pick(v, [.25, .3, -.2]))
    filt(p, 'Low 12', 1200, .55, env=.3, key=.8)
    amp(p, .001, .25, 0, .2)
    dly(p, .3, '1/16 D', .4)
    return rev(p, .3, .5, 2)

# ===================================================================== BELLS & MALLETS
@recipe('Bells & Mallets', ['Tubular Bell', 'Church Bell', 'Temple Gong', 'Hand Bell', 'Glass Bell'], 'bell,fm')
def bell_fm(v):
    p = P()
    osc(p, 'A', 'FM Bell', pos=pick(v, [.5, .8, .9, .35, .25]), phase=0)
    osc(p, 'B', pick(v, ['Metallic', 'Metallic', 'Growl', 'Crystal', 'FM Glass']), pos=.6, level=.3, oct=pick(v, [0, -1, -1, 1, 1]), phase=0)
    menv(p, .001, 1.5, 0, 1); p.mod('Mod Env', 'Osc A Position', .3)
    nofilt(p)
    amp(p, .001, pick(v, [5, 8, 10, 3, 4]), 0, pick(v, [3, 5, 6, 2, 3]))
    return rev(p, .3, .8, 4)

@recipe('Bells & Mallets', ['Vibraphone', 'Jazz Vibes', 'Motor Vibes'], 'vibes,mallet')
def bell_vibes(v):
    p = P()
    osc(p, 'A', 'Basic Shapes', pos=0, phase=0)
    osc(p, 'B', 'Basic Shapes', pos=0, oct=2, level=.25, phase=0)
    menv(p, .001, .1, 0, .1); p.mod('Mod Env', 'Osc B Level', .4)
    nofilt(p)
    amp(p, .001, 3, 0, 1.2)
    lfo(p, 1, 'Sine', pick(v, [5, 0.0001, 7])); p.mod('LFO 1', 'Amp', pick(v, [-.35, 0, -.45]))
    return rev(p, .25, .6, 2.5)

@recipe('Bells & Mallets', ['Music Box', 'Lullaby', 'Snow Globe'], 'music box,delicate')
def bell_box(v):
    p = P()
    osc(p, 'A', pick(v, ['Crystal', 'FM Glass', 'Crystal']), pos=pick(v, [.2, .6, .5]), phase=0)
    osc(p, 'B', 'Basic Shapes', oct=1, level=.4, phase=0)
    filt2(p, 'High 12', 400)
    nofilt(p)
    amp(p, .001, 1.6, 0, 1)
    return rev(p, .3, .5, 2.5)

@recipe('Bells & Mallets', ['Glockenspiel', 'Celesta', 'Steel Drum', 'Gamelan'], 'mallet,percussive')
def bell_glock(v):
    p = P()
    osc(p, 'A', pick(v, ['Crystal', 'FM Glass', 'Metallic', 'Metallic']), pos=pick(v, [.7, .3, .4, .8]), phase=0)
    osc(p, 'B', 'Basic Shapes', pos=0, oct=pick(v, [2, 1, 0, 1]), level=.3, phase=0)
    if v == 3: p['oscB_semi'] = 7
    filt(p, 'Low 12', pick(v, [12000, 8000, 5000, 6000]), .1)
    amp(p, .001, pick(v, [1.5, 1.2, .8, 2.5]), 0, .8)
    return rev(p, .25, .5, 2)

@recipe('Bells & Mallets', ['Crystal Bowls', 'Singing Bowl', 'Meditation Bell'], 'bowl,meditative')
def bell_bowl(v):
    p = P()
    osc(p, 'A', 'Basic Shapes', phase=0)
    osc(p, 'B', 'Metallic', pos=.5, level=.25, fine=pick(v, [6, 4, 3]), phase=0)
    lfo(p, 1, 'Sine', pick(v, [3, 2.2, 1.5])); p.mod('LFO 1', 'Amp', -.25)
    nofilt(p)
    amp(p, pick(v, [.05, .2, .002]), 8, 0, 6)
    return rev(p, .4, .9, 8)

# ===================================================================== STRINGS
@recipe('Strings', ['String Ensemble', 'Solina Strings', 'Lush Strings', 'Cinematic Strings', 'Chamber Section'], 'strings,ensemble')
def str_ens(v):
    p = P()
    osc(p, 'A', pick(v, ['Bowed', 'Saw Tilt', 'Bowed', 'Bowed', 'Bowed']), pos=pick(v, [.5, .7, .7, .6, .3]), uni=5, det=pick(v, [.2, .3, .25, .22, .15]), spread=.9)
    osc(p, 'B', 'Saw Tilt', pos=.6, oct=pick(v, [0, 1, -1, -1, 0]), level=.35, uni=3, det=.2)
    filt(p, 'Low 24', pick(v, [3500, 4500, 3000, 2500, 4000]), .1, key=.4)
    amp(p, pick(v, [.35, .25, .6, .8, .2]), 1, .9, pick(v, [1.2, 1, 1.8, 2, .8]))
    lfo(p, 1, 'Sine', 5, fade=1, retrig=1); p.mod('LFO 1', 'Pitch', .003)
    cho(p, pick(v, [.5, .8, .5, .4, .3]), pick(v, [.5, .9, .4, .4, .3]), .6)
    return rev(p, .35, .8, pick(v, [3, 2.5, 4, 5, 2]))

@recipe('Strings', ['Slow Violins', 'Adagio', 'Long Bows', 'Sorrow'], 'strings,slow,emotional')
def str_slow(v):
    p = P()
    osc(p, 'A', 'Bowed', pos=pick(v, [.8, .5, .6, .4]), uni=6, det=.18)
    filt(p, 'Low 24', 2800, .1, key=.4)
    amp(p, pick(v, [1, 1.5, 2, 1.2]), 1, .95, 2.5)
    menv(p, 1.5, 1, 1, 1); p.mod('Mod Env', 'Filter 1 Cutoff', .1)
    lfo(p, 1, 'Sine', 5.2, fade=2, retrig=1); p.mod('LFO 1', 'Pitch', .004)
    return rev(p, .4, .85, 5, .5, 30)

@recipe('Strings', ['Cello Section', 'Low Strings', 'Contrabass Swell'], 'strings,low')
def str_low(v):
    p = P()
    osc(p, 'A', 'Bowed', pos=.4, uni=4, det=.15, oct=-1)
    osc(p, 'B', 'Saw Tilt', pos=.5, oct=-1, level=.3)
    filt(p, 'Low 24', pick(v, [1600, 1200, 900]), .15, key=.4)
    amp(p, pick(v, [.3, .5, 1.2]), 1, .9, 1.5)
    return rev(p, .3, .7, 3.5, .6)

@recipe('Strings', ['Tremolo Strings', 'Suspense Strings', 'Shiver'], 'strings,tension,cinematic')
def str_trem(v):
    p = P()
    osc(p, 'A', 'Bowed', pos=.6, uni=5, det=.22)
    lfo(p, 1, 'Triangle', div=pick(v, ['1/32', '1/16 T', '1/32']))
    p.mod('LFO 1', 'Amp', -.5)
    filt(p, 'Low 24', 3500, .1)
    amp(p, .2, 1, .9, 1.5)
    if v == 1: p['oscB_on'] = 1; p['oscB_table'] = 'Bowed'; p['oscB_semi'] = 1; p['oscB_level'] = .3
    return rev(p, .4, .85, 4)

# ===================================================================== BRASS & WINDS
@recipe('Brass & Winds', ['Synth Brass', 'Jump Brass', 'OB Brass', 'Power Brass', 'Soft Horns'], 'brass,analog')
def brass_synth(v):
    p = P()
    osc(p, 'A', pick(v, ['Saw Tilt', 'Saw Tilt', 'Brass', 'Brass', 'Brass']), pos=pick(v, [.8, .9, .6, .8, .3]), uni=3, det=.18)
    osc(p, 'B', 'Saw Tilt', pos=.8, fine=-6, level=.5)
    filt(p, 'Low 24', pick(v, [900, 1200, 800, 1500, 600]), .15, env=pick(v, [.3, .35, .3, .35, .2]), key=.5)
    fenv(p, pick(v, [.08, .03, .1, .05, .2]), .5, .5, .3)
    amp(p, pick(v, [.03, .01, .05, .02, .15]), .5, .9, .3)
    p.mod('Velocity', 'Filter 1 Cutoff', .1)
    return rev(p, .2, .6, 2)

@recipe('Brass & Winds', ['Breathy Flute', 'Pan Flute', 'Shakuhachi', 'Wooden Flute'], 'flute,breathy')
def wind_flute(v):
    p = P(); mono(p, .03)
    osc(p, 'A', 'Flute', pos=pick(v, [.3, .1, .5, .2]))
    noise(p, pick(v, [.12, .15, .2, .1]), pick(v, [.2, .4, -.2, 0]))
    filt(p, 'Low 12', 5000, .1, key=.5)
    amp(p, pick(v, [.06, .04, .08, .05]), .3, .9, .2)
    lfo(p, 1, 'Sine', 5, fade=.5, retrig=1); p.mod('LFO 1', 'Pitch', .005)
    menv(p, .001, .12, 0, .1); p.mod('Mod Env', 'Noise Level', .3)
    return rev(p, .3, .7, 3)

@recipe('Brass & Winds', ['Clarinet', 'Oboe', 'Bassoon', 'Reed Section'], 'reed,woodwind')
def wind_reed(v):
    p = P(); mono(p, .02) if v != 3 else None
    osc(p, 'A', pick(v, ['Reed', 'FM Reed', 'Reed', 'Reed']), pos=pick(v, [.4, .5, .2, .6]), oct=pick(v, [0, 0, -1, 0]), uni=(1 if v != 3 else 3), det=.12)
    filt(p, 'Low 12', pick(v, [2500, 3500, 1200, 3000]), .2, env=.1, key=.6)
    amp(p, .04, .3, .9, .15)
    lfo(p, 1, 'Sine', 5, fade=.6, retrig=1); p.mod('LFO 1', 'Pitch', .004)
    return rev(p, .2, .55, 2)

@recipe('Brass & Winds', ['Trumpet Lead', 'Muted Trumpet', 'French Horn', 'Tuba'], 'brass,solo')
def brass_solo(v):
    p = P(); mono(p, .03)
    osc(p, 'A', 'Brass', pos=pick(v, [.7, .4, .3, .2]), oct=pick(v, [0, 0, 0, -1]))
    filt(p, 'Low 24', pick(v, [1200, 700, 700, 500]), pick(v, [.2, .45, .2, .2]), env=.3, key=.6)
    fenv(p, .05, .3, .5, .2)
    amp(p, .03, .3, .9, .2)
    lfo(p, 1, 'Sine', 5.5, fade=.5, retrig=1); p.mod('LFO 1', 'Pitch', .004)
    return rev(p, .25, .6, 2.5)

# ===================================================================== ORGANS
@recipe('Organs', ['Gospel Drawbars', 'Rock Organ', 'Jazz Organ', 'Full Stops', 'Percussive Organ'], 'organ,tonewheel')
def organ_drawbar(v):
    p = P()
    osc(p, 'A', 'Organ', pos=pick(v, [.6, .8, .2, .6, .4]), phase=0)
    nofilt(p)
    amp(p, .003, .1, 1, .05)
    if v == 4: menv(p, .001, .2, 0, .1); p.update(oscB_on=1, oscB_table='Basic Shapes', oscB_oct=1, oscB_semi=7, oscB_level=0); p.mod('Mod Env', 'Osc B Level', .6)
    lfo(p, 1, 'Sine', pick(v, [6.5, 6.8, 0.8, 6.5, 6.2]))   # rotary speaker
    p.mod('LFO 1', 'Pan', .5).mod('LFO 1', 'Amp', -.15).mod('LFO 1', 'Pitch', .0015)
    if v == 1: dist(p, 'Tube', .35, .6, .6)
    cho(p, .3, 3, .2)
    return rev(p, .18, .5, 1.8)

@recipe('Organs', ['Cathedral Organ', 'Pipe Organ', 'Chapel'], 'organ,church')
def organ_pipe(v):
    p = P()
    osc(p, 'A', 'Organ', pos=pick(v, [1, .55, .3]))
    osc(p, 'B', 'Flute', pos=.4, oct=pick(v, [-1, 1, 0]), level=.4)
    nofilt(p)
    amp(p, .08, .1, 1, .6)
    return rev(p, .45, 1, 7, .4, 30)

@recipe('Organs', ['Combo Organ', 'Sixties Farfisa', 'Reed Organ'], 'organ,vintage')
def organ_combo(v):
    p = P()
    osc(p, 'A', pick(v, ['PWM', 'Reed', 'Reed']), pos=pick(v, [.3, .4, .2]))
    osc(p, 'B', 'Organ', pos=.4, level=.5)
    filt(p, 'Low 12', 6000, .1)
    amp(p, .005, .1, 1, .08)
    lfo(p, 1, 'Sine', 6); p.mod('LFO 1', 'Pitch', .003)
    return rev(p, .2, .5, 1.5)

# ===================================================================== CHOIRS & VOX
@recipe('Choirs & Vox', ['Aah Choir', 'Ooh Choir', 'Cathedral Voices', 'Boys Choir', 'Dark Choir'], 'choir,vocal')
def choir(v):
    p = P()
    osc(p, 'A', 'Choir', pos=pick(v, [.85, .1, .6, .4, .2]), uni=6, det=.25, spread=1)
    osc(p, 'B', 'Vowels', pos=pick(v, [0, .9, .1, .4, .8]), oct=pick(v, [0, 0, -1, 1, -1]), level=.4, uni=3, det=.2)
    noise(p, .03, .2)
    filt(p, 'Low 24', pick(v, [4000, 2500, 3500, 5500, 1800]), .1, key=.3)
    amp(p, pick(v, [.5, .7, 1, .4, 1]), 1, .9, 2.5)
    lfo(p, 1, 'Sine', 4.8, fade=1, retrig=1); p.mod('LFO 1', 'Pitch', .003)
    cho(p, .35, .3, .5)
    return rev(p, .45, .9, 6, .5, 30)

@recipe('Choirs & Vox', ['Vocal Chop', 'Hey Stab', 'Formant Pluck', 'Pitched Vox'], 'vocal,chop,modern')
def vox_chop(v):
    p = P()
    osc(p, 'A', pick(v, ['Vowels', 'Talk Bass', 'Vowels', 'Choir']), pos=pick(v, [.15, .5, .5, .8]), phase=0, uni=2, det=.08)
    menv(p, .001, .25, 0, .1); p.mod('Mod Env', 'Osc A Position', pick(v, [.5, -.3, .4, -.4]))
    filt(p, 'Low 12', 6000, .1)
    amp(p, .002, pick(v, [.3, .2, .25, .4]), 0, .2)
    dly(p, .25, '1/8 D', .35)
    return rev(p, .25, .6, 2)

@recipe('Choirs & Vox', ['Breathy Vox Pad', 'Angel Breath', 'Soft Hum'], 'vocal,soft,airy')
def vox_breath(v):
    p = P()
    osc(p, 'A', pick(v, ['Vowels', 'Choir', 'Vowels']), pos=pick(v, [.95, .5, .75]), uni=4, det=.2, level=.6)
    noise(p, .25, .5)
    filt(p, 'Vowel', pick(v, [500, 300, 1500]), .4)
    lfo(p, 1, 'Smooth Random', .2); p.mod('LFO 1', 'Filter 1 Cutoff', .15)
    amp(p, 1, 1, .9, 3)
    return rev(p, .45, .9, 6)

# ===================================================================== ARPS
def arp_base(p, cat_div, mode, octs, gate):
    arp(p, mode, cat_div, octs, gate); return p

@recipe('Arps & Sequences', ['Running Water', 'Eighties Arp', 'Up and Over', 'Bright Steps', 'City Lights'], 'arp,synced')
def arp_pluck(v):
    p = P()
    osc(p, 'A', 'Saw Tilt', pos=.85, uni=3, det=.15, phase=0)
    filt(p, 'Low 24', 600, .3, env=.35, key=.5)
    fenv(p, .001, .18, 0, .1)
    amp(p, .001, .25, 0, .15)
    arp(p, pick(v, ['Up', 'Up Down', 'Up', 'Random', 'As Played']), pick(v, ['1/16', '1/16', '1/8', '1/16', '1/16 T']), pick(v, [2, 1, 3, 2, 1]), .45)
    lfo(p, 1, 'Triangle', div='2 Bars'); p.mod('LFO 1', 'Filter 1 Cutoff', .12)
    dly(p, .25, '1/8 D', .4)
    return rev(p, .2, .6, 2)

@recipe('Arps & Sequences', ['Berlin School', 'Sequencer Drive', 'Krautrock Pulse', 'Tangerine Steps'], 'sequence,analog,vintage')
def arp_berlin(v):
    p = P()
    osc(p, 'A', pick(v, ['PWM', 'Saw Tilt', 'Saw-Square', 'PWM']), pos=.4, phase=0)
    osc(p, 'B', 'Saw Tilt', pos=.7, fine=8, level=.5, phase=0)
    filt(p, 'Ladder', 400, .55, env=.35, key=.4)
    fenv(p, .001, .15, .05, .1)
    amp(p, .001, .2, .3, .1)
    arp(p, pick(v, ['Up', 'Up Down', 'Down', 'Up']), '1/16', pick(v, [2, 2, 1, 3]), .6)
    lfo(p, 1, 'Sine', div='4 Bars'); p.mod('LFO 1', 'Filter 1 Cutoff', .15)
    dly(p, .2, '1/8 D', .45, .4)
    return rev(p, .2, .6, 2.5)

@recipe('Arps & Sequences', ['Crystal Arp', 'Glass Cascade', 'Twinkle'], 'arp,bright,bell')
def arp_bell(v):
    p = P()
    osc(p, 'A', pick(v, ['FM Glass', 'Crystal', 'FM Bell']), pos=.4, phase=0)
    nofilt(p)
    amp(p, .001, .6, 0, .4)
    arp(p, pick(v, ['Up', 'Up Down', 'Random']), pick(v, ['1/16', '1/8 T', '1/16']), 3, .4)
    dly(p, .35, '1/8 D', .5)
    return rev(p, .35, .8, 4)

@recipe('Arps & Sequences', ['Trance Gate', 'Gated Supersaw', 'Chopped Pad', 'Stutter Chords'], 'gate,trance,rhythmic')
def arp_gate(v):
    p = P()
    osc(p, 'A', 'Saw Tilt', pos=.95, uni=7, det=.45, spread=1)
    filt(p, 'Low 12', 7000, .1)
    lfo(p, 1, pick(v, ['Square', 'Square', 'Ramp Down', 'Square']), div=pick(v, ['1/16', '1/8', '1/8', '1/16 T']))
    p.mod('LFO 1', 'Amp', -.95)
    amp(p, .02, 1, 1, .4)
    dly(p, .2, '1/8 D', .3)
    return rev(p, .3, .7, 3)

@recipe('Arps & Sequences', ['Acid Arp', 'Rave Arp', 'Squelch Steps'], 'acid,arp')
def arp_acid(v):
    p = P()
    osc(p, 'A', 'Saw Tilt', pos=.95, phase=0)
    filt(p, 'Ladder', 350, .8, env=.4, drive=.3)
    fenv(p, .001, .2, .05, .1)
    amp(p, .001, .3, .5, .08)
    lfo(p, 1, 'Triangle', div='2 Bars'); p.mod('LFO 1', 'Filter 1 Cutoff', .2)
    arp(p, pick(v, ['Up Down', 'Random', 'Up']), '1/16', pick(v, [2, 1, 3]), pick(v, [.5, .7, .3]))
    if v == 1: dist(p, 'Tube', .3, .6)
    return dly(p, .2, '1/8 D', .4)

@recipe('Arps & Sequences', ['Swing Arp', 'Lazy Shuffle', 'Bounce'], 'swing,groove')
def arp_swing(v):
    p = P()
    osc(p, 'A', pick(v, ['E-Piano', 'Plucked', 'Hollow Build']), pos=.4, phase=0)
    filt(p, 'Low 12', 4000, .1)
    amp(p, .001, .4, 0, .2)
    arp(p, pick(v, ['Up', 'As Played', 'Up Down']), '1/16', 2, .5, swing=pick(v, [.5, .65, .4]))
    return rev(p, .2, .5, 2)

@recipe('Arps & Sequences', ['Chord Pulse', 'Eighth Chords', 'Off-Beat Chords'], 'chord,arp,rhythmic')
def arp_chord(v):
    p = P()
    osc(p, 'A', 'Saw Tilt', pos=.8, uni=5, det=.3)
    filt(p, 'Low 24', 1200, .2, env=.3)
    fenv(p, .001, .2, .1, .1)
    amp(p, .001, .25, .2, .1)
    arp(p, 'Chord', pick(v, ['1/8', '1/8', '1/4']), 1, pick(v, [.3, .5, .4]), swing=pick(v, [0, 0, .5]))
    dly(p, .2, '1/8 D', .35)
    return rev(p, .25, .6, 2.5)

# ===================================================================== CHORDS & STABS
@recipe('Chords & Stabs', ['Rave Stab', 'Hoover', 'Old School Stab', 'Big Chord Stab'], 'stab,rave')
def stab_rave(v):
    p = P()
    osc(p, 'A', pick(v, ['Power Fifth', 'Saw Tilt', 'Harmonic Minor', 'Harmonic Major']), pos=.7, uni=5, det=.35, oct=pick(v, [-1, 0, -2, -2]))
    osc(p, 'B', 'Saw Tilt', pos=.8, level=.4)
    filt(p, 'Low 24', 1500, .3, env=.3)
    fenv(p, .001, .3, .1, .2)
    amp(p, .001, .4, .2, .2)
    if v == 1: mono(p, .08, False); p.update(oscB_semi=-12); lfo(p, 1, 'Sine', 4); p.mod('LFO 1', 'Unison Detune', .3)
    return rev(p, .2, .6, 2)

@recipe('Chords & Stabs', ['Deep House Chord', 'Minor Seventh', 'Garage Chord', 'Lo-Fi Chord'], 'chord,house')
def stab_house(v):
    p = P()
    osc(p, 'A', pick(v, ['Harmonic Minor', 'Harmonic Minor', 'Harmonic Major', 'Harmonic Minor']), pos=pick(v, [.3, .5, .4, .2]), oct=-2, uni=3, det=.15)
    filt(p, 'Low 24', pick(v, [900, 1300, 1600, 700]), .2, env=.25, key=.3)
    fenv(p, .001, .35, .1, .2)
    amp(p, .002, .5, .3, .3)
    if v == 3: dist(p, 'Bit Crush', .3, .4, .4); p['drift'] = .5
    dly(p, .25, '1/8 D', .45)
    return rev(p, .25, .6, 2.5)

@recipe('Chords & Stabs', ['Power Fifths', 'Fifth Lead Stab', 'Anime Brass Stab'], 'fifths,stab')
def stab_fifth(v):
    p = P()
    osc(p, 'A', 'Power Fifth', pos=pick(v, [.5, .8, .6]), oct=-1, uni=3, det=.2)
    filt(p, 'Low 24', 2500, .1, env=.2)
    fenv(p, .001, .3, .4, .2)
    amp(p, .002, .4, .6, .2)
    return rev(p, .2, .5, 2)

@recipe('Chords & Stabs', ['Orchestra Hit', 'Impact Stab', 'Brass Hit'], 'hit,orchestral,dramatic')
def stab_hit(v):
    p = P()
    osc(p, 'A', pick(v, ['Brass', 'Power Fifth', 'Brass']), pos=.8, uni=7, det=.3, oct=pick(v, [0, -1, 0]))
    osc(p, 'B', 'Bowed', pos=.7, oct=1, level=.5, uni=3)
    noise(p, .1, 0)
    filt(p, 'Low 24', 5000, .1)
    amp(p, .001, .5, 0, .4)
    return rev(p, .35, .8, 3)

# ===================================================================== DRONES
@recipe('Drones', ['Endless Ohm', 'Monolith', 'Deep Meditation', 'Tanpura Drone', 'Ancient Hum'], 'drone,meditative')
def drone_ohm(v):
    p = P()
    osc(p, 'A', pick(v, ['Saw Tilt', 'Growl', 'Choir', 'Bowed', 'Reed']), pos=.4, uni=5, det=.12, oct=-1)
    osc(p, 'B', pick(v, ['Basic Shapes', 'Spectral Comb', 'Basic Shapes', 'Power Fifth', 'Hollow Build']), pos=.2, oct=pick(v, [-1, 0, 0, -1, 0]), semi=pick(v, [7, 0, 12, 0, 7]), level=.5)
    filt(p, 'Low 24', pick(v, [800, 600, 1200, 1800, 900]), .3, key=.2)
    lfo(p, 1, 'Smooth Random', pick(v, [.05, .07, .04, .2, .06]))
    p.mod('LFO 1', 'Filter 1 Cutoff', .1).mod('LFO 1', 'Osc A Position', .3)
    amp(p, 3, 2, 1, 6)
    return rev(p, .45, 1, 12, .6, 50)

@recipe('Drones', ['Black Hole', 'Abyss', 'Pressure', 'Earthquake'], 'drone,dark,cinematic')
def drone_dark(v):
    p = P()
    osc(p, 'A', pick(v, ['Growl', 'Metallic', 'Saw Tilt', 'Spectral Flux']), pos=.5, uni=7, det=.5, oct=-2)
    noise(p, .15, -.8)
    filt(p, 'Low 24', 400, .45)
    lfo(p, 1, 'Smooth Random', .1); p.mod('LFO 1', 'Filter 1 Cutoff', .15).mod('LFO 1', 'Osc A Position', .4)
    dist(p, 'Tube', .3, .5, .3)
    amp(p, 4, 2, 1, 8)
    return rev(p, .4, 1, 15, .8)

@recipe('Drones', ['Bright Horizon Drone', 'Sun Drone', 'Overtone Singer'], 'drone,bright,overtone')
def drone_bright(v):
    p = P()
    osc(p, 'A', pick(v, ['Overtone Sweep', 'Shimmer', 'Overtone Sweep']), pos=.3, uni=3, det=.1)
    lfo(p, 1, 'Triangle', pick(v, [.05, .08, .1])); p.mod('LFO 1', 'Osc A Position', .5)
    filt(p, 'Low 12', 8000, .1)
    amp(p, 3, 2, 1, 6)
    return rev(p, .5, 1, 14, .3, 40)

# ===================================================================== FX
@recipe('FX & Risers', ['White Noise Riser', 'Tension Riser', 'Saw Riser', 'Eight Bar Lift'], 'riser,transition')
def fx_riser(v):
    p = P()
    if v != 0: osc(p, 'A', 'Saw Tilt', pos=.9, uni=7, det=.4)
    else: p['oscA_level'] = 0
    noise(p, pick(v, [1, .3, .2, .4]), .2)
    menv(p, pick(v, [4, 6, 4, 16]), 1, 1, .5)
    p.mod('Mod Env', 'Filter 1 Cutoff', .6).mod('Mod Env', 'Pitch', pick(v, [0, .5, 1, .5]))
    filt(p, 'Low 24', pick(v, [600, 200, 200, 300]), .35)
    amp(p, pick(v, [1.5, 3, 2, 2]), 1, 1, 1)
    lfo(p, 1, 'Sine', 4); p.mod('LFO 1', 'Pan', .5, 'Mod Env')
    dly(p, .3, '1/8 D', .5)
    return rev(p, .45, .9, 6)

@recipe('FX & Risers', ['Downlifter', 'Falling Star', 'Sweep Down', 'Power Down'], 'downlifter,transition')
def fx_down(v):
    p = P()
    osc(p, 'A', pick(v, ['Saw Tilt', 'FM Glass', 'Saw Tilt', 'Basic Shapes']), pos=.8, uni=5, det=.3)
    noise(p, pick(v, [.4, 0, .3, 0]), 0)
    menv(p, .001, pick(v, [3, 2, 4, 1.5]), 0, .5)
    p.mod('Mod Env', 'Filter 1 Cutoff', .5).mod('Mod Env', 'Pitch', pick(v, [.5, 1, .25, .8]))
    filt(p, 'Low 24', 300, .2)
    amp(p, .01, pick(v, [3, 2, 4, 1.5]), 0, .5)
    return rev(p, .4, .9, 5)

@recipe('FX & Risers', ['Laser Zap', 'Pew Pew', 'Sci-Fi Blip', 'Robot Chirp'], 'zap,sci-fi,short')
def fx_zap(v):
    p = P()
    osc(p, 'A', pick(v, ['Basic Shapes', 'Saw Sync', 'FM Saw', 'Bit Crush']), pos=.5, phase=0)
    menv(p, .001, pick(v, [.15, .1, .05, .2]), 0, .05)
    p.mod('Mod Env', 'Pitch', pick(v, [1, .8, .6, -.5]))
    nofilt(p)
    amp(p, .001, pick(v, [.2, .15, .1, .25]), 0, .1)
    dly(p, .3, '1/16', .5)
    return rev(p, .2, .5, 2)

@recipe('FX & Risers', ['Alien Transmission', 'Broken Radio', 'Data Stream', 'Random Computer'], 'glitch,random')
def fx_glitch(v):
    p = P()
    osc(p, 'A', pick(v, ['Spectral Flux', 'Bit Crush', 'Stairs', 'Metallic']), pos=.5, phase=0)
    lfo(p, 1, 'Sample & Hold', div=pick(v, ['1/16', '1/32', '1/16 T', '1/32']))
    p.mod('LFO 1', 'Pitch', pick(v, [.3, .1, .5, .4])).mod('LFO 1', 'Osc A Position', .5).mod('LFO 1', 'Filter 1 Cutoff', .2)
    filt(p, 'Low 12', 4000, .3)
    amp(p, .01, 1, 1, .5)
    dist(p, 'Downsample', .3, .4)
    return dly(p, .3, '1/16 D', .5)

@recipe('FX & Risers', ['Impact Boom', 'Cinematic Hit', 'Sub Drop'], 'impact,boom')
def fx_impact(v):
    p = P()
    osc(p, 'A', 'Basic Shapes', pos=0, phase=0, oct=-2)
    noise(p, pick(v, [.5, .6, 0]), -.5)
    menv(p, .001, pick(v, [.3, .4, 1.5]), 0, .3); p.mod('Mod Env', 'Pitch', pick(v, [.5, .4, .6]))
    filt(p, 'Low 24', 300, .1)
    amp(p, .001, pick(v, [2, 3, 2]), 0, 1.5)
    dist(p, 'Soft', .4, .6, .4)
    return rev(p, .4, 1, 6, .7)

# ===================================================================== PERCUSSION
@recipe('Percussion', ['Analog Kick', 'Deep Kick', 'Hard Kick', 'Techno Kick'], 'drum,kick')
def perc_kick(v):
    p = P(); mono(p, 0, False)
    osc(p, 'A', 'Basic Shapes', pos=pick(v, [0, 0, .1, .05]), phase=0, oct=-1)
    menv(p, .001, pick(v, [.04, .06, .03, .045]), 0, .03); p.mod('Mod Env', 'Pitch', pick(v, [.9, .8, 1, .9]))
    nofilt(p)
    amp(p, .001, pick(v, [.35, .6, .25, .4]), 0, .1)
    if v in (2, 3): dist(p, 'Tube', pick(v, [0, 0, .5, .35]), 1, .5)
    p['vel_sens'] = .5; p['drift'] = 0
    return p

@recipe('Percussion', ['Synth Snare', 'Noise Snare', 'Clap Burst', 'Rim Shot'], 'drum,snare')
def perc_snare(v):
    p = P()
    osc(p, 'A', 'Basic Shapes', pos=.33, level=pick(v, [.5, .2, 0, 1]), phase=0, oct=pick(v, [0, 0, 0, 1]))
    noise(p, pick(v, [.7, 1, 1, .5]), pick(v, [.2, .4, .1, .5]))
    menv(p, .001, .03, 0, .03); p.mod('Mod Env', 'Pitch', .2)
    filt(p, 'High 12', pick(v, [800, 1200, 900, 500]), pick(v, [.1, .15, .2, .3]))
    amp(p, .001, pick(v, [.2, .25, .3, .06]), 0, .15)
    if v == 2: lfo(p, 1, 'Ramp Down', 30, retrig=1); p.mod('LFO 1', 'Amp', -.6)
    p['vel_sens'] = .6
    return rev(p, .15, .4, 1.2)

@recipe('Percussion', ['Closed Hat', 'Open Hat', 'Shaker', 'Metallic Hat'], 'drum,hat')
def perc_hat(v):
    p = P()
    p['oscA_level'] = 0 if v != 3 else .4; p['oscA_table'] = 'Metallic'; p['oscA_oct'] = 3
    noise(p, 1, .8)
    filt(p, 'High 24', pick(v, [7000, 6000, 5000, 6500]), .2)
    amp(p, .001, pick(v, [.05, .4, .12, .08]), 0, pick(v, [.04, .3, .08, .05]))
    p['vel_sens'] = .7
    return rev(p, .1, .3, 1)

@recipe('Percussion', ['Synth Tom', 'Disco Tom', 'Electro Conga'], 'drum,tom')
def perc_tom(v):
    p = P()
    osc(p, 'A', 'Basic Shapes', pos=pick(v, [0, .33, 0]), phase=0)
    noise(p, .1, -.3)
    menv(p, .001, pick(v, [.15, .4, .05]), 0, .1); p.mod('Mod Env', 'Pitch', pick(v, [.3, .5, .2]))
    nofilt(p)
    amp(p, .001, pick(v, [.5, .8, .2]), 0, .2)
    return rev(p, .15, .4, 1.2)


# ===================================================================== MORE
@recipe('Keys', ['Digital Piano', 'Pop Keys', 'Wide Keys', 'Morning Keys'], 'keys,modern')
def keys_digital(v):
    p = P()
    osc(p, 'A', 'E-Piano', pos=pick(v, [.6, .5, .4, .3]), phase=0)
    osc(p, 'B', 'Saw Tilt', pos=.3, level=pick(v, [.2, .3, .25, .15]), uni=pick(v, [1, 2, 5, 3]), det=.15, phase=0)
    filt(p, 'Low 12', 3000, .1, env=.25, key=.5)
    fenv(p, .001, .8, .2, .4)
    amp(p, .001, 3, .25, .5)
    p.mod('Velocity', 'Filter 1 Cutoff', .15)
    cho(p, pick(v, [.3, .4, .6, .35]), .5, .5)
    return rev(p, .22, .5, 2)

@recipe('Keys', ['Vapor Keys', 'Chillhop Keys', 'Dusty Keys'], 'lo-fi,warm,vintage')
def keys_lofi(v):
    p = P()
    osc(p, 'A', pick(v, ['E-Piano', 'Reed', 'E-Piano']), pos=.3, phase=0)
    filt(p, 'Low 12', pick(v, [2200, 1800, 2500]), .15, key=.3)
    amp(p, .002, 2.5, .25, .5)
    p['drift'] = .7
    lfo(p, 1, 'Smooth Random', .6); p.mod('LFO 1', 'Pitch', .002)
    dist(p, pick(v, ['Bit Crush', 'Tube', 'Downsample']), pick(v, [.3, .25, .15]), .35, .45)
    eq(p, 1, -2, 2500, -5)
    cho(p, .4, .3, .5)
    return rev(p, .25, .5, 2)

@recipe('Atmospheres', ['Spectral Tide', 'Prism Field', 'Glass Garden', 'Iridescent'], 'wavetable,evolving,shimmer')
def atm_scan(v):
    p = P()
    osc(p, 'A', pick(v, ['Spectral Phaser', 'Spectral Comb', 'Crystal', 'Shimmer']), pos=0, uni=4, det=.2, spread=1)
    osc(p, 'B', pick(v, ['Overtone Sweep', 'Octave Stack', 'FM Glass', 'Air']), pos=.5, oct=1, level=.35, uni=2, det=.2)
    lfo(p, 1, 'Triangle', pick(v, [.04, .06, .05, .03]))
    p.mod('LFO 1', 'Osc A Position', .9).mod('LFO 1', 'Osc B Position', -.4)
    filt(p, 'Low 12', 7000, .15)
    amp(p, 2.5, 2, .9, 6)
    dly(p, .25, '1/4 D', .55, .4)
    return rev(p, .5, 1, 12, .4, 50)

@recipe('Atmospheres', ['Bowed Metal', 'Rusted Strings', 'Cello Ghost'], 'bowed,dark,texture')
def atm_bowed(v):
    p = P()
    osc(p, 'A', pick(v, ['Metallic', 'Bowed', 'Bowed']), pos=.5, uni=3, det=.3)
    noise(p, .2, -.2)
    filt(p, 'Comb', pick(v, [261.6, 130.8, 65.4]), .8, key=1)
    lfo(p, 1, 'Smooth Random', .3); p.mod('LFO 1', 'Filter 1 Reso', .15).mod('LFO 1', 'Osc A Position', .3)
    amp(p, 1.5, 2, .9, 5)
    return rev(p, .45, .9, 9, .6)

@recipe('Plucks', ['Karplus String', 'Nylon Pluck', 'Steel String', 'Koto'], 'physical,string,organic')
def pluck_ks(v):
    p = P()
    p['oscA_level'] = 0
    noise(p, 0, pick(v, [0, -.5, .3, .1]))
    menv(p, .001, .04, 0, .02); p.mod('Mod Env', 'Noise Level', 1)
    filt(p, 'Comb', 261.63, pick(v, [.93, .88, .96, .9]), key=1)
    filt2(p, 'Low 12', pick(v, [6000, 3000, 9000, 5000]))
    amp(p, .001, pick(v, [2.5, 1.5, 3, 1.2]), 0, .3)
    if v == 3: osc(p, 'A', 'Plucked', pos=.8, level=.2, phase=0)
    p['vel_sens'] = .8
    return rev(p, .25, .55, 2)

@recipe('Plucks', ['FM Pluck', 'Steel Pluck', 'Glassy Pick'], 'fm,pluck,bright')
def pluck_fm(v):
    p = P()
    osc(p, 'A', 'Basic Shapes', phase=0)
    osc(p, 'B', 'Basic Shapes', phase=0, oct=pick(v, [1, 2, 3]), level=0)
    menv(p, .001, pick(v, [.15, .25, .1]), 0, .1); p.mod('Mod Env', 'FM Amount', pick(v, [.5, .6, .4]))
    nofilt(p)
    amp(p, .001, pick(v, [.5, .8, .6]), 0, .3)
    dly(p, .2, '1/8 D', .35)
    return rev(p, .2, .5, 2)

@recipe('Bells & Mallets', ['Clock Tower', 'Wind Chimes', 'Space Bell', 'Ring Mod Bell'], 'bell,inharmonic')
def bell_ring(v):
    p = P()
    osc(p, 'A', 'Basic Shapes', phase=0)
    osc(p, 'B', 'Basic Shapes', phase=0, oct=pick(v, [1, 2, 1, 0]), semi=pick(v, [6, 4, 3, 7]), fine=pick(v, [17, 31, -23, 41]), level=0)
    p['fm_amount'] = pick(v, [.35, .25, .3, .45])
    menv(p, .001, pick(v, [3, 1, 4, 2]), 0, 2); p.mod('Mod Env', 'FM Amount', .2)
    nofilt(p)
    amp(p, .001, pick(v, [6, 2.5, 7, 3]), 0, pick(v, [4, 2, 5, 2]))
    if v == 2: dly(p, .3, '1/4 D', .5)
    return rev(p, .35, .85, 5)

@recipe('Bells & Mallets', ['Marimba', 'Xylophone', 'Log Drum'], 'mallet,wood')
def bell_marimba(v):
    p = P()
    osc(p, 'A', 'Basic Shapes', pos=0, phase=0)
    osc(p, 'B', 'Basic Shapes', pos=0, oct=pick(v, [2, 2, 1]), semi=pick(v, [0, 7, 7]), level=.0, phase=0)
    menv(p, .001, .03, 0, .02); p.mod('Mod Env', 'Osc B Level', pick(v, [.6, .8, .4])).mod('Mod Env', 'FM Amount', pick(v, [.2, .3, .15]))
    nofilt(p)
    amp(p, .001, pick(v, [.6, .35, .4]), 0, .3)
    p['vel_sens'] = .8
    return rev(p, .2, .45, 1.5)

@recipe('Strings', ['Synth Strings', 'Analog Strings', 'Poly Strings', 'Stringer'], 'strings,analog,vintage')
def str_synth(v):
    p = P()
    osc(p, 'A', pick(v, ['Saw Tilt', 'Saw Tilt', 'PWM', 'Saw Tilt']), pos=pick(v, [.75, .6, .4, .9]), uni=pick(v, [5, 3, 3, 7]), det=.25)
    osc(p, 'B', 'Saw Tilt', pos=.7, oct=1, level=.3)
    filt(p, 'Low 24', pick(v, [3000, 2200, 2500, 4000]), .1, key=.3)
    filt2(p, 'High 12', 250)
    amp(p, pick(v, [.4, .5, .3, .2]), 1, .9, 1.2)
    cho(p, .7, pick(v, [.6, .4, .8, 1.0]), .7)
    return rev(p, .3, .7, 3)

@recipe('Strings', ['Octave Strings', 'High Violins', 'Film Score'], 'strings,cinematic')
def str_octave(v):
    p = P()
    osc(p, 'A', 'Bowed', pos=.6, uni=5, det=.2)
    osc(p, 'B', 'Bowed', pos=.5, oct=pick(v, [-1, 1, -1]), uni=3, det=.2, level=.55)
    filt(p, 'Low 24', 3500, .1, key=.3)
    amp(p, pick(v, [.5, .4, .9]), 1, .95, 2)
    lfo(p, 1, 'Sine', 5.2, fade=1.5, retrig=1); p.mod('LFO 1', 'Pitch', .003)
    return rev(p, .4, .85, 4.5, .5, 25)

@recipe('Organs', ['Horror Organ', 'Phantom', 'Detuned Organ'], 'organ,dark')
def organ_dark(v):
    p = P()
    osc(p, 'A', 'Organ', pos=pick(v, [.8, .6, .5]), uni=pick(v, [2, 3, 3]), det=pick(v, [.15, .2, .3]))
    osc(p, 'B', 'Organ', pos=.4, oct=-1, level=.5, semi=pick(v, [0, 6, 0]))
    nofilt(p)
    amp(p, .05, .1, 1, .8)
    lfo(p, 1, 'Sine', 5.5); p.mod('LFO 1', 'Pitch', .002)
    return rev(p, .45, .95, 7, .5)

@recipe('Organs', ['Ambient Organ', 'Swell Organ', 'Harmonium'], 'organ,soft')
def organ_ambient(v):
    p = P()
    osc(p, 'A', pick(v, ['Organ', 'Organ', 'Reed']), pos=pick(v, [.3, .5, .5]))
    osc(p, 'B', 'Flute', pos=.3, oct=1, level=.3)
    filt(p, 'Low 12', 4000, .1)
    amp(p, pick(v, [.8, 1.5, .15]), 1, 1, pick(v, [1.5, 2, .3]))
    if v == 2: noise(p, .05, -.3)
    cho(p, .3, .4, .4)
    return rev(p, .35, .8, 4)

@recipe('Choirs & Vox', ['Vocoder Choir', 'Robot Choir', 'Digital Voices', 'Formant Pad'], 'vocal,vocoder,electronic')
def choir_synth(v):
    p = P()
    osc(p, 'A', 'Saw Tilt', pos=.95, uni=pick(v, [3, 1, 5, 4]), det=.15)
    noise(p, .08, .5)
    filt(p, 'Vowel', pick(v, [500, 300, 1000, 2000]), .55)
    lfo(p, 1, pick(v, ['Triangle', 'Sample & Hold', 'Sine', 'Smooth Random']), pick(v, [.2, 2, .1, .3]))
    p.mod('LFO 1', 'Filter 1 Cutoff', pick(v, [.15, .2, .1, .2]))
    amp(p, pick(v, [.3, .01, .6, .8]), 1, .9, 1.5)
    cho(p, .4, .4, .5)
    return rev(p, .35, .8, 4)

@recipe('Chords & Stabs', ['Dub Chord', 'Techno Stab', 'Chord Memory', 'Detroit Stab'], 'chord,dub,techno')
def stab_dub(v):
    p = P()
    osc(p, 'A', pick(v, ['Harmonic Minor', 'Harmonic Minor', 'Harmonic Major', 'Harmonic Minor']), pos=.6, oct=-2, uni=2, det=.1)
    filt(p, pick(v, ['Low 24', 'Band', 'Low 24', 'Low 12']), pick(v, [700, 900, 1200, 1500]), pick(v, [.3, .3, .2, .25]), env=.2)
    fenv(p, .001, .2, 0, .1)
    amp(p, .001, pick(v, [.3, .2, .4, .25]), 0, .2)
    dly(p, pick(v, [.4, .3, .25, .3]), pick(v, ['1/8 D', '1/16 D', '1/8 D', '1/4 T']), pick(v, [.65, .5, .45, .55]), .4)
    return rev(p, .35, .8, 4, .6)

@recipe('Chords & Stabs', ['Future Bass Chord', 'Supersaw Chord', 'Wobble Chord'], 'chord,future bass')
def stab_future(v):
    p = P()
    osc(p, 'A', 'Saw Tilt', pos=.95, uni=7, det=.4, spread=1)
    osc(p, 'B', 'Saw Tilt', pos=.9, oct=1, uni=3, level=.35)
    filt(p, 'Low 24', 2500, .2, env=.25)
    fenv(p, .01, .4, .3, .3)
    amp(p, .01, .5, .7, .3)
    if v == 2: lfo(p, 1, 'Sine', div='1/8 T'); p.mod('LFO 1', 'Filter 1 Cutoff', .2).mod('LFO 1', 'Amp', -.3)
    comp(p, -18, 4, 8, 120, 4)
    return rev(p, .25, .7, 2.5)

@recipe('Drones', ['Infinite Strings', 'Glass Ocean', 'Iron Lung', 'Cave Resonance'], 'drone,texture')
def drone_texture(v):
    p = P()
    osc(p, 'A', pick(v, ['Bowed', 'FM Glass', 'Metallic', 'Air']), pos=.5, uni=6, det=.2)
    noise(p, pick(v, [0, .1, .2, .3]), -.4)
    filt(p, pick(v, ['Low 24', 'Low 12', 'Comb', 'Band']), pick(v, [1500, 4000, 110, 800]), pick(v, [.15, .1, .75, .4]))
    lfo(p, 1, 'Smooth Random', .07); p.mod('LFO 1', 'Osc A Position', .4).mod('LFO 1', 'Filter 1 Cutoff', .08)
    amp(p, 3, 2, 1, 7)
    return rev(p, .5, 1, 14, .5, 50)

@recipe('Leads', ['Gameboy', 'Eight-Bit Pulse', 'Arcade Hero'], 'chiptune,retro')
def lead_chip(v):
    p = P(); mono(p, 0, False)
    osc(p, 'A', pick(v, ['PWM', 'PWM', 'Stairs']), pos=pick(v, [.5, .75, .4]), phase=0)
    nofilt(p)
    amp(p, .001, .1, .85, .05)
    lfo(p, 1, 'Square', pick(v, [7, 0.5, 9]), retrig=1); p.mod('LFO 1', 'Pitch', pick(v, [.0, .0, .02]))
    if v == 1: p.mod('LFO 1', 'Osc A Position', .25)
    dist(p, 'Bit Crush', .5, .5, 1)
    return dly(p, .15, '1/8 D', .3)

@recipe('Bass', ['Neuro Bass', 'Foghorn', 'Mid Bass Growl'], 'neuro,dnb,aggressive')
def bass_neuro(v):
    p = P(); mono(p, .02)
    osc(p, 'A', pick(v, ['Growl', 'Talk Bass', 'Saw Sync']), pos=.4, uni=2, det=.1, phase=0, oct=-1)
    osc(p, 'B', 'Saw Tilt', pos=.9, oct=-1, fine=9, level=.6, phase=0)
    p['fm_amount'] = pick(v, [.08, .04, .12])
    sub(p, .5)
    lfo(p, 1, 'Smooth Random', div='1/4', retrig=1); p.mod('LFO 1', 'Osc A Position', .4).mod('LFO 1', 'FM Amount', .1)
    filt(p, 'Low 24', 1800, .25)
    filt2(p, 'Notch', 600, .3, parallel=False)
    dist(p, pick(v, ['Fold', 'Tube', 'Hard']), .4, .7, .7)
    comp(p, -16, 5, 3, 60, 3)
    amp(p, .002, .3, 1, .08)
    return p

@recipe('Pads', ['Wind Pad', 'Breath Pad', 'Tape Hiss Pad'], 'noise,airy,soft')
def pad_noise(v):
    p = P()
    osc(p, 'A', pick(v, ['Flute', 'Choir', 'Saw Tilt']), pos=.3, uni=3, det=.2, level=.5)
    noise(p, pick(v, [.5, .35, .25]), pick(v, [.2, .4, .8]))
    filt(p, 'Band', pick(v, [1200, 2200, 4000]), .2, key=.6)
    lfo(p, 1, 'Smooth Random', .25); p.mod('LFO 1', 'Filter 1 Cutoff', .15)
    amp(p, 1.2, 1, .9, 3)
    return rev(p, .45, .9, 6)

@recipe('FX & Risers', ['Filter Whoosh', 'Swoosh', 'Jet Pass'], 'whoosh,transition')
def fx_whoosh(v):
    p = P()
    p['oscA_level'] = 0
    noise(p, 1, pick(v, [0, .3, -.3]))
    menv(p, pick(v, [1, .4, 2]), pick(v, [1, .6, 2]), 0, .5)
    p.mod('Mod Env', 'Filter 1 Cutoff', .5)
    lfo(p, 1, 'Sine', .5); p.mod('LFO 1', 'Pan', .8, 'Mod Env')
    filt(p, 'Band', 300, .35)
    amp(p, pick(v, [.8, .3, 1.5]), pick(v, [1.5, .8, 2.5]), 0, .5)
    return rev(p, .4, .8, 4)

@recipe('Brass & Winds', ['Brass Section', 'Horn Ensemble', 'Big Band Stab'], 'brass,ensemble')
def brass_section(v):
    p = P()
    osc(p, 'A', 'Brass', pos=pick(v, [.6, .4, .8]), uni=5, det=.18)
    osc(p, 'B', 'Brass', pos=.5, oct=-1, level=.5, uni=3, det=.15)
    filt(p, 'Low 24', pick(v, [1000, 700, 1400]), .15, env=.3, key=.5)
    fenv(p, pick(v, [.06, .12, .02]), .4, .6, .3)
    amp(p, pick(v, [.04, .1, .01]), pick(v, [.5, .5, .3]), pick(v, [.9, .9, .3]), .3)
    p.mod('Velocity', 'Filter 1 Cutoff', .12)
    return rev(p, .3, .7, 2.5)

# ===================================================================== finishing
FREE = 12

def finalize(p, cat):
    phrase, wheel_kind, (mshape, mrate, mdiv) = CATS[cat]
    # Space macro needs the reverb running; a closed patch just starts at 0 % mix.
    if not p.get('reverb_on'): p.update(reverb_on=1, reverb_mix=0.0)
    cut = p.get('filter1_cutoff', 20000.0) if p.get('filter1_on', 1) else None
    extra = []
    # Macro 1 Tone
    if cut is None: extra.append(('Macro 1', 'None', 'Osc A Position', .35))
    else: extra.append(('Macro 1', 'None', 'Filter 1 Cutoff', .3 if cut < 6000 else -.3))
    # Macro 2 Motion: LFO 2 (or LFO 1 if the design already owns LFO 2)
    mlfo = 'LFO 1' if p.lfo2_used else 'LFO 2'
    if not p.lfo2_used:
        lfo(p, 2, mshape, rate=mrate, div=mdiv); p.lfo2_used = False
    extra.append((mlfo, 'Macro 2', 'Osc A Position' if p.get('oscA_on', 1) else 'Filter 1 Cutoff', .3))
    if cut is not None: extra.append((mlfo, 'Macro 2', 'Filter 1 Cutoff', .1))
    # Macro 3 Space
    extra.append(('Macro 3', 'None', 'Reverb Mix', .45))
    if p.get('delay_on'): extra.append(('Macro 3', 'None', 'Delay Mix', .3))
    # Macro 4 Grit
    if p.get('dist_on'): extra.append(('Macro 4', 'None', 'Dist Drive', .45))
    elif cut is not None: extra.append(('Macro 4', 'None', 'Filter 1 Drive', .75))
    else: extra.append(('Macro 4', 'None', 'FM Amount', .3) if p.get('oscB_on') else ('Macro 4', 'None', 'Noise Level', .25))
    # Mod wheel + aftertouch
    used_srcs = {m[0] for m in p.mods} | {m[1] for m in p.mods}
    if 'Mod Wheel' not in used_srcs:
        if wheel_kind == 'vibrato':
            lfo(p, 3, 'Sine', 5.5, retrig=1)
            extra.append(('LFO 3', 'Mod Wheel', 'Pitch', .012))
        elif cut is not None: extra.append(('Mod Wheel', 'None', 'Filter 1 Cutoff', .25 if cut < 6000 else -.25))
        else: extra.append(('Mod Wheel', 'None', 'Osc A Position', .4))
    if cut is not None: extra.append(('Aftertouch', 'None', 'Filter 1 Cutoff', .12 if cut < 8000 else -.12))
    mods = p.mods + extra
    while len(mods) > FREE: mods.pop()      # aftertouch is the first to go
    for i, (s, via, d, a) in enumerate(mods):
        n = i + 1
        p['mod%d_src' % n] = s; p['mod%d_via' % n] = via; p['mod%d_dst' % n] = d; p['mod%d_amt' % n] = a
    return p

def slug(name): return re.sub(r'[^A-Za-z0-9 \-]', '', name).strip()

def build():
    if os.path.isdir(OUT): shutil.rmtree(OUT)
    seen, out = set(), []
    for cat, names, tags, fn in RECIPES:
        for v, name in enumerate(names):
            if name in seen: sys.exit('duplicate preset name: ' + name)
            seen.add(name)
            p = finalize(fn(v), cat)
            text = ['@name=' + name, '@category=' + cat, '@tags=' + tags, '@author=Nacre Factory']
            for k, val in p.items():
                if isinstance(val, float): val = ('%.5g' % val)
                text.append('%s=%s' % (k, val))
            d = os.path.join(OUT, cat.replace('&', 'and'))
            os.makedirs(d, exist_ok=True)
            path = os.path.join(d, slug(name) + '.nacre')
            open(path, 'w').write('\n'.join(text) + '\n')
            out.append((cat, name, path, CATS[cat][0]))
    return out

if __name__ == '__main__':
    lst = build()
    from collections import Counter
    c = Counter(x[0] for x in lst)
    for k in CATS: print('%-20s %3d' % (k, c[k]))
    print('total', len(lst))
