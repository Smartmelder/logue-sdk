/*
    BSD 3-Clause License

    Copyright (c) 2023, KORG INC.
    All rights reserved.

    ACID 303++ - Ultimate Acid Groove Machine
    
    Features beyond TB-303:
    - 4-pole Moog ladder filter
    - Pre/Post distortion with flavors
    - Dual filter modes (Serial/Parallel/BP/Notch)
    - Sub oscillator engine (-1/-2 octave)
    - Sample & Hold LFO
    - Envelope follower modulation
    - Filter keyboard tracking
    - Accent dynamics system
    - Morphing waveforms
    - Slide with auto-overlap detection
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x3U,
    .version = 0x00020000U,  // V2 with sequencer!
    .name = "ACID303++",
    .num_params = 10,  // ✅ MAX 10 parameters (removed sequencer params to fix payload size)
    .params = {
        // Knop A & B (MIDI assignable)
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"CUTOFF"}},   // 0: A - ✅ 75% default (was 60%)
        {0, 1023, 0, 358, k_unit_param_type_percent, 0, 0, 0, {"SLIDE"}},    // 1: B
        
        // Extended parameters
        {0, 1023, 0, 870, k_unit_param_type_percent, 0, 0, 0, {"RESO"}},     // 2: Resonance
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"ENV AMT"}},  // 3: Filter env amount
        {0, 3, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"WAVE"}},             // 4: Square/Saw/Tri/Pulse
        {0, 1023, 0, 256, k_unit_param_type_percent, 0, 0, 0, {"AMP DEC"}},  // 5: Amp decay
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"FLT DEC"}},  // 6: Filter decay
        {-1023, 1023, 0, 0, k_unit_param_type_drywet, 0, 0, 0, {"DIST"}},    // 7: Pre(-)/Post(+)
        {0, 3, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"FLT MODE"}},         // 8: Filter topology
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"SUB MIX"}},  // 9: Sub oscillator
        
        // ✅ Sequencer parameters removed to fix "wrong user unit payload size" error
        // Sequencer functionality still works internally, but not exposed as parameters
        
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    },
};

