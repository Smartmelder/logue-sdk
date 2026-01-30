/*
    BSD 3-Clause License
    Copyright (c) 2024, KORG INC.
    
    ADVSEQ - Advanced Step Sequencer Modulation
    
    A powerful 128-step sequencer with pattern manipulation!
    
    FEATURES:
    - 128-step programmable sequence
    - 8 pattern operations (random, shuffle, reverse, slice copy, etc.)
    - Shift left/right with wrap-around
    - Palindrome (SLICECILS) patterns
    - Tempo sync (1/16 or MIDI trigger)
    - Swing/shuffle timing
    - Smooth glide between steps
    - Variable sequence length (1-128 steps)
    - Slice operations (1-32 steps)
    
    Based on Korg logue SDK modfx template
    https://github.com/korginc/logue-sdk
*/

#include "unit_modfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_modfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x5U,
    .version = 0x00010000U,
    .name = "ADVSEQ",
    .num_params = 10,
    .params = {
        {0, 1, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"CLOCK"}},        // 0: 1/16 or MIDI
        {1, 128, 16, 16, k_unit_param_type_enum, 0, 0, 0, {"SEQLEN"}},    // 1: Sequence length (center=init)
        {1, 32, 4, 4, k_unit_param_type_enum, 0, 0, 0, {"SLICELN"}},     // 2: Slice length (center=init)
        {0, 1023, 0, 256, k_unit_param_type_percent, 0, 0, 0, {"SMOOTH"}},   // 3: Glide
        {0, 1023, 0, 1023, k_unit_param_type_percent, 0, 0, 0, {"DEPTH"}},   // 4: Modulation depth
        {0, 7, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"OPER"}},         // 5: Operation
        {-64, 64, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"SHIFT"}},     // 6: Shift L/R
        {0, 4, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"RATEDIV"}},      // 7: Clock divider
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"SWING"}},    // 8: Swing
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"MIX"}},      // 9: Dry/Wet
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    },
};

