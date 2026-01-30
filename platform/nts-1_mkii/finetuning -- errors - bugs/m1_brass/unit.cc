/*
    M1 BRASS & STRINGS - Ultimate Recreation
    
    ═══════════════════════════════════════════════════════════════
    KORG M1 ARCHITECTURE
    ═══════════════════════════════════════════════════════════════
    
    === HISTORICAL CONTEXT ===
    
    The Korg M1 (1988) revolutionized music production:
    - First "workstation" synthesizer
    - 100,000+ units sold (best-selling synth ever)
    - Defined the sound of late 80s/90s music
    - Every hit record from 1988-1995 used M1
    
    Famous M1 sounds:
    - "Lore" strings (trance/house intro sound)
    - "M1 Piano" (already implemented separately)
    - "Organ 2" (Robin S - Show Me Love)
    - "Universe" (ambient pad)
    - "Lately Bass" (slap bass)
    
    === M1 SYNTHESIS METHOD ===
    
    AI² Synthesis (Advanced Integrated):
    - PCM waveforms (100+ samples)
    - Wavetable crossfading
    - Resonant filters
    - Digital effects
    - Velocity switching
    
    This implementation focuses on BRASS & STRINGS.
    
    ═══════════════════════════════════════════════════════════════
    BRASS SYNTHESIS
    ═══════════════════════════════════════════════════════════════
    
    === FORMANT FILTER BANK ===
    
    Brass instruments have resonant frequencies (formants):
    
    TRUMPET formants:
    - F1: 600 Hz (fundamental character)
    - F2: 1200 Hz (brightness)
    - F3: 2800 Hz (brilliance)
    
    TROMBONE formants:
    - F1: 400 Hz (deeper)
    - F2: 900 Hz
    - F3: 2200 Hz
    
    SAX formants:
    - F1: 500 Hz
    - F2: 1500 Hz
    - F3: 2500 Hz
    
    We implement 3-band peak filters with adjustable Q.
    
    === STRINGS SYNTHESIS ===
    
    String ensemble (M1 "Lore" style):
    - 8-voice unison (detuned)
    - Pulse wave oscillators
    - Chorus-like detuning
    - Slow attack
    - Stereo spread
    
*/

#include "unit_osc.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "osc_api.h"
#include "fx_api.h"  // ✅ For fx_pow2f()

// SDK compatibility
// SDK compatibility - PI is already defined in CMSIS arm_math.h
// const float PI = 3.14159265359f; // Removed - conflicts with CMSIS

inline float mod1(float x) {
    while (x >= 1.f) x -= 1.f;
    while (x < 0.f) x += 1.f;
    return x;
}

#define MAX_VOICES 3
#define ENSEMBLE_VOICES 8
#define CHORUS_BUFFER_SIZE 4096

static const unit_runtime_osc_context_t *s_context;

// Ensemble detune values (cents)
static const float s_ensemble_detune[ENSEMBLE_VOICES] = {
    0.0f, -8.0f, 8.0f, -5.0f, 5.0f, -3.0f, 3.0f, -1.5f
};

// Ensemble pan positions
static const float s_ensemble_pan[ENSEMBLE_VOICES] = {
    0.0f, -0.7f, 0.7f, -0.4f, 0.4f, -0.2f, 0.2f, -0.1f
};

// M1 Patch definitions
struct M1Patch {
    // Oscillator mix
    float osc_saw_level;
    float osc_pulse_level;
    float pulse_width;
    
    // Formant frequencies (Hz)
    float formant1_freq;
    float formant2_freq;
    float formant3_freq;
    
    // Formant Q values
    float formant1_q;
    float formant2_q;
    float formant3_q;
    
    // Envelope
    float attack;
    float decay;
    float sustain;
    float release;
    
    // Vibrato
    float vibrato_rate;
    float vibrato_depth;
    float vibrato_delay;
    
    const char* name;
};

