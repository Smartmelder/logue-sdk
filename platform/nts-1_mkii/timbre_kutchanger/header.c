/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    TIMBRE-KUTCHANGER - Ultimate Timbre Morphing ModFX
    
    Transform any oscillator into different characters:
    - Electric (synth leads)
    - Metallic (techno/psy)
    - Flute (melodic, acoustic)
    - Alt/Mezzo (warm vocal)
    - Soprano (bright, brilliant)
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/

#include "unit_modfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_modfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x16U,
    .version = 0x00010000U,
    .name = "TIMBREKUT",
    .num_params = 10,
    .params = {
        // Param 0: CHARACTER (A knob) - Main timbre selector
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"CHAR"}
        },
        // Param 1: BRIGHTNESS (B knob) - Edge/helderheid
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"BRIGHT"}
        },
        // Param 2: FORMANT - Vocal/brass body
        {
            .min = 0, .max = 1023, .center = 0, .init = 409,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"FORMANT"}
        },
        // Param 3: MOTION - Vibrato/expressie
        {
            .min = 0, .max = 1023, .center = 0, .init = 307,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"MOTION"}
        },
        // Param 4: ENSEMBLE - Stereo width
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"ENSEMBLE"}
        },
        // Param 5: HARMONIC - Harmonic emphasis
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"HARMONIC"}
        },
        // Param 6: ATTACK - Attack response
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"ATTACK"}
        },
        // Param 7: MIX - Dry/wet blend
        {
            .min = 0, .max = 1023, .center = 0, .init = 768,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"MIX"}
        },
        // Param 8: COLOR - Color variation
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"COLOR"}
        },
        // Param 9: DEPTH - Effect intensity
        {
            .min = 0, .max = 1023, .center = 0, .init = 614,
            .type = k_unit_param_type_percent, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DEPTH"}
        },
        // Unused param slot
        {
            .min = 0, .max = 0, .center = 0, .init = 0,
            .type = k_unit_param_type_none, .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {""}
        }
    }
};

