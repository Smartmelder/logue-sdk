/*
    BREVER INSIDE OUT - Optimized Reverb
    Simplified for NTS-1 mkII memory constraints
*/

#include "unit_revfx.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "utils/buffer_ops.h"
#include "fx_api.h"

// Reduced configuration
#define NUM_COMBS 4
#define NUM_ALLPASS 4
#define BUFFER_SIZE 12000

// Delay times
static const uint16_t s_comb_delays[NUM_COMBS] = {1557, 1617, 1491, 1422};
static const uint16_t s_allpass_delays[NUM_ALLPASS] = {225, 341, 441, 556};

struct CombFilter {
    uint32_t write_pos;
    uint16_t delay_length;
    float feedback;
    float damp_z1;
    float *buffer;
};

struct AllpassFilter {
    uint32_t write_pos;
    uint16_t delay_length;
    float *buffer;
};

static CombFilter s_combs_l[NUM_COMBS];
static CombFilter s_combs_r[NUM_COMBS];
static AllpassFilter s_allpass_l[NUM_ALLPASS];
static AllpassFilter s_allpass_r[NUM_ALLPASS];
static float *s_delay_buffer;

static float s_time, s_depth, s_mix, s_shimmer, s_motion, s_space;
static uint8_t s_mode;
static float s_lfo_phase;

inline float safe_clip(float x) {
    return clipminmaxf(-1.f, x, 1.f);
}

inline float comb_process(CombFilter *cf, float input) {
    uint32_t read_pos = (cf->write_pos + 1) % cf->delay_length;
    float delayed = cf->buffer[read_pos];
    cf->damp_z1 = cf->damp_z1 * 0.7f + delayed * 0.3f;
    float fb_signal = cf->damp_z1 * cf->feedback;
    cf->buffer[cf->write_pos] = input + fb_signal;
    cf->write_pos = (cf->write_pos + 1) % cf->delay_length;
    return delayed;
}

