/*
    SUNDAY CHURCH - Cathedral Reverb Implementation
    
    FEATURES:
    - Dattorro Figure-Eight topology
    - Cubic Hermite interpolation (smooth modulation)
    - Soft clipping (infinite reverb support)
    - SDRAM allocation (4MB+ buffers)
    - 10 parameters for fine control
*/

#include "unit_revfx.h"
#include "fx_api.h"
#include "utils/float_math.h"
#include "utils/int_math.h"

// ═══════════════════════════════════════════════════════════════════════════
// DELAY LINE WITH CUBIC INTERPOLATION
// ═══════════════════════════════════════════════════════════════════════════

class DelayLine {
public:
    void init(float *buffer, uint32_t size) {
        m_buffer = buffer;
        m_size = size;
        m_write_pos = 0;
        
        // Clear buffer
        for (uint32_t i = 0; i < size; i++) {
            m_buffer[i] = 0.f;
        }
    }
    
    inline void write(float sample) {
        m_buffer[m_write_pos] = sample;
        m_write_pos = (m_write_pos + 1) % m_size;
    }
    
    // Cubic Hermite interpolation (anti-distortion)
    inline float read_cubic(float delay_samples) {
        // Clamp delay
        if (delay_samples < 1.f) delay_samples = 1.f;
        if (delay_samples >= (float)(m_size - 4)) delay_samples = (float)(m_size - 4);
        
        // Calculate read position
        float read_pos_float = (float)m_write_pos - delay_samples;
        if (read_pos_float < 0.f) read_pos_float += (float)m_size;
        
        // Integer and fractional parts
        uint32_t read_pos = (uint32_t)read_pos_float;
        float frac = read_pos_float - (float)read_pos;
        
        // Get 4 samples for cubic interpolation
        uint32_t p0 = (read_pos + m_size - 1) % m_size;
        uint32_t p1 = read_pos;
        uint32_t p2 = (read_pos + 1) % m_size;
        uint32_t p3 = (read_pos + 2) % m_size;
        
        float y0 = m_buffer[p0];
        float y1 = m_buffer[p1];
        float y2 = m_buffer[p2];
        float y3 = m_buffer[p3];
        
        // Cubic Hermite interpolation
        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }
    
    inline float read_linear(float delay_samples) {
        // Clamp
        if (delay_samples < 1.f) delay_samples = 1.f;
        if (delay_samples >= (float)(m_size - 2)) delay_samples = (float)(m_size - 2);
        
        float read_pos_float = (float)m_write_pos - delay_samples;
        if (read_pos_float < 0.f) read_pos_float += (float)m_size;
        
        uint32_t read_pos = (uint32_t)read_pos_float;
        float frac = read_pos_float - (float)read_pos;
        
        uint32_t p1 = read_pos;
        uint32_t p2 = (read_pos + 1) % m_size;
        
        float y1 = m_buffer[p1];
        float y2 = m_buffer[p2];
        
        return y1 + frac * (y2 - y1);
    }
    
private:
    float *m_buffer;
    uint32_t m_size;
    uint32_t m_write_pos;
};

// ═══════════════════════════════════════════════════════════════════════════
// ALLPASS FILTER
// ═══════════════════════════════════════════════════════════════════════════

class Allpass {
public:
    void init(float *buffer, uint32_t size, float feedback) {
        m_delay.init(buffer, size);
        m_feedback = feedback;
    }
    
    inline float process(float input) {
        float delayed = m_delay.read_linear(1.f);
        float output = -input + delayed;
        m_delay.write(input + delayed * m_feedback);
        return output;
    }
    
    inline float process_modulated(float input, float mod_samples) {
        float delayed = m_delay.read_cubic(mod_samples);
        float output = -input + delayed;
        m_delay.write(input + delayed * m_feedback);
        return output;
    }
    
    void set_feedback(float fb) {
        m_feedback = clipminmaxf(0.f, fb, 0.7f);
    }
    
private:
    DelayLine m_delay;
    float m_feedback;
};

// ═══════════════════════════════════════════════════════════════════════════
// EARLY REFLECTIONS
// ═══════════════════════════════════════════════════════════════════════════

struct EarlyReflections {
    static constexpr uint8_t NUM_TAPS = 8;
    static const uint32_t TAP_DELAYS[NUM_TAPS];
    
    DelayLine delay;
    
    void init(float *buffer, uint32_t size) {
        delay.init(buffer, size);
    }
    
