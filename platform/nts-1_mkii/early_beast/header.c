/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    EARLY BEAST - Ultimate Early Reflections Reverb
    
    INSPIRED BY RELAB LX480 AMBIENCE ALGORITHM!
    
    FEATURES:
    - 16 randomized early reflection taps (30-180ms)
    - Relab LX480-style ambience algorithm
    - Modulation: Spin (rate) & Wander (depth)
    - Low-frequency multiplier (bass width)
    - Size control (room to hall)
    - Density control (tap spacing)
    - Subtle late diffusion (optional tail)
    - Stereo width control (depth without obviousness)
    - Pre-delay (localization)
    - Brightness control (tone shaping)
    
    ALGORITHM:
    - Randomization technology (realistic patterns)
    - Multiple early reflection clusters
    - Dead room → live space transformation
    - Perfect for house/techno production
    
    USE CASES:
    - Dead/dry recordings (isolation booths)
    - Adding thickness without obvious reverb
    - Tying early reflections to long reverbs
    - Acoustic instruments (guitars, vocals)
    - Drums (dimension without wash)
    - House/techno production
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/

#include "unit_revfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_revfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0xEU,
    .version = 0x00010000U,
    .name = "EARLYBST",
    .num_params = 10,
    .params = {
        // Param 0: Pre-Delay (TIME on hardware)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 102,  // 10%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = "PREDLY"
        },
        
        // Param 1: Size (DEPTH on hardware)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,  // 50%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = "SIZE"
        },
        
        // Param 2: Density (MIX = Shift+B)
        {
            .min = -100,
            .max = 100,
            .center = 0,
            .init = 60,  // 60%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = "DENSITY"
        },
        
        // Param 3: Spin (modulation rate)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 307,  // 30%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = "SPIN"
        },
        
        // Param 4: Wander (modulation depth)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 410,  // 40%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = "WANDER"
        },
        
        // Param 5: Low Multiplier
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 614,  // 60%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = "LOWMULT"
        },
        
        // Param 6: Diffusion (late tail)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 205,  // 20%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = "DIFF"
        },
        
        // Param 7: Width
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 768,  // 75%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = "WIDTH"
        },
        
        // Param 8: Brightness
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 614,  // 60%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = "BRIGHT"
        },
        
        // Param 9: Late Mix
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 307,  // 30%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = "LATEMIX"
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
            .name = ""
        }
    },
};

