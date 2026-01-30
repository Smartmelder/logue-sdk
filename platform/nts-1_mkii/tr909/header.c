/*
    BSD 3-Clause License

    Copyright (c) 2023, KORG INC.
    
    ROLAND TR-909 DRUM SYNTHESIZER
    Kick, Snare, Open Hi-Hat synthesis
    The sound of techno, house, and rave
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0xAU,
    .version = 0x00010000U,
    .name = "TR909",
    .num_params = 10,
    .params = {
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"ATTACK"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"DECAY"}},
        {0, 1023, 0, 819, k_unit_param_type_percent, 0, 0, 0, {"TONE"}},
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"PUNCH"}},
        {0, 1023, 0, 307, k_unit_param_type_percent, 0, 0, 0, {"SNAP"}},
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"METALLIC"}},
        {0, 1023, 0, 256, k_unit_param_type_percent, 0, 0, 0, {"NOISE"}},
        {0, 1023, 0, 666, k_unit_param_type_percent, 0, 0, 0, {"DIST"}},
        {0, 11, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"SOUND"}},
        {0, 7, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"PRESET"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    },
};

