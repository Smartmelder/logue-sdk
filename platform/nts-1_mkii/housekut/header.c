/*
   
    Copyright (c) 2023, KORG INC.
    
    HOUSEKUT - Nederhouse/Euro-House Melodic Lead Oscillator
    
    Melodische, heldere 90s/00s dance leads
    - House Bells
    - Trance Leads
    - Nederhouse Piano
    - Classic Club Leads
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x15U,
    .version = 0x00010000U,
    .name = "HOUSEKUT",
    .num_params = 10,
    .params = {
        // Param 0: CHARACTER (A knob) - Sound type
        {
            .min = 0, .max = 3, .center = 0, .init = 1,
            .type = k_unit_param_type_enum, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"CHAR"}
        },
        // Param 1: DETUNE (B knob) - Unison width
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DETUNE"}
        },
        // Param 2: BRIGHTNESS - Tone color
        {
            .min = 0, .max = 1023, .center = 0, .init = 614,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"BRIGHT"}
        },
        // Param 3: MOTION - Shape LFO
        {
            .min = 0, .max = 1023, .center = 0, .init = 307,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"MOTION"}
        },
        // Param 4: ATTACK - Attack style
        {
            .min = 0, .max = 1023, .center = 0, .init = 256,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"ATTACK"}
        },
        // Param 5: GLIDE - Portamento
        {
            .min = 0, .max = 1023, .center = 0, .init = 205,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"GLIDE"}
        },
        // Param 6: VIBRATO - Pitch vibrato
        {
            .min = 0, .max = 1023, .center = 0, .init = 307,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"VIBRATO"}
        },
        // Param 7: WARMTH - Body/warmth
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"WARMTH"}
        },
        // Param 8: FLAVOR - 90s to modern
        {
            .min = 0, .max = 1023, .center = 0, .init = 341,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"FLAVOR"}
        },
        // Param 9: SUSTAIN - Lead vs pluck
        {
            .min = 0, .max = 1023, .center = 0, .init = 768,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"SUSTAIN"}
        },
        // Unused param slot
        {
            .min = 0, .max = 0, .center = 0, .init = 0,
            .type = k_unit_param_type_none, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {""}
        }
    }
};

