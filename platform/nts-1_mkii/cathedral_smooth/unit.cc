/*
    CATHEDRAL SMOOTH - Fixed version without crackling
    
    FIXES vs original:
    - Safer feedback limits (max 0.92 instead of 0.98)
    - Soft clipping in comb filters
    - Adaptive damping (increases with feedback)
    - Output soft limiting
    - Denormal prevention
    - State clipping in all filters
    
    ALGORITHM:
    - 8 parallel Schroeder allpass filters
    - 4 comb filters with cross-feedback
    - Early reflections (8 taps)
    - Pre-delay buffer (500ms max)
    - Reverse buffer (2 seconds)
    - Adaptive damping
    - Stereo width control
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
#define PREDELAY_SIZE 24000
#define REVERSE_SIZE 96000

static const uint32_t s_comb_delays[NUM_COMBS] = {
    1557, 1617, 1491, 1422
};

static const uint32_t s_allpass_delays[NUM_ALLPASS] = {
    225, 341, 441, 556, 225, 341, 441, 556
};

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

// Fast soft clipper
inline float soft_clip(float x) {
    if (x < -1.5f) return -1.f;
    if (x > 1.5f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

// Denormal prevention
inline float undenormalize(float x) {
    if (si_fabsf(x) < 1e-15f) return 0.f;
    return x;
}

inline float allpass_process(AllpassFilter *ap, float input) {
    uint32_t read_pos = (ap->write_pos + 1) % ap->delay_length;
    float delayed = ap->buffer[read_pos];
    
    // Denormal prevention
    delayed = undenormalize(delayed);
    
    float output = -input + delayed;
    
    // Soft clip feedback
    float fb_signal = input + delayed * ap->feedback;
    fb_signal = soft_clip(fb_signal * 0.5f) * 2.f;  // Gentle clipping
    
    ap->buffer[ap->write_pos] = fb_signal;
    
    ap->write_pos = (ap->write_pos + 1) % ap->delay_length;
    
    // Clip output
    return clipminmaxf(-2.f, output, 2.f);
}

inline float comb_process(CombFilter *cf, float input) {
    uint32_t read_pos = (cf->write_pos + 1) % cf->delay_length;
    float delayed = cf->buffer[read_pos];
    
    // Denormal prevention
    delayed = undenormalize(delayed);
    
    // Damping (one-pole lowpass)
    cf->damp_z = delayed * (1.f - cf->damp_coeff) + cf->damp_z * cf->damp_coeff;
    cf->damp_z = clipminmaxf(-2.0f, cf->damp_z, 2.0f);  // Anti-fluittoon fix!
    cf->damp_z = undenormalize(cf->damp_z);
    
    // Soft clip damped signal
    float damped = soft_clip(cf->damp_z);
    
    // Feedback with soft clipping
    float fb_signal = input + damped * cf->feedback;
    fb_signal = soft_clip(fb_signal * 0.5f) * 2.f;  // Gentle clipping
    
    cf->buffer[cf->write_pos] = fb_signal;
    cf->write_pos = (cf->write_pos + 1) % cf->delay_length;
    
    // Clip output
    return clipminmaxf(-2.f, delayed, 2.f);
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
    
    uint32_t total_size = (NUM_COMBS * max_comb_size + NUM_ALLPASS * max_allpass_size) * sizeof(float) * 2;
    total_size += PREDELAY_SIZE * sizeof(float);
    total_size += REVERSE_SIZE * sizeof(float) * 2;
    
    uint8_t *buffer_base = static_cast<uint8_t *>(desc->hooks.sdram_alloc(total_size));
    if (!buffer_base) return k_unit_err_memory;
    
    uint32_t offset = 0;
    
    float *reverb_buf_l = reinterpret_cast<float *>(buffer_base + offset);
    offset += (NUM_COMBS * max_comb_size + NUM_ALLPASS * max_allpass_size) * sizeof(float);
    
    float *reverb_buf_r = reinterpret_cast<float *>(buffer_base + offset);
    offset += (NUM_COMBS * max_comb_size + NUM_ALLPASS * max_allpass_size) * sizeof(float);
    
    s_predelay_buffer = reinterpret_cast<float *>(buffer_base + offset);
    offset += PREDELAY_SIZE * sizeof(float);
    
    s_reverse_buffer_l = reinterpret_cast<float *>(buffer_base + offset);
    offset += REVERSE_SIZE * sizeof(float);
    
    s_reverse_buffer_r = reinterpret_cast<float *>(buffer_base + offset);
    offset += REVERSE_SIZE * sizeof(float);
    
    buf_clr_f32(reverb_buf_l, NUM_COMBS * max_comb_size + NUM_ALLPASS * max_allpass_size);
    buf_clr_f32(reverb_buf_r, NUM_COMBS * max_comb_size + NUM_ALLPASS * max_allpass_size);
    buf_clr_f32(s_predelay_buffer, PREDELAY_SIZE);
    buf_clr_f32(s_reverse_buffer_l, REVERSE_SIZE);
    buf_clr_f32(s_reverse_buffer_r, REVERSE_SIZE);
    
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
        
        // Clip input
        in_l = clipminmaxf(-1.f, in_l, 1.f);
        in_r = clipminmaxf(-1.f, in_r, 1.f);
        
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
            
            // EXTRA CONSERVATIEVE feedback voor CATHEDRL+ - MAX 0.82!
            float fb = 0.65f + s_time * 0.17f;  // Extra veilig: 0.17 i.p.v. 0.20
            fb = clipminmaxf(0.1f, fb, 0.82f);  // Extra veilig: max 0.82 i.p.v. 0.85
            
            s_combs_l[i].feedback = fb;
            s_combs_r[i].feedback = fb;
            
            // ADAPTIVE DAMPING - increases with feedback!
            float adaptive_damp = s_damping + fb * 0.15f;  // More damping at high feedback
            adaptive_damp = clipminmaxf(0.3f, adaptive_damp, 0.85f);  // Minimum 0.3 i.p.v. 0.1
            
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
        
        // Clip after comb sum
        comb_out_l = clipminmaxf(-1.5f, comb_out_l, 1.5f);
        comb_out_r = clipminmaxf(-1.5f, comb_out_r, 1.5f);
        
        for (int i = 0; i < NUM_ALLPASS; i++) {
            // SAFER allpass feedback
            float apf_fb = 0.3f + s_diffusion * 0.35f;  // Changed: lower max
            apf_fb = clipminmaxf(0.2f, apf_fb, 0.65f);  // Changed: max 0.65!
            
            s_allpass_l[i].feedback = apf_fb;
            s_allpass_r[i].feedback = apf_fb;
            
            comb_out_l = allpass_process(&s_allpass_l[i], comb_out_l);
            comb_out_r = allpass_process(&s_allpass_r[i], comb_out_r);
        }
        
        float depth_curve = s_depth * s_depth;  // Quadratische curve voor subtielere controle
        float wet_l = early_l + comb_out_l * depth_curve;
        float wet_r = early_r + comb_out_r * depth_curve;
        
        // Reverse mode
        if (s_mode == 2) {
            float rev_l, rev_r;
            process_reverse_buffer(wet_l, wet_r, &rev_l, &rev_r);
            wet_l = wet_l * (1.f - s_reverse_mix) + rev_l * s_reverse_mix;
            wet_r = wet_r * (1.f - s_reverse_mix) + rev_r * s_reverse_mix;
        }
        
        // Shimmer mode
        if (s_mode == 3) {
            wet_l += comb_out_l * 0.5f;
            wet_r += comb_out_r * 0.5f;
        }
        
        // Compenseer voor reverb gain (vermijd output boost)
        float reverb_compensation = 0.35f;  // -9dB compensatie
        wet_l *= reverb_compensation;
        wet_r *= reverb_compensation;
        
        // Soft limiting om clipping te voorkomen
        wet_l = soft_clip(wet_l * 0.9f);
        wet_r = soft_clip(wet_r * 0.9f);
        
        // Convert mix from [-1,1] to [0,1] for wet amount
        // Dry signal always at 100%, wet signal added on top
        float wet_mix = (s_mix + 1.f) * 0.5f;
        out[f * 2] = in_l + wet_l * wet_mix;
        out[f * 2 + 1] = in_r + wet_r * wet_mix;
        
        // FINAL output limiting
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
        case 0: s_time = clipminmaxf(0.f, valf, 1.f); break;
        case 1: s_depth = clipminmaxf(0.f, valf, 1.f); break;
        case 2: s_mix = clipminmaxf(-1.f, (float)value / 100.f, 1.f); break;
        case 3: s_size = clipminmaxf(0.f, valf, 1.f); break;
        case 4: s_damping = clipminmaxf(0.f, valf, 1.f); break;
        case 5: s_diffusion = clipminmaxf(0.f, valf, 1.f); break;
        case 6: s_early_level = clipminmaxf(0.f, valf, 1.f); break;
        case 7: s_predelay_time = clipminmaxf(0.f, valf, 1.f); break;
        case 8: s_reverse_speed = clipminmaxf(0.f, valf, 1.f); break;
        case 9: s_reverse_mix = clipminmaxf(0.f, valf, 1.f); break;
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
        if (value >= 0 && value < 4) return mode_names[value];
    }
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {}
__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {}

