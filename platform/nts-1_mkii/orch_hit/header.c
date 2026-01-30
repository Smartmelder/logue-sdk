/*
      Copyright (c) 2023, KORG INC.
    
    90s ORCHESTRA HIT - SPECTRAL STACKING SYNTHESIS
    4-Layer Engine: Brass + Strings + Timpani + Grit
    Emulates Fairlight ORCH5 / Stravinsky Firebird sample
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x7U,
    .version = 0x00010000U,
    .name = "ORCHHIT",
    .num_params = 10,
    .params = {
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"SIZE"}},
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"GRIT"}},
        {0, 1023, 0, 666, k_unit_param_type_percent, 0, 0, 0, {"IMPACT"}},
        {0, 1023, 0, 307, k_unit_param_type_percent, 0, 0, 0, {"RELEASE"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"BRASS"}},
        {0, 1023, 0, 819, k_unit_param_type_percent, 0, 0, 0, {"STRINGS"}},
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"TIMBRE"}},
        {0, 1023, 0, 256, k_unit_param_type_percent, 0, 0, 0, {"VINTAGE"}},
        {0, 7, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"PRESET"}},
        {0, 3, 0, 1, k_unit_param_type_enum, 0, 0, 0, {"VOICES"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    },
};

