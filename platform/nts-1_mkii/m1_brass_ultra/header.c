/*
    M1 BRASS ULTRA - Definitive Working Version
    
    Classic M1 Brass/Strings synthesizer with:
    - Dual oscillators (Saw + Pulse)
    - 4-band formant filtering
    - Multi-voice ensemble
    - Velocity layers
    - Vibrato with fade-in
    - 12 authentic patches
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x14U,
    .version = 0x00010000U,
    .name = "M1BRASS",
    .num_params = 10,
    .params = {
        // Knob A: Brightness
        {
            .min = 0, .max = 1023, .center = 0, .init = 614,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"BRIGHT"}
        },
        
        // Knob B: Resonance
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"RESONANCE"}
        },
        
        // Param 0: Detune
        {
            .min = 0, .max = 1023, .center = 0, .init = 307,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DETUNE"}
        },
        
        // Param 1: Ensemble
        {
            .min = 0, .max = 1023, .center = 0, .init = 409,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"ENSEMBLE"}
        },
        
        // Param 2: Vibrato
        {
            .min = 0, .max = 1023, .center = 0, .init = 409,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"VIBRATO"}
        },
        
        // Param 3: Attack
        {
            .min = 0, .max = 1023, .center = 0, .init = 307,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"ATTACK"}
        },
        
        // Param 4: Release
        {
            .min = 0, .max = 1023, .center = 0, .init = 614,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"RELEASE"}
        },
        
        // Param 5: Voices
        {
            .min = 1, .max = 8, .center = 4, .init = 4,
            .type = k_unit_param_type_enum,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"VOICES"}
        },
        
        // Param 6: Patch
        {
            .min = 0, .max = 11, .center = 5, .init = 0,
            .type = k_unit_param_type_enum,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"PATCH"}
        },
        
        // Param 7: Width
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
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
    },
};
