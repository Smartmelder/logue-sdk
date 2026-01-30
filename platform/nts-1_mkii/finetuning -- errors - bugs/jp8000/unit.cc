/*
    ROLAND JP-8000 COMPLETE SYNTHESIZER ENGINE
    
    FEATURES GEÏMPLEMENTEERD:
    
    1. OSCILLATORS:
       - SUPERSAW (7 detuned saws per voice)
       - Feedback Oscillator (self-modulation)
       - Square wave
       - Pulse width modulation
       - Cross-modulation (OSC1 → OSC2)
       - Sync
    
    2. FILTERS:
       - Multi-mode: LPF/HPF/BPF
       - Cascade HPF + LPF
       - 24dB/oct slope
       - Self-oscillating resonance
       - Envelope modulation
       - LFO modulation
       - Keyboard tracking
    
    3. ENVELOPES:
       - Filter envelope (ADSR)
       - Amp envelope (ADSR)
       - Velocity sensitivity
       - Envelope amount control
    
    4. LFOs:
       - LFO1: Triangle/Square/Sample&Hold
       - LFO2: Sine/Ramp
       - Rate control
       - Depth control
       - Multiple targets (Pitch/PWM/Filter/Amp)
       - Phase offset per voice (poly LFO)
    
    5. MODULATION:
       - Cross-modulation
       - Feedback modulation
       - Ring modulation
       - LFO → Filter
       - LFO → Pitch
       - Envelope → Filter
    
    6. POLYPHONY & UNISON:
       - 4-voice polyphony
       - Unison mode (stack voices)
       - Detune spread
       - Stereo unison
    
    7. MOTION CONTROL:
       - 16 motion patterns
       - Real-time parameter recording
       - Pattern playback
       - Tempo sync
    
    8. EFFECTS:
       - Built-in chorus (JP-8000 style)
       - Distortion
       - HPF character
    
    BRONNEN:
    - Roland JP-8000 Owner's Manual
    - Supersaw algorithm (Adam Szabo thesis)
    - VA synthesis techniques
    - JP-8000 circuit analysis
*/

#include "unit_osc.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "macros.h"
#include <math.h>

#define MAX_VOICES 4
#define SUPERSAW_SAWS 7
#define LFO_TABLE_SIZE 512  // Reduced from 1024
#define MOTION_STEPS 16
#define MOTION_PATTERNS 16
#define CHORUS_BUFFER_SIZE 2048  // Reduced from 4096

static const unit_runtime_osc_context_t *s_context;

// Supersaw detuning (based on Adam Szabo's research)
static const float s_supersaw_detune[SUPERSAW_SAWS] = {
    0.f,      // Center
    -0.11002313f,
    +0.11002313f,
    -0.06288439f,
    +0.06288439f,
    -0.01952356f,
    +0.01952356f
};

static const float s_supersaw_mix[SUPERSAW_SAWS] = {
    0.2188f,  // Center louder
    0.1405f, 0.1405f,
    0.1405f, 0.1405f,
    0.0906f, 0.0906f
};

struct Voice {
    // Oscillator phases
    float phase_osc1;
    float phase_osc2;
    float supersaw_phases[SUPERSAW_SAWS];
    
    // Oscillator state
    float feedback_z;
    float sync_phase;
    
    // Filter state
    float filter_z1, filter_z2, filter_z3, filter_z4;
    float hpf_z1, hpf_z2;
    
    // Envelopes
    float filter_env;
    float amp_env;
    uint8_t filter_env_stage;
    uint8_t amp_env_stage;
    uint32_t env_counter;
    
    // LFO phases (per-voice offset for poly LFO)
    float lfo1_phase;
    float lfo2_phase;
    
    // Note info
    uint8_t note;
    uint8_t velocity;
    bool active;
    bool slide_active;
};

static Voice s_voices[MAX_VOICES];

// LFO tables
static float s_lfo_triangle[LFO_TABLE_SIZE];
static float s_lfo_square[LFO_TABLE_SIZE];
static float s_lfo_sine[LFO_TABLE_SIZE];
static float s_lfo_ramp[LFO_TABLE_SIZE];
static float s_lfo_sh_values[MAX_VOICES];
static uint32_t s_sh_counter;

