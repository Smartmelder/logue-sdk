/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    ELECTRIBE OSC - Electribe-Style Oscillator
    
    Musical translation of Korg Electribe vibe:
    - Digitally gritty, rhythm-forward
    - Mid-focused, pattern-ready
    - 3 modes: VA / PCM / FX
    
    Perfect for techno/house patterns, stabs, basses, leads
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x11U,
    .version = 0x00010000U,
    .name = "E-TRIBE",
    .num_params = 10,
    .params = {
        {
            .min = 0, .max = 1023, .center = 0, .init = 341,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"E-MODE"}
        },
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"WAVECHAR"}
        },
        {
            .min = 0, .max = 1023, .center = 0, .init = 307,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"GRIT"}
        },
        {
            .min = 0, .max = 1023, .center = 0, .init = 614,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"BODY"}
        },
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"SNAP"}
        },
        {
            .min = 0, .max = 1023, .center = 0, .init = 205,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"MOTION"}
        },
        {
            .min = 0, .max = 1023, .center = 0, .init = 307,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"SPREAD"}
        },
        {
            .min = 0, .max = 1023, .center = 0, .init = 409,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DENSITY"}
        },
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"COLOR"}
        },
        {
            .min = 0, .max = 1023, .center = 0, .init = 409,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"PUNCH"}
        },
        {
            .min = 0, .max = 0, .center = 0, .init = 0,
            .type = k_unit_param_type_none, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {""}
        }
    }
};

