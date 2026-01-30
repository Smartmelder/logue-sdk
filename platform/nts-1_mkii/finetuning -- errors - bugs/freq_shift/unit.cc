/*
    BODE FREQUENCY SHIFTER - Single-sideband ring modulation
    
    THEORY:
    Unlike pitch shifting, frequency shifting moves ALL frequencies
    by the same Hz amount, creating inharmonic spectra.
    
    ALGORITHM:
    1. Hilbert transform (90° phase shift)
    2. Ring modulation with quadrature oscillators
    3. Sum/difference for upper/lower sideband
    
    FEATURES:
    - Frequency shift: ±2000 Hz
    - 4 ranges: Subtle, Medium, Extreme, Ultra
    - Up/Down shift direction
    - Stereo spread (different shift L/R)
    - Feedback loop (regeneration)
    - 4 modes: Clean, Ring, Barber, Chaos
    - Distortion
    
    MODES:
    0. CLEAN - Pure frequency shift
    1. RING - Ring modulation character
    2. BARBER - Barber-pole phasing
    3. CHAOS - Feedback chaos
*/

#include "unit_modfx.h"
#include "utils/float_math.h"
#include "osc_api.h"
#include <math.h>

#define HILBERT_TAP_COUNT 32
#define FEEDBACK_BUFFER_SIZE 4096

// Hilbert transform coefficients (90° phase shift)
static const float s_hilbert_coeffs[HILBERT_TAP_COUNT] = {
    0.f, 0.0318f, 0.f, -0.0955f, 0.f, 0.1592f, 0.f, -0.2229f,
    0.f, 0.2866f, 0.f, -0.3503f, 0.f, 0.4140f, 0.f, -0.4777f,
    0.f, 0.4777f, 0.f, -0.4140f, 0.f, 0.3503f, 0.f, -0.2866f,
    0.f, 0.2229f, 0.f, -0.1592f, 0.f, 0.0955f, 0.f, -0.0318f
};

// Delay lines for Hilbert transform
static float s_delay_l[HILBERT_TAP_COUNT];
static float s_delay_r[HILBERT_TAP_COUNT];
static uint32_t s_delay_write;

// Oscillator phases
static float s_osc_phase_cos;
static float s_osc_phase_sin;

// Feedback buffer
static float s_feedback_l[FEEDBACK_BUFFER_SIZE];
static float s_feedback_r[FEEDBACK_BUFFER_SIZE];
static uint32_t s_feedback_write;

// Parameters
static float s_shift_amount;
static float s_mix;
static float s_feedback;
static float s_stereo_spread;
static float s_detune;
static float s_distortion;
static uint8_t s_range;
static uint8_t s_direction;
static uint8_t s_mode;
static bool s_stereo_mode;

static uint32_t s_sample_counter;