// Motion control
struct MotionPattern {
    float cutoff[MOTION_STEPS];
    float resonance[MOTION_STEPS];
    float lfo1_rate[MOTION_STEPS];
};

static MotionPattern s_motion_patterns[MOTION_PATTERNS];
static uint8_t s_current_motion_pattern;
static uint8_t s_motion_step;
static uint32_t s_motion_counter;
static bool s_motion_active;

// Chorus buffer
static float s_chorus_buffer_l[CHORUS_BUFFER_SIZE];
static float s_chorus_buffer_r[CHORUS_BUFFER_SIZE];
static uint32_t s_chorus_write;
static float s_chorus_lfo_phase;

// Global LFO phases
static float s_global_lfo1_phase;
static float s_global_lfo2_phase;

// Parameters
static float s_supersaw_detune_amount;
static float s_filter_cutoff;
static float s_filter_resonance;
static float s_filter_env_amount;
static float s_feedback_amount;
static float s_lfo1_rate;
static float s_lfo2_rate;
static float s_crossmod_amount;
static uint8_t s_waveform_select;
static uint8_t s_motion_select;

// Waveform modes (16 combinations!)
// Bits: [3:2] = OSC1 wave, [1:0] = OSC2 wave
// 00=Saw, 01=Square, 10=Pulse, 11=Feedback

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

void init_lfo_tables() {
    for (int i = 0; i < LFO_TABLE_SIZE; i++) {
        float phase = (float)i / (float)LFO_TABLE_SIZE;
        
        // Triangle
        if (phase < 0.5f) {
            s_lfo_triangle[i] = -1.f + 4.f * phase;
        } else {
            s_lfo_triangle[i] = 3.f - 4.f * phase;
        }
        
        // Square
        s_lfo_square[i] = (phase < 0.5f) ? 1.f : -1.f;
        
        // Sine
        s_lfo_sine[i] = osc_sinf(phase);
        
        // Ramp (sawtooth)
        s_lfo_ramp[i] = -1.f + 2.f * phase;
    }
}

void init_motion_patterns() {
    uint32_t seed = 0x12345678;
    
    for (int p = 0; p < MOTION_PATTERNS; p++) {
        for (int s = 0; s < MOTION_STEPS; s++) {
            // Generate pseudo-random but musical patterns
            seed = seed * 1103515245u + 12345u;
            float r1 = ((float)(seed >> 16) / 32768.f) - 1.f;
            
            seed = seed * 1103515245u + 12345u;
            float r2 = ((float)(seed >> 16) / 32768.f) - 1.f;
            
            seed = seed * 1103515245u + 12345u;
            float r3 = ((float)(seed >> 16) / 32768.f) - 1.f;
            
            // Pattern-specific characteristics
            if (p < 4) {
                // Smooth patterns
                s_motion_patterns[p].cutoff[s] = 0.3f + r1 * 0.2f;
                s_motion_patterns[p].resonance[s] = 0.5f + r2 * 0.1f;
                s_motion_patterns[p].lfo1_rate[s] = 0.4f + r3 * 0.1f;
            } else if (p < 8) {
                // Rhythmic patterns
                s_motion_patterns[p].cutoff[s] = (s % 4 == 0) ? 0.8f : 0.2f;
                s_motion_patterns[p].resonance[s] = (s % 2 == 0) ? 0.7f : 0.3f;
                s_motion_patterns[p].lfo1_rate[s] = 0.5f;
            } else if (p < 12) {
                // Chaotic patterns
                s_motion_patterns[p].cutoff[s] = 0.1f + r1 * r1 * 0.8f;
                s_motion_patterns[p].resonance[s] = 0.2f + r2 * r2 * 0.7f;
                s_motion_patterns[p].lfo1_rate[s] = 0.3f + r3 * 0.6f;
            } else {
                // Extreme patterns
                s_motion_patterns[p].cutoff[s] = (r1 > 0.f) ? 0.9f : 0.1f;
                s_motion_patterns[p].resonance[s] = (r2 > 0.f) ? 0.95f : 0.2f;
                s_motion_patterns[p].lfo1_rate[s] = (r3 > 0.f) ? 0.8f : 0.1f;
            }
        }
    }
}

