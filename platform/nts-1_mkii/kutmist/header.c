/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    KUTMIST - Warm Hazy Reverb
    
    CINEMATIC AMBIENT REVERB!
    
    FEATURES:
    - 16 early reflection taps (rich, complex)
    - Soft diffusion network (4 allpass)
    - 4 comb filters (smooth tail)
    - SIDE mode (stereo-aware processing)
    - High/Low cut filters (frequency shaping)
    - Pre-delay (0-500ms)
    - Room size control (small to cathedral)
    - Bass boost/cut (warm low-end control)
    - High damping (natural decay)
    - Optimized for SDRAM (efficient)
    
    ALGORITHM:
    - Based on Schroeder reverb (1962)
    - Freeverb-inspired diffusion
    - Custom early reflections
    - Soft, non-metallic character
    
    Perfect for: Ambient pads, vocal processing,
                 cinematic textures, film scoring,
                 mix bus reverb with SIDE mode
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/

#include "unit_revfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_revfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0xDU,
    .version = 0x00010000U,
    .name = "KUTMIST",
    .num_params = 10,
    .params = {
        // Param 0: Pre-Delay (TIME on hardware)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 205,  // 20%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"PREDLY"}
        },
        
        // Param 1: Size (DEPTH on hardware)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 614,  // 60%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"SIZE"}
        },
        
        // Param 2: Diffusion (MIX = Shift+B, special for revfx!)
        {
            .min = -100,
            .max = 100,
            .center = 0,
            .init = 50,  // 50%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"DIFF"}
        },
        
        // Param 3: Decay
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 614,  // 60%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"DECAY"}
        },
        
        // Param 4: High Damping
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 410,  // 40%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"DAMP"}
        },
        
        // Param 5: Low Cut
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 102,  // 10%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"LOWCUT"}
        },
        
        // Param 6: High Cut
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 819,  // 80%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"HICUT"}
        },
        
        // Param 7: Bass Boost/Cut
        {
            .min = -100,
            .max = 100,
            .center = 0,
            .init = 20,  // +20%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"BASS"}
        },
        
        // Param 8: SIDE Mode
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 0,  // Off
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"SIDE"}
        },
        
        // Param 9: Early Reflection Level
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 410,  // 40%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"EARLY"}
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

