/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    DISCO STRING FALL - Ultimate String Synthesizer
    
    The sound of Jamiroquai, Chic, Daft Punk, Earth Wind & Fire
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0xBU,
    .version = 0x00010000U,
    .name = "DISCOFALL",
    .num_params = 7,
    .params = {
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"FALLSPD"}},
        {0, 1023, 0, 307, k_unit_param_type_percent, 0, 0, 0, {"FALLDPT"}},
        {0, 1023, 0, 716, k_unit_param_type_percent, 0, 0, 0, {"DETUNE"}},
        {0, 1023, 0, 102, k_unit_param_type_percent, 0, 0, 0, {"ATTACK"}},
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"RELEASE"}},
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"CHORUS"}},
        {0, 1023, 0, 204, k_unit_param_type_percent, 0, 0, 0, {"PORTAM"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    }
};