static const M1Patch s_patches[8] = {
    // BRASS 1 - Full Section
    {
        .osc_saw_level = 0.8f,
        .osc_pulse_level = 0.3f,
        .pulse_width = 0.5f,
        .formant1_freq = 600.f,
        .formant2_freq = 1200.f,
        .formant3_freq = 2800.f,
        .formant1_q = 5.f,
        .formant2_q = 8.f,
        .formant3_q = 4.f,
        .attack = 0.02f,
        .decay = 0.1f,
        .sustain = 0.7f,
        .release = 0.3f,
        .vibrato_rate = 5.5f,
        .vibrato_depth = 0.015f,
        .vibrato_delay = 0.3f,
        .name = "BRASS1"
    },
    
    // BRASS 2 - Solo Trumpet
    {
        .osc_saw_level = 0.9f,
        .osc_pulse_level = 0.2f,
        .pulse_width = 0.4f,
        .formant1_freq = 650.f,
        .formant2_freq = 1300.f,
        .formant3_freq = 3000.f,
        .formant1_q = 6.f,
        .formant2_q = 10.f,
        .formant3_q = 5.f,
        .attack = 0.01f,
        .decay = 0.05f,
        .sustain = 0.8f,
        .release = 0.2f,
        .vibrato_rate = 6.0f,
        .vibrato_depth = 0.025f,
        .vibrato_delay = 0.4f,
        .name = "BRASS2"
    },
    
    // STRINGS 1 - Ensemble (The "Lore" sound!)
    {
        .osc_saw_level = 0.4f,
        .osc_pulse_level = 0.9f,
        .pulse_width = 0.6f,
        .formant1_freq = 400.f,
        .formant2_freq = 800.f,
        .formant3_freq = 2000.f,
        .formant1_q = 3.f,
        .formant2_q = 4.f,
        .formant3_q = 3.f,
        .attack = 0.08f,
        .decay = 0.2f,
        .sustain = 0.9f,
        .release = 0.5f,
        .vibrato_rate = 4.5f,
        .vibrato_depth = 0.008f,
        .vibrato_delay = 0.5f,
        .name = "STRING1"
    },
    
    // STRINGS 2 - Chamber
    {
        .osc_saw_level = 0.5f,
        .osc_pulse_level = 0.7f,
        .pulse_width = 0.55f,
        .formant1_freq = 350.f,
        .formant2_freq = 700.f,
        .formant3_freq = 1800.f,
        .formant1_q = 4.f,
        .formant2_q = 5.f,
        .formant3_q = 4.f,
        .attack = 0.06f,
        .decay = 0.15f,
        .sustain = 0.85f,
        .release = 0.4f,
        .vibrato_rate = 4.0f,
        .vibrato_depth = 0.006f,
        .vibrato_delay = 0.6f,
        .name = "STRING2"
    },
    
    // CHOIR - Synth Voices
    {
        .osc_saw_level = 0.3f,
        .osc_pulse_level = 0.8f,
        .pulse_width = 0.7f,
        .formant1_freq = 500.f,
        .formant2_freq = 1000.f,
        .formant3_freq = 2500.f,
        .formant1_q = 7.f,
        .formant2_q = 9.f,
        .formant3_q = 6.f,
        .attack = 0.1f,
        .decay = 0.3f,
        .sustain = 0.8f,
        .release = 0.6f,
        .vibrato_rate = 3.5f,
        .vibrato_depth = 0.012f,
        .vibrato_delay = 0.7f,
        .name = "CHOIR"
    },
    
    // SAX - Tenor
    {
        .osc_saw_level = 0.85f,
        .osc_pulse_level = 0.25f,
        .pulse_width = 0.45f,
        .formant1_freq = 500.f,
        .formant2_freq = 1500.f,
        .formant3_freq = 2500.f,
        .formant1_q = 8.f,
        .formant2_q = 12.f,
        .formant3_q = 6.f,
        .attack = 0.015f,
        .decay = 0.08f,
        .sustain = 0.75f,
        .release = 0.25f,
        .vibrato_rate = 5.0f,
        .vibrato_depth = 0.03f,
        .vibrato_delay = 0.2f,
        .name = "SAX"
    },
    
    // FLUTE - Breathy
    {
        .osc_saw_level = 0.2f,
        .osc_pulse_level = 0.4f,
        .pulse_width = 0.3f,
        .formant1_freq = 800.f,
        .formant2_freq = 1600.f,
        .formant3_freq = 3500.f,
        .formant1_q = 2.f,
        .formant2_q = 3.f,
        .formant3_q = 2.f,
        .attack = 0.01f,
        .decay = 0.05f,
        .sustain = 0.6f,
        .release = 0.15f,
        .vibrato_rate = 4.5f,
        .vibrato_depth = 0.02f,
        .vibrato_delay = 0.3f,
        .name = "FLUTE"
    },
    
    // HORN - French Horn
    {
        .osc_saw_level = 0.75f,
        .osc_pulse_level = 0.35f,
        .pulse_width = 0.5f,
        .formant1_freq = 400.f,
        .formant2_freq = 900.f,
        .formant3_freq = 2200.f,
        .formant1_q = 6.f,
        .formant2_q = 9.f,
        .formant3_q = 5.f,
        .attack = 0.03f,
        .decay = 0.12f,
        .sustain = 0.7f,
        .release = 0.35f,
        .vibrato_rate = 4.8f,
        .vibrato_depth = 0.018f,
        .vibrato_delay = 0.5f,
        .name = "HORN"
    }
};

