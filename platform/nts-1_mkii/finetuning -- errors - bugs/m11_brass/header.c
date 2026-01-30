/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    M1 BRASS ULTRA - Ultimate Enhanced M1 Recreation
    
    The most advanced M1 brass/strings synthesizer!
    
    FEATURES:
    - Dual oscillator system (Saw + Pulse)
    - 4-band formant filter (extra resonance!)
    - 10-voice ensemble mode (massive!)
    - Noise layer (breath/air simulation)
    - Attack transient layer (PCM-style)
    - Pitch envelope (swell/fall)
    - Filter LFO (movement)
    - Velocity layers (4 zones)
    - Vibrato with delay & fade-in
    - Stereo width control
    - 12 classic M1 patches
    
    ENHANCED VERSION - Maximum realism!
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0xCU,  // Different ID from ultra
    .version = 0x00010000U,
    .name = "M1BRASS",
    .num_params = 10,
    .params = {
        {0, 1023, 0, 614, k_unit_param_type_percent, 0, 0, 0, {"BRIGHT"}},      // ✅ 60% - ORIGINELE WERKENDE DEFAULT
        {0, 1023, 0, 768, k_unit_param_type_percent, 0, 0, 0, {"RESONANCE"}},   // ✅ 75% - ORIGINELE WERKENDE DEFAULT
        {0, 1023, 0, 512, k_unit_param_type_percent, 0, 0, 0, {"DETUNE"}},      // ✅ 50% - Perfect chorus
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"ENSEMBLE"}},    // ✅ 40% - Nice width
        {0, 1023, 0, 409, k_unit_param_type_percent, 0, 0, 0, {"VIBRATO"}},     // ✅ 40% - ORIGINELE WERKENDE DEFAULT
        {0, 1023, 0, 256, k_unit_param_type_percent, 0, 0, 0, {"BREATH"}},      // ✅ 25% - ORIGINELE WERKENDE DEFAULT
        {0, 1023, 0, 666, k_unit_param_type_percent, 0, 0, 0, {"ATTACK"}},      // ✅ 65% - ORIGINELE WERKENDE DEFAULT
        {0, 1023, 0, 819, k_unit_param_type_percent, 0, 0, 0, {"RELEASE"}},     // ✅ 80% - ORIGINELE WERKENDE DEFAULT
        {0, 11, 0, 0, k_unit_param_type_enum, 0, 0, 0, {"PATCH"}},
        {0, 3, 0, 2, k_unit_param_type_enum, 0, 0, 0, {"VOICES"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    },
};

