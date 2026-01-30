/*
    AUTO-PAN & TREMOLO - Advanced stereo modulation
    
    FEATURES:
    - Auto-pan: LFO-controlled stereo movement
    - Tremolo: Amplitude modulation
    - Stereo width control
    - 8 LFO waveforms (sine, triangle, square, saw, random, etc.)
    - Tempo sync with divisions
    - Phase offset between L/R channels
    - 4 modes: Pan only, Trem only, Pan+Trem, Crossfade
    
    LFO WAVEFORMS:
    0. SINE - Smooth circular motion
    1. TRIANGLE - Linear sweep
    2. SQUARE - Hard gate
    3. SAW UP - Rising sweep
    4. SAW DOWN - Falling sweep
    5. RANDOM - Sample & hold
    6. SMOOTH RANDOM - Interpolated random
    7. CUSTOM - User-definable shape
    
    MODES:
    0. PAN ONLY - Pure stereo panning
    1. TREM ONLY - Pure amplitude modulation
    2. PAN + TREM - Both simultaneously
    3. CROSSFADE - Pan morphs to trem
*/

#include "unit_modfx.h"
#include "utils/float_math.h"
#include "fx_api.h"
#include "osc_api.h"
#include <math.h>

#define LFO_TABLE_SIZE 256  // Reduced from 1024 for memory (effect limit 32KB)

// LFO tables
static float s_lfo_sine[LFO_TABLE_SIZE];
static float s_lfo_triangle[LFO_TABLE_SIZE];
static float s_lfo_square[LFO_TABLE_SIZE];
static float s_lfo_saw_up[LFO_TABLE_SIZE];
static float s_lfo_saw_down[LFO_TABLE_SIZE];

// Random LFO state
static float s_random_value;
static float s_random_target;
static uint32_t s_random_seed;
static float s_last_random_phase;  // Track phase for random updates

// LFO phase
static float s_lfo_phase;

// Parameters
static float s_rate;
static float s_depth;
static float s_stereo_width;
static float s_phase_offset;
static float s_tremolo_amount;
static float s_pan_amount;
static uint8_t s_waveform;
static bool s_tempo_sync;
static uint8_t s_division;
static uint8_t s_mode;

static uint32_t s_sample_counter;

void init_lfo_tables() {
    for (int i = 0; i < LFO_TABLE_SIZE; i++) {
        float phase = (float)i / (float)LFO_TABLE_SIZE;
        
        // Sine
        s_lfo_sine[i] = osc_sinf(phase);  // osc_sinf expects [0,1] phase
        
        // Triangle
        if (phase < 0.5f) {
            s_lfo_triangle[i] = -1.f + 4.f * phase;
        } else {
            s_lfo_triangle[i] = 3.f - 4.f * phase;
        }
        
        // Square
        s_lfo_square[i] = (phase < 0.5f) ? 1.f : -1.f;
        
        // Saw up
        s_lfo_saw_up[i] = -1.f + 2.f * phase;
        
        // Saw down
        s_lfo_saw_down[i] = 1.f - 2.f * phase;
    }
}

inline float lfo_read(float *table, float phase) {
    phase -= (int32_t)phase;
    if (phase < 0.f) phase += 1.f;
    
    float idx_f = phase * (float)(LFO_TABLE_SIZE - 1);
    uint32_t idx0 = (uint32_t)idx_f;
    uint32_t idx1 = (idx0 + 1) % LFO_TABLE_SIZE;
    float frac = idx_f - (float)idx0;
    
    return table[idx0] * (1.f - frac) + table[idx1] * frac;
}

inline float random_lfo(float phase) {
    // Generate new random target when phase wraps or crosses threshold
    // Use phase-based updates for tempo sync compatibility
    float phase_quantized = (float)((int32_t)(phase * 16.f)) / 16.f;  // 16 steps per cycle
    
    // Check if we crossed a threshold (new random value)
    if (phase_quantized != s_last_random_phase) {
        s_last_random_phase = phase_quantized;
        s_random_seed = s_random_seed * 1103515245u + 12345u;
        s_random_target = ((float)(s_random_seed >> 16) / 32768.f) - 1.f;
    }
    
    // Smooth random (interpolate based on phase within quantized step)
    if (s_waveform == 6) {
        float step_phase = (phase * 16.f) - (float)((int32_t)(phase * 16.f));
        s_random_value += (s_random_target - s_random_value) * (0.01f + step_phase * 0.05f);
        return s_random_value;
    }
    
    // Hard random (S&H) - return current target
    return s_random_target;
}

