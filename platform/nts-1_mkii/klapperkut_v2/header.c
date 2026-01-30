/*
    KLAPPERKUT V2 - Multi-FX Modulation Unit
    MAX 10 PARAMETERS (ModFX limit)
*/

#include "unit_modfx.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_modfx,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0xBU,  // 0x0B = 11
    .version = 0x00020000U,  // V2!
    .name = "KLAPPERKUT",
    .num_params = 10,  // ✅ CORRECT: Max 10 voor modfx!
    .params = {
        // Param 0: Mode (Knop A)
        {
            .min = 0,
            .max = 7,
            .center = 0,
            .init = 0,
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"MODE"}
        },
        
        // Param 1: Gain (Knop B)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 1023,
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"GAIN"}
        },
        
        // Param 2: Depth
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 410,
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"DEPTH"}
        },
        
        // Param 3: Feedback
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 307,
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"FEEDBK"}
        },
        
        // Param 4: Mix
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"MIX"}
        },
        
        // Param 5: Tempo Sync
        {
            .min = 0,
            .max = 5,
            .center = 0,
            .init = 0,
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"SYNC"}
        },
        
        // Param 6: LFO Shape
        {
            .min = 0,
            .max = 3,
            .center = 0,
            .init = 0,
            .type = k_unit_param_type_enum,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"SHAPE"}
        },
        
        // Param 7: Stereo Width
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"STEREO"}
        },
        
        // Param 8: Color/Tone
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 512,
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"COLOR"}
        },
        
        // ✅ Param 9: DUCK (was Param 10, nu MORPH verwijderd!)
        {
            .min = 0,
            .max = 1023,
            .center = 0,
            .init = 0,
            .type = k_unit_param_type_percent,
            .frac = 0,
            .frac_mode = 0,
            .reserved = 0,
            .name = {"DUCK"}
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