// Voice structure
struct Voice {
    bool active;
    uint8_t note;
    uint8_t velocity;
    
    // Oscillator phases (ensemble)
    float ensemble_phases_saw[ENSEMBLE_VOICES];
    float ensemble_phases_pulse[ENSEMBLE_VOICES];
    
    // Formant filters (3-band peak filter)
    float formant1_z1, formant1_z2;
    float formant2_z1, formant2_z2;
    float formant3_z1, formant3_z2;
    
    // Envelope
    float amp_env;
    uint8_t env_stage;  // 0=attack, 1=decay, 2=sustain, 3=release
    uint32_t env_counter;
    
    // Vibrato
    float vibrato_phase;
    float vibrato_fade;
    uint32_t vibrato_counter;
    
    // Breath controller
    float breath_level;
};

static Voice s_voices[MAX_VOICES];

// Chorus
static float s_chorus_buffer_l[CHORUS_BUFFER_SIZE];
static float s_chorus_buffer_r[CHORUS_BUFFER_SIZE];
static uint32_t s_chorus_write;
static float s_chorus_lfo;

// Parameters
static float s_brightness;
static float s_resonance;
static float s_detune_amount;
static float s_ensemble_amount;
static float s_vibrato_amount;
static float s_breath_amount;
static float s_attack_mod;
static float s_release_mod;
static uint8_t s_patch_select;
static uint8_t s_voice_count;

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

// Peak filter (for formants) - Biquad implementation
inline float process_peak_filter(float input, float freq, float q, float *z1, float *z2) {
    float w = 2.f * PI * freq / 48000.f;
    
    // Limit w to prevent instability
    if (w > PI * 0.99f) w = PI * 0.99f;
    
    // Convert to phase [0,1] for SDK functions
    float phase_w = w / (2.f * PI);
    if (phase_w >= 1.f) phase_w -= 1.f;
    if (phase_w < 0.f) phase_w += 1.f;
    
    float phase_sin = phase_w * 0.5f;
    if (phase_sin >= 1.f) phase_sin -= 1.f;
    if (phase_sin < 0.f) phase_sin += 1.f;
    float alpha = osc_sinf(phase_sin) * (1.f / (2.f * q));
    float cos_w = osc_cosf(phase_w);
    
    float b0 = alpha;
    float b1 = 0.f;
    float b2 = -alpha;
    float a0 = 1.f + alpha;
    float a1 = -2.f * cos_w;
    float a2 = 1.f - alpha;
    
    // Normalize
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a1 /= a0;
    a2 /= a0;
    
    float output = b0 * input + b1 * (*z1) + b2 * (*z2) - a1 * (*z1) - a2 * (*z2);
    
    *z2 = *z1;
    *z1 = output;
    
    return output;
}

