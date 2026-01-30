/*
    CATHEDRAL REVERB + REVERSE EFFECT
    
    ALGORITME:
    - 8 parallel Schroeder allpass filters (diffusion)
    - 4 comb filters met cross-feedback (dense reverb tail)
    - Early reflections (8 taps)
    - Pre-delay buffer (max 500ms)
    - Reverse buffer (2 seconds)
    - High-frequency damping
    - Stereo width control
    - Multi-mode: Cathedral / Hall / Reverse / Shimmer
    
    BRONNEN:
    - Schroeder Reverb (1962)
    - Freeverb Algorithm
    - Jon Dattorro Reverb (1997)
    - Reverse Reverb Techniques
*/

#include "unit_revfx.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "utils/buffer_ops.h"
#include "macros.h"
#include <algorithm>

#define NUM_COMBS 4
#define NUM_ALLPASS 8
#define NUM_EARLY_TAPS 8
#define PREDELAY_SIZE 24000  // 500ms @ 48kHz
#define REVERSE_SIZE 96000   // 2 seconds @ 48kHz

// Comb filter delays (prime numbers for density)
static const uint32_t s_comb_delays[NUM_COMBS] = {
    1557, 1617, 1491, 1422
};

// Allpass filter delays
static const uint32_t s_allpass_delays[NUM_ALLPASS] = {
    225, 341, 441, 556, 225, 341, 441, 556
};

// Early reflection taps (ms * 48)
static const uint32_t s_early_taps[NUM_EARLY_TAPS] = {
    480, 960, 1440, 1920, 2880, 3840, 5280, 7200
};

struct CombFilter {
    uint32_t write_pos;
    uint32_t delay_length;
    float feedback;
    float damp_z;
    float damp_coeff;
    float *buffer;
};

struct AllpassFilter {
    uint32_t write_pos;
    uint32_t delay_length;
    float feedback;
    float *buffer;
};

static CombFilter s_combs_l[NUM_COMBS];
static CombFilter s_combs_r[NUM_COMBS];
static AllpassFilter s_allpass_l[NUM_ALLPASS];
static AllpassFilter s_allpass_r[NUM_ALLPASS];

static float *s_predelay_buffer;
static float *s_reverse_buffer_l;
static float *s_reverse_buffer_r;

static uint32_t s_predelay_write;
static uint32_t s_reverse_write;
static uint32_t s_reverse_read;
static bool s_reverse_recording;
static uint32_t s_reverse_counter;

static float s_time;
static float s_depth;
static float s_mix;
static float s_size;
static float s_damping;
static float s_diffusion;
static float s_early_level;
static float s_predelay_time;
static float s_reverse_speed;
static float s_reverse_mix;
static uint8_t s_mode;

static uint32_t s_sample_counter;

inline float allpass_process(AllpassFilter *ap, float input) {
    uint32_t read_pos = (ap->write_pos + 1) % ap->delay_length;
    float delayed = ap->buffer[read_pos];
    
    float output = -input + delayed;
    ap->buffer[ap->write_pos] = input + delayed * ap->feedback;
    
    ap->write_pos = (ap->write_pos + 1) % ap->delay_length;
    return output;
}

inline float comb_process(CombFilter *cf, float input) {
    uint32_t read_pos = (cf->write_pos + 1) % cf->delay_length;
    float delayed = cf->buffer[read_pos];
    
    cf->damp_z = delayed * (1.f - cf->damp_coeff) + cf->damp_z * cf->damp_coeff;
    cf->damp_z = clipminmaxf(-2.0f, cf->damp_z, 2.0f);  // Anti-fluittoon fix!
    
    cf->buffer[cf->write_pos] = input + cf->damp_z * cf->feedback;
    cf->write_pos = (cf->write_pos + 1) % cf->delay_length;
    
    return delayed;
}

inline float process_early_reflections(float input, float level) {
    if (level < 0.01f) return 0.f;
    
    float output = 0.f;
    for (int i = 0; i < NUM_EARLY_TAPS; i++) {
        uint32_t tap_pos = (s_predelay_write + PREDELAY_SIZE - s_early_taps[i]) % PREDELAY_SIZE;
        float tap = s_predelay_buffer[tap_pos];
        float decay = 1.f - ((float)i / (float)NUM_EARLY_TAPS) * 0.6f;
        output += tap * decay;
    }
    return output * level / (float)NUM_EARLY_TAPS;
}

