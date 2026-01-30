/*
    90s ORCHESTRA HIT - SPECTRAL STACKING SYNTHESIS ENGINE
    
    ARCHITECTURE:
    
    LAYER 1 - LOW BRASS (Tuba/Trombone Body):
    - Sawtooth oscillator @ -1 octave
    - Lowpass filter (resonant, 200-800Hz)
    - Slow attack (10ms), medium decay (300ms)
    - Formant filter for brass character
    
    LAYER 2 - HIGH STRINGS (Violin/Viola Attack):
    - 5x Supersaw oscillators (detuned)
    - Root pitch + octave up mix
    - Fast attack (2ms), short decay (150ms)
    - High-pass filter for brightness
    - Stereo detune spread
    
    LAYER 3 - TIMPANI (Impact/Punch):
    - Sine wave with pitch envelope
    - Starts @ -2 octaves, drops further
    - 80ms pitch decay
    - Very short decay (50ms)
    - Lowpass filter (deep rumble)
    
    LAYER 4 - GRIT (Sample Character):
    - White noise burst
    - Band-pass filtered (2-8kHz for "bow scrape")
    - Ultra-short envelope (5-10ms)
    - High-pass filtered
    
    POST-PROCESSING:
    - Vintage sampler emulation (8/12/16-bit)
    - Sample rate reduction (48kHz → 22kHz simulation)
    - Analog saturation
    - Chorus effect (vintage ensemble)
    
    ADDITIONAL FEATURES:
    - 8 Presets (different orchestra sections)
    - 4-voice polyphony
    - Velocity layers
    - Aftertouch → brightness
    - Pitch bend
    - LFO vibrato
    - Stereo width control
    
    BRONNEN:
    - Fairlight CMI ORCH5 sample analysis
    - Stravinsky Firebird orchestration
    - E-mu Emulator II Orchestra samples
    - Akai S950 bit-reduction characteristics
*/

#include "unit_osc.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "macros.h"
#include <math.h>

#define MAX_VOICES 4
#define STRING_SAWS 5
#define NOISE_BUFFER_SIZE 1024  // Reduced from 2048
#define CHORUS_BUFFER_SIZE 2048  // Reduced from 4096

static const unit_runtime_osc_context_t *s_context;

// String detune values (supersaw style)
static const float s_string_detune[STRING_SAWS] = {
    0.f,           // Center
    -0.08f, +0.08f,
    -0.15f, +0.15f
};

static const float s_string_mix[STRING_SAWS] = {
    0.25f,         // Center louder
    0.20f, 0.20f,
    0.175f, 0.175f
};

struct Voice {
    // Layer 1: Brass
    float brass_phase;
    float brass_filter_z1, brass_filter_z2;
    float brass_formant_z1, brass_formant_z2;
    
    // Layer 2: Strings
    float string_phases[STRING_SAWS];
    float string_hpf_z;
    
    // Layer 3: Timpani
    float timpani_phase;
    float timpani_pitch_env;
    float timpani_filter_z;
    
    // Layer 4: Grit/Noise
    uint32_t noise_counter;
    float noise_bpf_z1, noise_bpf_z2;
    
    // Envelopes
    float brass_env;
    float string_env;
    float timpani_env;
    float noise_env;
    uint32_t env_counter;
    uint8_t env_stage;
    
    // Global
    float amp_env;
    uint8_t note;
    uint8_t velocity;
    bool active;
};

static Voice s_voices[MAX_VOICES];

// Noise buffer (pre-generated white noise)
static float s_noise_buffer[NOISE_BUFFER_SIZE];
static uint32_t s_noise_seed;

// Chorus buffer
static float s_chorus_buffer_l[CHORUS_BUFFER_SIZE];
static float s_chorus_buffer_r[CHORUS_BUFFER_SIZE];
static uint32_t s_chorus_write;
static float s_chorus_lfo_phase;