// Generate ensemble (8-voice unison)
inline void generate_ensemble(Voice *v, float base_w0, const M1Patch *patch, float *out_l, float *out_r) {
    float sum_l = 0.f;
    float sum_r = 0.f;
    
    int voices_active = (s_voice_count == 0) ? 1 : (1 << s_voice_count);  // 1, 2, 4, 8
    if (voices_active > ENSEMBLE_VOICES) voices_active = ENSEMBLE_VOICES;
    
    for (int i = 0; i < voices_active; i++) {
        // Detune
        float detune_cents = s_ensemble_detune[i] * s_detune_amount;
        float w0 = base_w0 * fx_pow2f(detune_cents / 1200.f);  // ✅ Fixed: fx_pow2f (fastpow2f doesn't exist!)
        
        // Limit w0 to prevent aliasing
        if (w0 > 0.48f) w0 = 0.48f;
        
        // SAWTOOTH
        float saw = 2.f * v->ensemble_phases_saw[i] - 1.f;
        saw -= poly_blep(v->ensemble_phases_saw[i], w0);
        
        // PULSE (variable width)
        float pw = patch->pulse_width;
        float pulse = (v->ensemble_phases_pulse[i] < pw) ? 1.f : -1.f;
        pulse += poly_blep(v->ensemble_phases_pulse[i], w0);
        float phase2 = v->ensemble_phases_pulse[i] + 1.f - pw;
        phase2 -= (uint32_t)phase2;
        if (phase2 < 0.f) phase2 += 1.f;
        if (phase2 >= 1.f) phase2 -= 1.f;
        pulse -= poly_blep(phase2, w0);
        
        // Mix oscillators
        float mixed = saw * patch->osc_saw_level + pulse * patch->osc_pulse_level;
        
        // Pan
        float pan = s_ensemble_pan[i] * s_ensemble_amount;
        float gain_l = (1.f - pan) * 0.5f;
        float gain_r = (1.f + pan) * 0.5f;
        
        sum_l += mixed * gain_l;
        sum_r += mixed * gain_r;
        
        // Update phases
        v->ensemble_phases_saw[i] += w0;
        v->ensemble_phases_saw[i] -= (uint32_t)v->ensemble_phases_saw[i];
        if (v->ensemble_phases_saw[i] < 0.f) v->ensemble_phases_saw[i] = 0.f;
        if (v->ensemble_phases_saw[i] >= 1.f) v->ensemble_phases_saw[i] = 0.f;
        
        v->ensemble_phases_pulse[i] += w0;
        v->ensemble_phases_pulse[i] -= (uint32_t)v->ensemble_phases_pulse[i];
        if (v->ensemble_phases_pulse[i] < 0.f) v->ensemble_phases_pulse[i] = 0.f;
        if (v->ensemble_phases_pulse[i] >= 1.f) v->ensemble_phases_pulse[i] = 0.f;
    }
    
    // Normalize
    *out_l = sum_l / (float)voices_active;
    *out_r = sum_r / (float)voices_active;
}

