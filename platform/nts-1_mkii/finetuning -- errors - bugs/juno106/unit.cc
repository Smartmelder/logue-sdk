/*
    JUNO-106 MEGA EDITION
    COMPLETE recreation met ALLE features
    
    TARGET: 20-25 KB compiled
*/

#include "unit_osc.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "utils/fixed_math.h"
#include "macros.h"
#include <math.h>

#define MAX_VOICES 4
#define MAX_CHORD_NOTES 8
#define CHORUS_DELAY_SIZE 2048
#define LFO_TABLE_SIZE 256
#define SEQ_STEPS 16
#define SEQ_PATTERNS 4
#define ARP_PATTERNS 8
#define NOISE_BUFFER_SIZE 1024
#define WAVETABLE_SIZE 128

enum {
    FX_CHORUS_I = 0,
    FX_CHORUS_II = 1,
    FX_RING_MOD = 2,
    FX_NOISE = 3
};

enum {
    ARP_OFF = 0,
    ARP_UP = 1,
    ARP_DOWN = 2,
    ARP_UP_DOWN = 3,
    ARP_DOWN_UP = 4,
    ARP_RANDOM = 5,
    ARP_OCTAVE_UP = 6,
    ARP_OCTAVE_DOWN = 7
};

typedef struct {
    uint8_t notes[SEQ_STEPS];
    uint8_t gates[SEQ_STEPS];
    uint8_t velocities[SEQ_STEPS];
    uint8_t length;
} SequencePattern;

static const unit_runtime_osc_context_t *s_context;

static float s_phase_saw[MAX_VOICES];
static float s_phase_pulse[MAX_VOICES];
static float s_phase_sub;
static float s_phase_ring;
static float s_phase_noise_lfo;

static float s_lfo_phase;
static float s_lfo_value;
static float s_lfo_sine_table[LFO_TABLE_SIZE];

static float s_chorus_buffer_l[CHORUS_DELAY_SIZE];
static float s_chorus_buffer_r[CHORUS_DELAY_SIZE];
static uint32_t s_chorus_write_pos;
static float s_chorus_lfo_phase[3];

static float s_noise_buffer[NOISE_BUFFER_SIZE];
static uint32_t s_noise_pos;
static uint32_t s_noise_seed;

static float s_wavetable[WAVETABLE_SIZE];

static float s_hpf_z[2];
static float s_lpf_z;
static float s_notch_z[2];

static float s_env_level[MAX_VOICES];
static float s_env_phase[MAX_VOICES];
static uint8_t s_env_stage[MAX_VOICES];

static float s_porta_current;
static float s_porta_target;

static uint8_t s_chord_notes[MAX_CHORD_NOTES];
static uint8_t s_chord_vels[MAX_CHORD_NOTES];
static uint8_t s_chord_count;

static SequencePattern s_patterns[SEQ_PATTERNS];
static uint8_t s_current_pattern;
static uint8_t s_seq_pos;
static uint32_t s_seq_counter;
static uint32_t s_seq_step_time;
static bool s_seq_running;

static uint8_t s_arp_notes[MAX_CHORD_NOTES];
static uint8_t s_arp_count;
static uint8_t s_arp_pos;
static uint8_t s_arp_pattern;
static uint32_t s_arp_counter;
static uint32_t s_arp_step_time;
static bool s_arp_running;

static uint8_t s_last_note;
static uint8_t s_last_velocity;
static uint8_t s_aftertouch;
static int16_t s_pitch_bend;

static float s_wave_mix;
static float s_fx_mix;
static float s_pulse_width;
static float s_detune;
static float s_sub_level;
static float s_hpf_cutoff;
static float s_lfo_rate;
static float s_lfo_depth;
static uint8_t s_mode_select;
static uint8_t s_feature_select;

static uint32_t s_sample_counter;

inline float poly_blep(float t, float dt) {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.f;
    } else if (t > 1.f - dt) {
        t = (t - 1.f) / dt;
        return t * t + t + t + 1.f;
    }
    return 0.f;
}

