/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    SHIVIKUTFREQ - Frequency Shifting Dub Delay
    
    Psychedelic frequency-shifting delay for dub techno/house
    - Linear frequency shift in Hz (not semitones!)
    - Spiraling, glidinging echoes
    - Perfect for dub chords, hi-hats, synth lines
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/

#include "unit_delfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_delfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x14U,
    .version = 0x00010000U,
    .name = "SHIVIKUT",
    .num_params = 10,
    .params = {
        // Param 0: TIME (A knob) - Delay time
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"TIME"}
        },
        // Param 1: FEEDBACK (B knob) - Feedback amount
        {
            .min = 0, .max = 1023, .center = 0, .init = 614,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"FEEDBCK"}
        },
        // Param 2: MIX (SHIFT+B) - Dry/wet balance
        {
            .min = -100, .max = 100, .center = 0, .init = 0,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"MIX"}
        },
        // Param 3: SHIFT HZ - Frequency shift amount
        {
            .min = 0, .max = 1023, .center = 0, .init = 307,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"SHIFT"}
        },
        // Param 4: DIRECTION - Up/Down/Off
        {
            .min = 0, .max = 2, .center = 0, .init = 1,
            .type = k_unit_param_type_enum, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DIRECT"}
        },
        // Param 5: TONE - Filter color
        {
            .min = 0, .max = 1023, .center = 0, .init = 409,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"TONE"}
        },
        // Param 6: STEREO - Stereo width
        {
            .min = 0, .max = 1023, .center = 0, .init = 768,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"STEREO"}
        },
        // Param 7: WANDER - Modulation/movement
        {
            .min = 0, .max = 1023, .center = 0, .init = 256,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"WANDER"}
        },
        // Param 8: SYNC - Tempo sync divisions
        {
            .min = 0, .max = 8, .center = 0, .init = 3,
            .type = k_unit_param_type_enum, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"SYNC"}
        },
        // Param 9: LOFI - Lo-fi/dirt amount
        {
            .min = 0, .max = 1023, .center = 0, .init = 205,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"LOFI"}
        },
        // Unused param slot
        {
            .min = 0, .max = 0, .center = 0, .init = 0,
            .type = k_unit_param_type_none, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {""}
        }
    }
};
