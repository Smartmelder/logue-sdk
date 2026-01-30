/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    ETERNAL FLANGER - Barber-Pole Flanger Effect
    
    Endless, continuously sweeping flanger:
    - Barber-pole illusion (UP/DOWN/BOTH)
    - Smooth, hypnotic movement
    - Chorus → Classic → Extreme modes
    
    Perfect for techno/house pads, intros, sound design
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/

#include "unit_modfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_modfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x12U,
    .version = 0x00010000U,
    .name = "ETERNALK",
    .num_params = 10,
    .params = {
        {
            .min = 0, .max = 2, .center = 0, .init = 2,
            .type = k_unit_param_type_enum, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DIRECT"}
        },
        {
            .min = 0, .max = 1023, .center = 0, .init = 307,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"RATE"}
        },
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DEPTH"}
        },
        {
            .min = 0, .max = 1023, .center = 0, .init = 409,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"FEEDBCK"}
        },
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"MIX"}
        },
        {
            .min = 0, .max = 1023, .center = 0, .init = 614,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"STEREO"}
        },
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"TONE"}
        },
        {
            .min = 0, .max = 1023, .center = 0, .init = 716,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"SMOOTH"}
        },
        {
            .min = 2, .max = 4, .center = 3, .init = 4,
            .type = k_unit_param_type_enum, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"STAGES"}
        },
        {
            .min = 0, .max = 1023, .center = 0, .init = 307,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"RESONAT"}
        },
        {
            .min = 0, .max = 0, .center = 0, .init = 0,
            .type = k_unit_param_type_none, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {""}
        }
    }
};
