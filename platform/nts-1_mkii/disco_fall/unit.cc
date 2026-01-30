/*
    DISCO STRING FALL - Ultimate String Synthesizer
    
    Based on working M1 Piano / ARP KUT patterns
*/

#include "unit_osc.h"
#include "osc_api.h"
#include "fx_api.h"
#include "utils/float_math.h"
#include "utils/int_math.h"

#define MAX_VOICES 4
#define SUPERSAW_VOICES 7
#define CHORUS_BUFFER_SIZE 2048

static const unit_runtime_osc_context_t *s_context;

// SuperSaw detune offsets (stereo spread)
static const float s_detune_offsets[SUPERSAW_VOICES] = {
    0.f,           // Center
    -0.08f, +0.08f,
    -0.15f, +0.15f,
    -0.22f, +0.22f
};

static const float s_detune_mix[SUPERSAW_VOICES] = {
    0.20f,         // Center louder
    0.15f, 0.15f,
    0.12f, 0.12f,
    0.10f, 0.10f
};

// Pan positions (stereo spread)
static const float s_pan_positions[SUPERSAW_VOICES] = {
    0.5f,          // Center
    0.3f, 0.7f,
    0.2f, 0.8f,
    0.1f, 0.9f
};

struct Voice {
    float phases[SUPERSAW_VOICES];
    float phase_sub;
    
    float pitch_fall_env;
    float amp_env;
    uint8_t env_stage;
    uint32_t env_counter;
    
    float current_pitch;
    float target_pitch;
    
    uint8_t note;
    uint8_t velocity;
    bool active;
};

static Voice s_voices[MAX_VOICES];

// Chorus buffers
static float s_chorus_buffer_l[CHORUS_BUFFER_SIZE];
static float s_chorus_buffer_r[CHORUS_BUFFER_SIZE];
static uint32_t s_chorus_write = 0;
static float s_chorus_lfo = 0.f;

// High-pass filter state
static float s_hpf_z1_l = 0.f;
static float s_hpf_z1_r = 0.f;

// Parameters
static float s_fall_speed = 0.6f;
static float s_fall_depth = 0.3f;
static float s_detune_amount = 0.7f;
static float s_attack_time = 0.1f;
static float s_release_time = 0.4f;
static float s_chorus_depth = 0.4f;
static float s_portamento = 0.2f;

// PolyBLEP anti-aliasing
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

// Envelope processor
inline void process_envelope(Voice* v) {
    float attack_samples = (0.01f + s_attack_time * 0.49f) * 48000.f;
    float release_samples = (0.05f + s_release_time * 1.95f) * 48000.f;
    float fall_time = 0.05f + s_fall_speed * 1.95f;
    
    v->env_counter++;
    
    switch (v->env_stage) {
        case 0: { // Attack
            if (v->env_counter < (uint32_t)attack_samples) {
                v->amp_env = (float)v->env_counter / attack_samples;
            } else {
                v->amp_env = 1.f;
                v->env_stage = 1;
                v->env_counter = 0;
            }
            break;
        }
        case 1: { // Sustain (with pitch fall)
            v->amp_env = 1.f;
            
            // Pitch fall envelope (exponential)
            float t_sec = (float)v->env_counter / 48000.f;
            v->pitch_fall_env = fx_pow2f(-t_sec / fall_time * 6.f);
            break;
        }
        case 2: { // Release
            float t_sec = (float)v->env_counter / 48000.f;
            float release_factor = 1.f - t_sec / (release_samples / 48000.f);
            if (release_factor < 0.f) release_factor = 0.f;
            
            v->amp_env *= release_factor;
            
            if (t_sec > (release_samples / 48000.f)) {
                v->env_stage = 3;
                v->active = false;
            }
            break;
        }
        case 3: // Off
            v->amp_env = 0.f;
            v->active = false;
            break;
    }
    
    // Safety clamp
    v->amp_env = clipminmaxf(0.f, v->amp_env, 1.f);
    v->pitch_fall_env = clipminmaxf(0.f, v->pitch_fall_env, 1.f);
}

// Portamento processor
inline void process_portamento(Voice* v) {
    if (s_portamento < 0.01f) {
        v->current_pitch = v->target_pitch;
        return;
    }
    
    float glide_speed = 0.001f + s_portamento * 0.049f;
    float diff = v->target_pitch - v->current_pitch;
    
    if (si_fabsf(diff) < 0.0001f) {
        v->current_pitch = v->target_pitch;
    } else {
        v->current_pitch += diff * glide_speed;
    }
}

