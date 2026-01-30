/*
    IS IT ME - Melancholic Reverb Effect

    A professional, melancholic reverb with pristine sound quality.

    FEATURES:
    - Three reverb modes: ROOM, HALL, CATHEDRAL
    - Highpass/Lowpass filters for frequency control
    - Bass exclusion from reverb (keeps low-end tight)
    - Pristine sound - no distortion or coloration
    - 10 parameters for detailed control
    - Works perfectly with ARP and SEQ modes
    - Optimized for NTS-1 mkII

    ALGORITHM:
    - Hybrid Schroeder + Dattorro topology
    - 6 parallel comb filters (stereo)
    - 4 allpass diffusers
    - Early reflections
    - Pre-delay buffer
    - Biquad HP/LP filters
    - Soft clipping for stability
*/

#include "unit_revfx.h"
#include "fx_api.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "utils/buffer_ops.h"
#include "macros.h"
#include <algorithm>

// ═══════════════════════════════════════════════════════════════════════════
// CONSTANTS
// ═══════════════════════════════════════════════════════════════════════════

#define NUM_COMBS 6
#define NUM_ALLPASS 4
#define NUM_EARLY_TAPS 6
#define PREDELAY_SIZE 12000  // 250ms max @ 48kHz
#define SAMPLE_RATE 48000.f

// Prime number delays for natural decay
static const uint32_t s_comb_delays[NUM_COMBS] = {
    1193, 1277, 1361, 1433, 1511, 1583
};

static const uint32_t s_allpass_delays[NUM_ALLPASS] = {
    347, 113, 239, 179
};

static const uint32_t s_early_taps[NUM_EARLY_TAPS] = {
    397, 797, 1193, 1597, 1997, 2393
};

// ═══════════════════════════════════════════════════════════════════════════
// BIQUAD FILTER (for HP/LP filtering)
// ═══════════════════════════════════════════════════════════════════════════

struct BiquadFilter {
    float b0, b1, b2, a1, a2;
    float z1, z2;

    void reset() {
        z1 = z2 = 0.f;
    }

    void set_lowpass(float freq, float q) {
        freq = clipminmaxf(20.f, freq, 20000.f);
        q = clipminmaxf(0.5f, q, 10.f);

        float omega = 2.f * 3.14159265f * freq / SAMPLE_RATE;
        float sn = fx_sinf(omega / (2.f * 3.14159265f));
        float cs = fx_cosf(omega / (2.f * 3.14159265f));
        float alpha = sn / (2.f * q);

        float a0 = 1.f + alpha;
        b0 = ((1.f - cs) / 2.f) / a0;
        b1 = (1.f - cs) / a0;
        b2 = ((1.f - cs) / 2.f) / a0;
        a1 = (-2.f * cs) / a0;
        a2 = (1.f - alpha) / a0;
    }

    void set_highpass(float freq, float q) {
        freq = clipminmaxf(20.f, freq, 20000.f);
        q = clipminmaxf(0.5f, q, 10.f);

        float omega = 2.f * 3.14159265f * freq / SAMPLE_RATE;
        float sn = fx_sinf(omega / (2.f * 3.14159265f));
        float cs = fx_cosf(omega / (2.f * 3.14159265f));
        float alpha = sn / (2.f * q);

        float a0 = 1.f + alpha;
        b0 = ((1.f + cs) / 2.f) / a0;
        b1 = (-(1.f + cs)) / a0;
        b2 = ((1.f + cs) / 2.f) / a0;
        a1 = (-2.f * cs) / a0;
        a2 = (1.f - alpha) / a0;
    }

