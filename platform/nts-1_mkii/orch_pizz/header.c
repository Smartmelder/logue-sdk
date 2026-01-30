/*
    BSD 3-Clause License
    Copyright (c) 2024, KORG INC.

    ORCHESTRAL PIZZICATO - 90s Sampler Emulation
    SuperSaw Chord Stack with Vintage Character
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x30U,
    .version = 0x00010000U,
    .name = "ORCH PIZZ",
    .num_params = 10,
    .params = {
        // Knob A: Pluck Decay Time
        {
            .min = 0, .max = 1023, .center = 0, .init = 410,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DECAY"}
        },

        // Knob B: SuperSaw Detune/Spread
        {
            .min = 0, .max = 1023, .center = 0, .init = 614,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"SPREAD"}
        },

        // Param 0: Chord Balance (Fifth/Octave Mix)
        {
            .min = 0, .max = 1023, .center = 0, .init = 717,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"CHORD"}
        },

        // Param 1: Filter Cutoff
        {
            .min = 0, .max = 1023, .center = 0, .init = 768,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"CUTOFF"}
        },

        // Param 2: Filter Resonance
        {
            .min = 0, .max = 1023, .center = 0, .init = 307,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"RESO"}
        },

        // Param 3: Bit Crush (Vintage Grit)
        {
            .min = 0, .max = 1023, .center = 0, .init = 768,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"BITS"}
        },

        // Param 4: Sample Rate Reduction
        {
            .min = 0, .max = 1023, .center = 0, .init = 205,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"SRATE"}
        },

        // Param 5: Stereo Width
        {
            .min = 0, .max = 1023, .center = 0, .init = 819,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"WIDTH"}
        },

        // Param 6: Velocity Sensitivity
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"VELSENS"}
        },

        // Param 7: Filter Mode
        {
            .min = 0, .max = 2, .center = 0, .init = 0,
            .type = k_unit_param_type_enum,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"FLTMODE"}
        },
    },
};
