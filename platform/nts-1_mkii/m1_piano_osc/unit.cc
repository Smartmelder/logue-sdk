/*
    M1 PIANO - Sample-Based Oscillator

    Korg M1-style piano with attack/loop samples
    10 M1-style parameters for complete control
*/

#include "unit_osc.h"
#include "samples_data.h"

#define BASE_FREQ 298.14f  // Loop frequency from extraction

static const unit_runtime_osc_context_t *s_context;

// Playback state
static float s_attack_pos = 0.f;
static float s_loop_pos = 0.f;
static bool s_in_attack = true;
static bool s_active = false;
static float s_velocity = 1.f;

// Envelope
static float s_env_level = 0.f;
static uint32_t s_env_counter = 0;
static uint8_t s_env_stage = 4;  // 0=A, 1=D, 2=S, 3=R, 4=off

// LFO
static float s_lfo_phase = 0.f;

// Filter
static float s_filt_l = 0.f;
static float s_filt_r = 0.f;

// Parameters
static float s_decay = 1.0f;
static float s_release = 0.5f;
static float s_bright = 0.8f;
static float s_reso = 0.2f;
static float s_vib_depth = 0.0f;
static float s_vib_speed = 5.0f;
static float s_attack = 0.001f;
static float s_sustain = 0.7f;
static float s_width = 0.5f;
static float s_detune = 0.f;

// Sample interpolation
inline float samp_lerp(const float *data, uint32_t len, float pos) {
    if (pos >= (float)(len - 1)) return data[len - 1];
    if (pos < 0.f) return data[0];

    uint32_t i = (uint32_t)pos;
    float frac = pos - (float)i;

    if (i + 1 >= len) return data[i];
    return data[i] + frac * (data[i + 1] - data[i]);
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc)
{
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 1) return k_unit_err_geometry;

    s_context = static_cast<const unit_runtime_osc_context_t *>(desc->hooks.runtime_context);

    s_attack_pos = 0.f;
    s_loop_pos = 0.f;
    s_in_attack = true;
    s_active = false;
    s_env_level = 0.f;
    s_env_counter = 0;
    s_env_stage = 4;
    s_lfo_phase = 0.f;
    s_filt_l = 0.f;
    s_filt_r = 0.f;

    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset()
{
    s_active = false;
    s_env_stage = 4;
    s_env_level = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames)
{
    (void)in;

    const uint8_t note = (s_context->pitch >> 8) & 0xFF;
    const uint8_t mod = s_context->pitch & 0xFF;

    const float w0 = osc_w0f_for_note(note, mod);
    const float note_freq = w0 * 48000.f;

    float pitch_ratio = note_freq / BASE_FREQ;

    // Detune
    if (si_fabsf(s_detune) > 0.01f) {
        pitch_ratio *= fastpow2f(s_detune / 1200.f);
    }

    const float lfo_inc = s_vib_speed / 48000.f;

    for (uint32_t f = 0; f < frames; f++) {
        float env = 0.f;

        if (s_active) {
            float t = (float)s_env_counter / 48000.f;

            switch (s_env_stage) {
                case 0: {  // Attack
                    if (t < s_attack) {
                        env = t / s_attack;
                    } else {
                        env = 1.f;
                        s_env_stage = 1;
                        s_env_counter = 0;
                    }
                    break;
                }
                case 1: {  // Decay
                    if (t < s_decay) {
                        env = 1.f - (t / s_decay) * (1.f - s_sustain);
                    } else {
                        env = s_sustain;
                        s_env_stage = 2;
                        s_env_counter = 0;
                    }
                    break;
                }
                case 2:  // Sustain
                    env = s_sustain;
                    break;
                case 3: {  // Release
                    if (t < s_release) {
                        env = s_env_level * (1.f - t / s_release);
                    } else {
                        env = 0.f;
                        s_env_stage = 4;
                        s_active = false;
                    }
                    break;
                }
                default:
                    env = 0.f;
                    s_active = false;
                    break;
            }

            s_env_level = env;
            s_env_counter++;
        }

        // Vibrato
        float vib = 0.f;
        if (s_vib_depth > 0.001f) {
            vib = osc_sinf(s_lfo_phase) * s_vib_depth * 0.02f;
            s_lfo_phase += lfo_inc;
            if (s_lfo_phase >= 1.f) s_lfo_phase -= 1.f;
        }

        float pitch_mod = pitch_ratio * (1.f + vib);

        // Sample playback
        float smp = 0.f;

        if (s_active && env > 0.001f) {
            if (s_in_attack) {
                smp = samp_lerp(k_attack_samples, k_attack_samples_len, s_attack_pos);
                s_attack_pos += pitch_mod;

                if (s_attack_pos >= (float)k_attack_samples_len) {
                    s_in_attack = false;
                    s_loop_pos = 0.f;
                }
            } else {
                smp = samp_lerp(k_loop_samples, k_loop_samples_len, s_loop_pos);
                s_loop_pos += pitch_mod;

                while (s_loop_pos >= (float)k_loop_samples_len) {
                    s_loop_pos -= (float)k_loop_samples_len;
                }
            }

            smp *= s_velocity * env;
        }

        // Stereo
        float l = smp;
        float r = smp;

        if (s_width > 0.01f) {
            float mid = (l + r) * 0.5f;
            float side = (l - r) * s_width * 0.5f;
            l = mid + side;
            r = mid - side;
        }

        // Filter (1-pole LP)
        float cutoff = clipminmaxf(0.001f, s_bright, 0.999f);
        s_filt_l += cutoff * (l - s_filt_l);
        s_filt_r += cutoff * (r - s_filt_r);

        l = s_filt_l;
        r = s_filt_r;

        // Resonance
        if (s_reso > 0.01f) {
            l += s_filt_l * s_reso * 0.3f;
            r += s_filt_r * s_reso * 0.3f;
        }

        // Soft clip
        l = osc_softclipf(0.05f, l);
        r = osc_softclipf(0.05f, r);

        // Mono output
        out[f] = clipminmaxf(-1.f, (l + r) * 0.5f, 1.f);
    }
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velo)
{
    (void)note;

    s_active = true;
    s_in_attack = true;
    s_attack_pos = 0.f;
    s_loop_pos = 0.f;
    s_env_stage = 0;
    s_env_level = 0.f;
    s_env_counter = 0;
    s_lfo_phase = 0.f;
    s_velocity = (float)velo / 127.f;
    s_filt_l = 0.f;
    s_filt_r = 0.f;
}

