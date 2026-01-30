/*
    HOUSEKUT - Nederhouse/Euro-House Melodic Lead Oscillator
    
    MELODIC DANCE LEADS:
    - House Bells (FM-like bright bells)
    - Trance Leads (Supersaw-style emotional)
    - Nederhouse Piano (Warm, smooth piano)
    - Classic Club (90s dance synth)
    
    NO NOISE, NO SCREECH, PURE MUSICAL VIBES!
    
    Based on Korg logue SDK
    https://github.com/korginc/logue-sdk
*/

#include "unit_osc.h"
#include "osc_api.h"

// ========== NaN/Inf CHECK MACRO ==========
#define is_finite(x) ((x) != (x) ? false : ((x) <= 1e10f && (x) >= -1e10f))

// ========== CHARACTER MODES ==========

enum Character {
    CHAR_BELLS = 0,      // House Bells (FM-like)
    CHAR_TRANCE = 1,     // Trance Leads (Supersaw)
    CHAR_PIANO = 2,      // Nederhouse Piano
    CHAR_CLASSIC = 3     // Classic Club Lead
};

const char* character_names[4] = {
    "BELLS", "TRANCE", "PIANO", "CLASSIC"
};

// ========== VOICE STATE ==========

struct Voice {
    float phase;
    float phase_detune1;
    float phase_detune2;
    float phase_detune3;
    float w0;
    float glide_phase;
    float target_pitch;
    float current_pitch;
    float attack_env;
    float vibrato_phase;
    float motion_phase;
    bool active;
    uint8_t note;
};

static Voice s_voice;

// ========== PARAMETERS ==========

static Character s_character = CHAR_TRANCE;
static float s_detune = 0.5f;
static float s_brightness = 0.6f;
static float s_motion = 0.3f;
static float s_attack = 0.25f;
static float s_glide = 0.2f;
static float s_vibrato = 0.3f;
static float s_warmth = 0.5f;
static float s_flavor = 0.33f;
static float s_sustain = 0.75f;

// ========== OSCILLATOR FUNCTIONS ==========

// Pure saw
inline float osc_saw(float phase) {
    return 2.f * phase - 1.f;
}

// Pure pulse with PWM
inline float osc_pulse(float phase, float pw) {
    return (phase < pw) ? 1.f : -1.f;
}

// Sine
inline float osc_sine(float phase) {
    return osc_sinf(phase);
}

// Triangle
inline float osc_tri(float phase) {
    if (phase < 0.5f) {
        return 4.f * phase - 1.f;
    } else {
        return 3.f - 4.f * phase;
    }
}

// ========== HOUSE BELLS (FM-like) ==========

inline float generate_bells(float phase, float motion) {
    // FM-style bell tone
    float carrier = osc_sine(phase);
    
    // Modulator for bell-like timbre
    float mod_ratio = 3.5f + s_brightness * 2.5f;
    float mod = osc_sine(phase * mod_ratio);
    
    // Motion adds subtle FM depth variation
    float mod_depth = 0.3f + motion * 0.4f;
    
    float fm = carrier + mod * mod_depth;
    
    // Add harmonics for sparkle
    float harmonic = osc_sine(phase * 2.f) * 0.2f;
    
    return (fm + harmonic) * 0.6f;
}

// ========== TRANCE LEADS (Supersaw-style) ==========

inline float generate_trance(float phase, float dt1, float dt2, float dt3) {
    // 4-voice supersaw
    float saw1 = osc_saw(phase);
    float saw2 = osc_saw(dt1);
    float saw3 = osc_saw(dt2);
    float saw4 = osc_saw(dt3);
    
    // Mix with slight level differences for thickness
    float mix = saw1 * 0.3f + saw2 * 0.25f + saw3 * 0.25f + saw4 * 0.2f;
    
    // Add sub for warmth
    float sub = osc_sine(phase) * s_warmth * 0.3f;
    
    // Brightness control via harmonic emphasis
    float bright_mult = 0.7f + s_brightness * 0.6f;
    
    return (mix * bright_mult + sub) * 0.8f;
}

// ========== NEDERHOUSE PIANO ==========

inline float generate_piano(float phase, float motion) {
    // Piano-like tone: triangle base with harmonics
    float fundamental = osc_tri(phase);
    
    // Piano harmonics
    float h2 = osc_sine(phase * 2.f) * 0.3f;
    float h3 = osc_sine(phase * 3.f) * 0.15f;
    float h4 = osc_sine(phase * 4.f) * 0.1f;
    
    // Motion adds subtle detuned layer
    float detune_phase = phase * (1.f + motion * 0.002f);
    float detune = osc_tri(detune_phase) * 0.2f * s_detune;
    
    // Warmth adds low-mid body
    float body = osc_sine(phase * 0.5f) * s_warmth * 0.2f;
    
    // Brightness shapes harmonic balance
    float bright = (1.f - s_brightness * 0.3f);
    
    return (fundamental + h2 * bright + h3 + h4 + detune + body) * 0.5f;
}

