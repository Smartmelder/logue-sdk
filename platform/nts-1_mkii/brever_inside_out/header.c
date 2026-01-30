/*
    BREVER INSIDE OUT - Quantum Reverb
    
    Features:
    - Granular diffusion engine
    - Spectral freeze mode
    - Modulated reverb character
    - Shimmer pitch shifting
    - Dynamic ducking/swelling
    - Multi-tap rhythmic delays
    
    Platform: Korg NTS-1 mkII
    SDK: logue-sdk 2.0
*/

#include "unit_revfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_revfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x8U,
    .version = 0x00010000U,
    .name = "BREVERIO",
    .num_params = 10,
    .params = {
        // Param 0: TIME (Knob A)
        {
            .min = 0, .max = 1023, .center = 0, .init = 614,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"TIME"}
        },
        
        // Param 1: DEPTH (Knob B)
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DEPTH"}
        },
        
        // Param 2: MIX (Shift+B)
        {
            .min = -100, .max = 100, .center = 0, .init = 50,
            .type = k_unit_param_type_drywet,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"MIX"}
        },
        
        // Param 3: SHIMMER (Pitch shift amount)
        {
            .min = 0, .max = 1023, .center = 0, .init = 0,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"SHIMMER"}
        },
        
        // Param 4: GRAIN (Granular density)
        {
            .min = 0, .max = 1023, .center = 0, .init = 307,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"GRAIN"}
        },
        
        // Param 5: MOTION (LFO modulation)
        {
            .min = 0, .max = 1023, .center = 0, .init = 256,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"MOTION"}
        },
        
        // Param 6: SPACE (Diffusion)
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"SPACE"}
        },
        
        // Param 7: DUCK (Dynamic control)
        {
            .min = 0, .max = 1023, .center = 0, .init = 0,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DUCK"}
        },
        
        // Param 8: MODE (Reverb character)
        {
            .min = 0, .max = 5, .center = 0, .init = 0,
            .type = k_unit_param_type_enum,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"MODE"}
        },
        
        // Param 9: FREEZE (Infinite hold)
        {
            .min = 0, .max = 1, .center = 0, .init = 0,
            .type = k_unit_param_type_onoff,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"FREEZE"}
        }
    },
};