// Process formant filters
inline void process_formants(Voice *v, const M1Patch *patch, float *in_l, float *in_r) {
    // Brightness control (scales all formants)
    float bright_scale = 0.5f + s_brightness * 1.5f;
    
    // Formant 1 (fundamental)
    float f1_freq = patch->formant1_freq * bright_scale;
    float f1_q = patch->formant1_q * (1.f + s_resonance * 2.f);
    
    *in_l = process_peak_filter(*in_l, f1_freq, f1_q, &v->formant1_z1, &v->formant1_z2);
    *in_r = process_peak_filter(*in_r, f1_freq, f1_q, &v->formant1_z1, &v->formant1_z2);
    
    // Formant 2 (brightness)
    float f2_freq = patch->formant2_freq * bright_scale;
    float f2_q = patch->formant2_q * (1.f + s_resonance * 2.f);
    
    *in_l = process_peak_filter(*in_l, f2_freq, f2_q, &v->formant2_z1, &v->formant2_z2);
    *in_r = process_peak_filter(*in_r, f2_freq, f2_q, &v->formant2_z1, &v->formant2_z2);
    
    // Formant 3 (brilliance)
    float f3_freq = patch->formant3_freq * bright_scale;
    float f3_q = patch->formant3_q * (1.f + s_resonance * 2.f);
    
    *in_l = process_peak_filter(*in_l, f3_freq, f3_q, &v->formant3_z1, &v->formant3_z2);
    *in_r = process_peak_filter(*in_r, f3_freq, f3_q, &v->formant3_z1, &v->formant3_z2);
}

// Envelope
inline float update_envelope(Voice *v, const M1Patch *patch) {
    float t_sec = (float)v->env_counter / 48000.f;
    
    float attack = patch->attack * (0.5f + s_attack_mod * 1.5f);
    float release = patch->release * (0.5f + s_release_mod * 1.5f);
    
    switch (v->env_stage) {
        case 0: // Attack
            v->amp_env = clipminmaxf(0.f, t_sec / attack, 1.f);
            if (v->amp_env >= 0.99f) {
                v->env_stage = 1;
                v->env_counter = 0;
            }
            break;
        
        case 1: // Decay
            v->amp_env = patch->sustain + (1.f - patch->sustain) * fx_pow2f(-t_sec / patch->decay * 5.f);  // ✅ Fixed: fx_pow2f (fastpow2f doesn't exist!)
            if (t_sec >= patch->decay) {
                v->env_stage = 2;
                v->env_counter = 0;
            }
            break;
        
        case 2: // Sustain
            v->amp_env = patch->sustain;
            break;
        
        case 3: // Release
            v->amp_env = patch->sustain * fx_pow2f(-t_sec / release * 5.f);  // ✅ Fixed: fx_pow2f (fastpow2f doesn't exist!)
            if (v->amp_env < 0.001f) {
                v->active = false;
            }
            break;
    }
    
    v->env_counter++;
    return v->amp_env;
}

// Vibrato with delay and fade-in
inline float update_vibrato(Voice *v, const M1Patch *patch) {
    float t_sec = (float)v->vibrato_counter / 48000.f;
    
    // Delay
    if (t_sec < patch->vibrato_delay) {
        v->vibrato_fade = 0.f;
    } else {
        // Fade in
        float fade_time = 0.5f;
        float fade_t = (t_sec - patch->vibrato_delay) / fade_time;
        v->vibrato_fade = clipminmaxf(0.f, fade_t, 1.f);
    }
    
    // LFO (osc_sinf expects phase in [0,1] range)
    v->vibrato_phase += patch->vibrato_rate / 48000.f;
    if (v->vibrato_phase >= 1.f) v->vibrato_phase -= 1.f;
    if (v->vibrato_phase < 0.f) v->vibrato_phase += 1.f;
    
    float lfo = osc_sinf(v->vibrato_phase);
    
    v->vibrato_counter++;
    
    return lfo * patch->vibrato_depth * v->vibrato_fade * s_vibrato_amount;
}

