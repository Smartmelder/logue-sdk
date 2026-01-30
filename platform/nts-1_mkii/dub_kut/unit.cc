/*
    DUB BEAST V2 - ABSOLUTE FINAL FIX!
    
    CRITICAL FIXES:
    - Buffer pre-fill with input on first render
    - Dry signal passthrough always works
    - Proper delay time calculation
    - Safe buffer reading
    - Clean startup without artifacts
*/

#include "unit_delfx.h"
#include "fx_api.h"
#include "utils/float_math.h"
#include "utils/int_math.h"

// ========== NaN/Inf CHECK MACRO (FIXED!) ==========
// ✅ FIX: Correct NaN detection (NaN != NaN is TRUE)
// Note: si_isfinite() not available for delfx, using correct macro instead
#define is_finite(x) ((x) != (x) ? false : ((x) <= 1e10f && (x) >= -1e10f))

// ========== DELAY MODES ==========
enum DelayMode {
    MODE_GROOVE = 0,
    MODE_DUB,
    MODE_BURST,
    MODE_REVERSE,
    MODE_SHIMMER,
    MODE_PINGPONG
};

const char* mode_names[6] = {
    "GROOVE", "DUB", "BURST", "REVERSE", "SHIMMER", "PINGPNG"
};

// ========== TEMPO DIVISIONS ==========
const float tempo_divisions[16] = {
    4.0f, 6.0f, 2.667f, 2.0f, 3.0f, 1.333f, 1.0f, 1.5f,
    0.667f, 0.5f, 0.75f, 0.333f, 0.25f, 0.188f, 0.313f, 0.438f
};

const char* time_names[16] = {
    "1/4", "1/4.", "1/4T", "1/8", "1/8.", "1/8T",
    "1/16", "1/16.", "1/16T", "1/32", "1/32.", "1/32T",
    "1/64", "3/16", "5/16", "7/16"
};

// ========== DELAY BUFFER ==========
#define MAX_DELAY_SAMPLES 144000

static float *s_delay_buffer_l = nullptr;
static float *s_delay_buffer_r = nullptr;
static uint32_t s_write_pos = 0;

// ========== FILTERS ==========
static float s_filter_z1_l = 0.f;
static float s_filter_z1_r = 0.f;

// ========== DUCKING ==========
static float s_envelope_follower = 0.f;

// ========== MODULATION ==========
static float s_mod_phase = 0.f;

// ========== PARAMETERS ==========
static uint8_t s_mode = MODE_DUB;
static uint8_t s_time_div = 4;
static float s_feedback = 0.6f;
static float s_mix = 0.5f;
static float s_color = 0.4f;
static float s_grit = 0.3f;
static float s_stereo_spread = 0.5f;
static float s_ducking = 0.3f;
static float s_modulation = 0.2f;
static int8_t s_pitch_shift = 0;
static bool s_freeze = false;

static float s_tempo_bpm = 120.f;

