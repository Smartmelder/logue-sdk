/*
    BSD 3-Clause License

    Copyright (c) 2023, KORG INC.
    
    KORG M1 "ORGAN 2" - 2-OPERATOR FM SYNTHESIS
    The iconic "Robin S - Show Me Love" organ sound
    Hollow, woody bass with percussive attack
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x8U,
    .version = 0x00010000U,
    .name = "ORGAN2",
    .num_params = 10,
    .params = {
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"HOLLOW"}},
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"PERCUSS"}},
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"OCTVSUB"}},
        {0, 1023, 0, 307, k_unit_param_type_percent, 0, 0, 0, {"CHORUS"}},
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"RELEASE"}},
        {0, 1023, 0, 256, k_unit_param_type_percent, 0, 0, 0, {"DIRT"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"FMRATIO"}},
        {0, 1023, 0, 205, k_unit_param_type_percent, 0, 0, 0, {"ATTACK"}},
        {0, 7, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"PRESET"}},
        {0, 3, 0, 3, k_unit_param_type_enum, 0, 0, 0, {"VOICES"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    },
};

