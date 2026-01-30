/*
    ROLAND TR-909 DRUM SYNTHESIZER ENGINE
    
    ARCHITECTURE:
    
    === 909 KICK ===
    
    OSCILLATOR:
    - Sine wave with pitch envelope
    - Starts at ~150Hz, drops to ~40Hz
    - 2-stage pitch envelope (fast + slow)
    
    TONE CONTROL:
    - Lowpass filter (sweeps with pitch)
    - Optional harmonic distortion
    - Click layer (noise burst)
    
    ENVELOPE:
    - Attack: <1ms (instant punch)
    - Decay: 50-800ms (adjustable boom)
    - Velocity sensitivity
    
    === 909 SNARE ===
    
    DUAL LAYER:
    1. TONE LAYER (Body):
       - 2 triangle waves (180Hz + 330Hz)
       - Short decay (100-300ms)
       - Lowpass filtered
    
    2. NOISE LAYER (Snap):
       - High-passed white noise
       - Band-pass filter (1-8kHz)
       - Very short decay (50-150ms)
    
    MIX:
    - 50/50 tone/noise (adjustable)
    - Envelope with snap control
    
    === 909 OPEN HI-HAT ===
    
    METALLIC SYNTHESIS:
    - 6 square wave oscillators (prime ratios)
    - Frequencies: 296, 387, 561, 742, 923, 1107 Hz
    - Band-pass filter (6-12kHz)
    - High-pass filter (>8kHz for sizzle)
    
    ENVELOPE:
    - Fast attack (<1ms)
    - Long decay (200-2000ms for "open")
    - Exponential curve
    
    === ADDITIONAL FEATURES ===
    
    - Pitch modulation (for kick/snare tuning)
    - Distortion/saturation
    - Velocity layers (127 steps)
    - Note pitch affects tuning
    - 12 drum sounds (selectable)
    - 8 preset kits
    
    DRUM SOUNDS:
    0.  KICK 1 - Deep 909 kick
    1.  KICK 2 - Punchy techno kick
    2.  KICK 3 - Hard kick
    3.  SNARE 1 - Classic 909 snare
    4.  SNARE 2 - Tight snare
    5.  SNARE 3 - Fat snare
    6.  OPEN HAT 1 - Long decay
    7.  OPEN HAT 2 - Medium
    8.  OPEN HAT 3 - Short
    9.  CLOSED HAT - Quick chick
    10. CLAP - Hand clap
    11. RIM - Rimshot
    
    PRESETS:
    0. CLASSIC 909 - Original sound
    1. TECHNO - Hard & punchy
    2. HOUSE - Deep & warm
    3. TRANCE - Bright & long
    4. HARDCORE - Distorted
    5. MINIMAL - Clean & tight
    6. ACID - Aggressive
    7. CUSTOM - User settings
    
    BRONNEN:
    - Roland TR-909 service manual
    - TR-909 circuit analysis
    - Drum synthesis theory
    - Classic techno/house recordings
*/

#include "unit_osc.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "macros.h"
#include <math.h>

#define MAX_VOICES 1  // Drums are monophonic per sound
#define NOISE_BUFFER_SIZE 1024  // Reduced from 2048

static const unit_runtime_osc_context_t *s_context;

// Noise buffer
static float s_noise_buffer[NOISE_BUFFER_SIZE];
static uint32_t s_noise_seed;

// Hi-hat square wave frequencies (prime ratios for metallic character)
static const float s_hihat_freqs[6] = {
    296.f, 387.f, 561.f, 742.f, 923.f, 1107.f
};

struct DrumVoice {
    // Common
    float phase;
    float env_level;
    uint32_t env_counter;
    uint8_t env_stage;
    bool active;
    float pitch_offset;  // Pitch transposition based on note (semitones)
    
    // Kick-specific
    float kick_pitch_env;
    float kick_click_env;
    
    // Snare-specific
    float snare_tone_phase_1;
    float snare_tone_phase_2;
    float snare_noise_env;
    
    // Hi-hat specific
    float hihat_phases[6];
    
    // Filters
    float lpf_z1, lpf_z2;
    float hpf_z1, hpf_z2;
    float bpf_z1, bpf_z2;
    
    // Voice info
    uint8_t velocity;
    uint8_t current_sound;
};

static DrumVoice s_voice;

