/*
    DISCO STRING FALL - String Synthesizer (CORRECTED)
    
    Based on Korg logue SDK patterns
    https://github.com/korginc/logue-sdk
    
    FIXES:
    - Correct PolyBLEP anti-aliasing formula
    - Proper detune scaling (0-1 range)
    - Realistic pitch fall depth (max 12 semitones)
    - Correct output gain (2.5x)
    - Simplified signal flow
*/

#include "unit_osc.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "osc_api.h"

#define MAX_VOICES 4
#define SUPERSAW_SAWS 7

static const unit_runtime_osc_context_t *s_context;

// SuperSaw detune values (cents) - Adam Szabo JP-8000 algorithm
static const float s_supersaw_detune[SUPERSAW_SAWS] = {
    0.0f,          // Center
    -11.002313f,   // Outer left
    11.002313f,    // Outer right
    -6.288439f,    // Middle left
    6.288439f,     // Middle right
    -1.952356f,    // Inner left
    1.952356f      // Inner right
};

// Mix levels (normalized to sum ~1.0)
static const float s_supersaw_mix[SUPERSAW_SAWS] = {
    0.2188f,  // Center
    0.1405f,  // Outer left
    0.1405f,  // Outer right
    0.1405f,  // Middle left
    0.1405f,  // Middle right
    0.0906f,  // Inner left
    0.0906f   // Inner right
};

// Pan positions for stereo spread
static const float s_supersaw_pan[SUPERSAW_SAWS] = {
    0.0f,   // Center
    -0.8f,  // Outer left
    0.8f,   // Outer right
    -0.5f,  // Middle left
    0.5f,   // Middle right
    -0.2f,  // Inner left
    0.2f    // Inner right
};

struct Voice {
    bool active;
    uint8_t note;
    uint8_t velocity;
    
    // Oscillator phases
    float supersaw_phases[SUPERSAW_SAWS];
    float sub_phase;
    
    // Envelopes
    float pitch_fall_env;
    uint32_t pitch_fall_counter;
    float amp_env;
    uint8_t amp_stage;  // 0=attack, 1=release
    uint32_t amp_counter;
    
    // Portamento
    float current_pitch;
    float target_pitch;
};

static Voice s_voices[MAX_VOICES];

// Parameters
static float s_fall_speed;
static float s_fall_depth;
static float s_detune_amount;
static float s_attack_time;
static float s_release_time;
static float s_chorus_depth;
static float s_portamento_time;

// Fast exponential approximation
inline float fast_exp(float x) {
    if (x < -5.f) return 0.f;
    if (x > 5.f) return 148.f;
    x = 1.f + x * 0.00390625f;
    x *= x; x *= x; x *= x; x *= x;
    x *= x; x *= x; x *= x; x *= x;
    return x;
}

// CORRECT PolyBLEP formula for sawtooth
// Source: https://www.kvraudio.com/forum/viewtopic.php?t=398553
inline float poly_blep(float t, float dt) {
    // For t near 0 (falling edge)
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.f;
    }
    // For t near 1 (rising edge)
    else if (t > 1.f - dt) {
        t = (t - 1.f) / dt;
        return t * t + t + t + 1.f;
    }
    return 0.f;
}

// Generate SuperSaw with proper anti-aliasing
inline void generate_supersaw(Voice *v, float base_w0, float detune_scale, float *out_l, float *out_r) {
    float sum_l = 0.f;
    float sum_r = 0.f;
    
    for (int i = 0; i < SUPERSAW_SAWS; i++) {
        // Apply detune (scale 0-1)
        float detune_cents = s_supersaw_detune[i] * detune_scale;
        float w0 = base_w0 * fastpow2f(detune_cents / 1200.f);
        
        // Nyquist limit
        if (w0 > 0.48f) w0 = 0.48f;
        
        // Generate sawtooth with PolyBLEP
        float saw = 2.f * v->supersaw_phases[i] - 1.f;
        saw -= poly_blep(v->supersaw_phases[i], w0);
        
        // Apply mix and pan
        float mix = s_supersaw_mix[i];
        float pan = s_supersaw_pan[i];
        
        // Constant power panning
        float pan_rad = pan * 0.7853981634f;  // ±45 degrees
        float gain_l = mix * osc_cosf(pan_rad);
        float gain_r = mix * osc_sinf(pan_rad);
        
        sum_l += saw * gain_l;
        sum_r += saw * gain_r;
        
        // Update phase
        v->supersaw_phases[i] += w0;
        if (v->supersaw_phases[i] >= 1.f) {
            v->supersaw_phases[i] -= 1.f;
        }
    }
    
    *out_l = sum_l;
    *out_r = sum_r;
}

