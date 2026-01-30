/*
BSD 3-Clause License
Copyright (c) 2024, KORG INC.

EDM GROOVEBOX - SEQUENCED OSC WITH KICK + CHORDS + PERCUSSION
Built-in groove engine with chord progressions
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0xFU,
    .version = 0x00010000U,
    .name = "GROOVEBOX",
    .num_params = 10,
    .params = {
        {0, 3, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"KICK PTN"}},
        {0, 7, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"CHORD"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"CLAP"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"HAT"}},
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"KICK VOL"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"CHORD VOL"}},
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"PERC VOL"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"TEMPO"}},
        {0, 3, 0, 1, k_unit_param_type_enum, 0, 0, 0, {"LOOP"}},
        {0, 1023, 0, 256, k_unit_param_type_percent, 0, 0, 0, {"HUMAN"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    },
};

