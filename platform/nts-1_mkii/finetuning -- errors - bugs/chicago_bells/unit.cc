/*
    CHICAGO BELLS V3 - Multi-Type Bell Synthesizer
    
    TYPE 0: COWBELL (808-style square wave bell)
    TYPE 1: CHURCH (FM bell with inharmonic partials)
    TYPE 2: AGOGO (High pitched FM percussion)
    TYPE 3: GONG (Inharmonic ring modulation)
*/

#include "unit_osc.h"
#include "osc_api.h"
#include "utils/float_math.h"
#include "utils/int_math.h"
#include "macros.h"

// ═══════════════════════════════════════════════════════════════════════════
// TYPES
// ═══════════════════════════════════════════════════════════════════════════

enum BellType {
    BELL_COWBELL = 0,
    BELL_CHURCH = 1,
    BELL_AGOGO = 2,
    BELL_GONG = 3
};

struct Voice {
    float phase;
};

// ═══════════════════════════════════════════════════════════════════════════
// GLOBAL STATE
// ═══════════════════════════════════════════════════════════════════════════

static const unit_runtime_osc_context_t *s_context;

static Voice s_voices[6];  // 6 partials max

// Parameters
static float s_tone;       // Brightness/FM index
static float s_decay;      // Decay time
static BellType s_type;    // Bell type
static float s_strike;     // Strike hardness
static float s_detune;     // Chorus/detune
static float s_bite;       // Attack transient
static float s_ring;       // Ring modulation
static float s_dirt;       // Saturation
static float s_air;        // High frequency content

// State
static float s_amp_env;
static float s_pitch_env;
static bool s_gate;
static uint8_t s_velocity;

// Bell harmonic ratios (SAFE - below Nyquist)
// Source: Fletcher & Rossing "Physics of Musical Instruments"
static const float CHURCH_RATIOS[6] = {
    1.0f,    // Fundamental (hum tone)
    2.0f,    // Prime
    2.4f,    // Tierce
    3.0f,    // Quint
    4.0f,    // Nominal
    5.0f     // Superquint
};

static const float CHURCH_AMPS[6] = {
    0.8f,    // Strong fundamental
    0.6f,    // Prime
    0.5f,    // Tierce
    0.4f,    // Quint
    0.3f,    // Nominal
    0.2f     // Superquint
};

// ═══════════════════════════════════════════════════════════════════════════
// HELPERS
// ═══════════════════════════════════════════════════════════════════════════

inline float safe_clip(float x) {
    if (x > 1.f) return 1.f;
    if (x < -1.f) return -1.f;
    // Check for NaN/Inf
    if (x != x) return 0.f;  // NaN check
    if (x > 1e10f || x < -1e10f) return 0.f;  // Inf check
    return x;
}

// PolyBLEP for anti-aliased square wave
inline float polyblep(float phase, float phase_inc) {
    float value = 0.f;
    
    if (phase < phase_inc) {
        float t = phase / phase_inc;
        value = t + t - t * t - 1.f;
    } else if (phase > 1.f - phase_inc) {
        float t = (phase - 1.f) / phase_inc;
        value = t * t + t + t + 1.f;
    }
    
    return value;
}

inline float square_wave(float phase, float phase_inc) {
    float naive = (phase < 0.5f) ? 1.f : -1.f;
    return naive - polyblep(phase, phase_inc);
}

// ═══════════════════════════════════════════════════════════════════════════
// TYPE 0: COWBELL (808-style)
// ═══════════════════════════════════════════════════════════════════════════