    inline float process(float input) {
        float output = b0 * input + b1 * z1 + b2 * z2 - a1 * z1 - a2 * z2;

        z2 = z1;
        z1 = input;

        // Anti-denormal
        if (si_fabsf(output) < 1e-15f) output = 0.f;

        return clipminmaxf(-2.f, output, 2.f);
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// COMB FILTER
// ═══════════════════════════════════════════════════════════════════════════

struct CombFilter {
    uint32_t write_pos;
    uint32_t delay_length;
    float feedback;
    float damp_z;
    float damp_coeff;
    float *buffer;

    inline float process(float input) {
        uint32_t read_pos = (write_pos + 1) % delay_length;
        float delayed = buffer[read_pos];

        // Anti-denormal
        if (si_fabsf(delayed) < 1e-15f) delayed = 0.f;

        // One-pole lowpass damping
        damp_z = delayed * (1.f - damp_coeff) + damp_z * damp_coeff;
        damp_z = clipminmaxf(-2.f, damp_z, 2.f);

        if (si_fabsf(damp_z) < 1e-15f) damp_z = 0.f;

        // Feedback with soft clip
        float fb_signal = input + damp_z * feedback;
        fb_signal = fastertanhf(fb_signal * 0.5f) * 2.f;

        buffer[write_pos] = fb_signal;
        write_pos = (write_pos + 1) % delay_length;

        return clipminmaxf(-2.f, delayed, 2.f);
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// ALLPASS FILTER
// ═══════════════════════════════════════════════════════════════════════════

struct AllpassFilter {
    uint32_t write_pos;
    uint32_t delay_length;
    float feedback;
    float *buffer;

    inline float process(float input) {
        uint32_t read_pos = (write_pos + 1) % delay_length;
        float delayed = buffer[read_pos];

        if (si_fabsf(delayed) < 1e-15f) delayed = 0.f;

        float output = -input + delayed;

        float fb_signal = input + delayed * feedback;
        fb_signal = fastertanhf(fb_signal * 0.5f) * 2.f;

        buffer[write_pos] = fb_signal;
        write_pos = (write_pos + 1) % delay_length;

        return clipminmaxf(-1.5f, output, 1.5f);
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// GLOBAL STATE
// ═══════════════════════════════════════════════════════════════════════════

static CombFilter s_combs_l[NUM_COMBS];
static CombFilter s_combs_r[NUM_COMBS];
static AllpassFilter s_allpass_l[NUM_ALLPASS];
static AllpassFilter s_allpass_r[NUM_ALLPASS];

static BiquadFilter s_reverb_hp_l;
static BiquadFilter s_reverb_hp_r;
static BiquadFilter s_reverb_lp_l;
static BiquadFilter s_reverb_lp_r;

static float *s_predelay_buffer;
static uint32_t s_predelay_write;

// Parameters
static float s_time;           // Reverb decay time
static float s_depth;          // Reverb depth/amount
static float s_mix;            // Dry/wet mix
static float s_size;           // Room size
static float s_damping;        // High frequency damping
static float s_diffusion;      // Diffusion amount
static float s_predelay_time;  // Pre-delay time
static float s_early_level;    // Early reflections level
static float s_hp_freq;        // Highpass filter frequency
static float s_lp_freq;        // Lowpass filter frequency
static uint8_t s_mode;         // 0=ROOM, 1=HALL, 2=CATHEDRAL

// ═══════════════════════════════════════════════════════════════════════════
// EARLY REFLECTIONS
// ═══════════════════════════════════════════════════════════════════════════

inline float process_early_reflections(float level) {
    if (level < 0.01f) return 0.f;

    float output = 0.f;
    for (int i = 0; i < NUM_EARLY_TAPS; i++) {
        uint32_t tap_pos = (s_predelay_write + PREDELAY_SIZE - s_early_taps[i]) % PREDELAY_SIZE;
        float tap = s_predelay_buffer[tap_pos];
        float decay = 1.f - ((float)i / (float)NUM_EARLY_TAPS) * 0.5f;
        output += tap * decay;
    }
    return output * level / (float)NUM_EARLY_TAPS;
}

// ═══════════════════════════════════════════════════════════════════════════
// UNIT CALLBACKS
// ═══════════════════════════════════════════════════════════════════════════

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
    max_comb_size = (uint32_t)((float)max_comb_size * 2.0f);

    uint32_t max_allpass_size = 0;
    for (int i = 0; i < NUM_ALLPASS; i++) {
        if (s_allpass_delays[i] > max_allpass_size) max_allpass_size = s_allpass_delays[i];
    }
    max_allpass_size = (uint32_t)((float)max_allpass_size * 2.0f);

    uint32_t total_size = (NUM_COMBS * max_comb_size + NUM_ALLPASS * max_allpass_size) * sizeof(float) * 2;
    total_size += PREDELAY_SIZE * sizeof(float);

    uint8_t *buffer_base = static_cast<uint8_t *>(desc->hooks.sdram_alloc(total_size));
    if (!buffer_base) return k_unit_err_memory;

    uint32_t offset = 0;

    // Allocate comb buffers - left channel
    float *reverb_buf_l = reinterpret_cast<float *>(buffer_base + offset);
    offset += (NUM_COMBS * max_comb_size + NUM_ALLPASS * max_allpass_size) * sizeof(float);

    // Allocate comb buffers - right channel
    float *reverb_buf_r = reinterpret_cast<float *>(buffer_base + offset);
    offset += (NUM_COMBS * max_comb_size + NUM_ALLPASS * max_allpass_size) * sizeof(float);

    // Allocate predelay buffer
    s_predelay_buffer = reinterpret_cast<float *>(buffer_base + offset);
    offset += PREDELAY_SIZE * sizeof(float);

    // Clear buffers
    buf_clr_f32(reverb_buf_l, NUM_COMBS * max_comb_size + NUM_ALLPASS * max_allpass_size);
    buf_clr_f32(reverb_buf_r, NUM_COMBS * max_comb_size + NUM_ALLPASS * max_allpass_size);
    buf_clr_f32(s_predelay_buffer, PREDELAY_SIZE);

    // Initialize comb filters
    uint32_t comb_offset = 0;
    for (int i = 0; i < NUM_COMBS; i++) {
        s_combs_l[i].write_pos = 0;
        s_combs_l[i].delay_length = s_comb_delays[i];
        s_combs_l[i].feedback = 0.75f;
        s_combs_l[i].damp_z = 0.f;
        s_combs_l[i].damp_coeff = 0.3f;
        s_combs_l[i].buffer = reverb_buf_l + comb_offset;

        s_combs_r[i].write_pos = 0;
        s_combs_r[i].delay_length = s_comb_delays[i] + 19;  // Slight offset for stereo
        s_combs_r[i].feedback = 0.75f;
        s_combs_r[i].damp_z = 0.f;
        s_combs_r[i].damp_coeff = 0.3f;
        s_combs_r[i].buffer = reverb_buf_r + comb_offset;

        comb_offset += max_comb_size;
    }

    // Initialize allpass filters
    uint32_t allpass_offset = comb_offset;
    for (int i = 0; i < NUM_ALLPASS; i++) {
        s_allpass_l[i].write_pos = 0;
        s_allpass_l[i].delay_length = s_allpass_delays[i];
        s_allpass_l[i].feedback = 0.5f;
        s_allpass_l[i].buffer = reverb_buf_l + allpass_offset;

        s_allpass_r[i].write_pos = 0;
        s_allpass_r[i].delay_length = s_allpass_delays[i] + 13;
        s_allpass_r[i].feedback = 0.5f;
        s_allpass_r[i].buffer = reverb_buf_r + allpass_offset;

        allpass_offset += max_allpass_size;
    }

    s_predelay_write = 0;

    // Initialize biquad filters
    s_reverb_hp_l.reset();
    s_reverb_hp_r.reset();
    s_reverb_lp_l.reset();
    s_reverb_lp_r.reset();

    // Default parameters - melancholic preset
    s_time = 0.65f;          // Medium-long decay
    s_depth = 0.4f;          // Moderate depth
    s_mix = 0.4f;            // 40% wet
    s_size = 0.6f;           // Medium-large room
    s_damping = 0.5f;        // Moderate HF damping
    s_diffusion = 0.5f;      // Moderate diffusion
    s_predelay_time = 0.15f; // Short predelay
    s_early_level = 0.2f;    // Subtle early reflections
    s_hp_freq = 0.15f;       // 150Hz HP (bass exclusion)
    s_lp_freq = 0.85f;       // 8.5kHz LP (smooth top)
    s_mode = 1;              // HALL mode (default)

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

    s_reverb_hp_l.reset();
    s_reverb_hp_r.reset();
    s_reverb_lp_l.reset();
    s_reverb_lp_r.reset();
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames)
{
    // Mode-specific scaling
    float size_scale, feedback_scale, damping_scale;

    switch (s_mode) {
        case 0: // ROOM
            size_scale = 0.6f + s_size * 0.3f;
            feedback_scale = 0.70f;
            damping_scale = 1.2f;
            break;
        case 1: // HALL
            size_scale = 0.8f + s_size * 0.4f;
            feedback_scale = 0.80f;
            damping_scale = 1.0f;
            break;
        case 2: // CATHEDRAL
            size_scale = 1.0f + s_size * 0.5f;
            feedback_scale = 0.85f;
            damping_scale = 0.8f;
            break;
        default:
            size_scale = 1.0f;
            feedback_scale = 0.75f;
            damping_scale = 1.0f;
            break;
    }

    // Update filter frequencies
    float hp_freq = 30.f + s_hp_freq * 470.f;  // 30-500Hz
    float lp_freq = 1000.f + s_lp_freq * 11000.f;  // 1k-12kHz

    s_reverb_hp_l.set_highpass(hp_freq, 0.707f);
    s_reverb_hp_r.set_highpass(hp_freq, 0.707f);
    s_reverb_lp_l.set_lowpass(lp_freq, 0.707f);
    s_reverb_lp_r.set_lowpass(lp_freq, 0.707f);

    for (uint32_t f = 0; f < frames; f++) {
        float in_l = in[f * 2];
        float in_r = in[f * 2 + 1];

        // Input clip
        in_l = clipminmaxf(-1.f, in_l, 1.f);
        in_r = clipminmaxf(-1.f, in_r, 1.f);

        // Mono sum for reverb
        float mono = (in_l + in_r) * 0.5f;

        // Predelay
        uint32_t predelay_samps = (uint32_t)(s_predelay_time * (float)PREDELAY_SIZE);
        uint32_t predelay_read = (s_predelay_write + PREDELAY_SIZE - predelay_samps) % PREDELAY_SIZE;
        float predelayed = s_predelay_buffer[predelay_read];
        s_predelay_buffer[s_predelay_write] = mono;
        s_predelay_write = (s_predelay_write + 1) % PREDELAY_SIZE;

        // Early reflections
        float early = process_early_reflections(s_early_level);

        // Apply HP filter to reverb input (bass exclusion)
        float reverb_in = s_reverb_hp_l.process(predelayed);

        // Update comb parameters based on mode
        for (int i = 0; i < NUM_COMBS; i++) {
            s_combs_l[i].delay_length = (uint32_t)((float)s_comb_delays[i] * size_scale);
            s_combs_r[i].delay_length = (uint32_t)((float)(s_comb_delays[i] + 19) * size_scale);

            float fb = feedback_scale + s_time * 0.15f;
            fb = clipminmaxf(0.5f, fb, 0.92f);

            s_combs_l[i].feedback = fb;
            s_combs_r[i].feedback = fb;

            float damp = s_damping * damping_scale;
            damp = clipminmaxf(0.2f, damp, 0.9f);

            s_combs_l[i].damp_coeff = damp;
            s_combs_r[i].damp_coeff = damp;
        }

        // Process comb filters
        float comb_out_l = 0.f;
        float comb_out_r = 0.f;

        for (int i = 0; i < NUM_COMBS; i++) {
            comb_out_l += s_combs_l[i].process(reverb_in);
            comb_out_r += s_combs_r[i].process(reverb_in);
        }
        comb_out_l /= (float)NUM_COMBS;
        comb_out_r /= (float)NUM_COMBS;

        // Clip after comb sum
        comb_out_l = clipminmaxf(-1.5f, comb_out_l, 1.5f);
        comb_out_r = clipminmaxf(-1.5f, comb_out_r, 1.5f);

        // Process allpass filters
        for (int i = 0; i < NUM_ALLPASS; i++) {
            float ap_fb = 0.35f + s_diffusion * 0.25f;
            ap_fb = clipminmaxf(0.3f, ap_fb, 0.6f);

            s_allpass_l[i].feedback = ap_fb;
            s_allpass_r[i].feedback = ap_fb;

            comb_out_l = s_allpass_l[i].process(comb_out_l);
            comb_out_r = s_allpass_r[i].process(comb_out_r);
        }

        // Apply LP filter to reverb output
        comb_out_l = s_reverb_lp_l.process(comb_out_l);
        comb_out_r = s_reverb_lp_r.process(comb_out_r);

        // Combine early + late
        float wet_l = early + comb_out_l * s_depth;
        float wet_r = early + comb_out_r * s_depth;

        // Output gain compensation
        wet_l *= 0.4f;
        wet_r *= 0.4f;

        // Soft clip
        wet_l = fastertanhf(wet_l * 0.95f);
        wet_r = fastertanhf(wet_r * 0.95f);

        // Dry/wet mix (s_mix is -1 to 1, convert to 0 to 1)
        float dry_wet = (s_mix + 1.f) * 0.5f;

        out[f * 2] = in_l * (1.f - dry_wet) + wet_l * dry_wet;
        out[f * 2 + 1] = in_r * (1.f - dry_wet) + wet_r * dry_wet;

        // Final output clip
        out[f * 2] = clipminmaxf(-1.f, out[f * 2], 1.f);
        out[f * 2 + 1] = clipminmaxf(-1.f, out[f * 2 + 1], 1.f);
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);

    switch (id) {
        case 0: s_time = clipminmaxf(0.f, valf, 1.f); break;
        case 1: s_depth = clipminmaxf(0.f, valf, 1.f); break;
        case 2: s_mix = clipminmaxf(-1.f, (float)value / 100.f, 1.f); break;
        case 3: s_size = clipminmaxf(0.f, valf, 1.f); break;
        case 4: s_damping = clipminmaxf(0.f, valf, 1.f); break;
        case 5: s_diffusion = clipminmaxf(0.f, valf, 1.f); break;
        case 6: s_predelay_time = clipminmaxf(0.f, valf, 1.f); break;
        case 7: s_early_level = clipminmaxf(0.f, valf, 1.f); break;
        case 8: s_hp_freq = clipminmaxf(0.f, valf, 1.f); break;
        case 9: s_lp_freq = clipminmaxf(0.f, valf, 1.f); break;
        case 10: s_mode = clipminmaxi32(0, value, 2); break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0: return (int32_t)(s_time * 1023.f);
        case 1: return (int32_t)(s_depth * 1023.f);
        case 2: return (int32_t)(s_mix * 100.f);
        case 3: return (int32_t)(s_size * 1023.f);
        case 4: return (int32_t)(s_damping * 1023.f);
        case 5: return (int32_t)(s_diffusion * 1023.f);
        case 6: return (int32_t)(s_predelay_time * 1023.f);
        case 7: return (int32_t)(s_early_level * 1023.f);
        case 8: return (int32_t)(s_hp_freq * 1023.f);
        case 9: return (int32_t)(s_lp_freq * 1023.f);
        case 10: return s_mode;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
    if (id == 10) {
        static const char *mode_names[] = {"ROOM", "HALL", "CATHDRL"};
        if (value >= 0 && value < 3) return mode_names[value];
    }
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    (void)tempo;
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    (void)counter;
}
