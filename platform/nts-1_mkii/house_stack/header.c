/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    HOUSE STACK - Ultimate Chord/Lead Oscillator
    
    THE PERFECT OSCILLATOR FOR HOUSE & MELODIC TECHNO!
    
    FEATURES:
    - 3 internal voices (main, detune, chord)
    - 5 waveform types (mellow to bright digital)
    - 16 chord intervals (maj, min, 7th, 9th, sus, etc.)
    - Built-in tilt EQ (dark to bright)
    - Attack envelope shaping
    - Harmonic bending (waveshaping)
    - Portamento/glide
    - Internal LFO with routing macro
    - Stereo spread control
    - Unison detune spread
    
    WAVEFORMS:
    0: MELLOW - Triangle/sine blend (soft pads)
    1: SAW - Classic analog saw (leads)
    2: SQUARE - Pulse/square (organ-like)
    3: ORGAN - Drawbar-style additive (stabs)
    4: DIGITAL - Bright digital (hooks)
    
    CHORD INTERVALS:
    0: UNISON - No chord voice
    1: MAJ3 - Major third (+4 semitones)
    2: MIN3 - Minor third (+3 semitones)
    3: P5TH - Perfect fifth (+7 semitones)
    4: MAJ7 - Major seventh (+11 semitones)
    5: MIN7 - Minor seventh (+10 semitones)
    6: OCT - Octave (+12 semitones)
    7: 9TH - Major ninth (+14 semitones)
    8: 11TH - Perfect 11th (+17 semitones)
    9: SUS4 - Suspended 4th (+5 semitones)
    10: SUS2 - Suspended 2nd (+2 semitones)
    11: AUG - Augmented (+8 semitones)
    12: DIM - Diminished (+6 semitones)
    13: MAJCHORD - Major triad (0, +4, +7)
    14: MINCHORD - Minor triad (0, +3, +7)
    15: DOM7 - Dominant 7th (0, +4, +7, +10)
    
    Perfect for: House chords, melodic hooks, organ stabs,
                 progressive pads, techno leads, piano chords
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0xBU,
    .version = 0x00010000U,
    .name = "HOUSESTACK",
    .num_params = 10,
    .params = {
        // Param 0: Wave Type (Knop A)
        {
            .min = 0,
            .max = 4,
            .center = 0,
            .init = 1,  // SAW default
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"WAVE"}
        },
        
        // Param 1: Detune Amount (Knop B)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 410,  // 40% default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"DETUNE"}
        },
        
        // Param 2: Stereo Spread
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,  // 50% default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"STEREO"}
        },
        
        // Param 3: Chord Interval
        {
            .min = 0,
            .max = 15,
            .center = 0,
            .init = 0,  // Unison (no chord)
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"CHORD"}
        },
        
        // Param 4: Chord Spread
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 307,  // 30% default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"CHRDSPRD"}
        },
        
        // Param 5: Tone (Tilt EQ)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,  // 50% = neutral
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"TONE"}
        },
        
        // Param 6: Attack Shape
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 51,  // 5% = fast
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"ATTACK"}
        },
        
        // Param 7: Harmonic Bend
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 205,  // 20% default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"HARMBND"}
        },
        
        // Param 8: Glide/Portamento
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 0,  // Off default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"GLIDE"}
        },
        
        // Param 9: Mod Amount
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 205,  // 20% default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"MODAMT"}
        }
    },
};