inline float generate_cowbell(float freq) {
    // Two detuned square waves (classic 808 cowbell recipe)
    float freq1 = freq;
    float freq2 = freq * 1.5f;  // Perfect 5th up
    
    // Detune slightly
    freq1 *= 1.f - s_detune * 0.01f;
    freq2 *= 1.f + s_detune * 0.01f;
    
    // Clamp
    if (freq1 > 20000.f) freq1 = 20000.f;
    if (freq2 > 20000.f) freq2 = 20000.f;
    
    float phase_inc1 = freq1 / 48000.f;
    float phase_inc2 = freq2 / 48000.f;
    
    // Generate anti-aliased square waves
    float sq1 = square_wave(s_voices[0].phase, phase_inc1);
    float sq2 = square_wave(s_voices[1].phase, phase_inc2);
    
    // Advance phases
    s_voices[0].phase += phase_inc1;
    s_voices[1].phase += phase_inc2;
    while (s_voices[0].phase >= 1.f) s_voices[0].phase -= 1.f;
    while (s_voices[1].phase >= 1.f) s_voices[1].phase -= 1.f;
    
    // Mix (voice 2 quieter)
    float output = sq1 * 0.6f + sq2 * 0.4f;
    
    // Simple bandpass effect (boost mids)
    output *= (1.f + s_tone * 0.5f);
    
    return output * 0.5f;
}

// ═══════════════════════════════════════════════════════════════════════════
// TYPE 1: CHURCH BELL (FM with inharmonic partials)
// ═══════════════════════════════════════════════════════════════════════════

inline float generate_church(float freq) {
    float output = 0.f;
    
    // FM index controlled by TONE
    float fm_index = s_tone * 2.f;
    
    // Generate 6 inharmonic partials
    for (uint8_t i = 0; i < 6; i++) {
        float partial_freq = freq * CHURCH_RATIOS[i];
        
        // Add slight inharmonicity (real bells are not perfectly harmonic)
        float inharmonicity = 1.f + (float)i * 0.002f * s_ring;
        partial_freq *= inharmonicity;
        
        // Detune for chorus
        float detune_cents = ((float)(i % 3) - 1.f) * s_detune * 3.f;
        partial_freq *= 1.f + (detune_cents / 1200.f);
        
        // Clamp
        if (partial_freq > 20000.f) continue;  // Skip if too high
        
        float phase_inc = partial_freq / 48000.f;
        
        // Simple FM (modulator at same frequency)
        float modulator = osc_sinf(s_voices[i].phase) * fm_index;
        float carrier = osc_sinf(s_voices[i].phase + modulator);
        
        // Apply amplitude envelope (higher partials decay faster)
        float partial_env = s_amp_env;
        if (i > 0) {
            float decay_mult = 1.f - ((float)i / 6.f) * 0.5f;
            partial_env *= decay_mult;
        }
        
        // Brightness control (affects higher partials)
        float brightness_mult = CHURCH_AMPS[i];
        if (i > 2) {
            brightness_mult *= s_air;
        }
        
        output += carrier * brightness_mult * partial_env;
        
        // Advance phase
        s_voices[i].phase += phase_inc;
        while (s_voices[i].phase >= 1.f) s_voices[i].phase -= 1.f;
    }
    
    return output * 0.3f;
}

// ═══════════════════════════════════════════════════════════════════════════
// TYPE 2: AGOGO (High FM percussion)
// ═══════════════════════════════════════════════════════════════════════════

inline float generate_agogo(float freq) {
    // High pitched, short decay metallic sound
    float carrier_freq = freq * 2.f;  // One octave up
    float mod_freq = carrier_freq * 3.5f;  // High ratio for metallic sound
    
    // Clamp
    if (carrier_freq > 20000.f) carrier_freq = 20000.f;
    if (mod_freq > 20000.f) mod_freq = 20000.f;
    
    float carrier_inc = carrier_freq / 48000.f;
    float mod_inc = mod_freq / 48000.f;
    
    // FM synthesis
    float fm_index = s_tone * 3.f + s_strike * 2.f;
    float modulator = osc_sinf(s_voices[1].phase) * fm_index;
    float carrier = osc_sinf(s_voices[0].phase + modulator);
    
    // Advance phases
    s_voices[0].phase += carrier_inc;
    s_voices[1].phase += mod_inc;
    while (s_voices[0].phase >= 1.f) s_voices[0].phase -= 1.f;
    while (s_voices[1].phase >= 1.f) s_voices[1].phase -= 1.f;
    
    // Add some detuned harmonics for thickness
    float harm2_freq = carrier_freq * 2.f * (1.f + s_detune * 0.02f);
    if (harm2_freq < 20000.f) {
        float harm2_inc = harm2_freq / 48000.f;
        float harm2 = osc_sinf(s_voices[2].phase) * 0.3f;
        carrier += harm2;
        
        s_voices[2].phase += harm2_inc;
        while (s_voices[2].phase >= 1.f) s_voices[2].phase -= 1.f;
    }
    
    return carrier * 0.4f;
}