// Chorus
inline void chorus_process(float *in_l, float *in_r) {
    s_chorus_buffer_l[s_chorus_write] = *in_l;
    s_chorus_buffer_r[s_chorus_write] = *in_r;
    
    s_chorus_lfo += 0.5f / 48000.f;
    if (s_chorus_lfo >= 1.f) s_chorus_lfo -= 1.f;
    if (s_chorus_lfo < 0.f) s_chorus_lfo += 1.f;
    
    float lfo = osc_sinf(s_chorus_lfo);
    
    float delay_samples = 1200.f + lfo * 600.f;
    uint32_t delay_int = (uint32_t)delay_samples;
    float delay_frac = delay_samples - (float)delay_int;
    
    uint32_t read_0 = (s_chorus_write + CHORUS_BUFFER_SIZE - delay_int) % CHORUS_BUFFER_SIZE;
    uint32_t read_1 = (read_0 + CHORUS_BUFFER_SIZE - 1) % CHORUS_BUFFER_SIZE;
    
    float delayed_l = s_chorus_buffer_l[read_0] * (1.f - delay_frac) + s_chorus_buffer_l[read_1] * delay_frac;
    float delayed_r = s_chorus_buffer_r[read_0] * (1.f - delay_frac) + s_chorus_buffer_r[read_1] * delay_frac;
    
    *in_l = *in_l * 0.7f + delayed_l * 0.3f;
    *in_r = *in_r * 0.7f + delayed_r * 0.3f;
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc)
{
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 1) return k_unit_err_geometry;

    s_context = static_cast<const unit_runtime_osc_context_t *>(desc->hooks.runtime_context);

    // Init voices
    for (int v = 0; v < MAX_VOICES; v++) {
        s_voices[v].active = false;
        
        for (int i = 0; i < ENSEMBLE_VOICES; i++) {
            s_voices[v].ensemble_phases_saw[i] = 0.f;
            s_voices[v].ensemble_phases_pulse[i] = 0.f;
        }
        
        s_voices[v].formant1_z1 = s_voices[v].formant1_z2 = 0.f;
        s_voices[v].formant2_z1 = s_voices[v].formant2_z2 = 0.f;
        s_voices[v].formant3_z1 = s_voices[v].formant3_z2 = 0.f;
        
        s_voices[v].amp_env = 0.f;
        s_voices[v].env_stage = 0;
        s_voices[v].env_counter = 0;
        
        s_voices[v].vibrato_phase = 0.f;
        s_voices[v].vibrato_fade = 0.f;
        s_voices[v].vibrato_counter = 0;
        
        s_voices[v].breath_level = 1.f;
    }
    
    // Init chorus
    for (int i = 0; i < CHORUS_BUFFER_SIZE; i++) {
        s_chorus_buffer_l[i] = 0.f;
        s_chorus_buffer_r[i] = 0.f;
    }
    s_chorus_write = 0;
    s_chorus_lfo = 0.f;
    
    // Init parameters
    s_brightness = 0.6f;
    s_resonance = 0.75f;
    s_detune_amount = 0.5f;
    s_ensemble_amount = 0.3f;
    s_vibrato_amount = 0.4f;
    s_breath_amount = 0.25f;
    s_attack_mod = 0.65f;
    s_release_mod = 0.8f;
    s_patch_select = 0;
    s_voice_count = 1;
    
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
    
    const M1Patch *patch = &s_patches[s_patch_select];
    
    for (uint32_t f = 0; f < frames; f++) {
        float sig_l = 0.f;
        float sig_r = 0.f;
        int active_count = 0;
        
        for (int v = 0; v < MAX_VOICES; v++) {
            Voice *voice = &s_voices[v];
            if (!voice->active) continue;
            
            // Vibrato modulation
            float vib = update_vibrato(voice, patch);
            
            // Calculate pitch with vibrato
            float pitch_mod = vib * 12.f;  // ±12 semitones max
            float w0 = osc_w0f_for_note(voice->note + (int8_t)pitch_mod, mod);
            
            // Generate ensemble
            float ens_l, ens_r;
            generate_ensemble(voice, w0, patch, &ens_l, &ens_r);
            
            // Formant filtering (THE BRASS CHARACTER!)
            process_formants(voice, patch, &ens_l, &ens_r);
            
            // Envelope
            float env = update_envelope(voice, patch);
            
            // Velocity scaling
            float vel_scale = (float)voice->velocity / 127.f;
            vel_scale = 0.5f + vel_scale * 0.5f;
            
            // ✅ FIX: Breath controller should NOT dampen the main signal!
            // Apply envelope and velocity (breath_level removed - it was causing silence!)
            ens_l *= env * vel_scale;
            ens_r *= env * vel_scale;
            
            sig_l += ens_l;
            sig_r += ens_r;
            active_count++;
        }
        
        // Normalize
        if (active_count > 0) {
            sig_l /= (float)active_count;
            sig_r /= (float)active_count;
        }
        
        // Chorus (ensemble effect)
        chorus_process(&sig_l, &sig_r);
        
        // Mono mix
        float mono = (sig_l + sig_r) * 0.5f;
        
        // DC offset removal
        static float dc_z = 0.f;
        dc_z = dc_z * 0.995f + mono;
        mono = mono - dc_z;
        
        // OUTPUT with increased gain (3.0× for better volume match with other oscillators)
        out[f] = clipminmaxf(-1.f, mono * 3.0f, 1.f);
        
        s_chorus_write = (s_chorus_write + 1) % CHORUS_BUFFER_SIZE;
        s_sample_counter++;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_brightness = valf; break;
        case 1: s_resonance = valf; break;
        case 2: s_detune_amount = valf; break;
        case 3: s_ensemble_amount = valf; break;
        case 4: s_vibrato_amount = valf; break;
        case 5: s_breath_amount = valf; break;
        case 6: s_attack_mod = valf; break;
        case 7: s_release_mod = valf; break;
        case 8: s_patch_select = value; break;
        case 9: s_voice_count = value; break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0: return (int32_t)(s_brightness * 1023.f);
        case 1: return (int32_t)(s_resonance * 1023.f);
        case 2: return (int32_t)(s_detune_amount * 1023.f);
        case 3: return (int32_t)(s_ensemble_amount * 1023.f);
        case 4: return (int32_t)(s_vibrato_amount * 1023.f);
        case 5: return (int32_t)(s_breath_amount * 1023.f);
        case 6: return (int32_t)(s_attack_mod * 1023.f);
        case 7: return (int32_t)(s_release_mod * 1023.f);
        case 8: return s_patch_select;
        case 9: return s_voice_count;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
    if (id == 8) {
        if (value >= 0 && value < 8) return s_patches[value].name;
    }
    if (id == 9) {
        static const char *voice_names[] = {"MONO", "UNI2", "UNI4", "UNI8"};
        if (value >= 0 && value < 4) return voice_names[value];
    }
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
    
    if (free_voice == -1) free_voice = 0;  // Steal oldest
    
    Voice *voice = &s_voices[free_voice];
    voice->active = true;
    voice->note = note;
    voice->velocity = velo;
    
    // Reset envelopes
    voice->env_counter = 0;
    voice->env_stage = 0;
    voice->amp_env = 0.f;
    
    // Reset vibrato
    voice->vibrato_counter = 0;
    voice->vibrato_phase = 0.f;
    voice->vibrato_fade = 0.f;
    
    // Reset phases
    for (int i = 0; i < ENSEMBLE_VOICES; i++) {
        voice->ensemble_phases_saw[i] = 0.f;
        voice->ensemble_phases_pulse[i] = 0.f;
    }
    
    // Reset filters
    voice->formant1_z1 = voice->formant1_z2 = 0.f;
    voice->formant2_z1 = voice->formant2_z2 = 0.f;
    voice->formant3_z1 = voice->formant3_z2 = 0.f;
    
    // ✅ FIX: Breath level always 1.0 (not used for main signal anymore)
    voice->breath_level = 1.f;
}

__unit_callback void unit_note_off(uint8_t note)
{
    for (int v = 0; v < MAX_VOICES; v++) {
        if (s_voices[v].active && s_voices[v].note == note) {
            s_voices[v].env_stage = 3;  // Start release
            s_voices[v].env_counter = 0;
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

