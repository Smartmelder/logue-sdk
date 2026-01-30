/*
    ADVSEQ - Advanced Step Sequencer Modulation
    
    A powerful 128-step sequencer with pattern manipulation!
    
    FEATURES:
    - 128-step programmable sequence
    - 8 pattern operations (random, shuffle, reverse, slice copy, etc.)
    - Shift left/right with wrap-around
    - Palindrome (SLICECILS) patterns
    - Tempo sync (1/16 or MIDI trigger)
    - Swing/shuffle timing
    - Smooth glide between steps
    - Variable sequence length (1-128 steps)
    - Slice operations (1-32 steps)
    
    Based on Korg logue SDK modfx template
    https://github.com/korginc/logue-sdk
*/

#include "unit_modfx.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "fx_api.h"
#include <algorithm>

// SDK compatibility - PI is already defined in CMSIS arm_math.h
#ifndef PI
#define PI 3.14159265359f
#endif

#define MAX_STEPS 128
#define MAX_SLICE_LEN 32

// Sequencer data
static float s_sequence[MAX_STEPS];      // Step values (0.0-1.0)
static uint8_t s_sequence_length = 16;
static uint8_t s_current_step = 0;
static uint32_t s_step_counter = 0;
static uint32_t s_samples_per_step = 3000;  // ~16th note @ 120 BPM

// Parameters
static uint8_t s_clock_mode = 0;         // 0=1/16, 1=MIDI
static uint8_t s_slice_length = 4;
static float s_smooth_amount = 0.25f;
static float s_mod_depth = 1.0f;
static uint8_t s_operation = 0;          // 0-7
static int8_t s_shift_amount = 0;
static uint8_t s_rate_divider = 0;
static float s_swing_amount = 0.5f;
static float s_mix = 0.75f;

// State
static float s_current_value = 0.5f;     // Current interpolated step value
static float s_target_value = 0.5f;      // Target step value
static uint32_t s_tempo_bpm = 120;
static float s_prev_level = 0.f;        // For MIDI trigger detection

// Random seed
static uint32_t s_random_seed = 12345;

// ========== HELPER FUNCTIONS ==========

// XORShift random generator
inline uint32_t xorshift32() {
    s_random_seed ^= s_random_seed << 13;
    s_random_seed ^= s_random_seed >> 17;
    s_random_seed ^= s_random_seed << 5;
    return s_random_seed;
}

inline float random_float() {
    return (float)(xorshift32() % 10000) / 10000.f;
}

// ========== SEQUENCE OPERATIONS ==========

void shift_sequence_left(uint8_t amount) {
    for (uint8_t i = 0; i < amount; i++) {
        float first = s_sequence[0];
        for (uint8_t j = 0; j < s_sequence_length - 1; j++) {
            s_sequence[j] = s_sequence[j + 1];
        }
        s_sequence[s_sequence_length - 1] = first;
    }
}

void shift_sequence_right(uint8_t amount) {
    for (uint8_t i = 0; i < amount; i++) {
        float last = s_sequence[s_sequence_length - 1];
        for (int8_t j = s_sequence_length - 1; j > 0; j--) {
            s_sequence[j] = s_sequence[j - 1];
        }
        s_sequence[0] = last;
    }
}

void randomize_sequence() {
    for (uint8_t i = 0; i < s_sequence_length; i++) {
        s_sequence[i] = random_float();
    }
}

void shuffle_sequence() {
    for (uint8_t i = s_sequence_length - 1; i > 0; i--) {
        uint8_t j = xorshift32() % (i + 1);
        float temp = s_sequence[i];
        s_sequence[i] = s_sequence[j];
        s_sequence[j] = temp;
    }
}

void reverse_sequence() {
    for (uint8_t i = 0; i < s_sequence_length / 2; i++) {
        float temp = s_sequence[i];
        s_sequence[i] = s_sequence[s_sequence_length - 1 - i];
        s_sequence[s_sequence_length - 1 - i] = temp;
    }
}

// ========== SLICE OPERATIONS ==========

void slice_copy() {
    uint8_t slice_len = clipminmaxi32(1, s_slice_length, s_sequence_length);
    float slice_buffer[MAX_SLICE_LEN];
    
    // Copy eerste slice_len steps
    for (uint8_t i = 0; i < slice_len; i++) {
        slice_buffer[i] = s_sequence[i];
    }
    
    // Herhaal over hele sequence
    for (uint8_t i = 0; i < s_sequence_length; i++) {
        s_sequence[i] = slice_buffer[i % slice_len];
    }
}