__unit_callback void unit_note_off(uint8_t note)
{
    (void)note;

    if (s_active && s_env_stage < 3) {
        s_env_stage = 3;
        s_env_counter = 0;
    }
}

__unit_callback void unit_all_note_off()
{
    unit_note_off(0);
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float v = param_val_to_f32(value);

    switch (id) {
        case 0: s_decay = 0.1f + v * 4.9f; break;      // DECAY
        case 1: s_release = 0.01f + v * 2.99f; break;  // RELEASE
        case 2: s_bright = clipminmaxf(0.1f, v, 1.f); break;  // BRIGHTNESS
        case 3: s_reso = v; break;                     // RESONANCE
        case 4: s_vib_depth = v; break;                // VIBRATO
        case 5: s_vib_speed = 0.5f + v * 9.5f; break;  // VIB SPEED
        case 6: s_attack = 0.001f + v * 0.999f; break; // ATTACK
        case 7: s_sustain = v; break;                  // SUSTAIN
        case 8: s_width = v; break;                    // WIDTH
        case 9: s_detune = (v - 0.5f) * 200.f; break;  // DETUNE
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0: return (int32_t)((s_decay - 0.1f) / 4.9f * 1023.f);
        case 1: return (int32_t)((s_release - 0.01f) / 2.99f * 1023.f);
        case 2: return (int32_t)(s_bright * 1023.f);
        case 3: return (int32_t)(s_reso * 1023.f);
        case 4: return (int32_t)(s_vib_depth * 1023.f);
        case 5: return (int32_t)((s_vib_speed - 0.5f) / 9.5f * 1023.f);
        case 6: return (int32_t)((s_attack - 0.001f) / 0.999f * 1023.f);
        case 7: return (int32_t)(s_sustain * 1023.f);
        case 8: return (int32_t)(s_width * 1023.f);
        case 9: return (int32_t)((s_detune / 200.f + 0.5f) * 1023.f);
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
    (void)id;
    (void)value;
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo)
{
    (void)tempo;
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter)
{
    (void)counter;
}

__unit_callback void unit_gate_on(uint8_t note, uint8_t velo)
{
    unit_note_on(note, velo);
}

__unit_callback void unit_gate_off(uint8_t note)
{
    unit_note_off(note);
}

__unit_callback void unit_all_sound_off()
{
    s_active = false;
    s_env_stage = 4;
}
