/*
    HOUSE STACK - Ultimate Chord/Lead Oscillator
    
    Perfect for house, melodic techno, progressive
*/

#include "unit_osc.h"
#include "osc_api.h"
#include "fx_api.h"  // For fx_pow2f()
#include "utils/float_math.h"
#include "utils/int_math.h"

// ========== WAVEFORM TYPES ==========

enum WaveType {
    WAVE_MELLOW = 0,   // Triangle/sine blend
    WAVE_SAW,          // Classic saw
    WAVE_SQUARE,       // Pulse/square
    WAVE_ORGAN,        // Drawbar additive
    WAVE_DIGITAL       // Bright digital
};

const char* wave_names[5] = {
    "MELLOW",
    "SAW",
    "SQUARE",
    "ORGAN",
    "DIGITAL"
};

// ========== CHORD INTERVALS ==========

const float chord_intervals[16][4] = {
    {0.f, 0.f, 0.f, 0.f},      // 0: UNISON (no chord)
    {4.f, 0.f, 0.f, 0.f},      // 1: MAJ3
    {3.f, 0.f, 0.f, 0.f},      // 2: MIN3
    {7.f, 0.f, 0.f, 0.f},      // 3: P5TH
    {11.f, 0.f, 0.f, 0.f},     // 4: MAJ7
    {10.f, 0.f, 0.f, 0.f},     // 5: MIN7
    {12.f, 0.f, 0.f, 0.f},     // 6: OCT
    {14.f, 0.f, 0.f, 0.f},     // 7: 9TH
    {17.f, 0.f, 0.f, 0.f},     // 8: 11TH
    {5.f, 0.f, 0.f, 0.f},      // 9: SUS4
    {2.f, 0.f, 0.f, 0.f},      // 10: SUS2
    {8.f, 0.f, 0.f, 0.f},      // 11: AUG
    {6.f, 0.f, 0.f, 0.f},      // 12: DIM
    {0.f, 4.f, 7.f, 0.f},      // 13: MAJ CHORD (triad)
    {0.f, 3.f, 7.f, 0.f},      // 14: MIN CHORD (triad)
    {0.f, 4.f, 7.f, 10.f}      // 15: DOM7 (4 notes)
};

const char* chord_names[16] = {
    "UNISON", "MAJ3", "MIN3", "P5TH",
    "MAJ7", "MIN7", "OCT", "9TH",
    "11TH", "SUS4", "SUS2", "AUG",
    "DIM", "MAJCHRD", "MINCHRD", "DOM7"
};

// ========== VOICE STRUCTURE ==========

struct Voice {
    // Oscillator phases (3 internal voices)
    float phase_main;
    float phase_detune;
    float phase_chord[4];  // Up to 4 chord notes
    
    // Frequency
    float w0;              // Base frequency
    float w0_target;       // For glide
    
    // Envelope
    float amp_env;
    
    // LFO
    float lfo_phase;
    
    bool active;
};

static Voice s_voice;

// ========== PARAMETERS ==========

static uint8_t s_wave_type = WAVE_SAW;
static float s_detune = 0.4f;
static float s_stereo_spread = 0.5f;
static uint8_t s_chord_interval = 0;  // Unison
static float s_chord_spread = 0.3f;
static float s_tone = 0.5f;  // Neutral
static float s_attack = 0.05f;
static float s_harmonic_bend = 0.2f;
static float s_glide = 0.0f;
static float s_mod_amount = 0.2f;

// ========== POLY BLEP ==========

inline float poly_blep(float t, float dt) {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.f;
    }
    else if (t > 1.f - dt) {
        t = (t - 1.f) / dt;
        return t * t + t + t + 1.f;
    }
    return 0.f;
}

// ========== WAVEFORM GENERATORS ==========

inline float generate_mellow(float phase, float w) {
    // Triangle/sine blend
    float tri = (phase < 0.5f) ? (4.f * phase - 1.f) : (3.f - 4.f * phase);
    float sine = osc_sinf(phase);
    return tri * 0.7f + sine * 0.3f;
}

