/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    STEPSEQ - Programmable Step Sequencer Modulation
    
    The ultimate ARP replacement with full control!
    
    FEATURES:
    - 16-step programmable sequencer
    - Per-step pitch offset (±2 octaves)
    - Per-step filter cutoff modulation
    - Per-step gate length control
    - Variable loop length (1-16 steps)
    - Tempo sync (MIDI clock)
    - Swing/shuffle (25-75%)
    - Ratcheting (1-4x repeats per step)
    - Step probability (controlled randomness)
    - Pattern save/recall (8 patterns)
    - Transpose mode
    - Direction: Forward/Reverse/Ping-Pong/Random
    
    HOW IT WORKS:
    - Modulates filter cutoff for rhythmic filtering
    - Adds pitch offset for melodic sequences
    - Gate length controls envelope modulation
    - Works with ANY oscillator/sound!
    
    LIKE A CV SEQUENCER BUT DIGITAL!
    
    Based on Korg logue SDK modfx template
    https://github.com/korginc/logue-sdk
*/

#include "unit_modfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_modfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x4U,
    .version = 0x00010000U,
    .name = "STEPSEQ",
    .num_params = 11,  // Changed from 10 to 11
    .params = {
        {0, 15, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"STEP"}},
        {-24, 24, 0, 0, k_unit_param_type_semi, 0, 0, 0, {"PITCH"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"FILTER"}},
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"GATE"}},
        {0, 15, 0, 15, k_unit_param_type_enum, 0, 0, 0, {"LENGTH"}},
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"SWING"}},
        {0, 3, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"RATCHET"}},
        {0, 1023, 0, 1023, k_unit_param_type_percent, 0, 0, 0, {"PROBAB"}},
        {0, 7, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"PATTERN"}},
        {0, 3, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"DIRECTN"}},
        {0, 1, 0, 1, k_unit_param_type_enum, 0, 0, 0, {"PLAY"}}  // NEW: Parameter 10
    },
};

