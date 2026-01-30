/*
    SHIVIKUTFREQ - Frequency Shifting Dub Delay
    
    ULTIMATE PSYCHEDELIC DUB DELAY WITH FREQUENCY SHIFTER!
    
    FEATURES:
    - Real frequency shifter using Hilbert transform
    - Linear Hz shift (not pitch shift!)
    - Each repeat shifts in frequency
    - Spiraling, glidinging, sci-fi echoes
    - Perfect for dub techno/house
    
    ALGORITHM:
    - Delay buffer with feedback
    - Hilbert transform for 90° phase shift
    - Single-sideband modulation for freq shift
    - Per-repeat frequency shifting
    - Tempo sync support
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/

#include "unit_delfx.h"
#include "fx_api.h"
#include "utils/float_math.h"

// ========== NaN/Inf CHECK MACRO (FIXED!) ==========
// ✅ FIX: Correct NaN detection (NaN != NaN is TRUE)
// Note: si_isfinite() not available for delfx, using correct macro instead
#define is_finite(x) ((x) != (x) ? false : ((x) <= 1e10f && (x) >= -1e10f))

// ========== FAST TANH ==========
inline float fast_tanh(float x) {
    if (x < -3.f) return -1.f;
    if (x > 3.f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

// ========== MEMORY BUDGET ==========

#define MAX_DELAY_SAMPLES 144000  // 3 seconds @ 48kHz (3MB SDRAM)

// ========== DIRECTION MODES ==========

enum ShiftDirection {
    DIR_OFF = 0,     // No shift (normal delay)
    DIR_UP = 1,      // Shift up in frequency
    DIR_DOWN = 2     // Shift down in frequency
};

// ========== HILBERT TRANSFORM ==========
// Simple 4-tap Hilbert transformer for 90° phase shift

struct HilbertTransform {
    float z1, z2, z3, z4;  // Delay taps
};

static HilbertTransform s_hilbert_l, s_hilbert_r;

// ========== DELAY BUFFER ==========

static float *s_delay_buffer_l = nullptr;
static float *s_delay_buffer_r = nullptr;
static uint32_t s_write_pos = 0;

// ========== FREQUENCY SHIFTER STATE ==========

static float s_shift_phase_l = 0.f;
static float s_shift_phase_r = 0.f;

// ========== TONE FILTER ==========

static float s_tone_z1_l = 0.f;
static float s_tone_z1_r = 0.f;

// ========== WANDER/MODULATION ==========

static float s_wander_phase = 0.f;

// ========== PARAMETERS ==========

static float s_time = 0.5f;              // Delay time (seconds or tempo-synced)
static float s_feedback = 0.6f;          // Feedback amount (0-0.93)
static float s_mix = 0.5f;               // Dry/wet mix
static float s_shift_hz = 5.f;           // Frequency shift in Hz
static ShiftDirection s_direction = DIR_UP;
static float s_tone = 0.4f;              // Tone filter (dark to bright)
static float s_stereo = 0.75f;           // Stereo width
static float s_wander = 0.25f;           // Modulation amount
static uint8_t s_sync = 3;               // Tempo sync (0=OFF, 1-8=divisions)
static float s_lofi = 0.2f;              // Lo-fi/dirt amount

static float s_tempo_bpm = 120.f;

// ========== HILBERT TRANSFORM ==========

inline void hilbert_init(HilbertTransform *h) {
    h->z1 = 0.f;
    h->z2 = 0.f;
    h->z3 = 0.f;
    h->z4 = 0.f;
}

// Hilbert transform: generates 90° phase-shifted version of input
inline float hilbert_process(HilbertTransform *h, float input) {
    // Simple 4-tap FIR Hilbert approximation
    // Coefficients: [0.6, 0.0, -0.6, 0.0] (simplified)
    
    float output = 0.6f * h->z1 - 0.6f * h->z3;
    
    // Shift delays
    h->z4 = h->z3;
    h->z3 = h->z2;
    h->z2 = h->z1;
    h->z1 = input;
    
    // Denormal kill
    if (si_fabsf(output) < 1e-15f) output = 0.f;
    
    return output;
}

// ========== FREQUENCY SHIFTER ==========

inline float frequency_shift(float input, float hilbert_output, 
                             float *phase, float shift_hz) {
    if (si_fabsf(shift_hz) < 0.01f) return input;
    
    // Update oscillator phase
    *phase += shift_hz / 48000.f;
    if (*phase >= 1.f) *phase -= 1.f;
    if (*phase < 0.f) *phase += 1.f;
    
    // Generate quadrature oscillators
    float osc_cos = fx_cosf(*phase * 2.f * 3.14159265f);
    float osc_sin = fx_sinf(*phase * 2.f * 3.14159265f);
    
    // Single-sideband modulation
    // Upper sideband: I*cos - Q*sin
    // Lower sideband: I*cos + Q*sin
    float shifted = input * osc_cos - hilbert_output * osc_sin;
    
    return shifted;
}

// ========== DELAY READ ==========

inline float delay_read(float *buffer, float delay_samples) {
    if (!buffer) return 0.f;
    
    delay_samples = clipminmaxf(1.f, delay_samples, (float)(MAX_DELAY_SAMPLES - 2));
    
    float read_pos_f = (float)s_write_pos - delay_samples;
    while (read_pos_f < 0.f) read_pos_f += (float)MAX_DELAY_SAMPLES;
    
    uint32_t read_pos_0 = (uint32_t)read_pos_f;
    uint32_t read_pos_1 = (read_pos_0 + 1) % MAX_DELAY_SAMPLES;
    float frac = read_pos_f - (float)read_pos_0;
    
    float sample = buffer[read_pos_0] * (1.f - frac) + 
                   buffer[read_pos_1] * frac;
    
    if (!is_finite(sample)) sample = 0.f;
    
    return sample;
}

// ========== TONE FILTER ==========

inline float apply_tone(float input, float *z1) {
    float coeff = 0.3f + s_tone * 0.5f;
    
    *z1 += coeff * (input - *z1);
    
    if (si_fabsf(*z1) < 1e-15f) *z1 = 0.f;
    
    // Tilt EQ: dark = more LP, bright = more HP
    if (s_tone < 0.5f) {
        return *z1;  // Lowpass
    } else {
        float hp = input - *z1;
        return input + hp * ((s_tone - 0.5f) * 2.f);
    }
}

// ========== LO-FI/DIRT ==========

inline float apply_lofi(float input) {
    if (s_lofi < 0.01f) return input;
    
    // Bit crushing
    float bits = 16.f - s_lofi * 12.f;  // 16-bit to 4-bit
    float scale = fx_pow2f(bits);
    float crushed = si_floorf(input * scale + 0.5f) / scale;
    
    // Sample rate reduction effect
    static uint32_t lofi_counter = 0;
    static float lofi_hold = 0.f;
    
    lofi_counter++;
    uint32_t reduction = 1 + (uint32_t)(s_lofi * 7.f);
    
    if (lofi_counter >= reduction) {
        lofi_counter = 0;
        lofi_hold = crushed;
    }
    
    return input * (1.f - s_lofi * 0.7f) + lofi_hold * s_lofi * 0.7f;
}

// ========== WANDER/MODULATION ==========

inline float get_wander_mod() {
    if (s_wander < 0.01f) return 0.f;
    
    // Slow LFO
    float rate = 0.1f + s_wander * 2.9f;  // 0.1-3 Hz
    s_wander_phase += rate / 48000.f;
    if (s_wander_phase >= 1.f) s_wander_phase -= 1.f;
    
    return fx_sinf(s_wander_phase * 2.f * 3.14159265f) * s_wander * 0.15f;
}

// ========== MAIN PROCESSOR ==========

inline void process_shivikutfreq(float in_l, float in_r, float *out_l, float *out_r) {
    // Input validation
    if (!is_finite(in_l)) in_l = 0.f;
    if (!is_finite(in_r)) in_r = 0.f;
    
    // Calculate delay time
    float delay_time = s_time;
    
    if (s_sync > 0) {
        // Tempo sync
        float divisions[] = {
            1.f/16.f, 1.f/8.f, 3.f/16.f, 1.f/4.f,
            3.f/8.f, 1.f/2.f, 3.f/4.f, 1.f/1.f
        };
        float div = divisions[s_sync - 1];
        delay_time = (60.f / s_tempo_bpm) * 4.f * div;
    }
    
    // Add wander modulation
    float wander_mod = get_wander_mod();
    delay_time *= 1.f + wander_mod;
    
    delay_time = clipminmaxf(0.001f, delay_time, 3.f);
    
    float delay_samples = delay_time * 48000.f;
    delay_samples = clipminmaxf(48.f, delay_samples, (float)(MAX_DELAY_SAMPLES - 1));
    
    // Read delayed signal
    float delayed_l = delay_read(s_delay_buffer_l, delay_samples);
    float delayed_r = delay_read(s_delay_buffer_r, delay_samples);
    
    if (!is_finite(delayed_l)) delayed_l = 0.f;
    if (!is_finite(delayed_r)) delayed_r = 0.f;
    
    // Apply frequency shift to delayed signal
    if (s_direction != DIR_OFF && si_fabsf(s_shift_hz) > 0.01f) {
        // Generate Hilbert transform (90° phase shift)
        float hilbert_l = hilbert_process(&s_hilbert_l, delayed_l);
        float hilbert_r = hilbert_process(&s_hilbert_r, delayed_r);
        
        // Determine shift direction
        float shift_amount = s_shift_hz;
        if (s_direction == DIR_DOWN) {
            shift_amount = -shift_amount;
        }
        
        // Apply frequency shift
        delayed_l = frequency_shift(delayed_l, hilbert_l, &s_shift_phase_l, shift_amount);
        delayed_r = frequency_shift(delayed_r, hilbert_r, &s_shift_phase_r, shift_amount);
    }
    
    // Apply tone filter
    delayed_l = apply_tone(delayed_l, &s_tone_z1_l);
    delayed_r = apply_tone(delayed_r, &s_tone_z1_r);
    
    // Apply lo-fi/dirt
    delayed_l = apply_lofi(delayed_l);
    delayed_r = apply_lofi(delayed_r);
    
    // Feedback limiting (CRITICAL for stability!)
    float fb = clipminmaxf(0.f, s_feedback, 0.93f);
    
    // Write to delay buffer with feedback
    float write_l = in_l + delayed_l * fb;
    float write_r = in_r + delayed_r * fb;
    
    // Soft clip feedback
    write_l = fast_tanh(write_l * 0.5f) * 2.f;
    write_r = fast_tanh(write_r * 0.5f) * 2.f;
    
    write_l = clipminmaxf(-2.f, write_l, 2.f);
    write_r = clipminmaxf(-2.f, write_r, 2.f);
    
    if (!is_finite(write_l)) write_l = 0.f;
    if (!is_finite(write_r)) write_r = 0.f;
    
    if (s_delay_buffer_l && s_delay_buffer_r) {
        s_delay_buffer_l[s_write_pos] = write_l;
        s_delay_buffer_r[s_write_pos] = write_r;
    }
    
    // Stereo width
    if (s_stereo != 1.f) {
        float mid = (delayed_l + delayed_r) * 0.5f;
        float side = (delayed_l - delayed_r) * 0.5f;
        
        float width = s_stereo * 2.f;  // 0-200%
        side *= width;
        
        delayed_l = mid + side;
        delayed_r = mid - side;
    }
    
    // Validate output
    if (!is_finite(delayed_l)) delayed_l = 0.f;
    if (!is_finite(delayed_r)) delayed_r = 0.f;
    
    // Mix
    float dry_gain = 1.f - si_fabsf(s_mix);
    float wet_gain = (s_mix + 1.f) * 0.5f;
    
    *out_l = in_l * dry_gain + delayed_l * wet_gain;
    *out_r = in_r * dry_gain + delayed_r * wet_gain;
}

// ========== CALLBACKS ==========

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;
    if (!desc->hooks.sdram_alloc) return k_unit_err_memory;
    
    // Allocate SDRAM
    size_t buffer_size = MAX_DELAY_SAMPLES * sizeof(float) * 2;
    uint8_t *buffer_base = static_cast<uint8_t *>(desc->hooks.sdram_alloc(buffer_size));
    
    if (!buffer_base) return k_unit_err_memory;
    
    s_delay_buffer_l = reinterpret_cast<float *>(buffer_base);
    s_delay_buffer_r = reinterpret_cast<float *>(buffer_base + MAX_DELAY_SAMPLES * sizeof(float));
    
    // Clear buffers
    for (uint32_t i = 0; i < MAX_DELAY_SAMPLES; i++) {
        s_delay_buffer_l[i] = 0.f;
        s_delay_buffer_r[i] = 0.f;
    }
    
    s_write_pos = 0;
    
    // Init Hilbert transforms
    hilbert_init(&s_hilbert_l);
    hilbert_init(&s_hilbert_r);
    
    // Init frequency shifter
    s_shift_phase_l = 0.f;
    s_shift_phase_r = 0.f;
    
    // Init filters
    s_tone_z1_l = 0.f;
    s_tone_z1_r = 0.f;
    
    // Init modulation
    s_wander_phase = 0.f;
    
    // Init parameters
    s_time = 0.5f;
    s_feedback = 0.6f;
    s_mix = 0.5f;
    s_shift_hz = 5.f;
    s_direction = DIR_UP;
    s_tone = 0.4f;
    s_stereo = 0.75f;
    s_wander = 0.25f;
    s_sync = 3;
    s_lofi = 0.2f;
    
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
    
    hilbert_init(&s_hilbert_l);
    hilbert_init(&s_hilbert_r);
    
    s_shift_phase_l = 0.f;
    s_shift_phase_r = 0.f;
    
    s_tone_z1_l = 0.f;
    s_tone_z1_r = 0.f;
    
    s_wander_phase = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    const float *in_ptr = in;
    float *out_ptr = out;
    
    for (uint32_t f = 0; f < frames; f++) {
        float out_l, out_r;
        process_shivikutfreq(in_ptr[0], in_ptr[1], &out_l, &out_r);
        
        out_ptr[0] = clipminmaxf(-1.f, out_l, 1.f);
        out_ptr[1] = clipminmaxf(-1.f, out_r, 1.f);
        
        s_write_pos = (s_write_pos + 1) % MAX_DELAY_SAMPLES;
        
        in_ptr += 2;
        out_ptr += 2;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: // TIME
            s_time = 0.01f + valf * 2.99f;  // 10ms to 3s
            break;
            
        case 1: // FEEDBACK
            s_feedback = valf;
            break;
            
        case 2: // MIX
            s_mix = (float)value / 100.f;
            s_mix = clipminmaxf(-1.f, s_mix, 1.f);
            break;
            
        case 3: // SHIFT HZ
            s_shift_hz = valf * 100.f;  // 0-100 Hz
            break;
            
        case 4: // DIRECTION
            s_direction = (ShiftDirection)value;
            if (s_direction > DIR_DOWN) s_direction = DIR_DOWN;
            break;
            
        case 5: // TONE
            s_tone = valf;
            break;
            
        case 6: // STEREO
            s_stereo = valf;
            break;
            
        case 7: // WANDER
            s_wander = valf;
            break;
            
        case 8: // SYNC
            s_sync = (uint8_t)value;
            break;
            
        case 9: // LOFI
            s_lofi = valf;
            break;
            
        default:
            break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return (int32_t)((s_time - 0.01f) / 2.99f * 1023.f);
        case 1: return (int32_t)(s_feedback * 1023.f);
        case 2: return (int32_t)(s_mix * 100.f);
        case 3: return (int32_t)(s_shift_hz / 100.f * 1023.f);
        case 4: return (int32_t)s_direction;
        case 5: return (int32_t)(s_tone * 1023.f);
        case 6: return (int32_t)(s_stereo * 1023.f);
        case 7: return (int32_t)(s_wander * 1023.f);
        case 8: return (int32_t)s_sync;
        case 9: return (int32_t)(s_lofi * 1023.f);
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    if (id == 4) {
        static const char *dir_names[] = {"OFF", "UP", "DOWN"};
        if (value >= 0 && value < 3) return dir_names[value];
    }
    if (id == 8) {
        static const char *sync_names[] = {
            "OFF", "1/16", "1/8", "3/16", "1/4", "3/8", "1/2", "3/4", "1/1"
        };
        if (value >= 0 && value < 9) return sync_names[value];
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
