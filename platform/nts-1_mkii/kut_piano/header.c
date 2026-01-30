/*
    Copyright (c) 2023, KORG INC.
    
     PIANO V2 - WAVETABLE EDITION
    Multi-sample wavetable synthesis met M1-geïnspireerde timbres
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x4U,
    .version = 0x00020000U,
    .name = "KUTPIANO",
    .num_params = 10,
    .params = {
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"BRIGHT"}},
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"DECAY"}},
        {0, 1023, 0, 358, k_unit_param_type_percent, 0, 0, 0, {"DETUNE"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"FORMNT"}},
        {0, 1023, 0, 666, k_unit_param_type_percent, 0, 0, 0, {"ATTACK"}},
        {0, 1023, 0, 307, k_unit_param_type_percent, 0, 0, 0, {"CHORUS"}},
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"VELO"}},
        {0, 1023, 0, 256, k_unit_param_type_percent, 0, 0, 0, {"RELEASE"}},
        {0, 7, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"PRESET"}},
        {0, 11, 0, 2, k_unit_param_type_enum, 0, 0, 0, {"CHORD"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    },
};

