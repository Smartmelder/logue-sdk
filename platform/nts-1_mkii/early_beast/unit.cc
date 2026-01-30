/*
    EARLY BEAST - Ultimate Early Reflections Reverb (FIXED!)
    
    CRITICAL FIX:
    - Random generator now ONLY updates per step (not per sample!)
    - Pre-calculated random offsets stored per tap
    - Stable, noise-free operation
    
    Inspired by Relab LX480 ambience algorithm
*/

#include "unit_revfx.h"
#include "fx_api.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "utils/buffer_ops.h"

#define NUM_EARLY_TAPS 16
#define NUM_LATE_TAPS 4
#define PREDELAY_SIZE 4800    // 100ms @ 48kHz
#define EARLY_BUFFER_SIZE 8640  // 180ms @ 48kHz
#define LATE_BUFFER_SIZE 12000  // 250ms @ 48kHz

// ========== EARLY REFLECTION TAP PATTERN ==========

// Base pattern (30-180ms window, in samples)
static const uint32_t s_early_tap_base[NUM_EARLY_TAPS] = {
    1440,  // 30ms
    1920,  // 40ms
    2400,  // 50ms
    2880,  // 60ms
    3360,  // 70ms
    3840,  // 80ms
    4320,  // 90ms
    4800,  // 100ms
    5280,  // 110ms
    5760,  // 120ms
    6240,  // 130ms
    6720,  // 140ms
    7200,  // 150ms
    7680,  // 160ms
    8160,  // 170ms
    8640   // 180ms
};

// Decay curve (natural energy decay)
static const float s_early_tap_levels[NUM_EARLY_TAPS] = {
    0.9f, 0.85f, 0.8f, 0.75f,
    0.7f, 0.65f, 0.6f, 0.55f,
    0.5f, 0.45f, 0.4f, 0.35f,
    0.3f, 0.25f, 0.2f, 0.15f
};

// ========== LATE DIFFUSION TAPS ==========

static const uint32_t s_late_taps[NUM_LATE_TAPS] = {
    3000, 4500, 6000, 9000
};

// ========== GLOBAL BUFFERS ==========

static float *s_predelay_buffer_l = nullptr;
static float *s_predelay_buffer_r = nullptr;
static float *s_early_buffer_l = nullptr;
static float *s_early_buffer_r = nullptr;
static float *s_late_buffer_l = nullptr;
static float *s_late_buffer_r = nullptr;

static uint32_t s_predelay_write = 0;
static uint32_t s_early_write = 0;
static uint32_t s_late_write = 0;

// ========== MODULATION ==========

static float s_spin_phase = 0.f;
static float s_wander_phase = 0.f;

// ========== FILTERS ==========

static float s_bright_z1_l = 0.f;
static float s_bright_z1_r = 0.f;
static float s_low_z1_l = 0.f;
static float s_low_z1_r = 0.f;

// ========== RANDOM STATE ==========

static uint32_t s_rand_state = 12345;

// ✅ FIX: Pre-calculated random offsets (updated slowly)
static float s_tap_random_offsets_l[NUM_EARLY_TAPS];
static float s_tap_random_offsets_r[NUM_EARLY_TAPS];
static uint32_t s_random_update_counter = 0;

// ========== PARAMETERS ==========

static float s_predelay = 0.1f;
static float s_size = 0.5f;
static float s_density = 0.6f;
static float s_spin = 0.3f;
static float s_wander = 0.4f;
static float s_low_mult = 0.6f;
static float s_diffusion = 0.2f;
static float s_width = 0.75f;
static float s_brightness = 0.6f;
static float s_late_mix = 0.3f;

// ========== RANDOM GENERATOR ==========

inline float random_float() {
    s_rand_state ^= s_rand_state << 13;
    s_rand_state ^= s_rand_state >> 17;
    s_rand_state ^= s_rand_state << 5;
    return (float)(s_rand_state % 10000) / 10000.f;
}