inline float fast_tanh(float x) {
    if (x < -3.f) return -1.f;
    if (x > 3.f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

// Hilbert transform (90° phase shift)
inline float hilbert_transform(float input, float *delay_line) {
    // Write input to delay line
    delay_line[s_delay_write] = input;
    
    // Convolve with Hilbert coefficients
    float output = 0.f;
    uint32_t read_pos = s_delay_write;
    
    for (int i = 0; i < HILBERT_TAP_COUNT; i++) {
        output += delay_line[read_pos] * s_hilbert_coeffs[i];
        read_pos = (read_pos + HILBERT_TAP_COUNT - 1) % HILBERT_TAP_COUNT;
    }
    
    return output;
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc)
{
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;

    for (int i = 0; i < HILBERT_TAP_COUNT; i++) {
        s_delay_l[i] = 0.f;
        s_delay_r[i] = 0.f;
    }
    s_delay_write = 0;
    
    for (int i = 0; i < FEEDBACK_BUFFER_SIZE; i++) {
        s_feedback_l[i] = 0.f;
        s_feedback_r[i] = 0.f;
    }
    s_feedback_write = 0;
    
    s_osc_phase_cos = 0.f;
    s_osc_phase_sin = 0.25f;  // 90° offset
    
    s_shift_amount = 0.5f;
    s_mix = 0.6f;
    s_feedback = 0.3f;
    s_stereo_spread = 0.4f;
    s_detune = 0.25f;
    s_distortion = 0.25f;
    s_range = 0;
    s_direction = 0;
    s_mode = 0;
    s_stereo_mode = false;
    
    s_sample_counter = 0;

    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset()
{
    s_osc_phase_cos = 0.f;
    s_osc_phase_sin = 0.25f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames)
{
    const float *in_ptr = in;
    float *out_ptr = out;
    
    // Calculate shift frequency based on range
    float base_freq;
    switch (s_range) {
        case 0: base_freq = 10.f + s_shift_amount * 90.f; break;    // 10-100 Hz
        case 1: base_freq = 50.f + s_shift_amount * 450.f; break;   // 50-500 Hz
        case 2: base_freq = 100.f + s_shift_amount * 1900.f; break; // 100-2000 Hz
        case 3: base_freq = 500.f + s_shift_amount * 4500.f; break; // 500-5000 Hz
        default: base_freq = 100.f; break;
    }
    
    // Direction
    if (s_direction == 1) base_freq = -base_freq;
    
    // Oscillator increment
    float osc_w0 = base_freq / 48000.f;
    
    for (uint32_t i = 0; i < frames; i++) {
        float in_l = in_ptr[0];
        float in_r = in_ptr[1];
        
        // Add feedback
        uint32_t fb_read = (s_feedback_write + FEEDBACK_BUFFER_SIZE - 2400) % FEEDBACK_BUFFER_SIZE;
        in_l += s_feedback_l[fb_read] * s_feedback;
        in_r += s_feedback_r[fb_read] * s_feedback;
        
        // Hilbert transform (90° phase shift)
        float hilbert_l = hilbert_transform(in_l, s_delay_l);
        float hilbert_r = hilbert_transform(in_r, s_delay_r);
        
        // Quadrature oscillators
        float osc_cos = fastcosf(s_osc_phase_cos * 2.f * M_PI);
        float osc_sin = osc_sinf(s_osc_phase_sin);
        
        // Ring modulation with SSB
        float shifted_l, shifted_r;
        
        if (s_mode == 0) {
            // CLEAN - Single sideband
            shifted_l = in_l * osc_cos - hilbert_l * osc_sin;
            shifted_r = in_r * osc_cos - hilbert_r * osc_sin;
        } else if (s_mode == 1) {
            // RING - Classic ring mod
            shifted_l = in_l * osc_cos;
            shifted_r = in_r * osc_cos;
        } else if (s_mode == 2) {
            // BARBER - Both sidebands
            float upper_l = in_l * osc_cos - hilbert_l * osc_sin;
            float lower_l = in_l * osc_cos + hilbert_l * osc_sin;
            float upper_r = in_r * osc_cos - hilbert_r * osc_sin;
            float lower_r = in_r * osc_cos + hilbert_r * osc_sin;
            
            shifted_l = (upper_l + lower_l) * 0.5f;
            shifted_r = (upper_r + lower_r) * 0.5f;
        } else {
            // CHAOS - Feedback-modulated
            float chaos_mod = s_feedback_l[fb_read] * 2.f;
            float chaos_phase = s_osc_phase_cos + chaos_mod;
            chaos_phase -= (int32_t)chaos_phase;
            if (chaos_phase < 0.f) chaos_phase += 1.f;
            shifted_l = in_l * fastcosf(chaos_phase * 2.f * M_PI);
            shifted_r = in_r * fastcosf(chaos_phase * 2.f * M_PI);
        }
        
        // Stereo spread
        if (s_stereo_mode) {
            float spread_offset = s_stereo_spread * 0.1f;
            float phase_r = s_osc_phase_cos + spread_offset;
            phase_r -= (int32_t)phase_r;
            if (phase_r < 0.f) phase_r += 1.f;
            float osc_cos_r = fastcosf(phase_r * 2.f * M_PI);
            float osc_sin_r = osc_sinf(s_osc_phase_sin + spread_offset);
            shifted_r = in_r * osc_cos_r - hilbert_r * osc_sin_r;
        }
        
        // Distortion
        if (s_distortion > 0.01f) {
            float drive = 1.f + s_distortion * 3.f;
            shifted_l = fast_tanh(shifted_l * drive);
            shifted_r = fast_tanh(shifted_r * drive);
        }
        
        // Store in feedback buffer
        s_feedback_l[s_feedback_write] = shifted_l;
        s_feedback_r[s_feedback_write] = shifted_r;
        
        // Mix
        out_ptr[0] = in_l * (1.f - s_mix) + shifted_l * s_mix;
        out_ptr[1] = in_r * (1.f - s_mix) + shifted_r * s_mix;
        
        out_ptr[0] = clipminmaxf(-1.f, out_ptr[0], 1.f);
        out_ptr[1] = clipminmaxf(-1.f, out_ptr[1], 1.f);
        
        in_ptr += 2;
        out_ptr += 2;
        
        // Update phases
        s_osc_phase_cos += osc_w0;
        s_osc_phase_sin += osc_w0;
        if (s_osc_phase_cos >= 1.f) s_osc_phase_cos -= 1.f;
        if (s_osc_phase_cos < 0.f) s_osc_phase_cos += 1.f;
        if (s_osc_phase_sin >= 1.f) s_osc_phase_sin -= 1.f;
        if (s_osc_phase_sin < 0.f) s_osc_phase_sin += 1.f;
        
        s_delay_write = (s_delay_write + 1) % HILBERT_TAP_COUNT;
        s_feedback_write = (s_feedback_write + 1) % FEEDBACK_BUFFER_SIZE;
        
        s_sample_counter++;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_shift_amount = valf; break;
        case 1: s_mix = valf; break;
        case 2: s_feedback = valf; break;
        case 3: s_stereo_spread = valf; break;
        case 4: s_detune = valf; break;
        case 5: s_distortion = valf; break;
        case 6: s_range = value; break;
        case 7: s_direction = value; break;
        case 8: s_mode = value; break;
        case 9: s_stereo_mode = (value > 0); break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0: return (int32_t)(s_shift_amount * 1023.f);
        case 1: return (int32_t)(s_mix * 1023.f);
        case 2: return (int32_t)(s_feedback * 1023.f);
        case 3: return (int32_t)(s_stereo_spread * 1023.f);
        case 4: return (int32_t)(s_detune * 1023.f);
        case 5: return (int32_t)(s_distortion * 1023.f);
        case 6: return s_range;
        case 7: return s_direction;
        case 8: return s_mode;
        case 9: return s_stereo_mode ? 1 : 0;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
    if (id == 6) {
        static const char *range_names[] = {"SUBTLE", "MEDIUM", "EXTREME", "ULTRA"};
        if (value >= 0 && value < 4) return range_names[value];
    }
    if (id == 7) {
        static const char *dir_names[] = {"UP", "DOWN"};
        if (value >= 0 && value < 2) return dir_names[value];
    }
    if (id == 8) {
        static const char *mode_names[] = {"CLEAN", "RING", "BARBER", "CHAOS"};
        if (value >= 0 && value < 4) return mode_names[value];
    }
    if (id == 9) {
        return value ? "STEREO" : "MONO";
    }
    return "";
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {}
__unit_callback void unit_set_tempo(uint32_t tempo) {}

