/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    HYPERPOLY ULTIMATE - Maximum Edition
    
    ULTIMATE CHORD OSCILLATOR WITH SEQUENCER!
    
    FEATURES:
    - 12 chord types (major, minor, 7th, sus, etc.)
    - 4 independent voices with detune
    - Voice count control (1-4 voices)
    - Phase offset modulation
    - PWM (pulse width modulation)
    - Built-in low-pass filter
    - 16-step sequencer (OFF/PLAY/RECORD)
    - Sub oscillator
    - Brightness control
    - 10 PARAMETERS - MAXIMUM CONTROL!
    
    Perfect for: House, techno, ambient, progressive, chords, leads
    
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
    .version = 0x00020000U,  // Version 2.0!
    .name = "HYPERMAX",
    .num_params = 10,  // ✅ MAXIMUM!
    .params = {
        // Param 0: Chord Type (Knop A)
        {
            .min = 0,
            .max = 11,
            .center = 0,
            .init = 3,  // MAJOR default
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"CHORD"}
        },
        
        // Param 1: Detune (Knop B)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,  // 50% default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"DETUNE"}
        },
        
        // Param 2: Sub Mix
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,  // 50% default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"SUBMIX"}
        },
        
        // Param 3: Brightness
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 768,  // 75% default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"BRIGHT"}
        },
        
        // ✅ NEW: Param 4: Voice Count
        {
            .min = 1,
            .max = 4,
            .center = 2,  // ✅ FIX: Center must be between min and max
            .init = 4,  // All voices default
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"VOICES"}
        },
        
        // ✅ NEW: Param 5: Phase Offset
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 0,  // No offset default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"PHASE"}
        },
        
        // ✅ NEW: Param 6: PWM Depth
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 205,  // 20% default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"PWM"}
        },
        
        // ✅ NEW: Param 7: Filter Cutoff
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 1023,  // Fully open default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"FILTER"}
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
        
        // ✅ NEW: Param 9: Sequencer Step
        {
            .min = 0,
            .max = 15,
            .center = 0,
            .init = 0,  // Step 1
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
