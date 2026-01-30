/*
    KORG M1 "ORGAN 2" - 2-OPERATOR FM SYNTHESIS ENGINE
    
    SYNTHESIS ARCHITECTURE:
    
    2-OPERATOR FM:
    - Carrier: Sine wave (fundamental)
    - Modulator: Sine wave (2:1 ratio - 2nd harmonic)
    - Modulation Index: Controlled by percussive envelope
    
    THE "ORGAN 2" CHARACTER:
    1. HOLLOW TONE:
       - Low modulation index creates hollow sine
       - FM adds "woody" character
       - Sub oscillator adds weight
    
    2. PERCUSSIVE ATTACK:
       - Fast envelope on modulation index
       - Creates "bonk" transient
       - Quick decay to sustained tone
    
    3. ADDITIONAL FEATURES:
       - Sub oscillator (square wave -1 octave)
       - Chorus (stereo detune)
       - Overdrive/saturation
       - Multi-voice polyphony
       - Velocity layers
    
    FM FORMULA:
    output = sin(2π * carrier_phase + mod_index * sin(2π * modulator_phase))
    
    MODULATION ENVELOPE:
    - Attack: 1-5ms (instant percussive click)
    - Decay: 50-500ms (controls "bonk" character)
    - Sustain: 10-40% (hollow organ sustain)
    - Release: 50-2000ms
    
    PRESETS:
    0. ROBIN S - Classic "Show Me Love" sound
    1. DEEP HOUSE - More sub, less percussion
    2. GARAGE - Aggressive attack, chorus
    3. TRANCE - Long release, bright
    4. MINIMAL - Pure hollow tone
    5. TECHNO - Hard, dirty
    6. LOFI - Detuned, gritty
    7. EPIC - Maximum everything!
    
    BRONNEN:
    - Korg M1 service manual
    - FM synthesis theory (Chowning)
    - M1 "Organ 2" spectral analysis
    - Robin S - Show Me Love (1992)
*/

#include "unit_osc.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "macros.h"
#include <math.h>

#define MAX_VOICES 4
#define CHORUS_BUFFER_SIZE 2048  // Reduced from 4096

static const unit_runtime_osc_context_t *s_context;

struct Voice {
    // FM oscillators
    float carrier_phase;
    float modulator_phase;
    
    // Sub oscillator
    float sub_phase;
    
    // Envelopes
    float mod_env;           // Modulation index envelope
    float amp_env;           // Amplitude envelope
    uint8_t mod_env_stage;   // 0=Attack, 1=Decay, 2=Sustain, 3=Release, 4=Off
    uint8_t amp_env_stage;
    uint32_t env_counter;
    
    // Voice info
    uint8_t note;
    uint8_t velocity;
    bool active;
    
    // Per-voice detune (for chorus)
    float detune_offset;
};

static Voice s_voices[MAX_VOICES];

// Chorus buffer
static float s_chorus_buffer_l[CHORUS_BUFFER_SIZE];
static float s_chorus_buffer_r[CHORUS_BUFFER_SIZE];
static uint32_t s_chorus_write;
static float s_chorus_lfo_phase;

// Parameters
static float s_hollowness;        // Base FM modulation index
static float s_percussion;        // Percussion envelope amount
static float s_octave_sub;        // Sub oscillator level
static float s_chorus_depth;      // Chorus amount
static float s_release_time;      // Release time
static float s_dirt_amount;       // Saturation/overdrive
static float s_fm_ratio;          // FM ratio (normally 2:1)
static float s_attack_time;       // Attack time
static uint8_t s_preset;
static uint8_t s_voice_count;

static uint32_t s_sample_counter;

// Presets
struct Organ2Preset {
    float hollowness;
    float percussion;
    float sub_level;
    float chorus;
    float release;
    float dirt;
    float fm_ratio;
    float attack;
    const char* name;
};

static const Organ2Preset s_presets[8] = {
    {0.50f, 0.75f, 0.60f, 0.30f, 0.40f, 0.25f, 0.50f, 0.20f, "ROBINS"},   // Robin S classic
    {0.40f, 0.60f, 0.80f, 0.20f, 0.60f, 0.15f, 0.45f, 0.15f, "DEEP"},     // Deep house
    {0.60f, 0.85f, 0.50f, 0.50f, 0.35f, 0.40f, 0.55f, 0.25f, "GARAGE"},  // UK Garage
    {0.55f, 0.70f, 0.40f, 0.40f, 0.80f, 0.20f, 0.52f, 0.18f, "TRANCE"},  // Trance organ
    {0.30f, 0.40f, 0.30f, 0.10f, 0.50f, 0.05f, 0.48f, 0.10f, "MINIMAL"}, // Minimal
    {0.70f, 0.90f, 0.70f, 0.25f, 0.30f, 0.60f, 0.58f, 0.30f, "TECHNO"},  // Techno
    {0.45f, 0.65f, 0.65f, 0.70f, 0.45f, 0.50f, 0.46f, 0.22f, "LOFI"},    // Lo-fi
    {0.80f, 0.95f, 0.90f, 0.60f, 0.70f, 0.45f, 0.60f, 0.35f, "EPIC"}     // Epic
};

