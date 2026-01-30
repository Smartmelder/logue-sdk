/*
    MELANCHOLIC CIRCUIT - Simple Bell Synth
    ALL IN ONE FILE (like m1_piano_pm)
*/

#include "unit_osc.h"
#include "osc_api.h"
#include "utils/float_math.h"
#include "utils/int_math.h"

static const unit_runtime_osc_context_t *s_context;

// Bell harmonic ratios (proven formula)
static const float HARMONIC_RATIOS[6] = {
    1.0f,   // Fundamental
    2.76f,  // Minor 3rd + octave
    5.40f,  // Perfect 5th + 2 octaves
    8.93f,  // Major 6th + 3 octaves
    11.34f, // Octave + minor 7th + 3 octaves
    14.42f  // Double octave + major 2nd + 3 octaves
};

struct Voice {
    float phase;
    float freq_mult;
};

static Voice s_voices[6];

// Parameters
static float s_brightness;
static float s_decay;
static float s_strike;
static float s_detune;
static float s_attack;
static float s_release;
static float s_chorus;
static float s_tone;
static uint8_t s_voice_count;

// State
static float s_env;
static bool s_gate;
static uint8_t s_velocity;
static float s_mod_phase;

// Helpers
inline float safe_clip(float x) {
    if (x > 1.f) return 1.f;
    if (x < -1.f) return -1.f;
    // NaN check
    if (x != x) return 0.f;
    return x;
}

inline float generate_bell(float phase, float harmonic) {
    // Ensure phase wrapping
    while (phase >= 1.f) phase -= 1.f;
    while (phase < 0.f) phase += 1.f;
    
    // Simple FM bell
    float mod_amount = s_strike * 0.5f;
    float modulator = osc_sinf(phase * harmonic * 1.5f);
    float carrier = osc_sinf(phase + modulator * mod_amount);
    
    return carrier;
}

