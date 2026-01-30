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
    .num_params = 10,  // ✅ 10 parameters (max for modfx is 10!)
    .params = {
        // ✅ PARAM 0: PLAY/STOP (Knop A when MOD held) - MOST IMPORTANT!
        {
            .min = 0,
            .max = 1,
            .center = 0,
            .init = 1,  // ✅ ON by default!
            .type = k_unit_param_type_onoff,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"PLAY"}
        },
        
        // PARAM 1: STEP SELECT (Knob A in normal mode)
        {
            .min = 0,
            .max = 15,
            .center = 0,
            .init = 0,
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"STEP"}
        },
        
        // PARAM 2: PITCH OFFSET (Knob B)
        {
            .min = -24,
            .max = 24,
            .center = 0,
            .init = 0,
            .type = k_unit_param_type_semi,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"PITCH"}
        },
        
        // PARAM 3: FILTER MOD
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"FILTER"}
        },
        
        // PARAM 4: GATE LENGTH
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 768,
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"GATE"}
        },
        
        // PARAM 5: SEQUENCE LENGTH
        {
            .min = 0,
            .max = 15,
            .center = 0,
            .init = 15,
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"LENGTH"}
        },
        
        // PARAM 6: SWING
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"SWING"}
        },
        
        // PARAM 7: RATCHET
        {
            .min = 0,
            .max = 3,
            .center = 0,
            .init = 0,
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"RATCHET"}
        },
        
        // PARAM 8: PATTERN SELECT
        {
            .min = 0,
            .max = 7,
            .center = 0,
            .init = 0,
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"PATTERN"}
        },
        
        // PARAM 9: DIRECTION
        {
            .min = 0,
            .max = 3,
            .center = 0,
            .init = 0,
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"DIRECTN"}
        },
        
        // Terminator
        {
            .min = 0,
            .max = 0,
            .center = 0,
            .init = 0,
            .type = k_unit_param_type_none,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {""}
        }
    },
};

