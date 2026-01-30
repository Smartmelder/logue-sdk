/*
    BSD 3-Clause License
    Copyright (c) 2023, KORG INC.
    
    ARP BEAST - Ultimate Arpeggiator Controller
    
    BLOW YOUR MIND WITH UNLIMITED ARP VARIATIONS!
    
    Transform the boring NTS-1 mkII arpeggiator into a powerhouse!
    
    FEATURES:
    - 16 arpeggio patterns (up, down, random, pendulum, etc.)
    - Octave range control (1-4 octaves)
    - Swing/shuffle (25-75%)
    - Per-step gate length (staccato to legato)
    - Per-step accent patterns
    - Note probability (euclidean rhythms)
    - Harmony modes (add 3rds, 5ths, octaves)
    - Tempo multiplier (1/4× to 4×)
    - Pattern randomization (generative)
    - Wet/dry mix
    
    PATTERNS:
    0: UP - Classic ascending
    1: DOWN - Classic descending
    2: UP-DOWN - Pendulum
    3: DOWN-UP - Inverted pendulum
    4: RANDOM - Chaotic
    5: DRUNK - Random walk
    6: OCTAVES - Jump octaves
    7: SPIRAL - Expanding spiral
    8: BOUNCE - Ping-pong with repeats
    9: STUTTER - Repeated notes
    10: SKIP - Skip notes
    11: DOUBLE - Play each note twice
    12: THIRDS - Interval jumps
    13: FIFTHS - Power chord style
    14: BROKEN - Broken chord style
    15: EUCLIDEAN - Euclidean rhythm
    
    HARMONY MODES:
    0: NONE - Original notes
    1: 3RD - Add major third
    2: 5TH - Add perfect fifth
    3: OCT - Add octave
    4: TRIAD - Add 3rd + 5th
    5: 7TH - Add major 7th
    6: POWER - Add 5th + octave
    7: CLUSTER - Add 2nd + 4th
    
    This is the ARP controller you've been waiting for!
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/

#include "unit_modfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_modfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0xAU,
    .version = 0x00010000U,
    .name = "ARPBEAST",
    .num_params = 10,
    .params = {
        // Param 0: Pattern (Knop A)
        {
            .min = 0,
            .max = 15,
            .center = 0,
            .init = 0,  // UP default
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"PATTERN"}
        },
        
        // Param 1: Octave Range (Knop B)
        {
            .min = 1,
            .max = 4,
            .center = 2,  // ✅ FIX: Center must be between min and max
            .init = 2,  // 2 octaves default
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"OCTAVES"}
        },
        
        // Param 2: Swing Amount
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,  // 50% = no swing
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"SWING"}
        },
        
        // Param 3: Gate Length
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 768,  // 75% default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"GATE"}
        },
        
        // Param 4: Accent Amount
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 307,  // 30% default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"ACCENT"}
        },
        
        // Param 5: Note Probability
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 1023,  // 100% = all notes
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"PROBAB"}
        },
        
        // Param 6: Harmony Mode
        {
            .min = 0,
            .max = 7,
            .center = 0,
            .init = 0,  // None default
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"HARMONY"}
        },
        
        // Param 7: Tempo Multiplier
        {
            .min = 0,
            .max = 7,
            .center = 0,
            .init = 3,  // 1× default
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"TEMPO"}
        },
        
        // Param 8: Randomization
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 0,  // No random default
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"RANDOM"}
        },
        
        // Param 9: Wet/Dry Mix
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 1023,  // 100% wet (full ARP effect)
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"MIX"}
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