inline float generate_saw(float phase, float w) {
    float saw = 2.f * phase - 1.f;
    saw -= poly_blep(phase, w);
    return saw;
}

inline float generate_square(float phase, float w) {
    float pulse_width = 0.5f;
    float square = (phase < pulse_width) ? 1.f : -1.f;
    
    square += poly_blep(phase, w);
    
    float phase_shifted = phase + (1.f - pulse_width);
    if (phase_shifted >= 1.f) phase_shifted -= 1.f;
    square -= poly_blep(phase_shifted, w);
    
    return square;
}

inline float generate_organ(float phase) {
    // Additive synthesis (drawbar style)
    float sum = 0.f;
    sum += osc_sinf(phase) * 0.5f;          // Fundamental
    sum += osc_sinf(phase * 2.f) * 0.3f;    // Octave
    sum += osc_sinf(phase * 3.f) * 0.15f;   // Fifth
    sum += osc_sinf(phase * 4.f) * 0.05f;   // 2 octaves
    return sum;
}

inline float generate_digital(float phase, float w) {
    // Bright digital waveform
    float saw = generate_saw(phase, w);
    float square = generate_square(phase, w);
    return saw * 0.6f + square * 0.4f;
}

// ========== WAVEFORM SELECTOR ==========

inline float generate_waveform(float phase, float w) {
    switch (s_wave_type) {
        case WAVE_MELLOW:
            return generate_mellow(phase, w);
        case WAVE_SAW:
            return generate_saw(phase, w);
        case WAVE_SQUARE:
            return generate_square(phase, w);
        case WAVE_ORGAN:
            return generate_organ(phase);
        case WAVE_DIGITAL:
            return generate_digital(phase, w);
        default:
            return generate_saw(phase, w);
    }
}

// ========== HARMONIC BENDING ==========

inline float apply_harmonic_bend(float input) {
    if (s_harmonic_bend < 0.01f) return input;
    
    // Waveshaping (adds harmonics)
    float amount = s_harmonic_bend * 2.f;
    float bent = fastertanh2f(input * (1.f + amount));
    
    return input * (1.f - s_harmonic_bend) + bent * s_harmonic_bend;
}

// ========== TILT EQ ==========

static float s_tilt_hp_z1 = 0.f;
static float s_tilt_lp_z1 = 0.f;

inline float apply_tilt_eq(float input) {
    // Simple tilt: <50% = darker, >50% = brighter
    float tilt = (s_tone - 0.5f) * 2.f;  // -1 to +1
    
    // High shelf (brightness)
    float hp = input - s_tilt_hp_z1;
    s_tilt_hp_z1 = s_tilt_hp_z1 + 0.3f * (input - s_tilt_hp_z1);
    
    // Low shelf (warmth)
    s_tilt_lp_z1 = s_tilt_lp_z1 + 0.3f * (input - s_tilt_lp_z1);
    
    // Denormal kill
    if (si_fabsf(s_tilt_hp_z1) < 1e-15f) s_tilt_hp_z1 = 0.f;
    if (si_fabsf(s_tilt_lp_z1) < 1e-15f) s_tilt_lp_z1 = 0.f;
    
    // Mix based on tilt
    if (tilt > 0.f) {
        // Brighter
        return input + hp * tilt * 0.5f;
    } else {
        // Darker
        return s_tilt_lp_z1 + input * (1.f + tilt);
    }
}

// ========== LFO ==========

inline float get_lfo_value() {
    // Simple sine LFO at ~3 Hz
    s_voice.lfo_phase += 3.f / 48000.f;
    if (s_voice.lfo_phase >= 1.f) s_voice.lfo_phase -= 1.f;
    
    return osc_sinf(s_voice.lfo_phase);
}

// ========== MAIN OSCILLATOR ==========