// ✅ FIX: Update random offsets slowly (not per sample!)
inline void update_random_offsets() {
    // Only update every 4800 samples (~100ms @ 48kHz)
    s_random_update_counter++;
    if (s_random_update_counter < 4800) return;
    s_random_update_counter = 0;
    
    // Generate new random offsets
    for (int i = 0; i < NUM_EARLY_TAPS; i++) {
        s_tap_random_offsets_l[i] = (random_float() - 0.5f) * s_density * 200.f;
        s_tap_random_offsets_r[i] = (random_float() - 0.5f) * s_density * 200.f;
    }
}

// ========== MODULATION LFOs ==========

inline float get_spin_modulation() {
    if (s_spin < 0.01f) return 0.f;
    
    // Spin rate: 0.1 to 5 Hz
    float rate = 0.1f + s_spin * 4.9f;
    s_spin_phase += rate / 48000.f;
    if (s_spin_phase >= 1.f) s_spin_phase -= 1.f;
    
    return fx_sinf(s_spin_phase * 2.f * 3.14159265f);
}

inline float get_wander_modulation() {
    if (s_wander < 0.01f) return 0.f;
    
    // Wander: slow, gentle drift (0.05 to 0.5 Hz)
    float rate = 0.05f + s_wander * 0.45f;
    s_wander_phase += rate / 48000.f;
    if (s_wander_phase >= 1.f) s_wander_phase -= 1.f;
    
    // Triangle wave (more natural drift)
    float triangle = (s_wander_phase < 0.5f) ? 
                     (4.f * s_wander_phase - 1.f) : 
                     (3.f - 4.f * s_wander_phase);
    
    return triangle;
}

// ========== EARLY REFLECTIONS PROCESSOR ==========

inline float process_early_reflections_l() {
    float output = 0.f;
    
    // Get modulation
    float spin = get_spin_modulation();
    float wander = get_wander_modulation();
    
    // Combined modulation
    float mod = spin * s_spin * 0.1f + wander * s_wander * 0.05f;
    
    for (int i = 0; i < NUM_EARLY_TAPS; i++) {
        // Base tap time (scaled by size)
        uint32_t tap_time = (uint32_t)((float)s_early_tap_base[i] * 
                                       (0.5f + s_size * 1.0f));
        
        // ✅ FIX: Use pre-calculated random offset (NOT per-sample random!)
        tap_time = (uint32_t)((float)tap_time + s_tap_random_offsets_l[i]);
        
        // Add modulation
        float mod_offset = mod * 100.f;
        tap_time = (uint32_t)((float)tap_time + mod_offset);
        
        // Clamp to buffer size
        tap_time = clipminmaxu32(100, tap_time, EARLY_BUFFER_SIZE - 1);
        
        // Read tap
        uint32_t read_pos = (s_early_write + EARLY_BUFFER_SIZE - tap_time) % EARLY_BUFFER_SIZE;
        float tap = s_early_buffer_l[read_pos];
        
        // Apply level with natural decay
        output += tap * s_early_tap_levels[i];
    }
    
    return output / (float)NUM_EARLY_TAPS;
}

inline float process_early_reflections_r() {
    float output = 0.f;
    
    float spin = get_spin_modulation();
    float wander = get_wander_modulation();
    float mod = spin * s_spin * 0.1f + wander * s_wander * 0.05f;
    
    for (int i = 0; i < NUM_EARLY_TAPS; i++) {
        // Offset right channel taps for stereo width
        uint32_t tap_time = (uint32_t)((float)s_early_tap_base[i] * 
                                       (0.5f + s_size * 1.0f));
        
        // Add stereo offset (prime number)
        tap_time += 37;
        
        // ✅ FIX: Use pre-calculated random offset
        tap_time = (uint32_t)((float)tap_time + s_tap_random_offsets_r[i]);
        
        float mod_offset = mod * 100.f;
        tap_time = (uint32_t)((float)tap_time + mod_offset);
        
        tap_time = clipminmaxu32(100, tap_time, EARLY_BUFFER_SIZE - 1);
        
        uint32_t read_pos = (s_early_write + EARLY_BUFFER_SIZE - tap_time) % EARLY_BUFFER_SIZE;
        float tap = s_early_buffer_r[read_pos];
        
        output += tap * s_early_tap_levels[i];
    }
    
    return output / (float)NUM_EARLY_TAPS;
}

