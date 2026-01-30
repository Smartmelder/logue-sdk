/*
    GABBER_Kutje - Rhythm Dance Oscillator
    Turn Up The Bass Edition
    
    Platform: Korg NTS-1 mkII
    SDK: logue-sdk 2.0
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x42U,  // ID 66 (0x42 = gabber!)
    .version = 0x00010000U,
    .name = "GABBRKUTJE",
    .num_params = 6,  // Maximum for user osc
    .params = {
        // Param 0: SHAPE (mapped to knob A)
        {
            .min = 0, .max = 1023, .center = 512, .init = 512,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DIST"}
        },
        
        // Param 1: ALT (mapped to knob B)
        {
            .min = 0, .max = 7, .center = 0, .init = 0,
            .type = k_unit_param_type_enum,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"MODE"}
        },
        
        // Param 2: PENV (Pitch Envelope)
        {
            .min = 0, .max = 1023, .center = 0, .init = 307,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"PENV"}
        },
        
        // Param 3: SUB (Sub Oscillator)
        {
            .min = 0, .max = 1023, .center = 0, .init = 409,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"SUB"}
        },
        
        // Param 4: PUMP (Sidechain simulation)
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"PUMP"}
        },
        
        // Param 5: RAVE (Chord/Interval)
        {
            .min = 0, .max = 4, .center = 0, .init = 0,
            .type = k_unit_param_type_enum,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"RAVE"}
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