inline float fast_tanh(float x) {
    if (x < -3.f) return -1.f;
    if (x > 3.f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

// 2-Operator FM synthesis
inline float fm_operator(float carrier_phase, float modulator_phase, 
                         float mod_index, float fm_ratio) {
    // Modulator frequency (normally 2x carrier for 2nd harmonic)
    float mod_freq_mult = 1.5f + fm_ratio;  // 1.5-2.5x range
    
    // FM synthesis formula - osc_sinf expects [0,1] phase
    float mod_phase_norm = modulator_phase * mod_freq_mult;
    mod_phase_norm -= (uint32_t)mod_phase_norm;
    if (mod_phase_norm < 0.f) mod_phase_norm += 1.f;
    
    float modulator = osc_sinf(mod_phase_norm);
    
    float carrier_phase_mod = carrier_phase + mod_index * modulator * 0.5f;
    carrier_phase_mod -= (uint32_t)carrier_phase_mod;
    if (carrier_phase_mod < 0.f) carrier_phase_mod += 1.f;
    
    float carrier = osc_sinf(carrier_phase_mod);
    
    return carrier;
}

// Modulation envelope (percussive)
inline float process_mod_envelope(Voice *v, float attack_time, float decay_time, 
                                  float sustain_level, float release_time) {
    float env = 0.f;
    float t_sec = (float)v->env_counter / 48000.f;
    
    switch (v->mod_env_stage) {
        case 0: { // Attack (very fast!)
            float attack = 0.001f + attack_time * 0.004f;  // 1-5ms
            if (t_sec < attack) {
                env = t_sec / attack;
                env = env * env;  // Power curve for snap
            } else {
                env = 1.f;
                v->mod_env_stage = 1;
                v->env_counter = 0;
            }
            break;
        }
        case 1: { // Decay (controls percussion)
            float decay = 0.05f + decay_time * 0.45f;  // 50-500ms
            if (t_sec < decay) {
                float t = t_sec / decay;
                env = 1.f - t * (1.f - sustain_level);
                env = 1.f - (1.f - env) * (1.f - env);  // Exponential decay
            } else {
                env = sustain_level;
                v->mod_env_stage = 2;
            }
            break;
        }
        case 2: // Sustain
            env = sustain_level;
            break;
        case 3: { // Release
            float release = 0.05f + release_time * 1.95f;  // 50-2000ms
            if (t_sec < release) {
                float t = t_sec / release;
                env = sustain_level * (1.f - t);
            } else {
                env = 0.f;
                v->mod_env_stage = 4;
            }
            break;
        }
        case 4: // Off
            env = 0.f;
            v->active = false;
            break;
    }
    
    v->mod_env = env;
    return env;
}

// Amplitude envelope
inline float process_amp_envelope(Voice *v, float attack_time, float release_time) {
    float env = 0.f;
    float t_sec = (float)v->env_counter / 48000.f;
    
    switch (v->amp_env_stage) {
        case 0: { // Attack
            float attack = 0.001f + attack_time * 0.009f;  // 1-10ms
            if (t_sec < attack) {
                env = t_sec / attack;
            } else {
                env = 1.f;
                v->amp_env_stage = 1;
            }
            break;
        }
        case 1: // Sustain
            env = 1.f;
            break;
        case 2: { // Release
            float release = 0.05f + release_time * 1.95f;
            if (t_sec < release) {
                float t = t_sec / release;
                env = 1.f - t;
            } else {
                env = 0.f;
                v->amp_env_stage = 3;
                v->active = false;
            }
            break;
        }
        case 3: // Off
            env = 0.f;
            v->active = false;
            break;
    }
    
    v->amp_env = env;
    return env;
}

// Sub oscillator (square wave)
inline float sub_oscillator(float phase) {
    return (phase < 0.5f) ? 1.f : -1.f;
}

// Chorus effect
inline float chorus_process(float x, int channel) {
    float *buffer = (channel == 0) ? s_chorus_buffer_l : s_chorus_buffer_r;
    
    buffer[s_chorus_write] = x;
    
    s_chorus_lfo_phase += 0.5f / 48000.f;
    if (s_chorus_lfo_phase >= 1.f) s_chorus_lfo_phase -= 1.f;
    
    float lfo = osc_sinf(s_chorus_lfo_phase);
    float delay_samps = 400.f + lfo * 200.f * s_chorus_depth + (float)channel * 60.f;
    
    uint32_t delay_int = (uint32_t)delay_samps;
    uint32_t read_pos = (s_chorus_write + CHORUS_BUFFER_SIZE - delay_int) % CHORUS_BUFFER_SIZE;
    
    float chorus_mix = s_chorus_depth * 0.5f;
    return x * (1.f - chorus_mix) + buffer[read_pos] * chorus_mix;
}

// Saturation/overdrive
inline float apply_dirt(float x, float amount) {
    if (amount < 0.01f) return x;
    
    float drive = 1.f + amount * 4.f;
    return fast_tanh(x * drive) / drive;
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc)
{
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 1) return k_unit_err_geometry;

    s_context = static_cast<const unit_runtime_osc_context_t *>(desc->hooks.runtime_context);

    for (int v = 0; v < MAX_VOICES; v++) {
        Voice *voice = &s_voices[v];
        voice->carrier_phase = 0.f;
        voice->modulator_phase = 0.f;
        voice->sub_phase = 0.f;
        
        voice->mod_env = 0.f;
        voice->amp_env = 0.f;
        voice->mod_env_stage = 4;
        voice->amp_env_stage = 3;
        voice->env_counter = 0;
        
        voice->active = false;
        
        // Random detune offset per voice for chorus
        voice->detune_offset = ((float)v / (float)MAX_VOICES - 0.5f) * 0.02f;
    }
    
    for (int i = 0; i < CHORUS_BUFFER_SIZE; i++) {
        s_chorus_buffer_l[i] = 0.f;
        s_chorus_buffer_r[i] = 0.f;
    }
    s_chorus_write = 0;
    s_chorus_lfo_phase = 0.f;
    
    s_hollowness = 0.5f;
    s_percussion = 0.75f;
    s_octave_sub = 0.6f;
    s_chorus_depth = 0.3f;
    s_release_time = 0.4f;
    s_dirt_amount = 0.25f;
    s_fm_ratio = 0.5f;
    s_attack_time = 0.2f;
    s_preset = 0;
    s_voice_count = 3;
    
    s_sample_counter = 0;

    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset()
{
    for (int v = 0; v < MAX_VOICES; v++) {
        s_voices[v].carrier_phase = 0.f;
        s_voices[v].modulator_phase = 0.f;
        s_voices[v].sub_phase = 0.f;
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
        int active_count = 0;
        
        for (int v = 0; v <= s_voice_count && v < MAX_VOICES; v++) {
            Voice *voice = &s_voices[v];
            if (!voice->active) continue;
            
            // Process envelopes
            float mod_env = process_mod_envelope(voice, s_attack_time, s_percussion, 
                                                 0.1f + s_hollowness * 0.3f, s_release_time);
            float amp_env = process_amp_envelope(voice, s_attack_time, s_release_time);
            
            voice->env_counter++;
            
            if (amp_env < 0.001f && voice->amp_env_stage >= 2) {
                voice->active = false;
                continue;
            }
            
            // Calculate pitch with detune
            float w0 = osc_w0f_for_note(voice->note, mod);
            w0 *= (1.f + voice->detune_offset * s_chorus_depth);
            
            // FM modulation index (controlled by envelope + hollowness)
            float base_mod_index = s_hollowness * 3.f;  // 0-3 range
            float percussion_mod = mod_env * s_percussion * 4.f;  // Percussion boost
            float mod_index = base_mod_index + percussion_mod;
            
            // 2-OPERATOR FM SYNTHESIS
            float fm_out = fm_operator(voice->carrier_phase, voice->modulator_phase, 
                                       mod_index, s_fm_ratio);
            
            // SUB OSCILLATOR (square wave -1 octave)
            float sub_w0 = w0 * 0.5f;
            float sub_out = sub_oscillator(voice->sub_phase) * s_octave_sub * 0.5f;
            
            // MIX FM + SUB
            float mixed = fm_out + sub_out;
            
            // Velocity sensitivity
            float vel_scale = (float)voice->velocity / 127.f;
            vel_scale = 0.5f + vel_scale * 0.5f;
            mixed *= vel_scale;
            
            // Apply amplitude envelope
            mixed *= amp_env;
            
            // Stereo spread (slight pan per voice)
            float pan = (float)v / (float)MAX_VOICES;
            sig_l += mixed * (1.f - pan * 0.3f);
            sig_r += mixed * (0.7f + pan * 0.3f);
            
            // Update phases
            voice->carrier_phase += w0;
            voice->carrier_phase -= (uint32_t)voice->carrier_phase;
            
            voice->modulator_phase += w0;
            voice->modulator_phase -= (uint32_t)voice->modulator_phase;
            
            voice->sub_phase += sub_w0;
            voice->sub_phase -= (uint32_t)voice->sub_phase;
            
            active_count++;
        }
        
        if (active_count > 0) {
            sig_l /= (float)active_count;
            sig_r /= (float)active_count;
        }
        
        // Mono mix
        float mono = (sig_l + sig_r) * 0.5f;
        
        // Chorus
        mono = chorus_process(mono, 0);
        
        // Dirt/saturation
        mono = apply_dirt(mono, s_dirt_amount);
        
        out[f] = clipminmaxf(-1.f, mono * 2.2f, 1.f);  // Volume boost!
        
        s_chorus_write = (s_chorus_write + 1) % CHORUS_BUFFER_SIZE;
        s_sample_counter++;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_hollowness = valf; break;
        case 1: s_percussion = valf; break;
        case 2: s_octave_sub = valf; break;
        case 3: s_chorus_depth = valf; break;
        case 4: s_release_time = valf; break;
        case 5: s_dirt_amount = valf; break;
        case 6: s_fm_ratio = valf; break;
        case 7: s_attack_time = valf; break;
        case 8:
            s_preset = value;
            // Load preset
            s_hollowness = s_presets[value].hollowness;
            s_percussion = s_presets[value].percussion;
            s_octave_sub = s_presets[value].sub_level;
            s_chorus_depth = s_presets[value].chorus;
            s_release_time = s_presets[value].release;
            s_dirt_amount = s_presets[value].dirt;
            s_fm_ratio = s_presets[value].fm_ratio;
            s_attack_time = s_presets[value].attack;
            break;
        case 9: s_voice_count = value; break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0: return (int32_t)(s_hollowness * 1023.f);
        case 1: return (int32_t)(s_percussion * 1023.f);
        case 2: return (int32_t)(s_octave_sub * 1023.f);
        case 3: return (int32_t)(s_chorus_depth * 1023.f);
        case 4: return (int32_t)(s_release_time * 1023.f);
        case 5: return (int32_t)(s_dirt_amount * 1023.f);
        case 6: return (int32_t)(s_fm_ratio * 1023.f);
        case 7: return (int32_t)(s_attack_time * 1023.f);
        case 8: return s_preset;
        case 9: return s_voice_count;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
    if (id == 8) {
        static const char *preset_names[] = {
            "ROBINS", "DEEP", "GARAGE", "TRANCE",
            "MINIMAL", "TECHNO", "LOFI", "EPIC"
        };
        return preset_names[value];
    }
    if (id == 9) {
        static const char *voice_names[] = {"1V", "2V", "3V", "4V"};
        return voice_names[value];
    }
    return "";
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velo)
{
    int free_voice = -1;
    
    // Find free voice
    for (int v = 0; v <= s_voice_count && v < MAX_VOICES; v++) {
        if (!s_voices[v].active) {
            free_voice = v;
            break;
        }
    }
    
    // Voice stealing if no free voice
    if (free_voice == -1) {
        free_voice = 0;
    }
    
    Voice *voice = &s_voices[free_voice];
    voice->note = note;
    voice->velocity = velo;
    voice->active = true;
    
    // Reset phases (important for attack!)
    voice->carrier_phase = 0.f;
    voice->modulator_phase = 0.f;
    voice->sub_phase = 0.f;
    
    // Trigger envelopes
    voice->mod_env_stage = 0;
    voice->amp_env_stage = 0;
    voice->env_counter = 0;
}

__unit_callback void unit_note_off(uint8_t note)
{
    for (int v = 0; v < MAX_VOICES; v++) {
        if (s_voices[v].note == note && s_voices[v].active) {
            if (s_voices[v].mod_env_stage < 3) {
                s_voices[v].mod_env_stage = 3;
                s_voices[v].env_counter = 0;
            }
            if (s_voices[v].amp_env_stage < 2) {
                s_voices[v].amp_env_stage = 2;
                s_voices[v].env_counter = 0;
            }
        }
    }
}

__unit_callback void unit_all_note_off()
{
    for (int v = 0; v < MAX_VOICES; v++) {
        s_voices[v].active = false;
        s_voices[v].mod_env_stage = 4;
        s_voices[v].amp_env_stage = 3;
    }
}

__unit_callback void unit_set_tempo(uint32_t tempo) {}
__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {}

__unit_callback void unit_pitch_bend(uint16_t bend) {}

__unit_callback void unit_channel_pressure(uint8_t press) {}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t press) {}

