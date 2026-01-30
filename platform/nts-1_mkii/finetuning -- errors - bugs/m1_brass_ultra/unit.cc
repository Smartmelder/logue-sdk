/*
    M1 BRASS ULTRA - STEREO Implementation
    
    CRITICAL FIXES:
    1. STEREO geometry: 2 input / 2 output (interleaved)
    2. Proper L/R separation with ensemble
    3. Correct pitch extraction
    4. Safe Q-factor (max 2.0)
    5. DC blocker per channel
    6. Phase reset on note_on
*/

#include "unit_osc.h"
#include "osc_api.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "fx_api.h"

// ═══════════════════════════════════════════════════════════════════════════
// CONSTANTS
// ═══════════════════════════════════════════════════════════════════════════

#define MAX_VOICES 3
#define MAX_ENSEMBLE 8
#define SAMPLERATE 48000.f
// PI is already defined in CMSIS arm_math.h

// ═══════════════════════════════════════════════════════════════════════════
// M1 PATCH STRUCTURE
// ═══════════════════════════════════════════════════════════════════════════

struct M1Patch {
    float saw_level;
    float pulse_level;
    float pulse_width;
    
    // 4 formant bands
    float f1_freq, f1_q;
    float f2_freq, f2_q;
    float f3_freq, f3_q;
    float f4_freq, f4_q;
    
    float attack;
    float decay;
    float sustain;
    float release;
    
    float vibrato_rate;
    float vibrato_depth;
    float vibrato_delay;
    
    const char* name;
};

// 12 Authentic M1 Patches
static const M1Patch s_patches[12] = {
    // BRASS 1
    {0.8f, 0.3f, 0.5f,
     500.f, 1.5f, 1200.f, 2.0f, 2800.f, 1.2f, 5000.f, 0.8f,
     0.03f, 0.1f, 0.7f, 0.3f,
     5.5f, 0.015f, 0.3f,
     "BRASS1"},
    
    // BRASS 2
    {0.9f, 0.2f, 0.4f,
     600.f, 1.8f, 1400.f, 2.2f, 3000.f, 1.5f, 5500.f, 1.0f,
     0.02f, 0.08f, 0.8f, 0.2f,
     6.0f, 0.02f, 0.4f,
     "BRASS2"},
    
    // STRING 1
    {0.6f, 0.5f, 0.5f,
     400.f, 1.2f, 900.f, 1.5f, 2000.f, 1.0f, 4000.f, 0.6f,
     0.08f, 0.15f, 0.85f, 0.5f,
     4.5f, 0.01f, 0.5f,
     "STRING1"},
    
    // STRING 2
    {0.5f, 0.7f, 0.55f,
     350.f, 1.0f, 700.f, 1.3f, 1800.f, 0.9f, 3500.f, 0.5f,
     0.06f, 0.12f, 0.9f, 0.4f,
     4.0f, 0.008f, 0.6f,
     "STRING2"},
    
    // CHOIR
    {0.3f, 0.8f, 0.6f,
     450.f, 1.5f, 1000.f, 1.8f, 2500.f, 1.2f, 4500.f, 0.8f,
     0.1f, 0.2f, 0.8f, 0.6f,
     3.5f, 0.012f, 0.7f,
     "CHOIR"},
    
    // SAX
    {0.85f, 0.25f, 0.45f,
     550.f, 2.0f, 1500.f, 2.5f, 2800.f, 1.5f, 5200.f, 1.0f,
     0.015f, 0.05f, 0.75f, 0.25f,
     5.0f, 0.025f, 0.2f,
     "SAX"},
    
    // FLUTE
    {0.2f, 0.4f, 0.3f,
     700.f, 0.8f, 1600.f, 1.0f, 3500.f, 0.7f, 6000.f, 0.5f,
     0.01f, 0.04f, 0.6f, 0.15f,
     4.5f, 0.018f, 0.3f,
     "FLUTE"},
    
    // HORN
    {0.75f, 0.35f, 0.5f,
     450.f, 1.7f, 1000.f, 2.0f, 2200.f, 1.3f, 4200.f, 0.8f,
     0.03f, 0.09f, 0.7f, 0.35f,
     4.8f, 0.015f, 0.5f,
     "HORN"},
    
    // OBOE
    {0.8f, 0.4f, 0.35f,
     600.f, 2.0f, 1400.f, 2.5f, 2800.f, 1.8f, 5500.f, 1.2f,
     0.02f, 0.07f, 0.72f, 0.28f,
     5.5f, 0.02f, 0.35f,
     "OBOE"},
    
    // CLARINET
    {0.3f, 0.85f, 0.25f,
     500.f, 1.8f, 1200.f, 2.2f, 2400.f, 1.5f, 4800.f, 1.0f,
     0.018f, 0.06f, 0.78f, 0.22f,
     5.2f, 0.017f, 0.38f,
     "CLARIN"},
    
    // BRASS 3
    {0.7f, 0.5f, 0.5f,
     520.f, 1.6f, 1250.f, 2.1f, 2700.f, 1.4f, 4800.f, 0.9f,
     0.025f, 0.09f, 0.75f, 0.32f,
     5.3f, 0.018f, 0.42f,
     "BRASS3"},
    
    // STRING 3
    {0.55f, 0.65f, 0.52f,
     380.f, 1.1f, 850.f, 1.4f, 1900.f, 0.95f, 3800.f, 0.55f,
     0.07f, 0.13f, 0.88f, 0.45f,
     4.2f, 0.009f, 0.58f,
     "STRING3"}
};