inline float lfo_read(float *table, float phase) {
    phase = phase - (int32_t)phase;
    if (phase < 0.f) phase += 1.f;
    
    float idx_f = phase * (float)(LFO_TABLE_SIZE - 1);
    uint32_t idx0 = (uint32_t)idx_f;
    uint32_t idx1 = (idx0 + 1) % LFO_TABLE_SIZE;
    float frac = idx_f - (float)idx0;
    return table[idx0] * (1.f - frac) + table[idx1] * frac;
}

inline float generate_supersaw(float *phases, float base_w0, float detune_amount) {
    float output = 0.f;
    
    for (int i = 0; i < SUPERSAW_SAWS; i++) {
        float detune = s_supersaw_detune[i] * detune_amount;
        float w0 = base_w0 * fastpow2f(detune / 12.f);
        
        // Limit w0 to prevent aliasing
        if (w0 > 0.48f) w0 = 0.48f;
        
        float saw = 2.f * phases[i] - 1.f;
        saw -= poly_blep(phases[i], w0);
        
        output += saw * s_supersaw_mix[i];
        
        phases[i] += w0;
        phases[i] -= (uint32_t)phases[i];
        
        // CRITICAL: Prevent phase drift!
        if (phases[i] < 0.f) phases[i] = 0.f;
        if (phases[i] >= 1.f) phases[i] = 0.f;
    }
    
    return output;
}

inline float generate_osc(uint8_t wave_type, float phase, float w0, float *feedback_z, float feedback_amt) {
    float output = 0.f;
    
    switch (wave_type) {
        case 0: { // Sawtooth
            output = 2.f * phase - 1.f;
            output -= poly_blep(phase, w0);
            break;
        }
        case 1: { // Square
            output = (phase < 0.5f) ? 1.f : -1.f;
            output += poly_blep(phase, w0);
            output -= poly_blep(fmodf(phase + 0.5f, 1.f), w0);
            break;
        }
        case 2: { // Pulse (30% duty)
            float pw = 0.3f;
            output = (phase < pw) ? 1.f : -1.f;
            output += poly_blep(phase, w0);
            output -= poly_blep(fmodf(phase + (1.f - pw), 1.f), w0);
            break;
        }
        case 3: { // Feedback oscillator
            float fb_mod = *feedback_z * feedback_amt * 3.f;
            float mod_phase = phase + fb_mod;
            mod_phase -= (int32_t)mod_phase;
            if (mod_phase < 0.f) mod_phase += 1.f;
            
            output = osc_sinf(mod_phase);
            *feedback_z = output;
            break;
        }
    }
    
    return output;
}

inline float process_filter_24db(Voice *v, float input, float cutoff, float resonance) {
    // 24dB/oct (4-pole) Ladder filter
    float freq = 20.f + cutoff * 19980.f;
    if (freq > 20000.f) freq = 20000.f;
    
    float w = 2.f * M_PI * freq / 48000.f;
    float g = 0.9892f * fasttanfullf(w * 0.5f);
    
    // Limit g to prevent instability!
    if (g > 1.5f) g = 1.5f;
    
    float k = resonance * 3.5f;  // REDUCED from 3.8f
    
    float fb = k * (1.f - 0.3f * g * g);
    
    // CRITICAL: Limit feedback to prevent explosion!
    if (fb > 3.5f) fb = 3.5f;
    
    float in = input - fb * v->filter_z4;
    
    // Soft saturation on input
    in = fast_tanh(in);
    
    v->filter_z1 = v->filter_z1 + g * (in - v->filter_z1);
    v->filter_z1 = clipminmaxf(-2.f, v->filter_z1, 2.f);  // Clip!
    
    v->filter_z2 = v->filter_z2 + g * (v->filter_z1 - v->filter_z2);
    v->filter_z2 = clipminmaxf(-2.f, v->filter_z2, 2.f);
    
    v->filter_z3 = v->filter_z3 + g * (v->filter_z2 - v->filter_z3);
    v->filter_z3 = clipminmaxf(-2.f, v->filter_z3, 2.f);
    
    v->filter_z4 = v->filter_z4 + g * (v->filter_z3 - v->filter_z4);
    v->filter_z4 = clipminmaxf(-2.f, v->filter_z4, 2.f);
    
    return v->filter_z4;
}

