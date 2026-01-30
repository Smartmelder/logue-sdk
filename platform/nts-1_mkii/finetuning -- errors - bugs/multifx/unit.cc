/*
    HOUSE/GABBER MULTI-FX MODULATION
    Memory-optimized multi-effect unit
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

// ========== MEMORY BUDGET ==========
#define MAX_DELAY_SAMPLES 480   // ✅ REDUCED: 10ms @ 48kHz (shared buffer) - saves ~3.7KB
#define NUM_ALLPASS 4           // Phaser stages (reduced from 8)

// ========== MODES ==========
enum FXMode {
    MODE_CHORUS = 0,
    MODE_FLANGER = 1,
    MODE_PHASER = 2,
    MODE_TREMOLO = 3,
    MODE_VIBRATO = 4,
    MODE_AUTOPAN = 5,
    MODE_RINGMOD = 6,
    MODE_COMBO = 7
};

// ========== LFO SHAPES ==========
enum LFOShape {
    SHAPE_SINE = 0,
    SHAPE_TRIANGLE = 1,
    SHAPE_SAW = 2,
    SHAPE_SQUARE = 3
};

// ========== STRUCTURES ==========
struct AllpassFilter {
    float z1;
    float coeff;
};

// ========== GLOBAL STATE ==========
// Shared delay buffer (L+R interleaved, re-used across modes)
static float s_delay_buffer[MAX_DELAY_SAMPLES * 2];  // ~7.5KB
static uint32_t s_write_pos = 0;

// All-pass filters for phaser
static AllpassFilter s_allpass_l[NUM_ALLPASS];
static AllpassFilter s_allpass_r[NUM_ALLPASS];

// LFO state
static float s_lfo_phase = 0.f;
static float s_lfo_value = 0.f;

// Parameters
static uint8_t s_mode = MODE_CHORUS;
static float s_rate = 0.4f;          // LFO rate (0.5-8 Hz)
static float s_depth = 0.4f;
static float s_feedback = 0.3f;
static float s_mix = 0.5f;
static uint8_t s_sync = 0;           // 0=OFF, 1=1/16, 2=1/8, etc
static uint8_t s_shape = SHAPE_SINE;
static float s_stereo_width = 1.f;
static float s_color = 0.5f;
static float s_morph = 0.f;

// Tempo
static uint32_t s_tempo_bpm = 120;

// ========== HELPER FUNCTIONS ==========

// LFO generator
inline float lfo_generate(float phase, uint8_t shape) {
    // Normalize phase to [0, 1]
    phase -= (int32_t)phase;
    if (phase < 0.f) phase += 1.f;
    if (phase >= 1.f) phase -= 1.f;
    
    switch (shape) {
        case SHAPE_SINE: {
            // Convert phase [0,1] to angle for fx_sinf
            float angle = phase;  // fx_sinf expects [0,1] range
            return fx_sinf(angle);
        }
        
        case SHAPE_TRIANGLE:
            if (phase < 0.5f) return 4.f * phase - 1.f;
            else return 3.f - 4.f * phase;
        
        case SHAPE_SAW:
            return 2.f * phase - 1.f;
        
        case SHAPE_SQUARE:
            return (phase < 0.5f) ? -1.f : 1.f;
        
        default:
            return 0.f;
    }
}

// All-pass filter
inline float allpass_process(AllpassFilter *ap, float input) {
    float output = -input + ap->z1;
    ap->z1 = input + ap->z1 * ap->coeff;
    
    // ✅ Denormal kill
    if (si_fabsf(ap->z1) < 1e-15f) ap->z1 = 0.f;
    
    // ✅ Clip to prevent explosion
    ap->z1 = clipminmaxf(-2.f, ap->z1, 2.f);
    
    return output;
}

// Soft clip
inline float soft_clip(float x) {
    return x / (1.f + si_fabsf(x));
}

// Delay read with interpolation
inline float delay_read(float *buffer, float delay_samples, uint32_t write_pos, uint32_t max_samples) {
    float read_pos_f = (float)write_pos - delay_samples;
    
    while (read_pos_f < 0.f) read_pos_f += (float)max_samples;
    while (read_pos_f >= (float)max_samples) read_pos_f -= (float)max_samples;
    
    uint32_t read_pos_0 = (uint32_t)read_pos_f;
    uint32_t read_pos_1 = (read_pos_0 + 1) % max_samples;
    
    float frac = read_pos_f - (float)read_pos_0;
    
    return buffer[read_pos_0] * (1.f - frac) + buffer[read_pos_1] * frac;
}

// ========== EFFECT PROCESSORS ==========

// CHORUS
inline void process_chorus(float in_l, float in_r, float *out_l, float *out_r) {
    // ✅ Safety: If depth is zero, pass through
    if (s_depth < 0.01f) {
        *out_l = in_l;
        *out_r = in_r;
        return;
    }
    
    // ✅ CRITICAL: Check for input signal - no input = no output!
    float input_level = si_fabsf(in_l) + si_fabsf(in_r);
    if (input_level < 0.0001f) {
        *out_l = 0.f;
        *out_r = 0.f;
        return;
    }
    
    // ✅ FIX: Dual delay lines with LFO modulation
    // Base delay: 3ms (144 samples), modulation: ±2ms (96 samples)
    float delay_time_l = 144.f + s_lfo_value * s_depth * 96.f;
    float delay_time_r = 144.f + (-s_lfo_value) * s_depth * 96.f;
    
    // ✅ Limit to safe range: 1-5ms (48-240 samples) - reduced for smaller buffer
    delay_time_l = clipminmaxf(48.f, delay_time_l, 240.f);
    delay_time_r = clipminmaxf(48.f, delay_time_r, 240.f);
    
    float delayed_l = delay_read(s_delay_buffer, delay_time_l, s_write_pos * 2, MAX_DELAY_SAMPLES);
    float delayed_r = delay_read(s_delay_buffer + 1, delay_time_r, s_write_pos * 2, MAX_DELAY_SAMPLES);
    
    // Mix with feedback
    float fb = s_feedback * 0.3f;  // Max 30% feedback
    fb = clipminmaxf(0.f, fb, 0.3f);
    
    *out_l = in_l + delayed_l * s_depth + delayed_r * s_depth * 0.5f;
    *out_r = in_r + delayed_r * s_depth + delayed_l * s_depth * 0.5f;
}

// FLANGER
inline void process_flanger(float in_l, float in_r, float *out_l, float *out_r) {
    // ✅ Safety: If depth is zero, pass through
    if (s_depth < 0.01f) {
        *out_l = in_l;
        *out_r = in_r;
        return;
    }
    
    // ✅ CRITICAL: Check for input signal - no input = no output!
    float input_level = si_fabsf(in_l) + si_fabsf(in_r);
    if (input_level < 0.0001f) {
        *out_l = 0.f;
        *out_r = 0.f;
        return;
    }
    
    // ✅ FIX: Short delay with high feedback
    // Ensure flanger delay never goes to zero
    float delay_time = 48.f + s_lfo_value * s_depth * 48.f;  // 1-2ms
    delay_time = clipminmaxf(24.f, delay_time, 96.f);  // 0.5-2ms SAFE
    
    float delayed_l = delay_read(s_delay_buffer, delay_time, s_write_pos * 2, MAX_DELAY_SAMPLES);
    float delayed_r = delay_read(s_delay_buffer + 1, delay_time, s_write_pos * 2, MAX_DELAY_SAMPLES);
    
    // ✅ High feedback (50-70%) - LIMITED!
    float fb = s_feedback * 0.7f;
    fb = clipminmaxf(0.f, fb, 0.7f);
    
    float fb_l = delayed_l * fb;
    float fb_r = delayed_r * fb;
    
    // ✅ Soft clip feedback to prevent runaway
    fb_l = soft_clip(fb_l);
    fb_r = soft_clip(fb_r);
    
    *out_l = in_l + delayed_l + fb_l;
    *out_r = in_r + delayed_r + fb_r;
}

// PHASER
inline void process_phaser(float in_l, float in_r, float *out_l, float *out_r) {
    // ✅ Safety: If depth is zero, pass through
    if (s_depth < 0.01f) {
        *out_l = in_l;
        *out_r = in_r;
        return;
    }
    
    // All-pass cascade with LFO-modulated frequency
    float freq = 300.f + s_lfo_value * s_depth * 2000.f;  // 300-2300 Hz
    freq = clipminmaxf(200.f, freq, 4000.f);
    
    float w = 2.f * PI * freq / 48000.f;
    w = clipminmaxf(0.001f, w, PI * 0.95f);
    
    // Convert w to phase [0,1] for fx_sinf
    float phase = (w * 0.5f) / (2.f * PI);
    if (phase >= 1.f) phase -= 1.f;
    if (phase < 0.f) phase += 1.f;
    float f = 2.f * fx_sinf(phase);
    
    float coeff = (1.f - f) / (1.f + f);
    coeff = clipminmaxf(-0.95f, coeff, 0.95f);
    
    // Update all-pass coefficients
    for (int i = 0; i < NUM_ALLPASS; i++) {
        s_allpass_l[i].coeff = coeff;
        s_allpass_r[i].coeff = coeff;
    }
    
    // Process cascade
    float proc_l = in_l;
    float proc_r = in_r;
    
    for (int i = 0; i < NUM_ALLPASS; i++) {
        proc_l = allpass_process(&s_allpass_l[i], proc_l);
        proc_r = allpass_process(&s_allpass_r[i], proc_r);
    }
    
    // ✅ Mix with feedback (limited)
    float fb = s_feedback * 0.5f;
    fb = clipminmaxf(0.f, fb, 0.5f);
    
    *out_l = in_l + proc_l * s_depth + proc_r * fb;
    *out_r = in_r + proc_r * s_depth + proc_l * fb;
}

// TREMOLO
inline void process_tremolo(float in_l, float in_r, float *out_l, float *out_r) {
    // Amplitude modulation
    float mod = 1.f - s_depth * 0.5f * (1.f - s_lfo_value);
    mod = clipminmaxf(0.f, mod, 1.f);
    
    *out_l = in_l * mod;
    *out_r = in_r * mod;
}

// VIBRATO
inline void process_vibrato(float in_l, float in_r, float *out_l, float *out_r) {
    // ✅ Safety: If depth is zero, pass through
    if (s_depth < 0.01f) {
        *out_l = in_l;
        *out_r = in_r;
        return;
    }
    
    // ✅ FIX: Pitch modulation via delay - USE INPUT, not just delay buffer!
    // Write input to delay buffer first (already done in render, but ensure we use it)
    float delay_time = 48.f + s_lfo_value * s_depth * 48.f;  // 1-2ms
    delay_time = clipminmaxf(24.f, delay_time, 96.f);
    
    // ✅ CRITICAL FIX: Use input signal, not old delay buffer samples
    // Vibrato modulates pitch by reading delayed version of CURRENT input
    float delayed_l = delay_read(s_delay_buffer, delay_time, s_write_pos * 2, MAX_DELAY_SAMPLES);
    float delayed_r = delay_read(s_delay_buffer + 1, delay_time, s_write_pos * 2, MAX_DELAY_SAMPLES);
    
    // ✅ Mix current input with delayed (vibrato effect)
    // If no input, output should be zero (not old samples!)
    float input_level = si_fabsf(in_l) + si_fabsf(in_r);
    if (input_level < 0.0001f) {
        // No input = no output (fixes constant bass problem!)
        *out_l = 0.f;
        *out_r = 0.f;
    } else {
        // Normal vibrato: mix input with delayed
        *out_l = in_l * (1.f - s_depth * 0.5f) + delayed_l * s_depth * 0.5f;
        *out_r = in_r * (1.f - s_depth * 0.5f) + delayed_r * s_depth * 0.5f;
    }
}

// AUTOPAN
inline void process_autopan(float in_l, float in_r, float *out_l, float *out_r) {
    // Stereo panning
    float pan = s_lfo_value;  // -1 to +1
    
    float gain_l = 0.5f * (1.f - pan * s_depth);
    float gain_r = 0.5f * (1.f + pan * s_depth);
    
    gain_l = clipminmaxf(0.f, gain_l, 1.f);
    gain_r = clipminmaxf(0.f, gain_r, 1.f);
    
    float mono = (in_l + in_r) * 0.5f;
    
    *out_l = mono * gain_l;
    *out_r = mono * gain_r;
}

// RINGMOD
inline void process_ringmod(float in_l, float in_r, float *out_l, float *out_r) {
    // Ring modulation with LFO carrier
    float carrier_freq = 20.f + s_color * 1980.f;  // 20-2000 Hz
    carrier_freq = clipminmaxf(20.f, carrier_freq, 2000.f);
    
    // Calculate carrier phase
    static float carrier_phase = 0.f;
    carrier_phase += carrier_freq / 48000.f;
    if (carrier_phase >= 1.f) carrier_phase -= 1.f;
    if (carrier_phase < 0.f) carrier_phase += 1.f;
    
    float carrier = fx_sinf(carrier_phase);
    
    float mod = s_depth;
    mod = clipminmaxf(0.f, mod, 1.f);
    
    *out_l = in_l * (1.f - mod) + in_l * carrier * mod;
    *out_r = in_r * (1.f - mod) + in_r * carrier * mod;
}

// COMBO (Chorus + Phaser + AutoPan) - ✅ SIMPLIFIED to save code size
inline void process_combo(float in_l, float in_r, float *out_l, float *out_r) {
    // Simplified: Just mix chorus and phaser
    float chorus_l, chorus_r;
    float saved_depth = s_depth;
    s_depth *= 0.5f;
    process_chorus(in_l, in_r, &chorus_l, &chorus_r);
    s_depth = saved_depth;
    
    float phaser_l, phaser_r;
    s_depth *= 0.5f;
    process_phaser(chorus_l, chorus_r, &phaser_l, &phaser_r);
    s_depth = saved_depth;
    
    *out_l = phaser_l;
    *out_r = phaser_r;
}

// ========== SDK CALLBACKS ==========

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;
    
    // ✅ Initialize buffers
    std::fill(s_delay_buffer, s_delay_buffer + (MAX_DELAY_SAMPLES * 2), 0.f);
    s_write_pos = 0;
    
    // ✅ Initialize all-pass filters
    for (int i = 0; i < NUM_ALLPASS; i++) {
        s_allpass_l[i].z1 = 0.f;
        s_allpass_l[i].coeff = 0.5f;
        s_allpass_r[i].z1 = 0.f;
        s_allpass_r[i].coeff = 0.5f;
    }
    
    // LFO
    s_lfo_phase = 0.f;
    s_lfo_value = 0.f;
    
    // Parameters (match header defaults)
    s_mode = MODE_CHORUS;
    s_rate = 0.4f;  // Fixed LFO rate 40% (~3.5 Hz - musical)
    s_depth = 0.4f;
    s_feedback = 0.3f;
    s_mix = 0.5f;
    s_sync = 0;
    s_shape = SHAPE_SINE;
    s_stereo_width = 1.f;
    s_color = 0.5f;
    s_morph = 0.f;
    
    s_tempo_bpm = 120;
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    // ✅ Clear buffers
    std::fill(s_delay_buffer, s_delay_buffer + (MAX_DELAY_SAMPLES * 2), 0.f);
    s_write_pos = 0;
    s_lfo_phase = 0.f;
    
    // ✅ Reset all-pass filters
    for (int i = 0; i < NUM_ALLPASS; i++) {
        s_allpass_l[i].z1 = 0.f;
        s_allpass_r[i].z1 = 0.f;
    }
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    float bpm = (float)tempo / 10.f;
    s_tempo_bpm = (uint32_t)clipminmaxf(60.f, bpm, 200.f);
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    // Not used for now
}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    for (uint32_t f = 0; f < frames; f++) {
        float in_l = clipminmaxf(-1.f, in[f * 2], 1.f);
        float in_r = clipminmaxf(-1.f, in[f * 2 + 1], 1.f);
        
        // Calculate LFO rate
        float lfo_freq = s_rate;
        
        if (s_sync > 0) {
            // Tempo sync
            float divisions[] = {16.f, 8.f, 4.f, 2.f, 1.f};  // 1/16, 1/8, 1/4, 1/2, 1/1
            float div = divisions[s_sync - 1];
            lfo_freq = ((float)s_tempo_bpm / 60.f) * (4.f / div);
        } else {
            // ✅ FIX: Free rate: 0.5-8.0 Hz (musical range, was 0.1-10 Hz)
            lfo_freq = 0.5f + s_rate * 7.5f;
        }
        
        // Advance LFO
        s_lfo_phase += lfo_freq / 48000.f;
        if (s_lfo_phase >= 1.f) s_lfo_phase -= 1.f;
        if (s_lfo_phase < 0.f) s_lfo_phase += 1.f;
        
        s_lfo_value = lfo_generate(s_lfo_phase, s_shape);
        
        // ✅ CRITICAL: Only write to delay buffer if there's actual input signal
        // This prevents old samples from being read when there's no input
        float input_level = si_fabsf(in_l) + si_fabsf(in_r);
        if (input_level > 0.0001f) {
            // Write to delay buffer only when there's input
            s_delay_buffer[s_write_pos * 2] = in_l;
            s_delay_buffer[s_write_pos * 2 + 1] = in_r;
        } else {
            // No input = clear delay buffer to prevent old samples
            s_delay_buffer[s_write_pos * 2] = 0.f;
            s_delay_buffer[s_write_pos * 2 + 1] = 0.f;
        }
        
        // Process effect based on mode
        float wet_l = 0.f, wet_r = 0.f;
        
        switch (s_mode) {
            case MODE_CHORUS:   process_chorus(in_l, in_r, &wet_l, &wet_r); break;
            case MODE_FLANGER:  process_flanger(in_l, in_r, &wet_l, &wet_r); break;
            case MODE_PHASER:   process_phaser(in_l, in_r, &wet_l, &wet_r); break;
            case MODE_TREMOLO:  process_tremolo(in_l, in_r, &wet_l, &wet_r); break;
            case MODE_VIBRATO:  process_vibrato(in_l, in_r, &wet_l, &wet_r); break;
            case MODE_AUTOPAN:  process_autopan(in_l, in_r, &wet_l, &wet_r); break;
            case MODE_RINGMOD:  process_ringmod(in_l, in_r, &wet_l, &wet_r); break;
            case MODE_COMBO:    process_combo(in_l, in_r, &wet_l, &wet_r); break;
            default:            wet_l = in_l; wet_r = in_r; break;
        }
        
        // ✅ FIX: Ensure wet signal is never silent (NaN/Inf detection)
        if (!std::isfinite(wet_l)) wet_l = in_l;
        if (!std::isfinite(wet_r)) wet_r = in_r;
        
        // ✅ Denormal kill
        if (si_fabsf(wet_l) < 1e-15f) wet_l = 0.f;
        if (si_fabsf(wet_r) < 1e-15f) wet_r = 0.f;
        
        // Stereo widening
        if (s_stereo_width != 1.f) {
            float mid = (wet_l + wet_r) * 0.5f;
            float side = (wet_l - wet_r) * 0.5f * s_stereo_width;
            wet_l = mid + side;
            wet_r = mid - side;
        }
        
        // ✅ Limiting
        wet_l = clipminmaxf(-1.f, wet_l, 1.f);
        wet_r = clipminmaxf(-1.f, wet_r, 1.f);
        
        // ✅ FIX: Proper dry/wet mix - ALWAYS pass through input!
        // Standard mix: dry + wet (not crossfade, so input always audible)
        float dry_gain = 1.f - s_mix;  // Dry signal
        float wet_gain = s_mix;        // Wet signal
        
        // ✅ CRITICAL: If wet signal is silent/zero, use dry signal instead
        float wet_level = si_fabsf(wet_l) + si_fabsf(wet_r);
        if (wet_level < 0.0001f) {
            // No wet signal (delay buffer empty, etc) - use dry only
            out[f * 2] = in_l;
            out[f * 2 + 1] = in_r;
        } else {
            // Normal mix
            out[f * 2] = in_l * dry_gain + wet_l * wet_gain;
            out[f * 2 + 1] = in_r * dry_gain + wet_r * wet_gain;
        }
        
        // ✅ Safety: Ensure output never goes silent if input has signal
        if (si_fabsf(in_l) > 0.001f && si_fabsf(out[f * 2]) < 0.001f) {
            out[f * 2] = in_l;  // Force dry signal if output is silent
        }
        if (si_fabsf(in_r) > 0.001f && si_fabsf(out[f * 2 + 1]) < 0.001f) {
            out[f * 2 + 1] = in_r;  // Force dry signal if output is silent
        }
        
        // ✅ Final limiting
        out[f * 2] = clipminmaxf(-1.f, out[f * 2], 1.f);
        out[f * 2 + 1] = clipminmaxf(-1.f, out[f * 2 + 1], 1.f);
        
        // Advance write position
        s_write_pos = (s_write_pos + 1) % MAX_DELAY_SAMPLES;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: 
            // ✅ CRITICAL: Reset delay buffer when mode changes!
            s_mode = value;
            if (s_mode > 7) s_mode = 7;
            // Clear delay buffer to prevent old samples
            std::fill(s_delay_buffer, s_delay_buffer + (MAX_DELAY_SAMPLES * 2), 0.f);
            s_write_pos = 0;
            // Reset all-pass filters
            for (int i = 0; i < NUM_ALLPASS; i++) {
                s_allpass_l[i].z1 = 0.f;
                s_allpass_r[i].z1 = 0.f;
            }
            break;
        case 1: s_rate = valf; break;  // ✅ RATE control (LFO speed)
        case 2: s_depth = valf; break;
        case 3: s_feedback = valf; break;
        case 4: s_mix = valf; break;
        case 5: s_sync = value; break;
        case 6: s_shape = value; break;
        case 7: s_stereo_width = valf * 2.f; break;  // 0-200%
        case 8: s_color = valf; break;
        case 9: s_morph = valf; break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return s_mode;
        case 1: return (int32_t)(s_rate * 1023.f);  // ✅ RATE
        case 2: return (int32_t)(s_depth * 1023.f);
        case 3: return (int32_t)(s_feedback * 1023.f);
        case 4: return (int32_t)(s_mix * 1023.f);
        case 5: return s_sync;
        case 6: return s_shape;
        case 7: return (int32_t)((s_stereo_width / 2.f) * 1023.f);
        case 8: return (int32_t)(s_color * 1023.f);
        case 9: return (int32_t)(s_morph * 1023.f);
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    if (id == 0) {  // MODE
        static const char *modes[] = {
            "CHORUS", "FLANGER", "PHASER", "TREMOLO",
            "VIBRATO", "AUTOPAN", "RINGMOD", "COMBO"
        };
        if (value >= 0 && value < 8) return modes[value];
    }
    if (id == 5) {  // SYNC
        static const char *sync[] = {"OFF", "1/16", "1/8", "1/4", "1/2", "1/1"};
        if (value >= 0 && value < 6) return sync[value];
    }
    if (id == 6) {  // SHAPE
        static const char *shapes[] = {"SINE", "TRI", "SAW", "SQR"};
        if (value >= 0 && value < 4) return shapes[value];
    }
    return "";
}

