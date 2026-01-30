/*
    PSY FLUTE - Psychedelic Synth-Flute Oscillator
    
    A musical synth-flute voice for house melodies, psychedelic ambient,
    and intros/breakdowns in electronic music.
    
    ALGORITHM:
    - Fundamental oscillator (sine/triangle)
    - Harmonic generator (soft saw + waveshaping)
    - Breath noise (band-passed, attack-modulated)
    - Vibrato (pitch LFO)
    - Tone motion (evolving brightness + noise)
    - Detune/spread (phase-offset second voice)
    
    Based on Korg logue SDK
*/

#include "unit_osc.h"
#include "osc_api.h"
#include "fx_api.h"
#include "utils/float_math.h"

// ========== VOICE STATE ==========

struct Voice {
    float phase;
    float phase_detune;
    float w0;
    float vibrato_phase;
    float motion_phase;
    float attack_env;
    float breath_env;
    bool active;
};

static Voice s_voice;

// ========== NOISE GENERATOR ==========

static uint32_t s_noise_state = 12345;

inline float generate_noise() {
    s_noise_state ^= s_noise_state << 13;
    s_noise_state ^= s_noise_state >> 17;
    s_noise_state ^= s_noise_state << 5;
    return ((float)(s_noise_state % 10000) / 10000.f) * 2.f - 1.f;
}

// ========== PARAMETERS ==========

static float s_flute_type = 0.4f;
static float s_breath = 0.3f;
static float s_brightness = 0.5f;
static float s_vib_rate = 0.4f;
static float s_vib_depth = 0.3f;
static float s_motion = 0.2f;
static float s_spread = 0.3f;
static float s_attack_shape = 0.5f;
static float s_harm_tilt = 0.5f;
static float s_space = 0.5f;

// ========== FLUTE TYPE (3 MODES) ==========

inline void get_flute_character(float type, float *fundamental_gain, 
                                float *harmonic_gain, float *brightness_bias) {
    if (type < 0.33f) {
        // SOFT WOOD (0-33%): Mellow, low harmonics
        *fundamental_gain = 0.9f;
        *harmonic_gain = 0.3f;
        *brightness_bias = -0.2f;
    } else if (type < 0.66f) {
        // BRIGHT SYNTH FLUTE (33-66%): Forward mids
        *fundamental_gain = 0.7f;
        *harmonic_gain = 0.6f;
        *brightness_bias = 0.1f;
    } else {
        // OVERBLOWN PSY (66-100%): Strong upper harmonics
        *fundamental_gain = 0.6f;
        *harmonic_gain = 0.8f;
        *brightness_bias = 0.3f;
    }
}

// ========== FUNDAMENTAL OSCILLATOR ==========

inline float generate_fundamental(float phase) {
    // Sine/triangle blend for fundamental
    float sine = osc_sinf(phase);
    
    // Triangle wave
    float triangle = (phase < 0.5f) ? (4.f * phase - 1.f) : (3.f - 4.f * phase);
    
    // Blend: more sine for flute-like, some triangle for body
    return sine * 0.7f + triangle * 0.3f;
}

// ========== HARMONIC GENERATOR ==========

inline float generate_harmonics(float phase, float harm_tilt) {
    // Soft saw for harmonics
    float saw = 2.f * phase - 1.f;
    
    // Waveshaping for soft harmonics (not harsh)
    float shaped = saw + fastertanh2f(saw * 2.f) * 0.3f;
    
    // Tilt: fundamental vs harmonics
    float harmonic_amount = 0.3f + harm_tilt * 0.7f;
    
    return shaped * harmonic_amount;
}

// ========== BREATH NOISE ==========

static float s_bp_z1_l = 0.f;
static float s_bp_z1_r = 0.f;

inline float generate_breath(float breath_amount, float breath_env) {
    if (breath_amount < 0.01f) return 0.f;
    
    // Generate noise
    float noise = generate_noise();
    
    // Band-pass filter (centered ~2-4 kHz for breath character)
    float bp_coeff = 0.3f;
    
    s_bp_z1_l += bp_coeff * (noise - s_bp_z1_l);
    s_bp_z1_r += bp_coeff * (s_bp_z1_l - s_bp_z1_r);
    
    float breath = s_bp_z1_r;
    
    // Apply breath envelope (more on attack)
    breath *= breath_env * breath_amount;
    
    // Denormal kill
    if (si_fabsf(s_bp_z1_l) < 1e-15f) s_bp_z1_l = 0.f;
    if (si_fabsf(s_bp_z1_r) < 1e-15f) s_bp_z1_r = 0.f;
    
    return breath * 0.5f;
}

