/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    90s RAVE MULTI-ENGINE
    
    The sound of early 90s Eurodance, Rave, and House!
    
    ENGINES:
    1. HOOVER - The vacuum cleaner screech (2 Unlimited)
    2. FM DONK - TX81Z metallic bass pluck (Quadrophonia)
    3. RAVE SAW - Thick detuned stabs (Praga Khan)
    4. HOUSE ORGAN - M1 Organ 2 sound (Robin S)
    
    FEATURES:
    - 4 synthesis engines
    - Shape: Timbre/Intensity per engine
    - Alt: Envelope decay time
    - Velocity sensitive
    - Band-limited (PolyBLEP)
    - Authentic 90s character
    
    INSPIRED BY:
    - Roland Alpha Juno "Hoover" patch
    - Yamaha TX81Z bass sounds
    - Supersaw detuning techniques
    - Korg M1 "Organ 2" PCM
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0xEU,
    .version = 0x00010000U,
    .name = "RAVE90s",
    .num_params = 10,
    .params = {
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"TIMBRE"}},
        {0, 1023, 0, 307, k_unit_param_type_percent, 0, 0, 0, {"DECAY"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"ATTACK"}},
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"RELEASE"}},
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"BRIGHT"}},
        {0, 1023, 0, 666, k_unit_param_type_percent, 0, 0, 0, {"PUNCH"}},
        {0, 1023, 0, 256, k_unit_param_type_percent, 0, 0, 0, {"DETUNE"}},
        {0, 1023, 0, 358, k_unit_param_type_percent, 0, 0, 0, {"DRIVE"}},
        {0, 3, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"ENGINE"}},
        {0, 7, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"PRESET"}}
    },
};

