/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    M1 BRASS & STRINGS - Ultimate Recreation
    
    The legendary Korg M1 (1988) sound engine recreated!
    
    FEATURES:
    - Dual oscillators (Saw + Pulse with PWM)
    - 3-band formant filter (authentic brass vowels!)
    - 8-voice ensemble (M1 "Lore" strings!)
    - Vibrato with delay & fade-in
    - Velocity layers (4-stage ADSR)
    - Breath controller (dynamic brightness)
    - Stereo chorus (ensemble shimmer)
    - 8 M1 patches (Brass/Strings/Choir/Sax/Flute/Horn)
    
    INSPIRED BY:
    - Korg M1 (1988) - The best-selling synthesizer ever
    - "Lore" strings - The trance/house intro sound
    - 90s power brass - Every hit record 1988-1995
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0xCU,
    .version = 0x00010000U,
    .name = "M1BRASS",
    .num_params = 10,
    .params = {
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"BRIGHT"}},
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"RESON"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"DETUNE"}},
        {0, 1023, 0, 307, k_unit_param_type_percent, 0, 0, 0, {"ENSEMB"}},
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"VIBRAT"}},
        {0, 1023, 0, 256, k_unit_param_type_percent, 0, 0, 0, {"BREATH"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"ATTACK"}},
        {0, 1023, 0, 819, k_unit_param_type_percent, 0, 0, 0, {"RELEASE"}},
        {0, 7, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"PATCH"}},
        {0, 3, 0, 1, k_unit_param_type_enum, 0, 0, 0, {"VOICES"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    },
};