// ═══════════════════════════════════════════════════════════════════════════
// VOICE STRUCTURE
// ═══════════════════════════════════════════════════════════════════════════

struct Voice {
    bool active;
    uint8_t note;
    uint8_t velocity;
    
    // Oscillators (per ensemble voice)
    float phases_saw[MAX_ENSEMBLE];
    float phases_pulse[MAX_ENSEMBLE];
    
    // Filters (SEPARATE L/R for true stereo)
    float f1_z1_l, f1_z2_l, f1_z1_r, f1_z2_r;
    float f2_z1_l, f2_z2_l, f2_z1_r, f2_z2_r;
    float f3_z1_l, f3_z2_l, f3_z1_r, f3_z2_r;
    float f4_z1_l, f4_z2_l, f4_z1_r, f4_z2_r;
    
    // DC blockers (per channel)
    float dc_z_l, dc_z_r;
    
    // Envelopes
    float amp_env;
    uint8_t env_stage;
    uint32_t env_counter;
    
    // Vibrato
    float vib_phase;
    float vib_fade;
    uint32_t vib_counter;
};

// ═══════════════════════════════════════════════════════════════════════════
// GLOBAL STATE
// ═══════════════════════════════════════════════════════════════════════════

static const unit_runtime_osc_context_t *s_context;

static Voice s_voices[MAX_VOICES];

// Parameters
static float s_brightness;
static float s_resonance;
static float s_detune;
static float s_ensemble;
static float s_vibrato;
static float s_attack;
static float s_release;
static uint8_t s_voice_count;
static uint8_t s_patch_num;
static float s_width;

// Ensemble detuning (cents) and panning
static const float ENSEMBLE_DETUNE[MAX_ENSEMBLE] = {
    0.f, -8.f, 8.f, -5.f, 5.f, -3.f, 3.f, -1.5f
};

static const float ENSEMBLE_PAN[MAX_ENSEMBLE] = {
    0.f, -0.7f, 0.7f, -0.4f, 0.4f, -0.2f, 0.2f, -0.1f
};

// ═══════════════════════════════════════════════════════════════════════════
// HELPERS
// ═══════════════════════════════════════════════════════════════════════════

inline float safe_clip(float x) {
    if (x > 1.f) return 1.f;
    if (x < -1.f) return -1.f;
    // Check for NaN/Inf (NaN != NaN is always true)
    if (x != x) return 0.f;
    return x;
}

