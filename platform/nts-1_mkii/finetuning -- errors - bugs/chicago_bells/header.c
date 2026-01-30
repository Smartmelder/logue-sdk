/*
    CHICAGO BELLS V3
    
    4 Authentic Bell Types:
    0 = COWBELL (808-style)
    1 = CHURCH (Cathedral bells)
    2 = AGOGO (Latin percussion)
    3 = GONG (Industrial)
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x13U,
    .version = 0x00030000U,
    .name = "CHI-BELLS",
    .num_params = 9,
    .params = {
        // Knob A: TONE (Brightness/FM)
        {
            .min = 0, .max = 1023, .center = 512, .init = 512,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"TONE"}
        },
        
        // Knob B: DECAY
        {
            .min = 0, .max = 1023, .center = 512, .init = 512,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DECAY"}
        },
        
        // OSC Param 0: TYPE
        {
            .min = 0, .max = 3, .center = 0, .init = 0,
            .type = k_unit_param_type_strings,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"TYPE"}
        },
        
        // OSC Param 1: STRIKE
        {
            .min = 0, .max = 1023, .center = 0, .init = 409,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"STRIKE"}
        },
        
        // OSC Param 2: DETUNE
        {
            .min = 0, .max = 1023, .center = 0, .init = 256,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DETUNE"}
        },
        
        // OSC Param 3: BITE (Attack)
        {
            .min = 0, .max = 1023, .center = 0, .init = 307,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"BITE"}
        },
        
        // OSC Param 4: RING (Modulation)
        {
            .min = 0, .max = 1023, .center = 0, .init = 307,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"RING"}
        },
        
        // OSC Param 5: DIRT (Saturation)
        {
            .min = 0, .max = 1023, .center = 0, .init = 0,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DIRT"}
        },
        
        // OSC Param 6: AIR (High freq content)
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"AIR"}
        },
        
        // Terminator
        {
            .min = 0, .max = 0, .center = 0, .init = 0,
            .type = k_unit_param_type_none,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {""}
        }
    },
};