// ========== VIBRATO ==========

inline float get_vibrato_offset() {
    if (s_vib_depth < 0.01f) return 0.f;
    
    // Vibrato rate: 0-10 Hz
    float rate = 0.5f + s_vib_rate * 9.5f;
    s_voice.vibrato_phase += rate / 48000.f;
    if (s_voice.vibrato_phase >= 1.f) s_voice.vibrato_phase -= 1.f;
    
    // Sine LFO
    float lfo = osc_sinf(s_voice.vibrato_phase);
    
    // Depth: ±20 cents max
    float depth = s_vib_depth * 0.02f;
    
    return lfo * depth;
}

// ========== TONE MOTION ==========

inline float get_tone_motion_brightness() {
    if (s_motion < 0.01f) return 0.f;
    
    // Slow LFO: 0.05-2 Hz
    float rate = 0.05f + s_motion * 1.95f;
    s_voice.motion_phase += rate / 48000.f;
    if (s_voice.motion_phase >= 1.f) s_voice.motion_phase -= 1.f;
    
    // Triangle LFO for smooth motion
    float triangle = (s_voice.motion_phase < 0.5f) ? 
                     (4.f * s_voice.motion_phase - 1.f) : 
                     (3.f - 4.f * s_voice.motion_phase);
    
    return triangle * s_motion * 0.3f;
}

// ========== BRIGHTNESS SHAPER ==========

static float s_bright_z1 = 0.f;

inline float apply_brightness(float signal, float brightness, float motion_mod) {
    // Combined brightness
    float total_bright = brightness + motion_mod;
    total_bright = clipminmaxf(0.f, total_bright, 1.f);
    
    // Simple tilt EQ
    float coeff = 0.2f + total_bright * 0.6f;
    
    s_bright_z1 += coeff * (signal - s_bright_z1);
    
    float hp = signal - s_bright_z1;
    
    // Tilt: dark = more LP, bright = more HP
    float output = s_bright_z1 * (1.f - total_bright) + 
                   (signal + hp * 0.5f) * total_bright;
    
    // Denormal kill
    if (si_fabsf(s_bright_z1) < 1e-15f) s_bright_z1 = 0.f;
    
    return output;
}

// ========== ATTACK ENVELOPE ==========

inline void update_attack_envelope() {
    // Attack envelope for breath
    float attack_time = 0.01f + s_attack_shape * 0.1f;  // 10-110ms
    float attack_rate = 1.f / (attack_time * 48000.f);
    
    if (s_voice.attack_env < 1.f) {
        s_voice.attack_env += attack_rate;
        if (s_voice.attack_env > 1.f) s_voice.attack_env = 1.f;
    }
    
    // Breath envelope (strong on attack, fades)
    float breath_decay = 0.001f;
    if (s_voice.breath_env > 0.1f) {
        s_voice.breath_env -= breath_decay;
    } else {
        s_voice.breath_env = 0.1f;
    }
}

// ========== SPACE HELPER ==========

inline float apply_space_helper(float signal) {
    // Pre-emphasis for reverb/delay
    // 0% = dry (upfront), 100% = optimized for deep FX
    
    if (s_space < 0.01f) return signal;
    
    // Subtle high-shelf cut and low-shelf cut for "sitting in mix"
    float space_amount = s_space;
    
    // Gentle roll-off
    return signal * (1.f - space_amount * 0.3f);
}

// ========== MAIN OSCILLATOR ==========

