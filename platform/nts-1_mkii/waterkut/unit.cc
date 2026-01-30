/*
    WATERKUT - Raindrop Delay Effect V2 (STANDALONE FIXED!)
    
    CRITICAL FIX:
    - No external dependencies (no waterkut.h, no Processor class)
    - All code in single file
    - Proper initialization
    - No uninitialized memory
    - Safe buffer handling
    
    10 parallel delay lines with chaos and modulation
*/

#include "unit_delfx.h"
#include "fx_api.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "utils/buffer_ops.h"

#define NUM_DELAY_LINES 10
#define MAX_DELAY_SAMPLES 144000  // 3 seconds @ 48kHz

// ========== NaN/Inf CHECK MACRO (FIXED!) ==========
// ✅ FIX: Correct NaN detection (NaN != NaN is TRUE)
// Note: si_isfinite() not available for delfx, using correct macro instead
#define is_finite(x) ((x) != (x) ? false : ((x) <= 1e10f && (x) >= -1e10f))

// ========== DELAY LINE STRUCTURE ==========

struct DelayLine {
    float *buffer_l;
    float *buffer_r;
    uint32_t write_pos;
    uint32_t delay_samples;
    float feedback;
    float tone_z1_l;
    float tone_z1_r;
};

static DelayLine s_delay_lines[NUM_DELAY_LINES];
static float *s_delay_buffer_base = nullptr;

// ========== MODULATION ==========

static float s_mod_phase = 0.f;

// ========== RANDOM GENERATOR ==========

static uint32_t s_rand_state = 12345;

inline float random_float() {
    s_rand_state ^= s_rand_state << 13;
    s_rand_state ^= s_rand_state >> 17;
    s_rand_state ^= s_rand_state << 5;
    return (float)(s_rand_state % 10000) / 10000.f;
}

// ========== PARAMETERS ==========

static float s_time = 0.8f;
static float s_depth = 0.75f;
static float s_mix = 0.5f;
static float s_chaos = 0.5f;
static float s_mod_intensity = 0.3f;
static float s_mod_rate = 0.1f;
static float s_tone = 0.5f;
static float s_stereo_width = 0.75f;
static uint8_t s_lines = 10;
static float s_diffusion = 0.4f;
static bool s_freeze = false;

static float s_tempo_bpm = 120.f;

// ========== DELAY LINE PROCESSOR ==========

inline void process_delay_line(DelayLine *line, float in_l, float in_r, 
                               float *out_l, float *out_r, bool active) {
    if (!active || !line->buffer_l || !line->buffer_r) {
        *out_l = 0.f;
        *out_r = 0.f;
        return;
    }
    
    // Read delayed signal
    uint32_t read_pos = (line->write_pos + MAX_DELAY_SAMPLES - line->delay_samples) % MAX_DELAY_SAMPLES;
    
    float delayed_l = line->buffer_l[read_pos];
    float delayed_r = line->buffer_r[read_pos];
    
    // ✅ FIX: Use correct NaN check!
    if (!is_finite(delayed_l)) delayed_l = 0.f;
    if (!is_finite(delayed_r)) delayed_r = 0.f;
    
    // Apply tone filter
    float tone_coeff = 0.3f + s_tone * 0.4f;
    line->tone_z1_l += tone_coeff * (delayed_l - line->tone_z1_l);
    line->tone_z1_r += tone_coeff * (delayed_r - line->tone_z1_r);
    
    delayed_l = line->tone_z1_l;
    delayed_r = line->tone_z1_r;
    
    // Denormal kill
    if (si_fabsf(line->tone_z1_l) < 1e-15f) line->tone_z1_l = 0.f;
    if (si_fabsf(line->tone_z1_r) < 1e-15f) line->tone_z1_r = 0.f;
    
    // Write to buffer
    float write_l, write_r;
    
    if (s_freeze) {
        // Freeze mode: don't write new input
        write_l = delayed_l * line->feedback;
        write_r = delayed_r * line->feedback;
    } else {
        // Normal mode: mix input with feedback
        write_l = in_l + delayed_l * line->feedback;
        write_r = in_r + delayed_r * line->feedback;
    }
    
    // ✅ FIX: Clip before write
    write_l = clipminmaxf(-2.f, write_l, 2.f);
    write_r = clipminmaxf(-2.f, write_r, 2.f);
    
    // ✅ FIX: Use correct NaN check!
    if (!is_finite(write_l)) write_l = 0.f;
    if (!is_finite(write_r)) write_r = 0.f;
    
    line->buffer_l[line->write_pos] = write_l;
    line->buffer_r[line->write_pos] = write_r;
    
    line->write_pos = (line->write_pos + 1) % MAX_DELAY_SAMPLES;
    
    *out_l = delayed_l;
    *out_r = delayed_r;
}

