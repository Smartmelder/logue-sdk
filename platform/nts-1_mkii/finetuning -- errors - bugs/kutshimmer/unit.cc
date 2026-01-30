/*
    KUTSHIMMER - Shimmer/Ambient Reverb Effect
    
    ARCHITECTURE:
    - Pre-delay buffer
    - 8× all-pass diffusion network
    - Pitch shifter (+1 octave for shimmer)
    - Modulation (chorus-like movement)
    - Feedback with tone filtering
    - Ducking (sidechain compression)
    - Freeze/hold function
    
    4 MODES:
    - SHIMMER: Octave-up tails
    - REVERSE: Swelling/reverse-like
    - CLOUD: Dense, granular
    - INFINITE: Frozen soundscape
*/
#include "unit_revfx.h"
#include "fx_api.h"
#include "utils/float_math.h"

// ========== NaN/Inf CHECK MACRO ==========
#define is_finite(x) ((x) == (x) && (x) <= 1e10f && (x) >= -1e10f)

// ========== REVERB MODES ==========
enum ReverbMode {
    MODE_SHIMMER = 0,   // Octave-up shimmer
    MODE_REVERSE = 1,   // Reverse/swelling
    MODE_CLOUD = 2,     // Dense/granular
    MODE_INFINITE = 3   // Frozen/endless
};

// ========== MEMORY BUDGET ==========
#define MAX_REVERB_SAMPLES 96000   // 2 seconds @ 48kHz
#define NUM_ALLPASS 8
#define MAX_PREDELAY_SAMPLES 4800  // 100ms

// ========== ALL-PASS FILTER ==========
struct AllpassFilter {
    float z1;
    float coeff;
};

// ========== BUFFERS ==========
static float *s_reverb_buffer_l = nullptr;
static float *s_reverb_buffer_r = nullptr;
static float *s_predelay_buffer_l = nullptr;
static float *s_predelay_buffer_r = nullptr;
static uint32_t s_reverb_write_pos = 0;
static uint32_t s_predelay_write_pos = 0;
static AllpassFilter s_allpass_l[NUM_ALLPASS];
static AllpassFilter s_allpass_r[NUM_ALLPASS];

// ========== TONE FILTERS ==========
static float s_tone_lp_z1_l = 0.f;
static float s_tone_lp_z1_r = 0.f;
static float s_tone_hp_z1_l = 0.f;
static float s_tone_hp_z1_r = 0.f;

// ========== MODULATION ==========
static float s_mod_phase = 0.f;

// ========== PITCH SHIFTER STATE ==========
static float s_pitch_phase = 0.f;
static float s_pitch_crossfade = 0.f;

// ========== PARAMETERS ==========
static ReverbMode s_mode = MODE_SHIMMER;
static float s_time = 0.6f;
static float s_shimmer = 0.5f;
static float s_mix = 0.5f;
static float s_mod_rate = 0.3f;
static float s_mod_depth = 0.4f;
static float s_tone = 0.5f;
static float s_predelay = 0.2f;
static float s_duck = 0.3f;
static bool s_freeze = false;

// ========== DUCKING ENVELOPE ==========
static float s_duck_env = 1.f;

// ========== HELPER FUNCTIONS ==========
inline float delay_read(float *buffer, float delay_samples, uint32_t write_pos, uint32_t max_samples) {
    delay_samples = clipminmaxf(1.f, delay_samples, (float)(max_samples - 2));
    
    float read_pos_f = (float)write_pos - delay_samples;
    
    while (read_pos_f < 0.f) read_pos_f += (float)max_samples;
    while (read_pos_f >= (float)max_samples) read_pos_f -= (float)max_samples;
    
    uint32_t read_pos_0 = (uint32_t)read_pos_f;
    uint32_t read_pos_1 = (read_pos_0 + 1) % max_samples;
    
    float frac = read_pos_f - (float)read_pos_0;
    
    return buffer[read_pos_0] * (1.f - frac) + buffer[read_pos_1] * frac;
}

