/*
    KUTMIST - Warm Hazy Reverb
    
    Cinematic ambient reverb for pads, vocals & textures
    
    ALGORITHM:
    - 16 early reflection taps
    - 4 allpass diffusion network
    - 4 comb filters (smooth tail)
    - SIDE mode (stereo processing)
    - High/Low cut filters
    - Bass boost/cut
    - Natural decay
    
    SOURCES:
    - Schroeder Reverb (1962)
    - Freeverb Algorithm
    - Custom soft diffusion
*/

#include "unit_revfx.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "utils/buffer_ops.h"
#include <algorithm>

#define NUM_COMBS 4
#define NUM_ALLPASS 4
#define NUM_EARLY_TAPS 16
#define PREDELAY_SIZE 24000  // 500ms @ 48kHz

// Comb filter delays (optimized for warmth)
static const uint32_t s_comb_delays[NUM_COMBS] = {
    1557, 1617, 1491, 1422
};

// Allpass filter delays (soft diffusion)
static const uint32_t s_allpass_delays[NUM_ALLPASS] = {
    225, 341, 441, 556
};

// Early reflection taps (warm, natural pattern)
static const uint32_t s_early_taps[NUM_EARLY_TAPS] = {
    240, 480, 720, 960,     // First cluster (5-20ms)
    1440, 1920, 2400, 2880, // Second cluster (30-60ms)
    3840, 4800, 5760, 6720, // Third cluster (80-140ms)
    7680, 8640, 9600, 10560 // Fourth cluster (160-220ms)
};

// Early tap levels (natural decay curve)
static const float s_early_levels[NUM_EARLY_TAPS] = {
    0.8f, 0.75f, 0.7f, 0.65f,
    0.6f, 0.55f, 0.5f, 0.45f,
    0.4f, 0.35f, 0.3f, 0.25f,
    0.2f, 0.15f, 0.1f, 0.05f
};

// ========== FILTER STRUCTURES ==========

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

// ========== GLOBAL STATE ==========

static CombFilter s_combs_l[NUM_COMBS];
static CombFilter s_combs_r[NUM_COMBS];
static AllpassFilter s_allpass_l[NUM_ALLPASS];
static AllpassFilter s_allpass_r[NUM_ALLPASS];
static float *s_predelay_buffer_l;
static float *s_predelay_buffer_r;
static uint32_t s_predelay_write;

// Filter states
static float s_lowcut_z1_l = 0.f;
static float s_lowcut_z1_r = 0.f;
static float s_hicut_z1_l = 0.f;
static float s_hicut_z1_r = 0.f;
static float s_bass_z1_l = 0.f;
static float s_bass_z1_r = 0.f;

// ========== PARAMETERS ==========

static float s_predelay = 0.2f;
static float s_size = 0.6f;
static float s_diffusion = 0.5f;
static float s_decay = 0.6f;
static float s_damping = 0.4f;
static float s_lowcut = 0.1f;
static float s_hicut = 0.8f;
static float s_bass = 0.2f;
static float s_side_mode = 0.0f;
static float s_early_level = 0.4f;

// ========== ALLPASS PROCESSOR ==========

inline float allpass_process(AllpassFilter *ap, float input) {
    uint32_t read_pos = (ap->write_pos + 1) % ap->delay_length;
    if (read_pos >= ap->delay_length) read_pos = 0;
    
    float delayed = ap->buffer[read_pos];
    
    float output = -input + delayed;
    ap->buffer[ap->write_pos] = input + delayed * ap->feedback;
    
    ap->write_pos = (ap->write_pos + 1) % ap->delay_length;
    if (ap->write_pos >= ap->delay_length) ap->write_pos = 0;
    
    return output;
}

// ========== COMB PROCESSOR ==========

inline float comb_process(CombFilter *cf, float input) {
    uint32_t read_pos = (cf->write_pos + 1) % cf->delay_length;
    if (read_pos >= cf->delay_length) read_pos = 0;
    
    float delayed = cf->buffer[read_pos];
    
    // One-pole lowpass damping
    cf->damp_z = delayed * (1.f - cf->damp_coeff) + cf->damp_z * cf->damp_coeff;
    
    cf->buffer[cf->write_pos] = input + cf->damp_z * cf->feedback;
    cf->write_pos = (cf->write_pos + 1) % cf->delay_length;
    if (cf->write_pos >= cf->delay_length) cf->write_pos = 0;
    
    return delayed;
}

