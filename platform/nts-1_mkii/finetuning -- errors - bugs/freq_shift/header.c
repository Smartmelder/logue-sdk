/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    BODE FREQUENCY SHIFTER - Ring modulation with single-sideband
    Shifts all frequencies up or down by a fixed amount
*/

#include "unit_modfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_modfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x3U,
    .version = 0x00010000U,
    .name = "FREQSHFT",
    .num_params = 10,
    .params = {
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"SHIFT"}},
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"MIX"}},
        {0, 1023, 0, 307, k_unit_param_type_percent, 0, 0, 0, {"FEEDBACK"}},
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"SPREAD"}},
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"DETUNE"}},
        {0, 1023, 0, 256, k_unit_param_type_percent, 0, 0, 0, {"DISTORT"}},
        {0, 3, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"RANGE"}},
        {0, 1, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"DIRECTION"}},
        {0, 3, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"MODE"}},
        {0, 1, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"STEREO"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    },
};

