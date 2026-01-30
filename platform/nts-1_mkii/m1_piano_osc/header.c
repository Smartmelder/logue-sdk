/*
    BSD 3-Clause License
    Copyright (c) 2024, KORG INC.

    M1 PIANO - Sample-Based Oscillator
    Korg M1-style piano with attack/loop samples
*/

#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,
    .unit_id = 0x50U,
    .version = 0x00010000U,
    .name = "M1 PIANO",
    .num_params = 10,
    .params = {
        // Knob A: ATTACK TIME
        {
            .min = 0, .max = 1023, .center = 0, .init = 10,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"ATTACK"}
        },

        // Knob B: SUSTAIN LEVEL
        {
            .min = 0, .max = 1023, .center = 0, .init = 716,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"SUSTAIN"}
        },

        // Param 1: DECAY TIME
        {
            .min = 0, .max = 1023, .center = 0, .init = 410,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DECAY"}
        },

        // Param 2: RELEASE TIME
        {
            .min = 0, .max = 1023, .center = 0, .init = 256,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"RELEASE"}
        },

        // Param 3: BRIGHTNESS (Filter Cutoff)
        {
            .min = 0, .max = 1023, .center = 0, .init = 819,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"BRIGHT"}
        },

        // Param 4: RESONANCE (Filter Q)
        {
            .min = 0, .max = 1023, .center = 0, .init = 205,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"RESON"}
        },

        // Param 5: VIBRATO DEPTH
        {
            .min = 0, .max = 1023, .center = 0, .init = 0,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"VIBRATO"}
        },

        // Param 6: VIBRATO SPEED
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"VSPEED"}
        },

        // Param 7: STEREO WIDTH
        {
            .min = 0, .max = 1023, .center = 0, .init = 512,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"WIDTH"}
        },

        // Param 8: DETUNE (Fine tuning)
        {
            .min = 0, .max = 1023, .center = 512, .init = 512,
            .frac = 0, .frac_mode = 0, .reserved = 0,
            .name = {"DETUNE"}
        },
    }
};
