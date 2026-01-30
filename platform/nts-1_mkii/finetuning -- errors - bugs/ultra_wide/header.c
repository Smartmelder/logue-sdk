/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    ULTRA WIDE RANGE DELAY/LOOPER
    
    The ultimate delay/reverb hybrid for infinite possibilities!
    
    FEATURES:
    - 10ms to 125ms delays (dual stereo)
    - Multi-tap system (8 taps per side)
    - Shimmer reverb (pitch-shifted feedback)
    - Diffusion network (8×8 matrix)
    - Freeze mode (infinite hold)
    - Granular smearing
    - 8 special modes
    - Tempo sync with divisions
    - Stereo widening (Haas + diffusion)
    - Modulation (LFO, random, envelope)
    
    INSPIRED BY:
    - Eventide H9 (Space, ModEchoVerb)
    - Strymon BigSky/TimeLine
    - Empress Reverb/Echosystem
    - Chase Bliss/Cooper FX collaboration
*/

#include "unit_revfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_revfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x7U,
    .version = 0x00010000U,
    .name = "ULTRAWIDE",
    .num_params = 10,
    .params = {
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"TIME"}},
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"FEEDBACK"}},  // ✅ 60% default (was 75% - TE HOOG!)
        {0, 1023, 0, 819, k_unit_param_type_percent, 0, 0, 0, {"WIDTH"}},
        {0, 1023, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"SHIMMER"}},  // ✅ 0% default (was 40% - voorkomt gekraak!)
        {0, 1023, 0, 307, k_unit_param_type_percent, 0, 0, 0, {"DIFFUSE"}},
        {0, 1023, 0, 256, k_unit_param_type_percent, 0, 0, 0, {"MODULATE"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"MIX"}},
        {0, 15, 0, 3, k_unit_param_type_enum, 0, 0, 0, {"DIVISION"}},
        {0, 7, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"MODE"}},
        {0, 1, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"FREEZE"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    },
};