inline float generate_oscillator() {
    if (!s_voice.active) {
        return 0.f;
    }
    
    // Get LFO
    float lfo = get_lfo_value() * s_mod_amount;
    
    // Apply glide (portamento)
    if (s_glide > 0.01f) {
        float glide_coeff = 1.f - (s_glide * 0.999f);
        glide_coeff = clipminmaxf(0.9f, glide_coeff, 0.9999f);
        s_voice.w0 += (s_voice.w0_target - s_voice.w0) * (1.f - glide_coeff);
    } else {
        s_voice.w0 = s_voice.w0_target;
    }
    
    float sum = 0.f;
    
    // ===== VOICE 1: MAIN =====
    float w_main = s_voice.w0;
    w_main = clipminmaxf(0.0001f, w_main, 0.45f);
    
    float osc_main = generate_waveform(s_voice.phase_main, w_main);
    osc_main = apply_harmonic_bend(osc_main);
    
    sum += osc_main * 0.5f;
    
    s_voice.phase_main += w_main;
    if (s_voice.phase_main >= 1.f) s_voice.phase_main -= 1.f;
    
    // ===== VOICE 2: DETUNE =====
    if (s_detune > 0.01f) {
        // Detune amount (±20 cents max)
        float detune_cents = s_detune * 20.f;
        float detune_ratio = fx_pow2f(detune_cents / 1200.f);
        
        // Add LFO modulation
        detune_ratio *= (1.f + lfo * 0.01f);
        
        float w_detune = s_voice.w0 * detune_ratio;
        w_detune = clipminmaxf(0.0001f, w_detune, 0.45f);
        
        float osc_detune = generate_waveform(s_voice.phase_detune, w_detune);
        osc_detune = apply_harmonic_bend(osc_detune);
        
        // Stereo spread (panning effect)
        float spread = s_stereo_spread;
        float pan = (lfo * 0.5f + 0.5f) * spread;  // 0 to spread
        sum += osc_detune * 0.3f * (1.f + pan * 0.3f);
        
        s_voice.phase_detune += w_detune;
        if (s_voice.phase_detune >= 1.f) s_voice.phase_detune -= 1.f;
    }
    
    // ===== VOICE 3: CHORD =====
    if (s_chord_interval > 0) {
        const float *intervals = chord_intervals[s_chord_interval];
        
        for (int c = 0; c < 4; c++) {
            float interval = intervals[c];
            if (interval == 0.f && c > 0) break;  // No more notes
            
            // Convert semitones to ratio
            float chord_ratio = fx_pow2f(interval / 12.f);
            
            // Add chord spread modulation
            chord_ratio *= (1.f + lfo * s_chord_spread * 0.02f);
            
            float w_chord = s_voice.w0 * chord_ratio;
            w_chord = clipminmaxf(0.0001f, w_chord, 0.45f);
            
            float osc_chord = generate_waveform(s_voice.phase_chord[c], w_chord);
            osc_chord = apply_harmonic_bend(osc_chord);
            
            // Spread in stereo (panning effect)
            float pan = ((float)c / 3.f - 0.5f) * s_chord_spread;
            sum += osc_chord * 0.2f * (1.f + pan * 0.2f);
            
            s_voice.phase_chord[c] += w_chord;
            if (s_voice.phase_chord[c] >= 1.f) s_voice.phase_chord[c] -= 1.f;
        }
    }
    
    // Apply tilt EQ
    sum = apply_tilt_eq(sum);
    
    // Apply envelope
    sum *= s_voice.amp_env;
    
    return sum;
}

// ========== ENVELOPE ==========

inline void process_envelope() {
    if (!s_voice.active) return;
    
    // Attack envelope
    float target = 1.f;
    float attack_coeff = 0.9999f - (s_attack * s_attack * 0.999f);
    attack_coeff = clipminmaxf(0.95f, attack_coeff, 0.9999f);
    
    s_voice.amp_env += (target - s_voice.amp_env) * (1.f - attack_coeff);
    
    if (s_voice.amp_env > 0.999f) s_voice.amp_env = 1.f;
}

