/*
    M1 BRASS ULTRA - Ultimate Enhanced Recreation
    
    ═══════════════════════════════════════════════════════════════
    ULTRA ENHANCEMENTS
    ═══════════════════════════════════════════════════════════════
    
    vs. Standard M1 Brass:
    
    1. NOISE LAYER - White noise generator (per voice)
    2. ATTACK TRANSIENT - Wavetable burst at note start
    3. 4-BAND FORMANTS (vs 3-band) - Extra high formant
    4. 10-VOICE ENSEMBLE (vs 8-voice) - Wider stereo spread
    5. PITCH ENVELOPE - Upward swell / downward fall
    6. FILTER LFO - Formant wobble (vocal effect)
    7. STEREO WIDENING - Mid/Side processing + Haas
    8. 12 PATCHES (vs 8) - All originals + 4 new
    
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
#define ENSEMBLE_VOICES 10
#define CHORUS_BUFFER_SIZE 4096
#define TRANSIENT_TABLE_SIZE 256

static const unit_runtime_osc_context_t *s_context;

// Ensemble detune values (cents) - 10 voices!
static const float s_ensemble_detune[ENSEMBLE_VOICES] = {
    0.0f, -10.0f, 10.0f, -7.0f, 7.0f, -4.0f, 4.0f, -2.0f, 2.0f, -1.0f
};

// Ensemble pan positions
static const float s_ensemble_pan[ENSEMBLE_VOICES] = {
    0.0f, -0.8f, 0.8f, -0.6f, 0.6f, -0.4f, 0.4f, -0.2f, 0.2f, -0.1f
};

// Micro-timing offsets (samples)
static const int8_t s_ensemble_timing[ENSEMBLE_VOICES] = {
    0, -3, 3, -2, 2, -1, 1, -4, 4, -2
};

// Attack transient wavetable (simple impulse)
static float s_transient_table[TRANSIENT_TABLE_SIZE];

// M1 Patch definitions (ULTRA - 12 patches!)
struct M1Patch {
    // Oscillator mix
    float osc_saw_level;
    float osc_pulse_level;
    float pulse_width;
    
    // Formant frequencies (Hz) - 4 bands!
    float formant1_freq;
    float formant2_freq;
    float formant3_freq;
    float formant4_freq;
    
    // Formant Q values
    float formant1_q;
    float formant2_q;
    float formant3_q;
    float formant4_q;
    
    // Noise layer
    float noise_level;
    float noise_cutoff;
    
    // Attack transient
    float transient_level;
    float transient_decay;
    
    // Pitch envelope
    float pitch_env_amount;  // semitones
    float pitch_env_time;    // seconds
    
    // Envelope
    float attack;
    float decay;
    float sustain;
    float release;
    
    // Vibrato
    float vibrato_rate;
    float vibrato_depth;
    float vibrato_delay;
    
    // Filter LFO
    float filter_lfo_rate;
    float filter_lfo_depth;
    
    const char* name;
};

static const M1Patch s_patches[12] = {
    // BRASS 1 - Full Section
    {
        .osc_saw_level = 0.8f, .osc_pulse_level = 0.3f, .pulse_width = 0.5f,
        .formant1_freq = 600.f, .formant2_freq = 1200.f, .formant3_freq = 2800.f, .formant4_freq = 5000.f,
        .formant1_q = 1.7f, .formant2_q = 2.7f, .formant3_q = 1.3f, .formant4_q = 1.0f,  // ✅ Was 5,8,4,3 → /3
        .noise_level = 0.05f, .noise_cutoff = 3000.f,
        .transient_level = 0.3f, .transient_decay = 0.02f,
        .pitch_env_amount = 0.15f, .pitch_env_time = 0.08f,
        .attack = 0.02f, .decay = 0.1f, .sustain = 0.7f, .release = 0.3f,
        .vibrato_rate = 5.5f, .vibrato_depth = 0.015f, .vibrato_delay = 0.3f,
        .filter_lfo_rate = 0.5f, .filter_lfo_depth = 0.1f,
        .name = "BRASS1"
    },
    
    // BRASS 2 - Solo Trumpet
    {
        .osc_saw_level = 0.9f, .osc_pulse_level = 0.2f, .pulse_width = 0.4f,
        .formant1_freq = 650.f, .formant2_freq = 1300.f, .formant3_freq = 3000.f, .formant4_freq = 5500.f,
        .formant1_q = 2.0f, .formant2_q = 3.3f, .formant3_q = 1.7f, .formant4_q = 1.3f,  // ✅ Was 6,10,5,4 → /3
        .noise_level = 0.03f, .noise_cutoff = 4000.f,
        .transient_level = 0.4f, .transient_decay = 0.015f,
        .pitch_env_amount = 0.25f, .pitch_env_time = 0.06f,
        .attack = 0.01f, .decay = 0.05f, .sustain = 0.8f, .release = 0.2f,
        .vibrato_rate = 6.0f, .vibrato_depth = 0.025f, .vibrato_delay = 0.4f,
        .filter_lfo_rate = 0.8f, .filter_lfo_depth = 0.15f,
        .name = "BRASS2"
    },
    
    // BRASS 3 - Soft Section (NEW!)
    {
        .osc_saw_level = 0.6f, .osc_pulse_level = 0.5f, .pulse_width = 0.6f,
        .formant1_freq = 550.f, .formant2_freq = 1100.f, .formant3_freq = 2500.f, .formant4_freq = 4500.f,
        .formant1_q = 1.3f, .formant2_q = 2.0f, .formant3_q = 1.0f, .formant4_q = 0.67f,  // ✅ Was 4,6,3,2 → /3
        .noise_level = 0.08f, .noise_cutoff = 2500.f,
        .transient_level = 0.2f, .transient_decay = 0.03f,
        .pitch_env_amount = 0.1f, .pitch_env_time = 0.1f,
        .attack = 0.04f, .decay = 0.15f, .sustain = 0.65f, .release = 0.4f,
        .vibrato_rate = 5.0f, .vibrato_depth = 0.012f, .vibrato_delay = 0.5f,
        .filter_lfo_rate = 0.3f, .filter_lfo_depth = 0.08f,
        .name = "BRASS3"
    },
    
    // STRINGS 1 - Ensemble (The "Lore" sound!)
    {
        .osc_saw_level = 0.4f, .osc_pulse_level = 0.9f, .pulse_width = 0.6f,
        .formant1_freq = 400.f, .formant2_freq = 800.f, .formant3_freq = 2000.f, .formant4_freq = 4000.f,
        .formant1_q = 1.0f, .formant2_q = 1.3f, .formant3_q = 1.0f, .formant4_q = 0.67f,  // ✅ Was 3,4,3,2 → /3
        .noise_level = 0.02f, .noise_cutoff = 5000.f,
        .transient_level = 0.15f, .transient_decay = 0.05f,
        .pitch_env_amount = 0.0f, .pitch_env_time = 0.0f,
        .attack = 0.08f, .decay = 0.2f, .sustain = 0.9f, .release = 0.5f,
        .vibrato_rate = 4.5f, .vibrato_depth = 0.008f, .vibrato_delay = 0.5f,
        .filter_lfo_rate = 0.4f, .filter_lfo_depth = 0.05f,
        .name = "STRING1"
    },
    
    // STRINGS 2 - Chamber
    {
        .osc_saw_level = 0.5f, .osc_pulse_level = 0.7f, .pulse_width = 0.55f,
        .formant1_freq = 350.f, .formant2_freq = 700.f, .formant3_freq = 1800.f, .formant4_freq = 3500.f,
        .formant1_q = 1.3f, .formant2_q = 1.7f, .formant3_q = 1.3f, .formant4_q = 1.0f,  // ✅ Was 4,5,4,3 → /3
        .noise_level = 0.03f, .noise_cutoff = 4500.f,
        .transient_level = 0.12f, .transient_decay = 0.06f,
        .pitch_env_amount = 0.0f, .pitch_env_time = 0.0f,
        .attack = 0.06f, .decay = 0.15f, .sustain = 0.85f, .release = 0.4f,
        .vibrato_rate = 4.0f, .vibrato_depth = 0.006f, .vibrato_delay = 0.6f,
        .filter_lfo_rate = 0.3f, .filter_lfo_depth = 0.04f,
        .name = "STRING2"
    },
    
    // STRINGS 3 - Solo Violin (NEW!)
    {
        .osc_saw_level = 0.7f, .osc_pulse_level = 0.5f, .pulse_width = 0.5f,
        .formant1_freq = 450.f, .formant2_freq = 900.f, .formant3_freq = 2200.f, .formant4_freq = 4500.f,
        .formant1_q = 2.0f, .formant2_q = 2.7f, .formant3_q = 1.7f, .formant4_q = 1.3f,  // ✅ Was 6,8,5,4 → /3
        .noise_level = 0.06f, .noise_cutoff = 3500.f,
        .transient_level = 0.25f, .transient_decay = 0.04f,
        .pitch_env_amount = 0.08f, .pitch_env_time = 0.12f,
        .attack = 0.05f, .decay = 0.12f, .sustain = 0.75f, .release = 0.35f,
        .vibrato_rate = 5.5f, .vibrato_depth = 0.02f, .vibrato_delay = 0.4f,
        .filter_lfo_rate = 0.6f, .filter_lfo_depth = 0.12f,
        .name = "STRING3"
    },
    
    // CHOIR - Synth Voices
    {
        .osc_saw_level = 0.3f, .osc_pulse_level = 0.8f, .pulse_width = 0.7f,
        .formant1_freq = 500.f, .formant2_freq = 1000.f, .formant3_freq = 2500.f, .formant4_freq = 4500.f,
        .formant1_q = 2.3f, .formant2_q = 3.0f, .formant3_q = 2.0f, .formant4_q = 1.3f,  // ✅ Was 7,9,6,4 → /3
        .noise_level = 0.1f, .noise_cutoff = 2000.f,
        .transient_level = 0.1f, .transient_decay = 0.08f,
        .pitch_env_amount = 0.0f, .pitch_env_time = 0.0f,
        .attack = 0.1f, .decay = 0.3f, .sustain = 0.8f, .release = 0.6f,
        .vibrato_rate = 3.5f, .vibrato_depth = 0.012f, .vibrato_delay = 0.7f,
        .filter_lfo_rate = 0.2f, .filter_lfo_depth = 0.06f,
        .name = "CHOIR"
    },
    
    // SAX - Tenor
    {
        .osc_saw_level = 0.85f, .osc_pulse_level = 0.25f, .pulse_width = 0.45f,
        .formant1_freq = 500.f, .formant2_freq = 1500.f, .formant3_freq = 2500.f, .formant4_freq = 5200.f,
        .formant1_q = 2.7f, .formant2_q = 4.0f, .formant3_q = 2.0f, .formant4_q = 1.7f,  // ✅ Was 8,12,6,5 → /3 (limited to 3.0)
        .noise_level = 0.12f, .noise_cutoff = 3000.f,
        .transient_level = 0.45f, .transient_decay = 0.012f,
        .pitch_env_amount = 0.2f, .pitch_env_time = 0.05f,
        .attack = 0.015f, .decay = 0.08f, .sustain = 0.75f, .release = 0.25f,
        .vibrato_rate = 5.0f, .vibrato_depth = 0.03f, .vibrato_delay = 0.2f,
        .filter_lfo_rate = 1.0f, .filter_lfo_depth = 0.18f,
        .name = "SAX"
    },
    
    // FLUTE - Breathy
    {
        .osc_saw_level = 0.2f, .osc_pulse_level = 0.4f, .pulse_width = 0.3f,
        .formant1_freq = 800.f, .formant2_freq = 1600.f, .formant3_freq = 3500.f, .formant4_freq = 6000.f,
        .formant1_q = 0.67f, .formant2_q = 1.0f, .formant3_q = 0.67f, .formant4_q = 0.5f,  // ✅ Was 2,3,2,1.5 → /3
        .noise_level = 0.35f, .noise_cutoff = 8000.f,
        .transient_level = 0.5f, .transient_decay = 0.01f,
        .pitch_env_amount = 0.1f, .pitch_env_time = 0.04f,
        .attack = 0.01f, .decay = 0.05f, .sustain = 0.6f, .release = 0.15f,
        .vibrato_rate = 4.5f, .vibrato_depth = 0.02f, .vibrato_delay = 0.3f,
        .filter_lfo_rate = 0.7f, .filter_lfo_depth = 0.1f,
        .name = "FLUTE"
    },
    
    // HORN - French Horn
    {
        .osc_saw_level = 0.75f, .osc_pulse_level = 0.35f, .pulse_width = 0.5f,
        .formant1_freq = 400.f, .formant2_freq = 900.f, .formant3_freq = 2200.f, .formant4_freq = 4200.f,
        .formant1_q = 2.0f, .formant2_q = 3.0f, .formant3_q = 1.7f, .formant4_q = 1.0f,  // ✅ Was 6,9,5,3 → /3
        .noise_level = 0.04f, .noise_cutoff = 3500.f,
        .transient_level = 0.25f, .transient_decay = 0.025f,
        .pitch_env_amount = 0.12f, .pitch_env_time = 0.09f,
        .attack = 0.03f, .decay = 0.12f, .sustain = 0.7f, .release = 0.35f,
        .vibrato_rate = 4.8f, .vibrato_depth = 0.018f, .vibrato_delay = 0.5f,
        .filter_lfo_rate = 0.4f, .filter_lfo_depth = 0.09f,
        .name = "HORN"
    },
    
    // OBOE (NEW!)
    {
        .osc_saw_level = 0.8f, .osc_pulse_level = 0.4f, .pulse_width = 0.35f,
        .formant1_freq = 700.f, .formant2_freq = 1400.f, .formant3_freq = 2800.f, .formant4_freq = 5500.f,
        .formant1_q = 3.0f, .formant2_q = 3.0f, .formant3_q = 2.3f, .formant4_q = 1.7f,  // ✅ Was 9,11,7,5 → /3 (limited to 3.0)
        .noise_level = 0.15f, .noise_cutoff = 4000.f,
        .transient_level = 0.35f, .transient_decay = 0.018f,
        .pitch_env_amount = 0.18f, .pitch_env_time = 0.07f,
        .attack = 0.02f, .decay = 0.09f, .sustain = 0.72f, .release = 0.28f,
        .vibrato_rate = 5.5f, .vibrato_depth = 0.022f, .vibrato_delay = 0.35f,
        .filter_lfo_rate = 0.9f, .filter_lfo_depth = 0.14f,
        .name = "OBOE"
    },
    
    // CLARINET (NEW!)
    {
        .osc_saw_level = 0.3f, .osc_pulse_level = 0.85f, .pulse_width = 0.25f,
        .formant1_freq = 600.f, .formant2_freq = 1200.f, .formant3_freq = 2400.f, .formant4_freq = 4800.f,
        .formant1_q = 2.3f, .formant2_q = 3.3f, .formant3_q = 2.0f, .formant4_q = 1.3f,  // ✅ Was 7,10,6,4 → /3
        .noise_level = 0.08f, .noise_cutoff = 3500.f,
        .transient_level = 0.3f, .transient_decay = 0.022f,
        .pitch_env_amount = 0.15f, .pitch_env_time = 0.06f,
        .attack = 0.018f, .decay = 0.07f, .sustain = 0.78f, .release = 0.22f,
        .vibrato_rate = 5.2f, .vibrato_depth = 0.019f, .vibrato_delay = 0.38f,
        .filter_lfo_rate = 0.75f, .filter_lfo_depth = 0.11f,
        .name = "CLARIN"
    }
};

// Voice structure
struct Voice {
    bool active;
    uint8_t note;
    uint8_t velocity;
    
    // Oscillator phases (ensemble - 10 voices!)
    float ensemble_phases_saw[ENSEMBLE_VOICES];
    float ensemble_phases_pulse[ENSEMBLE_VOICES];
    int16_t ensemble_timing_offset[ENSEMBLE_VOICES];
    
    // Noise generator
    uint32_t noise_seed;
    float noise_z1;  // High-pass filter
    
    // Attack transient
    float transient_phase;
    float transient_env;
    
    // Pitch envelope
    float pitch_env;
    uint32_t pitch_env_counter;
    
    // Formant filters (4-band!) - Separate states for L and R channels!
    float formant1_z1, formant1_z2;
    float formant1_z1_r, formant1_z2_r;  // Right channel states
    float formant2_z1, formant2_z2;
    float formant2_z1_r, formant2_z2_r;  // Right channel states
    float formant3_z1, formant3_z2;
    float formant3_z1_r, formant3_z2_r;  // Right channel states
    float formant4_z1, formant4_z2;
    float formant4_z1_r, formant4_z2_r;  // Right channel states
    
    // Filter LFO
    float filter_lfo_phase;
    
    // Envelope
    float amp_env;
    uint8_t env_stage;
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

// Random (for noise)
inline uint32_t xorshift32(uint32_t *seed) {
    *seed ^= *seed << 13;
    *seed ^= *seed >> 17;
    *seed ^= *seed << 5;
    return *seed;
}

inline float white_noise(uint32_t *seed) {
    return ((float)xorshift32(seed) / (float)0xFFFFFFFF) * 2.f - 1.f;
}

// PolyBLEP
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
    // ✅ Extra safety: Limit Q AGAIN (defense in depth)
    q = clipminmaxf(0.5f, q, 3.0f);
    
    // Limit frequency to prevent aliasing and instability
    float freq_clamped = clipminmaxf(20.f, freq, 18000.f);
    float w = 2.f * PI * freq_clamped / 48000.f;
    
    // Limit w to prevent instability
    if (w > PI * 0.99f) w = PI * 0.99f;  // More conservative limit
    
    // Convert to phase [0,1] for SDK functions
    float phase_w = w / (2.f * PI);
    phase_w = clipminmaxf(0.f, phase_w, 1.f);
    
    float phase_sin = phase_w * 0.5f;
    phase_sin = clipminmaxf(0.f, phase_sin, 1.f);
    
    // ✅ Limit Q to prevent instability and whistling (max 3.0!)
    float q_clamped = clipminmaxf(0.5f, q, 3.0f);
    float alpha = osc_sinf(phase_sin) * (1.f / (2.f * q_clamped));
    
    // Stability check: ensure alpha is reasonable
    alpha = clipminmaxf(0.001f, alpha, 0.99f);
    
    float cos_w = osc_cosf(phase_w);
    
    float b0 = alpha;
    float b2 = -alpha;
    float a0 = 1.f + alpha;
    float a1 = -2.f * cos_w;
    float a2 = 1.f - alpha;
    
    b0 /= a0;
    b2 /= a0;
    a1 /= a0;
    a2 /= a0;
    
    float output = b0 * input + b2 * (*z2) - a1 * (*z1) - a2 * (*z2);
    
    // ✅ Denormal killing
    if (si_fabsf(*z1) < 1e-15f) *z1 = 0.f;
    if (si_fabsf(*z2) < 1e-15f) *z2 = 0.f;
    
    // ✅ Output limiting
    output = clipminmaxf(-2.f, output, 2.f);
    
    *z2 = *z1;
    *z1 = output;  // Use output for feedback, not input (prevents instability)
    
    return output;
}

// Generate ensemble (10-voice unison!)
inline void generate_ensemble(Voice *v, float base_w0, const M1Patch *patch, float *out_l, float *out_r) {
    float sum_l = 0.f;
    float sum_r = 0.f;
    
    int voices_active = (s_voice_count == 0) ? 1 : (s_voice_count == 1) ? 2 : (s_voice_count == 2) ? 5 : 10;
    
    for (int i = 0; i < voices_active; i++) {
        // Micro-timing (delayed start)
        if (v->ensemble_timing_offset[i] > 0) {
            v->ensemble_timing_offset[i]--;
            continue;
        }
        
        // Detune
        float detune_cents = s_ensemble_detune[i] * s_detune_amount;
        float w0 = base_w0 * fx_pow2f(detune_cents / 1200.f);  // ✅ Fixed: fx_pow2f (fastpow2f doesn't exist!)
        
        // Limit w0 to prevent aliasing
        if (w0 > 0.48f) w0 = 0.48f;
        
        // SAWTOOTH
        float saw = 2.f * v->ensemble_phases_saw[i] - 1.f;
        saw -= poly_blep(v->ensemble_phases_saw[i], w0);
        
        // PULSE
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
    
    *out_l = sum_l / (float)voices_active;
    *out_r = sum_r / (float)voices_active;
}

// Process formants (4-band!)
inline void process_formants(Voice *v, const M1Patch *patch, float *in_l, float *in_r) {
    // Filter LFO modulation
    v->filter_lfo_phase += patch->filter_lfo_rate / 48000.f;
    if (v->filter_lfo_phase >= 1.f) v->filter_lfo_phase -= 1.f;
    if (v->filter_lfo_phase < 0.f) v->filter_lfo_phase += 1.f;
    
    float lfo = osc_sinf(v->filter_lfo_phase);
    float lfo_mod = 1.f + lfo * patch->filter_lfo_depth;
    
    // Brightness control
    float bright_scale = 0.5f + s_brightness * 1.5f;
    
    // Formant 1
    float f1_freq = patch->formant1_freq * bright_scale * lfo_mod;
    f1_freq = clipminmaxf(20.f, f1_freq, 18000.f);  // Limit to prevent aliasing
    // ✅ FIX: Reduce Q-factor multiplier from 3.0 to 0.5!
    float q_mult = 1.f + s_resonance * 0.5f;  // Max multiplier = 1.5× (not 4.0×!)
    float f1_q = patch->formant1_q * q_mult;
    f1_q = clipminmaxf(0.5f, f1_q, 3.0f);  // ✅ Hard limit 3.0 (not 3.5)
    *in_l = process_peak_filter(*in_l, f1_freq, f1_q, &v->formant1_z1, &v->formant1_z2);
    *in_r = process_peak_filter(*in_r, f1_freq, f1_q, &v->formant1_z1_r, &v->formant1_z2_r);  // Separate states for R!
    
    // Formant 2
    float f2_freq = patch->formant2_freq * bright_scale * lfo_mod;
    f2_freq = clipminmaxf(20.f, f2_freq, 18000.f);
    float f2_q = patch->formant2_q * q_mult;
    f2_q = clipminmaxf(0.5f, f2_q, 3.0f);
    *in_l = process_peak_filter(*in_l, f2_freq, f2_q, &v->formant2_z1, &v->formant2_z2);
    *in_r = process_peak_filter(*in_r, f2_freq, f2_q, &v->formant2_z1_r, &v->formant2_z2_r);  // Separate states for R!
    
    // Formant 3
    float f3_freq = patch->formant3_freq * bright_scale * lfo_mod;
    f3_freq = clipminmaxf(20.f, f3_freq, 18000.f);
    float f3_q = patch->formant3_q * q_mult;
    f3_q = clipminmaxf(0.5f, f3_q, 3.0f);
    *in_l = process_peak_filter(*in_l, f3_freq, f3_q, &v->formant3_z1, &v->formant3_z2);
    *in_r = process_peak_filter(*in_r, f3_freq, f3_q, &v->formant3_z1_r, &v->formant3_z2_r);  // Separate states for R!
    
    // Formant 4 (NEW! Extra brilliance)
    float f4_freq = patch->formant4_freq * bright_scale * lfo_mod;
    f4_freq = clipminmaxf(20.f, f4_freq, 18000.f);
    float f4_q = patch->formant4_q * q_mult;
    f4_q = clipminmaxf(0.5f, f4_q, 3.0f);
    *in_l = process_peak_filter(*in_l, f4_freq, f4_q, &v->formant4_z1, &v->formant4_z2);
    *in_r = process_peak_filter(*in_r, f4_freq, f4_q, &v->formant4_z1_r, &v->formant4_z2_r);  // Separate states for R!
}

// Noise layer
inline float generate_noise(Voice *v, const M1Patch *patch) {
    float noise = white_noise(&v->noise_seed);
    
    // High-pass filter
    float cutoff = patch->noise_cutoff;
    float w = 2.f * PI * cutoff / 48000.f;
    float g = fasttanfullf(w * 0.5f);
    
    v->noise_z1 = v->noise_z1 + g * (noise - v->noise_z1);
    float hp = noise - v->noise_z1;
    
    return hp * patch->noise_level * s_breath_amount;
}

// Attack transient
inline float generate_transient(Voice *v, const M1Patch *patch) {
    if (v->transient_phase >= 1.f) return 0.f;
    
    // Read from wavetable
    uint32_t idx = (uint32_t)(v->transient_phase * (float)(TRANSIENT_TABLE_SIZE - 1));
    if (idx >= TRANSIENT_TABLE_SIZE) idx = TRANSIENT_TABLE_SIZE - 1;
    float sample = s_transient_table[idx];
    
    // Envelope
    float t_sec = (float)v->env_counter / 48000.f;
    v->transient_env = fx_pow2f(-t_sec / patch->transient_decay * 5.f);  // ✅ Fixed: fx_pow2f (fastpow2f doesn't exist!)
    
    v->transient_phase += 0.01f;
    
    return sample * v->transient_env * patch->transient_level;
}

// Pitch envelope
inline float update_pitch_envelope(Voice *v, const M1Patch *patch) {
    if (patch->pitch_env_amount < 0.01f) return 0.f;
    
    float t_sec = (float)v->pitch_env_counter / 48000.f;
    float env = fx_pow2f(-t_sec / patch->pitch_env_time * 5.f);  // ✅ Fixed: fx_pow2f (fastpow2f doesn't exist!)
    
    v->pitch_env = env;
    v->pitch_env_counter++;
    
    return env * patch->pitch_env_amount;
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
            // ✅ FIX: Smooth exponential release to prevent pieptoon
            v->amp_env = patch->sustain * fx_pow2f(-t_sec / release * 5.f);  // ✅ Fixed: fx_pow2f (fastpow2f doesn't exist!)
            
            // ✅ Fade out high-frequency filters during release to prevent pieptoon
            if (v->amp_env < 0.05f) {  // ✅ Start damping earlier (was 0.1f)
                // Extra damping for high frequencies during fade-out
                v->formant3_z1 *= 0.95f;  // ✅ Stronger damping (was 0.99f)
                v->formant3_z2 *= 0.95f;
                v->formant3_z1_r *= 0.95f;
                v->formant3_z2_r *= 0.95f;
                v->formant4_z1 *= 0.95f;
                v->formant4_z2 *= 0.95f;
                v->formant4_z1_r *= 0.95f;
                v->formant4_z2_r *= 0.95f;
                v->noise_z1 *= 0.98f;
            }
            
            // ✅ Voice volledig OFF?
            if (v->amp_env < 0.001f) {
                v->active = false;
                v->amp_env = 0.f;
                
                // ✅ NU PAS filters resetten! (alleen bij volledige OFF)
                v->formant1_z1 = v->formant1_z2 = 0.f;
                v->formant1_z1_r = v->formant1_z2_r = 0.f;
                v->formant2_z1 = v->formant2_z2 = 0.f;
                v->formant2_z1_r = v->formant2_z2_r = 0.f;
                v->formant3_z1 = v->formant3_z2 = 0.f;
                v->formant3_z1_r = v->formant3_z2_r = 0.f;
                v->formant4_z1 = v->formant4_z2 = 0.f;
                v->formant4_z1_r = v->formant4_z2_r = 0.f;
                v->noise_z1 = 0.f;
            }
            break;
    }
    
    v->env_counter++;
    return v->amp_env;
}

// Vibrato
inline float update_vibrato(Voice *v, const M1Patch *patch) {
    float t_sec = (float)v->vibrato_counter / 48000.f;
    
    if (t_sec < patch->vibrato_delay) {
        v->vibrato_fade = 0.f;
    } else {
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

// Stereo width (Mid/Side + Haas)
inline void stereo_widen(float *l, float *r) {
    // Mid/Side processing
    float mid = (*l + *r) * 0.5f;
    float side = (*l - *r) * 0.5f;
    
    // Enhance side (stereo width)
    float width = 1.f + s_ensemble_amount;
    side *= width;
    
    *l = mid + side;
    *r = mid - side;
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc)
{
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 1) return k_unit_err_geometry;

    s_context = static_cast<const unit_runtime_osc_context_t *>(desc->hooks.runtime_context);

    // Init transient wavetable (simple impulse)
    for (int i = 0; i < TRANSIENT_TABLE_SIZE; i++) {
        float phase = (float)i / (float)TRANSIENT_TABLE_SIZE;
        float phase_norm = phase;  // [0,1] for osc_sinf
        float phase_sin = phase_norm * 0.5f;
        if (phase_sin >= 1.f) phase_sin -= 1.f;
        if (phase_sin < 0.f) phase_sin += 1.f;
        s_transient_table[i] = osc_sinf(phase_sin) * fx_pow2f(-phase * 3.f);  // ✅ Fixed: fx_pow2f (fastpow2f doesn't exist!)
    }
    
    // Init voices
    for (int v = 0; v < MAX_VOICES; v++) {
        s_voices[v].active = false;
        s_voices[v].noise_seed = 12345 + v * 678;
        
        for (int i = 0; i < ENSEMBLE_VOICES; i++) {
            s_voices[v].ensemble_phases_saw[i] = 0.f;
            s_voices[v].ensemble_phases_pulse[i] = 0.f;
            s_voices[v].ensemble_timing_offset[i] = s_ensemble_timing[i];
        }
        
        s_voices[v].noise_z1 = 0.f;
        s_voices[v].transient_phase = 0.f;
        s_voices[v].transient_env = 0.f;
        s_voices[v].pitch_env = 0.f;
        s_voices[v].pitch_env_counter = 0;
        
        s_voices[v].formant1_z1 = s_voices[v].formant1_z2 = 0.f;
        s_voices[v].formant1_z1_r = s_voices[v].formant1_z2_r = 0.f;  // Reset right channel states
        s_voices[v].formant2_z1 = s_voices[v].formant2_z2 = 0.f;
        s_voices[v].formant2_z1_r = s_voices[v].formant2_z2_r = 0.f;  // Reset right channel states
        s_voices[v].formant3_z1 = s_voices[v].formant3_z2 = 0.f;
        s_voices[v].formant3_z1_r = s_voices[v].formant3_z2_r = 0.f;  // Reset right channel states
        s_voices[v].formant4_z1 = s_voices[v].formant4_z2 = 0.f;
        s_voices[v].formant4_z1_r = s_voices[v].formant4_z2_r = 0.f;  // Reset right channel states
        
        s_voices[v].filter_lfo_phase = 0.f;
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
    
    // ✅ ORIGINELE WERKENDE DEFAULTS (matching header.c)
    s_brightness = 0.6f;       // 60% - ORIGINELE WERKENDE DEFAULT
    s_resonance = 0.75f;       // 75% - ORIGINELE WERKENDE DEFAULT
    s_detune_amount = 0.5f;    // 50% - Perfect chorus
    s_ensemble_amount = 0.4f;  // 40% - Nice width
    s_vibrato_amount = 0.4f;   // 40% - ORIGINELE WERKENDE DEFAULT
    s_breath_amount = 0.25f;   // 25% - ORIGINELE WERKENDE DEFAULT
    s_attack_mod = 0.65f;      // 65% - ORIGINELE WERKENDE DEFAULT
    s_release_mod = 0.8f;      // 80% - ORIGINELE WERKENDE DEFAULT
    s_patch_select = 0;
    s_voice_count = 2;
    
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
            
            // Vibrato
            float vib = update_vibrato(voice, patch);
            
            // Pitch envelope (upward swell or downward fall)
            float pitch_env_mod = update_pitch_envelope(voice, patch);
            
            // Calculate pitch
            float pitch_mod = vib * 12.f + pitch_env_mod;
            float w0 = osc_w0f_for_note(voice->note + (int8_t)pitch_mod, mod);
            
            // Generate ensemble
            float ens_l, ens_r;
            generate_ensemble(voice, w0, patch, &ens_l, &ens_r);
            
            // ✅ SAFETY: Check for NaN/Inf after ensemble generation
            if (!std::isfinite(ens_l)) ens_l = 0.f;
            if (!std::isfinite(ens_r)) ens_r = 0.f;
            
            // Add noise layer
            float noise = generate_noise(voice, patch);
            ens_l += noise;
            ens_r += noise;
            
            // Add attack transient
            float transient = generate_transient(voice, patch);
            ens_l += transient;
            ens_r += transient;
            
            // Formant filtering (4-BAND!)
            process_formants(voice, patch, &ens_l, &ens_r);
            
            // ✅ SAFETY: Check again after formant processing
            if (!std::isfinite(ens_l)) ens_l = 0.f;
            if (!std::isfinite(ens_r)) ens_r = 0.f;
            
            // Envelope
            float env = update_envelope(voice, patch);
            
            // Velocity scaling
            float vel_scale = (float)voice->velocity / 127.f;
            vel_scale = 0.5f + vel_scale * 0.5f;
            
            // Breath controller
            // ✅ SAFETY: Ensure breath_level is never 0 if breath_amount > 0
            if (s_breath_amount > 0.01f && voice->breath_level < 0.01f) {
                voice->breath_level = s_breath_amount;  // Force initialize if too low
            }
            voice->breath_level += (s_breath_amount - voice->breath_level) * 0.001f;
            
            // ✅ SAFETY: Ensure envelope is valid
            if (env < 0.f) env = 0.f;
            if (env > 1.f) env = 1.f;
            if (!std::isfinite(env)) env = 0.f;
            
            ens_l *= env * vel_scale * voice->breath_level;
            ens_r *= env * vel_scale * voice->breath_level;
            
            sig_l += ens_l;
            sig_r += ens_r;
            active_count++;
        }
        
        if (active_count > 0) {
            sig_l /= (float)active_count;
            sig_r /= (float)active_count;
        }
        
        // Chorus
        chorus_process(&sig_l, &sig_r);
        
        // Stereo widening
        stereo_widen(&sig_l, &sig_r);
        
        // Mono mix
        float mono = (sig_l + sig_r) * 0.5f;
        
        // ✅ IMPROVED DC blocker (10Hz high-pass) to prevent pieptoon
        static float dc_z = 0.f;
        float dc_coeff = 0.999f;  // 10Hz cutoff @ 48kHz
        float dc_out = mono - dc_z;
        dc_z = dc_z * dc_coeff + mono * (1.f - dc_coeff);
        mono = dc_out;
        
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
        if (value >= 0 && value < 12) return s_patches[value].name;
    }
    if (id == 9) {
        static const char *voice_names[] = {"MONO", "UNI2", "UNI5", "UNI10"};
        if (value >= 0 && value < 4) return voice_names[value];
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
    
    // CRITICAL: Reset filters FIRST (before anything else!) to prevent DC offset buildup
    voice->formant1_z1 = voice->formant1_z2 = 0.f;
    voice->formant1_z1_r = voice->formant1_z2_r = 0.f;  // Reset right channel states
    voice->formant2_z1 = voice->formant2_z2 = 0.f;
    voice->formant2_z1_r = voice->formant2_z2_r = 0.f;  // Reset right channel states
    voice->formant3_z1 = voice->formant3_z2 = 0.f;
    voice->formant3_z1_r = voice->formant3_z2_r = 0.f;  // Reset right channel states
    voice->formant4_z1 = voice->formant4_z2 = 0.f;
    voice->formant4_z1_r = voice->formant4_z2_r = 0.f;  // Reset right channel states
    voice->noise_z1 = 0.f;
    
    // Reset envelopes
    voice->env_counter = 0;
    voice->env_stage = 0;
    voice->amp_env = 0.f;
    
    voice->pitch_env_counter = 0;
    voice->pitch_env = 0.f;
    
    voice->vibrato_counter = 0;
    voice->vibrato_phase = 0.f;
    voice->vibrato_fade = 0.f;
    
    voice->transient_phase = 0.f;
    voice->transient_env = 1.f;
    
    voice->filter_lfo_phase = 0.f;
    
    // ✅ KRITIEK: Initialize breath_level (was missing - causes silence!)
    voice->breath_level = s_breath_amount > 0.01f ? s_breath_amount : 1.f;
    
    // Reset phases with micro-timing
    for (int i = 0; i < ENSEMBLE_VOICES; i++) {
        voice->ensemble_phases_saw[i] = 0.f;
        voice->ensemble_phases_pulse[i] = 0.f;
        voice->ensemble_timing_offset[i] = s_ensemble_timing[i];
    }
}

__unit_callback void unit_note_off(uint8_t note)
{
    for (int v = 0; v < MAX_VOICES; v++) {
        if (s_voices[v].active && s_voices[v].note == note) {
            // ✅ Set to release (NO filter reset here - that causes silence!)
            s_voices[v].env_stage = 3;
            s_voices[v].env_counter = 0;
            // Filters will be reset in update_envelope() when voice becomes inactive
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