    float process(float input, float level) {
        if (level < 0.01f) return 0.f;
        
        delay.write(input);
        
        float output = 0.f;
        for (uint8_t i = 0; i < NUM_TAPS; i++) {
            float tap = delay.read_linear((float)TAP_DELAYS[i]);
            float decay = 1.f - ((float)i / (float)NUM_TAPS) * 0.7f;
            output += tap * decay;
        }
        
        return output * level / (float)NUM_TAPS;
    }
};

const uint32_t EarlyReflections::TAP_DELAYS[8] = {
    397, 797, 1193, 1597, 1993, 2393, 2797, 3191
};

// ═══════════════════════════════════════════════════════════════════════════
// DATTORRO TANK
// ═══════════════════════════════════════════════════════════════════════════

struct DattorroTank {
    // Left tank
    Allpass ap1_l;
    DelayLine delay1_l;
    Allpass ap2_l;
    DelayLine delay2_l;
    
    // Right tank
    Allpass ap1_r;
    DelayLine delay1_r;
    Allpass ap2_r;
    DelayLine delay2_r;
    
    // Damping filters (one-pole lowpass)
    float damp_z_l1, damp_z_l2;
    float damp_z_r1, damp_z_r2;
    
    // LFOs for modulation
    float lfo_phase_l;
    float lfo_phase_r;
    
    void init(float *buffer, uint32_t offset) {
        // Left tank delays
        ap1_l.init(buffer + offset, 672, 0.7f);
        offset += 672;
        delay1_l.init(buffer + offset, 4453);
        offset += 4453;
        ap2_l.init(buffer + offset, 1800, 0.5f);
        offset += 1800;
        delay2_l.init(buffer + offset, 3720);
        offset += 3720;
        
        // Right tank delays (prime numbers, slightly different)
        ap1_r.init(buffer + offset, 908, 0.7f);
        offset += 908;
        delay1_r.init(buffer + offset, 4217);
        offset += 4217;
        ap2_r.init(buffer + offset, 2656, 0.5f);
        offset += 2656;
        delay2_r.init(buffer + offset, 3163);
        offset += 3163;
        
        // Init damping
        damp_z_l1 = damp_z_l2 = 0.f;
        damp_z_r1 = damp_z_r2 = 0.f;
        
        // Init LFOs (90 degrees out of phase for stereo)
        lfo_phase_l = 0.f;
        lfo_phase_r = 0.25f;
    }
    
