/*
    CHIKUTCAGO - Chicago House Melodic Oscillator
    
    5 AUTHENTIC CHICAGO HOUSE SOUNDS
*/

#include "unit_osc.h"
#include "osc_api.h"
#include "fx_api.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "macros.h"

#define MAX_VOICES 2
#define CHORUS_BUFFER_SIZE 2048

static const unit_runtime_osc_context_t *s_context;

// ========== SOUND TYPES ==========

enum ChicagoSound {
    SOUND_HOUSE_PIANO = 0,
    SOUND_DEEP_FLUTE,
    SOUND_BRASS_STAB,
    SOUND_WAREHOUSE_BELL,
    SOUND_ACID_DRONE
};

const char* sound_names[5] = {
    "PIANO", "FLUTE", "BRASS", "BELL", "DRONE"
};

// ========== VOICE STRUCTURE ==========

struct Voice {
    float phase_carrier;
    float phase_modulator;
    float phase_sub;
    
    float env_level;
    uint8_t env_stage;
    uint32_t env_counter;
    
    float filter_z1;
    float filter_z2;
    
    uint8_t note;
    uint8_t velocity;
    bool active;
};

static Voice s_voices[MAX_VOICES];

// ========== NOISE STATE ==========

static uint32_t s_noise_seed = 0x12345678;

inline float white_noise() {
    s_noise_seed = s_noise_seed * 1103515245u + 12345u;
    return ((float)(s_noise_seed >> 16) / 32768.f) - 1.f;
}

// ========== CHORUS BUFFER ==========

static float s_chorus_buffer_l[CHORUS_BUFFER_SIZE];
static float s_chorus_buffer_r[CHORUS_BUFFER_SIZE];
static uint32_t s_chorus_write = 0;
static float s_chorus_lfo = 0.f;

// ========== PARAMETERS ==========

static uint8_t s_sound_type = SOUND_HOUSE_PIANO;
static float s_brightness = 0.6f;
static float s_decay_time = 0.5f;
static float s_detune = 0.3f;
static float s_attack_click = 0.75f;
static float s_warmth = 0.4f;
static float s_body = 0.25f;
static float s_release_time = 0.2f;
static float s_velocity_sens = 0.5f;
static float s_chorus_depth = 0.3f;

// ========== POLY BLEP (ANTI-ALIASING) ==========

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

// ========== FAST TANH ==========