// SuperSaw generator
inline void generate_supersaw(Voice* v, float* out_l, float* out_r) {
    float sum_l = 0.f;
    float sum_r = 0.f;
    
    // Calculate base frequency with pitch fall
    float base_w0 = v->current_pitch;
    float fall_semitones = s_fall_depth * 12.f;
    float pitch_mod = fx_pow2f(-fall_semitones * v->pitch_fall_env / 12.f);
    base_w0 *= pitch_mod;
    
    // Clamp base_w0
    base_w0 = clipminmaxf(0.0001f, base_w0, 0.45f);
    
    // Generate 7 detuned saws
    for (int i = 0; i < SUPERSAW_VOICES; i++) {
        float detune_cents = s_detune_offsets[i] * s_detune_amount * 50.f;
        float detune_ratio = fx_pow2f(detune_cents / 1200.f);
        float w = base_w0 * detune_ratio;
        w = clipminmaxf(0.0001f, w, 0.45f);
        
        // Generate sawtooth with PolyBLEP
        float p = v->phases[i];
        float saw = 2.f * p - 1.f;
        saw -= poly_blep(p, w);
        
        // Safety check
        if (p != p) p = 0.f;  // NaN check
        if (saw != saw) saw = 0.f;
        
        // Apply mix and pan
        float level = s_detune_mix[i];
        float pan = s_pan_positions[i];
        
        sum_l += saw * level * (1.f - pan);
        sum_r += saw * level * pan;
        
        // Advance phase
        v->phases[i] += w;
        while (v->phases[i] >= 1.f) v->phases[i] -= 1.f;
        while (v->phases[i] < 0.f) v->phases[i] += 1.f;
    }
    
    // Add sub oscillator (mono, -1 octave)
    float sub_w = base_w0 * 0.5f;
    sub_w = clipminmaxf(0.0001f, sub_w, 0.45f);
    
    float sub_p = v->phase_sub;
    float sub = 2.f * sub_p - 1.f;
    sub -= poly_blep(sub_p, sub_w);
    sub *= 0.25f;
    
    // Safety check
    if (sub != sub) sub = 0.f;
    
    sum_l += sub;
    sum_r += sub;
    
    v->phase_sub += sub_w;
    while (v->phase_sub >= 1.f) v->phase_sub -= 1.f;
    while (v->phase_sub < 0.f) v->phase_sub += 1.f;
    
    *out_l = sum_l;
    *out_r = sum_r;
}

// High-pass filter (30Hz cutoff)
inline void process_hpf(float* in_l, float* in_r) {
    float cutoff_hz = 30.f;
    float w = 2.f * 3.14159265f * cutoff_hz / 48000.f;
    w = clipminmaxf(0.001f, w, 3.14159265f * 0.95f);
    
    // Use fasttanfullf for tangent calculation
    float g = fasttanfullf(w * 0.5f);
    g = clipminmaxf(0.001f, g, 10.f);
    
    float alpha = g / (1.f + g);
    
    s_hpf_z1_l = alpha * (*in_l - s_hpf_z1_l) + s_hpf_z1_l;
    s_hpf_z1_r = alpha * (*in_r - s_hpf_z1_r) + s_hpf_z1_r;
    
    // Denormal kill
    if (si_fabsf(s_hpf_z1_l) < 1e-15f) s_hpf_z1_l = 0.f;
    if (si_fabsf(s_hpf_z1_r) < 1e-15f) s_hpf_z1_r = 0.f;
    
    *in_l = *in_l - s_hpf_z1_l;
    *in_r = *in_r - s_hpf_z1_r;
}

