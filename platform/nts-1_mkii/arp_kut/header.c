/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    ARP KUT - Ultimate Arpeggiator Oscillator
    
    TRUE ARPEGGIATOR with 16 patterns!
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x10U,
    .version = 0x00010000U,
    .name = "ARPKUT",
    .num_params = 10,
    .params = {
        // Param 0: PATTERN (A knob)
        {
            .min = 0, .max = 15, .center = 0, .init = 0,
            .type = k_unit_param_type_enum, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"PATTERN"}
        },
        // Param 1: OCTAVES (B knob)
        {
            .min = 1, .max = 4, .center = 2, .init = 2,
            .type = k_unit_param_type_enum, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"OCTAVES"}
        },
        // Param 2: STEPS
        {
            .min = 1, .max = 16, .center = 8, .init = 8,
            .type = k_unit_param_type_enum, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"STEPS"}
        },
        // Param 3: GATE
        {
            .min = 0, .max = 1023, .center = 512, .init = 768,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"GATE"}
        },
        // Param 4: SWING
        {
            .min = 0, .max = 1023, .center = 512, .init = 512,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"SWING"}
        },
        // Param 5: ACCENT
        {
            .min = 0, .max = 1023, .center = 0, .init = 307,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"ACCENT"}
        },
        // Param 6: SHAPE
        {
            .min = 0, .max = 3, .center = 0, .init = 0,
            .type = k_unit_param_type_enum, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"SHAPE"}
        },
        // Param 7: DETUNE
        {
            .min = 0, .max = 1023, .center = 0, .init = 205,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DETUNE"}
        },
        // Param 8: SUB
        {
            .min = 0, .max = 1023, .center = 0, .init = 205,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"SUB"}
        },
        // Param 9: CHARACTER - NEW!
        {
            .min = 0, .max = 5, .center = 0, .init = 0,
            .type = k_unit_param_type_enum, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"CHAR"}
        },
        // Terminator entry (required by SDK)
        {
            .min = 0, .max = 0, .center = 0, .init = 0,
            .type = k_unit_param_type_none, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {""}
        }
    }
};

