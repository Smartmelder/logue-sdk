/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    SELF-RANDOMIZING AUDIO REPEATER
    
    A generative granular repeater that evolves itself!
    
    FEATURES:
    - 32 polyphonic grain buffers
    - Probability-based grain triggering (Markov chains)
    - Self-mutating patterns (AI-like evolution)
    - 8 randomization modes (Gentle → Industrial)
    - Pattern memory (8 snapshots)
    - Tempo sync with quantization
    - Spectral filtering per grain
    - Freeze/evolve control
    
    INSPIRED BY:
    - Autechre's generative systems
    - Mutable Instruments Clouds/Beads
    - Alesis Bitrman
    - Glitch aesthetics
*/

#include "unit_revfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_revfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x6U,
    .version = 0x00010000U,
    .name = "RANDREPT",
    .num_params = 10,
    .params = {
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"DENSITY"}},
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"CHAOS"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"MUTATE"}},
        {0, 1023, 0, 819, k_unit_param_type_percent, 0, 0, 0, {"GRAINSIZE"}},
        {0, 1023, 0, 307, k_unit_param_type_percent, 0, 0, 0, {"PITCH"}},
        {0, 1023, 0, 666, k_unit_param_type_percent, 0, 0, 0, {"FEEDBACK"}},
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"MIX"}},
        {0, 7, 0, 1, k_unit_param_type_enum, 0, 0, 0, {"MODE"}},
        {0, 7, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"PATTERN"}},
        {0, 1, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"FREEZE"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    },
};