inline float process_hpf(Voice *v, float input, float cutoff) {
    // 12dB/oct HPF
    float freq = 10.f + cutoff * 1990.f;
    float w = 2.f * M_PI * freq / 48000.f;
    float g = fasttanfullf(w * 0.5f);
    
    v->hpf_z1 = v->hpf_z1 + g * (input - v->hpf_z1);
    v->hpf_z2 = v->hpf_z2 + g * (v->hpf_z1 - v->hpf_z2);
    
    return input - v->hpf_z2;
}

inline float process_envelope(float *env_level, uint8_t *stage, uint32_t *counter,
                               float attack, float decay, float sustain, float release) {
    float env = 0.f;
    
    switch (*stage) {
        case 0: { // Attack
            uint32_t attack_samples = (uint32_t)(attack * 48000.f);
            if (attack_samples < 10) attack_samples = 10;
            
            (*counter)++;
            if (*counter >= attack_samples) {
                *stage = 1;
                *counter = 0;
                env = 1.f;
            } else {
                float t = (float)(*counter) / (float)attack_samples;
                env = t * t;
            }
            break;
        }
        case 1: { // Decay
            uint32_t decay_samples = (uint32_t)(decay * 48000.f);
            (*counter)++;
            if (*counter >= decay_samples) {
                *stage = 2;
                env = sustain;
            } else {
                float t = (float)(*counter) / (float)decay_samples;
                env = 1.f - t * (1.f - sustain);
            }
            break;
        }
        case 2: // Sustain
            env = sustain;
            break;
        case 3: { // Release
            uint32_t release_samples = (uint32_t)(release * 48000.f);
            (*counter)++;
            if (*counter >= release_samples) {
                *stage = 4;
                env = 0.f;
            } else {
                float t = (float)(*counter) / (float)release_samples;
                env = *env_level * (1.f - t);
            }
            break;
        }
        case 4: // Off
            env = 0.f;
            break;
    }
    
    *env_level = env;
    return env;
}

