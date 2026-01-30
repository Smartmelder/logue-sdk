/*
    WATERKUT V2 - FIXED VERSION!
    
    CRITICAL FIXES:
    - Parameter mapping corrected (matched to header.c)
    - fastertanh2f replaced with fast_tanh
    - Feedback properly limited (max 0.93)
    - NaN/Inf protection everywhere
    - Buffer clearing on reset
    
    Raindrop delay with 10 parallel lines
*/

#include "waterkut.h"
#include <algorithm>

// ========== NaN/Inf CHECK MACRO (FIXED!) ==========
// ✅ FIX: Correct NaN detection (NaN != NaN is TRUE)
// Note: si_isfinite() not available for delfx, using correct macro instead
#define is_finite(x) ((x) != (x) ? false : ((x) <= 1e10f && (x) >= -1e10f))

// ========== INITIALIZATION ==========

void Processor::init(float *buffer) {
    m_buffer_base = buffer;
    m_buffer_allocated = getBufferSize();
    
    // Clear buffer
    if (buffer) {
        std::fill(buffer, buffer + m_buffer_allocated, 0.f);
    }
    
    // Defaults (matched to header.c)
    m_time = 0.8f;           // Param 0: TIME
    m_feedback = 0.75f;      // Param 1: DEPTH (feedback)
    m_mix = 0.f;             // Param 2: MIX (balanced)
    m_chaos = 0.5f;          // Param 3: CHAOS
    m_mod_intensity = 0.3f;  // Param 4: MODINT
    m_mod_rate = 0.1f;       // Param 5: MODRATE
    m_tone = 0.5f;           // Param 6: TONE
    m_stereo_width = 1.5f;   // Param 7: STEREO (150%)
    m_active_lines = 10;     // Param 8: LINES
    m_diffusion = 0.4f;      // Param 9: DIFFUSE
    m_freeze = false;        // Param 10: FREEZE
    
    // Modulation
    m_lfo_phase = 0.f;
    
    // Filters
    m_tone_z1_l = m_tone_z1_r = 0.f;
    
    // Random
    m_random_seed = 12345;
    
    // Setup delay lines
    uint32_t offset = 0;
    const uint32_t line_size = 60000;  // 1.25 seconds each
    
    for (uint8_t i = 0; i < NUM_DELAY_LINES; i++) {
        m_delays[i].size = line_size;
        m_delays[i].buffer = buffer ? (buffer + offset) : nullptr;
        m_delays[i].write_pos = 0;
        m_delays[i].base_time = 0.05f + i * 0.14f;  // 50ms to 1.3s
        m_delays[i].random_offset = 0.f;
        m_delays[i].feedback_mult = 0.9f - i * 0.07f;
        m_delays[i].filter_z1 = 0.f;
        m_delays[i].pan = (float)i / (float)(NUM_DELAY_LINES - 1) * 2.f - 1.f;
        
        offset += line_size;
    }
    
    init_random_offsets();
}

void Processor::teardown() {
    // Nothing to teardown
}

void Processor::reset() {
    // Clear all delay buffers
    if (m_buffer_base) {
        std::fill(m_buffer_base, m_buffer_base + m_buffer_allocated, 0.f);
    }
    
    // Reset write positions
    for (uint8_t i = 0; i < NUM_DELAY_LINES; i++) {
        m_delays[i].write_pos = 0;
        m_delays[i].filter_z1 = 0.f;
    }
    
    // Reset filters
    m_tone_z1_l = 0.f;
    m_tone_z1_r = 0.f;
    
    // Reset LFO
    m_lfo_phase = 0.f;
}

void Processor::resume() {}

void Processor::suspend() {}

// ========== RANDOM GENERATOR ==========

inline float Processor::random_float() {
    m_random_seed = m_random_seed * 1103515245 + 12345;
    return ((m_random_seed >> 16) & 0x7FFF) / 16384.f - 1.f;
}

void Processor::init_random_offsets() {
    for (uint8_t i = 0; i < NUM_DELAY_LINES; i++) {
        m_delays[i].random_offset = random_float() * m_chaos * 0.3f;
    }
}

// ========== MODULATION ==========