inline void process_reverse_buffer(float in_l, float in_r, float *out_l, float *out_r) {
    if (s_reverse_speed < 0.01f) {
        *out_l = 0.f;
        *out_r = 0.f;
        return;
    }
    
    s_reverse_buffer_l[s_reverse_write] = in_l;
    s_reverse_buffer_r[s_reverse_write] = in_r;
    s_reverse_write = (s_reverse_write + 1) % REVERSE_SIZE;
    
    if (s_reverse_recording) {
        s_reverse_counter++;
        if (s_reverse_counter >= REVERSE_SIZE) {
            s_reverse_recording = false;
            s_reverse_read = s_reverse_write;
        }
        *out_l = 0.f;
        *out_r = 0.f;
    } else {
        float playback_speed = 1.f + s_reverse_speed * 3.f;
        s_reverse_read = (s_reverse_read + REVERSE_SIZE - (uint32_t)playback_speed) % REVERSE_SIZE;
        
        *out_l = s_reverse_buffer_l[s_reverse_read];
        *out_r = s_reverse_buffer_r[s_reverse_read];
        
        if (s_reverse_read <= 10) {
            s_reverse_recording = true;
            s_reverse_counter = 0;
        }
    }
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc)
{
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;

    if (!desc->hooks.sdram_alloc) return k_unit_err_memory;
    
    uint32_t max_comb_size = 0;
    for (int i = 0; i < NUM_COMBS; i++) {
        if (s_comb_delays[i] > max_comb_size) max_comb_size = s_comb_delays[i];
    }
    max_comb_size = (uint32_t)((float)max_comb_size * 2.5f);
    
    uint32_t max_allpass_size = 0;
    for (int i = 0; i < NUM_ALLPASS; i++) {
        if (s_allpass_delays[i] > max_allpass_size) max_allpass_size = s_allpass_delays[i];
    }
    max_allpass_size = (uint32_t)((float)max_allpass_size * 2.5f);
    
    uint32_t total_size = (NUM_COMBS * max_comb_size + NUM_ALLPASS * max_allpass_size) * sizeof(float) * 2; // L+R
    total_size += PREDELAY_SIZE * sizeof(float);
    total_size += REVERSE_SIZE * sizeof(float) * 2; // L+R
    
    uint8_t *buffer_base = static_cast<uint8_t *>(desc->hooks.sdram_alloc(total_size));
    if (!buffer_base) return k_unit_err_memory;
    
    uint32_t offset = 0;
    
    // Left reverb buffers
    float *reverb_buf_l = reinterpret_cast<float *>(buffer_base + offset);
    offset += (NUM_COMBS * max_comb_size + NUM_ALLPASS * max_allpass_size) * sizeof(float);
    
    // Right reverb buffers
    float *reverb_buf_r = reinterpret_cast<float *>(buffer_base + offset);
    offset += (NUM_COMBS * max_comb_size + NUM_ALLPASS * max_allpass_size) * sizeof(float);
    
    // Pre-delay buffer
    s_predelay_buffer = reinterpret_cast<float *>(buffer_base + offset);
    offset += PREDELAY_SIZE * sizeof(float);
    
    // Reverse buffers
    s_reverse_buffer_l = reinterpret_cast<float *>(buffer_base + offset);
    offset += REVERSE_SIZE * sizeof(float);
    
    s_reverse_buffer_r = reinterpret_cast<float *>(buffer_base + offset);
    offset += REVERSE_SIZE * sizeof(float);
    
    // Clear all buffers
    buf_clr_f32(reverb_buf_l, NUM_COMBS * max_comb_size + NUM_ALLPASS * max_allpass_size);
    buf_clr_f32(reverb_buf_r, NUM_COMBS * max_comb_size + NUM_ALLPASS * max_allpass_size);
    buf_clr_f32(s_predelay_buffer, PREDELAY_SIZE);
    buf_clr_f32(s_reverse_buffer_l, REVERSE_SIZE);
    buf_clr_f32(s_reverse_buffer_r, REVERSE_SIZE);
    
    // Initialize comb filters
    uint32_t comb_offset = 0;
    for (int i = 0; i < NUM_COMBS; i++) {
        s_combs_l[i].write_pos = 0;
        s_combs_l[i].delay_length = s_comb_delays[i];
        s_combs_l[i].feedback = 0.84f;
        s_combs_l[i].damp_z = 0.f;
        s_combs_l[i].damp_coeff = 0.2f;
        s_combs_l[i].buffer = reverb_buf_l + comb_offset;
        
        s_combs_r[i].write_pos = 0;
        s_combs_r[i].delay_length = s_comb_delays[i] + 23;
        s_combs_r[i].feedback = 0.84f;
        s_combs_r[i].damp_z = 0.f;
        s_combs_r[i].damp_coeff = 0.2f;
        s_combs_r[i].buffer = reverb_buf_r + comb_offset;
        
        comb_offset += max_comb_size;
    }
    
    // Initialize allpass filters
    uint32_t allpass_offset = comb_offset;
    for (int i = 0; i < NUM_ALLPASS; i++) {
        s_allpass_l[i].write_pos = 0;
        s_allpass_l[i].delay_length = s_allpass_delays[i];
        s_allpass_l[i].feedback = 0.5f;
        s_allpass_l[i].buffer = reverb_buf_l + allpass_offset;
        
        s_allpass_r[i].write_pos = 0;
        s_allpass_r[i].delay_length = s_allpass_delays[i] + 17;
        s_allpass_r[i].feedback = 0.5f;
        s_allpass_r[i].buffer = reverb_buf_r + allpass_offset;
        
        allpass_offset += max_allpass_size;
    }
    
    s_predelay_write = 0;
    s_reverse_write = 0;
    s_reverse_read = 0;
    s_reverse_recording = true;
    s_reverse_counter = 0;
    
    s_time = 0.3f;           // 30% - kortere reverb tail
    s_depth = 0.2f;          // 20% - subtielere mix
    s_mix = 0.35f;           // 35% dry/wet
    s_size = 0.4f;           // 40% - medium room
    s_damping = 0.5f;        // 50% - meer HF demping
    s_diffusion = 0.25f;     // 25% - natuurlijker
    s_early_level = 0.1f;    // 10% - subtiele early reflections
    s_predelay_time = 0.15f; // 15% - korte pre-delay
    s_reverse_speed = 0.f;   // 0% - uit
    s_reverse_mix = 0.f;     // 0% - uit
    s_mode = 0;
    
    s_sample_counter = 0;

    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset()
{
    for (int i = 0; i < NUM_COMBS; i++) {
        s_combs_l[i].write_pos = 0;
        s_combs_l[i].damp_z = 0.f;
        s_combs_r[i].write_pos = 0;
        s_combs_r[i].damp_z = 0.f;
    }
    for (int i = 0; i < NUM_ALLPASS; i++) {
        s_allpass_l[i].write_pos = 0;
        s_allpass_r[i].write_pos = 0;
    }
    s_predelay_write = 0;
    s_reverse_write = 0;
    s_reverse_read = 0;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames)
{
    for (uint32_t f = 0; f < frames; f++) {
        float in_l = in[f * 2];
        float in_r = in[f * 2 + 1];
        
        float predelay_time_samps = s_predelay_time * (float)PREDELAY_SIZE;
        uint32_t predelay_read = (s_predelay_write + PREDELAY_SIZE - (uint32_t)predelay_time_samps) % PREDELAY_SIZE;
        
        float predelayed = (s_predelay_buffer[predelay_read] + (in_l + in_r) * 0.5f) * 0.5f;
        s_predelay_buffer[s_predelay_write] = (in_l + in_r) * 0.5f;
        s_predelay_write = (s_predelay_write + 1) % PREDELAY_SIZE;
        
        float early_l = process_early_reflections(predelayed, s_early_level);
        float early_r = process_early_reflections(predelayed, s_early_level);
        
        float size_scale = 0.7f + s_size * 0.6f;
        for (int i = 0; i < NUM_COMBS; i++) {
            s_combs_l[i].delay_length = (uint32_t)((float)s_comb_delays[i] * size_scale);
            s_combs_r[i].delay_length = (uint32_t)((float)(s_comb_delays[i] + 23) * size_scale);
            
            float fb = 0.65f + s_time * 0.20f;
            fb = clipminmaxf(0.1f, fb, 0.85f);
            s_combs_l[i].feedback = fb;
            s_combs_r[i].feedback = fb;
            
            float adaptive_damp = s_damping + fb * 0.15f;
            adaptive_damp = clipminmaxf(0.3f, adaptive_damp, 0.85f);
            s_combs_l[i].damp_coeff = adaptive_damp;
            s_combs_r[i].damp_coeff = adaptive_damp;
        }
        
        float comb_input = predelayed;
        
        float comb_out_l = 0.f;
        float comb_out_r = 0.f;
        
        for (int i = 0; i < NUM_COMBS; i++) {
            comb_out_l += comb_process(&s_combs_l[i], comb_input);
            comb_out_r += comb_process(&s_combs_r[i], comb_input);
        }
        comb_out_l /= (float)NUM_COMBS;
        comb_out_r /= (float)NUM_COMBS;
        
        for (int i = 0; i < NUM_ALLPASS; i++) {
            s_allpass_l[i].feedback = 0.3f + s_diffusion * 0.4f;
            s_allpass_r[i].feedback = 0.3f + s_diffusion * 0.4f;
            
            comb_out_l = allpass_process(&s_allpass_l[i], comb_out_l);
            comb_out_r = allpass_process(&s_allpass_r[i], comb_out_r);
        }
        
        float depth_curve = s_depth * s_depth;  // Quadratische curve voor subtielere controle
        float wet_l = early_l + comb_out_l * depth_curve;
        float wet_r = early_r + comb_out_r * depth_curve;
        
        if (s_mode == 2) {
            float rev_l, rev_r;
            process_reverse_buffer(wet_l, wet_r, &rev_l, &rev_r);
            wet_l = wet_l * (1.f - s_reverse_mix) + rev_l * s_reverse_mix;
            wet_r = wet_r * (1.f - s_reverse_mix) + rev_r * s_reverse_mix;
        }
        
        if (s_mode == 3) {
            wet_l += comb_out_l * 0.5f;
            wet_r += comb_out_r * 0.5f;
        }
        
        // Compenseer voor reverb gain (vermijd output boost)
        float reverb_compensation = 0.35f;  // -9dB compensatie
        wet_l *= reverb_compensation;
        wet_r *= reverb_compensation;
        
        // Soft limiting om clipping te voorkomen
        wet_l = fastertanhf(wet_l * 0.9f);
        wet_r = fastertanhf(wet_r * 0.9f);
        
        float dry_wet = (s_mix + 1.f) / 2.f;
        out[f * 2] = in_l * (1.f - dry_wet) + wet_l * dry_wet;
        out[f * 2 + 1] = in_r * (1.f - dry_wet) + wet_r * dry_wet;
        
        // Output limiting to prevent clipping
        out[f * 2] = clipminmaxf(-1.f, out[f * 2], 1.f);
        out[f * 2 + 1] = clipminmaxf(-1.f, out[f * 2 + 1], 1.f);
        
        s_sample_counter++;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_time = valf; break;
        case 1: s_depth = valf; break;
        case 2: s_mix = (float)value / 100.f; break;
        case 3: s_size = valf; break;
        case 4: s_damping = valf; break;
        case 5: s_diffusion = valf; break;
        case 6: s_early_level = valf; break;
        case 7: s_predelay_time = valf; break;
        case 8: s_reverse_speed = valf; break;
        case 9: s_reverse_mix = valf; break;
        case 10: s_mode = value; break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0: return (int32_t)(s_time * 1023.f);
        case 1: return (int32_t)(s_depth * 1023.f);
        case 2: return (int32_t)(s_mix * 100.f);
        case 3: return (int32_t)(s_size * 1023.f);
        case 4: return (int32_t)(s_damping * 1023.f);
        case 5: return (int32_t)(s_diffusion * 1023.f);
        case 6: return (int32_t)(s_early_level * 1023.f);
        case 7: return (int32_t)(s_predelay_time * 1023.f);
        case 8: return (int32_t)(s_reverse_speed * 1023.f);
        case 9: return (int32_t)(s_reverse_mix * 1023.f);
        case 10: return s_mode;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
    if (id == 10) {
        static const char *mode_names[] = {"CATHDRL", "HALL", "REVERSE", "SHIMMER"};
        return mode_names[value];
    }
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {}
__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {}

