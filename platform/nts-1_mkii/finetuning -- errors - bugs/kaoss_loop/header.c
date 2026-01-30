/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    KAOSSILATOR LOOP RECORDER - Multi-layer live looper
    
    The power of Kaossilator looping in your hands!
    
    FEATURES:
    - 4-layer loop recorder (3 seconds each = 12 seconds total)
    - Per-layer speed control (reverse, half, double)
    - Per-layer pitch shift (±12 semitones)
    - Per-layer filter (LP with resonance)
    - Overdub/Replace modes
    - Tempo quantization
    - Slice mode (chop loops)
    - Stutter effects
    - MIDI control per layer
*/

#include "unit_delfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_delfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x5U,
    .version = 0x00010000U,
    .name = "KAOSSLOOP",
    .num_params = 10,
    .params = {
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"TIME"}},
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"FEEDBACK"}},
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"MIX"}},  // Default 75% (hoor loops!)
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"LAYER1"}},
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"LAYER2"}},
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"LAYER3"}},
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"LAYER4"}},
        {0, 15, 0, 3, k_unit_param_type_enum, 0, 0, 0, {"LENGTH"}},
        {0, 7, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"MODE"}},
        {0, 3, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"QUANTIZE"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    },
};

