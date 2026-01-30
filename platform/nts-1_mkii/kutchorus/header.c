/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    KUTCHORUS - Ultimate Multi-Mode Chorus
    
    ADVANCED CHORUS FOR TECHNO & HOUSE!
    
    Inspired by:
    - Roland Juno Chorus
    - BBD (Bucket Brigade) analog chorus
    - Modern digital chorus units
    
    FEATURES:
    - 4 chorus types (Soft, Classic, Wide, Dirty)
    - 4 independent delay lines with LFO
    - Stereo width control (mono to ultra-wide)
    - Bass protection (keep low-end tight)
    - Motion/randomization (analog character)
    - Tone control (dark to bright)
    - Variable voice count (2-4 voices)
    - Feedback control (subtle to flanging)
    
    CHORUS TYPES:
    0-24%:   SOFT - Subtle widening (deep house pads)
    25-49%:  CLASSIC - Juno/'80s style (synth stabs)
    50-74%:  WIDE - Big chorus (wide leads)
    75-100%: DIRTY - Aggressive techno textures
    
    Perfect for: Deep house, techno, wide pads, thick bass,
                 moving sequences, experimental FX
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/

#include "unit_modfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_modfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0xCU,
    .version = 0x00010000U,
    .name = "KUTCHORUS",
    .num_params = 10,
    .params = {
        // Param 0: Type (Knop A when MOD held)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 256,  // Classic (25%)
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"TYPE"}
        },
        
        // Param 1: Rate (Knop B)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 307,  // 30% (moderate)
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"RATE"}
        },
        
        // Param 2: Depth
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,  // 50%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"DEPTH"}
        },
        
        // Param 3: Mix (Shift + Knop B)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,  // 50%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"MIX"}
        },
        
        // Param 4: Width
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,  // 50%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"WIDTH"}
        },
        
        // Param 5: Tone
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,  // 50% neutral
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"TONE"}
        },
        
        // Param 6: Motion/Random
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 205,  // 20%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"MOTION"}
        },
        
        // Param 7: Bass Cut (Pump-Safe)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 307,  // 30%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"BASSCUT"}
        },
        
        // Param 8: Voice Count
        {
            .min = 2,
            .max = 4,
            .center = 3,  // ✅ FIX: Center must be between min and max
            .init = 3,  // 3 voices
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"VOICES"}
        },
        
        // Param 9: Feedback
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 102,  // 10%
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"FEEDBK"}
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