// ========== CLASSIC CLUB LEAD ==========

inline float generate_classic(float phase, float dt1, float pw) {
    // Classic 90s lead: saw + pulse
    float saw = osc_saw(phase);
    float pulse = osc_pulse(phase, pw);
    
    // Detuned layer
    float saw2 = osc_saw(dt1);
    
    // Mix based on flavor (90s = more pulse, modern = more saw)
    float saw_amt = 0.4f + s_flavor * 0.3f;
    float pulse_amt = 0.6f - s_flavor * 0.3f;
    
    float mix = saw * saw_amt + pulse * pulse_amt + saw2 * 0.2f * s_detune;
    
    // Sub for body
    float sub = osc_sine(phase) * s_warmth * 0.25f;
    
    // Brightness filter simulation
    float bright_mult = 0.6f + s_brightness * 0.7f;
    
    return (mix * bright_mult + sub) * 0.7f;
}

// ========== MAIN OSCILLATOR ==========

inline float generate_housekut() {
    if (!s_voice.active) return 0.f;
    
    // Update motion LFO
    s_voice.motion_phase += 0.5f / 48000.f;
    if (s_voice.motion_phase >= 1.f) s_voice.motion_phase -= 1.f;
    
    float motion_lfo = osc_sinf(s_voice.motion_phase) * s_motion;
    
    // Update vibrato
    float vibrato_rate = 4.f + s_vibrato * 3.f;
    s_voice.vibrato_phase += vibrato_rate / 48000.f;
    if (s_voice.vibrato_phase >= 1.f) s_voice.vibrato_phase -= 1.f;
    
    float vibrato = osc_sinf(s_voice.vibrato_phase) * s_vibrato * 0.005f;
    
    // Update attack envelope
    if (s_voice.attack_env < 1.f) {
        float attack_time = 0.001f + s_attack * 0.05f;
        s_voice.attack_env += 1.f / (attack_time * 48000.f);
        if (s_voice.attack_env > 1.f) s_voice.attack_env = 1.f;
    }
    
    // Update glide
    if (si_fabsf(s_voice.current_pitch - s_voice.target_pitch) > 0.0001f) {
        float glide_speed = 0.0001f + s_glide * 0.01f;
        if (s_voice.current_pitch < s_voice.target_pitch) {
            s_voice.current_pitch += glide_speed;
            if (s_voice.current_pitch > s_voice.target_pitch) {
                s_voice.current_pitch = s_voice.target_pitch;
            }
        } else {
            s_voice.current_pitch -= glide_speed;
            if (s_voice.current_pitch < s_voice.target_pitch) {
                s_voice.current_pitch = s_voice.target_pitch;
            }
        }
    }
    
    // Apply pitch modulation
    float pitch_mod = 1.f + vibrato + motion_lfo * 0.002f;
    float w0_mod = s_voice.w0 * pitch_mod;
    
    // Update phases
    s_voice.phase += w0_mod;
    if (s_voice.phase >= 1.f) s_voice.phase -= 1.f;
    
    // Detune amounts based on character
    float detune_amt = s_detune * 0.01f;
    
    s_voice.phase_detune1 += w0_mod * (1.f + detune_amt);
    if (s_voice.phase_detune1 >= 1.f) s_voice.phase_detune1 -= 1.f;
    
    s_voice.phase_detune2 += w0_mod * (1.f - detune_amt);
    if (s_voice.phase_detune2 >= 1.f) s_voice.phase_detune2 -= 1.f;
    
    s_voice.phase_detune3 += w0_mod * (1.f + detune_amt * 0.5f);
    if (s_voice.phase_detune3 >= 1.f) s_voice.phase_detune3 -= 1.f;
    
    // Generate waveform based on character
    float output = 0.f;
    
    // PWM for pulse modes
    float pw = 0.5f + motion_lfo * 0.2f;
    pw = clipminmaxf(0.1f, pw, 0.9f);
    
    switch (s_character) {
        case CHAR_BELLS:
            output = generate_bells(s_voice.phase, motion_lfo);
            break;
            
        case CHAR_TRANCE:
            output = generate_trance(s_voice.phase, 
                                    s_voice.phase_detune1,
                                    s_voice.phase_detune2,
                                    s_voice.phase_detune3);
            break;
            
        case CHAR_PIANO:
            output = generate_piano(s_voice.phase, motion_lfo);
            break;
            
        case CHAR_CLASSIC:
            output = generate_classic(s_voice.phase, 
                                     s_voice.phase_detune1, 
                                     pw);
            break;
    }
    
    // Apply attack envelope
    output *= s_voice.attack_env;
    
    // Apply sustain behavior
    float sustain_mult = 0.3f + s_sustain * 0.7f;
    output *= sustain_mult;
    
    // Output gain boost (to match other oscillators)
    output *= 2.0f;
    
    // Validate output
    if (!is_finite(output)) output = 0.f;
    
    return clipminmaxf(-1.f, output, 1.f);
}

