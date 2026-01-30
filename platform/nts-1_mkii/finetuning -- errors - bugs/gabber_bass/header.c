/*
    
    Copyright (c) 2023, KORG INC.
    
    GABBER BASS V2 - Hardcore gabber oscillator
    NOW WITH 10 PARAMETERS!
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x1U,
    .version = 0x00010000U,
    .name = "GABBER",
    .num_params = 10,
    .params = {
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"DIST"}},
        {0, 7, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"MODE"}},
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"PENV"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"SUB"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"DETUNE"}},
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"CUTOFF"}},
        {0, 1023, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"CRUSH"}},
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"DRIVE"}},
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"RESONAN"}},
        {0, 1023, 0, 307, k_unit_param_type_percent, 0, 0, 0, {"PUNCH"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    }
};