inline float Processor::mod_oscillator() {
    // LFO rate: 0.1 - 10 Hz
    float rate_hz = 0.1f + m_mod_rate * 9.9f;
    
    m_lfo_phase += rate_hz / 48000.f;
    if (m_lfo_phase >= 1.f) m_lfo_phase -= 1.f;
    
    return si_sinf(m_lfo_phase * 2.f * 3.14159265f);
}

// ========== DELAY READING ==========

inline float Processor::read_delay(const DelayLine &delay, float delay_time) {
    if (!delay.buffer) return 0.f;
    
    float delay_samples = delay_time * 48000.f;
    delay_samples = clipminmaxf(1.f, delay_samples, (float)(delay.size - 1));
    
    float read_pos_f = (float)delay.write_pos - delay_samples;
    while (read_pos_f < 0.f) read_pos_f += (float)delay.size;
    
    uint32_t read_pos_0 = (uint32_t)read_pos_f;
    uint32_t read_pos_1 = (read_pos_0 + 1) % delay.size;
    float frac = read_pos_f - (float)read_pos_0;
    
    float sample = delay.buffer[read_pos_0] * (1.f - frac) + 
                   delay.buffer[read_pos_1] * frac;
    
    // ✅ FIX: Use correct NaN check!
    if (!is_finite(sample)) sample = 0.f;
    
    return sample;
}

// ========== DELAY LINE PROCESSING ==========

inline float Processor::process_delay_line(DelayLine &delay, float input) {
    if (!delay.buffer) return input;
    
    // Freeze mode: don't write new input
    if (m_freeze) {
        input = 0.f;
    }
    
    // Calculate modulation
    float lfo = mod_oscillator();
    float modulation = lfo * m_mod_intensity * 0.1f;
    
    // Calculate effective delay time
    float delay_time = delay.base_time * m_time * 
                      (1.f + delay.random_offset + modulation);
    delay_time = clipminmaxf(0.001f, delay_time, 2.5f);
    
    // Read delayed signal
    float delayed = read_delay(delay, delay_time);
    
    // Apply tone filter per line
    float g = 0.1f + m_tone * 0.85f;
    delay.filter_z1 += g * (delayed - delay.filter_z1);
    
    if (si_fabsf(delay.filter_z1) < 1e-15f) delay.filter_z1 = 0.f;
    
    if (m_tone < 0.5f) {
        // Lowpass
        delayed = delay.filter_z1;
    } else {
        // Highpass
        delayed = delayed - delay.filter_z1 * ((m_tone - 0.5f) * 2.f);
    }
    
    // ✅ FIX: Safe feedback limiting
    float feedback_amount = delay.feedback_mult * m_feedback;
    feedback_amount = clipminmaxf(0.f, feedback_amount, 0.93f);  // SAFE MAX!
    
    // Mix input with feedback
    float mixed = input + delayed * feedback_amount;
    
    // ✅ FIX: Use si_tanhf() for delfx!
    mixed = si_tanhf(mixed * 0.5f) * 2.f;
    
    // Clip to prevent runaway
    mixed = clipminmaxf(-3.f, mixed, 3.f);
    
    // ✅ FIX: Use correct NaN check!
    if (!is_finite(mixed)) mixed = input;
    
    // Write to buffer
    delay.buffer[delay.write_pos] = mixed;
    delay.write_pos = (delay.write_pos + 1) % delay.size;
    
    return delayed;
}

// ========== DIFFUSION (ALL-PASS) ==========

inline float Processor::apply_diffusion(float input) {
    if (m_diffusion < 0.01f) return input;
    
    // Simple all-pass diffusion
    static float diff_z1 = 0.f;
    
    float coeff = 0.5f * m_diffusion;
    float output = -input + diff_z1;
    diff_z1 = input + diff_z1 * coeff;
    
    if (si_fabsf(diff_z1) < 1e-15f) diff_z1 = 0.f;
    diff_z1 = clipminmaxf(-2.f, diff_z1, 2.f);
    
    return input * (1.f - m_diffusion * 0.5f) + output * m_diffusion * 0.5f;
}

// ========== MAIN PROCESSING ==========

