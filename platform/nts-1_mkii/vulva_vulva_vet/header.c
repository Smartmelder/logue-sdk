/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    VULVA_VULVA_VET - Compact Plate Reverb
    Memory-optimized for NTS-1 mkII modfx
*/

#include "unit_modfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_modfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0xBU,
    .version = 0x00010000U,
    .name = "VULVA_VULVA_VET",
    .num_params = 3,
    .params = {
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"TIME"}},
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"DAMP"}},
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"MIX"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    }
};