void slice_shuffle_copy() {
    uint8_t slice_len = clipminmaxi32(1, s_slice_length, s_sequence_length);
    float slice_buffer[MAX_SLICE_LEN];
    
    // Copy
    for (uint8_t i = 0; i < slice_len; i++) {
        slice_buffer[i] = s_sequence[i];
    }
    
    // Shuffle buffer
    for (uint8_t i = slice_len - 1; i > 0; i--) {
        uint8_t j = xorshift32() % (i + 1);
        float temp = slice_buffer[i];
        slice_buffer[i] = slice_buffer[j];
        slice_buffer[j] = temp;
    }
    
    // Repeat
    for (uint8_t i = 0; i < s_sequence_length; i++) {
        s_sequence[i] = slice_buffer[i % slice_len];
    }
}

void slice_palindrome_copy() {
    uint8_t slice_len = clipminmaxi32(1, s_slice_length, s_sequence_length);
    float slice_buffer[MAX_SLICE_LEN * 2];
    
    // Forward
    for (uint8_t i = 0; i < slice_len; i++) {
        slice_buffer[i] = s_sequence[i];
    }
    
    // Backward (skip first/last om herhaling te voorkomen)
    for (uint8_t i = 0; i < slice_len - 2; i++) {
        slice_buffer[slice_len + i] = s_sequence[slice_len - 2 - i];
    }
    
    uint8_t palindrome_len = (slice_len * 2) - 2;
    if (palindrome_len == 0) palindrome_len = 1;
    
    // Repeat
    for (uint8_t i = 0; i < s_sequence_length; i++) {
        s_sequence[i] = slice_buffer[i % palindrome_len];
    }
}

void slice_palindrome_shuffle_copy() {
    // Shuffle first, then palindrome
    uint8_t orig_len = s_sequence_length;
    uint8_t temp_len = clipminmaxi32(1, s_slice_length, s_sequence_length);
    
    // Shuffle first temp_len steps
    for (uint8_t i = temp_len - 1; i > 0; i--) {
        uint8_t j = xorshift32() % (i + 1);
        float temp = s_sequence[i];
        s_sequence[i] = s_sequence[j];
        s_sequence[j] = temp;
    }
    
    // Then apply palindrome
    slice_palindrome_copy();
}

// ========== OPERATION DISPATCHER ==========

void apply_operation(uint8_t op) {
    switch (op) {
        case 0: /* NONE */ break;
        case 1: randomize_sequence(); break;
        case 2: shuffle_sequence(); break;
        case 3: reverse_sequence(); break;
        case 4: slice_copy(); break;
        case 5: slice_shuffle_copy(); break;
        case 6: slice_palindrome_copy(); break;
        case 7: slice_palindrome_shuffle_copy(); break;
        default: break;
    }
}

// ========== SDK CALLBACKS ==========

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;

    // Initialize sequence met default pattern (ramp up)
    for (uint8_t i = 0; i < MAX_STEPS; i++) {
        s_sequence[i] = (float)i / (float)MAX_STEPS;
    }
    
    s_sequence_length = 16;
    s_current_step = 0;
    s_step_counter = 0;
    s_samples_per_step = 3000;  // 120 BPM, 16th notes
    
    // Parameters
    s_clock_mode = 0;
    s_slice_length = 4;
    s_smooth_amount = 0.25f;
    s_mod_depth = 1.0f;
    s_operation = 0;
    s_shift_amount = 0;
    s_rate_divider = 0;
    s_swing_amount = 0.5f;
    s_mix = 0.75f;
    
    s_current_value = 0.5f;
    s_target_value = s_sequence[0];
    s_tempo_bpm = 120;
    s_prev_level = 0.f;
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {
    // Geen dynamisch geheugen, niks te cleanen
}

