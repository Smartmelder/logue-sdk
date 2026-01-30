/*
    BSD 3-Clause License

    Copyright (c) 2023, KORG INC.
    
    BEHRINGER TD-3 / ROLAND TB-303 ACID BASS
    Complete TB-303 clone met 18dB/oct resonant filter
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x5U,
    .version = 0x00010000U,
    .name = "TD3ACID",
    .num_params = 10,
    .params = {
        {0, 1023, 0, 102, k_unit_param_type_percent, 0, 0, 0, {"CUTOFF"}},
        {0, 1023, 0, 819, k_unit_param_type_percent, 0, 0, 0, {"RESO"}},
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"ENVMOD"}},
        {0, 1023, 0, 205, k_unit_param_type_percent, 0, 0, 0, {"DECAY"}},
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"ACCENT"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"WAVE"}},
        {0, 1023, 0, 307, k_unit_param_type_percent, 0, 0, 0, {"DETUNE"}},
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"DIST"}},
        {0, 3, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"MODE"}},
        {0, 3, 0, 1, k_unit_param_type_enum, 0, 0, 0, {"SLIDE"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    },
};