// ========== UNIT CALLBACKS ==========

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    
    // Init voice
    s_voice.phase_main = 0.f;
    s_voice.phase_detune = 0.f;
    for (int i = 0; i < 4; i++) {
        s_voice.phase_chord[i] = 0.f;
    }
    s_voice.w0 = 0.f;
    s_voice.w0_target = 0.f;
    s_voice.amp_env = 0.f;
    s_voice.lfo_phase = 0.f;
    s_voice.active = false;
    
    // Init tilt EQ
    s_tilt_hp_z1 = 0.f;
    s_tilt_lp_z1 = 0.f;
    
    // Init parameters
    s_wave_type = WAVE_SAW;
    s_detune = 0.4f;
    s_stereo_spread = 0.5f;
    s_chord_interval = 0;
    s_chord_spread = 0.3f;
    s_tone = 0.5f;
    s_attack = 0.05f;
    s_harmonic_bend = 0.2f;
    s_glide = 0.0f;
    s_mod_amount = 0.2f;
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    s_voice.active = false;
    s_voice.amp_env = 0.f;
    s_tilt_hp_z1 = 0.f;
    s_tilt_lp_z1 = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    (void)in;
    
    for (uint32_t f = 0; f < frames; f++) {
        // Process envelope
        process_envelope();
        
        // Generate oscillator
        float sample = generate_oscillator();
        
        // Output gain
        sample *= 1.5f;
        
        // Limiting
        sample = clipminmaxf(-1.f, sample, 1.f);
        
        // Mono output (oscillators are mono)
        out[f] = sample;
    }
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
    (void)velocity;
    
    // Reset phases
    s_voice.phase_main = 0.f;
    s_voice.phase_detune = 0.f;
    for (int i = 0; i < 4; i++) {
        s_voice.phase_chord[i] = 0.f;
    }
    
    // Set target frequency (for glide)
    s_voice.w0_target = osc_w0f_for_note(note, 0);
    
    // If no glide, set immediately
    if (s_glide < 0.01f) {
        s_voice.w0 = s_voice.w0_target;
    }
    
    // Activate
    s_voice.active = true;
    s_voice.amp_env = 0.f;  // Start attack
}

__unit_callback void unit_note_off(uint8_t note) {
    (void)note;
    s_voice.active = false;
}

__unit_callback void unit_all_note_off() {
    s_voice.active = false;
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

// ========== PARAMETER HANDLING ==========

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: // Wave Type
            s_wave_type = (uint8_t)value;
            break;
            
        case 1: // Detune
            s_detune = valf;
            break;
            
        case 2: // Stereo Spread
            s_stereo_spread = valf;
            break;
            
        case 3: // Chord Interval
            s_chord_interval = (uint8_t)value;
            break;
            
        case 4: // Chord Spread
            s_chord_spread = valf;
            break;
            
        case 5: // Tone
            s_tone = valf;
            break;
            
        case 6: // Attack
            s_attack = valf;
            break;
            
        case 7: // Harmonic Bend
            s_harmonic_bend = valf;
            break;
            
        case 8: // Glide
            s_glide = valf;
            break;
            
        case 9: // Mod Amount
            s_mod_amount = valf;
            break;
            
        default:
            break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return s_wave_type;
        case 1: return (int32_t)(s_detune * 1023.f);
        case 2: return (int32_t)(s_stereo_spread * 1023.f);
        case 3: return s_chord_interval;
        case 4: return (int32_t)(s_chord_spread * 1023.f);
        case 5: return (int32_t)(s_tone * 1023.f);
        case 6: return (int32_t)(s_attack * 1023.f);
        case 7: return (int32_t)(s_harmonic_bend * 1023.f);
        case 8: return (int32_t)(s_glide * 1023.f);
        case 9: return (int32_t)(s_mod_amount * 1023.f);
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    if (id == 0 && value >= 0 && value < 5) {
        return wave_names[value];
    }
    if (id == 3 && value >= 0 && value < 16) {
        return chord_names[value];
    }
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    (void)tempo;
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    (void)counter;
}

