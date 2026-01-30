/*
     Copyright (c) 2023, KORG INC.
    
    DIGITONE FM - 4-Operator FM Synthesizer
    
    Authentic FM synthesis inspired by Elektron Digitone II
    
    FEATURES:
    - 4-operator FM synthesis
    - 8 classic FM algorithms
    - Per-operator envelopes
    - Operator 1 feedback
    - Frequency ratios (0.5× to 16×)
    - LFO modulation
    - Resonant filter
    - Analog-style overdrive
    
    ALGORITHMS:
    0: 1→2→3→4 (Serial cascade - brass/bell)
    1: 1→2→3, 1→4 (Parallel carriers - pad)
    2: 1→2, 3→4 (Dual stacks - organ)
    3: 1→2→3, 4 (Mixed - electric piano)
    4: 1→2, 1→3, 1→4 (One modulator - bass)
    5: 1→2, 1→3, 4 (Asymmetric - lead)
    6: 1, 2, 3, 4 (All parallel - additive)
    7: 1→4, 2→4, 3→4 (Triple mod - complex)
    
    Perfect for: Classic FM sounds, DX7-style patches, digital bells, 
                 electric pianos, basses, pads, brass
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x3U,
    .version = 0x00020000U,  // V2 with sequencer!
    .name = "DIGITONE",
    .num_params = 10,  // Was 8, now 10!
    .params = {
        // Param 0: Algorithm (Knop A)
        {
            .min = 0,
            .max = 7,
            .center = 0,
            .init = 0,  // Serial cascade default
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"ALGO"}
        },
        
        // Param 1: FM Amount (Knop B)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 307,  // ✅ 30% default (safer for clean sound)
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"FM"}
        },
        
        // Param 2: Frequency Ratio
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 341,  // ~33% = ratio 2.0
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"RATIO"}
        },
        
        // Param 3: Operator 1 Feedback
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 205,  // 20% default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"FEEDBK"}
        },
        
        // Param 4: Envelope Attack
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 51,  // 5% = fast
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"ATTACK"}
        },
        
        // Param 5: Envelope Decay
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,  // 50% default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"DECAY"}
        },
        
        // Param 6: Filter Cutoff
        {
            .min = 0,
            .max = 700,  // ✅ FIX: Max 700 (was 1023) - prevents mute above 700
            .center = 0,
            .init = 500,  // ✅ FIX: 500 default (was 819) - safe value with sound
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"FILTER"}
        },
        
        // Param 7: Filter Resonance
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 205,  // 20% default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"RESON"}
        },
        
        // ✅ NEW: Param 8: PLAY/STOP (ON/OFF button!)
        {
            .min = 0,
            .max = 1,
            .center = 0,
            .init = 0,  // OFF default
            .type = k_unit_param_type_onoff,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"PLAY"}
        },
        
        // ✅ NEW: Param 9: Sequencer Step Edit
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