static void init_bell() {
    for (uint8_t i = 0; i < 6; i++) {
        s_voices[i].phase = 0.f;
        s_voices[i].freq_mult = HARMONIC_RATIOS[i];
    }
    
    s_brightness = 0.5f;
    s_decay = 0.5f;
    s_strike = 0.3f;
    s_detune = 0.2f;
    s_attack = 0.05f;
    s_release = 0.4f;
    s_chorus = 0.25f;
    s_tone = 0.5f;
    s_voice_count = 4;
    
    s_env = 0.f;
    s_gate = false;
    s_velocity = 100;
    s_mod_phase = 0.f;
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 1) return k_unit_err_geometry;
    
    s_context = static_cast<const unit_runtime_osc_context_t *>(desc->hooks.runtime_context);
    
    init_bell();
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    init_bell();
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    (void)in;
    
    // Extract note and convert to frequency
    uint8_t note = (s_context->pitch >> 8) & 0xFF;
    uint8_t mod = s_context->pitch & 0xFF;
    const float f0 = osc_w0f_for_note(note, mod);
    
    // ✅ FIX #1: Use FLOAT output (not Q31!)
    for (uint32_t i = 0; i < frames; i++) {
        // === ENVELOPE ===
        if (s_gate) {
            // Attack phase
            if (s_env < 1.f) {
                float attack_rate = 0.1f / (1.f + s_attack * 19.f);
                s_env += attack_rate;
                if (s_env > 1.f) s_env = 1.f;
            }
        } else {
            // Release phase
            float release_rate = 0.999f - s_release * 0.002f;
            s_env *= release_rate;
        }
        
        // Natural decay (always active)
        float decay_rate = 0.9995f - s_decay * 0.0005f;
        s_env *= decay_rate;
        
        // Clamp
        if (s_env < 0.0001f) s_env = 0.f;
        
        // === MODULATION ===
        s_mod_phase += 4.f / 48000.f;
        if (s_mod_phase >= 1.f) s_mod_phase -= 1.f;
        
        float vibrato = osc_sinf(s_mod_phase) * s_chorus * 0.005f;
        
        // === PITCH ===
        float freq = f0 * (1.f + vibrato);
        
        // Clamp frequency
        if (freq > 20000.f) freq = 20000.f;
        if (freq < 20.f) freq = 20.f;
        
        // === VOICE GENERATION ===
        float output = 0.f;
        
        for (uint8_t v = 0; v < s_voice_count; v++) {
            // Detune per voice
            float detune_cents = ((float)(v % 3) - 1.f) * s_detune * 5.f;
            // ✅ FIX #3: EXPONENTIAL detune (not linear!)
            float detune_mult = fastpow2f(detune_cents / 1200.f);
            
            // Harmonic frequency
            float voice_freq = freq * s_voices[v].freq_mult * detune_mult;
            
            // ✅ FIX #5: Clamp frequency BEFORE phase calculation
            if (voice_freq > 20000.f) voice_freq = 20000.f;
            if (voice_freq < 20.f) voice_freq = 20.f;
            
            float phase_inc = voice_freq / 48000.f;
            
            // ✅ CRITICAL: Clamp phase increment to Nyquist
            if (phase_inc > 0.45f) phase_inc = 0.45f;
            
            // Generate
            float partial = generate_bell(s_voices[v].phase, s_voices[v].freq_mult);
            
            // Envelope per voice (higher harmonics decay faster)
            float voice_env = s_env;
            if (v > 0) {
                float decay_mult = 1.f - ((float)v / 6.f) * 0.3f;
                voice_env *= decay_mult;
            }
            
            // Brightness control (affects amplitude of higher harmonics)
            float bright_mult = 1.f;
            if (v > 0) {
                bright_mult = s_brightness;
            }
            
            partial *= voice_env * bright_mult;
            
            // Amplitude scaling
            partial *= 0.5f / (float)(v + 1);
            
            output += partial;
            
            // Advance phase
            s_voices[v].phase += phase_inc;
            while (s_voices[v].phase >= 1.f) s_voices[v].phase -= 1.f;
        }
        
        // Normalize
        output *= 0.6f;
        
        // Velocity
        output *= (float)s_velocity / 127.f;
        
        // Tone shaping
        output = fastertanhf(output * (1.f + s_tone * 0.5f));
        
        // Safety clip
        output = safe_clip(output);
        
        // ✅ FIX #1: Direct float output (not Q31!)
        out[i] = output;
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_brightness = valf; break;
        case 1: s_decay = valf; break;
        case 2: s_strike = valf; break;
        case 3: s_detune = valf; break;
        case 4: s_attack = valf; break;
        case 5: s_release = valf; break;
        case 6: s_chorus = valf; break;
        case 7: s_tone = valf; break;
        case 8: 
            s_voice_count = clipminmaxi32(1, value, 6);
            break;
        case 9:
            // Bell type (not used yet, reserved for future)
            break;
        default: break;
    }
}

// ✅ FIX #4: Return ACTUAL parameter values (not init!)
__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return (int32_t)(s_brightness * 1023.f);
        case 1: return (int32_t)(s_decay * 1023.f);
        case 2: return (int32_t)(s_strike * 1023.f);
        case 3: return (int32_t)(s_detune * 1023.f);
        case 4: return (int32_t)(s_attack * 1023.f);
        case 5: return (int32_t)(s_release * 1023.f);
        case 6: return (int32_t)(s_chorus * 1023.f);
        case 7: return (int32_t)(s_tone * 1023.f);
        case 8: return s_voice_count;
        case 9: return 0;  // Bell type (not used)
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    if (id == 8) {
        static const char *voice_str[] = {"1", "2", "3", "4", "5", "6"};
        int idx = value - 1;
        if (idx >= 0 && idx < 6) return voice_str[idx];
    }
    if (id == 9) {
        static const char *type_str[] = {"TUBULAR", "CHURCH", "GLASS", "METAL"};
        if (value >= 0 && value < 4) return type_str[value];
    }
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    (void)tempo;
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
    (void)note;
    s_gate = true;
    s_velocity = velocity;
    // ✅ FIX #2: Start envelope at 0 (smooth attack, no click!)
    s_env = 0.f;
    
    // Reset phases
    for (uint8_t i = 0; i < 6; i++) {
        s_voices[i].phase = 0.f;
    }
}

__unit_callback void unit_note_off(uint8_t note) {
    (void)note;
    s_gate = false;
}

__unit_callback void unit_all_note_off() {
    s_gate = false;
}