// PolyBLEP anti-aliasing
inline float polyblep(float t, float dt) {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.f;
    } else if (t > 1.f - dt) {
        t = (t - 1.f) / dt;
        return t * t + t + t + 1.f;
    }
    return 0.f;
}

// Biquad peak filter (formant)
inline float peak_filter(float input, float freq, float q, 
                        float *z1, float *z2) {
    // Clamp frequency
    freq = clipminmaxf(100.f, freq, 18000.f);
    
    // Clamp Q (CRITICAL: max 2.0 for stability!)
    q = clipminmaxf(0.5f, q, 2.0f);
    
    // Calculate coefficients
    float w0 = 2.f * PI * freq / SAMPLERATE;
    if (w0 > PI * 0.95f) w0 = PI * 0.95f;
    
    float alpha = fx_sinf(w0 / (2.f * PI)) / (2.f * q);
    alpha = clipminmaxf(0.001f, alpha, 0.9f);
    
    float cos_w0 = fx_cosf(w0 / (2.f * PI));
    
    // Coefficients
    float b0 = alpha;
    float b2 = -alpha;
    float a0 = 1.f + alpha;
    float a1 = -2.f * cos_w0;
    float a2 = 1.f - alpha;
    
    // Normalize
    b0 /= a0;
    b2 /= a0;
    a1 /= a0;
    a2 /= a0;
    
    // Process
    float output = b0 * input + b2 * (*z2) - a1 * (*z1) - a2 * (*z2);
    
    // Denormal protection
    if (si_fabsf(*z1) < 1e-20f) *z1 = 0.f;
    if (si_fabsf(*z2) < 1e-20f) *z2 = 0.f;
    
    // Safety clip
    output = clipminmaxf(-4.f, output, 4.f);
    
    // Update states
    *z2 = *z1;
    *z1 = output;
    
    return output;
}

// ═══════════════════════════════════════════════════════════════════════════
// VOICE PROCESSING
// ═══════════════════════════════════════════════════════════════════════════

inline void generate_voice_stereo(Voice *v, float base_freq, const M1Patch *patch, 
                                   float *out_l, float *out_r) {
    float sum_l = 0.f;
    float sum_r = 0.f;
    
    int voices = s_voice_count;
    if (voices < 1) voices = 1;
    if (voices > MAX_ENSEMBLE) voices = MAX_ENSEMBLE;
    
    for (int i = 0; i < voices; i++) {
        // Detune
        float detune_cents = ENSEMBLE_DETUNE[i] * s_detune;
        float freq = base_freq * fx_pow2f(detune_cents / 1200.f);
        
        // Clamp
        if (freq > 20000.f) freq = 20000.f;
        if (freq < 20.f) freq = 20.f;
        
        float w0 = freq / SAMPLERATE;
        if (w0 > 0.49f) w0 = 0.49f;
        
        // Generate SAW
        float saw = 2.f * v->phases_saw[i] - 1.f;
        saw -= polyblep(v->phases_saw[i], w0);
        
        // Generate PULSE
        float pw = patch->pulse_width;
        float pulse = (v->phases_pulse[i] < pw) ? 1.f : -1.f;
        pulse += polyblep(v->phases_pulse[i], w0);
        
        float phase2 = v->phases_pulse[i] + 1.f - pw;
        phase2 -= (uint32_t)phase2;
        if (phase2 < 0.f) phase2 += 1.f;
        if (phase2 >= 1.f) phase2 -= 1.f;
        pulse -= polyblep(phase2, w0);
        
        // Mix oscillators
        float mixed = saw * patch->saw_level + pulse * patch->pulse_level;
        
        // TRUE STEREO PANNING (not mid/side)
        float pan = ENSEMBLE_PAN[i] * s_ensemble;
        float gain_l = (1.f - pan) * 0.5f;
        float gain_r = (1.f + pan) * 0.5f;
        
        sum_l += mixed * gain_l;
        sum_r += mixed * gain_r;
        
        // Advance phases
        v->phases_saw[i] += w0;
        v->phases_pulse[i] += w0;
        
        // Wrap phases
        while (v->phases_saw[i] >= 1.f) v->phases_saw[i] -= 1.f;
        while (v->phases_pulse[i] >= 1.f) v->phases_pulse[i] -= 1.f;
    }
    
    // Normalize
    float norm = 1.f / (float)voices;
    *out_l = sum_l * norm;
    *out_r = sum_r * norm;
}