// ========== LATE DIFFUSION ==========

inline float process_late_diffusion_l() {
    if (s_diffusion < 0.01f) return 0.f;
    
    float output = 0.f;
    
    for (int i = 0; i < NUM_LATE_TAPS; i++) {
        uint32_t tap_time = s_late_taps[i];
        uint32_t read_pos = (s_late_write + LATE_BUFFER_SIZE - tap_time) % LATE_BUFFER_SIZE;
        
        float tap = s_late_buffer_l[read_pos];
        float decay = 1.f - ((float)i / (float)NUM_LATE_TAPS) * 0.5f;
        
        output += tap * decay;
    }
    
    return output / (float)NUM_LATE_TAPS * s_diffusion;
}

inline float process_late_diffusion_r() {
    if (s_diffusion < 0.01f) return 0.f;
    
    float output = 0.f;
    
    for (int i = 0; i < NUM_LATE_TAPS; i++) {
        uint32_t tap_time = s_late_taps[i] + 23;  // Stereo offset
        uint32_t read_pos = (s_late_write + LATE_BUFFER_SIZE - tap_time) % LATE_BUFFER_SIZE;
        
        float tap = s_late_buffer_r[read_pos];
        float decay = 1.f - ((float)i / (float)NUM_LATE_TAPS) * 0.5f;
        
        output += tap * decay;
    }
    
    return output / (float)NUM_LATE_TAPS * s_diffusion;
}

// ========== BRIGHTNESS CONTROL ==========

inline void process_brightness(float *l, float *r) {
    // High-shelf filter
    float coeff = 0.3f + s_brightness * 0.4f;
    
    float hp_l = *l - s_bright_z1_l;
    s_bright_z1_l += coeff * (*l - s_bright_z1_l);
    
    float hp_r = *r - s_bright_z1_r;
    s_bright_z1_r += coeff * (*r - s_bright_z1_r);
    
    // Mix based on brightness
    float bright_amount = (s_brightness - 0.5f) * 2.f;  // -1 to +1
    bright_amount = clipminmaxf(-1.f, bright_amount, 1.f);
    
    if (bright_amount > 0.f) {
        *l = *l + hp_l * bright_amount * 0.5f;
        *r = *r + hp_r * bright_amount * 0.5f;
    } else {
        *l = s_bright_z1_l + *l * (1.f + bright_amount);
        *r = s_bright_z1_r + *r * (1.f + bright_amount);
    }
    
    // Denormal kill
    if (si_fabsf(s_bright_z1_l) < 1e-15f) s_bright_z1_l = 0.f;
    if (si_fabsf(s_bright_z1_r) < 1e-15f) s_bright_z1_r = 0.f;
}

// ========== LOW-FREQUENCY MULTIPLIER ==========

inline void process_low_multiplier(float *l, float *r) {
    if (s_low_mult < 0.01f) return;
    
    // Extract low frequencies (crossover ~200 Hz)
    float coeff = 0.9f;
    
    s_low_z1_l += coeff * (*l - s_low_z1_l);
    s_low_z1_r += coeff * (*r - s_low_z1_r);
    
    // Apply multiplier to bass (width and richness)
    float bass_gain = 1.f + s_low_mult * 0.5f;
    
    *l = *l + (s_low_z1_l - *l * 0.5f) * (bass_gain - 1.f);
    *r = *r + (s_low_z1_r - *r * 0.5f) * (bass_gain - 1.f);
    
    // Denormal kill
    if (si_fabsf(s_low_z1_l) < 1e-15f) s_low_z1_l = 0.f;
    if (si_fabsf(s_low_z1_r) < 1e-15f) s_low_z1_r = 0.f;
}

// ========== STEREO WIDTH ==========

inline void process_width(float *l, float *r) {
    // M/S processing
    float mid = (*l + *r) * 0.5f;
    float side = (*l - *r) * 0.5f;
    
    // Apply width (0-200%)
    float width_amount = s_width * 2.f;
    side *= width_amount;
    
    *l = mid + side;
    *r = mid - side;
}

