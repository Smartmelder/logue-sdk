/*
    BSD 3-Clause License

    Copyright (c) 2023, KORG INC.
    
    CATHEDRAL REVERB + REVERSE
    Massive reverb met reverse buffer voor ultimate space
*/

#include "unit_revfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_revfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x4U,
    .version = 0x00010000U,
    .name = "CATHEDRAL",
    .num_params = 11,
    .params = {
        {0, 1023, 0, 307, k_unit_param_type_percent, 0, 0, 0, {"TIME"}},
        {0, 1023, 0, 205, k_unit_param_type_percent, 0, 0, 0, {"DEPTH"}},
        {-100, 100, 0, 35, k_unit_param_type_drywet, 0, 0, 0, {"MIX"}},
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"SIZE"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"DAMP"}},
        {0, 1023, 0, 256, k_unit_param_type_percent, 0, 0, 0, {"DIFF"}},
        {0, 1023, 0, 102, k_unit_param_type_percent, 0, 0, 0, {"EARLY"}},
        {0, 1023, 0, 154, k_unit_param_type_percent, 0, 0, 0, {"PREDLY"}},
        {0, 1023, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"REVS"}},
        {0, 1023, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"REVMIX"}},
        {0, 3, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"MODE"}}
    },
};