// ========== MODULATION ==========

inline float get_modulation() {
    if (s_mod_intensity < 0.01f) return 0.f;
    
    // Mod rate: 0.1-10 Hz
    float rate = 0.1f + s_mod_rate * 9.9f;
    s_mod_phase += rate / 48000.f;
    if (s_mod_phase >= 1.f) s_mod_phase -= 1.f;
    
    float lfo = fx_sinf(s_mod_phase * 2.f * 3.14159265f);
    return lfo * s_mod_intensity * 0.1f;
}

// ========== STEREO WIDTH ==========

inline void apply_stereo_width(float *l, float *r) {
    float mid = (*l + *r) * 0.5f;
    float side = (*l - *r) * 0.5f;
    
    // Width: 0-200%
    float width = s_stereo_width * 2.f;
    side *= width;
    
    *l = mid + side;
    *r = mid - side;
}

// ========== UNIT CALLBACKS ==========

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;
    
    if (!desc->hooks.sdram_alloc) return k_unit_err_memory;
    
    // ✅ FIX: Allocate ONE big buffer for all delay lines
    size_t total_size = MAX_DELAY_SAMPLES * sizeof(float) * 2 * NUM_DELAY_LINES;  // L+R per line
    uint8_t *buffer_base = desc->hooks.sdram_alloc(total_size);
    
    if (!buffer_base) return k_unit_err_memory;
    
    s_delay_buffer_base = reinterpret_cast<float *>(buffer_base);
    
    // ✅ FIX: Clear entire buffer
    for (size_t i = 0; i < MAX_DELAY_SAMPLES * 2 * NUM_DELAY_LINES; i++) {
        s_delay_buffer_base[i] = 0.f;
    }
    
    // ✅ FIX: Assign buffer pointers for each delay line
    for (int i = 0; i < NUM_DELAY_LINES; i++) {
        size_t offset = i * MAX_DELAY_SAMPLES * 2;
        s_delay_lines[i].buffer_l = s_delay_buffer_base + offset;
        s_delay_lines[i].buffer_r = s_delay_buffer_base + offset + MAX_DELAY_SAMPLES;
        s_delay_lines[i].write_pos = 0;
        s_delay_lines[i].delay_samples = 24000;  // 0.5s default
        s_delay_lines[i].feedback = 0.5f;
        s_delay_lines[i].tone_z1_l = 0.f;
        s_delay_lines[i].tone_z1_r = 0.f;
    }
    
    s_mod_phase = 0.f;
    
    // Init parameters
    s_time = 0.8f;
    s_depth = 0.75f;
    s_mix = 0.5f;
    s_chaos = 0.5f;
    s_mod_intensity = 0.3f;
    s_mod_rate = 0.1f;
    s_tone = 0.5f;
    s_stereo_width = 0.75f;
    s_lines = 10;
    s_diffusion = 0.4f;
    s_freeze = false;
    
    s_tempo_bpm = 120.f;
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {
    // Nothing to free (SDRAM managed by SDK)
}