// ========== CALLBACKS ==========

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    
    // Init voice
    s_voice.phase = 0.f;
    s_voice.phase_detune1 = 0.f;
    s_voice.phase_detune2 = 0.f;
    s_voice.phase_detune3 = 0.f;
    s_voice.w0 = 0.f;
    s_voice.glide_phase = 0.f;
    s_voice.target_pitch = 0.f;
    s_voice.current_pitch = 0.f;
    s_voice.attack_env = 0.f;
    s_voice.vibrato_phase = 0.f;
    s_voice.motion_phase = 0.f;
    s_voice.active = false;
    s_voice.note = 0;
    
    // Init parameters
    s_character = CHAR_TRANCE;
    s_detune = 0.5f;
    s_brightness = 0.6f;
    s_motion = 0.3f;
    s_attack = 0.25f;
    s_glide = 0.2f;
    s_vibrato = 0.3f;
    s_warmth = 0.5f;
    s_flavor = 0.33f;
    s_sustain = 0.75f;
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    s_voice.phase = 0.f;
    s_voice.phase_detune1 = 0.f;
    s_voice.phase_detune2 = 0.f;
    s_voice.phase_detune3 = 0.f;
    s_voice.attack_env = 0.f;
    s_voice.vibrato_phase = 0.f;
    s_voice.motion_phase = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    (void)in;
    
    for (uint32_t f = 0; f < frames; f++) {
        float sample = generate_housekut();
        out[f] = sample;
    }
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
    s_voice.note = note;
    s_voice.target_pitch = osc_notehzf(note);
    
    // Glide only if already active
    if (!s_voice.active || s_glide < 0.01f) {
        s_voice.current_pitch = s_voice.target_pitch;
    }
    
    s_voice.w0 = s_voice.current_pitch / 48000.f;
    s_voice.active = true;
    
    // Reset attack envelope
    s_voice.attack_env = 0.f;
    
    (void)velocity;
}

__unit_callback void unit_note_off(uint8_t note) {
    if (note == s_voice.note) {
        // Sustain behavior: don't immediately stop
        // Let NTS-1 envelope handle release
        if (s_sustain < 0.3f) {
            s_voice.active = false;
        }
    }
}

__unit_callback void unit_all_note_off() {
    s_voice.active = false;
}

__unit_callback void unit_pitch_bend(uint16_t bend) {
    // Pitch bend range: ±2 semitones
    float bend_normalized = ((float)bend - 8192.f) / 8192.f;
    // Calculate 2^(bend_semitones/12) manually for pitch bend
    float bend_semitones = bend_normalized * 2.f;
    float bend_ratio = 1.f;
    if (bend_semitones != 0.f) {
        // Simple approximation: 2^(x/12) ≈ 1 + x/12 * 0.693
        bend_ratio = 1.f + (bend_semitones / 12.f) * 0.693f;
    }
    
    s_voice.w0 = (s_voice.current_pitch * bend_ratio) / 48000.f;
}

__unit_callback void unit_channel_pressure(uint8_t pressure) {
    (void)pressure;
}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t aftertouch) {
    (void)note;
    (void)aftertouch;
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: // CHARACTER (A knob)
            s_character = (Character)clipminmaxu32(0, value, 3);
            break;
        case 1: // DETUNE (B knob)
            s_detune = valf;
            break;
        case 2: // BRIGHTNESS
            s_brightness = valf;
            break;
        case 3: // MOTION
            s_motion = valf;
            break;
        case 4: // ATTACK
            s_attack = valf;
            break;
        case 5: // GLIDE
            s_glide = valf;
            break;
        case 6: // VIBRATO
            s_vibrato = valf;
            break;
        case 7: // WARMTH
            s_warmth = valf;
            break;
        case 8: // FLAVOR
            s_flavor = valf;
            break;
        case 9: // SUSTAIN
            s_sustain = valf;
            break;
        default:
            break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return (int32_t)s_character;
        case 1: return (int32_t)(s_detune * 1023.f);
        case 2: return (int32_t)(s_brightness * 1023.f);
        case 3: return (int32_t)(s_motion * 1023.f);
        case 4: return (int32_t)(s_attack * 1023.f);
        case 5: return (int32_t)(s_glide * 1023.f);
        case 6: return (int32_t)(s_vibrato * 1023.f);
        case 7: return (int32_t)(s_warmth * 1023.f);
        case 8: return (int32_t)(s_flavor * 1023.f);
        case 9: return (int32_t)(s_sustain * 1023.f);
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    if (id == 0 && value >= 0 && value < 4) {
        return character_names[value];
    }
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    (void)tempo;
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    (void)counter;
}

