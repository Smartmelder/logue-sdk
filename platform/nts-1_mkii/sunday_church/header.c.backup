/*
    SUNDAY CHURCH - Cathedral Reverb for NTS-1 mkII
    
    Premium cathedral reverb met 10 parameters
    - Dattorro topology voor lush sound
    - Cubic interpolation (anti-distortion)
    - SDRAM allocatie (stabiel)
    - Infinite reverb support
*/

#include "unit_revfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_revfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x12U,
    .version = 0x00010000U,
    .name = "SNDY CHRCH",
    .num_params = 10,
    .params = {
        // Knob A: Reverb Time
        {
            .min = 0, .max = 1023, .center = 0, .init = 614,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"TIME"}
        },
        
        // Knob B: Modulation Depth
        {
            .min = 0, .max = 1023, .center = 0, .init = 307,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DEPTH"}
        },
        
        // Param 0: Dry/Wet Mix
        {
            .min = -100, .max = 100, .center = 0, .init = 50,
            .type = k_unit_param_type_drywet,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"MIX"}
        },
        
        // Param 1: Room Size
        {
            .min = 0, .max = 1023, .center = 0, .init = 717,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"SIZE"}
        },
        
        // Param 2: Damping (HF decay) - INVERTED!
        // 0 = minimale damping (max galm)
        // 998 = maximale damping (min galm) 
        // 1023 = default (max damping bij opstarten)
        {
            .min = 0, .max = 1023, .center = 0, .init = 1023,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DAMP"}
        },
        
        // Param 3: Diffusion
        {
            .min = 0, .max = 1023, .center = 0, .init = 614,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DIFFUSE"}
        },
        
        // Param 4: Pre-Delay
        {
            .min = 0, .max = 1023, .center = 0, .init = 205,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"PREDLY"}
        },
        
        // Param 5: Early Reflections
        {
            .min = 0, .max = 1023, .center = 0, .init = 307,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"EARLY"}
        },
        
        // Param 6: Modulation Rate
        {
            .min = 0, .max = 1023, .center = 0, .init = 154,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"MODRATE"}
        },
        
        // Param 7: Stereo Width
        {
            .min = 0, .max = 1023, .center = 0, .init = 717,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"WIDTH"}
        },
        
        // Terminator
        {
            .min = 0, .max = 0, .center = 0, .init = 0,
            .type = k_unit_param_type_none,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {""}
        }
    }
};