inline float fast_tanh(float x) {
    if (x < -3.f) return -1.f;
    if (x > 3.f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

// ========== ENVELOPE PROCESSOR ==========

inline float process_envelope(Voice* v) {
    float attack_time = 0.002f + s_attack_click * 0.018f;
    float decay_time = 0.1f + s_decay_time * 1.9f;
    float release_time = 0.05f + s_release_time * 0.95f;
    
    float t_sec = (float)v->env_counter / 48000.f;
    
    switch (v->env_stage) {
        case 0: { // Attack
            float attack_samples = attack_time * 48000.f;
            if (v->env_counter < (uint32_t)attack_samples) {
                v->env_level = (float)v->env_counter / attack_samples;
            } else {
                v->env_level = 1.f;
                v->env_stage = 1;
                v->env_counter = 0;
            }
            break;
        }
        case 1: { // Decay
            v->env_level = fx_pow2f(-t_sec / decay_time * 6.f);
            if (v->env_level < 0.001f) {
                v->env_stage = 2;
            }
            break;
        }
        case 2: { // Release
            float release_factor = 1.f - t_sec / release_time;
            if (release_factor < 0.f) release_factor = 0.f;
            v->env_level *= release_factor;
            if (t_sec > release_time) {
                v->env_stage = 3;
                v->active = false;
            }
            break;
        }
        case 3: // Off
            v->env_level = 0.f;
            v->active = false;
            break;
    }
    
    v->env_counter++;
    return clipminmaxf(0.f, v->env_level, 1.f);
}

// ========== SOUND 0: HOUSE PIANO (2-OP FM) ==========

inline float house_piano(Voice* v, float w0, float env) {
    // FM synthesis: Carrier modulated by modulator
    float fm_ratio = 4.2f;  // Non-integer for metallic character
    float mod_index = 8.f + s_brightness * 12.f;
    
    // Modulator
    float mod_phase = v->phase_modulator * fm_ratio;
    mod_phase -= (uint32_t)mod_phase;
    float modulator = osc_sinf(mod_phase);
    
    // Carrier with FM
    float carrier_phase = v->phase_carrier + modulator * mod_index * w0;
    carrier_phase -= (uint32_t)carrier_phase;
    float carrier = osc_sinf(carrier_phase);
    
    // Attack transient (click)
    float click_env = (v->env_stage == 0) ? (1.f - env) : 0.f;
    float attack_transient = click_env * s_attack_click * 0.3f;
    
    float output = carrier + attack_transient;
    
    // Advance phases
    v->phase_carrier += w0;
    v->phase_carrier -= (uint32_t)v->phase_carrier;
    v->phase_modulator += w0;
    v->phase_modulator -= (uint32_t)v->phase_modulator;
    
    return output;
}

// ========== SOUND 1: DEEP FLUTE (PARABOLIC SINE + NOISE) ==========

inline float deep_flute(Voice* v, float w0, float env) {
    // Parabolic sine approximation (woody tone)
    float x = v->phase_carrier * 2.f - 1.f;
    float parabolic = 4.f * x * (1.f - si_fabsf(x));
    
    // Add breathiness (noise) during attack
    float noise_amount = (v->env_stage == 0) ? (1.f - env) * 0.2f : 0.05f;
    float noise = white_noise() * noise_amount;
    
    float output = parabolic + noise;
    
    // Simple lowpass filter (dampen highs)
    float cutoff = 0.3f + s_brightness * 0.6f;
    v->filter_z1 += cutoff * (output - v->filter_z1);
    
    // Denormal kill
    if (si_fabsf(v->filter_z1) < 1e-15f) v->filter_z1 = 0.f;
    
    // Advance phase
    v->phase_carrier += w0;
    v->phase_carrier -= (uint32_t)v->phase_carrier;
    
    return v->filter_z1;
}

// ========== SOUND 2: BRASS STAB (BAND-LIMITED SAWTOOTH) ==========

inline float brass_stab(Voice* v, float w0, float env) {
    // Band-limited sawtooth with PolyBLEP
    float saw = 2.f * v->phase_carrier - 1.f;
    saw -= poly_blep(v->phase_carrier, w0);
    
    // Add sub oscillator
    float sub_phase = v->phase_sub;
    float sub = osc_sinf(sub_phase);
    
    float output = saw + sub * 0.3f * s_warmth;
    
    // State variable filter (resonant)
    float cutoff_hz = 300.f + env * s_brightness * 4000.f;
    float w = 2.f * 3.14159265f * cutoff_hz / 48000.f;
    float f = 2.f * osc_sinf(w * 0.5f / (2.f * 3.14159265f));
    f = clipminmaxf(0.001f, f, 1.4f);
    
    float q = 1.f / (0.5f + s_body * 1.5f);
    q = clipminmaxf(0.5f, q, 2.f);
    
    v->filter_z2 += f * v->filter_z1;
    float hp = output - v->filter_z2 - q * v->filter_z1;
    v->filter_z1 += f * hp;
    
    // Denormal kill
    if (si_fabsf(v->filter_z1) < 1e-15f) v->filter_z1 = 0.f;
    if (si_fabsf(v->filter_z2) < 1e-15f) v->filter_z2 = 0.f;
    
    // Advance phases
    v->phase_carrier += w0;
    v->phase_carrier -= (uint32_t)v->phase_carrier;
    v->phase_sub += w0 * 0.5f;
    v->phase_sub -= (uint32_t)v->phase_sub;
    
    return v->filter_z2;  // Lowpass output
}

// ========== SOUND 3: WAREHOUSE BELL (INHARMONIC FM) ==========

inline float warehouse_bell(Voice* v, float w0, float env) {
    // Inharmonic FM ratio (√2 for metallic dissonance)
    float fm_ratio = 1.414f;
    float mod_index = 3.f + s_brightness * 9.f;
    
    // Fast decay on FM index
    mod_index *= env;
    
    // Modulator
    float mod_phase = v->phase_modulator * fm_ratio;
    mod_phase -= (uint32_t)mod_phase;
    float modulator = osc_sinf(mod_phase);
    
    // Carrier
    float carrier_phase = v->phase_carrier + modulator * mod_index * w0;
    carrier_phase -= (uint32_t)carrier_phase;
    float carrier = osc_sinf(carrier_phase);
    
    // Advance phases
    v->phase_carrier += w0;
    v->phase_carrier -= (uint32_t)v->phase_carrier;
    v->phase_modulator += w0;
    v->phase_modulator -= (uint32_t)v->phase_modulator;
    
    return carrier;
}

// ========== SOUND 4: ACID DRONE (COMPLEX FM) ==========

inline float acid_drone(Voice* v, float w0, float env) {
    // Two modulators at odd ratios
    float fm_ratio1 = 1.3f;
    float fm_ratio2 = 3.5f;
    
    float mod_index = 2.f + s_brightness * 6.f;
    
    // Slow LFO on index (timbre movement)
    static float lfo_phase = 0.f;
    lfo_phase += 0.3f / 48000.f;
    if (lfo_phase >= 1.f) lfo_phase -= 1.f;
    float lfo = osc_sinf(lfo_phase);
    mod_index *= (1.f + lfo * 0.5f);
    
    // Modulator 1
    float mod1_phase = v->phase_modulator * fm_ratio1;
    mod1_phase -= (uint32_t)mod1_phase;
    float mod1 = osc_sinf(mod1_phase);
    
    // Modulator 2
    float mod2_phase = v->phase_modulator * fm_ratio2;
    mod2_phase -= (uint32_t)mod2_phase;
    float mod2 = osc_sinf(mod2_phase);
    
    // Carrier
    float combined_mod = (mod1 + mod2) * 0.5f;
    float carrier_phase = v->phase_carrier + combined_mod * mod_index * w0;
    carrier_phase -= (uint32_t)carrier_phase;
    float carrier = osc_sinf(carrier_phase);
    
    // Advance phases
    v->phase_carrier += w0;
    v->phase_carrier -= (uint32_t)v->phase_carrier;
    v->phase_modulator += w0;
    v->phase_modulator -= (uint32_t)v->phase_modulator;
    
    return carrier;
}

// ========== CHORUS PROCESSOR ==========

inline float process_chorus(float input, int channel) {
    if (s_chorus_depth < 0.01f) return input;
    
    float* buffer = (channel == 0) ? s_chorus_buffer_l : s_chorus_buffer_r;
    
    buffer[s_chorus_write] = input;
    
    s_chorus_lfo += 0.5f / 48000.f;
    if (s_chorus_lfo >= 1.f) s_chorus_lfo -= 1.f;
    
    float lfo = osc_sinf(s_chorus_lfo);
    float delay_samples = 800.f + lfo * 400.f * s_chorus_depth + (float)channel * 100.f;
    
    uint32_t delay_int = (uint32_t)delay_samples;
    uint32_t read_pos = (s_chorus_write + CHORUS_BUFFER_SIZE - delay_int) % CHORUS_BUFFER_SIZE;
    
    float wet = buffer[read_pos];
    return input * (1.f - s_chorus_depth * 0.4f) + wet * s_chorus_depth * 0.4f;
}

// ========== MAIN OSCILLATOR ==========

inline float generate_oscillator() {
    float sum = 0.f;
    int active_voices = 0;
    
    for (int v = 0; v < MAX_VOICES; v++) {
        Voice* voice = &s_voices[v];
        if (!voice->active) continue;
        
        float env = process_envelope(voice);
        
        if (env < 0.001f && voice->env_stage >= 2) {
            voice->active = false;
            continue;
        }
        
        float w0 = osc_w0f_for_note(voice->note, 0);
        
        // Apply detune for second voice
        if (v > 0) {
            float detune_cents = s_detune * 20.f;
            w0 *= fx_pow2f(detune_cents / 1200.f);
        }
        
        float sample = 0.f;
        
        // Generate sound based on type
        switch (s_sound_type) {
            case SOUND_HOUSE_PIANO:
                sample = house_piano(voice, w0, env);
                break;
            case SOUND_DEEP_FLUTE:
                sample = deep_flute(voice, w0, env);
                break;
            case SOUND_BRASS_STAB:
                sample = brass_stab(voice, w0, env);
                break;
            case SOUND_WAREHOUSE_BELL:
                sample = warehouse_bell(voice, w0, env);
                break;
            case SOUND_ACID_DRONE:
                sample = acid_drone(voice, w0, env);
                break;
        }
        
        // Apply envelope
        sample *= env;
        
        // Velocity sensitivity
        float vel_scale = (float)voice->velocity / 127.f;
        vel_scale = 0.4f + vel_scale * 0.6f * s_velocity_sens + (1.f - s_velocity_sens) * 0.6f;
        sample *= vel_scale;
        
        sum += sample;
        active_voices++;
    }
    
    if (active_voices > 0) {
        sum /= (float)active_voices;
    }
    
    // Chorus
    sum = process_chorus(sum, 0);
    
    // Soft clipping
    sum = fast_tanh(sum * 1.2f);
    
    return sum;
}

// ========== UNIT CALLBACKS ==========

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 1) return k_unit_err_geometry;
    
    s_context = static_cast<const unit_runtime_osc_context_t*>(desc->hooks.runtime_context);
    
    // Init voices
    for (int i = 0; i < MAX_VOICES; i++) {
        s_voices[i].phase_carrier = 0.f;
        s_voices[i].phase_modulator = 0.f;
        s_voices[i].phase_sub = 0.f;
        s_voices[i].env_level = 0.f;
        s_voices[i].env_stage = 3;
        s_voices[i].env_counter = 0;
        s_voices[i].filter_z1 = 0.f;
        s_voices[i].filter_z2 = 0.f;
        s_voices[i].active = false;
    }
    
    // Init chorus
    for (int i = 0; i < CHORUS_BUFFER_SIZE; i++) {
        s_chorus_buffer_l[i] = 0.f;
        s_chorus_buffer_r[i] = 0.f;
    }
    s_chorus_write = 0;
    s_chorus_lfo = 0.f;
    
    // Init parameters
    s_sound_type = SOUND_HOUSE_PIANO;
    s_brightness = 0.6f;
    s_decay_time = 0.5f;
    s_detune = 0.3f;
    s_attack_click = 0.75f;
    s_warmth = 0.4f;
    s_body = 0.25f;
    s_release_time = 0.2f;
    s_velocity_sens = 0.5f;
    s_chorus_depth = 0.3f;
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    for (int i = 0; i < MAX_VOICES; i++) {
        s_voices[i].active = false;
    }
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    (void)in;
    
    for (uint32_t f = 0; f < frames; f++) {
        float sample = generate_oscillator();
        
        // Output gain (2.5× for proper level)
        sample *= 2.5f;
        
        // Hard limit
        out[f] = clipminmaxf(-1.f, sample, 1.f);
        
        // Advance chorus
        s_chorus_write = (s_chorus_write + 1) % CHORUS_BUFFER_SIZE;
    }
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
    // Find free voice
    int free_voice = -1;
    for (int v = 0; v < MAX_VOICES; v++) {
        if (!s_voices[v].active) {
            free_voice = v;
            break;
        }
    }
    
    if (free_voice == -1) free_voice = 0;
    
    Voice* voice = &s_voices[free_voice];
    voice->note = note;
    voice->velocity = velocity;
    voice->active = true;
    
    // Reset phases
    voice->phase_carrier = 0.f;
    voice->phase_modulator = 0.f;
    voice->phase_sub = 0.f;
    
    // Reset filters
    voice->filter_z1 = 0.f;
    voice->filter_z2 = 0.f;
    
    // Trigger envelope
    voice->env_stage = 0;
    voice->env_counter = 0;
    voice->env_level = 0.f;
}