// ========== EARLY REFLECTIONS ==========

inline float process_early_reflections_l(float level) {
    if (level < 0.01f) return 0.f;
    
    float output = 0.f;
    
    for (int i = 0; i < NUM_EARLY_TAPS; i++) {
        uint32_t tap_pos = (s_predelay_write + PREDELAY_SIZE - s_early_taps[i]) % PREDELAY_SIZE;
        if (tap_pos >= PREDELAY_SIZE) tap_pos = 0;
        
        float tap = s_predelay_buffer_l[tap_pos];
        output += tap * s_early_levels[i] * level;
    }
    
    return output;
}

inline float process_early_reflections_r(float level) {
    if (level < 0.01f) return 0.f;
    
    float output = 0.f;
    
    for (int i = 0; i < NUM_EARLY_TAPS; i++) {
        // Offset right channel taps slightly for stereo width
        uint32_t offset = s_early_taps[i] + 37;  // Prime number offset
        uint32_t tap_pos = (s_predelay_write + PREDELAY_SIZE - offset) % PREDELAY_SIZE;
        if (tap_pos >= PREDELAY_SIZE) tap_pos = 0;
        
        float tap = s_predelay_buffer_r[tap_pos];
        output += tap * s_early_levels[i] * level;
    }
    
    return output;
}

// ========== LOW CUT FILTER ==========

inline void process_lowcut(float *input_l, float *input_r) {
    if (s_lowcut < 0.01f) return;
    
    // High-pass filter (removes low frequencies)
    // Cutoff: 20Hz to 500Hz
    float cutoff = 20.f + s_lowcut * 480.f;
    float w = 2.f * 3.14159265f * cutoff / 48000.f;
    float coeff = 1.f - w;
    coeff = clipminmaxf(0.8f, coeff, 0.999f);
    
    // One-pole HP
    *input_l = *input_l - s_lowcut_z1_l;
    s_lowcut_z1_l += coeff * (*input_l - s_lowcut_z1_l);
    
    *input_r = *input_r - s_lowcut_z1_r;
    s_lowcut_z1_r += coeff * (*input_r - s_lowcut_z1_r);
    
    // Denormal kill
    if (si_fabsf(s_lowcut_z1_l) < 1e-15f) s_lowcut_z1_l = 0.f;
    if (si_fabsf(s_lowcut_z1_r) < 1e-15f) s_lowcut_z1_r = 0.f;
}

// ========== HIGH CUT FILTER ==========

inline void process_hicut(float *input_l, float *input_r) {
    if (s_hicut > 0.99f) return;
    
    // Low-pass filter (removes high frequencies)
    // Cutoff: 1kHz to 20kHz
    float cutoff = 1000.f + s_hicut * 19000.f;
    float w = 2.f * 3.14159265f * cutoff / 48000.f;
    float coeff = 1.f - w;
    coeff = clipminmaxf(0.1f, coeff, 0.95f);
    
    // One-pole LP
    s_hicut_z1_l += coeff * (*input_l - s_hicut_z1_l);
    s_hicut_z1_r += coeff * (*input_r - s_hicut_z1_r);
    
    *input_l = s_hicut_z1_l;
    *input_r = s_hicut_z1_r;
    
    // Denormal kill
    if (si_fabsf(s_hicut_z1_l) < 1e-15f) s_hicut_z1_l = 0.f;
    if (si_fabsf(s_hicut_z1_r) < 1e-15f) s_hicut_z1_r = 0.f;
}

// ========== BASS BOOST/CUT ==========

