/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    KUTSHIMMER - Shimmer/Ambient Reverb Effect
    
    Multi-mode reverb with shimmer, reverse, cloud and infinite modes
    
    MODES:
    - SHIMMER: Octave-up tails, emotional
    - REVERSE: Swelling tails, pre-drop
    - CLOUD: Dense, granular atmosphere
    - INFINITE: Frozen, endless soundscapes
    
    Perfect for house/techno intros, breakdowns, ambient
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/
#include "unit_revfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_revfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x13U,
    .version = 0x00010000U,
    .name = "KUTSHIMR",
    .num_params = 10,
    .params = {
        {.min = 0, .max = 3, .center = 0, .init = 0, .type = k_unit_param_type_enum, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {"TYPE"}},
        {.min = 0, .max = 1023, .center = 0, .init = 614, .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {"TIME"}},
        {.min = 0, .max = 1023, .center = 0, .init = 512, .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {"SHIMMER"}},
        {.min = 0, .max = 1023, .center = 0, .init = 512, .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {"MIX"}},
        {.min = 0, .max = 1023, .center = 0, .init = 307, .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {"MODRATE"}},
        {.min = 0, .max = 1023, .center = 0, .init = 409, .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {"MODDPTH"}},
        {.min = 0, .max = 1023, .center = 0, .init = 512, .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {"TONE"}},
        {.min = 0, .max = 1023, .center = 0, .init = 205, .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {"PREDLY"}},
        {.min = 0, .max = 1023, .center = 0, .init = 307, .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {"DUCK"}},
        {.min = 0, .max = 1, .center = 0, .init = 0, .type = k_unit_param_type_enum, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {"FREEZE"}},
        {.min = 0, .max = 0, .center = 0, .init = 0, .type = k_unit_param_type_none, .frac = 0, .frac_mode = 0, .reserved = 0, .name = {""}}
    }
};

