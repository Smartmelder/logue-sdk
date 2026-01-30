/*
    BSD 3-Clause License

    Copyright (c) 2023, KORG INC.
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x0U,
    .version = 0x00020000U,
    .name = "J106MEGA",
    .num_params = 10,
    .params = {
        {0, 1023, 0, 341, k_unit_param_type_percent, 0, 0, 0, {"WAVE"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"FX"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"PW"}},
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"DETUNE"}},
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"SUB"}},
        {0, 1023, 0, 102, k_unit_param_type_percent, 0, 0, 0, {"HPF"}},
        {0, 1023, 0, 256, k_unit_param_type_percent, 0, 0, 0, {"LFO"}},
        {0, 1023, 0, 307, k_unit_param_type_percent, 0, 0, 0, {"LFODEP"}},
        {0, 15, 0, 5, k_unit_param_type_enum, 0, 0, 0, {"MODE"}},
        {0, 15, 0, 2, k_unit_param_type_enum, 0, 0, 0, {"FEAT"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    },
};