inline float get_lfo_value(float phase, uint8_t waveform) {
    switch (waveform) {
        case 0: return lfo_read(s_lfo_sine, phase);
        case 1: return lfo_read(s_lfo_triangle, phase);
        case 2: return lfo_read(s_lfo_square, phase);
        case 3: return lfo_read(s_lfo_saw_up, phase);
        case 4: return lfo_read(s_lfo_saw_down, phase);
        case 5: return random_lfo(phase);  // Hard random (S&H) - FIXED: now phase-based
        case 6: return random_lfo(phase);  // Smooth random - FIXED: now phase-based
        case 7: {
            // Custom wave (combination)
            float sine = lfo_read(s_lfo_sine, phase);
            float tri = lfo_read(s_lfo_triangle, phase);
            return sine * 0.7f + tri * 0.3f;
        }
        default: return 0.f;
    }
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc)
{
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;

    init_lfo_tables();
    
    s_lfo_phase = 0.f;
    s_random_value = 0.f;
    s_random_target = 0.f;
    s_random_seed = 0x12345678;
    s_last_random_phase = -1.f;  // FIXED: Initialize phase tracker
    
    s_rate = 0.6f;
    s_depth = 0.75f;
    s_stereo_width = 0.5f;
    s_phase_offset = 0.3f;
    s_tremolo_amount = 0.f;
    s_pan_amount = 0.8f;
    s_waveform = 0;
    s_tempo_sync = true;
    s_division = 3;
    s_mode = 0;
    
    s_sample_counter = 0;

    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset()
{
    s_lfo_phase = 0.f;
    s_last_random_phase = -1.f;  // FIXED: Reset phase tracker
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames)
{
    const float *in_ptr = in;
    float *out_ptr = out;
    
    // Calculate LFO rate
    float lfo_rate;
    if (s_tempo_sync) {
        // Tempo sync (120 BPM base = 2 Hz at 1/4)
        float bpm = fx_get_bpmf();
        if (bpm < 60.f) bpm = 120.f;  // Default if no tempo
        
        float beat_rate = bpm / 60.f;  // Beats per second
        lfo_rate = beat_rate / (1 << s_division);
    } else {
        // Free-running (0.1-20 Hz)
        lfo_rate = 0.1f + s_rate * 19.9f;
    }
    
    float lfo_inc = lfo_rate / 48000.f;
    
    for (uint32_t i = 0; i < frames; i++) {
        // Get LFO values
        float lfo_main = get_lfo_value(s_lfo_phase, s_waveform);
        
        // Phase-offset for stereo
        float lfo_phase_r = s_lfo_phase + s_phase_offset;
        if (lfo_phase_r >= 1.f) lfo_phase_r -= 1.f;
        float lfo_stereo = get_lfo_value(lfo_phase_r, s_waveform);
        
        // Convert LFO to pan position (-1 to +1)
        float pan_l = lfo_main * s_depth;
        float pan_r = lfo_stereo * s_depth;
        
        // Input signals
        float in_l = in_ptr[0];
        float in_r = in_ptr[1];
        
        // Mid/Side processing for width
        float mid = (in_l + in_r) * 0.5f;
        float side = (in_l - in_r) * 0.5f;
        side *= (1.f + s_stereo_width);
        
        in_l = mid + side;
        in_r = mid - side;
        
        float out_l, out_r;
        
        // Apply effect based on mode
        switch (s_mode) {
            case 0: { // PAN ONLY
                float pan_pos = pan_l * s_pan_amount;
                float gain_l = 0.5f * (1.f - pan_pos);
                float gain_r = 0.5f * (1.f + pan_pos);
                
                out_l = (in_l * gain_l + in_r * (1.f - gain_l));
                out_r = (in_r * gain_r + in_l * (1.f - gain_r));
                break;
            }
            case 1: { // TREM ONLY
                float trem = 1.f - s_tremolo_amount + lfo_main * s_tremolo_amount;
                trem = clipminmaxf(0.f, trem, 1.f);
                
                out_l = in_l * trem;
                out_r = in_r * trem;
                break;
            }
            case 2: { // PAN + TREM
                float pan_pos = pan_l * s_pan_amount;
                float gain_l = 0.5f * (1.f - pan_pos);
                float gain_r = 0.5f * (1.f + pan_pos);
                
                float trem = 1.f - s_tremolo_amount + lfo_main * s_tremolo_amount;
                trem = clipminmaxf(0.f, trem, 1.f);
                
                out_l = (in_l * gain_l + in_r * (1.f - gain_l)) * trem;
                out_r = (in_r * gain_r + in_l * (1.f - gain_r)) * trem;
                break;
            }
            case 3: { // CROSSFADE
                float morph = (lfo_main + 1.f) * 0.5f;  // 0-1
                
                // Pan
                float pan_pos = pan_l * s_pan_amount;
                float gain_l_pan = 0.5f * (1.f - pan_pos);
                float gain_r_pan = 0.5f * (1.f + pan_pos);
                float pan_l_out = (in_l * gain_l_pan + in_r * (1.f - gain_l_pan));
                float pan_r_out = (in_r * gain_r_pan + in_l * (1.f - gain_r_pan));
                
                // Trem
                float trem = 1.f - s_tremolo_amount + lfo_main * s_tremolo_amount;
                trem = clipminmaxf(0.f, trem, 1.f);
                float trem_l_out = in_l * trem;
                float trem_r_out = in_r * trem;
                
                // Crossfade
                out_l = pan_l_out * (1.f - morph) + trem_l_out * morph;
                out_r = pan_r_out * (1.f - morph) + trem_r_out * morph;
                break;
            }
        }
        
        out_ptr[0] = clipminmaxf(-1.f, out_l, 1.f);
        out_ptr[1] = clipminmaxf(-1.f, out_r, 1.f);
        
        in_ptr += 2;
        out_ptr += 2;
        
        // Update LFO phase
        s_lfo_phase += lfo_inc;
        if (s_lfo_phase >= 1.f) s_lfo_phase -= 1.f;
        
        s_sample_counter++;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_rate = valf; break;
        case 1: s_depth = valf; break;
        case 2: s_stereo_width = valf; break;
        case 3: s_phase_offset = valf; break;
        case 4: s_tremolo_amount = valf; break;
        case 5: s_pan_amount = valf; break;
        case 6: s_waveform = value; break;
        case 7: s_tempo_sync = (value > 0); break;
        case 8: s_division = value; break;
        case 9: s_mode = value; break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0: return (int32_t)(s_rate * 1023.f);
        case 1: return (int32_t)(s_depth * 1023.f);
        case 2: return (int32_t)(s_stereo_width * 1023.f);
        case 3: return (int32_t)(s_phase_offset * 1023.f);
        case 4: return (int32_t)(s_tremolo_amount * 1023.f);
        case 5: return (int32_t)(s_pan_amount * 1023.f);
        case 6: return s_waveform;
        case 7: return s_tempo_sync ? 1 : 0;
        case 8: return s_division;
        case 9: return s_mode;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
    if (id == 6) {
        static const char *wave_names[] = {
            "SINE", "TRI", "SQR", "SAWUP", "SAWDN", "RANDOM", "SMOOTH", "CUSTOM"
        };
        if (value >= 0 && value < 8) return wave_names[value];
    }
    if (id == 7) {
        return value ? "SYNC" : "FREE";
    }
    if (id == 8) {
        static const char *div_names[] = {
            "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/128"
        };
        if (value >= 0 && value < 8) return div_names[value];
    }
    if (id == 9) {
        static const char *mode_names[] = {"PAN", "TREM", "BOTH", "XFADE"};
        if (value >= 0 && value < 4) return mode_names[value];
    }
    return "";
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    if (s_tempo_sync && counter % 16 == 0) {
        s_lfo_phase = 0.f;
    }
}

__unit_callback void unit_set_tempo(uint32_t tempo) {}