__unit_callback void unit_reset() {
    // ✅ FIX: Clear all delay line buffers
    if (s_delay_buffer_base) {
        for (size_t i = 0; i < MAX_DELAY_SAMPLES * 2 * NUM_DELAY_LINES; i++) {
            s_delay_buffer_base[i] = 0.f;
        }
    }
    
    // Reset delay line state
    for (int i = 0; i < NUM_DELAY_LINES; i++) {
        s_delay_lines[i].write_pos = 0;
        s_delay_lines[i].tone_z1_l = 0.f;
        s_delay_lines[i].tone_z1_r = 0.f;
    }
    
    s_mod_phase = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    // ✅ FIX: Safety check
    if (!s_delay_buffer_base) {
        for (uint32_t f = 0; f < frames; f++) {
            out[f * 2] = in[f * 2];
            out[f * 2 + 1] = in[f * 2 + 1];
        }
        return;
    }
    
    // Get modulation
    float mod = get_modulation();
    
    // Update delay line parameters
    for (int i = 0; i < NUM_DELAY_LINES; i++) {
        // Base delay time (scaled by TIME parameter)
        float base_time = 0.1f + s_time * 2.9f;  // 0.1-3.0 seconds
        
        // Add chaos (randomization per line)
        float chaos_offset = (random_float() - 0.5f) * s_chaos * 0.5f;
        float delay_time = base_time + chaos_offset;
        
        // Add modulation
        delay_time *= 1.f + mod;
        
        // Clamp
        delay_time = clipminmaxf(0.01f, delay_time, 3.f);
        
        // Convert to samples
        s_delay_lines[i].delay_samples = (uint32_t)(delay_time * 48000.f);
        s_delay_lines[i].delay_samples = clipminmaxu32(480, s_delay_lines[i].delay_samples, MAX_DELAY_SAMPLES - 1);
        
        // ✅ FIX: Feedback (scaled by DEPTH) - SAFE LIMITING
        s_delay_lines[i].feedback = s_depth * 0.8f;
        s_delay_lines[i].feedback = clipminmaxf(0.f, s_delay_lines[i].feedback, 0.93f);  // SAFE MAX!
    }
    
    for (uint32_t f = 0; f < frames; f++) {
        float in_l = in[f * 2];
        float in_r = in[f * 2 + 1];
        
        // ✅ FIX: Use correct NaN check!
        if (!is_finite(in_l)) in_l = 0.f;
        if (!is_finite(in_r)) in_r = 0.f;
        
        float wet_l = 0.f;
        float wet_r = 0.f;
        
        // Process active delay lines
        uint8_t active_lines = clipminmaxu32(1, s_lines, NUM_DELAY_LINES);
        
        for (int i = 0; i < NUM_DELAY_LINES; i++) {
            float line_out_l, line_out_r;
            bool active = (i < active_lines);
            
            process_delay_line(&s_delay_lines[i], in_l, in_r, &line_out_l, &line_out_r, active);
            
            if (active) {
                wet_l += line_out_l;
                wet_r += line_out_r;
            }
        }
        
        // Average active lines
        if (active_lines > 0) {
            wet_l /= (float)active_lines;
            wet_r /= (float)active_lines;
        }
        
        // Apply diffusion (smoothing)
        static float diff_z1_l = 0.f, diff_z1_r = 0.f;
        if (s_diffusion > 0.01f) {
            float diff_coeff = 0.1f + s_diffusion * 0.4f;
            diff_z1_l += diff_coeff * (wet_l - diff_z1_l);
            diff_z1_r += diff_coeff * (wet_r - diff_z1_r);
            
            wet_l = wet_l * (1.f - s_diffusion) + diff_z1_l * s_diffusion;
            wet_r = wet_r * (1.f - s_diffusion) + diff_z1_r * s_diffusion;
            
            if (si_fabsf(diff_z1_l) < 1e-15f) diff_z1_l = 0.f;
            if (si_fabsf(diff_z1_r) < 1e-15f) diff_z1_r = 0.f;
        }
        
        // Apply stereo width
        apply_stereo_width(&wet_l, &wet_r);
        
        // Mix (0-1 range from -100 to +100)
        float dry_gain = 1.f - s_mix;
        float wet_gain = s_mix;
        
        float out_l = in_l * dry_gain + wet_l * wet_gain;
        float out_r = in_r * dry_gain + wet_r * wet_gain;
        
        // ✅ FIX: Use correct NaN check!
        if (!is_finite(out_l)) out_l = 0.f;
        if (!is_finite(out_r)) out_r = 0.f;
        
        // Output limiting
        out[f * 2] = clipminmaxf(-1.f, out_l, 1.f);
        out[f * 2 + 1] = clipminmaxf(-1.f, out_r, 1.f);
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_time = valf; break;
        case 1: s_depth = valf; break;
        case 2: s_mix = (float)(value + 100) / 200.f; break;  // -100...+100 → 0...1
        case 3: s_chaos = valf; break;
        case 4: s_mod_intensity = valf; break;
        case 5: s_mod_rate = valf; break;
        case 6: s_tone = valf; break;
        case 7: s_stereo_width = valf; break;
        case 8:
            s_lines = (uint8_t)value;
            if (s_lines < 1) s_lines = 1;
            if (s_lines > 10) s_lines = 10;
            break;
        case 9: s_diffusion = valf; break;
        case 10: s_freeze = (value != 0); break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return (int32_t)(s_time * 1023.f);
        case 1: return (int32_t)(s_depth * 1023.f);
        case 2: return (int32_t)(s_mix * 200.f - 100.f);
        case 3: return (int32_t)(s_chaos * 1023.f);
        case 4: return (int32_t)(s_mod_intensity * 1023.f);
        case 5: return (int32_t)(s_mod_rate * 1023.f);
        case 6: return (int32_t)(s_tone * 1023.f);
        case 7: return (int32_t)(s_stereo_width * 1023.f);
        case 8: return s_lines;
        case 9: return (int32_t)(s_diffusion * 1023.f);
        case 10: return s_freeze ? 1 : 0;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    if (id == 8) {
        static char buf[4];
        buf[0] = '0' + (char)value;
        buf[1] = '\0';
        return buf;
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
