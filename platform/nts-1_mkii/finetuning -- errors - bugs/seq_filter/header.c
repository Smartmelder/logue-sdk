/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    SEQUENCE FILTER - Tempo-synced step sequencer filter
    16-step pattern with tempo sync, resonance, and modulation
*/

#include "unit_modfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_modfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x1U,
    .version = 0x00010000U,
    .name = "SEQFILT",
    .num_params = 10,
    .params = {
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"RANGE"}},
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"RESO"}},
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"SPEED"}},
        {0, 1023, 0, 307, k_unit_param_type_percent, 0, 0, 0, {"PATTERN"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"MIX"}},
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"FEEDBACK"}},
        {0, 15, 0, 3, k_unit_param_type_enum, 0, 0, 0, {"DIVISION"}},
        {0, 7, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"PRESET"}},
        {0, 1, 0, 1, k_unit_param_type_enum, 0, 0, 0, {"SYNC"}},
        {0, 1, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"DIRECTION"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    },
};