inline void process_bass(float *input_l, float *input_r) {
    if (si_fabsf(s_bass) < 0.01f) return;
    
    // Low shelf filter
    // Crossover: 200 Hz
    float w = 2.f * 3.14159265f * 200.f / 48000.f;
    float coeff = 1.f - w;
    coeff = clipminmaxf(0.8f, coeff, 0.95f);
    
    // Extract bass
    s_bass_z1_l += coeff * (*input_l - s_bass_z1_l);
    s_bass_z1_r += coeff * (*input_r - s_bass_z1_r);
    
    // Apply boost/cut
    float bass_gain = 1.f + s_bass;
    bass_gain = clipminmaxf(0.5f, bass_gain, 1.5f);
    
    *input_l = *input_l + (s_bass_z1_l - *input_l * 0.5f) * (bass_gain - 1.f);
    *input_r = *input_r + (s_bass_z1_r - *input_r * 0.5f) * (bass_gain - 1.f);
    
    // Denormal kill
    if (si_fabsf(s_bass_z1_l) < 1e-15f) s_bass_z1_l = 0.f;
    if (si_fabsf(s_bass_z1_r) < 1e-15f) s_bass_z1_r = 0.f;
}

// ========== SIDE MODE PROCESSOR ==========

inline void extract_side(float in_l, float in_r, float *mid, float *side) {
    *mid = (in_l + in_r) * 0.5f;
    *side = (in_l - in_r) * 0.5f;
}

