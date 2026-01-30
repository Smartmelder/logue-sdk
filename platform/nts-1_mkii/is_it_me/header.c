/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.

    IS IT ME - Melancholic Reverb
    A professional reverb with pristine sound quality
*/

#include "unit_revfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_revfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x20U,
    .version = 0x00010000U,
    .name = "IS IT ME",
    .num_params = 11,
    .params = {
        // Knob A: Reverb Time
        {
            .min = 0, .max = 1023, .center = 0, .init = 665,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"TIME"}
        },

        // Knob B: Reverb Depth
        {
            .min = 0, .max = 1023, .center = 0, .init = 409,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DEPTH"}
        },

        // Param 0: Dry/Wet Mix
        {
            .min = -100, .max = 100, .center = 0, .init = 40,
            .type = k_unit_param_type_drywet,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"MIX"}
        },

        // Param 1: Room Size
        {
            .min = 0, .max = 1023, .center = 0, .init = 614,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"SIZE"}
        },

        // Param 2: HF Damping
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DAMP"}
        },

        // Param 3: Diffusion
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DIFFUSE"}
        },

        // Param 4: Pre-Delay
        {
            .min = 0, .max = 1023, .center = 0, .init = 154,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"PREDLY"}
        },

        // Param 5: Early Reflections
        {
            .min = 0, .max = 1023, .center = 0, .init = 205,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"EARLY"}
        },

        // Param 6: Highpass Frequency (bass exclusion)
        {
            .min = 0, .max = 1023, .center = 0, .init = 154,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"HP"}
        },

        // Param 7: Lowpass Frequency
        {
            .min = 0, .max = 1023, .center = 0, .init = 870,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"LP"}
        },

        // Param 8: Mode (ROOM/HALL/CATHEDRAL)
        {
            .min = 0, .max = 2, .center = 0, .init = 1,
            .type = k_unit_param_type_enum,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"MODE"}
        },
    },
};