__unit_callback void unit_note_off(uint8_t note) {
    for (int v = 0; v < MAX_VOICES; v++) {
        if (s_voices[v].note == note && s_voices[v].active) {
            if (s_voices[v].env_stage < 2) {
                s_voices[v].env_stage = 2;
                s_voices[v].env_counter = 0;
            }
        }
    }
}

__unit_callback void unit_all_note_off() {
    for (int v = 0; v < MAX_VOICES; v++) {
        s_voices[v].active = false;
        s_voices[v].env_stage = 3;
    }
}

__unit_callback void unit_pitch_bend(uint16_t bend) {
    (void)bend;
}

__unit_callback void unit_channel_pressure(uint8_t pressure) {
    (void)pressure;
}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t aftertouch) {
    (void)note;
    (void)aftertouch;
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_sound_type = (ChicagoSound)value; break;
        case 1: s_brightness = valf; break;
        case 2: s_decay_time = valf; break;
        case 3: s_detune = valf; break;
        case 4: s_attack_click = valf; break;
        case 5: s_warmth = valf; break;
        case 6: s_body = valf; break;
        case 7: s_release_time = valf; break;
        case 8: s_velocity_sens = valf; break;
        case 9: s_chorus_depth = valf; break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return (int32_t)s_sound_type;
        case 1: return (int32_t)(s_brightness * 1023.f);
        case 2: return (int32_t)(s_decay_time * 1023.f);
        case 3: return (int32_t)(s_detune * 1023.f);
        case 4: return (int32_t)(s_attack_click * 1023.f);
        case 5: return (int32_t)(s_warmth * 1023.f);
        case 6: return (int32_t)(s_body * 1023.f);
        case 7: return (int32_t)(s_release_time * 1023.f);
        case 8: return (int32_t)(s_velocity_sens * 1023.f);
        case 9: return (int32_t)(s_chorus_depth * 1023.f);
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    if (id == 0 && value >= 0 && value < 5) {
        return sound_names[value];
    }
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    (void)tempo;
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    (void)counter;
}

