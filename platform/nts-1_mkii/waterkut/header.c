/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    WATERKUT - Raindrop Delay Effect V2
    
    10 parallel delay lines with tempo sync, chaos, modulation
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/

#include "unit_delfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_delfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x10U,
    .version = 0x00020000U,
    .name = "WATERKUT",
    .num_params = 11,
    .params = {
        {.min = 0, .max = 1023, .center = 0, .init = 819, .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {"TIME"}},
        {.min = 0, .max = 1023, .center = 0, .init = 768, .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {"DEPTH"}},
        {.min = -100, .max = 100, .center = 0, .init = 0, .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {"MIX"}},
        {.min = 0, .max = 1023, .center = 0, .init = 512, .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {"CHAOS"}},
        {.min = 0, .max = 1023, .center = 0, .init = 307, .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {"MODINT"}},
        {.min = 0, .max = 1023, .center = 0, .init = 102, .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {"MODRATE"}},
        {.min = 0, .max = 1023, .center = 0, .init = 512, .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {"TONE"}},
        {.min = 0, .max = 1023, .center = 0, .init = 768, .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {"STEREO"}},
        {.min = 1, .max = 10, .center = 5, .init = 10, .type = k_unit_param_type_none, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {"LINES"}},
        {.min = 0, .max = 1023, .center = 0, .init = 409, .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {"DIFFUSE"}},
        {.min = 0, .max = 1, .center = 0, .init = 0, .type = k_unit_param_type_none, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {"FREEZE"}},
        {.min = 0, .max = 0, .center = 0, .init = 0, .type = k_unit_param_type_none, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {""}}
    }
};