// Parameters
static float s_attack_time;
static float s_decay_time;
static float s_tone_control;
static float s_punch_amount;
static float s_snap_amount;
static float s_metallic_amount;
static float s_noise_level;
static float s_distortion;
static uint8_t s_sound_select;
static uint8_t s_preset_select;

static uint32_t s_sample_counter;

// Presets
struct TR909Preset {
    float attack;
    float decay;
    float tone;
    float punch;
    float snap;
    float metallic;
    float noise;
    float dist;
    const char* name;
};

static const TR909Preset s_presets[8] = {
    {0.60f, 0.50f, 0.80f, 0.75f, 0.30f, 0.40f, 0.25f, 0.65f, "CLASSIC"},
    {0.75f, 0.40f, 0.85f, 0.90f, 0.50f, 0.60f, 0.35f, 0.80f, "TECHNO"},
    {0.50f, 0.60f, 0.70f, 0.65f, 0.25f, 0.35f, 0.20f, 0.50f, "HOUSE"},
    {0.70f, 0.70f, 0.90f, 0.80f, 0.40f, 0.70f, 0.40f, 0.60f, "TRANCE"},
    {0.85f, 0.35f, 0.75f, 0.95f, 0.60f, 0.50f, 0.45f, 0.95f, "HARDCORE"},
    {0.55f, 0.45f, 0.65f, 0.60f, 0.20f, 0.30f, 0.15f, 0.40f, "MINIMAL"},
    {0.80f, 0.55f, 0.95f, 0.85f, 0.55f, 0.80f, 0.50f, 0.85f, "ACID"},
    {0.60f, 0.50f, 0.80f, 0.75f, 0.30f, 0.40f, 0.25f, 0.65f, "CUSTOM"}
};

void init_noise_buffer() {
    s_noise_seed = 0x87654321;
    for (int i = 0; i < NOISE_BUFFER_SIZE; i++) {
        s_noise_seed = s_noise_seed * 1103515245u + 12345u;
        float white = ((float)(s_noise_seed >> 16) / 32768.f) - 1.f;
        s_noise_buffer[i] = white;
    }
}

inline float read_noise() {
    s_noise_seed = s_noise_seed * 1103515245u + 12345u;
    uint32_t idx = (s_noise_seed >> 16) % NOISE_BUFFER_SIZE;
    return s_noise_buffer[idx];
}