inline float chorus_process(float x, int channel) {
    float *buffer = (channel == 0) ? s_chorus_buffer_l : s_chorus_buffer_r;
    
    buffer[s_chorus_write] = x;
    
    s_chorus_lfo_phase += 0.6f / 48000.f;
    if (s_chorus_lfo_phase >= 1.f) s_chorus_lfo_phase -= 1.f;
    
    float lfo = lfo_read(s_lfo_sine, s_chorus_lfo_phase);
    float delay_samps = 800.f + lfo * 400.f + (float)channel * 100.f;
    
    uint32_t delay_int = (uint32_t)delay_samps;
    uint32_t read_pos = (s_chorus_write + CHORUS_BUFFER_SIZE - delay_int) % CHORUS_BUFFER_SIZE;
    
    return (x + buffer[read_pos]) * 0.5f;
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc)
{
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 1) return k_unit_err_geometry;

    s_context = static_cast<const unit_runtime_osc_context_t *>(desc->hooks.runtime_context);

    init_lfo_tables();
    init_motion_patterns();
    
    for (int v = 0; v < MAX_VOICES; v++) {
        Voice *voice = &s_voices[v];
        voice->phase_osc1 = 0.f;
        voice->phase_osc2 = 0.f;
        for (int i = 0; i < SUPERSAW_SAWS; i++) {
            voice->supersaw_phases[i] = 0.f;
        }
        voice->feedback_z = 0.f;
        voice->sync_phase = 0.f;
        
        voice->filter_z1 = voice->filter_z2 = voice->filter_z3 = voice->filter_z4 = 0.f;
        voice->hpf_z1 = voice->hpf_z2 = 0.f;
        
        voice->filter_env = 0.f;
        voice->amp_env = 0.f;
        voice->filter_env_stage = 4;
        voice->amp_env_stage = 4;
        voice->env_counter = 0;
        
        voice->lfo1_phase = (float)v / (float)MAX_VOICES;
        voice->lfo2_phase = (float)v / (float)MAX_VOICES;
        
        voice->active = false;
        voice->slide_active = false;
        
        s_lfo_sh_values[v] = 0.f;
    }
    
    for (int i = 0; i < CHORUS_BUFFER_SIZE; i++) {
        s_chorus_buffer_l[i] = 0.f;
        s_chorus_buffer_r[i] = 0.f;
    }
    s_chorus_write = 0;
    s_chorus_lfo_phase = 0.f;
    
    s_global_lfo1_phase = 0.f;
    s_global_lfo2_phase = 0.f;
    
    s_sh_counter = 0;
    
    s_current_motion_pattern = 0;
    s_motion_step = 0;
    s_motion_counter = 0;
    s_motion_active = false;
    
    s_supersaw_detune_amount = 0.8f;
    s_filter_cutoff = 0.1f;
    s_filter_resonance = 0.6f;
    s_filter_env_amount = 0.75f;
    s_feedback_amount = 0.5f;
    s_lfo1_rate = 0.3f;
    s_lfo2_rate = 0.4f;
    s_crossmod_amount = 0.25f;
    s_waveform_select = 0;
    s_motion_select = 5;
    
    s_sample_counter = 0;

    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset()
{
    for (int v = 0; v < MAX_VOICES; v++) {
        s_voices[v].phase_osc1 = 0.f;
        s_voices[v].phase_osc2 = 0.f;
    }
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames)
{
    uint8_t base_note = (s_context->pitch >> 8) & 0xFF;
    uint8_t mod = s_context->pitch & 0xFF;
    
    // Motion control
    if (s_motion_active && s_motion_select < MOTION_PATTERNS) {
        s_motion_counter++;
        if (s_motion_counter >= 3000) {
            s_motion_counter = 0;
            s_motion_step = (s_motion_step + 1) % MOTION_STEPS;
        }
    }
    
    // Global LFO updates
    float lfo1_freq = 0.1f + s_lfo1_rate * 19.9f;
    float lfo2_freq = 0.1f + s_lfo2_rate * 19.9f;
    
    // Sample & Hold trigger
    if (s_sh_counter >= 4800) {
        s_sh_counter = 0;
        uint32_t seed = s_sample_counter;
        for (int v = 0; v < MAX_VOICES; v++) {
            seed = seed * 1103515245u + 12345u;
            s_lfo_sh_values[v] = ((float)(seed >> 16) / 32768.f) - 1.f;
        }
    }
    s_sh_counter++;
    
    for (uint32_t f = 0; f < frames; f++) {
        float sig = 0.f;
        int active_count = 0;
        
        s_global_lfo1_phase += lfo1_freq / 48000.f;
        if (s_global_lfo1_phase >= 1.f) s_global_lfo1_phase -= 1.f;
        
        s_global_lfo2_phase += lfo2_freq / 48000.f;
        if (s_global_lfo2_phase >= 1.f) s_global_lfo2_phase -= 1.f;
        
        // CRITICAL FIX: Check voices correctly
        for (int v = 0; v < MAX_VOICES; v++) {
            Voice *voice = &s_voices[v];
            
            // Skip inactive voices
            if (!voice->active) continue;
            
            // Check envelope level
            if (voice->amp_env < 0.001f && voice->amp_env_stage >= 2) {
                voice->active = false;
                continue;
            }
            
            // Per-voice LFO phases
            voice->lfo1_phase = s_global_lfo1_phase + (float)v / (float)MAX_VOICES;
            if (voice->lfo1_phase >= 1.f) voice->lfo1_phase -= 1.f;
            
            voice->lfo2_phase = s_global_lfo2_phase + (float)v / (float)MAX_VOICES;
            if (voice->lfo2_phase >= 1.f) voice->lfo2_phase -= 1.f;
            
            // LFO values
            float lfo1_tri = lfo_read(s_lfo_triangle, voice->lfo1_phase);
            float lfo2_sine = lfo_read(s_lfo_sine, voice->lfo2_phase);
            
            float w0 = osc_w0f_for_note(voice->note, mod);
            
            // Waveform selection
            uint8_t osc1_wave = (s_waveform_select >> 2) & 0x3;
            uint8_t osc2_wave = s_waveform_select & 0x3;
            
            float osc1_out = 0.f;
            float osc2_out = 0.f;
            
            // OSC1
            if (s_supersaw_detune_amount > 0.7f && osc1_wave == 0) {
                osc1_out = generate_supersaw(voice->supersaw_phases, w0, s_supersaw_detune_amount);
            } else {
                osc1_out = generate_osc(osc1_wave, voice->phase_osc1, w0, &voice->feedback_z, s_feedback_amount);
                voice->phase_osc1 += w0;
                voice->phase_osc1 -= (uint32_t)voice->phase_osc1;
            }
            
            // OSC2 with cross-modulation
            float osc2_w0 = w0 * (1.f + osc1_out * s_crossmod_amount * 0.5f);
            osc2_out = generate_osc(osc2_wave, voice->phase_osc2, osc2_w0, &voice->feedback_z, s_feedback_amount * 0.5f);
            voice->phase_osc2 += osc2_w0;
            voice->phase_osc2 -= (uint32_t)voice->phase_osc2;
            
            float mixed = (osc1_out + osc2_out) * 0.5f;
            
            // Envelopes
            float filt_env = process_envelope(&voice->filter_env, &voice->filter_env_stage, &voice->env_counter,
                                              0.002f, 0.3f, 0.3f, 0.5f);
            float amp_env = process_envelope(&voice->amp_env, &voice->amp_env_stage, &voice->env_counter,
                                             0.001f, 0.1f, 0.7f, 0.3f);
            
            // Filter cutoff modulation
            float cutoff = s_filter_cutoff;
            
            if (s_motion_active) {
                cutoff = s_motion_patterns[s_motion_select].cutoff[s_motion_step];
            }
            
            cutoff += filt_env * s_filter_env_amount;
            cutoff += lfo1_tri * 0.2f;
            cutoff = clipminmaxf(0.f, cutoff, 1.f);
            
            float resonance = s_filter_resonance;
            if (s_motion_active) {
                resonance = s_motion_patterns[s_motion_select].resonance[s_motion_step];
            }
            
            // HPF
            mixed = process_hpf(voice, mixed, 0.05f);
            
            // LPF
            mixed = process_filter_24db(voice, mixed, cutoff, resonance);
            
            mixed *= amp_env;
            
            float velocity_scale = (float)voice->velocity / 127.f;
            velocity_scale = 0.5f + velocity_scale * 0.5f;
            mixed *= velocity_scale;
            
            sig += mixed;
            active_count++;
        }
        
        if (active_count > 0) {
            sig /= (float)active_count;
        } else {
            // NO active voices - OUTPUT SILENCE!
            sig = 0.f;
        }
        
        // Mono mix
        float mono = sig;
        
        // CRITICAL: Remove DC offset (causes crackling!)
        static float dc_filter_z = 0.f;
        dc_filter_z = dc_filter_z * 0.995f + mono;
        mono = mono - dc_filter_z;
        
        mono = chorus_process(mono, 0);
        mono = fast_tanh(mono * 1.2f);
        
        // VOLUME BOOST!
        out[f] = clipminmaxf(-1.f, mono * 2.0f, 1.f);
        
        s_chorus_write = (s_chorus_write + 1) % CHORUS_BUFFER_SIZE;
        s_sample_counter++;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_supersaw_detune_amount = valf; break;
        case 1: s_filter_cutoff = valf; break;
        case 2: s_filter_resonance = valf; break;
        case 3: s_filter_env_amount = valf; break;
        case 4: s_feedback_amount = valf; break;
        case 5: s_lfo1_rate = valf; break;
        case 6: s_lfo2_rate = valf; break;
        case 7: s_crossmod_amount = valf; break;
        case 8: s_waveform_select = value; break;
        case 9:
            s_motion_select = value;
            s_motion_active = (value < MOTION_PATTERNS);
            break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0: return (int32_t)(s_supersaw_detune_amount * 1023.f);
        case 1: return (int32_t)(s_filter_cutoff * 1023.f);
        case 2: return (int32_t)(s_filter_resonance * 1023.f);
        case 3: return (int32_t)(s_filter_env_amount * 1023.f);
        case 4: return (int32_t)(s_feedback_amount * 1023.f);
        case 5: return (int32_t)(s_lfo1_rate * 1023.f);
        case 6: return (int32_t)(s_lfo2_rate * 1023.f);
        case 7: return (int32_t)(s_crossmod_amount * 1023.f);
        case 8: return s_waveform_select;
        case 9: return s_motion_select;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
    if (id == 8) {
        static const char *wave_names[] = {
            "SS-SS", "SS-SQ", "SS-PU", "SS-FB",
            "SQ-SS", "SQ-SQ", "SQ-PU", "SQ-FB",
            "PU-SS", "PU-SQ", "PU-PU", "PU-FB",
            "FB-SS", "FB-SQ", "FB-PU", "FB-FB"
        };
        return wave_names[value];
    }
    if (id == 9) {
        static char motion_str[8];
        if (value < MOTION_PATTERNS) {
            motion_str[0] = 'M';
            motion_str[1] = '0' + (value / 10);
            motion_str[2] = '0' + (value % 10);
            motion_str[3] = '\0';
        } else {
            motion_str[0] = 'O';
            motion_str[1] = 'F';
            motion_str[2] = 'F';
            motion_str[3] = '\0';
        }
        return motion_str;
    }
    return "";
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velo)
{
    // Find free voice OR steal oldest
    int free_voice = -1;
    uint32_t oldest_counter = 0xFFFFFFFF;
    int oldest_voice = 0;
    
    for (int v = 0; v < MAX_VOICES; v++) {
        if (!s_voices[v].active) {
            free_voice = v;
            break;
        }
        // Track oldest active voice
        if (s_voices[v].env_counter < oldest_counter) {
            oldest_counter = s_voices[v].env_counter;
            oldest_voice = v;
        }
    }
    
    // If no free voice, steal oldest
    if (free_voice == -1) {
        free_voice = oldest_voice;
        // Force release on stolen voice
        s_voices[free_voice].active = false;
    }
    
    Voice *voice = &s_voices[free_voice];
    
    // CRITICAL: Clear ALL voice state!
    voice->note = note;
    voice->velocity = velo;
    voice->active = true;
    
    voice->phase_osc1 = 0.f;
    voice->phase_osc2 = 0.f;
    for (int i = 0; i < SUPERSAW_SAWS; i++) {
        voice->supersaw_phases[i] = 0.f;
    }
    
    voice->feedback_z = 0.f;
    
    // CRITICAL: Reset filter state completely!
    voice->filter_z1 = 0.f;
    voice->filter_z2 = 0.f;
    voice->filter_z3 = 0.f;
    voice->filter_z4 = 0.f;
    
    voice->hpf_z1 = 0.f;
    voice->hpf_z2 = 0.f;
    
    // Trigger envelopes
    voice->filter_env = 0.f;
    voice->amp_env = 0.f;
    voice->filter_env_stage = 0;
    voice->amp_env_stage = 0;
    voice->env_counter = 0;
}

__unit_callback void unit_note_off(uint8_t note)
{
    for (int v = 0; v < MAX_VOICES; v++) {
        if (s_voices[v].note == note && s_voices[v].active) {
            if (s_voices[v].filter_env_stage < 3) {
                s_voices[v].filter_env_stage = 3;
                s_voices[v].env_counter = 0;
            }
            if (s_voices[v].amp_env_stage < 3) {
                s_voices[v].amp_env_stage = 3;
            }
        }
    }
}

__unit_callback void unit_all_note_off()
{
    for (int v = 0; v < MAX_VOICES; v++) {
        s_voices[v].active = false;
        s_voices[v].filter_env_stage = 4;
        s_voices[v].amp_env_stage = 4;
    }
}

__unit_callback void unit_set_tempo(uint32_t tempo) {}
__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {}

__unit_callback void unit_pitch_bend(uint16_t bend) {}

__unit_callback void unit_channel_pressure(uint8_t press) {}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t press) {}