// ═══════════════════════════════════════════════════════════════════════════
// TYPE 3: GONG (Industrial ring modulation)
// ═══════════════════════════════════════════════════════════════════════════

inline float generate_gong(float freq) {
    // Inharmonic, noisy, industrial sound
    
    // Two inharmonic frequencies
    float freq1 = freq;
    float freq2 = freq * 1.414f;  // Sqrt(2) - very inharmonic
    
    // Add ring modulation
    freq2 *= 1.f + s_ring * 0.3f;
    
    // Clamp
    if (freq1 > 20000.f) freq1 = 20000.f;
    if (freq2 > 20000.f) freq2 = 20000.f;
    
    float phase_inc1 = freq1 / 48000.f;
    float phase_inc2 = freq2 / 48000.f;
    
    // Generate waves (sine for freq1, triangle for freq2)
    float sine = osc_sinf(s_voices[0].phase);
    
    // Triangle wave
    float tri_phase = s_voices[1].phase;
    float triangle = (tri_phase < 0.5f) ? (4.f * tri_phase - 1.f) : (3.f - 4.f * tri_phase);
    
    // Ring modulation
    float ring = sine * triangle;
    
    // Mix with some pure sine for body
    float output = sine * 0.3f + ring * 0.7f;
    
    // Advance phases
    s_voices[0].phase += phase_inc1;
    s_voices[1].phase += phase_inc2;
    while (s_voices[0].phase >= 1.f) s_voices[0].phase -= 1.f;
    while (s_voices[1].phase >= 1.f) s_voices[1].phase -= 1.f;
    
    // Add upper partials for brightness
    if (s_air > 0.3f) {
        float freq3 = freq * 3.f * (1.f + s_detune * 0.05f);
        if (freq3 < 20000.f) {
            float phase_inc3 = freq3 / 48000.f;
            float harm = osc_sinf(s_voices[2].phase) * 0.2f * s_air;
            output += harm;
            
            s_voices[2].phase += phase_inc3;
            while (s_voices[2].phase >= 1.f) s_voices[2].phase -= 1.f;
        }
    }
    
    return output * 0.5f;
}

// ═══════════════════════════════════════════════════════════════════════════
// INITIALIZATION
// ═══════════════════════════════════════════════════════════════════════════

static void init_bells() {
    // Reset voices
    for (uint8_t i = 0; i < 6; i++) {
        s_voices[i].phase = 0.f;
    }
    
    // Init parameters
    s_tone = 0.5f;
    s_decay = 0.5f;
    s_type = BELL_COWBELL;
    s_strike = 0.4f;
    s_detune = 0.25f;
    s_bite = 0.3f;
    s_ring = 0.3f;
    s_dirt = 0.0f;
    s_air = 0.5f;
    
    // Init state
    s_amp_env = 0.f;
    s_pitch_env = 0.f;
    s_gate = false;
    s_velocity = 100;
}