inline void combine_mid_side(float mid, float side, float *out_l, float *out_r) {
    *out_l = mid + side;
    *out_r = mid - side;
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
    
    // Calculate buffer sizes
    uint32_t max_comb_size = 0;
    for (int i = 0; i < NUM_COMBS; i++) {
        if (s_comb_delays[i] > max_comb_size) max_comb_size = s_comb_delays[i];
    }
    max_comb_size = (uint32_t)((float)max_comb_size * 2.5f);  // Size scaling headroom
    
    uint32_t max_allpass_size = 0;
    for (int i = 0; i < NUM_ALLPASS; i++) {
        if (s_allpass_delays[i] > max_allpass_size) max_allpass_size = s_allpass_delays[i];
    }
    max_allpass_size = (uint32_t)((float)max_allpass_size * 2.5f);
    
    // Total memory needed
    uint32_t total_size = 0;
    total_size += (NUM_COMBS * max_comb_size) * sizeof(float) * 2;  // L+R combs
    total_size += (NUM_ALLPASS * max_allpass_size) * sizeof(float) * 2;  // L+R allpass
    total_size += PREDELAY_SIZE * sizeof(float) * 2;  // L+R predelay
    
    // Allocate SDRAM
    uint8_t *buffer_base = static_cast<uint8_t *>(desc->hooks.sdram_alloc(total_size));
    if (!buffer_base) return k_unit_err_memory;
    
    uint32_t offset = 0;
    
    // Allocate comb buffers (Left)
    float *comb_buf_l = reinterpret_cast<float *>(buffer_base + offset);
    offset += NUM_COMBS * max_comb_size * sizeof(float);
    
    // Allocate comb buffers (Right)
    float *comb_buf_r = reinterpret_cast<float *>(buffer_base + offset);
    offset += NUM_COMBS * max_comb_size * sizeof(float);
    
    // Allocate allpass buffers (Left)
    float *allpass_buf_l = reinterpret_cast<float *>(buffer_base + offset);
    offset += NUM_ALLPASS * max_allpass_size * sizeof(float);
    
    // Allocate allpass buffers (Right)
    float *allpass_buf_r = reinterpret_cast<float *>(buffer_base + offset);
    offset += NUM_ALLPASS * max_allpass_size * sizeof(float);
    
    // Allocate predelay buffers
    s_predelay_buffer_l = reinterpret_cast<float *>(buffer_base + offset);
    offset += PREDELAY_SIZE * sizeof(float);
    
    s_predelay_buffer_r = reinterpret_cast<float *>(buffer_base + offset);
    offset += PREDELAY_SIZE * sizeof(float);
    
    // Clear all buffers
    buf_clr_f32(comb_buf_l, NUM_COMBS * max_comb_size);
    buf_clr_f32(comb_buf_r, NUM_COMBS * max_comb_size);
    buf_clr_f32(allpass_buf_l, NUM_ALLPASS * max_allpass_size);
    buf_clr_f32(allpass_buf_r, NUM_ALLPASS * max_allpass_size);
    buf_clr_f32(s_predelay_buffer_l, PREDELAY_SIZE);
    buf_clr_f32(s_predelay_buffer_r, PREDELAY_SIZE);
    
    // Initialize comb filters
    uint32_t comb_offset = 0;
    for (int i = 0; i < NUM_COMBS; i++) {
        s_combs_l[i].write_pos = 0;
        s_combs_l[i].delay_length = s_comb_delays[i];
        s_combs_l[i].feedback = 0.84f;
        s_combs_l[i].damp_z = 0.f;
        s_combs_l[i].damp_coeff = 0.2f;
        s_combs_l[i].buffer = comb_buf_l + comb_offset;
        
        s_combs_r[i].write_pos = 0;
        s_combs_r[i].delay_length = s_comb_delays[i] + 23;  // Stereo offset
        s_combs_r[i].feedback = 0.84f;
        s_combs_r[i].damp_z = 0.f;
        s_combs_r[i].damp_coeff = 0.2f;
        s_combs_r[i].buffer = comb_buf_r + comb_offset;
        
        comb_offset += max_comb_size;
    }
    
    // Initialize allpass filters
    uint32_t allpass_offset = 0;
    for (int i = 0; i < NUM_ALLPASS; i++) {
        s_allpass_l[i].write_pos = 0;
        s_allpass_l[i].delay_length = s_allpass_delays[i];
        s_allpass_l[i].feedback = 0.5f;
        s_allpass_l[i].buffer = allpass_buf_l + allpass_offset;
        
        s_allpass_r[i].write_pos = 0;
        s_allpass_r[i].delay_length = s_allpass_delays[i] + 17;  // Stereo offset
        s_allpass_r[i].feedback = 0.5f;
        s_allpass_r[i].buffer = allpass_buf_r + allpass_offset;
        
        allpass_offset += max_allpass_size;
    }
    
    s_predelay_write = 0;
    
    // Clear filter states
    s_lowcut_z1_l = 0.f;
    s_lowcut_z1_r = 0.f;
    s_hicut_z1_l = 0.f;
    s_hicut_z1_r = 0.f;
    s_bass_z1_l = 0.f;
    s_bass_z1_r = 0.f;
    
    // Init parameters
    s_predelay = 0.2f;
    s_size = 0.6f;
    s_diffusion = 0.5f;
    s_decay = 0.6f;
    s_damping = 0.4f;
    s_lowcut = 0.1f;
    s_hicut = 0.8f;
    s_bass = 0.2f;
    s_side_mode = 0.0f;
    s_early_level = 0.4f;
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
    
    s_lowcut_z1_l = 0.f;
    s_lowcut_z1_r = 0.f;
    s_hicut_z1_l = 0.f;
    s_hicut_z1_r = 0.f;
    s_bass_z1_l = 0.f;
    s_bass_z1_r = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames)
{
    for (uint32_t f = 0; f < frames; f++) {
        float in_l = in[f * 2];
        float in_r = in[f * 2 + 1];
        
        // SIDE mode processing
        float process_l, process_r;
        if (s_side_mode > 0.01f) {
            float mid, side;
            extract_side(in_l, in_r, &mid, &side);
            
            // Apply reverb only to side signal
            float side_wet = side * s_side_mode;
            float mid_dry = mid * (1.f - s_side_mode * 0.5f);
            
            process_l = mid_dry + side_wet;
            process_r = mid_dry - side_wet;
        } else {
            process_l = in_l;
            process_r = in_r;
        }
        
        // Apply low cut
        process_lowcut(&process_l, &process_r);
        
        // Pre-delay
        float predelay_time_samps = s_predelay * (float)PREDELAY_SIZE;
        uint32_t predelay_read = (s_predelay_write + PREDELAY_SIZE - (uint32_t)predelay_time_samps) % PREDELAY_SIZE;
        if (predelay_read >= PREDELAY_SIZE) predelay_read = 0;
        
        float predelayed_l = s_predelay_buffer_l[predelay_read];
        float predelayed_r = s_predelay_buffer_r[predelay_read];
        
        s_predelay_buffer_l[s_predelay_write] = process_l;
        s_predelay_buffer_r[s_predelay_write] = process_r;
        s_predelay_write = (s_predelay_write + 1) % PREDELAY_SIZE;
        if (s_predelay_write >= PREDELAY_SIZE) s_predelay_write = 0;
        
        // Early reflections
        float early_l = process_early_reflections_l(s_early_level);
        float early_r = process_early_reflections_r(s_early_level);
        
        // Update comb parameters based on SIZE and DECAY
        float size_scale = 0.5f + s_size * 1.5f;
        float fb = 0.7f + s_decay * 0.28f;
        fb = clipminmaxf(0.1f, fb, 0.98f);
        
        for (int i = 0; i < NUM_COMBS; i++) {
            uint32_t new_len_l = (uint32_t)((float)s_comb_delays[i] * size_scale);
            uint32_t new_len_r = (uint32_t)((float)(s_comb_delays[i] + 23) * size_scale);
            
            // Clamp to buffer size
            uint32_t max_size = (uint32_t)((float)s_comb_delays[i] * 2.5f);
            s_combs_l[i].delay_length = clipminmaxu32(100, new_len_l, max_size);
            s_combs_r[i].delay_length = clipminmaxu32(100, new_len_r, max_size);
            
            s_combs_l[i].feedback = fb;
            s_combs_r[i].feedback = fb;
            
            s_combs_l[i].damp_coeff = 0.1f + s_damping * 0.7f;
            s_combs_r[i].damp_coeff = 0.1f + s_damping * 0.7f;
        }
        
        // Update allpass diffusion
        float diffusion_fb = 0.3f + s_diffusion * 0.4f;
        for (int i = 0; i < NUM_ALLPASS; i++) {
            s_allpass_l[i].feedback = diffusion_fb;
            s_allpass_r[i].feedback = diffusion_fb;
        }
        
        // Comb filters (parallel)
        float comb_out_l = 0.f;
        float comb_out_r = 0.f;
        
        for (int i = 0; i < NUM_COMBS; i++) {
            comb_out_l += comb_process(&s_combs_l[i], predelayed_l);
            comb_out_r += comb_process(&s_combs_r[i], predelayed_r);
        }
        comb_out_l /= (float)NUM_COMBS;
        comb_out_r /= (float)NUM_COMBS;
        
        // Allpass diffusion (series)
        for (int i = 0; i < NUM_ALLPASS; i++) {
            comb_out_l = allpass_process(&s_allpass_l[i], comb_out_l);
            comb_out_r = allpass_process(&s_allpass_r[i], comb_out_r);
        }
        
        // Combine early + late
        float wet_l = early_l + comb_out_l * 0.7f;
        float wet_r = early_r + comb_out_r * 0.7f;
        
        // Apply high cut
        process_hicut(&wet_l, &wet_r);
        
        // Apply bass boost/cut
        process_bass(&wet_l, &wet_r);
        
        // Output (revfx uses MIX parameter automatically via hardware)
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
        case 2: // Diffusion (MIX parameter: -100 to +100)
            s_diffusion = (float)(value + 100) / 200.f;  // Normalize to 0-1
            break;
        case 3: // Decay
            s_decay = valf;
            break;
        case 4: // Damping
            s_damping = valf;
            break;
        case 5: // Low Cut
            s_lowcut = valf;
            break;
        case 6: // High Cut
            s_hicut = valf;
            break;
        case 7: // Bass (-100 to +100)
            s_bass = (float)value / 100.f;
            break;
        case 8: // SIDE mode
            s_side_mode = valf;
            break;
        case 9: // Early Level
            s_early_level = valf;
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
        case 2: return (int32_t)(s_diffusion * 200.f - 100.f);
        case 3: return (int32_t)(s_decay * 1023.f);
        case 4: return (int32_t)(s_damping * 1023.f);
        case 5: return (int32_t)(s_lowcut * 1023.f);
        case 6: return (int32_t)(s_hicut * 1023.f);
        case 7: return (int32_t)(s_bass * 100.f);
        case 8: return (int32_t)(s_side_mode * 1023.f);
        case 9: return (int32_t)(s_early_level * 1023.f);
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