__unit_callback void unit_reset() {
    s_current_step = 0;
    s_step_counter = 0;
    s_current_value = 0.5f;
    s_target_value = s_sequence[0];
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    // Tempo in BPM × 10 (e.g., 1200 = 120.0 BPM)
    float bpm = (float)tempo / 10.f;
    if (bpm < 60.f) bpm = 120.f;  // Default
    s_tempo_bpm = (uint32_t)bpm;
    if (s_tempo_bpm < 60) s_tempo_bpm = 60;
    if (s_tempo_bpm > 200) s_tempo_bpm = 200;
    
    // Bereken samples per 16th note
    float sixteenth_per_sec = ((float)s_tempo_bpm / 60.f) * 4.f;
    s_samples_per_step = (uint32_t)(48000.f / sixteenth_per_sec);
    
    // Apply rate divider
    uint32_t dividers[] = {1, 2, 4, 8, 16};  // 1/16, 1/8, 1/4, 1/2, 1/1
    if (s_rate_divider < 5) {
        s_samples_per_step *= dividers[s_rate_divider];
    }
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    // 4PPQN = 16th note tick in 1/16 mode
    if (s_clock_mode == 0) {
        // Could use this for ultra-tight sync, maar we gebruiken internal counter
    }
}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    for (uint32_t f = 0; f < frames; f++) {
        float in_l = clipminmaxf(-1.f, in[f * 2], 1.f);
        float in_r = clipminmaxf(-1.f, in[f * 2 + 1], 1.f);
        
        // Step advance logic
        if (s_clock_mode == 0) {  // 1/16 mode
            s_step_counter++;
            
            // Apply swing (offset even steps)
            uint32_t step_len = s_samples_per_step;
            if (s_current_step % 2 == 1) {
                float swing_offset = (s_swing_amount - 0.5f) * 0.5f;  // ±25%
                step_len = (uint32_t)((float)s_samples_per_step * (1.f + swing_offset));
            }
            
            if (s_step_counter >= step_len) {
                s_current_step = (s_current_step + 1) % s_sequence_length;
                s_step_counter = 0;
                s_target_value = s_sequence[s_current_step];
            }
        } else {  // MIDI mode - detect input level change
            float current_level = si_fabsf(in_l) + si_fabsf(in_r);
            
            // Detect attack (sudden increase)
            if (current_level > s_prev_level + 0.3f) {
                // Trigger next step
                s_current_step = (s_current_step + 1) % s_sequence_length;
                s_target_value = s_sequence[s_current_step];
            }
            
            s_prev_level = current_level * 0.99f;  // Decay
        }
        
        // Smooth interpolation
        float smooth_coeff = 0.001f + s_smooth_amount * 0.099f;  // 0.1% - 10% per sample
        s_current_value += (s_target_value - s_current_value) * smooth_coeff;
        
        // Apply modulation
        float mod_gain = s_current_value * s_mod_depth;
        mod_gain = clipminmaxf(0.f, mod_gain, 1.f);
        
        float mod_l = in_l * mod_gain;
        float mod_r = in_r * mod_gain;
        
        // Dry/wet mix
        out[f * 2] = in_l * (1.f - s_mix) + mod_l * s_mix;
        out[f * 2 + 1] = in_r * (1.f - s_mix) + mod_r * s_mix;
        
        // Output limiting
        out[f * 2] = clipminmaxf(-1.f, out[f * 2], 1.f);
        out[f * 2 + 1] = clipminmaxf(-1.f, out[f * 2 + 1], 1.f);
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    static int8_t last_shift = 0;
    static uint8_t last_operation = 0;
    
    switch (id) {
        case 0: s_clock_mode = value; break;
        case 1: 
            s_sequence_length = value;
            if (s_current_step >= s_sequence_length) {
                s_current_step = 0;
            }
            break;
        case 2: s_slice_length = value; break;
        case 3: s_smooth_amount = valf; break;
        case 4: s_mod_depth = valf; break;
        case 5: 
            if (value != last_operation) {
                apply_operation(value);
                last_operation = value;
            }
            s_operation = value;
            break;
        case 6:
            if (value != last_shift) {
                int8_t diff = value - last_shift;
                if (diff > 0) {
                    shift_sequence_right(diff);
                } else if (diff < 0) {
                    shift_sequence_left(-diff);
                }
                last_shift = value;
            }
            s_shift_amount = value;
            break;
        case 7: 
            s_rate_divider = value;
            unit_set_tempo(s_tempo_bpm * 10);  // Recalc step length
            break;
        case 8: s_swing_amount = valf; break;
        case 9: s_mix = valf; break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return s_clock_mode;
        case 1: return s_sequence_length;
        case 2: return s_slice_length;
        case 3: return (int32_t)(s_smooth_amount * 1023.f);
        case 4: return (int32_t)(s_mod_depth * 1023.f);
        case 5: return s_operation;
        case 6: return s_shift_amount;
        case 7: return s_rate_divider;
        case 8: return (int32_t)(s_swing_amount * 1023.f);
        case 9: return (int32_t)(s_mix * 1023.f);
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    if (id == 0) {  // Clock mode
        return value ? "MIDI" : "1/16";
    }
    if (id == 5) {  // Operation
        static const char *ops[] = {
            "NONE", "RAND", "SHUF", "REV", 
            "COPY", "CSHUF", "PCOPY", "PSHUF"
        };
        if (value >= 0 && value < 8) return ops[value];
    }
    if (id == 7) {  // Rate divider
        static const char *divs[] = {"1/16", "1/8", "1/4", "1/2", "1/1"};
        if (value >= 0 && value < 5) return divs[value];
    }
    return "";
}

