/*
    BSD 3-Clause License
    Copyright (c) 2024, KORG INC.
    
    HOUSE/GABBER MULTI-FX MODULATION
    
    All essential modulation effects in one unit!
    
    MODES:
    0. CHORUS - Thick pads/stabs (house)
    1. FLANGER - Sweeping risers/snares
    2. PHASER - Percussive movement
    3. TREMOLO - Rhythmic pump
    4. VIBRATO - Pitch wobble
    5. AUTOPAN - Stereo groove
    6. RINGMOD - Gabber metallic
    7. COMBO - Multi-effect blend
    
    FEATURES:
    - Tempo sync (1/16 to 1/1)
    - 4 LFO shapes
    - Stereo widening
    - Mode morphing
    - Memory optimized (<6KB buffers)
    
    Based on Korg logue SDK modfx template
    https://github.com/korginc/logue-sdk
*/

#include "unit_modfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_modfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x8U,
    .version = 0x00010000U,
    .name = "MULTIFX",
    .num_params = 10,
    .params = {
        {0, 7, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"MODE"}},
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"RATE"}},  // ✅ LFO RATE control (0-100%)
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"DEPTH"}},
        {0, 1023, 0, 307, k_unit_param_type_percent, 0, 0, 0, {"FEEDBACK"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"MIX"}},
        {0, 5, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"SYNC"}},
        {0, 3, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"SHAPE"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"STEREO"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"COLOR"}},
        {0, 1023, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"MORPH"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    },
};