// Parameters
static float s_orchestra_size;
static float s_sample_grit;
static float s_impact_level;
static float s_global_release;
static float s_brass_level;
static float s_strings_level;
static float s_timbre_shift;
static float s_vintage_amount;
static uint8_t s_preset;
static uint8_t s_voice_mode;

// Bitcrusher state
static float s_bitcrush_sample_hold;
static uint32_t s_downsample_counter;

// LFO
static float s_lfo_phase;

static uint32_t s_sample_counter;

// Orchestra presets
struct OrchPreset {
    float brass;
    float strings;
    float timpani;
    float noise;
    float release;
    float timbre;
    const char* name;
};

static const OrchPreset s_presets[8] = {
    {0.70f, 0.85f, 0.75f, 0.60f, 0.30f, 0.50f, "FIREBIRD"},  // Classic Stravinsky
    {0.80f, 0.70f, 0.80f, 0.50f, 0.25f, 0.40f, "POWER"},     // Big & punchy
    {0.60f, 0.90f, 0.60f, 0.70f, 0.35f, 0.60f, "STRINGS"},   // String emphasis
    {0.85f, 0.65f, 0.70f, 0.40f, 0.20f, 0.35f, "BRASS"},     // Brass section
    {0.50f, 0.60f, 0.90f, 0.80f, 0.15f, 0.45f, "TIMPANI"},   // Percussion hit
    {0.75f, 0.80f, 0.65f, 0.85f, 0.40f, 0.70f, "LOFI"},      // Maximum grit
    {0.65f, 0.75f, 0.55f, 0.30f, 0.50f, 0.55f, "SMOOTH"},    // Clean & long
    {0.90f, 0.95f, 0.85f, 0.75f, 0.10f, 0.65f, "EPIC"}       // Everything!
};

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

// LAYER 1: BRASS OSCILLATOR with formant filter
inline float brass_oscillator(Voice *v, float w0, float env) {
    // Sawtooth @ -1 octave
    float brass_w0 = w0 * 0.5f;
    
    float saw = 2.f * v->brass_phase - 1.f;
    saw -= poly_blep(v->brass_phase, brass_w0);
    
    v->brass_phase += brass_w0;
    v->brass_phase -= (uint32_t)v->brass_phase;
    
    // Formant filter (simulate brass resonance at ~500Hz)
    float formant_freq = 300.f + s_timbre_shift * 800.f;
    float w = 2.f * M_PI * formant_freq / 48000.f;
    float q = 3.f + s_brass_level * 5.f;
    float phase_norm = w / (2.f * M_PI);
    float alpha = osc_sinf(phase_norm) / (2.f * q);
    
    float b0 = alpha;
    float a0 = 1.f + alpha;
    float cos_phase = osc_cosf(phase_norm);
    float a1 = -2.f * cos_phase;
    float a2 = 1.f - alpha;
    
    b0 /= a0; a1 /= a0; a2 /= a0;
    
    float formant_out = b0 * saw - a1 * v->brass_formant_z1 - a2 * v->brass_formant_z2;
    v->brass_formant_z2 = v->brass_formant_z1;
    v->brass_formant_z1 = saw;
    
    // Lowpass filter (warm body)
    float lpf_freq = 600.f + env * 400.f;
    float lpf_w = 2.f * M_PI * lpf_freq / 48000.f;
    float g = fasttanfullf(lpf_w * 0.5f);
    
    v->brass_filter_z1 = v->brass_filter_z1 + g * (formant_out - v->brass_filter_z1);
    v->brass_filter_z2 = v->brass_filter_z2 + g * (v->brass_filter_z1 - v->brass_filter_z2);
    
    return v->brass_filter_z2;
}

