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
    .num_params = 10,
    .params = {
        {0, 15, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"STEP"}},        // Knop A: Select step (0-15)
        {-24, 24, 0, 0, k_unit_param_type_semi, 0, 0, 0, {"PITCH"}},     // Knop B: Pitch for selected step
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"FILTER"}},   // Filter mod for selected step
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"GATE"}},     // Gate length for selected step
        {0, 15, 0, 15, k_unit_param_type_enum, 0, 0, 0, {"LENGTH"}},     // Sequence length (1-16)
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"SWING"}},    // Swing amount
        {0, 3, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"RATCHET"}},      // Ratcheting (1x/2x/3x/4x)
        {0, 1023, 0, 1023, k_unit_param_type_percent, 0, 0, 0, {"PROBAB"}},  // Step probability
        {0, 7, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"PATTERN"}},      // Pattern select (8 patterns)
        {0, 3, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"DIRECTN"}},      // Direction: FWD/REV/PING/RND
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    },
};
