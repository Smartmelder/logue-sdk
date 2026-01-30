/*
    90s RAVE MULTI-ENGINE - Complete Implementation
    
    ═══════════════════════════════════════════════════════════════
    HISTORICAL CONTEXT
    ═══════════════════════════════════════════════════════════════
    
    Early 90s Eurodance/Rave sound was defined by:
    
    1. THE HOOVER (Roland Alpha Juno)
       - Used in: 2 Unlimited, Human Resource, L.A. Style
       - PWM sawtooth with fast pitch LFO
       - "Vacuum cleaner" sound
    
    2. FM DONK (Yamaha TX81Z)
       - Used in: Quadrophonia, Praga Khan, Cubic 22
       - 2-operator FM with fast decay
       - Metallic, percussive bass
    
    3. RAVE SAW (Supersaw technique)
       - Used in: Stabs, leads, pads
       - Multiple detuned saws
       - Thick, aggressive sound
    
    4. M1 ORGAN (Korg M1 "Organ 2")
       - Used in: Robin S, Crystal Waters, Ce Ce Peniston
       - Mixed waveforms with attack click
       - House music signature sound
    
*/

#include "unit_osc.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "osc_api.h"

// SDK compatibility - PI is already defined in CMSIS arm_math.h
// const float PI = 3.14159265359f; // Removed - conflicts with CMSIS

inline float mod1(float x) {
    while (x >= 1.f) x -= 1.f;
    while (x < 0.f) x += 1.f;
    return x;
}

#define MAX_VOICES 3
#define RAVE_SAW_COUNT 3

static const unit_runtime_osc_context_t *s_context;

// Rave saw detune values (cents)
static const float s_rave_saw_detune[RAVE_SAW_COUNT] = {
    0.0f,    // Center
    -15.0f,  // Left
    15.0f    // Right
};

// Preset definitions
struct RavePreset {
    uint8_t engine;      // 0=Hoover, 1=FM, 2=Saw, 3=Organ
    float timbre;
    float decay;
    float attack;
    float brightness;
    float punch;
    float detune;
    const char* name;
};

static const RavePreset s_presets[8] = {
    // UNLIMITED - 2 Unlimited hoover
    {0, 0.75f, 0.3f, 0.01f, 0.7f, 0.8f, 0.6f, "UNLIMIT"},
    
    // QUADRO - Quadrophonia FM donk
    {1, 0.85f, 0.15f, 0.005f, 0.8f, 0.9f, 0.2f, "QUADRO"},
    
    // PRAGA - Praga Khan rave saw
    {2, 0.6f, 0.4f, 0.02f, 0.75f, 0.6f, 0.8f, "PRAGA"},
    
    // ROBIN - Robin S organ
    {3, 0.5f, 0.35f, 0.015f, 0.65f, 0.85f, 0.3f, "ROBIN"},
    
    // HUMAN - Human Resource hoover lead
    {0, 0.8f, 0.25f, 0.008f, 0.8f, 0.75f, 0.7f, "HUMAN"},
    
    // SNAP - Snap! bass
    {1, 0.9f, 0.18f, 0.003f, 0.7f, 0.95f, 0.15f, "SNAP"},
    
    // URBAN - Urban Cookie stab
    {2, 0.7f, 0.3f, 0.025f, 0.8f, 0.7f, 0.75f, "URBAN"},
    
    // CUSTOM
    {0, 0.6f, 0.3f, 0.02f, 0.7f, 0.7f, 0.5f, "CUSTOM"}
};

struct Voice {
    bool active;
    uint8_t note;
    uint8_t velocity;
    
    // Oscillator phases
    float phase_main;
    float phase_left;
    float phase_right;
    
    // PWM phases (for hoover)
    float pwm_phase;
    float pwm_lfo;
    
    // FM phases
    float fm_carrier_phase;
    float fm_mod_phase;
    
    // Envelopes
    float amp_env;
    uint8_t amp_stage;  // 0=attack, 1=decay, 2=sustain, 3=release
    uint32_t amp_counter;
    
    float pitch_env;
    uint32_t pitch_env_counter;
    
    // Filter
    float filter_z1, filter_z2;
    float filter_cutoff;
};

static Voice s_voices[MAX_VOICES];

// Parameters
static float s_timbre;
static float s_decay_time;
static float s_attack_time;
static float s_release_time;
static float s_brightness;
static float s_punch;
static float s_detune_amount;
static float s_drive;
static uint8_t s_engine;
static uint8_t s_preset;