// ═══════════════════════════════════════════════════════════════════════════
// UNIT CALLBACKS
// ═══════════════════════════════════════════════════════════════════════════

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->input_channels != 2 || desc->output_channels != 1) return k_unit_err_geometry;
    
    s_context = static_cast<const unit_runtime_osc_context_t *>(desc->hooks.runtime_context);
    
    init_bells();
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    init_bells();
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    (void)in;
    
    // Get pitch from context
    const float f0 = s_context->pitch;
    
    q31_t * __restrict y = (q31_t *)out;
    
    for (uint32_t i = 0; i < frames; i++) {
        // ========== ENVELOPES ==========
        
        if (s_gate) {
            // Decay (always active when gate on)
            float decay_rate = 0.9990f + s_decay * 0.0009f;
            s_amp_env *= decay_rate;
        } else {
            // Release
            s_amp_env *= 0.9995f;
        }
        
        // Pitch envelope (for bite/strike)
        s_pitch_env *= 0.995f;
        
        // Clamp
        if (s_amp_env < 0.0001f) s_amp_env = 0.f;
        if (s_pitch_env < 0.001f) s_pitch_env = 0.f;
        
        // ========== PITCH ==========
        
        // Strike transient (pitch envelope)
        float pitch_bend = s_pitch_env * s_strike * 0.1f;
        float freq = f0 * (1.f + pitch_bend);
        
        // Safety clamp
        if (freq > 20000.f) freq = 20000.f;
        if (freq < 20.f) freq = 20.f;
        
        // ========== SYNTHESIS (TYPE DEPENDENT) ==========
        
        float output = 0.f;
        
        switch (s_type) {
            case BELL_COWBELL:
                output = generate_cowbell(freq);
                break;
            
            case BELL_CHURCH:
                output = generate_church(freq);
                break;
            
            case BELL_AGOGO:
                output = generate_agogo(freq);
                break;
            
            case BELL_GONG:
                output = generate_gong(freq);
                break;
        }
        
        // Apply amplitude envelope
        output *= s_amp_env;
        
        // ========== DIRT (SATURATION) ==========
        
        if (s_dirt > 0.01f) {
            float drive = 1.f + s_dirt * 3.f;
            output *= drive;
            output = fastertanhf(output);
        }
        
        // ========== FINAL ==========
        
        // Velocity
        output *= (float)s_velocity / 127.f;
        
        // CRITICAL: 2.5x output gain for NTS-1 mkII
        output *= 2.5f;
        
        // Safety clip
        output = safe_clip(output);
        
        // Convert to Q31
        y[i] = f32_to_q31(output);
    }
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_tone = valf; break;           // Knob A
        case 1: s_decay = valf; break;          // Knob B
        case 2: s_type = (BellType)value; break; // TYPE
        case 3: s_strike = valf; break;         // STRIKE
        case 4: s_detune = valf; break;         // DETUNE
        case 5: s_bite = valf; break;           // BITE
        case 6: s_ring = valf; break;           // RING
        case 7: s_dirt = valf; break;           // DIRT
        case 8: s_air = valf; break;            // AIR
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return (int32_t)(s_tone * 1023.f);
        case 1: return (int32_t)(s_decay * 1023.f);
        case 2: return (int32_t)s_type;
        case 3: return (int32_t)(s_strike * 1023.f);
        case 4: return (int32_t)(s_detune * 1023.f);
        case 5: return (int32_t)(s_bite * 1023.f);
        case 6: return (int32_t)(s_ring * 1023.f);
        case 7: return (int32_t)(s_dirt * 1023.f);
        case 8: return (int32_t)(s_air * 1023.f);
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    if (id == 2) {
        static const char *type_names[] = {"COWBELL", "CHURCH", "AGOGO", "GONG"};
        if (value >= 0 && value < 4) return type_names[value];
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
    
    // Full amplitude on strike
    s_amp_env = 1.f;
    
    // Pitch envelope for transient
    s_pitch_env = s_bite;
    
    // Reset phases for clean attack
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

__unit_callback void unit_pitch_bend(uint16_t bend) {
    (void)bend;
}

__unit_callback void unit_channel_pressure(uint8_t pressure) {
    (void)pressure;
}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t aftertouch) {
    (void)note;
    (void)aftertouch;
}