inline void process_formants_stereo(Voice *v, const M1Patch *patch, 
                                   float *l, float *r) {
    // Brightness scaling
    float bright = 0.5f + s_brightness * 1.5f;
    
    // Q scaling (safe!)
    float q_mult = 1.f + s_resonance * 0.5f;
    
    // Band 1
    float f1 = patch->f1_freq * bright;
    float q1 = patch->f1_q * q_mult;
    *l = peak_filter(*l, f1, q1, &v->f1_z1_l, &v->f1_z2_l);
    *r = peak_filter(*r, f1, q1, &v->f1_z1_r, &v->f1_z2_r);
    
    // Band 2
    float f2 = patch->f2_freq * bright;
    float q2 = patch->f2_q * q_mult;
    *l = peak_filter(*l, f2, q2, &v->f2_z1_l, &v->f2_z2_l);
    *r = peak_filter(*r, f2, q2, &v->f2_z1_r, &v->f2_z2_r);
    
    // Band 3
    float f3 = patch->f3_freq * bright;
    float q3 = patch->f3_q * q_mult;
    *l = peak_filter(*l, f3, q3, &v->f3_z1_l, &v->f3_z2_l);
    *r = peak_filter(*r, f3, q3, &v->f3_z1_r, &v->f3_z2_r);
    
    // Band 4
    float f4 = patch->f4_freq * bright;
    float q4 = patch->f4_q * q_mult;
    *l = peak_filter(*l, f4, q4, &v->f4_z1_l, &v->f4_z2_l);
    *r = peak_filter(*r, f4, q4, &v->f4_z1_r, &v->f4_z2_r);
}

inline float update_envelope(Voice *v, const M1Patch *patch) {
    float t = (float)v->env_counter / SAMPLERATE;
    
    float attack = patch->attack * (0.5f + s_attack * 1.5f);
    attack = clipminmaxf(0.001f, attack, 5.f);
    
    float release = patch->release * (0.5f + s_release * 1.5f);
    release = clipminmaxf(0.001f, release, 5.f);
    
    switch (v->env_stage) {
        case 0:  // Attack
            v->amp_env = clipminmaxf(0.f, t / attack, 1.f);
            if (v->amp_env >= 0.99f) {
                v->env_stage = 1;
                v->env_counter = 0;
            }
            break;
        
        case 1:  // Decay
            v->amp_env = patch->sustain + (1.f - patch->sustain) * 
                        fx_pow2f(-t / patch->decay * 5.f);
            if (t >= patch->decay) {
                v->env_stage = 2;
                v->env_counter = 0;
            }
            break;
        
        case 2:  // Sustain
            v->amp_env = patch->sustain;
            break;
        
        case 3:  // Release
            v->amp_env = patch->sustain * fx_pow2f(-t / release * 5.f);
            if (v->amp_env < 0.001f) {
                v->active = false;
                v->amp_env = 0.f;
            }
            break;
    }
    
    v->env_counter++;
    return v->amp_env;
}

inline float update_vibrato(Voice *v, const M1Patch *patch) {
    float t = (float)v->vib_counter / SAMPLERATE;
    
    if (t < patch->vibrato_delay) {
        v->vib_fade = 0.f;
    } else {
        float fade_t = (t - patch->vibrato_delay) / 0.5f;
        v->vib_fade = clipminmaxf(0.f, fade_t, 1.f);
    }
    
    v->vib_phase += patch->vibrato_rate / SAMPLERATE;
    while (v->vib_phase >= 1.f) v->vib_phase -= 1.f;
    
        float lfo = fx_sinf(v->vib_phase);
    
    v->vib_counter++;
    
    return lfo * patch->vibrato_depth * v->vib_fade * s_vibrato;
}