inline float allpass_process(AllpassFilter *ap, float input) {
    float output = -input + ap->z1;
    ap->z1 = input + ap->z1 * ap->coeff;
    
    // Denormal kill
    if (si_fabsf(ap->z1) < 1e-15f) ap->z1 = 0.f;
    
    // Clip
    ap->z1 = clipminmaxf(-3.f, ap->z1, 3.f);
    
    return output;
}

// ========== PITCH SHIFTER (Simple Octave-Up) ==========
inline float pitch_shift_octave(float *buffer, uint32_t write_pos, uint32_t max_samples) {
    // Simple pitch doubling using two read heads with crossfade
    float grain_size = 2400.f;  // 50ms grains
    
    s_pitch_phase += 2.f;  // Double speed = octave up
    if (s_pitch_phase >= grain_size) s_pitch_phase -= grain_size;
    
    // Two read heads
    float delay1 = s_pitch_phase;
    float delay2 = s_pitch_phase + grain_size * 0.5f;
    
    float read1 = delay_read(buffer, delay1, write_pos, max_samples);
    float read2 = delay_read(buffer, delay2, write_pos, max_samples);
    
    // Crossfade
    s_pitch_crossfade += 1.f / grain_size;
    if (s_pitch_crossfade >= 1.f) s_pitch_crossfade -= 1.f;
    
    float fade = s_pitch_crossfade;
    if (fade < 0.5f) {
        fade = fade * 2.f;
        return read1 * (1.f - fade) + read2 * fade;
    } else {
        fade = (fade - 0.5f) * 2.f;
        return read2 * (1.f - fade) + read1 * fade;
    }
}

// ========== TONE FILTER ==========
inline void apply_tone(float *l, float *r) {
    // LP filter
    float lp_coeff = 0.3f + s_tone * 0.5f;
    s_tone_lp_z1_l += lp_coeff * (*l - s_tone_lp_z1_l);
    s_tone_lp_z1_r += lp_coeff * (*r - s_tone_lp_z1_r);
    
    // HP filter
    float hp_coeff = 0.1f + (1.f - s_tone) * 0.3f;
    s_tone_hp_z1_l += hp_coeff * (*l - s_tone_hp_z1_l);
    s_tone_hp_z1_r += hp_coeff * (*r - s_tone_hp_z1_r);
    
    // Denormal kill
    if (si_fabsf(s_tone_lp_z1_l) < 1e-15f) s_tone_lp_z1_l = 0.f;
    if (si_fabsf(s_tone_lp_z1_r) < 1e-15f) s_tone_lp_z1_r = 0.f;
    if (si_fabsf(s_tone_hp_z1_l) < 1e-15f) s_tone_hp_z1_l = 0.f;
    if (si_fabsf(s_tone_hp_z1_r) < 1e-15f) s_tone_hp_z1_r = 0.f;
    
    // Mix LP/HP based on tone
    float hp_l = *l - s_tone_hp_z1_l;
    float hp_r = *r - s_tone_hp_z1_r;
    
    *l = s_tone_lp_z1_l * (1.f - s_tone * 0.5f) + hp_l * (s_tone * 0.3f);
    *r = s_tone_lp_z1_r * (1.f - s_tone * 0.5f) + hp_r * (s_tone * 0.3f);
}

// ========== DUCKING ==========
inline void update_ducking(float input_level) {
    if (s_duck < 0.01f) {
        s_duck_env = 1.f;
        return;
    }
    
    // Fast attack, slow release
    float target = 1.f - input_level * s_duck;
    target = clipminmaxf(0.1f, target, 1.f);
    
    if (target < s_duck_env) {
        // Attack
        s_duck_env += (target - s_duck_env) * 0.1f;
    } else {
        // Release
        s_duck_env += (target - s_duck_env) * 0.01f;
    }
}