static uint32_t s_sample_counter;

// Fast math
inline float fast_tanh(float x) {
    if (x < -3.f) return -1.f;
    if (x > 3.f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

// PolyBLEP antialiasing
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

// Generate pulse wave with PWM
inline float pulse_wave(float phase, float pw, float dt) {
    float pulse = (phase < pw) ? 1.f : -1.f;
    pulse += poly_blep(phase, dt);
    float phase2 = phase + 1.f - pw;
    phase2 -= (uint32_t)phase2;
    if (phase2 < 0.f) phase2 += 1.f;
    if (phase2 >= 1.f) phase2 -= 1.f;
    pulse -= poly_blep(phase2, dt);
    return pulse;
}

// Generate sawtooth
inline float saw_wave(float phase, float dt) {
    float saw = 2.f * phase - 1.f;
    saw -= poly_blep(phase, dt);
    return saw;
}

// State-variable filter (lowpass) - SAFE VERSION
inline float process_filter(Voice *v, float input, float cutoff, float resonance) {
    cutoff = clipminmaxf(20.f, cutoff, 20000.f);
    
    float w = 2.f * PI * cutoff / 48000.f;
    w = clipminmaxf(0.01f, w, 1.5f);
    
    // Convert to phase [0,1] for SDK functions
    float phase_w = w / (2.f * PI);
    if (phase_w >= 1.f) phase_w -= 1.f;
    if (phase_w < 0.f) phase_w += 1.f;
    
    float phase_sin = phase_w * 0.5f;
    if (phase_sin >= 1.f) phase_sin -= 1.f;
    if (phase_sin < 0.f) phase_sin += 1.f;
    float f = 2.f * osc_sinf(phase_sin);
    f = clipminmaxf(0.f, f, 1.9f);
    
    float q = 1.f / (0.5f + resonance * 4.f);
    q = clipminmaxf(0.1f, q, 10.f);
    
    v->filter_z1 = v->filter_z1 + f * v->filter_z2;
    v->filter_z1 = clipminmaxf(-3.f, v->filter_z1, 3.f);
    
    float hp = input - v->filter_z1 - q * v->filter_z2;
    hp = clipminmaxf(-3.f, hp, 3.f);
    
    v->filter_z2 = v->filter_z2 + f * hp;
    v->filter_z2 = clipminmaxf(-3.f, v->filter_z2, 3.f);
    
    return v->filter_z1;  // Lowpass output
}

// ENGINE 1: HOOVER
inline float generate_hoover(Voice *v, float w0) {
    // Limit w0 to prevent aliasing
    if (w0 > 0.48f) w0 = 0.48f;
    
    // Pitch LFO for hoover scream
    float lfo_rate = 10.f + s_timbre * 40.f;  // 10-50 Hz
    v->pwm_lfo += lfo_rate / 48000.f;
    if (v->pwm_lfo >= 1.f) v->pwm_lfo -= 1.f;
    if (v->pwm_lfo < 0.f) v->pwm_lfo += 1.f;
    
    float lfo = osc_sinf(v->pwm_lfo);
    
    // Pitch envelope (rises then falls)
    float t_sec = (float)v->pitch_env_counter / 48000.f;
    float pitch_env_time = 0.05f + s_decay_time * 0.45f;
    
    // Sine envelope (0 to 1)
    float env_phase = t_sec / pitch_env_time;
    if (env_phase > 1.f) env_phase = 1.f;
    float env_phase_sin = env_phase * 0.5f;
    if (env_phase_sin >= 1.f) env_phase_sin -= 1.f;
    if (env_phase_sin < 0.f) env_phase_sin += 1.f;
    v->pitch_env = osc_sinf(env_phase_sin);  // Half cycle = 0 to 1
    v->pitch_env_counter++;
    
    // Apply pitch modulation
    float pitch_mod = lfo * v->pitch_env * s_timbre * 0.5f;  // ±50 cents max
    float w0_mod = w0 * fastpow2f(pitch_mod / 12.f);  // Fixed: fastpow2f for oscillator
    if (w0_mod > 0.48f) w0_mod = 0.48f;
    
    // Dual pulse waves with different PWM
    float pw1 = 0.5f + lfo * 0.3f;
    float pw2 = 0.5f - lfo * 0.3f * s_detune_amount;
    
    float pulse1 = pulse_wave(v->phase_main, pw1, w0_mod);
    float pulse2 = pulse_wave(v->phase_left, pw2, w0_mod * 0.995f);  // Slight detune
    
    v->phase_main += w0_mod;
    v->phase_main -= (uint32_t)v->phase_main;
    if (v->phase_main < 0.f) v->phase_main = 0.f;
    if (v->phase_main >= 1.f) v->phase_main = 0.f;
    
    v->phase_left += w0_mod * 0.995f;
    v->phase_left -= (uint32_t)v->phase_left;
    if (v->phase_left < 0.f) v->phase_left = 0.f;
    if (v->phase_left >= 1.f) v->phase_left = 0.f;
    
    float mixed = (pulse1 + pulse2) * 0.5f;
    
    // Filter sweep
    float filter_env = v->amp_env;
    float cutoff = 300.f + filter_env * s_brightness * 18000.f;
    float resonance = 0.7f + s_brightness * 0.25f;
    
    return process_filter(v, mixed, cutoff, resonance);
}

// ENGINE 2: FM DONK
inline float generate_fm_donk(Voice *v, float w0) {
    // Limit w0 to prevent aliasing
    if (w0 > 0.48f) w0 = 0.48f;
    
    // FM parameters
    float fm_index = 8.f + s_timbre * 12.f;  // 8-20
    float mod_ratio = 1.f + (int)(s_brightness * 7.f);  // 1-8×
    
    // Decay envelope for FM index
    float t_sec = (float)v->amp_counter / 48000.f;
    float decay = 0.05f + s_decay_time * 0.45f;
    float env = fastpow2f(-t_sec / decay * 5.f);  // Fixed: fastpow2f
    
    // Modulator
    float mod_w0 = w0 * mod_ratio;
    if (mod_w0 > 0.48f) mod_w0 = 0.48f;
    
    float modulator = osc_sinf(v->fm_mod_phase);
    v->fm_mod_phase += mod_w0;
    v->fm_mod_phase -= (uint32_t)v->fm_mod_phase;
    if (v->fm_mod_phase < 0.f) v->fm_mod_phase += 1.f;
    if (v->fm_mod_phase >= 1.f) v->fm_mod_phase -= 1.f;
    
    // Carrier with FM
    float fm_amount = modulator * fm_index * env;
    float carrier_phase = v->fm_carrier_phase + fm_amount * 0.5f;
    carrier_phase -= (uint32_t)carrier_phase;
    if (carrier_phase < 0.f) carrier_phase += 1.f;
    if (carrier_phase >= 1.f) carrier_phase -= 1.f;
    float carrier = osc_sinf(carrier_phase);  // Scale FM amount
    
    v->fm_carrier_phase += w0;
    v->fm_carrier_phase -= (uint32_t)v->fm_carrier_phase;
    if (v->fm_carrier_phase < 0.f) v->fm_carrier_phase += 1.f;
    if (v->fm_carrier_phase >= 1.f) v->fm_carrier_phase -= 1.f;
    
    // Punch (attack click)
    float punch_env = (t_sec < 0.01f) ? (1.f - t_sec / 0.01f) : 0.f;
    float punch = punch_env * s_punch * 0.5f;
    
    return carrier + punch;
}

// ENGINE 3: RAVE SAW
inline float generate_rave_saw(Voice *v, float w0) {
    // Limit w0 to prevent aliasing
    if (w0 > 0.48f) w0 = 0.48f;
    
    float sum = 0.f;
    
    for (int i = 0; i < RAVE_SAW_COUNT; i++) {
        float detune_cents = s_rave_saw_detune[i] * s_detune_amount;
        float w0_det = w0 * fastpow2f(detune_cents / 1200.f);  // Fixed: fastpow2f
        if (w0_det > 0.48f) w0_det = 0.48f;
        
        float saw;
        if (i == 0) {
            saw = saw_wave(v->phase_main, w0_det);
            v->phase_main += w0_det;
            v->phase_main -= (uint32_t)v->phase_main;
            if (v->phase_main < 0.f) v->phase_main = 0.f;
            if (v->phase_main >= 1.f) v->phase_main = 0.f;
        } else if (i == 1) {
            saw = saw_wave(v->phase_left, w0_det);
            v->phase_left += w0_det;
            v->phase_left -= (uint32_t)v->phase_left;
            if (v->phase_left < 0.f) v->phase_left = 0.f;
            if (v->phase_left >= 1.f) v->phase_left = 0.f;
        } else {
            saw = saw_wave(v->phase_right, w0_det);
            v->phase_right += w0_det;
            v->phase_right -= (uint32_t)v->phase_right;
            if (v->phase_right < 0.f) v->phase_right = 0.f;
            if (v->phase_right >= 1.f) v->phase_right = 0.f;
        }
        
        sum += saw;
    }
    
    sum /= (float)RAVE_SAW_COUNT;
    
    // Filter
    float cutoff = 500.f + s_timbre * 15000.f;
    float resonance = 0.5f + s_brightness * 0.45f;
    
    return process_filter(v, sum, cutoff, resonance);
}

// ENGINE 4: HOUSE ORGAN
inline float generate_house_organ(Voice *v, float w0) {
    // Limit w0 to prevent aliasing
    if (w0 > 0.48f) w0 = 0.48f;
    
    // Sine wave (fundamental)
    float sine = osc_sinf(v->phase_main);
    v->phase_main += w0;
    v->phase_main -= (uint32_t)v->phase_main;
    if (v->phase_main < 0.f) v->phase_main = 0.f;
    if (v->phase_main >= 1.f) v->phase_main = 0.f;
    
    // Filtered square (harmonics)
    float square = (v->phase_left < 0.5f) ? 1.f : -1.f;
    v->phase_left += w0;
    v->phase_left -= (uint32_t)v->phase_left;
    if (v->phase_left < 0.f) v->phase_left = 0.f;
    if (v->phase_left >= 1.f) v->phase_left = 0.f;
    
    // Mix based on timbre
    float mixed = sine * (1.f - s_timbre) + square * s_timbre;
    
    // Attack click
    float t_sec = (float)v->amp_counter / 48000.f;
    float click_env = (t_sec < 0.005f) ? (1.f - t_sec / 0.005f) : 0.f;
    float click = click_env * s_punch;
    
    // Filter
    float cutoff = 2000.f + s_brightness * 8000.f;
    mixed = process_filter(v, mixed, cutoff, 0.3f);
    
    return mixed + click;
}

// Envelope
inline float update_envelope(Voice *v) {
    float t_sec = (float)v->amp_counter / 48000.f;
    
    const RavePreset *preset = &s_presets[s_preset];
    float attack = preset->attack * (0.5f + s_attack_time);
    float decay = preset->decay * (0.5f + s_decay_time * 1.5f);
    float release = 0.1f + s_release_time * 0.9f;
    
    switch (v->amp_stage) {
        case 0: // Attack
            v->amp_env = clipminmaxf(0.f, t_sec / attack, 1.f);
            if (v->amp_env >= 0.99f) {
                v->amp_stage = 1;
                v->amp_counter = 0;
            }
            break;
        
        case 1: // Decay
            v->amp_env = 0.7f + 0.3f * fastpow2f(-t_sec / decay * 5.f);  // Fixed: fastpow2f
            if (t_sec >= decay) {
                v->amp_stage = 2;
                v->amp_counter = 0;
            }
            break;
        
        case 2: // Sustain
            v->amp_env = 0.7f;
            break;
        
        case 3: // Release
            v->amp_env = 0.7f * fastpow2f(-t_sec / release * 5.f);  // Fixed: fastpow2f
            if (v->amp_env < 0.001f) {
                v->active = false;
            }
            break;
    }
    
    v->amp_counter++;
    return v->amp_env;
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
        s_voices[v].active = false;
        s_voices[v].phase_main = 0.f;
        s_voices[v].phase_left = 0.f;
        s_voices[v].phase_right = 0.f;
        s_voices[v].pwm_phase = 0.f;
        s_voices[v].pwm_lfo = 0.f;
        s_voices[v].fm_carrier_phase = 0.f;
        s_voices[v].fm_mod_phase = 0.f;
        s_voices[v].filter_z1 = s_voices[v].filter_z2 = 0.f;
    }
    
    s_timbre = 0.6f;
    s_decay_time = 0.3f;
    s_attack_time = 0.5f;
    s_release_time = 0.75f;
    s_brightness = 0.7f;
    s_punch = 0.7f;
    s_detune_amount = 0.5f;
    s_drive = 0.35f;
    s_engine = 0;
    s_preset = 0;
    
    s_sample_counter = 0;

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
        float sig = 0.f;
        int active_count = 0;
        
        for (int v = 0; v < MAX_VOICES; v++) {
            Voice *voice = &s_voices[v];
            if (!voice->active) continue;
            
            float w0 = osc_w0f_for_note(voice->note, mod);
            
            // Generate based on engine
            float sample = 0.f;
            switch (s_engine) {
                case 0: sample = generate_hoover(voice, w0); break;
                case 1: sample = generate_fm_donk(voice, w0); break;
                case 2: sample = generate_rave_saw(voice, w0); break;
                case 3: sample = generate_house_organ(voice, w0); break;
            }
            
            // Envelope
            float env = update_envelope(voice);
            
            // Velocity
            float vel = (float)voice->velocity / 127.f;
            vel = 0.5f + vel * 0.5f;
            
            sample *= env * vel;
            
            // Drive/distortion
            if (s_drive > 0.01f) {
                sample = fast_tanh(sample * (1.f + s_drive * 3.f));
            }
            
            sig += sample;
            active_count++;
        }
        
        if (active_count > 0) {
            sig /= (float)active_count;
        }
        
        // DC offset removal
        static float dc_z = 0.f;
        dc_z = dc_z * 0.995f + sig;
        sig = sig - dc_z;
        
        // OUTPUT - 90s LOUD! 🔥
        out[f] = clipminmaxf(-1.f, sig * 3.0f, 1.f);
        
        s_sample_counter++;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_timbre = valf; break;
        case 1: s_decay_time = valf; break;
        case 2: s_attack_time = valf; break;
        case 3: s_release_time = valf; break;
        case 4: s_brightness = valf; break;
        case 5: s_punch = valf; break;
        case 6: s_detune_amount = valf; break;
        case 7: s_drive = valf; break;
        case 8: 
            s_engine = value;
            // Load preset defaults for engine
            s_timbre = s_presets[s_preset].timbre;
            break;
        case 9: 
            s_preset = value;
            s_engine = s_presets[value].engine;
            s_timbre = s_presets[value].timbre;
            s_decay_time = s_presets[value].decay;
            s_brightness = s_presets[value].brightness;
            s_punch = s_presets[value].punch;
            s_detune_amount = s_presets[value].detune;
            break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0: return (int32_t)(s_timbre * 1023.f);
        case 1: return (int32_t)(s_decay_time * 1023.f);
        case 2: return (int32_t)(s_attack_time * 1023.f);
        case 3: return (int32_t)(s_release_time * 1023.f);
        case 4: return (int32_t)(s_brightness * 1023.f);
        case 5: return (int32_t)(s_punch * 1023.f);
        case 6: return (int32_t)(s_detune_amount * 1023.f);
        case 7: return (int32_t)(s_drive * 1023.f);
        case 8: return s_engine;
        case 9: return s_preset;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
    if (id == 8) {
        static const char *engine_names[] = {"HOOVER", "FM DONK", "RAVE SAW", "ORGAN"};
        if (value >= 0 && value < 4) return engine_names[value];
    }
    if (id == 9) {
        if (value >= 0 && value < 8) return s_presets[value].name;
    }
    return "";
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velo)
{
    int free_voice = -1;
    for (int v = 0; v < MAX_VOICES; v++) {
        if (!s_voices[v].active) {
            free_voice = v;
            break;
        }
    }
    
    if (free_voice == -1) free_voice = 0;
    
    Voice *voice = &s_voices[free_voice];
    voice->active = true;
    voice->note = note;
    voice->velocity = velo;
    
    voice->amp_counter = 0;
    voice->amp_stage = 0;
    voice->amp_env = 0.f;
    
    voice->pitch_env_counter = 0;
    voice->pitch_env = 0.f;
    
    voice->phase_main = 0.f;
    voice->phase_left = 0.f;
    voice->phase_right = 0.f;
    voice->pwm_phase = 0.f;
    voice->pwm_lfo = 0.f;
    voice->fm_carrier_phase = 0.f;
    voice->fm_mod_phase = 0.f;
    
    voice->filter_z1 = voice->filter_z2 = 0.f;
}

__unit_callback void unit_note_off(uint8_t note)
{
    for (int v = 0; v < MAX_VOICES; v++) {
        if (s_voices[v].active && s_voices[v].note == note) {
            s_voices[v].amp_stage = 3;
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