// ═══════════════════════════════════════════════════════════════════════════
// UNIT CALLBACKS
// ═══════════════════════════════════════════════════════════════════════════

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    
    // CRITICAL: STEREO oscillator geometry
    // Input: 2 channels (can use audio input)
    // Output: 2 channels (STEREO!)
    if (desc->input_channels != 2 || desc->output_channels != 2)
        return k_unit_err_geometry;
    
    // Get context
    s_context = static_cast<const unit_runtime_osc_context_t *>(desc->hooks.runtime_context);
    
    // Init voices
    for (int i = 0; i < MAX_VOICES; i++) {
        s_voices[i].active = false;
        s_voices[i].dc_z_l = 0.f;
        s_voices[i].dc_z_r = 0.f;
        
        for (int j = 0; j < MAX_ENSEMBLE; j++) {
            s_voices[i].phases_saw[j] = 0.f;
            s_voices[i].phases_pulse[j] = 0.f;
        }
    }
    
    // Init parameters
    s_brightness = 0.6f;
    s_resonance = 0.5f;
    s_detune = 0.3f;
    s_ensemble = 0.4f;
    s_vibrato = 0.4f;
    s_attack = 0.3f;
    s_release = 0.6f;
    s_voice_count = 4;
    s_patch_num = 0;
    s_width = 0.5f;
    
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
    
    const M1Patch *patch = &s_patches[s_patch_num];
    
    // Extract pitch (CORRECT METHOD!)
    uint8_t note = (s_context->pitch >> 8) & 0xFF;
    uint8_t mod = s_context->pitch & 0xFF;
    
    // CRITICAL: Output is INTERLEAVED stereo (L, R, L, R...)
    for (uint32_t f = 0; f < frames; f++) {
        float sum_l = 0.f;
        float sum_r = 0.f;
        int active = 0;
        
        for (int v = 0; v < MAX_VOICES; v++) {
            Voice *voice = &s_voices[v];
            if (!voice->active) continue;
            
            // Vibrato
            float vib = update_vibrato(voice, patch);
            
            // Calculate frequency
            float final_note = (float)voice->note + ((float)mod / 255.f) + 
                              vib * 12.f;
            final_note = clipminmaxf(0.f, final_note, 127.f);
            
            uint8_t n_int = (uint8_t)final_note;
            uint8_t n_frac = (uint8_t)((final_note - (float)n_int) * 255.f);
            
            float w0 = osc_w0f_for_note(n_int, n_frac);
            float freq = w0 * SAMPLERATE;
            
            // Generate STEREO
            float voice_l, voice_r;
            generate_voice_stereo(voice, freq, patch, &voice_l, &voice_r);
            
            // Formants STEREO
            process_formants_stereo(voice, patch, &voice_l, &voice_r);
            
            // Safety (NaN check: NaN != NaN is always true)
            if (voice_l != voice_l) voice_l = 0.f;
            if (voice_r != voice_r) voice_r = 0.f;
            
            // Envelope
            float env = update_envelope(voice, patch);
            
            // Velocity
            float vel = 0.5f + ((float)voice->velocity / 127.f) * 0.5f;
            
            float gain = env * vel;
            
            sum_l += voice_l * gain;
            sum_r += voice_r * gain;
            
            active++;
        }
        
        if (active > 0) {
            float norm = 1.f / sqrtf((float)active);
            sum_l *= norm;
            sum_r *= norm;
        }
        
        // Stereo width control
        float mid = (sum_l + sum_r) * 0.5f;
        float side = (sum_l - sum_r) * 0.5f * s_width;
        sum_l = mid + side;
        sum_r = mid - side;
        
        // DC blocker per channel
        static float dc_z_l = 0.f;
        static float dc_z_r = 0.f;
        
        float dc_out_l = sum_l - dc_z_l;
        float dc_out_r = sum_r - dc_z_r;
        
        dc_z_l = sum_l * 0.995f + dc_z_l * 0.005f;
        dc_z_r = sum_r * 0.995f + dc_z_r * 0.005f;
        
        sum_l = dc_out_l;
        sum_r = dc_out_r;
        
        // Saturation
        sum_l = fastertanhf(sum_l * 1.5f);
        sum_r = fastertanhf(sum_r * 1.5f);
        
        // CRITICAL: 2.5x gain for NTS-1 mkII
        sum_l *= 2.5f;
        sum_r *= 2.5f;
        
        // Safety clip
        sum_l = safe_clip(sum_l);
        sum_r = safe_clip(sum_r);
        
        // CRITICAL: INTERLEAVED stereo output (L, R, L, R...)
        out[f * 2] = sum_l;
        out[f * 2 + 1] = sum_r;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, 
                          unit_header.params[id].max);
    float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_brightness = valf; break;
        case 1: s_resonance = valf; break;
        case 2: s_detune = valf; break;
        case 3: s_ensemble = valf; break;
        case 4: s_vibrato = valf; break;
        case 5: s_attack = valf; break;
        case 6: s_release = valf; break;
        case 7: s_voice_count = value; break;
        case 8: s_patch_num = value; break;
        case 9: s_width = valf; break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return (int32_t)(s_brightness * 1023.f);
        case 1: return (int32_t)(s_resonance * 1023.f);
        case 2: return (int32_t)(s_detune * 1023.f);
        case 3: return (int32_t)(s_ensemble * 1023.f);
        case 4: return (int32_t)(s_vibrato * 1023.f);
        case 5: return (int32_t)(s_attack * 1023.f);
        case 6: return (int32_t)(s_release * 1023.f);
        case 7: return s_voice_count;
        case 8: return s_patch_num;
        case 9: return (int32_t)(s_width * 1023.f);
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    if (id == 8 && value >= 0 && value < 12) {
        return s_patches[value].name;
    }
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    (void)tempo;
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
    int free = -1;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!s_voices[i].active) {
            free = i;
            break;
        }
    }
    
    if (free == -1) free = 0;
    
    Voice *v = &s_voices[free];
    
    v->active = true;
    v->note = note;
    v->velocity = velocity;
    
    // CRITICAL: Reset ALL states
    for (int i = 0; i < MAX_ENSEMBLE; i++) {
        v->phases_saw[i] = 0.f;
        v->phases_pulse[i] = 0.f;
    }
    
    // Reset filters (L/R)
    v->f1_z1_l = v->f1_z2_l = 0.f;
    v->f1_z1_r = v->f1_z2_r = 0.f;
    v->f2_z1_l = v->f2_z2_l = 0.f;
    v->f2_z1_r = v->f2_z2_r = 0.f;
    v->f3_z1_l = v->f3_z2_l = 0.f;
    v->f3_z1_r = v->f3_z2_r = 0.f;
    v->f4_z1_l = v->f4_z2_l = 0.f;
    v->f4_z1_r = v->f4_z2_r = 0.f;
    
    v->dc_z_l = v->dc_z_r = 0.f;
    
    // Reset envelopes
    v->amp_env = 0.f;
    v->env_stage = 0;
    v->env_counter = 0;
    
    // Reset vibrato
    v->vib_phase = 0.f;
    v->vib_fade = 0.f;
    v->vib_counter = 0;
}

__unit_callback void unit_note_off(uint8_t note) {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (s_voices[i].active && s_voices[i].note == note) {
            if (s_voices[i].env_stage < 3) {
                s_voices[i].env_stage = 3;
                s_voices[i].env_counter = 0;
            }
        }
    }
}

__unit_callback void unit_all_note_off() {
    for (int i = 0; i < MAX_VOICES; i++) {
        s_voices[i].active = false;
    }
}