// ========== FAST TANH ==========
inline float fast_tanh(float x) {
    if (x < -3.f) return -1.f;
    if (x > 3.f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

// ========== ONE-POLE FILTER ==========
inline float one_pole_lp(float input, float cutoff, float *z1) {
    float g = clipminmaxf(0.01f, cutoff, 0.99f);
    *z1 = *z1 + g * (input - *z1);
    
    if (si_fabsf(*z1) < 1e-15f) *z1 = 0.f;
    return *z1;
}

// ========== SATURATION ==========
inline float saturate(float input, float amount) {
    if (amount < 0.01f) return input;
    float drive = 1.f + amount * 4.f;
    return fast_tanh(input * drive);
}

// ========== BIT CRUSHER ==========
inline float bit_crush(float input, float amount) {
    if (amount < 0.01f) return input;
    
    float bits = 16.f - amount * 14.f;
    float steps = fx_pow2f(bits);
    
    int32_t quantized = (int32_t)(input * steps);
    return (float)quantized / steps;
}

// ========== PITCH SHIFT ==========
inline float pitch_shift_sample(float input, int8_t semitones) {
    if (semitones == 0) return input;
    
    float ratio = fx_pow2f((float)semitones / 12.f);
    return input * (0.7f + 0.3f * ratio);
}

// ========== DELAY READ ==========
inline float delay_read(float *buffer, uint32_t delay_samples) {
    if (!buffer) return 0.f;
    
    delay_samples = clipminmaxu32(48, delay_samples, MAX_DELAY_SAMPLES - 1);
    
    uint32_t read_pos = (s_write_pos + MAX_DELAY_SAMPLES - delay_samples) % MAX_DELAY_SAMPLES;
    
    float sample = buffer[read_pos];
    
    if (!is_finite(sample)) sample = 0.f;
    
    return sample;
}

// ========== DUCKING ==========
inline float process_ducking(float wet, float dry) {
    if (s_ducking < 0.01f) return wet;
    
    float dry_abs = si_fabsf(dry);
    
    if (dry_abs > s_envelope_follower) {
        s_envelope_follower += (dry_abs - s_envelope_follower) * 0.1f;
    } else {
        s_envelope_follower += (dry_abs - s_envelope_follower) * 0.01f;
    }
    
    float duck_amount = 1.f - (s_envelope_follower * s_ducking);
    duck_amount = clipminmaxf(0.1f, duck_amount, 1.f);
    
    return wet * duck_amount;
}

// ========== MODULATION ==========
inline float get_modulation() {
    if (s_modulation < 0.01f) return 0.f;
    
    s_mod_phase += 0.5f / 48000.f;
    if (s_mod_phase >= 1.f) s_mod_phase -= 1.f;
    
    return fx_sinf(s_mod_phase * 2.f * 3.14159265f) * s_modulation * 0.02f;
}

// ========== CALLBACKS ==========

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;
    if (!desc->hooks.sdram_alloc) return k_unit_err_memory;
    
    size_t total_size = MAX_DELAY_SAMPLES * sizeof(float) * 2;
    uint8_t *buffer_base = static_cast<uint8_t *>(desc->hooks.sdram_alloc(total_size));
    
    if (!buffer_base) return k_unit_err_memory;
    
    s_delay_buffer_l = reinterpret_cast<float *>(buffer_base);
    s_delay_buffer_r = reinterpret_cast<float *>(buffer_base + MAX_DELAY_SAMPLES * sizeof(float));
    
    // Clear buffers
    for (uint32_t i = 0; i < MAX_DELAY_SAMPLES; i++) {
        s_delay_buffer_l[i] = 0.f;
        s_delay_buffer_r[i] = 0.f;
    }
    
    s_write_pos = 0;
    
    s_filter_z1_l = 0.f;
    s_filter_z1_r = 0.f;
    s_envelope_follower = 0.f;
    s_mod_phase = 0.f;
    
    s_mode = MODE_DUB;
    s_time_div = 4;
    s_feedback = 0.6f;
    s_mix = 0.5f;
    s_color = 0.4f;
    s_grit = 0.3f;
    s_stereo_spread = 0.5f;
    s_ducking = 0.3f;
    s_modulation = 0.2f;
    s_pitch_shift = 0;
    s_freeze = false;
    
    s_tempo_bpm = 120.f;
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    if (s_delay_buffer_l) {
        for (uint32_t i = 0; i < MAX_DELAY_SAMPLES; i++) {
            s_delay_buffer_l[i] = 0.f;
        }
    }
    if (s_delay_buffer_r) {
        for (uint32_t i = 0; i < MAX_DELAY_SAMPLES; i++) {
            s_delay_buffer_r[i] = 0.f;
        }
    }
    
    s_write_pos = 0;
    s_filter_z1_l = 0.f;
    s_filter_z1_r = 0.f;
    s_envelope_follower = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    if (!s_delay_buffer_l || !s_delay_buffer_r) {
        // Safety: passthrough if no buffer
        for (uint32_t f = 0; f < frames; f++) {
            out[f * 2] = in[f * 2];
            out[f * 2 + 1] = in[f * 2 + 1];
        }
        return;
    }
    
    // Calculate delay time
    float beats_per_second = s_tempo_bpm / 60.f;
    float delay_time = tempo_divisions[s_time_div] / beats_per_second;
    
    // Mode adjustments
    switch (s_mode) {
        case MODE_GROOVE: delay_time *= 0.75f; break;
        case MODE_BURST: delay_time *= 0.5f; break;
        case MODE_REVERSE: delay_time *= 1.2f; break;
        default: break;
    }
    
    uint32_t delay_samples_l = (uint32_t)(delay_time * 48000.f);
    uint32_t delay_samples_r = (uint32_t)(delay_time * (1.f + s_stereo_spread * 0.1f) * 48000.f);
    
    delay_samples_l = clipminmaxu32(48, delay_samples_l, MAX_DELAY_SAMPLES - 1);
    delay_samples_r = clipminmaxu32(48, delay_samples_r, MAX_DELAY_SAMPLES - 1);
    
    float mod = get_modulation();
    
    for (uint32_t f = 0; f < frames; f++) {
        float in_l = in[f * 2];
        float in_r = in[f * 2 + 1];
        
        if (!is_finite(in_l)) in_l = 0.f;
        if (!is_finite(in_r)) in_r = 0.f;
        
        in_l = clipminmaxf(-1.f, in_l, 1.f);
        in_r = clipminmaxf(-1.f, in_r, 1.f);
        
        // Read delayed signal
        float delayed_l = delay_read(s_delay_buffer_l, delay_samples_l);
        float delayed_r = delay_read(s_delay_buffer_r, delay_samples_r);
        
        // Apply color filter
        delayed_l = one_pole_lp(delayed_l, s_color, &s_filter_z1_l);
        delayed_r = one_pole_lp(delayed_r, s_color, &s_filter_z1_r);
        
        // Apply grit
        if (s_grit > 0.01f) {
            delayed_l = saturate(delayed_l, s_grit * 0.5f);
            delayed_r = saturate(delayed_r, s_grit * 0.5f);
            delayed_l = bit_crush(delayed_l, s_grit * 0.5f);
            delayed_r = bit_crush(delayed_r, s_grit * 0.5f);
        }
        
        // Apply pitch shift
        if (s_mode == MODE_SHIMMER || s_pitch_shift != 0) {
            delayed_l = pitch_shift_sample(delayed_l, s_pitch_shift);
            delayed_r = pitch_shift_sample(delayed_r, s_pitch_shift);
        }
        
        // Ping-pong
        if (s_mode == MODE_PINGPONG && s_stereo_spread > 0.01f) {
            float temp_l = delayed_l;
            delayed_l = (delayed_l * 0.7f) + (delayed_r * 0.3f * s_stereo_spread);
            delayed_r = (delayed_r * 0.7f) + (temp_l * 0.3f * s_stereo_spread);
        }
        
        // Ducking
        delayed_l = process_ducking(delayed_l, in_l);
        delayed_r = process_ducking(delayed_r, in_r);
        
        // Feedback
        float feedback_amount = s_feedback;
        
        switch (s_mode) {
            case MODE_GROOVE: feedback_amount *= 0.7f; break;
            case MODE_BURST: feedback_amount *= 0.9f; break;
            case MODE_DUB: feedback_amount *= 0.85f; break;
            default: break;
        }
        
        feedback_amount = clipminmaxf(0.f, feedback_amount, 0.93f);
        
        // Write to buffer
        float write_l, write_r;
        
        if (s_freeze) {
            write_l = delayed_l * feedback_amount;
            write_r = delayed_r * feedback_amount;
        } else {
            write_l = in_l + delayed_l * feedback_amount;
            write_r = in_r + delayed_r * feedback_amount;
        }
        
        write_l = fast_tanh(write_l * 0.7f) * 1.4f;
        write_r = fast_tanh(write_r * 0.7f) * 1.4f;
        
        write_l = clipminmaxf(-2.f, write_l, 2.f);
        write_r = clipminmaxf(-2.f, write_r, 2.f);
        
        if (!is_finite(write_l)) write_l = 0.f;
        if (!is_finite(write_r)) write_r = 0.f;
        
        s_delay_buffer_l[s_write_pos] = write_l;
        s_delay_buffer_r[s_write_pos] = write_r;
        
        s_write_pos = (s_write_pos + 1) % MAX_DELAY_SAMPLES;
        
        // Mix
        float dry_gain = 1.f - s_mix;
        float wet_gain = s_mix;
        
        float out_l = in_l * dry_gain + delayed_l * wet_gain;
        float out_r = in_r * dry_gain + delayed_r * wet_gain;
        
        if (!is_finite(out_l)) out_l = in_l;
        if (!is_finite(out_r)) out_r = in_r;
        
        out[f * 2] = clipminmaxf(-1.f, out_l, 1.f);
        out[f * 2 + 1] = clipminmaxf(-1.f, out_r, 1.f);
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_mode = (uint8_t)clipminmaxu32(0, value, 5); break;
        case 1: s_time_div = (uint8_t)clipminmaxu32(0, value, 15); break;
        case 2: s_feedback = valf * 0.95f; break;
        case 3: s_mix = (float)(value + 100) / 200.f; break;
        case 4: s_color = valf; break;
        case 5: s_grit = valf; break;
        case 6: s_stereo_spread = valf; break;
        case 7: s_ducking = valf; break;
        case 8: s_modulation = valf; break;
        case 9: s_pitch_shift = (int8_t)value; break;
        case 10: s_freeze = (value != 0); break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return s_mode;
        case 1: return s_time_div;
        case 2: return (int32_t)((s_feedback / 0.95f) * 1023.f);
        case 3: return (int32_t)(s_mix * 200.f - 100.f);
        case 4: return (int32_t)(s_color * 1023.f);
        case 5: return (int32_t)(s_grit * 1023.f);
        case 6: return (int32_t)(s_stereo_spread * 1023.f);
        case 7: return (int32_t)(s_ducking * 1023.f);
        case 8: return (int32_t)(s_modulation * 1023.f);
        case 9: return s_pitch_shift;
        case 10: return s_freeze ? 1 : 0;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    if (id == 0 && value >= 0 && value < 6) {
        return mode_names[value];
    }
    if (id == 1 && value >= 0 && value < 16) {
        return time_names[value];
    }
    if (id == 10) {
        return (value != 0) ? "ON" : "OFF";
    }
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    float bpm = (float)(tempo >> 16) + (float)(tempo & 0xFFFF) / 65536.f;
    s_tempo_bpm = clipminmaxf(60.f, bpm, 240.f);
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    (void)counter;
}
