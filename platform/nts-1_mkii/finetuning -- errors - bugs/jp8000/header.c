/*
    BSD 3-Clause License

    Copyright (c) 2023, KORG INC.
    
    ROLAND JP-8000 COMPLETE SYNTHESIZER ENGINE
    Supersaw, Feedback OSC, Dual Filters, LFOs, Motion Control
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x6U,
    .version = 0x00010000U,
    .name = "JP8000",
    .num_params = 10,
    .params = {
        {0, 1023, 0, 819, k_unit_param_type_percent, 0, 0, 0, {"SUPERSAW"}},
        {0, 1023, 0, 102, k_unit_param_type_percent, 0, 0, 0, {"CUTOFF"}},
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"RESO"}},
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"ENVMOD"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"FEEDBACK"}},
        {0, 1023, 0, 307, k_unit_param_type_percent, 0, 0, 0, {"LFO1"}},
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"LFO2"}},
        {0, 1023, 0, 256, k_unit_param_type_percent, 0, 0, 0, {"XMOD"}},
        {0, 15, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"WAVE"}},
        {0, 15, 0, 5, k_unit_param_type_enum, 0, 0, 0, {"MOTION"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    },
};