inline float fast_tanh(float x) {
    if (x < -3.f) return -1.f;
    if (x > 3.f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

// 2-pole lowpass filter
inline float process_lpf(DrumVoice *v, float input, float cutoff, float q) {
    float w = 2.f * M_PI * cutoff / 48000.f;
    if (w > M_PI * 0.49f) w = M_PI * 0.49f;
    
    float g = fasttanfullf(w * 0.5f);
    float k = 1.f / q;
    
    float a1 = 1.f / (1.f + g * (g + k));
    float a2 = g * a1;
    float a3 = g * a2;
    
    float v0 = input;
    float v1 = v->lpf_z1;
    float v2 = v->lpf_z2;
    
    float v3 = v0 - v2;
    v1 = a1 * v->lpf_z1 + a2 * v3;
    v2 = v->lpf_z2 + a3 * v3;
    
    v->lpf_z1 = 2.f * v1 - v->lpf_z1;
    v->lpf_z2 = 2.f * v2 - v->lpf_z2;
    
    return v2;
}

// High-pass filter
inline float process_hpf(DrumVoice *v, float input, float cutoff) {
    float w = 2.f * M_PI * cutoff / 48000.f;
    float g = fasttanfullf(w * 0.5f);
    
    v->hpf_z1 = v->hpf_z1 + g * (input - v->hpf_z1);
    v->hpf_z2 = v->hpf_z2 + g * (v->hpf_z1 - v->hpf_z2);
    
    return input - v->hpf_z2;
}

// Band-pass filter
inline float process_bpf(DrumVoice *v, float input, float center, float q) {
    float w = 2.f * M_PI * center / 48000.f;
    float phase_norm = w / (2.f * M_PI);
    float bw = w / q;
    
    float r = 1.f - bw * 0.5f;
    float cos_phase = osc_cosf(phase_norm);
    float k = (1.f - 2.f * r * cos_phase + r * r) / (2.f - 2.f * cos_phase);
    
    float a0 = 1.f - k;
    float a1 = 2.f * (k - r) * cos_phase;
    float a2 = r * r - k;
    float b1 = 2.f * r * cos_phase;
    float b2 = -r * r;
    
    float output = a0 * input + a1 * v->bpf_z1 + a2 * v->bpf_z2 
                   - b1 * v->bpf_z1 - b2 * v->bpf_z2;
    
    v->bpf_z2 = v->bpf_z1;
    v->bpf_z1 = input;
    
    return output;
}

// === 909 KICK SYNTHESIS ===
inline float synthesize_909_kick(DrumVoice *v, float decay, float tone, float punch) {
    float t_sec = (float)v->env_counter / 48000.f;
    
    // PITCH ENVELOPE (2-stage: fast drop + slow tail)
    // Apply pitch offset from note (octave transposition)
    float pitch_mult = fastpow2f(v->pitch_offset / 12.f);
    float pitch_start = (150.f + tone * 100.f) * pitch_mult;  // 150-250 Hz
    float pitch_end = (35.f + tone * 15.f) * pitch_mult;      // 35-50 Hz
    
    // Fast initial drop
    float pitch_env_fast = fastpow2f(-t_sec * 40.f);
    // Slow tail
    float pitch_env_slow = fastpow2f(-t_sec * 8.f);
    
    v->kick_pitch_env = pitch_env_fast * 0.7f + pitch_env_slow * 0.3f;
    
    float current_pitch = pitch_end + (pitch_start - pitch_end) * v->kick_pitch_env;
    float w0 = current_pitch / 48000.f;  // Normalized frequency
    
    // SINE OSCILLATOR
    float sine = osc_sinf(v->phase);
    v->phase += w0;
    v->phase -= (uint32_t)v->phase;
    if (v->phase < 0.f) v->phase += 1.f;
    
    // AMPLITUDE ENVELOPE
    float decay_time = 0.05f + decay * 0.75f;  // 50-800ms
    float amp_env = fastpow2f(-t_sec / decay_time * 6.f);
    
    // CLICK LAYER (attack transient) - Boosted for D&B
    float click_decay = 0.005f;
    v->kick_click_env = (t_sec < click_decay) ? (1.f - t_sec / click_decay) : 0.f;
    float click = read_noise() * v->kick_click_env * punch * 0.6f;  // Increased from 0.3f
    
    // MIX - Boosted for D&B
    float mixed = sine * amp_env * 1.5f + click;  // Boost sine amplitude
    
    // TONE FILTER (follows pitch envelope)
    float filter_cutoff = current_pitch * (2.f + tone * 2.f);
    mixed = process_lpf(v, mixed, filter_cutoff, 0.7f);
    
    v->env_level = amp_env;
    
    return mixed;
}

// === 909 SNARE SYNTHESIS ===
inline float synthesize_909_snare(DrumVoice *v, float decay, float tone, float snap) {
    float t_sec = (float)v->env_counter / 48000.f;
    
    // TONE LAYER (2 triangle oscillators)
    // Apply pitch offset from note (octave transposition)
    float pitch_mult = fastpow2f(v->pitch_offset / 12.f);
    float freq1 = (180.f + tone * 100.f) * pitch_mult;
    float freq2 = (330.f + tone * 150.f) * pitch_mult;
    
    float w1 = freq1 / 48000.f;
    float w2 = freq2 / 48000.f;
    
    // Triangle waves
    float phase1_norm = v->snare_tone_phase_1;
    float phase2_norm = v->snare_tone_phase_2;
    
    float tri1 = 2.f * si_fabsf(2.f * (phase1_norm - si_floorf(phase1_norm + 0.5f))) - 1.f;
    float tri2 = 2.f * si_fabsf(2.f * (phase2_norm - si_floorf(phase2_norm + 0.5f))) - 1.f;
    
    v->snare_tone_phase_1 += w1;
    v->snare_tone_phase_2 += w2;
    v->snare_tone_phase_1 -= (uint32_t)v->snare_tone_phase_1;
    v->snare_tone_phase_2 -= (uint32_t)v->snare_tone_phase_2;
    if (v->snare_tone_phase_1 < 0.f) v->snare_tone_phase_1 += 1.f;
    if (v->snare_tone_phase_2 < 0.f) v->snare_tone_phase_2 += 1.f;
    
    float tone_layer = (tri1 + tri2) * 0.5f;
    
    // Tone envelope
    float tone_decay = 0.1f + decay * 0.2f;  // 100-300ms
    float tone_env = fastpow2f(-t_sec / tone_decay * 6.f);
    
    tone_layer *= tone_env;
    tone_layer = process_lpf(v, tone_layer, 3000.f + tone * 2000.f, 1.5f);
    
    // NOISE LAYER (snare rattle)
    float noise = read_noise();
    
    // Noise envelope (shorter than tone)
    float noise_decay = 0.05f + snap * 0.1f;  // 50-150ms
    v->snare_noise_env = fastpow2f(-t_sec / noise_decay * 8.f);
    
    noise *= v->snare_noise_env;
    
    // Band-pass filter (1-8kHz)
    float bp_center = 2000.f + snap * 4000.f;
    noise = process_bpf(v, noise, bp_center, 2.f);
    
    // High-pass for crispness
    noise = process_hpf(v, noise, 1000.f);
    
    // MIX tone + noise - Boosted for D&B
    float noise_mix = 0.4f + s_noise_level * 0.4f;
    float mixed = (tone_layer * (1.f - noise_mix) + noise * noise_mix) * 1.3f;  // Boost snare
    
    v->env_level = clipmaxf(tone_env, v->snare_noise_env);
    
    return mixed;
}

// === 909 OPEN HI-HAT SYNTHESIS ===
inline float synthesize_909_hihat(DrumVoice *v, float decay, float metallic, bool is_open) {
    float t_sec = (float)v->env_counter / 48000.f;
    
    // 6 square wave oscillators (metallic character)
    float mixed = 0.f;
    
    // Apply pitch offset from note (octave transposition)
    float pitch_mult = fastpow2f(v->pitch_offset / 12.f);
    
    for (int i = 0; i < 6; i++) {
        // Square wave
        float phase_norm = v->hihat_phases[i];
        float square = (phase_norm < 0.5f) ? 1.f : -1.f;
        
        // Frequency modulation for more metallicness
        float freq = s_hihat_freqs[i] * (1.f + metallic * 0.3f) * pitch_mult;
        float w = freq / 48000.f;
        
        v->hihat_phases[i] += w;
        v->hihat_phases[i] -= (uint32_t)v->hihat_phases[i];
        if (v->hihat_phases[i] < 0.f) v->hihat_phases[i] += 1.f;
        
        // Mix with decreasing amplitude
        float amp = 1.f / (float)(i + 1);
        mixed += square * amp;
    }
    
    mixed /= 6.f;
    mixed *= 1.4f;  // Boost hi-hat for D&B
    
    // ENVELOPE
    float decay_time;
    if (is_open) {
        decay_time = 0.2f + decay * 1.8f;  // 200-2000ms for open
    } else {
        decay_time = 0.05f + decay * 0.15f;  // 50-200ms for closed
    }
    
    float env = fastpow2f(-t_sec / decay_time * 6.f);
    
    mixed *= env;
    
    // BAND-PASS FILTER (6-12kHz for sizzle)
    float bp_center = 7000.f + metallic * 4000.f;
    mixed = process_bpf(v, mixed, bp_center, 1.5f);
    
    // HIGH-PASS for brightness
    mixed = process_hpf(v, mixed, 8000.f);
    
    v->env_level = env;
    
    return mixed;
}

// === CLAP SYNTHESIS ===
inline float synthesize_909_clap(DrumVoice *v, float decay) {
    float t_sec = (float)v->env_counter / 48000.f;
    
    // Multiple noise bursts (simulates hand clap reflections)
    float noise = read_noise();
    
    // 3-stage burst envelope
    float burst1 = (t_sec < 0.01f) ? 1.f : 0.f;
    float burst2 = (t_sec > 0.02f && t_sec < 0.03f) ? 0.7f : 0.f;
    float burst3 = (t_sec > 0.04f && t_sec < 0.05f) ? 0.5f : 0.f;
    
    float bursts = burst1 + burst2 + burst3;
    
    // Overall envelope
    float env = fastpow2f(-t_sec / (0.05f + decay * 0.15f) * 6.f);
    
    noise *= bursts * env;
    
    // Band-pass filter (1-4kHz)
    noise = process_bpf(v, noise, 2000.f, 2.f);
    
    v->env_level = env;
    
    return noise * 1.3f;  // Boost clap for D&B
}

// === RIM SHOT SYNTHESIS ===
inline float synthesize_909_rim(DrumVoice *v, float tone) {
    float t_sec = (float)v->env_counter / 48000.f;
    
    // Triangle wave @ 1kHz
    // Apply pitch offset from note (octave transposition)
    float pitch_mult = fastpow2f(v->pitch_offset / 12.f);
    float freq = (800.f + tone * 600.f) * pitch_mult;
    float w = freq / 48000.f;
    
    float phase_norm = v->phase;
    float tri = 2.f * si_fabsf(2.f * (phase_norm - si_floorf(phase_norm + 0.5f))) - 1.f;
    
    v->phase += w;
    v->phase -= (uint32_t)v->phase;
    if (v->phase < 0.f) v->phase += 1.f;
    
    // Very short decay
    float env = fastpow2f(-t_sec / 0.05f * 10.f);
    
    tri *= env;
    
    // Add click - Boosted for D&B
    float click = read_noise() * (t_sec < 0.003f ? 1.f : 0.f) * 0.5f;  // Increased from 0.3f
    
    v->env_level = env;
    
    return process_lpf(v, (tri + click) * 1.2f, 4000.f, 1.f);  // Boost rim for D&B
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc)
{
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 1) return k_unit_err_geometry;

    s_context = static_cast<const unit_runtime_osc_context_t *>(desc->hooks.runtime_context);

    init_noise_buffer();
    
    s_voice.phase = 0.f;
    s_voice.env_level = 0.f;
    s_voice.env_counter = 0;
    s_voice.env_stage = 0;
    s_voice.active = false;
    s_voice.pitch_offset = 0.f;  // Default: no transposition
    
    s_voice.kick_pitch_env = 0.f;
    s_voice.kick_click_env = 0.f;
    
    s_voice.snare_tone_phase_1 = 0.f;
    s_voice.snare_tone_phase_2 = 0.f;
    s_voice.snare_noise_env = 0.f;
    
    for (int i = 0; i < 6; i++) {
        s_voice.hihat_phases[i] = 0.f;
    }
    
    s_voice.lpf_z1 = s_voice.lpf_z2 = 0.f;
    s_voice.hpf_z1 = s_voice.hpf_z2 = 0.f;
    s_voice.bpf_z1 = s_voice.bpf_z2 = 0.f;
    
    s_voice.velocity = 100;
    s_voice.current_sound = 0;
    
    s_attack_time = 0.6f;
    s_decay_time = 0.5f;
    s_tone_control = 0.8f;
    s_punch_amount = 0.75f;
    s_snap_amount = 0.3f;
    s_metallic_amount = 0.4f;
    s_noise_level = 0.25f;
    s_distortion = 0.65f;
    s_sound_select = 0;
    s_preset_select = 0;
    
    s_sample_counter = 0;

    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset()
{
    s_voice.phase = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames)
{
    for (uint32_t f = 0; f < frames; f++) {
        float sig = 0.f;
        
        if (!s_voice.active) {
            out[f] = 0.f;
            continue;
        }
        
        // Route to appropriate synthesis engine
        switch (s_voice.current_sound) {
            case 0:  // KICK 1
            case 1:  // KICK 2
            case 2:  // KICK 3
                sig = synthesize_909_kick(&s_voice, s_decay_time, s_tone_control, s_punch_amount);
                break;
                
            case 3:  // SNARE 1
            case 4:  // SNARE 2
            case 5:  // SNARE 3
                sig = synthesize_909_snare(&s_voice, s_decay_time, s_tone_control, s_snap_amount);
                break;
                
            case 6:  // OPEN HAT 1
            case 7:  // OPEN HAT 2
            case 8:  // OPEN HAT 3
                sig = synthesize_909_hihat(&s_voice, s_decay_time, s_metallic_amount, true);
                break;
                
            case 9:  // CLOSED HAT
                sig = synthesize_909_hihat(&s_voice, s_decay_time * 0.3f, s_metallic_amount, false);
                break;
                
            case 10: // CLAP
                sig = synthesize_909_clap(&s_voice, s_decay_time);
                break;
                
            case 11: // RIM
                sig = synthesize_909_rim(&s_voice, s_tone_control);
                break;
        }
        
        // Velocity sensitivity
        float vel_scale = (float)s_voice.velocity / 127.f;
        vel_scale = 0.5f + vel_scale * 0.5f;
        sig *= vel_scale;
        
        // Distortion
        if (s_distortion > 0.01f) {
            float drive = 1.f + s_distortion * 3.f;
            sig = fast_tanh(sig * drive) / drive;
        }
        
        out[f] = clipminmaxf(-1.f, sig * 32.0f, 1.f);  // MAXIMUM VOLUME for D&B - bass must be audible!
        
        s_voice.env_counter++;
        
        // Check if voice should stop
        if (s_voice.env_level < 0.001f && s_voice.env_counter > 2400) {
            s_voice.active = false;
        }
        
        s_sample_counter++;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_attack_time = valf; break;
        case 1: s_decay_time = valf; break;
        case 2: s_tone_control = valf; break;
        case 3: s_punch_amount = valf; break;
        case 4: s_snap_amount = valf; break;
        case 5: s_metallic_amount = valf; break;
        case 6: s_noise_level = valf; break;
        case 7: s_distortion = valf; break;
        case 8: s_sound_select = value; break;
        case 9:
            s_preset_select = value;
            if (value < 8) {
                s_attack_time = s_presets[value].attack;
                s_decay_time = s_presets[value].decay;
                s_tone_control = s_presets[value].tone;
                s_punch_amount = s_presets[value].punch;
                s_snap_amount = s_presets[value].snap;
                s_metallic_amount = s_presets[value].metallic;
                s_noise_level = s_presets[value].noise;
                s_distortion = s_presets[value].dist;
            }
            break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0: return (int32_t)(s_attack_time * 1023.f);
        case 1: return (int32_t)(s_decay_time * 1023.f);
        case 2: return (int32_t)(s_tone_control * 1023.f);
        case 3: return (int32_t)(s_punch_amount * 1023.f);
        case 4: return (int32_t)(s_snap_amount * 1023.f);
        case 5: return (int32_t)(s_metallic_amount * 1023.f);
        case 6: return (int32_t)(s_noise_level * 1023.f);
        case 7: return (int32_t)(s_distortion * 1023.f);
        case 8: return s_sound_select;
        case 9: return s_preset_select;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
    if (id == 8) {
        static const char *sound_names[] = {
            "KICK1", "KICK2", "KICK3",
            "SNARE1", "SNARE2", "SNARE3",
            "OPHAT1", "OPHAT2", "OPHAT3",
            "CLHAT", "CLAP", "RIM"
        };
        return sound_names[value];
    }
    if (id == 9) {
        static const char *preset_names[] = {
            "CLASSIC", "TECHNO", "HOUSE", "TRANCE",
            "HRDCORE", "MINIMAL", "ACID", "CUSTOM"
        };
        return preset_names[value];
    }
    return "";
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velo)
{
    s_voice.velocity = velo;
    s_voice.active = true;
    s_voice.current_sound = s_sound_select;
    
    // Calculate pitch offset based on note (C3 = note 48 = 0 semitones)
    // Each octave = 12 semitones
    // C3 = 48, C4 = 60, C5 = 72, etc.
    float base_note = 48.f;  // C3
    s_voice.pitch_offset = (float)note - base_note;  // Semitones from C3
    
    // Reset all
    s_voice.phase = 0.f;
    s_voice.env_counter = 0;
    s_voice.env_stage = 0;
    
    s_voice.kick_pitch_env = 1.f;
    s_voice.kick_click_env = 1.f;
    
    s_voice.snare_tone_phase_1 = 0.f;
    s_voice.snare_tone_phase_2 = 0.f;
    s_voice.snare_noise_env = 1.f;
    
    for (int i = 0; i < 6; i++) {
        s_voice.hihat_phases[i] = 0.f;
    }
    
    s_voice.lpf_z1 = s_voice.lpf_z2 = 0.f;
    s_voice.hpf_z1 = s_voice.hpf_z2 = 0.f;
    s_voice.bpf_z1 = s_voice.bpf_z2 = 0.f;
}

__unit_callback void unit_note_off(uint8_t note)
{
    // Drums are one-shot, ignore note off
}

__unit_callback void unit_all_note_off()
{
    s_voice.active = false;
}

__unit_callback void unit_set_tempo(uint32_t tempo) {}
__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {}

__unit_callback void unit_pitch_bend(uint16_t bend) {}

__unit_callback void unit_channel_pressure(uint8_t press) {}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t press) {}