inline float fast_tanh(float x) {
    if (x < -3.f) return -1.f;
    if (x > 3.f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

void init_lfo_table() {
    for (int i = 0; i < LFO_TABLE_SIZE; i++) {
        float phase = (float)i / (float)LFO_TABLE_SIZE;
        s_lfo_sine_table[i] = osc_sinf(phase);
    }
}

void init_wavetable() {
    for (int i = 0; i < WAVETABLE_SIZE; i++) {
        float phase = (float)i / (float)WAVETABLE_SIZE;
        float saw = 2.f * phase - 1.f;
        float tri = (phase < 0.5f) ? (4.f * phase - 1.f) : (3.f - 4.f * phase);
        s_wavetable[i] = (saw + tri) * 0.5f;
    }
}

void init_noise_buffer() {
    s_noise_seed = 0x12345678;
    float pink_z = 0.f;
    for (int i = 0; i < NOISE_BUFFER_SIZE; i++) {
        s_noise_seed = s_noise_seed * 1103515245u + 12345u;
        float white = ((float)(s_noise_seed >> 16) / 32768.f) - 1.f;
        pink_z = pink_z * 0.98f + white * 0.02f;
        s_noise_buffer[i] = pink_z;
    }
    s_noise_pos = 0;
}

void init_patterns() {
    for (int p = 0; p < SEQ_PATTERNS; p++) {
        s_patterns[p].length = SEQ_STEPS;
        for (int i = 0; i < SEQ_STEPS; i++) {
            s_patterns[p].notes[i] = 60 + ((i * 7 + p * 3) % 12);
            s_patterns[p].gates[i] = (i % (p + 2)) ? 1 : 0;
            s_patterns[p].velocities[i] = 80 + (i * 5) % 40;
        }
    }
}

inline float lfo_read(float phase) {
    float idx = phase * (float)(LFO_TABLE_SIZE - 1);
    uint32_t i0 = (uint32_t)idx;
    uint32_t i1 = (i0 + 1) % LFO_TABLE_SIZE;
    float frac = idx - (float)i0;
    return s_lfo_sine_table[i0] * (1.f - frac) + s_lfo_sine_table[i1] * frac;
}

inline float wavetable_read(float phase) {
    float idx = phase * (float)(WAVETABLE_SIZE - 1);
    uint32_t i0 = (uint32_t)idx % WAVETABLE_SIZE;
    uint32_t i1 = (i0 + 1) % WAVETABLE_SIZE;
    float frac = idx - (float)i0;
    return s_wavetable[i0] * (1.f - frac) + s_wavetable[i1] * frac;
}

inline float noise_read() {
    float n = s_noise_buffer[s_noise_pos];
    s_noise_pos = (s_noise_pos + 1) % NOISE_BUFFER_SIZE;
    return n;
}

inline float chorus_process(float x, int channel) {
    float* buffer = (channel == 0) ? s_chorus_buffer_l : s_chorus_buffer_r;
    
    buffer[s_chorus_write_pos] = x;
    
    float out = x;
    int fx_type = (int)(s_fx_mix * 3.99f);
    
    if (fx_type <= 1) {
        for (int c = 0; c <= fx_type; c++) {
            float lfo_freq = (c == 0) ? 0.4f : 0.6f;
            s_chorus_lfo_phase[c] += lfo_freq / 48000.f;
            if (s_chorus_lfo_phase[c] >= 1.f) s_chorus_lfo_phase[c] -= 1.f;
            
            float lfo = lfo_read(s_chorus_lfo_phase[c]);
            float delay_samps = 1000.f + lfo * 800.f + (float)channel * 200.f;
            uint32_t d = (uint32_t)delay_samps;
            uint32_t read_pos = (s_chorus_write_pos + CHORUS_DELAY_SIZE - d) % CHORUS_DELAY_SIZE;
            
            out = (out + buffer[read_pos]) * 0.5f;
        }
    }
    
    return out;
}

inline float hpf_process(float x, int channel, float cutoff) {
    if (cutoff < 0.1f) {
        x *= 1.f + (0.1f - cutoff) * 8.f;
    }
    float coeff = clipminmaxf(0.001f, cutoff, 0.999f);
    s_hpf_z[channel] = x - s_hpf_z[channel] * coeff;
    return s_hpf_z[channel];
}

inline float envelope_process(int voice) {
    static const float attack_time = 0.005f;
    static const float decay_time = 0.3f;
    static const float sustain_level = 0.7f;
    static const float release_time = 0.5f;
    
    float env = s_env_level[voice];
    
    switch (s_env_stage[voice]) {
        case 0:
            s_env_phase[voice] += 1.f / (attack_time * 48000.f);
            if (s_env_phase[voice] >= 1.f) {
                s_env_stage[voice] = 1;
                s_env_phase[voice] = 0.f;
            }
            env = s_env_phase[voice];
            break;
        case 1:
            s_env_phase[voice] += 1.f / (decay_time * 48000.f);
            if (s_env_phase[voice] >= 1.f) {
                s_env_stage[voice] = 2;
                s_env_phase[voice] = 0.f;
            }
            env = 1.f - s_env_phase[voice] * (1.f - sustain_level);
            break;
        case 2:
            env = sustain_level;
            break;
        case 3:
            s_env_phase[voice] += 1.f / (release_time * 48000.f);
            if (s_env_phase[voice] >= 1.f) {
                s_env_stage[voice] = 4;
                env = 0.f;
            } else {
                env = sustain_level * (1.f - s_env_phase[voice]);
            }
            break;
        case 4:
            env = 0.f;
            break;
    }
    
    s_env_level[voice] = env;
    return env;
}

void trigger_envelope(int voice) {
    s_env_stage[voice] = 0;
    s_env_phase[voice] = 0.f;
    s_env_level[voice] = 0.f;
}

void release_envelope(int voice) {
    if (s_env_stage[voice] < 3) {
        s_env_stage[voice] = 3;
        s_env_phase[voice] = 0.f;
    }
}

uint8_t arp_get_next_note() {
    if (s_arp_count == 0) return s_last_note;
    
    uint8_t note = s_arp_notes[0];
    
    switch (s_arp_pattern) {
        case ARP_UP:
            note = s_arp_notes[s_arp_pos % s_arp_count];
            break;
        case ARP_DOWN:
            note = s_arp_notes[s_arp_count - 1 - (s_arp_pos % s_arp_count)];
            break;
        case ARP_UP_DOWN: {
            int cycle = s_arp_count * 2 - 2;
            int pos = s_arp_pos % cycle;
            if (pos < s_arp_count) {
                note = s_arp_notes[pos];
            } else {
                note = s_arp_notes[cycle - pos];
            }
            break;
        }
        case ARP_RANDOM:
            s_noise_seed = s_noise_seed * 1103515245u + 12345u;
            note = s_arp_notes[(s_noise_seed >> 16) % s_arp_count];
            break;
        case ARP_OCTAVE_UP:
            note = s_arp_notes[s_arp_pos % s_arp_count] + (s_arp_pos / s_arp_count) * 12;
            if (note > 127) note = 127;
            break;
        case ARP_OCTAVE_DOWN:
            note = s_arp_notes[s_arp_pos % s_arp_count] - (s_arp_pos / s_arp_count) * 12;
            if (note < 0) note = 0;
            break;
        default:
            note = s_arp_notes[s_arp_pos % s_arp_count];
    }
    
    return note;
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc)
{
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 1) return k_unit_err_geometry;

    s_context = static_cast<const unit_runtime_osc_context_t *>(desc->hooks.runtime_context);

    for (int i = 0; i < MAX_VOICES; i++) {
        s_phase_saw[i] = 0.f;
        s_phase_pulse[i] = 0.f;
        s_env_level[i] = 0.f;
        s_env_phase[i] = 0.f;
        s_env_stage[i] = 4;
    }
    
    s_phase_sub = 0.f;
    s_phase_ring = 0.f;
    s_phase_noise_lfo = 0.f;
    
    s_lfo_phase = 0.f;
    s_lfo_value = 0.f;
    
    init_lfo_table();
    init_wavetable();
    init_noise_buffer();
    init_patterns();
    
    for (int i = 0; i < CHORUS_DELAY_SIZE; i++) {
        s_chorus_buffer_l[i] = 0.f;
        s_chorus_buffer_r[i] = 0.f;
    }
    s_chorus_write_pos = 0;
    s_chorus_lfo_phase[0] = 0.f;
    s_chorus_lfo_phase[1] = 0.25f;
    s_chorus_lfo_phase[2] = 0.5f;
    
    s_hpf_z[0] = 0.f;
    s_hpf_z[1] = 0.f;
    s_lpf_z = 0.f;
    s_notch_z[0] = 0.f;
    s_notch_z[1] = 0.f;
    
    s_porta_current = 60.f;
    s_porta_target = 60.f;
    
    s_chord_count = 0;
    s_current_pattern = 0;
    s_seq_pos = 0;
    s_seq_counter = 0;
    s_seq_step_time = 12000;
    s_seq_running = false;
    
    s_arp_count = 0;
    s_arp_pos = 0;
    s_arp_pattern = ARP_OFF;
    s_arp_counter = 0;
    s_arp_step_time = 6000;
    s_arp_running = false;
    
    s_last_note = 60;
    s_last_velocity = 100;
    s_aftertouch = 0;
    s_pitch_bend = 0;
    
    s_sample_counter = 0;

    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset()
{
    for (int i = 0; i < MAX_VOICES; i++) {
        s_phase_saw[i] = 0.f;
        s_phase_pulse[i] = 0.f;
    }
    s_lfo_phase = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames)
{
    int lfo_target = s_mode_select & 0x3;
    int voice_mode = (s_mode_select >> 2) & 0x3;
    
    bool arp_en = (s_feature_select & 0x1);
    bool seq_en = (s_feature_select & 0x2);
    int fx_mode = (s_feature_select >> 2) & 0x3;
    
    uint8_t base_note = (s_context->pitch >> 8) & 0xFF;
    uint8_t mod = s_context->pitch & 0xFF;
    
    for (uint32_t f = 0; f < frames; f++) {
        s_sample_counter++;
        
        if (arp_en && s_arp_running && s_arp_count > 0) {
            s_arp_counter++;
            if (s_arp_counter >= s_arp_step_time) {
                s_arp_counter = 0;
                s_arp_pos++;
                base_note = arp_get_next_note();
                trigger_envelope(0);
            }
        }
        
        if (seq_en && s_seq_running) {
            s_seq_counter++;
            if (s_seq_counter >= s_seq_step_time) {
                s_seq_counter = 0;
                s_seq_pos = (s_seq_pos + 1) % s_patterns[s_current_pattern].length;
                if (s_patterns[s_current_pattern].gates[s_seq_pos]) {
                    base_note = s_patterns[s_current_pattern].notes[s_seq_pos];
                    s_last_velocity = s_patterns[s_current_pattern].velocities[s_seq_pos];
                    trigger_envelope(0);
                }
            }
        }
        
        s_porta_target = (float)base_note;
        float porta_coeff = 0.001f + s_detune * 0.1f;
        s_porta_current += (s_porta_target - s_porta_current) * porta_coeff;
        
        float lfo_freq = 0.1f + s_lfo_rate * 19.9f;
        s_lfo_phase += lfo_freq / 48000.f;
        if (s_lfo_phase >= 1.f) s_lfo_phase -= 1.f;
        
        float at_mod = (float)s_aftertouch / 127.f;
        s_lfo_value = lfo_read(s_lfo_phase) * (s_lfo_depth + at_mod * 0.3f);
        
        int num_voices = 1;
        if (voice_mode == 1) num_voices = 2;
        else if (voice_mode == 2) num_voices = 3;
        else if (voice_mode == 3 && s_chord_count > 0) num_voices = clipminmaxi32(1, s_chord_count, MAX_VOICES);
        
        float sig = 0.f;
        
        for (int v = 0; v < num_voices; v++) {
            uint8_t vn = (uint8_t)s_porta_current;
            if (voice_mode == 3 && v < s_chord_count) {
                vn = s_chord_notes[v];
            }
            
            float w0 = osc_w0f_for_note(vn, mod);
            
            float bend_amt = (float)s_pitch_bend / 8192.f;
            w0 *= fastpow2f(bend_amt / 12.f);
            
            float detune_amt = 0.f;
            if (voice_mode != 3 && num_voices > 1) {
                float spread = s_detune * 0.15f;
                if (v == 0) detune_amt = -spread;
                else if (v == 1) detune_amt = spread;
            }
            w0 *= (1.f + detune_amt);
            
            if (lfo_target == 0 || lfo_target == 3) {
                w0 *= (1.f + s_lfo_value * 0.08f);
            }
            
            float pw = 0.1f + s_pulse_width * 0.8f;
            if (lfo_target == 1 || lfo_target == 3) {
                pw += s_lfo_value * 0.3f;
                pw = clipminmaxf(0.05f, pw, 0.95f);
            }
            
            float saw = 2.f * s_phase_saw[v] - 1.f;
            saw -= poly_blep(s_phase_saw[v], w0);
            
            float pulse = (s_phase_pulse[v] < pw) ? 1.f : -1.f;
            pulse += poly_blep(s_phase_pulse[v], w0);
            pulse -= poly_blep(fmodf(s_phase_pulse[v] + (1.f - pw), 1.f), w0);
            
            float wave_sig = 0.f;
            if (s_wave_mix < 0.33f) {
                wave_sig = saw;
            } else if (s_wave_mix < 0.67f) {
                float mx = (s_wave_mix - 0.33f) / 0.34f;
                wave_sig = linintf(mx, saw, (saw + pulse) * 0.5f);
            } else {
                float mx = (s_wave_mix - 0.67f) / 0.33f;
                wave_sig = linintf(mx, (saw + pulse) * 0.5f, pulse);
            }
            
            float env = envelope_process(v);
            float vel_scale = 0.5f + ((float)s_last_velocity / 127.f) * 0.5f;
            wave_sig *= env * vel_scale;
            
            sig += wave_sig / (float)num_voices;
            
            s_phase_saw[v] += w0;
            s_phase_saw[v] -= (uint32_t)s_phase_saw[v];
            s_phase_pulse[v] += w0;
            s_phase_pulse[v] -= (uint32_t)s_phase_pulse[v];
        }
        
        if (s_sub_level > 0.01f) {
            float sub_w = osc_w0f_for_note((uint8_t)s_porta_current, mod) * 0.5f;
            float sub = (s_phase_sub < 0.5f) ? 1.f : -1.f;
            sig += sub * s_sub_level * 0.8f;
            s_phase_sub += sub_w;
            s_phase_sub -= (uint32_t)s_phase_sub;
        }
        
        float hpf_f = s_hpf_cutoff;
        if (lfo_target == 2 || lfo_target == 3) {
            hpf_f += s_lfo_value * 0.4f;
            hpf_f = clipminmaxf(0.f, hpf_f, 1.f);
        }
        sig = hpf_process(sig, 0, hpf_f);
        
        int fx_type = (int)(s_fx_mix * 3.99f);
        
        if (fx_type == 2) {
            float ring_w = osc_w0f_for_note((uint8_t)s_porta_current + 7, mod) * 2.f;
            float ring_mod = osc_sinf(s_phase_ring);
            sig = sig * (0.5f + ring_mod * 0.5f);
            s_phase_ring += ring_w;
            s_phase_ring -= (uint32_t)s_phase_ring;
        } else if (fx_type == 3) {
            float noise = noise_read();
            s_phase_noise_lfo += 0.1f / 48000.f;
            if (s_phase_noise_lfo >= 1.f) s_phase_noise_lfo -= 1.f;
            float noise_lfo = lfo_read(s_phase_noise_lfo);
            sig = sig * 0.7f + noise * 0.3f * (0.5f + noise_lfo * 0.5f);
        } else {
            sig = chorus_process(sig, 0);
        }
        
        sig = fast_tanh(sig * 1.2f);
        
        out[f] = clipminmaxf(-1.f, sig * 2.8f, 1.f);  // Volume boost!
        
        if ((s_sample_counter & 0x1) == 0) {
            s_chorus_write_pos = (s_chorus_write_pos + 1) % CHORUS_DELAY_SIZE;
        }
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_wave_mix = valf; break;
        case 1: s_fx_mix = valf; break;
        case 2: s_pulse_width = valf; break;
        case 3: s_detune = valf; break;
        case 4: s_sub_level = valf; break;
        case 5: s_hpf_cutoff = valf; break;
        case 6: s_lfo_rate = valf; break;
        case 7: s_lfo_depth = valf; break;
        case 8: s_mode_select = value; break;
        case 9: 
            s_feature_select = value;
            if (value & 0x1) {
                s_arp_pattern = 1 + ((value >> 4) & 0x7);
            }
            if (value & 0x2) {
                s_seq_running = true;
                s_current_pattern = (value >> 4) & 0x3;
            }
            break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0: return (int32_t)(s_wave_mix * 1023.f);
        case 1: return (int32_t)(s_fx_mix * 1023.f);
        case 2: return (int32_t)(s_pulse_width * 1023.f);
        case 3: return (int32_t)(s_detune * 1023.f);
        case 4: return (int32_t)(s_sub_level * 1023.f);
        case 5: return (int32_t)(s_hpf_cutoff * 1023.f);
        case 6: return (int32_t)(s_lfo_rate * 1023.f);
        case 7: return (int32_t)(s_lfo_depth * 1023.f);
        case 8: return s_mode_select;
        case 9: return s_feature_select;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
    if (id == 8) {
        static const char *mode_strs[] = {
            "PIT M", "PWM M", "FLT M", "ALL M",
            "PIT U2", "PWM U2", "FLT U2", "ALL U2",
            "PIT U3", "PWM U3", "FLT U3", "ALL U3",
            "PIT CH", "PWM CH", "FLT CH", "ALL CH"
        };
        if (value >= 0 && value < 16) {
            return mode_strs[value];
        }
    }
    if (id == 9) {
        static char feat_str[8];
        feat_str[0] = (value & 0x1) ? 'A' : '-';
        feat_str[1] = (value & 0x2) ? 'S' : '-';
        feat_str[2] = (value & 0x4) ? 'R' : '-';
        feat_str[3] = (value & 0x8) ? 'N' : '-';
        feat_str[4] = '0' + ((value >> 4) & 0xF);
        feat_str[5] = '\0';
        return feat_str;
    }
    return "";
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velo)
{
    s_last_note = note;
    s_last_velocity = velo;
    s_porta_target = (float)note;
    
    for (int i = 0; i < MAX_VOICES; i++) {
        trigger_envelope(i);
    }
    
    if (s_arp_pattern != ARP_OFF) {
        if (s_arp_count < MAX_CHORD_NOTES) {
            s_arp_notes[s_arp_count++] = note;
        }
        s_arp_running = true;
        s_arp_pos = 0;
    }
    
    if (s_chord_count < MAX_CHORD_NOTES) {
        s_chord_notes[s_chord_count] = note;
        s_chord_vels[s_chord_count] = velo;
        s_chord_count++;
    }
}

__unit_callback void unit_note_off(uint8_t note)
{
    for (int i = 0; i < MAX_VOICES; i++) {
        release_envelope(i);
    }
    
    for (int i = 0; i < s_arp_count; i++) {
        if (s_arp_notes[i] == note) {
            for (int j = i; j < s_arp_count - 1; j++) {
                s_arp_notes[j] = s_arp_notes[j + 1];
            }
            s_arp_count--;
            break;
        }
    }
    if (s_arp_count == 0) {
        s_arp_running = false;
        s_arp_pos = 0;
    }
    
    for (int i = 0; i < s_chord_count; i++) {
        if (s_chord_notes[i] == note) {
            for (int j = i; j < s_chord_count - 1; j++) {
                s_chord_notes[j] = s_chord_notes[j + 1];
                s_chord_vels[j] = s_chord_vels[j + 1];
            }
            s_chord_count--;
            break;
        }
    }
}

__unit_callback void unit_all_note_off()
{
    s_chord_count = 0;
    s_arp_count = 0;
    s_arp_running = false;
    for (int i = 0; i < MAX_VOICES; i++) {
        s_env_stage[i] = 4;
    }
}

__unit_callback void unit_set_tempo(uint32_t tempo)
{
    float bpm = (float)tempo / 10.f;
    float beat_time = 60.f / bpm;
    s_seq_step_time = (uint32_t)(beat_time * 48000.f / 4.f);
    s_arp_step_time = (uint32_t)(beat_time * 48000.f / 8.f);
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {}

__unit_callback void unit_pitch_bend(uint16_t bend)
{
    s_pitch_bend = (int16_t)bend - 8192;
}

__unit_callback void unit_channel_pressure(uint8_t press) {}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t press)
{
    s_aftertouch = press;
}
