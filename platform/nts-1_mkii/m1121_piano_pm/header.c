/*
    Copyright (c) 2023, KORG INC.
    
    KORG M1 PIANO - HYBRID PHYSICAL MODELING
    2-Op FM Exciter + Karplus-Strong + Stiffness Allpass
    The authentic M1 metallic "clank" with wire inharmonicity
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x9U,
    .version = 0x00010000U,
    .name = "M1PIANOPM",
    .num_params = 10,
    .params = {
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"HARDNESS"}},
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"DECAY"}},
        {0, 1023, 0, 819, k_unit_param_type_percent, 0, 0, 0, {"STIFF"}},
        {0, 1023, 0, 307, k_unit_param_type_percent, 0, 0, 0, {"DETUNE"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"BRIGHT"}},
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"BODY"}},
        {0, 1023, 0, 256, k_unit_param_type_percent, 0, 0, 0, {"CHORUS"}},
        {0, 1023, 0, 205, k_unit_param_type_percent, 0, 0, 0, {"RELEASE"}},
        {0, 7, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"PRESET"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"VELOCITY"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    },
};

