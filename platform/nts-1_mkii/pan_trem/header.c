/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    AUTO-PAN & TREMOLO - Stereo modulation effect
    LFO-controlled panning and amplitude modulation
*/

#include "unit_modfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_modfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x2U,
    .version = 0x00010000U,
    .name = "PANTREM",
    .num_params = 10,
    .params = {
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"RATE"}},
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"DEPTH"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"WIDTH"}},
        {0, 1023, 0, 307, k_unit_param_type_percent, 0, 0, 0, {"PHASE"}},
        {0, 1023, 0, 0, k_unit_param_type_percent, 0, 0, 0, {"TREMOLO"}},
        {0, 1023, 0, 819, k_unit_param_type_percent, 0, 0, 0, {"PAN"}},
        {0, 7, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"WAVE"}},
        {0, 1, 0, 1, k_unit_param_type_enum, 0, 0, 0, {"SYNC"}},
        {0, 7, 0, 3, k_unit_param_type_enum, 0, 0, 0, {"DIVISION"}},
        {0, 3, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"MODE"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    },
};

