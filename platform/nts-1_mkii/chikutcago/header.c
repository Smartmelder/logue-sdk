/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    CHIKUTCAGO - Chicago House Melodic Oscillator
    
    5 Classic Chicago House Sounds:
    0. HOUSE PIANO - M1-style metallic piano (2-OP FM)
    1. DEEP FLUTE - Lo-fi breathy sampler (parabolic sine + noise)
    2. BRASS STAB - Fat analog sawtooth (PolyBLEP)
    3. WAREHOUSE BELL - Inharmonic metallic pluck (FM 1:1.414)
    4. ACID DRONE - Complex evolving FM texture
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x1U,
    .version = 0x00010000U,
    .name = "CHIKUTCAGO",
    .num_params = 10,
    .params = {
        {0, 4, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"SOUND"}},
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"BRIGHT"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"DECAY"}},
        {0, 1023, 0, 307, k_unit_param_type_percent, 0, 0, 0, {"DETUNE"}},
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"ATTACK"}},
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"WARMTH"}},
        {0, 1023, 0, 256, k_unit_param_type_percent, 0, 0, 0, {"BODY"}},
        {0, 1023, 0, 205, k_unit_param_type_percent, 0, 0, 0, {"RELEASE"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"VELO"}},
        {0, 1023, 0, 307, k_unit_param_type_percent, 0, 0, 0, {"CHORUS"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    }
};