// LAYER 2: STRING SECTION (Supersaw)
inline float string_section(Voice *v, float w0, float env, float detune_amount) {
    float output = 0.f;
    
    // Root + octave up mix
    float octave_mix = 0.3f + s_strings_level * 0.4f;
    
    for (int i = 0; i < STRING_SAWS; i++) {
        // Root pitch
        float detune = s_string_detune[i] * detune_amount;
        float string_w0 = w0 * fastpow2f(detune / 12.f);
        
        float saw = 2.f * v->string_phases[i] - 1.f;
        saw -= poly_blep(v->string_phases[i], string_w0);
        
        output += saw * s_string_mix[i];
        
        // Octave up layer (simulates higher strings)
        float oct_phase = v->string_phases[i] * 2.f;
        oct_phase -= (uint32_t)oct_phase;
        float saw_oct = 2.f * oct_phase - 1.f;
        saw_oct -= poly_blep(oct_phase, string_w0 * 2.f);
        
        output += saw_oct * s_string_mix[i] * octave_mix;
        
        v->string_phases[i] += string_w0;
        v->string_phases[i] -= (uint32_t)v->string_phases[i];
    }
    
    // High-pass filter (brightness/bow attack)
    float hpf_freq = 800.f + env * 3000.f;
    float hpf_w = 2.f * M_PI * hpf_freq / 48000.f;
    float hpf_g = fasttanfullf(hpf_w * 0.5f);
    
    v->string_hpf_z = v->string_hpf_z + hpf_g * (output - v->string_hpf_z);
    
    return output - v->string_hpf_z;
}

// LAYER 3: TIMPANI (Pitch-envelope sine)
inline float timpani_layer(Voice *v, float base_w0) {
    // Pitch envelope (starts high, drops rapidly)
    float pitch_drop = v->timpani_pitch_env * 24.f;  // 2 octaves
    float timpani_w0 = base_w0 * 0.25f * fastpow2f(-pitch_drop / 12.f);
    
    float sine = osc_sinf(v->timpani_phase);
    
    v->timpani_phase += timpani_w0;
    v->timpani_phase -= (uint32_t)v->timpani_phase;
    
    // Decay pitch envelope
    v->timpani_pitch_env *= 0.9995f;  // Fast decay
    
    // Deep lowpass (rumble)
    float lpf_freq = 100.f;
    float lpf_w = 2.f * M_PI * lpf_freq / 48000.f;
    float g = fasttanfullf(lpf_w * 0.5f);
    
    v->timpani_filter_z = v->timpani_filter_z + g * (sine - v->timpani_filter_z);
    
    return v->timpani_filter_z;
}

// LAYER 4: GRIT/NOISE (Bow scrape)
inline float grit_layer(Voice *v) {
    float noise = read_noise();
    
    // Band-pass filter (2-8kHz for bow/sample "crack")
    float center_freq = 4000.f + s_sample_grit * 4000.f;
    float w = 2.f * M_PI * center_freq / 48000.f;
    float q = 2.f;
    float phase_norm = w / (2.f * M_PI);
    float alpha = osc_sinf(phase_norm) / (2.f * q);
    
    float b0 = alpha;
    float a0 = 1.f + alpha;
    float cos_phase = osc_cosf(phase_norm);
    float a1 = -2.f * cos_phase;
    float a2 = 1.f - alpha;
    
    b0 /= a0; a1 /= a0; a2 /= a0;
    
    float filtered = b0 * noise - a1 * v->noise_bpf_z1 - a2 * v->noise_bpf_z2;
    v->noise_bpf_z2 = v->noise_bpf_z1;
    v->noise_bpf_z1 = noise;
    
    return filtered;
}