inline float generate_psy_flute() {
    if (!s_voice.active) return 0.f;
    
    // Update envelopes
    update_attack_envelope();
    
    // Get flute character
    float fund_gain, harm_gain, bright_bias;
    get_flute_character(s_flute_type, &fund_gain, &harm_gain, &bright_bias);
    
    // Vibrato
    float vib_offset = get_vibrato_offset();
    float w_vibrato = s_voice.w0 * fx_pow2f(vib_offset);
    
    // Fundamental voice
    s_voice.phase += w_vibrato;
    if (s_voice.phase >= 1.f) s_voice.phase -= 1.f;
    
    float fundamental = generate_fundamental(s_voice.phase);
    float harmonics = generate_harmonics(s_voice.phase, s_harm_tilt);
    
    // Detuned voice (for spread)
    float detune_cents = s_spread * 0.15f;  // ±15 cents max
    float w_detune = w_vibrato * fx_pow2f(detune_cents / 12.f);
    
    s_voice.phase_detune += w_detune;
    if (s_voice.phase_detune >= 1.f) s_voice.phase_detune -= 1.f;
    
    float fundamental2 = generate_fundamental(s_voice.phase_detune);
    float harmonics2 = generate_harmonics(s_voice.phase_detune, s_harm_tilt);
    
    // Mix voices (more spread = more second voice)
    float spread_mix = s_spread;
    fundamental = fundamental * (1.f - spread_mix * 0.5f) + fundamental2 * spread_mix * 0.5f;
    harmonics = harmonics * (1.f - spread_mix * 0.5f) + harmonics2 * spread_mix * 0.5f;
    
    // Combine fundamental + harmonics
    float osc = fundamental * fund_gain + harmonics * harm_gain;
    
    // Add breath noise
    float breath = generate_breath(s_breath, s_voice.breath_env);
    osc += breath;
    
    // Tone motion
    float motion_mod = get_tone_motion_brightness();
    
    // Apply brightness
    float total_brightness = s_brightness + bright_bias;
    total_brightness = clipminmaxf(0.f, total_brightness, 1.f);
    
    osc = apply_brightness(osc, total_brightness, motion_mod);
    
    // Apply space helper
    osc = apply_space_helper(osc);
    
    // Final gain
    osc *= 0.8f;
    
    return clipminmaxf(-1.f, osc, 1.f);
}

// ========== UNIT CALLBACKS ==========

__unit_callback int8_t unit_init(const unit_runtime_desc_t *desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    
    s_voice.active = false;
    s_voice.phase = 0.f;
    s_voice.phase_detune = 0.25f;  // Phase offset
    s_voice.w0 = 0.f;
    s_voice.vibrato_phase = 0.f;
    s_voice.motion_phase = 0.f;
    s_voice.attack_env = 0.f;
    s_voice.breath_env = 1.f;
    
    s_flute_type = 0.4f;
    s_breath = 0.3f;
    s_brightness = 0.5f;
    s_vib_rate = 0.4f;
    s_vib_depth = 0.3f;
    s_motion = 0.2f;
    s_spread = 0.3f;
    s_attack_shape = 0.5f;
    s_harm_tilt = 0.5f;
    s_space = 0.5f;
    
    // Reset filter states
    s_bp_z1_l = s_bp_z1_r = 0.f;
    s_bright_z1 = 0.f;
    
    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    s_voice.active = false;
    s_voice.attack_env = 0.f;
    s_voice.breath_env = 1.f;
    s_bp_z1_l = s_bp_z1_r = 0.f;
    s_bright_z1 = 0.f;
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float *in, float *out, uint32_t frames) {
    (void)in;
    
    for (uint32_t f = 0; f < frames; f++) {
        float sample = generate_psy_flute();
        out[f] = sample;
    }
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
    (void)velocity;
    
    s_voice.phase = 0.f;
    s_voice.phase_detune = 0.25f;
    s_voice.vibrato_phase = 0.f;
    s_voice.attack_env = 0.f;
    s_voice.breath_env = 1.f;
    
    s_voice.w0 = osc_w0f_for_note(note, 0);
    s_voice.active = true;
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

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    value = clipminmaxi32(unit_header.params[id].min, value, unit_header.params[id].max);
    const float valf = param_val_to_f32(value);
    
    switch (id) {
        case 0: s_flute_type = valf; break;
        case 1: s_breath = valf; break;
        case 2: s_brightness = valf; break;
        case 3: s_vib_rate = valf; break;
        case 4: s_vib_depth = valf; break;
        case 5: s_motion = valf; break;
        case 6: s_spread = valf; break;
        case 7: s_attack_shape = valf; break;
        case 8: s_harm_tilt = valf; break;
        case 9: s_space = valf; break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    switch (id) {
        case 0: return (int32_t)(s_flute_type * 1023.f);
        case 1: return (int32_t)(s_breath * 1023.f);
        case 2: return (int32_t)(s_brightness * 1023.f);
        case 3: return (int32_t)(s_vib_rate * 1023.f);
        case 4: return (int32_t)(s_vib_depth * 1023.f);
        case 5: return (int32_t)(s_motion * 1023.f);
        case 6: return (int32_t)(s_spread * 1023.f);
        case 7: return (int32_t)(s_attack_shape * 1023.f);
        case 8: return (int32_t)(s_harm_tilt * 1023.f);
        case 9: return (int32_t)(s_space * 1023.f);
        default: return 0;
    }
}

__unit_callback const char *unit_get_param_str_value(uint8_t id, int32_t value) {
    (void)id;
    (void)value;
    return "";
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    (void)tempo;
}

__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) {
    (void)counter;
}