inline float allpass_process(AllpassFilter *ap, float input) {
    uint32_t read_pos = (ap->write_pos + 1) % ap->delay_length;
    float delayed = ap->buffer[read_pos];
    float output = -input + delayed;
    ap->buffer[ap->write_pos] = input + delayed * 0.5f;
    ap->write_pos = (ap->write_pos + 1) % ap->delay_length;
    return output;
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc)
{
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 2) return k_unit_err_geometry;
    if (!desc->hooks.sdram_alloc) return k_unit_err_memory;

    uint32_t total_size = 0;
    for (int i = 0; i < NUM_COMBS; i++) total_size += s_comb_delays[i] * 2;
    for (int i = 0; i < NUM_ALLPASS; i++) total_size += s_allpass_delays[i] * 2;
    total_size += BUFFER_SIZE;
    total_size *= sizeof(float);

    uint8_t *buffer_base = static_cast<uint8_t *>(desc->hooks.sdram_alloc(total_size));
    if (!buffer_base) return k_unit_err_memory;

    float *fbuf = reinterpret_cast<float *>(buffer_base);
    uint32_t offset = 0;

    for (int i = 0; i < NUM_COMBS; i++) {
        s_combs_l[i].write_pos = 0;
        s_combs_l[i].delay_length = s_comb_delays[i];
        s_combs_l[i].feedback = 0.84f;
        s_combs_l[i].damp_z1 = 0.f;
        s_combs_l[i].buffer = fbuf + offset;
        offset += s_comb_delays[i];

        s_combs_r[i].write_pos = 0;
        s_combs_r[i].delay_length = s_comb_delays[i];
        s_combs_r[i].feedback = 0.84f;
        s_combs_r[i].damp_z1 = 0.f;
        s_combs_r[i].buffer = fbuf + offset;
        offset += s_comb_delays[i];
    }

    for (int i = 0; i < NUM_ALLPASS; i++) {
        s_allpass_l[i].write_pos = 0;
        s_allpass_l[i].delay_length = s_allpass_delays[i];
        s_allpass_l[i].buffer = fbuf + offset;
        offset += s_allpass_delays[i];

        s_allpass_r[i].write_pos = 0;
        s_allpass_r[i].delay_length = s_allpass_delays[i];
        s_allpass_r[i].buffer = fbuf + offset;
        offset += s_allpass_delays[i];
    }

    s_delay_buffer = fbuf + offset;
    buf_clr_f32(fbuf, offset + BUFFER_SIZE);

    s_time = 0.6f; s_depth = 0.5f; s_mix = 0.5f; s_shimmer = 0.f;
    s_motion = 0.25f; s_space = 0.5f; s_mode = 0; s_lfo_phase = 0.f;

    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset()
{
    for (int i = 0; i < NUM_COMBS; i++) {
        s_combs_l[i].write_pos = 0; s_combs_l[i].damp_z1 = 0.f;
        s_combs_r[i].write_pos = 0; s_combs_r[i].damp_z1 = 0.f;
    }
    for (int i = 0; i < NUM_ALLPASS; i++) {
        s_allpass_l[i].write_pos = 0;
        s_allpass_r[i].write_pos = 0;
    }
    s_lfo_phase = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames)
{
    for (uint32_t f = 0; f < frames; f++) {
        float in_l = in[f * 2];
        float in_r = in[f * 2 + 1];

        s_lfo_phase += (s_motion * 2.f) / 48000.f;
        if (s_lfo_phase >= 1.f) s_lfo_phase -= 1.f;
        float lfo = fx_sinf(s_lfo_phase);

        float mono_in = (in_l + in_r) * 0.5f;
        float reverb_input = mono_in;
        
        if (s_mode == 1 && s_shimmer > 0.01f) {
            reverb_input = mono_in * (1.f - s_shimmer) + mono_in * 1.5f * s_shimmer;
        }

        float comb_out_l = 0.f, comb_out_r = 0.f;

        for (int i = 0; i < NUM_COMBS; i++) {
            float fb_mod = 1.f + lfo * s_motion * 0.1f;
            float fb = (0.7f + s_time * 0.23f) * fb_mod;
            s_combs_l[i].feedback = clipminmaxf(0.1f, fb, 0.93f);
            s_combs_r[i].feedback = clipminmaxf(0.1f, fb, 0.93f);

            comb_out_l += comb_process(&s_combs_l[i], reverb_input);
            comb_out_r += comb_process(&s_combs_r[i], reverb_input);
        }

        comb_out_l /= (float)NUM_COMBS;
        comb_out_r /= (float)NUM_COMBS;

        for (int i = 0; i < NUM_ALLPASS; i++) {
            comb_out_l = allpass_process(&s_allpass_l[i], comb_out_l);
            comb_out_r = allpass_process(&s_allpass_r[i], comb_out_r);
        }

        float wet_l = comb_out_l * s_depth;
        float wet_r = comb_out_r * s_depth;
        float dry_wet = (s_mix + 1.f) * 0.5f;
        
        out[f * 2] = safe_clip(in_l * (1.f - dry_wet) + wet_l * dry_wet);
        out[f * 2 + 1] = safe_clip(in_r * (1.f - dry_wet) + wet_r * dry_wet);
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);

    switch (id) {
        case 0: s_time = valf; break;
        case 1: s_depth = valf; break;
        case 2: s_mix = (float)value / 100.f; break;
        case 3: s_shimmer = valf; break;
        case 5: s_motion = valf; break;
        case 6: s_space = valf; break;
        case 8: s_mode = value; break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0: return (int32_t)(s_time * 1023.f);
        case 1: return (int32_t)(s_depth * 1023.f);
        case 2: return (int32_t)(s_mix * 100.f);
        case 3: return (int32_t)(s_shimmer * 1023.f);
        case 5: return (int32_t)(s_motion * 1023.f);
        case 6: return (int32_t)(s_space * 1023.f);
        case 8: return s_mode;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
    if (id == 8) {
        static const char *mode_names[] = {"HALL", "SHIMMER", "PLATE", "ROOM", "SPRING", "CHAMBER"};
        if (value >= 0 && value < 6) return mode_names[value];
    }
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) { (void)tempo; }
__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) { (void)counter; }
