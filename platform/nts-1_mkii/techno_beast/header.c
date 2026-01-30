/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    TECHNO BEAST - Ultimate Lead/Bass Oscillator
    
    The most wanted techno oscillator for NTS-1 mkII!
    
    FEATURES:
    - Supersaw/Supersquare unison engine (3-7 voices)
    - Octave doubling mode (perfect octaves)
    - Sub oscillator (-1 & -2 octaves)
    - Hard sync (classic acid techno)
    - PWM modulation (auto-sweep)
    - Built-in resonant filter
    - Analog-style overdrive
    - 8 powerful parameters
    
    MODES:
    0: UNISON SAW - Classic supersaw (7 voices)
    1: UNISON SQR - Supersquare (5 voices)
    2: OCTAVE SAW - Octave doubling (3 octaves)
    3: OCTAVE SQR - Octave square (3 octaves)
    
    Perfect for: Techno leads, acid basslines, rave stabs, trance plucks
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x2U,
    .version = 0x00010000U,
    .name = "TECHNOBST",  // 10 char max
    .num_params = 11,  // ✅ Updated: 10 → 11 (Noise removed, Accent + Glide added)
    .params = {
        // Param 0: Mode (Knop A)
        {
            .min = 0,
            .max = 3,
            .center = 0,
            .init = 0,  // UNISON SAW default
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"MODE"}
        },
        
        // Param 1: Detune/Spread (Knop B)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 614,  // 60% default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"DETUNE"}
        },
        
        // Param 2: Sub Oscillator Mix
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 410,  // 40% default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"SUB"}
        },
        
        // Param 3: Hard Sync Amount
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 0,  // Off default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"SYNC"}
        },
        
        // Param 4: PWM Depth
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 307,  // 30% default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"PWM"}
        },
        
        // Param 5: Filter Cutoff
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 819,  // 80% default (open)
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"FILTER"}
        },
        
        // Param 6: Filter Resonance
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 307,  // 30% default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"RESON"}
        },
        
        // Param 7: Overdrive/Distortion
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 205,  // 20% default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"DRIVE"}
        },
        
        // Param 8: Accent (Velocity Sensitivity) ✅ NEW!
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,  // 50% default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"ACCENT"}
        },
        
        // Param 9: Glide (Portamento) ✅ NEW!
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
        
        // Param 10: Phase Spread
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 0,  // Off default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"PHSSPRD"}
        },
        
        // Terminator
        {
            .min = 0,
            .max = 0,
            .center = 0,
            .init = 0,
            .type = k_unit_param_type_none,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {""}
        }
    },
};