// ========== MAIN REVERB PROCESSOR ==========
inline void process_kutshimmer(float in_l, float in_r, float *out_l, float *out_r) {
    // Input validation
    if (!is_finite(in_l)) in_l = 0.f;
    if (!is_finite(in_r)) in_r = 0.f;
    
    // Pre-delay
    float predelay_samples = s_predelay * (float)MAX_PREDELAY_SAMPLES;
    
    float predel_l = delay_read(s_predelay_buffer_l, predelay_samples, s_predelay_write_pos, MAX_PREDELAY_SAMPLES);
    float predel_r = delay_read(s_predelay_buffer_r, predelay_samples, s_predelay_write_pos, MAX_PREDELAY_SAMPLES);
    
    if (s_predelay_buffer_l && s_predelay_buffer_r) {
        s_predelay_buffer_l[s_predelay_write_pos] = in_l;
        s_predelay_buffer_r[s_predelay_write_pos] = in_r;
    }
    
    // Modulation LFO
    float mod_rate_hz = 0.1f + s_mod_rate * 4.9f;  // 0.1-5 Hz
    s_mod_phase += mod_rate_hz / 48000.f;
    if (s_mod_phase >= 1.f) s_mod_phase -= 1.f;
    
    float lfo = fx_sinf(s_mod_phase * 2.f * 3.14159265f);
    
    // Calculate decay time based on mode
    float decay_mult = 1.f;
    switch (s_mode) {
        case MODE_SHIMMER:
            decay_mult = 0.8f + s_time * 0.95f;  // 0.8-1.75
            break;
        case MODE_REVERSE:
            decay_mult = 0.7f + s_time * 0.9f;   // 0.7-1.6
            break;
        case MODE_CLOUD:
            decay_mult = 0.85f + s_time * 0.98f; // 0.85-1.83
            break;
        case MODE_INFINITE:
            decay_mult = 0.95f + s_time * 0.04f; // 0.95-0.99 (very long!)
            break;
    }
    
    if (s_freeze) {
        decay_mult = 0.999f;  // Near-infinite
    }
    
    // Read from reverb buffer (with modulation)
    float base_delay = 24000.f;  // 500ms base
    float mod_offset = lfo * s_mod_depth * 1200.f;
    float reverb_delay = base_delay + mod_offset;
    
    float reverb_l = delay_read(s_reverb_buffer_l, reverb_delay, s_reverb_write_pos, MAX_REVERB_SAMPLES);
    float reverb_r = delay_read(s_reverb_buffer_r, reverb_delay, s_reverb_write_pos, MAX_REVERB_SAMPLES);
    
    // Validate
    if (!is_finite(reverb_l)) reverb_l = 0.f;
    if (!is_finite(reverb_r)) reverb_r = 0.f;
    
    // Diffusion (all-pass network)
    for (uint8_t i = 0; i < NUM_ALLPASS; i++) {
        float coeff = 0.5f + (float)i * 0.05f;
        s_allpass_l[i].coeff = coeff;
        s_allpass_r[i].coeff = coeff;
        
        reverb_l = allpass_process(&s_allpass_l[i], reverb_l);
        reverb_r = allpass_process(&s_allpass_r[i], reverb_r);
    }
    
    // Mode-specific processing
    switch (s_mode) {
        case MODE_SHIMMER: {
            // Add pitch-shifted octave
            if (s_shimmer > 0.01f) {
                float shimmer_l = pitch_shift_octave(s_reverb_buffer_l, s_reverb_write_pos, MAX_REVERB_SAMPLES);
                float shimmer_r = pitch_shift_octave(s_reverb_buffer_r, s_reverb_write_pos, MAX_REVERB_SAMPLES);
                
                reverb_l = reverb_l * (1.f - s_shimmer * 0.5f) + shimmer_l * s_shimmer * 0.5f;
                reverb_r = reverb_r * (1.f - s_shimmer * 0.5f) + shimmer_r * s_shimmer * 0.5f;
            }
            break;
        }
        
        case MODE_REVERSE: {
            // Reverse-like envelope (swell)
            static float reverse_env = 0.f;
            reverse_env += 0.0001f;
            if (reverse_env > 1.f) reverse_env = 0.f;
            
            float swell = reverse_env * reverse_env;  // Exponential swell
            reverb_l *= swell;
            reverb_r *= swell;
            break;
        }
        
        case MODE_CLOUD: {
            // Dense, granular feel
            float grain_mod = fx_sinf(s_mod_phase * 8.f * 3.14159265f) * 0.3f;
            reverb_l += reverb_l * grain_mod * s_mod_depth;
            reverb_r += reverb_r * grain_mod * s_mod_depth;
            break;
        }
        
        case MODE_INFINITE: {
            // Extra long, frozen
            // (handled by decay_mult)
            break;
        }
    }
    
    // Apply tone filter
    apply_tone(&reverb_l, &reverb_r);
    
    // Feedback
    float feedback_signal_l = predel_l + reverb_l * decay_mult;
    float feedback_signal_r = predel_r + reverb_r * decay_mult;
    
    // Clip feedback
    feedback_signal_l = clipminmaxf(-2.f, feedback_signal_l, 2.f);
    feedback_signal_r = clipminmaxf(-2.f, feedback_signal_r, 2.f);
    
    // Write to reverb buffer
    if (s_reverb_buffer_l && s_reverb_buffer_r) {
        s_reverb_buffer_l[s_reverb_write_pos] = feedback_signal_l;
        s_reverb_buffer_r[s_reverb_write_pos] = feedback_signal_r;
    }
    
    // Update ducking
    float input_level = si_fabsf(in_l) + si_fabsf(in_r);
    update_ducking(input_level);
    
    // Apply ducking to reverb
    reverb_l *= s_duck_env;
    reverb_r *= s_duck_env;
    
    // Validate output
    if (!is_finite(reverb_l)) reverb_l = 0.f;
    if (!is_finite(reverb_r)) reverb_r = 0.f;
    
    // Dry/wet mix
    *out_l = in_l * (1.f - s_mix) + reverb_l * s_mix;
    *out_r = in_r * (1.f - s_mix) + reverb_r * s_mix;
}

