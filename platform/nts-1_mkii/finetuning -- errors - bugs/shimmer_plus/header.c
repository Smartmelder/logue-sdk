/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    SHIMMER+ REVERB - Advanced pitch-shifting reverb
    
    FEATURES:
    - Pitch shifter (+12 semitones for shimmer effect)
    - Modulation (chorus-like tail movement)
    - Freeze mode (infinite sustain)
    - Envelope follower with ducking
    - High-pass & low-pass filters
    - Diffusion network (8 allpass stages)
    - 4 comb filters with cross-feedback
    - Early reflections
    - Pre-delay (up to 500ms)
    
    Based on Korg logue SDK revfx template
    https://github.com/korginc/logue-sdk
*/

#include "unit_revfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_revfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x6U,
    .version = 0x00010000U,
    .name = "SHIMMER+",
    .num_params = 11,
    .params = {
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"TIME"}},
        {0, 1023, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"SHIMMER"}},  // ✅ 0% default - FORCE OFF!
        {-100, 100, 0, 60, k_unit_param_type_drywet, 0, 0, 0, {"MIX"}},
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"MODRATE"}},
        {0, 1023, 0, 307, k_unit_param_type_percent, 0, 0, 0, {"MODDEP"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"LPCUT"}},
        {0, 1023, 0, 256, k_unit_param_type_percent, 0, 0, 0, {"HPCUT"}},
        {0, 1023, 0, 358, k_unit_param_type_percent, 0, 0, 0, {"PREDLY"}},  // Max 100ms now
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"DUCK"}},
        {0, 1, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"FREEZE"}},
        {0, 3, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"MODE"}}
    },
};

