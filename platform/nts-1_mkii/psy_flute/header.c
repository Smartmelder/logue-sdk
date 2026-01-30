/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    PSY FLUTE - Psychedelic Synth-Flute Oscillator
    
    A musical "synth-flute" voice for house melodies, psychedelic ambient,
    and intros/breakdowns in electronic music.
    
    SONIC CHARACTER:
    - Flute-like core with dominant fundamental
    - Breathy attack with noise burst
    - Soft upper harmonics (not harsh)
    - Expressive vibrato and evolving timbre
    - Morphs from clean → house lead → psychedelic texture
    
    USE CASES:
    - House melodies/leads (cutting but flute-ish)
    - Psychedelic ambient (evolving, hypnotic drones)
    - Intros/breakdowns (intimate to ceremonial)
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x10U,  // 16 (0xF is already used by edm_groovebox)
    .version = 0x00010000U,
    .name = "PSYFLUTE",
    .num_params = 10,
    .params = {
        // Param 0: Flute Type
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 409,  // 40%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"TYPE"}
        },
        
        // Param 1: Breath
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 307,  // 30%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"BREATH"}
        },
        
        // Param 2: Brightness
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,  // 50%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"BRIGHT"}
        },
        
        // Param 3: Vibrato Rate
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 409,  // 40%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"VIBRATE"}
        },
        
        // Param 4: Vibrato Depth
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 307,  // 30%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"VIBDPTH"}
        },
        
        // Param 5: Tone Motion
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 205,  // 20%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"MOTION"}
        },
        
        // Param 6: Spread/Detune
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 307,  // 30%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"SPREAD"}
        },
        
        // Param 7: Attack Shape
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,  // 50%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"ATTACK"}
        },
        
        // Param 8: Harmonic Tilt
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,  // 50%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"HARMTLT"}
        },
        
        // Param 9: Space Helper
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,  // 50%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"SPACE"}
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

