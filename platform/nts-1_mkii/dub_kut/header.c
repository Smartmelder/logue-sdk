/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    DUB BEAST - Ultimate Rhythmic Delay
    
    THE ULTIMATE TECHNO/HOUSE/DUB DELAY!
    
    FEATURES:
    - 6 delay modes (Groove, Dub, Burst, Reverse, Shimmer, Ping-Pong)
    - 16 tempo divisions (1/4 to 1/64, dotted, triplets)
    - Saturated feedback with color filter
    - Bit crusher for lo-fi repeats
    - Stereo spread with cross-feedback
    - Ducking (sidechain compression)
    - Pitch shift in feedback (shimmer!)
    - Modulation (chorus-like movement)
    - Freeze mode (infinite hold)
    - Auto-pan delays
    - 11 parameters for TOTAL CONTROL!
    
    MODES:
    0: GROOVE - Short, tight, ducked (perfect for drums)
    1: DUB - Long, dark, saturated (classic dub techno)
    2: BURST - Very short, high feedback (percussive resonance)
    3: REVERSE - Backwards delay tails (psychedelic!)
    4: SHIMMER - Pitch-shifted feedback (ambient shimmer)
    5: PINGPONG - L/R bounce (stereo movement)
    
    TEMPO DIVISIONS:
    0: 1/4 straight
    1: 1/4 dotted
    2: 1/4 triplet
    3: 1/8 straight
    4: 1/8 dotted
    5: 1/8 triplet
    6: 1/16 straight
    7: 1/16 dotted
    8: 1/16 triplet
    9: 1/32 straight
    10: 1/32 dotted
    11: 1/32 triplet
    12: 1/64 straight
    13: 3/16 (polyrhythmic)
    14: 5/16 (polyrhythmic)
    15: 7/16 (polyrhythmic)
    
    Perfect for: Techno, house, dub techno, ambient, IDM, experimental
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/

#include "unit_delfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_delfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x5U,
    .version = 0x00010000U,
    .name = "DUBBEAST",
    .num_params = 11,
    .params = {
        // Param 0: Mode (Knop A)
        {
            .min = 0,
            .max = 5,
            .center = 0,
            .init = 1,  // DUB default
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"MODE"}
        },
        
        // Param 1: Tempo Division (Knop B)
        {
            .min = 0,
            .max = 15,
            .center = 0,
            .init = 4,  // 1/8 dotted default
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"TIME"}
        },
        
        // Param 2: Feedback
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 614,  // 60%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"FEEDBK"}
        },
        
        // Param 3: Mix (CRITICAL - hardware mapped!)
        {
            .min = -100,
            .max = 100,
            .center = 0,
            .init = 0,  // Balanced
            .type = k_unit_param_type_drywet,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"MIX"}
        },
        
        // Param 4: Color (Tone filter)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 410,  // 40% = darker dub
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"COLOR"}
        },
        
        // Param 5: Grit (Saturation/Drive)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 307,  // 30%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"GRIT"}
        },
        
        // Param 6: Stereo Spread
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,  // 50%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"STEREO"}
        },
        
        // Param 7: Ducking Amount
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 307,  // 30%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"DUCK"}
        },
        
        // Param 8: Modulation Depth
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 205,  // 20%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"MODULA"}
        },
        
        // Param 9: Pitch Shift
        {
            .min = -12,
            .max = 12,
            .center = 0,
            .init = 0,  // No pitch
            .type = k_unit_param_type_semi,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"PITCH"}
        },
        
        // Param 10: Freeze
        {
            .min = 0,
            .max = 1,
            .center = 0,
            .init = 0,  // Off
            .type = k_unit_param_type_onoff,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"FREEZE"}
        }
    },
};