void Processor::process(const float *in, float *out, uint32_t frames) {
    for (uint32_t f = 0; f < frames; f++) {
        float in_l = in[f * 2];
        float in_r = in[f * 2 + 1];
        
        // ✅ FIX: Use correct NaN check!
        if (!is_finite(in_l)) in_l = 0.f;
        if (!is_finite(in_r)) in_r = 0.f;
        
        // Mono sum for delay processing
        float mono = (in_l + in_r) * 0.5f;
        
        // Process through delay lines (only active lines)
        float wet_l = 0.f;
        float wet_r = 0.f;
        
        uint8_t active = clipminmaxu32(1, m_active_lines, NUM_DELAY_LINES);
        
        for (uint8_t i = 0; i < active; i++) {
            float delayed = process_delay_line(m_delays[i], mono);
            
            // Apply diffusion
            delayed = apply_diffusion(delayed);
            
            // Pan to stereo
            float pan = m_delays[i].pan;  // -1 to +1
            float pan_l = (1.f - pan) * 0.5f;
            float pan_r = (1.f + pan) * 0.5f;
            
            wet_l += delayed * pan_l;
            wet_r += delayed * pan_r;
        }
        
        // Normalize by active line count
        if (active > 1) {
            wet_l /= (float)active;
            wet_r /= (float)active;
        }
        
        // Apply stereo width
        float mid = (wet_l + wet_r) * 0.5f;
        float side = (wet_l - wet_r) * 0.5f * m_stereo_width;
        
        wet_l = mid + side;
        wet_r = mid - side;
        
        // ✅ FIX: Use correct NaN check!
        if (!is_finite(wet_l)) wet_l = 0.f;
        if (!is_finite(wet_r)) wet_r = 0.f;
        
        // Mix dry/wet (m_mix range: -1 to +1)
        float dry_gain = 1.f - si_fabsf(m_mix);
        float wet_gain = (m_mix + 1.f) * 0.5f;
        
        float out_l = in_l * dry_gain + wet_l * wet_gain;
        float out_r = in_r * dry_gain + wet_r * wet_gain;
        
        // Output limiting
        out[f * 2] = clipminmaxf(-1.f, out_l, 1.f);
        out[f * 2 + 1] = clipminmaxf(-1.f, out_r, 1.f);
    }
}

// ========== PARAMETER UPDATES ==========

void Processor::setParameter(uint8_t id, int32_t value) {
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: // TIME
            m_time = 0.01f + valf * 2.99f;  // 10ms to 3s
            break;
            
        case 1: // DEPTH (Feedback)
            m_feedback = valf;
            break;
            
        case 2: // MIX (hardware mapped: -100 to +100)
            m_mix = (float)value / 100.f;
            m_mix = clipminmaxf(-1.f, m_mix, 1.f);
            break;
            
        case 3: // CHAOS
            m_chaos = valf;
            init_random_offsets();
            break;
            
        case 4: // MOD INTENSITY
            m_mod_intensity = valf;
            break;
            
        case 5: // MOD RATE
            m_mod_rate = valf;
            break;
            
        case 6: // TONE
            m_tone = valf;
            break;
            
        case 7: // STEREO WIDTH
            m_stereo_width = valf * 2.f;  // 0-200%
            break;
            
        case 8: // LINES (1-10)
            m_active_lines = (uint8_t)value;
            m_active_lines = clipminmaxu32(1, m_active_lines, NUM_DELAY_LINES);
            break;
            
        case 9: // DIFFUSION
            m_diffusion = valf;
            break;
            
        case 10: // FREEZE
            m_freeze = (value != 0);
            break;
            
        default:
            break;
    }
}

const char* Processor::getParameterStrValue(uint8_t id, int32_t value) {
    static char buf[16];
    
    if (id == 8) {
        // LINES parameter
        buf[0] = '0' + (char)(value / 10);
        buf[1] = '0' + (char)(value % 10);
        buf[2] = '\0';
        return buf;
    }
    
    if (id == 10) {
        // FREEZE parameter
        return (value != 0) ? "ON" : "OFF";
    }
    
    return "";
}

void Processor::setTempo(float bpm) {
    (void)bpm;
}

void Processor::tempo4ppqnTick(uint32_t counter) {
    (void)counter;
}