// Chorus processor
inline void process_chorus(float* in_l, float* in_r) {
    if (s_chorus_depth < 0.01f) return;
    
    s_chorus_buffer_l[s_chorus_write] = *in_l;
    s_chorus_buffer_r[s_chorus_write] = *in_r;
    
    s_chorus_lfo += 0.6f / 48000.f;
    if (s_chorus_lfo >= 1.f) s_chorus_lfo -= 1.f;
    if (s_chorus_lfo < 0.f) s_chorus_lfo += 1.f;
    
    float lfo = osc_sinf(s_chorus_lfo);
    float delay_samples = 600.f + lfo * 300.f * s_chorus_depth;
    delay_samples = clipminmaxf(1.f, delay_samples, (float)(CHORUS_BUFFER_SIZE - 2));
    
    uint32_t delay_int = (uint32_t)delay_samples;
    uint32_t read_pos = (s_chorus_write + CHORUS_BUFFER_SIZE - delay_int) % CHORUS_BUFFER_SIZE;
    
    float wet_l = s_chorus_buffer_l[read_pos];
    float wet_r = s_chorus_buffer_r[read_pos];
    
    // Safety check
    if (wet_l != wet_l) wet_l = 0.f;
    if (wet_r != wet_r) wet_r = 0.f;
    
    float mix = s_chorus_depth * 0.3f;
    *in_l = *in_l * (1.f - mix) + wet_l * mix;
    *in_r = *in_r * (1.f - mix) + wet_r * mix;
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
    for (int v = 0; v < MAX_VOICES; v++) {
        for (int i = 0; i < SUPERSAW_VOICES; i++) {
            s_voices[v].phases[i] = 0.f;
        }
        s_voices[v].phase_sub = 0.f;
        s_voices[v].pitch_fall_env = 0.f;
        s_voices[v].amp_env = 0.f;
        s_voices[v].env_stage = 3;
        s_voices[v].env_counter = 0;
        s_voices[v].current_pitch = 0.f;
        s_voices[v].target_pitch = 0.f;
        s_voices[v].active = false;
        s_voices[v].note = 60;
        s_voices[v].velocity = 100;
    }
    
    // Init chorus
    for (int i = 0; i < CHORUS_BUFFER_SIZE; i++) {
        s_chorus_buffer_l[i] = 0.f;
        s_chorus_buffer_r[i] = 0.f;
    }
    s_chorus_write = 0;
    s_chorus_lfo = 0.f;
    
    // Init filters
    s_hpf_z1_l = 0.f;
    s_hpf_z1_r = 0.f;
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    for (int v = 0; v < MAX_VOICES; v++) {
        s_voices[v].active = false;
        s_voices[v].env_stage = 3;
    }
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    (void)in;
    
    for (uint32_t f = 0; f < frames; f++) {
        float sum_l = 0.f;
        float sum_r = 0.f;
        int active_count = 0;
        
        for (int v = 0; v < MAX_VOICES; v++) {
            Voice* voice = &s_voices[v];
            if (!voice->active) continue;
            
            process_envelope(voice);
            process_portamento(voice);
            
            if (voice->amp_env < 0.001f && voice->env_stage >= 2) {
                voice->active = false;
                continue;
            }
            
            float voice_l = 0.f;
            float voice_r = 0.f;
            generate_supersaw(voice, &voice_l, &voice_r);
            
            // Safety check
            if (voice_l != voice_l) voice_l = 0.f;
            if (voice_r != voice_r) voice_r = 0.f;
            
            // Apply envelope
            voice_l *= voice->amp_env;
            voice_r *= voice->amp_env;
            
            // Velocity sensitivity
            float vel_scale = (float)voice->velocity / 127.f;
            vel_scale = 0.5f + vel_scale * 0.5f;
            voice_l *= vel_scale;
            voice_r *= vel_scale;
            
            sum_l += voice_l;
            sum_r += voice_r;
            active_count++;
        }
        
        if (active_count > 0) {
            sum_l /= (float)active_count;
            sum_r /= (float)active_count;
        }
        
        // Safety check
        if (sum_l != sum_l) sum_l = 0.f;
        if (sum_r != sum_r) sum_r = 0.f;
        
        // High-pass filter
        process_hpf(&sum_l, &sum_r);
        
        // Chorus
        process_chorus(&sum_l, &sum_r);
        
        // Mono mix
        float mono = (sum_l + sum_r) * 0.5f;
        
        // Safety check
        if (mono != mono) mono = 0.f;
        
        // Output gain
        mono *= 2.2f;
        
        // Hard limit
        out[f] = clipminmaxf(-1.f, mono, 1.f);
        
        s_chorus_write = (s_chorus_write + 1) % CHORUS_BUFFER_SIZE;
    }
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
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
    for (int i = 0; i < SUPERSAW_VOICES; i++) {
        voice->phases[i] = 0.f;
    }
    voice->phase_sub = 0.f;
    
    // Set pitch
    voice->target_pitch = osc_w0f_for_note(note, 0);
    if (s_portamento < 0.01f) {
        voice->current_pitch = voice->target_pitch;
    }
    
    // Trigger envelope
    voice->env_stage = 0;
    voice->env_counter = 0;
    voice->amp_env = 0.f;
    voice->pitch_fall_env = 1.f;
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
        case 0: s_fall_speed = valf; break;
        case 1: s_fall_depth = valf; break;
        case 2: s_detune_amount = valf; break;
        case 3: s_attack_time = valf; break;
        case 4: s_release_time = valf; break;
        case 5: s_chorus_depth = valf; break;
        case 6: s_portamento = valf; break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return (int32_t)(s_fall_speed * 1023.f);
        case 1: return (int32_t)(s_fall_depth * 1023.f);
        case 2: return (int32_t)(s_detune_amount * 1023.f);
        case 3: return (int32_t)(s_attack_time * 1023.f);
        case 4: return (int32_t)(s_release_time * 1023.f);
        case 5: return (int32_t)(s_chorus_depth * 1023.f);
        case 6: return (int32_t)(s_portamento * 1023.f);
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
