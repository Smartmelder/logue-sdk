/*
       Copyright (c) 2023, KORG INC.
    
    S-1 TWEAK - Roland AIRA S-1 / SH-101 Inspired Synthesizer
    
    ULTIMATE TECHNO/HOUSE OSCILLATOR!
    
    Inspired by:
    - Roland SH-101 (classic mono synth character)
    - Roland AIRA Compact S-1 Tweak Synth
    
    FEATURES:
    - SH-101 style VA core (saw, pulse, sub, noise)
    - S-1 OSC Draw (waveform morphing)
    - S-1 OSC Chop/Comb (harmonic slicing)
    - 4 modes: Mono / Para-Poly / Unison / Chord
    - Built-in ratcheting (sub-steps like S-1)
    - Motion recording (evolving patterns)
    - Probability/humanization
    - House-friendly chord progressions
    - Works seamlessly with NTS-1 ARP/SEQ
    
    MODES:
    0-24%:   MONO (SH-101 style legato)
    25-49%:  POLY (4 voices paraphonic)
    50-74%:  UNISON (stacked detuned)
    75-100%: CHORD (house progressions)
    
    Perfect for: Techno bass, house chords, ratcheted leads,
                 evolving pads, metallic FX, ambient soundscapes
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0xCU,
    .version = 0x00010000U,
    .name = "S1TWEAK",
    .num_params = 10,
    .params = {
        // Param 0: Wave Mix (Knop A)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,  // 50% balanced
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"WAVEMIX"}
        },
        
        // Param 1: Draw Shape (Knop B)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 0,  // Pure waveforms
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"DRAW"}
        },
        
        // Param 2: Chop/Comb
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 0,  // Off
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"CHOP"}
        },
        
        // Param 3: Mode (Mono/Poly/Unison/Chord)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 0,  // Mono
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"MODE"}
        },
        
        // Param 4: Chord Type
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 0,  // Major
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"CHORD"}
        },
        
        // Param 5: Detune/Spread
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 307,  // 30%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"DETUNE"}
        },
        
        // Param 6: Noise/Riser
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 102,  // 10%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"NOISE"}
        },
        
        // Param 7: Rattle/Sub-Step
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 0,  // Off
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"RATTLE"}
        },
        
        // Param 8: Probability
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 0,  // Deterministic
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"PROBAB"}
        },
        
        // Param 9: Motion
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 0,  // Static
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"MOTION"}
        },
        
        // Terminator
        {
            .min = 0,
            .max = 0,
            .center = 0,
            .init = 0,
            .type = k_unit_param_type_none,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {""}
        }
    },
};

