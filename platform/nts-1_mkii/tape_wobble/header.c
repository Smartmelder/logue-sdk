/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    TAPE WOBBLE SIMULATOR - Vintage tape machine emulation
    Wow, flutter, saturation, noise, and tape compression
*/

#include "unit_modfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_modfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x4U,
    .version = 0x00010000U,
    .name = "TAPEWOB",
    .num_params = 10,
    .params = {
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 614,
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"WOW"}
        },
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"FLUTTER"}
        },
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 768,
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"SATURAT"}
        },
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 307,
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"NOISE"}
        },
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 409,
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"COMPRESS"}
        },
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 256,
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"WARBLE"}
        },
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 666,
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"AGE"}
        },
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"MIX"}
        },
        {
            .min = 0,
            .max = 7,
            .center = 0,
            .init = 2,
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"TAPE"}
        },
        {
            .min = 0,
            .max = 3,
            .center = 0,
            .init = 1,
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"SPEED"}
        },
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