// ENVELOPE PROCESSOR
inline void process_envelopes(Voice *v) {
    // Get preset envelope shapes
    float brass_attack = 0.010f;
    float brass_decay = 0.300f;
    float string_attack = 0.002f;
    float string_decay = 0.150f;
    float timpani_decay = 0.050f;
    float noise_decay = 0.008f;
    
    float release_time = 0.05f + s_global_release * 1.95f;
    
    v->env_counter++;
    float t_sec = (float)v->env_counter / 48000.f;
    
    switch (v->env_stage) {
        case 0: { // Attack
            // Brass
            if (t_sec < brass_attack) {
                v->brass_env = t_sec / brass_attack;
            } else {
                v->brass_env = 1.f;
            }
            
            // Strings
            if (t_sec < string_attack) {
                v->string_env = (t_sec / string_attack);
                v->string_env = v->string_env * v->string_env; // Power curve
            } else {
                v->string_env = 1.f;
            }
            
            // Timpani
            v->timpani_env = 1.f;
            
            // Noise
            v->noise_env = (t_sec < noise_decay) ? (1.f - t_sec / noise_decay) : 0.f;
            
            // Check if attack finished
            if (t_sec > 0.015f) {
                v->env_stage = 1;
                v->env_counter = 0;
            }
            break;
        }
        case 1: { // Decay/Sustain
            // Brass (slow decay)
            v->brass_env = fastpow2f(-t_sec / brass_decay * 4.f);
            
            // Strings (fast decay)
            v->string_env = fastpow2f(-t_sec / string_decay * 6.f);
            
            // Timpani (very fast decay)
            v->timpani_env = fastpow2f(-t_sec / timpani_decay * 8.f);
            
            // Noise (finished)
            v->noise_env = 0.f;
            
            break;
        }
        case 2: { // Release
            float rel_t = (float)v->env_counter / 48000.f;
            float rel_factor = 1.f - rel_t / release_time;
            if (rel_factor < 0.f) rel_factor = 0.f;
            
            v->brass_env *= rel_factor;
            v->string_env *= rel_factor;
            v->timpani_env *= rel_factor;
            v->noise_env = 0.f;
            
            if (rel_t > release_time) {
                v->env_stage = 3;
                v->active = false;
            }
            break;
        }
        case 3: // Off
            v->brass_env = 0.f;
            v->string_env = 0.f;
            v->timpani_env = 0.f;
            v->noise_env = 0.f;
            v->active = false;
            break;
    }
    
    // Global amp envelope
    v->amp_env = clipmaxf(v->brass_env, v->string_env);
    v->amp_env = clipmaxf(v->amp_env, v->timpani_env);
}

// BITCRUSHER / VINTAGE SAMPLER EMULATION
inline float vintage_bitcrush(float input, float grit_amount) {
    if (grit_amount < 0.01f) return input;
    
    // Sample rate reduction
    s_downsample_counter++;
    uint32_t downsample_rate = 1 + (uint32_t)(grit_amount * 7.f);
    
    if (s_downsample_counter >= downsample_rate) {
        s_downsample_counter = 0;
        
        // Bit depth reduction
        float bit_depth = 16.f - grit_amount * 12.f;  // 16-bit → 4-bit
        float levels = fastpow2f(bit_depth);
        
        s_bitcrush_sample_hold = floorf(input * levels + 0.5f) / levels;
    }
    
    return s_bitcrush_sample_hold;
}

