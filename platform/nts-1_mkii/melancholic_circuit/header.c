/*
    MELANCHOLIC CIRCUIT - Melancholic Bell Synthesizer
    
    6-voice additive synthesis with inharmonic bell ratios
    Smooth envelopes, exponential detune, vintage character
    
    FIXED: Float output, proper attack, exponential detune, parameter readback
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x11U,
    .version = 0x00040000U,
    .name = "BELLSIMPL",
    .num_params = 10,
    .params = {
        // Knob A: Harmonic Content
        {
            .min = 0, .max = 1023, .center = 512, .init = 512,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"BRIGHT"}
        },
        
        // Knob B: Decay Time
        {
            .min = 0, .max = 1023, .center = 512, .init = 512,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DECAY"}
        },
        
        // OSC 0: Strike Hardness
        {
            .min = 0, .max = 1023, .center = 0, .init = 307,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"STRIKE"}
        },
        
        // OSC 1: Detune Amount
        {
            .min = 0, .max = 1023, .center = 0, .init = 205,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DETUNE"}
        },
        
        // OSC 2: Attack Time
        {
            .min = 0, .max = 1023, .center = 0, .init = 51,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"ATTACK"}
        },
        
        // OSC 3: Release Time
        {
            .min = 0, .max = 1023, .center = 0, .init = 409,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"RELEASE"}
        },
        
        // OSC 4: Chorus Mix
        {
            .min = 0, .max = 1023, .center = 0, .init = 256,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"CHORUS"}
        },
        
        // OSC 5: Tone Color
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .type = k_unit_param_type_percent,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"TONE"}
        },
        
        // OSC 6: Voice Count
        {
            .min = 1, .max = 6, .center = 3, .init = 4,
            .type = k_unit_param_type_strings,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"VOICES"}
        },
        
        // OSC 7: Bell Type
        {
            .min = 0, .max = 3, .center = 0, .init = 0,
            .type = k_unit_param_type_strings,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"TYPE"}
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