// ========== UNIT CALLBACKS ==========

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc)
{
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;

    if (!desc->hooks.sdram_alloc) return k_unit_err_memory;
    
    // Calculate total buffer size
    size_t total_size = 0;
    total_size += PREDELAY_SIZE * sizeof(float) * 2;      // L+R predelay
    total_size += EARLY_BUFFER_SIZE * sizeof(float) * 2;  // L+R early
    total_size += LATE_BUFFER_SIZE * sizeof(float) * 2;   // L+R late
    
    // Allocate SDRAM
    uint8_t *buffer_base = static_cast<uint8_t *>(desc->hooks.sdram_alloc(total_size));
    if (!buffer_base) return k_unit_err_memory;
    
    uint32_t offset = 0;
    
    // Assign buffers
    s_predelay_buffer_l = reinterpret_cast<float *>(buffer_base + offset);
    offset += PREDELAY_SIZE * sizeof(float);
    
    s_predelay_buffer_r = reinterpret_cast<float *>(buffer_base + offset);
    offset += PREDELAY_SIZE * sizeof(float);
    
    s_early_buffer_l = reinterpret_cast<float *>(buffer_base + offset);
    offset += EARLY_BUFFER_SIZE * sizeof(float);
    
    s_early_buffer_r = reinterpret_cast<float *>(buffer_base + offset);
    offset += EARLY_BUFFER_SIZE * sizeof(float);
    
    s_late_buffer_l = reinterpret_cast<float *>(buffer_base + offset);
    offset += LATE_BUFFER_SIZE * sizeof(float);
    
    s_late_buffer_r = reinterpret_cast<float *>(buffer_base + offset);
    offset += LATE_BUFFER_SIZE * sizeof(float);
    
    // Clear all buffers
    buf_clr_f32(s_predelay_buffer_l, PREDELAY_SIZE);
    buf_clr_f32(s_predelay_buffer_r, PREDELAY_SIZE);
    buf_clr_f32(s_early_buffer_l, EARLY_BUFFER_SIZE);
    buf_clr_f32(s_early_buffer_r, EARLY_BUFFER_SIZE);
    buf_clr_f32(s_late_buffer_l, LATE_BUFFER_SIZE);
    buf_clr_f32(s_late_buffer_r, LATE_BUFFER_SIZE);
    
    // Init positions
    s_predelay_write = 0;
    s_early_write = 0;
    s_late_write = 0;
    
    // Init modulation
    s_spin_phase = 0.f;
    s_wander_phase = 0.25f;  // Phase offset
    
    // Init filters
    s_bright_z1_l = 0.f;
    s_bright_z1_r = 0.f;
    s_low_z1_l = 0.f;
    s_low_z1_r = 0.f;
    
    // ✅ FIX: Initialize random offsets
    for (int i = 0; i < NUM_EARLY_TAPS; i++) {
        s_tap_random_offsets_l[i] = 0.f;
        s_tap_random_offsets_r[i] = 0.f;
    }
    s_random_update_counter = 0;
    
    // Init parameters
    s_predelay = 0.1f;
    s_size = 0.5f;
    s_density = 0.6f;
    s_spin = 0.3f;
    s_wander = 0.4f;
    s_low_mult = 0.6f;
    s_diffusion = 0.2f;
    s_width = 0.75f;
    s_brightness = 0.6f;
    s_late_mix = 0.3f;

    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset()
{
    if (s_predelay_buffer_l) buf_clr_f32(s_predelay_buffer_l, PREDELAY_SIZE);
    if (s_predelay_buffer_r) buf_clr_f32(s_predelay_buffer_r, PREDELAY_SIZE);
    if (s_early_buffer_l) buf_clr_f32(s_early_buffer_l, EARLY_BUFFER_SIZE);
    if (s_early_buffer_r) buf_clr_f32(s_early_buffer_r, EARLY_BUFFER_SIZE);
    if (s_late_buffer_l) buf_clr_f32(s_late_buffer_l, LATE_BUFFER_SIZE);
    if (s_late_buffer_r) buf_clr_f32(s_late_buffer_r, LATE_BUFFER_SIZE);
    
    s_predelay_write = 0;
    s_early_write = 0;
    s_late_write = 0;
    
    s_bright_z1_l = 0.f;
    s_bright_z1_r = 0.f;
    s_low_z1_l = 0.f;
    s_low_z1_r = 0.f;
    
    // ✅ Reset random offsets
    for (int i = 0; i < NUM_EARLY_TAPS; i++) {
        s_tap_random_offsets_l[i] = 0.f;
        s_tap_random_offsets_r[i] = 0.f;
    }
    s_random_update_counter = 0;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames)
{
    // ✅ FIX: Update random offsets slowly
    update_random_offsets();
    
    for (uint32_t f = 0; f < frames; f++) {
        float in_l = in[f * 2];
        float in_r = in[f * 2 + 1];
        
        // Pre-delay
        uint32_t predelay_samples = (uint32_t)(s_predelay * (float)PREDELAY_SIZE);
        uint32_t predelay_read = (s_predelay_write + PREDELAY_SIZE - predelay_samples) % PREDELAY_SIZE;
        
        float predelayed_l = s_predelay_buffer_l[predelay_read];
        float predelayed_r = s_predelay_buffer_r[predelay_read];
        
        s_predelay_buffer_l[s_predelay_write] = in_l;
        s_predelay_buffer_r[s_predelay_write] = in_r;
        s_predelay_write = (s_predelay_write + 1) % PREDELAY_SIZE;
        
        // Write to early reflection buffers
        s_early_buffer_l[s_early_write] = predelayed_l;
        s_early_buffer_r[s_early_write] = predelayed_r;
        s_early_write = (s_early_write + 1) % EARLY_BUFFER_SIZE;
        
        // Process early reflections
        float early_l = process_early_reflections_l();
        float early_r = process_early_reflections_r();
        
        // Write to late buffer
        s_late_buffer_l[s_late_write] = early_l;
        s_late_buffer_r[s_late_write] = early_r;
        s_late_write = (s_late_write + 1) % LATE_BUFFER_SIZE;
        
        // Process late diffusion
        float late_l = process_late_diffusion_l();
        float late_r = process_late_diffusion_r();
        
        // Combine early + late
        float wet_l = early_l + late_l * s_late_mix;
        float wet_r = early_r + late_r * s_late_mix;
        
        // Apply brightness
        process_brightness(&wet_l, &wet_r);
        
        // Apply low-frequency multiplier
        process_low_multiplier(&wet_l, &wet_r);
        
        // Apply stereo width
        process_width(&wet_l, &wet_r);
        
        // Output (revfx uses MIX automatically via hardware)
        out[f * 2] = wet_l;
        out[f * 2 + 1] = wet_r;
    }
}

// ========== PARAMETER HANDLING ==========

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: // Pre-delay
            s_predelay = valf;
            break;
        case 1: // Size
            s_size = valf;
            break;
        case 2: // Density (-100 to +100)
            s_density = (float)(value + 100) / 200.f;
            break;
        case 3: // Spin
            s_spin = valf;
            break;
        case 4: // Wander
            s_wander = valf;
            break;
        case 5: // Low Mult
            s_low_mult = valf;
            break;
        case 6: // Diffusion
            s_diffusion = valf;
            break;
        case 7: // Width
            s_width = valf;
            break;
        case 8: // Brightness
            s_brightness = valf;
            break;
        case 9: // Late Mix
            s_late_mix = valf;
            break;
        default:
            break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0: return (int32_t)(s_predelay * 1023.f);
        case 1: return (int32_t)(s_size * 1023.f);
        case 2: return (int32_t)(s_density * 200.f - 100.f);
        case 3: return (int32_t)(s_spin * 1023.f);
        case 4: return (int32_t)(s_wander * 1023.f);
        case 5: return (int32_t)(s_low_mult * 1023.f);
        case 6: return (int32_t)(s_diffusion * 1023.f);
        case 7: return (int32_t)(s_width * 1023.f);
        case 8: return (int32_t)(s_brightness * 1023.f);
        case 9: return (int32_t)(s_late_mix * 1023.f);
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
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