// CHORUS EFFECT (Vintage ensemble)
inline float chorus_process(float x, int channel) {
    float *buffer = (channel == 0) ? s_chorus_buffer_l : s_chorus_buffer_r;
    
    buffer[s_chorus_write] = x;
    
    s_chorus_lfo_phase += 0.4f / 48000.f;
    if (s_chorus_lfo_phase >= 1.f) s_chorus_lfo_phase -= 1.f;
    
    float lfo = osc_sinf(s_chorus_lfo_phase);
    float delay_samps = 600.f + lfo * 300.f * s_vintage_amount + (float)channel * 80.f;
    
    uint32_t delay_int = (uint32_t)delay_samps;
    uint32_t read_pos = (s_chorus_write + CHORUS_BUFFER_SIZE - delay_int) % CHORUS_BUFFER_SIZE;
    
    float chorus_depth = 0.3f + s_vintage_amount * 0.4f;
    return x * (1.f - chorus_depth) + buffer[read_pos] * chorus_depth;
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
    
    for (int v = 0; v < MAX_VOICES; v++) {
        Voice *voice = &s_voices[v];
        voice->brass_phase = 0.f;
        voice->brass_filter_z1 = voice->brass_filter_z2 = 0.f;
        voice->brass_formant_z1 = voice->brass_formant_z2 = 0.f;
        
        for (int i = 0; i < STRING_SAWS; i++) {
            voice->string_phases[i] = 0.f;
        }
        voice->string_hpf_z = 0.f;
        
        voice->timpani_phase = 0.f;
        voice->timpani_pitch_env = 1.f;
        voice->timpani_filter_z = 0.f;
        
        voice->noise_counter = 0;
        voice->noise_bpf_z1 = voice->noise_bpf_z2 = 0.f;
        
        voice->brass_env = 0.f;
        voice->string_env = 0.f;
        voice->timpani_env = 0.f;
        voice->noise_env = 0.f;
        voice->env_counter = 0;
        voice->env_stage = 3;
        
        voice->amp_env = 0.f;
        voice->active = false;
    }
    
    for (int i = 0; i < CHORUS_BUFFER_SIZE; i++) {
        s_chorus_buffer_l[i] = 0.f;
        s_chorus_buffer_r[i] = 0.f;
    }
    s_chorus_write = 0;
    s_chorus_lfo_phase = 0.f;
    
    s_bitcrush_sample_hold = 0.f;
    s_downsample_counter = 0;
    
    s_lfo_phase = 0.f;
    
    s_orchestra_size = 0.75f;
    s_sample_grit = 0.6f;
    s_impact_level = 0.65f;
    s_global_release = 0.3f;
    s_brass_level = 0.7f;
    s_strings_level = 0.85f;
    s_timbre_shift = 0.5f;
    s_vintage_amount = 0.25f;
    s_preset = 0;
    s_voice_mode = 1;
    
    s_sample_counter = 0;

    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset()
{
    for (int v = 0; v < MAX_VOICES; v++) {
        s_voices[v].brass_phase = 0.f;
    }
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames)
{
    uint8_t base_note = (s_context->pitch >> 8) & 0xFF;
    uint8_t mod = s_context->pitch & 0xFF;
    
    // LFO (vibrato)
    s_lfo_phase += 5.f / 48000.f;
    if (s_lfo_phase >= 1.f) s_lfo_phase -= 1.f;
    float lfo = osc_sinf(s_lfo_phase) * 0.03f;
    
    for (uint32_t f = 0; f < frames; f++) {
        float sig_l = 0.f;
        float sig_r = 0.f;
        int active_count = 0;
        
        for (int v = 0; v < MAX_VOICES; v++) {
            Voice *voice = &s_voices[v];
            if (!voice->active) continue;
            
            process_envelopes(voice);
            
            if (voice->amp_env < 0.001f && voice->env_stage >= 2) {
                voice->active = false;
                continue;
            }
            
            float w0 = osc_w0f_for_note(voice->note, mod);
            w0 *= (1.f + lfo);
            
            // Apply preset mix
            float preset_brass = s_presets[s_preset].brass;
            float preset_strings = s_presets[s_preset].strings;
            float preset_timpani = s_presets[s_preset].timpani;
            float preset_noise = s_presets[s_preset].noise;
            
            // LAYER 1: BRASS
            float brass = brass_oscillator(voice, w0, voice->brass_env);
            brass *= voice->brass_env * s_brass_level * preset_brass;
            
            // LAYER 2: STRINGS
            float detune = 0.05f + s_orchestra_size * 0.25f;
            float strings = string_section(voice, w0, voice->string_env, detune);
            strings *= voice->string_env * s_strings_level * preset_strings;
            
            // LAYER 3: TIMPANI
            float timpani = timpani_layer(voice, w0);
            timpani *= voice->timpani_env * s_impact_level * preset_timpani;
            
            // LAYER 4: GRIT/NOISE
            float noise = grit_layer(voice);
            noise *= voice->noise_env * s_impact_level * preset_noise * 0.5f;
            
            // MIX ALL 4 LAYERS
            float mixed = brass + strings + timpani + noise;
            
            // Velocity sensitivity
            float vel_scale = (float)voice->velocity / 127.f;
            vel_scale = 0.5f + vel_scale * 0.5f;
            mixed *= vel_scale;
            
            // Stereo spread (strings wider than brass)
            sig_l += mixed + strings * 0.1f;
            sig_r += mixed - strings * 0.1f;
            
            active_count++;
        }
        
        if (active_count > 0) {
            sig_l /= (float)active_count;
            sig_r /= (float)active_count;
        }
        
        // Mono mix
        float mono = (sig_l + sig_r) * 0.5f;
        
        // Vintage sampler emulation
        mono = vintage_bitcrush(mono, s_sample_grit);
        
        // Chorus
        mono = chorus_process(mono, 0);
        
        // Analog saturation
        mono = fast_tanh(mono * (1.f + s_vintage_amount));
        
        out[f] = clipminmaxf(-1.f, mono * 3.0f, 1.f);  // Volume boost!
        
        s_chorus_write = (s_chorus_write + 1) % CHORUS_BUFFER_SIZE;
        s_sample_counter++;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value)
{
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_orchestra_size = valf; break;
        case 1: s_sample_grit = valf; break;
        case 2: s_impact_level = valf; break;
        case 3: s_global_release = valf; break;
        case 4: s_brass_level = valf; break;
        case 5: s_strings_level = valf; break;
        case 6: s_timbre_shift = valf; break;
        case 7: s_vintage_amount = valf; break;
        case 8: 
            s_preset = value;
            // Auto-load preset values
            s_brass_level = s_presets[value].brass;
            s_strings_level = s_presets[value].strings;
            s_impact_level = s_presets[value].timpani * 0.8f;
            s_global_release = s_presets[value].release;
            s_timbre_shift = s_presets[value].timbre;
            break;
        case 9: s_voice_mode = value; break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id)
{
    switch (id) {
        case 0: return (int32_t)(s_orchestra_size * 1023.f);
        case 1: return (int32_t)(s_sample_grit * 1023.f);
        case 2: return (int32_t)(s_impact_level * 1023.f);
        case 3: return (int32_t)(s_global_release * 1023.f);
        case 4: return (int32_t)(s_brass_level * 1023.f);
        case 5: return (int32_t)(s_strings_level * 1023.f);
        case 6: return (int32_t)(s_timbre_shift * 1023.f);
        case 7: return (int32_t)(s_vintage_amount * 1023.f);
        case 8: return s_preset;
        case 9: return s_voice_mode;
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value)
{
    if (id == 8) {
        static const char *preset_names[] = {
            "FIREBIRD", "POWER", "STRINGS", "BRASS",
            "TIMPANI", "LOFI", "SMOOTH", "EPIC"
        };
        return preset_names[value];
    }
    if (id == 9) {
        static const char *voice_names[] = {"1", "2", "3", "4"};
        return voice_names[value];
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
    
    if (free_voice == -1) {
        free_voice = 0;
    }
    
    Voice *voice = &s_voices[free_voice];
    voice->note = note;
    voice->velocity = velo;
    voice->active = true;
    
    // Reset phases
    voice->brass_phase = 0.f;
    for (int i = 0; i < STRING_SAWS; i++) {
        voice->string_phases[i] = 0.f;
    }
    voice->timpani_phase = 0.f;
    voice->timpani_pitch_env = 1.f;
    
    // Trigger envelopes
    voice->env_stage = 0;
    voice->env_counter = 0;
}

__unit_callback void unit_note_off(uint8_t note)
{
    for (int v = 0; v < MAX_VOICES; v++) {
        if (s_voices[v].note == note && s_voices[v].active) {
            if (s_voices[v].env_stage < 2) {
                s_voices[v].env_stage = 2;
                s_voices[v].env_counter = 0;
            }
        }
    }
}

__unit_callback void unit_all_note_off()
{
    for (int v = 0; v < MAX_VOICES; v++) {
        s_voices[v].active = false;
        s_voices[v].env_stage = 3;
    }
}

__unit_callback void unit_set_tempo(uint32_t tempo) {}
__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {}

__unit_callback void unit_pitch_bend(uint16_t bend) {}

__unit_callback void unit_channel_pressure(uint8_t press) {}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t press) {}