// Pitch fall envelope (one-shot exponential)
inline float update_pitch_fall(Voice *v) {
    float t_sec = (float)v->pitch_fall_counter / 48000.f;
    
    // Exponential curve
    float speed = 0.05f + s_fall_speed * 0.95f;  // 50-1000ms
    float env = 1.f - fast_exp(-t_sec / speed * 5.f);
    
    v->pitch_fall_env = env;
    v->pitch_fall_counter++;
    
    // Return pitch offset (max 12 semitones = 1 octave)
    float depth = s_fall_depth * 12.f;
    return -depth * env;
}

// Amplitude envelope (ADSR with fixed sustain)
inline float update_amp_env(Voice *v) {
    float t_sec = (float)v->amp_counter / 48000.f;
    
    if (v->amp_stage == 0) {
        // Attack
        float attack = 0.001f + s_attack_time * 0.499f;  // 1-500ms
        v->amp_env = clipminmaxf(0.f, t_sec / attack, 1.f);
        
        if (v->amp_env >= 0.99f) {
            v->amp_stage = 1;
            v->amp_counter = 0;
        }
    } else {
        // Release
        float release = 0.1f + s_release_time * 2.9f;  // 100-3000ms
        v->amp_env = fast_exp(-t_sec / release * 5.f);
        
        if (v->amp_env < 0.001f) {
            v->active = false;
        }
    }
    
    v->amp_counter++;
    return v->amp_env;
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc)
{
    if (!desc) return k_unit_err_undef;

    s_context = static_cast<const unit_runtime_osc_context_t *>(desc->hooks.runtime_context);

    // Init voices
    for (int v = 0; v < MAX_VOICES; v++) {
        s_voices[v].active = false;
        s_voices[v].amp_env = 0.f;
        s_voices[v].pitch_fall_env = 0.f;
        s_voices[v].current_pitch = 60.f;
        s_voices[v].target_pitch = 60.f;
        
        for (int i = 0; i < SUPERSAW_SAWS; i++) {
            s_voices[v].supersaw_phases[i] = 0.f;
        }
        s_voices[v].sub_phase = 0.f;
    }
    
    // Init parameters with musical defaults
    s_fall_speed = 0.6f;    // Medium speed
    s_fall_depth = 0.3f;    // Subtle fall
    s_detune_amount = 0.7f; // Nice detuning
    s_attack_time = 0.1f;   // Quick attack
    s_release_time = 0.4f;  // Medium release
    s_chorus_depth = 0.4f;  // Subtle chorus
    s_portamento_time = 0.2f;

    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset()
{
    for (int v = 0; v < MAX_VOICES; v++) {
        s_voices[v].active = false;
    }
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames)
{
    uint8_t base_note = (s_context->pitch >> 8) & 0xFF;
    uint8_t mod = s_context->pitch & 0xFF;
    
    for (uint32_t f = 0; f < frames; f++) {
        float sig_l = 0.f;
        float sig_r = 0.f;
        
        // Render all active voices
        for (int v = 0; v < MAX_VOICES; v++) {
            Voice *voice = &s_voices[v];
            if (!voice->active) continue;
            
            // Portamento (smooth pitch glide)
            float delta = voice->target_pitch - voice->current_pitch;
            float porta_speed = 0.001f + s_portamento_time * 0.05f;
            voice->current_pitch += delta * porta_speed;
            
            // Pitch fall envelope
            float pitch_offset = update_pitch_fall(voice);
            
            // Final pitch
            float final_pitch = voice->current_pitch + pitch_offset;
            float w0 = osc_w0f_for_note((uint8_t)final_pitch, mod);
            
            // Generate SuperSaw (detune 0-1 range)
            float saw_l, saw_r;
            generate_supersaw(voice, w0, s_detune_amount, &saw_l, &saw_r);
            
            // Sub oscillator (square wave -1 octave)
            float sub = (voice->sub_phase < 0.5f) ? 0.25f : -0.25f;
            voice->sub_phase += w0 * 0.5f;
            if (voice->sub_phase >= 1.f) {
                voice->sub_phase -= 1.f;
            }
            
            saw_l += sub;
            saw_r += sub;
            
            // Amplitude envelope
            float amp = update_amp_env(voice);
            
            // Velocity scaling
            float vel_scale = (float)voice->velocity / 127.f;
            vel_scale = 0.6f + vel_scale * 0.4f;  // 60-100% range
            
            sig_l += saw_l * amp * vel_scale;
            sig_r += saw_r * amp * vel_scale;
        }
        
        // Mix to mono
        float mono = (sig_l + sig_r) * 0.5f;
        
        // Soft clipping
        if (mono > 1.f) mono = 1.f - 0.1f * (mono - 1.f);
        if (mono < -1.f) mono = -1.f + 0.1f * (-mono - 1.f);
        
        // OUTPUT with proper gain (2.5x as per working oscillators)
        out[f] = clipminmaxf(-1.f, mono * 2.5f, 1.f);
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_fall_speed = valf; break;
        case 1: s_fall_depth = valf; break;
        case 2: s_detune_amount = valf; break;
        case 3: s_attack_time = valf; break;
        case 4: s_release_time = valf; break;
        case 5: s_chorus_depth = valf; break;
        case 6: s_portamento_time = valf; break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0: return (int32_t)(s_fall_speed * 1023.f);
        case 1: return (int32_t)(s_fall_depth * 1023.f);
        case 2: return (int32_t)(s_detune_amount * 1023.f);
        case 3: return (int32_t)(s_attack_time * 1023.f);
        case 4: return (int32_t)(s_release_time * 1023.f);
        case 5: return (int32_t)(s_chorus_depth * 1023.f);
        case 6: return (int32_t)(s_portamento_time * 1023.f);
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
    return "";
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velo)
{
    // Find free voice
    int free_voice = -1;
    for (int v = 0; v < MAX_VOICES; v++) {
        if (!s_voices[v].active) {
            free_voice = v;
            break;
        }
    }
    
    if (free_voice == -1) free_voice = 0;  // Voice stealing
    
    Voice *voice = &s_voices[free_voice];
    voice->active = true;
    voice->note = note;
    voice->velocity = velo;
    voice->target_pitch = (float)note;
    
    // Portamento: only glide if already playing
    if (voice->current_pitch < 1.f) {
        voice->current_pitch = (float)note;
    }
    
    // Reset envelopes
    voice->pitch_fall_counter = 0;
    voice->pitch_fall_env = 0.f;
    voice->amp_counter = 0;
    voice->amp_stage = 0;
    voice->amp_env = 0.f;
    
    // Reset phases
    for (int i = 0; i < SUPERSAW_SAWS; i++) {
        voice->supersaw_phases[i] = 0.f;
    }
    voice->sub_phase = 0.f;
}

__unit_callback void unit_note_off(uint8_t note)
{
    for (int v = 0; v < MAX_VOICES; v++) {
        if (s_voices[v].active && s_voices[v].note == note) {
            s_voices[v].amp_stage = 1;  // Start release
            s_voices[v].amp_counter = 0;
        }
    }
}

__unit_callback void unit_all_note_off()
{
    for (int v = 0; v < MAX_VOICES; v++) {
        s_voices[v].active = false;
    }
}

__unit_callback void unit_set_tempo(uint32_t tempo) {}
__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {}
__unit_callback void unit_pitch_bend(uint16_t bend) {}
__unit_callback void unit_channel_pressure(uint8_t press) {}
__unit_callback void unit_aftertouch(uint8_t note, uint8_t press) {}