    void process(float input_l, float input_r,
                 float *out_l, float *out_r,
                 float feedback, float damping, 
                 float mod_depth, float mod_rate,
                 float size_mult) {
        
        // Update LFOs
        lfo_phase_l += mod_rate / 48000.f;
        lfo_phase_r += mod_rate / 48000.f;
        if (lfo_phase_l >= 1.f) lfo_phase_l -= 1.f;
        if (lfo_phase_r >= 1.f) lfo_phase_r -= 1.f;
        
        // LFO values
        float lfo_l = fx_sinf(lfo_phase_l);
        float lfo_r = fx_sinf(lfo_phase_r);
        
        // Modulation amount (samples)
        float mod_l = 8.f + lfo_l * mod_depth * 8.f;
        float mod_r = 8.f + lfo_r * mod_depth * 8.f;
        
        // LEFT TANK
        float tank_in_l = input_l + feedback * damp_z_r2;
        
        tank_in_l = ap1_l.process(tank_in_l);
        delay1_l.write(tank_in_l);
        
        float d1_out_l = delay1_l.read_cubic(4453.f * size_mult);
        
        // Damping (one-pole lowpass)
        damp_z_l1 = d1_out_l * (1.f - damping) + damp_z_l1 * damping;
        damp_z_l1 = clipminmaxf(-2.f, damp_z_l1, 2.f);
        
        float ap2_in_l = ap2_l.process_modulated(damp_z_l1, 1800.f + mod_l);
        delay2_l.write(ap2_in_l);
        
        float d2_out_l = delay2_l.read_cubic(3720.f * size_mult);
        
        damp_z_l2 = d2_out_l * (1.f - damping) + damp_z_l2 * damping;
        damp_z_l2 = clipminmaxf(-2.f, damp_z_l2, 2.f);
        
        // Soft clip to prevent explosion
        damp_z_l2 = fastertanhf(damp_z_l2);
        
        // RIGHT TANK
        float tank_in_r = input_r + feedback * damp_z_l2;
        
        tank_in_r = ap1_r.process(tank_in_r);
        delay1_r.write(tank_in_r);
        
        float d1_out_r = delay1_r.read_cubic(4217.f * size_mult);
        
        damp_z_r1 = d1_out_r * (1.f - damping) + damp_z_r1 * damping;
        damp_z_r1 = clipminmaxf(-2.f, damp_z_r1, 2.f);
        
        float ap2_in_r = ap2_r.process_modulated(damp_z_r1, 2656.f + mod_r);
        delay2_r.write(ap2_in_r);
        
        float d2_out_r = delay2_r.read_cubic(3163.f * size_mult);
        
        damp_z_r2 = d2_out_r * (1.f - damping) + damp_z_r2 * damping;
        damp_z_r2 = clipminmaxf(-2.f, damp_z_r2, 2.f);
        
        damp_z_r2 = fastertanhf(damp_z_r2);
        
        // Output (multiple taps for richness)
        *out_l = d1_out_l * 0.6f + ap2_in_l * 0.4f;
        *out_r = d1_out_r * 0.6f + ap2_in_r * 0.4f;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// GLOBAL STATE
// ═══════════════════════════════════════════════════════════════════════════

static float *s_reverb_buffer = nullptr;
static constexpr uint32_t REVERB_BUFFER_SIZE = 120000; // ~2.5 seconds @ 48kHz

static DelayLine s_predelay;
static EarlyReflections s_early_l;
static EarlyReflections s_early_r;

static Allpass s_input_diffuser[4];
static DattorroTank s_tank;

// Parameters
static float s_time;
static float s_depth;
static float s_mix;
static float s_size;
static float s_damping;
static float s_diffusion;
static float s_predelay_time;
static float s_early_level;
static float s_mod_rate;
static float s_width;

// ═══════════════════════════════════════════════════════════════════════════
// UNIT CALLBACKS
// ═══════════════════════════════════════════════════════════════════════════

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;
    
    // CRITICAL: Allocate SDRAM
    if (!desc->hooks.sdram_alloc) return k_unit_err_memory;
    
    uint32_t total_size = REVERB_BUFFER_SIZE * sizeof(float);
    uint8_t *raw_buffer = desc->hooks.sdram_alloc(total_size);
    if (!raw_buffer) return k_unit_err_memory;
    s_reverb_buffer = reinterpret_cast<float *>(raw_buffer);
    
    if (!s_reverb_buffer) return k_unit_err_memory;
    
    // Clear buffer
    for (uint32_t i = 0; i < REVERB_BUFFER_SIZE; i++) {
        s_reverb_buffer[i] = 0.f;
    }
    
    uint32_t offset = 0;
    
    // Pre-delay (500ms max)
    s_predelay.init(s_reverb_buffer + offset, 24000);
    offset += 24000;
    
    // Early reflections
    s_early_l.init(s_reverb_buffer + offset, 8000);
    offset += 8000;
    s_early_r.init(s_reverb_buffer + offset, 8000);
    offset += 8000;
    
    // Input diffusers
    s_input_diffuser[0].init(s_reverb_buffer + offset, 142, 0.75f);
    offset += 142;
    s_input_diffuser[1].init(s_reverb_buffer + offset, 107, 0.75f);
    offset += 107;
    s_input_diffuser[2].init(s_reverb_buffer + offset, 379, 0.625f);
    offset += 379;
    s_input_diffuser[3].init(s_reverb_buffer + offset, 277, 0.625f);
    offset += 277;
    
    // Dattorro tank
    s_tank.init(s_reverb_buffer, offset);
    
    // Init parameters (60% time, 30% depth, 50% mix)
    s_time = 0.6f;
    s_depth = 0.3f;
    s_mix = 0.5f;
    s_size = 0.7f;
    s_damping = 1.0f;  // FIXED: Start met maximale damping
    s_diffusion = 0.6f;
    s_predelay_time = 0.2f;
    s_early_level = 0.3f;
    s_mod_rate = 0.15f;
    s_width = 0.7f;
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    // Clear all delay buffers
    if (s_reverb_buffer) {
        for (uint32_t i = 0; i < REVERB_BUFFER_SIZE; i++) {
            s_reverb_buffer[i] = 0.f;
        }
    }
    
    s_tank.damp_z_l1 = 0.f;
    s_tank.damp_z_l2 = 0.f;
    s_tank.damp_z_r1 = 0.f;
    s_tank.damp_z_r2 = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    const float * __restrict in_p = in;
    float * __restrict out_p = out;
    
    for (uint32_t f = 0; f < frames; f++) {
        float in_l = in_p[f * 2];
        float in_r = in_p[f * 2 + 1];
        
        // Input clip
        in_l = clipminmaxf(-1.f, in_l, 1.f);
        in_r = clipminmaxf(-1.f, in_r, 1.f);
        
        // Mono sum for reverb
        float mono = (in_l + in_r) * 0.5f;
        
        // Pre-delay
        s_predelay.write(mono);
        float predelayed = s_predelay.read_linear(s_predelay_time * 24000.f);
        
        // Early reflections
        float early_l = s_early_l.process(predelayed, s_early_level);
        float early_r = s_early_r.process(predelayed, s_early_level);
        
        // Input diffusion
        float diffused = predelayed;
        for (uint8_t i = 0; i < 4; i++) {
            diffused = s_input_diffuser[i].process(diffused);
        }
        
        // Scale by diffusion parameter
        diffused *= s_diffusion;
        
        // Feedback calculation (safe for infinite reverb)
        float feedback = 0.65f + s_time * 0.33f;
        feedback = clipminmaxf(0.65f, feedback, 0.98f);
        
        // Modulation rate (0.1 - 5 Hz)
        float mod_rate = 0.1f + s_mod_rate * 4.9f;
        
        // Size multiplier (0.5 - 1.5x)
        float size_mult = 0.5f + s_size;
        
        // Process tank
        float tank_l, tank_r;
        s_tank.process(diffused, diffused,
                      &tank_l, &tank_r,
                      feedback, s_damping,
                      s_depth, mod_rate, size_mult);
        
        // Combine early + late
        float wet_l = early_l + tank_l;
        float wet_r = early_r + tank_r;
        
        // Stereo width control
        float mid = (wet_l + wet_r) * 0.5f;
        float side = (wet_l - wet_r) * 0.5f * s_width;
        wet_l = mid + side;
        wet_r = mid - side;
        
        // Output gain compensation (-6dB)
        wet_l *= 0.5f;
        wet_r *= 0.5f;
        
        // Soft clip wet signal
        wet_l = fastertanhf(wet_l * 0.9f);
        wet_r = fastertanhf(wet_r * 0.9f);
        
        // Dry/wet mix
        float dry_wet = (s_mix + 1.f) * 0.5f;
        
        out_p[f * 2] = in_l * (1.f - dry_wet) + wet_l * dry_wet;
        out_p[f * 2 + 1] = in_r * (1.f - dry_wet) + wet_r * dry_wet;
        
        // Final safety clip
        out_p[f * 2] = clipminmaxf(-1.f, out_p[f * 2], 1.f);
        out_p[f * 2 + 1] = clipminmaxf(-1.f, out_p[f * 2 + 1], 1.f);
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_time = clipminmaxf(0.f, valf, 1.f); break;
        case 1: s_depth = clipminmaxf(0.f, valf, 1.f); break;
        case 2: s_mix = clipminmaxf(-1.f, (float)value / 100.f, 1.f); break;
        case 3: s_size = clipminmaxf(0.f, valf, 1.f); break;
        case 4: {
            // INVERTED DAMP: 0 = min damp (max galm), 1 = max damp (min galm)
            float inverted_damp = 1.f - valf;
            s_damping = clipminmaxf(0.002f, inverted_damp, 0.998f);
            break;
        }
        case 5: s_diffusion = clipminmaxf(0.f, valf, 1.f); break;
        case 6: s_predelay_time = clipminmaxf(0.f, valf, 1.f); break;
        case 7: s_early_level = clipminmaxf(0.f, valf, 1.f); break;
        case 8: s_mod_rate = clipminmaxf(0.f, valf, 1.f); break;
        case 9: s_width = clipminmaxf(0.f, valf, 1.f); break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return (int32_t)(s_time * 1023.f);
        case 1: return (int32_t)(s_depth * 1023.f);
        case 2: return (int32_t)(s_mix * 100.f);
        case 3: return (int32_t)(s_size * 1023.f);
        case 4: return (int32_t)((1.f - s_damping) * 1023.f);  // INVERTED
        case 5: return (int32_t)(s_diffusion * 1023.f);
        case 6: return (int32_t)(s_predelay_time * 1023.f);
        case 7: return (int32_t)(s_early_level * 1023.f);
        case 8: return (int32_t)(s_mod_rate * 1023.f);
        case 9: return (int32_t)(s_width * 1023.f);
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    (void)id;
    (void)value;
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    (void)tempo;
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    (void)counter;
}