// ========== UNIT CALLBACKS ==========
__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;
    if (!desc->hooks.sdram_alloc) return k_unit_err_memory;
    
    // Allocate SDRAM buffers
    size_t reverb_size = MAX_REVERB_SAMPLES * sizeof(float) * 2;
    size_t predelay_size = MAX_PREDELAY_SAMPLES * sizeof(float) * 2;
    size_t total_size = reverb_size + predelay_size;
    
    uint8_t *buffer_base = static_cast<uint8_t *>(desc->hooks.sdram_alloc(total_size));
    
    if (!buffer_base) return k_unit_err_memory;
    
    s_reverb_buffer_l = reinterpret_cast<float *>(buffer_base);
    s_reverb_buffer_r = reinterpret_cast<float *>(buffer_base + MAX_REVERB_SAMPLES * sizeof(float));
    s_predelay_buffer_l = reinterpret_cast<float *>(buffer_base + reverb_size);
    s_predelay_buffer_r = reinterpret_cast<float *>(buffer_base + reverb_size + MAX_PREDELAY_SAMPLES * sizeof(float));
    
    // Clear buffers
    for (uint32_t i = 0; i < MAX_REVERB_SAMPLES; i++) {
        s_reverb_buffer_l[i] = 0.f;
        s_reverb_buffer_r[i] = 0.f;
    }
    
    for (uint32_t i = 0; i < MAX_PREDELAY_SAMPLES; i++) {
        s_predelay_buffer_l[i] = 0.f;
        s_predelay_buffer_r[i] = 0.f;
    }
    
    s_reverb_write_pos = 0;
    s_predelay_write_pos = 0;
    
    // Initialize all-pass filters
    for (uint8_t i = 0; i < NUM_ALLPASS; i++) {
        s_allpass_l[i].z1 = 0.f;
        s_allpass_l[i].coeff = 0.5f;
        s_allpass_r[i].z1 = 0.f;
        s_allpass_r[i].coeff = 0.5f;
    }
    
    s_tone_lp_z1_l = 0.f;
    s_tone_lp_z1_r = 0.f;
    s_tone_hp_z1_l = 0.f;
    s_tone_hp_z1_r = 0.f;
    
    s_mod_phase = 0.f;
    s_pitch_phase = 0.f;
    s_pitch_crossfade = 0.f;
    s_duck_env = 1.f;
    
    s_mode = MODE_SHIMMER;
    s_time = 0.6f;
    s_shimmer = 0.5f;
    s_mix = 0.5f;
    s_mod_rate = 0.3f;
    s_mod_depth = 0.4f;
    s_tone = 0.5f;
    s_predelay = 0.2f;
    s_duck = 0.3f;
    s_freeze = false;
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    if (s_reverb_buffer_l) {
        for (uint32_t i = 0; i < MAX_REVERB_SAMPLES; i++) {
            s_reverb_buffer_l[i] = 0.f;
        }
    }
    if (s_reverb_buffer_r) {
        for (uint32_t i = 0; i < MAX_REVERB_SAMPLES; i++) {
            s_reverb_buffer_r[i] = 0.f;
        }
    }
    if (s_predelay_buffer_l) {
        for (uint32_t i = 0; i < MAX_PREDELAY_SAMPLES; i++) {
            s_predelay_buffer_l[i] = 0.f;
        }
    }
    if (s_predelay_buffer_r) {
        for (uint32_t i = 0; i < MAX_PREDELAY_SAMPLES; i++) {
            s_predelay_buffer_r[i] = 0.f;
        }
    }
    
    for (uint8_t i = 0; i < NUM_ALLPASS; i++) {
        s_allpass_l[i].z1 = 0.f;
        s_allpass_r[i].z1 = 0.f;
    }
    
    s_tone_lp_z1_l = 0.f;
    s_tone_lp_z1_r = 0.f;
    s_tone_hp_z1_l = 0.f;
    s_tone_hp_z1_r = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    const float *in_ptr = in;
    float *out_ptr = out;
    
    for (uint32_t f = 0; f < frames; f++) {
        float out_l, out_r;
        process_kutshimmer(in_ptr[0], in_ptr[1], &out_l, &out_r);
        
        // Final limiting
        out_ptr[0] = clipminmaxf(-1.f, out_l, 1.f);
        out_ptr[1] = clipminmaxf(-1.f, out_r, 1.f);
        
        s_reverb_write_pos = (s_reverb_write_pos + 1) % MAX_REVERB_SAMPLES;
        s_predelay_write_pos = (s_predelay_write_pos + 1) % MAX_PREDELAY_SAMPLES;
        
        in_ptr += 2;
        out_ptr += 2;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_mode = (ReverbMode)value; break;
        case 1: s_time = valf; break;
        case 2: s_shimmer = valf; break;
        case 3: s_mix = valf; break;
        case 4: s_mod_rate = valf; break;
        case 5: s_mod_depth = valf; break;
        case 6: s_tone = valf; break;
        case 7: s_predelay = valf; break;
        case 8: s_duck = valf; break;
        case 9: s_freeze = (value != 0); break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return (int32_t)s_mode;
        case 1: return (int32_t)(s_time * 1023.f);
        case 2: return (int32_t)(s_shimmer * 1023.f);
        case 3: return (int32_t)(s_mix * 1023.f);
        case 4: return (int32_t)(s_mod_rate * 1023.f);
        case 5: return (int32_t)(s_mod_depth * 1023.f);
        case 6: return (int32_t)(s_tone * 1023.f);
        case 7: return (int32_t)(s_predelay * 1023.f);
        case 8: return (int32_t)(s_duck * 1023.f);
        case 9: return s_freeze ? 1 : 0;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    if (id == 0) {
        switch ((ReverbMode)value) {
            case MODE_SHIMMER: return "SHIMMER";
            case MODE_REVERSE: return "REVERSE";
            case MODE_CLOUD: return "CLOUD";
            case MODE_INFINITE: return "INFINIT";
        }
    }
    if (id == 9) {
        return (value != 0) ? "ON" : "OFF";
    }
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    (void)tempo;
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    (void)counter;
}

